/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef NVSDK_NGX_DEFS_VSR_H
#define NVSDK_NGX_DEFS_VSR_H
#pragma once

#include "nvsdk_ngx_defs.h"

#define NVSDK_NGX_EParameter_VSR_Available NVSDK_NGX_EParameter_Reserved_49

constexpr NVSDK_NGX_Feature NVSDK_NGX_Feature_VSR = NVSDK_NGX_Feature_Reserved16;

typedef enum NVSDK_NGX_VSR_QualityLevel
{
    NVSDK_NGX_VSR_Quality_Bicubic = 0,
    NVSDK_NGX_VSR_Quality_Low     = 1,
    NVSDK_NGX_VSR_Quality_Medium  = 2,
    NVSDK_NGX_VSR_Quality_High    = 3,
    NVSDK_NGX_VSR_Quality_Ultra   = 4,
} NVSDK_NGX_VSR_QualityLevel;

#define NVSDK_NGX_Parameter_VSR_Available              "VSR.Available"
#define NVSDK_NGX_Parameter_VSR_NeedsUpdatedDriver     "VSR.NeedsUpdatedDriver"
#define NVSDK_NGX_Parameter_VSR_MinDriverVersionMajor  "VSR.MinDriverVersionMajor"
#define NVSDK_NGX_Parameter_VSR_MinDriverVersionMinor  "VSR.MinDriverVersionMinor"
#define NVSDK_NGX_Parameter_VSR_FeatureInitResult      "VSR.FeatureInitResult"
#define NVSDK_NGX_Parameter_VSR_QualityLevel           "VSR.QualityLevel"

#endif // NVSDK_NGX_DEFS_VSR_H
