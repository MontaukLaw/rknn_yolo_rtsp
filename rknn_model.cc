/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <vector>
#include <string>
#include <stdbool.h>

#define _BASETSD_H

#undef cimg_display
#define cimg_display 0
#undef cimg_use_jpeg
#define cimg_use_jpeg 1
#undef cimg_use_png
#define cimg_use_png 1

#include "include/3rdparty/CImg/CImg.h"
#include "include/rga_func.h"
#include "include/yolo.h"
#include "include/comm.h"
#include "include/drm_func.h"

#define PERF_WITH_POST 1
#define COCO_IMG_NUMBER 5000
#define DUMP_INPUT 0

using namespace cimg_library;

rknn_input inputs[1];
rknn_output outputs[3];
int fixAnchors[18] = {10, 13, 16, 30, 33, 23,
                      30, 61, 62, 45, 59, 119,
                      116, 90, 156, 198, 373, 326};

/*-------------------------------------------
                  Main Functions
-------------------------------------------*/
static rknn_context m_ctx;
static MODEL_INFO m_info;
static rga_context rga_ctx;
static drm_context drm_ctx;
static void *drm_buf = NULL;
static int drm_fd = -1;
static int buf_fd = -1; // converted from buffer handle
static unsigned int handle;
static LETTER_BOX letter_box;
static size_t actual_size = 0;
static int img_width = 0;
static int img_height = 0;
static int img_channel = 0;

static void printRKNNTensor(rknn_tensor_attr *attr) {
    printf("index=%d name=%s n_dims=%d dims=[%d %d %d %d] n_elems=%d size=%d "
           "fmt=%d type=%d qnt_type=%d fl=%d zp=%d scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[3], attr->dims[2],
           attr->dims[1], attr->dims[0], attr->n_elems, attr->size, 0, attr->type,
           attr->qnt_type, attr->fl, attr->zp, attr->scale);
}

int query_model_info(MODEL_INFO *m, rknn_context ctx) {
    int ret;
    /* Query sdk version */
    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version,
                     sizeof(rknn_sdk_version));
    if (ret < 0) {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version,
           version.drv_version);

    /* Get input,output attr */
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input,
           io_num.n_output);
    m->in_nodes = io_num.n_input;
    m->out_nodes = io_num.n_output;
    m->in_attr = (rknn_tensor_attr *) malloc(sizeof(rknn_tensor_attr) * io_num.n_input);
    m->out_attr = (rknn_tensor_attr *) malloc(sizeof(rknn_tensor_attr) * io_num.n_output);
    if (m->in_attr == NULL || m->out_attr == NULL) {
        printf("alloc memery failed\n");
        return -1;
    }

    for (int i = 0; i < io_num.n_input; i++) {
        m->in_attr[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &m->in_attr[i],
                         sizeof(rknn_tensor_attr));
        if (ret < 0) {
            printf("rknn_init error ret=%d\n", ret);
            return -1;
        }
        printRKNNTensor(&m->in_attr[i]);
    }

    for (int i = 0; i < io_num.n_output; i++) {
        m->out_attr[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(m->out_attr[i]),
                         sizeof(rknn_tensor_attr));
        printRKNNTensor(&(m->out_attr[i]));
    }

    /* get input shape */
    if (io_num.n_input > 1) {
        printf("expect model have 1 input, but got %d\n", io_num.n_input);
        return -1;
    }

    if (m->in_attr[0].fmt == RKNN_TENSOR_NCHW) {
        printf("model is NCHW input fmt\n");
        m->width = m->in_attr[0].dims[0];
        m->height = m->in_attr[0].dims[1];
        m->channel = m->in_attr[0].dims[2];
    } else {
        printf("model is NHWC input fmt\n");
        m->width = m->in_attr[0].dims[1];
        m->height = m->in_attr[0].dims[2];
        m->channel = m->in_attr[0].dims[0];
    }
    printf("model input height=%d, width=%d, channel=%d\n", m->height, m->width, m->channel);

    return 0;
}

double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

// 模型初始化
int init_model(int argc, char **argv) {

    int ret = 0;
    m_info.m_path = (char *) argv[1];
    // 初始化模型参数
    printf("model file is : %s \n", m_info.m_path);
    m_info.m_type = YOLOV5;
    m_info.color_expect = RK_FORMAT_RGB_888;
    m_info.anchor_per_branch = 3;
    memcpy(m_info.anchors, fixAnchors, sizeof(fixAnchors));
    m_info.post_type = U8;
    m_info.in_source = VIDEO_STREAM;

    /* Create the neural network */
    // 根据模型文件创建模型
    printf("Loading model...\n");
    int model_data_size = 0;
    unsigned char *model_data = load_model(m_info.m_path, &model_data_size);
    printf("model_data_size = %d \n", model_data_size);

    // rknn初始化
    ret = rknn_init(&m_ctx, model_data, model_data_size, 0);
    if (ret < 0) {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }

    // 使用rknn的api查询模型信息
    printf("query info\n");
    ret = query_model_info(&m_info, m_ctx);
    if (ret < 0) {
        return -1;
    }

    /* 初始化输入tensor */
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8; /* SAME AS INPUT IMAGE */
    inputs[0].size = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3;
    // model is NCHW, but it's fine
    inputs[0].fmt = RKNN_TENSOR_NHWC; // RKNN_TENSOR_NCHW; // RKNN_TENSOR_NHWC; /* SAME AS INPUT IMAGE */
    inputs[0].pass_through = 0;

    /* 初始化输出的tensor */
    memset(outputs, 0, sizeof(outputs));

    // 不使用float， 因为模型是用u8量化过
    for (int i = 0; i < m_info.out_nodes; i++) {
        outputs[i].want_float = 0;
    }

    return 0;
}

/**
 * 对数据进行推理
 * @param bufData 输入数据，RGB格式
 * @param detect_result_group  输出结果
 * @return
 */
int predict(void *bufData, detect_result_group_t *detect_result_group) {

    printf("start detect \n");

    struct timeval start_time, stop_time;
    int ret;

    // 把数据放入输入tensor
    inputs[0].buf = bufData;

    // 统计时间， 方便后面计时
    gettimeofday(&start_time, NULL);

    // 使用rknn的api进行推理
    rknn_inputs_set(m_ctx, 1, inputs);
    ret = rknn_run(m_ctx, NULL);
    ret = rknn_outputs_get(m_ctx, 3, outputs, NULL);

    /* 后处理 */
    post_process_640_v5(outputs, &m_info, detect_result_group);

    gettimeofday(&stop_time, NULL);
    printf("once run use %f ms\n",
           (__get_us(stop_time) - __get_us(start_time)) / 1000);

    for (int i = 0; i < detect_result_group->detect_count; i++) {
        detect_result_t *det_result = &(detect_result_group->results[i]);
        printf("%s @ (%d %d %d %d) %f\n",
               det_result->name,
               det_result->box.left, det_result->box.top, det_result->box.right, det_result->box.bottom,
               det_result->prop);
    }

    // fclose(fp);
    return ret;
}

int main_test(int argc, char **argv) {
    int ret = 0;
    // MODEL_INFO m_info;
    memset(&rga_ctx, 0, sizeof(rga_context));
    memset(&drm_ctx, 0, sizeof(drm_context));
    // drm_fd = drm_init(&drm_ctx);

    ret = init_model(argc, argv);
    if (ret < 0) {
        printf("init model failed\n");
        return -1;
    }

    printf("init model success\n");

    // predict(NULL);

    // release
    ret = rknn_destroy(m_ctx);

    return 0;
}

