/* $Id$ */
/** @file
 * Host DNS listener for Windows.
 */

/*
 * Copyright (C) 2014-2024 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

/*
 * XXX: need <winsock2.h> to reveal IP_ADAPTER_ADDRESSES in
 * <iptypes.h> and it must be included before <windows.h>, which is
 * pulled in by IPRT headers.
 */
#include <iprt/win/winsock2.h>

#include "../HostDnsService.h"

#include <VBox/com/string.h>
#include <VBox/com/ptr.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <VBox/log.h>

#include <iprt/win/windows.h>
#include <windns.h>
#include <iptypes.h>
#include <iprt/win/iphlpapi.h>

#include <algorithm>
#include <vector>


DECLINLINE(int) registerNotification(const HKEY &hKey, HANDLE &hEvent)
{
    LONG lrc = RegNotifyChangeKeyValue(hKey,
                                       TRUE,
                                       REG_NOTIFY_CHANGE_LAST_SET,
                                       hEvent,
                                       TRUE);
    AssertLogRelMsgReturn(lrc == ERROR_SUCCESS,
                          ("Failed to register event on the key. Please debug me!"),
                          VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}

static void appendTokenizedStrings(std::vector<com::Utf8Str> &vecStrings, const com::Utf8Str &strToAppend, char chDelim = ' ')
{
    if (strToAppend.isEmpty())
        return;

    RTCString const strDelim(1, chDelim);
    auto const      lstSubStrings  = strToAppend.split(strDelim);
    size_t const    cSubStrings    = lstSubStrings.size();
    for (size_t i = 0; i < cSubStrings; i++)
    {
        RTCString const &strCur = lstSubStrings[i];
        if (strCur.isNotEmpty())
            if (std::find(vecStrings.cbegin(), vecStrings.cend(), strCur) == vecStrings.cend())
                vecStrings.push_back(strCur);
    }
}


struct HostDnsServiceWin::Data
{
    HKEY hKeyTcpipParameters;
    bool fTimerArmed;

#define DATA_SHUTDOWN_EVENT   0
#define DATA_DNS_UPDATE_EVENT 1
#define DATA_TIMER            2
#define DATA_MAX_EVENT        3
    HANDLE ahDataEvents[DATA_MAX_EVENT];

    Data()
    {
        hKeyTcpipParameters = NULL;
        fTimerArmed = false;

        for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
            ahDataEvents[i] = NULL;
    }

    ~Data()
    {
        if (hKeyTcpipParameters != NULL)
        {
            RegCloseKey(hKeyTcpipParameters);
            hKeyTcpipParameters = NULL;
        }

        for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
            if (ahDataEvents[i] != NULL)
            {
                CloseHandle(ahDataEvents[i]);
                ahDataEvents[i] = NULL;
            }
    }
};


HostDnsServiceWin::HostDnsServiceWin()
    : HostDnsServiceBase(true)
{
    m = new Data();
}

HostDnsServiceWin::~HostDnsServiceWin()
{
    if (m != NULL)
        delete m;
}

HRESULT HostDnsServiceWin::init(HostDnsMonitorProxy *pProxy)
{
    if (m == NULL)
        return E_FAIL;

    LONG lRc = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                             L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                             0,
                             KEY_READ | KEY_NOTIFY,
                             &m->hKeyTcpipParameters);
    if (lRc != ERROR_SUCCESS)
    {
        LogRel(("HostDnsServiceWin: failed to open key Tcpip\\Parameters (error %d)\n", lRc));
        return E_FAIL;
    }

    for (size_t i = 0; i < DATA_MAX_EVENT; ++i)
    {
        HANDLE h;
        if (i == DATA_TIMER)
            h = CreateWaitableTimer(NULL, FALSE, NULL);
        else
            h = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (h == NULL)
        {
            LogRel(("HostDnsServiceWin: failed to create %s (error %d)\n",
                    i == DATA_TIMER ? "waitable timer" : "event", GetLastError()));
            return E_FAIL;
        }

        m->ahDataEvents[i] = h;
    }

    HRESULT hrc = HostDnsServiceBase::init(pProxy);
    if (FAILED(hrc))
        return hrc;

    return updateInfo();
}

void HostDnsServiceWin::uninit(void)
{
    HostDnsServiceBase::uninit();
}

int HostDnsServiceWin::monitorThreadShutdown(RTMSINTERVAL uTimeoutMs)
{
    AssertPtrReturn(m, VINF_SUCCESS);

    SetEvent(m->ahDataEvents[DATA_SHUTDOWN_EVENT]);
    RT_NOREF(uTimeoutMs); /* (Caller waits for the thread) */

    return VINF_SUCCESS;
}

