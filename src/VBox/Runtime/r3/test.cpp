/* $Id$ */
/** @file
 * IPRT - Testcase Framework.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
#include <iprt/test.h>

#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>

#include "internal/magics.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Guarded memory allocation record.
 */
typedef struct RTTESTGUARDEDMEM
{
    /** Pointer to the next record. */
    struct RTTESTGUARDEDMEM *pNext;
    /** The address we return to the user. */
    void           *pvUser;
    /** The base address of the allocation. */
    void           *pvAlloc;
    /** The size of the allocation. */
    size_t          cbAlloc;
    /** Guards. */
    struct
    {
        /** The guard address. */
        void       *pv;
        /** The guard size. */
        size_t      cb;
    }               aGuards[2];
} RTTESTGUARDEDMEM;
/** Pointer to an guarded memory allocation. */
typedef RTTESTGUARDEDMEM *PRTTESTGUARDEDMEM;

/**
 * Test instance structure.
 */
typedef struct RTTESTINT
{
    /** Magic. */
    uint32_t            u32Magic;
    /** The number of errors. */
    volatile uint32_t   cErrors;
    /** The test name. */
    const char         *pszTest;
    /** The length of the test name.  */
    size_t              cchTest;
    /** The size of a guard. Multiple of PAGE_SIZE. */
    uint32_t            cbGuard;
    /** The verbosity level. */
    RTTESTLVL           enmMaxLevel;
    /** The creation flags. */
    uint32_t            fFlags;


    /** Critical section serializing output. */
    RTCRITSECT          OutputLock;
    /** The output stream. */
    PRTSTREAM           pOutStrm;
    /** Whether we're currently at a newline. */
    bool                fNewLine;


    /** Critical section serializing access to the members following it. */
    RTCRITSECT          Lock;

    /** The list of guarded memory allocations. */
    PRTTESTGUARDEDMEM   pGuardedMem;

    /** The current sub-test. */
    const char         *pszSubTest;
    /** The length of the sub-test name. */
    size_t              cchSubTest;
    /** Whether the current subtest should figure as 'SKIPPED'. */
    bool                fSubTestSkipped;
    /** Whether we've reported the sub-test result or not. */
    bool                fSubTestReported;
    /** The start error count of the current subtest. */
    uint32_t            cSubTestAtErrors;

    /** The number of sub tests. */
    uint32_t            cSubTests;
    /** The number of sub tests that failed. */
    uint32_t            cSubTestsFailed;

    /** Set if XML output is enabled. */
    bool                fXmlEnabled;
    /** Set if we omit the top level test in the XML report. */
    bool                fXmlOmitTopTest;
    /** Set if we've reported the top test (for RTTEST_C_XML_DELAY_TOP_TEST). */
    bool                fXmlTopTestDone;
    enum {
        kXmlPos_ValueStart,
        kXmlPos_Value,
        kXmlPos_ElementEnd
    }                   eXmlState;
    /** Test pipe for the XML output stream going to the server. */
    RTPIPE              hXmlPipe;
    /** File where the XML output stream might be directed.  */
    RTFILE              hXmlFile;
    /** The number of XML elements on the stack. */
    size_t              cXmlElements;
    /** XML element stack. */
    const char         *apszXmlElements[10];

    /** Number of times assertions has been disabled and quieted. */
    uint32_t volatile   cAssertionsDisabledAndQuieted;
    /** Saved RTAssertSetQuiet return code. */
    bool                fAssertSavedQuiet;
    /** Saved RTAssertSetMayPanic return code. */
    bool                fAssertSavedMayPanic;
} RTTESTINT;
/** Pointer to a test instance. */
typedef RTTESTINT *PRTTESTINT;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Validate a test instance. */
#define RTTEST_VALID_RETURN(pTest)  \
    do { \
        AssertPtrReturn(pTest, VERR_INVALID_HANDLE); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, VERR_INVALID_HANDLE); \
    } while (0)

/** Gets and validates a test instance.
 * If the handle is nil, we will try retrieve it from the test TLS entry.
 */
#define RTTEST_GET_VALID_RETURN(pTest)  \
    do { \
        if (pTest == NIL_RTTEST) \
            pTest = (PRTTESTINT)RTTlsGet(g_iTestTls); \
        AssertPtrReturn(pTest, VERR_INVALID_HANDLE); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, VERR_INVALID_MAGIC); \
    } while (0)


/** Gets and validates a test instance.
 * If the handle is nil, we will try retrieve it from the test TLS entry.
 */
#define RTTEST_GET_VALID_RETURN_RC(pTest, rc)  \
    do { \
        if (pTest == NIL_RTTEST) \
            pTest = (PRTTESTINT)RTTlsGet(g_iTestTls); \
        AssertPtrReturn(pTest, (rc)); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, (rc)); \
    } while (0)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void rtTestGuardedFreeOne(PRTTESTGUARDEDMEM pMem);
static int  rtTestPrintf(PRTTESTINT pTest, const char *pszFormat, ...);
static void rtTestXmlStart(PRTTESTINT pTest, const char *pszTest);
static void rtTestXmlElemV(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, va_list va);
static void rtTestXmlElem(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, ...);
static void rtTestXmlElemStartV(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, va_list va);
static void rtTestXmlElemStart(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, ...);
static void rtTestXmlElemEnd(PRTTESTINT pTest, const char *pszTag);
static void rtTestXmlEnd(PRTTESTINT pTest);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** For serializing TLS init. */
static RTONCE   g_TestInitOnce = RTONCE_INITIALIZER;
/** Our TLS entry. */
static RTTLS    g_iTestTls = NIL_RTTLS;



/**
 * Init TLS index once.
 *
 * @returns IPRT status code.
 * @param   pvUser      Ignored.
 */
static DECLCALLBACK(int32_t) rtTestInitOnce(void *pvUser)
{
    NOREF(pvUser);
    return RTTlsAllocEx(&g_iTestTls, NULL);
}


