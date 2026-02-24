/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/*
*  HOW TO USE:
*
*  IMPORTANT: FOR DLSS/DLISP PLEASE SEE THE PROGRAMMING GUIDE
* 
*  IMPORTANT: Methods in this library are NOT thread safe. It is up to the
*  client to ensure that thread safety is enforced as needed.
*  
*  1) Call NVSDK_CONV NVSDK_NGX_D3D11/D3D12/CUDA_Init and pass your app Id
*     and other parameters. This will initialize SDK or return an error code
*     if SDK cannot run on target machine. Depending on error user might
*     need to update drivers. Please note that application Id is provided
*     by NVIDIA so if you do not have one please contact us.
* 
*  2) Call NVSDK_NGX_D3D11/D3D12/CUDA_GetCapabilityParameters to obtain pointer to 
*     interface used to pass parameters to SDK. Interface instance is 
*     allocated and released by SDK so there is no need to do any memory 
*     management on client side.
*    
*  3) Set key parameters for the feature you want to use. For example, 
*     width and height are required for all features and they can be
*     set like this: 
*         Params->Set(NVSDK_NGX_Parameter_Width,MY_WIDTH);
*         Params->Set(NVSDK_NGX_Parameter_Height,MY_HEIGHT);
*
*     You can also provide hints like NVSDK_NGX_Parameter_Hint_HDR to tell
*     SDK that it should expect HDR color space is needed. Please refer to 
*     samples since different features need different parameters and hints.
*
*  4) Call NVSDK_NGX_D3D11/D3D12/CUDA_GetScratchBufferSize to obtain size of
*     the scratch buffer needed by specific feature. This D3D or CUDA buffer
*     should be allocated by client and passed as:
*        Params->Set(NVSDK_NGX_Parameter_Scratch,MY_SCRATCH_POINTER)
*        Params->Set(NVSDK_NGX_Parameter_Scratch_SizeInBytes,MY_SCRATCH_SIZE_IN_BYTES)
*     NOTE: Returned size can be 0 if feature does not use any scratch buffer.
*     It is OK to use bigger buffer or reuse buffers across features as long
*     as minimum size requirement is met.
*
*  5) Call NVSDK_NGX_D3D11/D3D12/CUDA_CreateFeature to create feature you need.
*     On success SDK will return a handle which must be used in any successive 
*     calls to SDK which require feature handle. SDK will use all parameters
*     and hints provided by client to generate feature. If feature with the same
*     parameters already exists and error code will be returned.
*
*  6) Call NVSDK_NGX_D3D11/D3D12/CUDA_EvaluateFeature to invoke execution of
*     specific feature. Before feature can be evaluated input parameters must
*     be specified (like for example color/albedo buffer, motion vectors etc)
* 
*  6) Call NVSDK_NGX_D3D11/D3D12/CUDA_ReleaseFeature when feature is no longer
*     needed. After this call feature handle becomes invalid and cannot be used.
* 
*  7) Call NVSDK_NGX_D3D11/D3D12/CUDA_Shutdown when SDK is no longer needed to
*     release all resources.

*  Contact: ngxsupport@nvidia.com
*/


#ifndef NVSDK_NGX_H
#define NVSDK_NGX_H

#include <stddef.h> // For size_t

#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_params.h"
#ifndef __cplusplus
#include <stdbool.h> 
#include <wchar.h> 
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct IUnknown IUnknown;

typedef struct IDXGIAdapter              IDXGIAdapter;
typedef struct ID3D11Device              ID3D11Device;
typedef struct ID3D11Resource            ID3D11Resource;
typedef struct ID3D11DeviceContext       ID3D11DeviceContext;
typedef struct D3D11_TEXTURE2D_DESC      D3D11_TEXTURE2D_DESC;
typedef struct D3D11_BUFFER_DESC         D3D11_BUFFER_DESC;
typedef struct ID3D11Buffer              ID3D11Buffer;
typedef struct ID3D11Texture2D           ID3D11Texture2D;

