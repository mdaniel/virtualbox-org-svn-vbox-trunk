/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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
#define LOG_GROUP RTLOGGROUP_ACPI
#include <iprt/acpi.h>
#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/script.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include <iprt/formats/acpi-aml.h>
#include <iprt/formats/acpi-resources.h>

#include "internal/acpi.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Terminals in the ACPI ASL lnaguage like keywords, operators and punctuators.
 */
typedef enum RTACPIASLTERMINAL
{
    RTACPIASLTERMINAL_INVALID = 0,
    RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK,
    RTACPIASLTERMINAL_KEYWORD_SCOPE,
    RTACPIASLTERMINAL_KEYWORD_PROCESSOR,

    RTACPIASLTERMINAL_KEYWORD_AND,
    RTACPIASLTERMINAL_KEYWORD_ARRAY,
    RTACPIASLTERMINAL_KEYWORD_ASM,
    RTACPIASLTERMINAL_KEYWORD_BEGIN,
    RTACPIASLTERMINAL_KEYWORD_BREAK,
    RTACPIASLTERMINAL_KEYWORD_CASE,
    RTACPIASLTERMINAL_KEYWORD_CONSTRUCTOR,
    RTACPIASLTERMINAL_KEYWORD_CONST,
    RTACPIASLTERMINAL_KEYWORD_CONTINUE,
    RTACPIASLTERMINAL_KEYWORD_DESTRUCTOR,
    RTACPIASLTERMINAL_KEYWORD_DIV,
    RTACPIASLTERMINAL_KEYWORD_DOWNTO,
    RTACPIASLTERMINAL_KEYWORD_DO,
    RTACPIASLTERMINAL_KEYWORD_ELSE,
    RTACPIASLTERMINAL_KEYWORD_END,
    RTACPIASLTERMINAL_KEYWORD_FALSE,
    RTACPIASLTERMINAL_KEYWORD_FILE,
    RTACPIASLTERMINAL_KEYWORD_FOR,
    RTACPIASLTERMINAL_KEYWORD_FUNCTION,
    RTACPIASLTERMINAL_KEYWORD_GOTO,
    RTACPIASLTERMINAL_KEYWORD_IF,
    RTACPIASLTERMINAL_KEYWORD_IMPLEMENTATION,
    RTACPIASLTERMINAL_KEYWORD_INTERFACE,
    RTACPIASLTERMINAL_KEYWORD_INLINE,
    RTACPIASLTERMINAL_KEYWORD_IN,
    RTACPIASLTERMINAL_KEYWORD_LABEL,
    RTACPIASLTERMINAL_KEYWORD_MOD,
    RTACPIASLTERMINAL_KEYWORD_NIL,
    RTACPIASLTERMINAL_KEYWORD_NOT,
    RTACPIASLTERMINAL_KEYWORD_OBJECT,
    RTACPIASLTERMINAL_KEYWORD_OF,
    RTACPIASLTERMINAL_KEYWORD_ON,
    RTACPIASLTERMINAL_KEYWORD_OPERATOR,
    RTACPIASLTERMINAL_KEYWORD_OR,
    RTACPIASLTERMINAL_KEYWORD_PACKED,
    RTACPIASLTERMINAL_KEYWORD_PROCEDURE,
    RTACPIASLTERMINAL_KEYWORD_PROGRAM,
    RTACPIASLTERMINAL_KEYWORD_RECORD,
    RTACPIASLTERMINAL_KEYWORD_REPEAT,
    RTACPIASLTERMINAL_KEYWORD_SET,
    RTACPIASLTERMINAL_KEYWORD_SHL,
    RTACPIASLTERMINAL_KEYWORD_SHR,
    RTACPIASLTERMINAL_KEYWORD_STRING,
    RTACPIASLTERMINAL_KEYWORD_THEN,
    RTACPIASLTERMINAL_KEYWORD_TO,
    RTACPIASLTERMINAL_KEYWORD_TRUE,
    RTACPIASLTERMINAL_KEYWORD_TYPE,
    RTACPIASLTERMINAL_KEYWORD_UNIT,
    RTACPIASLTERMINAL_KEYWORD_UNTIL,
    RTACPIASLTERMINAL_KEYWORD_USES,
    RTACPIASLTERMINAL_KEYWORD_VAR,
    RTACPIASLTERMINAL_KEYWORD_WHILE,
    RTACPIASLTERMINAL_KEYWORD_WITH,
    RTACPIASLTERMINAL_KEYWORD_XOR,

    RTACPIASLTERMINAL_OPERATOR_EQUALS,
    RTACPIASLTERMINAL_OPERATOR_ASSIGN,
    RTACPIASLTERMINAL_OPERATOR_UNEQUAL,
    RTACPIASLTERMINAL_OPERATOR_GREATER_EQUAL,
    RTACPIASLTERMINAL_OPERATOR_SMALLER_EQUAL,
    RTACPIASLTERMINAL_OPERATOR_SHIFT_RIGHT,
    RTACPIASLTERMINAL_OPERATOR_SHIFT_LEFT,
    RTACPIASLTERMINAL_OPERATOR_GREATER,
    RTACPIASLTERMINAL_OPERATOR_SMALLER,
    RTACPIASLTERMINAL_OPERATOR_PLUS,
    RTACPIASLTERMINAL_OPERATOR_MINUS,
    RTACPIASLTERMINAL_OPERATOR_MUL,
    RTACPIASLTERMINAL_OPERATOR_DIV,
    RTACPIASLTERMINAL_OPERATOR_MOD,
    RTACPIASLTERMINAL_OPERATOR_TILDE,
    RTACPIASLTERMINAL_OPERATOR_NOT,

    RTACPIASLTERMINAL_PUNCTUATOR_COMMA,
    RTACPIASLTERMINAL_PUNCTUATOR_DOT,
    RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET,
    RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET,

} RTACPIASLTERMINAL;


