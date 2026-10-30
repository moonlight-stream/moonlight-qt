/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXPCP_H__
#define __MFXPCP_H__
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*! The Protected enumerator describes the protection schemes. */
enum {
    MFX_PROTECTION_CENC_WV_CLASSIC      = 0x0004, /*!< The protection scheme is based on the Widevine* DRM from Google*. */
    MFX_PROTECTION_CENC_WV_GOOGLE_DASH  = 0x0005, /*!< The protection scheme is based on the Widevine* Modular DRM* from Google*. */
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_CENC_PARAM          = MFX_MAKEFOURCC('C','E','N','P') /*!< This structure is used to pass decryption status report index for Common
                                                                           Encryption usage model. See the mfxExtCencParam structure for more details. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to pass the decryption status report index for the Common Encryption usage model. The application can
   attach this extended buffer to the mfxBitstream structure at runtime.
*/
typedef struct _mfxExtCencParam{
    mfxExtBuffer Header;      /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CENC_PARAM. */

    mfxU32 StatusReportIndex; /*!< Decryption status report index. */
    mfxU32 reserved[15];
} mfxExtCencParam;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif
