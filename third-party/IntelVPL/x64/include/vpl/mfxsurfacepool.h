/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFX_SURFACE_POOL_H__
#define __MFX_SURFACE_POOL_H__

#include "mfxstructures.h"

/*! GUID to obtain mfxSurfacePoolInterface. */
static const mfxGUID  MFX_GUID_SURFACE_POOL        = {{0x35, 0x24, 0xf3, 0xda, 0x96, 0x4e, 0x47, 0xf1, 0xaf, 0xb4, 0xec, 0xb1, 0x15, 0x08, 0x06, 0xb1}};

/*! Specifies type of pool for VPP component. */
typedef enum {
    MFX_VPP_POOL_IN  = 0, /*!< Input pool. */
    MFX_VPP_POOL_OUT = 1  /*!< Output pool. */
} mfxVPPPoolType;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The extension buffer specifies surface pool management policy.
    Absence of the attached buffer means MFX_ALLOCATION_UNLIMITED policy:
    each call of GetSurfaceForXXX leads to surface allocation.
*/
typedef struct {
    mfxExtBuffer              Header;           /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ALLOCATION_HINTS. */
    mfxPoolAllocationPolicy   AllocationPolicy; /*!< Allocation policy. */
    /*! How many surfaces to allocate during Init.
    It's applicable for any polices set by mfxPoolAllocationPolicy::AllocationPolicy
    even if the requested number exceeds recommended size of the pool. */
    mfxU32                    NumberToPreAllocate;
    /*! DeltaToAllocateOnTheFly specifies how many surfaces are allocated
    in addition to NumberToPreAllocate in MFX_ALLOCATION_LIMITED mode.
    Maximum number of allocated frames  will be
    NumberToPreAllocate + DeltaToAllocateOnTheFly.
    */
    mfxU32                    DeltaToAllocateOnTheFly;
    union {
        mfxVPPPoolType            VPPPoolType; /*!< Defines what VPP pool is targeted - input or output. Ignored for other components. */
        mfxU32                    reserved;
    };
    mfxU32                    Wait;         /*!< Time in milliseconds for GetSurfaceForXXX() and DecodeFrameAsync functions to wait until surface will be available. */
    mfxU32                    reserved1[4]; /*!< Reserved for future use */
} mfxExtAllocationHints;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Specifies the surface pool interface. */
typedef struct mfxSurfacePoolInterface
{
    mfxHDL              Context; /*!< The context of the surface pool interface. User should not touch (change, set, null) this pointer. */

    /*! @brief
    Increments the internal reference counter of the mfxSurfacePoolInterface. The mfxSurfacePoolInterface is not destroyed until the
    mfxSurfacePoolInterface is destroyed with mfxSurfacePoolInterface::Release function. mfxSurfacePoolInterface::AddRef should be used each time a new link to the
    mfxSurfacePoolInterface is created for proper management.

    @param[in]  pool  Valid pool.

    @return
     MFX_ERR_NONE              If no error. \n
     MFX_ERR_NULL_PTR          If pool is NULL. \n
     MFX_ERR_INVALID_HANDLE    If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN           Any internal error.

    */
    mfxStatus           (MFX_CDECL *AddRef)(struct mfxSurfacePoolInterface *pool);
    /*! @brief
    Decrements the internal reference counter of the mfxSurfacePoolInterface. mfxSurfacePoolInterface::Release
    should be called after using the mfxSurfacePoolInterface::AddRef function to add a mfxSurfacePoolInterface or when allocation logic requires it.
    For example, call mfxSurfacePoolInterface::Release to release a mfxSurfacePoolInterface obtained with
    the mfxFrameSurfaceInterface::QueryInterface function.

    @param[in]  pool  Valid pool.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNDEFINED_BEHAVIOR If Reference Counter of mfxSurfacePoolInterface is zero before call. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Release)(struct mfxSurfacePoolInterface *pool);
    /*! @brief
    Returns current reference counter of mfxSurfacePoolInterface structure.

    @param[in]   pool  Valid pool.
    @param[out]  counter  Sets counter to the current reference counter value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool or counter is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *GetRefCounter)(struct mfxSurfacePoolInterface *pool, mfxU32* counter);
    /*! @brief
    The function should be called by oneAPI Video Processing Library (oneVPL) components or application to specify how many surfaces
    it will use concurrently.
    Internally, oneVPL allocates surfaces in the shared pool according to the component's policy set by mfxPoolAllocationPolicy.
    The exact moment of surfaces allocation is defined by the component and generally independent from that call.

    @param[in]  pool  Valid pool.
    @param[in]  num_surfaces  The number of surfaces required by the component.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_WRN_INCOMPATIBLE_VIDEO_PARAM If pool has MFX_ALLOCATION_UNLIMITED or MFX_ALLOCATION_LIMITED policy. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus             (MFX_CDECL *SetNumSurfaces)(struct mfxSurfacePoolInterface *pool, mfxU32 num_surfaces);

    /*! @brief
    The function should be called by oneVPL components when component is closed or reset and doesn't need to use pool more. It helps
    to manage memory accordingly and release redundant memory. Important to specify the same number of surfaces which is requested
    during SetNumSurfaces call, otherwise it may lead to the pipeline stalls.

    @param[in]  pool  Valid pool.
    @param[in]  num_surfaces  The number of surfaces used by the component.

    @return
     MFX_ERR_NONE               If no error. \n

     MFX_WRN_OUT_OF_RANGE       If num_surfaces doesn't equal to num_surfaces requested during SetNumSurfaces call. \n

     MFX_ERR_NULL_PTR           If pool is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_WRN_INCOMPATIBLE_VIDEO_PARAM If pool has MFX_ALLOCATION_UNLIMITED or MFX_ALLOCATION_LIMITED policy. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus               (MFX_CDECL *RevokeSurfaces)(struct mfxSurfacePoolInterface *pool, mfxU32 num_surfaces);
    /*! @brief
    Returns current allocation policy.

    @param[in]   pool  Valid pool.
    @param[out]  policy  Sets policy to the current allocation policy value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool or policy is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus                (MFX_CDECL *GetAllocationPolicy)(struct mfxSurfacePoolInterface *pool, mfxPoolAllocationPolicy *policy);

    /*! @brief
    Returns maximum pool size. In case of mfxPoolAllocationPolicy::MFX_ALLOCATION_UNLIMITED policy 0xFFFFFFFF will be returned.

    @param[in]   pool  Valid pool.
    @param[out]  size  Sets size to the maximum pool size value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool or size is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus                 (MFX_CDECL *GetMaximumPoolSize)(struct mfxSurfacePoolInterface *pool, mfxU32 *size);

    /*! @brief
    Returns current pool size.

    @param[in]   pool  Valid pool.
    @param[out]  size  Sets size to the current pool size value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If pool or size is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfacePoolInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus              (MFX_CDECL *GetCurrentPoolSize)(struct mfxSurfacePoolInterface *pool, mfxU32 *size);

    mfxHDL                 reserved[4]; /*!< Reserved for future use. */

} mfxSurfacePoolInterface;
MFX_PACK_END()


#endif /* __MFX_SURFACE_POOL_H__ */

