#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include "include/yolo.h"
#include <stdint.h>
#include "include/comm.h"

#define LABEL_NALE_TXT_PATH "coco_80_labels_list.txt"

static char *labels[OBJ_CLASS_NUM];

const int anchor0[6] = {10, 13, 16, 30, 33, 23};
const int anchor1[6] = {30, 61, 62, 45, 59, 119};
const int anchor2[6] = {116, 90, 156, 198, 373, 326};

inline static int clamp(float val, int min, int max) {
    return val > min ? (val < max ? val : max) : min;
}

// 读一行数据
char *readLine(FILE *fp, char *buffer, int *len) {
    int ch;
    int i = 0;
    size_t buff_len = 0;

    buffer = (char *) malloc(buff_len + 1);
    if (!buffer)
        return NULL; // Out of memory

    while ((ch = fgetc(fp)) != '\n' && ch != EOF) {
        buff_len++;
        void *tmp = realloc(buffer, buff_len + 1);
        if (tmp == NULL) {
            free(buffer);
            return NULL; // Out of memory
        }
        buffer = (char *) tmp;

        buffer[i] = (char) ch;
        i++;
    }
    buffer[i] = '\0';

    *len = buff_len;

    // Detect end
    if (ch == EOF && (i == 0 || ferror(fp))) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

// 按行读取文件
int readLines(const char *fileName, char *lines[], int max_line) {
    FILE *file = fopen(fileName, "r");
    char *s;
    int i = 0;
    int n = 0;
    while ((s = readLine(file, s, &n)) != NULL) {
        lines[i++] = s;
        if (i >= max_line)
            break;
    }
    return i;
}

// 读取标签文件
int loadLabelName(const char *locationFilename, char *label[]) {
    printf("loadLabelName %s\n", locationFilename);
    readLines(locationFilename, label, OBJ_CLASS_NUM);
    return 0;
}

// 计算iou`
static float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1, float ymax1) {
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) + (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
    return u <= 0.f ? 0.f : (i / u);
}

// 极大值抑制
static int nms(int validCount, std::vector<float> &outputLocations, std::vector<int> &class_id, std::vector<int> &order, float threshold, bool class_agnostic) {
    // printf("class_agnostic: %d\n", class_agnostic);
    for (int i = 0; i < validCount; ++i) {
        if (order[i] == -1) {
            continue;
        }
        int n = order[i];
        for (int j = i + 1; j < validCount; ++j) {
            int m = order[j];
            if (m == -1) {
                continue;
            }

            if (class_agnostic == false && class_id[n] != class_id[m]) {
                continue;
            }

            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}

// 排序
static int quick_sort_indice_inverse(std::vector<float> &input, int left, int right, std::vector<int> &indices) {
    float key;
    int key_index;
    int low = left;
    int high = right;
    if (left < right) {
        key_index = indices[left];
        key = input[left];
        while (low < high) {
            while (low < high && input[high] <= key) {
                high--;
            }
            input[low] = input[high];
            indices[low] = indices[high];
            while (low < high && input[low] >= key) {
                low++;
            }
            input[high] = input[low];
            indices[high] = indices[low];
        }
        input[low] = key;
        indices[low] = key_index;
        quick_sort_indice_inverse(input, left, low - 1, indices);
        quick_sort_indice_inverse(input, low + 1, right, indices);
    }
    return low;
}

static float sigmoid(float x) {
    return 1.0 / (1.0 + expf(-x));
}

static float unsigmoid(float y) {
    return -1.0 * logf((1.0 / y) - 1.0);
}

inline static int32_t __clip(float val, float min, float max) {
    float f = val <= min ? min : (val >= max ? max : val);
    return f;
}

static uint8_t qnt_f32_to_affine(float f32, uint8_t zp, float scale) {
    float dst_val = (f32 / scale) + zp;
    uint8_t res = (uint8_t) __clip(dst_val, 0, 255);
    return res;
}

static float deqnt_affine_to_f32(uint8_t qnt, uint8_t zp, float scale) {
    return ((float) qnt - (float) zp) * scale;
}

static int process_u8(uint8_t *input, int *anchor, int anchor_per_branch, int grid_h, int grid_w, int height, int width, int stride,
                      std::vector<float> &boxes, std::vector<float> &boxScores, std::vector<int> &classId,
                      float threshold, uint8_t zp, float scale, MODEL_TYPE yolo) {
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    float thres = threshold;
    uint8_t thres_u8 = qnt_f32_to_affine(thres, zp, scale);
    // printf("threash %f\n", thres);
    // printf("thres_u8 %u\n", thres_u8);
    // printf("scale %f\n", scale);
    // printf("zp %u\n", zp);

    for (int a = 0; a < anchor_per_branch; a++) {
        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                uint8_t box_confidence = input[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];
                if (box_confidence >= thres_u8) {
                    // printf("box_conf %u, thres_u8 %u\n", box_confidence, thres_u8);
                    int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                    uint8_t *in_ptr = input + offset;

                    uint8_t maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;
                    for (int k = 1; k < OBJ_CLASS_NUM; ++k) {
                        uint8_t prob = in_ptr[(5 + k) * grid_len];
                        if (prob > maxClassProbs) {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }

                    float box_conf_f32 = deqnt_affine_to_f32(box_confidence, zp, scale);
                    float class_prob_f32 = deqnt_affine_to_f32(maxClassProbs, zp, scale);
                    float limit_score = 0;
                    limit_score = box_conf_f32 * class_prob_f32;

                    // printf("limit score: %f\n", limit_score);
                    if (limit_score > threshold) {
                        float box_x, box_y, box_w, box_h;
                        if (yolo == YOLOX) {
                            box_x = deqnt_affine_to_f32(*in_ptr, zp, scale);
                            box_y = deqnt_affine_to_f32(in_ptr[grid_len], zp, scale);
                            box_w = deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale);
                            box_h = deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale);
                            box_w = exp(box_w) * stride;
                            box_h = exp(box_h) * stride;
                        } else {
                            box_x = deqnt_affine_to_f32(*in_ptr, zp, scale) * 2.0 - 0.5;
                            box_y = deqnt_affine_to_f32(in_ptr[grid_len], zp, scale) * 2.0 - 0.5;
                            box_w = deqnt_affine_to_f32(in_ptr[2 * grid_len], zp, scale) * 2.0;
                            box_h = deqnt_affine_to_f32(in_ptr[3 * grid_len], zp, scale) * 2.0;
                            box_w = box_w * box_w;
                            box_h = box_h * box_h;
                        }
                        box_x = (box_x + j) * (float) stride;
                        box_y = (box_y + i) * (float) stride;
                        box_w *= (float) anchor[a * 2];
                        box_h *= (float) anchor[a * 2 + 1];
                        box_x -= (box_w / 2.0);
                        box_y -= (box_h / 2.0);

                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                        boxScores.push_back(box_conf_f32 * class_prob_f32);
                        classId.push_back(maxClassId);
                        validCount++;
                    }
                }
            }
        }
    }
    return validCount;
}

