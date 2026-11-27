/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXVIDEO_H__
#define __MFXVIDEO_H__
#include "mfxsession.h"
#include "mfxstructures.h"
#include "mfxmemory.h"

#ifdef __cplusplus
extern "C"
{
#endif


MFX_PACK_BEGIN_STRUCT_W_PTR()
/*!
   Describes the API callback functions Alloc, Lock, Unlock, GetHDL, and Free that the
   implementation might use for allocating internal frames. Applications that operate on OS-specific video surfaces must
   implement these API callback functions.

   Using the default allocator implies that frame data passes in or out of functions through pointers,
   as opposed to using memory IDs.

   Behavior is undefined when using an incompletely defined external allocator.
   \verbatim embed:rst
    See the :ref:`Memory Allocation and External Allocators section <mem-alloc-ext-alloc>` for additional information.
    \endverbatim
*/
typedef struct {
    mfxU32      reserved[4];
    mfxHDL      pthis;    /*!< Pointer to the allocator object. */

    /*!
       @brief Allocates surface frames. For decoders, MFXVideoDECODE_Init calls Alloc only once. That call
              includes all frame allocation requests. For encoders, MFXVideoENCODE_Init calls Alloc twice: once for the
              input surfaces and again for the internal reconstructed surfaces. If application also calls this function explicitly, 
              it should have the logic to avoid duplicated allocation for the same request.

              If two library components must share DirectX* surfaces, this function should pass the pre-allocated surface
              chain to the library instead of allocating new DirectX surfaces.
              \verbatim embed:rst
              See the :ref:`Surface Pool Allocation section <surface_pool_alloc>` for additional information.
              \endverbatim

       @param[in]  pthis    Pointer to the allocator object.
       @param[in]  request  Pointer to the mfxFrameAllocRequest structure that specifies the type and number of required frames.
       @param[out] response Pointer to the mfxFrameAllocResponse structure that retrieves frames actually allocated.
       @return
             MFX_ERR_NONE               The function successfully allocated the memory block. \n
             MFX_ERR_MEMORY_ALLOC       The function failed to allocate the video frames. \n
             MFX_ERR_UNSUPPORTED        The function does not support allocating the specified type of memory.
    */
    mfxStatus  (MFX_CDECL  *Alloc)    (mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    /*!
       @brief Locks a frame and returns its pointer.
       @param[in]  pthis    Pointer to the allocator object.
       @param[in]  mid      Memory block ID.
       @param[out] ptr      Pointer to the returned frame structure.
       @return
             MFX_ERR_NONE               The function successfully locked the memory block. \n
             MFX_ERR_LOCK_MEMORY        This function failed to lock the frame.
    */
    mfxStatus  (MFX_CDECL  *Lock)     (mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);

    /*!
       @brief Unlocks a frame and invalidates the specified frame structure.
       @param[in]  pthis    Pointer to the allocator object.
       @param[in]  mid      Memory block ID.
       @param[out] ptr      Pointer to the frame structure. This pointer can be NULL.
       @return
             MFX_ERR_NONE               The function successfully locked the memory block.
    */
    mfxStatus  (MFX_CDECL  *Unlock)   (mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);

    /*!
       @brief Returns the OS-specific handle associated with a video frame. If the handle is a COM interface,
              the reference counter must increase. The library will release the interface afterward.
       @param[in]  pthis    Pointer to the allocator object.
       @param[in]  mid      Memory block ID.
       @param[out] handle   Pointer to the returned OS-specific handle.
       @return
             MFX_ERR_NONE               The function successfully returned the OS-specific handle. \n
             MFX_ERR_UNSUPPORTED        The function does not support obtaining OS-specific handle..
       @note For D3D11 surfaces, GetHDL should return an mfxHDLPair instead of an mfxHDL. In the mfxHDLPair struct,
       mfxHDLPair.first should be is set to the Texture2D address, and mfxHDLPair.second should be set to the array index. 
    */
    mfxStatus  (MFX_CDECL  *GetHDL)   (mfxHDL pthis, mfxMemId mid, mfxHDL *handle);

    /*!
       @brief De-allocates all allocated frames. 
              MFXClose will call this function. If application also calls this function, it should have logic to avoid double free.
       @param[in]  pthis    Pointer to the allocator object.
       @param[in]  response Pointer to the mfxFrameAllocResponse structure returned by the Alloc function.
       @return
             MFX_ERR_NONE               The function successfully de-allocated the memory block.
    */
    mfxStatus  (MFX_CDECL  *Free)     (mfxHDL pthis, mfxFrameAllocResponse *response);
} mfxFrameAllocator;
MFX_PACK_END()

/*!
   @brief
      Sets the external allocator callback structure for frame allocation.

      If the allocator argument is NULL, the library uses the
      default allocator, which allocates frames from system memory or hardware devices. The behavior of the API is undefined if it uses this
      function while the previous allocator is in use. A general guideline is to set the allocator immediately after initializing the session.

   @param[in]  session Session handle.
   @param[in] allocator   Pointer to the mfxFrameAllocator structure

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator);

/*!
   @brief
    Sets any essential system handle that the library might use. The handle must remain valid until after
    the application calls the MFXClose function.

    If the specified system handle is a COM interface, the reference counter of the COM interface will increase.
    The counter will decrease when the session closes.

   @param[in] session Session handle.
   @param[in] type   Handle type
   @param[in] hdl   Handle to be set

   @returns
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_UNDEFINED_BEHAVIOR The same handle is redefined.
                              For example, the function has been called twice with the same handle type or an
                              internal handle has been created before this function call.
   MFX_ERR_DEVICE_FAILED The SDK cannot initialize using the handle.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl);

/*!
   @brief
    Obtains system handles previously set by the MFXVideoCORE_SetHandle function.

    If the handler is a COM interface, the reference counter of the interface increases.
    The calling application must release the COM interface.

   @param[in] session Session handle.
   @param[in] type   Handle type
   @param[in] hdl  Pointer to the handle to be set

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_UNDEFINED_BEHAVIOR Specified handle type not found.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl);

/*!
   @brief
    Returns information about current hardware platform in the Legacy mode.

   @param[in] session Session handle.
   @param[out] platform Pointer to the mfxPlatform structure

   @return
   MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 1.19.

   Notes: Deprecated mfxPlatform::CodeName will be filled with MFX_PLATFORM_MAXIMUM for future new platforms.
*/
mfxStatus MFX_CDECL MFXVideoCORE_QueryPlatform(mfxSession session, mfxPlatform* platform);

/*!
   @brief
    Initiates execution of an asynchronous function not already started and returns the status code after the specified asynchronous operation completes.
    If wait is zero, the function returns immediately

   @param[in] session Session handle.
   @param[in] syncp Sync point
   @param[in] wait  wait time in milliseconds

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NONE_PARTIAL_OUTPUT   The function completed successfully, bitstream contains a portion of the encoded frame according to required granularity. \n
   MFX_WRN_IN_EXECUTION   The specified asynchronous function is in execution. \n
   MFX_ERR_ABORTED  The specified asynchronous function aborted due to data dependency on a previous asynchronous function that did not complete.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

/*! Maximum allowed length of parameter key and value strings, in bytes. */
#define MAX_PARAM_STRING_LENGTH     4096

/*! The mfxStructureType enumerator specifies the structure type for configuration with the string interface. */
typedef enum {
    MFX_STRUCTURE_TYPE_UNKNOWN = 0,         /*!< Unknown structure type. */

    MFX_STRUCTURE_TYPE_VIDEO_PARAM = 1,     /*!< Structure of type mfxVideoParam. */
} mfxStructureType;

#define MFX_CONFIGINTERFACE_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
/* Specifies config interface. */
typedef struct mfxConfigInterface {
    mfxHDL              Context; /*!< The context of the config interface. User should not touch (change, set, null) this pointer. */
    mfxStructVersion    Version; /*!< The version of the structure. */

    /*! @brief
       Sets a parameter to specified value in the current session. If a parameter already has a value,
       the new value will overwrite the existing value.

       @param[in] config_interface     The valid interface returned by calling MFXQueryInterface().
       @param[in] key                  Null-terminated string containing parameter to set. The string length must be < MAX_PARAM_STRING_LENGTH bytes.
       @param[in] value                Null-terminated string containing value to which key should be set. The string length must be < MAX_PARAM_STRING_LENGTH bytes.
                                       value will be converted from a string to the expected data type for the given key, or return an error if conversion fails.
       @param[in] struct_type          Type of structure pointed to by structure.
       @param[out] structure           If and only if SetParameter returns MFX_ERR_NONE, the contents of structure (including any attached extension
                                       buffers) will be updated according to the provided key and value. If key modifies a field in an extension buffer
                                       which is not already attached, the function will return MFX_ERR_MORE_EXTBUFFER and fill ext_buffer with the header for
                                       the required mfxExtBuffer type.
       @param[out] ext_buffer          If and only if SetParameter returns MFX_ERR_MORE_EXTBUFFER, ext_buffer will contain the header for a buffer
                                       of type mfxExtBuffer. The caller should allocate a buffer of the size ext_buffer.BufferSz, copy the header in ext_buffer
                                       to the start of this new buffer, attach this buffer to videoParam, then call SetParameter again. Otherwise, the
                                       contents of ext_buffer will be cleared.
       @return
          MFX_ERR_NONE                 The function completed successfully.
          MFX_ERR_NULL_PTR             If key, value, videoParam, and/or ext_buffer is NULL.
          MFX_ERR_NOT_FOUND            If key contains an unknown parameter name.
          MFX_ERR_UNSUPPORTED          If value is of the wrong format for key (for example, a string is provided where an integer is required)
                                       or if value cannot be converted into any valid data type.
          MFX_ERR_INVALID_VIDEO_PARAM  If length of key or value is >= MAX_PARAM_STRING_LENGTH or is zero (empty string).
          MFX_ERR_MORE_EXTBUFFER       If key requires modifying a field in an mfxExtBuffer which is not attached. Caller must allocate and attach
                                       the buffer type provided in ext_buffer then call the function again.

       @since This function is available since API version 2.10.
    */
    mfxStatus (MFX_CDECL *SetParameter)(struct mfxConfigInterface *config_interface, const mfxU8* key, const mfxU8* value, mfxStructureType struct_type, mfxHDL structure, mfxExtBuffer *ext_buffer);

    mfxHDL     reserved[16];
} mfxConfigInterface;
MFX_PACK_END()

/*! Alias for returning interface of type mfxConfigInterface. */
#define MFXGetConfigInterface(session, piface)  MFXVideoCORE_GetHandle((session), MFX_HANDLE_CONFIG_INTERFACE, (mfxHDL *)(piface))

/* VideoENCODE */

/*!
   @brief
     Works in either of four modes:

     @li If the @p in parameter is zero, the function returns the class configurability in the output structure. The application must set to zero the fields it wants to check for support. If the field is supported, function sets non-zero value to this field, otherwise it would be ignored. It indicates that the SDK implementation can configure the field with Init.

     @li If the @p in parameter is non-zero, the function checks the validity of the fields in the input structure. Then the function returns the corrected values in
     the output structure. If there is insufficient information to determine the validity or correction is impossible, the function zeroes the fields.
     This feature can verify whether the implementation supports certain profiles, levels or bitrates.

     @li If the @p in parameter is non-zero and mfxExtEncoderResetOption structure is attached to it, the function queries for the outcome of the MFXVideoENCODE_Reset function
     and returns it in the mfxExtEncoderResetOption structure attached to out. The query function succeeds if a reset is possible and returns an error otherwise. Unlike other
     modes that are independent of the encoder state, this one checks if reset is possible in the present encoder state.
     This mode also requires a completely defined mfxVideoParam structure, unlike other modes that support partially defined configurations.
     See mfxExtEncoderResetOption description for more details.

     @li If the @p in parameter is non-zero and mfxExtEncoderCapability structure is attached to it, the function returns encoder capability in the mfxExtEncoderCapability structure
     attached to out. It is recommended to fill in the mfxVideoParam structure and set the hardware acceleration device handle before calling the function in this mode.

     The application can call this function before or after it initializes the encoder. The ``CodecId`` field of the output structure is a mandated field (to be filled by the
     application) to identify the coding standard.

   @param[in] session Session handle.
   @param[in] in   Pointer to the mfxVideoParam structure as input.
   @param[out] out  Pointer to the mfxVideoParam structure as output.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_UNSUPPORTED  The function failed to identify a specific implementation for the required features. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The encoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);

/*!
   @brief
    Returns minimum and suggested numbers of the input frame surfaces required for encoding initialization and their type.

    Init will call the external allocator for the required frames with the same set of numbers.
    This function does not validate I/O parameters except those used in calculating the number of input surfaces.

    The use of this function is recommended.
    \verbatim embed:rst
    For more information, see the :ref:`Working with Hardware Acceleration section<hw-acceleration>`.
    \endverbatim


   @param[in] session Session handle.
   @param[in] par     Pointer to the mfxVideoParam structure as input.
   @param[in] request Pointer to the mfxFrameAllocRequest structure as output.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The encoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);

/*!
   @brief
    Allocates memory and prepares tables and necessary structures for encoding.

    This function also does extensive validation to ensure if the
    configuration, as specified in the input parameters, is supported.

   @param[in] session Session handle.
   @param[in] par Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The encoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved. \n
   MFX_ERR_UNDEFINED_BEHAVIOR  The function is called twice without a close;

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Stops the current encoding operation and restores internal structures or parameters for a new encoding operation, possibly with new parameters.

   @param[in] session Session handle.
   @param[in] par   Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The function detected that video parameters provided by the application are incompatible with initialization parameters.
                                     Reset requires additional memory allocation and cannot be executed. The application should close the
                                     component and then reinitialize it. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Terminates the current encoding operation and de-allocates any internal tables or structures.

   @param[in] session Session handle.

   @return
   MFX_ERR_NONE  The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_Close(mfxSession session);

/*!
   @brief
    Retrieves current working parameters to the specified output structure.

    If extended buffers are to be returned, the
    application must allocate those extended buffers and attach them as part of the output structure.
    The application can retrieve a copy of the bitstream header by attaching the mfxExtCodingOptionSPSPPS structure to the mfxVideoParam structure.

   @param[in] session Session handle.
   @param[in] par Pointer to the corresponding parameter structure.

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Obtains statistics collected during encoding.

   @param[in] session Session handle.
   @param[in] stat  Pointer to the mfxEncodeStat structure.

   @return MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat);

/*!
   @brief
    Takes a single input frame in either encoded or display order and generates its output bitstream.

    In the case of encoded ordering, the mfxEncodeCtrl
    structure must specify the explicit frame type. In the case of display ordering, this function handles frame order shuffling according to the GOP structure
    parameters specified during initialization.

    Since encoding may process frames differently from the input order, not every call of the function generates output and the function returns MFX_ERR_MORE_DATA.
    If the encoder needs to cache the frame, the function locks the frame. The application should not alter the frame until the encoder unlocks the frame.
    If there is output (with return status MFX_ERR_NONE), the return is a frame's worth of bitstream.

    It is the calling application's responsibility to ensure that there is sufficient space in the output buffer. The value ``BufferSizeInKB`` in the
    mfxVideoParam structure at encoding initialization specifies the maximum possible size for any compressed frames. This value can also be obtained from the
    MFXVideoENCODE_GetVideoParam function after encoding initialization.

    To mark the end of the encoding sequence, call this function with a NULL surface pointer. Repeat the call to drain any remaining internally cached bitstreams
    (one frame at a time) until MFX_ERR_MORE_DATA is returned.

    This function is asynchronous.

   @param[in] session Session handle.
   @param[in] ctrl  Pointer to the mfxEncodeCtrl structure for per-frame encoding control; this parameter is optional (it can be NULL) if the encoder works in the display order mode.
                    ctrl can be freed right after successful MFXVideoENCODE_EncodeFrameAsync (it is copied inside), but not the ext buffers attached to this ctrl.
                    If the ext buffers are allocated by the user, do not move, alter or delete unless surface.Data.Locked is zero.
   @param[in] surface  Pointer to the frame surface structure.
                       For surfaces allocated by oneAPI Video Processing Library (oneVPL) RT it is safe to call mfxFrameSurface1::FrameInterface->Release after successful MFXVideoENCODE_EncodeFrameAsync.
                       If it is allocated by user, do not move, alter or delete unless surface.Data.Locked is zero.
   @param[out] bs   Pointer to the output bitstream.
   @param[out] syncp  Pointer to the returned sync point associated with this operation.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_NOT_ENOUGH_BUFFER  The bitstream buffer size is insufficient. \n
   MFX_ERR_MORE_DATA   The function requires more data to generate any output. \n
   MFX_ERR_DEVICE_LOST Hardware device was lost.
   \verbatim embed:rst
   See the :ref:`Working with Microsoft* DirectX* Applications section<work_ms_directx_app>` for further information.
   \endverbatim
   \n
   MFX_WRN_DEVICE_BUSY  Hardware device is currently busy. Call this function again after MFXVideoCORE_SyncOperation or in a few milliseconds.  \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  Inconsistent parameters detected not conforming to Configuration Parameter Constraints.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);

/*!
   @brief
     Works in one of two modes:

     @li If the @p in parameter is zero, the function returns the class configurability in the output structure. A non-zero value in each field of the output structure
     indicates that the field is configurable by the implementation with the MFXVideoDECODE_Init function.

     @li If the @p in parameter is non-zero, the function checks the validity of the fields in the input structure. Then the function returns the corrected values to
     the output structure. If there is insufficient information to determine the validity or correction is impossible, the function zeros the fields. This
     feature can verify whether the implementation supports certain profiles, levels, or bitrates.

     The application can call this function before or after it initializes the decoder. The ``CodecId`` field of the output structure is a mandated field
     (to be filled by the application) to identify the coding standard.

   @param[in] session Session handle.
   @param[in] in   Pointer to the mfxVideoParam structure as input.
   @param[out] out  Pointer to the mfxVideoParam structure as output.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_UNSUPPORTED  The function failed to identify a specific implementation for the required features. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The decoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/

mfxStatus MFX_CDECL MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);

/*!
   @brief
    Parses the input bitstream and fills the mfxVideoParam structure with appropriate values, such as resolution and frame rate, for the Init API function.

    The application can then pass the resulting structure to the MFXVideoDECODE_Init function for decoder initialization.

    An application can call this API function at any time before or after decoder initialization. If the library finds a sequence header in the bitstream, the function
    moves the bitstream pointer to the first bit of the sequence header. Otherwise, the function moves the bitstream pointer close to the end of the bitstream buffer but leaves enough data in the buffer to avoid possible loss of start code.

    The ``CodecId`` field of the mfxVideoParam structure is a mandated field (to be filled by the application) to identify the coding standard.

    The application can retrieve a copy of the bitstream header, by attaching the mfxExtCodingOptionSPSPPS structure to the mfxVideoParam structure.

   @param[in] session Session handle.
   @param[in] bs   Pointer to the bitstream.
   @param[in] par  Pointer to the mfxVideoParam structure.

   @return
    - MFX_ERR_NONE The function successfully filled the structure. It does not mean that the stream can be decoded by the library.
                The application should call MFXVideoDECODE_Query function to check if decoding of the stream is supported. \n
    - MFX_ERR_MORE_DATA   The function requires more bitstream data. \n
    - MFX_ERR_UNSUPPORTED ``CodecId`` field of the mfxVideoParam structure indicates some unsupported codec. \n
    - MFX_ERR_INVALID_HANDLE  Session is not initialized. \n
    - MFX_ERR_NULL_PTR @p bs or @p par pointer is NULL.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par);

/*!
   @brief
    Returns minimum and suggested numbers of the output frame surfaces required for decoding initialization and their type.

    Init will call the external allocator for the required frames with the same set of numbers.
    The use of this function is recommended.
    \verbatim embed:rst
    For more information, see the :ref:`Working with Hardware Acceleration section<hw-acceleration>`.
    \endverbatim

    The ``CodecId`` field of the mfxVideoParam structure is a mandated field (to be filled by the application) to identify the coding standard.
    This function does not validate I/O parameters except those used in calculating the number of output surfaces.

   @param[in] session Session handle.
   @param[in] par     Pointer to the mfxVideoParam structure as input.
   @param[in] request Pointer to the mfxFrameAllocRequest structure as output.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The encoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);

/*!
   @brief
    Allocates memory and prepares tables and necessary structures for encoding.

    This function also does extensive validation to ensure if the
    configuration, as specified in the input parameters, is supported.

   @param[in] session Session handle.
   @param[in] par Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The encoding may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved. \n
   MFX_ERR_UNDEFINED_BEHAVIOR  The function is called twice without a close.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Stops the current decoding operation and restores internal structures or parameters for a new decoding operation.

    Reset serves two purposes:

    @li It recovers the decoder from errors.
    @li It restarts decoding from a new position

    The function resets the old sequence header (sequence parameter set in H.264, or sequence header in MPEG-2 and VC-1). The decoder will expect a new sequence header
    before it decodes the next frame and will skip any bitstream before encountering the new sequence header.

   @param[in] session Session handle.
   @param[in] par   Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected that video parameters are wrong or they conflict with initialization parameters. Reset is impossible. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The function detected that video parameters provided by the application are incompatible with initialization parameters.
                                     Reset requires additional memory allocation and cannot be executed. The application should close the
                                     component and then reinitialize it. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Terminates the current decoding operation and de-allocates any internal tables or structures.

   @param[in] session Session handle.

   @return
   MFX_ERR_NONE  The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_Close(mfxSession session);

/*!
   @brief
    Retrieves current working parameters to the specified output structure.

    If extended buffers are to be returned, the
    application must allocate those extended buffers and attach them as part of the output structure.
    The application can retrieve a copy of the bitstream header, by attaching the mfxExtCodingOptionSPSPPS structure to the mfxVideoParam structure.

   @param[in] session Session handle.
   @param[in] par Pointer to the corresponding parameter structure.

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Obtains statistics collected during decoding.

   @param[in] session Session handle.
   @param[in] stat  Pointer to the mfxDecodeStat structure.

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat);

/*!
   @brief
    Sets the decoder skip mode.

    The application may use this API function to increase decoding performance by sacrificing output quality. Increasing the skip
    level first results in skipping of some decoding operations like deblocking and then leads to frame skipping; first B, then P. Particular details are platform dependent.

   @param[in] session Session handle.
   @param[in] mode   Decoder skip mode. See the mfxSkipMode enumerator for details.

   @return
   MFX_ERR_NONE The function completed successfully and the output surface is ready for decoding \n
   MFX_WRN_VALUE_NOT_CHANGED   The skip mode is not affected as the maximum or minimum skip range is reached.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode);

/*!
   @brief
    Extracts user data (MPEG-2) or SEI (H.264) messages from the bitstream.

    Internally, the decoder implementation stores encountered user data or
    SEI messages. The application may call this API function multiple times to retrieve the user data or SEI messages, one at a time.

    If there is no payload available, the function returns with payload->NumBit=0.

   @param[in] session Session handle.
   @param[in] ts  Pointer to the user data time stamp in units of 90 KHz; divide ts by 90,000 (90 KHz) to obtain the time in seconds; the time stamp matches the payload
                  with a specific decoded frame.
   @param[in] payload  Pointer to the mfxPayload structure; the payload contains user data in MPEG-2 or SEI messages in H.264.

   @return
   MFX_ERR_NONE The function completed successfully and the output buffer is ready for decoding. \n
   MFX_ERR_NOT_ENOUGH_BUFFER  The payload buffer size is insufficient.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload);

/*!
   @brief
   Decodes the input bitstream to a single output frame.

   The @p surface_work parameter provides a working frame buffer for the decoder. The application should allocate the working frame buffer, which stores decoded frames.
   If the function requires caching frames after decoding, it locks the frames and the application must provide a new frame buffer in the next call.

   If, and only if, the function returns MFX_ERR_NONE, the pointer @p surface_out points to the output frame in the display order. If there are no further frames,
   the function will reset the pointer to zero and return the appropriate status code.

   Before decoding the first frame, a sequence header (sequence parameter set in H.264 or sequence header in MPEG-2 and VC-1) must be present. The function skips any
   bitstreams before it encounters the new sequence header.

   The input bitstream @p bs can be of any size. If there are not enough bits to decode a frame, the function returns MFX_ERR_MORE_DATA, and consumes all input bits except if
   a partial start code or sequence header is at the end of the buffer. In this case, the function leaves the last few bytes in the bitstream buffer.
   If there is more incoming bitstream, the application should append the incoming bitstream to the bitstream buffer. Otherwise, the application should ignore the
   remaining bytes in the bitstream buffer and apply the end of stream procedure described below.

   The application must set @p bs to NULL to signal end of stream. The application may need to call this API function several times to drain any internally cached frames until the
   function returns MFX_ERR_MORE_DATA.

   If more than one frame is in the bitstream buffer, the function decodes until the buffer is consumed. The decoding process can be interrupted for events such as if the
   decoder needs additional working buffers, is readying a frame for retrieval, or encountering a new header. In these cases, the function returns appropriate status code
   and moves the bitstream pointer to the remaining data.

   The decoder may return MFX_ERR_NONE without taking any data from the input bitstream buffer. If the application appends additional data to the bitstream buffer, it
   is possible that the bitstream buffer may contain more than one frame. It is recommended that the application invoke the function repeatedly until the function
   returns MFX_ERR_MORE_DATA, before appending any more data to the bitstream buffer.

   Starting from API 2.0 it is possible to pass NULL instead of surface_work. In such case runtime will allocate output frames internally.

   This function is asynchronous.

   @param[in] session Session handle.
   @param[in] bs  Pointer to the input bitstream.
   @param[in] surface_work  Pointer to the working frame buffer for the decoder.
   @param[out] surface_out   Pointer to the output frame in the display order.
   @param[out] syncp   Pointer to the sync point associated with this operation.

   @return
   MFX_ERR_NONE The function completed successfully and the output surface is ready for decoding. \n
   MFX_ERR_MORE_DATA The function requires more bitstream at input before decoding can proceed. \n
   MFX_ERR_MORE_SURFACE The function requires more frame surface at output before decoding can proceed. \n
   MFX_ERR_DEVICE_LOST  Hardware device was lost.
   \verbatim embed:rst
   See the :ref:`Working with Microsoft* DirectX* Applications section<work_ms_directx_app>` for further information.
   \endverbatim
   \n
   MFX_WRN_DEVICE_BUSY  Hardware device is currently busy. Call this function again after MFXVideoCORE_SyncOperation or in a few milliseconds.  \n
   MFX_WRN_VIDEO_PARAM_CHANGED  The decoder detected a new sequence header in the bitstream. Video parameters may have changed. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The decoder detected incompatible video parameters in the bitstream and failed to follow them. \n
   MFX_ERR_REALLOC_SURFACE  Bigger surface_work required. May be returned only if mfxInfoMFX::EnableReallocRequest was set to ON during initialization. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED Timeout expired for internal output frame allocation (if set with mfxExtAllocationHints and NULL passed as surface_work). Repeat the call in a few milliseconds or re-initialize decoder with higher surface limit.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

/* VideoVPP */

/*!
   @brief
     Works in one of two modes:

     @li If the @p in pointer is zero, the function returns the class configurability in the output structure. A non-zero value in a field indicates that the
       implementation can configure it with Init.

     @li If the @p in parameter is non-zero, the function checks the validity of the fields in the input structure. Then the function returns the corrected values to
     the output structure. If there is insufficient information to determine the validity or correction is impossible, the function zeroes the fields.

     The application can call this function before or after it initializes the preprocessor.

   @param[in] session Session handle.
   @param[in] in   Pointer to the mfxVideoParam structure as input.
   @param[out] out  Pointer to the mfxVideoParam structure as output.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_UNSUPPORTED  The implementation does not support the specified configuration. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The video processing may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);

/*!
   @brief
    Returns minimum and suggested numbers of the input frame surfaces required for video processing initialization and their type.

    The parameter ``request[0]`` refers to the input requirements; ``request[1]`` refers to output requirements. Init will call the external allocator for the
    required frames with the same set of numbers.
    This function does not validate I/O parameters except those used in calculating the number of input surfaces.

    The use of this function is recommended.
    \verbatim embed:rst
    For more information, see the :ref:`Working with Hardware Acceleration section<hw-acceleration>`.
    \endverbatim

   @param[in] session Session handle.
   @param[in] par     Pointer to the mfxVideoParam structure as input.
   @param[in] request Pointer to the mfxFrameAllocRequest structure; use ``request[0]`` for input requirements and ``request[1]`` for output requirements for video processing.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The video processing may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest request[2]);

/*!
   @brief
    Allocates memory and prepares tables and necessary structures for video processing.

    This function also does extensive validation to ensure if the
    configuration, as specified in the input parameters, is supported.

   @param[in] session Session handle.
   @param[in] par Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected invalid video parameters. These parameters may be out of the valid range, or the combination of them
                                resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_PARTIAL_ACCELERATION  The underlying hardware does not fully support the specified video parameters.
                                 The video processing may be partially accelerated. Only hardware implementations may return this status code. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved. \n
   MFX_ERR_UNDEFINED_BEHAVIOR  The function is called twice without a close. \n
   MFX_WRN_FILTER_SKIPPED    The VPP skipped one or more filters requested by the application.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Stops the current video processing operation and restores internal structures or parameters for a new operation

   @param[in] session Session handle.
   @param[in] par   Pointer to the mfxVideoParam structure.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected that video parameters are wrong or they conflict with initialization parameters. Reset is impossible. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The function detected that video parameters provided by the application are incompatible with initialization parameters.
                                     Reset requires additional memory allocation and cannot be executed. The application should close the
                                     component and then reinitialize it. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Terminates the current video processing operation and de-allocates any internal tables or structures.

   @param[in] session Session handle.

   @return MFX_ERR_NONE
   The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_Close(mfxSession session);

/*!
   @brief
    Retrieves current working parameters to the specified output structure.

    If extended buffers are to be returned, the
    application must allocate those extended buffers and attach them as part of the output structure.

   @param[in] session Session handle.
   @param[in] par Pointer to the corresponding parameter structure.

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par);

/*!
   @brief
    Obtains statistics collected during video processing.

   @param[in] session Session handle.
   @param[in] stat  Pointer to the mfxVPPStat structure.

   @return
   MFX_ERR_NONE The function completed successfully. \n

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat);

/*!
   @brief
    Processes a single input frame to a single output frame.

    Retrieval of the auxiliary data is optional; the encoding process may use it.
    The video processing process may not generate an instant output given an input.
    \verbatim embed:rst
    See the :ref:`Video Processing Procedures section<vid_process_procedure>` for details on how to
    correctly send input and retrieve output.
    \endverbatim


    At the end of the stream, call this function with the input argument ``in=NULL`` to retrieve any remaining frames, until the function returns MFX_ERR_MORE_DATA.
    This function is asynchronous.

   @param[in] session Session handle.
   @param[in] in  Pointer to the input video surface structure.
   @param[out] out  Pointer to the output video surface structure.
   @param[in] aux  Optional pointer to the auxiliary data structure.
   @param[out] syncp  Pointer to the output sync point.

   @return
   MFX_ERR_NONE The output frame is ready after synchronization. \n
   MFX_ERR_MORE_DATA Need more input frames before VPP can produce an output. \n
   MFX_ERR_MORE_SURFACE The output frame is ready after synchronization. Need more surfaces at output for additional output frames available. \n
   MFX_ERR_DEVICE_LOST  Hardware device was lost.
   \verbatim embed:rst
    See the :ref:`Working with Microsoft* DirectX* Applications section<work_ms_directx_app>` for further information.
    \endverbatim
    \n
   MFX_WRN_DEVICE_BUSY  Hardware device is currently busy. Call this function again after MFXVideoCORE_SyncOperation or in a few milliseconds.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);

/*!
   @brief
    The function processes a single input frame to a single output frame with internal allocation of output frame.

       At the end of the stream, call this function with the input argument ``in=NULL`` to retrieve any remaining frames, until the function returns MFX_ERR_MORE_DATA.
    This function is asynchronous.

   @param[in] session Session handle.
   @param[in] in  Pointer to the input video surface structure.
   @param[out] out  Pointer to the output video surface structure which is reference counted object allocated by the library.

   @return
   MFX_ERR_NONE The output frame is ready after synchronization. \n
   MFX_ERR_MORE_DATA Need more input frames before VPP can produce an output. \n
   MFX_ERR_MEMORY_ALLOC The function failed to allocate output video frame. \n

   MFX_ERR_DEVICE_LOST  Hardware device was lost.
   \verbatim embed:rst
    See the :ref:`Working with Microsoft* DirectX* Applications section<work_ms_directx_app>` for further information.
    \endverbatim
    \n
   MFX_WRN_DEVICE_BUSY  Hardware device is currently busy. Call this function again after MFXVideoCORE_SyncOperation or in a few milliseconds. \n
   MFX_WRN_ALLOC_TIMEOUT_EXPIRED Timeout expired for internal output frame allocation (if set with mfxExtAllocationHints). Repeat the call in a few milliseconds or reinitialize VPP with higher surface limit.

   @since This function is available since API version 2.1.
*/
mfxStatus MFX_CDECL MFXVideoVPP_ProcessFrameAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 **out);

/*!
   @brief
   Initialize the SDK in (decode + vpp) mode. The logic of this function is similar to  MFXVideoDECODE_Init,
   but application has to provide array of pointers to mfxVideoChannelParam and num_channel_param - number of channels. Application is responsible for
   memory allocation for mfxVideoChannelParam parameters and for each channel it should specify channel IDs:
   mfxVideoChannelParam::mfxFrameInfo::ChannelId. ChannelId should be unique value within one session. ChannelID equals to the 0
   is reserved for the original decoded frame.
   The application can attach mfxExtInCrops to mfxVideoChannelParam::ExtParam to annotate input video frame if it wants to enable
   letterboxing operation.
   @param[in] session SDK session handle.
   @param[in] decode_par Pointer to the mfxVideoParam structure which contains initialization parameters for decoder.
   @param[in] vpp_par_array Array of pointers to `mfxVideoChannelParam`structures. Each mfxVideoChannelParam contains initialization
              parameters for each VPP channel.
   @param[in] num_vpp_par Size of array of pointers to mfxVideoChannelParam structures.

   @return
   MFX_ERR_NONE The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM The function detected invalid video parameters. These parameters may be out of the valid range, or
   the combination of them resulted in incompatibility. Incompatibility not resolved. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM The function detected some video parameters were incompatible with others; incompatibility
   resolved. \n
   MFX_ERR_UNDEFINED_BEHAVIOR The component is already initialized. \n
   MFX_WRN_FILTER_SKIPPED The VPP skipped one or more filters requested by the application.

   @since This function is available since API version 2.1.
*/
mfxStatus  MFX_CDECL MFXVideoDECODE_VPP_Init(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_vpp_par);

/*!
   @brief
   This function is similar to MFXVideoDECODE_DecodeFrameAsync and inherits all bitstream processing logic. As output,
   it allocates and returns @p surf_array_out array of processed surfaces according to the chain of filters specified
   by application in MFXVideoDECODE_VPP_Init, including original decoded frames. In the @p surf_array_out, the original
   decoded frames are returned through surfaces with mfxFrameInfo::ChannelId == 0, followed by each of the subsequent
   frame surfaces for each of the requested mfxVideoChannelParam entries provided to the MFXVideoCECODE_VPP_Init
   function. At maximum, the number of frame surfaces return is 1 + the value of @p num_vpp_par to the
   MFXVideoDECODE_VPP_Init function, but the application must be prepared to the case when some particular filters
   are not ready to output surfaces, so the length of @p surf_array_out will be less. Application should use
   mfxFrameInfo::ChannelId parameter to match output surface against configured filter.

   An application must synchronize each output surface from the @p surf_array_out surface array independently.

   @param[in] session SDK session handle.
   @param[in] bs Pointer to the input bitstream.
   @param[in] skip_channels Pointer to the array of `ChannelId`s which specifies channels with skip output frames. Memory for
              the array is allocated by application.
   @param[in] num_skip_channels Number of channels addressed by skip_channels.
   @param[out] surf_array_out The address of a pointer to the structure with frame surfaces.

   @return
   MFX_ERR_NONE The function completed successfully and the output surface is ready for decoding. \n
   MFX_ERR_MORE_DATA The function requires more bitstream at input before decoding can proceed. \n
   MFX_ERR_MORE_SURFACE The function requires more frame surface at output before decoding can proceed. \n
   MFX_ERR_DEVICE_LOST  Hardware device was lost.
   \verbatim embed:rst
   See the :ref:`Working with Microsoft* DirectX* Applications section<work_ms_directx_app>` for further information.
   \endverbatim
   \n
   MFX_WRN_DEVICE_BUSY  Hardware device is currently busy. Call this function again after MFXVideoCORE_SyncOperation or in a few milliseconds.  \n
   MFX_WRN_VIDEO_PARAM_CHANGED  The decoder detected a new sequence header in the bitstream. Video parameters may have changed. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The decoder detected incompatible video parameters in the bitstream and failed to follow them. \n
   MFX_ERR_NULL_PTR num_skip_channels doesn't equal to 0 when skip_channels is NULL.

   @since This function is available since API version 2.1.
*/
mfxStatus  MFX_CDECL MFXVideoDECODE_VPP_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxU32* skip_channels, mfxU32 num_skip_channels, mfxSurfaceArray **surf_array_out);

