/* $Id$ */
/** @file
 * VBox WDDM Display backdoor logger implementation
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* We're unable to use standard r3 vbgl-based backdoor logging API because win8 Metro apps
 * can not do CreateFile/Read/Write by default
 * this is why we use miniport escape functionality to issue backdoor log string to the miniport
 * and submit it to host via standard r0 backdoor logging api accordingly */

#include <VBoxDispMpLogger.h>
#include <iprt/win/windows.h>
#include <iprt/win/d3d9.h>
#include <d3dumddi.h>
#include <../../../common/wddm/VBoxMPIf.h>
#include <VBoxDispKmt.h>

#define VBOX_VIDEO_LOG_NAME "VBoxDispMpLogger"
#include <../../../common/VBoxVideoLog.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>

#include <stdio.h>

typedef enum
{
    VBOXDISPMPLOGGER_STATE_UNINITIALIZED = 0,
    VBOXDISPMPLOGGER_STATE_INITIALIZING,
    VBOXDISPMPLOGGER_STATE_INITIALIZED,
    VBOXDISPMPLOGGER_STATE_UNINITIALIZING
} VBOXDISPMPLOGGER_STATE;

typedef struct VBOXDISPMPLOGGER
{
    VBOXDISPKMT_CALLBACKS KmtCallbacks;
    VBOXDISPMPLOGGER_STATE enmState;
} VBOXDISPMPLOGGER, *PVBOXDISPMPLOGGER;

static VBOXDISPMPLOGGER g_VBoxDispMpLogger = {0};

static PVBOXDISPMPLOGGER vboxDispMpLoggerGet()
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_INITIALIZING, VBOXDISPMPLOGGER_STATE_UNINITIALIZED))
    {
        HRESULT hr = vboxDispKmtCallbacksInit(&g_VBoxDispMpLogger.KmtCallbacks);
        if (hr == S_OK)
        {
            /* we are on Vista+
             * check if we can Open Adapter, i.e. WDDM driver is installed */
            VBOXDISPKMT_ADAPTER Adapter;
            hr = vboxDispKmtOpenAdapter(&g_VBoxDispMpLogger.KmtCallbacks, &Adapter);
            if (hr == S_OK)
            {
                ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_INITIALIZED);
                vboxDispKmtCloseAdapter(&Adapter);
                return &g_VBoxDispMpLogger;
            }
            vboxDispKmtCallbacksTerm(&g_VBoxDispMpLogger.KmtCallbacks);
        }
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState) == VBOXDISPMPLOGGER_STATE_INITIALIZED)
    {
        return &g_VBoxDispMpLogger;
    }
    return NULL;
}

VBOXDISPMPLOGGER_DECL(int) VBoxDispMpLoggerInit(void)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return VERR_NOT_SUPPORTED;
    return VINF_SUCCESS;
}

VBOXDISPMPLOGGER_DECL(int) VBoxDispMpLoggerTerm(void)
{
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_UNINITIALIZING, VBOXDISPMPLOGGER_STATE_INITIALIZED))
    {
        vboxDispKmtCallbacksTerm(&g_VBoxDispMpLogger.KmtCallbacks);
        ASMAtomicWriteU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState, VBOXDISPMPLOGGER_STATE_UNINITIALIZED);
        return S_OK;
    }
    else if (ASMAtomicReadU32((volatile uint32_t *)&g_VBoxDispMpLogger.enmState) == VBOXDISPMPLOGGER_STATE_UNINITIALIZED)
    {
        return S_OK;
    }
    return VERR_NOT_SUPPORTED;
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerLog(const char *pszString)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    VBOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vboxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbString = (uint32_t)strlen(pszString) + 1;
        uint32_t cbCmd = RT_UOFFSETOF_DYN(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[cbString]);
        PVBOXDISPIFESCAPE_DBGPRINT pCmd = (PVBOXDISPIFESCAPE_DBGPRINT)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGPRINT;
            memcpy(pCmd->aStringBuf, pszString, cbString);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vboxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerLogF(const char *pszString, ...)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    char szBuffer[4096] = {0};
    va_list pArgList;
    va_start(pArgList, pszString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), pszString, pArgList);
    va_end(pArgList);

    VBoxDispMpLoggerLog(szBuffer);
}

