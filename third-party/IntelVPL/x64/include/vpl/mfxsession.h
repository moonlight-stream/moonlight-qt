/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXSESSION_H__
#define __MFXSESSION_H__
#include "mfxcommon.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Global Functions */

/*! Session handle. */
typedef struct _mfxSession *mfxSession;

/*!
   @brief
      Creates and initializes a session in the legacy mode for compatibility with Intel(r) Media SDK applications.
      This function is deprecated starting from API version 2.0, applications must use MFXLoad with mfxCreateSession
      to select the implementation and initialize the session.

      Call this function before calling
      any other API function. If the desired implementation specified by ``impl`` is MFX_IMPL_AUTO,
      the function will search for the platform-specific implementation.
      If the function cannot find the platform-specific implementation, it will use the software implementation instead.

      The ``ver`` argument indicates the desired version of the library implementation.
      The loaded implementation will have an API version compatible to the specified version (equal in
      the major version number, and no less in the minor version number.) If the desired version
      is not specified, the default is to use the API version from the library release with
      which an application is built.

      Production applications should always specify the minimum API version that meets the
      functional requirements. For example, if an application uses only H.264 decoding as described
      in API v1.0, the application should initialize the library with API v1.0. This ensures
      backward compatibility.

   @param[in] impl     mfxIMPL enumerator that indicates the desired legacy Intel(r) Media SDK implementation.
   @param[in] ver      Pointer to the minimum library version or zero, if not specified.
   @param[out] session Pointer to the legacy Intel(r) Media SDK session handle.

   @return
      MFX_ERR_NONE        The function completed successfully. The output parameter contains the handle of the session.\n
      MFX_ERR_UNSUPPORTED The function cannot find the desired legacy Intel(r) Media SDK implementation or version.

   @since This function is available since API version 1.0.

   @deprecated Deprecated in API version 2.3. Use MFXLoad and MFXCreateSession to initialize the session.
               Use MFX_DEPRECATED_OFF macro to turn off the deprecation message visualization.
*/
MFX_DEPRECATED mfxStatus MFX_CDECL MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session);

/*!
   @brief
      Creates and initializes a session in the legacy mode for compatibility with Intel(r) Media SDK applications.
      This function is deprecated starting from API version 2.0, applications must use MFXLoad with mfxCreateSession
      to select the implementation and initialize the session.

      Call this function before calling any other API functions.
      If the desired implementation specified by ``par`` is MFX_IMPL_AUTO, the function will search for
      the platform-specific implementation. If the function cannot find the platform-specific implementation, it will use the software implementation instead.

      The argument ``par.Version`` indicates the desired version of the implementation. The loaded implementation will have an API
      version compatible to the specified version (equal in the major version number, and no less in the minor version number.)
      If the desired version is not specified, the default is to use the API version from the library release with
      which an application is built.

      Production applications should always specify the minimum API version that meets the functional requirements.
      For example, if an application uses only H.264 decoding as described in API v1.0, the application should initialize the library with API v1.0. This ensures backward compatibility.

      The argument ``par.ExternalThreads`` specifies threading mode. Value 0 means that the implementation should create and
      handle work threads internally (this is essentially the equivalent of the regular MFXInit).

   @param[in]  par     mfxInitParam structure that indicates the desired implementation, minimum library version and desired threading mode.
   @param[out] session Pointer to the session handle.

   @return
      MFX_ERR_NONE        The function completed successfully. The output parameter contains the handle of the session.\n
      MFX_ERR_UNSUPPORTED The function cannot find the desired implementation or version.

   @since This function is available since API version 1.14.

   @deprecated Deprecated in API version 2.3. Use MFXLoad and MFXCreateSession to initialize the session.
               Use MFX_DEPRECATED_OFF macro to turn off the deprecation message visualization.
*/
MFX_DEPRECATED mfxStatus MFX_CDECL MFXInitEx(mfxInitParam par, mfxSession *session);

