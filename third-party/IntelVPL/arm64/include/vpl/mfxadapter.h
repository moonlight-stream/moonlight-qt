/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "mfxdefs.h"
#ifndef __MFXADAPTER_H__
#define __MFXADAPTER_H__

#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*!
   @brief
    Returns a list of adapters that are suitable to handle workload @p input_info. The list is sorted in priority order, with iGPU given the highest precedence.
    This rule may change in the future. If the @p input_info pointer is NULL, the list of all available adapters will be returned.

   @param[in] input_info Pointer to workload description. See mfxComponentInfo description for details.
   @param[out] adapters  Pointer to output description of all suitable adapters for input workload. See mfxAdaptersInfo description for details.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NULL_PTR
   @p input_info or adapters pointer is NULL. \n
   MFX_ERR_NOT_FOUND  No suitable adapters found. \n
   MFX_WRN_OUT_OF_RANGE  Not enough memory to report back entire list of adapters. In this case as many adapters as possible will be returned.

   @since This function is available since API version 1.31.

   @deprecated Deprecated in API version 2.9. Use MFXEnumImplementations and MFXSetConfigFilterProperty to query adapter capabilities and
               to select a suitable adapter for the input workload.
               Use MFX_DEPRECATED_OFF macro to turn off the deprecation message visualization.
*/
MFX_DEPRECATED mfxStatus MFX_CDECL MFXQueryAdapters(mfxComponentInfo* input_info, mfxAdaptersInfo* adapters);

/*!
   @brief
    Returns list of adapters that are suitable to decode the input bitstream. The list is sorted in priority order, with iGPU given the highest precedence. This rule may change in the future. This function is a simplification of MFXQueryAdapters, because bitstream is a description of the workload itself.

   @param[in]  bitstream Pointer to bitstream with input data.
   @param[in]  codec_id  Codec ID to determine the type of codec for the input bitstream.
   @param[out] adapters  Pointer to the output list of adapters. Memory should be allocated by user. See mfxAdaptersInfo description for details.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NULL_PTR  bitstream or @p adapters pointer is NULL. \n
   MFX_ERR_NOT_FOUND No suitable adapters found. \n
   MFX_WRN_OUT_OF_RANGE Not enough memory to report back entire list of adapters. In this case as many adapters as possible will be returned.

   @since This function is available since API version 1.31.

   @deprecated Deprecated in API version 2.9. Use MFXEnumImplementations and MFXSetConfigFilterProperty to query adapter capabilities and
               to select a suitable adapter for the input workload.
               Use MFX_DEPRECATED_OFF macro to turn off the deprecation message visualization.
*/
MFX_DEPRECATED mfxStatus MFX_CDECL MFXQueryAdaptersDecode(mfxBitstream* bitstream, mfxU32 codec_id, mfxAdaptersInfo* adapters);

/*!
   @brief
    Returns the number of detected graphics adapters. It can be used before calling MFXQueryAdapters  to determine the size of input data that the user will need to allocate.

   @param[out] num_adapters Pointer for the output number of detected graphics adapters.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NULL_PTR  num_adapters pointer is NULL.

   @since This function is available since API version 1.31.

   @deprecated Deprecated in API version 2.9. Use MFXEnumImplementations and MFXSetConfigFilterProperty to query adapter capabilities and
               to select a suitable adapter for the input workload.
               Use MFX_DEPRECATED_OFF macro to turn off the deprecation message visualization.
*/
MFX_DEPRECATED mfxStatus MFX_CDECL MFXQueryAdaptersNumber(mfxU32* num_adapters);
#ifdef __cplusplus
} // extern "C"
#endif

#endif // __MFXADAPTER_H__

