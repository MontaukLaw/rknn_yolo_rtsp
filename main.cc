#include "comm.h"
#include "postprocess.h"
#include "rga_trans.h"
#include "rknn_funcs.h"

// rtsp obj for streaming main_stream and sub_stream
static rtsp_demo_handle g_rtsplive = NULL;
// rknn result list to exchange data between two thread
// its just a simple chain list
static rknn_list* rknn_list_;
// iqfile
static RK_CHAR* pIqfilesPath = (RK_CHAR*)"/oem/etc/iqfiles/";
static RK_S32 s32CamId = 0;
static RK_BOOL bMultictx = RK_FALSE;
static rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
static int g_flag_run = 1;

static pthread_t observer_thread_id;
static pthread_t get_rga_buffer_thread_id;
static pthread_t rknn_yolo_thread_id;

static struct demo_cfg cfg;
static char* yoloModelFilePath = NULL;
static int yoloModelSize = 0;
RK_BOOL ifDetecting = RK_FALSE;

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

  while (g_flag_run) {
    // get both session
    for (int i = 0; i < cfg.session_count; i++) {
      MEDIA_BUFFER buffer;
      // get data from venc, then send to rtsp
      buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, cfg.session_cfg[i].stVenChn.s32ChnId, 0);
      if (buffer) {
        rtsp_tx_video(cfg.session_cfg[i].session, (const uint8_t*)RK_MPI_MB_GetPtr(buffer),
          RK_MPI_MB_GetSize(buffer), RK_MPI_MB_GetTimestamp(buffer));

        RK_MPI_MB_ReleaseBuffer(buffer);
      }
    }
    rtsp_do_event(g_rtsplive);
  }
}

static void init_sessions(void) {
  int ret = 0;
  // there are 2 session, one for DRAW box one for RTSP
  for (int i = 0; i < cfg.session_count; i++) {
    cfg.session_cfg[i].session =
      rtsp_new_session(g_rtsplive, cfg.session_cfg[i].path);

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
    }
    else {
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
    }
    else {
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

static int nv12_border(char* pic, int pic_w, int pic_h, int rect_x, int rect_y,
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
      if (k < (rect_x + border) || k >(rect_x + rect_w - border) ||
        j < (rect_y + border) || j >(rect_y + rect_h - border)) {
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

// to draw box and send to rtsp
static void* observer_thread(void* args) {
  MEDIA_BUFFER buffer;
  float xStart = (cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width - MODEL_INPUT_SIZE) / 2;
  float yStart = (cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height - MODEL_INPUT_SIZE) / 2;
  printf("xStart:%f, yStart:%f\n", xStart, yStart);

  int cnt = 0;
  // printf("main stream chn id is %d\n", cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stRgaChn.s32ChnId);
  while (g_flag_run) {
    buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stViChn.s32ChnId, -1);
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
        int x = detect_result_group.results[j].box.left + xStart;
        int y = detect_result_group.results[j].box.top + yStart;
        int w = (detect_result_group.results[j].box.right -
          detect_result_group.results[j].box.left);
        int h = (detect_result_group.results[j].box.bottom -
          detect_result_group.results[j].box.top);
        while ((uint32_t)(x + w) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Width) {
          w -= 16;
        }
        while ((uint32_t)(y + h) >= cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].u32Height) {
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
        nv12_border((char*)RK_MPI_MB_GetPtr(buffer),
          picWidth, picHeight, rotatedX, rotatedY, w, h, 0, 0, 255);
      }
    }
    // print_mb_info(buffer);

    // send from VI to VENC
    RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, cfg.session_cfg[DRAW_RESULT_BOX_CHN_INDEX].stVenChn.s32ChnId, buffer);
    RK_MPI_MB_ReleaseBuffer(buffer);
  }

  return NULL;
}

static int load_cfg(const char* cfg_file) {
  FILE* fp = fopen(cfg_file, "r");
  char line[1024];
  int count = 0;

  if (!fp) {
    fprintf(stderr, "open %s failed\n", cfg_file);
    return -1;
  }

  memset(&cfg, 0, sizeof(cfg));
  while (fgets(line, sizeof(line) - 1, fp)) {
    const char* p;
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
    }
    else {
      printf("parse line %s failed\n", line);
    }
  }
  cfg.session_count = count;
  fclose(fp);
  dump_cfg();
  return count;
}

static int nv12_to_rgb24_640x640(void* yuvBuffer, void* rgbBuffer) {

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
  else {
    printf("imcvtcolor nv12_to_rgb24_640x640 success!\n");
  }

  return 0;
}

// this thread is for rknn
static void* rknn_yolo_thread(void* args) {

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
    print_mb_info(buffer);

    // int rga_buffer_size = cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Width * cfg.session_cfg[RK_NN_INDEX].u32Height * 3; // nv12 3/2, rgb 3
    // int rga_buffer_model_input_size = MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3;
    // auto rga_buffer_model_input = (unsigned char*)malloc(rga_buffer_model_input_size);

    // trans_data_for_yolo_input(rga_buffer_model_input, cfg, buffer);
    void* pRknnInputData = malloc(YOLO_INPUT_SIZE);
    ret = nv12_to_rgb24_640x640(RK_MPI_MB_GetPtr(buffer), pRknnInputData);
    if (ret < 0) {
      printf("nv12_to_rgb24_640x640 failed\n");
    }

    if (ret == 0) {
      detect_result_group_t detect_result_group;
      memset(&detect_result_group, 0, sizeof(detect_result_group_t));

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

static void* get_rga_buffer_thread(void* arg) {
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

int main(int argc, char** argv) {

  signal(SIGINT, sig_proc);

  int ret = 0;
  if (argc < 3) {
    printf("please input model name and rtsp config file\n");
    return -1;
  }

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

  // Get the sub-stream buffer for humanoid recognition
  pthread_create(&rknn_yolo_thread_id, NULL, rknn_yolo_thread, NULL);

  // The mainstream draws a box asynchronously based on the recognition result
  pthread_create(&observer_thread_id, NULL, observer_thread, NULL);

  // pthread_create(&get_rga_buffer_thread_id, NULL, get_rga_buffer_thread, NULL);

  // main thread will loop here
  main_vi_process();

  // if goes here mean the program is going to exit
  destroy_all();

  return 0;

}