RTR3DECL(int) RTTestCreateEx(const char *pszTest, uint32_t fFlags, RTTESTLVL enmMaxLevel,
                             RTHCINTPTR iNativeTestPipe, const char *pszXmlFile, PRTTEST phTest)
{
    AssertReturn(!(fFlags & ~RTTEST_C_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertPtrNull(phTest);
    AssertPtrNull(pszXmlFile);
    /* RTTESTLVL_INVALID is valid! */
    AssertReturn(enmMaxLevel >= RTTESTLVL_INVALID && enmMaxLevel < RTTESTLVL_END, VERR_INVALID_PARAMETER);

    /*
     * Global init.
     */
    int rc = RTOnce(&g_TestInitOnce, rtTestInitOnce, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the instance.
     */
    PRTTESTINT pTest = (PRTTESTINT)RTMemAllocZ(sizeof(*pTest));
    if (!pTest)
        return VERR_NO_MEMORY;
    pTest->u32Magic         = RTTESTINT_MAGIC;
    pTest->pszTest          = RTStrDup(pszTest);
    pTest->cchTest          = strlen(pszTest);
    pTest->cbGuard          = PAGE_SIZE * 7;
    pTest->enmMaxLevel      = enmMaxLevel == RTTESTLVL_INVALID ? RTTESTLVL_INFO : enmMaxLevel;
    pTest->fFlags           = fFlags;

    pTest->pOutStrm         = g_pStdOut;
    pTest->fNewLine         = true;

    pTest->pGuardedMem      = NULL;

    pTest->pszSubTest       = NULL;
    pTest->cchSubTest       = 0;
    pTest->fSubTestSkipped  = false;
    pTest->fSubTestReported = true;
    pTest->cSubTestAtErrors = 0;
    pTest->cSubTests        = 0;
    pTest->cSubTestsFailed  = 0;

    pTest->fXmlEnabled      = false;
    pTest->fXmlTopTestDone  = false;
    pTest->eXmlState        = RTTESTINT::kXmlPos_ElementEnd;
    pTest->hXmlPipe         = NIL_RTPIPE;
    pTest->hXmlFile         = NIL_RTFILE;
    pTest->cXmlElements     = 0;
    pTest->cAssertionsDisabledAndQuieted = 0;
    pTest->fAssertSavedMayPanic          = true;
    pTest->fAssertSavedQuiet             = false;

    rc = RTCritSectInit(&pTest->Lock);
    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectInit(&pTest->OutputLock);
        if (RT_SUCCESS(rc))
        {
            /*
             * Associate it with our TLS entry unless there is already
             * an instance there.
             */
            if (   !(fFlags & RTTEST_C_NO_TLS)
                && !RTTlsGet(g_iTestTls))
                rc = RTTlsSet(g_iTestTls, pTest);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Output level override?
                 */
                char szEnvVal[RTPATH_MAX];
                if ((fFlags & RTTEST_C_USE_ENV) && enmMaxLevel == RTTESTLVL_INVALID)
                {
                    rc = RTEnvGetEx(RTENV_DEFAULT, "IPRT_TEST_MAX_LEVEL", szEnvVal, sizeof(szEnvVal), NULL);
                    if (RT_SUCCESS(rc))
                    {
                        char *pszMaxLevel = RTStrStrip(szEnvVal);
                        if (!strcmp(pszMaxLevel, "all"))
                            pTest->enmMaxLevel = RTTESTLVL_DEBUG;
                        if (!strcmp(pszMaxLevel, "quiet"))
                            pTest->enmMaxLevel = RTTESTLVL_FAILURE;
                        else if (!strcmp(pszMaxLevel, "debug"))
                            pTest->enmMaxLevel = RTTESTLVL_DEBUG;
                        else if (!strcmp(pszMaxLevel, "info"))
                            pTest->enmMaxLevel = RTTESTLVL_INFO;
                        else if (!strcmp(pszMaxLevel, "sub_test"))
                            pTest->enmMaxLevel = RTTESTLVL_SUB_TEST;
                        else if (!strcmp(pszMaxLevel, "failure"))
                            pTest->enmMaxLevel = RTTESTLVL_FAILURE;
                    }
                    else if (rc != VERR_ENV_VAR_NOT_FOUND)
                        RTStrmPrintf(g_pStdErr, "%s: test pipe error: RTEnvGetEx(IPRT_TEST_MAX_LEVEL) -> %Rrc\n", pszTest, rc);
                }

                /*
                 * Any test driver we are connected or should connect to?
                 */
                if (!(fFlags & RTTEST_C_NO_XML_REPORTING_PIPE))
                {
                    if (   (fFlags & RTTEST_C_USE_ENV)
                        && iNativeTestPipe == -1)
                    {
                        rc = RTEnvGetEx(RTENV_DEFAULT, "IPRT_TEST_PIPE", szEnvVal, sizeof(szEnvVal), NULL);
                        if (RT_SUCCESS(rc))
                        {
#if ARCH_BITS == 64
                            rc = RTStrToInt64Full(szEnvVal, 0, &iNativeTestPipe);
#else
                            rc = RTStrToInt32Full(szEnvVal, 0, &iNativeTestPipe);
#endif
                            if (RT_FAILURE(rc))
                            {
                                RTStrmPrintf(g_pStdErr, "%s: test pipe error: RTStrToInt32Full(\"%s\") -> %Rrc\n",
                                             pszTest, szEnvVal, rc);
                                iNativeTestPipe = -1;
                            }
                        }
                        else if (rc != VERR_ENV_VAR_NOT_FOUND)
                            RTStrmPrintf(g_pStdErr, "%s: test pipe error: RTEnvGetEx(IPRT_TEST_PIPE) -> %Rrc\n", pszTest, rc);
                    }
                    if (iNativeTestPipe != -1)
                    {
                        rc = RTPipeFromNative(&pTest->hXmlPipe, iNativeTestPipe, RTPIPE_N_WRITE);
                        if (RT_SUCCESS(rc))
                            pTest->fXmlEnabled = true;
                        else
                        {
                            RTStrmPrintf(g_pStdErr, "%s: test pipe error: RTPipeFromNative(,%p,WRITE) -> %Rrc\n",
                                         pszTest, iNativeTestPipe, rc);
                            pTest->hXmlPipe = NIL_RTPIPE;
                        }
                    }
                }

                /*
                 * Any test file we should write the test report to?
                 */
                if (!(fFlags & RTTEST_C_NO_XML_REPORTING_FILE))
                {
                    if ((fFlags & RTTEST_C_USE_ENV) && pszXmlFile == NULL)
                    {
                        rc = RTEnvGetEx(RTENV_DEFAULT, "IPRT_TEST_FILE", szEnvVal, sizeof(szEnvVal), NULL);
                        if (RT_SUCCESS(rc))
                            pszXmlFile = szEnvVal;
                        else if (rc != VERR_ENV_VAR_NOT_FOUND)
                            RTStrmPrintf(g_pStdErr, "%s: test file error: RTEnvGetEx(IPRT_TEST_MAX_LEVEL) -> %Rrc\n", pszTest, rc);
                    }
                    if (pszXmlFile && *pszXmlFile)
                    {
                        rc = RTFileOpen(&pTest->hXmlFile, pszXmlFile,
                                        RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE);
                        if (RT_SUCCESS(rc))
                            pTest->fXmlEnabled = true;
                        else
                        {
                            RTStrmPrintf(g_pStdErr, "%s: test file error: RTFileOpen(,\"%s\",) -> %Rrc\n",
                                         pszTest, pszXmlFile, rc);
                            pTest->hXmlFile = NIL_RTFILE;
                        }
                    }
                }

                /*
                 * What do we report in the XML stream/file.?
                 */
                pTest->fXmlOmitTopTest = (fFlags & RTTEST_C_XML_OMIT_TOP_TEST)
                                      || (   (fFlags & RTTEST_C_USE_ENV)
                                          && RTEnvExistEx(RTENV_DEFAULT, "IPRT_TEST_OMIT_TOP_TEST"));

                /*
                 * Tell the test driver that we're up to.
                 */
                rtTestXmlStart(pTest, pszTest);

                *phTest = pTest;
                return VINF_SUCCESS;
            }

            /* bail out. */
            RTCritSectDelete(&pTest->OutputLock);
        }
        RTCritSectDelete(&pTest->Lock);
    }
    pTest->u32Magic = 0;
    RTStrFree((char *)pTest->pszTest);
    RTMemFree(pTest);
    return rc;
}


RTR3DECL(int) RTTestCreate(const char *pszTest, PRTTEST phTest)
{
    return RTTestCreateEx(pszTest, RTTEST_C_USE_ENV, RTTESTLVL_INVALID, -1 /*iNativeTestPipe*/, NULL /*pszXmlFile*/, phTest);
}


RTR3DECL(int) RTTestCreateChild(const char *pszTest, PRTTEST phTest)
{
    return RTTestCreateEx(pszTest, RTTEST_C_USE_ENV | RTTEST_C_NO_XML_REPORTING,
                          RTTESTLVL_INVALID, -1 /*iNativeTestPipe*/, NULL /*pszXmlFile*/, phTest);
}


RTR3DECL(RTEXITCODE) RTTestInitAndCreate(const char *pszTest, PRTTEST phTest)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "%s: fatal error: RTR3InitExeNoArguments failed with rc=%Rrc\n", pszTest, rc);
        return RTEXITCODE_INIT;
    }

    rc = RTTestCreate(pszTest, phTest);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "%s: fatal error: RTTestCreate failed with rc=%Rrc\n", pszTest, rc);
        return RTEXITCODE_INIT;
    }
    return RTEXITCODE_SUCCESS;
}


