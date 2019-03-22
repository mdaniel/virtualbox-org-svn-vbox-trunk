/* $Id$ */
/** @file
 * IPRT - RTSystemQueryOSInfo, generic stub.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/win/windows.h>
#include <WinUser.h>

#include "internal-r3-win.h"
#include <iprt/system.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/string.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * These are the PRODUCT_* defines found in the Vista Platform SDK and returned
 * by GetProductInfo().
 *
 * We define them ourselves because we don't necessarily have any Vista PSDK around.
 */
typedef enum RTWINPRODTYPE
{
    kRTWinProdType_UNDEFINED                    = 0x00000000,  ///< An unknown product
    kRTWinProdType_BUSINESS                     = 0x00000006,  ///< Business Edition
    kRTWinProdType_BUSINESS_N                   = 0x00000010,  ///< Business Edition
    kRTWinProdType_CLUSTER_SERVER               = 0x00000012,  ///< Cluster Server Edition
    kRTWinProdType_DATACENTER_SERVER            = 0x00000008,  ///< Server Datacenter Edition (full installation)
    kRTWinProdType_DATACENTER_SERVER_CORE       = 0x0000000C,  ///< Server Datacenter Edition (core installation)
    kRTWinProdType_ENTERPRISE                   = 0x00000004,  ///< Enterprise Edition
    kRTWinProdType_ENTERPRISE_N                 = 0x0000001B,  ///< Enterprise Edition
    kRTWinProdType_ENTERPRISE_SERVER            = 0x0000000A,  ///< Server Enterprise Edition (full installation)
    kRTWinProdType_ENTERPRISE_SERVER_CORE       = 0x0000000E,  ///< Server Enterprise Edition (core installation)
    kRTWinProdType_ENTERPRISE_SERVER_IA64       = 0x0000000F,  ///< Server Enterprise Edition for Itanium-based Systems
    kRTWinProdType_HOME_BASIC                   = 0x00000002,  ///< Home Basic Edition
    kRTWinProdType_HOME_BASIC_N                 = 0x00000005,  ///< Home Basic Edition
    kRTWinProdType_HOME_PREMIUM                 = 0x00000003,  ///< Home Premium Edition
    kRTWinProdType_HOME_PREMIUM_N               = 0x0000001A,  ///< Home Premium Edition
    kRTWinProdType_HOME_SERVER                  = 0x00000013,  ///< Home Server Edition
    kRTWinProdType_SERVER_FOR_SMALLBUSINESS     = 0x00000018,  ///< Server for Small Business Edition
    kRTWinProdType_SMALLBUSINESS_SERVER         = 0x00000009,  ///< Small Business Server
    kRTWinProdType_SMALLBUSINESS_SERVER_PREMIUM = 0x00000019,  ///< Small Business Server Premium Edition
    kRTWinProdType_STANDARD_SERVER              = 0x00000007,  ///< Server Standard Edition (full installation)
    kRTWinProdType_STANDARD_SERVER_CORE         = 0x0000000D,  ///< Server Standard Edition (core installation)
    kRTWinProdType_STARTER                      = 0x0000000B,  ///< Starter Edition
    kRTWinProdType_STORAGE_ENTERPRISE_SERVER    = 0x00000017,  ///< Storage Server Enterprise Edition
    kRTWinProdType_STORAGE_EXPRESS_SERVER       = 0x00000014,  ///< Storage Server Express Edition
    kRTWinProdType_STORAGE_STANDARD_SERVER      = 0x00000015,  ///< Storage Server Standard Edition
    kRTWinProdType_STORAGE_WORKGROUP_SERVER     = 0x00000016,  ///< Storage Server Workgroup Edition
    kRTWinProdType_ULTIMATE                     = 0x00000001,  ///< Ultimate Edition
    kRTWinProdType_ULTIMATE_N                   = 0x0000001C,  ///< Ultimate Edition
    kRTWinProdType_WEB_SERVER                   = 0x00000011,  ///< Web Server Edition (full)
    kRTWinProdType_WEB_SERVER_CORE              = 0x0000001D   ///< Web Server Edition (core)
} RTWINPRODTYPE;


/**
 * Wrapper around the GetProductInfo API.
 *
 * @returns The vista type.
 */
