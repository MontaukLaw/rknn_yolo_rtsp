// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/sample_common.h"
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef RKAIQ

#define MAX_AIQ_CTX 4
static rk_aiq_sys_ctx_t *g_aiq_ctx[MAX_AIQ_CTX];
rk_aiq_working_mode_t g_WDRMode[MAX_AIQ_CTX];

RK_S32 SAMPLE_COMM_ISP_Init(RK_S32 CamId, rk_aiq_working_mode_t WDRMode,
                            RK_BOOL MultiCam, const char *iq_file_dir)
{
  if (CamId >= MAX_AIQ_CTX)
  {
    printf("%s : CamId is over 3\n", __FUNCTION__);
    return -1;
  }
  // char *iq_file_dir = "iqfiles/";
  setlinebuf(stdout);
  if (iq_file_dir == NULL)
  {
    printf("SAMPLE_COMM_ISP_Init : not start.\n");
    g_aiq_ctx[CamId] = NULL;
    return 0;
  }

  // must set HDR_MODE, before init
  g_WDRMode[CamId] = WDRMode;
  char hdr_str[16];
  snprintf(hdr_str, sizeof(hdr_str), "%d", (int)WDRMode);
  setenv("HDR_MODE", hdr_str, 1);

  rk_aiq_sys_ctx_t *aiq_ctx;
  rk_aiq_static_info_t aiq_static_info;
  rk_aiq_uapi_sysctl_enumStaticMetas(CamId, &aiq_static_info);

  printf("ID: %d, sensor_name is %s, iqfiles is %s\n", CamId,
         aiq_static_info.sensor_info.sensor_name, iq_file_dir);

  aiq_ctx = rk_aiq_uapi_sysctl_init(aiq_static_info.sensor_info.sensor_name,
                                    iq_file_dir, NULL, NULL);
  if (MultiCam)
    rk_aiq_uapi_sysctl_setMulCamConc(aiq_ctx, true);

  g_aiq_ctx[CamId] = aiq_ctx;
  return 0;
}

RK_S32 SAMPLE_COMM_ISP_Stop(RK_S32 CamId)
{
  if (CamId >= MAX_AIQ_CTX || !g_aiq_ctx[CamId])
  {
    printf("%s : CamId is over 3 or not init\n", __FUNCTION__);
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_stop enter\n");
  rk_aiq_uapi_sysctl_stop(g_aiq_ctx[CamId], false);
  printf("rk_aiq_uapi_sysctl_deinit enter\n");
  rk_aiq_uapi_sysctl_deinit(g_aiq_ctx[CamId]);
  printf("rk_aiq_uapi_sysctl_deinit exit\n");
  g_aiq_ctx[CamId] = NULL;
  return 0;
}

RK_S32 SAMPLE_COMM_ISP_Run(RK_S32 CamId)
{
  if (CamId >= MAX_AIQ_CTX || !g_aiq_ctx[CamId])
  {
    printf("%s : CamId is over 3 or not init\n", __FUNCTION__);
    return -1;
  }
  if (rk_aiq_uapi_sysctl_prepare(g_aiq_ctx[CamId], 0, 0, g_WDRMode[CamId]))
  {
    printf("rkaiq engine prepare failed !\n");
    g_aiq_ctx[CamId] = NULL;
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_init/prepare succeed\n");
  if (rk_aiq_uapi_sysctl_start(g_aiq_ctx[CamId]))
  {
    printf("rk_aiq_uapi_sysctl_start  failed\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_start succeed\n");
  return 0;
}

RK_S32 SAMPLE_COMM_ISP_SetFrameRate(RK_S32 CamId, RK_U32 uFps)
{
  if (CamId >= MAX_AIQ_CTX || !g_aiq_ctx[CamId])
  {
    printf("%s : CamId is over 3 or not init\n", __FUNCTION__);
    return -1;
  }
  RK_S32 ret = 0;
  printf("SAMPLE_COMM_ISP_SetFrameRate start %d\n", uFps);

  frameRateInfo_t info;
  info.mode = OP_MANUAL;
  info.fps = uFps;
  ret = rk_aiq_uapi_setFrameRate(g_aiq_ctx[CamId], info);

  printf("SAMPLE_COMM_ISP_SetFrameRate %d\n", uFps);
  return ret;
}

#endif
