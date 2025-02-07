/* $Id$ */
/** @file
 * IPRT Testcase - ACPI API.
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
#include <iprt/acpi.h>

#include <iprt/err.h>
#include <iprt/file.h> /* RTFILE_O_READ and RTFILE_O_WRITE */
#include <iprt/script.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static DECLCALLBACK(int) tstRtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser);


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
    /* Punctuators */
    { RT_STR_TUPLE(","), RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("("), RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE(")"), RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("{"), RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { RT_STR_TUPLE("}"), RTSCRIPTLEXTOKTYPE_PUNCTUATOR, false, 0 },
    { NULL, 0,           RTSCRIPTLEXTOKTYPE_INVALID,    false, 0 }
};


static const RTSCRIPTLEXRULE s_aRules[] =
{
    { '\"', '\"',  RTSCRIPT_LEX_RULE_CONSUME, RTScriptLexScanStringLiteralC,  NULL},
    { '0',  '9',   RTSCRIPT_LEX_RULE_DEFAULT, tstRtAcpiAslLexerParseNumber,   NULL},
    { 'A',  'Z',   RTSCRIPT_LEX_RULE_DEFAULT, RTScriptLexScanIdentifier,      NULL},
    { '_',  '_',   RTSCRIPT_LEX_RULE_DEFAULT, RTScriptLexScanIdentifier,      NULL},
    { '^',  '^',   RTSCRIPT_LEX_RULE_DEFAULT, RTScriptLexScanIdentifier,      NULL},
    { '\\',  '\\', RTSCRIPT_LEX_RULE_DEFAULT, RTScriptLexScanIdentifier,      NULL},

    { '\0', '\0',  RTSCRIPT_LEX_RULE_DEFAULT, NULL,                           NULL}
};


