#ifndef __RKNN_RTSP_COMM_H
#define __RKNN_RTSP_COMM_H

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "rkmedia_api.h"
#include "rtsp_demo.h"
#include "sample_common.h"

#include "comm.h"
#include "postprocess.h"
#include "rga_trans.h"
#include "rknn_funcs.h"
#include "rtsp_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_rtsp(void);

void video_packet_cb(MEDIA_BUFFER mb);

void clean_rtsp(void);

#ifdef __cplusplus
}
#endif

#endif //__RKNN_RTSP_COMM_H