/*!
   @brief
   This function is similar to MFXVideoDECODE_Reset and  stops the current decoding and vpp operation, and restores internal
   structures or parameters for a new decoding plus vpp operation. It resets the state of the decoder and/or all initialized vpp
   channels. Applications have to care about draining of buffered frames for decode and all vpp channels before call this function.
   The application can attach mfxExtInCrops to mfxVideoChannelParam::ExtParam to annotate input video frame if it wants to enable
   letterboxing operation.

   @param[in] session Session handle.
   @param[in] decode_par   Pointer to the `mfxVideoParam` structure which contains new initialization parameters for decoder. Might
                           be NULL if application wants to Reset only VPP channels.
   @param[in] vpp_par_array Array of pointers to mfxVideoChannelParam structures. Each mfxVideoChannelParam contains new
                            initialization parameters for each VPP channel.
   @param[in] num_vpp_par Size of array of pointers to mfxVideoChannelParam structures.

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_INVALID_VIDEO_PARAM  The function detected that video parameters are wrong or they conflict with initialization parameters. Reset is impossible. \n
   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM  The function detected that video parameters provided by the application are incompatible with initialization parameters.
                                     Reset requires additional memory allocation and cannot be executed. The application should close the
                                     component and then reinitialize it. \n
   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM  The function detected some video parameters were incompatible with others; incompatibility resolved.
   MFX_ERR_NULL_PTR  Both pointers decode_par and vpp_par_array` equal to zero.

   @since This function is available since API version 2.1.
*/
mfxStatus  MFX_CDECL MFXVideoDECODE_VPP_Reset(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_vpp_par);

