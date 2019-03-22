/* $Id$ */
/** @file
 * Main - NetIfList, Linux implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_MAIN

#include <iprt/err.h>
#include <list>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <iprt/asm.h>

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"
#include "Logging.h"

/**
 * Obtain the name of the interface used for default routing.
 *
 * NOTE: There is a copy in Devices/Network/testcase/tstIntNet-1.cpp.
 *
 * @returns VBox status code.
 *
 * @param   pszName     The buffer of IFNAMSIZ+1 length where to put the name.
 */
static int getDefaultIfaceName(char *pszName)
{
    FILE *fp = fopen("/proc/net/route", "r");
    char szBuf[1024];
    char szIfName[17];
    uint32_t uAddr;
    uint32_t uGateway;
    uint32_t uMask;
    int  iTmp;
    unsigned uFlags;

    if (fp)
    {
        while (fgets(szBuf, sizeof(szBuf)-1, fp))
        {
            int n = sscanf(szBuf, "%16s %x %x %x %d %d %d %x %d %d %d\n",
                           szIfName, &uAddr, &uGateway, &uFlags, &iTmp, &iTmp, &iTmp,
                           &uMask, &iTmp, &iTmp, &iTmp);
            if (n < 10 || !(uFlags & RTF_UP))
                continue;

            if (uAddr == 0 && uMask == 0)
            {
                fclose(fp);
                strncpy(pszName, szIfName, 16);
                pszName[16] = 0;
                return VINF_SUCCESS;
            }
        }
        fclose(fp);
    }
    return VERR_INTERNAL_ERROR;
}

static uint32_t getInterfaceSpeed(const char *pszName)
{
    /*
     * I wish I could do simple ioctl here, but older kernels require root
     * privileges for any ethtool commands.
     */
    char szBuf[256];
    uint32_t uSpeed = 0;
    /* First, we try to retrieve the speed via sysfs. */
    RTStrPrintf(szBuf, sizeof(szBuf), "/sys/class/net/%s/speed", pszName);
    FILE *fp = fopen(szBuf, "r");
    if (fp)
    {
        if (fscanf(fp, "%u", &uSpeed) != 1)
            uSpeed = 0;
        fclose(fp);
    }
    if (uSpeed == 10)
    {
        /* Check the cable is plugged in at all */
        unsigned uCarrier = 0;
        RTStrPrintf(szBuf, sizeof(szBuf), "/sys/class/net/%s/carrier", pszName);
        fp = fopen(szBuf, "r");
        if (fp)
        {
            if (fscanf(fp, "%u", &uCarrier) != 1 || uCarrier == 0)
                uSpeed = 0;
            fclose(fp);
        }
    }

    if (uSpeed == 0)
    {
        /* Failed to get speed via sysfs, go to plan B. */
        int rc = NetIfAdpCtlOut(pszName, "speed", szBuf, sizeof(szBuf));
        if (RT_SUCCESS(rc))
            uSpeed = RTStrToUInt32(szBuf);
    }
    return uSpeed;
}

static int getInterfaceInfo(int iSocket, const char *pszName, PNETIFINFO pInfo)
{
    // Zeroing out pInfo is a bad idea as it should contain both short and long names at
    // this point. So make sure the structure is cleared by the caller if necessary!
    // memset(pInfo, 0, sizeof(*pInfo));
    struct ifreq Req;
    RT_ZERO(Req);
    RTStrCopy(Req.ifr_name, sizeof(Req.ifr_name), pszName);
    if (ioctl(iSocket, SIOCGIFHWADDR, &Req) >= 0)
    {
        switch (Req.ifr_hwaddr.sa_family)
        {
            case ARPHRD_ETHER:
                pInfo->enmMediumType = NETIF_T_ETHERNET;
                break;
            default:
                pInfo->enmMediumType = NETIF_T_UNKNOWN;
                break;
        }
        /* Generate UUID from name and MAC address. */
        RTUUID uuid;
        RTUuidClear(&uuid);
        memcpy(&uuid, Req.ifr_name, RT_MIN(sizeof(Req.ifr_name), sizeof(uuid)));
        uuid.Gen.u8ClockSeqHiAndReserved = (uint8_t)((uuid.Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80);
        uuid.Gen.u16TimeHiAndVersion = (uint16_t)((uuid.Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000);
        memcpy(uuid.Gen.au8Node, &Req.ifr_hwaddr.sa_data, sizeof(uuid.Gen.au8Node));
        pInfo->Uuid = uuid;

        memcpy(&pInfo->MACAddress, Req.ifr_hwaddr.sa_data, sizeof(pInfo->MACAddress));

        if (ioctl(iSocket, SIOCGIFADDR, &Req) >= 0)
            memcpy(pInfo->IPAddress.au8,
                   &((struct sockaddr_in *)&Req.ifr_addr)->sin_addr.s_addr,
                   sizeof(pInfo->IPAddress.au8));

        if (ioctl(iSocket, SIOCGIFNETMASK, &Req) >= 0)
            memcpy(pInfo->IPNetMask.au8,
                   &((struct sockaddr_in *)&Req.ifr_addr)->sin_addr.s_addr,
                   sizeof(pInfo->IPNetMask.au8));

        if (ioctl(iSocket, SIOCGIFFLAGS, &Req) >= 0)
            pInfo->enmStatus = Req.ifr_flags & IFF_UP ? NETIF_S_UP : NETIF_S_DOWN;

        struct iwreq WRq;
        RT_ZERO(WRq);
        RTStrCopy(WRq.ifr_name, sizeof(WRq.ifr_name), pszName);
        pInfo->fWireless = ioctl(iSocket, SIOCGIWNAME, &WRq) >= 0;

        FILE *fp = fopen("/proc/net/if_inet6", "r");
        if (fp)
        {
            RTNETADDRIPV6 IPv6Address;
            unsigned uIndex, uLength, uScope, uTmp;
            char szName[30];
            for (;;)
            {
                RT_ZERO(szName);
                int n = fscanf(fp,
                               "%08x%08x%08x%08x"
                               " %02x %02x %02x %02x %20s\n",
                               &IPv6Address.au32[0], &IPv6Address.au32[1],
                               &IPv6Address.au32[2], &IPv6Address.au32[3],
                               &uIndex, &uLength, &uScope, &uTmp, szName);
                if (n == EOF)
                    break;
                if (n != 9 || uLength > 128)
                {
                    Log(("getInterfaceInfo: Error while reading /proc/net/if_inet6, n=%d uLength=%u\n",
                         n, uLength));
                    break;
                }
                if (!strcmp(Req.ifr_name, szName))
                {
                    pInfo->IPv6Address.au32[0] = htonl(IPv6Address.au32[0]);
                    pInfo->IPv6Address.au32[1] = htonl(IPv6Address.au32[1]);
                    pInfo->IPv6Address.au32[2] = htonl(IPv6Address.au32[2]);
                    pInfo->IPv6Address.au32[3] = htonl(IPv6Address.au32[3]);
                    RTNetPrefixToMaskIPv6(uLength, &pInfo->IPv6NetMask);
                }
            }
            fclose(fp);
        }
        /*
         * Don't even try to get speed for non-Ethernet interfaces, it only
         * produces errors.
         */
        if (pInfo->enmMediumType == NETIF_T_ETHERNET)
            pInfo->uSpeedMbits = getInterfaceSpeed(pszName);
        else
            pInfo->uSpeedMbits = 0;
    }
    return VINF_SUCCESS;
}

int NetIfList(std::list <ComObjPtr<HostNetworkInterface> > &list)
{
    char szDefaultIface[256];
    int rc = getDefaultIfaceName(szDefaultIface);
    if (RT_FAILURE(rc))
    {
        Log(("NetIfList: Failed to find default interface.\n"));
        szDefaultIface[0] = 0;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0)
    {
        FILE *fp = fopen("/proc/net/dev", "r");
        if (fp)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp))
            {
                char *pszEndOfName = strchr(buf, ':');
                if (!pszEndOfName)
                    continue;
                *pszEndOfName = 0;
                size_t iFirstNonWS = strspn(buf, " ");
                char *pszName = buf + iFirstNonWS;
                NETIFINFO Info;
                RT_ZERO(Info);
                rc = getInterfaceInfo(sock, pszName, &Info);
                if (RT_FAILURE(rc))
                    break;
                if (Info.enmMediumType == NETIF_T_ETHERNET)
                {
                    ComObjPtr<HostNetworkInterface> IfObj;
                    IfObj.createObject();

                    HostNetworkInterfaceType_T enmType;
                    if (strncmp(pszName, RT_STR_TUPLE("vboxnet")))
                        enmType = HostNetworkInterfaceType_Bridged;
                    else
                        enmType = HostNetworkInterfaceType_HostOnly;

                    if (SUCCEEDED(IfObj->init(Bstr(pszName), enmType, &Info)))
                    {
                        if (strcmp(pszName, szDefaultIface) == 0)
                            list.push_front(IfObj);
                        else
                            list.push_back(IfObj);
                    }
                }

            }
            fclose(fp);
        }
        close(sock);
    }
    else
        rc = VERR_INTERNAL_ERROR;

    return rc;
}

int NetIfGetConfigByName(PNETIFINFO pInfo)
{
    int rc = VINF_SUCCESS;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return VERR_NOT_IMPLEMENTED;
    rc = getInterfaceInfo(sock, pInfo->szShortName, pInfo);
    close(sock);
    return rc;
}

/**
 * Retrieve the physical link speed in megabits per second. If the interface is
 * not up or otherwise unavailable the zero speed is returned.
 *
 * @returns VBox status code.
 *
 * @param   pcszIfName  Interface name.
 * @param   puMbits     Where to store the link speed.
 */
int NetIfGetLinkSpeed(const char *pcszIfName, uint32_t *puMbits)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return VERR_OUT_OF_RESOURCES;
    struct ifreq Req;
    RT_ZERO(Req);
    RTStrCopy(Req.ifr_name, sizeof(Req.ifr_name), pcszIfName);
    if (ioctl(sock, SIOCGIFHWADDR, &Req) >= 0)
    {
        if (ioctl(sock, SIOCGIFFLAGS, &Req) >= 0)
            if (Req.ifr_flags & IFF_UP)
            {
                close(sock);
                *puMbits = getInterfaceSpeed(pcszIfName);
                return VINF_SUCCESS;
            }
    }
    close(sock);
    *puMbits = 0;
    return VWRN_NOT_FOUND;
}
