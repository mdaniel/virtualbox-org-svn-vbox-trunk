/* $Id$ */
/** @file
 * IPRT R0 Testcase - Debug kernel information, common header.
 */

/*
 * Copyright (C) 2012-2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_SRC_testcase_tstRTR0DbgKrnlInfo_h
#define IPRT_INCLUDED_SRC_testcase_tstRTR0DbgKrnlInfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef IN_RING0
RT_C_DECLS_BEGIN
DECLEXPORT(int) TSTR0DbgKrnlInfoSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation,
                                              uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);
RT_C_DECLS_END
#endif

typedef enum TSTR0DBGKRNLINFO
{
    TSTRTR0DBGKRNLINFO_SANITY_OK = 1,
    TSTRTR0DBGKRNLINFO_SANITY_FAILURE,
    TSTRTR0DBGKRNLINFO_BASIC
} TSTR0DBGKRNLINFO;

#endif /* !IPRT_INCLUDED_SRC_testcase_tstRTR0DbgKrnlInfo_h */