/**
 * The ACPI ASL compilation unit state.
 */
typedef struct RTACPIASLCU
{
    /** The lexer handle. */
    RTSCRIPTLEX             hLexSource;
    /** The VFS I/O input stream. */
    RTVFSIOSTREAM           hVfsIosIn;
    /** The ACPI table handle. */
    RTACPITBL               hAcpiTbl;
    /** Error information. */
    PRTERRINFO              pErrInfo;
} RTACPIASLCU;
/** Pointer to an ACPI ASL compilation unit state. */
typedef RTACPIASLCU *PRTACPIASLCU;
/** Pointer to a constant ACPI ASL compilation unit state. */
typedef const RTACPIASLCU *PCRTACPIASLCU;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static DECLCALLBACK(int) rtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);
static DECLCALLBACK(int) rtAcpiAslLexerParseNameString(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);


static const char *s_aszSingleStart[] =
{
    "//",
    NULL
};


static const char *s_aszMultiStart[] =
{
    "/*",
    NULL
};


static const char *s_aszMultiEnd[] =
{
    "*/",
    NULL
};


static const RTSCRIPTLEXTOKMATCH s_aMatches[] =
{
    /* Keywords */
    { RT_STR_TUPLE("DefinitionBlock"),          RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK       },
    { RT_STR_TUPLE("Processor"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_PROCESSOR              },
    { RT_STR_TUPLE("Scope"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  RTACPIASLTERMINAL_KEYWORD_SCOPE                  },

    /* Punctuators */
    { RT_STR_TUPLE(","),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_COMMA               },
    { RT_STR_TUPLE("."),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_DOT                 },
    { RT_STR_TUPLE("("),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET        },
    { RT_STR_TUPLE(")"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET       },
    { RT_STR_TUPLE("{"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET  },
    { RT_STR_TUPLE("}"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET },
    { NULL, 0,                                  RTSCRIPTLEXTOKTYPE_INVALID,    false, 0 }
};


static const RTSCRIPTLEXRULE s_aRules[] =
{
    { '\"', '\"',  RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanStringLiteralC,      NULL},
    { '0',  '9',   RTSCRIPT_LEX_RULE_CONSUME, rtAcpiAslLexerParseNumber,          NULL},
    { 'A',  'Z',   RTSCRIPT_LEX_RULE_CONSUME, rtAcpiAslLexerParseNameString,      NULL},
    { '_',  '_',   RTSCRIPT_LEX_RULE_CONSUME, rtAcpiAslLexerParseNameString,      NULL},
    { '^',  '^',   RTSCRIPT_LEX_RULE_CONSUME, rtAcpiAslLexerParseNameString,      NULL},
    { '\\',  '\\', RTSCRIPT_LEX_RULE_CONSUME, rtAcpiAslLexerParseNameString,      NULL},

    { '\0', '\0',  RTSCRIPT_LEX_RULE_DEFAULT, NULL,                               NULL}
};


static const RTSCRIPTLEXCFG s_AslLexCfg =
{
    /** pszName */
    "AcpiAsl",
    /** pszDesc */
    "ACPI ASL lexer",
    /** fFlags */
    RTSCRIPT_LEX_CFG_F_DEFAULT,
    /** pszWhitespace */
    NULL,
    /** pszNewline */
    NULL,
    /** papszCommentMultiStart */
    s_aszMultiStart,
    /** papszCommentMultiEnd */
    s_aszMultiEnd,
    /** papszCommentSingleStart */
    s_aszSingleStart,
    /** paTokMatches */
    s_aMatches,
    /** paRules */
    s_aRules,
    /** pfnProdDef */
    NULL,
    /** pfnProdDefUser */
    NULL
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static DECLCALLBACK(int) rtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser)
{
    RT_NOREF(ch, pvUser);
    return RTScriptLexScanNumber(hScriptLex, 0 /*uBase*/, false /*fAllowReal*/, pToken);
}


static int rtAcpiAslLexerParseNameSeg(RTSCRIPTLEX hScriptLex, PRTSCRIPTLEXTOKEN pTok, char *pachNameSeg)
{
    /*
     * A Nameseg consist of a lead character and up to 3 following characters A-Z, 0-9 or _.
     * If the name segment is not 4 characters long the remainder is filled with _.
     */
    char ch = RTScriptLexGetCh(hScriptLex);
    if (   ch != '_'
        && (  ch < 'A'
            || ch > 'Z'))
        return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_INVALID_PARAMETER, "Lexer: Name segment starts with invalid character '%c'", ch);
    RTScriptLexConsumeCh(hScriptLex);

    /* Initialize the default name segment. */
    pachNameSeg[0] = ch;
    pachNameSeg[1] = '_';
    pachNameSeg[2] = '_';
    pachNameSeg[3] = '_';

    for (uint8_t i = 1; i < 4; i++)
    {
        ch = RTScriptLexGetCh(hScriptLex);

        /* Anything not belonging to the allowed characters terminates the parsing. */
        if (   ch != '_'
            && (  ch < 'A'
                || ch > 'Z')
            && (  ch < '0'
                || ch > '9'))
            return VINF_SUCCESS;
        RTScriptLexConsumeCh(hScriptLex);
        pachNameSeg[i] = ch;
    } 

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) rtAcpiAslLexerParseNameString(RTSCRIPTLEX hScriptLex, char ch,
                                                       PRTSCRIPTLEXTOKEN pTok, void *pvUser)
{
    RT_NOREF(pvUser);

    char aszIde[513]; RT_ZERO(aszIde);
    unsigned idx = 0;
    aszIde[idx++] = ch;

    if (ch == '^') /* PrefixPath */
    {
        ch = RTScriptLexGetCh(hScriptLex);
        while (   idx < sizeof(aszIde) - 1
               && ch == '^')
        {
            RTScriptLexConsumeCh(hScriptLex);
            aszIde[idx++] = ch;
            ch = RTScriptLexGetCh(hScriptLex);
        }

        if (idx == sizeof(aszIde) - 1)
            return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_BUFFER_OVERFLOW, "Lexer: PrefixPath exceeds the allowed length");
    } 

    /* Now there is only a sequence of NameSeg allowed (separated by the . separator). */
    while (idx < sizeof(aszIde) - 1 - 4)
    {
        char achNameSeg[4];
        int rc = rtAcpiAslLexerParseNameSeg(hScriptLex, pTok, &achNameSeg[0]);
        if (RT_FAILURE(rc))
            return rc;

        aszIde[idx++] = achNameSeg[0];
        aszIde[idx++] = achNameSeg[1];
        aszIde[idx++] = achNameSeg[2];
        aszIde[idx++] = achNameSeg[3];

        ch = RTScriptLexGetCh(hScriptLex);
        if (ch != '.')
            break;
    }

    if (idx == sizeof(aszIde) - 1)
        return RTScriptLexProduceTokError(hScriptLex, pTok, VERR_BUFFER_OVERFLOW, "Lexer: Identifier exceeds the allowed length");

    return RTScriptLexProduceTokIde(hScriptLex, pTok, &aszIde[0], idx);
}


static DECLCALLBACK(int) rtAcpiAslLexerRead(RTSCRIPTLEX hScriptLex, size_t offBuf, char *pchCur,
                                            size_t cchBuf, size_t *pcchRead, void *pvUser)
{
    PCRTACPIASLCU pThis = (PCRTACPIASLCU)pvUser;
    RT_NOREF(hScriptLex, offBuf);

    size_t cbRead = 0;
    int rc = RTVfsIoStrmRead(pThis->hVfsIosIn, pchCur, cchBuf, true /*fBlocking*/, &cbRead);
    if (RT_FAILURE(rc))
        return rc;

    *pcchRead = cbRead * sizeof(char);
    if (!cbRead)
        return VINF_EOF;

    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfKeyword(PCRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerm, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query keyword token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_KEYWORD
        && pTok->Type.Keyword.pKeyword->u64Val == enmTerm)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        *pfConsumed = true;
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfPunctuator(PCRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerm, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_PUNCTUATOR
        && pTok->Type.Keyword.pKeyword->u64Val == enmTerm)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        *pfConsumed = true;
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfStringLit(PCRTACPIASLCU pThis, const char **ppszStrLit)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query string literal token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_STRINGLIT)
    {
        *ppszStrLit = pTok->Type.StringLit.pszString;
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfIdentifier(PCRTACPIASLCU pThis, const char **ppszIde)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query string literal token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_IDENTIFIER)
    {
        *ppszIde = pTok->Type.Id.pszIde;
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}


static int rtAcpiAslLexerConsumeIfNatural(PCRTACPIASLCU pThis, uint64_t *pu64, bool *pfConsumed)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_NUMBER
        && pTok->Type.Number.enmType == RTSCRIPTLEXTOKNUMTYPE_NATURAL)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        *pfConsumed = true;
        *pu64 = pTok->Type.Number.Type.u64;
        return VINF_SUCCESS;
    }

    *pfConsumed = false;
    return VINF_SUCCESS;
}