static const RTSCRIPTLEXCFG s_AslLexCfg =
{
    /** pszName */
    "TstAcpiAsl",
    /** pszDesc */
    "ACPI ASL lexer for the testcase",
    /** fFlags */
    RTSCRIPT_LEX_CFG_F_CASE_INSENSITIVE_UPPER,
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


static DECLCALLBACK(int) tstRtAcpiAslLexerParseNumber(RTSCRIPTLEX hScriptLex, char ch, PRTSCRIPTLEXTOKEN pToken, void *pvUser)
{
    RT_NOREF(ch, pvUser);
    return RTScriptLexScanNumber(hScriptLex, 0 /*uBase*/, false /*fAllowReal*/, pToken);
}


static DECLCALLBACK(int) tstRtAcpiAslLexerRead(RTSCRIPTLEX hScriptLex, size_t offBuf, char *pchCur,
                                               size_t cchBuf, size_t *pcchRead, void *pvUser)
{
    RT_NOREF(hScriptLex, offBuf);

    size_t cbRead = 0;
    int rc = RTVfsFileRead((RTVFSFILE)pvUser, pchCur, cchBuf, &cbRead);
    if (RT_FAILURE(rc))
        return rc;

    *pcchRead = cbRead * sizeof(char);
    if (!cbRead)
        return VINF_EOF;

    return VINF_SUCCESS;
}


static void tstAcpiVerifySemantic(RTTEST hTest, RTVFSFILE hVfsFileAslSrc, RTVFSFILE hVfsFileAslOut)
{
    RT_NOREF(hTest);

    /* Build the lexer and compare that it semantically is equal to the source input. */
    RTSCRIPTLEX hLexAslSrc = NULL;
    int rc = RTScriptLexCreateFromReader(&hLexAslSrc, tstRtAcpiAslLexerRead,
                                         NULL /*pfnDtor*/, hVfsFileAslSrc /*pvUser*/, 0 /*cchBuf*/,
                                         NULL /*phStrCacheId*/, NULL /*phStrCacheStringLit*/,
                                         &s_AslLexCfg);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return; /* Can't do our work if this fails. */

    RTSCRIPTLEX hLexAslOut = NULL;
    rc = RTScriptLexCreateFromReader(&hLexAslOut, tstRtAcpiAslLexerRead,
                                     NULL /*pfnDtor*/, hVfsFileAslOut /*pvUser*/, 0 /*cchBuf*/,
                                     NULL /*phStrCacheId*/, NULL /*phStrCacheStringLit*/,
                                     &s_AslLexCfg);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
    if (RT_FAILURE(rc))
    {
        RTScriptLexDestroy(hLexAslSrc);
        return; /* Can't do our work if this fails. */
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t const  cErrBefore = RTTestIErrorCount();

        /* Now compare the token streams until we hit EOS in the lexers. */
        for (;;)
        {
            PCRTSCRIPTLEXTOKEN pTokAslSrc;
            rc = RTScriptLexQueryToken(hLexAslSrc, &pTokAslSrc);
            RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

            PCRTSCRIPTLEXTOKEN pTokAslOut;
            rc = RTScriptLexQueryToken(hLexAslOut, &pTokAslOut);
            RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

            RTTESTI_CHECK(pTokAslSrc->enmType == pTokAslOut->enmType);
            if (pTokAslSrc->enmType == RTSCRIPTLEXTOKTYPE_EOS)
                break;

            if (pTokAslSrc->enmType == pTokAslOut->enmType)
            {
                switch (pTokAslSrc->enmType)
                {
                    case RTSCRIPTLEXTOKTYPE_IDENTIFIER:
                    {
                        int iCmp = strcmp(pTokAslSrc->Type.Id.pszIde, pTokAslOut->Type.Id.pszIde);
                        RTTESTI_CHECK(!iCmp);
                        if (iCmp)
                            RTTestIFailureDetails("<IDE{%u.%u, %u.%u}, %s != %s>\n",
                                                  pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                  pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                  pTokAslSrc->Type.Id.pszIde, pTokAslOut->Type.Id.pszIde);
                        break;
                    }
                    case RTSCRIPTLEXTOKTYPE_NUMBER:
                        RTTESTI_CHECK(pTokAslSrc->Type.Number.enmType == pTokAslOut->Type.Number.enmType);
                        if (pTokAslSrc->Type.Number.enmType == pTokAslOut->Type.Number.enmType)
                        {
                            switch (pTokAslSrc->Type.Number.enmType)
                            {
                                case RTSCRIPTLEXTOKNUMTYPE_NATURAL:
                                {
                                    RTTESTI_CHECK(pTokAslSrc->Type.Number.Type.u64 == pTokAslOut->Type.Number.Type.u64);
                                    if (pTokAslSrc->Type.Number.Type.u64 != pTokAslOut->Type.Number.Type.u64)
                                        RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %RU64 != %RU64>\n",
                                                              pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                              pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                              pTokAslSrc->Type.Number.Type.u64, pTokAslOut->Type.Number.Type.u64);
                                    break;
                                }
                                case RTSCRIPTLEXTOKNUMTYPE_INTEGER:
                                {
                                    RTTESTI_CHECK(pTokAslSrc->Type.Number.Type.i64 == pTokAslOut->Type.Number.Type.i64);
                                    if (pTokAslSrc->Type.Number.Type.i64 != pTokAslOut->Type.Number.Type.i64)
                                        RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %RI64 != %RI64>\n",
                                                              pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                              pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                              pTokAslSrc->Type.Number.Type.i64, pTokAslOut->Type.Number.Type.i64);
                                    break;
                                }
                            case RTSCRIPTLEXTOKNUMTYPE_REAL:
                                default:
                                    AssertReleaseFailed();
                            }
                        }
                        else
                            RTTestIFailureDetails("<NUM{%u.%u, %u.%u} %u != %u>\n",
                                                  pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                  pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                  pTokAslSrc->Type.Number.enmType, pTokAslOut->Type.Number.enmType);
                        break;
                    case RTSCRIPTLEXTOKTYPE_PUNCTUATOR:
                    {
                        int iCmp = strcmp(pTokAslSrc->Type.Punctuator.pPunctuator->pszMatch,
                                          pTokAslOut->Type.Punctuator.pPunctuator->pszMatch);
                        RTTESTI_CHECK(!iCmp);
                        if (iCmp)
                            RTTestIFailureDetails("<PUNCTUATOR{%u.%u, %u.%u}, %s != %s>\n",
                                                  pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                  pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                  pTokAslSrc->Type.Punctuator.pPunctuator->pszMatch,
                                                  pTokAslOut->Type.Punctuator.pPunctuator->pszMatch);
                        break;
                    }
                    case RTSCRIPTLEXTOKTYPE_STRINGLIT:
                    {
                        int iCmp = strcmp(pTokAslSrc->Type.StringLit.pszString,
                                          pTokAslOut->Type.StringLit.pszString);
                        RTTESTI_CHECK(!iCmp);
                        if (iCmp)
                            RTTestIFailureDetails("<STRINGLIT{%u.%u, %u.%u}, \"%s\" != \"%s\">\n",
                                                  pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                                  pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                                  pTokAslSrc->Type.StringLit.pszString,
                                                  pTokAslOut->Type.StringLit.pszString);
                        break;
                    }

                    /* These should never occur and indicate an issue in the lexer. */
                    case RTSCRIPTLEXTOKTYPE_KEYWORD:
                        RTTestIFailureDetails("<KEYWORD{%u.%u, %u.%u}, %s>\n",
                                              pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                              pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                              pTokAslSrc->Type.Keyword.pKeyword->pszMatch);
                        break;
                    case RTSCRIPTLEXTOKTYPE_OPERATOR:
                        RTTestIFailureDetails("<OPERATOR{%u.%u, %u.%u}, %s>\n",
                                              pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                              pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                              pTokAslSrc->Type.Operator.pOp->pszMatch);
                        break;
                    case RTSCRIPTLEXTOKTYPE_INVALID:
                        RTTestIFailureDetails("<INVALID>\n");
                        break;
                    case RTSCRIPTLEXTOKTYPE_ERROR:
                        RTTestIFailureDetails("<ERROR{%u.%u, %u.%u}> %s\n",
                                              pTokAslSrc->PosStart.iLine, pTokAslSrc->PosStart.iCh,
                                              pTokAslSrc->PosEnd.iLine, pTokAslSrc->PosEnd.iCh,
                                              pTokAslSrc->Type.Error.pErr->pszMsg);
                        break;
                    case RTSCRIPTLEXTOKTYPE_EOS:
                        RTTestIFailureDetails("<EOS>\n");
                        break;
                    default:
                        AssertFailed();
                }
            }
            else
                RTTestIFailureDetails("pTokAslSrc->enmType=%u pTokAslOut->enmType=%u\n",
                                      pTokAslSrc->enmType, pTokAslOut->enmType);

            /*
             * Abort on error as the streams are now out of sync and matching will not work
             * anymore producing lots of noise.
             */
            if (cErrBefore != RTTestIErrorCount())
                break;

            /* Advance to the next token. */
            pTokAslSrc = RTScriptLexConsumeToken(hLexAslSrc);
            Assert(pTokAslSrc);

            pTokAslOut = RTScriptLexConsumeToken(hLexAslOut);
            Assert(pTokAslOut);
        }
    }

    RTScriptLexDestroy(hLexAslSrc);
    RTScriptLexDestroy(hLexAslOut);
}