typedef struct ID3D12Device              ID3D12Device;
typedef struct ID3D12Resource            ID3D12Resource;
typedef struct ID3D12GraphicsCommandList ID3D12GraphicsCommandList;
typedef struct D3D12_RESOURCE_DESC       D3D12_RESOURCE_DESC;
typedef struct CD3DX12_HEAP_PROPERTIES   CD3DX12_HEAP_PROPERTIES;

typedef void (NVSDK_CONV *PFN_NVSDK_NGX_D3D12_ResourceAllocCallback)(D3D12_RESOURCE_DESC *InDesc, int InState, CD3DX12_HEAP_PROPERTIES *InHeap, ID3D12Resource **OutResource);
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_D3D11_BufferAllocCallback)(D3D11_BUFFER_DESC *InDesc, ID3D11Buffer **OutResource);
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_D3D11_Tex2DAllocCallback)(D3D11_TEXTURE2D_DESC *InDesc, ID3D11Texture2D **OutResource);
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_ResourceReleaseCallback)(IUnknown *InResource);

typedef unsigned long long CUtexObject;

// NOTE: Functions under the same name and different function signatures exist
// between the NGX SDK, NGX Core (driver), and NGX Snippets. To discern the
// different signatures here we check if NGX_SNIPPET_BUILD is defined. When that
// is the case we know that the function signature in-use should be that of
// between NGX Core and the Snippet. Otherwise the signature should be between
// NGX SDK and NGX Core.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Init
// -------------------------------------
// 
// InApplicationId:
//      Unique Id provided by NVIDIA
//
// InApplicationDataPath:
//      Folder to store logs and other temporary files (write access required),
//      Normally this would be a location in Documents or ProgramData.
//
// InDevice: [d3d11/12 only]
//      DirectX device to use
//
// InFeatureInfo:
//      Contains information common to all features, presently only a list of all paths 
//      feature dlls can be located in, other than the default path - application directory.
//
// DESCRIPTION:
//      Initializes new SDK instance.
//
#if defined(NGX_SNIPPET_BUILD)
#ifdef __cplusplus
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, const NVSDK_NGX_Parameter* InParameters = nullptr);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, const NVSDK_NGX_Parameter* InParameters = nullptr);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, const NVSDK_NGX_Parameter* InParameters = nullptr);
#else
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter* InParameters);
#endif
#else
#ifdef __cplusplus
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
#else
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
#endif
#endif // defined(NGX_SNIPPET_BUILD)

///////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Init_with_ProjectID
// -------------------------------------
// 
// InProjectId:
//      Unique Id provided by the rendering engine used
//
// InEngineType:
//      Rendering engine used by the application / plugin.
//      Use NVSDK_NGX_ENGINE_TYPE_CUSTOM if the specific engine type is not supported explicitly
//
// InEngineVersion:
//      Version number of the rendering engine used by the application / plugin.
//
// InApplicationDataPath:
//      Folder to store logs and other temporary files (write access required),
//      Normally this would be a location in Documents or ProgramData.
//
// InDevice: [d3d11/12 only]
//      DirectX device to use
//
// InFeatureInfo:
//      Contains information common to all features, presently only a list of all paths 
//      feature dlls can be located in, other than the default path - application directory.
//
// DESCRIPTION:
//      Initializes new SDK instance.
//
#if defined(NGX_SNIPPET_BUILD)
// No NGX Core <---> Snippet interfaces
#else
#ifdef __cplusplus
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
#else
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D11_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, ID3D11Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_D3D12_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_CUDA_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
#endif
#endif // defined(NGX_SNIPPET_BUILD)

////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Shutdown
// -------------------------------------
// 
// DESCRIPTION:
//      Shuts down the current SDK instance and releases all resources.
//      Shutdown1(Device) only affects specified device
//      Shutdown1(nullptr) = Shutdown() and shuts down all devices
//
#ifdef NGX_ENABLE_DEPRECATED_SHUTDOWN
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_Shutdown(void);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_Shutdown(void);
#endif
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_Shutdown1(ID3D11Device *InDevice);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *InDevice);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_Shutdown(void);

