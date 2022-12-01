/*
 *rk_aiq_types_alsc_algo_int.h
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

#ifndef _RK_AIQ_TYPE_ALSC_ALGO_INT_H_
#define _RK_AIQ_TYPE_ALSC_ALGO_INT_H_
#include "alsc/rk_aiq_types_alsc_algo.h"


RKAIQ_BEGIN_DECLARE

//add more here
typedef struct alsc_sw_info_s {
    float sensorGain;
    float awbGain[2];
    float awbIIRDampCoef;
    float varianceLuma;
    bool grayMode;
    bool awbConverged;
    int prepare_type;
} alsc_sw_info_t;

typedef struct rk_aiq_lsc_mlsc_attrib_s {
    unsigned short r_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gr_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gb_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short b_data_tbl[LSC_DATA_TBL_SIZE];

} rk_aiq_lsc_mlsc_attrib_t;

typedef enum rk_aiq_lsc_op_mode_s {
    RK_AIQ_LSC_MODE_INVALID                     = 0,        /**< initialization value */
    RK_AIQ_LSC_MODE_MANUAL                      = 1,        /**< run manual lens shading correction */
    RK_AIQ_LSC_MODE_AUTO                        = 2,        /**< run auto lens shading correction */
    RK_AIQ_LSC_MODE_MAX
} rk_aiq_lsc_op_mode_t;


typedef struct rk_aiq_lsc_attrib_s {
    bool byPass;
    rk_aiq_lsc_op_mode_t mode;
    rk_aiq_lsc_mlsc_attrib_t stManual;

} rk_aiq_lsc_attrib_t;

typedef struct rk_aiq_lsc_querry_info_s {
    bool lsc_en;
    unsigned short r_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gr_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gb_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short b_data_tbl[LSC_DATA_TBL_SIZE];
} rk_aiq_lsc_querry_info_t;

RKAIQ_END_DECLARE

#endif