// 对经过了骨干网络后的数据做后处理
int post_process_640_v5(rknn_output *rk_outputs, MODEL_INFO *m, detect_result_group_t *group) {
    printf("post_process_640_v5\n");
    static int init = -1;
    // 这个读取label的动作只需要做一次
    if (init == -1) {
        int ret = 0;
        // load only once!
        ret = loadLabelName(LABEL_NALE_TXT_PATH, labels);
        if (ret < 0) {
            return -1;
        }

        init = 0;
    }

    memset(group, 0, sizeof(detect_result_group_t));

    std::vector<float> filterBoxes;
    std::vector<float> boxesScore;
    std::vector<int> classId;
    int validCount = 0;
    int stride = 0;
    int grid_h = 0;
    int grid_w = 0;
    int *anchors;

    // 输出3个output， 如你所知， yolo有三个粒度的输出
    for (int i = 0; i < m->out_nodes; i++) {
        stride = m->strides[i];
        grid_h = m->height / stride;
        grid_w = m->width / stride;
        anchors = &(m->anchors[i * 2 * m->anchor_per_branch]);
        // printf("post process parse\n");
        // 把置信度的阈值当作输入，因为后处理需要IOU抑制，低于置信度阈值不用下一步
        validCount = validCount + process_u8((uint8_t *) rk_outputs[i].buf, anchors, m->anchor_per_branch,
                                             grid_h, grid_w, m->height, m->width, stride,
                                             filterBoxes, boxesScore, classId, CONF_THRESHOLD, m->out_attr[i].zp,
                                             m->out_attr[i].scale, m->m_type);
    }

    // no object detect
    if (validCount == 0) {
        printf("found fucking nothing\n");
        return -1;
    }

    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i) {
        indexArray.push_back(i);
    }

    quick_sort_indice_inverse(boxesScore, 0, validCount - 1, indexArray);

    // 非极大值抑制
    nms(validCount, filterBoxes, classId, indexArray, NMS_THRESHOLD, false);

    int last_count = 0;
    group->detect_count = 0;
    /* 同一个画面如果有多个预测框结果，逐一输出到detect_result_group_t中*/
    for (int i = 0; i < validCount; ++i) {

        if (indexArray[i] == -1 || boxesScore[i] < CONF_THRESHOLD || last_count >= OBJ_NUMB_MAX_SIZE) {
            continue;
        }
        int n = indexArray[i];

        float x1 = filterBoxes[n * 4 + 0];
        float y1 = filterBoxes[n * 4 + 1];
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        int id = classId[n];

        group->results[last_count].box.left = (int) ((clamp(x1, 0, MODEL_INPUT_SIZE)));
        group->results[last_count].box.top = (int) ((clamp(y1, 0, MODEL_INPUT_SIZE)));
        group->results[last_count].box.right = (int) ((clamp(x2, 0, MODEL_INPUT_SIZE)));
        group->results[last_count].box.bottom = (int) ((clamp(y2, 0, MODEL_INPUT_SIZE)));
        group->results[last_count].prop = boxesScore[i];
        group->results[last_count].class_index = id;
        char *label = labels[id];
        strncpy(group->results[last_count].name, label, OBJ_NAME_MAX_SIZE);

        printf("result %2d: (%4d, %4d, %4d, %4d), %s\n", i, group->results[last_count].box.left, group->results[last_count].box.top,
               group->results[last_count].box.right, group->results[last_count].box.bottom, label);

        last_count++;
    }

    group->detect_count = last_count;
    return 0;
}