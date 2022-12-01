#include "comm.h"
#include "postprocess.h"
#include "rknn_funcs.h"
#include "include/rtsp_comm.h"

#define RTSP_INPUT_VI_WIDTH 1920
#define RTSP_INPUT_VI_HEIGHT 1080

#define X_START ((RTSP_INPUT_VI_WIDTH - MODEL_INPUT_SIZE) / 2)
#define Y_START ((RTSP_INPUT_VI_HEIGHT - MODEL_INPUT_SIZE) / 2)
// (1080-640)= 440/2 = 220

// rtsp obj for streaming main_stream and sub_stream
static rtsp_demo_handle g_rtsplive = NULL;
// rknn result list to exchange data between two thread
// its just a simple chain list
static rknn_list *rknn_list_;
// iqfile
static RK_CHAR *pIqfilesPath = (RK_CHAR *) "/oem/etc/iqfiles/";
static RK_S32 s32CamId = 0;
static RK_BOOL bMultictx = RK_FALSE;
static rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
int g_flag_run = 1;

static pthread_t observer_thread_id;
static pthread_t get_rga_buffer_thread_id;
static pthread_t rknn_yolo_thread_id;

static struct demo_cfg cfg;
static char *yoloModelFilePath = NULL;
static int yoloModelSize = 0;
RK_BOOL ifDetecting = RK_FALSE;

rtsp_session_handle g_rtsp_session;

static int nv12_border(char *pic, int pic_w, int pic_h, int rect_x, int rect_y,
                       int rect_w, int rect_h, int R, int G, int B) {
    /* Set up the rectangle border size */
    const int border = 5;

    /* RGB convert YUV */
    int Y, U, V;
    Y = 0.299 * R + 0.587 * G + 0.114 * B;
    U = -0.1687 * R + 0.3313 * G + 0.5 * B + 128;
    V = 0.5 * R - 0.4187 * G - 0.0813 * B + 128;
    /* Locking the scope of rectangle border range */
    int j, k;
    for (j = rect_y; j < rect_y + rect_h; j++) {
        for (k = rect_x; k < rect_x + rect_w; k++) {
            if (k < (rect_x + border) || k > (rect_x + rect_w - border) ||
                j < (rect_y + border) || j > (rect_y + rect_h - border)) {
                /* Components of YUV's storage address index */
                int y_index = j * pic_w + k;
                int u_index =
                        (y_index / 2 - pic_w / 2 * ((j + 1) / 2)) * 2 + pic_w * pic_h;
                int v_index = u_index + 1;
                /* set up YUV's conponents value of rectangle border */
                pic[y_index] = Y;
                pic[u_index] = U;
                pic[v_index] = V;
            }
        }
    }

    return 0;
}

static void init_isp(void) {
    SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, pIqfilesPath);
    SAMPLE_COMM_ISP_Run(s32CamId);
    SAMPLE_COMM_ISP_SetFrameRate(s32CamId, FPS);
}

static void dump_cfg() {
    for (int i = 0; i < cfg.session_count; i++) {
        printf("rtsp path = %s.\n", cfg.session_cfg[i].path);
        printf("video_type = %d.\n", cfg.session_cfg[i].video_type);
        printf("width = %d.\n", cfg.session_cfg[i].u32Width);
        printf("height = %d.\n", cfg.session_cfg[i].u32Height);
        printf("video path =%s.\n", cfg.session_cfg[i].videopath);
        printf("image type = %u.\n", cfg.session_cfg[i].enImageType);
    }
}

static void sig_proc(int signo) {
    fprintf(stderr, "signal %d\n", signo);
    g_flag_run = 0;
}

//get data buffer for both session
static void main_vi_process(void) {
    MEDIA_BUFFER buffer;

    while (g_flag_run) {

        // only get venc chn 0
        buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, DRAW_RESULT_BOX_CHN_INDEX, -1);
        if (buffer) {
            rtsp_tx_video(cfg.session_cfg[0].session, (const uint8_t *) RK_MPI_MB_GetPtr(buffer),
                          RK_MPI_MB_GetSize(buffer), RK_MPI_MB_GetTimestamp(buffer));

            rtsp_do_event(cfg.session_cfg[0].session);

            RK_MPI_MB_ReleaseBuffer(buffer);
        }

        usleep(1000);
    }
}

