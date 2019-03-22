/* $Id$ */
/** @file
 * IPRT testcase - Crypto - X.509 \#1.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
#include <iprt/crypto/x509.h>

#include <iprt/string.h>
#include <iprt/test.h>

#include "tstRTCrX509-1.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;

/** List of test certificates + keys, PEM encoding, and their corresponding
 * .der certificate encodings. */
static const struct
{
    const char     *pszFile;
    bool            fMaybeNotInOpenSSL;
    bool            fSelfSigned;

    char const     *pchPem;
    size_t          cbPem;

    uint8_t const  *pbDer;
    size_t          cbDer;
} g_aFiles[] =
{
#define MY_CERT_ENTRY(a_fMaybeNotInOpenSSL, a_fSelfSigned, a_Name) \
    { #a_Name, a_fMaybeNotInOpenSSL, a_fSelfSigned, \
      (const char *)RT_CONCAT(g_abPem_, a_Name), RT_CONCAT(g_cbPem_, a_Name), \
                    RT_CONCAT(g_abDer_, a_Name), RT_CONCAT(g_cbDer_, a_Name) }
    MY_CERT_ENTRY(true,   true, md4),
    MY_CERT_ENTRY(false,  true, md5),
    MY_CERT_ENTRY(false,  true, sha1),
    MY_CERT_ENTRY(false,  true, sha224),
    MY_CERT_ENTRY(false,  true, sha256),
    MY_CERT_ENTRY(false,  true, sha384),
    MY_CERT_ENTRY(false,  true, sha512),
    MY_CERT_ENTRY(false, false, cert1),
};


static void test1()
{
    RTTestSub(g_hTest, "Basics");
    int rc;
    for (unsigned i = 0; i < RT_ELEMENTS(g_aFiles); i++)
    {
        /*
         * Decode.
         */
        /* Raw decoding of DER bytes, structure will have pointers to the raw data. */
        RTCRX509CERTIFICATE Cert0;
        RTASN1CURSORPRIMARY PrimaryCursor;
        RTAsn1CursorInitPrimary(&PrimaryCursor, g_aFiles[i].pbDer, (uint32_t)g_aFiles[i].cbDer,
                                NULL /*pErrInfo*/, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, NULL /*pszErrorTag*/);
        rc = RTCrX509Certificate_DecodeAsn1(&PrimaryCursor.Cursor, 0, &Cert0, "Cert0");
        if (RT_SUCCESS(rc))
        {
            rc = RTCrX509Certificate_CheckSanity(&Cert0, 0, NULL /*pErrInfo*/, "Cert0");
            if (RT_SUCCESS(rc))
            {
                /* Check the API, this clones the certificate so no data pointers. */
                RTCRX509CERTIFICATE Cert1;
                memset(&Cert1, i, sizeof(Cert1));
                rc = RTCrX509Certificate_ReadFromBuffer(&Cert1, g_aFiles[i].pbDer, g_aFiles[i].cbDer, 0 /*fFlags*/,
                                                        &g_RTAsn1EFenceAllocator, NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
                if (RT_SUCCESS(rc))
                {
                    /* Read the PEM variant. */
                    RTCRX509CERTIFICATE Cert2;
                    memset(&Cert2, ~i, sizeof(Cert2));
                    rc = RTCrX509Certificate_ReadFromBuffer(&Cert2, g_aFiles[i].pchPem, g_aFiles[i].cbPem, 0 /*fFlags*/,
                                                            &g_RTAsn1DefaultAllocator, NULL /*pErrInfo*/, NULL /*pszErrorTag*/);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Compare them, they should be all the same.
                         */
                        if (RTCrX509Certificate_Compare(&Cert0, &Cert1) != 0)
                            RTTestIFailed("Cert0 and Cert1 (DER) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else if (RTCrX509Certificate_Compare(&Cert0, &Cert2) != 0)
                            RTTestIFailed("Cert0 and Cert2 (PEM) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else if (RTCrX509Certificate_Compare(&Cert1, &Cert2) != 0)
                            RTTestIFailed("Cert1 (DER) and Cert2 (PEM) decoding of file %s (#%u) differs", g_aFiles[i].pszFile, i);
                        else
                        {
                            /*
                             * Encode the certificates.
                             */
                            unsigned j;
                            PRTCRX509CERTIFICATE paCerts[] = { &Cert0, &Cert1, &Cert2 };
                            for (j = 0; j < RT_ELEMENTS(paCerts); j++)
                            {
                                uint32_t cbEncoded = ~(j ^ i);
                                RTTESTI_CHECK_RC(rc = RTAsn1EncodePrepare(&paCerts[j]->SeqCore.Asn1Core,
                                                                          RTASN1ENCODE_F_DER, &cbEncoded, NULL), VINF_SUCCESS);
                                if (RT_SUCCESS(rc) && cbEncoded != g_aFiles[i].cbDer)
                                    RTTestIFailed("RTAsn1EncodePrepare of file %s (#%u) returned %#x bytes instead of %#x",
                                                  g_aFiles[i].pszFile, i, cbEncoded, g_aFiles[i].cbDer);

                                cbEncoded = (uint32_t)g_aFiles[i].cbDer;
                                void *pvTmp = RTTestGuardedAllocTail(g_hTest, cbEncoded);
                                RTTESTI_CHECK_RC(rc = RTAsn1EncodeToBuffer(&paCerts[j]->SeqCore.Asn1Core, RTASN1ENCODE_F_DER,
                                                                           pvTmp, cbEncoded, NULL /*pErrInfo*/), VINF_SUCCESS);
                                if (RT_SUCCESS(rc) && memcmp(pvTmp, g_aFiles[i].pbDer, cbEncoded) != 0)
                                    RTTestIFailed("RTAsn1EncodeToBuffer produces the wrong output for file %s (#%u), variation %u",
                                                  g_aFiles[i].pszFile, i, j);
                                RTTestGuardedFree(g_hTest, pvTmp);
                            }

                            /*
                             * Check that our self signed check works.
                             */
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert0) == g_aFiles[i].fSelfSigned);
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert1) == g_aFiles[i].fSelfSigned);
                            RTTESTI_CHECK(RTCrX509Certificate_IsSelfSigned(&Cert2) == g_aFiles[i].fSelfSigned);

                            if (g_aFiles[i].fSelfSigned)
                            {
                                /*
                                 * Verify the certificate signature (self signed).
                                 */
                                for (j = 0; j < RT_ELEMENTS(paCerts); j++)
                                {
                                    rc = RTCrX509Certificate_VerifySignatureSelfSigned(paCerts[j], NULL /*pErrInfo*/);
                                    if (   RT_FAILURE(rc)
                                        && (   rc != VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN_EVP
                                            || !g_aFiles[i].fMaybeNotInOpenSSL) )
                                        RTTestIFailed("RTCrX509Certificate_VerifySignatureSelfSigned failed for %s (#%u), variation %u: %Rrc",
                                                      g_aFiles[i].pszFile, i, j, rc);
                                }
                            }
                        }

                        RTCrX509Certificate_Delete(&Cert2);
                    }
                    else
                        RTTestIFailed("Error %Rrc decoding PEM file %s (#%u)", rc, g_aFiles[i].pszFile, i);
                    RTCrX509Certificate_Delete(&Cert1);
                }
                else
                    RTTestIFailed("Error %Rrc decoding DER file %s (#%u)", rc, g_aFiles[i].pszFile, i);
            }
            RTCrX509Certificate_Delete(&Cert0);
        }
    }
}




int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTCrX509-1", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    test1();


    return RTTestSummaryAndDestroy(g_hTest);
}