/*!
   @brief
   Returns actual VPP parameters for selected channel which should be specified by application through
   mfxVideoChannelParam::mfxFrameInfo::ChannelId.

   @param[in] session Session handle.
   @param[in] par   Pointer to the `mfxVideoChannelParam` structure which allocated by application
   @param[in] channel_id specifies the requested channel's info

   @return
   MFX_ERR_NONE  The function completed successfully. \n
   MFX_ERR_NULL_PTR  par pointer is NULL. \n
   MFX_ERR_NOT_FOUND the library is not able to find VPP channel with such channel_id.

   @since This function is available since API version 2.1.
*/
mfxStatus  MFX_CDECL MFXVideoDECODE_VPP_GetChannelParam(mfxSession session, mfxVideoChannelParam *par, mfxU32 channel_id);

/*!
   @brief
   This function is similar to MFXVideoDECODE_Close. It terminates the current decoding and vpp operation and de-allocates any internal tables or structures.

   @param[in] session Session handle.

   @return
   MFX_ERR_NONE  The function completed successfully. \n

   @since This function is available since API version 2.1.
*/

mfxStatus  MFX_CDECL MFXVideoDECODE_VPP_Close(mfxSession session);

/*! Alias for MFXVideoDECODE_DecodeHeader function. */
#define MFXVideoDECODE_VPP_DecodeHeader     MFXVideoDECODE_DecodeHeader

/*! Alias for MFXVideoDECODE_GetVideoParam function. */
#define MFXVideoDECODE_VPP_GetVideoParam    MFXVideoDECODE_GetVideoParam

/*! Alias for MFXVideoDECODE_GetDecodeStat function. */
#define MFXVideoDECODE_VPP_GetDecodeStat    MFXVideoDECODE_GetDecodeStat

/*! Alias for MFXVideoDECODE_SetSkipMode function. */
#define MFXVideoDECODE_VPP_SetSkipMode      MFXVideoDECODE_SetSkipMode

/*! Alias for MFXVideoDECODE_GetPayload function. */
#define MFXVideoDECODE_VPP_GetPayload       MFXVideoDECODE_GetPayload

#ifdef __cplusplus
} // extern "C"
#endif

#endif