#if 0
static void main_vi_process_old(void) {

    while (g_flag_run) {
        // get both session
        for (int i = 0; i < cfg.session_count; i++) {
            MEDIA_BUFFER buffer;
            // get data from venc, then send to rtsp
            buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, cfg.session_cfg[i].stVenChn.s32ChnId, 0);
            if (buffer) {
                rtsp_tx_video(cfg.session_cfg[i].session, (const uint8_t *) RK_MPI_MB_GetPtr(buffer),
                              RK_MPI_MB_GetSize(buffer), RK_MPI_MB_GetTimestamp(buffer));

                RK_MPI_MB_ReleaseBuffer(buffer);
            }
        }
        rtsp_do_event(g_rtsplive);

        usleep(1000);
    }
}
#endif

static void init_sessions(void) {
    int ret = 0;
    // there are 2 session, one for DRAW box one for RTSP
    for (int i = 0; i < cfg.session_count; i++) {
        if (i == 0) {
            cfg.session_cfg[i].session = rtsp_new_session(g_rtsplive, "/live/main_stream");
        } else {
            cfg.session_cfg[i].session = rtsp_new_session(g_rtsplive, "/live/sub_stream");
        }

        // VI create
        printf("VI create\n");
        cfg.session_cfg[i].stViChn.enModId = RK_ID_VI;
        cfg.session_cfg[i].stViChn.s32ChnId = i;

        // same cam id means same sensor, but defferent channel
        common_vi_setup(&cfg.session_cfg[i], VI_WORK_MODE_NORMAL, s32CamId);

        // VENC create
        printf("VENC create\n");
        cfg.session_cfg[i].stVenChn.enModId = RK_ID_VENC;
        cfg.session_cfg[i].stVenChn.s32ChnId = i;
        if (i == DRAW_RESULT_BOX_CHN_INDEX) {
            common_venc_setup(&cfg.session_cfg[i], true);
        } else {
            common_venc_setup(&cfg.session_cfg[i], false);
        }
        cfg.session_cfg[i].stRgaChn.enModId = RK_ID_RGA;
        cfg.session_cfg[i].stRgaChn.s32ChnId = i;
        // one session get the stream to rknn
        // stream camera 0 to rknn

        // bind RKNN chn to rga RK_NN_RGA_CHN_INDEX == 1
        if (i == RK_NN_RGA_CHN_INDEX) {
            ret = bind_rga_for_vi(cfg.session_cfg[i]);
            if (ret < 0) {
                printf("bind rga for vi failed\n");
                break;
            }
            RK_MPI_VI_StartStream(s32CamId, cfg.session_cfg[i].stViChn.s32ChnId);
        } else {
            printf("venc chn:%d \r\n", cfg.session_cfg[i].stVenChn.s32ChnId);

            // bind vi to venc
            RK_MPI_SYS_Bind(&cfg.session_cfg[i].stViChn, &cfg.session_cfg[i].stVenChn);
        }

        // if (i == DRAW_RESULT_BOX_CHN_INDEX) {
        // RK_MPI_VI_StartStream(s32CamId, cfg.session_cfg[i].stViChn.s32ChnId);
        // }
        // else {
        //RK_MPI_VI_StartStream(s32CamId, cfg.session_cfg[i].stViChn.s32ChnId);
        // this session go to rtsp directly
        // RK_MPI_SYS_Bind(&cfg.session_cfg[i].stViChn, &cfg.session_cfg[i].stVenChn);
        //}

        // rtsp video
        printf("rtsp video\n");
        // both session need to set rtsp video
        rtsp_set_video(cfg.session_cfg[i].session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
        rtsp_sync_video_ts(cfg.session_cfg[i].session, rtsp_get_reltime(), rtsp_get_ntptime());
    }
}

static void destroy_all(void) {

    if (observer_thread_id) {
        pthread_join(observer_thread_id, NULL);
    }

    if (get_rga_buffer_thread_id) {
        pthread_join(get_rga_buffer_thread_id, NULL);
    }

    if (rknn_yolo_thread_id) {
        pthread_join(rknn_yolo_thread_id, NULL);
    }

    rtsp_del_demo(g_rtsplive);
    RK_MPI_SYS_UnBind(&cfg.session_cfg[0].stViChn, &cfg.session_cfg[0].stVenChn);
    RK_MPI_VENC_DestroyChn(cfg.session_cfg[0].stVenChn.s32ChnId);
    RK_MPI_VI_DisableChn(s32CamId, cfg.session_cfg[0].stViChn.s32ChnId);

    RK_MPI_SYS_UnBind(&cfg.session_cfg[1].stViChn, &cfg.session_cfg[1].stRgaChn);
    RK_MPI_SYS_UnBind(&cfg.session_cfg[1].stRgaChn, &cfg.session_cfg[1].stVenChn);
    RK_MPI_VENC_DestroyChn(cfg.session_cfg[1].stVenChn.s32ChnId);
    RK_MPI_RGA_DestroyChn(cfg.session_cfg[1].stRgaChn.s32ChnId);
    RK_MPI_VI_DisableChn(s32CamId, cfg.session_cfg[1].stViChn.s32ChnId);

    SAMPLE_COMM_ISP_Stop(s32CamId);

    destory_rknn_list(&rknn_list_);

}

void print_mb_info(MEDIA_BUFFER buffer) {
    int cnt = 0;
    printf("#%d Get Frame:ptr:%p, size:%zu, mode:%d, channel:%d, timestamp:%lld\n",
           cnt++, RK_MPI_MB_GetPtr(buffer), RK_MPI_MB_GetSize(buffer),
           RK_MPI_MB_GetModeID(buffer), RK_MPI_MB_GetChannelID(buffer),
           RK_MPI_MB_GetTimestamp(buffer));
}

static void draw_box(MEDIA_BUFFER *buffer) {
    // draw box
    if (rknn_list_size(rknn_list_)) {
        long time_before;
        detect_result_group_t detect_result_group;
        memset(&detect_result_group, 0, sizeof(detect_result_group));

        // pick up the first one
        rknn_list_pop(rknn_list_, &time_before, &detect_result_group);
        // printf("time interval is %ld\n", getCurrentTimeMsec() - time_before);

        // iterate the detected object
        for (int j = 0; j < detect_result_group.count; j++) {
            int x = detect_result_group.results[j].box.left + X_START;
            int y = detect_result_group.results[j].box.top + Y_START;
            int w = (detect_result_group.results[j].box.right -
                     detect_result_group.results[j].box.left);
            int h = (detect_result_group.results[j].box.bottom -
                     detect_result_group.results[j].box.top);
            while ((uint32_t) (x + w) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width) {
                w -= 16;
            }
            while ((uint32_t) (y + h) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height) {
                h -= 16;
            }
            printf("border=(%d %d %d %d)\n", x, y, w, h);
            printf("u32Width: %d, u32Height: %d\n", cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width,
                   cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height);

            int picWidth = cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width;
            int picHeight = cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height;
            int rotatedX = picWidth - x - w;
            int rotatedY = picHeight - y - h;
            printf("rotatedX: %d, rotatedY: %d\n", rotatedX, rotatedY);
            nv12_border((char *) RK_MPI_MB_GetPtr(buffer), picWidth, picHeight, rotatedX,
                        rotatedY, w, h, 0, 0, 255);
        }
    }
}

typedef struct g_box_info_t {
    int x;
    int y;
    int w;
    int h;
} box_info_t;

box_info_t boxInfoList[10];
int boxInfoListNumber = 0;
int boxDisplayCounterDown = 0;

static void *observer_thread(void *args) {

    memset(boxInfoList, 0, sizeof(boxInfoList));

    MEDIA_BUFFER buffer;

    while (g_flag_run) {

        buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, DRAW_RESULT_BOX_CHN_INDEX, -1);

        if (!buffer) {
            usleep(1000);
            continue;
        }
        if (rknn_list_size(rknn_list_)) {

            long time_before;
            detect_result_group_t detect_result_group;
            memset(&detect_result_group, 0, sizeof(detect_result_group));

            // pick up the first one
            rknn_list_pop(rknn_list_, &time_before, &detect_result_group);
            // printf("result count:%d \n", detect_result_group.count);

            for (int j = 0; j < detect_result_group.count; j++) {
                int x = detect_result_group.results[j].box.left + X_START;
                int y = detect_result_group.results[j].box.top + Y_START;
                int w = (detect_result_group.results[j].box.right -
                         detect_result_group.results[j].box.left);
                int h = (detect_result_group.results[j].box.bottom -
                         detect_result_group.results[j].box.top);

                while ((uint32_t) (x + w) >= RTSP_INPUT_VI_WIDTH) {
                    w -= 16;
                }
                while ((uint32_t) (y + h) >= RTSP_INPUT_VI_HEIGHT) {
                    h -= 16;
                }
                printf("border=(%d %d %d %d)\n", x, y, w, h);
                boxInfoList[j] = {x, y, w, h};
                boxInfoListNumber++;
            }

            boxDisplayCounterDown = 3;
        }

        // hold box for 3 frames
        if (boxInfoListNumber > 0) {
            for (int i = 0; i < boxInfoListNumber; i++) {
                nv12_border((char *) RK_MPI_MB_GetPtr(buffer), RTSP_INPUT_VI_WIDTH, RTSP_INPUT_VI_HEIGHT,
                            boxInfoList[i].x, boxInfoList[i].y, boxInfoList[i].w, boxInfoList[i].h, 0, 0, 255);
            }
            boxDisplayCounterDown--;
        }

        if (boxDisplayCounterDown == 0) {
            boxInfoListNumber = 0;
            memset(boxInfoList, 0, sizeof(boxInfoList));
        }

        nv12_border((char *) RK_MPI_MB_GetPtr(buffer), RTSP_INPUT_VI_WIDTH, RTSP_INPUT_VI_HEIGHT,
                    X_START, Y_START, 640, 640, 0, 200, 0);

        RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, DRAW_RESULT_BOX_CHN_INDEX, buffer);
        RK_MPI_MB_ReleaseBuffer(buffer);
    }
    return nullptr;
}

#if 0
// to draw box and send to rtsp
static void *observer_thread_old(void *args) {
    MEDIA_BUFFER buffer;
    // float xStart = (cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width - MODEL_INPUT_SIZE) / 2;
    // float yStart = (cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height - MODEL_INPUT_SIZE) / 2;
    // printf("xStart:%f, yStart:%f\n", xStart, yStart);

    int cnt = 0;
    // printf("main stream chn id is %d\n", cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stRgaChn.s32ChnId);

    while (g_flag_run) {
        buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stViChn.s32ChnId, -1);
        // buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stRgaChn.s32ChnId, -1);

        if (!buffer) {
            usleep(1000);
            continue;
        }
        // draw box
        if (rknn_list_size(rknn_list_)) {
            long time_before;
            detect_result_group_t detect_result_group;
            memset(&detect_result_group, 0, sizeof(detect_result_group));

            // pick up the first one
            rknn_list_pop(rknn_list_, &time_before, &detect_result_group);
            // printf("time interval is %ld\n", getCurrentTimeMsec() - time_before);

            // iterate the detected object
            for (int j = 0; j < detect_result_group.count; j++) {
                int x = detect_result_group.results[j].box.left + X_START;
                int y = detect_result_group.results[j].box.top + Y_START;
                int w = (detect_result_group.results[j].box.right -
                         detect_result_group.results[j].box.left);
                int h = (detect_result_group.results[j].box.bottom -
                         detect_result_group.results[j].box.top);
                while ((uint32_t) (x + w) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width) {
                    w -= 16;
                }
                while ((uint32_t) (y + h) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height) {
                    h -= 16;
                }
                printf("border=(%d %d %d %d)\n", x, y, w, h);
                printf("u32Width: %d, u32Height: %d\n", cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width,
                       cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height);

                int picWidth = cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width;
                int picHeight = cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height;
                int rotatedX = picWidth - x - w;
                int rotatedY = picHeight - y - h;
                printf("rotatedX: %d, rotatedY: %d\n", rotatedX, rotatedY);
                nv12_border((char *) RK_MPI_MB_GetPtr(buffer),
                            picWidth, picHeight, rotatedX,
                            rotatedY, w, h, 0, 0, 255);
            }
        }
        // print_mb_info(buffer);

        // send from VI to VENC
        RK_S32 vencChn = cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stVenChn.s32ChnId;
        printf("vencChn: %d\r\n", vencChn);
        RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, vencChn, buffer);
        RK_MPI_MB_ReleaseBuffer(buffer);
    }

    return NULL;
}
#endif