static int rtAcpiAslParserConsumeEos(PCRTACPIASLCU pThis)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Lexer: Failed to query punctuator token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_EOS)
    {
        RTScriptLexConsumeToken(pThis->hLexSource);
        return VINF_SUCCESS;
    }

    return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER,
                         "Parser: Found unexpected token after final closing }, expected end of stream");
}


/* Some parser helper macros. */
#define RTACPIASL_PARSE_KEYWORD(a_enmKeyword, a_pszKeyword) \
    do { \
        bool fConsumed = false; \
        int rc2 = rtAcpiAslLexerConsumeIfKeyword(pThis, a_enmKeyword, &fConsumed); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected keyword '%s'", a_pszKeyword); \
    } while(0)

#define RTACPIASL_PARSE_PUNCTUATOR(a_enmPunctuator, a_chPunctuator) \
    do { \
        bool fConsumed = false; \
        int rc2 = rtAcpiAslLexerConsumeIfPunctuator(pThis, a_enmPunctuator, &fConsumed); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected punctuator '%c'", a_chPunctuator); \
    } while(0)

#define RTACPIASL_PARSE_STRING_LIT(a_pszStrLit) \
    const char *a_pszStrLit = NULL; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfStringLit(pThis, &a_pszStrLit); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!a_pszStrLit) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected a string literal"); \
    } while(0)

#define RTACPIASL_PARSE_NAME_STRING(a_pszIde) \
    const char *a_pszIde = NULL; \
    do { \
        int rc2 = rtAcpiAslLexerConsumeIfIdentifier(pThis, &a_pszIde); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!a_pszIde) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected an identifier"); \
    } while(0)

#define RTACPIASL_PARSE_NATURAL(a_u64) \
    uint64_t a_u64 = 0; \
    do { \
        bool fConsumed = false; \
        int rc2 = rtAcpiAslLexerConsumeIfNatural(pThis, &a_u64, &fConsumed); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!fConsumed) \
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Expected a natural number"); \
    } while(0)

static int rtAcpiTblAslParseInner(PRTACPIASLCU pThis);

static int rtAcpiTblAslParseTerminal(PRTACPIASLCU pThis, RTACPIASLTERMINAL enmTerminal)
{
    int rc = VINF_SUCCESS;

    switch (enmTerminal)
    {
        case RTACPIASLTERMINAL_KEYWORD_SCOPE:
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
            RTACPIASL_PARSE_NAME_STRING(pszName);
            RT_NOREF(pszName);
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
            rc = rtAcpiTblAslParseInner(pThis);
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
            break;
        }
        case RTACPIASLTERMINAL_KEYWORD_PROCESSOR:
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
            RTACPIASL_PARSE_NAME_STRING(pszName);
            RT_NOREF(pszName);
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
            AssertFailed();
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
            rc = rtAcpiTblAslParseInner(pThis);
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
            break;
        }
        default:
            rc = RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected keyword encountered");
            break;
    }

    return rc;
}