/*!
   @brief
   Creates and initializes a session starting from API version 2.0. This function is used by the dispatcher.
   The dispatcher creates and fills the mfxInitializationParam structure according to mfxConfig values set by an application.
   Calling this function directly is not recommended. Instead, applications must call the MFXCreateSession function.


   @param[in]  par     mfxInitializationParam structure that indicates the minimum library version and acceleration type.
   @param[out] session Pointer to the session handle.

   @return
      MFX_ERR_NONE        The function completed successfully. The output parameter contains the handle of the session.\n
      MFX_ERR_UNSUPPORTED The function cannot find the desired implementation or version.

   @since This function is available since API version 2.0.
*/
mfxStatus MFX_CDECL MFXInitialize(mfxInitializationParam par, mfxSession *session);

/*!
   @brief Completes and deinitializes a session. Any active tasks in execution or
    in queue are aborted. The application cannot call any API function after calling this function.

   All child sessions must be disjoined before closing a parent session.
   @param[in] session session handle.

   @return MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXClose(mfxSession session);

/*!
   @brief Returns the implementation type of a given session.

   @param[in]  session Session handle.
   @param[out] impl    Pointer to the implementation type

   @return MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXQueryIMPL(mfxSession session, mfxIMPL *impl);

/*!
   @brief Returns the implementation version.

   @param[in]  session Session handle.
   @param[out] version Pointer to the returned implementation version.

   @return MFX_ERR_NONE The function completed successfully.

   @since This function is available since API version 1.0.
*/
mfxStatus MFX_CDECL MFXQueryVersion(mfxSession session, mfxVersion *version);

/*!
   @brief Joins the child session to the current session.

   After joining, the two sessions share thread and resource scheduling for asynchronous
   operations. However, each session still maintains its own device manager and buffer/frame
   allocator. Therefore, the application must use a compatible device manager and buffer/frame
   allocator to share data between two joined sessions.

   The application can join multiple sessions by calling this function multiple times. When joining
   the first two sessions, the current session becomes the parent responsible for thread and
   resource scheduling of any later joined sessions.

   Joining of two parent sessions is not supported.

   @param[in,out] session    The current session handle.
   @param[in]     child      The child session handle to be joined

   @return MFX_ERR_NONE         The function completed successfully. \n
           MFX_WRN_IN_EXECUTION Active tasks are executing or in queue in one of the
                                sessions. Call this function again after all tasks are completed. \n
           MFX_ERR_UNSUPPORTED  The child session cannot be joined with the current session.

   @since This function is available since API version 1.1.
*/
mfxStatus MFX_CDECL MFXJoinSession(mfxSession session, mfxSession child);

/*!
   @brief Removes the joined state of the current session.

          After disjoining, the current session becomes independent. The application must ensure there is no active task running in the session before calling this API function.

   @param[in,out] session    The current session handle.

   @return MFX_ERR_NONE                The function completed successfully. \n
           MFX_WRN_IN_EXECUTION        Active tasks are executing or in queue in one of the
                                       sessions. Call this function again after all tasks are completed. \n
           MFX_ERR_UNDEFINED_BEHAVIOR  The session is independent, or this session is the parent of all joined sessions.

   @since This function is available since API version 1.1.
*/
mfxStatus MFX_CDECL MFXDisjoinSession(mfxSession session);

/*!
   @brief Creates a clean copy of the current session.

          The cloned session is an independent session and does not inherit any user-defined buffer, frame allocator, or device manager handles from the current session.
          This function is a light-weight equivalent of MFXJoinSession after MFXInit.

   @param[in] session    The current session handle.
   @param[out] clone     Pointer to the cloned session handle.

   @return MFX_ERR_NONE                The function completed successfully.

   @since This function is available since API version 1.1.
*/
mfxStatus MFX_CDECL MFXCloneSession(mfxSession session, mfxSession *clone);

/*!
   @brief Sets the current session priority.

   @param[in] session    The current session handle.
   @param[in] priority   Priority value.

   @return MFX_ERR_NONE                The function completed successfully.

   @since This function is available since API version 1.1.
*/
mfxStatus MFX_CDECL MFXSetPriority(mfxSession session, mfxPriority priority);

/*!
   @brief Returns the current session priority.

   @param[in] session    The current session handle.
   @param[out] priority   Pointer to the priority value.

   @return MFX_ERR_NONE                The function completed successfully.

   @since This function is available since API version 1.1.
*/
mfxStatus MFX_CDECL MFXGetPriority(mfxSession session, mfxPriority *priority);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