RTR3DECL(RTEXITCODE) RTTestInitExAndCreate(int cArgs, char ***ppapszArgs, uint32_t fRtInit, const char *pszTest, PRTTEST phTest)
{
    int rc;
    if (cArgs <= 0 && ppapszArgs == NULL)
        rc = RTR3InitExeNoArguments(fRtInit);
    else
        rc = RTR3InitExe(cArgs, ppapszArgs, fRtInit);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "%s: fatal error: RTR3InitExe(,,%#x) failed with rc=%Rrc\n", pszTest, fRtInit, rc);
        return RTEXITCODE_INIT;
    }

    rc = RTTestCreate(pszTest, phTest);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "%s: fatal error: RTTestCreate failed with rc=%Rrc\n", pszTest, rc);
        return RTEXITCODE_INIT;
    }
    return RTEXITCODE_SUCCESS;
}


/**
 * Destroys a test instance previously created by RTTestCreate.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. NIL_RTTEST is ignored.
 */
RTR3DECL(int) RTTestDestroy(RTTEST hTest)
{
    /*
     * Validate
     */
    if (hTest == NIL_RTTEST)
        return VINF_SUCCESS;
    RTTESTINT *pTest = hTest;
    RTTEST_VALID_RETURN(pTest);

    /*
     * Make sure we end with a new line and have finished up the XML.
     */
    if (!pTest->fNewLine)
        rtTestPrintf(pTest, "\n");
    rtTestXmlEnd(pTest);

    /*
     * Clean up.
     */
    if ((RTTESTINT *)RTTlsGet(g_iTestTls) == pTest)
        RTTlsSet(g_iTestTls, NULL);

    ASMAtomicWriteU32(&pTest->u32Magic, ~RTTESTINT_MAGIC);
    RTCritSectDelete(&pTest->Lock);
    RTCritSectDelete(&pTest->OutputLock);

    /* free guarded memory. */
    PRTTESTGUARDEDMEM pMem = pTest->pGuardedMem;
    pTest->pGuardedMem = NULL;
    while (pMem)
    {
        PRTTESTGUARDEDMEM pFree = pMem;
        pMem = pMem->pNext;
        rtTestGuardedFreeOne(pFree);
    }

    RTStrFree((char *)pTest->pszSubTest);
    pTest->pszSubTest = NULL;
    RTStrFree((char *)pTest->pszTest);
    pTest->pszTest = NULL;
    RTMemFree(pTest);
    return VINF_SUCCESS;
}


/**
 * Changes the default test instance for the calling thread.
 *
 * @returns IPRT status code.
 *
 * @param   hNewDefaultTest The new default test. NIL_RTTEST is fine.
 * @param   phOldTest       Where to store the old test handle. Optional.
 */
RTR3DECL(int) RTTestSetDefault(RTTEST hNewDefaultTest, PRTTEST phOldTest)
{
    if (phOldTest)
        *phOldTest = (RTTEST)RTTlsGet(g_iTestTls);
    return RTTlsSet(g_iTestTls, hNewDefaultTest);
}


RTR3DECL(int) RTTestChangeName(RTTEST hTest, const char *pszName)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);

    size_t cchName = strlen(pszName);
    AssertReturn(cchName < 128, VERR_INVALID_PARAMETER);
    char *pszDupName = RTStrDup(pszName);
    if (!pszDupName)
        return VERR_NO_STR_MEMORY;

    RTCritSectEnter(&pTest->Lock);
    RTCritSectEnter(&pTest->OutputLock);

    char *pszOldName = (char *)pTest->pszTest;
    pTest->pszTest = pszDupName;
    pTest->cchTest = cchName;

    RTCritSectLeave(&pTest->OutputLock);
    RTCritSectLeave(&pTest->Lock);

    RTStrFree(pszOldName);
    return VINF_SUCCESS;
}


/**
 * Allocate a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 * @param   cbAlign     The alignment of the returned block.
 * @param   fHead       Head or tail optimized guard.
 * @param   ppvUser     Where to return the pointer to the block.
 */
RTR3DECL(int) RTTestGuardedAlloc(RTTEST hTest, size_t cb, uint32_t cbAlign, bool fHead, void **ppvUser)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    if (cbAlign == 0)
        cbAlign = 1;
    AssertReturn(cbAlign <= PAGE_SIZE, VERR_INVALID_PARAMETER);
    AssertReturn(cbAlign == (UINT32_C(1) << (ASMBitFirstSetU32(cbAlign) - 1)), VERR_INVALID_PARAMETER);

    /*
     * Allocate the record and block and initialize them.
     */
    int                 rc = VERR_NO_MEMORY;
    PRTTESTGUARDEDMEM   pMem = (PRTTESTGUARDEDMEM)RTMemAlloc(sizeof(*pMem));
    if (RT_LIKELY(pMem))
    {
        size_t const    cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
        pMem->aGuards[0].cb = pMem->aGuards[1].cb = pTest->cbGuard;
        pMem->cbAlloc       = pMem->aGuards[0].cb + pMem->aGuards[1].cb + cbAligned;
        pMem->pvAlloc       = RTMemPageAlloc(pMem->cbAlloc);
        if (pMem->pvAlloc)
        {
            pMem->aGuards[0].pv = pMem->pvAlloc;
            pMem->pvUser        = (uint8_t *)pMem->pvAlloc + pMem->aGuards[0].cb;
            pMem->aGuards[1].pv = (uint8_t *)pMem->pvUser + cbAligned;
            if (!fHead)
            {
                size_t off = cb & PAGE_OFFSET_MASK;
                if (off)
                {
                    off = PAGE_SIZE - RT_ALIGN_Z(off, cbAlign);
                    pMem->pvUser = (uint8_t *)pMem->pvUser + off;
                }
            }

            /*
             * Set up the guards and link the record.
             */
            ASMMemFill32(pMem->aGuards[0].pv, pMem->aGuards[0].cb, 0xdeadbeef);
            ASMMemFill32(pMem->aGuards[1].pv, pMem->aGuards[1].cb, 0xdeadbeef);
            rc = RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_NONE);
            if (RT_SUCCESS(rc))
            {
                rc = RTMemProtect(pMem->aGuards[1].pv, pMem->aGuards[1].cb, RTMEM_PROT_NONE);
                if (RT_SUCCESS(rc))
                {
                    *ppvUser = pMem->pvUser;

                    RTCritSectEnter(&pTest->Lock);
                    pMem->pNext = pTest->pGuardedMem;
                    pTest->pGuardedMem = pMem;
                    RTCritSectLeave(&pTest->Lock);

                    return VINF_SUCCESS;
                }

                RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ);
            }

            RTMemPageFree(pMem->pvAlloc, pMem->cbAlloc);
        }
        RTMemFree(pMem);
    }
    return rc;
}


/**
 * Allocates a block of guarded memory where the guarded is immediately after
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocTail(RTTEST hTest, size_t cb)
{
    void *pvUser;
    int rc = RTTestGuardedAlloc(hTest, cb, 1 /* cbAlign */, false /* fHead */, &pvUser);
    if (RT_SUCCESS(rc))
        return pvUser;
    return NULL;
}


/**
 * Allocates a block of guarded memory where the guarded is right in front of
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocHead(RTTEST hTest, size_t cb)
{
    void *pvUser;
    int rc = RTTestGuardedAlloc(hTest, cb, 1 /* cbAlign */, true /* fHead */, &pvUser);
    if (RT_SUCCESS(rc))
        return pvUser;
    return NULL;
}


/**
 * Frees one block of guarded memory.
 *
 * The caller is responsible for unlinking it.
 *
 * @param   pMem        The memory record.
 */
static void rtTestGuardedFreeOne(PRTTESTGUARDEDMEM pMem)
{
    int rc;
    rc = RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ); AssertRC(rc);
    rc = RTMemProtect(pMem->aGuards[1].pv, pMem->aGuards[1].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ); AssertRC(rc);
    RTMemPageFree(pMem->pvAlloc, pMem->cbAlloc);
    RTMemFree(pMem);
}


/**
 * Frees a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pv          The memory. NULL is ignored.
 */
RTR3DECL(int) RTTestGuardedFree(RTTEST hTest, void *pv)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Find it.
     */
    int                 rc = VERR_INVALID_POINTER;
    PRTTESTGUARDEDMEM   pPrev = NULL;

    RTCritSectEnter(&pTest->Lock);
    for (PRTTESTGUARDEDMEM pMem = pTest->pGuardedMem; pMem; pMem = pMem->pNext)
    {
        if (pMem->pvUser == pv)
        {
            if (pPrev)
                pPrev->pNext = pMem->pNext;
            else
                pTest->pGuardedMem = pMem->pNext;
            rtTestGuardedFreeOne(pMem);
            rc = VINF_SUCCESS;
            break;
        }
        pPrev = pMem;
    }
    RTCritSectLeave(&pTest->Lock);

    return rc;
}


/**
 * Outputs the formatted XML.
 *
 * @param   pTest               The test instance.
 * @param   pszFormat           The format string.
 * @param   va                  The format arguments.
 */
static void rtTestXmlOutputV(PRTTESTINT pTest, const char *pszFormat, va_list va)
{
    if (pTest->fXmlEnabled)
    {
        char *pszStr;
        ssize_t cchStr = RTStrAPrintfV(&pszStr, pszFormat, va);
        if (pszStr)
        {
            if (pTest->hXmlPipe != NIL_RTPIPE)
                RTPipeWriteBlocking(pTest->hXmlPipe, pszStr, cchStr,  NULL);
            if (pTest->hXmlFile != NIL_RTFILE)
                RTFileWrite(pTest->hXmlFile, pszStr, cchStr, NULL);
            RTStrFree(pszStr);
        }
    }
}


/**
 * Outputs the formatted XML.
 *
 * @param   pTest               The test instance.
 * @param   pszFormat           The format string.
 * @param   ...                 The format arguments.
 */
static void rtTestXmlOutput(PRTTESTINT pTest, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    rtTestXmlOutputV(pTest, pszFormat, va);
    va_end(va);
}


/**
 * Starts the XML stream.
 *
 * @param   pTest               The test instance.
 * @param   pszTest             The test name.
 */
