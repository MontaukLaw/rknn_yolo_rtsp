/*
 *rk_aiq_types_alsc_hw.h
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

#ifndef _RK_AIQ_TYPE_ALSC_HW_H_
#define _RK_AIQ_TYPE_ALSC_HW_H_
#include "rk_aiq_comm.h"
#define LSC_DATA_TBL_V_SIZE         17
#define LSC_DATA_TBL_H_SIZE         17
#define LSC_DATA_TBL_SIZE           289
#define LSC_GRAD_TBL_SIZE           8
#define LSC_SIZE_TBL_SIZE           8


RKAIQ_BEGIN_DECLARE

typedef struct rk_aiq_lsc_cfg_s {
    bool lsc_en;
    unsigned short r_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gr_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short gb_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short b_data_tbl[LSC_DATA_TBL_SIZE];
    unsigned short x_grad_tbl[LSC_GRAD_TBL_SIZE];
    unsigned short y_grad_tbl[LSC_GRAD_TBL_SIZE];
    unsigned short x_size_tbl[LSC_SIZE_TBL_SIZE];
    unsigned short y_size_tbl[LSC_SIZE_TBL_SIZE];
} rk_aiq_lsc_cfg_t;



RKAIQ_END_DECLARE

#endif

