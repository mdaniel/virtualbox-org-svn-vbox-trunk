/* $Id$ */
/** @file
 * IPRT Testcase - RTMemWipe* functions.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/path.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static void doMemWipeThoroughly(RTTEST hTest)
{
    for (uint32_t p = 0; p < RTRandU32Ex(1, 64); p++)
    {
        uint32_t cbAlloc = RTRandS32Ex(1, _1M) * sizeof(uint8_t);

        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Testing wipe #%.02RU32 (%u bytes) ...\n",
                     p + 1, cbAlloc);

        void *pvBuf = RTMemAlloc(cbAlloc);
        if (!pvBuf)
        {
            RTTestIFailed("No memory for first buffer (%z bytes)\n",
                          cbAlloc);
            continue;
        }
        RTRandBytes(pvBuf, cbAlloc);

        void *pvWipe = RTMemDup(pvBuf, cbAlloc);
        if (!pvWipe)
        {
            RTMemFree(pvBuf);

            RTTestIFailed("No memory for second buffer (%z bytes)\n",
                          cbAlloc);
            continue;
        }
        uint32_t cbWipeRand = RTRandU32Ex(1, cbAlloc);
        RTMemWipeThoroughly(pvWipe, RT_MIN(cbAlloc, cbWipeRand), p /* Passes */);
        if (!memcmp(pvWipe, pvBuf, cbAlloc))
            RTTestIFailed("Memory blocks must differ (%z bytes, 0x%p vs. 0x%p)!\n",
                          cbAlloc, pvWipe, pvBuf);

        RTMemFree(pvWipe);
        RTMemFree(pvBuf);
    }
}

int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("memwipe", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    doMemWipeThoroughly(hTest);

    return RTTestSummaryAndDestroy(hTest);
}

