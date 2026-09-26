/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/
#ifndef __MFXCAMERA_H__
#define __MFXCAMERA_H__

#include "mfxcommon.h"


#if !defined(__GNUC__)
    #pragma warning(disable : 4201)
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*!
  The ExtendedBufferID enumerator itemizes and defines identifiers (BufferId) for extended buffers in camera processing.
  The application should attach these extended buffers to the mfxVideoParam structure to configure camera processing through VideoVPP functions.
  And Implementation capabilities of camera processing features can be delivered by the function MFXQueryImplsDescription via VPP configuration
  mfxVPPDescription.
  */
enum {
    /*!
      This extended buffer is mandatory for camera raw accelerator initialization. See the mfxExtCamPipeControl structure for details.
      The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_PIPECONTROL = MFX_MAKEFOURCC('C', 'P', 'P', 'C'),
    /*!
      This extended buffer defines control parameters for the Camera White Balance filter algorithm. See mfxExtCamWhiteBalance structure for details.
      The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_WHITE_BALANCE = MFX_MAKEFOURCC('C', 'W', 'B', 'L'),
    /*!
     This extended buffer defines control parameters for the Camera Hot Pixel Removal filter algorithm. See mfxExtCamHotPixelRemoval structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
   */
    MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL = MFX_MAKEFOURCC('C', 'H', 'P', 'R'),
    /*!
     This extended buffer defines control parameters for the Camera Black Level Correction filter algorithm. See mfxExtCamBlackLevelCorrection structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION = MFX_MAKEFOURCC('C', 'B', 'L', 'C'),
    /*!
     This extended buffer defines control parameters for the Camera Vignette Correction filter algorithm. See mfxCamVignetteCorrectionParam structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_VIGNETTE_CORRECTION = MFX_MAKEFOURCC('C', 'V', 'G', 'T'),
    /*!
     This extended buffer defines control parameters for the Camera Bayer Denoise filter algorithm. See mfxExtCamBayerDenoise structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_BAYER_DENOISE = MFX_MAKEFOURCC('C', 'D', 'N', 'S'),
    /*!
     This extended buffer defines control parameters for the Camera Color Correction filter algorithm. See mfxExtCamColorCorrection3x3 structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3 = MFX_MAKEFOURCC('C', 'C', '3', '3'),
    /*!
     This extended buffer defines control parameters for the Camera Padding. See mfxExtCamPadding structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_PADDING = MFX_MAKEFOURCC('C', 'P', 'A', 'D'),
    /*!
     This extended buffer defines control parameters for the Camera Forward Gamma Correction filter algorithm. See mfxExtCamFwdGamma structure for details.
     The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_FORWARD_GAMMA_CORRECTION =
        MFX_MAKEFOURCC('C', 'F', 'G', 'C'),
    /*!
    This extended buffer defines control parameters for the Camera Lens Geometry Distortion and Chroma Aberration Correction filter algorithm. See mfxExtCamLensGeomDistCorrection structure for details.
    The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION =
        MFX_MAKEFOURCC('C', 'L', 'G', 'D'),
    /*!
    This extended buffer defines control parameters for the Camera 3DLUT filter algorithm. See mfxExtCam3DLut structure for details.
    The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_3DLUT = MFX_MAKEFOURCC('C', 'L', 'U', 'T'),
    /*!
    This extended buffer defines control parameters for the Camera Total Color Control algorithm. See mfxExtCamTotalColorControl structure for details.
    The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL = MFX_MAKEFOURCC('C', 'T', 'C', 'C'),
    /*!
    This extended buffer defines control parameters for the Camera YUV to RGB conversion algorithm. See mfxExtCamCscYuvRgb structure for details.
    The application should attach this extended buffer to the mfxVideoParam structure to configure camera processing initialization.
    */
    MFX_EXTBUF_CAM_CSC_YUV_RGB = MFX_MAKEFOURCC('C', 'C', 'Y', 'R')
};

