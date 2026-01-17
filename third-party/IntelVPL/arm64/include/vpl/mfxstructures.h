/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXSTRUCTURES_H__
#define __MFXSTRUCTURES_H__
#include "mfxcommon.h"

#if !defined (__GNUC__)
#pragma warning(disable: 4201)
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Frame ID for MVC */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Describes the view and layer of a frame picture. */
typedef struct {
    mfxU16      TemporalId;  /*!< The temporal identifier as defined in the annex H of the ITU*-T H.264 specification. */
    mfxU16      PriorityId;  /*!< Reserved and must be zero. */
    union {
        struct {
            mfxU16  DependencyId; /*!< Reserved for future use. */
            mfxU16  QualityId;    /*!< Reserved for future use. */
        };
        struct {
            mfxU16  ViewId; /*!< The view identifier as defined in the annex H of the ITU-T H.264 specification. */
        };
    };
} mfxFrameId;
MFX_PACK_END()

/* This struct has 4-byte alignment for binary compatibility with previously released versions of API. */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies properties of video frames. See also "Configuration Parameter Constraints" chapter. */
typedef struct {
    mfxU32  reserved[4]; /*!< Reserved for future use. */
    /*! The unique ID of each VPP channel set by application. It's required that during Init/Reset application fills ChannelId for
        each mfxVideoChannelParam provided by the application and the SDK sets it back to the correspondent
        mfxSurfaceArray::mfxFrameSurface1 to distinguish different channels. It's expected that surfaces for some channels might be
        returned with some delay so application has to use mfxFrameInfo::ChannelId to distinguish what returned surface belongs to
        what VPP channel. Decoder's initialization parameters are always sent through channel with mfxFrameInfo::ChannelId equals to
        zero. It's allowed to skip setting of decoder's parameters for simplified decoding procedure */
    mfxU16  ChannelId;
    /*! Number of bits used to represent luma samples.
        @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU16  BitDepthLuma;
    /*! Number of bits used to represent chroma samples.
        @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU16  BitDepthChroma;
    /*! The Shift flag indicates whether the values of luma and chroma samples are shifted. For the decoding process, the recommended value
        is specified by the DecodeHeader or GetVideoParam API. A value of one indicates that the luma and chroma sample values are shifted, 
        while a value of zero indicates that there is no shift. Please refer to the example data alignment provided below.
        
        @note Not all codecs and implementations support this flag. Use the Query API function to check if this feature is supported. 
        AVC and HEVC allow users to set the Shift flag to either 0 or 1. However, setting the Shift flag to a value different from the 
        recommended one may lead to increased CPU utilization. For other codecs, attempting to set the Shift flag to a value other than 
        the recommended one will result in an error status.
    */
    mfxU16  Shift;
    mfxFrameId FrameId; /*!< Describes the view and layer of a frame picture. */
    mfxU32  FourCC;     /*!< FourCC code of the color format. See the ColorFourCC enumerator for details. */
    union {
        struct { /* Frame parameters */
            /*! Width of the video frame in pixels. Must be a multiple of 16.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that scaling operation is not requested. */
            mfxU16  Width;
            /*! Height of the video frame in pixels. Must be a multiple of 16 for progressive frame sequence and a multiple of 32 otherwise.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that scaling operation is not requested. */
            mfxU16  Height;

            /*! @{
             @name ROI
             The region of interest of the frame. Specify the display width and height in mfxVideoParam. */
            /*! X coordinate.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that cropping operation is not requested. */
            mfxU16  CropX;
            /*! Y coordinate.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that cropping operation is not requested. */
            mfxU16  CropY;
            /*! Width in pixels.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that cropping operation is not requested. */
            mfxU16  CropW;
            /*! Height in pixels.
                In case of fused operation of decode plus VPP it can be set to zero to signalize that cropping operation is not requested. */
            mfxU16  CropH;
            /*! @} */
        };
        struct { /* Buffer parameters (for plain formats like P8) */
            mfxU64 BufferSize; /*!< Size of frame buffer in bytes. Valid only for plain formats (when FourCC is P8). In this case, Width, Height, and crop values are invalid. */
            mfxU32 reserved5;
        };
    };

    /*! @{
        @name FrameRate
        Specify the frame rate with the following formula: FrameRateExtN / FrameRateExtD.

        For encoding, frame rate must be specified. For decoding, frame rate may be unspecified (FrameRateExtN and FrameRateExtD
        are all zeros.) In this case, the frame rate is defaulted to 0 frames per second, and timestamp will be calculated by 30fps in SDK.

        In decoding process:

        If there is frame rate information in bitstream, MFXVideoDECODE_DecodeHeader will carry actual frame rate in FrameRateExtN and FrameRateExtD parameters.
        MFXVideoDECODE_Init, MFXVideoDECODE_Query, MFXVideoDECODE_DecodeFrameAsync and MFXVideoDECODE_GetVideoParam will also carry these values for frame rate.
        Timestamp will be calculated by the actual frame rate.

        If there is no frame rate information in bitstream, MFXVideoDECODE_DecodeHeader will assign 0 for frame rate in FrameRateExtN and FrameRateExtD parameters.
        MFXVideoDECODE_Init, MFXVideoDECODE_Query, MFXVideoDECODE_DecodeFrameAsync and MFXVideoDECODE_GetVideoParam will also assign 0 for frame rate. Timestamp will be calculated by 30fps.

        If these two parameters are modified through MFXVideoDECODE_Init, then the modified values for frame rate will be used in
        MFXVideoDECODE_Query, MFXVideoDECODE_DecodeFrameAsync and MFXVideoDECODE_GetVideoParam. Timestamps will be calculated using the modified values.
    */
    mfxU32  FrameRateExtN; /*!< Frame rate numerator. */
    mfxU32  FrameRateExtD; /*!< Frame rate denominator. */
    /*! @} */
    mfxU16  reserved3;

    /*! @{
        @name AspectRatio
        AspectRatioW and AspectRatioH are used to specify the sample aspect ratio. If sample aspect ratio is explicitly defined by the standards (see
        Table 6-3 in the MPEG-2 specification or Table E-1 in the H.264 specification), AspectRatioW and AspectRatioH should be the defined values.
        Otherwise, the sample aspect ratio can be derived as follows:

        @li @c AspectRatioW=display_aspect_ratio_width*display_height

        @li @c AspectRatioH=display_aspect_ratio_height*display_width

        For MPEG-2, the above display aspect ratio must be one of the defined values in Table 6-3 in the MPEG-2 specification. For H.264, there is no restriction
        on display aspect ratio values.

        If both parameters are zero, the encoder uses the default value of sample aspect ratio.
    */
    mfxU16  AspectRatioW; /*!< Aspect Ratio for width. */
    mfxU16  AspectRatioH; /*!< Aspect Ratio for height. */
    /*! @} */

    mfxU16  PicStruct;    /*!< Picture type as specified in the PicStruct enumerator. */
    mfxU16  ChromaFormat; /*!< Color sampling method. Value is the same as that of ChromaFormatIdc.
                               ChromaFormat is not defined if FourCC is zero.*/
    mfxU16  reserved2;
} mfxFrameInfo;
MFX_PACK_END()

/*! The ColorFourCC enumerator itemizes color formats. */
enum {
    MFX_FOURCC_NV12         = MFX_MAKEFOURCC('N','V','1','2'),   /*!< NV12 color planes. Native format for 4:2:0/8b Gen hardware implementation. */
    MFX_FOURCC_YV12         = MFX_MAKEFOURCC('Y','V','1','2'),   /*!< YV12 color planes. */
    MFX_FOURCC_NV16         = MFX_MAKEFOURCC('N','V','1','6'),   /*!< 4:2:2 color format with similar to NV12 layout. */
    MFX_FOURCC_YUY2         = MFX_MAKEFOURCC('Y','U','Y','2'),   /*!< YUY2 color planes. */
    MFX_FOURCC_RGB565       = MFX_MAKEFOURCC('R','G','B','2'),   /*!< 2 bytes per pixel, uint16 in little-endian format, where 0-4 bits are blue, bits 5-10 are green and bits 11-15 are red. */
    /*! RGB 24 bit planar layout (3 separate channels, 8-bits per sample each). This format should be mapped to D3DFMT_R8G8B8 or VA_FOURCC_RGBP. */
    MFX_FOURCC_RGBP         = MFX_MAKEFOURCC('R','G','B','P'),
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_FOURCC_RGB3)         = MFX_MAKEFOURCC('R','G','B','3'),   /* Deprecated. */
    MFX_FOURCC_RGB4         = MFX_MAKEFOURCC('R','G','B','4'),   /*!< RGB4 (RGB32) color planes. BGRA is the order, 'B' is 8 LSBs in 32-bit unit, then 8 bits for 'G' channel, then 'R' and 'A' channels. */
    /*!
       Internal color format. The application should use the following functions to create a surface that corresponds to the Direct3D* version in use.

       For Direct3D* 9: IDirectXVideoDecoderService::CreateSurface()

       For Direct3D* 11: ID3D11Device::CreateBuffer()
    */
    MFX_FOURCC_P8           = 41,
    /*!
       Internal color format. The application should use the following functions to create a surface that corresponds to the Direct3D* version in use.

       For Direct3D 9: IDirectXVideoDecoderService::CreateSurface()

       For Direct3D 11: ID3D11Device::CreateTexture2D()
    */
    MFX_FOURCC_P8_TEXTURE   = MFX_MAKEFOURCC('P','8','M','B'),
    MFX_FOURCC_P010         = MFX_MAKEFOURCC('P','0','1','0'), /*!< P010 color format. This is 10 bit per sample format with similar to NV12 layout. This format should be mapped to DXGI_FORMAT_P010. */
    MFX_FOURCC_P016         = MFX_MAKEFOURCC('P','0','1','6'), /*!< P016 color format. This is 16 bit per sample format with similar to NV12 layout. This format should be mapped to DXGI_FORMAT_P016. */
    MFX_FOURCC_P210         = MFX_MAKEFOURCC('P','2','1','0'), /*!< 10 bit per sample 4:2:2 color format with similar to NV12 layout. */
    MFX_FOURCC_BGR4         = MFX_MAKEFOURCC('B','G','R','4'), /*!< RGBA color format. It is similar to MFX_FOURCC_RGB4 but with different order of channels. 'R' is 8 LSBs in 32-bit unit, then 8 bits for 'G' channel, then 'B' and 'A' channels. */
    MFX_FOURCC_A2RGB10      = MFX_MAKEFOURCC('R','G','1','0'), /*!< 10 bits ARGB color format packed in 32 bits. 'A' channel is two MSBs, then 'R', then 'G' and then 'B' channels. This format should be mapped to DXGI_FORMAT_R10G10B10A2_UNORM or D3DFMT_A2R10G10B10. */
    MFX_FOURCC_ARGB16       = MFX_MAKEFOURCC('R','G','1','6'), /*!< 16 bits ARGB color format packed in 64 bits. 'A' channel is 16 MSBs, then 'R', then 'G' and then 'B' channels. This format should be mapped to DXGI_FORMAT_R16G16B16A16_UNORM or D3DFMT_A16B16G16R16 formats. */
    MFX_FOURCC_ABGR16       = MFX_MAKEFOURCC('B','G','1','6'), /*!< 16 bits ABGR color format packed in 64 bits. 'A' channel is 16 MSBs, then 'B', then 'G' and then 'R' channels. This format should be mapped to DXGI_FORMAT_R16G16B16A16_UNORM or D3DFMT_A16B16G16R16 formats. */
    MFX_FOURCC_R16          = MFX_MAKEFOURCC('R','1','6','U'), /*!< 16 bits single channel color format. This format should be mapped to DXGI_FORMAT_R16_TYPELESS or D3DFMT_R16F. */
    MFX_FOURCC_AYUV         = MFX_MAKEFOURCC('A','Y','U','V'), /*!< YUV 4:4:4, AYUV color format. This format should be mapped to DXGI_FORMAT_AYUV. */
    MFX_FOURCC_AYUV_RGB4    = MFX_MAKEFOURCC('A','V','U','Y'), /*!< RGB4 stored in AYUV surface. This format should be mapped to DXGI_FORMAT_AYUV. */
    MFX_FOURCC_UYVY         = MFX_MAKEFOURCC('U','Y','V','Y'), /*!< UYVY color planes. Same as YUY2 except the byte order is reversed. */
    MFX_FOURCC_Y210         = MFX_MAKEFOURCC('Y','2','1','0'), /*!< 10 bit per sample 4:2:2 packed color format with similar to YUY2 layout. This format should be mapped to DXGI_FORMAT_Y210. */
    MFX_FOURCC_Y410         = MFX_MAKEFOURCC('Y','4','1','0'), /*!< 10 bit per sample 4:4:4 packed color format. This format should be mapped to DXGI_FORMAT_Y410. */
    MFX_FOURCC_Y216         = MFX_MAKEFOURCC('Y','2','1','6'), /*!< 16 bit per sample 4:2:2 packed color format with similar to YUY2 layout. This format should be mapped to DXGI_FORMAT_Y216. */
    MFX_FOURCC_Y416         = MFX_MAKEFOURCC('Y','4','1','6'), /*!< 16 bit per sample 4:4:4 packed color format. This format should be mapped to DXGI_FORMAT_Y416. */
    MFX_FOURCC_NV21         = MFX_MAKEFOURCC('N', 'V', '2', '1'), /*!< Same as NV12 but with weaved V and U values. */
    MFX_FOURCC_IYUV         = MFX_MAKEFOURCC('I', 'Y', 'U', 'V'), /*!< Same as  YV12 except that the U and V plane order is reversed. */
    MFX_FOURCC_I010         = MFX_MAKEFOURCC('I', '0', '1', '0'), /*!< 10-bit YUV 4:2:0, each component has its own plane. */
    MFX_FOURCC_I210         = MFX_MAKEFOURCC('I', '2', '1', '0'), /*!< 10-bit YUV 4:2:2, each component has its own plane. */
    MFX_FOURCC_I420         = MFX_FOURCC_IYUV,                 /*!< Alias for the IYUV color format. */
    MFX_FOURCC_I422         = MFX_MAKEFOURCC('I', '4', '2', '2'), /*!< Same as YV16 except that the U and V plane order is reversed */
    MFX_FOURCC_BGRA         = MFX_FOURCC_RGB4,                 /*!< Alias for the RGB4 color format. */
    /*! BGR 24 bit planar layout (3 separate channels, 8-bits per sample each). This format should be mapped to VA_FOURCC_BGRP. */
    MFX_FOURCC_BGRP         = MFX_MAKEFOURCC('B','G','R','P'),
    /*! 8bit per sample 4:4:4 format packed in 32 bits, X=unused/undefined, 'X' channel is 8 MSBs, then 'Y', then 'U', and then 'V' channels. This format should be mapped to VA_FOURCC_XYUV. */
    MFX_FOURCC_XYUV         = MFX_MAKEFOURCC('X','Y','U','V'),
    MFX_FOURCC_ABGR16F      = MFX_MAKEFOURCC('B', 'G', 'R', 'F'),  /*!< 16 bits float point ABGR color format packed in 64 bits. 'A' channel is 16 MSBs, then 'B', then 'G' and then 'R' channels. This format should be mapped to DXGI_FORMAT_R16G16B16A16_FLOAT or D3DFMT_A16B16G16R16F formats.. */
};

/* PicStruct */
enum {
    MFX_PICSTRUCT_UNKNOWN       =0x00,  /*!< Unspecified or mixed progressive/interlaced/field pictures. */
    MFX_PICSTRUCT_PROGRESSIVE   =0x01,  /*!< Progressive picture. */
    MFX_PICSTRUCT_FIELD_TFF     =0x02,  /*!< Top field in first interlaced picture.  */
    MFX_PICSTRUCT_FIELD_BFF     =0x04,  /*!< Bottom field in first interlaced picture.  */

    MFX_PICSTRUCT_FIELD_REPEATED=0x10,  /*!< First field repeated: pic_struct=5 or 6 in H.264. */
    MFX_PICSTRUCT_FRAME_DOUBLING=0x20,  /*!< Double the frame for display: pic_struct=7 in H.264. */
    MFX_PICSTRUCT_FRAME_TRIPLING=0x40,  /*!< Triple the frame for display: pic_struct=8 in H.264. */

    MFX_PICSTRUCT_FIELD_SINGLE      =0x100, /*!< Single field in a picture. */
    MFX_PICSTRUCT_FIELD_TOP         =MFX_PICSTRUCT_FIELD_SINGLE | MFX_PICSTRUCT_FIELD_TFF, /*!< Top field in a picture: pic_struct = 1 in H.265. */
    MFX_PICSTRUCT_FIELD_BOTTOM      =MFX_PICSTRUCT_FIELD_SINGLE | MFX_PICSTRUCT_FIELD_BFF, /*!< Bottom field in a picture: pic_struct = 2 in H.265. */
    MFX_PICSTRUCT_FIELD_PAIRED_PREV =0x200, /*!< Paired with previous field: pic_struct = 9 or 10 in H.265. */
    MFX_PICSTRUCT_FIELD_PAIRED_NEXT =0x400, /*!< Paired with next field: pic_struct = 11 or 12 in H.265 */
};

/*! The ChromaFormatIdc enumerator itemizes color-sampling formats. */
enum {
    MFX_CHROMAFORMAT_MONOCHROME =0, /*!< Monochrome. */
    MFX_CHROMAFORMAT_YUV420     =1, /*!< 4:2:0 color. */
    MFX_CHROMAFORMAT_YUV422     =2, /*!< 4:2:2 color. */
    MFX_CHROMAFORMAT_YUV444     =3, /*!< 4:4:4 color. */
    MFX_CHROMAFORMAT_YUV400     = MFX_CHROMAFORMAT_MONOCHROME, /*!< Equal to monochrome. */
    MFX_CHROMAFORMAT_YUV411     = 4, /*!< 4:1:1 color. */
    MFX_CHROMAFORMAT_YUV422H    = MFX_CHROMAFORMAT_YUV422, /*!< 4:2:2 color, horizontal sub-sampling. It is equal to 4:2:2 color. */
    MFX_CHROMAFORMAT_YUV422V    = 5, /*!< 4:2:2 color, vertical sub-sampling. */
    MFX_CHROMAFORMAT_RESERVED1  = 6  /*!< Reserved. */
};

enum {
    MFX_TIMESTAMP_UNKNOWN = -1 /*!< Indicates that time stamp is unknown for this frame/bitstream portion. */
};

enum {
    MFX_FRAMEORDER_UNKNOWN = -1 /*!< Unused entry or API functions that generate the frame output do not use this frame. */
};

/*! The FrameDataFlag enumerator itemizes DataFlag value in mfxFrameData. */
enum {
    MFX_FRAMEDATA_TIMESTAMP_UNKNOWN  = 0x0000,/*!< Indicates the time stamp of this frame is unknown and will be calculated by SDK. */
    MFX_FRAMEDATA_ORIGINAL_TIMESTAMP = 0x0001 /*!< Indicates the time stamp of this frame is not calculated and is a pass-through of the original time stamp. */
};

/*! Corrupted in mfxFrameData */
enum {
    MFX_CORRUPTION_NO              = 0x0000, /*!< No corruption. */
    MFX_CORRUPTION_MINOR           = 0x0001, /*!< Minor corruption in decoding certain macro-blocks. */
    MFX_CORRUPTION_MAJOR           = 0x0002, /*!< Major corruption in decoding the frame - incomplete data, for example. */
    MFX_CORRUPTION_ABSENT_TOP_FIELD           = 0x0004, /*!< Top field of frame is absent in bitstream. Only bottom field has been decoded. */
    MFX_CORRUPTION_ABSENT_BOTTOM_FIELD           = 0x0008, /*!< Bottom field of frame is absent in bitstream. Only top filed has been decoded. */
    MFX_CORRUPTION_REFERENCE_FRAME = 0x0010, /*!< Decoding used a corrupted reference frame. A corrupted reference frame was used for decoding this
                                                frame. For example, if the frame uses a reference frame that was decoded with minor/major corruption flag, then this
                                                frame is also marked with a reference corruption flag. */
    MFX_CORRUPTION_REFERENCE_LIST  = 0x0020, /*!< The reference list information of this frame does not match what is specified in the Reference Picture Marking
                                                  Repetition SEI message. (ITU-T H.264 D.1.8 dec_ref_pic_marking_repetition) */
#ifdef ONEVPL_EXPERIMENTAL
    MFX_CORRUPTION_HW_RESET        = 0x0040  /*!< The hardware reset is reported from media driver. */
#endif
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies "pixel" in Y410 color format. */
typedef struct
{
    mfxU32 U : 10; /*!< U component. */
    mfxU32 Y : 10; /*!< Y component. */
    mfxU32 V : 10; /*!< V component. */
    mfxU32 A :  2; /*!< A component. */
} mfxY410;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies "pixel" in Y416 color format. */
typedef struct
{
    mfxU32 U : 16; /*!< U component. */
    mfxU32 Y : 16; /*!< Y component. */
    mfxU32 V : 16; /*!< V component. */
    mfxU32 A : 16; /*!< A component. */
} mfxY416;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies "pixel" in A2RGB10 color format */
typedef struct
{
    mfxU32 B : 10; /*!< B component. */
    mfxU32 G : 10; /*!< G component. */
    mfxU32 R : 10; /*!< R component. */
    mfxU32 A :  2; /*!< A component. */
} mfxA2RGB10;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies "pixel" in ABGR 16 bit half float point color format */
typedef struct
{
    mfxFP16 R; /*!< R component. */
    mfxFP16 G; /*!< G component. */
    mfxFP16 B; /*!< B component. */
    mfxFP16 A; /*!< A component. */
} mfxABGR16FP;
MFX_PACK_END()

/*! Describes frame buffer pointers. */
MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
typedef struct {
    /*!  @name Extension Buffers */
    /*! @{ */
    union {
        mfxExtBuffer **ExtParam; /*!< Points to an array of pointers to the extra configuration structures. See the ExtendedBufferID
                                      enumerator for a list of extended configurations. */
        mfxU64       reserved2;
    };
    mfxU16  NumExtParam; /*!< The number of extra configuration structures attached to this structure. */
    /*! @} */

    /*!  @name General members */
    /*! @{ */
    mfxU16      reserved[9]; /*!< Reserved for future use. */
    mfxU16      MemType;     /*!< Allocated memory type. See the ExtMemFrameType enumerator for details. Used for better integration of
                                  3rd party plugins into the pipeline. */
    mfxU16      PitchHigh;   /*!< Distance in bytes between the start of two consecutive rows in a frame. */

    mfxU64      TimeStamp;   /*!< Time stamp of the video frame in units of 90KHz. Divide TimeStamp by 90,000 (90 KHz) to obtain the time in seconds.
                                  A value of MFX_TIMESTAMP_UNKNOWN indicates that there is no time stamp. */
    mfxU32      FrameOrder;  /*!< Current frame counter for the top field of the current frame. An invalid value of MFX_FRAMEORDER_UNKNOWN indicates that
                            API functions that generate the frame output do not use this frame. */
    mfxU16      Locked;      /*!< Counter flag for the application. If Locked is greater than zero then the application locks the frame or field pair.
                                  Do not move, alter or delete the frame. */
    union{
        mfxU16  Pitch;
        mfxU16  PitchLow;    /*!< Distance in bytes between the start of two consecutive rows in a frame. */
    };
    /*! @} */

    /*!
        @name Color Planes
        Data pointers to corresponding color channels (planes). The frame buffer pointers must be 16-byte aligned. The application has to specify pointers to
        all color channels even for packed formats. For example, for YUY2 format the application must specify Y, U, and V pointers.
        For RGB32 format, the application must specify R, G, B, and A pointers.
    */
    /*! @{ */
    union {
        mfxU8   *Y; /*!< Y channel. */
        mfxU16  *Y16; /*!< Y16 channel. */
        mfxU8   *R; /*!< R channel. */
    };
    union {
        mfxU8   *UV;    /*!< UV channel for UV merged formats. */
        mfxU8   *VU;    /*!< YU channel for VU merged formats. */
        mfxU8   *CbCr;  /*!< CbCr channel for CbCr merged formats. */
        mfxU8   *CrCb;  /*!< CrCb channel for CrCb merged formats. */
        mfxU8   *Cb;    /*!< Cb channel. */
        mfxU8   *U;     /*!< U channel. */
        mfxU16  *U16;   /*!< U16 channel. */
        mfxU8   *G;     /*!< G channel. */
        mfxY410 *Y410;  /*!< T410 channel for Y410 format (merged AVYU). */
        mfxY416 *Y416;  /*!< This format is a packed 16-bit representation that includes 16 bits of alpha. */
    };
    union {
        mfxU8   *Cr;    /*!< Cr channel. */
        mfxU8   *V;     /*!< V channel. */
        mfxU16  *V16;   /*!< V16 channel. */
        mfxU8   *B;     /*!< B channel. */
        mfxA2RGB10 *A2RGB10; /*!< A2RGB10 channel for A2RGB10 format (merged ARGB). */
        mfxABGR16FP* ABGRFP16; /*!< ABGRFP16 channel for half float ARGB format (use this merged one due to no separate FP16 Alpha Channel). */
    };
    mfxU8       *A;     /*!< A channel. */
    mfxMemId    MemId;  /*!< Memory ID of the data buffers. Ignored if any of the preceding data pointers is non-zero. */
    /*! @} */

    /*!
        @name Additional Flags
    */
    /*! @{ */
    mfxU16  Corrupted; /*!< Some part of the frame or field pair is corrupted. See the Corruption enumerator for details. */
    mfxU16  DataFlag;  /*!< Additional flags to indicate frame data properties. See the FrameDataFlag enumerator for details. */
    /*! @} */
} mfxFrameData;
MFX_PACK_END()

/*! The mfxHandleType enumerator itemizes system handle types that implementations might use. */
typedef enum {
    MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9         = 1,  /*!< Pointer to the IDirect3DDeviceManager9 interface. See Working with Microsoft* DirectX* Applications for more details on how to use this handle. */
    MFX_HANDLE_D3D9_DEVICE_MANAGER              = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, /*!< Pointer to the IDirect3DDeviceManager9 interface. See Working with Microsoft* DirectX* Applications for more details on how to use this handle. */
    MFX_HANDLE_RESERVED1                        = 2,  /* Reserved.  */
    MFX_HANDLE_D3D11_DEVICE                     = 3,  /*!< Pointer to the ID3D11Device interface. See Working with Microsoft* DirectX* Applications for more details on how to use this handle. */
    MFX_HANDLE_VA_DISPLAY                       = 4,  /*!< VADisplay interface. See Working with VA-API Applications for more details on how to use this handle. */
    MFX_HANDLE_RESERVED3                        = 5,  /* Reserved.  */
    MFX_HANDLE_VA_CONFIG_ID                     = 6,  /*!< Pointer to VAConfigID interface. It represents external VA config for Common Encryption usage model. */
    MFX_HANDLE_VA_CONTEXT_ID                    = 7,  /*!< Pointer to VAContextID interface. It represents external VA context for Common Encryption usage model. */
    MFX_HANDLE_CM_DEVICE                        = 8,  /*!< Pointer to CmDevice interface ( Intel(r) C for Metal Runtime ). */
    MFX_HANDLE_HDDLUNITE_WORKLOADCONTEXT        = 9,  /*!< Pointer to HddlUnite::WorkloadContext interface. */
    MFX_HANDLE_PXP_CONTEXT                      = 10, /*!< Pointer to PXP context for protected content support. */

    MFX_HANDLE_CONFIG_INTERFACE                 = 1000,  /*!< Pointer to interface of type mfxConfigInterface. */
#ifdef ONEVPL_EXPERIMENTAL
    MFX_HANDLE_MEMORY_INTERFACE                 = 1001,  /*!< Pointer to interface of type mfxMemoryInterface. */
#endif
} mfxHandleType;

/*! The mfxMemoryFlags enumerator specifies memory access mode. */
typedef enum
{
    MFX_MAP_READ  = 0x1, /*!< The surface is mapped for reading. */
    MFX_MAP_WRITE = 0x2, /*!< The surface is mapped for writing. */
    MFX_MAP_READ_WRITE = MFX_MAP_READ|MFX_MAP_WRITE, /*!< The surface is mapped for reading and writing.  */
    /*!
     * The mapping would be done immediately without any implicit synchronizations.
     * \attention This flag is optional.
     */
    MFX_MAP_NOWAIT = 0x10
} mfxMemoryFlags;

#define MFX_FRAMESURFACE1_VERSION MFX_STRUCT_VERSION(1, 1)

/* Frame Surface */
MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*! Defines the uncompressed frames surface information and data buffers.
    The frame surface is in the frame or complementary field pairs of pixels up to four color-channels, in two parts:
    mfxFrameInfo and mfxFrameData.
*/
typedef struct {
    union
    {
        struct mfxFrameSurfaceInterface*  FrameInterface;       /*!< Specifies interface to work with surface. */
        mfxU32  reserved[2];
    };
    mfxStructVersion Version; /*!< Specifies version of mfxFrameSurface1 structure. */
    mfxU16          reserved1[3];
    mfxFrameInfo    Info; /*!< Specifies surface properties. */
    mfxFrameData    Data; /*!< Describes the actual frame buffer. */
} mfxFrameSurface1;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL

/*! The mfxSurfaceType enumerator specifies the surface type described by mfxSurfaceHeader. */
typedef enum {
    MFX_SURFACE_TYPE_UNKNOWN               = 0,      /*!< Unknown surface type. */

    MFX_SURFACE_TYPE_D3D11_TEX2D           = 2,      /*!< D3D11 surface of type ID3D11Texture2D. */
    MFX_SURFACE_TYPE_VAAPI                 = 3,      /*!< VA-API surface. */
    MFX_SURFACE_TYPE_OPENCL_IMG2D          = 4,      /*!< OpenCL 2D image (cl_mem). */
    MFX_SURFACE_TYPE_D3D12_TEX2D           = 5,      /*!< D3D12 surface of type ID3D12Resource with 2D texture type. */
    MFX_SURFACE_TYPE_VULKAN_IMG2D          = 6,      /*!< Vulkan 2D image (VkImage). */
} mfxSurfaceType;

/*! This enumerator specifies the sharing modes which are allowed for importing or exporting shared surfaces. */
enum {
    MFX_SURFACE_FLAG_DEFAULT          = 0x0000,      /*!< Default is SHARED import or export. */

    MFX_SURFACE_FLAG_IMPORT_SHARED    = 0x0010,      /*!< Import frames directly by mapping a shared native handle from an application-provided surface to an internally-allocated surface. */
    MFX_SURFACE_FLAG_IMPORT_COPY      = 0x0020,      /*!< Import frames by copying data from an application-provided surface to an internally-allocated surface. */

    MFX_SURFACE_FLAG_EXPORT_SHARED    = 0x0100,      /*!< Export frames directly by mapping a shared native handle from an internally-allocated surface to an application-provided surface. */
    MFX_SURFACE_FLAG_EXPORT_COPY      = 0x0200,      /*!< Export frames by copying data from an internally-allocated surface to an application-provided surface. */
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxSurfaceType SurfaceType;     /*!< Set to the MFX_SURFACE_TYPE enum corresponding to the specific structure. */
    mfxU32         SurfaceFlags;    /*!< Set to the MFX_SURFACE_FLAG enum (or combination) corresponding to the allowed import / export mode(s). Multiple flags may be combined with OR.
                                         Upon a successful Import or Export operation, this field will indicate the actual mode used.*/

    mfxU32         StructSize;      /*!< Size in bytes of the complete mfxSurfaceXXX structure. */

    mfxU16         NumExtParam;     /*!< The number of extra configuration structures attached to the structure. */
    mfxExtBuffer** ExtParam;        /*!< Points to an array of pointers to the extra configuration structures; see the ExtendedBufferID enumerator for a list of extended configurations. */

    mfxU32 reserved[6];
} mfxSurfaceHeader;
MFX_PACK_END()


#define MFX_SURFACEINTERFACE_VERSION MFX_STRUCT_VERSION(1, 0)

/*!
   Contains mfxSurfaceHeader and the callback functions AddRef, Release and GetRefCounter
   that the application may use to manage access to exported surfaces.
   These interfaces are only valid for surfaces obtained by mfxFrameSurfaceInterface::Export.
   They are not used for surface descriptions passed to function mfxMemoryInterface::ImportFrameSurface.
*/
MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct mfxSurfaceInterface {
    mfxSurfaceHeader Header;  /*!< Exported surface header. Contains description of current surface. */

    mfxStructVersion Version; /*!< The version of the structure. */

    mfxHDL           Context; /*!< The context of the exported surface interface. User should not touch (change, set, null) this pointer. */

    /*! @brief
    Increments the internal reference counter of the surface. The surface is not destroyed until the surface is released using the mfxSurfaceInterface::Release function.
    mfxSurfaceInterface::AddRef should be used each time a new link to the surface is created (for example, copy structure) for proper surface management.

    @param[in]  surface  Valid surface.

    @return
     MFX_ERR_NONE              If no error. \n
     MFX_ERR_NULL_PTR          If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE    If mfxSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN           Any internal error.

    */
    mfxStatus           (MFX_CDECL *AddRef)(struct mfxSurfaceInterface* surface);

    /*! @brief
    Decrements the internal reference counter of the surface. mfxSurfaceInterface::Release should be called after using the
    mfxSurfaceInterface::AddRef function to add a surface or when allocation logic requires it. For example, call
    mfxSurfaceInterface::Release to release a surface obtained with the mfxFrameSurfaceInterface::Export function.

    @param[in]  surface  Valid surface.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNDEFINED_BEHAVIOR If Reference Counter of surface is zero before call. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Release)(struct mfxSurfaceInterface* surface);

    /*! @brief
    Returns current reference counter of exported surface.

    @param[in]   surface  Valid surface.
    @param[out]  counter  Sets counter to the current reference counter value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface or counter is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *GetRefCounter)(struct mfxSurfaceInterface* surface, mfxU32* counter);

    /*! @brief
    This function is only valuable for surfaces which were exported in sharing mode (without a copy).
    Guarantees readiness of both the data (pixels) and any original mfxFrameSurface1 frame's meta information (for example corruption flags) after a function completes.

    Instead of MFXVideoCORE_SyncOperation, users may directly call the mfxSurfaceInterface::Synchronize function after the corresponding
    Decode or VPP function calls (MFXVideoDECODE_DecodeFrameAsync or MFXVideoVPP_RunFrameVPPAsync).
    The prerequisites to call the functions are:

    @li The main processing functions return MFX_ERR_NONE.
    @li A valid surface object.

    @param[in]   surface  Valid surface.
    @param[out]  wait  Wait time in milliseconds.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of surface is not valid object . \n
     MFX_WRN_IN_EXECUTION       If the given timeout is expired and the surface is not ready. \n
     MFX_ERR_ABORTED            If the specified asynchronous function aborted due to data dependency on a previous asynchronous function that did not complete. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Synchronize)(struct mfxSurfaceInterface* surface, mfxU32 wait);

    mfxHDL reserved[11];
} mfxSurfaceInterface;
MFX_PACK_END()

#endif

#ifdef ONEVPL_EXPERIMENTAL
#define MFX_FRAMESURFACEINTERFACE_VERSION MFX_STRUCT_VERSION(1, 1)
#else
#define MFX_FRAMESURFACEINTERFACE_VERSION MFX_STRUCT_VERSION(1, 0)
#endif

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*! Specifies frame surface interface. */
typedef struct mfxFrameSurfaceInterface {
    mfxHDL              Context; /*!< The context of the memory interface. User should not touch (change, set, null) this pointer. */
    mfxStructVersion    Version; /*!< The version of the structure. */
    mfxU16              reserved1[3];

    /*! @brief
    Increments the internal reference counter of the surface. The surface is not destroyed until the surface is released using the mfxFrameSurfaceInterface::Release function.
    mfxFrameSurfaceInterface::AddRef should be used each time a new link to the surface is created (for example, copy structure) for proper surface management.

    @param[in]  surface  Valid surface.

    @return
     MFX_ERR_NONE              If no error. \n
     MFX_ERR_NULL_PTR          If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE    If mfxFrameSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN           Any internal error.

    */
    mfxStatus           (MFX_CDECL *AddRef)(mfxFrameSurface1* surface);

    /*! @brief
    Decrements the internal reference counter of the surface. mfxFrameSurfaceInterface::Release should be called after using the
    mfxFrameSurfaceInterface::AddRef function to add a surface or when allocation logic requires it. For example, call
    mfxFrameSurfaceInterface::Release to release a surface obtained with the GetSurfaceForXXX function.

    @param[in]  surface  Valid surface.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxFrameSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNDEFINED_BEHAVIOR If Reference Counter of surface is zero before call. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Release)(mfxFrameSurface1* surface);

    /*! @brief
    Returns current reference counter of mfxFrameSurface1 structure.

    @param[in]   surface  Valid surface.
    @param[out]  counter  Sets counter to the current reference counter value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface or counter is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxFrameSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *GetRefCounter)(mfxFrameSurface1* surface, mfxU32* counter);

    /*! @brief
    Sets pointers of surface->Info.Data to actual pixel data, providing read-write access.

    In case of video memory, the surface with data in video memory becomes mapped to system memory.
    An application can map a surface for read access with any value of mfxFrameSurface1::Data::Locked, but can map a surface for write access only when mfxFrameSurface1::Data::Locked equals to 0.

    Note: A surface allows shared read access, but exclusive write access. Consider the following cases:
    @li Map with Write or Read|Write flags. A request during active another read or write access returns MFX_ERR_LOCK_MEMORY error immediately, without waiting.
    MFX_MAP_NOWAIT does not impact behavior. This type of request does not lead to any implicit synchronizations.
    @li Map with Read flag. A request during active write access will wait for resource to become free,
    or exits immediately with error if MFX_MAP_NOWAIT flag was set. This request may lead to the implicit synchronization (with same logic as Synchronize call)
    waiting for surface to become ready to use (all dependencies should be resolved and upstream components finished writing to this surface).

    It is guaranteed that read access will be acquired right after synchronization without allowing another thread to acquire this surface for writing.

    If MFX_MAP_NOWAIT was set and the surface is not ready yet (for example the surface has unresolved data dependencies or active processing), the read access request exits immediately with error.

    Read-write access with MFX_MAP_READ_WRITE provides exclusive simultaneous reading and writing access.

    @note Bitwise copying of mfxFrameSurface1 object between map / unmap calls may result in having dangling data pointers in copies.

    @param[in]   surface  Valid surface.
    @param[out]  flags  Specify mapping mode.
    @param[out]  surface->Info.Data Pointers set to actual pixel data.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxFrameSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNSUPPORTED        If flags are invalid. \n
     MFX_ERR_LOCK_MEMORY        If user wants to map the surface for write and surface->Data.Locked does not equal to 0. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Map)(mfxFrameSurface1* surface, mfxU32 flags);

    /*! @brief
    Invalidates pointers of surface->Info.Data and sets them to NULL.
    In case of video memory, the underlying texture becomes unmapped after last reader or writer unmap.


    @param[in]   surface  Valid surface.
    @param[out]  surface->Info.Data  Pointers set to NULL.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxFrameSurfaceInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNSUPPORTED        If surface is already unmapped. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Unmap)(mfxFrameSurface1* surface);

    /*! @brief
    Returns a native resource's handle and type. The handle is returned *as-is*, meaning that the reference counter of base resources is not incremented.
    The native resource is not detached from surface and the library still owns the resource. User must not destroy
    the native resource or assume that the resource will be alive after mfxFrameSurfaceInterface::Release.



    @param[in]   surface  Valid surface.
    @param[out]  resource Pointer is set to the native handle of the resource.
    @param[out]  resource_type Type of native resource. See mfxResourceType enumeration).


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If any of surface, resource or resource_type is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of surface, resource or resource_type is not valid object (no native resource was allocated). \n
     MFX_ERR_UNSUPPORTED        If surface is in system memory. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *GetNativeHandle)(mfxFrameSurface1* surface, mfxHDL* resource, mfxResourceType* resource_type);

    /*! @brief
    Returns a device abstraction that was used to create that resource.
    The handle is returned *as-is*, meaning that the reference counter for the device abstraction is not incremented.
    The native resource is not detached from the surface and the library still has a reference to the resource.
    User must not destroy the device or assume that the device will be alive after mfxFrameSurfaceInterface::Release.


    @param[in]   surface  Valid surface.
    @param[out]  device_handle Pointer is set to the device which created the resource
    @param[out]  device_type Type of device (see mfxHandleType enumeration).


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If any of surface, device_handle or device_type is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of surface, resource or resource_type is not valid object (no native resource was allocated). \n
     MFX_ERR_UNSUPPORTED        If surface is in system memory. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *GetDeviceHandle)(mfxFrameSurface1* surface, mfxHDL* device_handle, mfxHandleType* device_type);

    /*! @brief
    Guarantees readiness of both the data (pixels) and any frame's meta information (for example corruption flags) after a function completes.

    Instead of MFXVideoCORE_SyncOperation, users may directly call the mfxFrameSurfaceInterface::Synchronize function after the corresponding
    Decode or VPP function calls (MFXVideoDECODE_DecodeFrameAsync or MFXVideoVPP_RunFrameVPPAsync).
    The prerequisites to call the functions are:

    @li The main processing functions return MFX_ERR_NONE.
    @li A valid mfxFrameSurface1 object.



    @param[in]   surface  Valid surface.
    @param[out]  wait  Wait time in milliseconds.


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If any of surface is not valid object . \n
     MFX_WRN_IN_EXECUTION       If the given timeout is expired and the surface is not ready. \n
     MFX_ERR_ABORTED            If the specified asynchronous function aborted due to data dependency on a previous asynchronous function that did not complete. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *Synchronize)(mfxFrameSurface1* surface, mfxU32 wait);

    /*! @brief
    The library calls the function after complete of associated video operation
    notifying the application that frame surface is ready.

    @attention This is callback function and intended to be called by
               the library only.

    @note The library calls this callback only when this surface is used as the output surface.

    It is expected that the function is low-intrusive designed otherwise it may
    impact performance.

    @param[in] sts  The status of completed operation.

    */
    void               (MFX_CDECL *OnComplete)(mfxStatus sts);

   /*! @brief
    Returns an interface defined by the GUID. If the returned interface is a reference
    counted object the caller should release the obtained interface to avoid memory leaks.

    @param[in]  surface   Valid surface.
    @param[in]  guid      GUID of the requested interface.
    @param[out] iface     Interface.


    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If interface or surface is NULL. \n
     MFX_ERR_UNSUPPORTED        If requested interface is not supported. \n
     MFX_ERR_NOT_IMPLEMENTED    If requested interface is not implemented. \n
     MFX_ERR_NOT_INITIALIZED    If requested interface is not available (not created or already deleted). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus           (MFX_CDECL *QueryInterface)(mfxFrameSurface1* surface, mfxGUID guid, mfxHDL* iface);

#ifdef ONEVPL_EXPERIMENTAL
    /*! @brief
     If successful returns an exported surface, which is a refcounted object allocated by runtime. It could be exported with or without copy, depending
     on export flags and the possibility of such export. Exported surface is valid throughout the session, as long as the original mfxFrameSurface1
     object is not closed and the refcount of exported surface is not zero.

     @param[in]  surface              Valid surface.
     @param[in]  export_header        Description of export: caller should fill in SurfaceType (type to export to) and SurfaceFlags (allowed export modes).
     @param[out] exported_surface     Exported surface, allocated by runtime, user needs to decrement refcount after usage for object release.
                                      After successful export, the value of mfxSurfaceHeader::SurfaceFlags will contain the actual export mode.


     @return
      MFX_ERR_NONE               If no error. \n
      MFX_ERR_NULL_PTR           If export surface or surface is NULL. \n
      MFX_ERR_UNSUPPORTED        If requested export is not supported. \n
      MFX_ERR_NOT_IMPLEMENTED    If requested export is not implemented. \n
      MFX_ERR_UNKNOWN            Any internal error.
     */

     /* For reference with Import flow please search for mfxMemoryInterface::ImportFrameSurface. */
    mfxStatus           (MFX_CDECL *Export)(mfxFrameSurface1* surface, mfxSurfaceHeader export_header, mfxSurfaceHeader** exported_surface);

    mfxHDL              reserved2[1];
#else
    mfxHDL              reserved2[2];
#endif
} mfxFrameSurfaceInterface;
MFX_PACK_END()

/*! The TimeStampCalc enumerator itemizes time-stamp calculation methods. */
enum {
    /*! The time stamp calculation is based on the input frame rate if time stamp is not explicitly specified. */
    MFX_TIMESTAMPCALC_UNKNOWN = 0,
    /*! Adjust time stamp to 29.97fps on 24fps progressively encoded sequences if telecine attributes are available in the bitstream and
    time stamp is not explicitly specified. The input frame rate must be specified. */
    MFX_TIMESTAMPCALC_TELECINE = 1,
};

/* Transcoding Info */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies configurations for decoding, encoding, and transcoding processes.
    A zero value in any of these fields indicates that the field is not explicitly specified. */
typedef struct {
    mfxU32  reserved[7]; /*!< Reserved for future use. */

    /*! Hint to enable low power consumption mode for encoders. See the CodingOptionValue enumerator for values
        of this option. Use the Query API function to check if this feature is supported. */
    mfxU16  LowPower;
    /*! Specifies a multiplier for bitrate control parameters. Affects the following variables: InitialDelayInKB, BufferSizeInKB,
        TargetKbps, MaxKbps, WinBRCMaxAvgKbps. If this value is not equal to zero, the encoder calculates BRC parameters as ``value * BRCParamMultiplier``. */
    mfxU16  BRCParamMultiplier;

    mfxFrameInfo    FrameInfo; /*!< mfxFrameInfo structure that specifies frame parameters. */
    mfxU32  CodecId;           /*!< Specifies the codec format identifier in the FourCC code; see the CodecFormatFourCC enumerator for details.
                                    This is a mandated input parameter for the QueryIOSurf and Init API functions. */
    mfxU16  CodecProfile;      /*!< Specifies the codec profile; see the CodecProfile enumerator for details. Specify the codec profile explicitly or the API functions will determine
                                    the correct profile from other sources, such as resolution and bitrate. */
    mfxU16  CodecLevel;        /*!< Codec level; see the CodecLevel enumerator for details. Specify the codec level explicitly or the functions will determine the correct level from other sources,
                                    such as resolution and bitrate. */
    mfxU16  NumThread;

    union {
        struct {   /* Encoding Options */
            mfxU16  TargetUsage; /*!< Target usage model that guides the encoding process; see the TargetUsage enumerator for details. */

            /*! Number of pictures within the current GOP (Group of Pictures); if GopPicSize = 0, then the GOP size is unspecified. If GopPicSize = 1, only I-frames are used.
                The following pseudo-code that shows how the library uses this parameter:
                @code
                   mfxU16 get_gop_sequence (...) {
                      pos=display_frame_order;
                      if (pos == 0)
                          return MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;

                      If (GopPicSize == 1) // Only I-frames
                          return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;

                      if (GopPicSize == 0)
                                  frameInGOP = pos;    //Unlimited GOP
                              else
                                  frameInGOP = pos%GopPicSize;

                      if (frameInGOP == 0)
                          return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;

                      if (GopRefDist == 1 || GopRefDist == 0)    // Only I,P frames
                                  return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;

                      frameInPattern = (frameInGOP-1)%GopRefDist;
                      if (frameInPattern == GopRefDist - 1)
                          return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;

                      return MFX_FRAMETYPE_B;
                    }
                @endcode */
            mfxU16  GopPicSize;
            /*! Distance between I- or P (or GPB) - key frames; if it is zero, the GOP structure is unspecified. Note: If GopRefDist = 1,
                there are no regular B-frames used (only P or GPB); if mfxExtCodingOption3::GPB is ON, GPB frames (B without backward
                references) are used instead of P. */
            mfxU16  GopRefDist;
            /*! ORs of the GopOptFlag enumerator indicate the additional flags for the GOP specification. */
            mfxU16  GopOptFlag;
            /*! For H.264, specifies IDR-frame interval in terms of I-frames.
                For example:
                @li If IdrInterval = 0, then every I-frame is an IDR-frame.
                @li If IdrInterval = 1, then every other I-frame is an IDR-frame.

                For HEVC, if IdrInterval = 0, then only first I-frame is an IDR-frame. For example:
                @li If IdrInterval = 1, then every I-frame is an IDR-frame.
                @li If IdrInterval = 2, then every other I-frame is an IDR-frame.

                For MPEG2, IdrInterval defines sequence header interval in terms of I-frames. For example:
                @li If IdrInterval = 0 (default), then the sequence header is inserted once at the beginning of the stream.
                @li If IdrInterval = N, then the sequence header is inserted before every Nth I-frame.


                If GopPicSize or GopRefDist is zero, IdrInterval is undefined. */
            mfxU16  IdrInterval;

            mfxU16  RateControlMethod; /*! Rate control method; see the RateControlMethod enumerator for details. */
            union {
                /*! Initial size of the Video Buffering Verifier (VBV) buffer.
                    @note In this context, KB is 1000 bytes and Kbps is 1000 bps. */
                mfxU16  InitialDelayInKB;
                /*! Quantization Parameter (QP) for I-frames for constant QP mode (CQP). Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPI might be clipped to supported QPI range.
                    @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                    Adjusted QPI value = QPI - (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                    For HEVC_MAIN10, we minus (6*(10-8)=12) on our side and continue.
                    @note In av1 design, valid range is 0 to 255 inclusive, and if QPI=QPP=QPB=0, the encoder is in lossless mode.
                    @note In vp9 design, valid range is 1 to 255 inclusive, and zero QP that the default value is assigned by the library.
                    @note Default QPI value is implementation dependent and subject to change without additional notice in this document. */
                mfxU16  QPI;
                mfxU16  Accuracy; /*!< Specifies accuracy range in the unit of tenth of percent. */
            };
            mfxU16  BufferSizeInKB; /*!< Represents the maximum possible size of any compressed frames. */
            union {
                /*! Constant bitrate TargetKbps. Used to estimate the targeted frame size by dividing the frame rate by the bitrate. */
                mfxU16  TargetKbps;
                /*! Quantization Parameter (QP) for P-frames for constant QP mode (CQP). Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPP might be clipped to supported QPI range.
                    @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                    Adjusted QPP value = QPP - (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                    For HEVC_MAIN10, we minus (6*(10-8)=12) on our side and continue.
                    @note In av1 design, valid range is 0 to 255 inclusive, and if QPI=QPP=QPB=0, the encoder is in lossless mode.
                    @note In vp9 design, valid range is 1 to 255 inclusive, and zero QP that the default value is assigned by the library.
                    @note Default QPP value is implementation dependent and subject to change without additional notice in this document. */
                mfxU16  QPP;
                mfxU16  ICQQuality; /*!< Used by the Intelligent Constant Quality (ICQ) bitrate control algorithm. Values are in the 1 to 51 range, where 1 corresponds the best quality. */
            };
            union {
                /*! The maximum bitrate at which the encoded data enters the Video Buffering Verifier (VBV) buffer. */
                mfxU16  MaxKbps;
                /*! Quantization Parameter (QP) for B-frames for constant QP mode (CQP). Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPI might be clipped to supported QPB range.
                    @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                    Adjusted QPB value = QPB - (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                    For HEVC_MAIN10, we minus (6*(10-8)=12) on our side and continue.
                    @note In av1 design, valid range is 0 to 255 inclusive, and if QPI=QPP=QPB=0, the encoder is in lossless mode.
                    @note Default QPB value is implementation dependent and subject to change without additional notice in this document. */
                mfxU16  QPB;
                mfxU16  Convergence; /*!< Convergence period in the unit of 100 frames. */
            };

            /*! Number of slices in each video frame. Each slice contains one or more macro-block rows. If NumSlice equals zero, the encoder may choose any slice partitioning
                allowed by the codec standard. See also mfxExtCodingOption2::NumMbPerSlice. */
            mfxU16  NumSlice;
            /*! Max number of all available reference frames (for AVC/HEVC, NumRefFrame defines DPB size). If NumRefFrame = 0, this parameter is not specified.
                See also NumRefActiveP, NumRefActiveBL0, and NumRefActiveBL1 in the mfxExtCodingOption3 structure, which set a number of active references. */
            mfxU16  NumRefFrame;
            /*! If not zero, specifies that ENCODE takes the input surfaces in the encoded order and uses explicit frame type control.
                The application must still provide GopRefDist and mfxExtCodingOption2::BRefType so the library can pack headers and build reference
                lists correctly. */
            mfxU16  EncodedOrder;
        };
        struct {   /* Decoding Options */
            /*! For AVC and HEVC, used to instruct the decoder to return output frames in the decoded order. Must be zero for all other decoders.
                When enabled, correctness of mfxFrameData::TimeStamp and FrameOrder for output surface is not guaranteed, the application should ignore them. */
            mfxU16  DecodedOrder;
            /*! Instructs DECODE to output extended picture structure values for additional display attributes. See the PicStruct description for details. */
            mfxU16  ExtendedPicStruct;
            /*! Time stamp calculation method. See the TimeStampCalc description for details. */
            mfxU16  TimeStampCalc;
            /*! Nonzero value indicates that slice groups are present in the bitstream. Used only by AVC decoder. */
            mfxU16  SliceGroupsPresent;
            /*! Nonzero value specifies the maximum required size of the decoded picture buffer in frames for AVC and HEVC decoders. */
            mfxU16  MaxDecFrameBuffering;
            /*! For decoders supporting dynamic resolution change (VP9), set this option to ON to allow MFXVideoDECODE_DecodeFrameAsync
                return MFX_ERR_REALLOC_SURFACE. See the CodingOptionValue enumerator for values of this option. Use the Query API
                function to check if this feature is supported. */
            mfxU16  EnableReallocRequest;
            /*! Special parameter for AV1 decoder. Indicates presence/absence of film grain parameters in bitstream.
                Also controls decoding behavior for streams with film grain parameters. MFXVideoDECODE_DecodeHeader returns nonzero FilmGrain
                for streams with film grain parameters and zero for streams w/o them. Decoding with film grain requires additional output surfaces.
                If FilmGrain` is non-zero then MFXVideoDECODE_QueryIOSurf will request more surfaces in case of external allocated video memory at decoder output.
                FilmGrain is passed to MFXVideoDECODE_Init function to control decoding operation for AV1 streams with film grain parameters.
                If FilmGrain is nonzero decoding of each frame require two output surfaces (one for reconstructed frame and one for output frame with film grain applied).
                The decoder returns MFX_ERR_MORE_SURFACE from MFXVideoDECODE_DecodeFrameAsync if it has insufficient output surfaces to decode frame.
                Application can forcibly disable the feature passing zero value of `FilmGrain` to `MFXVideoDECODE_Init`.
                In this case the decoder will output reconstructed frames w/o film grain applied.
                Application can retrieve film grain parameters for a frame by attaching extended buffer mfxExtAV1FilmGrainParam to mfxFrameSurface1.
                If stream has no film grain parameters `FilmGrain` passed to `MFXVideoDECODE_Init` is ignored by the decoder. */
            mfxU16  FilmGrain;
            /*! If not zero, it forces SDK to attempt to decode bitstream even if a decoder may not support all features associated with given CodecLevel. Decoder may produce visual artifacts. Only AVC decoder supports this field. */
            mfxU16  IgnoreLevelConstrain;
            /*! This flag is used to disable output of main decoding channel. When it's ON SkipOutput = MFX_CODINGOPTION_ON decoder outputs only video processed channels. For pure decode this flag should be always disabled. */
            mfxU16  SkipOutput;
            mfxU16  reserved2[4];
        };
        struct {   /* JPEG Decoding Options */
            /*! Specify the chroma sampling format that has been used to encode a JPEG picture. See the ChromaFormat enumerator for details. */
            mfxU16  JPEGChromaFormat;
            /*! Rotation option of the output JPEG picture. See the Rotation enumerator for details. */
            mfxU16  Rotation;
            /*! Specify the color format that has been used to encode a JPEG picture. See the JPEGColorFormat enumerator for details. */
            mfxU16  JPEGColorFormat;
            /*! Specify JPEG scan type for decoder. See the JPEGScanType enumerator for details. */
            mfxU16  InterleavedDec;
            mfxU8   SamplingFactorH[4]; /*!< Horizontal sampling factor. */
            mfxU8   SamplingFactorV[4]; /*!< Vertical sampling factor. */
            mfxU16  reserved3[5];
        };
        struct {   /* JPEG Encoding Options */
            /*! Specify interleaved or non-interleaved scans. If it is equal to MFX_SCANTYPE_INTERLEAVED then the image is encoded as interleaved,
                all components are encoded in one scan. See the JPEG Scan Type enumerator for details. */
            mfxU16  Interleaved;
            /*! Specifies the image quality if the application does not specified quantization table.
                The value is from 1 to 100 inclusive. "100" is the best quality. */
            mfxU16  Quality;
            /*! Specifies the number of MCU in the restart interval. "0" means no restart interval. */
            mfxU16  RestartInterval;
            mfxU16  reserved5[10];
        };
    };
} mfxInfoMFX;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies configurations for video processing. A zero value in any of the fields indicates
   that the corresponding field is not explicitly specified. */
typedef struct {
    mfxU32  reserved[8];
    mfxFrameInfo    In;  /*!< Input format for video processing. */
    mfxFrameInfo    Out; /*!< Output format for video processing. */
} mfxInfoVPP;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Configuration parameters for encoding, decoding, transcoding, and video processing. */
typedef struct {
    /*! Unique component ID that will be passed by the library to mfxFrameAllocRequest. Useful in pipelines where several
        components of the same type share the same allocator. */
    mfxU32  AllocId;
    mfxU32  reserved[2];
    mfxU16  reserved3;
    /*! Specifies how many asynchronous operations an application performs before the application explicitly synchronizes the result.
        If zero, the value is not specified. */
    mfxU16  AsyncDepth;

    union {
        mfxInfoMFX  mfx; /*!< Configurations related to encoding, decoding, and transcoding. See the definition of the mfxInfoMFX structure for details. */
        mfxInfoVPP  vpp; /*!< Configurations related to video processing. See the definition of the mfxInfoVPP structure for details. */
    };
    /*! Specifies the content protection mechanism. See the Protected enumerator for a list of supported protection schemes. */
    mfxU16  Protected;
    /*! Input and output memory access types for functions. See the enumerator IOPattern for details.
        The Query API functions return the natively supported IOPattern if the Query input argument is NULL.
        This parameter is a mandated input for QueryIOSurf and Init API functions. The output pattern must be specified for DECODE.
        The input pattern must be specified for ENCODE. Both input and output pattern must be specified for VPP. */
    mfxU16  IOPattern;
    mfxExtBuffer** ExtParam; /*!< Points to an array of pointers to the extra configuration structures. See the ExtendedBufferID enumerator
                                  for a list of extended configurations.
                                  The list of extended buffers should not contain duplicated entries, such as entries of the same type.
                                  If the  mfxVideoParam structure is used to query library capability, then the list of extended buffers attached to the input
                                  and output mfxVideoParam structure should be equal, that is, it should contain the same number of extended
                                  buffers of the same type. */
    mfxU16  NumExtParam;     /*!< The number of extra configuration structures attached to this structure. */
    mfxU16  reserved2;
} mfxVideoParam;
MFX_PACK_END()

/*! The IOPattern enumerator itemizes memory access patterns for API functions. Use bit-ORed values to specify an input access
    pattern and an output access pattern. */
enum {
    MFX_IOPATTERN_IN_VIDEO_MEMORY   = 0x01, /*!< Input to functions is a video memory surface. */
    MFX_IOPATTERN_IN_SYSTEM_MEMORY  = 0x02, /*!< Input to functions is a linear buffer directly in system memory or in system memory through an external allocator. */
    MFX_IOPATTERN_OUT_VIDEO_MEMORY  = 0x10, /*!< Output to functions is a video memory surface.  */
    MFX_IOPATTERN_OUT_SYSTEM_MEMORY = 0x20 /*!< Output to functions is a linear buffer directly in system memory or in system memory through an external allocator.  */
};

/*! The CodecFormatFourCC enumerator itemizes codecs in the FourCC format. */
enum {
    MFX_CODEC_AVC         =MFX_MAKEFOURCC('A','V','C',' '), /*!< AVC, H.264, or MPEG-4, part 10 codec. */
    MFX_CODEC_HEVC        =MFX_MAKEFOURCC('H','E','V','C'), /*!< HEVC codec. */
    MFX_CODEC_MPEG2       =MFX_MAKEFOURCC('M','P','G','2'), /*!< MPEG-2 codec. */
    MFX_CODEC_VC1         =MFX_MAKEFOURCC('V','C','1',' '), /*!< VC-1 codec. */
    MFX_CODEC_CAPTURE     =MFX_MAKEFOURCC('C','A','P','T'), /*!<  */
    MFX_CODEC_VP9         =MFX_MAKEFOURCC('V','P','9',' '), /*!< VP9 codec. */
    MFX_CODEC_AV1         =MFX_MAKEFOURCC('A','V','1',' '), /*!< AV1 codec. */
    MFX_CODEC_VVC         =MFX_MAKEFOURCC('V','V','C',' ')  /*!< VVC codec. */
};

/*!
The CodecProfile enumerator itemizes codec profiles for all codecs.
CodecLevel
*/
enum {
    MFX_PROFILE_UNKNOWN                     =0, /*!< Unspecified profile. */
    MFX_LEVEL_UNKNOWN                       =0, /*!< Unspecified level. */

    /*! @{ */
    /* Combined with H.264 profile these flags impose additional constrains. See H.264 specification for the list of constrains. */
    MFX_PROFILE_AVC_CONSTRAINT_SET0     = (0x100 << 0),
    MFX_PROFILE_AVC_CONSTRAINT_SET1     = (0x100 << 1),
    MFX_PROFILE_AVC_CONSTRAINT_SET2     = (0x100 << 2),
    MFX_PROFILE_AVC_CONSTRAINT_SET3     = (0x100 << 3),
    MFX_PROFILE_AVC_CONSTRAINT_SET4     = (0x100 << 4),
    MFX_PROFILE_AVC_CONSTRAINT_SET5     = (0x100 << 5),
    /*! @} */

    /*! @{ */
    /* H.264 Profiles. */
    MFX_PROFILE_AVC_BASELINE                =66,
    MFX_PROFILE_AVC_MAIN                    =77,
    MFX_PROFILE_AVC_EXTENDED                =88,
    MFX_PROFILE_AVC_HIGH                    =100,
    MFX_PROFILE_AVC_HIGH10                  =110,
    MFX_PROFILE_AVC_HIGH_422                =122,
    MFX_PROFILE_AVC_CONSTRAINED_BASELINE    =MFX_PROFILE_AVC_BASELINE + MFX_PROFILE_AVC_CONSTRAINT_SET1,
    MFX_PROFILE_AVC_CONSTRAINED_HIGH        =MFX_PROFILE_AVC_HIGH     + MFX_PROFILE_AVC_CONSTRAINT_SET4
                                                                      + MFX_PROFILE_AVC_CONSTRAINT_SET5,
    MFX_PROFILE_AVC_PROGRESSIVE_HIGH        =MFX_PROFILE_AVC_HIGH     + MFX_PROFILE_AVC_CONSTRAINT_SET4,
    /*! @} */

    /*! @{ */
    /* H.264 level 1-1.3 */
    MFX_LEVEL_AVC_1                         =10,
    MFX_LEVEL_AVC_1b                        =9,
    MFX_LEVEL_AVC_11                        =11,
    MFX_LEVEL_AVC_12                        =12,
    MFX_LEVEL_AVC_13                        =13,
    /*! @} */
    /*! @{ */
    /* H.264 level 2-2.2 */
    MFX_LEVEL_AVC_2                         =20,
    MFX_LEVEL_AVC_21                        =21,
    MFX_LEVEL_AVC_22                        =22,
    /*! @} */
    /*! @{ */
    /* H.264 level 3-3.2 */
    MFX_LEVEL_AVC_3                         =30,
    MFX_LEVEL_AVC_31                        =31,
    MFX_LEVEL_AVC_32                        =32,
    /*! @} */
    /*! @{ */
    /* H.264 level 4-4.2 */
    MFX_LEVEL_AVC_4                         =40,
    MFX_LEVEL_AVC_41                        =41,
    MFX_LEVEL_AVC_42                        =42,
    /*! @} */
    /*! @{ */
    /* H.264 level 5-5.2 */
    MFX_LEVEL_AVC_5                         =50,
    MFX_LEVEL_AVC_51                        =51,
    MFX_LEVEL_AVC_52                        =52,
    /*! @} */
    /*! @{ */
    /* H.264 level 6-6.2 */
    MFX_LEVEL_AVC_6                         =60,
    MFX_LEVEL_AVC_61                        =61,
    MFX_LEVEL_AVC_62                        =62,
    /*! @} */

    /*! @{ */
    /* MPEG2 Profiles. */
    MFX_PROFILE_MPEG2_SIMPLE                =0x50,
    MFX_PROFILE_MPEG2_MAIN                  =0x40,
    MFX_PROFILE_MPEG2_HIGH                  =0x10,
    /*! @} */

    /*! @{ */
    /* MPEG2 Levels. */
    MFX_LEVEL_MPEG2_LOW                     =0xA,
    MFX_LEVEL_MPEG2_MAIN                    =0x8,
    MFX_LEVEL_MPEG2_HIGH                    =0x4,
    MFX_LEVEL_MPEG2_HIGH1440                =0x6,
    /*! @} */

    /*! @{ */
    /* VC-1 Profiles. */
    MFX_PROFILE_VC1_SIMPLE                  =(0+1),
    MFX_PROFILE_VC1_MAIN                    =(4+1),
    MFX_PROFILE_VC1_ADVANCED                =(12+1),
    /*! @} */

    /*! @{ */
    /* VC-1 Level Low (simple & main profiles) */
    MFX_LEVEL_VC1_LOW                       =(0+1),
    MFX_LEVEL_VC1_MEDIAN                    =(2+1),
    MFX_LEVEL_VC1_HIGH                      =(4+1),
    /*! @} */

    /*! @{ */
    /* VC-1 advanced profile levels */
    MFX_LEVEL_VC1_0                         =(0x00+1),
    MFX_LEVEL_VC1_1                         =(0x01+1),
    MFX_LEVEL_VC1_2                         =(0x02+1),
    MFX_LEVEL_VC1_3                         =(0x03+1),
    MFX_LEVEL_VC1_4                         =(0x04+1),
    /*! @} */

    /*! @{ */
    /* HEVC profiles */
    MFX_PROFILE_HEVC_MAIN             =1,
    MFX_PROFILE_HEVC_MAIN10           =2,
    MFX_PROFILE_HEVC_MAINSP           =3,
    MFX_PROFILE_HEVC_REXT             =4,
    MFX_PROFILE_HEVC_SCC              =9,
    /*! @} */

    /*! @{ */
    /* HEVC levels */
    MFX_LEVEL_HEVC_1   = 10,
    MFX_LEVEL_HEVC_2   = 20,
    MFX_LEVEL_HEVC_21  = 21,
    MFX_LEVEL_HEVC_3   = 30,
    MFX_LEVEL_HEVC_31  = 31,
    MFX_LEVEL_HEVC_4   = 40,
    MFX_LEVEL_HEVC_41  = 41,
    MFX_LEVEL_HEVC_5   = 50,
    MFX_LEVEL_HEVC_51  = 51,
    MFX_LEVEL_HEVC_52  = 52,
    MFX_LEVEL_HEVC_6   = 60,
    MFX_LEVEL_HEVC_61  = 61,
    MFX_LEVEL_HEVC_62  = 62,
    MFX_LEVEL_HEVC_85  = 85,
    /*! @} */

    /*! @{ */
    /* HEVC tiers */
    MFX_TIER_HEVC_MAIN  = 0,
    MFX_TIER_HEVC_HIGH  = 0x100,
    /*! @} */

    /*! @{ */
    /* VP9 Profiles */
    MFX_PROFILE_VP9_0                       = 1,
    MFX_PROFILE_VP9_1                       = 2,
    MFX_PROFILE_VP9_2                       = 3,
    MFX_PROFILE_VP9_3                       = 4,
    /*! @} */

    /*! @{ */
    /* AV1 Profiles */
    MFX_PROFILE_AV1_MAIN                    = 1,
    MFX_PROFILE_AV1_HIGH                    = 2,
    MFX_PROFILE_AV1_PRO                     = 3,
    /*! @} */

    /*! @{ */
    /* AV1 Levels */
    MFX_LEVEL_AV1_2                         = 20,
    MFX_LEVEL_AV1_21                        = 21,
    MFX_LEVEL_AV1_22                        = 22,
    MFX_LEVEL_AV1_23                        = 23,
    MFX_LEVEL_AV1_3                         = 30,
    MFX_LEVEL_AV1_31                        = 31,
    MFX_LEVEL_AV1_32                        = 32,
    MFX_LEVEL_AV1_33                        = 33,
    MFX_LEVEL_AV1_4                         = 40,
    MFX_LEVEL_AV1_41                        = 41,
    MFX_LEVEL_AV1_42                        = 42,
    MFX_LEVEL_AV1_43                        = 43,
    MFX_LEVEL_AV1_5                         = 50,
    MFX_LEVEL_AV1_51                        = 51,
    MFX_LEVEL_AV1_52                        = 52,
    MFX_LEVEL_AV1_53                        = 53,
    MFX_LEVEL_AV1_6                         = 60,
    MFX_LEVEL_AV1_61                        = 61,
    MFX_LEVEL_AV1_62                        = 62,
    MFX_LEVEL_AV1_63                        = 63,
    MFX_LEVEL_AV1_7                         = 70,
    MFX_LEVEL_AV1_71                        = 71,
    MFX_LEVEL_AV1_72                        = 72,
    MFX_LEVEL_AV1_73                        = 73,
    /*! @} */

    /*! @{ */
    /* VVC Profiles */
    MFX_PROFILE_VVC_MAIN10                  = 1,
    MFX_PROFILE_VVC_MAIN10_STILL_PICTURE    = 65,
    /*! @} */

    /*! @{ */
    /* VVC Levels */
    MFX_LEVEL_VVC_1                         = 16,
    MFX_LEVEL_VVC_2                         = 32,
    MFX_LEVEL_VVC_21                        = 35,
    MFX_LEVEL_VVC_3                         = 48,
    MFX_LEVEL_VVC_31                        = 51,
    MFX_LEVEL_VVC_4                         = 64,
    MFX_LEVEL_VVC_41                        = 67,
    MFX_LEVEL_VVC_5                         = 80,
    MFX_LEVEL_VVC_51                        = 83,
    MFX_LEVEL_VVC_52                        = 86,
    MFX_LEVEL_VVC_6                         = 96,
    MFX_LEVEL_VVC_61                        = 99,
    MFX_LEVEL_VVC_62                        = 102,
    MFX_LEVEL_VVC_63                        = 105,
    MFX_LEVEL_VVC_155                       = 255,
    /*! @} */

    /*! @{ */
    /* VVC tiers */
    MFX_TIER_VVC_MAIN                       = 0,
    MFX_TIER_VVC_HIGH                       = 0x100,
    /*! @} */
};

/*! The GopOptFlag enumerator itemizes special properties in the GOP (Group of Pictures) sequence. */
enum {
    /*!
       The encoder generates closed GOP if this flag is set. Frames in this GOP do not use frames in previous GOP as reference.

       The encoder generates open GOP if this flag is not set. In this GOP frames prior to the first frame of GOP in display order may use
       frames from previous GOP as reference. Frames subsequent to the first frame of GOP in display order do not use frames from previous
       GOP as reference.

       The AVC encoder ignores this flag if IdrInterval in mfxInfoMFX structure is set to 0, i.e. if every GOP starts from IDR frame.
       In this case, GOP is encoded as closed.

       This flag does not affect long-term reference frames.
    */
    MFX_GOP_CLOSED          =1,
    /*!
       The encoder must strictly follow the given GOP structure as defined by parameter GopPicSize, GopRefDist etc in the mfxVideoParam structure.
       Otherwise, the encoder can adapt the GOP structure for better efficiency, whose range is constrained by parameter GopPicSize and
       GopRefDist etc. See also description of AdaptiveI and AdaptiveB fields in the mfxExtCodingOption2 structure.
    */
    MFX_GOP_STRICT          =2
};

/*! The TargetUsage enumerator itemizes a range of numbers from MFX_TARGETUSAGE_1, best quality, to MFX_TARGETUSAGE_7, best speed.
    It indicates trade-offs between quality and speed. The application can use any number in the range. The actual number of supported
    target usages depends on implementation. If specified target usage is not supported, the encoder will use the closest supported value. */
enum {
    MFX_TARGETUSAGE_1    =1, /*!< Best quality */
    MFX_TARGETUSAGE_2    =2,
    MFX_TARGETUSAGE_3    =3,
    MFX_TARGETUSAGE_4    =4, /*!< Balanced quality and speed. */
    MFX_TARGETUSAGE_5    =5,
    MFX_TARGETUSAGE_6    =6,
    MFX_TARGETUSAGE_7    =7, /*!< Best speed */

    MFX_TARGETUSAGE_UNKNOWN         =0, /*!< Unspecified target usage. */
    MFX_TARGETUSAGE_BEST_QUALITY    =MFX_TARGETUSAGE_1, /*!< Best quality. */
    MFX_TARGETUSAGE_BALANCED        =MFX_TARGETUSAGE_4, /*!< Balanced quality and speed. */
    MFX_TARGETUSAGE_BEST_SPEED      =MFX_TARGETUSAGE_7  /*!< Best speed. */
};

/*! The RateControlMethod enumerator itemizes bitrate control methods. */
enum {
    MFX_RATECONTROL_CBR       =1, /*!< Use the constant bitrate control algorithm. */
    MFX_RATECONTROL_VBR       =2, /*!< Use the variable bitrate control algorithm. */
    MFX_RATECONTROL_CQP       =3, /*!< Use the constant quantization parameter algorithm. */
    MFX_RATECONTROL_AVBR      =4, /*!< Use the average variable bitrate control algorithm. */
    MFX_RATECONTROL_RESERVED1 =5,
    MFX_RATECONTROL_RESERVED2 =6,
    MFX_RATECONTROL_RESERVED3 =100,
    MFX_RATECONTROL_RESERVED4 =7,
    /*!
       Use the VBR algorithm with look ahead. It is a special bitrate control mode in the AVC encoder that has been designed
       to improve encoding quality. It works by performing extensive analysis of several dozen frames before the actual encoding and as a side
       effect significantly increases encoding delay and memory consumption.

       The only available rate control parameter in this mode is mfxInfoMFX::TargetKbps. Two other parameters, MaxKbps and InitialDelayInKB,
       are ignored. To control LA depth the application can use mfxExtCodingOption2::LookAheadDepth parameter.

       This method is not HRD compliant.
    */
    MFX_RATECONTROL_LA        =8,
    /*!
       Use the Intelligent Constant Quality algorithm. This algorithm improves subjective video quality of encoded stream. Depending on content,
       it may or may not decrease objective video quality. Only one control parameter is used - quality factor, specified by mfxInfoMFX::ICQQuality.
    */
    MFX_RATECONTROL_ICQ       =9,
    /*!
       Use the Video Conferencing Mode algorithm. This algorithm is similar to the VBR and uses the same set of parameters mfxInfoMFX::InitialDelayInKB,
       TargetKbpsandMaxKbps. It is tuned for IPPP GOP pattern and streams with strong temporal correlation between frames.
       It produces better objective and subjective video quality in these conditions than other bitrate control algorithms.
       It does not support interlaced content, B-frames and produced stream is not HRD compliant.
    */
    MFX_RATECONTROL_VCM       =10,
    /*!
       Use Intelligent Constant Quality algorithm with look ahead. Quality factor is specified by mfxInfoMFX::ICQQuality.
       To control LA depth the application can use mfxExtCodingOption2::LookAheadDepth parameter.

       This method is not HRD compliant.
    */
    MFX_RATECONTROL_LA_ICQ    =11,
    /*!
       MFX_RATECONTROL_LA_EXT has been removed
    */

    /*! Use HRD compliant look ahead rate control algorithm. */
    MFX_RATECONTROL_LA_HRD    =13,
    /*!
       Use the variable bitrate control algorithm with constant quality. This algorithm trying to achieve the target subjective quality with
       the minimum number of bits, while the bitrate constraint and HRD compliance are satisfied. It uses the same set of parameters
       as VBR and quality factor specified by mfxExtCodingOption3::QVBRQuality.
    */
    MFX_RATECONTROL_QVBR      =14,
};

/*!
   The TrellisControl enumerator is used to control trellis quantization in AVC encoder. The application can turn it on
   or off for any combination of I-, P- and B-frames by combining different enumerator values. For example, MFX_TRELLIS_I | MFX_TRELLIS_B
   turns it on for I- and B-frames.

   @note Due to performance reason on some target usages trellis quantization is always turned off and this control is ignored by the encoder.
*/
enum {
    MFX_TRELLIS_UNKNOWN =0,    /*!< Default value, it is up to the encoder to turn trellis quantization on or off. */
    MFX_TRELLIS_OFF     =0x01, /*!< Turn trellis quantization off for all frame types. */
    MFX_TRELLIS_I       =0x02, /*!< Turn trellis quantization on for I-frames. */
    MFX_TRELLIS_P       =0x04, /*!< Turn trellis quantization on for P-frames. */
    MFX_TRELLIS_B       =0x08  /*!< Turn trellis quantization on for B-frames. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Specifies additional options for encoding.

   The application can attach this extended buffer to the mfxVideoParam structure to configure initialization.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CODING_OPTION. */

    mfxU16      reserved1;
    mfxU16      RateDistortionOpt;      /*!< Set this flag if rate distortion optimization is needed. See the CodingOptionValue enumerator for values of this option. */
    mfxU16      MECostType;             /*!< Motion estimation cost type. This value is reserved and must be zero. */
    mfxU16      MESearchType;           /*!< Motion estimation search algorithm. This value is reserved and must be zero. */
    mfxI16Pair  MVSearchWindow;         /*!< Rectangular size of the search window for motion estimation. This parameter is reserved and must be (0, 0). */
    MFX_DEPRECATED mfxU16      EndOfSequence;          /* Deprecated */
    mfxU16      FramePicture;           /*!< Set this flag to encode interlaced fields as interlaced frames. This flag does not affect progressive input frames. See the CodingOptionValue enumerator for values of this option. */

    mfxU16      CAVLC;                  /*!< If set, CAVLC is used; if unset, CABAC is used for encoding. See the CodingOptionValue enumerator for values of this option. */
    mfxU16      reserved2[2];
    /*!
       Set this flag to insert the recovery point SEI message at the beginning of every intra refresh cycle. See the description of
       IntRefType in mfxExtCodingOption2 structure for details on how to enable and configure intra refresh.

       If intra refresh is not enabled then this flag is ignored.

       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      RecoveryPointSEI;
    /*!
       Set this flag to instruct the MVC encoder to output each view in separate bitstream buffer. See the CodingOptionValue enumerator
       for values of this option and the Multi-View Video Coding section for more details about usage of this flag.
    */
    mfxU16      ViewOutput;
    /*!
       If this option is turned ON, then AVC encoder produces an HRD conformant bitstream. If it is turned OFF, then the AVC encoder may (but not necessarily) violate HRD conformance. That is, this option can force the encoder to produce an HRD conformant stream, but
       cannot force it to produce a non-conformant stream.

       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      NalHrdConformance;
    /*!
       If set, encoder puts all SEI messages in the singe NAL unit. It includes messages provided by application and created
       by encoder. It is a three-states option. See CodingOptionValue enumerator for values of this option. The three states are:

       @li UNKNOWN Put each SEI in its own NAL unit.

       @li ON Put all SEI messages in the same NAL unit.

       @li OFF The same as unknown.
    */
    mfxU16      SingleSeiNalUnit;
    /*!
       If set and VBR rate control method is used, then VCL HRD parameters are written in bitstream with values identical to the values of the NAL HRD parameters.
       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      VuiVclHrdParameters;

    mfxU16      RefPicListReordering;   /*!< Set this flag to activate reference picture list reordering. This value is reserved and must be zero. */
    mfxU16      ResetRefList;           /*!< Set this flag to reset the reference list to non-IDR I-frames of a GOP sequence. See the CodingOptionValue enumerator for values of this option. */
    /*!
       Set this flag to write the reference picture marking repetition SEI message into the output bitstream.
       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      RefPicMarkRep;
    /*!
       Set this flag to instruct the AVC encoder to output bitstreams immediately after the encoder encodes a field,
       in the field-encoding mode. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      FieldOutput;

    mfxU16      IntraPredBlockSize;   /*!< Minimum block size of intra-prediction. This value is reserved and must be zero. */
    mfxU16      InterPredBlockSize;   /*!< Minimum block size of inter-prediction. This value is reserved and must be zero. */
    mfxU16      MVPrecision;          /*!< Specify the motion estimation precision. This parameter is reserved and must be zero. */
    mfxU16      MaxDecFrameBuffering; /*!< Specifies the maximum number of frames buffered in a DPB. A value of zero means unspecified. */

    mfxU16      AUDelimiter;            /*!< Set this flag to insert the Access Unit Delimiter NAL. See the CodingOptionValue enumerator for values of this option. */
    MFX_DEPRECATED mfxU16      EndOfStream;            /* Deprecated */
    /*!
       Set this flag to insert the picture timing SEI with pic_struct syntax element. See sub-clauses D.1.2 and D.2.2 of the ISO/IEC 14496-10
       specification for the definition of this syntax element. See the CodingOptionValue enumerator for values of this option.
       The default value is ON.
    */
    mfxU16      PicTimingSEI;
    mfxU16      VuiNalHrdParameters;  /*!< Set this flag to insert NAL HRD parameters in the VUI header. See the CodingOptionValue enumerator for values of this option. */
} mfxExtCodingOption;
MFX_PACK_END()

/*! The BRefControl enumerator is used to control usage of B-frames as reference in AVC encoder. */
enum {
    MFX_B_REF_UNKNOWN = 0, /*!< Default value, it is up to the encoder to use B-frames as reference. */
    MFX_B_REF_OFF     = 1, /*!< Do not use B-frames as reference. */
    MFX_B_REF_PYRAMID = 2  /*!< Arrange B-frames in so-called "B pyramid" reference structure. */
};

/*! The LookAheadDownSampling enumerator is used to control down sampling in look ahead bitrate control mode in AVC encoder. */
enum {
    MFX_LOOKAHEAD_DS_UNKNOWN = 0, /*!< Default value, it is up to the encoder what down sampling value to use. */
    MFX_LOOKAHEAD_DS_OFF     = 1, /*!< Do not use down sampling, perform estimation on original size frames. This is the slowest setting that produces the best quality. */
    MFX_LOOKAHEAD_DS_2x      = 2, /*!< Down sample frames two times before estimation. */
    MFX_LOOKAHEAD_DS_4x      = 3  /*!< Down sample frames four times before estimation. This option may significantly degrade quality. */
};

/*! The BPSEIControl enumerator is used to control insertion of buffering period SEI in the encoded bitstream. */
enum {
    MFX_BPSEI_DEFAULT = 0x00, /*!< encoder decides when to insert BP SEI. */
    MFX_BPSEI_IFRAME  = 0x01  /*!< BP SEI should be inserted with every I-frame */
};

/*! The SkipFrame enumerator is used to define usage of mfxEncodeCtrl::SkipFrame parameter. */
enum {
    MFX_SKIPFRAME_NO_SKIP         = 0, /*!< Frame skipping is disabled, mfxEncodeCtrl::SkipFrame is ignored. */
    MFX_SKIPFRAME_INSERT_DUMMY    = 1, /*!< Skipping is allowed, when mfxEncodeCtrl::SkipFrame is set encoder inserts into bitstream frame
                                            where all macroblocks are encoded as skipped. Only non-reference P- and B-frames can be skipped.
                                            If GopRefDist = 1 and mfxEncodeCtrl::SkipFrame is set for reference P-frame, it will be encoded
                                            as non-reference. */
    MFX_SKIPFRAME_INSERT_NOTHING  = 2, /*!< Similar to MFX_SKIPFRAME_INSERT_DUMMY, but when mfxEncodeCtrl::SkipFrame is set encoder inserts nothing into bitstream. */
    MFX_SKIPFRAME_BRC_ONLY        = 3, /*!< mfxEncodeCtrl::SkipFrame indicates number of missed frames before the current frame. Affects only BRC, current frame will be encoded as usual. */
};

/*! The IntraRefreshTypes enumerator itemizes types of intra refresh. */
enum {
        MFX_REFRESH_NO             = 0, /*!< Encode without refresh. */
        MFX_REFRESH_VERTICAL       = 1, /*!< Vertical refresh, by column of MBs. */
        MFX_REFRESH_HORIZONTAL     = 2, /*!< Horizontal refresh, by rows of MBs. */
        MFX_REFRESH_SLICE          = 3  /*!< Horizontal refresh by slices without overlapping. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used with the mfxExtCodingOption structure to specify additional options for encoding.

   The application can attach this extended buffer to the mfxVideoParam structure to configure initialization and to the mfxEncodeCtrl during runtime.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CODING_OPTION2. */

    /*!
       Specifies intra refresh type. See the IntraRefreshTypes. The major goal of intra refresh is improvement of error resilience without
       significant impact on encoded bitstream size caused by I-frames. The  encoder achieves this by encoding part of each frame in the refresh
       cycle using intra MBs.

       This parameter is valid during initialization and
       runtime. When used with temporal scalability, intra refresh applied only to base layer.

       MFX_REFRESH_NO No refresh.

       MFX_REFRESH_VERTICAL Vertical refresh, by column of MBs.

       MFX_REFRESH_HORIZONTAL Horizontal refresh, by rows of MBs.

       MFX_REFRESH_SLICE Horizontal refresh by slices without overlapping.

       MFX_REFRESH_SLICE Library ignores IntRefCycleSize (size of refresh cycle equals number slices).
    */
    mfxU16      IntRefType;
    /*!
       Specifies number of pictures within refresh cycle starting from 2. 0 and 1 are invalid values. This parameter is valid only during initialization.
    */
    mfxU16      IntRefCycleSize;
    /*!
       Specifies QP difference for inserted intra MBs. Signed values are in the -51 to 51 range. This parameter is valid during initialization and runtime.
    */
    mfxI16      IntRefQPDelta;

    /*!
       Specify maximum encoded frame size in byte. This parameter is used in VBR based bitrate control modes and ignored in others.
       The encoder tries to keep frame size below specified limit but minor overshoots are possible to preserve visual quality.
       This parameter is valid during initialization and runtime. It is recommended to set MaxFrameSize to 5x-10x target frame size
       ((TargetKbps*1000)/(8* FrameRateExtN/FrameRateExtD)) for I-frames and 2x-4x target frame size for P- and B-frames.
    */
    mfxU32      MaxFrameSize;
    /*!
       Specify maximum slice size in bytes. If this parameter is specified other controls over number of slices are ignored.

       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU32      MaxSliceSize;

    /*!
       Modifies bitrate to be in the range imposed by the encoder. The default value is ON, that is, bitrate is limited. Setting this flag to OFF may lead to violation of HRD conformance.Specifying bitrate below the encoder range might significantly affect quality.

       If set to ON, this option takes effect in non CQP modes:
       if TargetKbps is not in the range imposed by the encoder, it will be changed to be in the range.

       This parameter is valid only during initialization. Flag works with MFX_CODEC_AVC only, it is ignored with other codecs.
       See the CodingOptionValue
       enumerator for values of this option.

       @deprecated Deprecated in API version 2.9
    */
    MFX_DEPRECATED mfxU16      BitrateLimit; /* Deprecated */
    /*!
       Setting this flag enables macroblock level bitrate control that generally improves subjective visual quality. Enabling this flag may
       have negative impact on performance and objective visual quality metric. See the CodingOptionValue enumerator for values of this option.
       The default value depends on target usage settings.
    */
    mfxU16      MBBRC;
    /*!
       Set this option to ON to enable external BRC. See the CodingOptionValue enumerator for values of this option.
       Use the Query API function to check if this feature is supported.
    */
    mfxU16      ExtBRC;
    /*!
       Specifies the depth of the look ahead rate control algorithm. The depth value is the number of frames that the encoder analyzes before encoding. Values are in the 10 to 100 range, inclusive.
       To instruct the encoder to use the default value the application should zero this field.
    */
    mfxU16      LookAheadDepth;
    /*!
       Used to control trellis quantization in AVC encoder. See TrellisControl enumerator for values of this option.
       This parameter is valid only during initialization.
    */
    mfxU16      Trellis;
    /*!
       Controls picture parameter set repetition in AVC encoder. Set this flag to ON to repeat PPS with each frame.
       See the CodingOptionValue enumerator for values of this option. The default value is ON. This parameter is valid only during initialization.
    */
    mfxU16      RepeatPPS;
    /*!
       Controls usage of B-frames as reference. See BRefControl enumerator for values of this option.
       This parameter is valid only during initialization.
    */
    mfxU16      BRefType;
    /*!
       Controls insertion of I-frames by the encoder. Set this flag to ON to allow changing of frame type from P and B to I.
       This option is ignored if GopOptFlag in mfxInfoMFX structure is equal to MFX_GOP_STRICT. See the CodingOptionValue enumerator
       for values of this option. This parameter is valid only during initialization.
    */
    mfxU16      AdaptiveI;
    /*!
       Controls changing of frame type from B to P. Set this flag to ON enable changing of frame type from B to P. This option is ignored if
       GopOptFlag in mfxInfoMFX structure is equal to MFX_GOP_STRICT. See the CodingOptionValue enumerator for values of this option.
       This parameter is valid only during initialization.
    */
    mfxU16      AdaptiveB;
    /*!
       Controls down sampling in look ahead bitrate control mode. See LookAheadDownSampling enumerator for values
       of this option. This parameter is valid only during initialization.
    */
    mfxU16      LookAheadDS;
    /*!
       Specifies suggested slice size in number of macroblocks. The library can adjust this number based on platform capability.
       If this option is specified, that is, if it is not equal to zero, the library ignores mfxInfoMFX::NumSlice parameter.
    */
    mfxU16      NumMbPerSlice;
    /*!
       Enables usage of mfxEncodeCtrl::SkipFrame parameter. See the SkipFrame enumerator for values of this option.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      SkipFrame;
    mfxU8       MinQPI; /*!< Minimum allowed QP value for I-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU8       MaxQPI; /*!< Maximum allowed QP value for I-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                             Adjusted MaxQPI value = 51 + (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                             For HEVC_MAIN10, we add (6*(10-8)=12) on our side for MaxQPI will reach 63.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU8       MinQPP; /*!< Minimum allowed QP value for P-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU8       MaxQPP; /*!< Maximum allowed QP value for P-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                             Adjusted MaxQPP value = 51 + (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                             For HEVC_MAIN10, we add (6*(10-8)=12) on our side for MaxQPP will reach 63.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU8       MinQPB; /*!< Minimum allowed QP value for B-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    mfxU8       MaxQPB; /*!< Maximum allowed QP value for B-frame types. Valid range varies with the codec. Zero means default value, that is, no limitations on QP.
                             @note In the HEVC design, a further adjustment to QPs can occur based on bit depth.
                             Adjusted MaxQPB value = 51 + (6 * (BitDepthLuma - 8)) for BitDepthLuma in the range [8,14].
                             For HEVC_MAIN10, we add (6*(10-8)=12) on our side for MaxQPB will reach 63.
                             @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported. */
    /*!
       Sets fixed_frame_rate_flag in VUI.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      FixedFrameRate;
    /*! Disables deblocking.
        @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      DisableDeblockingIdc;
    /*!
       Completely disables VUI in the output bitstream.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      DisableVUI;
    /*!
       Controls insertion of buffering period SEI in the encoded bitstream. It should be one of the following values:

       MFX_BPSEI_DEFAULT   Encoder decides when to insert BP SEI,

       MFX_BPSEI_IFRAME    BP SEI should be inserted with every I-frame.
    */
    mfxU16      BufferingPeriodSEI;
    /*!
       Set this flag to ON to enable per-frame reporting of Mean Absolute Difference. This parameter is valid only during initialization.
    */
    mfxU16      EnableMAD;
    /*!
       Set this flag to ON to use raw frames for reference instead of reconstructed frames. This parameter is valid during
       initialization and runtime (only if was turned ON during initialization).
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      UseRawRef;
} mfxExtCodingOption2;
MFX_PACK_END()

/*! The WeightedPred enumerator itemizes weighted prediction modes. */
enum {
    MFX_WEIGHTED_PRED_UNKNOWN  = 0, /*!< Allow encoder to decide. */
    MFX_WEIGHTED_PRED_DEFAULT  = 1, /*!< Use default weighted prediction. */
    MFX_WEIGHTED_PRED_EXPLICIT = 2, /*!< Use explicit weighted prediction. */
    MFX_WEIGHTED_PRED_IMPLICIT = 3  /*!< Use implicit weighted prediction (for B-frames only). */
};

/*! The ScenarioInfo enumerator itemizes scenarios for the encoding session. */
enum {
    MFX_SCENARIO_UNKNOWN             = 0,
    MFX_SCENARIO_DISPLAY_REMOTING    = 1,
    MFX_SCENARIO_VIDEO_CONFERENCE    = 2,
    MFX_SCENARIO_ARCHIVE             = 3,
    MFX_SCENARIO_LIVE_STREAMING      = 4,
    MFX_SCENARIO_CAMERA_CAPTURE      = 5,
    MFX_SCENARIO_VIDEO_SURVEILLANCE  = 6,
    MFX_SCENARIO_GAME_STREAMING      = 7,
    MFX_SCENARIO_REMOTE_GAMING       = 8
};

/*! The ContentInfo enumerator itemizes content types for the encoding session. */
enum {
    MFX_CONTENT_UNKNOWN              = 0,
    MFX_CONTENT_FULL_SCREEN_VIDEO    = 1,
    MFX_CONTENT_NON_VIDEO_SCREEN     = 2,
    MFX_CONTENT_NOISY_VIDEO          = 3
};

/*! The PRefType enumerator itemizes models of reference list construction and DPB management when GopRefDist=1. */
enum {
    MFX_P_REF_DEFAULT = 0, /*!< Allow encoder to decide. */
    MFX_P_REF_SIMPLE  = 1, /*!< Regular sliding window used for DPB removal process. */
    MFX_P_REF_PYRAMID = 2  /*!< Let N be the max reference list's size. Encoder treats each N's frame as a 'strong'
                                reference and the others as 'weak' references. The encoder uses a 'weak' reference only for
                                prediction of the next frame and removes it from DPB immediately after use. 'Strong' references are removed from
                                DPB by a sliding window. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used with mfxExtCodingOption and mfxExtCodingOption2 structures to specify additional options for encoding.
   The application can attach this extended buffer to the mfxVideoParam structure to configure initialization and to the mfxEncodeCtrl during runtime.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CODING_OPTION3. */

    mfxU16      NumSliceI; /*!< The number of slices for I-frames.
                                @note Not all codecs and implementations support these values. Use the Query API function to check if this feature is supported */
    mfxU16      NumSliceP; /*!< The number of slices for P-frames.
                                @note Not all codecs and implementations support these values. Use the Query API function to check if this feature is supported */
    mfxU16      NumSliceB; /*!< The number of slices for B-frames.
                                @note Not all codecs and implementations support these values. Use the Query API function to check if this feature is supported */

    /*!
       When rate control method is MFX_RATECONTROL_CBR, MFX_RATECONTROL_VBR, MFX_RATECONTROL_LA, MFX_RATECONTROL_LA_HRD, or MFX_RATECONTROL_QVBR
       this parameter specifies the maximum bitrate averaged over a sliding window specified by WinBRCSize.
    */
    mfxU16      WinBRCMaxAvgKbps;
    /*!
       When rate control method is MFX_RATECONTROL_CBR, MFX_RATECONTROL_VBR, MFX_RATECONTROL_LA, MFX_RATECONTROL_LA_HRD, or MFX_RATECONTROL_QVBR
       this parameter specifies sliding window size in frames. Set WinBRCMaxAvgKbps and WinBRCSize to zero to disable sliding window.
    */
    mfxU16      WinBRCSize;

    /*! When rate control method is MFX_RATECONTROL_QVBR, this parameter specifies quality factor.
        Values are in the 1 to 51 range, where 1 corresponds to the best quality.
    */
    mfxU16      QVBRQuality;
    /*!
       Set this flag to ON to enable per-macroblock QP control. Rate control method must be MFX_RATECONTROL_CQP. See the CodingOptionValue
       enumerator for values of this option. This parameter is valid only during initialization.
    */
    mfxU16      EnableMBQP;
    /*!
       Distance between the beginnings of the intra-refresh cycles in frames. Zero means no distance between cycles.
    */
    mfxU16      IntRefCycleDist;
    /*!
       Set this flag to ON to enable the ENC mode decision algorithm to bias to fewer B Direct/Skip types. Applies only to B-frames,
       all other frames will ignore this setting. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      DirectBiasAdjustment;
    /*!
       Enables global motion bias. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      GlobalMotionBiasAdjustment;
    /*!
       Values are:

       @li 0: Set MV cost to be 0.

       @li 1: Scale MV cost to be 1/2 of the default value.

       @li 2: Scale MV cost to be 1/4 of the default value.

       @li 3: Scale MV cost to be 1/8 of the default value.
    */
    mfxU16      MVCostScalingFactor;
    /*!
       Set this flag to ON to enable usage of mfxExtMBDisableSkipMap. See the CodingOptionValue enumerator for values of this option.
       This parameter is valid only during initialization.
    */
    mfxU16      MBDisableSkipMap;

    mfxU16      WeightedPred;   /*!< Weighted prediction mode. See the WeightedPred enumerator for values of these options. */
    mfxU16      WeightedBiPred; /*!< Weighted prediction mode. See the WeightedPred enumerator for values of these options. */

    /*!
       Instructs encoder whether aspect ratio info should present in VUI parameters. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      AspectRatioInfoPresent;
    /*!
       Instructs encoder whether overscan info should present in VUI parameters. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      OverscanInfoPresent;
    /*!
       ON indicates that the cropped decoded pictures output are suitable for display using overscan. OFF indicates that the cropped decoded
       pictures output contain visually important information in the entire region out to the edges of the cropping rectangle of the picture.
       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      OverscanAppropriate;
    /*!
       Instructs encoder whether frame rate info should present in VUI parameters. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      TimingInfoPresent;
    /*!
       Instructs encoder whether bitstream restriction info should present in VUI parameters. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      BitstreamRestriction;
    /*!
       Corresponds to AVC syntax element low_delay_hrd_flag (VUI). See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      LowDelayHrd;
    /*!
       When set to OFF, no sample outside the picture boundaries and no sample at a fractional sample position for which the sample value
       is derived using one or more samples outside the picture boundaries is used for inter prediction of any sample.

       When set to ON, one or more samples outside picture boundaries may be used in inter prediction.

       See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      MotionVectorsOverPicBoundaries;
    mfxU16      reserved1[2];

    mfxU16      ScenarioInfo; /*!< Provides a hint to encoder about the scenario for the encoding session. See the ScenarioInfo enumerator for values of this option. */
    mfxU16      ContentInfo;  /*!< Provides a hint to encoder about the content for the encoding session. See the ContentInfo enumerator for values of this option. */

    mfxU16      PRefType;     /*!< When GopRefDist=1, specifies the model of reference list construction and DPB management. See the PRefType enumerator for values of this option. */
    /*!
       Instructs encoder whether internal fade detection algorithm should be used for calculation of weigh/offset values for pred_weight_table
       unless application provided mfxExtPredWeightTable for this frame. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      FadeDetection;
    mfxU16      reserved2[2];
    /*!
       Set this flag to OFF to make HEVC encoder use regular P-frames instead of GPB. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      GPB;

    /*!
       Same as mfxExtCodingOption2::MaxFrameSize but affects only I-frames. MaxFrameSizeI must be set if MaxFrameSizeP is set.
       If MaxFrameSizeI is not specified or greater than spec limitation, spec limitation will be applied to the sizes of I-frames.
    */
    mfxU32      MaxFrameSizeI;
    /*!
       Same as mfxExtCodingOption2::MaxFrameSize but affects only P/B-frames. If MaxFrameSizeP equals 0, the library sets MaxFrameSizeP
       equal to MaxFrameSizeI. If MaxFrameSizeP is not specified or greater than spec limitation, spec limitation will be applied to the
       sizes of P/B-frames.
    */
    mfxU32      MaxFrameSizeP;
    mfxU32      reserved3[3];

    /*!
       Enables QPOffset control. See the CodingOptionValue enumerator for values of this option.
    */
    mfxU16      EnableQPOffset;
    /*!
       Specifies QP offset per pyramid layer when EnableQPOffset is set to ON and RateControlMethod is CQP.

       For B-pyramid, B-frame QP = QPB + QPOffset[layer].

       For P-pyramid, P-frame QP = QPP + QPOffset[layer].
    */
    mfxI16      QPOffset[8];              /* FrameQP = QPX + QPOffset[pyramid_layer]; QPX = QPB for B-pyramid, QPP for P-pyramid */


    mfxU16      NumRefActiveP[8];   /*!< Max number of active references for P-frames. Array index is pyramid layer. */
    mfxU16      NumRefActiveBL0[8]; /*!< Max number of active references for B-frames in reference picture list 0. Array index is pyramid layer. */
    mfxU16      NumRefActiveBL1[8]; /*!< Max number of active references for B-frames in reference picture list 1. Array index is pyramid layer. */

    mfxU16      reserved6;
    /*!
       For HEVC if this option is turned ON, the transform_skip_enabled_flag will be set to 1 in PPS. OFF specifies that transform_skip_enabled_flag will be set to 0.
    */
    mfxU16      TransformSkip;
    /*!
       Minus 1 specifies target encoding chroma format (see ChromaFormatIdc enumerator). May differ from the source format.
       TargetChromaFormatPlus1 = 0 specifies the default target chroma format which is equal to source (mfxVideoParam::mfx::FrameInfo::ChromaFormat + 1),
       except RGB4 source format. In case of RGB4 source format default target , chroma format is 4:2:0 (instead of 4:4:4)
       for the purpose of backward compatibility.
    */
    mfxU16      TargetChromaFormatPlus1;
    /*!
       Target encoding bit-depth for luma samples. May differ from source bit-depth. 0 specifies a default target bit-depth that is equal to
       source (mfxVideoParam::mfx::FrameInfo::BitDepthLuma).
    */
    mfxU16      TargetBitDepthLuma;
    /*!
       Target encoding bit-depth for chroma samples. May differ from source bit-depth. 0 specifies a default target bit-depth that is equal to
       source (mfxVideoParam::mfx::FrameInfo::BitDepthChroma).
    */
    mfxU16      TargetBitDepthChroma;
    mfxU16      BRCPanicMode; /*!< Controls panic mode in AVC and MPEG2 encoders. */

    /*!
       When rate control method is MFX_RATECONTROL_VBR, MFX_RATECONTROL_QVBR or MFX_RATECONTROL_VCM this parameter specifies frame size
       tolerance. Set this parameter to MFX_CODINGOPTION_ON to allow strictly obey average frame size set by MaxKbps, for example cases when
       MaxFrameSize == (MaxKbps*1000)/(8* FrameRateExtN/FrameRateExtD). Also MaxFrameSizeI and MaxFrameSizeP can be set separately.
    */
    mfxU16      LowDelayBRC;
    /*!
       Set this flag to ON to enable usage of mfxExtMBForceIntra for AVC encoder. See the CodingOptionValue enumerator
       for values of this option. This parameter is valid only during initialization.
    */
    mfxU16      EnableMBForceIntra;
    /*!
       If this flag is set to ON, BRC may decide a larger P- or B-frame size than what MaxFrameSizeP dictates when the scene change is detected.
       It may benefit the video quality. AdaptiveMaxFrameSize feature is not supported with LowPower ON or if the value of MaxFrameSizeP = 0.
    */
    mfxU16      AdaptiveMaxFrameSize;

    /*!
       Controls AVC encoder attempts to predict from small partitions. Default value allows encoder to choose preferred mode.
       MFX_CODINGOPTION_ON forces encoder to favor quality and MFX_CODINGOPTION_OFF forces encoder to favor performance.
    */
    mfxU16      RepartitionCheckEnable;
    mfxU16      reserved5[3];
    mfxU16      EncodedUnitsInfo;          /*!< Set this flag to ON to make encoded units info available in mfxExtEncodedUnitsInfo. */
    /*!
       If this flag is set to ON, the HEVC encoder uses the NAL unit type provided by the application in the mfxEncodeCtrl::MfxNalUnitType field.
       This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16      EnableNalUnitType;

    union {
        MFX_DEPRECATED mfxU16      ExtBrcAdaptiveLTR; /* Deprecated */

        /*!
            If this flag is set to ON, encoder will mark, modify, or remove LTR frames based on encoding parameters and content
            properties. Turn OFF to prevent Adaptive marking of Long Term Reference Frames.
        */
        mfxU16      AdaptiveLTR;
    };
    /*!
       If this flag is set to ON, encoder adaptively selects one of implementation-defined quantization matrices for each frame.
       Non-default quantization matrices aim to improve subjective visual quality under certain conditions.
       Their number and definitions are API implementation specific.
       If this flag is set to OFF, default quantization matrix is used for all frames.
       This parameter is valid only during initialization.
    */
    mfxU16      AdaptiveCQM;
    /*!
       If this flag is set to ON, encoder adaptively selects list of reference frames to improve encoding quality.
       Enabling of the flag can increase computation complexity and introduce additional delay.
       If this flag is set to OFF, regular reference frames are used for encoding.
    */
    mfxU16      AdaptiveRef;

    mfxU16      reserved[161];

} mfxExtCodingOption3;
MFX_PACK_END()

/*! IntraPredBlockSize/InterPredBlockSize specifies the minimum block size of inter-prediction. */
enum {
    MFX_BLOCKSIZE_UNKNOWN   = 0, /*!< Unspecified. */
    MFX_BLOCKSIZE_MIN_16X16 = 1, /*!< 16x16 minimum block size.              */
    MFX_BLOCKSIZE_MIN_8X8   = 2, /*!< 8x8 minimum block size. May be 16x16 or 8x8.         */
    MFX_BLOCKSIZE_MIN_4X4   = 3  /*!< 4x4 minimum block size. May be 16x16, 8x8, or 4x4.    */
};

/*! The MVPrecision enumerator specifies the motion estimation precision. */
enum {
    MFX_MVPRECISION_UNKNOWN    = 0,
    MFX_MVPRECISION_INTEGER    = (1 << 0),
    MFX_MVPRECISION_HALFPEL    = (1 << 1),
    MFX_MVPRECISION_QUARTERPEL = (1 << 2)
};

/*! The CodingOptionValue enumerator defines a three-state coding option setting. */
enum {
    MFX_CODINGOPTION_UNKNOWN    =0,    /*!< Unspecified. */
    MFX_CODINGOPTION_ON         =0x10, /*!< Coding option set. */
    MFX_CODINGOPTION_OFF        =0x20, /*!< Coding option not set. */
    MFX_CODINGOPTION_ADAPTIVE   =0x30  /*!< Reserved. */
};

/*! The BitstreamDataFlag enumerator uses bit-ORed values to itemize additional information about the bitstream buffer. */
enum {
    MFX_BITSTREAM_NO_FLAG           = 0x0000, /*!< The bitstream doesn't contain any flags. */
    /*!
       The bitstream buffer contains a complete frame or complementary field pair of data for the bitstream. For decoding, this means
       that the decoder can proceed with this buffer without waiting for the start of the next frame, which effectively reduces decoding latency.
       If this flag is set, but the bitstream buffer contains incomplete frame or pair of field, then decoder will produce corrupted output.
    */
    MFX_BITSTREAM_COMPLETE_FRAME    = 0x0001,
    /*!
       The bitstream buffer contains the end of the stream. For decoding,
       this means that the application does not have any additional bitstream data to send to decoder.
    */
    MFX_BITSTREAM_EOS               = 0x0002
};
/*! The ExtendedBufferID enumerator itemizes and defines identifiers (BufferId) for extended buffers or video processing algorithm identifiers. */
enum {
    /*!
       This extended buffer defines additional encoding controls. See the mfxExtCodingOption structure for details.
       The application can attach this buffer to the structure for encoding initialization.
    */
    MFX_EXTBUFF_CODING_OPTION                   = MFX_MAKEFOURCC('C','D','O','P'),
    /*!
       This extended buffer defines sequence header and picture header for encoders and decoders. See the mfxExtCodingOptionSPSPPS
       structure for details. The application can attach this buffer to the mfxVideoParam structure for encoding initialization,
       and for obtaining raw headers from the decoders and encoders.
    */
    MFX_EXTBUFF_CODING_OPTION_SPSPPS            = MFX_MAKEFOURCC('C','O','S','P'),
    /*!
       This extended buffer defines a list of VPP algorithms that applications should not use. See the mfxExtVPPDoNotUse structure
       for details. The application can attach this buffer to the mfxVideoParam structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_DONOTUSE                    = MFX_MAKEFOURCC('N','U','S','E'),
    /*!
       This extended buffer defines auxiliary information at the VPP output. See the mfxExtVppAuxData structure for details. The application
       can attach this buffer to the mfxEncodeCtrl structure for per-frame encoding control.
    */
    MFX_EXTBUFF_VPP_AUXDATA                     = MFX_MAKEFOURCC('A','U','X','D'),
    /*!
       The extended buffer defines control parameters for the VPP denoise filter algorithm. See the mfxExtVPPDenoise2 structure for details.
       The application can attach this buffer to the mfxVideoParam structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_DENOISE2                    = MFX_MAKEFOURCC('D','N','I','2'),
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_EXTBUFF_VPP_DENOISE)                     = MFX_MAKEFOURCC('D','N','I','S'), /*!< Deprecated in 2.2 API version.*/
    MFX_EXTBUFF_VPP_SCENE_ANALYSIS              = MFX_MAKEFOURCC('S','C','L','Y'), /*!< Reserved for future use. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_EXTBUFF_VPP_SCENE_CHANGE)                = MFX_EXTBUFF_VPP_SCENE_ANALYSIS, /* Deprecated. */
    /*!
       The extended buffer defines control parameters for the VPP ProcAmp filter algorithm. See the mfxExtVPPProcAmp structure for details.
       The application can attach this buffer to the mfxVideoParam structure for video processing initialization or to the mfxFrameData
       structure in the mfxFrameSurface1 structure of output surface for per-frame processing configuration.
    */
    MFX_EXTBUFF_VPP_PROCAMP                     = MFX_MAKEFOURCC('P','A','M','P'),
    /*!
       The extended buffer defines control parameters for the VPP detail filter algorithm. See the mfxExtVPPDetail structure for details.
       The application can attach this buffer to the structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_DETAIL                      = MFX_MAKEFOURCC('D','E','T',' '),
    /*!
       This extended buffer defines video signal type. See the mfxExtVideoSignalInfo structure for details. The application can attach this
       buffer to the mfxVideoParam structure for encoding initialization, and for retrieving such information from the decoders. If video
       signal info changes per frame, the application can attach this buffer to the mfxFrameData structure for video processing.
    */
    MFX_EXTBUFF_VIDEO_SIGNAL_INFO               = MFX_MAKEFOURCC('V','S','I','N'),
    /*!
       This extended buffer defines video signal type. See the mfxExtVideoSignalInfo structure for details. The application can attach this
       buffer to the mfxVideoParam structure for the input of video processing if the input video signal information changes in sequence
       base.
    */
    MFX_EXTBUFF_VIDEO_SIGNAL_INFO_IN            = MFX_MAKEFOURCC('V','S','I','I'),
    /*!
       This extended buffer defines video signal type. See the mfxExtVideoSignalInfo structure for details. The application can attach this
       buffer to the mfxVideoParam structure for the output of video processing if the output video signal information changes in sequence
       base.
    */
    MFX_EXTBUFF_VIDEO_SIGNAL_INFO_OUT           = MFX_MAKEFOURCC('V','S','I','O'),
    /*!
       This extended buffer defines a list of VPP algorithms that applications should use. See the mfxExtVPPDoUse structure for details.
       The application can attach this buffer to the structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_DOUSE                       = MFX_MAKEFOURCC('D','U','S','E'),
    /*!
       This extended buffer defines additional encoding controls for reference list. See the mfxExtAVCRefListCtrl structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding & decoding initialization, or the mfxEncodeCtrl
       structure for per-frame encoding configuration.
    */
    MFX_EXTBUFF_AVC_REFLIST_CTRL                = MFX_MAKEFOURCC('R','L','S','T'),
    /*!
       This extended buffer defines control parameters for the VPP frame rate conversion algorithm. See the mfxExtVPPFrameRateConversion structure
       for details. The application can attach this buffer to the mfxVideoParam structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION       = MFX_MAKEFOURCC('F','R','C',' '),
    /*!
       This extended buffer configures the H.264 picture timing SEI message. See the mfxExtPictureTimingSEI structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding initialization, or the mfxEncodeCtrl structure
       for per-frame encoding configuration.
    */
    MFX_EXTBUFF_PICTURE_TIMING_SEI              = MFX_MAKEFOURCC('P','T','S','E'),
    /*!
       This extended buffer configures the structure of temporal layers inside the encoded H.264 bitstream. See the mfxExtAvcTemporalLayers
       structure for details. The application can attach this buffer to the mfxVideoParam structure for encoding initialization.
    */
    MFX_EXTBUFF_AVC_TEMPORAL_LAYERS             = MFX_MAKEFOURCC('A','T','M','L'),
    /*!
       This extended buffer defines additional encoding controls. See the mfxExtCodingOption2 structure for details.
       The application can attach this buffer to the structure for encoding initialization.
    */
    MFX_EXTBUFF_CODING_OPTION2                  = MFX_MAKEFOURCC('C','D','O','2'),
    /*!
       This extended buffer defines control parameters for the VPP image stabilization filter algorithm. See the mfxExtVPPImageStab structure
       for details. The application can attach this buffer to the mfxVideoParam structure for video processing initialization.
    */
    MFX_EXTBUFF_VPP_IMAGE_STABILIZATION         = MFX_MAKEFOURCC('I','S','T','B'),
    /*!
       This extended buffer is used to retrieve encoder capability. See the mfxExtEncoderCapability structure for details.
       The application can attach this buffer to the mfxVideoParam structure before calling MFXVideoENCODE_Query function.
    */
    MFX_EXTBUFF_ENCODER_CAPABILITY              = MFX_MAKEFOURCC('E','N','C','P'),
    /*!
       This extended buffer is used to control encoder reset behavior and also to query possible encoder reset outcome.
       See the mfxExtEncoderResetOption structure for details. The application can attach this buffer to the mfxVideoParam structure
       before calling MFXVideoENCODE_Query or MFXVideoENCODE_Reset functions.
    */
    MFX_EXTBUFF_ENCODER_RESET_OPTION            = MFX_MAKEFOURCC('E','N','R','O'),
    /*!
       This extended buffer is used by the encoder to report additional information about encoded picture.
       See the mfxExtAVCEncodedFrameInfo structure for details. The application can attach this buffer to the mfxBitstream structure
       before calling MFXVideoENCODE_EncodeFrameAsync function.
    */
    MFX_EXTBUFF_ENCODED_FRAME_INFO              = MFX_MAKEFOURCC('E','N','F','I'),
    /*!
       This extended buffer is used to control composition of several input surfaces in the one output. In this mode,
       the VPP skips any other filters. The VPP returns error if any mandatory filter is specified and filter skipped warning
       for optional filter. The only supported filters are deinterlacing and interlaced scaling.
    */
    MFX_EXTBUFF_VPP_COMPOSITE                   = MFX_MAKEFOURCC('V','C','M','P'),
    /*!
       This extended buffer is used to control transfer matrix and nominal range of YUV frames.
       The application should provide it during initialization.
    */
    MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO           = MFX_MAKEFOURCC('V','V','S','I'),
    /*!
       This extended buffer is used by the application to specify different Region Of Interests during encoding.
       The application should provide it at initialization or at runtime.
    */
    MFX_EXTBUFF_ENCODER_ROI                     = MFX_MAKEFOURCC('E','R','O','I'),
    /*!
       This extended buffer is used by the application to specify different deinterlacing algorithms.
    */
    MFX_EXTBUFF_VPP_DEINTERLACING               = MFX_MAKEFOURCC('V','P','D','I'),
    /*!
       This extended buffer specifies reference lists for the encoder.
    */
    MFX_EXTBUFF_AVC_REFLISTS                    = MFX_MAKEFOURCC('R','L','T','S'),
    /*!
       See the mfxExtDecVideoProcessing structure for details.
    */
    MFX_EXTBUFF_DEC_VIDEO_PROCESSING            = MFX_MAKEFOURCC('D','E','C','V'),
    /*!
       The extended buffer defines control parameters for the VPP field-processing algorithm. See the mfxExtVPPFieldProcessing
       structure for details. The application can attach this buffer to the mfxVideoParam structure for video processing initialization
       or to the mfxFrameData structure during runtime.
    */
    MFX_EXTBUFF_VPP_FIELD_PROCESSING            = MFX_MAKEFOURCC('F','P','R','O'),
    /*!
       This extended buffer defines additional encoding controls. See the mfxExtCodingOption3 structure for details.
       The application can attach this buffer to the structure for encoding initialization.
    */
    MFX_EXTBUFF_CODING_OPTION3                  = MFX_MAKEFOURCC('C','D','O','3'),
    /*!
       This extended buffer defines chroma samples location information. See the mfxExtChromaLocInfo structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding initialization.
    */
    MFX_EXTBUFF_CHROMA_LOC_INFO                 = MFX_MAKEFOURCC('C','L','I','N'),
    /*!
       This extended buffer defines per-macroblock QP. See the mfxExtMBQP structure for details.
       The application can attach this buffer to the mfxEncodeCtrl structure for per-frame encoding configuration.
    */
    MFX_EXTBUFF_MBQP                            = MFX_MAKEFOURCC('M','B','Q','P'),
    /*!
       This extended buffer defines per-macroblock force intra flag. See the mfxExtMBForceIntra structure for details.
       The application can attach this buffer to the mfxEncodeCtrl structure for per-frame encoding configuration.
    */
    MFX_EXTBUFF_MB_FORCE_INTRA                  = MFX_MAKEFOURCC('M','B','F','I'),
    /*!
       This extended buffer defines additional encoding controls for HEVC tiles. See the mfxExtHEVCTiles structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding initialization.
    */
    MFX_EXTBUFF_HEVC_TILES                      = MFX_MAKEFOURCC('2','6','5','T'),
    /*!
       This extended buffer defines macroblock map for current frame which forces specified macroblocks to be non skip. See the
       mfxExtMBDisableSkipMap structure for details. The application can attach this buffer to the mfxEncodeCtrl structure for
       per-frame encoding configuration.
    */
    MFX_EXTBUFF_MB_DISABLE_SKIP_MAP             = MFX_MAKEFOURCC('M','D','S','M'),
    /*!
       See the mfxExtHEVCParam structure for details.
    */
    MFX_EXTBUFF_HEVC_PARAM                      = MFX_MAKEFOURCC('2','6','5','P'),
    /*!
       This extended buffer is used by decoders to report additional information about decoded frame. See the
       mfxExtDecodedFrameInfo structure for more details.
    */
    MFX_EXTBUFF_DECODED_FRAME_INFO              = MFX_MAKEFOURCC('D','E','F','I'),
    /*!
       See the mfxExtTimeCode structure for more details.
    */
    MFX_EXTBUFF_TIME_CODE                       = MFX_MAKEFOURCC('T','M','C','D'),
    /*!
       This extended buffer specifies the region to encode. The application can attach this buffer to the
       mfxVideoParam structure during HEVC encoder initialization.
    */
    MFX_EXTBUFF_HEVC_REGION                     = MFX_MAKEFOURCC('2','6','5','R'),
    /*!
       See the mfxExtPredWeightTable structure for details.
    */
    MFX_EXTBUFF_PRED_WEIGHT_TABLE               = MFX_MAKEFOURCC('E','P','W','T'),
    /*!
       See the mfxExtDirtyRect structure for details.
    */
    MFX_EXTBUFF_DIRTY_RECTANGLES                = MFX_MAKEFOURCC('D','R','O','I'),
    /*!
       See the mfxExtMoveRect structure for details.
    */
    MFX_EXTBUFF_MOVING_RECTANGLES               = MFX_MAKEFOURCC('M','R','O','I'),
    /*!
       See the mfxExtCodingOptionVPS structure for details.
    */
    MFX_EXTBUFF_CODING_OPTION_VPS               = MFX_MAKEFOURCC('C','O','V','P'),
    /*!
       See the mfxExtVPPRotation structure for details.
    */
    MFX_EXTBUFF_VPP_ROTATION                    = MFX_MAKEFOURCC('R','O','T',' '),
    /*!
       See the mfxExtEncodedSlicesInfo structure for details.
    */
    MFX_EXTBUFF_ENCODED_SLICES_INFO             = MFX_MAKEFOURCC('E','N','S','I'),
    /*!
       See the mfxExtVPPScaling structure for details.
    */
    MFX_EXTBUFF_VPP_SCALING                     = MFX_MAKEFOURCC('V','S','C','L'),
    /*!
       This extended buffer defines additional encoding controls for reference list. See the mfxExtAVCRefListCtrl structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding & decoding initialization, or
       the mfxEncodeCtrl structure for per-frame encoding configuration.
    */
    MFX_EXTBUFF_HEVC_REFLIST_CTRL               = MFX_EXTBUFF_AVC_REFLIST_CTRL,
    /*!
       This extended buffer specifies reference lists for the encoder.
    */
    MFX_EXTBUFF_HEVC_REFLISTS                   = MFX_EXTBUFF_AVC_REFLISTS,
    /*!
       This extended buffer configures the structure of temporal layers inside the encoded H.265 bitstream. See the mfxExtHEVCTemporalLayers
       structure for details. The application can attach this buffer to the mfxVideoParam structure for encoding initialization.
    */
    MFX_EXTBUFF_HEVC_TEMPORAL_LAYERS            = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS,
    /*!
       See the mfxExtVPPMirroring structure for details.
    */
    MFX_EXTBUFF_VPP_MIRRORING                   = MFX_MAKEFOURCC('M','I','R','R'),
    /*!
       See the mfxExtMVOverPicBoundaries structure for details.
    */
    MFX_EXTBUFF_MV_OVER_PIC_BOUNDARIES          = MFX_MAKEFOURCC('M','V','P','B'),
    /*!
       See the mfxExtVPPColorFill structure for details.
    */
    MFX_EXTBUFF_VPP_COLORFILL                   = MFX_MAKEFOURCC('V','C','L','F'),
    /*!
       This extended buffer is used by decoders to report error information before frames get decoded.
       See the mfxExtDecodeErrorReport structure for more details.
    */
    MFX_EXTBUFF_DECODE_ERROR_REPORT             = MFX_MAKEFOURCC('D', 'E', 'R', 'R'),
    /*!
       See the mfxExtColorConversion structure for details.
    */
    MFX_EXTBUFF_VPP_COLOR_CONVERSION            = MFX_MAKEFOURCC('V', 'C', 'S', 'C'),
    /*!
       This extended buffer configures HDR SEI message. See the mfxExtContentLightLevelInfo structure for details.
    */
    MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO        = MFX_MAKEFOURCC('L', 'L', 'I', 'S'),
    /*!
       This extended buffer configures HDR SEI message. See the mfxExtMasteringDisplayColourVolume structure for details. If color volume changes
       per frame, the application can attach this buffer to the mfxFrameData structure for video processing.
    */
    MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME = MFX_MAKEFOURCC('D', 'C', 'V', 'S'),
    /*!
       This extended buffer configures HDR SEI message. See the mfxExtMasteringDisplayColourVolume structure for details. The application can
       attach this buffer to the mfxVideoParam structure for the input of video processing if the mastering display color volume changes per
       sequence. In this case, this buffer should be together with MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO to indicate the light level and mastering
       color volume of the input of video processing. If color Volume changes per frame instead of per sequence, the application can attach
       MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME to mfxFrameData for frame based processing.
    */
    MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME_IN         = MFX_MAKEFOURCC('D', 'C', 'V', 'I'),
    /*!
       This extended buffer configures HDR SEI message. See the mfxExtMasteringDisplayColourVolume structure for details. The application can
       attach this buffer to the mfxVideoParam structure for the output of video processing if the mastering display color volume changes per
       sequence. If color volume changes per frame instead of per sequence, the application can attach the buffer with MFX_EXTBUFF_MASTERING_
       DISPLAY_COLOUR_VOLUME to mfxFrameData for frame based processing.
    */
    MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME_OUT        = MFX_MAKEFOURCC('D', 'C', 'V', 'O'),
    /*!
       See the mfxExtEncodedUnitsInfo structure for details.
    */
    MFX_EXTBUFF_ENCODED_UNITS_INFO              = MFX_MAKEFOURCC('E', 'N', 'U', 'I'),
    /*!
       This video processing algorithm identifier is used to enable MCTF via mfxExtVPPDoUse and together with mfxExtVppMctf
    */
    MFX_EXTBUFF_VPP_MCTF                        = MFX_MAKEFOURCC('M', 'C', 'T', 'F'),
    /*!
       Extends mfxVideoParam structure with VP9 segmentation parameters. See the mfxExtVP9Segmentation structure for details.
    */
    MFX_EXTBUFF_VP9_SEGMENTATION                = MFX_MAKEFOURCC('9', 'S', 'E', 'G'),
    /*!
       Extends mfxVideoParam structure with parameters for VP9 temporal scalability. See the mfxExtVP9TemporalLayers structure for details.
    */
    MFX_EXTBUFF_VP9_TEMPORAL_LAYERS             = MFX_MAKEFOURCC('9', 'T', 'M', 'L'),
    /*!
       Extends mfxVideoParam structure with VP9-specific parameters. See the mfxExtVP9Param structure for details.
    */
    MFX_EXTBUFF_VP9_PARAM                       = MFX_MAKEFOURCC('9', 'P', 'A', 'R'),
    /*!
       See the mfxExtAVCRoundingOffset structure for details.
    */
    MFX_EXTBUFF_AVC_ROUNDING_OFFSET             = MFX_MAKEFOURCC('R','N','D','O'),
    /*!
       See the mfxExtPartialBitstreamParam structure for details.
    */
    MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM         = MFX_MAKEFOURCC('P','B','O','P'),

    /*!
       See the mfxExtEncoderIPCMArea structure for details.
    */
    MFX_EXTBUFF_ENCODER_IPCM_AREA  = MFX_MAKEFOURCC('P', 'C', 'M', 'R'),
    /*!
       See the mfxExtInsertHeaders structure for details.
    */
    MFX_EXTBUFF_INSERT_HEADERS  = MFX_MAKEFOURCC('S', 'P', 'R', 'E'),

    /*!
       See the mfxExtDeviceAffinityMask structure for details.
    */
    MFX_EXTBUFF_DEVICE_AFFINITY_MASK = MFX_MAKEFOURCC('D', 'A', 'F', 'M'),

    /*!
       See the mfxExtInCrops structure for details.
    */
    MFX_EXTBUFF_CROPS = MFX_MAKEFOURCC('C', 'R', 'O', 'P'),

    /*!
        See the mfxExtAV1BitstreamParam structure for more details.
    */
    MFX_EXTBUFF_AV1_BITSTREAM_PARAM             = MFX_MAKEFOURCC('A', '1', 'B', 'S'),

    /*!
        See the mfxExtAV1ResolutionParam structure for more details.
    */
    MFX_EXTBUFF_AV1_RESOLUTION_PARAM            = MFX_MAKEFOURCC('A', '1', 'R', 'S'),

    /*!
        See the mfxExtAV1TileParam structure for more details.
    */
    MFX_EXTBUFF_AV1_TILE_PARAM                  = MFX_MAKEFOURCC('A', '1', 'T', 'L'),

    /*!
        See the mfxExtAV1Segmentation structure for more details.
    */
    MFX_EXTBUFF_AV1_SEGMENTATION                = MFX_MAKEFOURCC('1', 'S', 'E', 'G'),

    /*!
       See the mfxExtAV1FilmGrainParam structure for more details.
    */
    MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM = MFX_MAKEFOURCC('A','1','F','G'),

    /*!
       See the mfxExtHyperModeParam structure for more details.
    */
    MFX_EXTBUFF_HYPER_MODE_PARAM = MFX_MAKEFOURCC('H', 'Y', 'P', 'M'),
    /*!
       See the mfxExtTemporalLayers structure for more details.
    */
    MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS = MFX_MAKEFOURCC('U', 'T', 'M', 'P'),
    /*!
       This extended buffer defines additional encoding controls for reference list. See the mfxExtRefListCtrl structure for details.
       The application can attach this buffer to the mfxVideoParam structure for encoding & decoding initialization, or
       the mfxEncodeCtrl structure for per-frame encoding configuration.
    */
    MFX_EXTBUFF_UNIVERSAL_REFLIST_CTRL = MFX_EXTBUFF_AVC_REFLIST_CTRL,
#ifdef ONEVPL_EXPERIMENTAL
    /*!
       See the mfxExtEncodeStats structure for details.
    */
    MFX_EXTBUFF_ENCODESTATS                   = MFX_MAKEFOURCC('E','N','S','B'),
#endif
    /*!
       See the mfxExtVPP3DLut structure for more details.
    */
    MFX_EXTBUFF_VPP_3DLUT = MFX_MAKEFOURCC('T','D','L','T'),

    /*!
       See the mfxExtAllocationHints structure for more details.
    */
    MFX_EXTBUFF_ALLOCATION_HINTS = MFX_MAKEFOURCC('A','L','C','H'),

#ifdef ONEVPL_EXPERIMENTAL
    /*!
       See the mfxExtVPPPercEncPrefilter structure for details.
    */
    MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER        = MFX_MAKEFOURCC('V','P','E','F'),
    /*!
       See the mfxExtTuneEncodeQuality structure for details.
    */
    MFX_EXTBUFF_TUNE_ENCODE_QUALITY           = MFX_MAKEFOURCC('T','U','N','E'),
    /*!
    See the mfxExtSurfaceOpenCLImg2DExportDescription structure for more details.
    */
    MFX_EXTBUFF_EXPORT_SHARING_DESC_OCL = MFX_MAKEFOURCC('E', 'O', 'C', 'L'),
    /*!
    See the mfxExtSurfaceD3D12Tex2DExportDescription structure for more details.
    */
    MFX_EXTBUFF_EXPORT_SHARING_DESC_D3D12 = MFX_MAKEFOURCC('E', 'D', '1', '2'),
    /*!
    See the mfxExtSurfaceVulkanImg2DExportDescription structure for more details.
    */
    MFX_EXTBUFF_EXPORT_SHARING_DESC_VULKAN = MFX_MAKEFOURCC('E', 'V', 'U', 'L'),
#endif
    /*!
       See the mfxExtVPPAISuperResolution structure for details.
    */
    MFX_EXTBUFF_VPP_AI_SUPER_RESOLUTION = MFX_MAKEFOURCC('V','A','S','R'),
    /*!
       See the mfxExtVPPAIFrameInterpolation structure for details.
    */
    MFX_EXTBUFF_VPP_AI_FRAME_INTERPOLATION = MFX_MAKEFOURCC('V', 'A', 'F', 'I'),
    /*!
      See the mfxExtQualityInfoMode structure for details.
   */
   MFX_EXTBUFF_ENCODED_QUALITY_INFO_MODE = MFX_MAKEFOURCC('E', 'N', 'Q', 'M'),
    /*!
      See the mfxExtQualityInfoOutput structure for details.
   */
   MFX_EXTBUFF_ENCODED_QUALITY_INFO_OUTPUT = MFX_MAKEFOURCC('E', 'N', 'Q', 'O'),
   /*!
      See the mfxExtAV1ScreenContentTools structure for details.
   */
   MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS = MFX_MAKEFOURCC('1', 'S', 'C', 'C'),
    /*!
        See the mfxExtAlphaChannelEncCtrl structure for more details.
    */
    MFX_EXTBUFF_ALPHA_CHANNEL_ENC_CTRL = MFX_MAKEFOURCC('A', 'C', 'E', 'C'),
    /*!
        See the mfxExtAlphaChannelSurface structure for more details.
    */
    MFX_EXTBUFF_ALPHA_CHANNEL_SURFACE = MFX_MAKEFOURCC('A', 'C', 'S', 'F'),
#ifdef ONEVPL_EXPERIMENTAL
    /*!
        See the mfxExtAIEncCtrl structure for more details.
    */
    MFX_EXTBUFF_AI_ENC_CTRL = MFX_MAKEFOURCC('A', 'I', 'E', 'C'),
#endif
};

/* VPP Conf: Do not use certain algorithms  */
MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Tells the VPP not to use certain filters in pipeline. See "Configurable VPP filters" table for complete
   list of configurable filters. The user can attach this structure to the mfxVideoParam structure when initializing video processing.
*/
typedef struct {
    mfxExtBuffer    Header;  /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DONOTUSE. */
    mfxU32          NumAlg;  /*!< Number of filters (algorithms) not to use */
    mfxU32*         AlgList; /*!< Pointer to a list of filters (algorithms) not to use */
} mfxExtVPPDoNotUse;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   A hint structure that configures the VPP denoise filter algorithm.
   @deprecated Deprecated in API version 2.5. Use mfxExtVPPDenoise2 instead.
*/
MFX_DEPRECATED typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DENOISE. */
    mfxU16  DenoiseFactor;  /*!< Indicates the level of noise to remove. Value range of 0 to 100 (inclusive).  */
} mfxExtVPPDenoise;
MFX_PACK_END()

/*! The mfxDenoiseMode enumerator specifies the mode of denoise. */
typedef enum {
    MFX_DENOISE_MODE_DEFAULT    = 0,     /*!< Default denoise mode. The library selects the most appropriate denoise mode. */
    MFX_DENOISE_MODE_VENDOR     = 1000,  /*!< The enumeration to separate common denoise mode above and vendor specific. */

    MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE     = MFX_DENOISE_MODE_VENDOR + 1,  /*!< Indicates auto BD rate improvement in pre-processing before video encoding,
                                                                                    ignore Strength.*/
    MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE = MFX_DENOISE_MODE_VENDOR + 2,  /*!< Indicates auto subjective quality improvement in pre-processing before video encoding,
                                                                                    ignore Strength.*/
    MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST     = MFX_DENOISE_MODE_VENDOR + 3,  /*!< Indicates auto adjust subjective quality in post-processing (after decoding) for video playback,
                                                                                    ignore Strength.*/
    MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL      = MFX_DENOISE_MODE_VENDOR + 4,  /*!< Indicates manual mode for pre-processing before video encoding,
                                                                                    allow to adjust the denoise strength manually.*/
    MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL     = MFX_DENOISE_MODE_VENDOR + 5,  /*!< Indicates manual mode for post-processing for video playback,
                                                                                    allow to adjust the denoise strength manually.*/
} mfxDenoiseMode;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   A hint structure that configures the VPP denoise filter algorithm.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DENOISE2. */
    mfxDenoiseMode  Mode;   /*!< Indicates the mode of denoise. mfxDenoiseMode enumerator.  */
    mfxU16  Strength;       /*!< Denoise strength in manual mode. Value of 0-100 (inclusive) indicates the strength of denoise.
                                 The strength of denoise controls degree of possible changes of pixel values; the bigger the strength
                                 the larger the change is.  */
    mfxU16  reserved[15];
} mfxExtVPPDenoise2;
MFX_PACK_END()

/*! The mfx3DLutChannelMapping enumerator specifies the channel mapping of 3DLUT. */
typedef enum {
    MFX_3DLUT_CHANNEL_MAPPING_DEFAULT            = 0,   /*!< Default 3DLUT channel mapping. The library selects the most appropriate 3DLUT channel mapping. */
    MFX_3DLUT_CHANNEL_MAPPING_RGB_RGB            = 1,   /*!< 3DLUT RGB channels map to RGB channels. */
    MFX_3DLUT_CHANNEL_MAPPING_YUV_RGB            = 2,   /*!< 3DLUT YUV channels map to RGB channels. */
    MFX_3DLUT_CHANNEL_MAPPING_VUY_RGB            = 3,   /*!< 3DLUT VUY channels map to RGB channels. */
} mfx3DLutChannelMapping;

/*! The mfx3DLutMemoryLayout enumerator specifies the memory layout of 3DLUT. */
typedef enum {
    MFX_3DLUT_MEMORY_LAYOUT_DEFAULT                        = 0,          /*!< Default 3DLUT memory layout. The library selects the most appropriate 3DLUT memory layout.*/

    MFX_3DLUT_MEMORY_LAYOUT_VENDOR                         = 0x1000,     /*!< The enumeration to separate default above and vendor specific.*/
    /*!
       Intel specific memory layout. The enumerator indicates the attributes and memory layout of 3DLUT.
       3DLUT size is 17(the number of elements per dimension), 4 channels(3 valid channels, 1 channel is reserved), every channel must be 16-bit unsigned integer.
       3DLUT contains 17x17x32 entries with holes that are not filled. Take RGB as example, the nodes RxGx17 to RxGx31 are not filled, are "don't care" bits, and not accessed for the 17x17x17 nodes.
    */
    MFX_3DLUT_MEMORY_LAYOUT_INTEL_17LUT                    = MFX_3DLUT_MEMORY_LAYOUT_VENDOR + 1,
    /*!
       Intel specific memory layout. The enumerator indicates the attributes and memory layout of 3DLUT.
       3DLUT size is 33(the number of elements per dimension), 4 channels(3 valid channels, 1 channel is reserved), every channel must be 16-bit unsigned integer.
       3DLUT contains 33x33x64 entries with holes that are not filled. Take RGB as example, the nodes RxGx33 to RxGx63 are not filled, are "don't care" bits, and not accessed for the 33x33x33 nodes.
    */
    MFX_3DLUT_MEMORY_LAYOUT_INTEL_33LUT                    = MFX_3DLUT_MEMORY_LAYOUT_VENDOR + 2,
    /*!
       Intel specific memory layout. The enumerator indicates the attributes and memory layout of 3DLUT.
       3DLUT size is 65(the number of elements per dimension), 4 channels(3 valid channels, 1 channel is reserved), every channel must be 16-bit unsigned integer.
       3DLUT contains 65x65x128 entries with holes that are not filled. Take RGB as example, the nodes RxGx65 to RxGx127 are not filled, are "don't care" bits, and not accessed for the 65x65x65 nodes.
    */
    MFX_3DLUT_MEMORY_LAYOUT_INTEL_65LUT                    = MFX_3DLUT_MEMORY_LAYOUT_VENDOR + 3,
} mfx3DLutMemoryLayout;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
    A hint structure that configures the data channel.
*/
typedef struct {
    mfxDataType  DataType;                /*!< Data type, mfxDataType enumerator.*/
    mfxU32       Size;                    /*!< Size of Look up table, the number of elements per dimension.*/
    union
    {
        mfxU8*     Data;                  /*!< The pointer to 3DLUT data, 8 bit unsigned integer.*/
        mfxU16*    Data16;                /*!< The pointer to 3DLUT data, 16 bit unsigned integer.*/
    };
    mfxU32       reserved[4];             /*!< Reserved for future extension.*/
} mfxChannel;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures 3DLUT system buffer.
*/
typedef struct {
    mfxChannel           Channel[3];        /*!< 3 Channels, can be RGB or YUV, mfxChannel structure.*/
    mfxU32               reserved[8];       /*!< Reserved for future extension.*/
} mfx3DLutSystemBuffer;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures 3DLUT video buffer.
*/
typedef struct {
    mfxDataType                DataType;       /*!< Data type, mfxDataType enumerator.*/
    mfx3DLutMemoryLayout       MemLayout;      /*!< Indicates 3DLUT memory layout. mfx3DLutMemoryLayout enumerator.*/
    mfxMemId                   MemId;          /*!< Memory ID for holding the lookup table data. One MemID is dedicated for one instance of VPP.*/
    mfxU32                     reserved[8];    /*!< Reserved for future extension.*/
} mfx3DLutVideoBuffer;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL
/*! The mfx3DLutInterpolationMethod enumerator specifies the 3DLUT interpolation method. */
typedef enum {
    MFX_3DLUT_INTERPOLATION_DEFAULT              = 0,   /*!< Default 3DLUT interpolation Method. The library selects the most appropriate 3DLUT interpolation method. */
    MFX_3DLUT_INTERPOLATION_TRILINEAR            = 1,   /*!< 3DLUT Trilinear interpolation method. */
    MFX_3DLUT_INTERPOLATION_TETRAHEDRAL          = 2,   /*!< 3DLUT Tetrahedral interpolation method. */
} mfx3DLutInterpolationMethod;
#endif

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    A hint structure that configures 3DLUT filter.
*/
typedef struct {
    mfxExtBuffer             Header;           /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_3DLUT.*/
    mfx3DLutChannelMapping   ChannelMapping;   /*!< Indicates 3DLUT channel mapping. mfx3DLutChannelMapping enumerator.*/
    mfxResourceType          BufferType;       /*!< Indicates 3DLUT buffer type. mfxResourceType enumerator, can be system memory, VA surface, DX11 texture/buffer etc.*/
    union
    {
        mfx3DLutSystemBuffer SystemBuffer;     /*!< The 3DLUT system buffer. mfx3DLutSystemBuffer structure describes the details of the buffer.*/
        mfx3DLutVideoBuffer  VideoBuffer;      /*!< The 3DLUT video buffer. mfx3DLutVideoBuffer describes the details of 3DLUT video buffer.*/
    };
#ifdef ONEVPL_EXPERIMENTAL
    mfx3DLutInterpolationMethod     InterpolationMethod;       /*!< Indicates 3DLUT Interpolation Method. mfx3DLutInterpolationMethod enumerator.*/
    mfxU32                          reserved[3];               /*!< Reserved for future extension.*/
#else
    mfxU32                   reserved[4];      /*!< Reserved for future extension.*/
#endif
} mfxExtVPP3DLut;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   A hint structure that configures the VPP detail/edge enhancement filter algorithm.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DETAIL. */
    mfxU16  DetailFactor;   /*!< Indicates the level of details to be enhanced. Value range of 0 to 100 (inclusive).  */
} mfxExtVPPDetail;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   A hint structure that configures the VPP ProcAmp filter algorithm.
   The structure parameters will be clipped to their corresponding range and rounded by their corresponding increment.
   @note There are no default values for fields in this structure, all settings must be explicitly specified every time this
         buffer is submitted for processing.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_PROCAMP. */
    mfxF64   Brightness;    /*!< The brightness parameter is in the range of -100.0F to 100.0F, in increments of 0.1F.
                                 Setting this field to 0.0F will disable brightness adjustment. */
    mfxF64   Contrast;      /*!< The contrast parameter in the range of 0.0F to 10.0F, in increments of 0.01F, is used for manual
                                 contrast adjustment. Setting this field to 1.0F will disable contrast adjustment. If the parameter
                                 is negative, contrast will be adjusted automatically. */
    mfxF64   Hue;           /*!< The hue parameter is in the range of -180F to 180F, in increments of 0.1F. Setting this field to 0.0F
                                 will disable hue adjustment. */
    mfxF64   Saturation;    /*!< The saturation parameter is in the range of 0.0F to 10.0F, in increments of 0.01F.
                                 Setting this field to 1.0F will disable saturation adjustment. */
} mfxExtVPPProcAmp;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Returns statistics collected during encoding.
*/
typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;       /*!< Number of encoded frames. */
    mfxU64  NumBit;         /*!< Number of bits for all encoded frames. */
    mfxU32  NumCachedFrame; /*!< Number of internally cached frames. */
} mfxEncodeStat;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Returns statistics collected during decoding.
*/
typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;        /*!< Number of total decoded frames. */
    mfxU32  NumSkippedFrame; /*!< Number of skipped frames. */
    mfxU32  NumError;        /*!< Number of errors recovered. */
    mfxU32  NumCachedFrame;  /*!< Number of internally cached frames. */
} mfxDecodeStat;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Returns statistics collected during video processing.
*/
typedef struct {
    mfxU32  reserved[16];
    mfxU32  NumFrame;       /*!< Total number of frames processed. */
    mfxU32  NumCachedFrame; /*!< Number of internally cached frames. */
} mfxVPPStat;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Returns auxiliary data generated by the video processing pipeline.
   The encoding process may use the auxiliary data by attaching this structure to the mfxEncodeCtrl structure.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_AUXDATA. */

    union{
        struct{
            MFX_DEPRECATED mfxU32  SpatialComplexity; /* Deprecated */
            MFX_DEPRECATED mfxU32  TemporalComplexity; /* Deprecated */
        };
        struct{
            /*!
               Detected picture structure - top field first, bottom field first, progressive or unknown if video processor cannot
               detect picture structure. See the PicStruct enumerator for definition of these values.

            */
            mfxU16  PicStruct;
            mfxU16  reserved[3];
        };
    };
    MFX_DEPRECATED  mfxU16          SceneChangeRate; /* Deprecated */
    mfxU16          RepeatedFrame;   /*!< The flag signalizes that the frame is identical to the previous one. */
} mfxExtVppAuxData;
MFX_PACK_END()

/*! The PayloadCtrlFlags enumerator itemizes additional payload properties. */
enum {
    MFX_PAYLOAD_CTRL_SUFFIX = 0x00000001 /*!< Insert this payload into HEVC Suffix SEI NAL-unit. */
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Describes user data payload in MPEG-2 or SEI message payload in H.264.

   For encoding, these payloads can be
   inserted into the bitstream. The payload buffer must contain a valid formatted payload.

   For H.264, this is the sei_message() as
   specified in the section 7.3.2.3.1 'Supplemental enhancement information message syntax' of the ISO/IEC 14496-10 specification.

   For MPEG-2,
   this is the section 6.2.2.2.2 'User data' of the ISO/IEC 13818-2 specification, excluding the user data start_code.

   For decoding,
   these payloads can be retrieved as the decoder parses the bitstream and caches them in an internal buffer.

   @internal
   +-----------+-------------------------------------------+
   | **Codec** | **Supported Types**                       |
   +===========+===========================================+
   | MPEG2     | 0x01B2 //User Data                        |
   +-----------+-------------------------------------------+
   | AVC       | 02 //pan_scan_rect                        |
   |           | 03 //filler_payload                       |
   |           | 04 //user_data_registered_itu_t_t35       |
   |           | 05 //user_data_unregistered               |
   |           | 06 //recovery_point                       |
   |           | 09 //scene_info                           |
   |           | 13 //full_frame_freeze                    |
   |           | 14 //full_frame_freeze_release            |
   |           | 15 //full_frame_snapshot                  |
   |           | 16 //progressive_refinement_segment_start |
   |           | 17 //progressive_refinement_segment_end   |
   |           | 19 //film_grain_characteristics           |
   |           | 20 //deblocking_filter_display_preference |
   |           | 21 //stereo_video_info                    |
   |           | 45 //frame_packing_arrangement            |
   +-----------+-------------------------------------------+
   | HEVC      | All                                       |
   +-----------+-------------------------------------------+
   @endinternal

*/
typedef struct {
    mfxU32      CtrlFlags;  /*!< Additional payload properties. See the PayloadCtrlFlags enumerator for details. */
    mfxU32      reserved[3];
    mfxU8       *Data;      /*!< Pointer to the actual payload data buffer. */
    mfxU32      NumBit;     /*!< Number of bits in the payload data */
    mfxU16      Type;       /*!< MPEG-2 user data start code or H.264 SEI message type. */
    mfxU16      BufSize;    /*!< Payload buffer size in bytes. */
} mfxPayload;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Contains parameters for per-frame based encoding control.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< This extension buffer doesn't have assigned buffer ID. Ignored. */
    mfxU32  reserved[4];
    mfxU16  reserved1;
    /*!
       Type of NAL unit that contains encoding frame. All supported values are defined by MfxNalUnitType enumerator. Other values
       defined in ITU-T H.265 specification are not supported.

       The encoder uses this field only if application sets mfxExtCodingOption3::EnableNalUnitType option to ON during encoder initialization.

       @note Only encoded order is supported. If application specifies this value in display order or uses value inappropriate for current frame or
             invalid value, then the encoder silently ignores it.
    */
    mfxU16  MfxNalUnitType;
    mfxU16  SkipFrame; /*!< Indicates that current frame should be skipped or the number of missed frames before the current frame. See mfxExtCodingOption2::SkipFrame for details. */

    mfxU16  QP;        /*!< If nonzero, this value overwrites the global QP value for the current frame in the constant QP mode. */

    /*!
       Encoding frame type. See the FrameType enumerator for details. If the encoder works in the encoded order, the application must
       specify the frame type. If the encoder works in the display order, only key frames are enforceable.
    */
    mfxU16  FrameType;
    mfxU16  NumExtParam; /*!< Number of extra control buffers. */
    mfxU16  NumPayload;  /*!< Number of payload records to insert into the bitstream. */
    mfxU16  reserved2;

    /*!
       Pointer to an array of pointers to external buffers that provide additional information or control to the encoder for this
       frame or field pair. A typical use is to pass the VPP auxiliary data generated by the video processing pipeline to the encoder.
       See the ExtendedBufferID for the list of extended buffers.
    */
    mfxExtBuffer    **ExtParam;
    /*!
       Pointer to an array of pointers to user data (MPEG-2) or SEI messages (H.264) for insertion into the bitstream. For field pictures,
       odd payloads are associated with the first field and even payloads are associated with the second field. See the mfxPayload structure
       for payload definitions.
    */
    mfxPayload      **Payload;
} mfxEncodeCtrl;
MFX_PACK_END()

/*! The ExtMemBufferType enumerator specifies the buffer type. It is a bit-ORed value of the following. */
enum {
    MFX_MEMTYPE_PERSISTENT_MEMORY   =0x0002 /*!< Memory page for persistent use. */
};

/* Frame Memory Types */
#define MFX_MEMTYPE_BASE(x) (0x90ff & (x))

/*!
   The ExtMemFrameType enumerator specifies the memory type of frame. It is a bit-ORed value of the following.
    \verbatim embed:rst
    For information on working with video memory surfaces, see the :ref:`Working with Hardware Acceleration section<hw-acceleration>`.
    \endverbatim
*/
enum {
    MFX_MEMTYPE_DXVA2_DECODER_TARGET       =0x0010, /*!< Frames are in video memory and belong to video decoder render targets. */
    MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET     =0x0020, /*!< Frames are in video memory and belong to video processor render targets. */
    MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET   = MFX_MEMTYPE_DXVA2_DECODER_TARGET,  /*!< Frames are in video memory and belong to video decoder render targets. */
    MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET,/*!< Frames are in video memory and belong to video processor render targets. */
    MFX_MEMTYPE_SYSTEM_MEMORY              =0x0040, /*!< The frames are in system memory. */
    MFX_MEMTYPE_RESERVED1                  =0x0080, /*!<  */

    MFX_MEMTYPE_FROM_ENCODE     = 0x0100, /*!< Allocation request comes from an ENCODE function */
    MFX_MEMTYPE_FROM_DECODE     = 0x0200, /*!< Allocation request comes from a DECODE function */
    MFX_MEMTYPE_FROM_VPPIN      = 0x0400, /*!< Allocation request comes from a VPP function for input frame allocation */
    MFX_MEMTYPE_FROM_VPPOUT     = 0x0800, /*!< Allocation request comes from a VPP function for output frame allocation */
    MFX_MEMTYPE_FROM_ENC        = 0x2000, /*!< Allocation request comes from an ENC function */
    MFX_MEMTYPE_FROM_PAK        = 0x4000, /* Reserved */

    MFX_MEMTYPE_INTERNAL_FRAME  = 0x0001, /*!< Allocation request for internal frames */
    MFX_MEMTYPE_EXTERNAL_FRAME  = 0x0002, /*!< Allocation request for I/O frames */
    MFX_MEMTYPE_EXPORT_FRAME    = 0x0008, /*!< Application requests frame handle export to some associated object. For Linux frame handle can be
                                               considered to be exported to DRM Prime FD, DRM FLink or DRM FrameBuffer Handle. Specifics of export
                                               types and export procedure depends on external frame allocator implementation */
    MFX_MEMTYPE_SHARED_RESOURCE = MFX_MEMTYPE_EXPORT_FRAME, /*!< For DX11 allocation use shared resource bind flag. */
    MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET = 0x1000, /*!< Frames are in video memory and belong to video encoder render targets. */

    MFX_MEMTYPE_VIDEO_MEMORY_UNORDERED_ACCESS = 0x8000 /*!< Frames are in video memory and used as an unordered access resource. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Describes multiple frame allocations when initializing encoders, decoders, and video preprocessors.
   A range specifies the number of video frames. Applications are free to allocate additional frames. In all cases, the minimum number of
   frames must be at least NumFrameMin or the called API function will return an error.
*/
typedef struct {
    union {
        mfxU32  AllocId;       /*!< Unique (within the session) ID of component requested the allocation. */
        mfxU32  reserved[1];
    };
    mfxU32  reserved3[3];
    mfxFrameInfo    Info;      /*!< Describes the properties of allocated frames. */
    mfxU16  Type;              /*!< Allocated memory type. See the ExtMemFrameType enumerator for details. */
    mfxU16  NumFrameMin;       /*!< Minimum number of allocated frames. */
    mfxU16  NumFrameSuggested; /*!< Suggested number of allocated frames. */
    mfxU16  reserved2;
} mfxFrameAllocRequest;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Describes the response to multiple frame allocations. The calling API function returns the number of
   video frames actually allocated and pointers to their memory IDs.
*/
typedef struct {
    mfxU32      AllocId;        /*!< Unique (within the session) ID of component requested the allocation. */
    mfxU32      reserved[3];
    mfxMemId    *mids;          /*!< Pointer to the array of the returned memory IDs. The application allocates or frees this array. */
    mfxU16      NumFrameActual; /*!< Number of frames actually allocated. */
    mfxU16      reserved2;
} mfxFrameAllocResponse;
MFX_PACK_END()

/*! The FrameType enumerator itemizes frame types. Use bit-ORed values to specify all that apply. */
enum {
    MFX_FRAMETYPE_UNKNOWN       =0x0000, /*!< Frame type is unspecified. */

    MFX_FRAMETYPE_I             =0x0001, /*!< This frame or the first field is encoded as an I-frame/field. */
    MFX_FRAMETYPE_P             =0x0002, /*!< This frame or the first field is encoded as an P-frame/field. */
    MFX_FRAMETYPE_B             =0x0004, /*!< This frame or the first field is encoded as an B-frame/field. */
    MFX_FRAMETYPE_S             =0x0008, /*!< This frame or the first field is either an SI- or SP-frame/field. */

    MFX_FRAMETYPE_REF           =0x0040, /*!< This frame or the first field is encoded as a reference. */
    MFX_FRAMETYPE_IDR           =0x0080, /*!< This frame or the first field is encoded as an IDR. */

    MFX_FRAMETYPE_xI            =0x0100, /*!< The second field is encoded as an I-field. */
    MFX_FRAMETYPE_xP            =0x0200, /*!< The second field is encoded as an P-field. */
    MFX_FRAMETYPE_xB            =0x0400, /*!< The second field is encoded as an S-field. */
    MFX_FRAMETYPE_xS            =0x0800, /*!< The second field is an SI- or SP-field. */

    MFX_FRAMETYPE_xREF          =0x4000, /*!< The second field is encoded as a reference. */
    MFX_FRAMETYPE_xIDR          =0x8000  /*!< The second field is encoded as an IDR. */
};

/*!
   The MfxNalUnitType enumerator specifies NAL unit types supported by the HEVC encoder.
*/
enum {
    MFX_HEVC_NALU_TYPE_UNKNOWN    =      0, /*!< The encoder will decide what NAL unit type to use. */
    MFX_HEVC_NALU_TYPE_TRAIL_N    = ( 0+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_TRAIL_R    = ( 1+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_RADL_N     = ( 6+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_RADL_R     = ( 7+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_RASL_N     = ( 8+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_RASL_R     = ( 9+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_IDR_W_RADL = (19+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_IDR_N_LP   = (20+1), /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
    MFX_HEVC_NALU_TYPE_CRA_NUT    = (21+1)  /*!< See Table 7-1 of the ITU-T H.265 specification for the definition of these type. */
};

/*! The mfxSkipMode enumerator describes the decoder skip-mode options. */
typedef enum {
    MFX_SKIPMODE_NOSKIP=0, /*! Do not skip any frames. */
    MFX_SKIPMODE_MORE=1,   /*! Skip more frames. */
    MFX_SKIPMODE_LESS=2    /*! Skip less frames. */
} mfxSkipMode;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Attach this structure as part of the extended buffers to configure the encoder during MFXVideoENCODE_Init. The sequence or picture
   parameters specified by this structure overwrite any parameters specified by the structure or any other attached extended buffers attached.

   For H.264, SPSBuffer and PPSBuffer must point to valid bitstreams that contain the sequence parameter set and picture parameter set,
   respectively.

   For MPEG-2, SPSBuffer must point to valid bitstreams that contain the sequence header followed by any sequence header extension. The PPSBuffer pointer is ignored.

   The encoder imports parameters from these buffers. If the encoder does not support the specified parameters,
   the encoder does not initialize and returns the status code MFX_ERR_INCOMPATIBLE_VIDEO_PARAM.

   Check with the MFXVideoENCODE_Query function for the support of this multiple segment encoding feature. If this feature is not supported,
   the query returns MFX_ERR_UNSUPPORTED.
*/
typedef struct {
    mfxExtBuffer    Header;     /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CODING_OPTION_SPSPPS. */
    mfxU8           *SPSBuffer; /*!< Pointer to a valid bitstream that contains the SPS (sequence parameter set for H.264 or sequence header
                                     followed by any sequence header extension for MPEG-2) buffer. Can be NULL to skip specifying the SPS. */
    mfxU8           *PPSBuffer; /*!< Pointer to a valid bitstream that contains the PPS (picture parameter set for H.264 or picture header
                                     followed by any picture header extension for MPEG-2) buffer. Can be NULL to skip specifying the PPS. */
    mfxU16          SPSBufSize; /*!< Size of the SPS in bytes. */
    mfxU16          PPSBufSize; /*!< Size of the PPS in bytes. */
    mfxU16          SPSId;      /*!< SPS identifier. The value is reserved and must be zero. */
    mfxU16          PPSId;      /*!< PPS identifier. The value is reserved and must be zero. */
} mfxExtCodingOptionSPSPPS;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Attach this structure as part of the extended buffers to configure the encoder during MFXVideoENCODE_Init. The sequence or picture
   parameters specified by this structure overwrite any parameters specified by the structure or any other attached extended buffers attached.

   If the encoder does not support the specified parameters, the encoder does not initialize and returns the status code
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM.

   Check with the MFXVideoENCODE_Query function for the support of this multiple segment encoding feature. If this feature is not supported,
   the query returns MFX_ERR_UNSUPPORTED.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CODING_OPTION_VPS. */

    union {
        mfxU8       *VPSBuffer; /*!< Pointer to a valid bitstream that contains the VPS (video parameter set for HEVC) buffer. */
        mfxU64      reserved1;
    };
    mfxU16          VPSBufSize; /*!< Size of the VPS in bytes. */
    mfxU16          VPSId;      /*!< VPS identifier; the value is reserved and must be zero. */

    mfxU16          reserved[6];
} mfxExtCodingOptionVPS;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Defines the video signal information.

   For H.264, see Annex E of the ISO/IEC 14496-10 specification for the definition of these parameters.

   For MPEG-2, see section 6.3.6 of the ITU* H.262 specification for the definition of these parameters. The field VideoFullRange is ignored.

   For VC-1, see section 6.1.14.5 of the SMPTE* 421M specification. The fields VideoFormat and VideoFullRange are ignored.

   @note If ColourDescriptionPresent is zero, the color description information (including ColourPrimaries, TransferCharacteristics,
         and MatrixCoefficients) does not present in the bitstream.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VIDEO_SIGNAL_INFO. */
    mfxU16          VideoFormat;
    mfxU16          VideoFullRange;
    mfxU16          ColourDescriptionPresent;
    mfxU16          ColourPrimaries;
    mfxU16          TransferCharacteristics;
    mfxU16          MatrixCoefficients;
} mfxExtVideoSignalInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Tells the VPP to include certain filters in the pipeline.

   Each filter may be included in the pipeline in one of two different ways:

   @li Adding a filter ID to this structure. In this method,
   the default filter parameters are used.

   @li Attaching a filter configuration structure directly to the mfxVideoParam structure.
   In this method, adding filter ID to the mfxExtVPPDoUse structure is optional.

   See Table "Configurable VPP filters" for complete list of
   configurable filters, their IDs, and configuration structures.

   The user can attach this structure to the mfxVideoParam structure when initializing video processing.

   @note MFX_EXTBUFF_VPP_COMPOSITE cannot be enabled using mfxExtVPPDoUse because default parameters are undefined for this filter.
         The application must attach the appropriate filter configuration structure directly to the mfxVideoParam structure to enable it.
*/
typedef struct {
    mfxExtBuffer    Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DOUSE. */
    mfxU32          NumAlg;   /*!< Number of filters (algorithms) to use */
    mfxU32          *AlgList; /*!< Pointer to a list of filters (algorithms) to use */
} mfxExtVPPDoUse;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures reference frame options for the H.264 encoder.
    \verbatim embed:rst
    See the :ref:`Reference List Selection <sec_reference_list_selection>` and :ref:`Long Term Reference Frame <sec_long_term_reference_frame>` sections for more details.
    \endverbatim


   @note Not all implementations of the encoder support LongTermIdx and ApplyLongTermIdx fields in this structure. The application must use
         query mode 1 to determine if such functionality is supported. To do this, the application must attach this extended buffer to the
         mfxVideoParam structure and call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE and these fields were set to non-zero value,
         then the functionality is supported. If the function fails or sets fields to zero, then the functionality is not supported.

*/
typedef struct {
    mfxExtBuffer    Header;            /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AVC_REFLIST_CTRL. */
    mfxU16          NumRefIdxL0Active; /*!< Specify the number of reference frames in the active reference list L0. This number should be less or equal to the NumRefFrame parameter from encoding initialization. */
    mfxU16          NumRefIdxL1Active; /*!< Specify the number of reference frames in the active reference list L1. This number should be less or equal to the NumRefFrame parameter from encoding initialization. */

    struct {
        /*! @{
        @name Reference Lists
        The following structure members are used by the reference lists contained in the parent structure. */
        mfxU32      FrameOrder;  /*!< Together FrameOrder and PicStruct fields are used to identify reference picture. Use FrameOrder = MFX_FRAMEORDER_UNKNOWN to mark unused entry. */
        mfxU16      PicStruct;   /*!< Together FrameOrder and PicStruct fields are used to identify reference picture. Use FrameOrder = MFX_FRAMEORDER_UNKNOWN to mark unused entry. */
        mfxU16      ViewId;      /*!< Reserved and must be zero. */
        mfxU16      LongTermIdx; /*!< Index that should be used by the encoder to mark long-term reference frame. */
        mfxU16      reserved[3]; /*!< Reserved */
        /*! @} */
    } PreferredRefList[32], /*!< Reference list that specifies the list of frames that should be used to predict the current frame. */
    RejectedRefList[16], /*!< Reference list that specifies the list of frames that should not be used for prediction. */
    LongTermRefList[16]; /*!< Reference list that specifies the list of frames that should be marked as long-term reference frame. */

    mfxU16      ApplyLongTermIdx;/*!< If it is equal to zero, the encoder assigns long-term index according to internal algorithm.
                                      If it is equal to one, the encoder uses LongTermIdx value as long-term index. */
    mfxU16      reserved[15];
} mfxExtAVCRefListCtrl;
MFX_PACK_END()

/*! The FrcAlgm enumerator itemizes frame rate conversion algorithms. See description of mfxExtVPPFrameRateConversion structure for more details. */
enum {
    MFX_FRCALGM_PRESERVE_TIMESTAMP         = 0x0001, /*!< Frame dropping/repetition based frame rate conversion algorithm with preserved original
                                                          time stamps. Any inserted frames will carry MFX_TIMESTAMP_UNKNOWN. */
    MFX_FRCALGM_DISTRIBUTED_TIMESTAMP      = 0x0002, /*!< Frame dropping/repetition based frame rate conversion algorithm with distributed time stamps.
                                                          The algorithm distributes output time stamps evenly according to the output frame rate. */
    MFX_FRCALGM_FRAME_INTERPOLATION        = 0x0004, /*!< Frame rate conversion algorithm based on frame interpolation. This flag may be combined with
                                                          MFX_FRCALGM_PRESERVE_TIMESTAMP or MFX_FRCALGM_DISTRIBUTED_TIMESTAMP flags. */
#ifdef ONEVPL_EXPERIMENTAL
    MFX_FRCALGM_AI_FRAME_INTERPOLATION     = 0x0008  /*!< Frame rate conversion algorithm based on AI powered frame interpolation. This flag may be combined with
                                                          MFX_FRCALGM_PRESERVE_TIMESTAMP or MFX_FRCALGM_DISTRIBUTED_TIMESTAMP flags. This flag can not be combined
                                                          with MFX_FRCALGM_FRAME_INTERPOLATION. If application sets this flag, the application needs to attach
                                                          MFX_EXTBUFF_VPP_AI_FRAME_INTERPOLATION for the details of frame interpolation to mfxVideoParam. Refer to
                                                          mfxExtVPPAIFrameInterpolation for more details.*/
#endif
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP frame rate conversion filter. The user can attach this structure to the
   mfxVideoParam structure when initializing, resetting, or querying capability of video processing.

   On some platforms the advanced frame rate conversion algorithm (the algorithm based on frame interpolation) is not supported. To query its support,
   the application should add the MFX_FRCALGM_FRAME_INTERPOLATION flag to the Algorithm value in the mfxExtVPPFrameRateConversion structure, attach it to the
   structure, and call the MFXVideoVPP_Query function. If the filter is supported, the function returns a MFX_ERR_NONE status and copies the content of the
   input structure to the output structure. If an advanced filter is not supported, then a simple filter will be used and the function returns
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, copies content of the input structure to the output structure, and corrects the Algorithm value.

   If advanced FRC algorithm is not supported, both MFXVideoVPP_Init and MFXVideoVPP_Reset functions return the MFX_WRN_INCOMPATIBLE_VIDEO_PARAM status.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION. */
    mfxU16      Algorithm;  /*!< See the FrcAlgm enumerator for a list of frame rate conversion algorithms. */
    mfxU16      reserved;
    mfxU32      reserved2[15];
} mfxExtVPPFrameRateConversion;
MFX_PACK_END()

/*! The ImageStabMode enumerator itemizes image stabilization modes. See description of mfxExtVPPImageStab structure for more details. */
enum {
    MFX_IMAGESTAB_MODE_UPSCALE = 0x0001, /*!< Upscale mode. */
    MFX_IMAGESTAB_MODE_BOXING  = 0x0002  /*!< Boxing mode. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   A hint structure that configures the VPP image stabilization filter.

   On some platforms this filter is not supported. To query its support, the application should use the same approach that it uses
   to configure VPP filters: adding the filter ID to the mfxExtVPPDoUse structure or by attaching the mfxExtVPPImageStab structure
   directly to the mfxVideoParam structure and calling the MFXVideoVPP_Query function.

   If this filter is supported, the function returns a MFX_ERR_NONE
   status and copies the content of the input structure to the output structure. If the filter is not supported, the function returns MFX_WRN_FILTER_SKIPPED, removes the
   filter from the mfxExtVPPDoUse structure, and zeroes the mfxExtVPPImageStab structure.

   If the image stabilization filter is not supported, both MFXVideoVPP_Init and MFXVideoVPP_Reset functions return a MFX_WRN_FILTER_SKIPPED status.

   The application can retrieve the list of active filters by attaching the mfxExtVPPDoUse structure to the mfxVideoParam structure and calling the
   MFXVideoVPP_GetVideoParam function. The application must allocate enough memory for the filter list.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_IMAGE_STABILIZATION. */
    mfxU16  Mode;           /*!< Image stabilization mode. See ImageStabMode enumerator for values. */
    mfxU16  reserved[11];
} mfxExtVPPImageStab;
MFX_PACK_END()


/*!
    The InsertHDRPayload enumerator itemizes HDR payloads insertion rules in the encoder,
    and indicates if there is valid HDR information in the clip in the decoder.
*/
enum {
    MFX_PAYLOAD_OFF = 0, /*!< Do not insert payload when encoding;
                              Clip does not have valid HDR information when decoding. */
    MFX_PAYLOAD_IDR = 1  /*!< Insert payload on IDR frames when encoding;
                              Clip has valid HDR information when decoding. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Handle the HDR information.

   During encoding: If the application attaches this structure to the mfxEncodeCtrl structure at runtime,
   the encoder inserts the HDR information for the current frame and ignores InsertPayloadToggle. If the application attaches this
   structure to the mfxVideoParam structure during initialization or reset, the encoder inserts the HDR information based on InsertPayloadToggle.

   During video processing: If the application attaches this structure for video processing, InsertPayloadToggle will be ignored.
   And DisplayPrimariesX[3], DisplayPrimariesY[3] specify the color primaries where 0,1,2 specifies Red, Green, Blue respectively.

   During decoding: If the application attaches this structure to the mfxFrameSurface1 structure at runtime
   which will seed to the MFXVideoDECODE_DecodeFrameAsync() as surface_work parameter,
   the decoder will parse the HDR information if the bitstream include HDR information per frame.
   The parsed HDR information will be attached to the ExtendBuffer of surface_out parameter of MFXVideoDECODE_DecodeFrameAsync()
   with flag `InsertPayloadToggle` to indicate if there is valid HDR information in the clip.
   `InsertPayloadToggle` will be set to `MFX_PAYLOAD_IDR` if oneAPI Video Processing Library (oneVPL) gets valid HDR information, otherwise it will be set
   to `MFX_PAYLOAD_OFF`.
   This function is support for HEVC and AV1 only now.

   Encoding or Decoding, Field semantics are defined in ITU-T* H.265 Annex D, AV1 6.7.4 Metadata OBU semantics.

   Video processing, `DisplayPrimariesX[3]` and `WhitePointX` are in increments of 0.00002, in the range of [5, 37000]. `DisplayPrimariesY[3]`
   and `WhitePointY` are in increments of 0.00002, in the range of [5, 42000]. `MaxDisplayMasteringLuminance` is in units of 1 candela per square meter.
   `MinDisplayMasteringLuminance` is in units of 0.0001 candela per square meter.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME. */
    mfxU16      reserved[15];

    mfxU16 InsertPayloadToggle;  /*!< InsertHDRPayload enumerator value. */
    mfxU16 DisplayPrimariesX[3]; /*!< Color primaries for a video source. Consist of RGB x coordinates and
                                       define how to convert colors from RGB color space to CIE XYZ color space. */
    mfxU16 DisplayPrimariesY[3]; /*!< Color primaries for a video source. Consists of RGB y coordinates and
                                       defines how to convert colors from RGB color space to CIE XYZ color space.*/
    mfxU16 WhitePointX;          /*!< White point X coordinate. */
    mfxU16 WhitePointY;          /*!< White point Y coordinate. */
    mfxU32 MaxDisplayMasteringLuminance; /*!< Specify maximum luminance of the display on which the content was authored.*/
    mfxU32 MinDisplayMasteringLuminance; /*!< Specify minimum luminance of the display on which the content was authored. */
} mfxExtMasteringDisplayColourVolume;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Handle the HDR information.

   During encoding: If the application attaches this structure to the mfxEncodeCtrl structure at runtime,
   the encoder inserts the HDR information for the current frame and ignores InsertPayloadToggle. If the application
   attaches this structure to the mfxVideoParam structure during initialization or reset, the encoder inserts
   the HDR information based on InsertPayloadToggle.

   During video processing: If the application attaches this structure for video processing, InsertPayloadToggle will be ignored.

   During decoding: If the application attaches this structure to the mfxFrameSurface1 structure at runtime
   which will seed to the MFXVideoDECODE_DecodeFrameAsync() as surface_work parameter,
   the decoder will parse the HDR information if the bitstream include HDR information per frame.
   The parsed HDR information will be attached to the ExtendBuffer of surface_out parameter of MFXVideoDECODE_DecodeFrameAsync()
   with flag `InsertPayloadToggle` to indicate if there is valid HDR information in the clip.
   `InsertPayloadToggle` will be set to `MFX_PAYLOAD_IDR` if oneVPL gets valid HDR information, otherwise it will be set to `MFX_PAYLOAD_OFF`.
   This function is support for HEVC and AV1 only now.

   Field semantics are defined in ITU-T* H.265 Annex D, AV1 6.7.3 Metadata high dynamic range content light level semantics.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to EXTBUFF_CONTENT_LIGHT_LEVEL_INFO. */
    mfxU16      reserved[9];

    mfxU16 InsertPayloadToggle;     /*!< InsertHDRPayload enumerator value. */
    mfxU16 MaxContentLightLevel;    /*!< Maximum luminance level of the content. Field range is 1 to 65535. */
    mfxU16 MaxPicAverageLightLevel; /*!< Maximum average per-frame luminance level of the content. Field range is 1 to 65535. */
} mfxExtContentLightLevelInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the H.264 picture timing SEI message. The encoder ignores it if HRD information in
   the stream is absent and the PicTimingSEI option in the mfxExtCodingOption structure is turned off. See mfxExtCodingOption for details.

   If the application attaches this structure to the mfxVideoParam structure during initialization, the encoder inserts the picture timing
   SEI message based on provided template in every access unit of coded bitstream.

   If application attaches this structure to the mfxEncodeCtrl structure at runtime, the encoder inserts the picture timing SEI message
   based on provided template in access unit that represents current frame.

   These parameters define the picture timing information. An invalid value of 0xFFFF indicates that application does not set the value and
   encoder must calculate it.

   See Annex D of the ISO*\/IEC* 14496-10 specification for the definition of these parameters.
*/
typedef struct {
  mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_PICTURE_TIMING_SEI. */
  mfxU32      reserved[14];

  struct {
      mfxU16    ClockTimestampFlag;
      mfxU16    CtType;
      mfxU16    NuitFieldBasedFlag;
      mfxU16    CountingType;
      mfxU16    FullTimestampFlag;
      mfxU16    DiscontinuityFlag;
      mfxU16    CntDroppedFlag;
      mfxU16    NFrames;
      mfxU16    SecondsFlag;
      mfxU16    MinutesFlag;
      mfxU16    HoursFlag;
      mfxU16    SecondsValue;
      mfxU16    MinutesValue;
      mfxU16    HoursValue;
      mfxU32    TimeOffset;
  } TimeStamp[3];
} mfxExtPictureTimingSEI;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the H.264 temporal layers hierarchy.

   If the application attaches it to the mfxVideoParam
   structure during initialization, the encoder generates the temporal layers and inserts the prefix NAL unit before each slice to
   indicate the temporal and priority IDs of the layer.

   This structure can be used with the display-order encoding mode only.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AVC_TEMPORAL_LAYERS. */
    mfxU32          reserved1[4];
    mfxU16          reserved2;
    mfxU16          BaseLayerPID; /*!< The priority ID of the base layer. The encoder increases the ID for each temporal layer and writes to the prefix NAL unit. */

    struct {
        mfxU16 Scale;       /*!< The ratio between the frame rates of the current temporal layer and the base layer. */
        mfxU16 reserved[3];
    }Layer[8];
} mfxExtAvcTemporalLayers;  /*!< The array of temporal layers; Use Scale=0 to specify absent layers. */
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to retrieve encoder capability. See the description of mode 4 of the MFXVideoENCODE_Query function
   for details on how to use this structure.

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine
         if the functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and
         call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE then the  functionality is supported.
*/
typedef struct {
    mfxExtBuffer Header;  /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODER_CAPABILITY. */

    mfxU32      MBPerSec; /*!< Specify the maximum processing rate in macro blocks per second. */
    mfxU16      reserved[58];
} mfxExtEncoderCapability;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to control the encoder behavior during reset. By using this structure, the application
   instructs the encoder to start a new coded sequence after reset or to continue encoding of the current sequence.

   This structure is also used in mode 3 of the MFXVideoENCODE_Query function to check for reset outcome before actual reset. The application
   should set StartNewSequence to the required behavior and call the query function. If the query fails (see status codes below), then reset is not
   possible in current encoder state. If the application sets StartNewSequence to MFX_CODINGOPTION_UNKNOWN, then the query function replaces the coding option with the
   actual reset type: MFX_CODINGOPTION_ON if the encoder will begin a new sequence after reset or MFX_CODINGOPTION_OFF if the encoder will continue the current sequence.

   Using this structure may cause one of the following status codes from the MFXVideoENCODE_Reset and MFXVideoENCODE_Queryfunctions:

   @li MFX_ERR_INVALID_VIDEO_PARAM If a reset is not possible. For example, the application sets StartNewSequence to off and requests resolution change.

   @li MFX_ERR_INCOMPATIBLE_VIDEO_PARAM If the application requests change that leads to memory allocation. For example, the application sets StartNewSequence to on and
                                        requests resolution change to greater than the initialization value.

   @li MFX_ERR_NONE If reset is possible.

   The following limited list of parameters can be changed without starting a new coded sequence:

   @li The bitrate parameters, TargetKbps and MaxKbps, in the mfxInfoMFX structure.

   @li The number of slices, NumSlice, in the mfxInfoMFX structure. Number of slices should be equal to or less than the number of slices during initialization.

   @li The number of temporal layers in the mfxExtAvcTemporalLayers structure. Reset should be called immediately before encoding of frame from base layer and
     number of reference frames should be large enough for the new temporal layers structure.

   @li The quantization parameters, QPI, QPP and QPB, in the mfxInfoMFX structure.

   The application should retrieve all cached frames before calling reset. When the Query API function
   checks for reset outcome, it expects that this requirement be satisfied. If it is not true and there are some cached frames inside the
 encoder, then the query result may differ from the reset result, because the encoder may insert an IDR frame to produce valid coded sequence.
   \verbatim embed:rst
   See the :ref:`Configuration Change <config-change>` section for more information.
   \endverbatim

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if the
         functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and call the
         MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE, then the functionality is supported.

   \verbatim embed:rst
   See the :ref:`Streaming and Video Conferencing Features <stream_vid_conf_features>` section for more information.
   \endverbatim

*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODER_RESET_OPTION. */

    /*!
       Instructs encoder to start new sequence after reset. Use one of the CodingOptionValue options:

       @li MFX_CODINGOPTION_ON The encoder completely reset internal state and begins new coded sequence after reset, including
                             insertion of IDR frame, sequence, and picture headers.

       @li MFX_CODINGOPTION_OFF The encoder continues encoding of current coded sequence after reset, without insertion of IDR frame.

       @li MFX_CODINGOPTION_UNKNOWN Depending on the current encoder state and changes in configuration parameters, the encoder may or may not
                                  start new coded sequence. This value is also used to query reset outcome.
    */
    mfxU16      StartNewSequence;
    mfxU16      reserved[11];
} mfxExtEncoderResetOption;
MFX_PACK_END()

/*! The LongTermIdx specifies long term index of picture control. */
enum {
    MFX_LONGTERM_IDX_NO_IDX = 0xFFFF /*!< Long term index of picture is undefined. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the encoder to report additional information about the encoded picture. The application can attach
   this buffer to the mfxBitstream structure before calling MFXVideoENCODE_EncodeFrameAsync function. For interlaced content the encoder
   requires two such structures. They correspond to fields in encoded order.

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if
         the functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and
         call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE then the functionality is supported.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODED_FRAME_INFO. */

    mfxU32          FrameOrder;        /*!< Frame order of encoded picture. */
    mfxU16          PicStruct;         /*!< Picture structure of encoded picture. */
    mfxU16          LongTermIdx;       /*!< Long term index of encoded picture if applicable. */
    mfxU32          MAD;               /*!< Mean Absolute Difference between original pixels of the frame and motion compensated (for inter macroblocks) or
                                            spatially predicted (for intra macroblocks) pixels. Only luma component, Y plane, is used in calculation. */
    mfxU16          BRCPanicMode;      /*!< Bitrate control was not able to allocate enough bits for this frame. Frame quality may be unacceptably low. */
    mfxU16          QP;                /*!< Luma QP. */
    mfxU32          SecondFieldOffset; /*!< Offset to second field. Second field starts at mfxBitstream::Data + mfxBitstream::DataOffset + mfxExtAVCEncodedFrameInfo::SecondFieldOffset. */
    mfxU16          reserved[2];

    struct {
        /*! @{
        @name Reference Lists
        The following structure members are used by the reference lists contained in the parent structure. */
        mfxU32      FrameOrder;        /*!< Frame order of reference picture. */
        mfxU16      PicStruct;         /*!< Picture structure of reference picture. */
        mfxU16      LongTermIdx;       /*!< Long term index of reference picture if applicable. */
        mfxU16      reserved[4];
        /*! @} */
    } UsedRefListL0[32], /*!< Reference list that has been used to encode picture. */
    UsedRefListL1[32]; /*!< Reference list that has been used to encode picture. */
} mfxExtAVCEncodedFrameInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to specify input stream details for composition of several input surfaces in the one output.
*/
typedef struct mfxVPPCompInputStream {
    mfxU32  DstX; /*!< X coordinate of location of input stream in output surface. */
    mfxU32  DstY; /*!< Y coordinate of location of input stream in output surface. */
    mfxU32  DstW; /*!< Width of of location of input stream in output surface.*/
    mfxU32  DstH; /*!< Height of of location of input stream in output surface.*/

    mfxU16  LumaKeyEnable; /*!< Non-zero value enables luma keying for the input stream. Luma keying is used to mark some of the areas
                                of the frame with specified luma values as transparent. It may, for example, be used for closed captioning. */
    mfxU16  LumaKeyMin;    /*!< Minimum value of luma key, inclusive. Pixels whose luma values fit in this range are rendered transparent. */
    mfxU16  LumaKeyMax;    /*!< Maximum value of luma key, inclusive. Pixels whose luma values fit in this range are rendered transparent. */

    mfxU16  GlobalAlphaEnable; /*!< Non-zero value enables global alpha blending for this input stream. */
    mfxU16  GlobalAlpha;       /*!< Alpha value for this stream. Should be in the range of 0 to 255, where 0 is transparent and 255 is opaque. */
    mfxU16  PixelAlphaEnable;  /*!< Non-zero value enables per pixel alpha blending for this input stream. The stream should have RGB color format. */

    mfxU16  TileId;        /*!< Specify the tile this video stream is assigned to. Should be in the range of 0 to NumTiles. Valid only if NumTiles > 0. */

    mfxU16  reserved2[17];
} mfxVPPCompInputStream;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Used to control composition of several input surfaces in one output. In this mode, the VPP skips
   any other filters. The VPP returns an error if any mandatory filter is specified and returns the filter skipped warning if an optional filter is specified. The only
   supported filters are deinterlacing and interlaced scaling. The only supported combinations of input and output color formats are:

   - RGB to RGB,

   - NV12 to NV12,

   - RGB and NV12 to NV12, for per the pixel alpha blending use case.

   The VPP returns MFX_ERR_MORE_DATA for additional input until an output is ready. When the output is ready, the VPP returns MFX_ERR_NONE.
   The application must process the output frame after synchronization.

   The composition process is controlled by:

   - mfxFrameInfo::CropXYWH in the input surface defines the location of the picture in the input frame.

   - InputStream[i].DstXYWH defines the location of the cropped input picture in the output frame.

   - mfxFrameInfo::CropXYWH in the output surface defines the actual part of the output frame. All pixels in the output frame outside this region will be filled by the specified color.

   If the application uses the composition process on video streams with different frame sizes, the application should provide maximum frame size in the
   mfxVideoParam structure during the initialization, reset, or query operations.

   If the application uses the composition process, the MFXVideoVPP_QueryIOSurf function returns the cumulative number of input surfaces, that is, the number
   required to process all input video streams. The function sets the frame size in the mfxFrameAllocRequest equal to the size provided by the
   application in the mfxVideoParam structure.

   The composition process supports all types of surfaces.

   All input surfaces should have the same type and color format, except for the per pixel alpha blending case, where it is allowable to mix NV12 and RGB
   surfaces.

   There are three different blending use cases:

   - <b>Luma keying.</b> All input surfaces should have the NV12 color format specified during VPP initialization. Part of each surface, including the
     first one, may be rendered transparent by using LumaKeyEnable, LumaKeyMin, and LumaKeyMax values.

   - <b>Global alpha blending.</b> All input surfaces should have the same color format, NV12 or RGB, specified during VPP initialization. Each input surface, including the first one, can be blended with underlying surfaces by using GlobalAlphaEnable and
     GlobalAlpha values.

   - <b>Per-pixel alpha blending.</b> It is allowed to mix NV12 and RGB input surfaces. Each RGB input surface, including the first one,
     can be blended with underlying surfaces by using PixelAlphaEnable value.

   It is not allowed to mix different blending use cases in the same function call.

   In the special case where the destination region of the output surface defined by output crops is fully covered with destination sub-regions of the
   surfaces, the fast compositing mode can be enabled. The main use case for this mode is a video-wall scenario with a fixed destination surface
   partition into sub-regions of potentially different size.

   In order to trigger this mode, the application must cluster input surfaces into tiles, defining at least one tile by setting the NumTiles
   field to be greater than 0, and assigning surfaces to the corresponding tiles by setting the TileId field to the value within the 0 to NumTiles range per
   input surface. Tiles should also satisfy the following additional constraints:

   - Each tile should not have more than 8 surfaces assigned to it.

   - Tile bounding boxes, as defined by the enclosing rectangles of a union of a surfaces assigned to this tile, should not intersect.

   Background color may be changed dynamically through Reset. There is no default value. YUV black is (0;128;128) or (16;128;128) depending
   on the sample range. The library uses a YUV or RGB triple depending on output color format.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_COMPOSITE. */

    /* background color*/
    union {
        mfxU16   Y; /*!< Y value of the background color. */
        mfxU16   R; /*!< R value of the background color. */
    };
    union {
        mfxU16   U; /*!< U value of the background color. */
        mfxU16   G; /*!< G value of the background color. */
    };
    union {
        mfxU16   V; /*!< V value of the background color. */
        mfxU16   B; /*!< B value of the background color. */
    };
    mfxU16       NumTiles;              /*!< Number of input surface clusters grouped together to enable fast compositing. May be changed dynamically
                                             at runtime through Reset. */
    mfxU16       reserved1[23];

    mfxU16       NumInputStream;        /*!< Number of input surfaces to compose one output. May be changed dynamically at runtime through Reset. Number of surfaces
                                             can be decreased or increased, but should not exceed the number specified during initialization. Query mode 2 should be used
                                             to find the maximum supported number. */
    mfxVPPCompInputStream *InputStream; /*!< An array of mfxVPPCompInputStream structures that describe composition of input video streams. It should consist of exactly NumInputStream elements. */
} mfxExtVPPComposite;
MFX_PACK_END()

/*! The TransferMatrix enumerator itemizes color transfer matrices. */
enum {
    MFX_TRANSFERMATRIX_UNKNOWN = 0, /*!< Transfer matrix is not specified */
    MFX_TRANSFERMATRIX_BT709   = 1, /*!< Transfer matrix from ITU-R BT.709 standard. */
    MFX_TRANSFERMATRIX_BT601   = 2  /*!< Transfer matrix from ITU-R BT.601 standard. */
};

/*! The NominalRange enumerator itemizes pixel's value nominal range. */
enum {
    MFX_NOMINALRANGE_UNKNOWN   = 0, /*!< Range is not defined. */
    MFX_NOMINALRANGE_0_255     = 1, /*!< Range is from  0 to 255. */
    MFX_NOMINALRANGE_16_235    = 2  /*!< Range is from 16 to 235. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to control transfer matrix and nominal range of YUV frames. The application
   should provide this during initialization. Supported for multiple conversions, for example YUV to YUV, YUV to RGB, and RGB to YUV.

   @note This structure is used by VPP only and is not compatible with mfxExtVideoSignalInfo.
*/
typedef struct {
    mfxExtBuffer    Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO. */
    mfxU16          reserved1[4];

    union {
        struct { // Init
            struct  {
                mfxU16  TransferMatrix; /*!< Transfer matrix. */
                mfxU16  NominalRange;   /*!< Nominal range. */
                mfxU16  reserved2[6];
            } In, Out;
        };
        struct { // Runtime<
            mfxU16  TransferMatrix; /*!< Transfer matrix. */
            mfxU16  NominalRange;   /*!< Nominal range. */
            mfxU16  reserved3[14];
        };
    };
} mfxExtVPPVideoSignalInfo;
MFX_PACK_END()

/*! The ROImode enumerator itemizes QP adjustment mode for ROIs. */
enum {
    MFX_ROI_MODE_PRIORITY =  0, /*!< Priority mode. */
    MFX_ROI_MODE_QP_DELTA =  1,  /*!< QP mode */
    MFX_ROI_MODE_QP_VALUE =  2 /*!< Absolute QP */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the application to specify different Region Of Interests during encoding. It may be used at
   initialization or at runtime.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODER_ROI. */

    mfxU16  NumROI;  /*!< Number of ROI descriptions in array. The Query API function mode 2 returns maximum supported value (set it to 256 and
                         query will update it to maximum supported value). */
    mfxU16  ROIMode; /*!< QP adjustment mode for ROIs. Defines if Priority or DeltaQP is used during encoding. */
    mfxU16  reserved1[10];

    struct  {
        /*! @{
        @name ROI location rectangle
        The ROI rectangle definition uses end-point exclusive notation. In other words, the pixel with (Right, Bottom)
        coordinates lies immediately outside of the ROI. Left, Top, Right, Bottom should be aligned by codec-specific block boundaries
        (should be dividable by 16 for AVC, or by 32 for HEVC). Every ROI with unaligned coordinates will be expanded by the library to minimal-area
        block-aligned ROI, enclosing the original one. For example (5, 5, 15, 31) ROI will be expanded to (0, 0, 16, 32) for AVC encoder,
        or to (0, 0, 32, 32) for HEVC.
         */
        mfxU32  Left;   /*!< Left ROI's coordinate. */
        mfxU32  Top;    /*!< Top ROI's coordinate. */
        mfxU32  Right;  /*!< Right ROI's coordinate. */
        mfxU32  Bottom; /*!< Bottom ROI's coordinate. */
        union {
            /*! Priority of ROI. Used if ROIMode = MFX_ROI_MODE_PRIORITY.This is an absolute value in the range of -3 to 3,
                                  which will be added to the MB QP. Priority is deprecated mode and is used only for backward compatibility.
                                  Bigger value produces better quality. */
            mfxI16  Priority;
            /*! Delta QP of ROI. Used if ROIMode = MFX_ROI_MODE_QP_DELTA. This is an absolute value in the range of -51 to 51,
                                  which will be added to the MB QP. Lesser value produces better quality. */
            mfxI16  DeltaQP;
        };
        mfxU16  reserved2[7];
        /*! @} */
    } ROI[256]; /*!< Array of ROIs. Different ROI may overlap each other. If macroblock belongs to several ROI,
                     Priority from ROI with lowest index is used. */
} mfxExtEncoderROI;
MFX_PACK_END()

/*! The DeinterlacingMode enumerator itemizes VPP deinterlacing modes. */
enum {
    MFX_DEINTERLACING_BOB                    =  1, /*!< BOB deinterlacing mode. */
    MFX_DEINTERLACING_ADVANCED               =  2, /*!< Advanced deinterlacing mode. */
    MFX_DEINTERLACING_AUTO_DOUBLE            =  3, /*!< Auto mode with deinterlacing double frame rate output. */
    MFX_DEINTERLACING_AUTO_SINGLE            =  4, /*!< Auto mode with deinterlacing single frame rate output. */
    MFX_DEINTERLACING_FULL_FR_OUT            =  5, /*!< Deinterlace only mode with full frame rate output. */
    MFX_DEINTERLACING_HALF_FR_OUT            =  6, /*!< Deinterlace only Mode with half frame rate output. */
    MFX_DEINTERLACING_24FPS_OUT              =  7, /*!< 24 fps fixed output mode. */
    MFX_DEINTERLACING_FIXED_TELECINE_PATTERN =  8, /*!< Fixed telecine pattern removal mode. */
    MFX_DEINTERLACING_30FPS_OUT              =  9, /*!< 30 fps fixed output mode. */
    MFX_DEINTERLACING_DETECT_INTERLACE       = 10, /*!< Only interlace detection. */
    MFX_DEINTERLACING_ADVANCED_NOREF         = 11, /*!< Advanced deinterlacing mode without using of reference frames. */
    MFX_DEINTERLACING_ADVANCED_SCD           = 12, /*!< Advanced deinterlacing mode with scene change detection. */
    MFX_DEINTERLACING_FIELD_WEAVING          = 13  /*!< Field weaving. */
};

/*! The TelecinePattern enumerator itemizes telecine patterns. */
enum {
    MFX_TELECINE_PATTERN_32           = 0, /*!< 3:2 telecine. */
    MFX_TELECINE_PATTERN_2332         = 1, /*!< 2:3:3:2 telecine. */
    MFX_TELECINE_PATTERN_FRAME_REPEAT = 2, /*!< One frame repeat telecine. */
    MFX_TELECINE_PATTERN_41           = 3, /*!< 4:1 telecine. */
    MFX_TELECINE_POSITION_PROVIDED    = 4  /*!< User must provide position inside a sequence of 5 frames where the artifacts start. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the application to specify different deinterlacing algorithms.
*/
typedef struct {
    mfxExtBuffer    Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_DEINTERLACING. */
    mfxU16  Mode;             /*!< Deinterlacing algorithm. See the DeinterlacingMode enumerator for details. */
    mfxU16  TelecinePattern;  /*!< Specifies telecine pattern when Mode = MFX_DEINTERLACING_FIXED_TELECINE_PATTERN. See the TelecinePattern enumerator for details.*/
    mfxU16  TelecineLocation; /*!< Specifies position inside a sequence of 5 frames where the artifacts start when TelecinePattern = MFX_TELECINE_POSITION_PROVIDED*/
    mfxU16  reserved[9];      /*!< Reserved for future use. */
} mfxExtVPPDeinterlacing;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Specifies reference lists for the encoder. It may be used together with the mfxExtAVCRefListCtrl
   structure to create customized reference lists. If both structures are used together, then the encoder takes reference lists from the
   mfxExtAVCRefLists structure and modifies them according to the mfxExtAVCRefListCtrl instructions. In case of interlaced coding,
   the first mfxExtAVCRefLists structure affects TOP field and the second - BOTTOM field.

   @note Not all implementations of the encoder support this structure. The application must use the Query API function to determine if it is supported.
*/
typedef struct {
    mfxExtBuffer    Header;            /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AVC_REFLISTS. */
    mfxU16          NumRefIdxL0Active; /*!< Specify the number of reference frames in the active reference list L0. This number should be less than or
                                            equal to the NumRefFrame parameter from encoding initialization. */
    mfxU16          NumRefIdxL1Active; /*!< Specify the number of reference frames in the active reference list L1. This number should be less than or
                                            equal to the NumRefFrame parameter from encoding initialization. */
    mfxU16          reserved[2];

    /*! Used by the reference lists contained in the parent structure. Together these fields are used to identify reference picture. */
    struct mfxRefPic{
        mfxU32      FrameOrder; /*!< Use FrameOrder = MFX_FRAMEORDER_UNKNOWN to mark
                                     unused entry. */
        mfxU16      PicStruct;  /*!< Use PicStruct = MFX_PICSTRUCT_FIELD_TFF for TOP field, PicStruct = MFX_PICSTRUCT_FIELD_BFF for
                                     BOTTOM field. */
        mfxU16      reserved[5];
    } RefPicList0[32], /*!< Specify L0 reference list. */
      RefPicList1[32]; /*!< Specify L1 reference list. */

}mfxExtAVCRefLists;
MFX_PACK_END()

/*! The VPPFieldProcessingMode enumerator is used to control VPP field processing algorithm. */
enum {
    MFX_VPP_COPY_FRAME      =0x01, /*!< Copy the whole frame. */
    MFX_VPP_COPY_FIELD      =0x02, /*!< Copy only one field. */
    MFX_VPP_SWAP_FIELDS     =0x03  /*!< Swap top and bottom fields. */
};

/*! The PicType enumerator itemizes picture type. */
enum {
    MFX_PICTYPE_UNKNOWN     =0x00, /*!< Picture type is unknown. */
    MFX_PICTYPE_FRAME       =0x01, /*!< Picture is a frame. */
    MFX_PICTYPE_TOPFIELD    =0x02, /*!< Picture is a top field. */
    MFX_PICTYPE_BOTTOMFIELD =0x04  /*!< Picture is a bottom field. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP field processing algorithm. The application can attach this extended buffer to
   the mfxVideoParam structure to configure initialization and/or to the mfxFrameData during runtime. Runtime configuration has priority
   over initialization configuration. If the field processing algorithm was activated via the mfxExtVPPDoUse structure and the mfxExtVPPFieldProcessing
   extended buffer was not provided during initialization, this buffer must be attached to the mfxFrameData structure of each input surface.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_FIELD_PROCESSING. */

    mfxU16          Mode;     /*!< Specifies the mode of the field processing algorithm. See the VPPFieldProcessingMode enumerator for values of this option. */
    mfxU16          InField;  /*!< When Mode is MFX_VPP_COPY_FIELD, specifies input field. See the PicType enumerator for values of this parameter. */
    mfxU16          OutField; /*!< When Mode is MFX_VPP_COPY_FIELD, specifies output field. See the PicType enumerator for values of this parameter. */
    mfxU16          reserved[25];
} mfxExtVPPFieldProcessing;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   If attached to the mfxVideoParam structure during the Init stage, this buffer will instruct the decoder to resize output frames via the
   fixed function resize engine (if supported by hardware), utilizing direct pipe connection and bypassing intermediate memory operations.
   The main benefits of this mode of pipeline operation are offloading resize operation to a dedicated engine, thus reducing power
   consumption and memory traffic.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_DEC_VIDEO_PROCESSING. */

    /*! Input surface description. */
    struct mfxIn{
        mfxU16  CropX; /*!< X coordinate of region of interest of the input surface. */
        mfxU16  CropY; /*!< Y coordinate of region of interest of the input surface. */
        mfxU16  CropW; /*!< Width coordinate of region of interest of the input surface. */
        mfxU16  CropH; /*!< Height coordinate of region of interest of the input surface. */
        mfxU16  reserved[12];
    }In; /*!< Input surface description. */

    /*! Output surface description. */
    struct mfxOut{
        mfxU32  FourCC;       /*!< FourCC of output surface Note: Should be MFX_FOURCC_NV12. */
        mfxU16  ChromaFormat; /*!< Chroma Format of output surface.
                                   @note Should be MFX_CHROMAFORMAT_YUV420 */
        mfxU16  reserved1;

        mfxU16  Width;        /*!< Width of output surface. */
        mfxU16  Height;       /*!< Height of output surface. */

        mfxU16  CropX; /*!< X coordinate of region of interest of the output surface. */
        mfxU16  CropY; /*!< Y coordinate of region of interest of the output surface. */
        mfxU16  CropW; /*!< Width coordinate of region of interest of the output surface. */
        mfxU16  CropH; /*!< Height coordinate of region of interest of the output surface. */
        mfxU16  reserved[22];
    }Out; /*!< Output surface description. */

    mfxU16  reserved[13];
} mfxExtDecVideoProcessing;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Members of this structure define the location of chroma samples information.

   See Annex E of the ISO*\/IEC* 14496-10 specification for the definition of these parameters.

   @note Not all implementations of the encoder support this structure. The application must use the Query API function to determine if it is supported.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CHROMA_LOC_INFO. */

    mfxU16       ChromaLocInfoPresentFlag;
    mfxU16       ChromaSampleLocTypeTopField;
    mfxU16       ChromaSampleLocTypeBottomField;
    mfxU16       reserved[9];
} mfxExtChromaLocInfo;
MFX_PACK_END()

/*! The MBQPMode enumerator itemizes QP update modes. */
enum {
    MFX_MBQP_MODE_QP_VALUE = 0, /*!< QP array contains QP values. */
    MFX_MBQP_MODE_QP_DELTA = 1,  /*!< QP array contains deltas for QP. */
    MFX_MBQP_MODE_QP_ADAPTIVE = 2 /*!< QP array contains deltas for QP or absolute QP values. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Specifies per-MB or per-CU mode and QP or DeltaQP value depending on the mode type.
 */
typedef struct{
    union {
          /*!
           QP for MB or CU. Valid when Mode = MFX_MBQP_MODE_QP_VALUE.

           For AVC, the valid range is 1 to 51.

           For HEVC, the valid range is 1 to 51. The application's provided QP values should be valid, otherwise invalid QP values may cause undefined behavior.

           MBQP map should be aligned for 16x16 block size. The align rule is: (width +15 /16) && (height +15 /16).

           For MPEG2, the valid range is 1 to 112. QP corresponds to quantizer_scale of the ISO*\/IEC* 13818-2 specification.
           */
        mfxU8 QP;
            /*!
             Per-macroblock QP delta. Valid when Mode = MFX_MBQP_MODE_QP_DELTA.
             */
        mfxI8 DeltaQP;
    };
    mfxU16 Mode; /*!< Defines QP update mode. Can be equal to MFX_MBQP_MODE_QP_VALUE or MFX_MBQP_MODE_QP_DELTA. */
} mfxQPandMode;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Specifies per-macroblock QP for current frame if mfxExtCodingOption3::EnableMBQP was turned ON during
   encoder initialization. The application can attach this extended buffer to the mfxEncodeCtrl structure during runtime.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MBQP. */

    mfxU32 reserved[9];
    mfxU32 Pitch;       /*!< Distance in bytes between the start of two consecutive rows in the QP array. */
    mfxU16 Mode;        /*!< Defines QP update mode. See MBQPMode enumerator for more details. */
    mfxU16 BlockSize;   /*!< QP block size, valid for HEVC only during Init and Runtime. */
    mfxU32 NumQPAlloc;  /*!< Size of allocated by application QP or DeltaQP array. */
    union {
        /*!
           Pointer to a list of per-macroblock QP in raster scan order. In case of interlaced encoding the first half of QP array affects the top
           field and the second half of QP array affects the bottom field. Valid when Mode = MFX_MBQP_MODE_QP_VALUE.

           For AVC, the valid range is 1 to 51.

           For HEVC, the valid range is 1 to 51. Application's provided QP values should be valid. Otherwise invalid QP values may cause undefined behavior.
           MBQP map should be aligned for 16x16 block size. The alignment rule is (width +15 /16) && (height +15 /16).

           For MPEG2, QP corresponds to quantizer_scale of the ISO*\/IEC* 13818-2 specification and has a valid range of 1 to 112.
        */
        mfxU8  *QP;
        mfxI8  *DeltaQP;    /*!< Pointer to a list of per-macroblock QP deltas in raster scan order.
                                 For block i: QP[i] = BrcQP[i] + DeltaQP[i]. Valid when Mode = MFX_MBQP_MODE_QP_DELTA. */
        mfxQPandMode *QPmode; /*!< Block-granularity modes when MFX_MBQP_MODE_QP_ADAPTIVE is set. */

        mfxU64 reserved2;
    };
} mfxExtMBQP;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Runtime ctrl buffer for SPS/PPS insertion with current encoding frame.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_INSERT_HEADERS. */
    mfxU16          SPS;      /*!< Tri-state option to insert SPS. */
    mfxU16          PPS;      /*!< Tri-state option to insert PPS. */
    mfxU16          reserved[8];
} mfxExtInsertHeaders;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
  Specifies rectangle areas for IPCM coding mode.
*/
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODER_IPCM_AREA. */
    mfxU16          reserve1[10];

    mfxU16          NumArea;  /*! Number of areas */
    struct area {

        mfxU32      Left; /*!< Left area coordinate. */
        mfxU32      Top;  /*!< Top area coordinate. */
        mfxU32      Right; /*!< Right area coordinate. */
        mfxU32      Bottom; /*!< Bottom area coordinate. */

        mfxU16      reserved2[8];

    } * Areas; /*!< Array of areas. */
} mfxExtEncoderIPCMArea;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Specifies macroblock map for current frame which forces specified macroblocks to be encoded as intra
   if mfxExtCodingOption3::EnableMBForceIntra was turned ON during encoder initialization. The application can attach this extended
   buffer to the mfxEncodeCtrl structure during runtime.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MB_FORCE_INTRA. */

    mfxU32 reserved[11];
    mfxU32 MapSize;  /*!< Macroblock map size. */
    union {
        mfxU8  *Map; /*!< Pointer to a list of force intra macroblock flags in raster scan order. Each flag is one byte in map. Set flag to 1
                          to force corresponding macroblock to be encoded as intra. In case of interlaced encoding, the first half of map
                          affects top field and the second half of map affects the bottom field. */
        mfxU64  reserved2;
    };
} mfxExtMBForceIntra;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures tiles options for the HEVC encoder. The application can attach this extended buffer to the
   mfxVideoParam structure to configure initialization.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_HEVC_TILES. */

    mfxU16 NumTileRows;    /*!< Number of tile rows. */
    mfxU16 NumTileColumns; /*!< Number of tile columns. */
    mfxU16 reserved[74];
}mfxExtHEVCTiles;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Specifies macroblock map for current frame which forces specified macroblocks to be non-skip if
   mfxExtCodingOption3::MBDisableSkipMap was turned ON during encoder initialization. The application can attach this
   extended buffer to the mfxEncodeCtrl structure during runtime.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MB_DISABLE_SKIP_MAP. */

    mfxU32 reserved[11];
    mfxU32 MapSize;  /*!< Macroblock map size. */
    union {
        mfxU8  *Map; /*!< Pointer to a list of non-skip macroblock flags in raster scan order. Each flag is one byte in map. Set flag to 1 to force
                          corresponding macroblock to be non-skip. In case of interlaced encoding, the first half of map affects
                          the top field and the second half of map affects the bottom field. */
        mfxU64  reserved2;
    };
} mfxExtMBDisableSkipMap;
MFX_PACK_END()

/*! The GeneralConstraintFlags enumerator uses bit-ORed values to itemize HEVC bitstream indications for specific profiles. Each value
    indicates for format range extensions profiles.
    To specify HEVC Main 10 Still Picture profile applications have to set mfxInfoMFX::CodecProfile == MFX_PROFILE_HEVC_MAIN10 and
    mfxExtHEVCParam::GeneralConstraintFlags == MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY. */
enum {
    /* REXT Profile constraint flags*/
    MFX_HEVC_CONSTR_REXT_MAX_12BIT          = (1 << 0),
    MFX_HEVC_CONSTR_REXT_MAX_10BIT          = (1 << 1),
    MFX_HEVC_CONSTR_REXT_MAX_8BIT           = (1 << 2),
    MFX_HEVC_CONSTR_REXT_MAX_422CHROMA      = (1 << 3),
    MFX_HEVC_CONSTR_REXT_MAX_420CHROMA      = (1 << 4),
    MFX_HEVC_CONSTR_REXT_MAX_MONOCHROME     = (1 << 5),
    MFX_HEVC_CONSTR_REXT_INTRA              = (1 << 6),
    MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY   = (1 << 7),
    MFX_HEVC_CONSTR_REXT_LOWER_BIT_RATE     = (1 << 8)
};


/*! The SampleAdaptiveOffset enumerator uses bit-ORed values to itemize corresponding HEVC encoding feature. */
enum {
    MFX_SAO_UNKNOWN       = 0x00, /*!< Use default value for platform/TargetUsage. */
    MFX_SAO_DISABLE       = 0x01, /*!< Disable SAO. If set during Init leads to SPS sample_adaptive_offset_enabled_flag = 0.
                                       If set during Runtime, leads to to slice_sao_luma_flag = 0 and slice_sao_chroma_flag = 0
                                       for current frame. */
    MFX_SAO_ENABLE_LUMA   = 0x02, /*!< Enable SAO for luma (slice_sao_luma_flag = 1). */
    MFX_SAO_ENABLE_CHROMA = 0x04  /*!< Enable SAO for chroma (slice_sao_chroma_flag = 1). */
};


/* This struct has 4-byte alignment for binary compatibility with previously released versions of API */
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_HEVC_PARAM. */

    mfxU16          PicWidthInLumaSamples;  /*!< Specifies the width of each coded picture in units of luma samples. */
    mfxU16          PicHeightInLumaSamples; /*!< Specifies the height of each coded picture in units of luma samples. */
    mfxU64          GeneralConstraintFlags; /*!< Additional flags to specify exact profile and constraints. See the GeneralConstraintFlags enumerator for values of this field. */
    mfxU16          SampleAdaptiveOffset;   /*!< Controls SampleAdaptiveOffset encoding feature. See the SampleAdaptiveOffset enumerator for supported values
                                                 (bit-ORed). Valid during encoder Init and Runtime. */
    mfxU16          LCUSize;                /*!< Specifies largest coding unit size (max luma coding block). Valid during encoder Init. */
    mfxU16          reserved[116];
} mfxExtHEVCParam;
MFX_PACK_END()

/*! The ErrorTypes enumerator uses bit-ORed values to itemize bitstream error types. */
enum {
    MFX_ERROR_NO                  =        0,  /*!< No error in bitstream. */
    MFX_ERROR_PPS                 = (1 << 0),  /*!< Invalid/corrupted PPS. */
    MFX_ERROR_SPS                 = (1 << 1),  /*!< Invalid/corrupted SPS. */
    MFX_ERROR_SLICEHEADER         = (1 << 2),  /*!< Invalid/corrupted slice header. */
    MFX_ERROR_SLICEDATA           = (1 << 3),  /*!< Invalid/corrupted slice data. */
    MFX_ERROR_FRAME_GAP           = (1 << 4),  /*!< Missed frames. */
    MFX_ERROR_JPEG_APP0_MARKER    = (1 << 5),  /*!< Invalid/corrupted APP0 marker. */
    MFX_ERROR_JPEG_APP1_MARKER    = (1 << 6),  /*!< Invalid/corrupted APP1 marker. */
    MFX_ERROR_JPEG_APP2_MARKER    = (1 << 7),  /*!< Invalid/corrupted APP2 marker. */
    MFX_ERROR_JPEG_APP3_MARKER    = (1 << 8),  /*!< Invalid/corrupted APP3 marker. */
    MFX_ERROR_JPEG_APP4_MARKER    = (1 << 9),  /*!< Invalid/corrupted APP4 marker. */
    MFX_ERROR_JPEG_APP5_MARKER    = (1 << 10), /*!< Invalid/corrupted APP5 marker. */
    MFX_ERROR_JPEG_APP6_MARKER    = (1 << 11), /*!< Invalid/corrupted APP6 marker. */
    MFX_ERROR_JPEG_APP7_MARKER    = (1 << 12), /*!< Invalid/corrupted APP7 marker. */
    MFX_ERROR_JPEG_APP8_MARKER    = (1 << 13), /*!< Invalid/corrupted APP8 marker. */
    MFX_ERROR_JPEG_APP9_MARKER    = (1 << 14), /*!< Invalid/corrupted APP9 marker. */
    MFX_ERROR_JPEG_APP10_MARKER   = (1 << 15), /*!< Invalid/corrupted APP10 marker. */
    MFX_ERROR_JPEG_APP11_MARKER   = (1 << 16), /*!< Invalid/corrupted APP11 marker. */
    MFX_ERROR_JPEG_APP12_MARKER   = (1 << 17), /*!< Invalid/corrupted APP12 marker. */
    MFX_ERROR_JPEG_APP13_MARKER   = (1 << 18), /*!< Invalid/corrupted APP13 marker. */
    MFX_ERROR_JPEG_APP14_MARKER   = (1 << 19), /*!< Invalid/corrupted APP14 marker. */
    MFX_ERROR_JPEG_DQT_MARKER     = (1 << 20), /*!< Invalid/corrupted DQT marker. */
    MFX_ERROR_JPEG_SOF0_MARKER    = (1 << 21), /*!< Invalid/corrupted SOF0 marker. */
    MFX_ERROR_JPEG_DHT_MARKER     = (1 << 22), /*!< Invalid/corrupted DHT marker. */
    MFX_ERROR_JPEG_DRI_MARKER     = (1 << 23), /*!< Invalid/corrupted DRI marker. */
    MFX_ERROR_JPEG_SOS_MARKER     = (1 << 24), /*!< Invalid/corrupted SOS marker. */
    MFX_ERROR_JPEG_UNKNOWN_MARKER = (1 << 25), /*!< Unknown Marker. */
};


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the decoders to report bitstream error information right after DecodeHeader or DecodeFrameAsync.
   The application can attach this extended buffer to the mfxBitstream structure at runtime.
*/
typedef struct {
    mfxExtBuffer    Header;     /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_DECODE_ERROR_REPORT. */

    mfxU32          ErrorTypes; /*!< Bitstream error types (bit-ORed values). See ErrorTypes enumerator for the list of types. */
    mfxU16          reserved[10];
} mfxExtDecodeErrorReport;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the decoders to report additional information about a decoded frame. The application can attach this
   extended buffer to the mfxFrameSurface1::mfxFrameData structure at runtime.
*/
typedef struct {
    mfxExtBuffer Header;    /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_DECODED_FRAME_INFO. */

    mfxU16       FrameType; /*!< Frame type. See FrameType enumerator for the list of types. */
    mfxU16       reserved[59];
} mfxExtDecodedFrameInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the library to pass MPEG 2 specific timing information.

   See ISO/IEC 13818-2 and ITU-T H.262, MPEG-2 Part 2 for the definition of these parameters.
*/
typedef struct {
    mfxExtBuffer Header;           /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_TIME_CODE. */

    mfxU16       DropFrameFlag;    /*!< Indicated dropped frame. */
    mfxU16       TimeCodeHours;    /*!< Hours. */
    mfxU16       TimeCodeMinutes;  /*!< Minutes. */
    mfxU16       TimeCodeSeconds;  /*!< Seconds. */
    mfxU16       TimeCodePictures; /*!< Pictures. */
    mfxU16       reserved[7];
} mfxExtTimeCode;
MFX_PACK_END()

