/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXBRC_H__
#define __MFXBRC_H__

#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*! See the mfxExtBRC structure for details. */
enum {
    MFX_EXTBUFF_BRC = MFX_MAKEFOURCC('E','B','R','C')
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Describes frame parameters required for external BRC functions.
*/
typedef struct {
    mfxU32 reserved[23];
    mfxU16 SceneChange;     /*!< Frame belongs to a new scene if non zero. */
    mfxU16 LongTerm;        /*!< Frame is a Long Term Reference frame if non zero. */
    mfxU32 FrameCmplx;      /*!< Frame Complexity Frame spatial complexity if non zero. Zero if complexity is not available. */
    mfxU32 EncodedOrder;    /*!< The frame number in a sequence of reordered frames starting from encoder Init. */
    mfxU32 DisplayOrder;    /*!< The frame number in a sequence of frames in display order starting from last IDR. */
    mfxU32 CodedFrameSize;  /*!< Size of the frame in bytes after encoding. */
    mfxU16 FrameType;       /*!< Frame type. See FrameType enumerator for possible values. */
    mfxU16 PyramidLayer;    /*!< B-pyramid or P-pyramid layer that the frame belongs to. */
    mfxU16 NumRecode;       /*!< Number of recodings performed for this frame. */
    mfxU16 NumExtParam;     /*!< Reserved for future use. */
    mfxExtBuffer** ExtParam;/*!< Reserved for future use. */
} mfxBRCFrameParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Specifies controls for next frame encoding provided by external BRC functions.
*/
typedef struct {
    mfxI32 QpY;                     /*!< Frame-level Luma QP. */
    mfxU32 InitialCpbRemovalDelay;  /*!< See initial_cpb_removal_delay in codec standard. Ignored if no HRD control:
                                         mfxExtCodingOption::VuiNalHrdParameters = MFX_CODINGOPTION_OFF. Calculated by encoder if
                                         initial_cpb_removal_delay==0 && initial_cpb_removal_offset == 0 && HRD control is switched on. */
    mfxU32 InitialCpbRemovalOffset; /*!< See initial_cpb_removal_offset in codec standard. Ignored if no HRD control:
                                         mfxExtCodingOption::VuiNalHrdParameters = MFX_CODINGOPTION_OFF. Calculated by encoder if
                                         initial_cpb_removal_delay==0 && initial_cpb_removal_offset == 0 && HRD control is switched on. */
    mfxU32 reserved1[7];
    mfxU32 MaxFrameSize;            /*!< Max frame size in bytes. Option for repack feature. Driver calls PAK until current frame size is
                                         less than or equal to MaxFrameSize, or number of repacking for this frame is equal to MaxNumRePak. Repack is available
                                         if there is driver support, MaxFrameSize !=0, and MaxNumRePak != 0. Ignored if MaxNumRePak == 0. */
    mfxU8  DeltaQP[8];              /*!< Option for repack feature. Ignored if MaxNumRePak == 0 or MaxNumRePak==0. If current
                                         frame size > MaxFrameSize and/or number of repacking (nRepack) for this frame <= MaxNumRePak,
                                         PAK is called with QP = mfxBRCFrameCtrl::QpY + Sum(DeltaQP[i]), where i = [0,nRepack].
                                         Non zero DeltaQP[nRepack] are ignored if nRepack > MaxNumRePak.
                                         If repacking feature is on ( MaxFrameSize & MaxNumRePak are not zero), it is calculated by the encoder. */
    mfxU16 MaxNumRepak;             /*!< Number of possible repacks in driver if current frame size > MaxFrameSize. Ignored if MaxFrameSize==0.
                                         See MaxFrameSize description. Possible values are in the range of 0 to 8. */
    mfxU16 NumExtParam;             /*!< Reserved for future use. */
    mfxExtBuffer** ExtParam;        /*!< Reserved for future use. */
} mfxBRCFrameCtrl;
MFX_PACK_END()

/*! The BRCStatus enumerator itemizes instructions to the encoder by mfxExtBrc::Update. */
enum {
    MFX_BRC_OK                = 0, /*!< CodedFrameSize is acceptable, no further recoding/padding/skip required, proceed to next frame. */
    MFX_BRC_BIG_FRAME         = 1, /*!< Coded frame is too big, recoding required. */
    MFX_BRC_SMALL_FRAME       = 2, /*!< Coded frame is too small, recoding required. */
    MFX_BRC_PANIC_BIG_FRAME   = 3, /*!< Coded frame is too big, no further recoding possible - skip frame. */
    MFX_BRC_PANIC_SMALL_FRAME = 4  /*!< Coded frame is too small, no further recoding possible - required padding to mfxBRCFrameStatus::MinFrameSize. */
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Specifies instructions for the encoder provided by external BRC after each frame encoding. See the BRCStatus enumerator for details.
*/
typedef struct {
    mfxU32 MinFrameSize;    /*!< Size in bytes, coded frame must be padded to when Status = MFX_BRC_PANIC_SMALL_FRAME. */
    mfxU16 BRCStatus;       /*!< BRC status. See the BRCStatus enumerator for possible values. */
    mfxU16 reserved[25];
    mfxHDL reserved1;
} mfxBRCFrameStatus;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Contains a set of callbacks to perform external bitrate control. Can be attached to the mfxVideoParam structure during
   encoder initialization. Set the mfxExtCodingOption2::ExtBRC option to ON to make the encoder use the external BRC instead of the native one.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_BRC. */

