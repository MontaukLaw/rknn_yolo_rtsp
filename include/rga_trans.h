#ifndef __RGA_TRANS_H_
#define __RGA_TRANS_H_

// #include "comm.h"

#ifdef __cplusplus
extern "C" {
#endif

  // int nv12_to_rgb24_640x640(void* yuvBuffer, void* rgbBuffer);

  void trans_data_for_yolo_input(unsigned char* rga_buffer_model_input,
    struct demo_cfg cfg, MEDIA_BUFFER buffer);

#ifdef __cplusplus
}
#endif

#endif
