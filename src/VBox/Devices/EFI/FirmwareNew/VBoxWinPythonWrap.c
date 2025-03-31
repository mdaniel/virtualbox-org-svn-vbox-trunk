/* $Id$ */
/** @file
 * Windows wrapper program for kicking off a Python script in the build python interpreter.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
#include <Windows.h>


/**
 * This is the only code in this image.
 */
void __stdcall BareBoneEntrypoint(void)
{
    static WCHAR const  s_wszPythonExe[] = L"" VBOX_BLD_PYTHON;
    static WCHAR const  s_wszScript[]    = L"" PYTHON_SCRIPT;
    WCHAR const        *pwszArgs;
    unsigned            cwcArgs;
    unsigned            cbNeeded;
    WCHAR               wc;
    WCHAR              *pwszCmdLine;
    WCHAR const        *pwszSrc;
    WCHAR              *pwszDst;
    STARTUPINFOW        StartInfo        = { sizeof(StartInfo), NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL};
    PROCESS_INFORMATION ProcInfo         = { NULL, NULL, 0, 0 };
    DWORD               dwIgnored;

    /* Skip the executable name.*/
    pwszArgs = GetCommandLineW();
    if (*pwszArgs != '"')
        while ((wc = *pwszArgs) != '\0' && wc != ' ' && wc != '\t')
            pwszArgs++;
    else /* ASSUME it's all quoted and not complicated by the need to escape quotes. */
    {
        ++pwszArgs;
        while ((wc = *pwszArgs) != '\0')
        {
            pwszArgs++;
            if (wc == '"')
                break;
        }
    }

    /* Skip whitespace. */
    while ((wc = *pwszArgs) == ' ' || wc == '\t')
        pwszArgs++;

    /* Figure the length of the command line. */
    cwcArgs = 0;
    while (pwszArgs[cwcArgs] != '\0')
        cwcArgs++;

    /* Construct the new command line. */
    cbNeeded = sizeof(WCHAR) + sizeof(s_wszPythonExe) + 3 * sizeof(WCHAR) + sizeof(s_wszScript) + 2 * sizeof(WCHAR)
             + cwcArgs * sizeof(WCHAR) + 2 * sizeof(WCHAR); /* (two WCHARs too many) */
    pwszDst = pwszCmdLine = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, cbNeeded);
    *pwszDst++ = '"';
    pwszSrc = s_wszPythonExe;
    while ((wc = *pwszSrc++) != '\0')
        *pwszDst++ = wc;
    *pwszDst++ = '"';
    *pwszDst++ = ' ';
    *pwszDst++ = '"';
    pwszSrc = s_wszScript;
    while ((wc = *pwszSrc++) != '\0')
        *pwszDst++ = wc;
    *pwszDst++ = '"';
    *pwszDst++ = ' ';
    pwszSrc = pwszArgs;
    while ((wc = *pwszSrc++) != '\0')
        *pwszDst++ = wc;
    *pwszDst++ = '\0';
    *pwszDst   = '\0';

    /* Set PYTHONPATH. */
    SetEnvironmentVariableW(L"PYTHONPATH", L"" VBOX_PATH_EFI_FIRMWARE "/BaseTools/Source/Python");

    /* Make sure we've got the standard handles. */
    GetStartupInfoW(&StartInfo);
    StartInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    StartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    StartInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    StartInfo.dwFlags   |= STARTF_USESTDHANDLES;

    /* Create the process and wait for it to complete. */
    if (CreateProcessW(s_wszPythonExe, pwszCmdLine, NULL, NULL, TRUE /*bInheritHandles*/,
                       0 /*fFlags*/, NULL /*pwszzEnv*/, NULL /*pwszCwd*/, &StartInfo, &ProcInfo))
    {
        CloseHandle(ProcInfo.hThread);
        for (;;)
        {
            DWORD dwExitCode = 1;
            WaitForSingleObject(ProcInfo.hProcess, INFINITE);
            if (GetExitCodeProcess(ProcInfo.hProcess, &dwExitCode))
                for (;;)
                    ExitProcess(dwExitCode);
            Sleep(1);
        }
    }
    else
    {
        static const char s_szMsg[] = "error: CreateProcessW failed for " PYTHON_SCRIPT "\r\n";
        WriteFile(StartInfo.hStdError, s_szMsg, sizeof(s_szMsg) - 1, &dwIgnored, NULL);
    }
    ExitProcess(42);
}

