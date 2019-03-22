/****************************************************************************
* Copyright (C) 2014-2017 Intel Corporation.   All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
****************************************************************************/

#ifndef __SWR_OS_H__
#define __SWR_OS_H__

#include <cstddef>
#include "core/knobs.h"

#if (defined(FORCE_WINDOWS) || defined(_WIN32)) && !defined(FORCE_LINUX)

#define SWR_API __cdecl
#define SWR_VISIBLE  __declspec(dllexport)

#ifndef NOMINMAX
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#else
#include <windows.h>
#endif
#include <intrin.h>
#include <cstdint>

#if defined(MemoryFence)
// Windows.h defines MemoryFence as _mm_mfence, but this conflicts with llvm::sys::MemoryFence
#undef MemoryFence
#endif

#define OSALIGN(RWORD, WIDTH) __declspec(align(WIDTH)) RWORD

#if defined(_DEBUG)
// We compile Debug builds with inline function expansion enabled.  This allows
// functions compiled with __forceinline to be inlined even in Debug builds.
// The inline_depth(0) pragma below will disable inline function expansion for
// normal INLINE / inline functions, but not for __forceinline functions.
// Our SIMD function wrappers (see simdlib.hpp) use __forceinline even in
// Debug builds.
#define INLINE inline
#pragma inline_depth(0)
#else
#define INLINE __forceinline
#endif
#define DEBUGBREAK __debugbreak()

#define PRAGMA_WARNING_PUSH_DISABLE(...) \
    __pragma(warning(push));\
    __pragma(warning(disable:__VA_ARGS__));

#define PRAGMA_WARNING_POP() __pragma(warning(pop))

static inline void *AlignedMalloc(size_t _Size, size_t _Alignment)
{
    return _aligned_malloc(_Size, _Alignment);
}

static inline void AlignedFree(void* p)
{
    return _aligned_free(p);
}

#if defined(_WIN64)
#define BitScanReverseSizeT BitScanReverse64
#define BitScanForwardSizeT BitScanForward64
#define _mm_popcount_sizeT _mm_popcnt_u64
#else
#define BitScanReverseSizeT BitScanReverse
#define BitScanForwardSizeT BitScanForward
#define _mm_popcount_sizeT _mm_popcnt_u32
#endif

#elif defined(__APPLE__) || defined(FORCE_LINUX) || defined(__linux__) || defined(__gnu_linux__)

#define SWR_API
#define SWR_VISIBLE __attribute__((visibility("default")))

#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>

typedef void            VOID;
typedef void*           LPVOID;
typedef int             INT;
typedef unsigned int    UINT;
typedef void*           HANDLE;
typedef int             LONG;
typedef unsigned int    DWORD;

#undef FALSE
#define FALSE 0

#undef TRUE
#define TRUE 1

#define MAX_PATH PATH_MAX

#define OSALIGN(RWORD, WIDTH) RWORD __attribute__((aligned(WIDTH)))
#ifndef INLINE
#define INLINE __inline
#endif
#define DEBUGBREAK asm ("int $3")

#if !defined(__CYGWIN__)

#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
    #define __declspec(x)           __declspec_##x
    #define __declspec_align(y)     __attribute__((aligned(y)))
    #define __declspec_deprecated   __attribute__((deprecated))
    #define __declspec_dllexport
    #define __declspec_dllimport
    #define __declspec_noinline     __attribute__((__noinline__))
    #define __declspec_nothrow      __attribute__((nothrow))
    #define __declspec_novtable
    #define __declspec_thread       __thread
#else
    #define __declspec(X)
#endif

#endif

#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if !defined(__clang__) && (__GNUC__) && (GCC_VERSION < 40500)
inline
uint64_t __rdtsc()
{
    long low, high;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return (low | ((uint64_t)high << 32));
}
#endif

