/** @file
 * IPRT - Zero Memory.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_zero_h
#define IPRT_INCLUDED_zero_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/param.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_zero    RTZero - Zeroed Memory Objects
 * @ingroup grp_rt
 * @{
 */

/* Not available on linux.arm64 due to unknown page size in userspace. */
#if !defined(RT_ARCH_ARM64) && !defined(RT_OS_LINUX)
extern RTDATADECL(uint8_t const)   g_abRTZeroPage[PAGE_SIZE];
#endif
extern RTDATADECL(uint8_t const)   g_abRTZero4K[_4K];
extern RTDATADECL(uint8_t const)   g_abRTZero8K[_8K];
extern RTDATADECL(uint8_t const)   g_abRTZero16K[_16K];
extern RTDATADECL(uint8_t const)   g_abRTZero32K[_32K];
extern RTDATADECL(uint8_t const)   g_abRTZero64K[_64K];

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_zero_h */

