/* $Id$ */
/** @file
 * IPRT - Crypto - X.509, Core APIs.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <iprt/crypto/x509.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/uni.h>

#include "x509-internal.h"


/*
 * Generate the code.
 */
#include <iprt/asn1-generator-core.h>


/*
 * X.509 Validity.
 */

RTDECL(bool) RTCrX509Validity_IsValidAtTimeSpec(PCRTCRX509VALIDITY pThis, PCRTTIMESPEC pTimeSpec)
{
    if (RTAsn1Time_CompareWithTimeSpec(&pThis->NotBefore, pTimeSpec) > 0)
        return false;
    if (RTAsn1Time_CompareWithTimeSpec(&pThis->NotAfter, pTimeSpec) < 0)
        return false;
    return true;
}


/*
 * One X.509 Algorithm Identifier.
 */

RTDECL(RTDIGESTTYPE) RTCrX509AlgorithmIdentifier_QueryDigestType(PCRTCRX509ALGORITHMIDENTIFIER pThis)
{
    AssertPtrReturn(pThis, RTDIGESTTYPE_INVALID);
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_MD5))
        return RTDIGESTTYPE_MD5;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA1))
        return RTDIGESTTYPE_SHA1;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA256))
        return RTDIGESTTYPE_SHA256;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512))
        return RTDIGESTTYPE_SHA512;

    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA384))
        return RTDIGESTTYPE_SHA384;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA224))
        return RTDIGESTTYPE_SHA224;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512T224))
        return RTDIGESTTYPE_SHA512T224;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512T256))
        return RTDIGESTTYPE_SHA512T256;
    return RTDIGESTTYPE_INVALID;
}


RTDECL(uint32_t) RTCrX509AlgorithmIdentifier_QueryDigestSize(PCRTCRX509ALGORITHMIDENTIFIER pThis)
{
    AssertPtrReturn(pThis, UINT32_MAX);

    /* common */
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_MD5))
        return 128 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA1))
        return 160 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA256))
        return 256 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512))
        return 512 / 8;

    /* Less common. */
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_MD2))
        return 128 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_MD4))
        return 128 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA384))
        return 384 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA224))
        return 224 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512T224))
        return 224 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_SHA512T256))
        return 256 / 8;
    if (!strcmp(pThis->Algorithm.szObjId, RTCRX509ALGORITHMIDENTIFIERID_WHIRLPOOL))
        return 512 / 8;

    return UINT32_MAX;
}


RTDECL(int) RTCrX509AlgorithmIdentifier_CompareWithString(PCRTCRX509ALGORITHMIDENTIFIER pThis, const char *pszObjId)
{
    return strcmp(pThis->Algorithm.szObjId, pszObjId);
}


RTDECL(int) RTCrX509AlgorithmIdentifier_CompareDigestOidAndEncryptedDigestOid(const char *pszDigestOid,
                                                                              const char *pszEncryptedDigestOid)
{
    /* common */
    if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD5))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA1))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA256))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA512))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA))
            return 0;
    }
    /* Less common. */
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD2))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD4))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA384))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA224))
    {
        if (!strcmp(pszEncryptedDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA))
            return 0;
    }
    else if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_WHIRLPOOL))
    {
        /* ?? */
    }
    else
        return -1;
    return 1;
}

RTDECL(int) RTCrX509AlgorithmIdentifier_CompareDigestAndEncryptedDigest(PCRTCRX509ALGORITHMIDENTIFIER pDigest,
                                                                        PCRTCRX509ALGORITHMIDENTIFIER pEncryptedDigest)
{
    return RTCrX509AlgorithmIdentifier_CompareDigestOidAndEncryptedDigestOid(pDigest->Algorithm.szObjId,
                                                                             pEncryptedDigest->Algorithm.szObjId);
}


RTDECL(const char *) RTCrX509AlgorithmIdentifier_CombineEncryptionOidAndDigestOid(const char *pszEncryptionOid,
                                                                                  const char *pszDigestOid)
{
    /* RSA: */
    if (!strcmp(pszEncryptionOid, RTCRX509ALGORITHMIDENTIFIERID_RSA))
    {
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD5)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA1)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA256)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA512)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD2)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD4)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA384)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA;
        if (   !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA224)
            || !strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA))
            return RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA;

        /* if (!strcmp(pszDigestOid, RTCRX509ALGORITHMIDENTIFIERID_WHIRLPOOL))
            return ???; */
    }
    else if (RTCrX509AlgorithmIdentifier_CompareDigestOidAndEncryptedDigestOid(pszDigestOid, pszEncryptionOid) == 0)
        return pszEncryptionOid;

    AssertMsgFailed(("enc=%s hash=%s\n", pszEncryptionOid, pszDigestOid));
    return NULL;
}


RTDECL(const char *) RTCrX509AlgorithmIdentifier_CombineEncryptionAndDigest(PCRTCRX509ALGORITHMIDENTIFIER pEncryption,
                                                                            PCRTCRX509ALGORITHMIDENTIFIER pDigest)
{
    return RTCrX509AlgorithmIdentifier_CombineEncryptionOidAndDigestOid(pEncryption->Algorithm.szObjId,
                                                                        pDigest->Algorithm.szObjId);
}


/*
 * Set of X.509 Algorithm Identifiers.
 */


/*
 * One X.509 AttributeTypeAndValue.
 */


/*
 * Set of X.509 AttributeTypeAndValues / X.509 RelativeDistinguishedName.
 */

/**
 * Slow code path of rtCrX509CanNameIsNothing.
 *
 * @returns true if @uc maps to nothing, false if not.
 * @param   uc          The unicode code point.
 */
