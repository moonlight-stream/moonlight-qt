/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXVP8_H__
#define __MFXVP8_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MFX_CODEC_VP8 = MFX_MAKEFOURCC('V','P','8',' '),
};

/* VP8 CodecProfile*/
enum {
    MFX_PROFILE_VP8_0                       = 0+1,
    MFX_PROFILE_VP8_1                       = 1+1,
    MFX_PROFILE_VP8_2                       = 2+1,
    MFX_PROFILE_VP8_3                       = 3+1,
};

/* Extended Buffer Ids */
enum {
    /*!
       This extended buffer describes VP8 encoder configuration parameters. See the mfxExtVP8CodingOption structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding initialization.
    */
    MFX_EXTBUFF_VP8_CODING_OPTION =   MFX_MAKEFOURCC('V','P','8','E'),
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Describes VP8 coding options. */
typedef struct {
    mfxExtBuffer    Header;             /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VP8_CODING_OPTION. */

    mfxU16   Version;                   /*!< Determines the bitstream version. Corresponds to the same VP8 syntax element in frame_tag. */
    mfxU16   EnableMultipleSegments;    /*!< Set this option to ON to enable segmentation. This is tri-state option. See the CodingOptionValue
                                             enumerator for values of this option. */
    mfxU16   LoopFilterType;            /*!< Select the type of filter (normal or simple). Corresponds to VP8 syntax element filter_type. */
    mfxU16   LoopFilterLevel[4];        /*!< Controls the filter strength. Corresponds to VP8 syntax element loop_filter_level. */
    mfxU16   SharpnessLevel;            /*!< Controls the filter sensitivity. Corresponds to VP8 syntax element sharpness_level. */
    mfxU16   NumTokenPartitions;        /*!< Specifies number of token partitions in the coded frame. */
    mfxI16   LoopFilterRefTypeDelta[4]; /*!< Loop filter level delta for reference type (intra, last, golden, altref). */
    mfxI16   LoopFilterMbModeDelta[4];  /*!< Loop filter level delta for MB modes. */
    mfxI16   SegmentQPDelta[4];         /*!< QP delta for segment. */
    mfxI16   CoeffTypeQPDelta[5];       /*!< QP delta for coefficient type (YDC, Y2AC, Y2DC, UVAC, UVDC). */
    mfxU16   WriteIVFHeaders;           /*!< Set this option to ON to enable insertion of IVF container headers into bitstream. This is tri-state
                                             option. See the CodingOptionValue enumerator for values of this option */
    mfxU32   NumFramesForIVFHeader;     /*!< Specifies number of frames for IVF header when WriteIVFHeaders is ON. */
    mfxU16   reserved[223];
} mfxExtVP8CodingOption;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif

