/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef NVSDK_NGX_HELPERS_VK_H
#define NVSDK_NGX_HELPERS_VK_H
#pragma once

#include "nvsdk_ngx_vk.h"

#define NVSDK_NGX_ENSURE_VK_IMAGEVIEW(InResource) if ((InResource) && (InResource)->Type != NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW) { return NVSDK_NGX_Result_FAIL_InvalidParameter; }

static inline NVSDK_NGX_Resource_VK NVSDK_NGX_Create_ImageView_Resource_VK(VkImageView imageView, VkImage image, VkImageSubresourceRange subresourceRange, VkFormat format, unsigned int width, unsigned int height, bool readWrite)
{
    NVSDK_NGX_Resource_VK resourceVK = {};
    resourceVK.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    resourceVK.Resource.ImageViewInfo.ImageView = imageView;
    resourceVK.Resource.ImageViewInfo.Image = image;
    resourceVK.Resource.ImageViewInfo.SubresourceRange = subresourceRange;
    resourceVK.Resource.ImageViewInfo.Height = height;
    resourceVK.Resource.ImageViewInfo.Width = width;
    resourceVK.Resource.ImageViewInfo.Format = format;
    resourceVK.ReadWrite = readWrite;
    return resourceVK;
}

static inline NVSDK_NGX_Resource_VK NVSDK_NGX_Create_Buffer_Resource_VK(VkBuffer buffer, unsigned int sizeInBytes, bool readWrite)
{
    NVSDK_NGX_Resource_VK resourceVK = {};
    resourceVK.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_BUFFER;
    resourceVK.Resource.BufferInfo.Buffer = buffer;
    resourceVK.Resource.BufferInfo.SizeInBytes = sizeInBytes;
    resourceVK.ReadWrite = readWrite;
    return resourceVK;
}

typedef struct NVSDK_NGX_VK_Feature_Eval_Params
{
    NVSDK_NGX_Resource_VK *pInColor;
    NVSDK_NGX_Resource_VK *pInOutput;
    /*** OPTIONAL for DLSS ***/
    float                  InSharpness;
} NVSDK_NGX_VK_Feature_Eval_Params;

typedef struct NVSDK_NGX_VK_GBuffer
{
    NVSDK_NGX_Resource_VK *pInAttrib[NVSDK_NGX_GBUFFERTYPE_NUM];
} NVSDK_NGX_VK_GBuffer;

typedef struct NVSDK_NGX_Coordinates_VK
{
    unsigned int X;
    unsigned int Y;
} NVSDK_NGX_Coordinates_VK;

typedef struct NVSDK_NGX_VK_DLSS_Eval_Params
{
    NVSDK_NGX_VK_Feature_Eval_Params    Feature;
    NVSDK_NGX_Resource_VK *             pInDepth;
    NVSDK_NGX_Resource_VK *             pInMotionVectors;
    float                               InJitterOffsetX;     /* Jitter offset must be in input/render pixel space */
    float                               InJitterOffsetY;
    NVSDK_NGX_Dimensions                InRenderSubrectDimensions;
    /*** OPTIONAL - leave to 0/0.0f if unused ***/
    int                                 InReset;             /* Set to 1 when scene changes completely (new level etc) */
    float                               InMVScaleX;          /* If MVs need custom scaling to convert to pixel space */
    float                               InMVScaleY;
    NVSDK_NGX_Resource_VK *             pInTransparencyMask; /* Unused/Reserved for future use */
    NVSDK_NGX_Resource_VK *             pInExposureTexture;
    NVSDK_NGX_Resource_VK *             pInBiasCurrentColorMask;
    NVSDK_NGX_Coordinates               InColorSubrectBase;
    NVSDK_NGX_Coordinates               InDepthSubrectBase;
    NVSDK_NGX_Coordinates               InMVSubrectBase;
    NVSDK_NGX_Coordinates               InTranslucencySubrectBase;
    NVSDK_NGX_Coordinates               InBiasCurrentColorSubrectBase;
    NVSDK_NGX_Coordinates               InOutputSubrectBase;
    float                               InPreExposure;
    float                               InExposureScale;
    int                                 InIndicatorInvertXAxis;
    int                                 InIndicatorInvertYAxis;
    /*** OPTIONAL - only for research purposes ***/
    NVSDK_NGX_VK_GBuffer                GBufferSurface;
    NVSDK_NGX_ToneMapperType            InToneMapperType;
    NVSDK_NGX_Resource_VK *             pInMotionVectors3D;
    NVSDK_NGX_Resource_VK *             pInIsParticleMask; /* to identify which pixels contains particles, essentially that are not drawn as part of base pass */
    NVSDK_NGX_Resource_VK *             pInAnimatedTextureMask; /* a binary mask covering pixels occupied by animated textures */
    NVSDK_NGX_Resource_VK *             pInDepthHighRes;
    NVSDK_NGX_Resource_VK *             pInPositionViewSpace;
    float                               InFrameTimeDeltaInMsec; /* helps in determining the amount to denoise or anti-alias based on the speed of the object from motion vector magnitudes and fps as determined by this delta */
    NVSDK_NGX_Resource_VK *             pInRayTracingHitDistance; /* for each effect - approximation to the amount of noise in a ray-traced color */
    NVSDK_NGX_Resource_VK *             pInMotionVectorsReflections; /* motion vectors of reflected objects like for mirrored surfaces */
} NVSDK_NGX_VK_DLSS_Eval_Params;

typedef struct NVSDK_NGX_VK_DLISP_Eval_Params
{
    NVSDK_NGX_VK_Feature_Eval_Params Feature;
    /*** OPTIONAL - leave to 0/0.0f if unused ***/
    unsigned int                        InRectX;
    unsigned int                        InRectY;
    unsigned int                        InRectW;
    unsigned int                        InRectH;
    float                               InDenoise;
} NVSDK_NGX_VK_DLISP_Eval_Params;

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_DLSS_EXT1(
    VkDevice InDevice,
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_DLSS_Create_Params *pInDlssCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlssCreateParams->Feature.InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlssCreateParams->Feature.InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pInDlssCreateParams->Feature.InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pInDlssCreateParams->Feature.InTargetHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, pInDlssCreateParams->Feature.InPerfQualityValue);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, pInDlssCreateParams->InFeatureCreateFlags);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects, pInDlssCreateParams->InEnableOutputSubrects ? 1 : 0);

    if (InDevice) return NVSDK_NGX_VULKAN_CreateFeature1(InDevice, InCmdList, NVSDK_NGX_Feature_SuperSampling, pInParams, ppOutHandle);
    else return NVSDK_NGX_VULKAN_CreateFeature(InCmdList, NVSDK_NGX_Feature_SuperSampling, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_DLSS_EXT(
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_DLSS_Create_Params *pInDlssCreateParams)
{
    return NGX_VULKAN_CREATE_DLSS_EXT1(NULL, InCmdList, InCreationNodeMask, InVisibilityNodeMask, ppOutHandle, pInParams, pInDlssCreateParams);
}

static inline NVSDK_NGX_Result NGX_VULKAN_EVALUATE_DLSS_EXT(
    VkCommandBuffer InCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_VK_DLSS_Eval_Params *pInDlssEvalParams)
{
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->Feature.pInColor);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInMotionVectors);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->Feature.pInOutput);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInDepth);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInTransparencyMask);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInExposureTexture);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInBiasCurrentColorMask);
	for (size_t i = 0; i <= 15; i++)
	{
        NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->GBufferSurface.pInAttrib[i]);
	}
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInMotionVectors3D);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInIsParticleMask);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInAnimatedTextureMask);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInDepthHighRes);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInPositionViewSpace);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInRayTracingHitDistance);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlssEvalParams->pInMotionVectorsReflections);

    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Color, pInDlssEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output, pInDlssEvalParams->Feature.pInOutput);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Depth, pInDlssEvalParams->pInDepth);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_MotionVectors, pInDlssEvalParams->pInMotionVectors);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_X, pInDlssEvalParams->InJitterOffsetX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Jitter_Offset_Y, pInDlssEvalParams->InJitterOffsetY);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pInDlssEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_Reset, pInDlssEvalParams->InReset);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_X, pInDlssEvalParams->InMVScaleX == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleX);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_MV_Scale_Y, pInDlssEvalParams->InMVScaleY == 0.0f ? 1.0f : pInDlssEvalParams->InMVScaleY);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_TransparencyMask, pInDlssEvalParams->pInTransparencyMask);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_ExposureTexture, pInDlssEvalParams->pInExposureTexture);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask, pInDlssEvalParams->pInBiasCurrentColorMask);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Albedo, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ALBEDO]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Roughness, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_ROUGHNESS]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Metallic, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_METALLIC]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Specular, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SPECULAR]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Subsurface, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SUBSURFACE]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Normals, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_NORMALS]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_ShadingModelId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_SHADINGMODELID]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_MaterialId, pInDlssEvalParams->GBufferSurface.pInAttrib[NVSDK_NGX_GBUFFER_MATERIALID]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_8, pInDlssEvalParams->GBufferSurface.pInAttrib[8]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_9, pInDlssEvalParams->GBufferSurface.pInAttrib[9]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_10, pInDlssEvalParams->GBufferSurface.pInAttrib[10]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_11, pInDlssEvalParams->GBufferSurface.pInAttrib[11]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_12, pInDlssEvalParams->GBufferSurface.pInAttrib[12]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_13, pInDlssEvalParams->GBufferSurface.pInAttrib[13]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_14, pInDlssEvalParams->GBufferSurface.pInAttrib[14]);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_GBuffer_Atrrib_15, pInDlssEvalParams->GBufferSurface.pInAttrib[15]);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_TonemapperType, pInDlssEvalParams->InToneMapperType);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_MotionVectors3D, pInDlssEvalParams->pInMotionVectors3D);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_IsParticleMask, pInDlssEvalParams->pInIsParticleMask);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_AnimatedTextureMask, pInDlssEvalParams->pInAnimatedTextureMask);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_DepthHighRes, pInDlssEvalParams->pInDepthHighRes);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Position_ViewSpace, pInDlssEvalParams->pInPositionViewSpace);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, pInDlssEvalParams->InFrameTimeDeltaInMsec);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_RayTracingHitDistance, pInDlssEvalParams->pInRayTracingHitDistance);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_MotionVectorsReflection, pInDlssEvalParams->pInMotionVectorsReflections);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, pInDlssEvalParams->InColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, pInDlssEvalParams->InColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X, pInDlssEvalParams->InDepthSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y, pInDlssEvalParams->InDepthSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X, pInDlssEvalParams->InMVSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y, pInDlssEvalParams->InMVSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X, pInDlssEvalParams->InTranslucencySubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y, pInDlssEvalParams->InTranslucencySubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X, pInDlssEvalParams->InBiasCurrentColorSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y, pInDlssEvalParams->InBiasCurrentColorSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X, pInDlssEvalParams->InOutputSubrectBase.X);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y, pInDlssEvalParams->InOutputSubrectBase.Y);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width , pInDlssEvalParams->InRenderSubrectDimensions.Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, pInDlssEvalParams->InRenderSubrectDimensions.Height);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Pre_Exposure, pInDlssEvalParams->InPreExposure == 0.0f ? 1.0f : pInDlssEvalParams->InPreExposure);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_DLSS_Exposure_Scale, pInDlssEvalParams->InExposureScale == 0.0f ? 1.0f : pInDlssEvalParams->InExposureScale);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis, pInDlssEvalParams->InIndicatorInvertXAxis);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, pInDlssEvalParams->InIndicatorInvertYAxis);

    return NVSDK_NGX_VULKAN_EvaluateFeature_C(InCmdList, pInHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_DLISP_EXT(
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInDlispCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlispCreateParams->InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlispCreateParams->InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pInDlispCreateParams->InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pInDlispCreateParams->InTargetHeight);
    NVSDK_NGX_Parameter_SetI(pInParams, NVSDK_NGX_Parameter_PerfQualityValue, pInDlispCreateParams->InPerfQualityValue);

    return NVSDK_NGX_VULKAN_CreateFeature(InCmdList, NVSDK_NGX_Feature_ImageSignalProcessing, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VULKAN_EVALUATE_DLISP_EXT(
    VkCommandBuffer InCmdList,
    NVSDK_NGX_Handle *InHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_VK_DLISP_Eval_Params *pInDlispEvalParams)
{
	NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlispEvalParams->Feature.pInColor);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlispEvalParams->Feature.pInOutput); 

   	NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Color, pInDlispEvalParams->Feature.pInColor);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output, pInDlispEvalParams->Feature.pInOutput);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pInDlispEvalParams->Feature.InSharpness);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Denoise, pInDlispEvalParams->InDenoise);
    // If input is atlas - use RECT to upscale only the required area
    if (pInDlispEvalParams->InRectW)
    {
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_X, pInDlispEvalParams->InRectX);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_Y, pInDlispEvalParams->InRectY);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_W, pInDlispEvalParams->InRectW);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Rect_H, pInDlispEvalParams->InRectH);
    }

    return NVSDK_NGX_VULKAN_EvaluateFeature_C(InCmdList, InHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_VULKAN_CREATE_DLRESOLVE_EXT(
    VkCommandBuffer InCmdList,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle **ppOutHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_Feature_Create_Params *pInDlresolveCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlresolveCreateParams->InWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlresolveCreateParams->InHeight);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutWidth, pInDlresolveCreateParams->InTargetWidth);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_OutHeight, pInDlresolveCreateParams->InTargetHeight);

    return NVSDK_NGX_VULKAN_CreateFeature(InCmdList, NVSDK_NGX_Feature_DeepResolve, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VULKAN_EVALUATE_DLRESOLVE_EXT(
    VkCommandBuffer InCmdList,
    NVSDK_NGX_Handle *pInHandle,
    NVSDK_NGX_Parameter *pInParams,
    NVSDK_NGX_VK_Feature_Eval_Params *pInDlresolveEvalParams)
{
	NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlresolveEvalParams->pInColor);
    NVSDK_NGX_ENSURE_VK_IMAGEVIEW(pInDlresolveEvalParams->pInOutput);
    
	NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Color, pInDlresolveEvalParams->pInColor);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_Output, pInDlresolveEvalParams->pInOutput);
    NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_Parameter_Sharpness, pInDlresolveEvalParams->InSharpness);

    return NVSDK_NGX_VULKAN_EvaluateFeature_C(InCmdList, pInHandle, pInParams, NULL);
}

#endif // NVSDK_NGX_HELPERS_VK_H