static bool rtCrX509CanNameIsNothingSlow(RTUNICP uc)
{
    switch (uc)
    {
        /* 2.2 Map - Paragraph 1: */
        case 0x00ad:
        case 0x1806:
        case 0x034f:
        case 0x180b: case 0x180c: case 0x180d:

        case 0xfe00: case 0xfe01: case 0xfe02: case 0xfe03:
        case 0xfe04: case 0xfe05: case 0xfe06: case 0xfe07:
        case 0xfe08: case 0xfe09: case 0xfe0a: case 0xfe0b:
        case 0xfe0c: case 0xfe0d: case 0xfe0e: case 0xfe0f:

        case 0xfffc:

        /* 2.2 Map - Paragraph 3 (control code/function): */
        case 0x0000: case 0x0001: case 0x0002: case 0x0003:
        case 0x0004: case 0x0005: case 0x0006: case 0x0007:
        case 0x0008:

        case 0x000e: case 0x000f:
        case 0x0010: case 0x0011: case 0x0012: case 0x0013:
        case 0x0014: case 0x0015: case 0x0016: case 0x0017:
        case 0x0018: case 0x0019: case 0x001a: case 0x001b:
        case 0x001c: case 0x001d: case 0x001e: case 0x001f:

        case 0x007f:
        case 0x0080: case 0x0081: case 0x0082: case 0x0083:
        case 0x0084: /*case 0x0085:*/ case 0x0086: case 0x0087:
        case 0x0088: case 0x0089: case 0x008a: case 0x008b:
        case 0x008c: case 0x008d: case 0x008e: case 0x008f:
        case 0x0090: case 0x0091: case 0x0092: case 0x0093:
        case 0x0094: case 0x0095: case 0x0096: case 0x0097:
        case 0x0098: case 0x0099: case 0x009a: case 0x009b:
        case 0x009c: case 0x009d: case 0x009e: case 0x009f:

        case 0x06dd:
        case 0x070f:
        case 0x180e:
        case 0x200c: case 0x200d: case 0x200e: case 0x200f:
        case 0x202a: case 0x202b: case 0x202c: case 0x202d: case 0x202e:
        case 0x2060: case 0x2061: case 0x2062: case 0x2063:
        case 0x206a: case 0x206b: case 0x206c: case 0x206d: case 0x206e: case 0x206f:
        case 0xfeff:
        case 0xfff9: case 0xfffa: case 0xfffb:
        case 0x1d173: case 0x1d174: case 0x1d175: case 0x1d176: case 0x1d177: case 0x1d178: case 0x1d179: case 0x1d17a:
        case 0xe0001:
        case 0xe0020: case 0xe0021: case 0xe0022: case 0xe0023:
        case 0xe0024: case 0xe0025: case 0xe0026: case 0xe0027:
        case 0xe0028: case 0xe0029: case 0xe002a: case 0xe002b:
        case 0xe002c: case 0xe002d: case 0xe002e: case 0xe002f:
        case 0xe0030: case 0xe0031: case 0xe0032: case 0xe0033:
        case 0xe0034: case 0xe0035: case 0xe0036: case 0xe0037:
        case 0xe0038: case 0xe0039: case 0xe003a: case 0xe003b:
        case 0xe003c: case 0xe003d: case 0xe003e: case 0xe003f:
        case 0xe0040: case 0xe0041: case 0xe0042: case 0xe0043:
        case 0xe0044: case 0xe0045: case 0xe0046: case 0xe0047:
        case 0xe0048: case 0xe0049: case 0xe004a: case 0xe004b:
        case 0xe004c: case 0xe004d: case 0xe004e: case 0xe004f:
        case 0xe0050: case 0xe0051: case 0xe0052: case 0xe0053:
        case 0xe0054: case 0xe0055: case 0xe0056: case 0xe0057:
        case 0xe0058: case 0xe0059: case 0xe005a: case 0xe005b:
        case 0xe005c: case 0xe005d: case 0xe005e: case 0xe005f:
        case 0xe0060: case 0xe0061: case 0xe0062: case 0xe0063:
        case 0xe0064: case 0xe0065: case 0xe0066: case 0xe0067:
        case 0xe0068: case 0xe0069: case 0xe006a: case 0xe006b:
        case 0xe006c: case 0xe006d: case 0xe006e: case 0xe006f:
        case 0xe0070: case 0xe0071: case 0xe0072: case 0xe0073:
        case 0xe0074: case 0xe0075: case 0xe0076: case 0xe0077:
        case 0xe0078: case 0xe0079: case 0xe007a: case 0xe007b:
        case 0xe007c: case 0xe007d: case 0xe007e: case 0xe007f:

        /* 2.2 Map - Paragraph 4. */
        case 0x200b:
            return true;
    }
    return false;
}


/**
 * Checks if @a uc maps to nothing according to mapping rules of RFC-5280 and
 * RFC-4518.
 *
 * @returns true if @uc maps to nothing, false if not.
 * @param   uc          The unicode code point.
 */
DECLINLINE(bool) rtCrX509CanNameIsNothing(RTUNICP uc)
{
    if (uc > 0x001f && uc < 0x00ad)
        return false;
    return rtCrX509CanNameIsNothingSlow(uc);
}


/**
 * Slow code path of rtCrX509CanNameIsSpace.
 *
 * @returns true if space, false if not.
 * @param   uc          The unicode code point.
 */
static bool rtCrX509CanNameIsSpaceSlow(RTUNICP uc)
{
    switch (uc)
    {
        /* 2.2 Map - Paragraph 2. */
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x20:
        case 0x0085:
        case 0x00a0:
        case 0x1680:
        case 0x2000: case 0x2001: case 0x2002: case 0x2003:
        case 0x2004: case 0x2005: case 0x2006: case 0x2007:
        case 0x2008: case 0x2009: case 0x200a:
        case 0x2028: case 0x2029:
        case 0x202f:
        case 0x205f:
        case 0x3000:
            return true;
    }
    return false;
}


/**
 * Checks if @a uc is a space character according to the mapping rules of
 * RFC-5280 and RFC-4518.
 *
 * @returns true if space, false if not.
 * @param   uc          The unicode code point.
 */
DECLINLINE(bool) rtCrX509CanNameIsSpace(RTUNICP uc)
{
    if (uc < 0x0085)
    {
        if (uc > 0x0020)
            return false;
        if (uc == 0x0020) /* space */
            return true;
    }
    return rtCrX509CanNameIsSpaceSlow(uc);
}


static const char *rtCrX509CanNameStripLeft(const char *psz, size_t *pcch)
{
    /*
     * Return space when we've encountered the first non-space-non-nothing code point.
     */
    const char * const pszStart = psz;
    const char        *pszPrev;
    for (;;)
    {
        pszPrev = psz;
        RTUNICP uc;
        int rc = RTStrGetCpEx(&psz, &uc);
        AssertRCBreak(rc);
        if (!uc)
        {
            if ((uintptr_t)(pszPrev - pszStart) >= *pcch)
                break;
            /* NUL inside the string, maps to nothing => ignore it. */
        }
        else if (!rtCrX509CanNameIsSpace(uc) && !rtCrX509CanNameIsNothing(uc))
            break;
    }
    *pcch -= pszPrev - pszStart;
    return pszPrev;
}


static RTUNICP rtCrX509CanNameGetNextCpWithMappingSlowSpace(const char **ppsz, size_t *pcch)
{
    /*
     * Return space when we've encountered the first non-space-non-nothing code point.
     */
    RTUNICP            uc;
    const char        *psz      = *ppsz;
    const char * const pszStart = psz;
    const char        *pszPrev;
    for (;;)
    {
        pszPrev = psz;
        int rc = RTStrGetCpEx(&psz, &uc);
        AssertRCBreakStmt(rc, uc = 0x20);
        if (!uc)
        {
            if ((uintptr_t)(pszPrev - pszStart) >= *pcch)
            {
                uc = 0;     /* End of string: Ignore trailing spaces. */
                break;
            }
            /* NUL inside the string, maps to nothing => ignore it. */
        }
        else if (!rtCrX509CanNameIsSpace(uc) && !rtCrX509CanNameIsNothing(uc))
        {
            uc = 0x20;      /* Return space before current char. */
            break;
        }
    }

    *ppsz  = pszPrev;
    *pcch -= pszPrev - pszStart;
    return uc;
}


DECLINLINE(RTUNICP) rtCrX509CanNameGetNextCpIgnoreNul(const char **ppsz, size_t *pcch)
{
    while (*pcch > 0)
    {
        const char *psz = *ppsz;
        RTUNICP uc = *psz;
        if (uc < 0x80)
        {
            *pcch -= 1;
            *ppsz = psz + 1;
        }
        else
        {
            int rc = RTStrGetCpEx(ppsz, &uc);
            AssertRCReturn(rc, uc);
            size_t cchCp = *ppsz - psz;
            AssertReturn(cchCp <= *pcch, 0);
            *pcch -= cchCp;
        }
        if (uc != 0)
            return uc;
    }
    return 0;
}