/**
 * Some basic ASL -> AML -> ASL testcases.
 */
static void tstBasic(RTTEST hTest)
{
    RTTestSub(hTest, "Basic valid tests");
    static struct
    {
        const char *pszName;
        const char *pszAsl;
    } const aTests[] =
    {
        { "Empty",  "DefinitionBlock (\"\", \"SSDT\", 1, \"VBOX  \", \"VBOXTEST\", 2) {}\n" },
        { "Method", "DefinitionBlock (\"\", \"SSDT\", 1, \"VBOX  \", \"VBOXTEST\", 2)\n"
                    "{\n"
                    "Method(TEST, 1, NotSerialized, 0) {\n"
                    "If (LEqual(Arg0, One)) {\n"
                    "    Return (One)\n"
                    "} Else {\n"
                    "    Return (Zero)\n"
                    "}\n"
                    "}\n"
                    "}\n" }

    };
    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        RTTestISub(aTests[iTest].pszName);

        RTVFSFILE hVfsFileSrc = NIL_RTVFSFILE;
        int rc = RTVfsFileFromBuffer(RTFILE_O_READ, aTests[iTest].pszAsl, strlen(aTests[iTest].pszAsl), &hVfsFileSrc);
        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

        if (RT_SUCCESS(rc))
        {
            RTVFSFILE hVfsFileDst = NIL_RTVFSFILE;
            rc = RTVfsFileFromBuffer(RTFILE_O_READ | RTFILE_O_WRITE, NULL, 0, &hVfsFileDst);
            RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

            if (RT_SUCCESS(rc))
            {
                RTVFSIOSTREAM hVfsIosSrc  = RTVfsFileToIoStream(hVfsFileSrc);
                RTVFSIOSTREAM hVfsIosDst = RTVfsFileToIoStream(hVfsFileDst);
                RTTESTI_CHECK(hVfsIosSrc != NIL_RTVFSIOSTREAM && hVfsIosDst != NIL_RTVFSIOSTREAM);

                RTERRINFOSTATIC ErrInfo;
                rc = RTAcpiTblConvertFromVfsIoStrm(hVfsIosDst, RTACPITBLTYPE_AML, hVfsIosSrc, RTACPITBLTYPE_ASL, RTErrInfoInitStatic(&ErrInfo));
                RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                RTVfsIoStrmRelease(hVfsIosSrc);
                RTVfsIoStrmRelease(hVfsIosDst);

                rc = RTVfsFileSeek(hVfsFileDst, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                rc = RTVfsFileSeek(hVfsFileSrc, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                hVfsIosSrc = RTVfsFileToIoStream(hVfsFileSrc);
                hVfsIosDst = RTVfsFileToIoStream(hVfsFileDst);

                RTVFSFILE hVfsFileDstAsl = NIL_RTVFSFILE;
                rc = RTVfsFileFromBuffer(RTFILE_O_READ | RTFILE_O_WRITE, NULL, 0, &hVfsFileDstAsl);
                RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                if (RT_SUCCESS(rc))
                {
                    RTVFSIOSTREAM hVfsIosDstAsl = RTVfsFileToIoStream(hVfsFileDstAsl);
                    RTTESTI_CHECK(hVfsIosDstAsl != NIL_RTVFSIOSTREAM);

                    rc = RTAcpiTblConvertFromVfsIoStrm(hVfsIosDstAsl, RTACPITBLTYPE_ASL, hVfsIosDst, RTACPITBLTYPE_AML, RTErrInfoInitStatic(&ErrInfo));
                    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
                    RTVfsIoStrmRelease(hVfsIosDstAsl);

                    rc = RTVfsFileSeek(hVfsFileDstAsl, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

                    tstAcpiVerifySemantic(hTest, hVfsFileSrc, hVfsFileDstAsl);

                    RTVfsFileRelease(hVfsFileDstAsl);
                }

                RTVfsIoStrmRelease(hVfsIosSrc);
                RTVfsIoStrmRelease(hVfsIosDst);
                RTVfsFileRelease(hVfsFileDst);
            }

            RTVfsFileRelease(hVfsFileSrc);
        }
    }
}

int main(int argc, char **argv)
{
    RTTEST hTest;
    int rc = RTTestInitExAndCreate(argc, &argv, 0, "tstRTAcpi", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstBasic(hTest);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

