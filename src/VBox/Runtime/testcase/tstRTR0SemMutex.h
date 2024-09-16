/* $Id$ */
/** @file
 * IPRT R0 Testcase - Mutex Semaphores, common header.
 */

/*
 * Copyright (C) 2009-2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_SRC_testcase_tstRTR0SemMutex_h
#define IPRT_INCLUDED_SRC_testcase_tstRTR0SemMutex_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef IN_RING0
RT_C_DECLS_BEGIN
DECLEXPORT(int) TSTRTR0SemMutexSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation,
                                             uint64_t u64Arg, PSUPR0SERVICEREQHDR pReqHdr);
RT_C_DECLS_END
#endif

typedef enum TSTRTR0SEMMUTEX
{
    TSTRTR0SEMMUTEX_SANITY_OK = 1,
    TSTRTR0SEMMUTEX_SANITY_FAILURE,
    TSTRTR0SEMMUTEX_BASIC,
    TSTRTR0SEMMUTEX_TEST2_SETUP,
    TSTRTR0SEMMUTEX_TEST2_DO,
    TSTRTR0SEMMUTEX_TEST2_CLEANUP,
    TSTRTR0SEMMUTEX_TEST3_SETUP,
    TSTRTR0SEMMUTEX_TEST3_DO,
    TSTRTR0SEMMUTEX_TEST3_CLEANUP,
    TSTRTR0SEMMUTEX_TEST4_SETUP,
    TSTRTR0SEMMUTEX_TEST4_DO,
    TSTRTR0SEMMUTEX_TEST4_CLEANUP
} TSTRTR0SEMMUTEX;

#endif /* !IPRT_INCLUDED_SRC_testcase_tstRTR0SemMutex_h */