static RTWINPRODTYPE rtSystemWinGetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion)
{
    BOOL (WINAPI *pfnGetProductInfo)(DWORD, DWORD, DWORD, DWORD, PDWORD);
    pfnGetProductInfo = (BOOL (WINAPI *)(DWORD, DWORD, DWORD, DWORD, PDWORD))GetProcAddress(GetModuleHandle("kernel32.dll"), "GetProductInfo");
    if (pfnGetProductInfo)
    {
        DWORD dwProductType = kRTWinProdType_UNDEFINED;
        if (pfnGetProductInfo(dwOSMajorVersion, dwOSMinorVersion, dwSpMajorVersion, dwSpMinorVersion, &dwProductType))
            return (RTWINPRODTYPE)dwProductType;
    }
    return kRTWinProdType_UNDEFINED;
}



/**
 * Appends the product type if available.
 *
 * @param   pszTmp      The buffer. Assumes it's big enough.
 */
static void rtSystemWinAppendProductType(char *pszTmp)
{
    RTWINPRODTYPE enmVistaType = rtSystemWinGetProductInfo(6, 0, 0, 0);
    switch (enmVistaType)
    {
        case kRTWinProdType_BUSINESS:                        strcat(pszTmp, " Business Edition"); break;
        case kRTWinProdType_BUSINESS_N:                      strcat(pszTmp, " Business Edition"); break;
        case kRTWinProdType_CLUSTER_SERVER:                  strcat(pszTmp, " Cluster Server Edition"); break;
        case kRTWinProdType_DATACENTER_SERVER:               strcat(pszTmp, " Server Datacenter Edition (full installation)"); break;
        case kRTWinProdType_DATACENTER_SERVER_CORE:          strcat(pszTmp, " Server Datacenter Edition (core installation)"); break;
        case kRTWinProdType_ENTERPRISE:                      strcat(pszTmp, " Enterprise Edition"); break;
        case kRTWinProdType_ENTERPRISE_N:                    strcat(pszTmp, " Enterprise Edition"); break;
        case kRTWinProdType_ENTERPRISE_SERVER:               strcat(pszTmp, " Server Enterprise Edition (full installation)"); break;
        case kRTWinProdType_ENTERPRISE_SERVER_CORE:          strcat(pszTmp, " Server Enterprise Edition (core installation)"); break;
        case kRTWinProdType_ENTERPRISE_SERVER_IA64:          strcat(pszTmp, " Server Enterprise Edition for Itanium-based Systems"); break;
        case kRTWinProdType_HOME_BASIC:                      strcat(pszTmp, " Home Basic Edition"); break;
        case kRTWinProdType_HOME_BASIC_N:                    strcat(pszTmp, " Home Basic Edition"); break;
        case kRTWinProdType_HOME_PREMIUM:                    strcat(pszTmp, " Home Premium Edition"); break;
        case kRTWinProdType_HOME_PREMIUM_N:                  strcat(pszTmp, " Home Premium Edition"); break;
        case kRTWinProdType_HOME_SERVER:                     strcat(pszTmp, " Home Server Edition"); break;
        case kRTWinProdType_SERVER_FOR_SMALLBUSINESS:        strcat(pszTmp, " Server for Small Business Edition"); break;
        case kRTWinProdType_SMALLBUSINESS_SERVER:            strcat(pszTmp, " Small Business Server"); break;
        case kRTWinProdType_SMALLBUSINESS_SERVER_PREMIUM:    strcat(pszTmp, " Small Business Server Premium Edition"); break;
        case kRTWinProdType_STANDARD_SERVER:                 strcat(pszTmp, " Server Standard Edition (full installation)"); break;
        case kRTWinProdType_STANDARD_SERVER_CORE:            strcat(pszTmp, " Server Standard Edition (core installation)"); break;
        case kRTWinProdType_STARTER:                         strcat(pszTmp, " Starter Edition"); break;
        case kRTWinProdType_STORAGE_ENTERPRISE_SERVER:       strcat(pszTmp, " Storage Server Enterprise Edition"); break;
        case kRTWinProdType_STORAGE_EXPRESS_SERVER:          strcat(pszTmp, " Storage Server Express Edition"); break;
        case kRTWinProdType_STORAGE_STANDARD_SERVER:         strcat(pszTmp, " Storage Server Standard Edition"); break;
        case kRTWinProdType_STORAGE_WORKGROUP_SERVER:        strcat(pszTmp, " Storage Server Workgroup Edition"); break;
        case kRTWinProdType_ULTIMATE:                        strcat(pszTmp, " Ultimate Edition"); break;
        case kRTWinProdType_ULTIMATE_N:                      strcat(pszTmp, " Ultimate Edition"); break;
        case kRTWinProdType_WEB_SERVER:                      strcat(pszTmp, " Web Server Edition (full installation)"); break;
        case kRTWinProdType_WEB_SERVER_CORE:                 strcat(pszTmp, " Web Server Edition (core installation)"); break;
        case kRTWinProdType_UNDEFINED:                       break;
    }
}


