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

#ifndef NVSDK_NGX_HELPERS_TRUEHDR_H
#define NVSDK_NGX_HELPERS_TRUEHDR_H
#pragma once

#include "nvsdk_ngx_helpers.h"
#include "nvsdk_ngx_defs_truehdr.h"

typedef struct NVSDK_NGX_D3D11_TRUEHDR_Eval_Params
{
    ID3D11Resource*                     pInput;
    ID3D11Resource*                     pOutput;
    NVSDK_NGX_Coordinates               InputSubrectTL;
    NVSDK_NGX_Dimensions                InputSubrectBR;
    NVSDK_NGX_Coordinates               OutputSubrectTL;
    NVSDK_NGX_Dimensions                OutputSubrectBR;
    unsigned int                        Contrast;
    unsigned int                        Saturation;
    unsigned int                        MiddleGray;
    unsigned int                        MaxLuminance;
} NVSDK_NGX_D3D11_TRUEHDR_Eval_Params;

typedef struct NVSDK_NGX_D3D12_TRUEHDR_Eval_Params
{
    ID3D12Resource*                     pInput;
    ID3D12Resource*                     pOutput;
    NVSDK_NGX_Coordinates               InputSubrectTL;
    NVSDK_NGX_Dimensions                InputSubrectBR;
    NVSDK_NGX_Coordinates               OutputSubrectTL;
    NVSDK_NGX_Dimensions                OutputSubrectBR;
    unsigned int                        Contrast;
    unsigned int                        Saturation;
    unsigned int                        MiddleGray;
    unsigned int                        MaxLuminance;
} NVSDK_NGX_D3D12_TRUEHDR_Eval_Params;

typedef struct NVSDK_NGX_CUDA_TRUEHDR_Create_Params
{
    NVSDK_NGX_Feature_Create_Params     Feature;
    void*                               InCUContext;
    void*                               InCUStream;
} NVSDK_NGX_CUDA_TRUEHDR_Create_Params;

typedef struct NVSDK_NGX_CUDA_TRUEHDR_Eval_Params
{
    void*                               pInput;
    void*                               pOutput;
    NVSDK_NGX_Coordinates               InputSubrectTL;
    NVSDK_NGX_Dimensions                InputSubrectBR;
    NVSDK_NGX_Coordinates               OutputSubrectTL;
    NVSDK_NGX_Dimensions                OutputSubrectBR;
    unsigned int                        Contrast;
    unsigned int                        Saturation;
    unsigned int                        MiddleGray;
    unsigned int                        MaxLuminance;
} NVSDK_NGX_CUDA_TRUEHDR_Eval_Params;

static inline NVSDK_NGX_Result NGX_D3D11_CREATE_TRUEHDR_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pTrueHDRCreateParams)
{
    return NVSDK_NGX_D3D11_CreateFeature(pInCtx, NVSDK_NGX_Feature_TrueHDR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_D3D12_CREATE_TRUEHDR_EXT(
    ID3D12GraphicsCommandList* InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle** ppOutHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_Feature_Create_Params* pTrueHDRCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    return NVSDK_NGX_D3D12_CreateFeature(InCmdList, NVSDK_NGX_Feature_TrueHDR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_CUDA_CREATE_TRUEHDR(
    NVSDK_NGX_Handle** ppOutHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_CUDA_TRUEHDR_Create_Params* pTrueHDRCreateParams)
{
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input1, pTrueHDRCreateParams->InCUContext);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Input2, pTrueHDRCreateParams->InCUStream);
    return NVSDK_NGX_CUDA_CreateFeature(NVSDK_NGX_Feature_TrueHDR, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_D3D11_EVALUATE_TRUEHDR_EXT(
    ID3D11DeviceContext *pInCtx,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D11_TRUEHDR_Eval_Params *pTrueHDREvalParams)
{
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Input1,     pTrueHDREvalParams->pInput);
    NVSDK_NGX_Parameter_SetD3d11Resource(pInParams, NVSDK_NGX_Parameter_Output,     pTrueHDREvalParams->pOutput);
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
    return NVSDK_NGX_D3D11_EvaluateFeature_C(pInCtx, pInHandle, pInParams, NULL);
}


static inline NVSDK_NGX_Result NGX_D3D12_EVALUATE_TRUEHDR_EXT(
    ID3D12GraphicsCommandList *pInCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_D3D12_TRUEHDR_Eval_Params *pTrueHDREvalParams)
{
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Input1,     pTrueHDREvalParams->pInput);
    NVSDK_NGX_Parameter_SetD3d12Resource(pInParams, NVSDK_NGX_Parameter_Output,     pTrueHDREvalParams->pOutput);
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
    return NVSDK_NGX_D3D12_EvaluateFeature_C(pInCmdList, pInHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_CUDA_EVALUATE_TRUEHDR(
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_CUDA_TRUEHDR_Eval_Params *pTrueHDREvalParams)
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
    return NVSDK_NGX_CUDA_EvaluateFeature_C(pInHandle, pInParams, NULL);
}

#endif // NVSDK_NGX_HELPERS_TRUEHDR_H