static void vboxDispMpLoggerDumpBuf(void *pvBuf, uint32_t cbBuf, VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE enmBuf)
{
    PVBOXDISPMPLOGGER pLogger = vboxDispMpLoggerGet();
    if (!pLogger)
        return;

    VBOXDISPKMT_ADAPTER Adapter;
    HRESULT hr = vboxDispKmtOpenAdapter(&pLogger->KmtCallbacks, &Adapter);
    if (hr == S_OK)
    {
        uint32_t cbCmd = RT_UOFFSETOF_DYN(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[cbBuf]);
        PVBOXDISPIFESCAPE_DBGDUMPBUF pCmd = (PVBOXDISPIFESCAPE_DBGDUMPBUF)RTMemAllocZ(cbCmd);
        if (pCmd)
        {
            pCmd->EscapeHdr.escapeCode = VBOXESC_DBGDUMPBUF;
            pCmd->enmType = enmBuf;
#ifdef VBOX_WDDM_WOW64
            pCmd->Flags.WoW64 = 1;
#endif
            memcpy(pCmd->aBuf, pvBuf, cbBuf);

            D3DKMT_ESCAPE EscapeData = {0};
            EscapeData.hAdapter = Adapter.hAdapter;
            //EscapeData.hDevice = NULL;
            EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    //        EscapeData.Flags.HardwareAccess = 1;
            EscapeData.pPrivateDriverData = pCmd;
            EscapeData.PrivateDriverDataSize = cbCmd;
            //EscapeData.hContext = NULL;

            int Status = pLogger->KmtCallbacks.pfnD3DKMTEscape(&EscapeData);
            if (Status)
            {
                BP_WARN();
            }

            RTMemFree(pCmd);
        }
        else
        {
            BP_WARN();
        }
        hr = vboxDispKmtCloseAdapter(&Adapter);
        if(hr != S_OK)
        {
            BP_WARN();
        }
    }
}

VBOXDISPMPLOGGER_DECL(void) VBoxDispMpLoggerDumpD3DCAPS9(struct _D3DCAPS9 *pCaps)
{
    vboxDispMpLoggerDumpBuf(pCaps, sizeof (*pCaps), VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9);
}

/*
 * Prefix the output string with module name and pid/tid.
 */
static const char *vboxUmLogGetModuleName(void)
{
    static int s_fModuleNameInited = 0;
    static char s_szModuleName[MAX_PATH];

    if (!s_fModuleNameInited)
    {
        const DWORD cchName = GetModuleFileNameA(NULL, s_szModuleName, RT_ELEMENTS(s_szModuleName));
        if (cchName == 0)
        {
            return "<no module>";
        }
        s_fModuleNameInited = 1;
    }
    return &s_szModuleName[0];
}

DECLCALLBACK(void) VBoxWddmUmLog(const char *pszString)
{
    char szBuffer[4096];
    const int cbBuffer = sizeof(szBuffer);
    char *pszBuffer = &szBuffer[0];

    int cbWritten = _snprintf(pszBuffer, cbBuffer, "['%s' 0x%x.0x%x]: ",
                              vboxUmLogGetModuleName(), GetCurrentProcessId(), GetCurrentThreadId());
    if (cbWritten < 0 || cbWritten >= cbBuffer)
    {
        AssertFailed();
        pszBuffer[0] = 0;
        cbWritten = 0;
    }

    const size_t cbLeft = cbBuffer - cbWritten;
    const size_t cbString = strlen(pszString) + 1;
    if (cbString <= cbLeft)
    {
        memcpy(pszBuffer + cbWritten, pszString, cbString);
    }
    else
    {
        memcpy(pszBuffer + cbWritten, pszString, cbLeft - 1);
        pszBuffer[cbWritten + cbLeft - 1] = 0;
    }

    VBoxDispMpLoggerLog(szBuffer);
}