static RTUNICP rtCrX509CanNameGetNextCpWithMappingSlowNothing(const char **ppsz, size_t *pcch)
{
    /*
     * Return first code point which doesn't map to nothing.  If we encounter
     * a space, we defer to the mapping-after-space routine above.
     */
    for (;;)
    {
        RTUNICP uc = rtCrX509CanNameGetNextCpIgnoreNul(ppsz, pcch);
        if (rtCrX509CanNameIsSpace(uc))
            return rtCrX509CanNameGetNextCpWithMappingSlowSpace(ppsz, pcch);
        if (!rtCrX509CanNameIsNothing(uc) || uc == 0)
            return uc;
    }
}


DECLINLINE(RTUNICP) rtCrX509CanNameGetNextCpWithMapping(const char **ppsz, size_t *pcch)
{
    RTUNICP uc = rtCrX509CanNameGetNextCpIgnoreNul(ppsz, pcch);
    if (uc)
    {
        if (!rtCrX509CanNameIsSpace(uc))
        {
            if (!rtCrX509CanNameIsNothing(uc))
                return uc;
            return rtCrX509CanNameGetNextCpWithMappingSlowNothing(ppsz, pcch);
        }
        return rtCrX509CanNameGetNextCpWithMappingSlowSpace(ppsz, pcch);
    }
    return uc;
}


RTDECL(bool) RTCrX509AttributeTypeAndValue_MatchAsRdnByRfc5280(PCRTCRX509ATTRIBUTETYPEANDVALUE pLeft,
                                                               PCRTCRX509ATTRIBUTETYPEANDVALUE pRight)
{
    if (RTAsn1ObjId_Compare(&pLeft->Type, &pRight->Type) == 0)
    {
        /*
         * Try for perfect match in case we get luck.
         */
#ifdef DEBUG_bird  /* Want to test the complicated code path first */
        if (pLeft->Value.enmType != RTASN1TYPE_STRING || pRight->Value.enmType != RTASN1TYPE_STRING)
#endif
        if (RTAsn1DynType_Compare(&pLeft->Value, &pRight->Value) == 0)
            return true;

        /*
         * If both are string types, we can compare them according to RFC-5280.
         */
        if (   pLeft->Value.enmType == RTASN1TYPE_STRING
            && pRight->Value.enmType == RTASN1TYPE_STRING)
        {
            size_t      cchLeft;
            const char *pszLeft;
            int rc = RTAsn1String_QueryUtf8(&pLeft->Value.u.String, &pszLeft, &cchLeft);
            if (RT_SUCCESS(rc))
            {
                size_t      cchRight;
                const char *pszRight;
                rc = RTAsn1String_QueryUtf8(&pRight->Value.u.String, &pszRight, &cchRight);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Perform a simplified RFC-5280 comparsion.
                     * The algorithm as be relaxed on the following counts:
                     *      1. No unicode normalization.
                     *      2. Prohibited characters not checked for.
                     *      3. Bidirectional characters are not ignored.
                     */
                    pszLeft  = rtCrX509CanNameStripLeft(pszLeft, &cchLeft);
                    pszRight = rtCrX509CanNameStripLeft(pszRight, &cchRight);
                    while (*pszLeft && *pszRight)
                    {
                        RTUNICP ucLeft  = rtCrX509CanNameGetNextCpWithMapping(&pszLeft, &cchLeft);
                        RTUNICP ucRight = rtCrX509CanNameGetNextCpWithMapping(&pszRight, &cchRight);
                        if (ucLeft != ucRight)
                        {
                            ucLeft  = RTUniCpToLower(ucLeft);
                            ucRight = RTUniCpToLower(ucRight);
                            if (ucLeft != ucRight)
                                return false;
                        }
                    }

                    return cchRight == 0 && cchLeft == 0;
                }
            }
        }
    }
    return false;
}


RTDECL(bool) RTCrX509RelativeDistinguishedName_MatchByRfc5280(PCRTCRX509RELATIVEDISTINGUISHEDNAME pLeft,
                                                              PCRTCRX509RELATIVEDISTINGUISHEDNAME pRight)
{
    /*
     * No match if the attribute count differs.
     */
    uint32_t const cItems = pLeft->cItems;
    if (cItems == pRight->cItems)
    {
        /*
         * Compare each attribute, but don't insist on the same order nor
         * bother checking for duplicates (too complicated).
         */
        for (uint32_t iLeft = 0; iLeft < cItems; iLeft++)
        {
            PCRTCRX509ATTRIBUTETYPEANDVALUE pLeftAttr = pLeft->papItems[iLeft];
            bool fFound = false;
            for (uint32_t iRight = 0; iRight < cItems; iRight++)
                if (RTCrX509AttributeTypeAndValue_MatchAsRdnByRfc5280(pLeftAttr, pRight->papItems[iRight]))
                {
                    fFound = true;
                    break;
                }
            if (!fFound)
                return false;
        }
        return true;
    }
    return false;

}


/*
 * X.509 Name.
 */

RTDECL(bool) RTCrX509Name_MatchByRfc5280(PCRTCRX509NAME pLeft, PCRTCRX509NAME pRight)
{
    uint32_t const cItems = pLeft->cItems;
    if (cItems == pRight->cItems)
    {
        /* Require exact order. */
        for (uint32_t iRdn = 0; iRdn < cItems; iRdn++)
            if (!RTCrX509RelativeDistinguishedName_MatchByRfc5280(pLeft->papItems[iRdn], pRight->papItems[iRdn]))
                return false;
        return true;
    }
    return false;
}


RTDECL(bool) RTCrX509Name_ConstraintMatch(PCRTCRX509NAME pConstraint, PCRTCRX509NAME pName)
{
    /*
     * Check that the constraint is a prefix of the name.  This means that
     * the name must have at least as many components and the constraint.
     */
    if (pName->cItems >= pConstraint->cItems)
    {
        /*
         * Parallel crawl of the two RDNs arrays.
         */
        for (uint32_t i = 0; pConstraint->cItems; i++)
        {
            PCRTCRX509RELATIVEDISTINGUISHEDNAME pConstrRdns = pConstraint->papItems[i];
            PCRTCRX509RELATIVEDISTINGUISHEDNAME pNameRdns   = pName->papItems[i];

            /*
             * Walk the constraint attribute & value array.
             */
            for (uint32_t iConstrAttrib = 0; iConstrAttrib < pConstrRdns->cItems; iConstrAttrib++)
            {
                PCRTCRX509ATTRIBUTETYPEANDVALUE pConstrAttrib = pConstrRdns->papItems[iConstrAttrib];

                /*
                 * Find matching attribute & value in the name.
                 */
                bool fFound = false;
                for (uint32_t iNameAttrib = 0; iNameAttrib < pNameRdns->cItems; iNameAttrib++)
                    if (RTCrX509AttributeTypeAndValue_MatchAsRdnByRfc5280(pConstrAttrib, pNameRdns->papItems[iNameAttrib]))
                    {
                        fFound = true;
                        break;
                    }
                if (fFound)
                    return false;
            }
        }
        return true;
    }
    return false;
}


/**
 * Mapping between X.500 object IDs and short and long names.
 *
 * See RFC-1327, RFC-4519 ...
 */
static struct
{
    const char *pszOid;
    const char *pszShortNm;
    size_t      cchShortNm;
    const char *pszLongNm;
} const g_aRdnMap[] =
{
    {   "0.9.2342.19200300.100.1.3",  RT_STR_TUPLE("Mail"),                 "Rfc822Mailbox" },
    {   "0.9.2342.19200300.100.1.25", RT_STR_TUPLE("DC"),                   "DomainComponent" },
    {   "1.2.840.113549.1.9.1",       RT_STR_TUPLE("Email") /*nonstandard*/,"EmailAddress" },
    {   "2.5.4.3",                    RT_STR_TUPLE("CN"),                   "CommonName" },
    {   "2.5.4.4",                    RT_STR_TUPLE("SN"),                   "Surname" },
    {   "2.5.4.5",                    RT_STR_TUPLE("SRN") /*nonstandard*/,  "SerialNumber" },
    {   "2.5.4.6",                    RT_STR_TUPLE("C"),                    "CountryName" },
    {   "2.5.4.7",                    RT_STR_TUPLE("L"),                    "LocalityName" },
    {   "2.5.4.8",                    RT_STR_TUPLE("ST"),                   "StateOrProviceName" },
    {   "2.5.4.9",                    RT_STR_TUPLE("street"),               "Street" },
    {   "2.5.4.10",                   RT_STR_TUPLE("O"),                    "OrganizationName" },
    {   "2.5.4.11",                   RT_STR_TUPLE("OU"),                   "OrganizationalUnitName" },
    {   "2.5.4.12",                   RT_STR_TUPLE("title"),                "Title" },
    {   "2.5.4.13",                   RT_STR_TUPLE("desc"),                 "Description" },
    {   "2.5.4.15",                   RT_STR_TUPLE("BC") /*nonstandard*/,   "BusinessCategory" },
    {   "2.5.4.17",                   RT_STR_TUPLE("ZIP") /*nonstandard*/,  "PostalCode" },
    {   "2.5.4.18",                   RT_STR_TUPLE("POBox") /*nonstandard*/,"PostOfficeBox" },
    {   "2.5.4.20",                   RT_STR_TUPLE("PN") /*nonstandard*/,   "TelephoneNumber" },
    {   "2.5.4.33",                   RT_STR_TUPLE("RO") /*nonstandard*/,   "RoleOccupant" },
    {   "2.5.4.34",                   RT_STR_TUPLE("SA") /*nonstandard*/,   "StreetAddress" },
    {   "2.5.4.41",                   RT_STR_TUPLE("N") /*nonstandard*/,    "Name" },
    {   "2.5.4.42",                   RT_STR_TUPLE("GN"),                   "GivenName" },
    {   "2.5.4.43",                   RT_STR_TUPLE("I") /*nonstandard*/,    "Initials" },
    {   "2.5.4.44",                   RT_STR_TUPLE("GQ") /*nonstandard*/,   "GenerationQualifier" },
    {   "2.5.4.46",                   RT_STR_TUPLE("DNQ") /*nonstandard*/,  "DNQualifier" },
    {   "2.5.4.51",                   RT_STR_TUPLE("HID") /*nonstandard*/,  "HouseIdentifier" },
};


RTDECL(const char *) RTCrX509Name_GetShortRdn(PCRTASN1OBJID pRdnId)
{
    uint32_t iName = RT_ELEMENTS(g_aRdnMap);
    while (iName-- > 0)
        if (RTAsn1ObjId_CompareWithString(pRdnId, g_aRdnMap[iName].pszOid) == 0)
            return g_aRdnMap[iName].pszShortNm;
    return NULL;
}


RTDECL(bool) RTCrX509Name_MatchWithString(PCRTCRX509NAME pThis, const char *pszString)
{
    /* Keep track of the string length. */
    size_t cchString = strlen(pszString);

    /*
     * The usual double loop for walking the components.
     */
    for (uint32_t i = 0; i < pThis->cItems; i++)
    {
        PCRTCRX509RELATIVEDISTINGUISHEDNAME pRdn = pThis->papItems[i];
        for (uint32_t j = 0; j < pRdn->cItems; j++)
        {
            PCRTCRX509ATTRIBUTETYPEANDVALUE pComponent = pRdn->papItems[j];

            /*
             * Must be a string.
             */
            if (pComponent->Value.enmType != RTASN1TYPE_STRING)
                return false;

            /*
             * Look up the component name prefix and check whether it's also in the string.
             */
            uint32_t iName = RT_ELEMENTS(g_aRdnMap);
            while (iName-- > 0)
                if (RTAsn1ObjId_CompareWithString(&pComponent->Type, g_aRdnMap[iName].pszOid) == 0)
                    break;
            AssertMsgReturn(iName != UINT32_MAX, ("Please extend g_aRdnMap with '%s'.\n", pComponent->Type.szObjId), false);

            if (   strncmp(pszString, g_aRdnMap[iName].pszShortNm, g_aRdnMap[iName].cchShortNm) != 0
                || pszString[g_aRdnMap[iName].cchShortNm] != '=')
                return false;

            pszString += g_aRdnMap[iName].cchShortNm + 1;
            cchString -= g_aRdnMap[iName].cchShortNm + 1;

            /*
             * Compare the component string.
             */
            size_t cchComponent;
            int rc = RTAsn1String_QueryUtf8Len(&pComponent->Value.u.String, &cchComponent);
            AssertRCReturn(rc, false);

            if (cchComponent > cchString)
                return false;
            if (RTAsn1String_CompareWithString(&pComponent->Value.u.String, pszString, cchComponent) != 0)
                return false;

            cchString -= cchComponent;
            pszString += cchComponent;

            /*
             * Check separator comma + space and skip extra spaces before the next component.
             */
            if (cchString)
            {
                if (pszString[0] != ',')
                    return false;
                if (pszString[1] != ' ' && pszString[1] != '\t')
                    return false;
                pszString += 2;
                cchString -= 2;

                while (*pszString == ' ' || *pszString == '\t')
                {
                    pszString++;
                    cchString--;
                }
            }
        }
    }

    /*
     * If we got thru the whole name and the whole string, we're good.
     */
    return *pszString == '\0';
}


RTDECL(int) RTCrX509Name_FormatAsString(PCRTCRX509NAME pThis, char *pszBuf, size_t cbBuf, size_t *pcbActual)
{
    /*
     * The usual double loop for walking the components.
     */
    size_t off = 0;
    int    rc  = VINF_SUCCESS;
    for (uint32_t i = 0; i < pThis->cItems; i++)
    {
        PCRTCRX509RELATIVEDISTINGUISHEDNAME pRdn = pThis->papItems[i];
        for (uint32_t j = 0; j < pRdn->cItems; j++)
        {
            PCRTCRX509ATTRIBUTETYPEANDVALUE pComponent = pRdn->papItems[j];

            /*
             * Must be a string.
             */
            if (pComponent->Value.enmType != RTASN1TYPE_STRING)
                return VERR_CR_X509_NAME_NOT_STRING;

            /*
             * Look up the component name prefix.
             */
            uint32_t iName = RT_ELEMENTS(g_aRdnMap);
            while (iName-- > 0)
                if (RTAsn1ObjId_CompareWithString(&pComponent->Type, g_aRdnMap[iName].pszOid) == 0)
                    break;
            AssertMsgReturn(iName != UINT32_MAX, ("Please extend g_aRdnMap with '%s'.\n", pComponent->Type.szObjId),
                            VERR_CR_X509_NAME_MISSING_RDN_MAP_ENTRY);

            /*
             * Append the prefix.
             */
            if (off)
            {
                if (off + 2 < cbBuf)
                {
                    pszBuf[off]     = ',';
                    pszBuf[off + 1] = ' ';
                }
                else
                    rc = VERR_BUFFER_OVERFLOW;
                off += 2;
            }

            if (off + g_aRdnMap[iName].cchShortNm + 1 < cbBuf)
            {
                memcpy(&pszBuf[off], g_aRdnMap[iName].pszShortNm, g_aRdnMap[iName].cchShortNm);
                pszBuf[off + g_aRdnMap[iName].cchShortNm] = '=';
            }
            else
                rc = VERR_BUFFER_OVERFLOW;
            off += g_aRdnMap[iName].cchShortNm + 1;

            /*
             * Add the component string.
             */
            const char *pszUtf8;
            size_t      cchUtf8;
            int rc2 = RTAsn1String_QueryUtf8(&pComponent->Value.u.String, &pszUtf8, &cchUtf8);
            AssertRCReturn(rc2, rc2);
            if (off + cchUtf8 < cbBuf)
                memcpy(&pszBuf[off], pszUtf8, cchUtf8);
            else
                rc = VERR_BUFFER_OVERFLOW;
            off += cchUtf8;
        }
    }

    if (pcbActual)
        *pcbActual = off + 1;
    if (off < cbBuf)
        pszBuf[off] = '\0';
    return rc;
}



/*
 * One X.509 GeneralName.
 */

/**
 * Name constraint matching (RFC-5280): DNS Name.
 *
 * @returns true on match, false on mismatch.
 * @param   pConstraint     The constraint name.
 * @param   pName           The name to match against the constraint.
 */
static bool rtCrX509GeneralName_ConstraintMatchDnsName(PCRTCRX509GENERALNAME pConstraint, PCRTCRX509GENERALNAME pName)
{
    /*
     * Empty constraint string is taken to match everything.
     */
    if (pConstraint->u.pT2_DnsName->Asn1Core.cb == 0)
        return true;

    /*
     * Get the UTF-8 strings for the two.
     */
    size_t      cchConstraint;
    char const *pszConstraint;
    int rc = RTAsn1String_QueryUtf8(pConstraint->u.pT2_DnsName, &pszConstraint, &cchConstraint);
    if (RT_SUCCESS(rc))
    {
        size_t      cchFull;
        char const *pszFull;
        rc = RTAsn1String_QueryUtf8(pName->u.pT2_DnsName, &pszFull, &cchFull);
        if (RT_SUCCESS(rc))
        {
            /*
             * No match if the constraint is longer.
             */
            if (cchConstraint > cchFull)
                return false;

            /*
             * No match if the constraint and name tail doesn't match
             * in a case-insensitive compare.
             */
            size_t offFull = cchFull - cchConstraint;
            if (RTStrICmp(&pszFull[offFull], pszConstraint) != 0)
                return false;
            if (!offFull)
                return true;

            /*
             * The matching constraint must be delimited by a dot in the full
             * name.  There seems to be some discussion whether ".oracle.com"
             * should match "www..oracle.com".  This implementation does choose
             * to not succeed in that case.
             */
            if ((pszFull[offFull - 1] == '.') ^ (pszFull[offFull] == '.'))
                return true;

            return false;
        }
    }

    /* fall back. */
    return RTCrX509GeneralName_Compare(pConstraint, pName) == 0;
}


/**
 * Name constraint matching (RFC-5280): RFC-822 (email).
 *
 * @returns true on match, false on mismatch.
 * @param   pConstraint     The constraint name.
 * @param   pName           The name to match against the constraint.
 */
static bool rtCrX509GeneralName_ConstraintMatchRfc822Name(PCRTCRX509GENERALNAME pConstraint, PCRTCRX509GENERALNAME pName)
{
    /*
     * Empty constraint string is taken to match everything.
     */
    if (pConstraint->u.pT1_Rfc822->Asn1Core.cb == 0)
        return true;

    /*
     * Get the UTF-8 strings for the two.
     */
    size_t      cchConstraint;
    char const *pszConstraint;
    int rc = RTAsn1String_QueryUtf8(pConstraint->u.pT1_Rfc822, &pszConstraint, &cchConstraint);
    if (RT_SUCCESS(rc))
    {
        size_t      cchFull;
        char const *pszFull;
        rc = RTAsn1String_QueryUtf8(pName->u.pT1_Rfc822, &pszFull, &cchFull);
        if (RT_SUCCESS(rc))
        {
            /*
             * No match if the constraint is longer.
             */
            if (cchConstraint > cchFull)
                return false;

            /*
             * A lone dot matches everything.
             */
            if (cchConstraint == 1 && *pszConstraint == '.')
                return true;

            /*
             * If there is a '@' in the constraint, the entire address must match.
             */
            const char *pszConstraintAt = (const char *)memchr(pszConstraint, '@', cchConstraint);
            if (pszConstraintAt)
                return cchConstraint == cchFull && RTStrICmp(pszConstraint, pszFull) == 0;

            /*
             * No match if the constraint and name tail doesn't match
             * in a case-insensitive compare.
             */
            size_t offFull = cchFull - cchConstraint;
            if (RTStrICmp(&pszFull[offFull], pszConstraint) != 0)
                return false;

            /*
             * If the constraint starts with a dot, we're supposed to be
             * satisfied with a tail match.
             */
            /** @todo Check if this should match even if offFull == 0. */
            if (*pszConstraint == '.')
                return true;

            /*
             * Otherwise, we require a hostname match and thus expect an '@'
             * immediatly preceding the constraint match.
             */
            if (pszFull[offFull - 1] == '@')
                return true;

            return false;
        }
    }

    /* fall back. */
    return RTCrX509GeneralName_Compare(pConstraint, pName) == 0;
}


/**
 * Extracts the hostname from an URI.
 *
 * @returns true if successfully extract, false if no hostname present.
 * @param   pszUri          The URI.
 * @param   pchHostName     .
 * @param   pcchHostName    .
 */
static bool rtCrX509GeneralName_ExtractHostName(const char *pszUri,  const char **pchHostName,  size_t *pcchHostName)
{
    /*
     * Skip the schema name.
     */
    const char *pszStart = strchr(pszUri, ':');
    while (pszStart && (pszStart[1] != '/' || pszStart[2] != '/'))
        pszStart = strchr(pszStart + 1, ':');
    if (pszStart)
    {
        pszStart += 3;

        /*
         * The name ends with the first slash or ":port".
         */
        const char *pszEnd = strchr(pszStart, '/');
        if (!pszEnd)
            pszEnd = strchr(pszStart, '\0');
        if (memchr(pszStart, ':', pszEnd - pszStart))
            do
                pszEnd--;
            while (*pszEnd != ':');
        if (pszEnd != pszStart)
        {
            /*
             * Drop access credentials at the front of the string if present.
             */
            const char *pszAt = (const char *)memchr(pszStart, '@', pszEnd - pszStart);
            if (pszAt)
                pszStart = pszAt + 1;

            /*
             * If there is still some string left, that's the host name.
             */
            if (pszEnd != pszStart)
            {
                *pcchHostName = pszEnd - pszStart;
                *pchHostName  = pszStart;
                return true;
            }
        }
    }

    *pcchHostName = 0;
    *pchHostName  = NULL;
    return false;
}


/**
 * Name constraint matching (RFC-5280): URI.
 *
 * @returns true on match, false on mismatch.
 * @param   pConstraint     The constraint name.
 * @param   pName           The name to match against the constraint.
 */
static bool rtCrX509GeneralName_ConstraintMatchUri(PCRTCRX509GENERALNAME pConstraint, PCRTCRX509GENERALNAME pName)
{
    /*
     * Empty constraint string is taken to match everything.
     */
    if (pConstraint->u.pT6_Uri->Asn1Core.cb == 0)
        return true;

    /*
     * Get the UTF-8 strings for the two.
     */
    size_t      cchConstraint;
    char const *pszConstraint;
    int rc = RTAsn1String_QueryUtf8(pConstraint->u.pT6_Uri, &pszConstraint, &cchConstraint);
    if (RT_SUCCESS(rc))
    {
        size_t      cchFull;
        char const *pszFull;
        rc = RTAsn1String_QueryUtf8(pName->u.pT6_Uri, &pszFull, &cchFull);
        if (RT_SUCCESS(rc))
        {
            /*
             * Isolate the hostname in the name.
             */
            size_t      cchHostName;
            const char *pchHostName;
            if (rtCrX509GeneralName_ExtractHostName(pszFull, &pchHostName, &cchHostName))
            {
                /*
                 * Domain constraint.
                 */
                if (*pszConstraint == '.')
                {
                    if (cchHostName >= cchConstraint)
                    {
                        size_t offHostName = cchHostName - cchConstraint;
                        if (RTStrICmp(&pchHostName[offHostName], pszConstraint) == 0)
                        {
                            /* "http://www..oracle.com" does not match ".oracle.com".
                               It's debatable whether "http://.oracle.com/" should match. */
                            if (  !offHostName
                                || pchHostName[offHostName - 1] != '.')
                                return true;
                        }
                    }
                }
                /*
                 * Host name constraint.  Full match required.
                 */
                else if (   cchHostName == cchConstraint
                         && RTStrNICmp(pchHostName, pszConstraint, cchHostName) == 0)
                    return true;
            }
            return false;
        }
    }

    /* fall back. */
    return RTCrX509GeneralName_Compare(pConstraint, pName) == 0;
}


/**
 * Name constraint matching (RFC-5280): IP address.
 *
 * @returns true on match, false on mismatch.
 * @param   pConstraint     The constraint name.
 * @param   pName           The name to match against the constraint.
 */
static bool rtCrX509GeneralName_ConstraintMatchIpAddress(PCRTCRX509GENERALNAME pConstraint, PCRTCRX509GENERALNAME pName)
{
    uint8_t const *pbConstraint = pConstraint->u.pT7_IpAddress->Asn1Core.uData.pu8;
    uint8_t const *pbFull       = pName->u.pT7_IpAddress->Asn1Core.uData.pu8;

    /*
     * IPv4.
     */
    if (   pConstraint->u.pT7_IpAddress->Asn1Core.cb == 8  /* ip+netmask*/
        && pName->u.pT7_IpAddress->Asn1Core.cb       == 4) /* ip */
        return ((pbFull[0] ^ pbConstraint[0]) & pbConstraint[4]) == 0
            && ((pbFull[1] ^ pbConstraint[1]) & pbConstraint[5]) == 0
            && ((pbFull[2] ^ pbConstraint[2]) & pbConstraint[6]) == 0
            && ((pbFull[3] ^ pbConstraint[3]) & pbConstraint[7]) == 0;

    /*
     * IPv6.
     */
    if (   pConstraint->u.pT7_IpAddress->Asn1Core.cb == 32  /* ip+netmask*/
        && pName->u.pT7_IpAddress->Asn1Core.cb       == 16) /* ip */
    {
        for (uint32_t i = 0; i < 16; i++)
            if (((pbFull[i] ^ pbConstraint[i]) & pbConstraint[i + 16]) != 0)
                return false;
        return true;
    }

    return RTCrX509GeneralName_Compare(pConstraint, pName) == 0;
}


RTDECL(bool) RTCrX509GeneralName_ConstraintMatch(PCRTCRX509GENERALNAME pConstraint, PCRTCRX509GENERALNAME pName)
{
    if (pConstraint->enmChoice == pName->enmChoice)
    {
        if (RTCRX509GENERALNAME_IS_DIRECTORY_NAME(pConstraint))
            return RTCrX509Name_ConstraintMatch(&pConstraint->u.pT4->DirectoryName,  &pName->u.pT4->DirectoryName);

        if (RTCRX509GENERALNAME_IS_DNS_NAME(pConstraint))
            return rtCrX509GeneralName_ConstraintMatchDnsName(pConstraint, pName);

        if (RTCRX509GENERALNAME_IS_RFC822_NAME(pConstraint))
            return rtCrX509GeneralName_ConstraintMatchRfc822Name(pConstraint, pName);

        if (RTCRX509GENERALNAME_IS_URI(pConstraint))
            return rtCrX509GeneralName_ConstraintMatchUri(pConstraint, pName);

        if (RTCRX509GENERALNAME_IS_IP_ADDRESS(pConstraint))
            return rtCrX509GeneralName_ConstraintMatchIpAddress(pConstraint, pName);

        AssertFailed();
        return RTCrX509GeneralName_Compare(pConstraint, pName) == 0;
    }
    return false;
}


/*
 * Sequence of X.509 GeneralNames.
 */


/*
 * X.509 UniqueIdentifier.
 */


/*
 * X.509 SubjectPublicKeyInfo.
 */


/*
 * X.509 AuthorityKeyIdentifier (IPRT representation).
 */


/*
 * One X.509 PolicyQualifierInfo.
 */


/*
 * Sequence of X.509 PolicyQualifierInfo.
 */


/*
 * One X.509 PolicyInformation.
 */


/*
 * Sequence of X.509 CertificatePolicies.
 */


/*
 * One X.509 PolicyMapping (IPRT representation).
 */


/*
 * Sequence of X.509 PolicyMappings (IPRT representation).
 */


/*
 * X.509 BasicConstraints (IPRT representation).
 */


/*
 * X.509 GeneralSubtree (IPRT representation).
 */


