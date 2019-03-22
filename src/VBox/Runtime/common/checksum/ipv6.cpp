/* $Id$ */
/** @file
 * IPRT - IPv6 Checksum calculation and validation.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
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
#include <iprt/net.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>


/**
 * @copydoc RTNetIPv6PseudoChecksumBits
 */
DECLINLINE(uint32_t) rtNetIPv6PseudoChecksumBits(PCRTNETADDRIPV6 pSrcAddr, PCRTNETADDRIPV6 pDstAddr,
                                                 uint8_t bProtocol, uint32_t cbPkt)
{
    uint32_t u32Sum = pSrcAddr->au16[0]
                    + pSrcAddr->au16[1]
                    + pSrcAddr->au16[2]
                    + pSrcAddr->au16[3]
                    + pSrcAddr->au16[4]
                    + pSrcAddr->au16[5]
                    + pSrcAddr->au16[6]
                    + pSrcAddr->au16[7]
                    + pDstAddr->au16[0]
                    + pDstAddr->au16[1]
                    + pDstAddr->au16[2]
                    + pDstAddr->au16[3]
                    + pDstAddr->au16[4]
                    + pDstAddr->au16[5]
                    + pDstAddr->au16[6]
                    + pDstAddr->au16[7]
                    + RT_H2BE_U16(RT_HIWORD(cbPkt))
                    + RT_H2BE_U16(RT_LOWORD(cbPkt))
                    + 0
                    + RT_H2BE_U16(RT_MAKE_U16(bProtocol, 0));
    return u32Sum;
}


/**
 * Calculates the checksum of a pseudo header given an IPv6 header, ASSUMING
 * that there are no headers between the IPv6 header and the upper layer header.
 *
 * Use this method with create care!  In most cases you should be using
 * RTNetIPv6PseudoChecksumEx.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pIpHdr      The IPv6 header (network endian (big)).
 */
RTDECL(uint32_t) RTNetIPv6PseudoChecksum(PCRTNETIPV6 pIpHdr)
{
    return rtNetIPv6PseudoChecksumBits(&pIpHdr->ip6_src, &pIpHdr->ip6_dst,
                                       pIpHdr->ip6_nxt, RT_N2H_U16(pIpHdr->ip6_plen));
}
RT_EXPORT_SYMBOL(RTNetIPv6PseudoChecksum);


/**
 * Calculates the checksum of a pseudo header given an IPv6 header.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pIpHdr          The IPv6 header (network endian (big)).
 * @param   bProtocol       The protocol number.  This can be the same as the
 *                          ip6_nxt field, but doesn't need to be.
 * @param   cbPkt           The packet size (host endian of course).  This can
 *                          be the same as the ip6_plen field, but as with @a
 *                          bProtocol it won't be when extension headers are
 *                          present.  For UDP this will be uh_ulen converted to
 *                          host endian.
 */
RTDECL(uint32_t) RTNetIPv6PseudoChecksumEx(PCRTNETIPV6 pIpHdr, uint8_t bProtocol, uint16_t cbPkt)
{
    return rtNetIPv6PseudoChecksumBits(&pIpHdr->ip6_src, &pIpHdr->ip6_dst, bProtocol, cbPkt);
}
RT_EXPORT_SYMBOL(RTNetIPv6PseudoChecksumEx);


/**
 * Calculates the checksum of a pseudo header given the individual components.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pSrcAddr        Pointer to the source address in network endian.
 * @param   pDstAddr        Pointer to the destination address in network endian.
 * @param   bProtocol       The protocol number.  This can be the same as the
 *                          ip6_nxt field, but doesn't need to be.
 * @param   cbPkt           The packet size (host endian of course).  This can
 *                          be the same as the ip6_plen field, but as with @a
 *                          bProtocol it won't be when extension headers are
 *                          present.  For UDP this will be uh_ulen converted to
 *                          host endian.
 */
RTDECL(uint32_t) RTNetIPv6PseudoChecksumBits(PCRTNETADDRIPV6 pSrcAddr, PCRTNETADDRIPV6 pDstAddr,
                                             uint8_t bProtocol, uint16_t cbPkt)
{
    return rtNetIPv6PseudoChecksumBits(pSrcAddr, pDstAddr, bProtocol, cbPkt);
}
RT_EXPORT_SYMBOL(RTNetIPv6PseudoChecksumBits);

