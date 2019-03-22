/* $Id$ */
/** @file
 * IPRT - Network Address String Handling.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
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
#include <iprt/net.h>

#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include "internal/string.h"


DECLHIDDEN(int) rtNetStrToIPv4AddrEx(const char *pcszAddr, PRTNETADDRIPV4 pAddr,
                                     char **ppszNext)
{
    char *pszNext;
    int rc;

    AssertPtrReturn(pcszAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pAddr, VERR_INVALID_PARAMETER);

    rc = RTStrToUInt8Ex(pcszAddr, &pszNext, 10, &pAddr->au8[0]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[1]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[2]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[3]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;

    if (ppszNext != NULL)
        *ppszNext = pszNext;
    return VINF_SUCCESS;
}


RTDECL(int) RTNetStrToIPv4AddrEx(const char *pcszAddr, PRTNETADDRIPV4 pAddr,
                                 char **ppszNext)
{
    return rtNetStrToIPv4AddrEx(pcszAddr, pAddr, ppszNext);
}
RT_EXPORT_SYMBOL(RTNetStrToIPv4AddrEx);


RTDECL(int) RTNetStrToIPv4Addr(const char *pcszAddr, PRTNETADDRIPV4 pAddr)
{
    char *pszNext;
    int rc;

    AssertPtrReturn(pcszAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pAddr, VERR_INVALID_PARAMETER);

    pcszAddr = RTStrStripL(pcszAddr);
    rc = rtNetStrToIPv4AddrEx(pcszAddr, pAddr, &pszNext);
    if (rc != VINF_SUCCESS)
        return VERR_INVALID_PARAMETER;

    pszNext = RTStrStripL(pszNext);
    if (*pszNext != '\0')
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetStrToIPv4Addr);


RTDECL(bool) RTNetIsIPv4AddrStr(const char *pcszAddr)
{
    RTNETADDRIPV4 addrIPv4;
    char *pszNext;
    int rc;

    if (pcszAddr == NULL)
        return false;

    rc = rtNetStrToIPv4AddrEx(pcszAddr, &addrIPv4, &pszNext);
    if (rc != VINF_SUCCESS)
        return false;

    if (*pszNext != '\0')
        return false;

    return true;
}
RT_EXPORT_SYMBOL(RTNetIsIPv4AddrStr);


RTDECL(bool) RTNetStrIsIPv4AddrAny(const char *pcszAddr)
{
    RTNETADDRIPV4 addrIPv4;
    char *pszNext;
    int rc;

    if (pcszAddr == NULL)
        return false;

    pcszAddr = RTStrStripL(pcszAddr);
    rc = rtNetStrToIPv4AddrEx(pcszAddr, &addrIPv4, &pszNext);
    if (rc != VINF_SUCCESS)
        return false;

    pszNext = RTStrStripL(pszNext);
    if (*pszNext != '\0')
        return false;

    if (addrIPv4.u != 0u)       /* INADDR_ANY? */
        return false;

    return true;
}
RT_EXPORT_SYMBOL(RTNetStrIsIPv4AddrAny);


RTDECL(int) RTNetMaskToPrefixIPv4(PCRTNETADDRIPV4 pMask, int *piPrefix)
{
    AssertReturn(pMask != NULL, VERR_INVALID_PARAMETER);

    if (pMask->u == 0)
    {
        if (piPrefix != NULL)
            *piPrefix = 0;
        return VINF_SUCCESS;
    }

    const uint32_t uMask = RT_N2H_U32(pMask->u);

    uint32_t uPrefixMask = UINT32_C(0xffffffff);
    int iPrefixLen = 32;

    while (iPrefixLen > 0)
    {
        if (uMask == uPrefixMask)
        {
            if (piPrefix != NULL)
                *piPrefix = iPrefixLen;
            return VINF_SUCCESS;
        }

        --iPrefixLen;
        uPrefixMask <<= 1;
    }

    return VERR_INVALID_PARAMETER;
}
RT_EXPORT_SYMBOL(RTNetMaskToPrefixIPv4);


RTDECL(int) RTNetPrefixToMaskIPv4(int iPrefix, PRTNETADDRIPV4 pMask)
{
    AssertReturn(pMask != NULL, VERR_INVALID_PARAMETER);

    if (RT_UNLIKELY(iPrefix < 0 || 32 < iPrefix))
        return VERR_INVALID_PARAMETER;

    if (RT_LIKELY(iPrefix != 0))
        pMask->u = RT_H2N_U32(UINT32_C(0xffffffff) << (32 - iPrefix));
    else /* avoid UB in the shift */
        pMask->u = 0;

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetPrefixToMaskIPv4);


static int rtNetStrToHexGroup(const char *pcszValue, char **ppszNext,
                              uint16_t *pu16)
{
    char *pszNext;
    int rc;

    rc = RTStrToUInt16Ex(pcszValue, &pszNext, 16, pu16);
    if (RT_FAILURE(rc))
        return rc;

    if (   rc != VINF_SUCCESS
        && rc != VWRN_TRAILING_CHARS
        && rc != VWRN_TRAILING_SPACES)
    {
        return -rc;             /* convert warning to error */
    }

    /* parser always accepts 0x prefix */
    if (pcszValue[0] == '0' && (pcszValue[1] == 'x' || pcszValue[1] == 'X'))
    {
        if (pu16)
            *pu16 = 0;
        if (ppszNext)
            *ppszNext = (/* UNCONST */ char *)pcszValue + 1; /* to 'x' */
        return VWRN_TRAILING_CHARS;
    }

    /* parser accepts leading zeroes "000000f" */
    if (pszNext - pcszValue > 4)
        return VERR_PARSE_ERROR;

    if (ppszNext)
        *ppszNext = pszNext;
    return rc;
}


/*
 * This function deals only with the hex-group IPv6 address syntax
 * proper (with possible embedded IPv4).
 */
