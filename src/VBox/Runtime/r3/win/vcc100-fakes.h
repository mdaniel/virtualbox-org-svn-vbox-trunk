/* $Id$ */
/** @file
 * IPRT - Common macros for the Visual C++ 2010+ CRT import fakes.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___vcc100_fakes_h___
#define ___vcc100_fakes_h___

#ifdef RT_STRICT
# include <stdio.h> /* _snprintf */
#endif


/** @def MY_ASSERT
 * We use a special assertion macro here to avoid dragging in IPRT bits in
 * places which cannot handle it (direct GA 3D bits or something like that).
 */
#ifdef RT_STRICT
# define MY_ASSERT(a_Expr, ...) \
    do { \
        if ((a_Expr)) \
        { /*likely*/ } \
        else \
        { \
            char szTmp[256]; \
            _snprintf(szTmp, sizeof(szTmp), "Assertion failed on line %u in '%s': %s", __LINE__, __PRETTY_FUNCTION__, #a_Expr); \
            OutputDebugStringA(szTmp); \
            _snprintf(szTmp, sizeof(szTmp), __VA_ARGS__); \
            OutputDebugStringA(szTmp); \
            RT_BREAKPOINT(); \
        } \
    } while (0)
#else
# define MY_ASSERT(a_Expr, ...) do { } while (0)
#endif

/**
 * Lazily initializes the fakes.
 */
#define INIT_FAKES(a_ApiNm, a_Params) \
    do { \
        if (g_fInitialized) \
            MY_ASSERT(!RT_CONCAT(g_pfn, a_ApiNm), #a_ApiNm); \
        else \
            InitFakes(); \
        if (RT_CONCAT(g_pfn, a_ApiNm)) \
            return RT_CONCAT(g_pfn, a_ApiNm) a_Params; \
    } while (0)

/**
 * variant of INIT_FAKES for functions returning void.
 */
#define INIT_FAKES_VOID(a_ApiNm, a_Params) \
    do { \
        if (g_fInitialized) \
            MY_ASSERT(!RT_CONCAT(g_pfn, a_ApiNm), #a_ApiNm); \
        else \
            InitFakes(); \
        if (RT_CONCAT(g_pfn, a_ApiNm)) \
        { \
            RT_CONCAT(g_pfn, a_ApiNm) a_Params; \
            return; \
        } \
    } while (0)


/** Dynamically resolves an NTDLL API we need.   */
#define RESOLVE_NTDLL_API(ApiNm) \
    static bool volatile    s_fInitialized##ApiNm = false; \
    static decltype(ApiNm) *s_pfn##ApiNm = NULL; \
    decltype(ApiNm)        *pfn##ApiNm; \
    if (s_fInitialized##ApiNm) \
        pfn##ApiNm = s_pfn##ApiNm; \
    else \
    { \
        pfn##ApiNm = (decltype(pfn##ApiNm))GetProcAddress(GetModuleHandleW(L"ntdll"), #ApiNm); \
        s_pfn##ApiNm = pfn##ApiNm; \
        s_fInitialized##ApiNm = true; \
    } do {} while (0)


/** Declare a kernel32 API.
 * @note We are not exporting them as that causes duplicate symbol troubles in
 *       the OpenGL bits. */
#define DECL_KERNEL32(a_Type) extern "C" a_Type WINAPI


/** Ignore comments. */
#define COMMENT(a_Text)

/** Used for MAKE_IMPORT_ENTRY when declaring external g_pfnXxxx variables. */
#define DECLARE_FUNCTION_POINTER(a_Name, a_cb) extern "C" decltype(a_Name) *RT_CONCAT(g_pfn, a_Name);

/** Used in the InitFakes method to decl are uCurVersion used by assertion. */
#ifdef RT_STRICT
# define CURRENT_VERSION_VARIABLE() \
    unsigned uCurVersion = GetVersion(); \
    uCurVersion = ((uCurVersion & 0xff) << 8) | ((uCurVersion >> 8) & 0xff)
#else
# define CURRENT_VERSION_VARIABLE() (void)0
#endif


/** Used for MAKE_IMPORT_ENTRY when resolving an import. */
#define RESOLVE_IMPORT(a_uMajorVer, a_uMinorVer, a_Name, a_cb) \
        do { \
            FARPROC pfnApi = GetProcAddress(hmod, #a_Name); \
            if (pfnApi) \
                RT_CONCAT(g_pfn, a_Name) = (decltype(a_Name) *)pfnApi; \
            else \
                MY_ASSERT(uCurVersion < (((a_uMajorVer) << 8) | (a_uMinorVer)), #a_Name); \
        } while (0);


#endif