static int load_cfg(const char *cfg_file) {
    FILE *fp = fopen(cfg_file, "r");
    char line[1024];
    int count = 0;

    if (!fp) {
        fprintf(stderr, "open %s failed\n", cfg_file);
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));
    while (fgets(line, sizeof(line) - 1, fp)) {
        const char *p;
        // char codec_type[20];
        memset(&cfg.session_cfg[count], 0, sizeof(cfg.session_cfg[count]));

        if (line[0] == '#')
            continue;
        p = strstr(line, "path=");
        if (!p)
            continue;
        if (sscanf(p, "path=%s", cfg.session_cfg[count].path) != 1)
            continue;

        if ((p = strstr(line, "video_type="))) {
            if (sscanf(p,
                       "video_type=%d width=%u height=%u image_type=%u video_path=%s",
                       &cfg.session_cfg[count].video_type,
                       &cfg.session_cfg[count].u32Width,
                       &cfg.session_cfg[count].u32Height,
                       &cfg.session_cfg[count].enImageType,
                       cfg.session_cfg[count].videopath) == 0) {
                printf("parse video file failed %s.\n", p);
            }
        }
        if (cfg.session_cfg[count].video_type != RK_CODEC_TYPE_NONE) {
            count++;
        } else {
            printf("parse line %s failed\n", line);
        }
    }
    cfg.session_count = count;
    fclose(fp);
    dump_cfg();
    return count;
}