/**
 * Services the  RTSYSOSINFO_PRODUCT, RTSYSOSINFO_RELEASE
 * and RTSYSOSINFO_SERVICE_PACK requests.
 *
 * @returns See RTSystemQueryOSInfo.
 * @param   enmInfo         See RTSystemQueryOSInfo.
 * @param   pszInfo         See RTSystemQueryOSInfo.
 * @param   cchInfo         See RTSystemQueryOSInfo.
 */
static int rtSystemWinQueryOSVersion(RTSYSOSINFO enmInfo, char *pszInfo, size_t cchInfo)
{
    /*
     * Make sure it's terminated correctly in case of error.
     */
    *pszInfo = '\0';

    /*
     * Check that we got the windows version at init time.
     */
    AssertReturn(g_WinOsInfoEx.dwOSVersionInfoSize, VERR_WRONG_ORDER);

    /*
     * Service the request.
     */
    char szTmp[512];
    szTmp[0] = '\0';
    switch (enmInfo)
    {
        /*
         * The product name.
         */
        case RTSYSOSINFO_PRODUCT:
        {
            switch (g_enmWinVer)
            {
                case kRTWinOSType_95:           strcpy(szTmp, "Windows 95"); break;
                case kRTWinOSType_95SP1:        strcpy(szTmp, "Windows 95 (Service Pack 1)"); break;
                case kRTWinOSType_95OSR2:       strcpy(szTmp, "Windows 95 (OSR 2)"); break;
                case kRTWinOSType_98:           strcpy(szTmp, "Windows 98"); break;
                case kRTWinOSType_98SP1:        strcpy(szTmp, "Windows 98 (Service Pack 1)"); break;
                case kRTWinOSType_98SE:         strcpy(szTmp, "Windows 98 (Second Edition)"); break;
                case kRTWinOSType_ME:           strcpy(szTmp, "Windows Me"); break;
                case kRTWinOSType_NT310:        strcpy(szTmp, "Windows NT 3.10"); break;
                case kRTWinOSType_NT350:        strcpy(szTmp, "Windows NT 3.50"); break;
                case kRTWinOSType_NT351:        strcpy(szTmp, "Windows NT 3.51"); break;
                case kRTWinOSType_NT4:          strcpy(szTmp, "Windows NT 4.0"); break;
                case kRTWinOSType_2K:           strcpy(szTmp, "Windows 2000"); break;
                case kRTWinOSType_XP:
                    strcpy(szTmp, "Windows XP");
                    if (g_WinOsInfoEx.wSuiteMask & VER_SUITE_PERSONAL)
                        strcat(szTmp, " Home");
                    if (    g_WinOsInfoEx.wProductType == VER_NT_WORKSTATION
                        && !(g_WinOsInfoEx.wSuiteMask & VER_SUITE_PERSONAL))
                        strcat(szTmp, " Professional");
#if 0 /** @todo fixme */
                    if (GetSystemMetrics(SM_MEDIACENTER))
                        strcat(szTmp, " Media Center");
#endif
                    break;

                case kRTWinOSType_2003:         strcpy(szTmp, "Windows 2003"); break;
                case kRTWinOSType_VISTA:
                {
                    strcpy(szTmp, "Windows Vista");
                    rtSystemWinAppendProductType(szTmp);
                    break;
                }
                case kRTWinOSType_2008:         strcpy(szTmp, "Windows 2008"); break;
                case kRTWinOSType_7:            strcpy(szTmp, "Windows 7"); break;
                case kRTWinOSType_2008R2:       strcpy(szTmp, "Windows 2008 R2"); break;
                case kRTWinOSType_8:            strcpy(szTmp, "Windows 8"); break;
                case kRTWinOSType_2012:         strcpy(szTmp, "Windows 2012"); break;
                case kRTWinOSType_81:           strcpy(szTmp, "Windows 8.1"); break;
                case kRTWinOSType_2012R2:       strcpy(szTmp, "Windows 2012 R2"); break;
                case kRTWinOSType_10:           strcpy(szTmp, "Windows 10"); break;
                case kRTWinOSType_2016:         strcpy(szTmp, "Windows 2016"); break;

                case kRTWinOSType_NT_UNKNOWN:
                    RTStrPrintf(szTmp, sizeof(szTmp), "Unknown NT v%u.%u",
                                g_WinOsInfoEx.dwMajorVersion, g_WinOsInfoEx.dwMinorVersion);
                    break;

                default:
                    AssertFailed();
                case kRTWinOSType_UNKNOWN:
                    RTStrPrintf(szTmp, sizeof(szTmp), "Unknown %d v%u.%u",
                                g_WinOsInfoEx.dwPlatformId, g_WinOsInfoEx.dwMajorVersion, g_WinOsInfoEx.dwMinorVersion);
                    break;
            }
            break;
        }

        /*
         * The release.
         */
        case RTSYSOSINFO_RELEASE:
        {
            RTStrPrintf(szTmp, sizeof(szTmp), "%u.%u.%u",
                        g_WinOsInfoEx.dwMajorVersion, g_WinOsInfoEx.dwMinorVersion, g_WinOsInfoEx.dwBuildNumber);
            break;
        }


        /*
         * Get the service pack.
         */
        case RTSYSOSINFO_SERVICE_PACK:
        {
            if (g_WinOsInfoEx.wServicePackMajor)
            {
                if (g_WinOsInfoEx.wServicePackMinor)
                    RTStrPrintf(szTmp, sizeof(szTmp), "%u.%u",
                                (unsigned)g_WinOsInfoEx.wServicePackMajor, (unsigned)g_WinOsInfoEx.wServicePackMinor);
                else
                    RTStrPrintf(szTmp, sizeof(szTmp), "%u",
                                (unsigned)g_WinOsInfoEx.wServicePackMajor);
            }
            else if (g_WinOsInfoEx.szCSDVersion[0])
            {
                /* just copy the entire string. */
                char *pszTmp = szTmp;
                int rc = RTUtf16ToUtf8Ex(g_WinOsInfoEx.szCSDVersion, RT_ELEMENTS(g_WinOsInfoEx.szCSDVersion),
                                         &pszTmp, sizeof(szTmp), NULL);
                if (RT_SUCCESS(rc))
                    RTStrStripR(szTmp);
                else
                    szTmp[0] = '\0';
                AssertCompile(sizeof(szTmp) > sizeof(g_WinOsInfoEx.szCSDVersion));
            }
            else
            {
                switch (g_enmWinVer)
                {
                    case kRTWinOSType_95SP1:    strcpy(szTmp, "1"); break;
                    case kRTWinOSType_98SP1:    strcpy(szTmp, "1"); break;
                    default:
                        break;
                }
            }
            break;
        }

        default:
            AssertFatalFailed();
    }

    /*
     * Copy the result to the return buffer.
     */
    size_t cchTmp = strlen(szTmp);
    Assert(cchTmp < sizeof(szTmp));
    if (cchTmp < cchInfo)
    {
        memcpy(pszInfo, szTmp, cchTmp + 1);
        return VINF_SUCCESS;
    }
    memcpy(pszInfo, szTmp, cchInfo - 1);
    pszInfo[cchInfo - 1] = '\0';
    return VERR_BUFFER_OVERFLOW;
}



