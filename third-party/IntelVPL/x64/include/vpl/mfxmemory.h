/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXMEMORY_H__
#define __MFXMEMORY_H__
#include "mfxsession.h"
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*!
   @brief
      Returns surface which can be used as input for VPP.

      VPP should be initialized before this call.
      Surface should be released with mfxFrameSurface1::FrameInterface.Release(...) after usage. The value of mfxFrameSurface1::Data.Locked for the returned surface is 0.


   @param[in]  session Session handle.
   @param[out] surface   Pointer is set to valid mfxFrameSurface1 object.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NULL_PTR If double-pointer to the @p surface is NULL. \n
   MFX_ERR_INVALID_HANDLE If @p session was not initialized. \n
   MFX_ERR_NOT_INITIALIZED If VPP was not initialized (allocator needs to know surface size from somewhere). \n
   MFX_ERR_MEMORY_ALLOC In case of any other internal allocation error. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED In case of waiting timeout expired (if set with mfxExtAllocationHints).

   @since This function is available since API version 2.0.

*/
mfxStatus MFX_CDECL MFXMemory_GetSurfaceForVPP(mfxSession session, mfxFrameSurface1** surface);

/*!
   @brief
      Returns surface which can be used as output of VPP.

      VPP should be initialized before this call.
      Surface should be released with mfxFrameSurface1::FrameInterface.Release(...) after usage. The value of mfxFrameSurface1::Data.Locked for the returned surface is 0.


   @param[in]  session Session handle.
   @param[out] surface   Pointer is set to valid mfxFrameSurface1 object.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NULL_PTR If double-pointer to the @p surface is NULL. \n
   MFX_ERR_INVALID_HANDLE If @p session was not initialized. \n
   MFX_ERR_NOT_INITIALIZED If VPP was not initialized (allocator needs to know surface size from somewhere). \n
   MFX_ERR_MEMORY_ALLOC In case of any other internal allocation error. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED In case of waiting timeout expired (if set with mfxExtAllocationHints).

   @since This function is available since API version 2.1.

*/
mfxStatus MFX_CDECL MFXMemory_GetSurfaceForVPPOut(mfxSession session, mfxFrameSurface1** surface);

/*! Alias for MFXMemory_GetSurfaceForVPP function. */
#define MFXMemory_GetSurfaceForVPPIn MFXMemory_GetSurfaceForVPP

/*!
   @brief
    Returns a surface which can be used as input for the encoder.

    Encoder should be initialized before this call.
    Surface should be released with mfxFrameSurface1::FrameInterface.Release(...) after usage. The value of mfxFrameSurface1::Data.Locked for the returned surface is 0.



   @param[in]  session Session handle.
   @param[out] surface   Pointer is set to valid mfxFrameSurface1 object.

   @return
   MFX_ERR_NONE The function completed successfully.\n
   MFX_ERR_NULL_PTR If surface is NULL.\n
   MFX_ERR_INVALID_HANDLE If session was not initialized.\n
   MFX_ERR_NOT_INITIALIZED If the encoder was not initialized (allocator needs to know surface size from somewhere).\n
   MFX_ERR_MEMORY_ALLOC In case of any other internal allocation error. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED In case of waiting timeout expired (if set with mfxExtAllocationHints).

   @since This function is available since API version 2.0.

*/
mfxStatus MFX_CDECL MFXMemory_GetSurfaceForEncode(mfxSession session, mfxFrameSurface1** surface);

/*!
   @brief
    Returns a surface which can be used as output of the decoder.

    Decoder should be initialized before this call.
    Surface should be released with mfxFrameSurface1::FrameInterface.Release(...) after usage. The value of mfxFrameSurface1::Data.Locked for the returned surface is 0.'

    @note This function was added to simplify transition from legacy surface management to the proposed internal allocation approach.
    Previously, the user allocated surfaces for the working pool and fed them to the decoder using DecodeFrameAsync calls. With MFXMemory_GetSurfaceForDecode
    it is possible to change the existing pipeline by just changing the source of work surfaces.
    Newly developed applications should prefer direct usage of DecodeFrameAsync with internal allocation.


   @param[in]  session Session handle.
   @param[out] surface   Pointer is set to valid mfxFrameSurface1 object.

   @return
   MFX_ERR_NONE The function completed successfully.\n
   MFX_ERR_NULL_PTR If surface is NULL.\n
   MFX_ERR_INVALID_HANDLE If session was not initialized.\n
   MFX_ERR_NOT_INITIALIZED If the decoder was not initialized (allocator needs to know surface size from somewhere).\n
   MFX_ERR_MEMORY_ALLOC Other internal allocation error. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED In case of waiting timeout expired (if set with mfxExtAllocationHints).

   @since This function is available since API version 2.0.

*/
mfxStatus MFX_CDECL MFXMemory_GetSurfaceForDecode(mfxSession session, mfxFrameSurface1** surface);

#ifdef ONEVPL_EXPERIMENTAL

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceInterface SurfaceInterface;

    mfxHDL texture2D;                           /*!< Pointer to texture, type ID3D11Texture2D* */
    mfxHDL reserved[7];
} mfxSurfaceD3D11Tex2D;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceInterface SurfaceInterface;

    mfxHDL vaDisplay;                           /*!< Object of type VADisplay. */
    mfxU32 vaSurfaceID;                         /*!< Object of type VASurfaceID. */
    mfxU32 reserved1;

    mfxHDL reserved[6];
} mfxSurfaceVAAPI;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Optional extension buffer, which can be attached to mfxSurfaceHeader::ExtParam
   (second parameter of mfxFrameSurfaceInterface::Export) in order to pass OCL parameters
   during mfxFrameSurface1 exporting to OCL surface.
   If buffer is not provided all resources will be created by oneAPI Video Processing Library (oneVPL) RT internally.
*/
typedef struct {
    mfxExtBuffer    Header;                     /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_EXPORT_SHARING_DESC_OCL. */

    mfxHDL ocl_context;                         /*!< Object of type cl_context (OpenCL context). */
    mfxHDL ocl_command_queue;                   /*!< Object of type cl_command_queue (OpenCL command queue). */

    mfxHDL reserved[8];
} mfxExtSurfaceOpenCLImg2DExportDescription;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceInterface SurfaceInterface;

    mfxHDL ocl_context;                         /*!< Object of type cl_context (OpenCL context). */
    mfxHDL ocl_command_queue;                   /*!< Object of type cl_command_queue (OpenCL command queue). */

    mfxHDL ocl_image[4];                        /*!< Object of type cl_mem[4] (array of 4 OpenCL 2D images). */
    mfxU32 ocl_image_num;                       /*!< Number of valid images (planes), depends on color format. */

    mfxHDL reserved[8];
} mfxSurfaceOpenCLImg2D;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Optional extension buffer, which can be attached to mfxSurfaceHeader::ExtParam
   (second parameter of mfxFrameSurfaceInterface::Export) in order to pass D3D12 parameters
   during mfxFrameSurface1 exporting to D3D12 resource.
   If buffer is not provided all resources will be created by oneAPI Video Processing Library (oneVPL) RT internally.
*/
typedef struct {
    mfxExtBuffer    Header;                     /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_EXPORT_SHARING_DESC_D3D12. */

    mfxHDL d3d12Device;                         /*!< Pointer to D3D12 Device, type ID3D12Device*. */

    mfxHDL reserved[9];
} mfxExtSurfaceD3D12Tex2DExportDescription;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceInterface SurfaceInterface;

    mfxHDL texture2D;                           /*!< Pointer to D3D12 resource, type ID3D12Resource*. */

    mfxHDL reserved[7];
} mfxSurfaceD3D12Tex2D;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Optional extension buffer, which can be attached to mfxSurfaceHeader::ExtParam
   (second parameter of mfxFrameSurfaceInterface::Export) in order to pass Vulkan parameters
   during mfxFrameSurface1 exporting to Vulkan surface.
   If buffer is not provided all resources will be created by oneAPI Video Processing Library (oneVPL) RT internally.
*/
typedef struct {
    mfxExtBuffer    Header;                     /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_EXPORT_SHARING_DESC_VULKAN. */

    mfxHDL instance;                            /*!< Object of type VkInstance (Vulkan instance). */
    mfxHDL physicalDevice;                      /*!< Object of type VkPhysicalDevice (Vulkan physical device). */
    mfxHDL device;                              /*!< Object of type VkDevice (Vulkan device). */

    mfxHDL reserved[7];
} mfxExtSurfaceVulkanImg2DExportDescription;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceInterface SurfaceInterface;

    mfxHDL instance;                            /*!< Object of type VkInstance (Vulkan instance). */
    mfxHDL physicalDevice;                      /*!< Object of type VkPhysicalDevice (Vulkan physical device). */
    mfxHDL device;                              /*!< Object of type VkDevice (Vulkan device). */

    mfxHDL image2D;                             /*!< Object of type VkImage (Vulkan 2D images). */
    mfxHDL image2DMemory;                       /*!< Object of type VkDeviceMemory (Vulkan device memory). */

    mfxHDL reserved[10];
} mfxSurfaceVulkanImg2D;
MFX_PACK_END()