#ifdef NGX_ENABLE_DEPRECATED_GET_PARAMETERS
////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetParameters
// ----------------------------------------------------------
//
// OutParameters:
//      Parameters interface used to set any parameter needed by the SDK
//
// DESCRIPTION:
//      This interface allows simple parameter setup using named fields.
//      For example one can set width by calling Set(NVSDK_NGX_Parameter_Denoiser_Width,100) or
//      provide CUDA buffer pointer by calling Set(NVSDK_NGX_Parameter_Denoiser_Color,cudaBuffer)
//      For more details please see sample code. Please note that allocated memory
//      will be freed by NGX so free/delete operator should NOT be called.
//      Parameter maps output by NVSDK_NGX_GetParameters are also pre-populated
//      with NGX capabilities and available features.
//      Unlike with NVSDK_NGX_AllocateParameters, parameter maps output by NVSDK_NGX_GetParameters
//      have their lifetimes managed by NGX, and must not
//      be destroyed by the app using NVSDK_NGX_DestroyParameters.
//      NVSDK_NGX_GetParameters is deprecated and apps should move to using
//      NVSDK_NGX_AllocateParameters and NVSDK_NGX_GetCapabilityParameters when possible.
//      Nevertheless, due to the possibility that the user will be using an older driver version,
//      NVSDK_NGX_GetParameters may still be used as a fallback if NVSDK_NGX_AllocateParameters
//      or NVSDK_NGX_GetCapabilityParameters return NVSDK_NGX_Result_FAIL_OutOfDate.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_GetParameters(NVSDK_NGX_Parameter **OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_GetParameters(NVSDK_NGX_Parameter **OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_GetParameters(NVSDK_NGX_Parameter **OutParameters);
#endif // NGX_ENABLE_DEPRECATED_GET_PARAMETERS

////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_AllocateParameters
// ----------------------------------------------------------
//
// OutParameters:
//      Parameters interface used to set any parameter needed by the SDK
//
// DESCRIPTION:
//      This interface allows allocating a simple parameter setup using named fields, whose
//      lifetime the app must manage.
//      For example one can set width by calling Set(NVSDK_NGX_Parameter_Denoiser_Width,100) or
//      provide CUDA buffer pointer by calling Set(NVSDK_NGX_Parameter_Denoiser_Color,cudaBuffer)
//      For more details please see sample code.
//      Parameter maps output by NVSDK_NGX_AllocateParameters must NOT be freed using
//      the free/delete operator; to free a parameter map 
//      output by NVSDK_NGX_AllocateParameters, NVSDK_NGX_DestroyParameters should be used.
//      Unlike with NVSDK_NGX_GetParameters, parameter maps allocated with NVSDK_NGX_AllocateParameters
//      must be destroyed by the app using NVSDK_NGX_DestroyParameters.
//      Also unlike with NVSDK_NGX_GetParameters, parameter maps output by NVSDK_NGX_AllocateParameters
//      do not come pre-populated with NGX capabilities and available features.
//      To create a new parameter map pre-populated with such information, NVSDK_NGX_GetCapabilityParameters
//      should be used.
//      This function may return NVSDK_NGX_Result_FAIL_OutOfDate if an older driver, which
//      does not support this API call is being used. In such a case, NVSDK_NGX_GetParameters
//      may be used as a fallback.
//      This function may only be called after a successful call into NVSDK_NGX_Init.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);