static void rtTestXmlStart(PRTTESTINT pTest, const char *pszTest)
{
    pTest->cXmlElements = 0;
    if (pTest->fXmlEnabled)
    {
        rtTestXmlOutput(pTest, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        pTest->eXmlState = RTTESTINT::kXmlPos_ElementEnd;
        pTest->fXmlTopTestDone = !(pTest->fFlags & RTTEST_C_XML_DELAY_TOP_TEST) || pTest->fXmlOmitTopTest;
        if (pTest->fXmlTopTestDone && !pTest->fXmlOmitTopTest)
            rtTestXmlElemStart(pTest, "Test", "name=%RMas", pszTest);
    }
}

/**
 * Emit an XML element that doesn't have any value and instead ends immediately.
 *
 * The caller must own the instance lock.
 *
 * @param   pTest               The test instance.
 * @param   pszTag              The element tag.
 * @param   pszAttrFmt          The element attributes as a format string. Use
 *                              NULL if none.
 * @param   va                  Format string arguments.
 */
static void rtTestXmlElemV(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, va_list va)
{
    if (pTest->fXmlEnabled)
    {
        RTTIMESPEC  TimeSpec;
        RTTIME      Time;
        char        szTS[80];
        RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&TimeSpec)), szTS, sizeof(szTS));

        if (pTest->eXmlState != RTTESTINT::kXmlPos_ElementEnd)
            rtTestXmlOutput(pTest, "\n");

        if (!pszAttrFmt || !*pszAttrFmt)
            rtTestXmlOutput(pTest, "%*s<%s timestamp=%RMas/>\n",
                            pTest->cXmlElements * 2, "", pszTag, szTS);
        else
        {
            va_list va2;
            va_copy(va2, va);
            rtTestXmlOutput(pTest, "%*s<%s timestamp=%RMas %N/>\n",
                            pTest->cXmlElements * 2, "", pszTag, szTS, pszAttrFmt, &va2);
            va_end(va2);
        }
        pTest->eXmlState = RTTESTINT::kXmlPos_ElementEnd;
    }
}

/**
 * Wrapper around rtTestXmlElemV.
 */
static void rtTestXmlElem(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, ...)
{
    va_list va;
    va_start(va, pszAttrFmt);
    rtTestXmlElemV(pTest, pszTag, pszAttrFmt, va);
    va_end(va);
}


/**
 * Starts a new XML element.
 *
 * The caller must own the instance lock.
 *
 * @param   pTest               The test instance.
 * @param   pszTag              The element tag.
 * @param   pszAttrFmt          The element attributes as a format string. Use
 *                              NULL if none.
 * @param   va                  Format string arguments.
 */
static void rtTestXmlElemStartV(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, va_list va)
{
    /* Push it onto the stack. */
    size_t i = pTest->cXmlElements;
    AssertReturnVoid(i < RT_ELEMENTS(pTest->apszXmlElements));
    pTest->apszXmlElements[i] = pszTag;
    pTest->cXmlElements       = i + 1;

    if (pTest->fXmlEnabled)
    {
        RTTIMESPEC  TimeSpec;
        RTTIME      Time;
        char        szTS[80];
        RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&TimeSpec)), szTS, sizeof(szTS));

        if (pTest->eXmlState != RTTESTINT::kXmlPos_ElementEnd)
            rtTestXmlOutput(pTest, "\n");

        if (!pszAttrFmt || !*pszAttrFmt)
            rtTestXmlOutput(pTest, "%*s<%s timestamp=%RMas>",
                            i * 2, "", pszTag, szTS);
        else
        {
            va_list va2;
            va_copy(va2, va);
            rtTestXmlOutput(pTest, "%*s<%s timestamp=%RMas %N>",
                            i * 2, "", pszTag, szTS, pszAttrFmt, &va2);
            va_end(va2);
        }
        pTest->eXmlState = RTTESTINT::kXmlPos_ValueStart;
    }
}


/**
 * Wrapper around rtTestXmlElemStartV.
 */
static void rtTestXmlElemStart(PRTTESTINT pTest, const char *pszTag, const char *pszAttrFmt, ...)
{
    va_list va;
    va_start(va, pszAttrFmt);
    rtTestXmlElemStartV(pTest, pszTag, pszAttrFmt, va);
    va_end(va);
}


/**
 * Ends the current element.
 *
 * The caller must own the instance lock.
 *
 * @param   pTest               The test instance.
 * @param   pszTag              The tag we're ending (chiefly for sanity
 *                              checking).
 */
static void rtTestXmlElemEnd(PRTTESTINT pTest, const char *pszTag)
{
    /* pop the element */
    size_t i = pTest->cXmlElements;
    AssertReturnVoid(i > 0);
    i--;
    AssertReturnVoid(!strcmp(pszTag, pTest->apszXmlElements[i]));
    pTest->cXmlElements = i;

    /* Do the closing. */
    if (pTest->fXmlEnabled)
    {
        if (pTest->eXmlState == RTTESTINT::kXmlPos_ValueStart)
            rtTestXmlOutput(pTest, "\n%*s</%s>\n", i * 2, "", pszTag);
        else if (pTest->eXmlState == RTTESTINT::kXmlPos_ElementEnd)
            rtTestXmlOutput(pTest, "%*s</%s>\n", i * 2, "", pszTag);
        else
            rtTestXmlOutput(pTest, "</%s>\n", pszTag);
        pTest->eXmlState = RTTESTINT::kXmlPos_ElementEnd;
    }
}


/**
 * Ends the XML stream, closing all open elements.
 *
 * The caller must own the instance lock.
 *
 * @param   pTest               The test instance.
 */
static void rtTestXmlEnd(PRTTESTINT pTest)
{
    if (pTest->fXmlEnabled)
    {
        /*
         * Close all the elements and add the final TestEnd one to get a
         * final timestamp and some certainty that the XML is valid.
         */
        size_t i = pTest->cXmlElements;
        AssertReturnVoid(i > 0 || pTest->fXmlOmitTopTest || !pTest->fXmlTopTestDone);
        while (i-- > 1)
        {
            const char *pszTag = pTest->apszXmlElements[pTest->cXmlElements];
            if (pTest->eXmlState == RTTESTINT::kXmlPos_ValueStart)
                rtTestXmlOutput(pTest, "\n%*s</%s>\n", i * 2, "", pszTag);
            else if (pTest->eXmlState == RTTESTINT::kXmlPos_ElementEnd)
                rtTestXmlOutput(pTest, "%*s</%s>\n", i * 2, "", pszTag);
            else
                rtTestXmlOutput(pTest, "</%s>\n", pszTag);
            pTest->eXmlState = RTTESTINT::kXmlPos_ElementEnd;
        }

        if (!pTest->fXmlOmitTopTest && pTest->fXmlTopTestDone)
        {
            rtTestXmlElem(pTest, "End", "SubTests=\"%u\" SubTestsFailed=\"%u\" errors=\"%u\"",
                          pTest->cSubTests, pTest->cSubTestsFailed, pTest->cErrors);
            rtTestXmlOutput(pTest, "</Test>\n");
        }

        /*
         * Close the XML outputs.
         */
        if (pTest->hXmlPipe != NIL_RTPIPE)
        {
            RTPipeClose(pTest->hXmlPipe);
            pTest->hXmlPipe = NIL_RTPIPE;
        }
        if (pTest->hXmlFile != NIL_RTFILE)
        {
            RTFileClose(pTest->hXmlFile);
            pTest->hXmlFile = NIL_RTFILE;
        }
        pTest->fXmlEnabled = false;
        pTest->eXmlState = RTTESTINT::kXmlPos_ElementEnd;
    }
    pTest->cXmlElements = 0;
}