/*! The mfxSurfaceComponent enumerator specifies the internal surface pool to use when importing surfaces. */
typedef enum {
    MFX_SURFACE_COMPONENT_UNKNOWN     = 0,      /*!< Unknown surface component. */

    MFX_SURFACE_COMPONENT_ENCODE      = 1,      /*!< Shared surface for encoding. */
    MFX_SURFACE_COMPONENT_DECODE      = 2,      /*!< Shared surface for decoding. */
    MFX_SURFACE_COMPONENT_VPP_INPUT   = 3,      /*!< Shared surface for VPP input. */
    MFX_SURFACE_COMPONENT_VPP_OUTPUT  = 4,      /*!< Shared surface for VPP output. */
} mfxSurfaceComponent;

/*! The current version of mfxSurfaceTypesSupported structure. */
#define MFX_SURFACETYPESSUPPORTED_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure describes the supported surface types and modes. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */

    mfxU16 NumSurfaceTypes;                         /*!< Number of supported surface types. */
    struct surftype {
        mfxSurfaceType SurfaceType;                 /*!< Supported surface type. */
        mfxU32         reserved[6];                 /*!< Reserved for future use. */
        mfxU16         NumSurfaceComponents;        /*!< Number of supported surface components. */
        struct surfcomp {
            mfxSurfaceComponent SurfaceComponent;   /*!< Supported surface component. */
            mfxU32              SurfaceFlags;       /*!< Supported surface flags for this component (may be OR'd). */
            mfxU32              reserved[7];        /*!< Reserved for future use. */
        } *SurfaceComponents;
    } *SurfaceTypes;

    mfxU32   reserved[4];                           /*!< Reserved for future use. */
} mfxSurfaceTypesSupported;
MFX_PACK_END()

#define MFX_MEMORYINTERFACE_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/* Specifies memory interface. */
typedef struct mfxMemoryInterface {
    mfxHDL              Context; /*!< The context of the memory interface. User should not touch (change, set, null) this pointer. */
    mfxStructVersion    Version; /*!< The version of the structure. */

    /*!
       @brief
        Imports an application-provided surface into mfxFrameSurface1 which may be used as input for encoding or video processing.

       @param[in]      memory_interface  Valid memory interface.
       @param[in]      surf_component    Surface component type. Required for allocating new surfaces from the appropriate pool.
       @param[in,out]  external_surface  Pointer to the mfxSurfaceXXX object describing the surface to be imported. All fields in
                                         mfxSurfaceHeader must be set by the application. mfxSurfaceHeader::SurfaceType is
                                         read by oneVPL runtime to determine which particular mfxSurfaceXXX structure is supplied.
                                         For example, if mfxSurfaceXXX::SurfaceType == MFX_SURFACE_TYPE_D3D11_TEX2D, then the handle
                                         will be interpreted as an object of type mfxSurfaceD3D11Tex2D. The application should
                                         set or clear other fields as specified in the corresponding structure description.
                                         After successful import, the value of mfxSurfaceHeader::SurfaceFlags will be replaced with the actual
                                         import type. It can be used to determine which import type (with or without copy) took place in the case
                                         of initial default setting, or if multiple import flags were OR'ed.
                                         All external sync operations on the ext_surface must be completed before calling this function.
       @param[out]     imported_surface  Pointer to a valid mfxFrameSurface1 object containing the imported frame.
                                         imported_surface may be passed as an input to Encode or VPP processing operations.

       @return
       MFX_ERR_NONE The function completed successfully.\n
       MFX_ERR_NULL_PTR If ext_surface or imported_surface are NULL.\n
       MFX_ERR_INVALID_HANDLE If the corresponding session was not initialized.\n
       MFX_ERR_UNSUPPORTED If surf_component is not one of [MFX_SURFACE_COMPONENT_ENCODE, MFX_SURFACE_COMPONENT_VPP_INPUT], or if
                           mfxSurfaceHeader::SurfaceType is not supported by oneVPL runtime for this operation.\n

       @since This function is available since API version 2.10.
    */

    /* For reference with Export flow please search for mfxFrameSurfaceInterface::Export. */
    mfxStatus (MFX_CDECL *ImportFrameSurface)(struct mfxMemoryInterface* memory_interface, mfxSurfaceComponent surf_component, mfxSurfaceHeader* external_surface, mfxFrameSurface1** imported_surface);

    mfxHDL     reserved[16];
} mfxMemoryInterface;
MFX_PACK_END()

/*! Alias for returning interface of type mfxMemoryInterface. */
#define MFXGetMemoryInterface(session, piface)  MFXVideoCORE_GetHandle((session), MFX_HANDLE_MEMORY_INTERFACE, (mfxHDL *)(piface))

#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif
