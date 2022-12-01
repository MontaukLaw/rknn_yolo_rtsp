/*
 * rk_aiq_types_accm_hw.h
 *
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

#ifndef _RK_AIQ_TYPES_ACCM_HW_H_
#define _RK_AIQ_TYPES_ACCM_HW_H_

#include "rk_aiq_comm.h"

RKAIQ_BEGIN_DECLARE
#define CCM_CURVE_DOT_NUM 17

typedef struct rk_aiq_ccm_cfg_s {
    bool ccmEnable;
    float  matrix[9];
    float  offs[3];
    float  alp_y[CCM_CURVE_DOT_NUM];
    float bound_bit;
    float rgb2y_para[3];
} rk_aiq_ccm_cfg_t;


RKAIQ_END_DECLARE

#endif

