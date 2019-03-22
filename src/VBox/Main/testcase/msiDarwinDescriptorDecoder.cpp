/* $Id$ */
/** @file
 * msiDarwinDescriptorDecoder
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#include <stdio.h>
#include <iprt/win/windows.h>  /* Avoid -Wall warnings. */



typedef DWORD (WINAPI *PFNMSIDECOMPOSEDESCRIPTORW)(PCWSTR pwszDescriptor,
                                                   LPWSTR pwszProductCode /*[40]*/,
                                                   LPWSTR pwszFeatureId /*[40]*/,
                                                   LPWSTR pwszComponentCode /*[40]*/,
                                                   DWORD *poffArguments);

int wmain(int cArgs, wchar_t **papwszArgs)
{
    HMODULE hmodMsi = LoadLibrary("msi.dll");
    PFNMSIDECOMPOSEDESCRIPTORW pfnMsiDecomposeDescriptorW;
    pfnMsiDecomposeDescriptorW = (PFNMSIDECOMPOSEDESCRIPTORW)GetProcAddress(hmodMsi, "MsiDecomposeDescriptorW");
    if (!pfnMsiDecomposeDescriptorW)
    {
        fprintf(stderr, "Failed to load msi.dll or resolve 'MsiDecomposeDescriptorW'\n");
        return 1;
    }

    int rcExit = 0;
    for (int iArg = 1; iArg < cArgs; iArg++)
    {
        wchar_t wszProductCode[40]   = { 0 };
        wchar_t wszFeatureId[40]     = { 0 };
        wchar_t wszComponentCode[40] = { 0 };
        DWORD   offArguments = ~(DWORD)0;
        DWORD dwErr = pfnMsiDecomposeDescriptorW(papwszArgs[iArg], wszProductCode, wszFeatureId, wszComponentCode, &offArguments);
        if (dwErr == 0)
        {
            fprintf(stderr,
                    "#%u: '%ls'\n"
                    " ->       Product=%ls\n"
                    " ->     FeatureId=%ls\n"
                    " -> ComponentCode=%ls\n"
                    " ->  offArguments=%#x (%d)\n"
                    , iArg, papwszArgs[iArg], wszProductCode, wszFeatureId, wszComponentCode, offArguments, offArguments);
        }
        else
        {
            fprintf(stderr,
                    "#%u: '%ls'\n"
                    " -> error %u (%#x)\n"
                    , iArg, papwszArgs[iArg], dwErr, dwErr);
            rcExit = 1;
        }
    }

    return rcExit;
}

