/* $Id$ */
/** @file
 * IPRT - iasl (acpica) like utility.
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

#include <iprt/buildconfig.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * IASL command options.
 */
typedef struct RTCMDIASLOPTS
{
    /** The input format. */
    RTACPITBLTYPE       enmInType;
    /** The output format. */
    RTACPITBLTYPE       enmOutType;
    /** The output filename */
    const char          *pszOutFile;
    /** Output blob version. */
    uint32_t            u32VersionBlobOut;
    /** The byte array name when converting to a C Header. */
    const char          *pszCHdrArrayName;
} RTCMDIASLOPTS;
/** Pointer to const IASL options. */
typedef RTCMDIASLOPTS const *PCRTCMDIASLOPTS;


/**
 * Private data of the to C header conversion I/O stream.
 */
typedef struct RTVFS2CHDRIOS
{
    /** The I/O stream handle. */
    RTVFSIOSTREAM   hVfsIos;
    /** Current stream offset. */
    RTFOFF          offStream;
    /** Number of characters to indent. */
    uint32_t        cchIndent;
    /** Number of bytes per line. */
    uint32_t        cBytesPerLine;
    /** Bytes outputted in the current line. */
    uint32_t        cBytesOutput;
} RTVFS2CHDRIOS;
/** Pointer to the private data the to C header conversion I/O stream. */
typedef RTVFS2CHDRIOS *PRTVFS2CHDRIOS;


