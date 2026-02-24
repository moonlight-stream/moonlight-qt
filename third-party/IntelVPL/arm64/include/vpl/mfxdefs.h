/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXDEFS_H__
#define __MFXDEFS_H__

#define MFX_VERSION_MAJOR 2
#define MFX_VERSION_MINOR 16

// MFX_VERSION - version of API that 'assumed' by build may be provided externally
// if it omitted then latest stable API derived from Major.Minor is assumed


#if !defined(MFX_VERSION)
    #define MFX_VERSION (MFX_VERSION_MAJOR * 1000 + MFX_VERSION_MINOR)
#else
  #undef MFX_VERSION_MAJOR
  #define MFX_VERSION_MAJOR ((MFX_VERSION) / 1000)

  #undef MFX_VERSION_MINOR
  #define MFX_VERSION_MINOR ((MFX_VERSION) % 1000)
#endif

/*! The corresponding version of the Intel(r) Media SDK legacy API that is used as a basis
   for the current API. */

#define MFX_LEGACY_VERSION 1035


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* In preprocessor syntax # symbol has stringize meaning,
   so to expand some macro to preprocessor pragma we need to use
   special compiler dependent construction */

#if defined(_MSC_VER)
    #define MFX_PRAGMA_IMPL(x) __pragma(x)
#else
    #define MFX_PRAGMA_IMPL(x) _Pragma(#x)
#endif

#define MFX_PACK_BEGIN_X(x) MFX_PRAGMA_IMPL(pack(push, x))
#define MFX_PACK_END()      MFX_PRAGMA_IMPL(pack(pop))

/* The general rule for alignment is following:
   - structures with pointers have 4/8 bytes alignment on 32/64 bit systems
   - structures with fields of type mfxU64/mfxF64 (unsigned long long / double)
     have alignment 8 bytes on 64 bit and 32 bit Windows, on Linux alignment is 4 bytes
   - all the rest structures are 4 bytes aligned
   - there are several exceptions: some structs which had 4-byte alignment were extended
     with pointer / long type fields; such structs have 4-byte alignment to keep binary
     compatibility with previously release API */

#define MFX_PACK_BEGIN_USUAL_STRUCT()        MFX_PACK_BEGIN_X(4)

/* 64-bit LP64 data model */
#if defined(_WIN64) || defined(__LP64__)
    #define MFX_PACK_BEGIN_STRUCT_W_PTR()    MFX_PACK_BEGIN_X(8)
    #define MFX_PACK_BEGIN_STRUCT_W_L_TYPE() MFX_PACK_BEGIN_X(8)
/* 32-bit ILP32 data model Windows* (Intel(r) architecture) */
#elif defined(_WIN32) || defined(_M_IX86) && !defined(__linux__)
    #define MFX_PACK_BEGIN_STRUCT_W_PTR()    MFX_PACK_BEGIN_X(4)
    #define MFX_PACK_BEGIN_STRUCT_W_L_TYPE() MFX_PACK_BEGIN_X(8)
/* 32-bit ILP32 data model Linux* */
#elif defined(__ILP32__) || defined(__arm__)
    #define MFX_PACK_BEGIN_STRUCT_W_PTR()    MFX_PACK_BEGIN_X(4)
    #define MFX_PACK_BEGIN_STRUCT_W_L_TYPE() MFX_PACK_BEGIN_X(4)
#else
    #error Unknown packing
#endif

#ifdef _WIN32
  #define MFX_CDECL __cdecl
  #define MFX_STDCALL __stdcall
#else
  #define MFX_CDECL
  #define MFX_STDCALL
#endif /* _WIN32 */

#define MFX_INFINITE 0xFFFFFFFF

#ifndef MFX_DEPRECATED_OFF
   #if defined(__cplusplus) && __cplusplus >= 201402L
     #define MFX_DEPRECATED [[deprecated]]
     #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg [[deprecated]]
     #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
   #elif defined(__clang__)
     #define MFX_DEPRECATED __attribute__((deprecated))
     #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg __attribute__((deprecated))
     #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
   #elif defined(__INTEL_COMPILER)
     #if (defined(_WIN32) || defined(_WIN64))
       #define MFX_DEPRECATED __declspec(deprecated)
       #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg
       #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg) __pragma(deprecated(arg))
     #elif defined(__linux__)
       #define MFX_DEPRECATED __attribute__((deprecated))
       #if defined(__cplusplus)
         #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg __attribute__((deprecated))
       #else
         #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg
       #endif
       #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
     #endif
   #elif defined(_MSC_VER) && _MSC_VER > 1200 // VS 6 doesn't support deprecation
     #define MFX_DEPRECATED __declspec(deprecated)
     #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg
     #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg) __pragma(deprecated(arg))
   #elif defined(__GNUC__)
     #define MFX_DEPRECATED __attribute__((deprecated))
     #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg __attribute__((deprecated))
     #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
   #else
     #define MFX_DEPRECATED
     #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg
     #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
   #endif
 #else
   #define MFX_DEPRECATED
   #define MFX_DEPRECATED_ENUM_FIELD_INSIDE(arg) arg
   #define MFX_DEPRECATED_ENUM_FIELD_OUTSIDE(arg)
 #endif

typedef unsigned char       mfxU8;         /*!< Unsigned integer, 8 bit type. */
typedef char                mfxI8;         /*!< Signed integer, 8 bit type. */
typedef short               mfxI16;        /*!< Signed integer, 16 bit type. */
typedef unsigned short      mfxU16;        /*!< Unsigned integer, 16 bit type. */
typedef unsigned int        mfxU32;        /*!< Unsigned integer, 32 bit type. */
typedef int                 mfxI32;        /*!< Signed integer, 32 bit type. */
#if defined( _WIN32 ) || defined ( _WIN64 )
typedef unsigned long       mfxUL32;       /*!< Unsigned integer, 32 bit type. */
typedef long                mfxL32;        /*!< Signed integer, 32 bit type. */
#else
typedef unsigned int        mfxUL32;       /*!< Unsigned integer, 32 bit type. */
typedef int                 mfxL32;        /*!< Signed integer, 32 bit type. */
#endif
typedef float               mfxF32;        /*!< Single-precision floating point, 32 bit type. */
typedef double              mfxF64;        /*!< Double-precision floating point, 64 bit type. */
typedef unsigned long long  mfxU64;        /*!< Unsigned integer, 64 bit type. */
typedef long long           mfxI64;        /*!< Signed integer, 64 bit type. */
typedef void*               mfxHDL;        /*!< Handle type. */
typedef mfxHDL              mfxMemId;      /*!< Memory ID type. */
typedef void*               mfxThreadTask; /*!< Thread task type. */
typedef char                mfxChar;       /*!< UTF-8 byte. */
typedef unsigned short      mfxFP16;       /*!< Half precision floating point, 16 bit type. */

/* MFX structures version info */
MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Introduce the field Version for any structure.
Assumed that any structure changes are backward binary compatible.
 mfxStructVersion starts from {1,0} for any new API structures. If mfxStructVersion is
 added to the existent legacy structure (replacing reserved fields) it starts from {1, 1}.
*/
typedef union {
    /*! Structure with Major and Minor fields.  */
    /*! @struct Anonymous */
    struct {
      /*! @{
      @name Major and Minor fields
      Anonymous structure with Major and Minor fields. Minor number is incremented when reserved fields are used. Major number is incremented when the size of structure is increased. */
        mfxU8  Minor; /*!< Minor number of the correspondent structure. */
        mfxU8  Major; /*!< Major number of the correspondent structure. */
      /*! @} */
    };
    mfxU16  Version;   /*!< Structure version number. */
} mfxStructVersion;
MFX_PACK_END()

#define MFX_STRUCT_VERSION(MAJOR, MINOR) (256*(MAJOR) + (MINOR))

#define MFX_VARIANT_VERSION MFX_STRUCT_VERSION(1, 1)

/*! The mfxDataType enumerates data type for mfxDataType. */
typedef enum {
    MFX_DATA_TYPE_UNSET   = 0,            /*!< Undefined type. */
    MFX_DATA_TYPE_U8,                     /*!< 8-bit unsigned integer. */
    MFX_DATA_TYPE_I8,                     /*!< 8-bit signed integer. */
    MFX_DATA_TYPE_U16,                    /*!< 16-bit unsigned integer. */
    MFX_DATA_TYPE_I16,                    /*!< 16-bit signed integer. */
    MFX_DATA_TYPE_U32,                    /*!< 32-bit unsigned integer. */
    MFX_DATA_TYPE_I32,                    /*!< 32-bit signed integer. */
    MFX_DATA_TYPE_U64,                    /*!< 64-bit unsigned integer. */
    MFX_DATA_TYPE_I64,                    /*!< 64-bit signed integer. */
    MFX_DATA_TYPE_F32,                    /*!< 32-bit single precision floating point. */
    MFX_DATA_TYPE_F64,                    /*!< 64-bit double precision floating point. */
    MFX_DATA_TYPE_PTR,                    /*!< Generic type pointer. */
    MFX_DATA_TYPE_FP16,                   /*!< 16-bit half precision floating point. */
}mfxDataType;