/*!
    A enumeration that defines white balance mode.
*/
typedef enum {
    MFX_CAM_WHITE_BALANCE_MANUAL = 0x0001, /*!< White balance manual mode.*/
    MFX_CAM_WHITE_BALANCE_AUTO   = 0x0002 /*!< White balance auto mode.*/
} mfxCamWhiteBalanceMode;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera White Balance filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_WHITE_BALANCE. */
    mfxU32       Mode;   /*!< Specifies one of White Balance operation modes defined in enumeration mfxCamWhiteBalanceMode. */
    mfxF64 R;  /*!< White Balance Red correction.*/
    mfxF64 G0; /*!< White Balance Green Top correction.*/
    mfxF64 B;  /*!< White Balance Blue correction.*/
    mfxF64 G1; /*!< White Balance Green Bottom correction. */
    mfxU32 reserved[8]; /*!< Reserved for future extension. */
} mfxExtCamWhiteBalance;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera Total Color Control filter.
*/
typedef struct {
    mfxExtBuffer  Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL. */
    mfxU16 R; /*!< Red element.*/
    mfxU16 G; /*!< Green element.*/
    mfxU16 B; /*!< Blue element.*/
    mfxU16 C; /*!< Cyan element.*/
    mfxU16 M; /*!< Magenta element.*/
    mfxU16 Y; /*!< Yellow element.*/
    mfxU16 reserved[6]; /*!< Reserved for future extension.*/
} mfxExtCamTotalColorControl;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera YUV to RGB format conversion.
*/
typedef struct {
    mfxExtBuffer Header;  /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_CSC_YUV_RGB. */
    mfxF32 PreOffset[3];  /*!< Specifies offset for conversion from full range RGB input to limited range YUV for input color coordinate.*/
    mfxF32 Matrix[3][3];  /*!< Specifies conversion matrix with CSC coefficients.*/
    mfxF32 PostOffset[3]; /*!< Specifies offset for conversion from full range RGB input to limited range YUV for output color coordinate.*/
    mfxU16 reserved[30];  /*!< Reserved for future extension.*/
} mfxExtCamCscYuvRgb;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera Hot Pixel Removal filter.
*/
typedef struct {
    mfxExtBuffer Header;             /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL. */
    mfxU16 PixelThresholdDifference; /*!< Threshold for Hot Pixel difference. */
    mfxU16 PixelCountThreshold;      /*!< Count pixel detection.*/
    mfxU16 reserved[32];             /*!< Reserved for future extension.*/
} mfxExtCamHotPixelRemoval;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
/*!
    A hint structure that configures Camera black level correction.
*/
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION. */
    mfxU16 R;  /*!< Black Level Red correction.*/
    mfxU16 G0; /*!< Black Level Green Top correction.*/
    mfxU16 B;  /*!< Black Level Blue correction.*/
    mfxU16 G1; /*!< Black Level Green Bottom correction.*/
    mfxU32 reserved[4]; /*!< Reserved for future extension.*/
} mfxExtCamBlackLevelCorrection;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A structure that defines Camera Vignette Correction Element.
*/
typedef struct {
    mfxU8 integer;     /*!< Integer part of correction element.*/
    mfxU8 mantissa;    /*!< Fractional part of correction element.*/
    mfxU8 reserved[6]; /*!< Reserved for future extension.*/
} mfxCamVignetteCorrectionElement;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A structure that defines Camera Vignette Correction Parameters.
*/
typedef struct {
    mfxCamVignetteCorrectionElement R;  /*!< Red correction element.*/
    mfxCamVignetteCorrectionElement G0; /*!< Green top correction element.*/
    mfxCamVignetteCorrectionElement B;  /*!< Blue Correction element.*/
    mfxCamVignetteCorrectionElement G1; /*!< Green bottom correction element.*/
    mfxU32 reserved[4]; /*!< Reserved for future extension.*/
} mfxCamVignetteCorrectionParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
    A hint structure that configures Camera Vignette Correction filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_VIGNETTE_CORRECTION. */
    mfxU32       Width;  /*!< Width of Correction Map 2D buffer in mfxCamVignetteCorrectionParam elements. */
    mfxU32       Height; /*!< Height of Correction Map 2D buffer in mfxCamVignetteCorrectionParam elements. */
    mfxU32       Pitch;  /*!< Pitch of Correction Map 2D buffer in mfxCamVignetteCorrectionParam elements. */
    mfxU32 reserved[7];  /*!< Reserved for future extension.*/

    union {
        mfxCamVignetteCorrectionParam* CorrectionMap; /*!< 2D buffer of mfxCamVignetteCorrectionParam elements.*/
        mfxU64 reserved1;                             /*!< Reserved for alignment on 32bit and 64bit.*/
    };
} mfxExtCamVignetteCorrection;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera Bayer denoise filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_BAYER_DENOISE. */
    mfxU16 Threshold;    /*!< Level of denoise, legal values: [0:63].*/
    mfxU16 reserved[27]; /*!< Reserved for future extension.*/
} mfxExtCamBayerDenoise;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
    A hint structure that configures Camera Color correction filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3. */
    mfxF32 CCM[3][3];    /*!< 3x3 dimension matrix providing RGB Color Correction coefficients.*/
    mfxU32 reserved[32]; /*!< Reserved for future extension.*/
} mfxExtCamColorCorrection3x3;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera Padding.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_PADDING. */
    mfxU16       Top;    /*!< Specify number of padded columns respectively. Currently only 8 pixels supported for all dimensions. */
    mfxU16       Bottom; /*!< Specify number of padded columns respectively. Currently only 8 pixels supported for all dimensions. */
    mfxU16       Left;   /*!< Specify number of padded rows respectively. Currently only 8 pixels supported for all dimensions. */
    mfxU16       Right;  /*!< Specify number of padded rows respectively. Currently only 8 pixels supported for all dimensions. */
    mfxU32 reserved[4];  /*!< Reserved for future extension.*/
} mfxExtCamPadding;
MFX_PACK_END()