/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       User argument.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) rtTestPrintfOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    size_t      cch   = 0;
    PRTTESTINT  pTest = (PRTTESTINT)pvArg;
    if (cbChars)
    {
        do
        {
            /* insert prefix if at a newline. */
            if (pTest->fNewLine)
            {
                RTStrmWrite(pTest->pOutStrm, pTest->pszTest, pTest->cchTest);
                RTStrmWrite(pTest->pOutStrm, ": ", 2);
                cch += 2 + pTest->cchTest;
            }

            /* look for newline and write the stuff. */
            const char *pchEnd = (const char *)memchr(pachChars, '\n', cbChars);
            if (!pchEnd)
            {
                pTest->fNewLine = false;
                RTStrmWrite(pTest->pOutStrm, pachChars, cbChars);
                cch += cbChars;
                break;
            }

            pTest->fNewLine = true;
            size_t const cchPart = pchEnd - pachChars + 1;
            RTStrmWrite(pTest->pOutStrm, pachChars, cchPart);
            cch       += cchPart;
            pachChars += cchPart;
            cbChars   -= cchPart;
        } while (cbChars);
    }
    else
        RTStrmFlush(pTest->pOutStrm);
    return cch;
}


/**
 * Internal output worker.
 *
 * Caller takes the lock.
 *
 * @returns Number of chars printed.
 * @param   pTest           The test instance.
 * @param   pszFormat       The message.
 * @param   va              The arguments.
 */
static int rtTestPrintfV(PRTTESTINT pTest, const char *pszFormat, va_list va)
{
    return (int)RTStrFormatV(rtTestPrintfOutput, pTest, NULL, NULL, pszFormat, va);
}


/**
 * Internal output worker.
 *
 * Caller takes the lock.
 *
 * @returns Number of chars printed.
 * @param   pTest           The test instance.
 * @param   pszFormat       The message.
 * @param   ...             The arguments.
 */
