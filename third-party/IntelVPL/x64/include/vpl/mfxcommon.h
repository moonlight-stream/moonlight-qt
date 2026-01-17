/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXCOMMON_H__
#define __MFXCOMMON_H__
#include "mfxdefs.h"

#if !defined (__GNUC__)
#pragma warning(disable: 4201)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define MFX_MAKEFOURCC(A,B,C,D)    ((((int)A))+(((int)B)<<8)+(((int)C)<<16)+(((int)D)<<24))

/* Extended Configuration Header Structure */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The common header definition for external buffers and video
    processing hints. */
typedef struct {
    mfxU32  BufferId; /*!< Identifier of the buffer content. See the ExtendedBufferID enumerator for a complete list of extended buffers. */
    mfxU32  BufferSz; /*!< Size of the buffer. */
} mfxExtBuffer;
MFX_PACK_END()


#define MFX_REFINTERFACE_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The structure represents reference counted interface structure.
    The memory is allocated and released by the implementation.
*/
typedef struct mfxRefInterface {
    mfxHDL              Context; /*!< The context of the container interface. User should not touch (change, set, null) this pointer. */
    mfxStructVersion    Version; /*!< The version of the structure. */
    /*! @brief
    Increments the internal reference counter of the container. The container is not destroyed until the container
    is released using the mfxRefInterface::Release function.
    mfxRefInterface::AddRef should be used each time a new link to the container is created
    (for example, copy structure) for proper  management.

    @param[in]  ref_interface  Valid interface.

    @return
     MFX_ERR_NONE              If no error. \n
     MFX_ERR_NULL_PTR          If interface is NULL. \n
     MFX_ERR_INVALID_HANDLE    If mfxRefInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN           Any internal error.

    */
    mfxStatus (MFX_CDECL *AddRef)(struct mfxRefInterface*  ref_interface);
   /*! @brief
    Decrements the internal reference counter of the container. mfxRefInterface::Release should be called after using the
    mfxRefInterface::AddRef function to add a container or when allocation logic requires it.

    @param[in]  ref_interface  Valid interface.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If interface is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxRefInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNDEFINED_BEHAVIOR If Reference Counter of container is zero before call. \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *Release)(struct mfxRefInterface*  ref_interface);
    /*! @brief
    Returns current reference counter of mfxRefInterface structure.

    @param[in]   ref_interface  Valid interface.
    @param[out]  counter  Sets counter to the current reference counter value.

    @return
     MFX_ERR_NONE               If no error. \n
     MFX_ERR_NULL_PTR           If interface or counter is NULL. \n
     MFX_ERR_INVALID_HANDLE     If mfxRefInterface->Context is invalid (for example NULL). \n
     MFX_ERR_UNKNOWN            Any internal error.
    */
    mfxStatus (MFX_CDECL *GetRefCounter)(struct mfxRefInterface*  ref_interface, mfxU32* counter);
    mfxHDL reserved[4];

}mfxRefInterface;
MFX_PACK_END()

/* Library initialization and deinitialization */
/*!
    This enumerator itemizes implementation types.
    The implementation type is a bit OR'ed value of the base type and any decorative flags.
    @note This enumerator is for legacy dispatcher compatibility only. The new dispatcher does not use it.
 */
typedef mfxI32 mfxIMPL;
/*!
    The application can use the macro MFX_IMPL_BASETYPE(x) to obtain the base implementation type.
*/
#define MFX_IMPL_BASETYPE(x) (0x00ff & (x))

enum  {
    MFX_IMPL_AUTO         = 0x0000,  /*!< Auto Selection/In or Not Supported/Out. */
    MFX_IMPL_SOFTWARE     = 0x0001,  /*!< Pure software implementation. */
    MFX_IMPL_HARDWARE     = 0x0002,  /*!< Hardware accelerated implementation (default device). */
    MFX_IMPL_AUTO_ANY     = 0x0003,  /*!< Auto selection of any hardware/software implementation. */
    MFX_IMPL_HARDWARE_ANY = 0x0004,  /*!< Auto selection of any hardware implementation. */
    MFX_IMPL_HARDWARE2    = 0x0005,  /*!< Hardware accelerated implementation (2nd device). */
    MFX_IMPL_HARDWARE3    = 0x0006,  /*!< Hardware accelerated implementation (3rd device). */
    MFX_IMPL_HARDWARE4    = 0x0007,  /*!< Hardware accelerated implementation (4th device). */
    MFX_IMPL_RUNTIME      = 0x0008,  /*!< This value cannot be used for session initialization. It may be returned by the MFXQueryIMPL
                                          function to show that the session has been initialized in run-time mode. */
    MFX_IMPL_VIA_ANY      = 0x0100,  /*!< Hardware acceleration can go through any supported OS infrastructure. This is the default value. The default value
                                          is used by the legacy Intel(r) Media SDK if none of the MFX_IMPL_VIA_xxx flags are specified by the application. */
    MFX_IMPL_VIA_D3D9     = 0x0200,  /*!< Hardware acceleration goes through the Microsoft* Direct3D* 9 infrastructure. */
    MFX_IMPL_VIA_D3D11    = 0x0300,  /*!< Hardware acceleration goes through the Microsoft* Direct3D* 11 infrastructure. */
    MFX_IMPL_VIA_VAAPI    = 0x0400,  /*!< Hardware acceleration goes through the Linux* VA-API infrastructure. */
    MFX_IMPL_VIA_HDDLUNITE     = 0x0500,  /*!< Hardware acceleration goes through the HDDL* Unite*. */