////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetCapabilityParameters
// ----------------------------------------------------------
//
// OutParameters:
//      The parameters interface populated with NGX and feature capabilities
//
// DESCRIPTION:
//      This interface allows the app to create a new parameter map
//      pre-populated with NGX capabilities and available features.
//      The output parameter map can also be used for any purpose
//      parameter maps output by NVSDK_NGX_AllocateParameters can be used for
//      but it is not recommended to use NVSDK_NGX_GetCapabilityParameters
//      unless querying NGX capabilities and available features
//      due to the overhead associated with pre-populating the parameter map.
//      Parameter maps output by NVSDK_NGX_GetCapabilityParameters must NOT be freed using
//      the free/delete operator; to free a parameter map
//      output by NVSDK_NGX_GetCapabilityParameters, NVSDK_NGX_DestroyParameters should be used.
//      Unlike with NVSDK_NGX_GetParameters, parameter maps allocated with NVSDK_NGX_GetCapabilityParameters
//      must be destroyed by the app using NVSDK_NGX_DestroyParameters.
//      This function may return NVSDK_NGX_Result_FAIL_OutOfDate if an older driver, which
//      does not support this API call is being used. This function may only be called
//      after a successful call into NVSDK_NGX_Init.
//      If NVSDK_NGX_GetCapabilityParameters fails with NVSDK_NGX_Result_FAIL_OutOfDate,
//      NVSDK_NGX_GetParameters may be used as a fallback, to get a parameter map pre-populated
//      with NGX capabilities and available features.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);

////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_DestroyParameters
// ----------------------------------------------------------
//
// InParameters:
//      The parameters interface to be destroyed
//
// DESCRIPTION:
//      This interface allows the app to destroy the parameter map passed in. Once
//      NVSDK_NGX_DestroyParameters is called on a parameter map, it
//      must not be used again.
//      NVSDK_NGX_DestroyParameters must not be called on any parameter map returned
//      by NVSDK_NGX_GetParameters; NGX will manage the lifetime of those
//      parameter maps.
//      This function may return NVSDK_NGX_Result_FAIL_OutOfDate if an older driver, which
//      does not support this API call is being used. This function may only be called
//      after a successful call into NVSDK_NGX_Init.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_DestroyParameters(NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_DestroyParameters(NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_DestroyParameters(NVSDK_NGX_Parameter* InParameters);

////////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetScratchBufferSize
// ----------------------------------------------------------
//
// InFeatureId:
//      AI feature in question
//
// InParameters:
//      Parameters used by the feature to help estimate scratch buffer size
//
// OutSizeInBytes:
//      Number of bytes needed for the scratch buffer for the specified feature.
//
// DESCRIPTION:
//      SDK needs a buffer of a certain size provided by the client in
//      order to initialize AI feature. Once feature is no longer
//      needed buffer can be released. It is safe to reuse the same
//      scratch buffer for different features as long as minimum size
//      requirement is met for all features. Please note that some
//      features might not need a scratch buffer so return size of 0
//      is completely valid.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter *InParameters, size_t *OutSizeInBytes);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter *InParameters, size_t *OutSizeInBytes);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter *InParameters, size_t *OutSizeInBytes);