static int nv12_to_rgb24_640x640(void *yuvBuffer, void *rgbBuffer) {

    rga_buffer_t src, dst;
    memset(&src, 0, sizeof(rga_buffer_t));
    memset(&dst, 0, sizeof(rga_buffer_t));

    src = wrapbuffer_virtualaddr(yuvBuffer, 640, 640, RK_FORMAT_YCbCr_420_SP);
    dst = wrapbuffer_virtualaddr(rgbBuffer, 640, 640, RK_FORMAT_RGB_888);

    src.format = RK_FORMAT_YCbCr_420_SP;
    dst.format = RK_FORMAT_RGB_888;

    IM_STATUS status = imcvtcolor(src, dst, src.format, dst.format);

    if (status != IM_STATUS_SUCCESS) {
        printf("ERROR: imcvtcolor failed!\n");
        return -1;
    }

    return 0;
}

static void write_rgb_file(void *data) {
    char saveFilePath[128];
    memset(saveFilePath, 0, sizeof(saveFilePath));

    static int counter = 0;
    counter++;
    sprintf(saveFilePath, "/tmp/rga_%d.rgb", counter);

    FILE *save_file = fopen(saveFilePath, "w");
    if (!save_file) {
        printf("ERROR: Open %s failed!\n", saveFilePath);
    }

    fwrite(data, 1, YOLO_INPUT_SIZE, save_file);
    fclose(save_file);
}


// this thread is for rknn
static void *rknn_yolo_thread(void *args) {

    int ret = 0;

    printf("Start get_media_buffer thread, \n");

    MEDIA_BUFFER buffer = NULL;

    // get data from vi
    while (g_flag_run) {
        ifDetecting = RK_TRUE;
        buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_RGA, RK_NN_RGA_CHN_INDEX, -1);
        if (!buffer) {
            usleep(1000);
            continue;
        }
        // print_mb_info(buffer);

        // int rga_buffer_size = cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Width * cfg.session_cfg[RK_NN_INDEX].u32Height * 3; // nv12 3/2, rgb 3
        // int rga_buffer_model_input_size = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3;
        // auto rga_buffer_model_input = (unsigned char*)malloc(rga_buffer_model_input_size);

        // trans_data_for_yolo_input(rga_buffer_model_input, cfg, buffer);
        void *pRknnInputData = malloc(YOLO_INPUT_SIZE);
        ret = nv12_to_rgb24_640x640(RK_MPI_MB_GetPtr(buffer), pRknnInputData);
        if (ret < 0) {
            printf("nv12_to_rgb24_640x640 failed\n");
        }

        if (ret == 0) {
            detect_result_group_t detect_result_group;
            memset(&detect_result_group, 0, sizeof(detect_result_group_t));

            write_rgb_file(pRknnInputData);

            // Post Process
            detect_by_buf(pRknnInputData, &detect_result_group);

            // put detect result to list
            if (detect_result_group.count > 0) {
                rknn_list_push(rknn_list_, get_current_time_ms(), detect_result_group);
                int size = rknn_list_size(rknn_list_);
                if (size >= MAX_RKNN_LIST_NUM) {
                    rknn_list_drop(rknn_list_);
                }
                printf("size is %d\n", size);
            }
        }

        RK_MPI_MB_ReleaseBuffer(buffer);
        free(pRknnInputData);

        ifDetecting = RK_FALSE;
    }

    return NULL;
}