    MFX_IMPL_UNSUPPORTED  = 0x0000  /*!< One of the MFXQueryIMPL returns. */
};

/* Version Info */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! The mfxVersion union describes the version of the implementation.*/
typedef union {
    /*! @brief Structure with Major and Minor fields.  */
    /*! @struct Anonymous */
    struct {
        /*! @{
          @name Major and Minor fields
          Anonymous structure with Major and Minor fields.
           */
        mfxU16  Minor; /*!< Minor number of the implementation. */
        mfxU16  Major; /*!< Major number of the implementation. */
        /*! @} */
    };
    mfxU32  Version;   /*!< Implementation version number. */
} mfxVersion;
MFX_PACK_END()

/*! The mfxPriority enumerator describes the session priority. */
typedef enum
{
    MFX_PRIORITY_LOW = 0,    /*!< Low priority: the session operation halts when high priority tasks are executing and more than 75% of the CPU is being used for normal priority tasks.*/
    MFX_PRIORITY_NORMAL = 1, /*!< Normal priority: the session operation is halted if there are high priority tasks.*/
    MFX_PRIORITY_HIGH = 2    /*!< High priority: the session operation blocks other lower priority session operations.*/

} mfxPriority;

typedef struct _mfxEncryptedData mfxEncryptedData;
MFX_PACK_BEGIN_STRUCT_W_L_TYPE()
/*! Defines the buffer that holds compressed video data. */
typedef struct {
     /*! @internal :unnamed(union) @endinternal */
     union {
        struct {
            mfxEncryptedData* EncryptedData; /*!< Reserved and must be zero. */
            mfxExtBuffer **ExtParam;         /*!< Array of extended buffers for additional bitstream configuration. See the ExtendedBufferID enumerator for a complete list of extended buffers. */
            mfxU16  NumExtParam;             /*!< The number of extended buffers attached to this structure. */
            mfxU16  reserved1;               /*!< Reserved for future use. */
            mfxU32  CodecId;                 /*!< Specifies the codec format identifier in the FourCC code. See the CodecFormatFourCC enumerator for details. This optional parameter is required for the simplified decode initialization.  */

        };
         mfxU32  reserved[6];
     };
    /*! Decode time stamp of the compressed bitstream in units of 90KHz. A value of MFX_TIMESTAMP_UNKNOWN indicates that there is no time stamp.

        This value is calculated by the encoder from the presentation time stamp provided by the application in the mfxFrameSurface1 structure and
        from the frame rate provided by the application during the encoder initialization. */
    mfxI64  DecodeTimeStamp;
    mfxU64  TimeStamp;                       /*!< Time stamp of the compressed bitstream in units of 90KHz. A value of MFX_TIMESTAMP_UNKNOWN indicates that there is no time stamp. */
    mfxU8*  Data;                            /*!< Bitstream buffer pointer, 32-bytes aligned. */
    mfxU32  DataOffset;                      /*!< Next reading or writing position in the bitstream buffer. */
    mfxU32  DataLength;                      /*!< Size of the actual bitstream data in bytes. */
    mfxU32  MaxLength;                       /*!< Allocated bitstream buffer size in bytes. */

    mfxU16  PicStruct;                       /*!< Type of the picture in the bitstream. Output parameter. */
    mfxU16  FrameType;                       /*!< Frame type of the picture in the bitstream. Output parameter. */
    mfxU16  DataFlag;                        /*!< Indicates additional bitstream properties. See the BitstreamDataFlag enumerator for details. */
    mfxU16  reserved2;                       /*!< Reserved for future use. */
} mfxBitstream;
MFX_PACK_END()

/*! Synchronization point object handle. */
typedef struct _mfxSyncPoint *mfxSyncPoint;

