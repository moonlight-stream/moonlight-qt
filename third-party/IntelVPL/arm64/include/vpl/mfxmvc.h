/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXMVC_H__
#define __MFXMVC_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CodecProfile, CodecLevel */
enum {
    /* MVC profiles */
    MFX_PROFILE_AVC_MULTIVIEW_HIGH = 118, /*!< Multi-view high profile. The encoding of VDEnc or LowPower ON is not supported. */
    MFX_PROFILE_AVC_STEREO_HIGH    = 128  /*!< Stereo high profile. The encoding of VDEnc or LowPower ON is not supported. */
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_MVC_SEQ_DESC     =   MFX_MAKEFOURCC('M','V','C','D'), /*!< This extended buffer describes the MVC stream information of view dependencies, view identifiers, and operation points. See the ITU*-T H.264 specification chapter H.7.3.2.1.4 for details. */
    MFX_EXTBUFF_MVC_TARGET_VIEWS =   MFX_MAKEFOURCC('M','V','C','T')  /*!< This extended buffer defines target views at the decoder output.*/
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
  Describes MVC view dependencies.
*/
typedef struct  {
    mfxU16 ViewId; /*!< View identifier of this dependency structure. */

    mfxU16 NumAnchorRefsL0; /*!< Number of view components for inter-view prediction in the initial reference picture list RefPicList0 for anchor view components. */
    mfxU16 NumAnchorRefsL1; /*!< Number of view components for inter-view prediction in the initial reference picture list RefPicList1 for anchor view components. */
    mfxU16 AnchorRefL0[16]; /*!< View identifiers of the view components for inter-view prediction in the initial reference picture list RefPicList0 for anchor view components. */
    mfxU16 AnchorRefL1[16]; /*!< View identifiers of the view components for inter-view prediction in the initial reference picture list RefPicList1 for anchor view components. */

    mfxU16 NumNonAnchorRefsL0; /*!< Number of view components for inter-view prediction in the initial reference picture list RefPicList0 for non-anchor view components. */
    mfxU16 NumNonAnchorRefsL1; /*!< Number of view components for inter-view prediction in the initial reference picture list RefPicList1 for non-anchor view components. */
    mfxU16 NonAnchorRefL0[16]; /*!< View identifiers of the view components for inter-view prediction in the initial reference picture list RefPicList0 for non-anchor view components. */
    mfxU16 NonAnchorRefL1[16]; /*!< View identifiers of the view components for inter-view prediction in the initial reference picture list RefPicList0 for non-anchor view components. */
} mfxMVCViewDependency;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
  Describes the MVC operation point.
*/
typedef struct {
    mfxU16 TemporalId;     /*!< Temporal identifier of the operation point. */
    mfxU16 LevelIdc;       /*!< Level value signaled for the operation point. */

    mfxU16 NumViews;       /*!< Number of views required for decoding the target output views that correspond to the operation point. */
    mfxU16 NumTargetViews; /*!< Number of target output views for the operation point. */
    mfxU16 *TargetViewId;  /*!< Target output view identifiers for operation point. */
} mfxMVCOperationPoint;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
  Describes the MVC stream information of view dependencies, view identifiers, and operation points. See the ITU*-T H.264 specification chapter H.7.3.2.1.4 for details.
*/
typedef struct  {
    mfxExtBuffer Header;        /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MVC_SEQUENCE_DESCRIPTION. */

    mfxU32 NumView;             /*!< Number of views. */
    mfxU32 NumViewAlloc;        /*!< The allocated view dependency array size. */
    mfxMVCViewDependency *View; /*!< Pointer to a list of the mfxMVCViewDependency. */

    mfxU32 NumViewId;           /*!< Number of view identifiers. */
    mfxU32 NumViewIdAlloc;      /*!< The allocated view identifier array size. */
    mfxU16 *ViewId;             /*!< Pointer to the list of view identifier. */

    mfxU32 NumOP;               /*!< Number of operation points. */
    mfxU32 NumOPAlloc;          /*!< The allocated operation point array size. */
    mfxMVCOperationPoint *OP;   /*!< Pointer to a list of the mfxMVCOperationPoint structure. */

    mfxU16 NumRefsTotal;        /*!< Total number of reference frames in all views required to decode the stream. This value is returned from the MFXVideoDECODE_Decodeheader function. Do not modify this value. */
    mfxU32 Reserved[16];

} mfxExtMVCSeqDesc;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
 Configures views for the decoding output.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MVC_TARGET_VIEWS. */

    mfxU16 TemporalId;      /*!< The temporal identifier to be decoded. */
    mfxU32 NumView;         /*!< The number of views to be decoded. */
    mfxU16 ViewId[1024];    /*!< List of view identifiers to be decoded. */
} mfxExtMVCTargetViews ;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif

