/* $Id$ */
/** @file
 * VBoxService - Virtual Machine Information for the Host, Windows specifics.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600
# undef  _WIN32_WINNT
# define _WIN32_WINNT 0x0600 /* QueryFullProcessImageNameW in recent SDKs. */
#endif
#include <iprt/win/windows.h>
#include <wtsapi32.h>        /* For WTS* calls. */
#include <psapi.h>           /* EnumProcesses. */
#include <Ntsecapi.h>        /* Needed for process security information. */

#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <iprt/localipc.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/utf16.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServiceVMInfo.h"
#include "../../WINNT/VBoxTray/VBoxTrayMsg.h" /* For IPC. */

static uint32_t s_uDebugGuestPropClientID = 0;
static uint32_t s_uDebugIter = 0;
/** Whether to skip the logged-in user detection over RDP or not.
 *  See notes in this section why we might want to skip this. */
static bool s_fSkipRDPDetection = false;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Structure for storing the looked up user information. */
typedef struct VBOXSERVICEVMINFOUSER
{
    WCHAR wszUser[_MAX_PATH];
    WCHAR wszAuthenticationPackage[_MAX_PATH];
    WCHAR wszLogonDomain[_MAX_PATH];
    /** Number of assigned user processes. */
    ULONG ulNumProcs;
    /** Last (highest) session ID. This
     *  is needed for distinguishing old session
     *  process counts from new (current) session
     *  ones. */
    ULONG ulLastSession;
} VBOXSERVICEVMINFOUSER, *PVBOXSERVICEVMINFOUSER;

/** Structure for the file information lookup. */
typedef struct VBOXSERVICEVMINFOFILE
{
    char *pszFilePath;
    char *pszFileName;
} VBOXSERVICEVMINFOFILE, *PVBOXSERVICEVMINFOFILE;

/** Structure for process information lookup. */
typedef struct VBOXSERVICEVMINFOPROC
{
    /** The PID. */
    DWORD id;
    /** The SID. */
    PSID pSid;
    /** The LUID. */
    LUID luid;
    /** Interactive process. */
    bool fInteractive;
} VBOXSERVICEVMINFOPROC, *PVBOXSERVICEVMINFOPROC;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static uint32_t vgsvcVMInfoWinSessionHasProcesses(PLUID pSession, PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs);
static bool vgsvcVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
static int  vgsvcVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
static void vgsvcVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs);
static int  vgsvcVMInfoWinWriteLastInput(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTONCE                                   g_vgsvcWinVmInitOnce = RTONCE_INITIALIZER;

/** @name Secur32.dll imports are dynamically resolved because of NT4.
 * @{ */
static decltype(LsaGetLogonSessionData)        *g_pfnLsaGetLogonSessionData = NULL;
static decltype(LsaEnumerateLogonSessions)     *g_pfnLsaEnumerateLogonSessions = NULL;
static decltype(LsaFreeReturnBuffer)           *g_pfnLsaFreeReturnBuffer = NULL;
/** @} */

/** @name WtsApi32.dll imports are dynamically resolved because of NT4.
 * @{ */
static decltype(WTSFreeMemory)                 *g_pfnWTSFreeMemory = NULL;
static decltype(WTSQuerySessionInformationA)   *g_pfnWTSQuerySessionInformationA = NULL;
/** @} */

/** @name PsApi.dll imports are dynamically resolved because of NT4.
 * @{ */
static decltype(EnumProcesses)                 *g_pfnEnumProcesses = NULL;
static decltype(GetModuleFileNameExW)          *g_pfnGetModuleFileNameExW = NULL;
/** @} */

/** @name New Kernel32.dll APIs we may use when present.
 * @{  */
static decltype(QueryFullProcessImageNameW)    *g_pfnQueryFullProcessImageNameW = NULL;

/** @} */

/** Windows version.  */
static OSVERSIONINFOEXA                         g_WinVersion;


/**
 * An RTOnce callback function.
 */
static DECLCALLBACK(int) vgsvcWinVmInfoInitOnce(void *pvIgnored)
{
    RT_NOREF1(pvIgnored);

    /* SECUR32 */
    RTLDRMOD hLdrMod;
    int rc = RTLdrLoadSystem("secur32.dll", true, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "LsaGetLogonSessionData", (void **)&g_pfnLsaGetLogonSessionData);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "LsaEnumerateLogonSessions", (void **)&g_pfnLsaEnumerateLogonSessions);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "LsaFreeReturnBuffer", (void **)&g_pfnLsaFreeReturnBuffer);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "Secur32.dll APIs are not available (%Rrc)\n", rc);
        g_pfnLsaGetLogonSessionData = NULL;
        g_pfnLsaEnumerateLogonSessions = NULL;
        g_pfnLsaFreeReturnBuffer = NULL;
        Assert(g_WinVersion.dwMajorVersion < 5);
    }

    /* WTSAPI32 */
    rc = RTLdrLoadSystem("wtsapi32.dll", true, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "WTSFreeMemory", (void **)&g_pfnWTSFreeMemory);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "WTSQuerySessionInformationA", (void **)&g_pfnWTSQuerySessionInformationA);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "WtsApi32.dll APIs are not available (%Rrc)\n", rc);
        g_pfnWTSFreeMemory = NULL;
        g_pfnWTSQuerySessionInformationA = NULL;
        Assert(g_WinVersion.dwMajorVersion < 5);
    }

    /* PSAPI */
    rc = RTLdrLoadSystem("psapi.dll", true, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "EnumProcesses", (void **)&g_pfnEnumProcesses);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "GetModuleFileNameExW", (void **)&g_pfnGetModuleFileNameExW);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "psapi.dll APIs are not available (%Rrc)\n", rc);
        g_pfnEnumProcesses = NULL;
        g_pfnGetModuleFileNameExW = NULL;
        Assert(g_WinVersion.dwMajorVersion < 5);
    }

    /* Kernel32: */
    rc = RTLdrLoadSystem("kernel32.dll", true, &hLdrMod);
    AssertRCReturn(rc, rc);
    rc = RTLdrGetSymbol(hLdrMod, "QueryFullProcessImageNameW", (void **)&g_pfnQueryFullProcessImageNameW);
    if (RT_FAILURE(rc))
    {
        Assert(g_WinVersion.dwMajorVersion < 6);
        g_pfnQueryFullProcessImageNameW = NULL;
    }
    RTLdrClose(hLdrMod);

    /*
     * Get the extended windows version once and for all.
     */
    g_WinVersion.dwOSVersionInfoSize = sizeof(g_WinVersion);
    if (!GetVersionExA((OSVERSIONINFO *)&g_WinVersion))
    {
        RT_ZERO(g_WinVersion);
        g_WinVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionExA((OSVERSIONINFO *)&g_WinVersion))
        {
            AssertFailed();
            RT_ZERO(g_WinVersion);
        }
    }

    return VINF_SUCCESS;
}


static bool vgsvcVMInfoSession0Separation(void)
{
    return g_WinVersion.dwPlatformId == VER_PLATFORM_WIN32_NT
        && g_WinVersion.dwMajorVersion >= 6; /* Vista = 6.0 */
}


/**
 * Retrieves the module name of a given process.
 *
 * @return  IPRT status code.
 */
static int vgsvcVMInfoWinProcessesGetModuleNameA(PVBOXSERVICEVMINFOPROC const pProc, PRTUTF16 *ppszName)
{
    AssertPtrReturn(pProc, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszName, VERR_INVALID_POINTER);

    /** @todo Only do this once. Later. */
    /* Platform other than NT (e.g. Win9x) not supported. */
    if (g_WinVersion.dwPlatformId != VER_PLATFORM_WIN32_NT)
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    DWORD dwFlags = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (g_WinVersion.dwMajorVersion >= 6 /* Vista or later */)
        dwFlags = PROCESS_QUERY_LIMITED_INFORMATION; /* possible to do on more processes */

    HANDLE h = OpenProcess(dwFlags, FALSE, pProc->id);
    if (h == NULL)
    {
        DWORD dwErr = GetLastError();
        if (g_cVerbosity)
            VGSvcError("Unable to open process with PID=%u, error=%u\n", pProc->id, dwErr);
        rc = RTErrConvertFromWin32(dwErr);
    }
    else
    {
        /* Since GetModuleFileNameEx has trouble with cross-bitness stuff (32-bit apps cannot query 64-bit
           apps and vice verse) we have to use a different code path for Vista and up. */
        WCHAR wszName[_1K];
        DWORD dwLen = sizeof(wszName); /** @todo r=bird: wrong? */

        /* Use QueryFullProcessImageNameW if available (Vista+). */
        if (g_pfnQueryFullProcessImageNameW)
        {
            if (!g_pfnQueryFullProcessImageNameW(h, 0 /*PROCESS_NAME_NATIVE*/, wszName, &dwLen))
                rc = VERR_ACCESS_DENIED;
        }
        else if (!g_pfnGetModuleFileNameExW(h, NULL /* Get main executable */, wszName, dwLen))
            rc = VERR_ACCESS_DENIED;

        if (   RT_FAILURE(rc)
            && g_cVerbosity > 3)
           VGSvcError("Unable to retrieve process name for PID=%u, error=%u\n", pProc->id, GetLastError());
        else
        {
            PRTUTF16 pszName = RTUtf16Dup(wszName);
            if (pszName)
                *ppszName = pszName;
            else
                rc = VERR_NO_MEMORY;
        }

        CloseHandle(h);
    }

    return rc;
}


/**
 * Fills in more data for a process.
 *
 * @returns VBox status code.
 * @param   pProc           The process structure to fill data into.
 * @param   tkClass         The kind of token information to get.
 */
static int vgsvcVMInfoWinProcessesGetTokenInfo(PVBOXSERVICEVMINFOPROC pProc, TOKEN_INFORMATION_CLASS tkClass)
{
    AssertPtrReturn(pProc, VERR_INVALID_POINTER);

    DWORD  dwErr = ERROR_SUCCESS;
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pProc->id);
    if (h == NULL)
    {
        dwErr = GetLastError();
        if (g_cVerbosity > 4)
            VGSvcError("Unable to open process with PID=%u, error=%u\n", pProc->id, dwErr);
        return RTErrConvertFromWin32(dwErr);
    }

    int    rc = VINF_SUCCESS;
    HANDLE hToken;
    if (OpenProcessToken(h, TOKEN_QUERY, &hToken))
    {
        void *pvTokenInfo = NULL;
        DWORD dwTokenInfoSize;
        switch (tkClass)
        {
            case TokenStatistics:
                /** @todo r=bird: Someone has been reading too many MSDN examples. You shall
                 *        use RTMemAlloc here!  There is absolutely not reason for
                 *        complicating things uncessarily by using HeapAlloc! */
                dwTokenInfoSize = sizeof(TOKEN_STATISTICS);
                pvTokenInfo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwTokenInfoSize);
                AssertPtr(pvTokenInfo);
                break;

            case TokenGroups:
                dwTokenInfoSize = 0;
                /* Allocation will follow in a second step. */
                break;

            case TokenUser:
                dwTokenInfoSize = 0;
                /* Allocation will follow in a second step. */
                break;

            default:
                VGSvcError("Token class not implemented: %d\n", tkClass);
                rc = VERR_NOT_IMPLEMENTED;
                dwTokenInfoSize = 0; /* Shut up MSC. */
                break;
        }

        if (RT_SUCCESS(rc))
        {
            DWORD dwRetLength;
            if (!GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
            {
                dwErr = GetLastError();
                if (dwErr == ERROR_INSUFFICIENT_BUFFER)
                {
                    dwErr = ERROR_SUCCESS;

                    switch (tkClass)
                    {
                        case TokenGroups:
                            pvTokenInfo = (PTOKEN_GROUPS)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwRetLength);
                            if (!pvTokenInfo)
                                dwErr = GetLastError();
                            dwTokenInfoSize = dwRetLength;
                            break;

                        case TokenUser:
                            pvTokenInfo = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwRetLength);
                            if (!pvTokenInfo)
                                dwErr = GetLastError();
                            dwTokenInfoSize = dwRetLength;
                            break;

                        default:
                            AssertMsgFailed(("Re-allocating of token information for token class not implemented\n"));
                            break;
                    }

                    if (dwErr == ERROR_SUCCESS)
                    {
                        if (!GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
                            dwErr = GetLastError();
                    }
                }
            }

            if (dwErr == ERROR_SUCCESS)
            {
                rc = VINF_SUCCESS;

                switch (tkClass)
                {
                    case TokenStatistics:
                    {
                        PTOKEN_STATISTICS pStats = (PTOKEN_STATISTICS)pvTokenInfo;
                        AssertPtr(pStats);
                        memcpy(&pProc->luid, &pStats->AuthenticationId, sizeof(LUID));
                        /** @todo Add more information of TOKEN_STATISTICS as needed. */
                        break;
                    }

                    case TokenGroups:
                    {
                        pProc->fInteractive = false;

                        SID_IDENTIFIER_AUTHORITY sidAuthNT = SECURITY_NT_AUTHORITY;
                        PSID pSidInteractive = NULL; /*  S-1-5-4 */
                        if (!AllocateAndInitializeSid(&sidAuthNT, 1, 4, 0, 0, 0, 0, 0, 0, 0, &pSidInteractive))
                            dwErr = GetLastError();

                        PSID pSidLocal = NULL; /*  S-1-2-0 */
                        if (dwErr == ERROR_SUCCESS)
                        {
                            SID_IDENTIFIER_AUTHORITY sidAuthLocal = SECURITY_LOCAL_SID_AUTHORITY;
                            if (!AllocateAndInitializeSid(&sidAuthLocal, 1, 0, 0, 0, 0, 0, 0, 0, 0, &pSidLocal))
                                dwErr = GetLastError();
                        }

                        if (dwErr == ERROR_SUCCESS)
                        {
                            PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)pvTokenInfo;
                            AssertPtr(pGroups);
                            for (DWORD i = 0; i < pGroups->GroupCount; i++)
                            {
                                if (   EqualSid(pGroups->Groups[i].Sid, pSidInteractive)
                                    || EqualSid(pGroups->Groups[i].Sid, pSidLocal)
                                    || pGroups->Groups[i].Attributes & SE_GROUP_LOGON_ID)
                                {
                                    pProc->fInteractive = true;
                                    break;
                                }
                            }
                        }

                        if (pSidInteractive)
                            FreeSid(pSidInteractive);
                        if (pSidLocal)
                            FreeSid(pSidLocal);
                        break;
                    }

                    case TokenUser:
                    {
                        PTOKEN_USER pUser = (PTOKEN_USER)pvTokenInfo;
                        AssertPtr(pUser);

                        DWORD dwLength = GetLengthSid(pUser->User.Sid);
                        Assert(dwLength);
                        if (dwLength)
                        {
                            pProc->pSid = (PSID)HeapAlloc(GetProcessHeap(),
                                                          HEAP_ZERO_MEMORY, dwLength);
                            AssertPtr(pProc->pSid);
                            if (CopySid(dwLength, pProc->pSid, pUser->User.Sid))
                            {
                                if (!IsValidSid(pProc->pSid))
                                    dwErr = ERROR_INVALID_NAME;
                            }
                            else
                                dwErr = GetLastError();
                        }
                        else
                            dwErr = ERROR_NO_DATA;

                        if (dwErr != ERROR_SUCCESS)
                        {
                            VGSvcError("Error retrieving SID of process PID=%u: %u\n", pProc->id, dwErr);
                            if (pProc->pSid)
                            {
                                HeapFree(GetProcessHeap(), 0 /* Flags */, pProc->pSid);
                                pProc->pSid = NULL;
                            }
                        }
                        break;
                    }

                    default:
                        AssertMsgFailed(("Unhandled token information class\n"));
                        break;
                }
            }

            if (pvTokenInfo)
                HeapFree(GetProcessHeap(), 0 /* Flags */, pvTokenInfo);
        }
        CloseHandle(hToken);
    }
    else
        dwErr = GetLastError();

    if (dwErr != ERROR_SUCCESS)
    {
        if (g_cVerbosity)
            VGSvcError("Unable to query token information for PID=%u, error=%u\n", pProc->id, dwErr);
        rc = RTErrConvertFromWin32(dwErr);
    }

    CloseHandle(h);
    return rc;
}


/**
 * Enumerate all the processes in the system and get the logon user IDs for
 * them.
 *
 * @returns VBox status code.
 * @param   ppaProcs    Where to return the process snapshot.  This must be
 *                      freed by calling vgsvcVMInfoWinProcessesFree.
 *
 * @param   pcProcs     Where to store the returned process count.
 */
static int vgsvcVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppaProcs, PDWORD pcProcs)
{
    AssertPtr(ppaProcs);
    AssertPtr(pcProcs);

    if (!g_pfnEnumProcesses)
        return VERR_NOT_SUPPORTED;

    /*
     * Call EnumProcesses with an increasingly larger buffer until it all fits
     * or we think something is screwed up.
     */
    DWORD   cProcesses  = 64;
    PDWORD  paPID       = NULL;
    int     rc          = VINF_SUCCESS;
    do
    {
        /* Allocate / grow the buffer first. */
        cProcesses *= 2;
        void *pvNew = RTMemRealloc(paPID, cProcesses * sizeof(DWORD));
        if (!pvNew)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        paPID = (PDWORD)pvNew;

        /* Query the processes. Not the cbRet == buffer size means there could be more work to be done. */
        DWORD cbRet;
        if (!g_pfnEnumProcesses(paPID, cProcesses * sizeof(DWORD), &cbRet))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }
        if (cbRet < cProcesses * sizeof(DWORD))
        {
            cProcesses = cbRet / sizeof(DWORD);
            break;
        }
    } while (cProcesses <= _32K); /* Should be enough; see: http://blogs.technet.com/markrussinovich/archive/2009/07/08/3261309.aspx */

    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate out process structures and fill data into them.
         * We currently only try lookup their LUID's.
         */
        PVBOXSERVICEVMINFOPROC paProcs;
        paProcs = (PVBOXSERVICEVMINFOPROC)RTMemAllocZ(cProcesses * sizeof(VBOXSERVICEVMINFOPROC));
        if (paProcs)
        {
            for (DWORD i = 0; i < cProcesses; i++)
            {
                paProcs[i].id = paPID[i];
                paProcs[i].pSid = NULL;

                int rc2 = vgsvcVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenUser);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VGSvcError("Get token class 'user' for process %u failed, rc=%Rrc\n", paProcs[i].id, rc2);

                rc2 = vgsvcVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenGroups);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VGSvcError("Get token class 'groups' for process %u failed, rc=%Rrc\n", paProcs[i].id, rc2);

                rc2 = vgsvcVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenStatistics);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VGSvcError("Get token class 'statistics' for process %u failed, rc=%Rrc\n", paProcs[i].id, rc2);
            }

            /* Save number of processes */
            if (RT_SUCCESS(rc))
            {
                *pcProcs  = cProcesses;
                *ppaProcs = paProcs;
            }
            else
                vgsvcVMInfoWinProcessesFree(cProcesses, paProcs);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTMemFree(paPID);
    return rc;
}

/**
 * Frees the process structures returned by
 * vgsvcVMInfoWinProcessesEnumerate() before.
 *
 * @param   cProcs      Number of processes in paProcs.
 * @param   paProcs     The process array.
 */
static void vgsvcVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs)
{
    for (DWORD i = 0; i < cProcs; i++)
        if (paProcs[i].pSid)
        {
            HeapFree(GetProcessHeap(), 0 /* Flags */, paProcs[i].pSid);
            paProcs[i].pSid = NULL;
        }
    RTMemFree(paProcs);
}

/**
 * Determines whether the specified session has processes on the system.
 *
 * @returns Number of processes found for a specified session.
 * @param   pSession            The current user's SID.
 * @param   paProcs             The process snapshot.
 * @param   cProcs              The number of processes in the snaphot.
 * @param   puTerminalSession   Where to return terminal session number.
 *                              Optional.
 */
/** @todo r=bird: The 'Has' indicates a predicate function, which this is
 *        not.  Predicate functions always returns bool. */
static uint32_t vgsvcVMInfoWinSessionHasProcesses(PLUID pSession, PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs,
                                                  PULONG puTerminalSession)
{
    if (!pSession)
    {
        VGSvcVerbose(1, "Session became invalid while enumerating!\n");
        return 0;
    }
    if (!g_pfnLsaGetLogonSessionData)
        return 0;

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = g_pfnLsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        VGSvcError("Could not get logon session data! rcNt=%#x\n", rcNt);
        return 0;
    }

    if (!IsValidSid(pSessionData->Sid))
    {
       VGSvcError("User SID=%p is not valid\n", pSessionData->Sid);
       if (pSessionData)
           g_pfnLsaFreeReturnBuffer(pSessionData);
       return 0;
    }


    /*
     * Even if a user seems to be logged in, it could be a stale/orphaned logon
     * session. So check if we have some processes bound to it by comparing the
     * session <-> process LUIDs.
     */
    int rc = VINF_SUCCESS;
    uint32_t cProcessesFound = 0;
    for (DWORD i = 0; i < cProcs; i++)
    {
        PSID pProcSID = paProcs[i].pSid;
        if (   RT_SUCCESS(rc)
            && pProcSID
            && IsValidSid(pProcSID))
        {
            if (EqualSid(pSessionData->Sid, paProcs[i].pSid))
            {
                if (g_cVerbosity)
                {
                    PRTUTF16 pszName;
                    int rc2 = vgsvcVMInfoWinProcessesGetModuleNameA(&paProcs[i], &pszName);
                    VGSvcVerbose(4, "Session %RU32: PID=%u (fInt=%RTbool): %ls\n",
                                 pSessionData->Session, paProcs[i].id, paProcs[i].fInteractive,
                                 RT_SUCCESS(rc2) ? pszName : L"<Unknown>");
                    if (RT_SUCCESS(rc2))
                        RTUtf16Free(pszName);
                }

                if (paProcs[i].fInteractive)
                {
                    cProcessesFound++;
                    if (!g_cVerbosity) /* We want a bit more info on higher verbosity. */
                        break;
                }
            }
        }
    }

    if (puTerminalSession)
        *puTerminalSession = pSessionData->Session;

    g_pfnLsaFreeReturnBuffer(pSessionData);

    return cProcessesFound;
}


/**
 * Save and noisy string copy.
 *
 * @param   pwszDst             Destination buffer.
 * @param   cbDst               Size in bytes - not WCHAR count!
 * @param   pSrc                Source string.
 * @param   pszWhat             What this is. For the log.
 */
static void vgsvcVMInfoWinSafeCopy(PWCHAR pwszDst, size_t cbDst, LSA_UNICODE_STRING const *pSrc, const char *pszWhat)
{
    Assert(RT_ALIGN(cbDst, sizeof(WCHAR)) == cbDst);

    size_t cbCopy = pSrc->Length;
    if (cbCopy + sizeof(WCHAR) > cbDst)
    {
        VGSvcVerbose(0, "%s is too long - %u bytes, buffer %u bytes! It will be truncated.\n", pszWhat, cbCopy, cbDst);
        cbCopy = cbDst - sizeof(WCHAR);
    }
    if (cbCopy)
        memcpy(pwszDst, pSrc->Buffer, cbCopy);
    pwszDst[cbCopy / sizeof(WCHAR)] = '\0';
}


/**
 * Detects whether a user is logged on.
 *
 * @returns true if logged in, false if not (or error).
 * @param   pUserInfo           Where to return the user information.
 * @param   pSession            The session to check.
 */
static bool vgsvcVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER pUserInfo, PLUID pSession)
{
    AssertPtrReturn(pUserInfo, false);
    if (!pSession)
        return false;
    if (   !g_pfnLsaGetLogonSessionData
        || !g_pfnLsaNtStatusToWinError)
        return false;

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = g_pfnLsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG ulError = g_pfnLsaNtStatusToWinError(rcNt);
        switch (ulError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                /* If we don't have enough memory it's hard to judge whether the specified user
                 * is logged in or not, so just assume he/she's not. */
                VGSvcVerbose(3, "Not enough memory to retrieve logon session data!\n");
                break;

            case ERROR_NO_SUCH_LOGON_SESSION:
                /* Skip session data which is not valid anymore because it may have been
                 * already terminated. */
                break;

            default:
                VGSvcError("LsaGetLogonSessionData failed with error %u\n", ulError);
                break;
        }
        if (pSessionData)
            g_pfnLsaFreeReturnBuffer(pSessionData);
        return false;
    }
    if (!pSessionData)
    {
        VGSvcError("Invalid logon session data!\n");
        return false;
    }

    VGSvcVerbose(3, "Session data: Name=%ls, SessionID=%RU32, LogonID=%d,%u, LogonType=%u\n",
                 pSessionData->UserName.Buffer, pSessionData->Session,
                 pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart, pSessionData->LogonType);

    if (vgsvcVMInfoSession0Separation())
    {
        /* Starting at Windows Vista user sessions begin with session 1, so
         * ignore (stale) session 0 users. */
        if (   pSessionData->Session == 0
            /* Also check the logon time. */
            || pSessionData->LogonTime.QuadPart == 0)
        {
            g_pfnLsaFreeReturnBuffer(pSessionData);
            return false;
        }
    }

    /*
     * Only handle users which can login interactively or logged in
     * remotely over native RDP.
     */
    bool fFoundUser = false;
    if (   IsValidSid(pSessionData->Sid)
        && (   (SECURITY_LOGON_TYPE)pSessionData->LogonType == Interactive
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == RemoteInteractive
            /* Note: We also need CachedInteractive in case Windows cached the credentials
             *       or just wants to reuse them! */
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == CachedInteractive))
    {
        VGSvcVerbose(3, "Session LogonType=%u is supported -- looking up SID + type ...\n", pSessionData->LogonType);

        /*
         * Copy out relevant data.
         */
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszUser, sizeof(pUserInfo->wszUser), &pSessionData->UserName, "User name");
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszAuthenticationPackage, sizeof(pUserInfo->wszAuthenticationPackage),
                               &pSessionData->AuthenticationPackage, "Authentication pkg name");
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszLogonDomain, sizeof(pUserInfo->wszLogonDomain),
                               &pSessionData->LogonDomain, "Logon domain name");

        TCHAR           szOwnerName[_MAX_PATH]  = { 0 };
        DWORD           dwOwnerNameSize         = sizeof(szOwnerName);
        TCHAR           szDomainName[_MAX_PATH] = { 0 };
        DWORD           dwDomainNameSize        = sizeof(szDomainName);
        SID_NAME_USE    enmOwnerType            = SidTypeInvalid;
        if (!LookupAccountSid(NULL,
                              pSessionData->Sid,
                              szOwnerName,
                              &dwOwnerNameSize,
                              szDomainName,
                              &dwDomainNameSize,
                              &enmOwnerType))
        {
            DWORD dwErr = GetLastError();
            /*
             * If a network time-out prevents the function from finding the name or
             * if a SID that does not have a corresponding account name (such as a
             * logon SID that identifies a logon session), we get ERROR_NONE_MAPPED
             * here that we just skip.
             */
            if (dwErr != ERROR_NONE_MAPPED)
                VGSvcError("Failed looking up account info for user=%ls, error=$ld!\n", pUserInfo->wszUser, dwErr);
        }
        else
        {
            if (enmOwnerType == SidTypeUser) /* Only recognize users; we don't care about the rest! */
            {
                VGSvcVerbose(3, "Account User=%ls, Session=%u, LogonID=%d,%u, AuthPkg=%ls, Domain=%ls\n",
                             pUserInfo->wszUser, pSessionData->Session, pSessionData->LogonId.HighPart,
                             pSessionData->LogonId.LowPart, pUserInfo->wszAuthenticationPackage, pUserInfo->wszLogonDomain);

                /**
                 * Note: On certain Windows OSes WTSQuerySessionInformation leaks memory when used
                 * under a heavy stress situation. There are hotfixes available from Microsoft.
                 *
                 * See: http://support.microsoft.com/kb/970910
                 */
                if (!s_fSkipRDPDetection)
                {
                    /* Skip RDP detection on non-NT systems. */
                    if (g_WinVersion.dwPlatformId != VER_PLATFORM_WIN32_NT)
                        s_fSkipRDPDetection = true;

                    /* Skip RDP detection on Windows 2000.
                     * For Windows 2000 however we don't have any hotfixes, so just skip the
                     * RDP detection in any case. */
                    if (   g_WinVersion.dwMajorVersion == 5
                        && g_WinVersion.dwMinorVersion == 0)
                        s_fSkipRDPDetection = true;

                    /* Skip if we don't have the WTS API. */
                    if (!g_pfnWTSQuerySessionInformationA)
                        s_fSkipRDPDetection = true;

                    if (s_fSkipRDPDetection)
                        VGSvcVerbose(0, "Detection of logged-in users via RDP is disabled\n");
                }

                if (!s_fSkipRDPDetection)
                {
                    Assert(g_pfnWTSQuerySessionInformationA);
                    Assert(g_pfnWTSFreeMemory);

                    /* Detect RDP sessions as well. */
                    LPTSTR  pBuffer = NULL;
                    DWORD   cbRet   = 0;
                    int     iState  = -1;
                    if (g_pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE,
                                                         pSessionData->Session,
                                                         WTSConnectState,
                                                         &pBuffer,
                                                         &cbRet))
                    {
                        if (cbRet)
                            iState = *pBuffer;
                        VGSvcVerbose(3, "Account User=%ls, WTSConnectState=%d (%u)\n", pUserInfo->wszUser, iState, cbRet);
                        if (    iState == WTSActive           /* User logged on to WinStation. */
                             || iState == WTSShadow           /* Shadowing another WinStation. */
                             || iState == WTSDisconnected)    /* WinStation logged on without client. */
                        {
                            /** @todo On Vista and W2K, always "old" user name are still
                             *        there. Filter out the old one! */
                            VGSvcVerbose(3, "Account User=%ls using TCS/RDP, state=%d \n", pUserInfo->wszUser, iState);
                            fFoundUser = true;
                        }
                        if (pBuffer)
                            g_pfnWTSFreeMemory(pBuffer);
                    }
                    else
                    {
                        DWORD dwLastErr = GetLastError();
                        switch (dwLastErr)
                        {
                            /*
                             * Terminal services don't run (for example in W2K,
                             * nothing to worry about ...).  ... or is on the Vista
                             * fast user switching page!
                             */
                            case ERROR_CTX_WINSTATION_NOT_FOUND:
                                VGSvcVerbose(3, "No WinStation found for user=%ls\n", pUserInfo->wszUser);
                                break;

                            default:
                                VGSvcVerbose(3, "Cannot query WTS connection state for user=%ls, error=%u\n",
                                             pUserInfo->wszUser, dwLastErr);
                                break;
                        }

                        fFoundUser = true;
                    }
                }
            }
            else
                VGSvcVerbose(3, "SID owner type=%d not handled, skipping\n", enmOwnerType);
        }

        VGSvcVerbose(3, "Account User=%ls %s logged in\n", pUserInfo->wszUser, fFoundUser ? "is" : "is not");
    }

    if (fFoundUser)
        pUserInfo->ulLastSession = pSessionData->Session;

    g_pfnLsaFreeReturnBuffer(pSessionData);
    return fFoundUser;
}


static int vgsvcVMInfoWinWriteLastInput(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
    /* pszDomain is optional. */

    char szPipeName[512 + sizeof(VBOXTRAY_IPC_PIPE_PREFIX)];
    memcpy(szPipeName, VBOXTRAY_IPC_PIPE_PREFIX, sizeof(VBOXTRAY_IPC_PIPE_PREFIX));
    int rc = RTStrCat(szPipeName, sizeof(szPipeName), pszUser);
    if (RT_SUCCESS(rc))
    {
        bool fReportToHost = false;
        VBoxGuestUserState userState = VBoxGuestUserState_Unknown;

        RTLOCALIPCSESSION hSession;
        rc = RTLocalIpcSessionConnect(&hSession, szPipeName, RTLOCALIPC_FLAGS_NATIVE_NAME);
        if (RT_SUCCESS(rc))
        {
            VBOXTRAYIPCHEADER ipcHdr =
            {
                /* .uMagic      = */ VBOXTRAY_IPC_HDR_MAGIC,
                /* .uHdrVersion = */ 0,
                /* .uMsgType    = */ VBOXTRAYIPCMSGTYPE_USERLASTINPUT,
                /* .cbMsgData   = */ 0 /* No msg */
            };

            rc = RTLocalIpcSessionWrite(hSession, &ipcHdr, sizeof(ipcHdr));

            if (RT_SUCCESS(rc))
            {
                VBOXTRAYIPCRES_USERLASTINPUT ipcRes;
                rc = RTLocalIpcSessionRead(hSession, &ipcRes, sizeof(ipcRes), NULL /* Exact read */);
                if (   RT_SUCCESS(rc)
                    /* If uLastInput is set to UINT32_MAX VBoxTray was not able to retrieve the
                     * user's last input time. This might happen when running on Windows NT4 or older. */
                    && ipcRes.uLastInput != UINT32_MAX)
                {
                    userState = (ipcRes.uLastInput * 1000) < g_uVMInfoUserIdleThresholdMS
                              ? VBoxGuestUserState_InUse
                              : VBoxGuestUserState_Idle;

                    rc = VGSvcUserUpdateF(pCache, pszUser, pszDomain, "UsageState",
                                          userState == VBoxGuestUserState_InUse ? "InUse" : "Idle");

                    /*
                     * Note: vboxServiceUserUpdateF can return VINF_NO_CHANGE in case there wasn't anything
                     *       to update. So only report the user's status to host when we really got something
                     *       new.
                     */
                    fReportToHost = rc == VINF_SUCCESS;
                    VGSvcVerbose(4, "User '%s' (domain '%s') is idle for %RU32, fReportToHost=%RTbool\n",
                                 pszUser, pszDomain ? pszDomain : "<None>", ipcRes.uLastInput, fReportToHost);

#if 0 /* Do we want to write the idle time as well? */
                        /* Also write the user's current idle time, if there is any. */
                        if (userState == VBoxGuestUserState_Idle)
                            rc = vgsvcUserUpdateF(pCache, pszUser, pszDomain, "IdleTimeMs", "%RU32", ipcRes.uLastInputMs);
                        else
                            rc = vgsvcUserUpdateF(pCache, pszUser, pszDomain, "IdleTimeMs", NULL /* Delete property */);

                        if (RT_SUCCESS(rc))
#endif
                }
#ifdef DEBUG
                else if (RT_SUCCESS(rc) && ipcRes.uLastInput == UINT32_MAX)
                    VGSvcVerbose(4, "Last input for user '%s' is not supported, skipping\n", pszUser, rc);
#endif
            }
#ifdef DEBUG
            VGSvcVerbose(4, "Getting last input for user '%s' ended with rc=%Rrc\n", pszUser, rc);
#endif
            int rc2 = RTLocalIpcSessionClose(hSession);
            if (RT_SUCCESS(rc) && RT_FAILURE(rc2))
                rc = rc2;
        }
        else
        {
            switch (rc)
            {
                case VERR_FILE_NOT_FOUND:
                {
                    /* No VBoxTray (or too old version which does not support IPC) running
                       for the given user. Not much we can do then. */
                    VGSvcVerbose(4, "VBoxTray for user '%s' not running (anymore), no last input available\n", pszUser);

                    /* Overwrite rc from above. */
                    rc = VGSvcUserUpdateF(pCache, pszUser, pszDomain, "UsageState", "Idle");

                    fReportToHost = rc == VINF_SUCCESS;
                    if (fReportToHost)
                        userState = VBoxGuestUserState_Idle;
                    break;
                }

                default:
                    VGSvcError("Error querying last input for user '%s', rc=%Rrc\n", pszUser, rc);
                    break;
            }
        }

        if (fReportToHost)
        {
            Assert(userState != VBoxGuestUserState_Unknown);
            int rc2 = VbglR3GuestUserReportState(pszUser, pszDomain, userState, NULL /* No details */, 0);
            if (RT_FAILURE(rc2))
                VGSvcError("Error reporting usage state %d for user '%s' to host, rc=%Rrc\n", userState, pszUser, rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


/**
 * Retrieves the currently logged in users and stores their names along with the
 * user count.
 *
 * @returns VBox status code.
 * @param   pCache          Property cache to use for storing some of the lookup
 *                          data in between calls.
 * @param   ppszUserList    Where to store the user list (separated by commas).
 *                          Must be freed with RTStrFree().
 * @param   pcUsersInList   Where to store the number of users in the list.
 */
int VGSvcVMInfoWinWriteUsers(PVBOXSERVICEVEPROPCACHE pCache, char **ppszUserList, uint32_t *pcUsersInList)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszUserList, VERR_INVALID_POINTER);
    AssertPtrReturn(pcUsersInList, VERR_INVALID_POINTER);

    int rc = RTOnce(&g_vgsvcWinVmInitOnce, vgsvcWinVmInfoInitOnce, NULL);
    if (RT_FAILURE(rc))
        return rc;
    if (!g_pfnLsaEnumerateLogonSessions || !g_pfnEnumProcesses || !g_pfnLsaNtStatusToWinError)
        return VERR_NOT_SUPPORTED;

    rc = VbglR3GuestPropConnect(&s_uDebugGuestPropClientID);
    AssertRC(rc);

    char *pszUserList = NULL;
    uint32_t cUsersInList = 0;

    /* This function can report stale or orphaned interactive logon sessions
       of already logged off users (especially in Windows 2000). */
    PLUID    paSessions = NULL;
    ULONG    cSessions = 0;
    NTSTATUS rcNt = g_pfnLsaEnumerateLogonSessions(&cSessions, &paSessions);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG uError = g_pfnLsaNtStatusToWinError(rcNt);
        switch (uError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                VGSvcError("Not enough memory to enumerate logon sessions!\n");
                break;

            case ERROR_SHUTDOWN_IN_PROGRESS:
                /* If we're about to shutdown when we were in the middle of enumerating the logon
                 * sessions, skip the error to not confuse the user with an unnecessary log message. */
                VGSvcVerbose(3, "Shutdown in progress ...\n");
                uError = ERROR_SUCCESS;
                break;

            default:
                VGSvcError("LsaEnumerate failed with error %RU32\n", uError);
                break;
        }

        if (paSessions)
            g_pfnLsaFreeReturnBuffer(paSessions);

        return RTErrConvertFromWin32(uError);
    }
    VGSvcVerbose(3, "Found %u sessions\n", cSessions);

    PVBOXSERVICEVMINFOPROC  paProcs;
    DWORD                   cProcs;
    rc = vgsvcVMInfoWinProcessesEnumerate(&paProcs, &cProcs);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NO_MEMORY)
            VGSvcError("Not enough memory to enumerate processes\n");
        else
            VGSvcError("Failed to enumerate processes, rc=%Rrc\n", rc);
    }
    else
    {
        PVBOXSERVICEVMINFOUSER pUserInfo;
        pUserInfo = (PVBOXSERVICEVMINFOUSER)RTMemAllocZ(cSessions * sizeof(VBOXSERVICEVMINFOUSER) + 1);
        if (!pUserInfo)
            VGSvcError("Not enough memory to store enumerated users!\n");
        else
        {
            ULONG cUniqueUsers = 0;

            /*
             * Note: The cSessions loop variable does *not* correlate with
             *       the Windows session ID!
             */
            for (ULONG i = 0; i < cSessions; i++)
            {
                VGSvcVerbose(3, "Handling session %RU32 (of %RU32)\n", i + 1, cSessions);

                VBOXSERVICEVMINFOUSER userSession;
                if (vgsvcVMInfoWinIsLoggedIn(&userSession, &paSessions[i]))
                {
                    VGSvcVerbose(4, "Handling user=%ls, domain=%ls, package=%ls, session=%RU32\n",
                                 userSession.wszUser, userSession.wszLogonDomain, userSession.wszAuthenticationPackage,
                                 userSession.ulLastSession);

                    /* Retrieve assigned processes of current session. */
                    uint32_t cCurSessionProcs = vgsvcVMInfoWinSessionHasProcesses(&paSessions[i], paProcs, cProcs,
                                                                                  NULL /* Terminal session ID */);
                    /* Don't return here when current session does not have assigned processes
                     * anymore -- in that case we have to search through the unique users list below
                     * and see if got a stale user/session entry. */

                    if (g_cVerbosity > 3)
                    {
                        char szDebugSessionPath[255];
                        RTStrPrintf(szDebugSessionPath,  sizeof(szDebugSessionPath),
                                    "/VirtualBox/GuestInfo/Debug/LSA/Session/%RU32", userSession.ulLastSession);
                        VGSvcWritePropF(s_uDebugGuestPropClientID, szDebugSessionPath,
                                        "#%RU32: cSessionProcs=%RU32 (of %RU32 procs total)",
                                        s_uDebugIter, cCurSessionProcs, cProcs);
                    }

                    bool fFoundUser = false;
                    for (ULONG a = 0; a < cUniqueUsers; a++)
                    {
                        PVBOXSERVICEVMINFOUSER pCurUser = &pUserInfo[a];
                        AssertPtr(pCurUser);

                        if (   !wcscmp(userSession.wszUser, pCurUser->wszUser)
                            && !wcscmp(userSession.wszLogonDomain, pCurUser->wszLogonDomain)
                            && !wcscmp(userSession.wszAuthenticationPackage, pCurUser->wszAuthenticationPackage))
                        {
                            /*
                             * Only respect the highest session for the current user.
                             */
                            if (userSession.ulLastSession > pCurUser->ulLastSession)
                            {
                                VGSvcVerbose(4, "Updating user=%ls to %u processes (last used session: %RU32)\n",
                                                   pCurUser->wszUser, cCurSessionProcs, userSession.ulLastSession);

                                if (!cCurSessionProcs)
                                    VGSvcVerbose(3, "Stale session for user=%ls detected! Processes: %RU32 -> %RU32, Session: %RU32 -> %RU32\n",
                                                 pCurUser->wszUser, pCurUser->ulNumProcs, cCurSessionProcs,
                                                 pCurUser->ulLastSession, userSession.ulLastSession);

                                pCurUser->ulNumProcs = cCurSessionProcs;
                                pCurUser->ulLastSession  = userSession.ulLastSession;
                            }
                            /* There can be multiple session objects using the same session ID for the
                             * current user -- so when we got the same session again just add the found
                             * processes to it. */
                            else if (pCurUser->ulLastSession == userSession.ulLastSession)
                            {
                                VGSvcVerbose(4, "Updating processes for user=%ls (old procs=%RU32, new procs=%RU32, session=%RU32)\n",
                                             pCurUser->wszUser, pCurUser->ulNumProcs, cCurSessionProcs, pCurUser->ulLastSession);

                                pCurUser->ulNumProcs = cCurSessionProcs;
                            }

                            fFoundUser = true;
                            break;
                        }
                    }

                    if (!fFoundUser)
                    {
                        VGSvcVerbose(4, "Adding new user=%ls (session=%RU32) with %RU32 processes\n",
                                     userSession.wszUser, userSession.ulLastSession, cCurSessionProcs);

                        memcpy(&pUserInfo[cUniqueUsers], &userSession, sizeof(VBOXSERVICEVMINFOUSER));
                        pUserInfo[cUniqueUsers].ulNumProcs = cCurSessionProcs;
                        cUniqueUsers++;
                        Assert(cUniqueUsers <= cSessions);
                    }
                }
            }

            if (g_cVerbosity > 3)
                VGSvcWritePropF(s_uDebugGuestPropClientID, "/VirtualBox/GuestInfo/Debug/LSA",
                                "#%RU32: cSessions=%RU32, cProcs=%RU32, cUniqueUsers=%RU32",
                                s_uDebugIter, cSessions, cProcs, cUniqueUsers);

            VGSvcVerbose(3, "Found %u unique logged-in user(s)\n", cUniqueUsers);

            for (ULONG i = 0; i < cUniqueUsers; i++)
            {
                if (g_cVerbosity > 3)
                {
                    char szDebugUserPath[255]; RTStrPrintf(szDebugUserPath,  sizeof(szDebugUserPath), "/VirtualBox/GuestInfo/Debug/LSA/User/%RU32", i);
                    VGSvcWritePropF(s_uDebugGuestPropClientID, szDebugUserPath,
                                    "#%RU32: szName=%ls, sessionID=%RU32, cProcs=%RU32",
                                    s_uDebugIter, pUserInfo[i].wszUser, pUserInfo[i].ulLastSession, pUserInfo[i].ulNumProcs);
                }

                bool fAddUser = false;
                if (pUserInfo[i].ulNumProcs)
                    fAddUser = true;

                if (fAddUser)
                {
                    VGSvcVerbose(3, "User '%ls' has %RU32 interactive processes (session=%RU32)\n",
                                 pUserInfo[i].wszUser, pUserInfo[i].ulNumProcs, pUserInfo[i].ulLastSession);

                    if (cUsersInList > 0)
                    {
                        rc = RTStrAAppend(&pszUserList, ",");
                        AssertRCBreakStmt(rc, RTStrFree(pszUserList));
                    }

                    cUsersInList += 1;

                    char *pszUser = NULL;
                    char *pszDomain = NULL;
                    rc = RTUtf16ToUtf8(pUserInfo[i].wszUser, &pszUser);
                    if (   RT_SUCCESS(rc)
                        && pUserInfo[i].wszLogonDomain)
                        rc = RTUtf16ToUtf8(pUserInfo[i].wszLogonDomain, &pszDomain);
                    if (RT_SUCCESS(rc))
                    {
                        /* Append user to users list. */
                        rc = RTStrAAppend(&pszUserList, pszUser);

                        /* Do idle detection. */
                        if (RT_SUCCESS(rc))
                            rc = vgsvcVMInfoWinWriteLastInput(pCache, pszUser, pszDomain);
                    }
                    else
                        rc = RTStrAAppend(&pszUserList, "<string-conversion-error>");

                    RTStrFree(pszUser);
                    RTStrFree(pszDomain);

                    AssertRCBreakStmt(rc, RTStrFree(pszUserList));
                }
            }

            RTMemFree(pUserInfo);
        }
        vgsvcVMInfoWinProcessesFree(cProcs, paProcs);
    }
    if (paSessions)
        g_pfnLsaFreeReturnBuffer(paSessions);

    if (RT_SUCCESS(rc))
    {
        *ppszUserList = pszUserList;
        *pcUsersInList = cUsersInList;
    }

    s_uDebugIter++;
    VbglR3GuestPropDisconnect(s_uDebugGuestPropClientID);

    return rc;
}


int VGSvcVMInfoWinGetComponentVersions(uint32_t uClientID)
{
    int rc;
    char szSysDir[_MAX_PATH] = {0};
    char szWinDir[_MAX_PATH] = {0};
    char szDriversDir[_MAX_PATH + 32] = {0};

    /* ASSUME: szSysDir and szWinDir and derivatives are always ASCII compatible. */
    GetSystemDirectory(szSysDir, _MAX_PATH);
    GetWindowsDirectory(szWinDir, _MAX_PATH);
    RTStrPrintf(szDriversDir, sizeof(szDriversDir), "%s\\drivers", szSysDir);
#ifdef RT_ARCH_AMD64
    char szSysWowDir[_MAX_PATH + 32] = {0};
    RTStrPrintf(szSysWowDir, sizeof(szSysWowDir), "%s\\SysWow64", szWinDir);
#endif

    /* The file information table. */
    const VBOXSERVICEVMINFOFILE aVBoxFiles[] =
    {
        { szSysDir,     "VBoxControl.exe" },
        { szSysDir,     "VBoxHook.dll" },
        { szSysDir,     "VBoxDisp.dll" },
        { szSysDir,     "VBoxTray.exe" },
        { szSysDir,     "VBoxService.exe" },
        { szSysDir,     "VBoxMRXNP.dll" },
        { szSysDir,     "VBoxGINA.dll" },
        { szSysDir,     "VBoxCredProv.dll" },

 /* On 64-bit we don't yet have the OpenGL DLLs in native format.
    So just enumerate the 32-bit files in the SYSWOW directory. */
#ifdef RT_ARCH_AMD64
        { szSysWowDir,  "VBoxOGLarrayspu.dll" },
        { szSysWowDir,  "VBoxOGLcrutil.dll" },
        { szSysWowDir,  "VBoxOGLerrorspu.dll" },
        { szSysWowDir,  "VBoxOGLpackspu.dll" },
        { szSysWowDir,  "VBoxOGLpassthroughspu.dll" },
        { szSysWowDir,  "VBoxOGLfeedbackspu.dll" },
        { szSysWowDir,  "VBoxOGL.dll" },
#else  /* !RT_ARCH_AMD64 */
        { szSysDir,     "VBoxOGLarrayspu.dll" },
        { szSysDir,     "VBoxOGLcrutil.dll" },
        { szSysDir,     "VBoxOGLerrorspu.dll" },
        { szSysDir,     "VBoxOGLpackspu.dll" },
        { szSysDir,     "VBoxOGLpassthroughspu.dll" },
        { szSysDir,     "VBoxOGLfeedbackspu.dll" },
        { szSysDir,     "VBoxOGL.dll" },
#endif /* !RT_ARCH_AMD64 */

        { szDriversDir, "VBoxGuest.sys" },
        { szDriversDir, "VBoxMouseNT.sys" },
        { szDriversDir, "VBoxMouse.sys" },
        { szDriversDir, "VBoxSF.sys"    },
        { szDriversDir, "VBoxVideo.sys" },
    };

    for (unsigned i = 0; i < RT_ELEMENTS(aVBoxFiles); i++)
    {
        char szVer[128];
        rc = VGSvcUtilWinGetFileVersionString(aVBoxFiles[i].pszFilePath, aVBoxFiles[i].pszFileName, szVer, sizeof(szVer));
        char szPropPath[256];
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestAdd/Components/%s", aVBoxFiles[i].pszFileName);
        if (   rc != VERR_FILE_NOT_FOUND
            && rc != VERR_PATH_NOT_FOUND)
            VGSvcWritePropF(uClientID, szPropPath, "%s", szVer);
        else
            VGSvcWritePropF(uClientID, szPropPath, NULL);
    }

    return VINF_SUCCESS;
}

