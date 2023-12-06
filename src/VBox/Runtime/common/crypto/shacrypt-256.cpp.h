/* $Id$ */
/** @file
 * IPRT - Crypto - SHA-crypt, code template for SHA-256 core.
 *
 * This is almost identical to shacrypt-512.cpp.h, fixes generally applies to
 * both. Diff the files after updates!
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


RTDECL(int) RTCrShaCrypt256(const char *pszPhrase, const char *pszSalt, uint32_t cRounds, char *pszString, size_t cbString)
{
    uint8_t abHash[RTSHA256_HASH_SIZE];
    int rc = RTCrShaCrypt256Ex(pszPhrase, pszSalt, cRounds, abHash);
    if (RT_SUCCESS(rc))
        rc = RTCrShaCrypt256ToString(abHash, pszSalt, cRounds, pszString, cbString);
    return rc;
}


RTR3DECL(int) RTCrShaCrypt256Ex(const char *pszPhrase, const char *pszSalt, uint32_t cRounds, uint8_t pabHash[RTSHA256_HASH_SIZE])
{
    /*
     * Validate and adjust input.
     */
    AssertPtrReturn(pszPhrase, VERR_INVALID_POINTER);
    size_t const cchPhrase = strlen(pszPhrase);
    AssertReturn(cchPhrase, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszSalt, VERR_INVALID_POINTER);
    size_t cchSalt;
    pszSalt = rtCrShaCryptExtractSaltAndRounds(pszSalt, &cchSalt, &cRounds);
    AssertReturn(pszSalt != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(cchSalt >= RT_SHACRYPT_SALT_MIN_LEN, VERR_BUFFER_UNDERFLOW);
    AssertReturn(cchSalt <= RT_SHACRYPT_SALT_MAX_LEN, VERR_TOO_MUCH_DATA);
    AssertReturn(cRounds >= RT_SHACRYPT_ROUNDS_MIN && cRounds <= RT_SHACRYPT_ROUNDS_MAX, VERR_OUT_OF_RANGE);

    /*
     * Get started...
     */
    RTSHA256CONTEXT CtxA;
    RTSha256Init(&CtxA);                                                        /* Step 1. */
    RTSha256Update(&CtxA, pszPhrase, cchPhrase);                                /* Step 2. */
    RTSha256Update(&CtxA, pszSalt, cchSalt);                                    /* Step 3. */

    RTSHA256CONTEXT CtxB;
    RTSha256Init(&CtxB);                                                        /* Step 4. */
    RTSha256Update(&CtxB, pszPhrase, cchPhrase);                                /* Step 5. */
    RTSha256Update(&CtxB, pszSalt, cchSalt);                                    /* Step 6. */
    RTSha256Update(&CtxB, pszPhrase, cchPhrase);                                /* Step 7. */
    uint8_t abDigest[RTSHA256_HASH_SIZE];
    RTSha256Final(&CtxB, abDigest);                                             /* Step 8. */

    size_t cbLeft = cchPhrase;
    while (cbLeft > RTSHA256_HASH_SIZE)                                         /* Step 9. */
    {
        RTSha256Update(&CtxA, abDigest, sizeof(abDigest));
        cbLeft -= RTSHA256_HASH_SIZE;
    }
    RTSha256Update(&CtxA, abDigest, cbLeft);                                    /* Step 10. */

    size_t iPhraseBit = cchPhrase;
    while (iPhraseBit)                                                          /* Step 11. */
    {
        if ((iPhraseBit & 1) != 0)
            RTSha256Update(&CtxA, abDigest, sizeof(abDigest));                  /* a) */
        else
            RTSha256Update(&CtxA, pszPhrase, cchPhrase);                        /* b) */
        iPhraseBit >>= 1;
    }

    RTSha256Final(&CtxA, abDigest);                                             /* Step 12. */

    RTSha256Init(&CtxB);                                                        /* Step 13. */
    for (size_t i = 0; i < cchPhrase; i++)                                      /* Step 14. */
        RTSha256Update(&CtxB, pszPhrase, cchPhrase);

    uint8_t abDigestTemp[RTSHA256_HASH_SIZE];
    RTSha256Final(&CtxB, abDigestTemp);                                         /* Step 15. */

    /*
     * Byte sequence P (= password).
     */
    /* Step 16. */
    size_t const cbSeqP  = cchPhrase;
    uint8_t     *pabSeqP = (uint8_t *)RTMemTmpAllocZ(cbSeqP + 1);               /* +1 because the password may be empty */
    uint8_t     *pb       = pabSeqP;
    AssertPtrReturn(pabSeqP, VERR_NO_MEMORY);
    cbLeft = cbSeqP;
    while (cbLeft > RTSHA256_HASH_SIZE)
    {
        memcpy(pb, abDigestTemp, sizeof(abDigestTemp));                         /* a) */
        pb     += RTSHA256_HASH_SIZE;
        cbLeft -= RTSHA256_HASH_SIZE;
    }
    memcpy(pb, abDigestTemp, cbLeft);                                           /* b) */

    RTSha256Init(&CtxB);                                                        /* Step 17. */

    for (size_t i = 0; i < 16 + (unsigned)abDigest[0]; i++)                     /* Step 18. */
        RTSha256Update(&CtxB, pszSalt, cchSalt);

    RTSha256Final(&CtxB, abDigestTemp);                                         /* Step 19. */

    /*
     * Byte sequence S (= salt).
     */
    /* Step 20. */
    size_t   const  cbSeqS  = cchSalt;
#if 0 /* Given that the salt has a fixed range (8 thru 16 bytes), and SHA-256
       * producing 64 bytes, we can safely skip the loop part here (a) and go
       * straight for step (b). Further, we can drop the whole memory allocation,
       * let alone duplication (it's all overwritten!), and use an uninitalized
       * stack buffer. */
    uint8_t * const pabSeqS = (uint8_t *)RTMemDup(pszSalt, cbSeqS + 1);
    AssertPtrReturn(pabSeqS, VERR_NO_MEMORY);

    pb     = pabSeqS;
    cbLeft = cbSeqS;
    while (cbLeft > RTSHA256_HASH_SIZE)
    {
        memcpy(pb, (void *)abDigestTemp, sizeof(abDigestTemp));                 /* a) */
        pb     += RTSHA256_HASH_SIZE;
        cbLeft -= RTSHA256_HASH_SIZE
    }
    memcpy(pb, abDigestTemp, cbLeft);                                           /* b) */
#else
    AssertCompile(RT_SHACRYPT_SALT_MAX_LEN < RTSHA256_HASH_SIZE);
    uint8_t         abSeqS[RT_SHACRYPT_SALT_MAX_LEN + 2];
    uint8_t * const pabSeqS = abSeqS;
    memcpy(abSeqS, abDigestTemp, cbSeqS);                                       /* b) */
#endif

    /* Step 21. */
    for (uint32_t iRound = 0; iRound < cRounds; iRound++)
    {
        RTSHA256CONTEXT CtxC;
        RTSha256Init(&CtxC);                                                    /* a) */

        if ((iRound & 1) != 0)
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* b) */
        else
            RTSha256Update(&CtxC, abDigest, sizeof(abDigest));                  /* c) */

        if (iRound % 3 != 0)                                                    /* d) */
            RTSha256Update(&CtxC, pabSeqS, cbSeqS);

        if (iRound % 7 != 0)
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* e) */

        if ((iRound & 1) != 0)
            RTSha256Update(&CtxC, abDigest, sizeof(abDigest));                  /* f) */
        else
            RTSha256Update(&CtxC, pabSeqP, cbSeqP);                             /* g) */

        RTSha256Final(&CtxC, abDigest);                                         /* h) */
    }

    /*
     * Done.
     */
    memcpy(pabHash, abDigest, RTSHA256_HASH_SIZE);

    /*
     * Cleanup.
     */
    RTMemWipeThoroughly(abDigestTemp, RTSHA256_HASH_SIZE, 3);
    RTMemWipeThoroughly(pabSeqP, cbSeqP, 3);
    RTMemTmpFree(pabSeqP);
#if 0
    RTMemWipeThoroughly(pabSeqS, cbSeqS, 3);
    RTMemFree(pabSeqS);
#else
    RTMemWipeThoroughly(abSeqS, sizeof(abSeqS), 3);
#endif

    return VINF_SUCCESS;
}


RTR3DECL(int) RTCrShaCrypt256ToString(uint8_t const pabHash[RTSHA256_HASH_SIZE], const char *pszSalt, uint32_t cRounds,
                                      char *pszString, size_t cbString)
{
    /*
     * Validate and adjust input.
     */
    AssertPtrReturn(pszSalt, VERR_INVALID_POINTER);
    size_t cchSalt;
    pszSalt = rtCrShaCryptExtractSaltAndRounds(pszSalt, &cchSalt, &cRounds);
    AssertReturn(pszSalt != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(cchSalt >= RT_SHACRYPT_SALT_MIN_LEN, VERR_BUFFER_UNDERFLOW);
    AssertReturn(cchSalt <= RT_SHACRYPT_SALT_MAX_LEN, VERR_TOO_MUCH_DATA);
    AssertReturn(cRounds >= RT_SHACRYPT_ROUNDS_MIN && cRounds <= RT_SHACRYPT_ROUNDS_MAX, VERR_OUT_OF_RANGE);

    AssertPtrReturn(pszString, VERR_INVALID_POINTER);

    /*
     * Calc the necessary buffer space and check that the caller supplied enough.
     */
    char    szRounds[64];
    ssize_t cchRounds = 0;
    if (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT)
    {
        cchRounds = RTStrFormatU32(szRounds, sizeof(szRounds), cRounds, 10, 0, 0, 0);
        Assert(cchRounds > 0 && cchRounds <= 9);
    }

    size_t const cchNeeded = sizeof(RT_SHACRYPT_ID_STR_256) - 1
                           + (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT ? cchRounds + sizeof("rounds=$") - 1 : 0)
                           + cchSalt + 1
                           + RTSHA256_HASH_SIZE * 4 / 3
                           + 1;
    AssertReturn(cbString > cchNeeded, VERR_BUFFER_OVERFLOW);

    /*
     * Do the formatting.
     */
    memcpy(pszString, RT_STR_TUPLE(RT_SHACRYPT_ID_STR_256));
    size_t off = sizeof(RT_SHACRYPT_ID_STR_256) - 1;

    if (cRounds != RT_SHACRYPT_ROUNDS_DEFAULT)
    {
        memcpy(&pszString[off], RT_STR_TUPLE("rounds="));
        off += sizeof("rounds=") - 1;

        memcpy(&pszString[off], szRounds, cchRounds);
        off += cchRounds;
        pszString[off++] = '$';
    }

    memcpy(&pszString[off], pszSalt, cchSalt);
    off += cchSalt;
    pszString[off++] = '$';

    BASE64_ENCODE(pszString, off, pabHash[00], pabHash[10], pabHash[20], 4);
    BASE64_ENCODE(pszString, off, pabHash[21], pabHash[ 1], pabHash[11], 4);
    BASE64_ENCODE(pszString, off, pabHash[12], pabHash[22], pabHash[ 2], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 3], pabHash[13], pabHash[23], 4);
    BASE64_ENCODE(pszString, off, pabHash[24], pabHash[ 4], pabHash[14], 4);
    BASE64_ENCODE(pszString, off, pabHash[15], pabHash[25], pabHash[ 5], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 6], pabHash[16], pabHash[26], 4);
    BASE64_ENCODE(pszString, off, pabHash[27], pabHash[ 7], pabHash[17], 4);
    BASE64_ENCODE(pszString, off, pabHash[18], pabHash[28], pabHash[ 8], 4);
    BASE64_ENCODE(pszString, off, pabHash[ 9], pabHash[19], pabHash[29], 4);
    BASE64_ENCODE(pszString, off, 0,           pabHash[31], pabHash[30], 3);

    pszString[off] = '\0';
    Assert(off < cbString);

    return VINF_SUCCESS;
}

