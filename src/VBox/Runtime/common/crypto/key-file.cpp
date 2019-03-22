/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Keys, File I/O.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
#include <iprt/crypto/key.h>

#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/memsafer.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/crypto/rsa.h>
#include <iprt/crypto/pkix.h>
#include <iprt/crypto/x509.h>

#include "internal/magics.h"
#include "key-internal.h"

#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt-openssl.h"
# include "openssl/evp.h"
# ifndef OPENSSL_VERSION_NUMBER
#  error "Missing OPENSSL_VERSION_NUMBER!"
# endif
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
/** RSA public key marker words. */
static RTCRPEMMARKERWORD const g_aWords_RsaPublicKey[]  =
{ { RT_STR_TUPLE("RSA") }, { RT_STR_TUPLE("PUBLIC") }, { RT_STR_TUPLE("KEY") } };
/** Generic public key marker words. */
static RTCRPEMMARKERWORD const g_aWords_PublicKey[] =
{                          { RT_STR_TUPLE("PUBLIC") }, { RT_STR_TUPLE("KEY") } };

/** Public key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyPublicMarkers[] =
{
    { g_aWords_RsaPublicKey, RT_ELEMENTS(g_aWords_RsaPublicKey) },
    { g_aWords_PublicKey,    RT_ELEMENTS(g_aWords_PublicKey) },
};
/** Number of entries in g_aRTCrKeyPublicMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyPublicMarkers = RT_ELEMENTS(g_aRTCrKeyPublicMarkers);


/** RSA private key marker words. */
static RTCRPEMMARKERWORD const g_aWords_RsaPrivateKey[] =
{ { RT_STR_TUPLE("RSA") },       { RT_STR_TUPLE("PRIVATE") }, { RT_STR_TUPLE("KEY") } };
/** Generic encrypted private key marker words. */
static RTCRPEMMARKERWORD const g_aWords_EncryptedPrivateKey[] =
{ { RT_STR_TUPLE("ENCRYPTED") }, { RT_STR_TUPLE("PRIVATE") }, { RT_STR_TUPLE("KEY") } };
/** Generic private key marker words. */
static RTCRPEMMARKERWORD const g_aWords_PrivateKey[] =
{                                { RT_STR_TUPLE("PRIVATE") }, { RT_STR_TUPLE("KEY") } };

/** Private key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyPrivateMarkers[] =
{
    { g_aWords_RsaPrivateKey,       RT_ELEMENTS(g_aWords_RsaPrivateKey) },
    { g_aWords_EncryptedPrivateKey, RT_ELEMENTS(g_aWords_EncryptedPrivateKey) },
    { g_aWords_PrivateKey,          RT_ELEMENTS(g_aWords_PrivateKey) },
};
/** Number of entries in g_aRTCrKeyPrivateMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyPrivateMarkers = RT_ELEMENTS(g_aRTCrKeyPrivateMarkers);


/** Private and public key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyAllMarkers[] =
{
    { g_aWords_RsaPublicKey,  RT_ELEMENTS(g_aWords_RsaPublicKey) },
    { g_aWords_PublicKey,     RT_ELEMENTS(g_aWords_PublicKey) },
    { g_aWords_RsaPrivateKey, RT_ELEMENTS(g_aWords_RsaPrivateKey) },
    { g_aWords_PrivateKey,    RT_ELEMENTS(g_aWords_PrivateKey) },
};
/** Number of entries in g_aRTCrKeyAllMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyAllMarkers = RT_ELEMENTS(g_aRTCrKeyAllMarkers);


/**
 * Decrypts a PEM message.
 *
 * @returns IPRT status code
 * @param   pszDekInfo          The decryption info.  See RFC-1421 section 4.6.1.3
 *                              as well as RFC-1423).
 * @param   pszPassword         The password to use to decrypt the key text.
 * @param   pbEncrypted         The encrypted key text.
 * @param   cbEncrypted         The size of the encrypted text.
 * @param   ppbDecrypted        Where to return the decrypted message. Free using RTMemSaferFree.
 * @param   pcbDecrypted        Where to return the length of the decrypted message.
 * @param   pcbDecryptedAlloced Where to return the allocation size.
 * @param   pErrInfo            Where to return additional error information.
 */
static int rtCrKeyDecryptPemMessage(const char *pszDekInfo, const char *pszPassword, uint8_t *pbEncrypted, size_t cbEncrypted,
                                    uint8_t **ppbDecrypted, size_t *pcbDecrypted, size_t *pcbDecryptedAlloced, PRTERRINFO pErrInfo)
{
    /*
     * Initialize return values.
     */
    *ppbDecrypted        = NULL;
    *pcbDecrypted        = 0;
    *pcbDecryptedAlloced = 0;

    /*
     * Parse the DEK-Info.
     */
    if (!pszDekInfo)
        return VERR_CR_KEY_NO_DEK_INFO;

    /* Find the end of the algorithm */
    const char *pszParams = strchr(pszDekInfo, ',');
    if (!pszParams)
        pszParams = strchr(pszDekInfo, '\0');
    size_t cchAlgo = pszParams - pszDekInfo;
    while (cchAlgo > 0 && RT_C_IS_SPACE(pszDekInfo[cchAlgo - 1]))
        cchAlgo--;

    /* Copy it out and zero terminating it. */
    char szAlgo[256];
    if (cchAlgo >= sizeof(szAlgo))
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_DEK_INFO_TOO_LONG, "Algorithms list is too long (%s)", pszDekInfo);
    memcpy(szAlgo, pszDekInfo, cchAlgo);
    szAlgo[cchAlgo] = '\0';

    /* Parameters. */
    pszParams = RTStrStripL(*pszParams == ',' ? pszParams + 1 : pszParams);
    size_t const cchParams = strlen(pszParams);

    /*
     * Do we support the cihper?
     */
#ifdef IPRT_WITH_OPENSSL /** @todo abstract encryption & decryption. */
    const EVP_CIPHER *pCipher = EVP_get_cipherbyname(szAlgo);
    if (!pCipher)
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_UNSUPPORTED_CIPHER, "Unknown key cipher: %s (params: %s)", szAlgo, pszParams);

    /* Decode the initialization vector if one is required. */
    uint8_t *pbInitVector = NULL;
    int const cbInitVector = EVP_CIPHER_iv_length(pCipher);
    if (cbInitVector > 0)
    {
        if (*pszParams == '\0')
            return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_MISSING_CIPHER_PARAMS,
                                 "Cipher '%s' expected %u bytes initialization vector, none found", cbInitVector, szAlgo);
        if ((size_t)cbInitVector > cchParams / 2)
            return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_TOO_SHORT_CIPHER_IV,
                                 "Too short initialization vector for '%s', expected %u chars found only %u: %s",
                                 szAlgo, cbInitVector * 2, cchParams, pszParams);
        pbInitVector = (uint8_t *)alloca(cbInitVector);
        int rc = RTStrConvertHexBytes(pszParams, pbInitVector, cbInitVector, 0 /*fFlags*/);
        if (   RT_FAILURE(rc)
            && rc != VERR_BUFFER_OVERFLOW /* openssl ignores this condition */)
            return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_MALFORMED_CIPHER_IV,
                                 "Malformed initialization vector for '%s': %s (rc=%Rrc)", szAlgo, pszParams, rc);
    }
    else if (*pszParams != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_UNEXPECTED_CIPHER_PARAMS,
                             "Cipher '%s' expected no parameters, found: %s", szAlgo, pszParams);

    /*
     * Do we have a password?  If so try decrypt the key.
     */
    if (!pszPassword)
        return VERR_CR_KEY_ENCRYPTED;

    unsigned char abKey[EVP_MAX_KEY_LENGTH * 2];
    int cbKey = EVP_BytesToKey(pCipher, EVP_md5(), pbInitVector, (unsigned char const *)pszPassword, (int)strlen(pszPassword),
                               1, abKey, NULL);
    if (!cbKey)
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_PASSWORD_ENCODING, "EVP_BytesToKey failed to encode password");

    EVP_CIPHER_CTX *pCipherCtx = EVP_CIPHER_CTX_new();
    if (!pCipherCtx)
        return VERR_NO_MEMORY;

    int rc;
    if (EVP_DecryptInit_ex(pCipherCtx, pCipher, NULL /*pEngine*/, abKey, pbInitVector))
    {
        size_t   cbDecryptedAlloced = cbEncrypted;
        int      cbDecrypted = (int)cbDecryptedAlloced;
        uint8_t *pbDecrypted = (uint8_t *)RTMemSaferAllocZ(cbDecryptedAlloced);
        if (pbDecrypted)
        {
            if (EVP_DecryptUpdate(pCipherCtx, pbDecrypted, &cbDecrypted, pbEncrypted, (int)cbEncrypted))
            {
                int cbFinal = (int)cbDecryptedAlloced - cbDecrypted;
                if (EVP_DecryptFinal_ex(pCipherCtx, &pbDecrypted[cbDecrypted], &cbFinal))
                {
                    cbDecrypted += cbFinal;
                    Assert((size_t)cbDecrypted <= cbDecryptedAlloced);

                    /*
                     * Done! Just set the return values.
                     */
                    *pcbDecrypted        = cbDecrypted;
                    *pcbDecryptedAlloced = cbDecryptedAlloced;
                    *ppbDecrypted        = pbDecrypted;
                    pbDecrypted = NULL;
                    rc = VINF_CR_KEY_WAS_DECRYPTED;
                }
                else
                    rc = RTErrInfoSetF(pErrInfo, VERR_CR_KEY_DECRYPTION_FAILED,
                                       "Incorrect password? EVP_DecryptFinal_ex failed for %s", pszDekInfo);
            }
            else
                rc = RTErrInfoSetF(pErrInfo, VERR_CR_KEY_DECRYPTION_FAILED,
                                   "Incorrect password? EVP_DecryptUpdate failed for %s", pszDekInfo);
            if (pbDecrypted)
                RTMemSaferFree(pbDecrypted, cbDecryptedAlloced);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_CR_KEY_OSSL_DECRYPT_INIT_ERROR, "EVP_DecryptInit_ex failed for %s", pszDekInfo);
    EVP_CIPHER_CTX_free(pCipherCtx);
    return rc;
#else
    RT_NOREF(pbEncrypted, cbEncrypted, pszPassword, pErrInfo, cchParams);
    return VERR_CR_KEY_DECRYPTION_NOT_SUPPORTED;
#endif
}


