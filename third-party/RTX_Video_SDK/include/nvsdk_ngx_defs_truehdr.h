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

#ifndef NVSDK_NGX_DEFS_TRUEHDR_H
#define NVSDK_NGX_DEFS_TRUEHDR_H
#pragma once

#include "nvsdk_ngx_defs.h"

#define NVSDK_NGX_EParameter_TrueHDR_Avalilable NVSDK_NGX_EParameter_Reserved_48

constexpr NVSDK_NGX_Feature NVSDK_NGX_Feature_TrueHDR = NVSDK_NGX_Feature_Reserved14;

#define NVSDK_NGX_Parameter_TrueHDR_Available               "TrueHDR.Available"
#define NVSDK_NGX_Parameter_TrueHDR_NeedsUpdatedDriver      "TrueHDR.NeedsUpdatedDriver"
#define NVSDK_NGX_Parameter_TrueHDR_MinDriverVersionMajor   "TrueHDR.MinDriverVersionMajor"
#define NVSDK_NGX_Parameter_TrueHDR_MinDriverVersionMinor   "TrueHDR.MinDriverVersionMinor"
#define NVSDK_NGX_Parameter_TrueHDR_FeatureInitResult       "TrueHDR.FeatureInitResult"

#define NVSDK_NGX_Parameter_TrueHDR_InLeft                  "TrueHDR.InLeft"            // Input  left   pixel for source rect
#define NVSDK_NGX_Parameter_TrueHDR_InTop                   "TrueHDR.InTop"             // Input  top    pixel for source rect
#define NVSDK_NGX_Parameter_TrueHDR_InRight                 "TrueHDR.InRight"           // Input  right  pixel for source rect
#define NVSDK_NGX_Parameter_TrueHDR_InBottom                "TrueHDR.InBottom"          // Input  bottom pixel for source rect
#define NVSDK_NGX_Parameter_TrueHDR_OutLeft                 "TrueHDR.OutLeft"           // Output left   pixel for dest rect
#define NVSDK_NGX_Parameter_TrueHDR_OutTop                  "TrueHDR.OutTop"            // Output top    pixel for dest rect
#define NVSDK_NGX_Parameter_TrueHDR_OutRight                "TrueHDR.OutRight"          // Output right  pixel for dest rect
#define NVSDK_NGX_Parameter_TrueHDR_OutBottom               "TrueHDR.OutBottom"         // Output bottom pixel for dest rect
#define NVSDK_NGX_Parameter_TrueHDR_Contrast                "TrueHDR.Contrast"          // 0 to 200 for HDR Contrast
#define NVSDK_NGX_Parameter_TrueHDR_Saturation              "TrueHDR.Saturation"        // 0 to 200 for HDR Saturation
#define NVSDK_NGX_Parameter_TrueHDR_MiddleGray              "TrueHDR.MiddleGray"        // 10 to 100 for HDR MiddleGray
#define NVSDK_NGX_Parameter_TrueHDR_MaxLuminance            "TrueHDR.MaxLuminance"      // 400 to 2000 for Monitor MaxLuminance

#endif // NVSDK_NGX_DEFS_TRUEHDR_H
