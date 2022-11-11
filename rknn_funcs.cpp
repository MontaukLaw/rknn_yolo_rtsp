#include "comm.h"
#include "rknn_funcs.h"

#include <vector>
using namespace std;
using namespace cimg_library;

static rknn_input_output_num io_num;
static rknn_input_output_num out_num;

static rknn_tensor_attr output_attrs[3];
static rknn_tensor_attr input_attrs[1];
static const float nms_threshold = 0.65;
static const float conf_threshold = 0.6;

int model_input_width = 0;
int model_input_height = 0;
static rknn_input inputs[1];
static rknn_output outputs[3];
static int input_channel = 3;
static rknn_context rknnCtx;
static int modelSize;

void create_rknn_list(rknn_list** s) {
    if (*s != NULL)
        return;
    *s = (rknn_list*)malloc(sizeof(rknn_list));
    (*s)->top = NULL;
    (*s)->size = 0;
    printf("create rknn_list success\n");
}

void destory_rknn_list(rknn_list** s) {
    Node* t = NULL;
    if (*s == NULL)
        return;
    while ((*s)->top) {
        t = (*s)->top;
        (*s)->top = t->next;
        free(t);
    }
    free(*s);
    *s = NULL;
}

static void printRKNNTensor(rknn_tensor_attr* attr)
{
    printf("index=%d name=%s n_dims=%d dims=[%d %d %d %d] n_elems=%d size=%d "
        "fmt=%d type=%d qnt_type=%d fl=%d zp=%d scale=%f\n",
        attr->index, attr->name, attr->n_dims, attr->dims[3], attr->dims[2],
        attr->dims[1], attr->dims[0], attr->n_elems, attr->size, 0, attr->type,
        attr->qnt_type, attr->fl, attr->zp, attr->scale);
}

int rknn_list_size(rknn_list* s) {
    if (s == NULL)
        return -1;
    return s->size;
}

void rknn_list_pop(rknn_list* s, long* timeval, detect_result_group_t* detect_result_group) {
    Node* t = NULL;
    if (s == NULL || s->top == NULL)
        return;
    t = s->top;
    *timeval = t->timeval;
    *detect_result_group = t->detect_result_group;
    s->top = t->next;
    free(t);
    s->size--;
}

void rknn_list_drop(rknn_list* s) {
    Node* t = NULL;
    if (s == NULL || s->top == NULL)
        return;
    t = s->top;
    s->top = t->next;
    free(t);
    s->size--;
}

void rknn_list_push(rknn_list* s, long timeval, detect_result_group_t detect_result_group) {
    Node* t = NULL;
    t = (Node*)malloc(sizeof(Node));
    t->timeval = timeval;
    t->detect_result_group = detect_result_group;
    if (s->top == NULL) {
        s->top = t;
        t->next = NULL;
    }
    else {
        t->next = s->top;
        s->top = t;
    }
    s->size++;
}

// Load data from file, just like inputstream in java
static unsigned char* load_data(FILE* fp, size_t ofst, size_t sz)
{
    unsigned char* data;
    int ret;

    data = NULL;

    if (NULL == fp)
    {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        printf("blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char*)malloc(sz);
    if (data == NULL)
    {
        printf("buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

// load yolo model from file name
// return model data
unsigned char* load_model(const char* filename, int* model_size)
{

    FILE* fp;
    unsigned char* data;

    // Read model file
    fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}


static int get_input_attr(void) {
    printf("input tensors:\n");
    memset(input_attrs, 0, sizeof(input_attrs));
    for (unsigned int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        int ret = rknn_query(rknnCtx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]),
            sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        printRKNNTensor(&(input_attrs[i]));
    }

    return 0;
}

static int get_output_attr(void) {
    printf("output tensors:\n");

    memset(output_attrs, 0, sizeof(output_attrs));
    for (unsigned int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        int ret = rknn_query(rknnCtx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]),
            sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        printRKNNTensor(&(output_attrs[i]));
    }
    return 0;

}

int init_model(const char* modelPath) {
    unsigned char* model;
    int ret = 0;
    printf("Loading model ...\n");

    model = load_model(modelPath, &modelSize);
    ret = rknn_init(&rknnCtx, model, modelSize, 0);
    if (ret < 0) {
        printf("rknn_init fail! ret=%d\n", ret);
        return -1;
    }

    ret = rknn_query(rknnCtx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input,
        io_num.n_output);

    // Get Model Input Output Info
    ret = rknn_query(rknnCtx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input,
        io_num.n_output);

    ret = get_input_attr();
    if (ret < 0) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }

    ret = get_output_attr();
    if (ret < 0) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        printf("model is NCHW input fmt\n");
        model_input_width = input_attrs[0].dims[0];
        model_input_height = input_attrs[0].dims[1];
        // So the width and height is 640, 640
    }
    else
    {
        printf("model is NHWC input fmt\n");
        model_input_width = input_attrs[0].dims[1];
        model_input_height = input_attrs[0].dims[2];
    }

    printf("model input height=%d, width=%d, channel=%d\n", model_input_height, model_input_width, input_channel);

    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = model_input_width * model_input_height * input_channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;   // Default format is NHWC
    inputs[0].pass_through = 0;

    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < io_num.n_output; i++)
    {
        outputs[i].want_float = 0;
    }

    return 0;

}

double get_us_ts(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

int detect_by_buf(void* data, detect_result_group_t* detect_result_group) {
    int ret = 0;
    struct timeval start_time, stop_time;
    gettimeofday(&start_time, NULL);

    // memcpy(drm_buf, data, YOLO_INPUT_DATASIZE);
    // printf("Data been copied, size is %d\n", data_size);

    inputs[0].buf = data; // drm_buf;
    rknn_inputs_set(rknnCtx, io_num.n_input, inputs);
    ret = rknn_run(rknnCtx, NULL);
    ret = rknn_outputs_get(rknnCtx, io_num.n_output, outputs, NULL);

    std::vector<float> out_scales;
    std::vector<uint8_t> out_zps;
    for (int i = 0; i < io_num.n_output; ++i)
    {
        out_scales.push_back(output_attrs[i].scale);
        out_zps.push_back(output_attrs[i].zp);
    }

    post_process_u8((uint8_t*)outputs[0].buf, (uint8_t*)outputs[1].buf,
        (uint8_t*)outputs[2].buf, MODEL_INPUT_SIZE, MODEL_INPUT_SIZE,
        0, 0, 1.0f, conf_threshold, nms_threshold,
        out_zps, out_scales, detect_result_group);

    for (int i = 0; i < detect_result_group->count; i++)
    {
        detect_result_t* det_result = &(detect_result_group->results[i]);
        printf("\n");
        printf("----------->>>>>>>> found target label @: %s\n", det_result->name);
        printf("(%d %d %d %d) %f\n",
            det_result->box.left, det_result->box.top, det_result->box.right, det_result->box.bottom,
            det_result->prop);

    }

    gettimeofday(&stop_time, NULL);

    printf("whole detect used %f ms\n", (get_us_ts(stop_time) - get_us_ts(start_time)) / 1000);

    return 0;
}


