/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "mfxdefs.h"

#ifndef __MFXIMPLCAPS_H__
#define __MFXIMPLCAPS_H__

#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*!
   @brief
      Delivers implementation capabilities in the requested format according to the format value. Calling this
      function directly is not recommended. Instead, applications must call the MFXEnumImplementations function.

   @param[in]  format      Format in which capabilities must be delivered. See mfxImplCapsDeliveryFormat for more details.
   @param[out] num_impls   Number of the implementations.

   @return
      Array of handles to the capability report or NULL in case of unsupported format or NULL num_impls pointer.
      Length of array is equal to num_impls.

   @since This function is available since API version 2.0.
*/
mfxHDL* MFX_CDECL MFXQueryImplsDescription(mfxImplCapsDeliveryFormat format, mfxU32* num_impls);

/*!
   @brief
      Destroys the handle allocated by the MFXQueryImplsDescription function or the MFXQueryImplsProperties function.
      Implementation must remember which handles are released. Once the last handle is released, this function must release memory
      allocated for the array of handles. Calling this function directly is not recommended. Instead, applications must call
      the MFXDispReleaseImplDescription function.

   @param[in] hdl   Handle to destroy. Can be equal to NULL.

   @return
      MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 2.0.
*/
mfxStatus MFX_CDECL MFXReleaseImplDescription(mfxHDL hdl);

#ifdef ONEVPL_EXPERIMENTAL
/*!
   @brief
      Delivers implementation capabilities for configured properties.
      The returned capability report will be sparsely filled out, with only properties available which
      were set via MFXSetConfigFilterProperty(). Calling this function directly is not recommended.
      Instead, applications must call the MFXEnumImplementations function.


   @param[in]  properties      Array of property name/value pairs indicating which properties to populate in the capability report.
   @param[in]  num_properties  Number of property name/value pairs.
   @param[out] num_impls       Number of the implementations.

   @return
      Array of handles to the capability report or NULL in case of NULL properties pointer or zero num_properties or NULL num_impls pointer.
      Length of array is equal to num_impls.

   @since This function is available since API version 2.15.
*/
mfxHDL* MFX_CDECL MFXQueryImplsProperties(mfxQueryProperty** properties, mfxU32 num_properties, mfxU32* num_impls);
#endif


#ifdef __cplusplus
} // extern "C"
#endif

#endif // __MFXIMPLCAPS_H__