DECLHIDDEN(int) rtNetStrToIPv6AddrBase(const char *pcszAddr, PRTNETADDRIPV6 pAddrResult,
                                       char **ppszNext)
{
    RTNETADDRIPV6 ipv6;
    RTNETADDRIPV4 ipv4;
    const char *pcszPos;
    char *pszNext;
    int iGroup;
    uint16_t u16;
    int rc;

    RT_ZERO(ipv6);

    pcszPos = pcszAddr;

    if (pcszPos[0] == ':') /* compressed zero run at the beginning? */
    {
        if (pcszPos[1] != ':')
            return VERR_PARSE_ERROR;

        pcszPos += 2;           /* skip over "::" */
        pszNext = (/* UNCONST */ char *)pcszPos;
        iGroup = 1;
    }
    else
    {
        /*
         * Scan forward until we either get complete address or find
         * "::" compressed zero run.
         */
        pszNext = NULL; /* (MSC incorrectly thinks it may be used unitialized) */
        for (iGroup = 0; iGroup < 8; ++iGroup)
        {
            /* check for embedded IPv4 at the end */
            if (iGroup == 6)
            {
                rc = rtNetStrToIPv4AddrEx(pcszPos, &ipv4, &pszNext);
                if (rc == VINF_SUCCESS)
                {
                    ipv6.au32[3] = ipv4.au32[0];
                    iGroup = 8; /* filled 6 and 7 */
                    break;      /* we are done */
                }
            }

            rc = rtNetStrToHexGroup(pcszPos, &pszNext, &u16);
            if (RT_FAILURE(rc))
                return VERR_PARSE_ERROR;

            ipv6.au16[iGroup] = RT_H2N_U16(u16);

            if (iGroup == 7)
                pcszPos = pszNext;
            else
            {
                /* skip the colon that delimits this group */
                if (*pszNext != ':')
                    return VERR_PARSE_ERROR;
                pcszPos = pszNext + 1;

                /* compressed zero run? */
                if (*pcszPos == ':')
                {
                    ++pcszPos;    /* skip over :: */
                    pszNext += 2; /* skip over :: (in case we are done) */
                    iGroup += 2;  /* current field and the zero in the next */
                    break;
                }
            }
        }
    }

    if (iGroup != 8)
    {
        /*
         * iGroup is the first group that can be filled by the part of
         * the address after "::".
         */
        RTNETADDRIPV6 ipv6Tail;
        const int iMaybeStart = iGroup;
        int j;

        RT_ZERO(ipv6Tail);

        /*
         * We try to accept longest match; we'll shift if necessary.
         * Unlike the first loop, a failure to parse a group doesn't
         * mean invalid address.
         */
        for (; iGroup < 8; ++iGroup)
        {
            /* check for embedded IPv4 at the end */
            if (iGroup <= 6)
            {
                rc = rtNetStrToIPv4AddrEx(pcszPos, &ipv4, &pszNext);
                if (rc == VINF_SUCCESS)
                {
                    ipv6Tail.au16[iGroup]     = ipv4.au16[0];
                    ipv6Tail.au16[iGroup + 1] = ipv4.au16[1];
                    iGroup = iGroup + 2; /* these two are done */
                    break;               /* the rest is trailer */
                }
            }

            rc = rtNetStrToHexGroup(pcszPos, &pszNext, &u16);
            if (RT_FAILURE(rc))
                break;

            ipv6Tail.au16[iGroup] = RT_H2N_U16(u16);

            if (iGroup == 7)
                pcszPos = pszNext;
            else
            {
                if (*pszNext != ':')
                {
                    ++iGroup;   /* this one is done */
                    break;      /* the rest is trailer */
                }

                pcszPos = pszNext + 1;
            }
        }

        for (j = 7, --iGroup; iGroup >= iMaybeStart; --j, --iGroup)
            ipv6.au16[j] = ipv6Tail.au16[iGroup];
    }

    if (pAddrResult != NULL)
        memcpy(pAddrResult, &ipv6, sizeof(ipv6));
    if (ppszNext != NULL)
        *ppszNext = pszNext;
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtNetStrToIPv6AddrEx(const char *pcszAddr, PRTNETADDRIPV6 pAddr,
                                     char **ppszZone, char **ppszNext)
{
    char *pszNext, *pszZone;
    int rc;

    rc = rtNetStrToIPv6AddrBase(pcszAddr, pAddr, &pszNext);
    if (RT_FAILURE(rc))
        return rc;

    if (*pszNext != '%')      /* is there a zone id? */
    {
        pszZone = NULL;
    }
    else
    {
        pszZone = pszNext + 1; /* skip '%' zone id delimiter */
        if (*pszZone == '\0')
            return VERR_PARSE_ERROR; /* empty zone id */

        /*
         * XXX: this is speculative as zone id syntax is
         * implementation dependent, so we kinda guess here (accepting
         * unreserved characters from URI syntax).
         */
        for (pszNext = pszZone; *pszNext != '\0'; ++pszNext)
        {
            const char c = *pszNext;
            if (   !('0' <= c && c <= '9')
                   && !('a' <= c && c <= 'z')
                   && !('A' <= c && c <= 'Z')
                   && c != '_'
                   && c != '.'
                   && c != '-'
                   && c != '~')
            {
                break;
            }
        }
    }

    if (ppszZone != NULL)
        *ppszZone = pszZone;
    if (ppszNext != NULL)
        *ppszNext = pszNext;

    if (*pszNext == '\0')       /* all input string consumed */
        return VINF_SUCCESS;
    else
    {
        while (*pszNext == ' ' || *pszNext == '\t')
            ++pszNext;
        if (*pszNext == '\0')
            return VWRN_TRAILING_SPACES;
        else
            return VWRN_TRAILING_CHARS;
    }
}


RTDECL(int) RTNetStrToIPv6AddrEx(const char *pcszAddr, PRTNETADDRIPV6 pAddr,
                                 char **ppszNext)
{
    AssertPtrReturn(pcszAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pAddr, VERR_INVALID_PARAMETER);

    return rtNetStrToIPv6AddrBase(pcszAddr, pAddr, ppszNext);
}
RT_EXPORT_SYMBOL(RTNetStrToIPv6AddrEx);


RTDECL(int) RTNetStrToIPv6Addr(const char *pcszAddr, PRTNETADDRIPV6 pAddr,
                               char **ppszZone)
{
    int rc;

    AssertPtrReturn(pcszAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppszZone, VERR_INVALID_PARAMETER);

    pcszAddr = RTStrStripL(pcszAddr);
    rc = rtNetStrToIPv6AddrEx(pcszAddr, pAddr, ppszZone, NULL);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES)
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetStrToIPv6Addr);


RTDECL(bool) RTNetIsIPv6AddrStr(const char *pcszAddr)
{
    RTNETADDRIPV6 addrIPv6;
    int rc;

    if (pcszAddr == NULL)
        return false;

    rc = rtNetStrToIPv6AddrEx(pcszAddr, &addrIPv6, NULL, NULL);
    if (rc != VINF_SUCCESS)
        return false;

    return true;
}
RT_EXPORT_SYMBOL(RTNetIsIPv6AddrStr);


RTDECL(bool) RTNetStrIsIPv6AddrAny(const char *pcszAddr)
{
    RTNETADDRIPV6 addrIPv6;
    char *pszZone, *pszNext;
    int rc;

    if (pcszAddr == NULL)
        return false;

    pcszAddr = RTStrStripL(pcszAddr);
    rc = rtNetStrToIPv6AddrEx(pcszAddr, &addrIPv6, &pszZone, &pszNext);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES)
        return false;

    if (pszZone != NULL)
        return false;

    if (addrIPv6.s.Lo != 0 || addrIPv6.s.Hi != 0) /* in6addr_any? */
        return false;

    return true;
}
RT_EXPORT_SYMBOL(RTNetStrIsIPv6AddrAny);


RTDECL(int) RTNetMaskToPrefixIPv6(PCRTNETADDRIPV6 pMask, int *piPrefix)
{
    AssertReturn(pMask != NULL, VERR_INVALID_PARAMETER);

    int iPrefix = 0;
    unsigned int i;

    for (i = 0; i < RT_ELEMENTS(pMask->au8); ++i)
    {
        int iBits;
        switch (pMask->au8[i])
        {
            case 0x00: iBits = 0; break;
            case 0x80: iBits = 1; break;
            case 0xc0: iBits = 2; break;
            case 0xe0: iBits = 3; break;
            case 0xf0: iBits = 4; break;
            case 0xf8: iBits = 5; break;
            case 0xfc: iBits = 6; break;
            case 0xfe: iBits = 7; break;
            case 0xff: iBits = 8; break;
            default:                /* non-contiguous mask */
                return VERR_INVALID_PARAMETER;
        }

        iPrefix += iBits;
        if (iBits != 8)
            break;
    }

    for (++i; i < RT_ELEMENTS(pMask->au8); ++i)
        if (pMask->au8[i] != 0)
            return VERR_INVALID_PARAMETER;

    if (piPrefix != NULL)
        *piPrefix = iPrefix;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetMaskToPrefixIPv6);


RTDECL(int) RTNetPrefixToMaskIPv6(int iPrefix, PRTNETADDRIPV6 pMask)
{
    AssertReturn(pMask != NULL, VERR_INVALID_PARAMETER);

    if (RT_UNLIKELY(iPrefix < 0 || 128 < iPrefix))
        return VERR_INVALID_PARAMETER;

    for (unsigned int i = 0; i < RT_ELEMENTS(pMask->au32); ++i)
    {
        if (iPrefix == 0)
        {
            pMask->au32[i] = 0;
        }
        else if (iPrefix >= 32)
        {
            pMask->au32[i] = UINT32_C(0xffffffff);
            iPrefix -= 32;
        }
        else
        {
            pMask->au32[i] = RT_H2N_U32(UINT32_C(0xffffffff) << (32 - iPrefix));
            iPrefix = 0;
        }
    }

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetPrefixToMaskIPv6);