/*! The GPUCopy enumerator controls usage of GPU accelerated copying between video and system memory in the legacy Intel(r) Media SDK components. */
enum {
    MFX_GPUCOPY_DEFAULT = 0, /*!< Use default mode for the legacy Intel(r) Media SDK implementation. */
    MFX_GPUCOPY_ON      = 1, /*!< The hint to enable GPU accelerated copying when it is supported by the library.
                                  If the library doesn't support GPU accelerated copy the operation will be made by CPU.
                                  Buffer caching usage decision is up to runtime to decide, for explicit hints please use MFX_GPUCOPY_SAFE or MFX_GPUCOPY_FAST */
    MFX_GPUCOPY_OFF     = 2, /*!< Disable GPU accelerated copying. */
    MFX_GPUCOPY_SAFE    = 3, /*!< The hint to disable buffer caching for GPU accelerated copying. Actual when GPU accelerated copying is supported by the library. */
#ifdef ONEVPL_EXPERIMENTAL
    MFX_GPUCOPY_FAST    = 4  /*!< The hint to enable buffer caching for GPU accelerated copying. Actual when GPU accelerated copying is supported by the library. */
#endif
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Specifies advanced initialization parameters.
    A zero value in any of the fields indicates that the corresponding field
    is not explicitly specified.
*/
typedef struct {
    mfxIMPL     Implementation;  /*!< Enumerator that indicates the desired legacy Intel(r) Media SDK implementation. */
    mfxVersion  Version;         /*!< Structure which specifies minimum library version or zero, if not specified. */
    mfxU16      ExternalThreads; /*!< Desired threading mode. Value 0 means internal threading, 1 - external. */
    /*! @internal :unnamed(union) @endinternal */
    union {
        struct {
            mfxExtBuffer **ExtParam; /*!< Points to an array of pointers to the extra configuration structures; see the ExtendedBufferID enumerator for a list of extended configurations. */
            mfxU16  NumExtParam;     /*!< The number of extra configuration structures attached to this structure. */
        };
        mfxU16  reserved2[5];
    };
    mfxU16      GPUCopy;         /*!< Enables or disables GPU accelerated copying between video and system memory in legacy Intel(r) Media SDK components. See the GPUCopy enumerator for a list of valid values. */
    mfxU16      reserved[21];
} mfxInitParam;
MFX_PACK_END()

enum {
    MFX_EXTBUFF_THREADS_PARAM = MFX_MAKEFOURCC('T','H','D','P') /*!< mfxExtThreadsParam buffer ID. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies options for threads created by this session. Attached to the
    mfxInitParam structure during legacy Intel(r) Media SDK session initialization
    or to mfxInitializationParam by the dispatcher in MFXCreateSession function. */
typedef struct {
    mfxExtBuffer Header;         /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_THREADS_PARAM. */

    mfxU16       NumThread;      /*!< The number of threads. */
    mfxI32       SchedulingType; /*!< Scheduling policy for all threads.*/
    mfxI32       Priority;       /*!< Priority for all threads. */
    mfxU16       reserved[55];   /*!< Reserved for future use. */
} mfxExtThreadsParam;
MFX_PACK_END()

/*! Deprecated. */
enum {
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_UNKNOWN)        = 0,  /*!< Unknown platform. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_SANDYBRIDGE)    = 1,  /*!< Intel(r) microarchitecture code name Sandy Bridge. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_IVYBRIDGE)      = 2,  /*!< Intel(r) microarchitecture code name Ivy Bridge. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_HASWELL)        = 3,  /*!< Code name Haswell. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_BAYTRAIL)       = 4,  /*!< Code name Bay Trail. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_BROADWELL)      = 5,  /*!< Intel(r) microarchitecture code name Broadwell. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_CHERRYTRAIL)    = 6,  /*!< Code name Cherry Trail. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_SKYLAKE)        = 7,  /*!< Intel(r) microarchitecture code name Skylake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_APOLLOLAKE)     = 8,  /*!< Code name Apollo Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_KABYLAKE)       = 9,  /*!< Code name Kaby Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_GEMINILAKE)     = 10, /*!< Code name Gemini Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_COFFEELAKE)     = 11, /*!< Code name Coffee Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_CANNONLAKE)     = 20, /*!< Code name Cannon Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ICELAKE)        = 30, /*!< Code name Ice Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_JASPERLAKE)     = 32, /*!< Code name Jasper Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ELKHARTLAKE)    = 33, /*!< Code name Elkhart Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_TIGERLAKE)      = 40, /*!< Code name Tiger Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ROCKETLAKE)     = 42, /*!< Code name Rocket Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ALDERLAKE_S)    = 43, /*!< Code name Alder Lake S. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ALDERLAKE_P)    = 44, /*!< Code name Alder Lake P. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ARCTICSOUND_P)  = 45,
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_XEHP_SDV)       = 45, /*!< Code name XeHP SDV. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_DG2)            = 46, /*!< Code name DG2. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ATS_M)          = 46, /*!< Code name ATS-M, same media functionality as DG2. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ALDERLAKE_N)    = 55, /*!< Code name Alder Lake N. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_KEEMBAY)        = 50, /*!< Code name Keem Bay. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_METEORLAKE)     = 51, /*!< Code name Meteor Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_BATTLEMAGE)     = 52, /*!< Code name Battlemage. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_LUNARLAKE)      = 53, /*!< Code name Lunar Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_ARROWLAKE)      = 54, /*!< Code name Arrow Lake. */
    MFX_DEPRECATED_ENUM_FIELD_INSIDE(MFX_PLATFORM_MAXIMUM)        = 65535, /*!< General code name. */
};

/*! The mfxMediaAdapterType enumerator itemizes types of graphics adapters. */
typedef enum
{
    MFX_MEDIA_UNKNOWN           = 0xffff, /*!< Unknown type. */
    MFX_MEDIA_INTEGRATED        = 0,      /*!< Integrated graphics adapter. */
    MFX_MEDIA_DISCRETE          = 1       /*!< Discrete graphics adapter. */
} mfxMediaAdapterType;

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Contains information about hardware platform for the Legacy mode. */
typedef struct {
   MFX_DEPRECATED  mfxU16 CodeName;         /*!< Deprecated. */
    mfxU16 DeviceId;         /*!< Unique identifier of graphics device. */
    mfxU16 MediaAdapterType; /*!< Description of graphics adapter type. See the mfxMediaAdapterType enumerator for a list of possible values. */
    mfxU16 reserved[13];     /*!< Reserved for future use. */
} mfxPlatform;
MFX_PACK_END()


/*! The mfxResourceType enumerator specifies types of different native data frames and buffers. */
typedef enum {
    MFX_RESOURCE_SYSTEM_SURFACE                  = 1, /*!< System memory. */
    MFX_RESOURCE_VA_SURFACE_PTR                  = 2, /*!< Pointer to VA surface index. */
    MFX_RESOURCE_VA_SURFACE                      = MFX_RESOURCE_VA_SURFACE_PTR, /*!< Pointer to VA surface index. */
    MFX_RESOURCE_VA_BUFFER_PTR                   = 3, /*!< Pointer to VA buffer index. */
    MFX_RESOURCE_VA_BUFFER                       = MFX_RESOURCE_VA_BUFFER_PTR, /*!< Pointer to VA buffer index. */
    MFX_RESOURCE_DX9_SURFACE                     = 4, /*!< Pointer to IDirect3DSurface9. */
    MFX_RESOURCE_DX11_TEXTURE                    = 5, /*!< Pointer to ID3D11Texture2D. */
    MFX_RESOURCE_DX12_RESOURCE                   = 6, /*!< Pointer to ID3D12Resource. */
    MFX_RESOURCE_DMA_RESOURCE                    = 7, /*!< DMA resource. */
    MFX_RESOURCE_HDDLUNITE_REMOTE_MEMORY         = 8, /*!< HDDL Unite Remote memory handle. */
} mfxResourceType;

/*! Maximum allowed length of the implementation name. */
#define MFX_IMPL_NAME_LEN         32
/*! Maximum allowed length of the implementation name. */
#define MFX_STRFIELD_LEN          128

#ifdef ONEVPL_EXPERIMENTAL

#define MFX_DECEXTDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxDecExtDescription structure represents the extended description of a decoder. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16 reserved[14];                            /*!< Reserved for future use. */
    mfxU16 NumExtBufferIDs;                         /*!< Number of supported extended buffer IDs. */
    mfxU32* ExtBufferIDs;                           /*!< Pointer to the array of supported extended buffer IDs. */
} mfxDecExtDescription;
MFX_PACK_END()

#define MFX_DECMEMEXTDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxDecMemExtDescription structure represents the extended description for decoder memory. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16 reserved[13];                            /*!< Reserved for future use. */
    mfxU16 MaxBitDepth;                             /*!< Maximum supported bit depth. */
    mfxU16 NumChromaSubsamplings;                   /*!< Number of supported output chroma subsamplings. */
    mfxU16* ChromaSubsamplings;                     /*!< Pointer to the array of supported output chroma subsamplings. */
} mfxDecMemExtDescription;
MFX_PACK_END()

#endif

#ifdef ONEVPL_EXPERIMENTAL
#define MFX_DECODERDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 1)
#else
#define MFX_DECODERDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)
#endif

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxDecoderDescription structure represents the description of a decoder. */
typedef struct {
    mfxStructVersion Version;                            /*!< Version of the structure. */
    mfxU16 reserved[7];                                  /*!< Reserved for future use. */
    mfxU16 NumCodecs;                                    /*!< Number of supported decoders. */
    /*! This structure represents the decoder description. */
    struct decoder {
        mfxU32 CodecID;                                  /*!< Decoder ID in FourCC format. */
#ifdef ONEVPL_EXPERIMENTAL
        mfxU16 reserved[2];                              /*!< Reserved for future use. */
        union {
           mfxDecExtDescription* DecExtDesc;             /*!< Pointer to the extended descriptions of the decoder. */
           mfxU16 reserved2[4];                          /*!< Reserved for future use. */
        };
        mfxU16 reserved3[2];                             /*!< Reserved for future use. */
#else
        mfxU16 reserved[8];                              /*!< Reserved for future use. */
#endif
        mfxU16 MaxcodecLevel;                            /*!< Maximum supported codec level. See the CodecProfile enumerator for possible values. */
        mfxU16 NumProfiles;                              /*!< Number of supported profiles. */
        /*! This structure represents the codec profile description. */
        struct decprofile {
           mfxU32 Profile;                               /*!< Profile ID. See the CodecProfile enumerator for possible values.*/
           mfxU16 reserved[7];                           /*!< Reserved for future use. */
           mfxU16 NumMemTypes;                           /*!< Number of supported memory types. */
           /*! This structure represents the underlying details of the memory type. */
           struct decmemdesc {
              mfxResourceType MemHandleType;             /*!< Memory handle type. */
              mfxRange32U Width;                         /*!< Range of supported image widths. */
              mfxRange32U Height;                        /*!< Range of supported image heights. */
#ifdef ONEVPL_EXPERIMENTAL
              mfxU16 reserved[2];                        /*!< Reserved for future use. */
              union {
                 mfxDecMemExtDescription* MemExtDesc;    /*!< Pointer to the extended descriptions for decoder memory. */
                 mfxU16 reserved2[4];                    /*!< Reserved for future use. */
              };
              mfxU16 reserved3;                          /*!< Reserved for future use. */
#else
              mfxU16 reserved[7];                        /*!< Reserved for future use. */
#endif
              mfxU16 NumColorFormats;                    /*!< Number of supported output color formats. */
              mfxU32* ColorFormats;                      /*!< Pointer to the array of supported output color formats (in FOURCC). */
           } * MemDesc;                                  /*!< Pointer to the array of memory types. */
        } * Profiles;                                    /*!< Pointer to the array of profiles supported by the codec. */
    } * Codecs;                                          /*!< Pointer to the array of decoders. */
} mfxDecoderDescription;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL

#define MFX_ENCEXTDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxEncExtDescription structure represents the extended description of an encoder. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16 reserved[10];                            /*!< Reserved for future use. */
    mfxU16 NumRateControlMethods;                   /*!< Number of supported bitrate control methods. */
    mfxU16* RateControlMethods;                     /*!< Pointer to the array of supported bitrate control methods. */
    mfxU16 reserved2[11];                           /*!< Reserved for future use. */
    mfxU16 NumExtBufferIDs;                         /*!< Number of supported extended buffer IDs. */
    mfxU32* ExtBufferIDs;                           /*!< Pointer to the array of supported extended buffer IDs. */
} mfxEncExtDescription;
MFX_PACK_END()

#define MFX_ENCMEMEXTDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxEncMemExtDescription structure represents the extended description for encoder memory. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16 reserved[13];                            /*!< Reserved for future use. */
    mfxU16 TargetMaxBitDepth;                       /*!< Maximum supported bit depth. */
    mfxU16 NumTargetChromaSubsamplings;             /*!< Number of supported target chroma subsamplings. */
    mfxU16* TargetChromaSubsamplings;               /*!< Pointer to the array of supported target chroma subsamplings. */
} mfxEncMemExtDescription;
MFX_PACK_END()

#endif

#ifdef ONEVPL_EXPERIMENTAL
#define MFX_ENCODERDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 1)
#else
#define MFX_ENCODERDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)
#endif

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents an encoder description. */
typedef struct {
    mfxStructVersion Version;                            /*!< Version of the structure. */
    mfxU16 reserved[7];                                  /*!< Reserved for future use. */
    mfxU16 NumCodecs;                                    /*!< Number of supported encoders. */
    /*! This structure represents encoder description. */
    struct encoder {
        mfxU32 CodecID;                                  /*!< Encoder ID in FourCC format. */
        mfxU16 MaxcodecLevel;                            /*!< Maximum supported codec level. See the CodecProfile enumerator for possible values. */
        mfxU16 BiDirectionalPrediction;                  /*!< Indicates B-frames support. */
#ifdef ONEVPL_EXPERIMENTAL
        union {
           mfxEncExtDescription* EncExtDesc;             /*!< Pointer to the extended descriptions of the encoder. */
           mfxU16 reserved2[4];                          /*!< Reserved for future use. */
        };
        mfxU16 reserved[3];                              /*!< Reserved for future use. */
#else
        mfxU16 reserved[7];                              /*!< Reserved for future use. */
#endif
        mfxU16 NumProfiles;                              /*!< Number of supported profiles. */
        /*! This structure represents the codec profile description. */
        struct encprofile {
           mfxU32 Profile;                               /*!< Profile ID. See the CodecProfile enumerator for possible values.*/
           mfxU16 reserved[7];                           /*!< Reserved for future use. */
           mfxU16 NumMemTypes;                           /*!< Number of supported memory types. */
           /*! This structure represents the underlying details of the memory type. */
           struct encmemdesc {
              mfxResourceType MemHandleType;             /*!< Memory handle type. */
              mfxRange32U Width;                         /*!< Range of supported image widths. */
              mfxRange32U Height;                        /*!< Range of supported image heights. */
#ifdef ONEVPL_EXPERIMENTAL
              mfxU16 reserved[2];                        /*!< Reserved for future use. */
              union {
                 mfxEncMemExtDescription* MemExtDesc;    /*!< Pointer to the extended descriptions for encoder memory. */
                 mfxU16 reserved2[4];                    /*!< Reserved for future use. */
              };
              mfxU16 reserved3;                          /*!< Reserved for future use. */
#else
              mfxU16 reserved[7];                        /*!< Reserved for future use. */
#endif
              mfxU16 NumColorFormats;                    /*!< Number of supported input color formats. */
              mfxU32* ColorFormats;                      /*!< Pointer to the array of supported input color formats (in FOURCC). */
           } * MemDesc;                                  /*!< Pointer to the array of memory types. */
        } * Profiles;                                    /*!< Pointer to the array of profiles supported by the codec. */
    } * Codecs;                                          /*!< Pointer to the array of encoders. */
} mfxEncoderDescription;
MFX_PACK_END()