int HostDnsServiceWin::monitorThreadProc(void)
{
    Assert(m != NULL);

    registerNotification(m->hKeyTcpipParameters,
                         m->ahDataEvents[DATA_DNS_UPDATE_EVENT]);

    onMonitorThreadInitDone();

    for (;;)
    {
        DWORD dwReady = WaitForMultipleObjects(DATA_MAX_EVENT, m->ahDataEvents, FALSE, INFINITE);
        if (dwReady == WAIT_OBJECT_0 + DATA_SHUTDOWN_EVENT)
            break;

        if (dwReady == WAIT_OBJECT_0 + DATA_DNS_UPDATE_EVENT)
        {
            /*
             * Registry updates for multiple values are not atomic, so
             * wait a bit to avoid racing and reading partial update.
             */
            if (!m->fTimerArmed)
            {
                LARGE_INTEGER delay; /* in 100ns units */
                delay.QuadPart = -2 * 1000 * 1000 * 10LL; /* relative: 2s */
                if (SetWaitableTimer(m->ahDataEvents[DATA_TIMER], &delay, 0, NULL, NULL, FALSE))
                    m->fTimerArmed = true;
                else
                {
                    LogRel(("HostDnsServiceWin: failed to arm timer (error %d)\n", GetLastError()));
                    updateInfo();
                }
            }

            ResetEvent(m->ahDataEvents[DATA_DNS_UPDATE_EVENT]);
            registerNotification(m->hKeyTcpipParameters,
                                 m->ahDataEvents[DATA_DNS_UPDATE_EVENT]);
        }
        else if (dwReady == WAIT_OBJECT_0 + DATA_TIMER)
        {
            m->fTimerArmed = false;
            updateInfo();
        }
        else if (dwReady == WAIT_FAILED)
        {
            LogRel(("HostDnsServiceWin: WaitForMultipleObjects failed: error %d\n", GetLastError()));
            return VERR_INTERNAL_ERROR;
        }
        else
        {
            LogRel(("HostDnsServiceWin: WaitForMultipleObjects unexpected return value %d\n", dwReady));
            return VERR_INTERNAL_ERROR;
        }
    }

    return VINF_SUCCESS;
}

