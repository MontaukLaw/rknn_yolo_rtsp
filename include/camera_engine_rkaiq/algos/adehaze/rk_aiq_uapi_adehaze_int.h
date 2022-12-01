#ifndef _RK_AIQ_UAPI_ADEHAZE_INT_H_
#define _RK_AIQ_UAPI_ADEHAZE_INT_H_

#include "base/xcam_common.h"
#include "rk_aiq_algo_des.h"
#include "adehaze/rk_aiq_types_adehaze_algo_int.h"

// need_sync means the implementation should consider
// the thread synchronization
// if called by RkAiqAwbHandleInt, the sync has been done
// in framework. And if called by user app directly,
// sync should be done in inner. now we just need implement
// the case of need_sync == false; need_sync is for future usage.

XCamReturn
rk_aiq_uapi_adehaze_SetAttrib(RkAiqAlgoContext *ctx,
                              adehaze_sw_t attr,
                              bool need_sync);

XCamReturn
rk_aiq_uapi_adehaze_GetAttrib(RkAiqAlgoContext *ctx,
                              adehaze_sw_t *attr);

#endif