#define MFX_VPPDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents VPP description. */
typedef struct {
    mfxStructVersion Version;                            /*!< Version of the structure. */
    mfxU16 reserved[7];                                  /*!< Reserved for future use. */
    mfxU16 NumFilters;                                   /*!< Number of supported VPP filters. */
    /*! This structure represents the VPP filters description. */
    struct filter {
       mfxU32 FilterFourCC;                              /*!< Filter ID in FourCC format. */
       mfxU16 MaxDelayInFrames;                          /*!< Introduced output delay in frames. */
       mfxU16 reserved[7];                               /*!< Reserved for future use. */
       mfxU16 NumMemTypes;                               /*!< Number of supported memory types. */
       /*! This structure represents the underlying details of the memory type. */
       struct memdesc {
          mfxResourceType MemHandleType;                 /*!< Memory handle type. */
          mfxRange32U Width;                             /*!< Range of supported image widths. */
          mfxRange32U Height;                            /*!< Range of supported image heights. */
          mfxU16 reserved[7];                            /*!< Reserved for future use. */
          mfxU16 NumInFormats;                           /*!< Number of supported input color formats. */
          /*! This structure represents the input color format description. */
          struct format {
             mfxU32 InFormat;                            /*!< Input color in FourCC format. */
             mfxU16 reserved[5];                         /*!< Reserved for future use. */
             mfxU16 NumOutFormat;                        /*!< Number of supported output color formats. */
             mfxU32* OutFormats;                         /*!< Pointer to the array of supported output color formats (in FOURCC). */
          } * Formats;                                   /*!< Pointer to the array of supported formats. */
       } * MemDesc;                                      /*!< Pointer to the array of memory types. */
    } * Filters;                                         /*!< Pointer to the array of supported filters. */
} mfxVPPDescription;
MFX_PACK_END()

/*! The current version of mfxDeviceDescription structure. */
#define MFX_DEVICEDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 1)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents device description. */
typedef struct {
    mfxStructVersion Version;                            /*!< Version of the structure. */
    mfxU16 reserved[6];                                  /*!< reserved for future use. */
    mfxU16 MediaAdapterType; /*!< Graphics adapter type. See the mfxMediaAdapterType enumerator for a list of possible values. */
    mfxChar DeviceID[MFX_STRFIELD_LEN];                  /*!< Null terminated string with device ID. */
    mfxU16 NumSubDevices;                                /*!< Number of available uniform sub-devices. Pure software implementation can report 0. */
    /*! This structure represents sub-device description. */
    struct subdevices {
       mfxU32 Index;                                     /*!< Index of the sub-device, started from 0 and increased by 1.*/
       mfxChar SubDeviceID[MFX_STRFIELD_LEN];            /*!< Null terminated string with unique sub-device ID, mapped to the system ID. */
       mfxU32 reserved[7];                               /*!< reserved for future use. */
    } * SubDevices;                                      /*!< Pointer to the array of available sub-devices. */
} mfxDeviceDescription;
MFX_PACK_END()

/*! This enum itemizes implementation type. */
typedef enum {
    MFX_IMPL_TYPE_SOFTWARE = 0x0001,  /*!< Pure Software Implementation. */
    MFX_IMPL_TYPE_HARDWARE = 0x0002,  /*!< Hardware Accelerated Implementation. */
} mfxImplType;

/*! This enum itemizes hardware acceleration stack to use. */
typedef enum {
    MFX_ACCEL_MODE_NA           = 0,       /*!< Hardware acceleration is not applicable. */
    MFX_ACCEL_MODE_VIA_D3D9     = 0x0200,  /*!< Hardware acceleration goes through the Microsoft* Direct3D9* infrastructure. */
    MFX_ACCEL_MODE_VIA_D3D11    = 0x0300,  /*!< Hardware acceleration goes through the Microsoft* Direct3D11* infrastructure. */
    MFX_ACCEL_MODE_VIA_VAAPI    = 0x0400,  /*!< Hardware acceleration goes through the Linux* VA-API infrastructure. */
    MFX_ACCEL_MODE_VIA_VAAPI_DRM_RENDER_NODE
                       = MFX_ACCEL_MODE_VIA_VAAPI, /*!< Hardware acceleration goes through the Linux* VA-API infrastructure with DRM RENDER MODE as default acceleration access point. */
    MFX_ACCEL_MODE_VIA_VAAPI_DRM_MODESET = 0x0401, /*!< Hardware acceleration goes through the Linux* VA-API infrastructure with DRM MODESET as  default acceleration access point. */
    MFX_ACCEL_MODE_VIA_VAAPI_GLX         = 0x0402, /*!< Hardware acceleration goes through the Linux* VA-API infrastructure with OpenGL Extension to the X Window System
                                                        as default acceleration access point. */
    MFX_ACCEL_MODE_VIA_VAAPI_X11     = 0x0403, /*!< Hardware acceleration goes through the Linux* VA-API infrastructure with X11 as default acceleration access point. */
    MFX_ACCEL_MODE_VIA_VAAPI_WAYLAND = 0x0404, /*!< Hardware acceleration goes through the Linux* VA-API infrastructure with Wayland as default acceleration access point. */
    MFX_ACCEL_MODE_VIA_HDDLUNITE     = 0x0500, /*!< Hardware acceleration goes through the HDDL* Unite*. */
} mfxAccelerationMode;

