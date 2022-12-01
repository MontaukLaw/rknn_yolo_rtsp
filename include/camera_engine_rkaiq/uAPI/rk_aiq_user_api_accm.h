/*
 *  Copyright (c) 2019 Rockchip Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RK_AIQ_USER_API_ACCM_H_
#define _RK_AIQ_USER_API_ACCM_H_

#include "accm/rk_aiq_uapi_accm_int.h"

typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;

RKAIQ_BEGIN_DECLARE

XCamReturn
rk_aiq_user_api_accm_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_attrib_t attr);
XCamReturn
rk_aiq_user_api_accm_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_attrib_t *attr);
XCamReturn
rk_aiq_user_api_accm_QueryCcmInfo(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_ccm_querry_info_t *ccm_querry_info);

RKAIQ_END_DECLARE

#endif
