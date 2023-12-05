/* $Id$ */
/** @file
 * IPRT - Crypto - SHA-crypt.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/shacrypt.h>

#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/sha.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define BASE64_ENCODE(a_psz, a_off, a_bVal2, a_bVal1, a_bVal0, a_cOutputChars) \
    do { \
        uint32_t uWord = RT_MAKE_U32_FROM_MSB_U8(0, a_bVal2, a_bVal1, a_bVal0); \
        a_psz[(a_off)++] = g_achCryptBase64[uWord & 0x3f]; \
        if ((a_cOutputChars) > 1) \
        { \
            uWord >>= 6; \
            a_psz[(a_off)++] = g_achCryptBase64[uWord & 0x3f]; \
        } \
        if ((a_cOutputChars) > 2) \
        { \
            uWord >>= 6; \
            a_psz[(a_off)++] = g_achCryptBase64[uWord & 0x3f]; \
        } \
        if ((a_cOutputChars) > 3) \
        { \
            uWord >>= 6; \
            a_psz[(a_off)++] = g_achCryptBase64[uWord & 0x3f]; \
        } \
    } while (0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** This is the non-standard base-64 encoding characters. */
static const char g_achCryptBase64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
AssertCompile(sizeof(g_achCryptBase64) == 64 + 1);


/**
 * Extracts the salt from a given string.
 *
 * @returns Pointer to the salt string, or NULL if not found / invalid.
 * @param   pszSalt     The string containing the salt.
 * @param   pcchSalt    Where to return the extracted salt length (in
 *                      characters).
 * @param   pcRounds
 */
static const char *rtCrShaCryptExtractSaltAndRounds(const char *pszSalt, size_t *pcchSalt, uint32_t *pcRounds)
{
    /*
     * Skip either of the two SHA-2 prefixes.
     */
    if (   pszSalt[0] == '$'
        && (pszSalt[1] == '5' || pszSalt[1] == '6')
        && pszSalt[2] == '$')
        pszSalt += 3;

    /* Look for 'rounds=xxxxx$'. */
    if (strncmp(pszSalt, RT_STR_TUPLE("rounds=")) == 0)
    {
        const char * const pszValue  = &pszSalt[sizeof("rounds=") - 1];
        const char * const pszDollar = strchr(pszValue, '$');
        if (pszDollar)
        {
            char *pszNext = NULL;
            int rc = RTStrToUInt32Ex(pszValue, &pszNext, 10, pcRounds);
            if (rc == VWRN_TRAILING_CHARS && pszNext == pszDollar)
            { /* likely */ }
            else if (rc == VWRN_NUMBER_TOO_BIG)
                *pcRounds = UINT32_MAX;
            else
                return NULL;
            pszSalt = pszDollar + 1;
        }
    }

    /* Find the length of the salt - it sends with '$' or '\0'. */
    const char * const pszDollar = strchr(pszSalt, '$');
    if (!pszDollar)
        *pcchSalt = strlen(pszSalt);
    else
        *pcchSalt = (size_t)(pszDollar - pszSalt);
    return pszSalt;
}

#include "shacrypt-256.cpp.h"
#include "shacrypt-512.cpp.h"


RTDECL(int) RTCrShaCryptGenerateSalt(char *pszSalt, size_t cchSalt)
{
    AssertMsgReturn(cchSalt >= RT_SHACRYPT_SALT_MIN_LEN && cchSalt <= RT_SHACRYPT_SALT_MAX_LEN, ("len=%zu\n", cchSalt),
                    VERR_OUT_OF_RANGE);

    for (size_t i = 0; i < cchSalt; i++)
        pszSalt[i] = g_achCryptBase64[RTRandU32Ex(0, sizeof(g_achCryptBase64) - 2)];

    pszSalt[cchSalt] = '\0';
    return VINF_SUCCESS;
}