static int rtAcpiTblAslParseInner(PRTACPIASLCU pThis)
{
    for (;;)
    {
        PCRTSCRIPTLEXTOKEN pTok;
        int rc = RTScriptLexQueryToken(pThis->hLexSource, &pTok);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pThis->pErrInfo, rc, "Parser: Failed to query next token with %Rrc", rc);

        if (   pTok->enmType == RTSCRIPTLEXTOKTYPE_PUNCTUATOR
            && pTok->Type.Keyword.pKeyword->u64Val == RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET)
            return VINF_SUCCESS;

        if (pTok->enmType == RTSCRIPTLEXTOKTYPE_EOS)
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected end of stream");
        if (pTok->enmType != RTSCRIPTLEXTOKTYPE_KEYWORD)
            return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Parser: Unexpected token encountered");

        RTACPIASLTERMINAL enmKeyword = (RTACPIASLTERMINAL)pTok->Type.Keyword.pKeyword->u64Val;
        RTScriptLexConsumeToken(pThis->hLexSource);

        rc = rtAcpiTblAslParseTerminal(pThis, enmKeyword);
        if (RT_FAILURE(rc))
            return rc;
    }
}


static int rtAcpiTblAslParserParse(PRTACPIASLCU pThis)
{
    /*
     * The first keyword must be DefinitionBlock:
     *
     *     DefinitionBlock ("SSDT.aml", "SSDT", 1, "VBOX  ", "VBOXCPUT", 2)
     */
    RTACPIASL_PARSE_KEYWORD(RTACPIASLTERMINAL_KEYWORD_DEFINITION_BLOCK, "DefinitionBlock");
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_BRACKET, '(');
    RTACPIASL_PARSE_STRING_LIT(pszOutFile);
    RT_NOREF(pszOutFile); /* We ignore the output file hint. */
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszTblSig);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64ComplianceRev);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszOemId);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_STRING_LIT(pszOemTblId);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_COMMA, ',');
    RTACPIASL_PARSE_NATURAL(u64OemRev);
    RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_BRACKET, ')');

    /* Some additional checks. */
    uint32_t u32TblSig = ACPI_TABLE_HDR_SIGNATURE_MISC;
    if (!strcmp(pszTblSig, "DSDT"))
        u32TblSig = ACPI_TABLE_HDR_SIGNATURE_DSDT;
    else if (!strcmp(pszTblSig, "SSDT"))
        u32TblSig = ACPI_TABLE_HDR_SIGNATURE_SSDT;

    if (u32TblSig == ACPI_TABLE_HDR_SIGNATURE_MISC)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Table signature must be either 'DSDT' or 'SSDT': %s", pszTblSig);

    if (u64ComplianceRev > UINT8_MAX)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "Compliance revision %RU64 is out of range, must be in range [0..255]", u64ComplianceRev);

    if (strlen(pszOemId) > 6)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM ID string must be at most 6 characters long");

    if (strlen(pszOemTblId) > 8)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM table ID string must be at most 8 characters long");

    if (u64OemRev > UINT32_MAX)
        return RTErrInfoSetF(pThis->pErrInfo, VERR_INVALID_PARAMETER, "OEM revision ID %RU64 is out of range, must fit into 32-bit unsigned integer", u64OemRev);

    int rc = RTAcpiTblCreate(&pThis->hAcpiTbl, u32TblSig, (uint8_t)u64ComplianceRev, pszOemId,
                             pszOemTblId, (uint32_t)u64OemRev, "VBOX", RTBldCfgRevision());
    if (RT_SUCCESS(rc))
    {
        RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_OPEN_CURLY_BRACKET, '{');
        rc = rtAcpiTblAslParseInner(pThis);
        if (RT_SUCCESS(rc))
        {
            RTACPIASL_PARSE_PUNCTUATOR(RTACPIASLTERMINAL_PUNCTUATOR_CLOSE_CURLY_BRACKET, '}');
            rc = rtAcpiAslParserConsumeEos(pThis); /* No junk after the final closing bracket. */
        }
    }

    return rc;
}