static int rtTestPrintf(PRTTESTINT pTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = rtTestPrintfV(pTest, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Test vprintf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfNlV(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    RTCritSectEnter(&pTest->OutputLock);

    int cch = 0;
    if (enmLevel <= pTest->enmMaxLevel)
    {
        if (!pTest->fNewLine)
            cch += rtTestPrintf(pTest, "\n");
        cch += rtTestPrintfV(pTest, pszFormat, va);
    }

    RTCritSectLeave(&pTest->OutputLock);

    return cch;
}


/**
 * Test printf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintfNl(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestPrintfNlV(hTest, enmLevel, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Test vprintf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfV(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    RTCritSectEnter(&pTest->OutputLock);
    int cch = 0;
    if (enmLevel <= pTest->enmMaxLevel)
        cch += rtTestPrintfV(pTest, pszFormat, va);
    RTCritSectLeave(&pTest->OutputLock);

    return cch;
}


/**
 * Test printf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintf(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestPrintfV(hTest, enmLevel, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Prints the test banner.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestBanner(RTTEST hTest)
{
    return RTTestPrintfNl(hTest, RTTESTLVL_ALWAYS, "TESTING...\n");
}


/**
 * Prints the result of a sub-test if necessary.
 *
 * @returns Number of chars printed.
 * @param   pTest       The test instance.
 * @remarks Caller own the test Lock.
 */
static int rtTestSubTestReport(PRTTESTINT pTest)
{
    int cch = 0;
    if (    !pTest->fSubTestReported
        &&  pTest->pszSubTest)
    {
        pTest->fSubTestReported = true;
        uint32_t cErrors = ASMAtomicUoReadU32(&pTest->cErrors) - pTest->cSubTestAtErrors;
        if (!cErrors)
        {
            if (!pTest->fSubTestSkipped)
            {
                rtTestXmlElem(pTest, "Passed", NULL);
                rtTestXmlElemEnd(pTest, "Test");
                cch += RTTestPrintfNl(pTest, RTTESTLVL_SUB_TEST, "%-50s: PASSED\n", pTest->pszSubTest);
            }
            else
            {
                rtTestXmlElem(pTest, "Skipped", NULL);
                rtTestXmlElemEnd(pTest, "Test");
                cch += RTTestPrintfNl(pTest, RTTESTLVL_SUB_TEST, "%-50s: SKIPPED\n", pTest->pszSubTest);
            }
        }
        else
        {
            pTest->cSubTestsFailed++;
            rtTestXmlElem(pTest, "Failed", "errors=\"%u\"", cErrors);
            rtTestXmlElemEnd(pTest, "Test");
            cch += RTTestPrintfNl(pTest, RTTESTLVL_SUB_TEST, "%-50s: FAILED (%u errors)\n",
                                  pTest->pszSubTest, cErrors);
        }
    }
    return cch;
}


/**
 * RTTestSub and RTTestSubDone worker that cleans up the current (if any)
 * sub test.
 *
 * @returns Number of chars printed.
 * @param   pTest       The test instance.
 * @remarks Caller own the test Lock.
 */
static int rtTestSubCleanup(PRTTESTINT pTest)
{
    int cch = 0;
    if (pTest->pszSubTest)
    {
        cch += rtTestSubTestReport(pTest);

        RTStrFree((char *)pTest->pszSubTest);
        pTest->pszSubTest = NULL;
        pTest->fSubTestReported = true;
    }
    return cch;
}


/**
 * Summaries the test, destroys the test instance and return an exit code.
 *
 * @returns Test program exit code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(RTEXITCODE) RTTestSummaryAndDestroy(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, RTEXITCODE_FAILURE);

    RTCritSectEnter(&pTest->Lock);
    rtTestSubTestReport(pTest);
    RTCritSectLeave(&pTest->Lock);

    RTEXITCODE enmExitCode;
    if (!pTest->cErrors)
    {
        RTTestPrintfNl(hTest, RTTESTLVL_ALWAYS, "SUCCESS\n");
        enmExitCode = RTEXITCODE_SUCCESS;
    }
    else
    {
        RTTestPrintfNl(hTest, RTTESTLVL_ALWAYS, "FAILURE - %u errors\n", pTest->cErrors);
        enmExitCode = RTEXITCODE_FAILURE;
    }

    RTTestDestroy(pTest);
    return enmExitCode;
}


RTR3DECL(RTEXITCODE) RTTestSkipAndDestroyV(RTTEST hTest, const char *pszReasonFmt, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, RTEXITCODE_SKIPPED);

    RTCritSectEnter(&pTest->Lock);
    rtTestSubTestReport(pTest);
    RTCritSectLeave(&pTest->Lock);

    RTEXITCODE enmExitCode;
    if (!pTest->cErrors)
    {
        if (pszReasonFmt)
            RTTestPrintfNlV(hTest, RTTESTLVL_FAILURE, pszReasonFmt, va);
        RTTestPrintfNl(hTest, RTTESTLVL_ALWAYS, "SKIPPED\n");
        enmExitCode = RTEXITCODE_SKIPPED;
    }
    else
    {
        RTTestPrintfNl(hTest, RTTESTLVL_ALWAYS, "FAILURE - %u errors\n", pTest->cErrors);
        enmExitCode = RTEXITCODE_FAILURE;
    }

    RTTestDestroy(pTest);
    return enmExitCode;
}


RTR3DECL(RTEXITCODE) RTTestSkipAndDestroy(RTTEST hTest, const char *pszReasonFmt, ...)
{
    va_list va;
    va_start(va, pszReasonFmt);
    RTEXITCODE enmExitCode = RTTestSkipAndDestroyV(hTest, pszReasonFmt, va);
    va_end(va);
    return enmExitCode;
}


/**
 * Starts a sub-test.
 *
 * This will perform an implicit RTTestSubDone() call if that has not been done
 * since the last RTTestSub call.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszSubTest  The sub-test name
 */
RTR3DECL(int) RTTestSub(RTTEST hTest, const char *pszSubTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    RTCritSectEnter(&pTest->Lock);

    /* Cleanup, reporting if necessary previous sub test. */
    rtTestSubCleanup(pTest);

    /* Start new sub test. */
    pTest->cSubTests++;
    pTest->cSubTestAtErrors = ASMAtomicUoReadU32(&pTest->cErrors);
    pTest->pszSubTest = RTStrDup(pszSubTest);
    pTest->cchSubTest = strlen(pszSubTest);
    pTest->fSubTestSkipped  = false;
    pTest->fSubTestReported = false;

    int cch = 0;
    if (pTest->enmMaxLevel >= RTTESTLVL_DEBUG)
        cch = RTTestPrintfNl(hTest, RTTESTLVL_DEBUG, "debug: Starting sub-test '%s'\n", pszSubTest);

    if (!pTest->fXmlTopTestDone)
    {
        pTest->fXmlTopTestDone = true;
        rtTestXmlElemStart(pTest, "Test", "name=%RMas", pTest->pszTest);
    }

    rtTestXmlElemStart(pTest, "Test", "name=%RMas", pszSubTest);

    RTCritSectLeave(&pTest->Lock);

    return cch;
}


/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   ...             Arguments.
 */
RTR3DECL(int) RTTestSubF(RTTEST hTest, const char *pszSubTestFmt, ...)
{
    va_list va;
    va_start(va, pszSubTestFmt);
    int cch = RTTestSubV(hTest, pszSubTestFmt, va);
    va_end(va);
    return cch;
}


/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   ...             Arguments.
 */
RTR3DECL(int) RTTestSubV(RTTEST hTest, const char *pszSubTestFmt, va_list va)
{
    char *pszSubTest;
    RTStrAPrintfV(&pszSubTest, pszSubTestFmt, va);
    if (pszSubTest)
    {
        int cch = RTTestSub(hTest, pszSubTest);
        RTStrFree(pszSubTest);
        return cch;
    }
    return 0;
}


/**
 * Completes a sub-test.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestSubDone(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, VERR_INVALID_HANDLE);

    RTCritSectEnter(&pTest->Lock);
    int cch = rtTestSubCleanup(pTest);
    RTCritSectLeave(&pTest->Lock);

    return cch;
}

/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestPassedV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    AssertPtr(pszFormat);
    RTTEST_GET_VALID_RETURN_RC(pTest, VERR_INVALID_HANDLE);

    int cch = 0;
    if (pTest->enmMaxLevel >= RTTESTLVL_INFO)
    {
        va_list va2;
        va_copy(va2, va);

        RTCritSectEnter(&pTest->OutputLock);
        cch += rtTestPrintf(pTest, "%N\n", pszFormat, &va2);
        RTCritSectLeave(&pTest->OutputLock);

        va_end(va2);
    }

    return cch;
}


/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestPassed(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestPassedV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}


RTR3DECL(int) RTTestSkippedV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    AssertPtrNull(pszFormat);
    RTTEST_GET_VALID_RETURN_RC(pTest, VERR_INVALID_HANDLE);

    pTest->fSubTestSkipped = true;

    int cch = 0;
    if (pszFormat && *pszFormat && pTest->enmMaxLevel >= RTTESTLVL_INFO)
    {
        va_list va2;
        va_copy(va2, va);

        RTCritSectEnter(&pTest->OutputLock);
        cch += rtTestPrintf(pTest, "%N\n", pszFormat, &va2);
        RTCritSectLeave(&pTest->OutputLock);

        va_end(va2);
    }

    return cch;
}


RTR3DECL(int) RTTestSkipped(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestSkippedV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}



/**
 * Gets the unit name.
 *
 * @returns Unit name.
 * @param   enmUnit             The unit.
 */
static const char *rtTestUnitName(RTTESTUNIT enmUnit)
{
    switch (enmUnit)
    {
        case RTTESTUNIT_PCT:                    return "%";
        case RTTESTUNIT_BYTES:                  return "bytes";
        case RTTESTUNIT_BYTES_PER_SEC:          return "bytes/s";
        case RTTESTUNIT_KILOBYTES:              return "KB";
        case RTTESTUNIT_KILOBYTES_PER_SEC:      return "KB/s";
        case RTTESTUNIT_MEGABYTES:              return "MB";
        case RTTESTUNIT_MEGABYTES_PER_SEC:      return "MB/s";
        case RTTESTUNIT_PACKETS:                return "packets";
        case RTTESTUNIT_PACKETS_PER_SEC:        return "packets/s";
        case RTTESTUNIT_FRAMES:                 return "frames";
        case RTTESTUNIT_FRAMES_PER_SEC:         return "frames/s";
        case RTTESTUNIT_OCCURRENCES:            return "occurrences";
        case RTTESTUNIT_OCCURRENCES_PER_SEC:    return "occurrences/s";
        case RTTESTUNIT_ROUND_TRIP:             return "roundtrips";
        case RTTESTUNIT_CALLS:                  return "calls";
        case RTTESTUNIT_CALLS_PER_SEC:          return "calls/s";
        case RTTESTUNIT_SECS:                   return "s";
        case RTTESTUNIT_MS:                     return "ms";
        case RTTESTUNIT_NS:                     return "ns";
        case RTTESTUNIT_NS_PER_CALL:            return "ns/call";
        case RTTESTUNIT_NS_PER_FRAME:           return "ns/frame";
        case RTTESTUNIT_NS_PER_OCCURRENCE:      return "ns/occurrence";
        case RTTESTUNIT_NS_PER_PACKET:          return "ns/packet";
        case RTTESTUNIT_NS_PER_ROUND_TRIP:      return "ns/roundtrip";
        case RTTESTUNIT_INSTRS:                 return "ins";
        case RTTESTUNIT_INSTRS_PER_SEC:         return "ins/sec";
        case RTTESTUNIT_NONE:                   return "";
        case RTTESTUNIT_PP1K:                   return "pp1k";
        case RTTESTUNIT_PP10K:                  return "pp10k";
        case RTTESTUNIT_PPM:                    return "ppm";
        case RTTESTUNIT_PPB:                    return "ppb";

        /* No default so gcc helps us keep this up to date. */
        case RTTESTUNIT_INVALID:
        case RTTESTUNIT_END:
            break;
    }
    AssertMsgFailed(("%d\n", enmUnit));
    return "unknown";
}


RTR3DECL(int) RTTestValue(RTTEST hTest, const char *pszName, uint64_t u64Value, RTTESTUNIT enmUnit)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);

    const char *pszUnit = rtTestUnitName(enmUnit);

    RTCritSectEnter(&pTest->Lock);
    rtTestXmlElem(pTest, "Value", "name=%RMas unit=%RMas value=\"%llu\"", pszName, pszUnit, u64Value);
    RTCritSectLeave(&pTest->Lock);

    RTCritSectEnter(&pTest->OutputLock);
    rtTestPrintf(pTest, "  %-48s: %'16llu %s\n", pszName, u64Value, pszUnit);
    RTCritSectLeave(&pTest->OutputLock);

    return VINF_SUCCESS;
}