static void *get_rga_buffer_thread(void *arg) {
    MEDIA_BUFFER buffer = NULL;

    while (g_flag_run) {

        if (RK_TRUE == ifDetecting) {
            buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_RGA, RK_NN_RGA_CHN_INDEX, -1);
            if (!buffer) {
                usleep(1000);
                continue;
            }
            RK_MPI_MB_ReleaseBuffer(buffer);
        }
    }

    return NULL;
}

void venc_packet_cb(MEDIA_BUFFER mb) {

    static RK_S32 packet_cnt = 0;
    if (g_flag_run == 0)
        return;

    printf("#Get packet-%d, size %zu\n", packet_cnt, RK_MPI_MB_GetSize(mb));

    if (g_rtsplive && g_rtsp_session) {
        rtsp_tx_video(g_rtsp_session, (const uint8_t *) RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
                      RK_MPI_MB_GetTimestamp(mb));
        rtsp_do_event(g_rtsplive);
    }

    RK_MPI_MB_ReleaseBuffer(mb);
    packet_cnt++;
}

// H264 codec
int create_venc_chn(RK_U32 fps, RK_U32 u32Width, RK_U32 u32Height, RK_U32 vencChn) {

    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));

    int ret = 0;
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = fps;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = u32Width * u32Height;

    // frame rate: in 30/1, out 30/1.
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = fps;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = fps;

    venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
    venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
    venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
    venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
    venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
    venc_chn_attr.stVencAttr.u32Profile = 77;  // main profile

    ret = RK_MPI_VENC_CreateChn(vencChn, &venc_chn_attr);
    if (ret) {
        printf("ERROR: create VENC[%d] error! ret=%d\n", vencChn, ret);
        return -1;
    }
    return 0;
}