static char g_szHexDigits[17] = "0123456789abcdef";

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_Close(void *pvThis)
{
    PRTVFS2CHDRIOS pThis = (PRTVFS2CHDRIOS)pvThis;

    int rc = RTVfsIoStrmPrintf(pThis->hVfsIos, "\n};\n");

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;
    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFS2CHDRIOS pThis = (PRTVFS2CHDRIOS)pvThis;
    return RTVfsIoStrmQueryInfo(pThis->hVfsIos, pObjInfo, enmAddAttr); /** @todo This is kind of wrong. */
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_Read(void *pvThis, RTFOFF off, PRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    RT_NOREF(pvThis, off, pSgBuf, fBlocking, pcbRead);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_Write(void *pvThis, RTFOFF off, PRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTVFS2CHDRIOS pThis = (PRTVFS2CHDRIOS)pvThis;
    AssertReturn(off == -1 || off == pThis->offStream , VERR_INVALID_PARAMETER);
    RT_NOREF(fBlocking);

    int             rc        = VINF_SUCCESS;
    size_t          cbWritten = 0;
    uint8_t const  *pbSrc     = (uint8_t const *)pSgBuf->paSegs[0].pvSeg;
    size_t          cbLeft    = pSgBuf->paSegs[0].cbSeg;
    char            achBuf[_4K];
    uint32_t        offBuf = 0;
    for (;;)
    {
        if (!cbLeft)
            break;

        /* New line? */
        if (!pThis->cBytesOutput)
        {
            if (offBuf + pThis->cchIndent < RT_ELEMENTS(achBuf))
            {
                rc = RTVfsIoStrmWrite(pThis->hVfsIos, &achBuf[0], offBuf, true /*fBlocking*/, NULL /*pcbWritten*/);
                if (RT_FAILURE(rc))
                    return rc;
                offBuf = 0;
            }
            memset(&achBuf[offBuf], ' ', pThis->cchIndent);
            offBuf += pThis->cchIndent;
        }

        while (   cbLeft
               && pThis->cBytesOutput < pThis->cBytesPerLine)
        {
            /* Each byte tykes up to 6 characters '0x00, ' so flush if the buffer is too full. */
            if (offBuf + pThis->cBytesPerLine * 6 < RT_ELEMENTS(achBuf))
            {
                rc = RTVfsIoStrmWrite(pThis->hVfsIos, &achBuf[0], offBuf, true /*fBlocking*/, NULL /*pcbWritten*/);
                if (RT_FAILURE(rc))
                    return rc;
                offBuf = 0;
            }

            achBuf[offBuf++] = '0';
            achBuf[offBuf++] = 'x';
            achBuf[offBuf++] = g_szHexDigits[*pbSrc >> 4];
            achBuf[offBuf++] = g_szHexDigits[*pbSrc & 0xf];
            cbLeft--;
            pbSrc++;
            pThis->cBytesOutput++;

            if (cbLeft)
            {
                achBuf[offBuf++] = ',';

                if (pThis->cBytesOutput < pThis->cBytesPerLine)
                    achBuf[offBuf++] = ' ';
                else
                {
                    achBuf[offBuf++] = '\n';
                    pThis->cBytesOutput = 0;
                    break;
                }
            }
        }
    }

    /* Last flush */
    if (offBuf)
        rc = RTVfsIoStrmWrite(pThis->hVfsIos, &achBuf[0], offBuf, true /*fBlocking*/, NULL /*pcbWritten*/);

    pThis->offStream += cbWritten;
    if (pcbWritten)
        *pcbWritten = cbWritten;
    RTSgBufAdvance(pSgBuf, cbWritten);

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_Flush(void *pvThis)
{
    PRTVFS2CHDRIOS pThis = (PRTVFS2CHDRIOS)pvThis;
    return RTVfsIoStrmFlush(pThis->hVfsIos);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtVfs2CHdrIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTVFS2CHDRIOS pThis = (PRTVFS2CHDRIOS)pvThis;
    *poffActual = pThis->offStream;
    return VINF_SUCCESS;
}


/**
 * I/O stream progress operations.
 */
DECL_HIDDEN_CONST(const RTVFSIOSTREAMOPS) g_rtVfs2CHdrIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "I/O Stream 2 C header",
        rtVfs2CHdrIos_Close,
        rtVfs2CHdrIos_QueryInfo,
        NULL,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    RTVFSIOSTREAMOPS_FEAT_NO_SG,
    rtVfs2CHdrIos_Read,
    rtVfs2CHdrIos_Write,
    rtVfs2CHdrIos_Flush,
    NULL /*PollOne*/,
    rtVfs2CHdrIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,
};


/**
 * Opens the input file.
 *
 * @returns Command exit, error messages written using RTMsg*.
 *
 * @param   pszFile             The input filename.
 * @param   phVfsIos            Where to return the input stream handle.
 */
static RTEXITCODE rtCmdIaslOpenInput(const char *pszFile, PRTVFSIOSTREAM phVfsIos)
{
    int rc;

    if (!strcmp(pszFile, "-"))
    {
        rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_INPUT,
                                      RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                      true /*fLeaveOpen*/,
                                      phVfsIos);
        if (RT_FAILURE(rc))
            return RTMsgErrorExitFailure("Error opening standard input: %Rrc", rc);
    }
    else
    {
        uint32_t        offError = 0;
        RTERRINFOSTATIC ErrInfo;
        rc = RTVfsChainOpenIoStream(pszFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                    phVfsIos, &offError, RTErrInfoInitStatic(&ErrInfo));
        if (RT_FAILURE(rc))
            return RTVfsChainMsgErrorExitFailure("RTVfsChainOpenIoStream", pszFile, rc, offError, &ErrInfo.Core);
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Opens the output file.
 *
 * @returns IPRT status code.
 *
 * @param   pszInputFile        Input filename (for writing it to the header if enabled).
 * @param   pszFile             The output filename.
 * @param   pszCHdrArrayName    If not NULL the output will be a C header with the given byte array containing the AML.
 * @param   phVfsIos            Where to return the input stream handle.
 */
static int rtCmdIaslOpenOutput(const char *pszInputFile, const char *pszFile, const char *pszCHdrArrayName, PRTVFSIOSTREAM phVfsIos)
{
    int rc;

    RTVFSIOSTREAM hVfsIosOut = NIL_RTVFSIOSTREAM;
    if (!strcmp(pszFile, "-"))
    {
        rc = RTVfsIoStrmFromStdHandle(RTHANDLESTD_OUTPUT,
                                      RTFILE_O_WRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                      true /*fLeaveOpen*/,
                                      &hVfsIosOut);
        if (RT_FAILURE(rc))
            return RTMsgErrorRc(rc, "Error opening standard output: %Rrc", rc);
    }
    else
    {
        uint32_t        offError = 0;
        RTERRINFOSTATIC ErrInfo;
        rc = RTVfsChainOpenIoStream(pszFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE,
                                    &hVfsIosOut, &offError, RTErrInfoInitStatic(&ErrInfo));
        if (RT_FAILURE(rc))
        {
            RTVfsChainMsgError("RTVfsChainOpenIoStream", pszFile, rc, offError, &ErrInfo.Core);
            return rc;
        }
    }

    if (pszCHdrArrayName)
    {
        /* Print the header. */
        rc = RTVfsIoStrmPrintf(hVfsIosOut, "/*\n"
                                           " * This file was automatically generated\n"
                                           " * from %s\n"
                                           " * by RTIasl.\n"
                                           " */\n"
                                           "\n"
                                           "\n"
                                           "static const unsigned char %s[] =\n"
                                           "{\n",
                                           pszInputFile, pszCHdrArrayName);

        PRTVFS2CHDRIOS pThis;
        rc = RTVfsNewIoStream(&g_rtVfs2CHdrIosOps, sizeof(*pThis), RTVfsIoStrmGetOpenFlags(hVfsIosOut),
                              NIL_RTVFS, NIL_RTVFSLOCK, phVfsIos, (void **)&pThis);
        if (RT_SUCCESS(rc))
        {
            pThis->hVfsIos       = hVfsIosOut;
            pThis->offStream     =  0;
            pThis->cchIndent     =  4;
            pThis->cBytesPerLine = 16;
            pThis->cBytesOutput  =  0;
        }
        return rc;
    }
    else
        *phVfsIos = hVfsIosOut;

    return VINF_SUCCESS;

}


/**
 * Processes the given input according to the options.
 *
 * @returns Command exit code, error messages written using RTMsg*.
 * @param   pszInputFile        Input filename (for writing it to the header if enabled).
 * @param   pOpts               The command options.
 * @param   hVfsSrc             VFS I/O stream handle of the input.
 */
static RTEXITCODE rtCmdIaslProcess(const char *pszInputFile, PCRTCMDIASLOPTS pOpts, RTVFSIOSTREAM hVfsSrc)
{
    if (pOpts->enmInType == RTACPITBLTYPE_INVALID)
        return RTMsgErrorExitFailure("iASL input format wasn't given");
    if (pOpts->enmOutType == RTACPITBLTYPE_INVALID)
        return RTMsgErrorExitFailure("iASL output format wasn't given");

    RTERRINFOSTATIC ErrInfo;
    RTVFSIOSTREAM hVfsIosDst = NIL_RTVFSIOSTREAM;
    int rc = rtCmdIaslOpenOutput(pszInputFile, pOpts->pszOutFile, pOpts->pszCHdrArrayName, &hVfsIosDst);
    if (RT_SUCCESS(rc))
    {
        rc = RTAcpiTblConvertFromVfsIoStrm(hVfsIosDst, pOpts->enmOutType, hVfsSrc, pOpts->enmInType, RTErrInfoInitStatic(&ErrInfo));
        if (RT_FAILURE(rc) && RTErrInfoIsSet(&ErrInfo.Core))
            rc =  RTMsgErrorRc(rc, "Disassembling the ACPI table failed: %Rrc - %s", rc, ErrInfo.Core.pszMsg);
        else if (RT_FAILURE(rc))
            rc = RTMsgErrorRc(rc, "Writing the disassembled ACPI table failed: %Rrc\n", rc);
        RTVfsIoStrmRelease(hVfsIosDst);
    }

    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    return RTEXITCODE_SUCCESS;
}


/**
 * A iasl clone.
 *
 * @returns Program exit code.
 *
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           The argument vector.  (Note that this may be
 *                              reordered, so the memory must be writable.)
 */
static RTEXITCODE RTCmdIasl(unsigned cArgs, char **papszArgs)
{

    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--disassemble",                      'd', RTGETOPT_REQ_NOTHING },
        { "--out",                              'o', RTGETOPT_REQ_STRING  },
        { "--path",                             'p', RTGETOPT_REQ_STRING  },
        { "--help",                             'h', RTGETOPT_REQ_NOTHING },
        { "--version",                          'v', RTGETOPT_REQ_NOTHING },
        { "--text-c-hdr",                       't', RTGETOPT_REQ_STRING  }
    };

    RTCMDIASLOPTS Opts;
    Opts.enmInType              = RTACPITBLTYPE_ASL;
    Opts.enmOutType             = RTACPITBLTYPE_AML;
    Opts.pszCHdrArrayName       = NULL;

    RTEXITCODE      rcExit      = RTEXITCODE_SUCCESS;
    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 1,
                          RTGETOPTINIT_FLAGS_OPTS_FIRST);
    if (RT_SUCCESS(rc))
    {
        bool fContinue = true;
        do
        {
            RTGETOPTUNION ValueUnion;
            int chOpt = RTGetOpt(&GetState, &ValueUnion);
            switch (chOpt)
            {
                case VINF_GETOPT_NOT_OPTION:
                {
                    RTVFSIOSTREAM hVfsSrc;
                    RTEXITCODE rcExit2 = rtCmdIaslOpenInput(ValueUnion.psz, &hVfsSrc);
                    if (rcExit2 == RTEXITCODE_SUCCESS)
                    {
                        rcExit2 = rtCmdIaslProcess(ValueUnion.psz, &Opts, hVfsSrc);
                        RTVfsIoStrmRelease(hVfsSrc);
                    }
                    if (rcExit2 != RTEXITCODE_SUCCESS)
                        rcExit = rcExit2;
                    fContinue = false;
                    break;
                }

                case 'd':
                    Opts.enmInType  = RTACPITBLTYPE_AML;
                    Opts.enmOutType = RTACPITBLTYPE_ASL;
                    break;

                case 'o':
                case 'p':
                    Opts.pszOutFile = ValueUnion.psz;
                    break;

                case 't':
                    Opts.pszCHdrArrayName = ValueUnion.psz;
                    break;

                case 'h':
                    RTPrintf("Usage: to be written\nOption dump:\n");
                    for (unsigned i = 0; i < RT_ELEMENTS(s_aOptions); i++)
                        RTPrintf(" -%c,%s\n", s_aOptions[i].iShort, s_aOptions[i].pszLong);
                    fContinue = false;
                    break;

                case 'v':
                    RTPrintf("%sr%d\n", RTBldCfgVersion(), RTBldCfgRevision());
                    fContinue = false;
                    break;

                default:
                    rcExit = RTGetOptPrintError(chOpt, &ValueUnion);
                    fContinue = false;
                    break;
            }
        } while (fContinue);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_SYNTAX, "RTGetOptInit: %Rrc", rc);
    return rcExit;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);
    return RTCmdIasl(argc, argv);
}