/*! The mfxVariantType enumerator data types for mfxVariantType. */
typedef enum {
    MFX_VARIANT_TYPE_UNSET = MFX_DATA_TYPE_UNSET,                        /*!< Undefined type. */
    MFX_VARIANT_TYPE_U8    = MFX_DATA_TYPE_U8,                           /*!< 8-bit unsigned integer. */
    MFX_VARIANT_TYPE_I8    = MFX_DATA_TYPE_I8,                           /*!< 8-bit signed integer. */
    MFX_VARIANT_TYPE_U16   = MFX_DATA_TYPE_U16,                          /*!< 16-bit unsigned integer. */
    MFX_VARIANT_TYPE_I16   = MFX_DATA_TYPE_I16,                          /*!< 16-bit signed integer. */
    MFX_VARIANT_TYPE_U32   = MFX_DATA_TYPE_U32,                          /*!< 32-bit unsigned integer. */
    MFX_VARIANT_TYPE_I32   = MFX_DATA_TYPE_I32,                          /*!< 32-bit signed integer. */
    MFX_VARIANT_TYPE_U64   = MFX_DATA_TYPE_U64,                          /*!< 64-bit unsigned integer. */
    MFX_VARIANT_TYPE_I64   = MFX_DATA_TYPE_I64,                          /*!< 64-bit signed integer. */
    MFX_VARIANT_TYPE_F32   = MFX_DATA_TYPE_F32,                          /*!< 32-bit single precision floating point. */
    MFX_VARIANT_TYPE_F64   = MFX_DATA_TYPE_F64,                          /*!< 64-bit double precision floating point. */
    MFX_VARIANT_TYPE_PTR   = MFX_DATA_TYPE_PTR,                          /*!< Generic type pointer. */
    MFX_VARIANT_TYPE_FP16  = MFX_DATA_TYPE_FP16,                         /*!< 16-bit half precision floating point. */

#ifdef ONEVPL_EXPERIMENTAL
    MFX_VARIANT_TYPE_QUERY = 0x00000100,                                 /*!< Bitmask to OR with other variant types when using property-based query API */
#endif
} mfxVariantType;

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! The mfxVariantType enumerator data types for mfxVariant type. */
typedef struct {
    mfxStructVersion Version;    /*!< Version of the structure. */
    mfxVariantType   Type;       /*!< Value type. */
    /*! Value data holder. */
    union data {
        mfxU8   U8; /*!< mfxU8 data. */
        mfxI8   I8; /*!< mfxI8 data. */
        mfxU16 U16; /*!< mfxU16 data. */
        mfxI16 I16; /*!< mfxI16 data. */
        mfxU32 U32; /*!< mfxU32 data. */
        mfxI32 I32; /*!< mfxI32 data. */
        mfxU64 U64; /*!< mfxU64 data. */
        mfxI64 I64; /*!< mfxI64 data. */
        mfxF32 F32; /*!< mfxF32 data. */
        mfxF64 F64; /*!< mfxF64 data. */
        mfxFP16 FP16; /*!< mfxFP16 data. */
        mfxHDL Ptr; /*!< Pointer. When this points to a string the string must be null terminated. */
    } Data;         /*!< Value data member. */
} mfxVariant;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Represents a range of unsigned values. */
typedef struct {
    mfxU32 Min;  /*!< Minimal value of the range. */
    mfxU32 Max;  /*!< Maximal value of the range. */
    mfxU32 Step; /*!< Value increment. */
} mfxRange32U;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Represents a pair of numbers of type mfxI16. */
typedef struct {
    mfxI16  x; /*!< First number. */
    mfxI16  y; /*!< Second number. */
} mfxI16Pair;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Represents pair of handles of type mfxHDL. */
typedef struct {
    mfxHDL first;  /*!< First handle. */
    mfxHDL second; /*!< Second handle. */
} mfxHDLPair;
MFX_PACK_END()

