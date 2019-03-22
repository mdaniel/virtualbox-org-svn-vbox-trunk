/* $Id$ */
/** @file
 * IPRT R0 Testcase - Kernel thread, common header.
 */

/*
 * Copyright (C) 2015-2017 Oracle Corporation
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

#ifndef RT_INCLUDED_testcase_tstRTR0Thread_h
#define RT_INCLUDED_testcase_tstRTR0Thread_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "tstRTR0CommonReq.h"

#ifdef IN_RING0
RT_C_DECLS_BEGIN
DECLEXPORT(int) TSTRTR0ThreadSrvReqHandler(PSUPDRVSESSION pSession, uint32_t uOperation, uint64_t u64Arg,
                                           PSUPR0SERVICEREQHDR pReqHdr);
RT_C_DECLS_END
#endif

typedef enum TSTR0THREAD
{
    TSTRTR0THREAD_SANITY_OK = RTTSTR0REQ_FIRST_USER,
    TSTRTR0THREAD_SANITY_FAILURE,
    TSTRTR0THREAD_BASIC
} TSTR0THREAD;

#endif
