/* $Id$ */
/** @file
 * VBox xpidl clone - IDL parsing.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "xpidl.h"


typedef enum XPIDLKEYWORD
{
    kXpidlKeyword_Invalid = 0,
    kXpidlKeyword_Include,
    kXpidlKeyword_Typedef,
    kXpidlKeyword_32Bit_Hack = 0x7fffffff
} XPIDLKEYWORD;

static DECLCALLBACK(int) xpidlIdlLexParseNumberIdentifierOrUuid(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);


static const char *s_aszSingleStart[] =
{
    "//",
    NULL
};


static const char *s_aszMultiStart[] =
{
    "/*",
    "%{C++",
    NULL
};


static const char *s_aszMultiEnd[] =
{
    "*/",
    "%}",
    NULL
};


static const RTSCRIPTLEXTOKMATCH s_aMatches[] =
{
    { RT_STR_TUPLE("#include"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kXpidlKeyword_Include },
    { RT_STR_TUPLE("uuid"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("ptr"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("ref"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("in"),                       RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("out"),                      RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("scriptable"),               RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("noscript"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("array"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("size_is"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("readonly"),                 RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("attribute"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("retval"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("interface"),                RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("const"),                    RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("native"),                   RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("nsid"),                     RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  0 },
    { RT_STR_TUPLE("typedef"),                  RTSCRIPTLEXTOKTYPE_KEYWORD,    true,  kXpidlKeyword_Typedef },

    { RT_STR_TUPLE(","),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, ',' },
    { RT_STR_TUPLE("["),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, '[' },
    { RT_STR_TUPLE("]"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, ']' },
    { RT_STR_TUPLE("{"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, '{' },
    { RT_STR_TUPLE("}"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, '}' },
    { RT_STR_TUPLE("("),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, '(' },
    { RT_STR_TUPLE(")"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, ')' },
    { RT_STR_TUPLE(";"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, ';' },
    { RT_STR_TUPLE("="),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, '=' },
    { RT_STR_TUPLE(":"),                        RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, ':' },
    { NULL, 0,                                  RTSCRIPTLEXTOKTYPE_INVALID,    false, 0 }
};


static const RTSCRIPTLEXRULE s_aRules[] =
{
    { '\"', '\"',  RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanStringLiteralC,          NULL}, /** @todo This is not correct. */
    { '0',  '9',   RTSCRIPT_LEX_RULE_DEFAULT, xpidlIdlLexParseNumberIdentifierOrUuid, NULL},
    { 'a',  'z',   RTSCRIPT_LEX_RULE_DEFAULT, xpidlIdlLexParseNumberIdentifierOrUuid, NULL},
    { 'A',  'Z',   RTSCRIPT_LEX_RULE_DEFAULT, xpidlIdlLexParseNumberIdentifierOrUuid, NULL},
    { '_',  '_',   RTSCRIPT_LEX_RULE_DEFAULT, RTScriptLexScanIdentifier,              NULL},
    { '\0', '\0',  RTSCRIPT_LEX_RULE_DEFAULT, NULL,                                   NULL}
};


static const RTSCRIPTLEXCFG g_IdlLexCfg =
{
    /** pszName */
    "IDL",
    /** pszDesc */
    "IDL lexer",
    /** fFlags */
    RTSCRIPT_LEX_CFG_F_COMMENTS_AS_TOKENS,
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


static bool g_fRequiredUuid = false;

static int xpidlParseIdl(PXPIDLPARSE pThis, PXPIDLINPUT pInput, PRTLISTANCHOR pLstIncludePaths);


static DECLCALLBACK(int) xpidlIdlLexParseNumberIdentifierOrUuid(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser)
{
    RT_NOREF(pvUser);
    if (g_fRequiredUuid)
    {
        g_fRequiredUuid = false;
        /* Scan as an identifier. */
        static const char *g_aszIdeCharSetUuid = "abcdefABCDEF01234567809-";
        RTScriptLexConsumeCh(hScriptLex);
        return RTScriptLexScanIdentifier(hScriptLex, ch, pToken, (void *)g_aszIdeCharSetUuid);
    }
    else if (ch >= '0' && ch <= '9')
        return RTScriptLexScanNumber(hScriptLex, 0 /*uBase*/, false /*fAllowReal*/, pToken);

    RTScriptLexConsumeCh(hScriptLex);
    return RTScriptLexScanIdentifier(hScriptLex, ch, pToken, NULL);
}


/**
 * Create a new lexer from the given filename, possibly searching the include paths.
 *
 * @returns IPRT status code.
 * @param   pszFilename             The filename to read.
 * @param   pLstIncludePaths        The list of include paths to search for relative filenames.
 * @param   phIdlLex                Where to store the handle to the lexer on success.
 */
static int xpidlCreateLexerFromFilename(const char *pszFilename, PRTLISTANCHOR pLstIncludePaths,
                                        PRTSCRIPTLEX phIdlLex)
{
    char szPath[RTPATH_MAX];

    if (pszFilename[0] != '/')
    {
        PCXPIDLINCLUDEDIR pIt;
        RTListForEach(pLstIncludePaths, pIt, XPIDLINCLUDEDIR, NdIncludes)
        {
            ssize_t cch = RTStrPrintf2(&szPath[0], sizeof(szPath), "%s%c%s",
                                       pIt->pszPath, RTPATH_SLASH, pszFilename);
            if (cch <= 0)
                return VERR_BUFFER_OVERFLOW;

            if (RTFileExists(szPath))
            {
                pszFilename = szPath;
                break;
            }
        }
    }

    return RTScriptLexCreateFromFile(phIdlLex, pszFilename, NULL /*phStrCacheId*/,
                                     NULL /*phStrCacheStringLit*/, NULL /*phStrCacheComments*/,
                                     &g_IdlLexCfg);
}


static int xpidlParseError(PXPIDLPARSE pThis, PXPIDLINPUT pInput, PCRTSCRIPTLEXTOKEN pTok, int rc, const char *pszFmt, ...)
{
    va_list Args;
    va_start(Args, pszFmt);
    RT_NOREF(pTok);
    rc = RTErrInfoSetV(&pThis->ErrInfo.Core, rc, pszFmt, Args);
    va_end(Args);
    return rc;
}


static int xpidlLexerConsumeIfStringLit(PXPIDLPARSE pThis, PXPIDLINPUT pInput, const char **ppszStrLit)
{
    PCRTSCRIPTLEXTOKEN pTok;
    int rc = RTScriptLexQueryToken(pInput->hIdlLex, &pTok);
    if (RT_FAILURE(rc))
        return xpidlParseError(pThis, pInput, pTok, rc, "Lexer: Failed to query string literal token with %Rrc", rc);

    if (pTok->enmType == RTSCRIPTLEXTOKTYPE_STRINGLIT)
    {
        *ppszStrLit = pTok->Type.StringLit.pszString;
        RTScriptLexConsumeToken(pInput->hIdlLex);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}


#define XPIDL_PARSE_STRING_LIT(a_pszStrLit) \
    const char *a_pszStrLit = NULL; \
    do { \
        int rc2 = xpidlLexerConsumeIfStringLit(pThis, pInput, &a_pszStrLit); \
        if (RT_FAILURE(rc2)) \
            return rc2; \
        if (!a_pszStrLit) \
            return xpidlParseError(pThis, pInput, NULL, VERR_INVALID_PARAMETER, "Parser: Expected a string literal"); \
    } while(0)


static PXPIDLINPUT xpidlInputCreate(const char *pszFilename, PRTLISTANCHOR pLstIncludePaths)
{
    RTSCRIPTLEX hIdlLex = NULL;
    int rc = xpidlCreateLexerFromFilename(pszFilename, pLstIncludePaths, &hIdlLex);
    if (RT_FAILURE(rc))
        return NULL;

    PXPIDLINPUT pInput = (PXPIDLINPUT)xpidl_malloc(sizeof (*pInput));
    if (!pInput)
    {
        RTScriptLexDestroy(hIdlLex);
        return NULL;
    }

    RTListInit(&pInput->LstIncludes);
    pInput->hIdlLex     = hIdlLex;
    pInput->pszFilename = xpidl_strdup(pszFilename);
    return pInput;
}


static PXPIDLNODE xpidlNodeCreate(PXPIDLPARSE pThis, PXPIDLNODE pParent, PXPIDLINPUT pInput, XPIDLNDTYPE enmType)
{
    PXPIDLNODE pNode = (PXPIDLNODE)RTMemAllocZ(sizeof(*pNode));
    if (pNode)
    {
        pNode->pParent = pParent;
        pNode->pInput  = pInput;
        pNode->enmType = enmType;
    }
    else
        xpidlParseError(pThis, pInput, NULL, VERR_NO_MEMORY, "Failed to allocate node of type %u\n", enmType);

    return pNode;
}


static int xpidlParseKeyword(PXPIDLPARSE pThis, PXPIDLINPUT pInput, PRTLISTANCHOR pLstIncludePaths,
                             PCRTSCRIPTLEXTOKMATCH pKeyword)
{
    RT_NOREF(pThis, pInput, pLstIncludePaths);
    int rc;
    switch (pKeyword->u64Val)
    {
        case kXpidlKeyword_Include:
        {
            XPIDL_PARSE_STRING_LIT(pszFilename);
            PXPIDLINPUT pInput = xpidlInputCreate(pszFilename, pLstIncludePaths);
            if (!pInput)
                return xpidlParseError(pThis, pInput, NULL, VERR_INVALID_PARAMETER, "Failed opening include file '%s'",
                                       pszFilename);

            RTListAppend(&pThis->LstInputs, &pInput->NdInput);
            rc = xpidlParseIdl(pThis, pInput, pLstIncludePaths);
            break;
        }
        case kXpidlKeyword_Typedef:
        {
            /** @todo */
            break;
        } 
        default:
            rc = xpidlParseError(pThis, pInput, NULL, VERR_INVALID_PARAMETER, "Unexpected keyword '%s' found",
                                 pKeyword->pszMatch);
    } 
    return rc;
}


static int xpidlParseAttributes(PXPIDLPARSE pThis, PXPIDLINPUT pInput)
{
    RT_NOREF(pThis, pInput);
    return VERR_NOT_IMPLEMENTED;
}


static int xpidlParseIdl(PXPIDLPARSE pThis, PXPIDLINPUT pInput, PRTLISTANCHOR pLstIncludePaths)
{
    /* Parse IDL file. */
    int rc;
    for (;;)
    {
        PCRTSCRIPTLEXTOKEN pTok;
        rc = RTScriptLexQueryToken(pInput->hIdlLex, &pTok);
        if (RT_FAILURE(rc))
            return xpidlParseError(pThis, pInput, NULL, rc, "Parser: Failed to query next token with %Rrc", rc);

        if (pTok->enmType == RTSCRIPTLEXTOKTYPE_EOS)
            break;

        /*
         * In this outer loop we can either get comments, keywords or [] for
         * attributes of following nodes.
         */
        switch (pTok->enmType)
        {
            case RTSCRIPTLEXTOKTYPE_COMMENT_SINGLE_LINE:
                RTScriptLexConsumeToken(pInput->hIdlLex); /* These get ignored entirely. */
                break;
            case RTSCRIPTLEXTOKTYPE_COMMENT_MULTI_LINE:
            {
                /* Could be a raw block, check that the string starts with %{C++. */
                if (!strncmp(pTok->Type.Comment.pszComment, RT_STR_TUPLE("%{C++")))
                {
                    /* Create a new raw block node. */
                    PXPIDLNODE pNode = xpidlNodeCreate(pThis, NULL, pInput, kXpidlNdType_RawBlock);
                    if (pNode)
                    {
                        pNode->u.RawBlock.pszRaw = pTok->Type.Comment.pszComment + 5;
                        pNode->u.RawBlock.cchRaw = pTok->Type.Comment.cchComment - (5 + 2 + 1); /* Start + end + zero terminator. */
                        RTListAppend(&pThis->LstNodes, &pNode->NdLst);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
                /* else: Regular multi line comment, gets ignored. */
                RTScriptLexConsumeToken(pInput->hIdlLex);
                break;
            }
            case RTSCRIPTLEXTOKTYPE_KEYWORD:
            {
                PCRTSCRIPTLEXTOKMATCH pKeyword = pTok->Type.Keyword.pKeyword;
                RTScriptLexConsumeToken(pInput->hIdlLex);
                rc = xpidlParseKeyword(pThis, pInput, pLstIncludePaths, pKeyword); /** @todo This allows too much */
                break;
            }
            case RTSCRIPTLEXTOKTYPE_PUNCTUATOR:
            {
                if (pTok->Type.Punctuator.pPunctuator->u64Val == '[')
                {
                    RTScriptLexConsumeToken(pInput->hIdlLex);
                    rc = xpidlParseAttributes(pThis, pInput);
                }
                else
                    rc = xpidlParseError(pThis, pInput, pTok, VERR_INVALID_PARAMETER, "Unexpected punctuator found, expected '[', got '%c'",
                                         (char)pTok->Type.Punctuator.pPunctuator->u64Val);
                break;
            }
            case RTSCRIPTLEXTOKTYPE_ERROR:
                rc = xpidlParseError(pThis, pInput, pTok, VERR_INTERNAL_ERROR, "Internal lexer error: %s", pTok->Type.Error.pErr->pszMsg);
                break;
            default:
                rc = xpidlParseError(pThis, pInput, pTok, VERR_INVALID_PARAMETER, "Unexpected keyword found, expected raw block, keyword or '['");
                break;
        }

        if (RT_FAILURE(rc))
            break;
    }

    return rc;
}


int xpidl_process_idl(char *filename, PRTLISTANCHOR pLstIncludePaths,
                      char *file_basename, ModeData *mode)
{
    XPIDLPARSE ParseState;
    RTListInit(&ParseState.LstInputs);
    RTListInit(&ParseState.LstNodes);
    RTErrInfoInitStatic(&ParseState.ErrInfo);

    PXPIDLINPUT pInput = xpidlInputCreate(filename, pLstIncludePaths);
    if (!pInput)
        return VERR_NO_MEMORY;

    RTListAppend(&ParseState.LstInputs, &pInput->NdInput);
    int rc = xpidlParseIdl(&ParseState, pInput, pLstIncludePaths);
    if (RT_SUCCESS(rc))
    {
        /** @todo Output. */
    }
    else
        RTMsgError(ParseState.ErrInfo.Core.pszMsg);

    return rc;
}