/*********************************************************************************\
Error message
\*********************************************************************************/
/*! @enum mfxStatus Itemizes status codes returned by API functions. */
typedef enum
{
    /* no error */
    MFX_ERR_NONE                        = 0,    /*!< No error. */
    /* reserved for unexpected errors */
    MFX_ERR_UNKNOWN                     = -1,   /*!< Unknown error. */

    /* error codes <0 */
    MFX_ERR_NULL_PTR                    = -2,   /*!< Null pointer. */
    MFX_ERR_UNSUPPORTED                 = -3,   /*!< Unsupported feature. */
    MFX_ERR_MEMORY_ALLOC                = -4,   /*!< Failed to allocate memory. */
    MFX_ERR_NOT_ENOUGH_BUFFER           = -5,   /*!< Insufficient buffer at input/output. */
    MFX_ERR_INVALID_HANDLE              = -6,   /*!< Invalid handle. */
    MFX_ERR_LOCK_MEMORY                 = -7,   /*!< Failed to lock the memory block. */
    MFX_ERR_NOT_INITIALIZED             = -8,   /*!< Member function called before initialization. */
    MFX_ERR_NOT_FOUND                   = -9,   /*!< The specified object is not found. */
    MFX_ERR_MORE_DATA                   = -10,  /*!< Expect more data at input. */
    MFX_ERR_MORE_SURFACE                = -11,  /*!< Expect more surface at output. */
    MFX_ERR_ABORTED                     = -12,  /*!< Operation aborted. */
    MFX_ERR_DEVICE_LOST                 = -13,  /*!< Lose the hardware acceleration device. */
    MFX_ERR_INCOMPATIBLE_VIDEO_PARAM    = -14,  /*!< Incompatible video parameters. */
    MFX_ERR_INVALID_VIDEO_PARAM         = -15,  /*!< Invalid video parameters. */
    MFX_ERR_UNDEFINED_BEHAVIOR          = -16,  /*!< Undefined behavior. */
    MFX_ERR_DEVICE_FAILED               = -17,  /*!< Device operation failure. */
    MFX_ERR_MORE_BITSTREAM              = -18,  /*!< Expect more bitstream buffers at output. */
    MFX_ERR_GPU_HANG                    = -21,  /*!< Device operation failure caused by GPU hang. */
    MFX_ERR_REALLOC_SURFACE             = -22,  /*!< Bigger output surface required. */
    MFX_ERR_RESOURCE_MAPPED             = -23,  /*!< Write access is already acquired and user requested
                                                   another write access, or read access with MFX_MEMORY_NO_WAIT flag. */
    MFX_ERR_NOT_IMPLEMENTED             = -24,   /*!< Feature or function not implemented. */
    MFX_ERR_MORE_EXTBUFFER              = -25,   /*!< Expect additional extended configuration buffer. */

    /* warnings >0 */
    MFX_WRN_IN_EXECUTION                = 1,    /*!< The previous asynchronous operation is in execution. */
    MFX_WRN_DEVICE_BUSY                 = 2,    /*!< The hardware acceleration device is busy. */
    MFX_WRN_VIDEO_PARAM_CHANGED         = 3,    /*!< The video parameters are changed during decoding. */
    MFX_WRN_PARTIAL_ACCELERATION        = 4,    /*!< Software acceleration is used. */
    MFX_WRN_INCOMPATIBLE_VIDEO_PARAM    = 5,    /*!< Incompatible video parameters. */
    MFX_WRN_VALUE_NOT_CHANGED           = 6,    /*!< The value is saturated based on its valid range. */
    MFX_WRN_OUT_OF_RANGE                = 7,    /*!< The value is out of valid range. */
    MFX_WRN_FILTER_SKIPPED              = 10,   /*!< One of requested filters has been skipped. */
    /* low-delay partial output */
    MFX_ERR_NONE_PARTIAL_OUTPUT         = 12,   /*!< Frame is not ready, but bitstream contains partial output. */

    MFX_WRN_ALLOC_TIMEOUT_EXPIRED       = 13,   /*!< Timeout expired for internal frame allocation. */

    /* threading statuses */
    MFX_TASK_DONE = MFX_ERR_NONE,               /*!< Task has been completed. */
    MFX_TASK_WORKING                    = 8,    /*!< There is some more work to do. */
    MFX_TASK_BUSY                       = 9,    /*!< Task is waiting for resources. */

    /* plug-in statuses */
    MFX_ERR_MORE_DATA_SUBMIT_TASK       = -10000, /*!< Return MFX_ERR_MORE_DATA but submit internal asynchronous task. */

} mfxStatus;


MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Represents Globally Unique Identifier (GUID) with memory layout
    compliant to RFC 4122. See https://www.rfc-editor.org/info/rfc4122 for details. */
typedef struct
{
    mfxU8 Data[16]; /*!< Array to keep GUID. */
} mfxGUID;
MFX_PACK_END()



// Application
#if defined(MFX_DISPATCHER_EXPOSED_PREFIX)

#include "mfxdispatcherprefixedfunctions.h"

#endif // MFX_DISPATCHER_EXPOSED_PREFIX


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MFXDEFS_H__ */
