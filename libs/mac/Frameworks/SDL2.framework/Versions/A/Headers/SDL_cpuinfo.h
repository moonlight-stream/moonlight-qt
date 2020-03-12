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
 *  \file SDL_cpuinfo.h
 *
 *  CPU feature detection for SDL.
 */

#ifndef SDL_cpuinfo_h_
#define SDL_cpuinfo_h_

#include "SDL_stdinc.h"

/* Need to do this here because intrin.h has C++ code in it */
/* Visual Studio 2005 has a bug where intrin.h conflicts with winnt.h */
#if defined(_MSC_VER) && (_MSC_VER >= 1500) && (defined(_M_IX86) || defined(_M_X64))
#ifdef __clang__
/* Many of the intrinsics SDL uses are not implemented by clang with Visual Studio */
#undef __MMX__
#undef __SSE__
#undef __SSE2__
#else
#include <intrin.h>
#ifndef _WIN64
#ifndef __MMX__
#define __MMX__
#endif
#ifndef __3dNOW__
#define __3dNOW__
#endif
#endif
#ifndef __SSE__
#define __SSE__
#endif
#ifndef __SSE2__
#define __SSE2__
#endif
#endif /* __clang__ */
#elif defined(__MINGW64_VERSION_MAJOR)
#include <intrin.h>
#else
/* altivec.h redefining bool causes a number of problems, see bugs 3993 and 4392, so you need to explicitly define SDL_ENABLE_ALTIVEC_H to have it included. */
#if defined(HAVE_ALTIVEC_H) && defined(__ALTIVEC__) && !defined(__APPLE_ALTIVEC__) && defined(SDL_ENABLE_ALTIVEC_H)
#include <altivec.h>
#endif
#if !defined(SDL_DISABLE_ARM_NEON_H)
#  if defined(__ARM_NEON)
#    include <arm_neon.h>
#  elif defined(__WINDOWS__) || defined(__WINRT__)
/* Visual Studio doesn't define __ARM_ARCH, but _M_ARM (if set, always 7), and _M_ARM64 (if set, always 1). */
#    if defined(_M_ARM)
#      include <armintr.h>
#      include <arm_neon.h>
#      define __ARM_NEON 1 /* Set __ARM_NEON so that it can be used elsewhere, at compile time */
#    endif
#    if defined (_M_ARM64)
#      include <arm64intr.h>
#      include <arm64_neon.h>
#      define __ARM_NEON 1 /* Set __ARM_NEON so that it can be used elsewhere, at compile time */
#    endif
#  endif
#endif
#if defined(__3dNOW__) && !defined(SDL_DISABLE_MM3DNOW_H)
#include <mm3dnow.h>
#endif
#if defined(HAVE_IMMINTRIN_H) && !defined(SDL_DISABLE_IMMINTRIN_H)
#include <immintrin.h>
#else
#if defined(__MMX__) && !defined(SDL_DISABLE_MMINTRIN_H)
#include <mmintrin.h>
#endif
#if defined(__SSE__) && !defined(SDL_DISABLE_XMMINTRIN_H)
#include <xmmintrin.h>
#endif
#if defined(__SSE2__) && !defined(SDL_DISABLE_EMMINTRIN_H)
#include <emmintrin.h>
#endif
#if defined(__SSE3__) && !defined(SDL_DISABLE_PMMINTRIN_H)
#include <pmmintrin.h>
#endif
#endif /* HAVE_IMMINTRIN_H */
#endif /* compiler version */

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* This is a guess for the cacheline size used for padding.
 * Most x86 processors have a 64 byte cache line.
 * The 64-bit PowerPC processors have a 128 byte cache line.
 * We'll use the larger value to be generally safe.
 */
#define SDL_CACHELINE_SIZE  128

/**
 *  This function returns the number of CPU cores available.
 */
extern DECLSPEC int SDLCALL SDL_GetCPUCount(void);

/**
 *  This function returns the L1 cache line size of the CPU
 *
 *  This is useful for determining multi-threaded structure padding
 *  or SIMD prefetch sizes.
 */
extern DECLSPEC int SDLCALL SDL_GetCPUCacheLineSize(void);