/*!
    A enumeration that defines Bayer mode.
*/
typedef enum {
    /*!
     Pixel Representation BG
                          GR.
     */
    MFX_CAM_BAYER_BGGR = 0x0000,
    /*!
     Pixel Representation RG
                          GB.
     */
    MFX_CAM_BAYER_RGGB = 0x0001,
    /*!
     Pixel Representation GB
                          RG.
     */
    MFX_CAM_BAYER_GBRG = 0x0002,
    /*!
     Pixel Representation GR
                          BG.
     */
    MFX_CAM_BAYER_GRBG = 0x0003
} mfxCamBayerFormat;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures camera pipe control.
*/
typedef struct {
    mfxExtBuffer Header;    /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_PIPECONTROL. */
    mfxU16       RawFormat; /*!< Specifies one of the four Bayer patterns defined in mfxCamBayerFormat enumeration. */
    mfxU16 reserved1;       /*!< Reserved for future extension.*/
    mfxU32 reserved[5];     /*!< Reserved for future extension.*/
} mfxExtCamPipeControl;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A structure that specifies forward gamma segment.
*/
typedef struct {
    mfxU16 Pixel; /*!< Pixel value.*/
    mfxU16 Red;   /*!< Corrected Red value.*/
    mfxU16 Green; /*!< Corrected Green value.*/
    mfxU16 Blue;  /*!< Corrected Blue value.*/
} mfxCamFwdGammaSegment;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
    A hint structure that configures Camera Forward Gamma Correction filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_FORWARD_GAMMA_CORRECTION. */

    mfxU16 reserved[19]; /*!< Reserved for future extension.*/
    mfxU16 NumSegments;  /*!< Number of Gamma segments.*/
    union {
        mfxCamFwdGammaSegment* Segment; /*!< Pointer to Gamma segments array.*/
        mfxU64 reserved1;               /*!< Reserved for future extension.*/
    };
} mfxExtCamFwdGamma;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures Camera Lens Geometry Distortion and Chroma Aberration Correction filter.
*/
typedef struct {
    mfxExtBuffer
        Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION. */

    mfxF32 a[3]; /*!< Polynomial coefficients a for R/G/B*/
    mfxF32 b[3]; /*!< Polynomial coefficients b for R/G/B*/
    mfxF32 c[3]; /*!< Polynomial coefficients c for R/G/B*/
    mfxF32 d[3]; /*!< Polynomial coefficients d for R/G/B*/
    mfxU16 reserved[36]; /*!< Reserved for future extension.*/
} mfxExtCamLensGeomDistCorrection;
MFX_PACK_END()

/*!
    A enumeration that defines 3DLUT size.
*/
enum {
    MFX_CAM_3DLUT17_SIZE = (17 * 17 * 17), /*!< 17^3 LUT size*/
    MFX_CAM_3DLUT33_SIZE = (33 * 33 * 33), /*!< 33^3 LUT size*/
    MFX_CAM_3DLUT65_SIZE = (65 * 65 * 65)  /*!< 65^3 LUT size*/
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A structure that defines 3DLUT entry.
*/
typedef struct {
    mfxU16 R; /*!< R channel*/
    mfxU16 G; /*!< G channel*/
    mfxU16 B; /*!< B channel*/
    mfxU16 Reserved; /*!< Reserved for future extension.*/
} mfxCam3DLutEntry;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
    A hint structure that configures Camera 3DLUT filter.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUF_CAM_3DLUT. */

    mfxU16 reserved[10]; /*!< Reserved for future extension.*/
    mfxU32         Size; /*!< LUT size, defined in MFX_CAM_3DLUT17/33/65_SIZE enumeration.*/
    union {
        mfxCam3DLutEntry* Table; /*!< Pointer to mfxCam3DLutEntry, size of each dimension depends on LUT size, e.g. LUT[17][17][17] for 17x17x17 look up table.*/
        mfxU64 reserved1; /*!< Reserved for future extension.*/
    };
} mfxExtCam3DLut;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __MFXCAMERA_H__