#define MFX_ACCELERATIONMODESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents acceleration modes description. */
typedef struct {
    mfxStructVersion Version;                            /*!< Version of the structure. */
    mfxU16 reserved[2];                                  /*!< reserved for future use. */
    mfxU16 NumAccelerationModes;                         /*!< Number of supported acceleration modes. */
    mfxAccelerationMode* Mode;                           /*!< Pointer to the array of supported acceleration modes. */
} mfxAccelerationModeDescription;
MFX_PACK_END()

/*! Specifies the surface pool allocation policies. */
 typedef enum {
    /*! Recommends to limit max pool size by sum of requested surfaces asked by components. */
    MFX_ALLOCATION_OPTIMAL = 0,

    /*! Dynamic allocation with no limit. */
    MFX_ALLOCATION_UNLIMITED   = 1,

    /*! Max pool size is limited by NumberToPreAllocate + DeltaToAllocateOnTheFly. */
    MFX_ALLOCATION_LIMITED     = 2,

} mfxPoolAllocationPolicy;

/*! The current version of mfxPoolPolicyDescription structure. */
#define MFX_POOLPOLICYDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents pool policy description. */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16 reserved[2];                             /*!< reserved for future use. */
    mfxU16 NumPoolPolicies;                         /*!< Number of supported pool policies. */
    mfxPoolAllocationPolicy* Policy;                /*!< Pointer to the array of supported pool policies. */
} mfxPoolPolicyDescription;
MFX_PACK_END()

/*! The current version of mfxImplDescription structure. */
#define MFX_IMPLDESCRIPTION_VERSION MFX_STRUCT_VERSION(1, 2)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents the implementation description. */
typedef struct {
    mfxStructVersion       Version;                      /*!< Version of the structure. */
    mfxImplType            Impl;                         /*!< Impl type: software/hardware. */
    mfxAccelerationMode    AccelerationMode;             /*!< Default Hardware acceleration stack to use. OS dependent parameter. Use VA for Linux* and DX* for Windows*. */
    mfxVersion             ApiVersion;                   /*!< Supported API version. */
    mfxChar                ImplName[MFX_IMPL_NAME_LEN];  /*!< Null-terminated string with implementation name given by vendor. */
    mfxChar                License[MFX_STRFIELD_LEN];    /*!< Null-terminated string with comma-separated list of license names of the implementation. */
    mfxChar                Keywords[MFX_STRFIELD_LEN];   /*!< Null-terminated string with comma-separated list of keywords specific to this implementation that dispatcher can search for. */
    mfxU32                 VendorID;                     /*!< Standard vendor ID 0x8086 - Intel. */
    mfxU32                 VendorImplID;                 /*!< Vendor specific number with given implementation ID. */
    mfxDeviceDescription   Dev;                          /*!< Supported device. */
    mfxDecoderDescription  Dec;                          /*!< Decoder configuration. */
    mfxEncoderDescription  Enc;                          /*!< Encoder configuration. */
    mfxVPPDescription      VPP;                          /*!< VPP configuration. */
    union
    {
        mfxAccelerationModeDescription   AccelerationModeDescription; /*!< Supported acceleration modes. */
        mfxU32 reserved3[4];
    };
    mfxPoolPolicyDescription  PoolPolicies;                /*!< Supported surface pool polices. */
    mfxU32                    reserved[8];                 /*!< Reserved for future use. */
    mfxU32                    NumExtParam;                 /*!< Number of extension buffers. Reserved for future use. Must be 0. */
    union {
        mfxExtBuffer **ExtParam;                         /*!< Array of extension buffers. */
        mfxU64       Reserved2;                          /*!< Reserved for future use. */
    } ExtParams;                                         /*!< Extension buffers. Reserved for future. */
} mfxImplDescription;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This structure represents the list of names of implemented functions. */
typedef struct {
    mfxU16   NumFunctions;     /*!< Number of function names in the FunctionsName array. */
    mfxChar** FunctionsName;   /*!< Array of the null-terminated strings. Each string contains name of the implemented function. */
} mfxImplementedFunctions;
MFX_PACK_END()


#define MFX_EXTENDEDDEVICEID_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Specifies various physical device properties for device matching and identification outside of oneAPI Video Processing Library (oneVPL). */
typedef struct {
    mfxStructVersion Version;                       /*!< Version of the structure. */
    mfxU16           VendorID;                      /*!< PCI vendor ID. */
    mfxU16           DeviceID;                      /*!< PCI device ID. */
    mfxU32           PCIDomain;                     /*!< PCI bus domain. Equals to '0' if OS doesn't support it or
                                                         has sequential numbering of buses across domains. */
    mfxU32           PCIBus;                        /*!< The number of the bus that the physical device is located on. */
    mfxU32           PCIDevice;                     /*!< The index of the physical device on the bus. */
    mfxU32           PCIFunction;                   /*!< The function number of the device on the physical device. */
    mfxU8            DeviceLUID[8];                 /*!< LUID of DXGI adapter. */
    mfxU32           LUIDDeviceNodeMask;            /*!< Bitfield identifying the node within a linked
                                                         device adapter corresponding to the device. */
    mfxU32           LUIDValid;                     /*!< Boolean value that will be 1 if DeviceLUID contains a valid LUID
                                                         and LUIDDeviceNodeMask contains a valid node mask,
                                                         and 0 if they do not. */
    mfxU32           DRMRenderNodeNum;              /*!< Number of the DRM render node from the path /dev/dri/RenderD\<num\>.
                                                         Value equals to 0 means that this field doesn't contain valid DRM Render
                                                         Node number.*/
    mfxU32           DRMPrimaryNodeNum;             /*!< Number of the DRM primary node from the path /dev/dri/card\<num\>.
                                                         Value equals to 0x7FFFFFFF means that this field doesn't contain valid DRM Primary
                                                         Node number.*/
    mfxU16           RevisionID;                    /*!< PCI revision ID. The value contains microarchitecture version. */
    mfxU8            reserved1[18];                 /*!< Reserved for future use. */
    mfxChar          DeviceName[MFX_STRFIELD_LEN];  /*!< Null-terminated string in utf-8 with the name of the device. */
} mfxExtendedDeviceId;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Cross domain structure to define device UUID. It is defined here to check backward compatibility.*/
typedef struct {
      mfxU16 vendor_id;                             /*!< PCI vendor ID. Same as mfxExtendedDeviceId::VendorID. */
      mfxU16 device_id;                             /*!< PCI device ID. Same as mfxExtendedDeviceId::DeviceID. */
      mfxU16 revision_id;                           /*!< PCI revision ID. Same as mfxExtendedDeviceId::RevisionID. */
      mfxU16 pci_domain;                            /*!< PCI bus domain. Same as mfxExtendedDeviceId::PCIDomain. */
      mfxU8  pci_bus;                               /*!< The number of the bus that the physical device is located on. Same as mfxExtendedDeviceId::PCIBus. */
      mfxU8  pci_dev;                               /*!< The index of the physical device on the bus. Same as mfxExtendedDeviceId::PCIDevice. */
      mfxU8  pci_func;                              /*!< The function number of the device on the physical device.  Same as mfxExtendedDeviceId::PCIFunction. */
      mfxU8  reserved[4];                           /*!< Reserved for future use. */
      mfxU8  sub_device_id;                         /*!< SubDevice ID.*/
} extDeviceUUID;
MFX_PACK_END()


/*! The mfxImplCapsDeliveryFormat enumerator specifies delivery format of the implementation capability. */
typedef enum {
    MFX_IMPLCAPS_IMPLDESCSTRUCTURE       = 1,  /*!< Deliver capabilities as mfxImplDescription structure. */
    MFX_IMPLCAPS_IMPLEMENTEDFUNCTIONS    = 2,  /*!< Deliver capabilities as mfxImplementedFunctions structure. */
    MFX_IMPLCAPS_IMPLPATH                = 3,  /*!< Deliver pointer to the null-terminated string with the path to the
                                                    implementation. String is delivered in a form of buffer of
                                                    mfxChar type. */
    MFX_IMPLCAPS_DEVICE_ID_EXTENDED      = 4,  /*!< Deliver extended device ID information as mfxExtendedDeviceId
                                                    structure.*/
#ifdef ONEVPL_EXPERIMENTAL
    MFX_IMPLCAPS_SURFACE_TYPES           = 5,  /*!< Deliver capabilities as mfxSurfaceTypesSupported structure. */
#endif
} mfxImplCapsDeliveryFormat;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Specifies initialization parameters for API version starting from 2.0.
*/
typedef struct {
    mfxAccelerationMode    AccelerationMode; /*!< Hardware acceleration stack to use. OS dependent parameter. Use VA for Linux*, DX* for Windows* or HDDL. */
#ifdef ONEVPL_EXPERIMENTAL
    mfxU16  DeviceCopy;                      /*!< Enables or disables device's accelerated copying between device and
                                                  host. See the GPUCopy enumerator for a list of valid values.
                                                  This parameter is the equivalent of mfxInitParam::GPUCopy. */
    mfxU16  reserved[2];                     /*!< Reserved for future use. */
#else
    mfxU16  reserved[3];                     /*!< Reserved for future use. */
#endif
    mfxU16  NumExtParam;                     /*!< The number of extra configuration structures attached to this
                                                  structure. */
    mfxExtBuffer **ExtParam;                 /*!< Points to an array of pointers to the extra configuration structures;
                                                  see the ExtendedBufferID enumerator for a list of extended
                                                  configurations. */
    mfxU32      VendorImplID;                /*!< Vendor specific number with given implementation ID. Represents
                                                  the same field from mfxImplDescription. */
    mfxU32      reserved2[3];                /*!< Reserved for future use. */
} mfxInitializationParam;
MFX_PACK_END()

#ifdef ONEVPL_EXPERIMENTAL
MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Represents a name/value pair to indicate requested properties. For use with MFXQueryImplsProperties() */
typedef struct {
    mfxU8* PropName;       /*!< Property name string to indicate the requested Property. */
    mfxVariant PropVar;    /*!< Property value corresponding to the property name. */
} mfxQueryProperty;
MFX_PACK_END()
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
