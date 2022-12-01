#ifndef __YOLO_V5_H_
#define __YOLO_V5_H_

#include <stdint.h>
#include "rknn_api.h"
#include "rga_func.h"
#include "comm.h"

#define OBJ_CLASS_NUM     80
#define PROP_BOX_SIZE     (5+OBJ_CLASS_NUM)
#define NMS_THRESHOLD     0.45
#define CONF_THRESHOLD    0.25

typedef enum {
    YOLOX = 0,
    YOLOV5,
    YOLOV7
} MODEL_TYPE;

typedef enum {
    U8 = 0,
    FP = 1,
} POST_PROCESS_TYPE;

typedef enum {
    SINGLE_IMG = 0,
    MULTI_IMG,
    VIDEO_STREAM
} INPUT_SOURCE;

typedef struct _MODEL_INFO {
    MODEL_TYPE m_type;
    POST_PROCESS_TYPE post_type;
    INPUT_SOURCE in_source;

    char *m_path = nullptr;
    char *in_path = nullptr;

    int channel;
    int height;
    int width;
    RgaSURF_FORMAT color_expect;

    int anchors[18];
    int anchor_per_branch;

    int in_nodes;
    rknn_tensor_attr *in_attr = nullptr;

    int out_nodes = 3;
    rknn_tensor_attr *out_attr = nullptr;

    int strides[3] = {8, 16, 32};

} MODEL_INFO;

typedef struct _LETTER_BOX {
    int in_width, in_height;
    int target_width, target_height;

    float img_wh_ratio, target_wh_ratio, resize_scale;
    int resize_width, resize_height;
    int h_pad, w_pad;
    bool add_extra_sz_h_pad = false;
    bool add_extra_sz_w_pad = false;
} LETTER_BOX;

int readLines(const char *fileName, char *lines[], int max_line);

int compute_letter_box(LETTER_BOX *lb);

int post_process(rknn_output *rk_outputs, MODEL_INFO *m, LETTER_BOX *lb, detect_result_group_t *group);

int readFloats(const char *fileName, float *result, int max_line, int *valid_number);

int post_process_640_v5(rknn_output *rk_outputs, MODEL_INFO *m, detect_result_group_t *group);

int loadLabelName(const char *locationFilename, char *label[]);


#endif //__YOLO_V5_H_