HRESULT HostDnsServiceWin::updateInfo(void)
{
    HostDnsInformation info;

    com::Utf8Str strDomain;
    com::Utf8Str strSearchList;  /* NB: comma separated, no spaces */

    /*
     * We ignore "DhcpDomain" key here since it's not stable.  If
     * there are two active interfaces that use DHCP (in particular
     * when host uses OpenVPN) then DHCP ACKs will take turns updating
     * that key.  Instead we call GetAdaptersAddresses() below (which
     * is what ipconfig.exe seems to do).
     */
    for (DWORD regIndex = 0; /**/; ++regIndex)
    {
        WCHAR wszKeyName[256] = {0};
        DWORD cbKeyName = RT_ELEMENTS(wszKeyName);
        DWORD keyType   = 0;
        WCHAR wszKeyData[1024];
        DWORD cbKeyData = sizeof(wszKeyData);
        LSTATUS lrc = RegEnumValueW(m->hKeyTcpipParameters, regIndex,
                                    wszKeyName, &cbKeyName, NULL /*pReserved*/,
                                    &keyType, (LPBYTE)wszKeyData, &cbKeyData);

        if (lrc == ERROR_NO_MORE_ITEMS)
            break;

        if (lrc == ERROR_MORE_DATA) /* buffer too small; handle? */
            continue;

        if (lrc != ERROR_SUCCESS)
        {
            LogRel2(("HostDnsServiceWin: RegEnumValue error %d\n", (int)lrc));
            return E_FAIL;
        }

        if (keyType != REG_SZ)
            continue;

        size_t cwcKeyData = cbKeyData / sizeof(wszKeyData[0]);
        if (cwcKeyData > 0 && wszKeyData[cwcKeyData - 1] == '\0')
            --cwcKeyData;     /* don't count trailing NUL if present */

        if (RTUtf16ICmpAscii(wszKeyName, "Domain") == 0)
        {
            strDomain.assign(wszKeyData, cwcKeyData);
            LogRel2(("HostDnsServiceWin: Domain=\"%s\"\n", strDomain.c_str()));
        }
        else if (RTUtf16ICmpAscii(wszKeyName, "SearchList") == 0)
        {
            strSearchList.assign(wszKeyData, cwcKeyData);
            LogRel2(("HostDnsServiceWin: SearchList=\"%s\"\n", strSearchList.c_str()));
        }
        else if (LogRelIs2Enabled() && RTUtf16ICmpAscii(wszKeyName, "DhcpDomain") == 0)
        {
            com::Utf8Str strDhcpDomain(wszKeyData, cwcKeyData);
            LogRel2(("HostDnsServiceWin: DhcpDomain=\"%s\"\n", strDhcpDomain.c_str()));
        }
    }

    /* statically configured domain name */
    if (strDomain.isNotEmpty())
    {
        info.domain = strDomain;
        info.searchList.push_back(strDomain);
    }

    /* statically configured search list */
    if (strSearchList.isNotEmpty())
        appendTokenizedStrings(info.searchList, strSearchList, ',');

    /*
     * When name servers are configured statically it seems that the
     * value of Tcpip\Parameters\NameServer is NOT set, inly interface
     * specific NameServer value is (which triggers notification for
     * us to pick up the change).  Fortunately, DnsApi seems to do the
     * right thing there.
     */
    PIP4_ARRAY pIp4Array = NULL;
    DWORD      cbBuffer  = sizeof(&pIp4Array); // NB: must be set on input it seems, despite docs' claim to the contrary.
    DNS_STATUS status = DnsQueryConfig(DnsConfigDnsServerList, DNS_CONFIG_FLAG_ALLOC, NULL, NULL, &pIp4Array, &cbBuffer);
    if (status == NO_ERROR && pIp4Array != NULL)
    {
        for (DWORD i = 0; i < pIp4Array->AddrCount; ++i)
        {
            char szAddr[16] = "";
            RTStrPrintf(szAddr, sizeof(szAddr), "%RTnaipv4", pIp4Array->AddrArray[i]);

            LogRel2(("HostDnsServiceWin: server %d: %s\n", i+1,  szAddr));
            info.servers.push_back(szAddr);
        }

        LocalFree(pIp4Array);
    }


    /*
     * DnsQueryConfig(DnsConfigSearchList, ...) is not implemented.
     * Call GetAdaptersAddresses() that orders the returned list
     * appropriately and collect IP_ADAPTER_ADDRESSES::DnsSuffix.
     */
    do /* not a loop */
    {
        ULONG                 cbAddrBuf    = _8K;
        PIP_ADAPTER_ADDRESSES pAddrBuf     = (PIP_ADAPTER_ADDRESSES)RTMemAllocZ(cbAddrBuf);
        if (pAddrBuf == NULL)
        {
            LogRel2(("HostDnsServiceWin: failed to allocate %zu bytes of GetAdaptersAddresses buffer\n", (size_t)cbAddrBuf));
            break;
        }

        for (unsigned iReallocLoops = 0; ; iReallocLoops++)
        {
            ULONG const cbAddrBufProvided = cbAddrBuf; /* for logging */

            ULONG err = GetAdaptersAddresses(AF_UNSPEC,
                                             GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
                                             NULL,
                                             pAddrBuf, &cbAddrBuf);
            if (err == NO_ERROR)
                break;
            if (err == ERROR_BUFFER_OVERFLOW)
            {
                LogRel2(("HostDnsServiceWin: provided GetAdaptersAddresses with %zu but asked again for %zu bytes\n",
                         (size_t)cbAddrBufProvided, (size_t)cbAddrBuf));
                if (iReallocLoops < 16)
                {
                    /* Reallocate the buffer and try again. */
                    void * const pvNew = RTMemRealloc(pAddrBuf, cbAddrBuf);
                    if (pvNew)
                    {
                        pAddrBuf = (PIP_ADAPTER_ADDRESSES)pvNew;
                        continue;
                    }

                    LogRel2(("HostDnsServiceWin: failed to reallocate %zu bytes\n", (size_t)cbAddrBuf));
                }
                else
                    LogRel2(("HostDnsServiceWin: iReallocLoops=%d - giving up!\n", iReallocLoops));
            }
            else
                LogRel2(("HostDnsServiceWin: GetAdaptersAddresses error %d\n", err));
            RTMemFree(pAddrBuf);
            pAddrBuf = NULL;
            break;
        }
        if (pAddrBuf)
        {
            for (PIP_ADAPTER_ADDRESSES pAdp = pAddrBuf; pAdp != NULL; pAdp = pAdp->Next)
            {
                LogRel2(("HostDnsServiceWin: %ls (status %u) ...\n",
                         pAdp->FriendlyName ? pAdp->FriendlyName : L"(null)", pAdp->OperStatus));

                if (pAdp->OperStatus != IfOperStatusUp)
                    continue;

                if (pAdp->DnsSuffix == NULL || *pAdp->DnsSuffix == L'\0')
                    continue;

                char *pszDnsSuffix = NULL;
                int vrc = RTUtf16ToUtf8Ex(pAdp->DnsSuffix, RTSTR_MAX, &pszDnsSuffix, 0, /* allocate */ NULL);
                if (RT_FAILURE(vrc))
                {
                    LogRel2(("HostDnsServiceWin: failed to convert DNS suffix \"%ls\": %Rrc\n", pAdp->DnsSuffix, vrc));
                    continue;
                }

                AssertContinue(pszDnsSuffix != NULL);
                AssertContinue(*pszDnsSuffix != '\0');
                LogRel2(("HostDnsServiceWin: ... suffix = \"%s\"\n", pszDnsSuffix));

                appendTokenizedStrings(info.searchList, pszDnsSuffix);
                RTStrFree(pszDnsSuffix);
            }

            RTMemFree(pAddrBuf);
        }
    } while (0);


    if (info.domain.isEmpty() && !info.searchList.empty())
        info.domain = info.searchList[0];

    if (info.searchList.size() == 1) /* ?? */
        info.searchList.clear();

    HostDnsServiceBase::setInfo(info);

    return S_OK;
}

