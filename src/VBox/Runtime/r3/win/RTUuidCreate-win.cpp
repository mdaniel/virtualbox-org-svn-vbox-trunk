/* $Id$ */
/** @file
 * IPRT - UUID, Windows RTUuidCreate implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_UUID
#include <iprt/win/windows.h>

#include <iprt/uuid.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/rand.h>

#include "internal-r3-win.h"


RTDECL(int)  RTUuidCreate(PRTUUID pUuid)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(pUuid, VERR_INVALID_POINTER);

    /*
     * When using the UuidCreate API shortly after boot on NT 3.1 it typcially
     * hangs for a long long time while polling for some service to start.
     * What then usually happens next is a failure because it couldn't figure
     * out the MAC address of the NIC.  So, on NT 3.1 we always use the fallback.
     */
    if (g_enmWinVer != kRTWinOSType_NT310)
    {
        RPC_STATUS rc = UuidCreate((UUID *)pUuid);
        if (   rc == RPC_S_OK
            || rc == RPC_S_UUID_LOCAL_ONLY)
            return VINF_SUCCESS;
        AssertMsg(rc == RPC_S_UUID_NO_ADDRESS, ("UuidCreate -> %u (%#x)\n", rc, rc));
    }

    /*
     * Use generic implementation as fallback (copy of RTUuidCreate-generic.cpp).
     */
    RTRandBytes(pUuid, sizeof(*pUuid));
    pUuid->Gen.u8ClockSeqHiAndReserved = (pUuid->Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
    pUuid->Gen.u16TimeHiAndVersion     = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    return VINF_SUCCESS;
}