RTR3DECL(int) RTTestValueF(RTTEST hTest, uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTTestValueV(hTest, u64Value, enmUnit, pszNameFmt, va);
    va_end(va);
    return rc;
}


RTR3DECL(int) RTTestValueV(RTTEST hTest, uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, va_list va)
{
    char *pszName;
    RTStrAPrintfV(&pszName, pszNameFmt, va);
    if (!pszName)
        return VERR_NO_MEMORY;
    int rc = RTTestValue(hTest, pszName, u64Value, enmUnit);
    RTStrFree(pszName);
    return rc;
}


/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestErrorInc(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);

    ASMAtomicIncU32(&pTest->cErrors);

    return VINF_SUCCESS;
}



/**
 * Get the current error count.
 *
 * @returns The error counter, UINT32_MAX if no valid test handle.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(uint32_t) RTTestErrorCount(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, UINT32_MAX);

    return ASMAtomicReadU32(&pTest->cErrors);
}


RTR3DECL(uint32_t) RTTestSubErrorCount(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, UINT32_MAX);

    return ASMAtomicReadU32(&pTest->cErrors) - pTest->cSubTestAtErrors;
}


/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestFailedV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, VERR_INVALID_HANDLE);

    RTTestErrorInc(pTest);

    int cch = 0;
    if (pTest->enmMaxLevel >= RTTESTLVL_FAILURE)
    {
        va_list va2;
        va_copy(va2, va);

        const char *pszEnd = strchr(pszFormat, '\0');
        bool fHasNewLine = pszFormat != pszEnd
                        && pszEnd[-1] == '\n';

        RTCritSectEnter(&pTest->OutputLock);
        cch += rtTestPrintf(pTest, fHasNewLine ? "%N" : "%N\n", pszFormat, &va2);
        RTCritSectLeave(&pTest->OutputLock);

        va_end(va2);
    }

    return cch;
}


/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestFailed(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestFailedV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Same as RTTestPrintfV with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestFailureDetailsV(RTTEST hTest, const char *pszFormat, va_list va)
{
    return RTTestPrintfV(hTest, RTTESTLVL_FAILURE, pszFormat, va);
}


/**
 * Same as RTTestPrintf with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestFailureDetails(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = RTTestFailureDetailsV(hTest, pszFormat, va);
    va_end(va);
    return cch;
}


RTR3DECL(int) RTTestDisableAssertions(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);

    uint32_t cTimes = ASMAtomicIncU32(&pTest->cAssertionsDisabledAndQuieted);
    if (cTimes >= 2 && cTimes <= 8)
        return VINF_SUCCESS;
    if (cTimes > 8)
    {
        RTAssertSetMayPanic(pTest->fAssertSavedMayPanic);
        RTAssertSetQuiet(pTest->fAssertSavedQuiet);
        Assert(cTimes <= 8);
    }
    pTest->fAssertSavedMayPanic = RTAssertSetMayPanic(false);
    pTest->fAssertSavedQuiet    = RTAssertSetQuiet(true);
    return VINF_SUCCESS;
}


RTR3DECL(int) RTTestRestoreAssertions(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);

    uint32_t cTimes = ASMAtomicDecU32(&pTest->cAssertionsDisabledAndQuieted);
    if (cTimes == 0)
    {
        RTAssertSetMayPanic(pTest->fAssertSavedMayPanic);
        RTAssertSetQuiet(pTest->fAssertSavedQuiet);
    }
    else
        AssertStmt(cTimes < UINT32_MAX / 2, ASMAtomicIncU32(&pTest->cAssertionsDisabledAndQuieted));
    return VINF_SUCCESS;
}

