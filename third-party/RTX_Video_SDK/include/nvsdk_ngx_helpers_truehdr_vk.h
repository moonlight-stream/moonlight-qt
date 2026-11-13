#pragma once
/*
* Copyright (c) 2021 NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software, related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is strictly
* prohibited.
*
* TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
* SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
* LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
* BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
* INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGES.
*/


#ifndef NVSDK_NGX_TRUEHDR_VK_H
#define NVSDK_NGX_TRUEHDR_VK_H

#include "nvsdk_ngx_defs_truehdr.h"
#include "nvsdk_ngx_helpers_vk.h"

typedef struct NVSDK_NGX_VK_TRUEHDR_Eval_Params
{
    NVSDK_NGX_Resource_VK*              pInput;
    NVSDK_NGX_Resource_VK*              pOutput;
    NVSDK_NGX_Coordinates               InputSubrectTL;
    NVSDK_NGX_Dimensions                InputSubrectBR;
    NVSDK_NGX_Coordinates               OutputSubrectTL;
    NVSDK_NGX_Dimensions                OutputSubrectBR;
    unsigned int                        Contrast;
    unsigned int                        Saturation;
    unsigned int                        MiddleGray;
    unsigned int                        MaxLuminance;
} NVSDK_NGX_VK_TRUEHDR_Eval_Params;

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_TRUEHDR_EXT1(
    VkDevice InDevice,
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pTrueHDRCreateParams)
{

    if (InDevice) return NVSDK_NGX_VULKAN_CreateFeature1(InDevice, InCmdList, NVSDK_NGX_Feature_TrueHDR, pInParams, ppOutHandle);
    else return NVSDK_NGX_VULKAN_CreateFeature(InCmdList, NVSDK_NGX_Feature_TrueHDR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VULKAN_EVALUATE_TRUEHDR_EXT1(
    VkCommandBuffer InCmdList,
    NVSDK_NGX_Handle* pInHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_VK_TRUEHDR_Eval_Params* pTrueHDREvalParams)
{
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input1,       pTrueHDREvalParams->pInput);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output,       pTrueHDREvalParams->pOutput);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_InLeft,        pTrueHDREvalParams->InputSubrectTL.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_InTop,         pTrueHDREvalParams->InputSubrectTL.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_InRight,       pTrueHDREvalParams->InputSubrectBR.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_InBottom,      pTrueHDREvalParams->InputSubrectBR.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_OutLeft,       pTrueHDREvalParams->OutputSubrectTL.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_OutTop,        pTrueHDREvalParams->OutputSubrectTL.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_OutRight,      pTrueHDREvalParams->OutputSubrectBR.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_OutBottom,     pTrueHDREvalParams->OutputSubrectBR.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_Contrast,      pTrueHDREvalParams->Contrast);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_Saturation,    pTrueHDREvalParams->Saturation);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_MiddleGray,    pTrueHDREvalParams->MiddleGray);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TrueHDR_MaxLuminance,  pTrueHDREvalParams->MaxLuminance);

    return NVSDK_NGX_VULKAN_EvaluateFeature_C(InCmdList, pInHandle, pInParams, NULL);
}


#endif // NVSDK_NGX_TRUEHDR_VK_H
