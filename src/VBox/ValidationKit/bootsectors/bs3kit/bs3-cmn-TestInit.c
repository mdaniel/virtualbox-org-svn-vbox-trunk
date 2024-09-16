/* $Id$ */
/** @file
 * BS3Kit - Bs3TestInit
 */

/*
 * Copyright (C) 2007-2024 Oracle and/or its affiliates.
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
#include "bs3kit-template-header.h"
#include "bs3-cmn-test.h"
#include <iprt/asm-amd64-x86.h>


/**
 * Equivalent to RTTestCreate + RTTestBanner.
 *
 * @param   pszTest         The test name.
 */
#undef Bs3TestInit
BS3_CMN_DEF(void, Bs3TestInit,(const char BS3_FAR *pszTest))
{
    /*
     * Initialize the globals.
     */
    BS3_CMN_NM(g_pszBs3Test)    = pszTest;
    g_szBs3SubTest[0]           = '\0';
    g_cusBs3TestErrors          = 0;
    g_cusBs3SubTestAtErrors     = 0;
    g_fbBs3SubTestReported      = true;
    g_fbBs3SubTestSkipped       = false;
    g_cusBs3SubTests            = 0;
    g_cusBs3SubTestsFailed      = 0;
    g_fbBs3VMMDevTesting        = bs3TestIsVmmDevTestingPresent();
    if (g_fbBs3VMMDevTesting)
    {
        uint16_t uValue = Bs3TestQueryCfgU16(VMMDEV_TESTING_CFG_THRESHOLD_NATIVE_RECOMPILER, 0);
        if (uValue > 0 && uValue < 1024)
            g_cBs3ThresholdNativeRecompiler = uValue;
    }

    /*
     * Print the name - RTTestBanner.
     */
    Bs3PrintStr(pszTest);
    Bs3PrintStr(": TESTING...\n");

    /*
     * Report it to the VMMDev.
     */
    bs3TestSendCmdWithStr(VMMDEV_TESTING_CMD_INIT, pszTest);
}

