/* $Id$ */
/** @file
 * IPRT - Crypto - X.509, Sanity Checkers.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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

#include "x509-internal.h"



static int rtCrX509Validity_CheckSanityExtra(PCRTCRX509VALIDITY pThis, uint32_t fFlags, PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);

    if (RTAsn1Time_Compare(&pThis->NotBefore, &pThis->NotAfter) > 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_VALIDITY_SWAPPED, "%s: NotBefore is after NotAfter", pszErrorTag);
    /** @todo check tag constraints? */
    return VINF_SUCCESS;
}


static int rtCrX509Name_CheckSanityExtra(PCRTCRX509NAME pThis, uint32_t fFlags, PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);

    if (pThis->cItems == 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_NAME_EMPTY_SET, "%s: Has no components.", pszErrorTag);

    for (uint32_t i = 0; i < pThis->cItems; i++)
    {
        PCRTCRX509RELATIVEDISTINGUISHEDNAME const pRdn = pThis->papItems[i];
        if (pRdn->cItems == 0)
            return RTErrInfoSetF(pErrInfo, VERR_CR_X509_NAME_EMPTY_SUB_SET,
                                 "%s: Items[%u] has no sub components.", pszErrorTag, i);

        for (uint32_t j = 0; j < pRdn->cItems; j++)
        {
            PCRTCRX509ATTRIBUTETYPEANDVALUE const pAttr = pRdn->papItems[j];

            if (pAttr->Value.enmType != RTASN1TYPE_STRING)
                return RTErrInfoSetF(pErrInfo, VERR_CR_X509_NAME_NOT_STRING,
                                     "%s: Items[%u].paItems[%u].enmType is %d instead of string (%d).",
                                     pszErrorTag, i, j, pAttr->Value.enmType, RTASN1TYPE_STRING);
            if (pAttr->Value.u.String.Asn1Core.cb == 0)
                return RTErrInfoSetF(pErrInfo, VERR_CR_X509_NAME_EMPTY_STRING,
                                     "%s: Items[%u].paItems[%u] is an empty string", pszErrorTag, i, j);
            switch (pAttr->Value.u.String.Asn1Core.uTag)
            {
                case ASN1_TAG_PRINTABLE_STRING:
                case ASN1_TAG_UTF8_STRING:
                    break;
                case ASN1_TAG_T61_STRING:
                case ASN1_TAG_UNIVERSAL_STRING:
                case ASN1_TAG_BMP_STRING:
                    break;
                case ASN1_TAG_IA5_STRING: /* Used by "Microsoft Root Certificate Authority" in the "com" part of the Issuer. */
                    break;
                default:
                    return RTErrInfoSetF(pErrInfo, VERR_CR_X509_INVALID_NAME_STRING_TAG,
                                         "%s: Items[%u].paItems[%u] invalid string type: %u",  pszErrorTag, i, j,
                                         pAttr->Value.u.String.Asn1Core.uTag);
            }
        }
    }

    return VINF_SUCCESS;
}


static int rtCrX509SubjectPublicKeyInfo_CheckSanityExtra(PCRTCRX509SUBJECTPUBLICKEYINFO pThis, uint32_t fFlags,
                                                         PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);
    if (pThis->SubjectPublicKey.cBits <= 32)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_PUBLIC_KEY_TOO_SMALL,
                             "%s: SubjectPublicKey is too small, only %u bits", pszErrorTag, pThis->SubjectPublicKey.cBits);
    return VINF_SUCCESS;
}


static int rtCrX509TbsCertificate_CheckSanityExtra(PCRTCRX509TBSCERTIFICATE pThis, uint32_t fFlags,
                                                   PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);

    if (   RTAsn1Integer_IsPresent(&pThis->T0.Version)
        && RTAsn1Integer_UnsignedCompareWithU32(&pThis->T0.Version, RTCRX509TBSCERTIFICATE_V1) != 0
        && RTAsn1Integer_UnsignedCompareWithU32(&pThis->T0.Version, RTCRX509TBSCERTIFICATE_V2) != 0
        && RTAsn1Integer_UnsignedCompareWithU32(&pThis->T0.Version, RTCRX509TBSCERTIFICATE_V3) != 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_TBSCERT_UNSUPPORTED_VERSION,
                             "%s: Unknown Version number: %llu",
                             pszErrorTag, pThis->T0.Version.uValue.u);

    if (   pThis->SerialNumber.Asn1Core.cb < 1
        || pThis->SerialNumber.Asn1Core.cb > 1024)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_TBSCERT_SERIAL_NUMBER_OUT_OF_BOUNDS,
                             "%s: Bad SerialNumber length: %u", pszErrorTag, pThis->SerialNumber.Asn1Core.cb);

    if (  (   RTAsn1BitString_IsPresent(&pThis->T1.IssuerUniqueId)
           || RTAsn1BitString_IsPresent(&pThis->T2.SubjectUniqueId))
        && RTAsn1Integer_UnsignedCompareWithU32(&pThis->T0.Version, RTCRX509TBSCERTIFICATE_V2) < 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_TBSCERT_UNIQUE_IDS_REQ_V2,
                             "%s: IssuerUniqueId and SubjectUniqueId requires version 2", pszErrorTag);

    if (   RTCrX509Extensions_IsPresent(&pThis->T3.Extensions)
        && RTAsn1Integer_UnsignedCompareWithU32(&pThis->T0.Version, RTCRX509TBSCERTIFICATE_V3) < 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_TBSCERT_EXTS_REQ_V3, "%s: Extensions requires version 3", pszErrorTag);

    return VINF_SUCCESS;
}


static int rtCrX509Certificate_CheckSanityExtra(PCRTCRX509CERTIFICATE pThis, uint32_t fFlags,
                                               PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    RT_NOREF_PV(fFlags);

    if (RTCrX509AlgorithmIdentifier_Compare(&pThis->SignatureAlgorithm, &pThis->TbsCertificate.Signature) != 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_X509_CERT_TBS_SIGN_ALGO_MISMATCH,
                             "%s: SignatureAlgorithm (%s) does not match TbsCertificate.Signature (%s).", pszErrorTag,
                             pThis->SignatureAlgorithm.Algorithm.szObjId,
                             pThis->TbsCertificate.Signature.Algorithm.szObjId);
    return VINF_SUCCESS;
}


/*
 * Generate the code.
 */
#include <iprt/asn1-generator-sanity.h>