RTDECL(bool) RTCrX509GeneralSubtree_ConstraintMatch(PCRTCRX509GENERALSUBTREE pConstraint, PCRTCRX509GENERALSUBTREE pName)
{
    return RTCrX509GeneralName_ConstraintMatch(&pConstraint->Base, &pName->Base);
}


/*
 * Sequence of X.509 GeneralSubtrees (IPRT representation).
 */


/*
 * X.509 NameConstraints (IPRT representation).
 */


/*
 * X.509 PolicyConstraints (IPRT representation).
 */


/*
 * One X.509 Extension.
 */


/*
 * Sequence of X.509 Extensions.
 */


/*
 * X.509 TbsCertificate.
 */

static void rtCrx509TbsCertificate_AddKeyUsageFlags(PRTCRX509TBSCERTIFICATE pThis, PCRTCRX509EXTENSION pExtension)
{
    AssertReturnVoid(pExtension->enmValue == RTCRX509EXTENSIONVALUE_BIT_STRING);
    /* 3 = 1 byte for unused bit count, followed by one or two bytes containing actual bits. RFC-5280 defines bits 0 thru 8. */
    AssertReturnVoid(pExtension->ExtnValue.pEncapsulated->cb <= 3);
    pThis->T3.fKeyUsage |= (uint32_t)RTAsn1BitString_GetAsUInt64((PCRTASN1BITSTRING)pExtension->ExtnValue.pEncapsulated);
}


static void rtCrx509TbsCertificate_AddExtKeyUsageFlags(PRTCRX509TBSCERTIFICATE pThis, PCRTCRX509EXTENSION pExtension)
{
    AssertReturnVoid(pExtension->enmValue == RTCRX509EXTENSIONVALUE_SEQ_OF_OBJ_IDS);
    PCRTASN1SEQOFOBJIDS pObjIds = (PCRTASN1SEQOFOBJIDS)pExtension->ExtnValue.pEncapsulated;
    uint32_t i = pObjIds->cItems;
    while (i-- > 0)
    {

        if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_ANY_EXTENDED_KEY_USAGE_OID) == 0)
            pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_ANY;
        else if (RTAsn1ObjId_StartsWith(pObjIds->papItems[i], RTCRX509_ID_KP_OID))
        {
            if (RTAsn1ObjIdCountComponents(pObjIds->papItems[i]) == 9)
                switch (RTAsn1ObjIdGetLastComponentsAsUInt32(pObjIds->papItems[i]))
                {
                    case  1: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_SERVER_AUTH; break;
                    case  2: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_CLIENT_AUTH; break;
                    case  3: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_CODE_SIGNING; break;
                    case  4: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_EMAIL_PROTECTION; break;
                    case  5: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_IPSEC_END_SYSTEM; break;
                    case  6: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_IPSEC_TUNNEL; break;
                    case  7: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_IPSEC_USER; break;
                    case  8: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_TIMESTAMPING; break;
                    case  9: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OCSP_SIGNING; break;
                    case 10: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_DVCS; break;
                    case 11: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_SBGP_CERT_AA_SERVICE_AUTH; break;
                    case 13: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_EAP_OVER_PPP; break;
                    case 14: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_EAP_OVER_LAN; break;
                    default: pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OTHER; break;
                }
            else
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OTHER;
        }
        else if (RTAsn1ObjId_StartsWith(pObjIds->papItems[i], RTCRX509_APPLE_EKU_APPLE_EXTENDED_KEY_USAGE_OID))
        {
            if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_CODE_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_CODE_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_CODE_SIGNING_DEVELOPMENT_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_CODE_SIGNING_DEVELOPMENT;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_SOFTWARE_UPDATE_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_SOFTWARE_UPDATE_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_CODE_SIGNING_THRID_PARTY_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_CODE_SIGNING_THIRD_PARTY;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_RESOURCE_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_RESOURCE_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_APPLE_EKU_SYSTEM_IDENTITY_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_APPLE_SYSTEM_IDENTITY;
            else
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OTHER;
        }
        else if (RTAsn1ObjId_StartsWith(pObjIds->papItems[i], "1.3.6.1.4.1.311"))
        {
            if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_TIMESTAMP_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_TIMESTAMP_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_NT5_CRYPTO_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_NT5_CRYPTO;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_OEM_WHQL_CRYPTO_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_OEM_WHQL_CRYPTO;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_EMBEDDED_NT_CRYPTO_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_EMBEDDED_NT_CRYPTO;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_KERNEL_MODE_CODE_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_KERNEL_MODE_CODE_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_LIFETIME_SIGNING_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_LIFETIME_SIGNING;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_DRM_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_DRM;
            else if (RTAsn1ObjId_CompareWithString(pObjIds->papItems[i], RTCRX509_MS_EKU_DRM_INDIVIDUALIZATION_OID) == 0)
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_MS_DRM_INDIVIDUALIZATION;
            else
                pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OTHER;
        }
        else
            pThis->T3.fExtKeyUsage |= RTCRX509CERT_EKU_F_OTHER;
    }
}


/**
 * (Re-)Process the certificate extensions.
 *
 * Will fail if duplicate extensions are encountered.
 *
 * @returns IPRT status code.
 * @param   pThis               The to-be-signed certificate part.
 * @param   pErrInfo            Where to return extended error details,
 *                              optional.
 */
RTDECL(int) RTCrX509TbsCertificate_ReprocessExtensions(PRTCRX509TBSCERTIFICATE pThis, PRTERRINFO pErrInfo)
{
    /*
     * Clear all variables we will set.
     */
    pThis->T3.fFlags                    = 0;
    pThis->T3.fKeyUsage                 = 0;
    pThis->T3.fExtKeyUsage              = 0;
    pThis->T3.pAuthorityKeyIdentifier   = NULL;
    pThis->T3.pSubjectKeyIdentifier     = NULL;
    pThis->T3.pAltSubjectName           = NULL;
    pThis->T3.pAltIssuerName            = NULL;
    pThis->T3.pCertificatePolicies      = NULL;
    pThis->T3.pPolicyMappings           = NULL;
    pThis->T3.pBasicConstraints         = NULL;
    pThis->T3.pNameConstraints          = NULL;
    pThis->T3.pPolicyConstraints        = NULL;
    pThis->T3.pInhibitAnyPolicy         = NULL;

#define CHECK_SET_PRESENT_RET_ON_DUP(a_pThis, a_pErrInfo, a_fPresentFlag) \
    do { \
        if ((a_pThis)->T3.fFlags & (a_fPresentFlag)) \
            return RTErrInfoSet(a_pErrInfo, VERR_CR_X509_TBSCERT_DUPLICATE_EXTENSION, \
                                "Duplicate extension " #a_fPresentFlag); \
        (a_pThis)->T3.fFlags |= (a_fPresentFlag); \
    } while (0)

    /*
     * Process all the extensions.
     */
    for (uint32_t i = 0; i < pThis->T3.Extensions.cItems; i++)
    {
        PCRTASN1OBJID       pExtnId   = &pThis->T3.Extensions.papItems[i]->ExtnId;
        PCRTASN1OCTETSTRING pExtValue = &pThis->T3.Extensions.papItems[i]->ExtnValue;
        if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_KEY_USAGE_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_KEY_USAGE);
            rtCrx509TbsCertificate_AddKeyUsageFlags(pThis, pThis->T3.Extensions.papItems[i]);
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_BIT_STRING);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_EXT_KEY_USAGE_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_EXT_KEY_USAGE);
            rtCrx509TbsCertificate_AddExtKeyUsageFlags(pThis, pThis->T3.Extensions.papItems[i]);
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_SEQ_OF_OBJ_IDS);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_AUTHORITY_KEY_IDENTIFIER_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_AUTHORITY_KEY_IDENTIFIER);
            pThis->T3.pAuthorityKeyIdentifier = (PCRTCRX509AUTHORITYKEYIDENTIFIER)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_AUTHORITY_KEY_IDENTIFIER);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_OLD_AUTHORITY_KEY_IDENTIFIER_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_OLD_AUTHORITY_KEY_IDENTIFIER);
            pThis->T3.pOldAuthorityKeyIdentifier = (PCRTCRX509OLDAUTHORITYKEYIDENTIFIER)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_OLD_AUTHORITY_KEY_IDENTIFIER);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_SUBJECT_KEY_IDENTIFIER_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_SUBJECT_KEY_IDENTIFIER);
            pThis->T3.pSubjectKeyIdentifier = (PCRTASN1OCTETSTRING)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_OCTET_STRING);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_SUBJECT_ALT_NAME_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_SUBJECT_ALT_NAME);
            pThis->T3.pAltSubjectName = (PCRTCRX509GENERALNAMES)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_GENERAL_NAMES);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_ISSUER_ALT_NAME_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_ISSUER_ALT_NAME);
            pThis->T3.pAltIssuerName = (PCRTCRX509GENERALNAMES)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_GENERAL_NAMES);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_CERTIFICATE_POLICIES_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_CERTIFICATE_POLICIES);
            pThis->T3.pCertificatePolicies = (PCRTCRX509CERTIFICATEPOLICIES)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_CERTIFICATE_POLICIES);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_POLICY_MAPPINGS_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_POLICY_MAPPINGS);
            pThis->T3.pPolicyMappings = (PCRTCRX509POLICYMAPPINGS)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_POLICY_MAPPINGS);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_BASIC_CONSTRAINTS_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_BASIC_CONSTRAINTS);
            pThis->T3.pBasicConstraints = (PCRTCRX509BASICCONSTRAINTS)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_BASIC_CONSTRAINTS);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_NAME_CONSTRAINTS_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_NAME_CONSTRAINTS);
            pThis->T3.pNameConstraints = (PCRTCRX509NAMECONSTRAINTS)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_NAME_CONSTRAINTS);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_POLICY_CONSTRAINTS_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_POLICY_CONSTRAINTS);
            pThis->T3.pPolicyConstraints = (PCRTCRX509POLICYCONSTRAINTS)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_POLICY_CONSTRAINTS);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_INHIBIT_ANY_POLICY_OID) == 0)
        {
            CHECK_SET_PRESENT_RET_ON_DUP(pThis, pErrInfo, RTCRX509TBSCERTIFICATE_F_PRESENT_INHIBIT_ANY_POLICY);
            pThis->T3.pInhibitAnyPolicy = (PCRTASN1INTEGER)pExtValue->pEncapsulated;
            Assert(pThis->T3.Extensions.papItems[i]->enmValue == RTCRX509EXTENSIONVALUE_INTEGER);
        }
        else if (RTAsn1ObjId_CompareWithString(pExtnId, RTCRX509_ID_CE_ACCEPTABLE_CERT_POLICIES_OID) == 0)
            pThis->T3.fFlags |= RTCRX509TBSCERTIFICATE_F_PRESENT_ACCEPTABLE_CERT_POLICIES;
        else
            pThis->T3.fFlags |= RTCRX509TBSCERTIFICATE_F_PRESENT_OTHER;
    }

    if (!pThis->T3.fFlags)
        pThis->T3.fFlags |= RTCRX509TBSCERTIFICATE_F_PRESENT_NONE;

#undef CHECK_SET_PRESENT_RET_ON_DUP
    return VINF_SUCCESS;
}



/*
 * One X.509 Certificate.
 */

RTDECL(bool) RTCrX509Certificate_MatchIssuerAndSerialNumber(PCRTCRX509CERTIFICATE pCertificate,
                                                            PCRTCRX509NAME pIssuer, PCRTASN1INTEGER pSerialNumber)
{
    if (   RTAsn1Integer_UnsignedCompare(&pCertificate->TbsCertificate.SerialNumber, pSerialNumber) == 0
        && RTCrX509Name_Compare(&pCertificate->TbsCertificate.Issuer, pIssuer) == 0)
        return true;
    return false;
}


RTDECL(bool) RTCrX509Certificate_MatchSubjectOrAltSubjectByRfc5280(PCRTCRX509CERTIFICATE pThis, PCRTCRX509NAME pName)
{
    if (RTCrX509Name_MatchByRfc5280(&pThis->TbsCertificate.Subject, pName))
        return true;

    if (RTCrX509Extensions_IsPresent(&pThis->TbsCertificate.T3.Extensions))
        for (uint32_t i = 0; i < pThis->TbsCertificate.T3.Extensions.cItems; i++)
        {
            PCRTCRX509EXTENSION pExt = pThis->TbsCertificate.T3.Extensions.papItems[i];
            if (   pExt->enmValue == RTCRX509EXTENSIONVALUE_GENERAL_NAMES
                && RTAsn1ObjId_CompareWithString(&pExt->ExtnId, RTCRX509_ID_CE_SUBJECT_ALT_NAME_OID))
            {
                PCRTCRX509GENERALNAMES pGeneralNames = (PCRTCRX509GENERALNAMES)pExt->ExtnValue.pEncapsulated;
                for (uint32_t j = 0; j < pGeneralNames->cItems; j++)
                    if (   RTCRX509GENERALNAME_IS_DIRECTORY_NAME(pGeneralNames->papItems[j])
                        && RTCrX509Name_MatchByRfc5280(&pGeneralNames->papItems[j]->u.pT4->DirectoryName, pName))
                        return true;
            }
        }
    return false;
}


RTDECL(bool) RTCrX509Certificate_IsSelfSigned(PCRTCRX509CERTIFICATE pCertificate)
{
    if (RTCrX509Certificate_IsPresent(pCertificate))
    {
        return RTCrX509Name_MatchByRfc5280(&pCertificate->TbsCertificate.Subject,
                                           &pCertificate->TbsCertificate.Issuer);
    }
    return false;
}


/*
 * Set of X.509 Certificates.
 */

RTDECL(PCRTCRX509CERTIFICATE)
RTCrX509Certificates_FindByIssuerAndSerialNumber(PCRTCRX509CERTIFICATES pCertificates,
                                                 PCRTCRX509NAME pIssuer, PCRTASN1INTEGER pSerialNumber)
{
    for (uint32_t i = 0; i < pCertificates->cItems; i++)
        if (RTCrX509Certificate_MatchIssuerAndSerialNumber(pCertificates->papItems[i], pIssuer, pSerialNumber))
            return pCertificates->papItems[i];
    return NULL;
}