RTDECL(int) RTSystemQueryOSInfo(RTSYSOSINFO enmInfo, char *pszInfo, size_t cchInfo)
{
    /*
     * Quick validation.
     */
    AssertReturn(enmInfo > RTSYSOSINFO_INVALID && enmInfo < RTSYSOSINFO_END, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszInfo, VERR_INVALID_POINTER);
    if (!cchInfo)
        return VERR_BUFFER_OVERFLOW;


    /*
     * Handle the request.
     */
    switch (enmInfo)
    {
        case RTSYSOSINFO_PRODUCT:
        case RTSYSOSINFO_RELEASE:
        case RTSYSOSINFO_SERVICE_PACK:
            return rtSystemWinQueryOSVersion(enmInfo, pszInfo, cchInfo);

        case RTSYSOSINFO_VERSION:
        default:
            *pszInfo = '\0';
    }

    return VERR_NOT_SUPPORTED;
}


RTDECL(uint32_t) RTSystemGetNtBuildNo(void)
{
    return g_WinOsInfoEx.dwBuildNumber;
}


RTDECL(uint64_t) RTSystemGetNtVersion(void)
{
    return RTSYSTEM_MAKE_NT_VERSION(g_WinOsInfoEx.dwMajorVersion, g_WinOsInfoEx.dwMinorVersion, g_WinOsInfoEx.dwBuildNumber);
}

