/** @file
 * Safe way to include ntddk.h.
 */

/*
 * Copyright (C) 2016-2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_nt_ntddk_h
#define IPRT_INCLUDED_nt_ntddk_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Make sure we get the right prototypes. */
#include <iprt/sanitized/intrin.h>

#ifdef _M_IX86
# define _InterlockedAddLargeStatistic _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#endif

#pragma warning(push)
#pragma warning(disable:4163)
#pragma warning(disable:4668) /* warning C4668: 'WHEA_DOWNLEVEL_TYPE_NAMES' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
#pragma warning(disable:4255) /* warning C4255: 'ObGetFilterVersion' : no function prototype given: converting '()' to '(void)' */
#if _MSC_VER >= 1800 /*RT_MSC_VER_VC120*/
# pragma warning(disable:4005) /* sdk/v7.1/include/sal_supp.h(57) : warning C4005: '__useHeader' : macro redefinition */
# pragma warning(disable:4471) /* wdm.h(11057) : warning C4471: '_POOL_TYPE' : a forward declaration of an unscoped enumeration must have an underlying type (int assumed) */
#endif

/* Include the sdk/ddk version header so _WIN32_VER and the rest gets defined before ntdef.h is included,
   otherwise we'll miss out on DECLARE_GLOBAL_CONST_UNICODE_STRING and friends in the W10 SDKs. */
#define DECLSPEC_DEPRECATED_DDK
#include <sdkddkver.h>

/*RT_C_DECLS_BEGIN - no longer necessary it seems */
#include <ntddk.h>
/*RT_C_DECLS_END - no longer necessary it seems */
#pragma warning(pop)

#ifdef _M_IX86
# undef _InterlockedAddLargeStatistic
#endif

#endif /* !IPRT_INCLUDED_nt_ntddk_h */

