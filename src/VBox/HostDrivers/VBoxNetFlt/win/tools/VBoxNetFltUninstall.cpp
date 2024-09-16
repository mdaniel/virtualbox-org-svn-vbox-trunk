/* $Id$ */
/** @file
 * NetFltUninstall - VBoxNetFlt uninstaller command line tool
 */

/*
 * Copyright (C) 2008-2024 Oracle and/or its affiliates.
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
#include <VBox/VBoxNetCfg-win.h>
#include <stdio.h>

#include <iprt/initterm.h>
#include <iprt/message.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define NETFLT_ID L"sun_VBoxNetFlt"
#define VBOX_NETCFG_APP_NAME L"NetFltUninstall"
#define VBOX_NETFLT_PT_INF L".\\VBoxNetFlt.inf"
#define VBOX_NETFLT_MP_INF L".\\VBoxNetFltM.inf"
#define VBOX_NETFLT_RETRIES 10


static DECLCALLBACK(void) winNetCfgLogger(const char *pszString)
{
    printf("%s", pszString);
}

static int VBoxNetFltUninstall()
{
    INetCfg *pnc;
    int rcExit = RTEXITCODE_FAILURE;

    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if (hr == S_OK)
    {
        for (int i = 0;; i++)
        {
            LPWSTR pwszLockedBy = NULL;
            hr = VBoxNetCfgWinQueryINetCfg(&pnc, TRUE, VBOX_NETCFG_APP_NAME, 10000, &pwszLockedBy);
            if (hr == S_OK)
            {
                hr = VBoxNetCfgWinNetFltUninstall(pnc);
                if (hr != S_OK && hr != S_FALSE)
                    wprintf(L"error uninstalling VBoxNetFlt (%#lx)\n", hr);
                else
                {
                    wprintf(L"uninstalled successfully\n");
                    rcExit = RTEXITCODE_SUCCESS;
                }

                VBoxNetCfgWinReleaseINetCfg(pnc, TRUE);
                break;
            }

            if (hr == NETCFG_E_NO_WRITE_LOCK && pwszLockedBy)
            {
                if (i < VBOX_NETFLT_RETRIES && !wcscmp(pwszLockedBy, L"6to4svc.dll"))
                {
                    wprintf(L"6to4svc.dll is holding the lock, retrying %d out of %d\n", i + 1, VBOX_NETFLT_RETRIES);
                    CoTaskMemFree(pwszLockedBy);
                }
                else
                {
                    wprintf(L"Error: write lock is owned by another application (%s), close the application and retry uninstalling\n",
                            pwszLockedBy);
                    CoTaskMemFree(pwszLockedBy);
                    break;
                }
            }
            else
            {
                wprintf(L"Error getting the INetCfg interface (%#lx)\n", hr);
                break;
            }
        }

        CoUninitialize();
    }
    else
        wprintf(L"Error initializing COM (%#lx)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return rcExit;
}

int __cdecl main(int argc, char **argv)
{
    RTR3InitExeNoArguments(0);
    if (argc != 1)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "This utility takes no arguments\n");
    NOREF(argv);

    return VBoxNetFltUninstall();
}

