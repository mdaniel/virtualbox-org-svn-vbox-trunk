/* $Id$ */
/** @file
 * IPRT - Crypto - PKCS \#7, Core APIs.
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
#include <iprt/crypto/pkcs7.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/crypto/tsp.h>

#include "pkcs7-internal.h"


/*
 * PCKS #7 SignerInfo
 */

RTDECL(PCRTASN1TIME) RTCrPkcs7SignerInfo_GetSigningTime(PCRTCRPKCS7SIGNERINFO pThis, PCRTCRPKCS7SIGNERINFO *ppSignerInfo)
{
    /*
     * Check the immediate level, unless we're continuing a previous search.
     * Note! We ASSUME a single signing time attribute, which simplifies the interface.
     */
    uint32_t                    cAttrsLeft;
    PRTCRPKCS7ATTRIBUTE const  *ppAttr;
    if (!ppSignerInfo || *ppSignerInfo == NULL)
    {
        cAttrsLeft = pThis->AuthenticatedAttributes.cItems;
        ppAttr     = pThis->AuthenticatedAttributes.papItems;
        while (cAttrsLeft-- > 0)
        {
            PCRTCRPKCS7ATTRIBUTE pAttr = *ppAttr;
            if (   pAttr->enmType == RTCRPKCS7ATTRIBUTETYPE_SIGNING_TIME
                && pAttr->uValues.pSigningTime->cItems > 0)
            {
                if (ppSignerInfo)
                    *ppSignerInfo = pThis;
                return pAttr->uValues.pSigningTime->papItems[0];
            }
            ppAttr++;
        }
    }
    else if (*ppSignerInfo == pThis)
        *ppSignerInfo = NULL;

    /*
     * Check counter signatures.
     */
    cAttrsLeft = pThis->UnauthenticatedAttributes.cItems;
    ppAttr     = pThis->UnauthenticatedAttributes.papItems;
    while (cAttrsLeft-- > 0)
    {
        PCRTCRPKCS7ATTRIBUTE pAttr = *ppAttr;
        if (pAttr->enmType == RTCRPKCS7ATTRIBUTETYPE_COUNTER_SIGNATURES)
        {
            uint32_t               cSignatures = pAttr->uValues.pCounterSignatures->cItems;
            PRTCRPKCS7SIGNERINFO  *ppSignature = pAttr->uValues.pCounterSignatures->papItems;

            /* Skip past the previous counter signature. */
            if (ppSignerInfo && *ppSignerInfo != NULL)
                while (cSignatures > 0)
                {
                    cSignatures--;
                    if (*ppSignature == *ppSignerInfo)
                    {
                        *ppSignerInfo = NULL;
                        ppSignature++;
                        break;
                    }
                    ppSignature++;
                }

            /* Search the counter signatures (if any remaining). */
            while (cSignatures-- > 0)
            {
                PCRTCRPKCS7SIGNERINFO       pSignature        = *ppSignature;
                uint32_t                    cCounterAttrsLeft = pSignature->AuthenticatedAttributes.cItems;
                PRTCRPKCS7ATTRIBUTE const  *ppCounterAttr     = pSignature->AuthenticatedAttributes.papItems;
                while (cCounterAttrsLeft-- > 0)
                {
                    PCRTCRPKCS7ATTRIBUTE pCounterAttr = *ppCounterAttr;
                    if (   pCounterAttr->enmType == RTCRPKCS7ATTRIBUTETYPE_SIGNING_TIME
                        && pCounterAttr->uValues.pSigningTime->cItems > 0)
                    {
                        if (ppSignerInfo)
                            *ppSignerInfo = pSignature;
                        return pCounterAttr->uValues.pSigningTime->papItems[0];
                    }
                    ppCounterAttr++;
                }
                ppSignature++;
            }
        }
        ppAttr++;
    }

    /*
     * No signing timestamp found.
     */
    if (ppSignerInfo)
        *ppSignerInfo = NULL;

    return NULL;
}


RTDECL(PCRTASN1TIME) RTCrPkcs7SignerInfo_GetMsTimestamp(PCRTCRPKCS7SIGNERINFO pThis, PCRTCRPKCS7CONTENTINFO *ppContentInfoRet)
{
    /*
     * Assume there is only one, so no need to enumerate anything here.
     */
    uint32_t                    cAttrsLeft  = pThis->UnauthenticatedAttributes.cItems;
    PRTCRPKCS7ATTRIBUTE const  *ppAttr      = pThis->UnauthenticatedAttributes.papItems;
    while (cAttrsLeft-- > 0)
    {
        PCRTCRPKCS7ATTRIBUTE pAttr = *ppAttr;
        if (pAttr->enmType == RTCRPKCS7ATTRIBUTETYPE_MS_TIMESTAMP)
        {
            uint32_t                     cLeft         = pAttr->uValues.pContentInfos->cItems;
            PRTCRPKCS7CONTENTINFO const *ppContentInfo = pAttr->uValues.pContentInfos->papItems;
            while (cLeft-- > 0)
            {
                PCRTCRPKCS7CONTENTINFO pContentInfo = *ppContentInfo;
                if (RTAsn1ObjId_CompareWithString(&pContentInfo->ContentType, RTCRPKCS7SIGNEDDATA_OID) == 0)
                {
                    if (RTAsn1ObjId_CompareWithString(&pContentInfo->u.pSignedData->ContentInfo.ContentType,
                                                      RTCRTSPTSTINFO_OID) == 0)
                    {
                        if (ppContentInfoRet)
                            *ppContentInfoRet = pContentInfo;
                        return &pContentInfo->u.pSignedData->ContentInfo.u.pTstInfo->GenTime;
                    }
                }

                pContentInfo++;
            }
        }
        ppAttr++;
    }

    /*
     * No signature was found.
     */
    if (ppContentInfoRet)
        *ppContentInfoRet = NULL;

    return NULL;
}


/*
 * PCKS #7 ContentInfo.
 */

RTDECL(bool) RTCrPkcs7ContentInfo_IsSignedData(PCRTCRPKCS7CONTENTINFO pThis)
{
    return RTAsn1ObjId_CompareWithString(&pThis->ContentType, RTCRPKCS7SIGNEDDATA_OID) == 0;
}


/*
 * Set of some kind of certificate supported by PKCS #7 or CMS.
 */

RTDECL(PCRTCRX509CERTIFICATE)
RTCrPkcs7SetOfCerts_FindX509ByIssuerAndSerialNumber(PCRTCRPKCS7SETOFCERTS pCertificates,
                                                    PCRTCRX509NAME pIssuer, PCRTASN1INTEGER pSerialNumber)
{
    for (uint32_t i = 0; i < pCertificates->cItems; i++)
    {
        PCRTCRPKCS7CERT pCert = pCertificates->papItems[i];
        if (   pCert->enmChoice == RTCRPKCS7CERTCHOICE_X509
            && RTCrX509Certificate_MatchIssuerAndSerialNumber(pCert->u.pX509Cert, pIssuer, pSerialNumber))
            return pCert->u.pX509Cert;
    }
    return NULL;
}


/*
 * Generate the standard core code.
 */
#include <iprt/asn1-generator-core.h>

