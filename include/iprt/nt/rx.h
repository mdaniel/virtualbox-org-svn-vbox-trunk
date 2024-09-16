/** @file
 * Safe way to include rx.h (DDK/IFS).
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

#ifndef IPRT_INCLUDED_nt_rx_h
#define IPRT_INCLUDED_nt_rx_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4668) /* rxovride.h(38) : warning C4668: 'DBG' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
# pragma warning(disable: 4100) /* Fcb.h(1558) : warning C4100: 'SrvOpen' : unreferenced formal parameter */
# pragma warning(disable: 4115) /* rxce.h(106) : warning C4115: '_ADAPTER_STATUS' : named type definition in parentheses */
# ifndef __cplusplus
#  pragma warning(disable: 4255) /* rxworkq.h(235) : warning C4255: 'RxInitializeDispatcher' : no function prototype given: converting '()' to '(void)' */
# endif
#endif

RT_C_DECLS_BEGIN

#include <rx.h>

RT_C_DECLS_END

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif /* !IPRT_INCLUDED_nt_rx_h */

