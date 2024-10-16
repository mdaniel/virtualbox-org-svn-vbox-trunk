/* $Id$ */
/** @file
 * VBoxWinDrvCommon - Common Windows driver functions.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_GuestHost_VBoxWinDrvCommon_h
#define VBOX_INCLUDED_GuestHost_VBoxWinDrvCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>
#include <iprt/win/setupapi.h>

#if defined(RT_ARCH_AMD64)
# define VBOXWINDRVINF_NATIVE_ARCH_STR     "AMD64"
#elif defined(RT_ARCH_X86)
# define VBOXWINDRVINF_NATIVE_ARCH_STR     "X86"
#elif defined(RT_ARCH_ARM64)
# define VBOXWINDRVINF_NATIVE_ARCH_STR     "ARM64"
#else
# error "Port me!"
#endif

/** Defines a string which emits the bulld target's native architeture, e.g. ".NTAMD64". */
#define VBOXWINDRVINF_DOT_NT_NATIVE_ARCH_STR ".NT" VBOXWINDRVINF_NATIVE_ARCH_STR

/** Maximum catalog file (.cat) length (in characters). */
#define VBOXWINDRVINF_MAX_CATALOG_FILE     255
/** Maximum driver version length (in characters). */
#define VBOXWINDRVINF_MAX_DRIVER_VER       255
/** Maximum provider name length (in characters). */
#define VBOXWINDRVINF_MAX_PROVIDER_FILE    255

/**
 * Structure for keeping INF Version section information.
 */
typedef struct _VBOXWINDRVINFSEC_VERSION
{
    /** Catalog (.cat) file. */
    RTUTF16 wszCatalogFile[VBOXWINDRVINF_MAX_CATALOG_FILE];
    /** Driver version. */
    RTUTF16 wszDriverVer[VBOXWINDRVINF_MAX_DRIVER_VER];
    /** Provider name. */
    RTUTF16 wszProvider[VBOXWINDRVINF_MAX_PROVIDER_FILE];
} VBOXWINDRVINFSEC_VERSION;
/** Pointer to structure for keeping INF Version section information. */
typedef VBOXWINDRVINFSEC_VERSION *PVBOXWINDRVINFSEC_VERSION;

int VBoxWinDrvInfOpenEx(PCRTUTF16 pwszInfFile, PRTUTF16 pwszClassName, HINF *phInf);
int VBoxWinDrvInfOpen(PCRTUTF16 pwszInfFile, HINF *phInf);
int VBoxWinDrvInfOpenUtf8(const char *pszInfFile, HINF *phInf);
int VBoxWinDrvInfClose(HINF hInf);
int VBoxWinDrvInfQueryFirstModel(HINF hInf, PRTUTF16 *ppwszModel);
int VBoxWinDrvInfQueryFirstPnPId(HINF hInf, PRTUTF16 pwszModel, PRTUTF16 *ppwszPnPId);
int VBoxWinDrvInfQueryKeyValue(PINFCONTEXT pCtx, DWORD iValue, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQueryModelsSectionName(HINF hInf, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQuerySectionVerEx(HINF hInf, UINT uIndex, PVBOXWINDRVINFSEC_VERSION pVer);
int VBoxWinDrvInfQuerySectionVer(HINF hInf, PVBOXWINDRVINFSEC_VERSION pVer);

#endif /* !VBOX_INCLUDED_GuestHost_VBoxWinDrvCommon_h */