#if !defined( __clang__) && !defined(__INTEL_COMPILER)
// Intrinsic not defined in gcc
static INLINE
void _mm256_storeu2_m128i(__m128i *hi, __m128i *lo, __m256i a)
{
    _mm_storeu_si128((__m128i*)lo, _mm256_castsi256_si128(a));
    _mm_storeu_si128((__m128i*)hi, _mm256_extractf128_si256(a, 0x1));
}

// gcc prior to 4.9 doesn't have _mm*_undefined_*
#if (__GNUC__) && (GCC_VERSION < 409000)
#define _mm_undefined_si128 _mm_setzero_si128
#define _mm256_undefined_ps _mm256_setzero_ps
#endif
#endif

inline
unsigned char _BitScanForward(unsigned long *Index, unsigned long Mask)
{
    *Index = __builtin_ctz(Mask);
    return (Mask != 0);
}

inline
unsigned char _BitScanForward(unsigned int *Index, unsigned int Mask)
{
    *Index = __builtin_ctz(Mask);
    return (Mask != 0);
}

inline
unsigned char _BitScanReverse(unsigned long *Index, unsigned long Mask)
{
    *Index = __builtin_clz(Mask);
    return (Mask != 0);
}

inline
unsigned char _BitScanReverse(unsigned int *Index, unsigned int Mask)
{
    *Index = __builtin_clz(Mask);
    return (Mask != 0);
}

inline
void *AlignedMalloc(unsigned int size, unsigned int alignment)
{
    void *ret;
    if (posix_memalign(&ret, alignment, size))
    {
        return NULL;
    }
    return ret;
}

static inline
void AlignedFree(void* p)
{
    free(p);
}

#define _countof(a) (sizeof(a)/sizeof(*(a)))

#define sprintf_s sprintf
#define strcpy_s(dst,size,src) strncpy(dst,src,size)
#define GetCurrentProcessId getpid

#define InterlockedCompareExchange(Dest, Exchange, Comparand) __sync_val_compare_and_swap(Dest, Comparand, Exchange)
#define InterlockedExchangeAdd(Addend, Value) __sync_fetch_and_add(Addend, Value)
#define InterlockedDecrement(Append) __sync_sub_and_fetch(Append, 1)
#define InterlockedDecrement64(Append) __sync_sub_and_fetch(Append, 1)
#define InterlockedIncrement(Append) __sync_add_and_fetch(Append, 1)
#define InterlockedAdd(Addend, Value) __sync_add_and_fetch(Addend, Value)
#define InterlockedAdd64(Addend, Value) __sync_add_and_fetch(Addend, Value)
#define _ReadWriteBarrier() asm volatile("" ::: "memory")

#define PRAGMA_WARNING_PUSH_DISABLE(...)
#define PRAGMA_WARNING_POP()

#else

#error Unsupported OS/system.

#endif

#define THREAD thread_local

// Universal types
typedef uint8_t     KILOBYTE[1024];
typedef KILOBYTE    MEGABYTE[1024];
typedef MEGABYTE    GIGABYTE[1024];

#define OSALIGNLINE(RWORD) OSALIGN(RWORD, 64)
#define OSALIGNSIMD(RWORD) OSALIGN(RWORD, KNOB_SIMD_BYTES)
#if ENABLE_AVX512_SIMD16
#define OSALIGNSIMD16(RWORD) OSALIGN(RWORD, KNOB_SIMD16_BYTES)
#endif

#include "common/swr_assert.h"

#ifdef __GNUC__
#define ATTR_UNUSED __attribute__((unused))
#else
#define ATTR_UNUSED
#endif

#define SWR_FUNC(_retType, _funcName, /* args */...)   \
   typedef _retType (SWR_API * PFN##_funcName)(__VA_ARGS__); \
  _retType SWR_API _funcName(__VA_ARGS__);

// Defined in os.cpp
void SWR_API SetCurrentThreadName(const char* pThreadName);
void SWR_API CreateDirectoryPath(const std::string& path);

#endif//__SWR_OS_H__