RTDECL(int) RTCrKeyCreateFromPemSection(PRTCRKEY phKey, PCRTCRPEMSECTION pSection, uint32_t fFlags, const char *pszPassword,
                                        PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(!(fFlags & (~RTCRKEYFROM_F_VALID_MASK | RTCRKEYFROM_F_ONLY_PEM)), VERR_INVALID_FLAGS);

    AssertPtrReturn(phKey, VERR_INVALID_POINTER);
    *phKey = NIL_RTCRKEY;
    AssertPtrReturn(pSection, VERR_INVALID_POINTER);
    NOREF(pszPassword);

    /*
     * If the source is PEM section, try identify the format from the markers.
     */
    enum
    {
        kKeyFormat_Unknown = 0,
        kKeyFormat_RsaPrivateKey,
        kKeyFormat_RsaEncryptedPrivateKey,
        kKeyFormat_RsaPublicKey,
        kKeyFormat_SubjectPublicKeyInfo,
        kKeyFormat_PrivateKeyInfo,
        kKeyFormat_EncryptedPrivateKeyInfo
    }               enmFormat     = kKeyFormat_Unknown;
    const char     *pszDekInfo    = NULL;
    PCRTCRPEMMARKER pMarker       = pSection->pMarker;
    if (pMarker)
    {
        if (   pMarker->cWords == 3
            && strcmp(pMarker->paWords[0].pszWord, "RSA") == 0
            && strcmp(pMarker->paWords[2].pszWord, "KEY") == 0)
        {
            if (strcmp(pMarker->paWords[1].pszWord, "PUBLIC") == 0)
                enmFormat  = kKeyFormat_RsaPublicKey;
            else if (strcmp(pMarker->paWords[1].pszWord, "PRIVATE") == 0)
            {
                enmFormat = kKeyFormat_RsaPrivateKey;

                /* RSA PRIVATE KEY encryption is advertised thru PEM header fields.
                   We need the DEK field to decrypt the message (see RFC-1421 4.6.1.3). */
                for (PCRTCRPEMFIELD pField = pSection->pFieldHead; pField; pField = pField->pNext)
                {
                    if (   pField->cchName == sizeof("Proc-Type") - 1
                        && pField->cchValue >= sizeof("4,ENCRYPTED") - 1
                        && memcmp(pField->szName, RT_STR_TUPLE("Proc-Type")) == 0)
                    {
                        const char *pszValue = pField->pszValue;
                        if (*pszValue == '4')
                        {
                            do
                                pszValue++;
                            while (RT_C_IS_SPACE(*pszValue) || RT_C_IS_PUNCT(*pszValue));
                            if (strcmp(pszValue, "ENCRYPTED") == 0)
                                enmFormat  = kKeyFormat_RsaEncryptedPrivateKey;
                        }
                    }
                    else if (   pField->cchName == sizeof("DEK-Info") - 1
                             && pField->cchValue > 0
                             && !pszDekInfo)
                        pszDekInfo = pField->pszValue;
                }
            }
            else
                AssertFailed();
        }
        else if (   pMarker->cWords == 2
                 && strcmp(pMarker->paWords[1].pszWord, "KEY") == 0)
        {
            if (strcmp(pMarker->paWords[0].pszWord, "PUBLIC") == 0)
                enmFormat = kKeyFormat_SubjectPublicKeyInfo;
            else if (strcmp(pMarker->paWords[0].pszWord, "PRIVATE") == 0)
                enmFormat = kKeyFormat_PrivateKeyInfo;
            else
                AssertFailed();
        }
        else if (   pMarker->cWords == 3
                 && strcmp(pMarker->paWords[0].pszWord, "ENCRYPTED") == 0
                 && strcmp(pMarker->paWords[1].pszWord, "PRIVATE") == 0
                 && strcmp(pMarker->paWords[2].pszWord, "KEY") == 0)
            enmFormat = kKeyFormat_EncryptedPrivateKeyInfo;
        else
            AssertFailed();
    }

    /*
     * Try guess the format from the binary data if needed.
     */
    RTASN1CURSORPRIMARY PrimaryCursor;
    if (   enmFormat == kKeyFormat_Unknown
        && pSection->cbData > 10)
    {
        RTAsn1CursorInitPrimary(&PrimaryCursor, pSection->pbData, (uint32_t)pSection->cbData,
                                pErrInfo, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, "probing/0");

        /*
         * First the must be a sequence.
         */
        RTASN1CORE Tag;
        int rc = RTAsn1CursorReadHdr(&PrimaryCursor.Cursor, &Tag, "#1");
        if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_SEQUENCE)
        {
            RTASN1CURSOR Cursor2;
            RTAsn1CursorInitSubFromCore(&PrimaryCursor.Cursor, &Tag, &Cursor2, "probing/1");
            rc = RTAsn1CursorReadHdr(&Cursor2, &Tag, "#2");

            /*
             * SEQUENCE SubjectPublicKeyInfo.Algorithm?
             */
            if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_SEQUENCE)
            {
                RTASN1CURSOR Cursor3;
                RTAsn1CursorInitSubFromCore(&Cursor2, &Tag, &Cursor3, "probing/2");
                rc = RTAsn1CursorReadHdr(&Cursor3, &Tag, "#3");

                /* SEQUENCE SubjectPublicKeyInfo.Algorithm.Algorithm? */
                if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_OID)
                    enmFormat = kKeyFormat_SubjectPublicKeyInfo;
            }
            /*
             * INTEGER PrivateKeyInfo.Version?
             * INTEGER RsaPublicKey.Modulus?
             * INTEGER RsaPrivateKey.Version?
             */
            else if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
            {
                rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#4");

                /* OBJECT PrivateKeyInfo.privateKeyAlgorithm? */
                if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_OID)
                    enmFormat = kKeyFormat_PrivateKeyInfo;
                /* INTEGER RsaPublicKey.PublicExponent?
                   INTEGER RsaPrivateKey.Modulus? */
                else if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                {
                    /* RsaPublicKey.PublicExponent is at the end. */
                    if (RTAsn1CursorIsEnd(&Cursor2))
                        enmFormat = kKeyFormat_RsaPublicKey;
                    else
                    {
                        /* Check for INTEGER RsaPrivateKey.PublicExponent nad PrivateExponent before concluding. */
                        rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#5");
                        if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                        {
                            rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#6");
                            if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                                enmFormat = kKeyFormat_RsaPrivateKey;
                        }
                    }
                }
            }
        }
    }

    if (enmFormat == kKeyFormat_Unknown)
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_UNKNOWN_TYPE,
                             "Unable to identify the key format (%.*Rhxs)", RT_MIN(16, pSection->cbData), pSection->pbData);

    /*
     * Do the reading.
     */
    int rc;
    switch (enmFormat)
    {
        case kKeyFormat_RsaPublicKey:
            rc = rtCrKeyCreateRsaPublic(phKey, pSection->pbData, (uint32_t)pSection->cbData, pErrInfo, pszErrorTag);
            break;

        case kKeyFormat_RsaPrivateKey:
            rc = rtCrKeyCreateRsaPrivate(phKey, pSection->pbData, (uint32_t)pSection->cbData, pErrInfo, pszErrorTag);
            break;

        case kKeyFormat_RsaEncryptedPrivateKey:
        {
            uint8_t *pbDecrypted = NULL;
            size_t   cbDecrypted = 0;
            size_t   cbDecryptedAlloced = 0;
            rc = rtCrKeyDecryptPemMessage(pszDekInfo, pszPassword, pSection->pbData, pSection->cbData,
                                          &pbDecrypted, &cbDecrypted, &cbDecryptedAlloced, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                int rc2 = rtCrKeyCreateRsaPrivate(phKey, pbDecrypted, (uint32_t)cbDecrypted, pErrInfo, pszErrorTag);
                if (rc2 != VINF_SUCCESS)
                    rc = rc2;
                RTMemSaferFree(pbDecrypted, cbDecryptedAlloced);
            }
            break;
        }

        case kKeyFormat_SubjectPublicKeyInfo:
        {
            RTAsn1CursorInitPrimary(&PrimaryCursor, pSection->pbData, (uint32_t)pSection->cbData,
                                    pErrInfo, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, pszErrorTag);
            RTCRX509SUBJECTPUBLICKEYINFO SubjectPubKeyInfo;
            RT_ZERO(SubjectPubKeyInfo);
            rc = RTCrX509SubjectPublicKeyInfo_DecodeAsn1(&PrimaryCursor.Cursor, 0, &SubjectPubKeyInfo, "SubjectPubKeyInfo");
            if (RT_SUCCESS(rc))
            {
                rc = RTCrKeyCreateFromSubjectPublicKeyInfo(phKey, &SubjectPubKeyInfo, pErrInfo, pszErrorTag);
                RTCrX509SubjectPublicKeyInfo_Delete(&SubjectPubKeyInfo);
            }
            break;
        }

        case kKeyFormat_PrivateKeyInfo:
            rc = RTErrInfoSet(pErrInfo, VERR_CR_KEY_FORMAT_NOT_SUPPORTED,
                              "Support for PKCS#8 PrivateKeyInfo is not yet implemented");
            break;

        case kKeyFormat_EncryptedPrivateKeyInfo:
            rc = RTErrInfoSet(pErrInfo, VERR_CR_KEY_FORMAT_NOT_SUPPORTED,
                              "Support for encrypted PKCS#8 PrivateKeyInfo is not yet implemented");
            break;

        default:
            AssertFailedStmt(rc = VERR_INTERNAL_ERROR_4);
    }
    return rc;
}


