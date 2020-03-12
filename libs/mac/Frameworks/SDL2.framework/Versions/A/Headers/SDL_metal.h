/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 *  \file SDL_metal.h
 *
 *  Header file for functions to creating Metal layers and views on SDL windows.
 */

#ifndef SDL_metal_h_
#define SDL_metal_h_

#include "SDL_video.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \brief A handle to a CAMetalLayer-backed NSView (macOS) or UIView (iOS/tvOS).
 *
 *  \note This can be cast directly to an NSView or UIView.
 */
typedef void *SDL_MetalView;

/**
 *  \name Metal support functions
 */
/* @{ */

/**
 *  \brief Create a CAMetalLayer-backed NSView/UIView and attach it to the
 *        specified window.
 *
 *  On macOS, this does *not* associate a MTLDevice with the CAMetalLayer on its
 *  own. It is up to user code to do that.
 *
 *  The returned handle can be casted directly to a NSView or UIView, and the
 *  CAMetalLayer can be accessed from the view's 'layer' property.
 *
 *  \code
 *  SDL_MetalView metalview = SDL_Metal_CreateView(window);
 *  UIView *uiview = (__bridge UIView *)metalview;
 *  CAMetalLayer *metallayer = (CAMetalLayer *)uiview.layer;
 *  // [...]
 *  SDL_Metal_DestroyView(metalview);
 *  \endcode
 *
 *  \sa SDL_Metal_DestroyView
 */
extern DECLSPEC SDL_MetalView SDLCALL SDL_Metal_CreateView(SDL_Window * window);

/**
 *  \brief Destroy an existing SDL_MetalView object.
 *
 *  This should be called before SDL_DestroyWindow, if SDL_Metal_CreateView was
 *  called after SDL_CreateWindow.
 *
 *  \sa SDL_Metal_CreateView
 */
extern DECLSPEC void SDLCALL SDL_Metal_DestroyView(SDL_MetalView view);

/* @} *//* Metal support functions */

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_metal_h_ */