/**
 *  This function returns true if the CPU has the RDTSC instruction.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasRDTSC(void);

/**
 *  This function returns true if the CPU has AltiVec features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasAltiVec(void);

/**
 *  This function returns true if the CPU has MMX features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasMMX(void);

/**
 *  This function returns true if the CPU has 3DNow! features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_Has3DNow(void);

/**
 *  This function returns true if the CPU has SSE features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE(void);

/**
 *  This function returns true if the CPU has SSE2 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE2(void);

/**
 *  This function returns true if the CPU has SSE3 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE3(void);

/**
 *  This function returns true if the CPU has SSE4.1 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE41(void);

/**
 *  This function returns true if the CPU has SSE4.2 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE42(void);

/**
 *  This function returns true if the CPU has AVX features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasAVX(void);

/**
 *  This function returns true if the CPU has AVX2 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasAVX2(void);

/**
 *  This function returns true if the CPU has AVX-512F (foundation) features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasAVX512F(void);

/**
 *  This function returns true if the CPU has ARM SIMD (ARMv6) features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasARMSIMD(void);

/**
 *  This function returns true if the CPU has NEON (ARM SIMD) features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasNEON(void);

/**
 *  This function returns the amount of RAM configured in the system, in MB.
 */
extern DECLSPEC int SDLCALL SDL_GetSystemRAM(void);

/**
 * \brief Report the alignment this system needs for SIMD allocations.
 *
 * This will return the minimum number of bytes to which a pointer must be
 *  aligned to be compatible with SIMD instructions on the current machine.
 *  For example, if the machine supports SSE only, it will return 16, but if
 *  it supports AVX-512F, it'll return 64 (etc). This only reports values for
 *  instruction sets SDL knows about, so if your SDL build doesn't have
 *  SDL_HasAVX512F(), then it might return 16 for the SSE support it sees and
 *  not 64 for the AVX-512 instructions that exist but SDL doesn't know about.
 *  Plan accordingly.
 */
extern DECLSPEC size_t SDLCALL SDL_SIMDGetAlignment(void);

/**
 * \brief Allocate memory in a SIMD-friendly way.
 *
 * This will allocate a block of memory that is suitable for use with SIMD
 *  instructions. Specifically, it will be properly aligned and padded for
 *  the system's supported vector instructions.
 *
 * The memory returned will be padded such that it is safe to read or write
 *  an incomplete vector at the end of the memory block. This can be useful
 *  so you don't have to drop back to a scalar fallback at the end of your
 *  SIMD processing loop to deal with the final elements without overflowing
 *  the allocated buffer.
 *
 * You must free this memory with SDL_FreeSIMD(), not free() or SDL_free()
 *  or delete[], etc.
 *
 * Note that SDL will only deal with SIMD instruction sets it is aware of;
 *  for example, SDL 2.0.8 knows that SSE wants 16-byte vectors
 *  (SDL_HasSSE()), and AVX2 wants 32 bytes (SDL_HasAVX2()), but doesn't
 *  know that AVX-512 wants 64. To be clear: if you can't decide to use an
 *  instruction set with an SDL_Has*() function, don't use that instruction
 *  set with memory allocated through here.
 *
 * SDL_AllocSIMD(0) will return a non-NULL pointer, assuming the system isn't
 *  out of memory.
 *
 *  \param len The length, in bytes, of the block to allocated. The actual
 *             allocated block might be larger due to padding, etc.
 * \return Pointer to newly-allocated block, NULL if out of memory.
 *
 * \sa SDL_SIMDAlignment
 * \sa SDL_SIMDFree
 */
extern DECLSPEC void * SDLCALL SDL_SIMDAlloc(const size_t len);

/**
 * \brief Deallocate memory obtained from SDL_SIMDAlloc
 *
 * It is not valid to use this function on a pointer from anything but
 *  SDL_SIMDAlloc(). It can't be used on pointers from malloc, realloc,
 *  SDL_malloc, memalign, new[], etc.
 *
 * However, SDL_SIMDFree(NULL) is a legal no-op.
 *
 * \sa SDL_SIMDAlloc
 */
extern DECLSPEC void SDLCALL SDL_SIMDFree(void *ptr);

/* vi: set ts=4 sw=4 expandtab: */
/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_cpuinfo_h_ */

/* vi: set ts=4 sw=4 expandtab: */
