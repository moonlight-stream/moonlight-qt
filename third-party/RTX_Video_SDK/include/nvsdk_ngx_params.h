/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


#ifndef NVSDK_NGX_PARAMS_H
#define NVSDK_NGX_PARAMS_H

#include "nvsdk_ngx_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ID3D11Resource ID3D11Resource;
typedef struct ID3D12Resource ID3D12Resource;

typedef struct NVSDK_NGX_Feature_Create_Params
{
    unsigned int InWidth;
    unsigned int InHeight;
    unsigned int InTargetWidth;
    unsigned int InTargetHeight;
    /*** OPTIONAL ***/
    NVSDK_NGX_PerfQuality_Value InPerfQualityValue;
} NVSDK_NGX_Feature_Create_Params;

typedef struct NVSDK_NGX_DLSS_Create_Params
{
    NVSDK_NGX_Feature_Create_Params Feature;
    /*** OPTIONAL ***/
    int     InFeatureCreateFlags;
    bool    InEnableOutputSubrects;
} NVSDK_NGX_DLSS_Create_Params;

typedef struct NVSDK_NGX_DLDenoise_Create_Params
{
    NVSDK_NGX_Feature_Create_Params Feature;
    /*** OPTIONAL ***/
    int InFeatureCreateFlags;
} NVSDK_NGX_DLDenoise_Create_Params;

#ifdef __cplusplus
typedef struct NVSDK_NGX_Parameter
{
    virtual void Set(const char * InName, unsigned long long InValue) = 0;
    virtual void Set(const char * InName, float InValue) = 0;
    virtual void Set(const char * InName, double InValue) = 0;
    virtual void Set(const char * InName, unsigned int InValue) = 0;
    virtual void Set(const char * InName, int InValue) = 0;    
    virtual void Set(const char * InName, ID3D11Resource *InValue) = 0;
    virtual void Set(const char * InName, ID3D12Resource *InValue) = 0;
    virtual void Set(const char * InName, void *InValue) = 0;

    virtual NVSDK_NGX_Result Get(const char * InName, unsigned long long *OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, float *OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, double *OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, unsigned int *OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, int *OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, ID3D11Resource **OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, ID3D12Resource **OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char * InName, void **OutValue) const = 0;
    
    virtual void Reset() = 0;
} NVSDK_NGX_Parameter;
#else
typedef struct NVSDK_NGX_Parameter NVSDK_NGX_Parameter;
#endif // _cplusplus

typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetULL)(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned long long InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetULL(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned long long InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetF)(NVSDK_NGX_Parameter *InParameter, const char * InName, float InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetF(NVSDK_NGX_Parameter *InParameter, const char * InName, float InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetD)(NVSDK_NGX_Parameter *InParameter, const char * InName, double InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetD(NVSDK_NGX_Parameter *InParameter, const char * InName, double InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetUI)(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned int InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetUI(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned int InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetI)(NVSDK_NGX_Parameter *InParameter, const char * InName, int InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetI(NVSDK_NGX_Parameter *InParameter, const char * InName, int InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetD3d11Resource)(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D11Resource *InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetD3d11Resource(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D11Resource *InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetD3d12Resource)(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D12Resource *InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetD3d12Resource(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D12Resource *InValue);
typedef void             (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_SetVoidPointer)(NVSDK_NGX_Parameter *InParameter, const char * InName, void *InValue);
NVSDK_NGX_API void        NVSDK_CONV      NVSDK_NGX_Parameter_SetVoidPointer(NVSDK_NGX_Parameter *InParameter, const char * InName, void *InValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetULL)(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned long long *OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetULL(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned long long *OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetF)(NVSDK_NGX_Parameter *InParameter, const char * InName, float *OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetF(NVSDK_NGX_Parameter *InParameter, const char * InName, float *OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetD)(NVSDK_NGX_Parameter *InParameter, const char * InName, double *OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetD(NVSDK_NGX_Parameter *InParameter, const char * InName, double *OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetUI)(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned int *OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetUI(NVSDK_NGX_Parameter *InParameter, const char * InName, unsigned int *OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetI)(NVSDK_NGX_Parameter *InParameter, const char * InName, int *OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetI(NVSDK_NGX_Parameter *InParameter, const char * InName, int *OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetD3d11Resource)(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D11Resource **OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetD3d11Resource(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D11Resource **OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetD3d12Resource)(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D12Resource **OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetD3d12Resource(NVSDK_NGX_Parameter *InParameter, const char * InName, ID3D12Resource **OutValue);
typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_NVSDK_NGX_Parameter_GetVoidPointer)(NVSDK_NGX_Parameter *InParameter, const char * InName, void **OutValue);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_Parameter_GetVoidPointer(NVSDK_NGX_Parameter *InParameter, const char * InName, void **OutValue);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #define NVSDK_NGX_PARAMS_H
