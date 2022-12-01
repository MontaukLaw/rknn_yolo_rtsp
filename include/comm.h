#ifndef __COMM_H__
#define __COMM_H__

#include "3rdparty/CImg/CImg.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <rga/im2d.h>
#include <rga/rga.h>

#include "rkmedia_api.h"
#include "rkmedia_venc.h"
#include "rknn_api.h"
#include "rtsp_demo.h"

#define MODEL_INPUT_SIZE 640
#define MAX_SESSION_NUM 2

#define DRAW_RESULT_BOX_CHN_INDEX 0
#define RK_NN_RGA_CHN_INDEX 1

#define OBJ_NAME_MAX_SIZE 16
#define OBJ_NUMB_MAX_SIZE 64
#define OBJ_CLASS_NUM     80
#define PROP_BOX_SIZE     (5+OBJ_CLASS_NUM)

#define MAX_RKNN_LIST_NUM 10
#define UPALIGNTO(value, align) ((value + align - 1) & (~(align - 1)))
#define UPALIGNTO16(value) UPALIGNTO(value, 16)
#define YOLO_INPUT_SIZE (MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3)
#define OBJ_NAME_MAX_SIZE 16
#define FPS 30

struct Session {
    char path[64];
    CODEC_TYPE_E video_type;
    RK_U32 u32Width;
    RK_U32 u32Height;
    IMAGE_TYPE_E enImageType;
    char videopath[120];

    rtsp_session_handle session;
    MPP_CHN_S stViChn;
    MPP_CHN_S stVenChn;
    MPP_CHN_S stRgaChn;
};
typedef struct _BOX_RECT {
    int left;
    int right;
    int top;
    int bottom;
} BOX_RECT;

typedef struct __detect_result_t {
    char name[OBJ_NAME_MAX_SIZE];
    int class_index;
    BOX_RECT box;
    float prop;
} detect_result_t;

typedef struct _detect_result_group_t {
    int id;
    int detect_count;
    detect_result_t results[OBJ_NUMB_MAX_SIZE];
} detect_result_group_t;

// rknn list to draw boxs asynchronously

struct demo_cfg {
    int session_count;
    struct Session session_cfg[MAX_SESSION_NUM];
};

void common_vi_setup(struct Session *session, VI_CHN_WORK_MODE mode, RK_S32 vi_pipe);

void common_venc_setup(struct Session *session, bool ifSubStream);
// void common_venc_setup(struct Session* session);

unsigned char *load_model(const char *filename, int *model_size);

void trans_data_for_yolo_input(unsigned char *rga_buffer_model_input, struct demo_cfg cfg, MEDIA_BUFFER buffer);

long get_current_time_ms(void);

int bind_rga_for_vi(struct Session session);
#ifdef __cplusplus
}
#endif

#endif