RTDECL(int) RTCrKeyCreateFromBuffer(PRTCRKEY phKey, uint32_t fFlags, void const *pvSrc, size_t cbSrc, const char *pszPassword,
                                    PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(!(fFlags & ~RTCRKEYFROM_F_VALID_MASK), VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemParseContent(pvSrc, cbSrc, fFlags, g_aRTCrKeyAllMarkers, g_cRTCrKeyAllMarkers, &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (pSectionHead)
        {
            rc = RTCrKeyCreateFromPemSection(phKey, pSectionHead, fFlags  & ~RTCRKEYFROM_F_ONLY_PEM, pszPassword,
                                             pErrInfo, pszErrorTag);
            RTCrPemFreeSections(pSectionHead);
        }
        else
            rc = rc != VINF_SUCCESS ? -rc : VERR_INTERNAL_ERROR_2;
    }
    return rc;
}


RTDECL(int) RTCrKeyCreateFromFile(PRTCRKEY phKey, uint32_t fFlags, const char *pszFilename,
                                  const char *pszPassword, PRTERRINFO pErrInfo)
{
    AssertReturn(!(fFlags & ~RTCRKEYFROM_F_VALID_MASK), VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemReadFile(pszFilename, fFlags, g_aRTCrKeyAllMarkers, g_cRTCrKeyAllMarkers, &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (pSectionHead)
        {
            rc = RTCrKeyCreateFromPemSection(phKey, pSectionHead, fFlags & ~RTCRKEYFROM_F_ONLY_PEM, pszPassword,
                                             pErrInfo, RTPathFilename(pszFilename));
            RTCrPemFreeSections(pSectionHead);
        }
        else
            rc = rc != VINF_SUCCESS ? -rc : VERR_INTERNAL_ERROR_2;
    }
    return rc;
}