DECLHIDDEN(int) rtAcpiTblConvertFromAslToAml(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo)
{
    int rc;
    PRTACPIASLCU pThis = (PRTACPIASLCU)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->hVfsIosIn  = hVfsIosIn;
        pThis->pErrInfo   = pErrInfo;

        rc = RTScriptLexCreateFromReader(&pThis->hLexSource, rtAcpiAslLexerRead,
                                         NULL /*pfnDtor*/, pThis /*pvUser*/, 0 /*cchBuf*/,
                                         NULL /*phStrCacheId*/, NULL /*phStrCacheStringLit*/,
                                         &s_AslLexCfg);
        if (RT_SUCCESS(rc))
        {
            rc = rtAcpiTblAslParserParse(pThis);
            if (RT_SUCCESS(rc))
            {
                /* 2. - Optimize AST (constant folding, etc). */
                /* 3. - Traverse AST and output table. */
                RT_NOREF(hVfsIosOut);
            }

            /** @todo Destroy AST. */
            RTScriptLexDestroy(pThis->hLexSource);
        }
        else
            rc = RTErrInfoSetF(pErrInfo, rc, "Creating the ASL lexer failed with %Rrc", rc);

        RTMemFree(pThis);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Out of memory allocating the ASL compilation unit state");

    return rc;
}
