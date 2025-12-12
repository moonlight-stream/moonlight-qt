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

#ifndef NVSDK_NGX_HELPERS_VSR_VK_H
#define NVSDK_NGX_HELPERS_VSR_VK_H
#pragma once

#include "nvsdk_ngx_defs_vsr.h"
#include "nvsdk_ngx_helpers_vk.h"

typedef struct NVSDK_NGX_VK_VSR_Eval_Params
{
    NVSDK_NGX_Resource_VK*              pInput;
    NVSDK_NGX_Resource_VK*              pOutput;
    NVSDK_NGX_Coordinates               InputSubrectBase;
    NVSDK_NGX_Dimensions                InputSubrectSize;
    NVSDK_NGX_Coordinates               OutputSubrectBase;
    NVSDK_NGX_Dimensions                OutputSubrectSize;
    NVSDK_NGX_VSR_QualityLevel          QualityLevel;
} NVSDK_NGX_VK_VSR_Eval_Params;

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_VSR_EXT1(
    VkDevice InDevice,
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInVSRCreateParams)
{

    if (InDevice) return NVSDK_NGX_VULKAN_CreateFeature1(InDevice, InCmdList, NVSDK_NGX_Feature_VSR, pInParams, ppOutHandle);
    else return NVSDK_NGX_VULKAN_CreateFeature(InCmdList, NVSDK_NGX_Feature_VSR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_VSR_EXT(
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInVSRCreateParams)
{
    return NGX_VULKAN_CREATE_VSR_EXT1(NULL, InCmdList, InCreationNodeMask, InVisibilityNodeMask, ppOutHandle, pInParams, pInVSRCreateParams);
}

static inline NVSDK_NGX_Result NGX_VULKAN_EVALUATE_VSR_EXT(
    VkCommandBuffer InCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_VK_VSR_Eval_Params *pInVSREvalParams)
{
	NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInVSREvalParams->pInput);
	NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInVSREvalParams->pOutput);

    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input1, pInVSREvalParams->pInput);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output, pInVSREvalParams->pOutput);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pInVSREvalParams->InputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pInVSREvalParams->InputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pInVSREvalParams->InputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pInVSREvalParams->InputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_X, pInVSREvalParams->OutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_Y, pInVSREvalParams->OutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_W, pInVSREvalParams->OutputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_H, pInVSREvalParams->OutputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VSR_QualityLevel, pInVSREvalParams->QualityLevel);
    return NVSDK_NGX_VULKAN_EvaluateFeature_C(InCmdList, pInHandle, pInParams, NULL);
}

#endif // NVSDK_NGX_HELPERS_VSR_VK_H