#if 0
int create_vi(RK_U32 u32Width, RK_U32 u32Height) {
    RK_S32 ret = 0;

    printf("init mpi\n");

    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = "rkispp_scale0";
    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = u32Width;
    vi_chn_attr.u32Height = u32Height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    ret = RK_MPI_VI_SetChnAttr(0, DRAW_RESULT_BOX_CHN_INDEX, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(s32CamId, DRAW_RESULT_BOX_CHN_INDEX);
    if (ret) {
        printf("ERROR: create VI[%d] error! ret=%d\n", DRAW_RESULT_BOX_CHN_INDEX, ret);
        return -1;
    }
    return 0;
}
#endif

int create_vi(RK_U32 u32Width, RK_U32 u32Height, const RK_CHAR *pVideoNode, RK_S32 viChn) {
    RK_S32 ret = 0;

    printf("init mpi\n");

    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = pVideoNode;
    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = u32Width;
    vi_chn_attr.u32Height = u32Height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    ret = RK_MPI_VI_SetChnAttr(0, viChn, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(s32CamId, viChn);
    if (ret) {
        printf("ERROR: create VI[%d] error! ret=%d\n", viChn, ret);
        return -1;
    }
    return 0;
}

// there are 2 way to get stream
// 1 is bind vi to venc
// 2 is start stream and get buffer
int bind_vi_venc() {
    MPP_CHN_S stSrcChn;
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = s32CamId;
    stSrcChn.s32ChnId = DRAW_RESULT_BOX_CHN_INDEX;

    MPP_CHN_S stDestChn;
    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32ChnId = DRAW_RESULT_BOX_CHN_INDEX;

    RK_S32 ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret) {
        printf("ERROR: Bind VI[0] and VENC[0] error! ret=%d\n", ret);
        return -1;
    }
    return 0;
}

int reg_venc_cb() {

    MPP_CHN_S stVenChn;
    memset(&stVenChn, 0, sizeof(MPP_CHN_S));
    stVenChn.enModId = RK_ID_VENC;
    stVenChn.s32ChnId = DRAW_RESULT_BOX_CHN_INDEX;

    // 注册数据输出回调。与RK_MPI_SYS_GetMediaBuffer相比，无需缓存buffer等待用户索取，因此更节省内存。
    RK_S32 ret = RK_MPI_SYS_RegisterOutCb(&stVenChn, venc_packet_cb);
    if (ret) {
        printf("ERROR: register output callback for VENC[0] error! ret=%d\n", ret);
        return -1;
    }

    return 0;

}

void init_rtsp(void) {
    printf("init rtsp\n");
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/main_stream");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());
}

int create_rga_chn(RK_S32 rgaChn, RK_U32 u32WidthIn, RK_U32 u32HeightIn, RK_U32 u32WidthOut, RK_U32 u32HeightOut) {
    RGA_ATTR_S stRgaAttr;
    memset(&stRgaAttr, 0, sizeof(stRgaAttr));
    stRgaAttr.bEnBufPool = RK_TRUE;
    stRgaAttr.u16BufPoolCnt = 3;
    stRgaAttr.stImgIn.imgType = IMAGE_TYPE_NV12;
    stRgaAttr.u16Rotaion = 0;
    stRgaAttr.stImgIn.u32X = (u32WidthIn - u32WidthOut) / 2;
    stRgaAttr.stImgIn.u32Y = (u32HeightIn - u32HeightOut) / 2;

    stRgaAttr.stImgIn.u32Width = u32WidthOut;
    stRgaAttr.stImgIn.u32Height = u32HeightOut;
    stRgaAttr.stImgIn.u32HorStride = u32WidthIn;
    stRgaAttr.stImgIn.u32VirStride = u32HeightIn;

    stRgaAttr.stImgOut.u32X = 0;
    stRgaAttr.stImgOut.u32Y = 0;
    stRgaAttr.stImgOut.imgType = IMAGE_TYPE_NV12;
    stRgaAttr.stImgOut.u32Width = u32WidthOut;
    stRgaAttr.stImgOut.u32Height = u32HeightOut;
    stRgaAttr.stImgOut.u32HorStride = u32WidthOut; // disp_width;
    stRgaAttr.stImgOut.u32VirStride = u32HeightOut; // disp_height;
    // ret = RK_MPI_RGA_SetChnAttr(rgaChn, &stRgaAttr);
    RK_S32 ret = RK_MPI_RGA_CreateChn(rgaChn, &stRgaAttr);

    if (ret) {
        printf("ERROR: RGA Set Attr: ImgIn:<%u,%u,%u,%u> "
               "ImgOut:<%u,%u,%u,%u>, rotation=%u failed! ret=%d\n",
               stRgaAttr.stImgIn.u32X, stRgaAttr.stImgIn.u32Y,
               stRgaAttr.stImgIn.u32Width, stRgaAttr.stImgIn.u32Height,
               stRgaAttr.stImgOut.u32X, stRgaAttr.stImgOut.u32Y,
               stRgaAttr.stImgOut.u32Width, stRgaAttr.stImgOut.u32Height,
               stRgaAttr.u16Rotaion, ret);
        return -1;
    }
    return 0;
}

int bind_vi_rga(RK_S32 cameraId, RK_U32 viChnId, RK_U32 rgaChnId) {

    RK_S32 ret = 0;

    MPP_CHN_S stSrcChn;
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = cameraId;
    stSrcChn.s32ChnId = viChnId;

    MPP_CHN_S stDestChn;
    stDestChn.enModId = RK_ID_RGA;
    stDestChn.s32ChnId = rgaChnId;

    ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret != 0) {
        printf("ERROR: Bind vi[%d] and rga[%d] failed! ret=%d\n", viChnId, rgaChnId, ret);
        return -1;
    }

    return 0;

}

int main(int argc, char **argv) {

    RK_U32 u32Width = 1920;
    RK_U32 u32Height = 1080;

    RK_U32 u32VC1Width = 1280;
    RK_U32 u32VC1Height = 720;

    signal(SIGINT, sig_proc);

    int ret = 0;
    if (argc < 3) {
        printf("please input model name and rtsp config file\n");
        return -1;
    }

    yoloModelFilePath = argv[1];

    ret = init_model(yoloModelFilePath);

    if (ret < 0) {
        printf("init model failed\n");
        return -1;
    }

    printf("xml dirpath: %s\n\n", pIqfilesPath);
    printf("#bMultictx: %d\n\n", bMultictx);

    init_isp();

    init_rtsp();

    RK_MPI_SYS_Init();

    // create vi for both channel
    ret = create_vi(u32Width, u32Height, "rkispp_scale0", DRAW_RESULT_BOX_CHN_INDEX);
    if (ret < 0) {
        printf("create_vi %d failed\n", DRAW_RESULT_BOX_CHN_INDEX);
        return -1;
    }

    ret = create_vi(u32VC1Width, u32VC1Height, "rkispp_scale1", RK_NN_RGA_CHN_INDEX);
    if (ret < 0) {
        printf("create_vi %d cb\n", RK_NN_RGA_CHN_INDEX);
        return -1;
    }

    ret = create_rga_chn(RK_NN_RGA_CHN_INDEX, u32VC1Width,
                         u32VC1Height, MODEL_INPUT_SIZE, MODEL_INPUT_SIZE);
    if (ret < 0) {
        printf("create_rga_chn failed ret:%d\n", ret);
        return -1;
    }

    // bind vi chn 1 to rga chn 1 after binding, can get buffer from rga
    bind_vi_rga(s32CamId, RK_NN_RGA_CHN_INDEX, RK_NN_RGA_CHN_INDEX);

    // create venc channel for vi chn 0
    ret = create_venc_chn(30, u32Width, u32Height, DRAW_RESULT_BOX_CHN_INDEX);
    if (ret < 0) {
        printf("create_venc_chn %d\n", DRAW_RESULT_BOX_CHN_INDEX);
        return -1;
    }

    ret = reg_venc_cb();
    if (ret < 0) {
        printf("reg venc cb\n");
        return -1;
    }

    // bind_vi_venc();
    // turn on the vi output
    ret = RK_MPI_VI_StartStream(s32CamId, DRAW_RESULT_BOX_CHN_INDEX);
    if (ret < 0) {
        printf("RK_MPI_VI_StartStream failed\n");
        return -1;
    }

    create_rknn_list(&rknn_list_);

    pthread_create(&rknn_yolo_thread_id, NULL, rknn_yolo_thread, NULL);

    // get the stream from vi and put in venc
    pthread_create(&observer_thread_id, NULL, observer_thread, NULL);

    // main_vi_process();
    while (g_flag_run) {
        usleep(1000);
    }

    // if goes here mean the program is going to exit
    destroy_all();

    return 0;
}

#if 0
int main__(int argc, char **argv) {

    signal(SIGINT, sig_proc);

    int ret = 0;
    if (argc < 3) {
        printf("please input model name and rtsp config file\n");
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));

    load_cfg(argv[2]);

    yoloModelFilePath = argv[1];

    ret = init_model(yoloModelFilePath);

    if (ret < 0) {
        printf("init model failed\n");
        return -1;
    }

    printf("xml dirpath: %s\n\n", pIqfilesPath);
    printf("#bMultictx: %d\n\n", bMultictx);

    init_isp();

    // init rtsp
    printf("init rtsp\n");

    g_rtsplive = create_rtsp_demo(554);

    // init mpi
    printf("init mpi\n");
    RK_MPI_SYS_Init();

    // init 2 sessions
    init_sessions();

    // get rknn init
    create_rknn_list(&rknn_list_);


    MPP_CHN_S stEncChn;
    stEncChn.enModId = RK_ID_VENC;
    stEncChn.s32ChnId = DRAW_RESULT_BOX_CHN_INDEX;


    // 注册数据输出回调。与RK_MPI_SYS_GetMediaBuffer相比，无需缓存buffer等待用户索取，因此更节省内存。
    ret = RK_MPI_SYS_RegisterOutCb(&stEncChn, venc_packet_cb);
    if (ret) {
        printf("ERROR: register output callback for VENC[0] error! ret=%d\n", ret);
        return 0;
    }

    // Get the sub-stream buffer for humanoid recognition
    pthread_create(&rknn_yolo_thread_id, NULL, rknn_yolo_thread, NULL);

    // The mainstream draws a box asynchronously based on the recognition result
    pthread_create(&observer_thread_id, NULL, observer_thread, NULL);


    // pthread_create(&get_rga_buffer_thread_id, NULL, get_rga_buffer_thread, NULL);

    // main thread will loop here
    // main_vi_process();
    while (g_flag_run) {
        usleep(1000);
    }
    // if goes here mean the program is going to exit
    destroy_all();

    return 0;

}
#endif
