/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef NVSDK_NGX_HELPERS_VSR_H
#define NVSDK_NGX_HELPERS_VSR_H
#pragma once

#include "nvsdk_ngx_defs_vsr.h"
#include "nvsdk_ngx_helpers.h"

typedef struct NVSDK_NGX_D3D11_VSR_Eval_Params
{
    ID3D11Resource*                     pInput;
    ID3D11Resource*                     pOutput;
    NVSDK_NGX_Coordinates               InputSubrectBase;
    NVSDK_NGX_Dimensions                InputSubrectSize;
    NVSDK_NGX_Coordinates               OutputSubrectBase;
    NVSDK_NGX_Dimensions                OutputSubrectSize;
    NVSDK_NGX_VSR_QualityLevel          QualityLevel;
} NVSDK_NGX_D3D11_VSR_Eval_Params;

static inline NVSDK_NGX_Result NGX_D3D11_CREATE_VSR_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInVSRCreateParams)
{
    return NVSDK_NGX_D3D11_CreateFeature(pInCtx, NVSDK_NGX_Feature_VSR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_D3D11_EVALUATE_VSR_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D11_VSR_Eval_Params *pInVSREvalParams)
{
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Input1, pInVSREvalParams->pInput);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Output, pInVSREvalParams->pOutput);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pInVSREvalParams->InputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pInVSREvalParams->InputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pInVSREvalParams->InputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pInVSREvalParams->InputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_X, pInVSREvalParams->OutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_Y, pInVSREvalParams->OutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_W, pInVSREvalParams->OutputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_H, pInVSREvalParams->OutputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VSR_QualityLevel, pInVSREvalParams->QualityLevel);
    return NVSDK_NGX_D3D11_EvaluateFeature_C(pInCtx, pInHandle, pInParams, NULL);
}

typedef struct NVSDK_NGX_D3D12_VSR_Eval_Params
{
    ID3D12Resource*                     pInput;
    ID3D12Resource*                     pOutput;
    NVSDK_NGX_Coordinates               InputSubrectBase;
    NVSDK_NGX_Dimensions                InputSubrectSize;
    NVSDK_NGX_Coordinates               OutputSubrectBase;
    NVSDK_NGX_Dimensions                OutputSubrectSize;
    NVSDK_NGX_VSR_QualityLevel          QualityLevel;
} NVSDK_NGX_D3D12_VSR_Eval_Params;

static inline NVSDK_NGX_Result NGX_D3D12_CREATE_VSR_EXT(
    ID3D12GraphicsCommandList *InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInVSRCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    return NVSDK_NGX_D3D12_CreateFeature(InCmdList, NVSDK_NGX_Feature_VSR, pInParams, ppOutHandle);
}
static inline NVSDK_NGX_Result NGX_D3D12_EVALUATE_VSR_EXT(
    ID3D12GraphicsCommandList *pInCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D12_VSR_Eval_Params *pInVSREvalParams)
{
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Input1, pInVSREvalParams->pInput);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Output, pInVSREvalParams->pOutput);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pInVSREvalParams->InputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pInVSREvalParams->InputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pInVSREvalParams->InputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pInVSREvalParams->InputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_X, pInVSREvalParams->OutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_Y, pInVSREvalParams->OutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_W, pInVSREvalParams->OutputSubrectSize.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutRect_H, pInVSREvalParams->OutputSubrectSize.Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VSR_QualityLevel, pInVSREvalParams->QualityLevel);
    return NVSDK_NGX_D3D12_EvaluateFeature_C(pInCmdList, pInHandle, pInParams, NULL);
}


typedef struct NVSDK_NGX_CUDA_VSR_Create_Params
{
    NVSDK_NGX_Feature_Create_Params     Feature;
    void*                               InCUContext;
    void*                               InCUStream;
} NVSDK_NGX_CUDA_VSRCreate_Params;

typedef struct NVSDK_NGX_CUDA_VSR_Eval_Params
{
    void*                               pInput;
    void*                               pOutput;
    NVSDK_NGX_Coordinates               InputSubrectBase;
    NVSDK_NGX_Dimensions                InputSubrectSize;
    NVSDK_NGX_Coordinates               OutputSubrectBase;
    NVSDK_NGX_Dimensions                OutputSubrectSize;
    NVSDK_NGX_VSR_QualityLevel          QualityLevel;
} NVSDK_NGX_CUDA_VSR_Eval_Params;

static inline NVSDK_NGX_Result NGX_CUDA_CREATE_VSR(
    NVSDK_NGX_Handle** ppOutHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_CUDA_VSRCreate_Params* pInVSRCreateParams)
{
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input1, pInVSRCreateParams->InCUContext);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input2, pInVSRCreateParams->InCUStream);
    return NVSDK_NGX_CUDA_CreateFeature(NVSDK_NGX_Feature_VSR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_CUDA_EVALUATE_VSR(
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_CUDA_VSR_Eval_Params *pInVSREvalParams)
{
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
    return NVSDK_NGX_CUDA_EvaluateFeature_C(pInHandle, pInParams, NULL);
}

#endif // NVSDK_NGX_HELPERS_VSR_H
