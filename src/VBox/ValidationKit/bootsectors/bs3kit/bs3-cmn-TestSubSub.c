/* $Id$ */
/** @file
 * BS3Kit - Bs3TestSubSub, Bs3TestSubSubF, Bs3TestSubSubV.
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



/**
 * Equivalent to RTTestISubSubV.
 */
#undef Bs3TestSubSubV
BS3_CMN_DEF(void, Bs3TestSubSubV,(const char *pszFormat, va_list BS3_FAR va))
{
    size_t cch;

    BS3_ASSERT(g_szBs3SubTest[0] != '\0');

    /*
     * Cleanup any previous sub-sub-test.
     */
    bs3TestSubSubCleanup();

    /*
     * Format the sub-test name and update globals.
     */
    cch = Bs3StrPrintfV(g_szBs3SubSubTest, sizeof(g_szBs3SubSubTest), pszFormat, va);
    g_cusBs3SubSubTestAtErrors = g_cusBs3TestErrors;
    BS3_ASSERT(!g_fbBs3SubSubTestSkipped);
    g_cusBs3SubSubTests++;

    /*
     * Tell VMMDev and output to the console.
     */
    bs3TestSendCmdWithStr(VMMDEV_TESTING_CMD_SUBSUB_NEW, g_szBs3SubSubTest);

    Bs3PrintStr(g_szBs3SubTest);
    cch += Bs3StrLen(g_szBs3SubTest) + 3;
    Bs3PrintStr(" / ");
    Bs3PrintStr(g_szBs3SubSubTest);
    Bs3PrintChr(':');
    do
       Bs3PrintChr(' ');
    while (cch++ < 48);
    Bs3PrintStr(" TESTING\n");

    /* The sub-sub-test result is not yet reported. */
    g_fbBs3SubSubTestReported = false;
}


/**
 * Equivalent to RTTestISubSubF.
 */
#undef Bs3TestSubSubF
BS3_CMN_DEF(void, Bs3TestSubSubF,(const char *pszFormat, ...))
{
    va_list va;
    va_start(va, pszFormat);
    BS3_CMN_NM(Bs3TestSubSubV)(pszFormat, va);
    va_end(va);
}


/**
 * Equivalent to RTTestISubSub.
 */
#undef Bs3TestSubSub
BS3_CMN_DEF(void, Bs3TestSubSub,(const char *pszMessage))
{
    BS3_CMN_NM(Bs3TestSubSubF)("%s", pszMessage);
}