/////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_CreateFeature
// -------------------------------------
//
// InCmdList:[d3d12 only]
//      Command list to use to execute GPU commands. Must be:
//      - Open and recording 
//      - With node mask including the device provided in NVSDK_NGX_D3D12_Init
//      - Execute on non-copy command queue.
// InDevCtx: [d3d11 only]
//      Device context to use to execute GPU commands
//
// InFeatureID:
//      AI feature to initialize
//
// InParameters:
//      List of parameters 
// 
// OutHandle:
//      Handle which uniquely identifies the feature. If feature with
//      provided parameters already exists the "already exists" error code is returned.
//
// DESCRIPTION:
//      Each feature needs to be created before it can be used. 
//      Refer to the sample code to find out which input parameters
//      are needed to create specific feature.
//
#if defined(NGX_SNIPPET_BUILD)
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_CreateFeature(ID3D11DeviceContext *InDevCtx, NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList *InCmdList, NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_CreateFeature(NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
#else
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_CreateFeature(ID3D11DeviceContext *InDevCtx, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList *InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_CreateFeature(NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);

#endif // defined(NGX_SNIPPET_BUILD)

/////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Release
// -------------------------------------
// 
// InHandle:
//      Handle to feature to be released
//
// DESCRIPTION:
//      Releases feature with a given handle.
//      Handles are not reference counted so
//      after this call it is invalid to use provided handle.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_ReleaseFeature(NVSDK_NGX_Handle *InHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_ReleaseFeature(NVSDK_NGX_Handle *InHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_ReleaseFeature(NVSDK_NGX_Handle *InHandle);

///////////////////////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetFeatureRequirements
// -------------------------------------
// Adapter:
//      Physical adapter Information
//
// FeatureDiscoveryInfo:
//      Contains information common to all NGX Features - required for Feature discovery, Initialization and Logging.
//
// DESCRIPTION:
//      Utility function used to identify system requirements to support a given NGX Feature
//      on a system given its display device subsytem adapter information that will be subsequently used for creating the graphics device.
//      The output parameter OutSupported will be populated with requirements and are valid if and only if NVSDK_NGX_Result_Success is returned:
//          OutSupported::FeatureSupported: bitfield of bit shifted values specified in NVSDK_NGX_Feature_Support_Result. 0 if Feature is Supported.
//          OutSupported::MinHWArchitecture: Returned HW Architecture value corresponding to NV_GPU_ARCHITECTURE_ID values defined in NvAPI GPU Framework.
//          OutSupported::MinOSVersion: Value corresponding to minimum OS version required for NGX Feature Support
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_GetFeatureRequirements(IDXGIAdapter *Adapter,
                                                                                 const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                 NVSDK_NGX_FeatureRequirement *OutSupported);
 
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter *Adapter,
                                                                                 const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                 NVSDK_NGX_FeatureRequirement *OutSupported);

/////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_EvaluateFeature
// -------------------------------------
//
// InCmdList:[d3d12 only]
//      Command list to use to execute GPU commands. Must be:
//      - Open and recording 
//      - With node mask including the device provided in NVSDK_NGX_D3D12_Init
//      - Execute on non-copy command queue.
// InDevCtx: [d3d11 only]
//      Device context to use to execute GPU commands
//
// InFeatureHandle:
//      Handle representing feature to be evaluated
// 
// InParameters:
//      List of parameters required to evaluate feature
//
// InCallback:
//      Optional callback for features which might take longer
//      to execute. If specified SDK will call it with progress
//      values in range 0.0f - 1.0f
//
// DESCRIPTION:
//      Evaluates given feature using the provided parameters and
//      pre-trained NN. Please note that for most features
//      it can be beneficial to pass as many input buffers and parameters
//      as possible (for example provide all render targets like color, albedo, normals, depth etc)
//

#ifdef __cplusplus
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_ProgressCallback)(float InCurrentProgress, bool &OutShouldCancel);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_EvaluateFeature(ID3D11DeviceContext *InDevCtx, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *InCmdList, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_EvaluateFeature(const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL);
#endif

typedef void (NVSDK_CONV *PFN_NVSDK_NGX_ProgressCallback_C)(float InCurrentProgress, bool *OutShouldCancel);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D11_EvaluateFeature_C(ID3D11DeviceContext *InDevCtx, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback_C InCallback);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_D3D12_EvaluateFeature_C(ID3D12GraphicsCommandList *InCmdList, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback_C InCallback);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_CUDA_EvaluateFeature_C(const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback_C InCallback);

#if defined(NGX_SNIPPET_BUILD)
// No NGX Core <---> Snippet interfaces
#else
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_UpdateFeature(const NVSDK_NGX_Application_Identifier *ApplicationId, const NVSDK_NGX_Feature FeatureID);
#endif // defined(NGX_SNIPPET_BUILD)

// NGX return-code conversion-to-string utility only as a helper for debugging/logging - not for official use.
const wchar_t* NVSDK_CONV GetNGXResultAsString(NVSDK_NGX_Result InNGXResult);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #define NVSDK_NGX_H