/*! The HEVCRegionType enumerator itemizes type of HEVC region. */
enum {
    MFX_HEVC_REGION_SLICE = 0 /*!< Slice type. */
};

/*! The HEVCRegionEncoding enumerator itemizes HEVC region's encoding. */
enum {
    MFX_HEVC_REGION_ENCODING_ON  = 0,
    MFX_HEVC_REGION_ENCODING_OFF = 1
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Attached to the mfxVideoParam structure during HEVC encoder initialization. Specifies the region to encode.

   @note Not all implementations of the encoder support this structure. The application must use the Query API function to determine if it is supported.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_HEVC_REGION. */

    mfxU32       RegionId;       /*!< ID of region. */
    mfxU16       RegionType;     /*!< Type of region. See HEVCRegionType enumerator for the list of types. */
    mfxU16       RegionEncoding; /*!< Set to MFX_HEVC_REGION_ENCODING_ON to encode only specified region. */
    mfxU16       reserved[24];
} mfxExtHEVCRegion;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Specifies weighted prediction table for current frame when all of the following conditions are met:

   @li mfxExtCodingOption3::WeightedPred was set to explicit during encoder Init or Reset .

   @li The current frame is P-frame or mfxExtCodingOption3::WeightedBiPred was set to explicit during encoder Init or Reset.

   @li The current frame is B-frame and is attached to the mfxEncodeCtrl structure.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_PRED_WEIGHT_TABLE. */

    mfxU16       LumaLog2WeightDenom;     /*!< Base 2 logarithm of the denominator for all luma weighting factors. Value must be in the range of 0 to 7, inclusive. */
    mfxU16       ChromaLog2WeightDenom;   /*!< Base 2 logarithm of the denominator for all chroma weighting factors. Value must be in the range of 0 to 7, inclusive. */
    mfxU16       LumaWeightFlag[2][32];   /*!< LumaWeightFlag[L][R] equal to 1 specifies that the weighting factors for the luma component are specified for R's entry of RefPicList L. */
    mfxU16       ChromaWeightFlag[2][32]; /*!< ChromaWeightFlag[L][R] equal to 1 specifies that the weighting factors for the chroma component are specified for R's entry of RefPicList L. */
    mfxI16       Weights[2][32][3][2];    /*!< The values of the weights and offsets used in the encoding processing. The value of Weights[i][j][k][m] is
                                               interpreted as: i refers to reference picture list 0 or 1; j refers to reference list entry 0-31;
                                               k refers to data for the luma component when it is 0, the Cb chroma component when it is 1 and
                                               the Cr chroma component when it is 2; m refers to weight when it is 0 and offset when it is 1 */
    mfxU16       reserved[58];
} mfxExtPredWeightTable;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by encoders to set rounding offset parameters for quantization. It is per-frame based encoding control,
   and can be attached to some frames and skipped for others. When the extension buffer is set the application can attach it to the mfxEncodeCtrl
   during runtime.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AVC_ROUNDING_OFFSET. */

    mfxU16       EnableRoundingIntra; /*!< Enable rounding offset for intra blocks. See the CodingOptionValue enumerator for values of this option. */
    mfxU16       RoundingOffsetIntra; /*!< Intra rounding offset. Value must be in the range of 0 to 7, inclusive. */
    mfxU16       EnableRoundingInter; /*!< Enable rounding offset for inter blocks. See the CodingOptionValue enumerator for values of this option. */
    mfxU16       RoundingOffsetInter; /*!< Inter rounding offset. Value must be in the range of 0 to 7, inclusive. */

    mfxU16       reserved[24];
} mfxExtAVCRoundingOffset;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the application to specify dirty regions within a frame during encoding. It may be used at initialization or at runtime.

   Dirty rectangle definition is using end-point exclusive notation. In other words, the pixel with (Right, Bottom) coordinates lies
   immediately outside of the dirty rectangle. Left, Top, Right, Bottom should be aligned by codec-specific block boundaries (should be
   dividable by 16 for AVC, or by block size (8, 16, 32 or 64, depends on platform) for HEVC).

   Every dirty rectangle with unaligned
   coordinates will be expanded to a minimal-area block-aligned dirty rectangle, enclosing the original one.
   For example, a (5, 5, 15, 31) dirty rectangle will be expanded to (0, 0, 16, 32) for AVC encoder, or to (0, 0, 32, 32) for HEVC,
   if block size is 32.

   Dirty rectangle (0, 0, 0, 0) is a valid dirty rectangle and means that the frame is not changed.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_DIRTY_RECTANGLES. */

    mfxU16  NumRect;    /*!< Number of dirty rectangles. */
    mfxU16  reserved1[11];

    struct {
        /*! @{
        @name Dirty rectangle coordinates
        The following structure members are used by the Rect array contained in the parent structure.

         */
        mfxU32  Left;   /*!< Dirty region left coordinate. */
        mfxU32  Top;    /*!< Dirty region top coordinate. */
        mfxU32  Right;  /*!< Dirty region right coordinate. */
        mfxU32  Bottom; /*!< Dirty region bottom coordinate. */

        mfxU16  reserved2[8];
        /*! @} */
    } Rect[256];        /*!< Array of dirty rectangles. */
} mfxExtDirtyRect;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the application to specify moving regions within a frame during encoding.

   Destination rectangle location should be aligned to MB boundaries (should be dividable by 16). If not, the encoder
   truncates it to MB boundaries, for example, both 17 and 31 will be truncated to 16.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MOVING_RECTANGLE. */

    mfxU16  NumRect;        /*!< Number of moving rectangles. */
    mfxU16  reserved1[11];

    struct {
        /*! @{
        @name Destination and source rectangle location
        The following structure members are used by the Rect array contained in the parent structure.
         */
        mfxU32  DestLeft;   /*!< Destination rectangle location. */
        mfxU32  DestTop;    /*!< Destination rectangle location. */
        mfxU32  DestRight;  /*!< Destination rectangle location. */
        mfxU32  DestBottom; /*!< Destination rectangle location. */

        mfxU32  SourceLeft; /*!< Source rectangle location. */
        mfxU32  SourceTop;  /*!< Source rectangle location. */
        mfxU16  reserved2[4];
        /*! @} */
    } Rect[256];            /*!< Array of moving rectangles. */
} mfxExtMoveRect;
MFX_PACK_END()

/*! The Angle enumerator itemizes valid rotation angles. */
enum {
    MFX_ANGLE_0     =   0, /*!< 0 degrees. */
    MFX_ANGLE_90    =  90, /*!< 90 degrees. */
    MFX_ANGLE_180   = 180, /*!< 180 degrees. */
    MFX_ANGLE_270   = 270  /*!< 270 degrees. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP Rotation filter algorithm.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_ROTATION. */

    mfxU16 Angle;        /*!< Rotation angle. See Angle enumerator for supported values. */
    mfxU16 reserved[11];
} mfxExtVPPRotation;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   Used by the encoder to report additional information about encoded slices. The application can attach this
   buffer to the mfxBitstream structure before calling the MFXVideoENCODE_EncodeFrameAsync function.

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if the
         functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and call the
         MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE, then the functionality is supported.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODED_SLICES_INFO. */

    mfxU16  SliceSizeOverflow;   /*!< When mfxExtCodingOption2::MaxSliceSize is used, indicates the requested slice size was not met for one or more generated slices. */
    mfxU16  NumSliceNonCopliant; /*!< When mfxExtCodingOption2::MaxSliceSize is used, indicates the number of generated slices exceeds specification limits. */
    mfxU16  NumEncodedSlice;     /*!< Number of encoded slices. */
    mfxU16  NumSliceSizeAlloc;   /*!< SliceSize array allocation size. Must be specified by application. */
    union {
        mfxU16  *SliceSize;      /*!< Slice size in bytes. Array must be allocated by application. */
        mfxU64  reserved1;
    };

    mfxU16 reserved[20];
} mfxExtEncodedSlicesInfo;
MFX_PACK_END()

/*! The ScalingMode enumerator itemizes variants of scaling filter implementation. */
enum {
    MFX_SCALING_MODE_DEFAULT    = 0, /*!< Default scaling mode. The library selects the most appropriate scaling method. */
    MFX_SCALING_MODE_LOWPOWER   = 1, /*!< Low power scaling mode which is applicable for library implementations.
                                         The exact scaling algorithm is defined by the library. */
    MFX_SCALING_MODE_QUALITY    = 2, /*!< The best quality scaling mode. */
    MFX_SCALING_MODE_VENDOR = 1000, /*!< The enumeration to separate common scaling controls above and vendor specific. */
    MFX_SCALING_MODE_INTEL_GEN_COMPUTE  = MFX_SCALING_MODE_VENDOR + 1, /*! The mode to run scaling operation on Execution Units (EUs). */
    MFX_SCALING_MODE_INTEL_GEN_VDBOX = MFX_SCALING_MODE_VENDOR + 2, /*! The special optimization mode where scaling operation running on SFC (Scaler & Format Converter) is coupled with VDBOX (also known as Multi-Format Codec Engines). This mode is applicable for DECODE_VPP domain functions. */
    MFX_SCALING_MODE_INTEL_GEN_VEBOX = MFX_SCALING_MODE_VENDOR + 3 /*! The special optimization mode where scaling operation running on SFC is coupled with VEBOX (HW video processing pipe). */
};

/*! The InterpolationMode enumerator specifies type of interpolation method used by VPP scaling filter. */
enum {
    MFX_INTERPOLATION_DEFAULT                = 0, /*!< Default interpolation mode for scaling. Library selects the most appropriate
                                                    scaling method. */
    MFX_INTERPOLATION_NEAREST_NEIGHBOR       = 1, /*!< Nearest neighbor interpolation method. */
    MFX_INTERPOLATION_BILINEAR               = 2, /*!< Bilinear interpolation method. */
    MFX_INTERPOLATION_ADVANCED               = 3  /*!< Advanced interpolation method is defined by each implementation and usually gives best                                                          quality. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP Scaling filter algorithm.
   Not all combinations of ScalingMode and InterpolationMethod are supported in the library. The application must use the Query API function to determine if a combination is supported.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_SCALING. */

    mfxU16 ScalingMode;  /*!< Scaling mode. See ScalingMode for values. */
    mfxU16 InterpolationMethod; /*!< Interpolation mode for scaling algorithm. See InterpolationMode for values. */
    mfxU16 reserved[10];
} mfxExtVPPScaling;
MFX_PACK_END()

typedef mfxExtAVCRefListCtrl mfxExtHEVCRefListCtrl;
typedef mfxExtAVCRefLists mfxExtHEVCRefLists;
typedef mfxExtAvcTemporalLayers mfxExtHEVCTemporalLayers;

typedef mfxExtAVCRefListCtrl mfxExtRefListCtrl;
typedef mfxExtAVCEncodedFrameInfo mfxExtEncodedFrameInfo;

/* The MirroringType enumerator itemizes mirroring types. */
enum
{
    MFX_MIRRORING_DISABLED   = 0,
    MFX_MIRRORING_HORIZONTAL = 1,
    MFX_MIRRORING_VERTICAL   = 2
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP Mirroring filter algorithm.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_MIRRORING. */

    mfxU16 Type;         /*!< Mirroring type. See MirroringType for values. */
    mfxU16 reserved[11];
} mfxExtVPPMirroring;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Instructs encoder to use or not use samples over specified picture border for inter prediction. Attached to the mfxVideoParam structure.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_MV_OVER_PIC_BOUNDARIES. */

    mfxU16 StickTop;     /*!< When set to OFF, one or more samples outside corresponding picture boundary may be used in inter prediction.
                              See the CodingOptionValue enumerator for values of this option. */
    mfxU16 StickBottom;  /*!< When set to OFF, one or more samples outside corresponding picture boundary may be used in inter prediction.
                              See the CodingOptionValue enumerator for values of this option. */
    mfxU16 StickLeft;    /*!< When set to OFF, one or more samples outside corresponding picture boundary may be used in inter prediction.
                              See the CodingOptionValue enumerator for values of this option. */
    mfxU16 StickRight;   /*!< When set to OFF, one or more samples outside corresponding picture boundary may be used in inter prediction.
                              See the CodingOptionValue enumerator for values of this option. */
    mfxU16 reserved[8];
} mfxExtMVOverPicBoundaries;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Configures the VPP ColorFill filter algorithm.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_COLORFILL. */

    mfxU16 Enable;       /*!< Set to ON makes VPP fill the area between Width/Height and Crop borders.
                              See the CodingOptionValue enumerator for values of this option. */
    mfxU16 reserved[11];
} mfxExtVPPColorFill;
MFX_PACK_END()


/*! The ChromaSiting enumerator defines chroma location. Use bit-OR'ed values to specify the desired location. */
enum {
    MFX_CHROMA_SITING_UNKNOWN             = 0x0000, /*!< Unspecified. */
    MFX_CHROMA_SITING_VERTICAL_TOP        = 0x0001, /*!< Chroma samples are co-sited vertically on the top with the luma samples. */
    MFX_CHROMA_SITING_VERTICAL_CENTER     = 0x0002, /*!< Chroma samples are not co-sited vertically with the luma samples. */
    MFX_CHROMA_SITING_VERTICAL_BOTTOM     = 0x0004, /*!< Chroma samples are co-sited vertically on the bottom with the luma samples. */
    MFX_CHROMA_SITING_HORIZONTAL_LEFT     = 0x0010, /*!< Chroma samples are co-sited horizontally on the left with the luma samples. */
    MFX_CHROMA_SITING_HORIZONTAL_CENTER   = 0x0020  /*!< Chroma samples are not co-sited horizontally with the luma samples. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   A hint structure that tunes the VPP Color Conversion algorithm when
   attached to the mfxVideoParam structure during VPP Init.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_COLOR_CONVERSION. */

    mfxU16 ChromaSiting; /*!< See ChromaSiting enumerator for details. */
    mfxU16 reserved[27];
} mfxExtColorConversion;
MFX_PACK_END()


/*! The VP9ReferenceFrame enumerator itemizes reference frame type by mfxVP9SegmentParam::ReferenceFrame parameter.  */
enum {
    MFX_VP9_REF_INTRA   = 0, /*!< Intra. */
    MFX_VP9_REF_LAST    = 1, /*!< Last. */
    MFX_VP9_REF_GOLDEN  = 2, /*!< Golden. */
    MFX_VP9_REF_ALTREF  = 3  /*!< Alternative reference. */
};

/*!
   The SegmentIdBlockSize enumerator indicates the block size represented by each segment_id in segmentation map.
   These values are used with the mfxExtVP9Segmentation::SegmentIdBlockSize parameter.
*/
enum {
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_UNKNOWN =  0, /*!< Unspecified block size. */
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_8x8     =  8, /*!<  8x8  block size. */
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16   = 16, /*!< 16x16 block size. */
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32   = 32, /*!< 32x32 block size. */
    MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64   = 64, /*!< 64x64 block size. */
};

/*!
   The SegmentFeature enumerator indicates features enabled for the segment.
   These values are used with the mfxVP9SegmentParam::FeatureEnabled parameter.
*/
enum {
    MFX_VP9_SEGMENT_FEATURE_QINDEX      = 0x0001, /*!< Quantization index delta. */
    MFX_VP9_SEGMENT_FEATURE_LOOP_FILTER = 0x0002, /*!< Loop filter level delta. */
    MFX_VP9_SEGMENT_FEATURE_REFERENCE   = 0x0004, /*!< Reference frame. */
    MFX_VP9_SEGMENT_FEATURE_SKIP        = 0x0008  /*!< Skip. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Contains features and parameters for the segment.
*/
typedef struct {
    mfxU16  FeatureEnabled;       /*!< Indicates which features are enabled for the segment. See the SegmentFeature enumerator for values for this
                                       option. Values from the enumerator can be bit-OR'ed. Support of a particular feature depends on underlying
                                       hardware platform. Application can check which features are supported by calling Query. */
    mfxI16  QIndexDelta;          /*!< Quantization index delta for the segment. Ignored if MFX_VP9_SEGMENT_FEATURE_QINDEX isn't set in FeatureEnabled.
                                       Valid range for this parameter is [-255, 255]. If QIndexDelta is out of this range, it will be ignored.
                                       If QIndexDelta is within valid range, but sum of base quantization index and QIndexDelta is out of [0, 255],
                                       QIndexDelta will be clamped. */
    mfxI16  LoopFilterLevelDelta; /*!< Loop filter level delta for the segment. Ignored if MFX_VP9_SEGMENT_FEATURE_LOOP_FILTER is not set in
                                       FeatureEnabled. Valid range for this parameter is [-63, 63]. If LoopFilterLevelDelta is out of this range,
                                       it will be ignored. If LoopFilterLevelDelta is within valid range, but sum of base loop filter level and
                                       LoopFilterLevelDelta is out of [0, 63], LoopFilterLevelDelta will be clamped. */
    mfxU16  ReferenceFrame;       /*!< Reference frame for the segment. See VP9ReferenceFrame enumerator for values for this option. Ignored
                                       if MFX_VP9_SEGMENT_FEATURE_REFERENCE isn't set in FeatureEnabled. */
    mfxU16  reserved[12];
} mfxVP9SegmentParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   In the VP9 encoder it is possible to divide a frame into up to 8 segments and apply particular features (like delta for quantization index or for
   loop filter level) on a per-segment basis. "Uncompressed header" of every frame indicates if segmentation is enabled for the current frame,
   and (if segmentation enabled) contains full information about features applied to every segment. Every "Mode info block" of a coded
   frame has segment_id in the range of 0 to 7.

   To enable Segmentation, the mfxExtVP9Segmentation structure with correct settings should be passed to the encoder. It can be attached to the
   mfxVideoParam structure during initialization or the MFXVideoENCODE_Reset call (static configuration). If the mfxExtVP9Segmentation buffer isn't
   attached during initialization, segmentation is disabled for static configuration. If the buffer isn't attached for the Reset call, the encoder
   continues to use static configuration for segmentation which was the default before this Reset call. If the mfxExtVP9Segmentation buffer with
   NumSegments=0 is provided during initialization or Reset call, segmentation becomes disabled for static configuration.

   The buffer can be attached to the mfxEncodeCtrl structure during runtime (dynamic configuration). Dynamic configuration is applied to the
   current frame only. After encoding of the current frame, the encoder will switch to the next dynamic configuration or to static configuration if dynamic configuration
   is not provided for next frame).

   The SegmentIdBlockSize, NumSegmentIdAlloc, and SegmentId parameters represent a segmentation map. Here, the segmentation map is an array of segment_ids (one
   byte per segment_id) for blocks of size NxN in raster scan order. The size NxN is specified by the application and is constant for the whole frame.
   If mfxExtVP9Segmentation is attached during initialization and/or during runtime, all three parameters should be set to proper values that do not
   conflict with each other and with NumSegments. If any of the parameters are not set or any conflict or error in these parameters is detected by the library, the segmentation
   map will be discarded.
*/
typedef struct {
    mfxExtBuffer        Header;             /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VP9_SEGMENTATION. */
    mfxU16              NumSegments;        /*!< Number of segments for frame. Value 0 means that segmentation is disabled. Sending 0 for a
                                                 particular frame will disable segmentation for this frame only. Sending 0 to the Reset API function will
                                                 disable segmentation permanently. Segmentation can be enabled again by a subsequent Reset call. */
    mfxVP9SegmentParam  Segment[8];         /*!< Array of mfxVP9SegmentParam structures containing features and parameters for every segment.
                                                 Entries with indexes bigger than NumSegments-1 are ignored. See the mfxVP9SegmentParam structure for
                                                 definitions of segment features and their parameters. */
    mfxU16              SegmentIdBlockSize; /*!< Size of block (NxN) for segmentation map. See SegmentIdBlockSize enumerator for values for this
                                                 option. An encoded block that is bigger than SegmentIdBlockSize uses segment_id taken from it's
                                                 top-left sub-block from the segmentation map. The application can check if a particular block size is
                                                 supported by calling Query. */
    mfxU32              NumSegmentIdAlloc;  /*!< Size of buffer allocated for segmentation map (in bytes). Application must assure that
                                                 NumSegmentIdAlloc is large enough to cover frame resolution with blocks of size SegmentIdBlockSize.
                                                 Otherwise the segmentation map will be discarded. */
    union {
        mfxU8           *SegmentId;         /*!< Pointer to the segmentation map buffer which holds the array of segment_ids in raster scan order. The application
                                                 is responsible for allocation and release of this memory. The buffer pointed to by SegmentId, provided during
                                                 initialization or Reset call should be considered in use until another SegmentId is provided via Reset
                                                 call (if any), or until MFXVideoENCODE_Close is called. The buffer pointed to by SegmentId provided with
                                                 mfxEncodeCtrl should be considered in use while the input surface is locked by the library. Every segment_id in the
                                                 map should be in the range of 0 to NumSegments-1. If some segment_id is out of valid range, the
                                                 segmentation map cannot be applied. If the mfxExtVP9Segmentation buffer is attached to the mfxEncodeCtrl structure in
                                                 runtime, SegmentId can be zero. In this case, the segmentation map from static configuration will be used. */
        mfxU64          reserved1;
    };
    mfxU16  reserved[52];
} mfxExtVP9Segmentation;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Specifies temporal layer.
*/
typedef struct {
    mfxU16 FrameRateScale;  /*!< The ratio between the frame rates of the current temporal layer and the base layer. The library treats a particular
                                 temporal layer as "defined" if it has FrameRateScale > 0. If the base layer is defined, it must have FrameRateScale = 1. FrameRateScale of each subsequent layer (if defined) must be a multiple of and greater than the
                                 FrameRateScale value of previous layer. */
    mfxU16 TargetKbps;      /*!< Target bitrate for the current temporal layer. Ignored if RateControlMethod is CQP. If RateControlMethod is not CQP, the
                                 application must provide TargetKbps for every defined temporal layer. TargetKbps of each subsequent layer (if defined)
                                 must be greater than the TargetKbps value of the previous layer. */
    mfxU16 reserved[14];
} mfxVP9TemporalLayer;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   API allows the encoding of VP9 bitstreams that contain several subset bitstreams that differ in frame rates, also called "temporal layers".

   When decoding, each temporal layer can be extracted from the coded stream and decoded separately. The mfxExtVP9TemporalLayers structure
   configures the temporal layers for the VP9 encoder. It can be attached to the mfxVideoParam structure during initialization or the
   MFXVideoENCODE_Reset call. If the mfxExtVP9TemporalLayers buffer isn't attached during initialization, temporal scalability is disabled. If the buffer isn't attached for the Reset call, the encoder continues to use the temporal scalability configuration that was defined before the Reset call.

   In the API, temporal layers are ordered by their frame rates in ascending order. Temporal layer 0 (having the lowest frame rate) is called the base layer.
   Each subsequent temporal layer includes all previous layers.

   The temporal scalability feature requires a minimum number of allocated reference
   frames (controlled by the NumRefFrame parameter). If the NumRefFrame value set by the application isn't enough to build the reference structure for the requested
   number of temporal layers, the library corrects the NumRefFrame value. The temporal layer structure is reset (re-started) after key-frames.
*/
typedef struct {
    mfxExtBuffer        Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VP9_TEMPORAL_LAYERS. */
    /*!
       The array of temporal layers. Layer[0] specifies the base layer.

       The library reads layers from the array when they are defined (FrameRateScale > 0).
       All layers starting from first layer with FrameRateScale = 0 are ignored. The last layer that is not ignored is considered the "highest layer".

       The frame rate of the highest layer is specified in the mfxVideoParam structure. Frame rates of lower layers are calculated using their FrameRateScale.

       TargetKbps of the highest layer should be equal to the TargetKbps value specified in the mfxVideoParam structure. If it is not true, TargetKbps of highest temporal layers has priority.

       If there are no defined layers in the Layer array, the temporal scalability feature is disabled. For example, to disable temporal scalability in runtime, the application should
       pass mfxExtVP9TemporalLayers buffer to Reset with all FrameRateScales set to 0.
    */
    mfxVP9TemporalLayer Layer[8];
    mfxU16              reserved[60];
} mfxExtVP9TemporalLayers;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Structure attached to the mfxVideoParam structure. Extends the mfxVideoParam structure with VP9-specific parameters. Used by both decoder and encoder.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VP9_PARAM. */

    mfxU16  FrameWidth;      /*!< Width of the coded frame in pixels. */
    mfxU16  FrameHeight;     /*!< Height of the coded frame in pixels. */

    mfxU16  WriteIVFHeaders; /*!< Set this option to ON to make the encoder insert IVF container headers to the output stream. The NumFrame field of the IVF
                                  sequence header will be zero. It is the responsibility of the application to update the NumFrame field  with the correct value. See the
                                  CodingOptionValue enumerator for values of this option. */

    mfxI16  reserved1[6];
    mfxI16  QIndexDeltaLumaDC;   /*!< Specifies an offset for a particular quantization parameter. */
    mfxI16  QIndexDeltaChromaAC; /*!< Specifies an offset for a particular quantization parameter. */
    mfxI16  QIndexDeltaChromaDC; /*!< Specifies an offset for a particular quantization parameter. */
    /*!
       Number of tile rows. Should be power of two. The maximum number of tile rows is 4, per the VP9 specification. In addition, the maximum supported number
       of tile rows may depend on the underlying library implementation.

       Use the Query API function to check if a particular pair of values (NumTileRows, NumTileColumns)
       is supported. In VP9, tile rows have dependencies and cannot be encoded or decoded in parallel. Therefore, tile rows are always encoded by the library in
       serial mode (one-by-one).
    */
    mfxU16  NumTileRows;
    /*!
       Number of tile columns. Should be power of two. Restricted with maximum and minimum tile width in luma pixels, as defined in the VP9
       specification (4096 and 256 respectively). In addition, the maximum supported number of tile columns may depend on the underlying library
       implementation.

       Use the Query API function to check if a particular pair of values (NumTileRows, NumTileColumns) is supported. In VP9,  tile columns do not have
       dependencies and can be encoded/decoded in parallel. Therefore, tile columns can be encoded by the library in both parallel and serial modes.

       Parallel mode is automatically utilized by the library when NumTileColumns exceeds 1 and does not exceed the number of tile coding engines on the
       platform. In other cases, serial mode is used. Parallel mode is capable of encoding more than 1 tile row (within limitations provided by VP9
       specification and particular platform). Serial mode supports only tile grids 1xN and Nx1.
    */
    mfxU16  NumTileColumns;
    mfxU16  reserved[110];
} mfxExtVP9Param;
MFX_PACK_END()


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to report encoded unit information.
*/
typedef struct {
    mfxU16 Type;      /*!< Codec-dependent coding unit type (NALU type for AVC/HEVC, start_code for MPEG2 etc). */
    mfxU16 reserved1;
    mfxU32 Offset;    /*!< Offset relative to the associated mfxBitstream::DataOffset. */
    mfxU32 Size;      /*!< Unit size, including delimiter. */
    mfxU32 reserved[5];
} mfxEncodedUnitInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*!
   If mfxExtCodingOption3::EncodedUnitsInfo was set to MFX_CODINGOPTION_ON during encoder initialization, the mfxExtEncodedUnitsInfo structure is
   attached to the mfxBitstream structure during encoding. It is used to report information about coding units in the resulting bitstream.

   The number of filled items in UnitInfo is min(NumUnitsEncoded, NumUnitsAlloc).

   For counting a minimal amount of encoded units you can use the following algorithm:
   @code
      nSEI = amountOfApplicationDefinedSEI;
      if (CodingOption3.NumSlice[IPB] != 0 || mfxVideoParam.mfx.NumSlice != 0)
        ExpectedAmount = 10 + nSEI + Max(CodingOption3.NumSlice[IPB], mfxVideoParam.mfx.NumSlice);
      else if (CodingOption2.NumMBPerSlice != 0)
        ExpectedAmount = 10 + nSEI + (FrameWidth * FrameHeight) / (256 * CodingOption2.NumMBPerSlice);
      else if (CodingOption2.MaxSliceSize != 0)
        ExpectedAmount = 10 + nSEI + Round(MaxBitrate / (FrameRate*CodingOption2.MaxSliceSize));
      else
        ExpectedAmount = 10 + nSEI;

      if (mfxFrameInfo.PictStruct != MFX_PICSTRUCT_PROGRESSIVE)
        ExpectedAmount = ExpectedAmount * 2;

      if (temporalScaleabilityEnabled)
        ExpectedAmount = ExpectedAmount * 2;
    @endcode
    @note Only supported by the AVC encoder.
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODED_UNITS_INFO. */

    union {
        mfxEncodedUnitInfo *UnitInfo; /*!< Pointer to an array of mfxEncodedUnitsInfo structures whose size is equal to or greater than NumUnitsAlloc. */
        mfxU64  reserved1;
    };
    mfxU16 NumUnitsAlloc;             /*!< UnitInfo array size. */
    mfxU16 NumUnitsEncoded;           /*!< Output field. Number of coding units to report. If NumUnitsEncoded is greater than NumUnitsAlloc, the UnitInfo
                                           array will contain information only for the first NumUnitsAlloc units. User may consider reallocating the
                                           UnitInfo array to avoid this for subsequent frames. */

    mfxU16 reserved[22];
} mfxExtEncodedUnitsInfo;
MFX_PACK_END()


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Provides setup for the Motion-Compensated Temporal Filter (MCTF) during the VPP initialization and for control
   parameters at runtime. By default, MCTF is off. An application may enable it by adding MFX_EXTBUFF_VPP_MCTF to the mfxExtVPPDoUse buffer or by
   attaching mfxExtVppMctf to the mfxVideoParam structure during initialization or reset.
*/
typedef struct {
    mfxExtBuffer Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_MCTF. */
    mfxU16       FilterStrength; /*!< Value in range of 0 to 20 (inclusive) to indicate the filter strength of MCTF.

                                    The strength of the MCTF process controls the degree of possible change of pixel values eligible for MCTF - the greater the strength value, the larger the change. It is a dimensionless quantity - values in the range of 1 to 20 inclusively imply strength; value 0 stands for AUTO mode and is
                                      valid during initialization or reset only

                                    If an invalid value is given, it is fixed to the default value of 0.
                                      If the field value is in the range of 1 to 20 inclusive, MCTF operates in fixed-strength mode with the given strength of MCTF process.

                                      At runtime, values of 0 and greater than 20 are ignored. */
    mfxU16       reserved[27];
} mfxExtVppMctf;
MFX_PACK_END()

/*! Describes type of workload passed to MFXQueryAdapters. */
typedef enum
{
    MFX_COMPONENT_ENCODE = 1, /*!< Encode workload. */
    MFX_COMPONENT_DECODE = 2, /*!< Decode workload. */
    MFX_COMPONENT_VPP    = 3  /*!< VPP workload. */
} mfxComponentType;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Contains workload description, which is accepted by MFXQueryAdapters function.
*/
typedef struct
{
    mfxComponentType Type;         /*!< Type of workload: Encode, Decode, VPP. See mfxComponentType enumerator for values. */
    mfxVideoParam    Requirements; /*!< Detailed description of workload. See mfxVideoParam for details. */

    mfxU16           reserved[4];
} mfxComponentInfo;
MFX_PACK_END()

/* Adapter description */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Contains a description of the graphics adapter for the Legacy mode.
*/
typedef struct
{
    mfxPlatform Platform; /*!< Platform type description. See mfxPlatform for details. */
    mfxU32      Number;   /*!< Value which uniquely characterizes media adapter. On Windows* this number can be used for initialization through
                               DXVA interface (see <a href="https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory1-enumadapters1">example</a>). */

    mfxU16      reserved[14];
} mfxAdapterInfo;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Contains description of all graphics adapters available on the current system.
*/
typedef struct
{
    mfxAdapterInfo * Adapters;  /*!< Pointer to array of mfxAdapterInfo structs allocated by user. */
    mfxU32           NumAlloc;  /*!< Length of Adapters array. */
    mfxU32           NumActual; /*!< Number of Adapters entries filled by MFXQueryAdapters. */

    mfxU16           reserved[4];
} mfxAdaptersInfo;
MFX_PACK_END()


/*! The PartialBitstreamOutput enumerator indicates flags of partial bitstream output type. */
enum {
    MFX_PARTIAL_BITSTREAM_NONE    = 0, /*!< Do not use partial output */
    MFX_PARTIAL_BITSTREAM_SLICE   = 1, /*!< Partial bitstream output will be aligned to slice granularity */
    MFX_PARTIAL_BITSTREAM_BLOCK   = 2, /*!< Partial bitstream output will be aligned to user-defined block size granularity */
    MFX_PARTIAL_BITSTREAM_ANY     = 3  /*!< Partial bitstream output will be return any coded data available at the end of SyncOperation timeout */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by an encoder to output parts of the bitstream as soon as they are ready. The application can attach this extended buffer to the
   mfxVideoParam structure at initialization. If this option is turned ON (Granularity != MFX_PARTIAL_BITSTREAM_NONE), then the encoder can output
   bitstream by part based on the required granularity.

   This parameter is valid only during initialization and reset. Absence of this buffer means default or previously configured bitstream output
   behavior.

   @note Not all codecs and implementations support this feature. Use the Query API function to check if this feature is supported.
*/
typedef struct {
    mfxExtBuffer    Header;      /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM. */
    mfxU32          BlockSize;   /*!< Output block granularity for PartialBitstreamGranularity. Valid only for MFX_PARTIAL_BITSTREAM_BLOCK. */
    mfxU16          Granularity; /*!< Granularity of the partial bitstream: slice/block/any, all types of granularity state in PartialBitstreamOutput enum. */
    mfxU16          reserved[8];
} mfxExtPartialBitstreamParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   The mfxExtDeviceAffinityMask structure is used by the application to specify
   affinity mask for the device with given device ID. See mfxDeviceDescription
   for the device ID definition and sub device indexes. If the implementation
   manages CPU threads for some purpose, the user can set the CPU thread affinity
   mask by using this structure with DeviceID set to "CPU".
*/
typedef struct {
    /*! Extension buffer header. Header.BufferId must be equal to
        MFX_EXTBUFF_DEVICE_AFFINITY_MASK. */
    mfxExtBuffer    Header;
    /*! Null terminated string with device ID. In case of CPU affinity mask
        it must be equal to "CPU". */
    mfxChar         DeviceID[MFX_STRFIELD_LEN];
    /*! Number of sub devices or threads in case of CPU in the mask. */
    mfxU32          NumSubDevices;
    /*! Mask array. Every bit represents sub-device (or thread for CPU).
        "1" means execution is allowed. "0" means that execution is prohibited on
        this sub-device (or thread). Length of the array is equal to the:
        "NumSubDevices / 8" and rounded to the closest (from the right) integer.
        Bits order within each entry of the mask array is LSB: bit 0 holds data
        for sub device with index 0 and bit 8 for sub device with index 8.
        Index of sub device is defined by the mfxDeviceDescription structure. */
    mfxU8           *Mask;
    mfxU32           reserved[4]; /*! Reserved for future use. */
} mfxExtDeviceAffinityMask;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used by AV1 encoder with more parameter control to encode frame. */
typedef struct {
    mfxExtBuffer Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_BITSTREAM_PARAM. */

    mfxU16 WriteIVFHeaders; /*!< Tri-state option to control IVF headers insertion, default is ON.
                                Writing IVF headers is enabled in the encoder when mfxExtAV1BitstreamParam is attached and its value is ON or zero.
                                Writing IVF headers is disabled by default in the encoder when mfxExtAV1BitstreamParam is not attached. */

    mfxU16 reserved[31];
} mfxExtAV1BitstreamParam;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used by AV1 encoder with more parameter control to encode frame. */
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_RESOLUTION_PARAM. */

    mfxU32 FrameWidth;   /*!< Width of the coded frame in pixels, default value is from mfxFrameInfo. */
    mfxU32 FrameHeight;  /*!< Height of the coded frame in pixels, default value is from mfxFrameInfo. */

    mfxU32 reserved[6];
} mfxExtAV1ResolutionParam;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
  /*! The structure is used by AV1 encoder with more parameter control to encode frame. */
typedef struct {
    mfxExtBuffer Header;   /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_TILE_PARAM. */

    mfxU16 NumTileRows;    /*!< Number of tile rows, default value is 1. */
    mfxU16 NumTileColumns; /*!< Number of tile columns, default value is 1. */
    mfxU16 NumTileGroups;  /*!< Number of tile groups, it will be ignored if the tile groups num is invalid, default value is 1. */

    mfxU16 reserved[5];
} mfxExtAV1TileParam;
MFX_PACK_END()

/*!
    The AV1 SegmentIdBlockSize enumerator indicates the block size represented by each segment_id in segmentation map.
    These values are used with the mfxExtAV1Segmentation::SegmentIdBlockSize parameter.
*/
typedef enum {
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_UNSPECIFIED = 0,  /*!< Unspecified block size. */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_4x4         = 4,  /*!< block size 4x4 */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_8x8         = 8,  /*!< block size 8x8 */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_16x16       = 16, /*!< block size 16x16 */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32       = 32, /*!< block size 32x32 */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64       = 64, /*!< block size 64x64 */
    MFX_AV1_SEGMENT_ID_BLOCK_SIZE_128x128     = 128 /*!< block size 128x128 */
} mfxAV1SegmentIdBlockSize;

/*!
    The AV1 SegmentFeature enumerator indicates features enabled for the segment.
    These values are used with the mfxAV1SegmentParam::FeatureEnabled parameter.
*/
enum {
    MFX_AV1_SEGMENT_FEATURE_ALT_QINDEX    = 0x0001, /*!< use alternate Quantizer. */
    MFX_AV1_SEGMENT_FEATURE_ALT_LF_Y_VERT = 0x0002, /*!< use alternate loop filter value on y plane vertical. */
    MFX_AV1_SEGMENT_FEATURE_ALT_LF_Y_HORZ = 0x0004, /*!< use alternate loop filter value on y plane horizontal. */
    MFX_AV1_SEGMENT_FEATURE_ALT_LF_U      = 0x0008, /*!< use alternate loop filter value on u plane. */
    MFX_AV1_SEGMENT_FEATURE_ALT_LF_V      = 0x0010, /*!< use alternate loop filter value on v plane. */
    MFX_AV1_SEGMENT_FEATURE_REFERENCE     = 0x0020, /*!< use segment reference frame. */
    MFX_AV1_SEGMENT_FEATURE_SKIP          = 0x0040, /*!< use segment (0,0) + skip mode. */
    MFX_AV1_SEGMENT_FEATURE_GLOBALMV      = 0x0080  /*!< use global motion vector. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
    Contains features and parameters for the segment.
*/
typedef struct {
    mfxU16 FeatureEnabled;  /*!< Indicates which features are enabled for the segment. See the AV1 SegmentFeature enumerator for values for
                                this option. Values from the enumerator can be bit-OR'ed. Support of a particular feature depends on underlying
                                hardware platform. Application can check which features are supported by calling Query. */
    mfxI16 AltQIndex;       /*!< Quantization index delta for the segment. Ignored if MFX_AV1_SEGMENT_FEATURE_ALT_QINDEX isn't set in FeatureEnabled.
                                Valid range for this parameter is [-255, 255]. If AltQIndex is out of this range, it will be ignored. If AltQIndex
                                is within valid range, but sum of base quantization index and AltQIndex is out of [0, 255], AltQIndex will be clamped. */
    mfxU16 reserved[30];
} mfxAV1SegmentParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
    In the AV1 encoder it is possible to divide a frame into up to 8 segments and apply particular features (like delta for quantization index or for
    loop filter level) on a per-segment basis. "Uncompressed header" of every frame indicates if segmentation is enabled for the current frame,
    and (if segmentation enabled) contains full information about features applied to every segment. Every "Mode info block" of a coded
    frame has segment_id in the range of 0 to 7.
    To enable Segmentation, the mfxExtAV1Segmentation structure with correct settings should be passed to the encoder. It can be attached to the
    mfxVideoParam structure during initialization or the MFXVideoENCODE_Reset call (static configuration). If the mfxExtAV1Segmentation buffer isn't
    attached during initialization, segmentation is disabled for static configuration. If the buffer isn't attached for the Reset call, the encoder
    continues to use static configuration for segmentation which was the default before this Reset call. If the mfxExtAV1Segmentation buffer with
    NumSegments=0 is provided during initialization or Reset call, segmentation becomes disabled for static configuration.
    The buffer can be attached to the mfxEncodeCtrl structure during runtime (dynamic configuration). Dynamic configuration is applied to the
    current frame only. After encoding of the current frame, the encoder will switch to the next dynamic configuration or to static configuration if
    dynamic configuration is not provided for next frame).
    The SegmentIdBlockSize, NumSegmentIdAlloc, and SegmentId parameters represent a segmentation map. Here, the segmentation map is an array of
    segment_ids (one byte per segment_id) for blocks of size NxN in raster scan order. The size NxN is specified by the application and is constant
    for the whole frame.
    If mfxExtAV1Segmentation is attached during initialization and/or during runtime, all three parameters should be set to proper values that do not
    conflict with each other and with NumSegments. If any of the parameters are not set or any conflict or error in these parameters is detected by
    the library, the segmentation map will be discarded.
*/
typedef struct {
    mfxExtBuffer       Header;            /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_SEGMENTATION. */
    mfxU8              NumSegments;       /*!< Number of segments for frame. Value 0 means that segmentation is disabled. Sending 0 for a
                                                particular frame will disable segmentation for this frame only. Sending 0 to the Reset API function will
                                                disable segmentation permanently. Segmentation can be enabled again by a subsequent Reset call. */
    mfxU8              reserved1[3];
    mfxAV1SegmentParam Segment[8];        /*!< Array of mfxAV1SegmentParam structures containing features and parameters for every segment.
                                                Entries with indexes bigger than NumSegments-1 are ignored. See the mfxAV1SegmentParam structure for
                                                definitions of segment features and their parameters. */
    mfxU16             SegmentIdBlockSize;/*!< Size of block (NxN) for segmentation map. See AV1 SegmentIdBlockSize enumerator for values for this
                                                option. An encoded block that is bigger than AV1 SegmentIdBlockSize uses segment_id taken from it's
                                                top-left sub-block from the segmentation map. The application can check if a particular block size is
                                                supported by calling Query. */
    mfxU16             reserved2;
    mfxU32             NumSegmentIdAlloc; /*!< Size of buffer allocated for segmentation map (in bytes). Application must assure that
                                                NumSegmentIdAlloc is large enough to cover frame resolution with blocks of size SegmentIdBlockSize.
                                                Otherwise the segmentation map will be discarded. */
    mfxU8 *            SegmentIds;        /*!< Pointer to the segmentation map buffer which holds the array of segment_ids in raster scan order. The application
                                                is responsible for allocation and release of this memory. The buffer pointed to by SegmentId, provided during
                                                initialization or Reset call should be considered in use until another SegmentId is provided via Reset
                                                call (if any), or until MFXVideoENCODE_Close is called. The buffer pointed to by SegmentId provided with
                                                mfxEncodeCtrl should be considered in use while the input surface is locked by the library. Every segment_id in the
                                                map should be in the range of 0 to NumSegments-1. If some segment_id is out of valid range, the
                                                segmentation map cannot be applied. If the mfxExtAV1Segmentation buffer is attached to the mfxEncodeCtrl structure in
                                                runtime, SegmentId can be zero. In this case, the segmentation map from static configuration will be used. */
    mfxU16             reserved[36];
} mfxExtAV1Segmentation;
MFX_PACK_END()

/*! The FilmGrainFlags enumerator itemizes flags in AV1 film grain parameters.
    The flags are equivalent to respective syntax elements from film_grain_params() section of uncompressed header. */
enum {
    MFX_FILM_GRAIN_NO                       =       0, /*!< Film grain isn't added to this frame. */
    MFX_FILM_GRAIN_APPLY                    = (1 << 0), /*!< Film grain is added to this frame. */
    MFX_FILM_GRAIN_UPDATE                   = (1 << 1), /*!< New set of film grain parameters is sent for this frame. */
    MFX_FILM_GRAIN_CHROMA_SCALING_FROM_LUMA = (1 << 2), /*!< Chroma scaling is inferred from luma scaling. */
    MFX_FILM_GRAIN_OVERLAP                  = (1 << 3), /*!< Overlap between film grain blocks is applied. */
    MFX_FILM_GRAIN_CLIP_TO_RESTRICTED_RANGE = (1 << 4) /*!< Clipping to the restricted (studio) range is applied after adding the film grain. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Defines film grain point. */
typedef struct {
    mfxU8 Value; /*!<  The x coordinate for the i-th point of the piece-wise linear scaling function for luma/Cb/Cr component. */
    mfxU8 Scaling; /*!<  The scaling (output) value for the i-th point of the piecewise linear scaling function for luma/Cb/Cr component. */
} mfxAV1FilmGrainPoint;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used by AV-1 decoder to report film grain parameters for decoded frame. */
typedef struct {
    mfxExtBuffer Header;    /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM. */

    mfxU16 FilmGrainFlags;  /*!< Bit map with bit-ORed flags from FilmGrainFlags enum. */
    mfxU16 GrainSeed;       /*!< Starting value for pseudo-random numbers used during film grain synthesis. */

    mfxU8  RefIdx;          /*!< Indicate which reference frame contains the film grain parameters to be used for this frame. */
    mfxU8  NumYPoints;      /*!< The number of points for the piece-wise linear scaling function of the luma component. */
    mfxU8  NumCbPoints;     /*!< The number of points for the piece-wise linear scaling function of the Cb component. */
    mfxU8  NumCrPoints;     /*!< The number of points for the piece-wise linear scaling function of the Cr component.*/

    mfxAV1FilmGrainPoint PointY[14]; /*!< The array of points for luma component. */
    mfxAV1FilmGrainPoint PointCb[10]; /*!< The array of points for Cb component. */
    mfxAV1FilmGrainPoint PointCr[10]; /*!< The array of points for Cr component. */

    mfxU8 GrainScalingMinus8; /*!< The shift - 8 applied to the values of the chroma component. The grain_scaling_minus_8 can take values of 0..3 and
                                   determines the range and quantization step of the standard deviation of film grain.*/
    mfxU8 ArCoeffLag;         /*!< The number of auto-regressive coefficients for luma and chroma.*/

    mfxU8 ArCoeffsYPlus128[24];  /*!< Auto-regressive coefficients used for the Y plane. */
    mfxU8 ArCoeffsCbPlus128[25]; /*!< Auto-regressive coefficients used for the Cb plane. */
    mfxU8 ArCoeffsCrPlus128[25]; /*!< The number of points for the piece-wise linear scaling function of the Cr component.*/

    mfxU8 ArCoeffShiftMinus6;  /*!< The range of the auto-regressive coefficients.
                                    Values of 0, 1, 2, and 3 correspond to the ranges for auto-regressive coefficients of
                                    [-2, 2), [-1, 1), [-0.5, 0.5) and [-0.25, 0.25) respectively.*/
    mfxU8 GrainScaleShift;     /*!< Downscaling factor of the grain synthesis process for the Gaussian random numbers .*/

    mfxU8  CbMult;     /*!< The multiplier for the Cb component used in derivation of the input index to the Cb component scaling function.*/
    mfxU8  CbLumaMult; /*!< The multiplier for the average luma component used in derivation of the input index to the Cb component scaling function. */
    mfxU16 CbOffset;   /*!< The offset used in derivation of the input index to the Cb component scaling function.*/

    mfxU8  CrMult;     /*!< The multiplier for the Cr component used in derivation of the input index to the Cr component scaling function.*/
    mfxU8  CrLumaMult; /*!< The multiplier for the average luma component used in derivation of the input index to the Cr component scaling function.*/
    mfxU16 CrOffset;   /*!< The offset used in derivation of the input index to the Cr component scaling function.*/

    mfxU16 reserved[43];
} mfxExtAV1FilmGrainParam;
MFX_PACK_END()

#define MFX_SURFACEARRAY_VERSION MFX_STRUCT_VERSION(1, 0)


MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure is reference counted object to return array of surfaces allocated and processed by the library. */
typedef struct mfxSurfaceArray
{
    mfxHDL              Context; /*!< The context of the memory interface. User should not touch (change, set, null) this pointer. */
    mfxStructVersion    Version; /*!< The version of the structure. */
    mfxU16 reserved[3];
    /*! @brief
    Increments the internal reference counter of the surface. The surface is not destroyed until the surface is released using the mfxSurfaceArray::Release function.
    mfxSurfaceArray::AddRef should be used each time a new link to the surface is created (for example, copy structure) for proper surface management.

    @param[in]  surface  Valid mfxSurfaceArray.

    @return
     MFX_ERR_NONE              If no error. \n
     MFX_ERR_NULL_PTR          If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE    If mfxSurfaceArray->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN           Any internal error.

    */
    mfxStatus (MFX_CDECL *AddRef)(struct mfxSurfaceArray*  surface_array);
    /*! @brief
    Decrements the internal reference counter of the surface. mfxSurfaceArray::Release should be called after
    using the mfxSurfaceArray::AddRef function to add a surface or when allocation logic requires it.

    @param[in]  surface_array  Valid mfxSurfaceArray.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfaceArray->Context is invalid (for example NULL). \n
     MFX_ERR_UNDEFINED_BEHAVIOR If Reference Counter of surface is zero before call. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *Release)(struct mfxSurfaceArray*  surface_array);

    /*! @brief
    Returns current reference counter of mfxSurfaceArray structure.

    @param[in]   surface  Valid surface_array.
    @param[out]  counter  Sets counter to the current reference counter value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If surface or counter is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxSurfaceArray->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *GetRefCounter)(struct mfxSurfaceArray*  surface_array, mfxU32* counter);

    mfxFrameSurface1** Surfaces; /*!< The array of pointers to mfxFrameSurface1. mfxFrameSurface1 surfaces are allocated by the same
    agent who allocates mfxSurfaceArray. */
    mfxU32 NumSurfaces; /*!<The size of array of pointers to mfxFrameSurface1. */
    mfxU32 reserved1;
} mfxSurfaceArray;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure is used for VPP channels initialization in Decode_VPP component.  */
typedef struct {
    mfxFrameInfo  VPP; /*!< The configuration parameters of VPP filters per each channel. */
    mfxU16  Protected; /*!< Specifies the content protection mechanism. */
    mfxU16  IOPattern; /*!< Output memory access types for SDK functions. */
    mfxExtBuffer** ExtParam; /*!< Points to an array of pointers to the extra configuration structures; see the ExtendedBufferID enumerator for a list of extended configurations. */
    mfxU16  NumExtParam; /*!< The number of extra configuration structures attached to the structure. */
    mfxU16  reserved1[7];
} mfxVideoChannelParam;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure describes rectangle coordinates that can be used for ROI or for Cropping. */
    typedef struct {
        mfxU16  Left;   /*!< X coordinate of region of top-left corner of rectangle. */
        mfxU16  Top;    /*!< Y coordinate of region of top-left corner of rectangle. */
        mfxU16  Right;  /*!< X coordinate of region of bottom-right corner of rectangle. */
        mfxU16  Bottom; /*!< Y coordinate of region of bottom-right corner of rectangle. */
    } mfxRect;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure contains crop parameters which applied by Decode_VPP component to input surfaces before video processing operation.
    It is used for letterboxing operations.
*/
typedef struct {
    mfxExtBuffer     Header; /*! Extension buffer header. BufferId must be equal to MFX_EXTBUFF_CROPS. */
    mfxRect          Crops;  /*!< Crops parameters for letterboxing operations. */
    mfxU32           reserved[4];
}mfxExtInCrops;
MFX_PACK_END()

/*! The mfxHyperMode enumerator describes HyperMode implementation behavior. */
typedef enum {
    MFX_HYPERMODE_OFF = 0x0,        /*!< Don't use HyperMode implementation. */
    MFX_HYPERMODE_ON = 0x1,         /*!< Enable HyperMode implementation and return error if some issue on initialization. */
    MFX_HYPERMODE_ADAPTIVE = 0x2,   /*!< Enable HyperMode implementation and switch to single fallback if some issue on initialization. */
} mfxHyperMode;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used for HyperMode initialization. */
typedef struct {
    mfxExtBuffer    Header; /*!< Extension buffer header. BufferId must be equal to MFX_EXTBUFF_HYPER_MODE_PARAM. */
    mfxHyperMode    Mode;   /*!< HyperMode implementation behavior. */
    mfxU16          reserved[19];
} mfxExtHyperModeParam;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used for universal temporal layer description. */
typedef struct {
    mfxU16 FrameRateScale;  /*!< The ratio between the frame rates of the current temporal layer and the base layer. The library treats a particular
                                 temporal layer as "defined" if it has FrameRateScale > 0. If the base layer is defined, it must have FrameRateScale = 1.
                                 FrameRateScale of each subsequent layer (if defined) must be a multiple of and greater than the
                                 FrameRateScale value of previous layer. */
    mfxU16  reserved[3]; /*!< Reserved for future use. */

    union {
          /*!< Type of bitrate controls is currently the same across all temporal layers and inherits from common parameters. */
          struct {
            mfxU32  InitialDelayInKB;/*!< Initial size of the Video Buffering Verifier (VBV) buffer for the current temporal layer.
                                          @note In this context, KB is 1000 bytes and Kbps is 1000 bps. */
            mfxU32  BufferSizeInKB; /*!< Represents the maximum possible size of any compressed frames for the current temporal layer. */
            mfxU32  TargetKbps;  /*!< Target bitrate for the current temporal layer. If RateControlMethod is not CQP, the
                                      application can provide TargetKbps for every defined temporal layer. If TargetKbps per temporal layer is not set then
                                      encoder doesn't apply any special bitrate limitations for the layer.  */
            mfxU32  MaxKbps;  /*!< The maximum bitrate at which the encoded data enters the Video Buffering Verifier (VBV) buffer for the current temporal layer. */

            mfxU32  reserved1[16]; /*!< Reserved for future use. */

          };
          struct {
            mfxI32  QPI;  /*!< Quantization Parameter (QP) for I-frames for constant QP mode (CQP) for the current temporal layer. Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPI might be clipped to supported QPI range.
                    @note Default QPI value is implementation dependent and subject to change without additional notice in this document. */
            mfxI32  QPP;  /*!< Quantization Parameter (QP) for P-frames for constant QP mode (CQP) for the current temporal layer. Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPP might be clipped to supported QPI range.
                    @note Default QPP value is implementation dependent and subject to change without additional notice in this document. */
            mfxI32  QPB; /*!< Quantization Parameter (QP) for B-frames for constant QP mode (CQP) for the current temporal layer. Zero QP is not valid and means that the default value is assigned by the library.
                    Non-zero QPI might be clipped to supported QPB range.
                    @note Default QPB value is implementation dependent and subject to change without additional notice in this document. */
          };
    };
    mfxU16  reserved2[4]; /*!< Reserved for future use. */

} mfxTemporalLayer;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure is used for universal temporal layers description. */
typedef struct {
    mfxExtBuffer     Header;     /*! Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS. */
    mfxU16           NumLayers; /*!< The  number of temporal layers. */
    mfxU16           BaseLayerPID; /*!< The priority ID of the base layer. The encoder increases the ID for each temporal layer and writes to the prefix NAL unit for AVC and HEVC. */
    mfxU16           reserved[2]; /*!< Reserved for future use. */
    mfxTemporalLayer *Layers; /*!< The array of temporal layers. */

    mfxU16 reserved1[8]; /*!< Reserved for future use. */
} mfxExtTemporalLayers;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The structure is used to configure perceptual encoding prefilter in VPP. */
typedef struct {
    mfxExtBuffer Header;         /*! Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_PERC_ENC_PREFILTER. */
    mfxU16       reserved[252];
} mfxExtVPPPercEncPrefilter;
MFX_PACK_END()
#endif

#ifdef ONEVPL_EXPERIMENTAL
/*! The TuneQuality enumerator specifies tuning option for encode. Multiple tuning options can be combined using bit mask. */
enum {
    MFX_ENCODE_TUNE_OFF = 0,  /*!< Tuning quality is disabled.  */
    MFX_ENCODE_TUNE_PSNR    = 0x1, /*!< The encoder optimizes quality according to Peak Signal-to-Noise Ratio (PSNR) metric. */
    MFX_ENCODE_TUNE_SSIM    = 0x2, /*!< The encoder optimizes quality according to Structural Similarity Index Measure (SSIM) metric. */
    MFX_ENCODE_TUNE_MS_SSIM = 0x4, /*!< The encoder optimizes quality according to Multi-Scale Structural Similarity Index Measure (MS-SSIM) metric. */
    MFX_ENCODE_TUNE_VMAF    = 0x8, /*!< The encoder optimizes quality according to Video Multi-Method Assessment Fusion (VMAF) metric. */
    MFX_ENCODE_TUNE_PERCEPTUAL    = 0x10, /*!< The encoder makes perceptual quality optimization. */
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure specifies type of quality optimization used by the encoder. The buffer can also be attached for VPP functions to make correspondent pre-filtering. */
typedef struct {
    mfxExtBuffer   Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_TUNE_ENCODE_QUALITY. */
    mfxU32         TuneQuality;    /*!< The control to specify type of encode quality metric(s) to optimize; See correspondent enum. */
    mfxExtBuffer** ExtParam;       /*!< Points to an array of pointers to the extra configuration structures; see the ExtendedBufferID enumerator for a list of extended configurations. */
    mfxU16         NumExtParam;    /*!< The number of extra configuration structures attached to the structure. */
    mfxU16         reserved[11];
} mfxExtTuneEncodeQuality;
MFX_PACK_END()
#endif

/*! The mfxAISuperResolutionMode enumerator specifies the mode of AI based super resolution. */
typedef enum {
    MFX_AI_SUPER_RESOLUTION_MODE_DISABLED = 0,        /*!< Super Resolution is disabled.*/
    MFX_AI_SUPER_RESOLUTION_MODE_DEFAULT = 1,         /*!< Default super resolution mode. The library selects the most appropriate super resolution mode.*/
#ifdef ONEVPL_EXPERIMENTAL
    MFX_AI_SUPER_RESOLUTION_MODE_SHARPEN = 2,         /*!< In this mode, super Resolution is optimized or trained to have high sharpness level. This mode is recommended to be used in video conference(camera
                                                           noise) or similar usage scenario.*/
    MFX_AI_SUPER_RESOLUTION_MODE_ARTIFACTREMOVAL= 3,  /*!< In this mode, Super Resolution is optimized or trained to remove encoding artifacts with medium sharpness level. This mode is recommended to be used in
                                                           video surveillance or similar usage scenarios which may have camera noise and encoding artifacts due to limited network bandwidth.*/
#endif
} mfxAISuperResolutionMode;

#ifdef ONEVPL_EXPERIMENTAL
typedef enum {
    MFX_AI_SUPER_RESOLUTION_ALGORITHM_DEFAULT = 0,        /*!< Super Resolution algorithm by default. The library selects the most appropriate super resolution algorithm.*/
    MFX_AI_SUPER_RESOLUTION_ALGORITHM_1       = 1,        /*!< Super Resolution algorithm1.*/
    MFX_AI_SUPER_RESOLUTION_ALGORITHM_2       = 2,        /*!< Super Resolution algorithm2, MFX_AI_SUPER_RESOLUTION_ALGORITHM_2 video quality is expected to be better than MFX_AI_SUPER_RESOLUTION_ALGORITHM_1.*/
} mfxAISuperResolutionAlgorithm;
#endif

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
    A hint structure that configures AI based super resolution VPP filter.
    Super resolution is an AI-powered upscaling feature which converts a low-resolution to high-resolution.
    On some platforms this filter is not supported. To query its support, the application should use the same approach that it uses to configure VPP filters:
    adding the filter ID to the mfxExtVPPDoUse structure or by attaching the mfxExtVPPAISuperResolution structure directly to the mfxVideoParam structure and
    calling the Query API function. If the filter is supported, the function returns a MFX_ERR_NONE status; otherwise, the function returns MFX_ERR_UNSUPPORTED.
    If both mfxExtVPPAISuperResolution and mfxExtVPPScaling are attached during initialization, the function will return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; if both
    of them are attached during runtime, the mfxExtVPPAISuperResolution will override the upscaling mode and use super resolution.
    If the application needs to switch on and off, the application can set the MFX_AI_SUPER_RESOLUTION_MODE_DISABLED to switch off, MFX_AI_SUPER_RESOLUTION_MODE_DEFAULT
    to switch on.
*/
typedef struct {
    mfxExtBuffer                Header;               /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_AI_SUPER_RESOLUTION.*/
    mfxAISuperResolutionMode    SRMode;               /*!< Indicates Super Resolution Mode. mfxAISuperResolutionMode enumerator.*/
#ifdef ONEVPL_EXPERIMENTAL
    mfxAISuperResolutionAlgorithm SRAlgorithm;        /*!< Indicates Super Resolution Algorithm. mfxAISuperResolutionAlgorithm enumerator.*/
    mfxU32                      reserved1[15];         /*!< Reserved for future use. */
#else
    mfxU32                      reserved1[16];         /*!< Reserved for future use. */
#endif
    mfxHDL                      reserved2[4];          /*!< Reserved for future use. */
} mfxExtVPPAISuperResolution;
MFX_PACK_END()

/* The mfxAIFrameInterpolationMode enumerator specifies the mode of AI based frame interpolation. */
typedef enum {
    MFX_AI_FRAME_INTERPOLATION_MODE_DISABLE = 0,         /*!< AI based frame interpolation is disabled. The library duplicates the frame if AI frame interpolation is disabled.*/
    MFX_AI_FRAME_INTERPOLATION_MODE_DEFAULT = 1,         /*!< Default AI based frame interpolation mode. The library selects the most appropriate AI based frame interpolation mode.*/

#ifdef ONEVPL_EXPERIMENTAL
    MFX_AI_FRAME_INTERPOLATION_MODE_BEST_SPEED = 2,      /*!< AI based frame interpolation in best speed.*/
    MFX_AI_FRAME_INTERPOLATION_MODE_BEST_QUALITY = 3,    /*!< AI based frame interpolation in best quality.*/
#endif
} mfxAIFrameInterpolationMode;

/*!
    A hint structure that configures AI based frame interpolation VPP filter.
    AI powered frame interpolation feature can reconstruct one or more intermediate frames between two consecutive frames by AI method.
    On some platforms this filter is not supported. To query its support, the application should use the same approach that it uses to configure VPP filters:
    Attaching the mfxExtVPPAIFrameInterpolation structure directly to the mfxVideoParam structure and setting the frame rate of input and output (FrameRateExtN and FrameRateExtD),
    then calling the Query API function. If the filter is supported, the Query function returns a MFX_ERR_NONE status; otherwise, the function returns MFX_ERR_UNSUPPORTED.
    As a new method of frame interpolation, the application can attach mfxExtVPPAIFrameInterpolation to mfxVideoParam during initialization for frame interpolation, or attach both
    mfxExtVPPAIFrameInterpolation and mfxExtVPPFrameRateConversion to mfxVideoParam and use which mfxExtVPPAIFrameInterpolation is regarded as a new algorithm of mfxExtVPPFrameRateConversion
    (MFX_FRCALGM_AI_FRAME_INTERPOLATION).
    The applications should follow video processing procedures and call the API mfxStatus MFXVideoVPP_RunFrameVPPAsync(Session, Input, Output, Auxdata, Syncp) to process the frames one by one.
    The below is detailed explanation of video processing procedures in this AI frame interpolation case. If the application does not follow the below input/output sequence, the application could
    get the unexpected output and get an error return value.
    Input:    Frame0                Frame1                Frame2                Frame3                        FrameN
    Output:   Frame0    Frame0.5    Frame1    Frame1.5    Frame2    Frame2.5    Frame3    FrameX.5            FrameN
    #0 API call: Input Frame0, Output Frame0, Return MFX_ERR_NONE.
    #1 API call: Input Frame1, Output Frame0.5 and Return MFX_ERR_MORE_SURFACE.
    #2 API call: Input Frame1, Output Frame1, Return MFX_ERR_NONE.
    #3 API call: Input Frame2, Output Frame1.5, Return MFX_ERR_MORE_SURFACE.
    #4 API call: Input Frame2, Output Frame2, Return MFX_ERR_NONE.
*/
MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxExtBuffer                        Header;                           /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_VPP_AI_FRAME_INTERPOLATION.*/
    mfxAIFrameInterpolationMode         FIMode;                           /*!< Indicates frame interpolation mode. The mfxAIFrameInterpolationMode enumerator.*/
    mfxU16                              EnableScd;                        /*!< Indicates if enabling scene change detection(SCD) of the library. Recommend to enable this flag for
                                                                               better quality. Value 0 means disable SCD, Value 1 means enable SCD.*/

    mfxU32                              reserved1[24];                    /*!< Reserved for future use. */
    mfxHDL                              reserved2[8];                     /*!< Reserved for future use. */
} mfxExtVPPAIFrameInterpolation;
MFX_PACK_END()

/*! The mfxQualityInfoMode enumerator specifies the mode of Quality information. */
typedef enum {
    MFX_QUALITY_INFO_DISABLE        = 0,   /*!< Quality reporting disabled. */
    MFX_QUALITY_INFO_LEVEL_FRAME    = 0x1, /*!< Frame level quality report. */
} mfxQualityInfoMode;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the encoder to set quality information report mode for the encoded picture.

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if
         the functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and
         call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE then the functionality is supported.
*/
typedef struct {
    mfxExtBuffer        Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODED_QUALITY_INFO_MODE. */
    mfxQualityInfoMode  QualityInfoMode;/*!< See mfxQualityInfoMode enumeration for supported modes. */
    mfxU32              reserved[5];    /*!< Reserved for future use. */
} mfxExtQualityInfoMode;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Used by the encoder to report quality information about the encoded picture. The application can attach
   this buffer to the mfxBitstream structure before calling MFXVideoENCODE_EncodeFrameAsync function.
*/
typedef struct {
    mfxExtBuffer        Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_ENCODED_QUALITY_INFO_OUTPUT. */
    mfxU32              FrameOrder;     /*!< Frame display order of encoded picture. */
    mfxU32              MSE[3];         /*!< Frame level mean squared errors (MSE) for Y/U/V channel.
                                             @note MSE is stored in U24.8 format. The calculation formula is: PSNR = 10 * log10(256.0 * (2^bitDepth - 1)^2 / (double)MSE)). */
    mfxU32              reserved1[50];  /*!< Reserved for future use. */
    mfxHDL              reserved2[4];   /*!< Reserved for future use. */
} mfxExtQualityInfoOutput;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the encoder to set the screen content tools.

   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if
         the functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and
         call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE then the functionality is supported.
*/
typedef struct {
    mfxExtBuffer        Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS. */
    /*!
       Set this flag to MFX_CODINGOPTION_ON to enable palette prediction for encoder. Set this flag to MFX_CODINGOPTION_OFF to disable it.
       If this flag is set to any other value, the default value will be used which can be obtained from the MFXVideoENCODE_GetVideoParam function after encoding initialization.
       See the CodingOptionValue enumerator for values of this option. This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16              Palette;
    /*!
       Set this flag to MFX_CODINGOPTION_ON to enable intra block copy prediction for encoder. Set this flag to MFX_CODINGOPTION_OFF to disable it.
       If this flag is set to any other value, the default value will be used which can be obtained from the MFXVideoENCODE_GetVideoParam function after encoding initialization.
       See the CodingOptionValue enumerator for values of this option. This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16              IntraBlockCopy;
    mfxU16              reserved[10];   /*!< Reserved for future use. */
} mfxExtAV1ScreenContentTools;
MFX_PACK_END()


/*! The AlphaChannelMode enumerator specifies alpha is straight or pre-multiplied. */
enum {
    /*!
        RGB and alpha are independent, then the alpha value specifies how solid it is.
        We set it to the default value, i.e., the alpha source data is already pre-multiplied, so that the decoded samples of the associated primary picture
        should not be multiplied by the interpretation sample values of the auxiliary coded picture in the display process after output from the decoding process.
    */
    MFX_ALPHA_MODE_PREMULTIPLIED    = 1,

    /*!
        RGB and alpha are linked, then the alpha value specifies how much it obscures whatever is behind it.
        Therefore, the decoded samples of the associated primary picture should be multiplied by the interpretation sample values
        of the auxiliary coded picture in the display process after output from the decoding process.
    */
    MFX_ALPHA_MODE_STRAIGHT         = 2
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Configure the alpha channel encoding. */
typedef struct {
    mfxExtBuffer        Header;     /*!< Extension buffer header. BufferId must be equal to MFX_EXTBUFF_ALPHA_CHANNEL_ENC_CTRL. */
    /*!
       Set this flag to MFX_CODINGOPTION_ON to enable alpha channel encoding. See the CodingOptionValue enumerator for values of this option.
       This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16              EnableAlphaChannelEncoding;
    /*!
        Specifies alpha is straight or pre-multiplied. See the AlphaChannelMode enumerator for details.
        Encoder just record this in the SEI for post-decoding rendering.
    */
    mfxU16              AlphaChannelMode;
    /*!
       Indicates the percentage of the auxiliary alpha layer in the total bitrate. Valid range for this parameter is [1, 99].
       We set 25 as the default value, i.e. Alpha(25) : Total(100), then 25% of the bits will be spent on alpha layer encoding whereas the other 75% will be spent on base(YUV) layer.
       Affects the following variables: InitialDelayInKB, BufferSizeInKB, TargetKbps, MaxKbps.
    */
    mfxU16              AlphaChannelBitrateRatio;
    mfxU16              reserved[9];
} mfxExtAlphaChannelEncCtrl;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Defines the uncompressed frames surface information and data buffers for alpha channel encoding. */
typedef struct {
    mfxExtBuffer        Header;         /*!< Extension buffer header. BufferId must be equal to MFX_EXTBUFF_ALPHA_CHANNEL_SURFACE. */
    mfxFrameSurface1*   AlphaSurface;   /*!< Alpha channel surface. */
    mfxU16              reserved[8];
} mfxExtAlphaChannelSurface;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL
MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used by the encoder to switch to ai assisted encoder solutions.
   @note Not all implementations of the encoder support this extended buffer. The application must use query mode 1 to determine if
         the functionality is supported. To do this, the application must attach this extended buffer to the mfxVideoParam structure and
         call the MFXVideoENCODE_Query function. If the function returns MFX_ERR_NONE then the functionality is supported.
*/
typedef struct {
    mfxExtBuffer        Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_AI_ENC_CTRL. */
    /*!
       Set this flag to MFX_CODINGOPTION_ON to enable saliency encoder solution. Set this flag to MFX_CODINGOPTION_OFF to disable it.
       If this flag is set to any other value, the default value OFF will be used.
       See the CodingOptionValue enumerator for values of this option. This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16              SaliencyEncoder;
    /*!
       Set this flag to MFX_CODINGOPTION_ON to enable ML-based adaptive target usage solution. Set this flag to MFX_CODINGOPTION_OFF to disable it.
       If this flag is set to any other value, the default value will be used which can be obtained from the MFXVideoENCODE_GetVideoParam function after encoding initialization.
       See the CodingOptionValue enumerator for values of this option. This parameter is valid only during initialization.
       @note Not all codecs and implementations support this value. Use the Query API function to check if this feature is supported.
    */
    mfxU16              AdaptiveTargetUsage;
    mfxU16              reserved[26];   /*!< Reserved for future use. */
} mfxExtAIEncCtrl;
MFX_PACK_END()
#endif

#ifdef __cplusplus
} // extern "C"

#endif


#endif