    mfxU32 reserved[14];
    mfxHDL pthis;        /*!< Pointer to the BRC object. */

    /*!
       @brief Initializes the BRC session according to parameters from input mfxVideoParam and attached structures. It does not modify the input mfxVideoParam and attached structures. Invoked during MFXVideoENCODE_Init.

       @param[in]      pthis Pointer to the BRC object.
       @param[in]      par   Pointer to the mfxVideoParam structure that was used for the encoder initialization.

       @return
          MFX_ERR_NONE               The function completed successfully. \n
          MFX_ERR_UNSUPPORTED        The function detected unsupported video parameters.
    */
    mfxStatus (MFX_CDECL *Init)         (mfxHDL pthis, mfxVideoParam* par);

    /*!
       @brief Resets BRC session according to new parameters. It does not modify the input mfxVideoParam and attached structures. Invoked during MFXVideoENCODE_Reset.

       @param[in]      pthis Pointer to the BRC object.
       @param[in]      par   Pointer to the mfxVideoParam structure that was used for the encoder initialization.

       @return
          MFX_ERR_NONE                       The function completed successfully. \n
          MFX_ERR_UNSUPPORTED                The function detected unsupported video parameters. \n
          MFX_ERR_INCOMPATIBLE_VIDEO_PARAM   The function detected that the video parameters provided by the application are incompatible with
                                             initialization parameters. Reset requires additional memory allocation and cannot be executed.
    */
    mfxStatus (MFX_CDECL *Reset)        (mfxHDL pthis, mfxVideoParam* par);

    /*!
       @brief Deallocates any internal resources acquired in Init for this BRC session. Invoked during MFXVideoENCODE_Close.

       @param[in]      pthis Pointer to the BRC object.

       @return
          MFX_ERR_NONE               The function completed successfully.
    */
    mfxStatus (MFX_CDECL *Close)        (mfxHDL pthis);

    /*! @brief Returns controls (@p ctrl) to encode next frame based on info from input mfxBRCFrameParam structure (@p par) and
               internal BRC state. Invoked asynchronously before each frame encoding or recoding.

       @param[in]      pthis Pointer to the BRC object.
       @param[in]      par   Pointer to the mfxVideoParam structure that was used for the encoder initialization.
       @param[out]     ctrl  Pointer to the output mfxBRCFrameCtrl structure.

       @return
          MFX_ERR_NONE               The function completed successfully.
    */
    mfxStatus (MFX_CDECL* GetFrameCtrl) (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl);

    /*!
       @brief Updates internal BRC state and returns status to instruct encoder whether it should recode the previous frame,
               skip the previous frame, do padding, or proceed to next frame based on info from input mfxBRCFrameParam and mfxBRCFrameCtrl structures.
               Invoked asynchronously after each frame encoding or recoding.

       @param[in]      pthis Pointer to the BRC object.
       @param[in]      par   Pointer to the mfxVideoParam structure that was used for the encoder initialization.
       @param[in]      ctrl  Pointer to the output mfxBRCFrameCtrl structure.
       @param[in]    status  Pointer to the output mfxBRCFrameStatus structure.


       @return
          MFX_ERR_NONE               The function completed successfully.
    */
    mfxStatus (MFX_CDECL* Update)       (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status);

    mfxHDL reserved1[10];
} mfxExtBRC;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif

