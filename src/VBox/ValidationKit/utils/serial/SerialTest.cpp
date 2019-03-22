/* $Id$ */
/** @file
 * SerialTest - Serial port testing utility.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
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
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/serialport.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** Pointer to the serial test data instance. */
typedef struct SERIALTEST *PSERIALTEST;

/**
 * Test callback function.
 *
 * @returns IPRT status code.
 * @param   pSerialTest         The serial test instance data.
 */
typedef DECLCALLBACK(int) FNSERIALTESTRUN(PSERIALTEST pSerialTest);
/** Pointer to the serial test callback. */
typedef FNSERIALTESTRUN *PFNSERIALTESTRUN;


/**
 * The serial test instance data.
 */
typedef struct SERIALTEST
{
    /** The assigned test handle. */
    RTTEST                      hTest;
    /** The assigned serial port. */
    RTSERIALPORT                hSerialPort;
    /** The currently active config. */
    PCRTSERIALPORTCFG           pSerialCfg;
} SERIALTEST;


/**
 * Test descriptor.
 */
typedef struct SERIALTESTDESC
{
    /** Test ID. */
    const char                  *pszId;
    /** Test description. */
    const char                  *pszDesc;
    /** Test run callback. */
    PFNSERIALTESTRUN            pfnRun;
} SERIALTESTDESC;
/** Pointer to a test descriptor. */
typedef SERIALTESTDESC *PSERIALTESTDESC;
/** Pointer to a constant test descriptor. */
typedef const SERIALTESTDESC *PCSERIALTESTDESC;


/**
 * TX/RX buffer containing a simple counter.
 */
typedef struct SERIALTESTTXRXBUFCNT
{
    /** The current counter value. */
    uint32_t                    iCnt;
    /** Number of bytes left to receive/transmit. */
    size_t                      cbTxRxLeft;
    /** The offset into the buffer to receive to/send from. */
    size_t                      offBuf;
    /** Maximum size to send/receive before processing is needed again. */
    size_t                      cbTxRxMax;
    /** The data buffer. */
    uint8_t                     abBuf[_1K];
} SERIALTESTTXRXBUFCNT;
/** Pointer to a TX/RX buffer. */
typedef SERIALTESTTXRXBUFCNT *PSERIALTESTTXRXBUFCNT;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    {"--device",           'd', RTGETOPT_REQ_STRING },
    {"--baudrate",         'b', RTGETOPT_REQ_UINT32 },
    {"--parity",           'p', RTGETOPT_REQ_STRING },
    {"--databits",         'c', RTGETOPT_REQ_UINT32 },
    {"--stopbits",         's', RTGETOPT_REQ_STRING },
    {"--loopbackdevice",   'l', RTGETOPT_REQ_STRING },
    {"--tests",            't', RTGETOPT_REQ_STRING },
    {"--txbytes",          'x', RTGETOPT_REQ_UINT32 },
    {"--help",             'h', RTGETOPT_REQ_NOTHING}
};


static DECLCALLBACK(int) serialTestRunReadWrite(PSERIALTEST pSerialTest);
static DECLCALLBACK(int) serialTestRunWrite(PSERIALTEST pSerialTest);

/** Implemented tests. */
static const SERIALTESTDESC g_aSerialTests[] =
{
    {"readwrite", "Simple Read/Write test on the same serial port",       serialTestRunReadWrite },
    {"write",     "Simple write test (verification done somewhere else)", serialTestRunWrite },
};

/** The test handle. */
static RTTEST          g_hTest               = NIL_RTTEST;
/** The serial port handle. */
static RTSERIALPORT    g_hSerialPort         = NIL_RTSERIALPORT;
/** The loopback serial port handle if configured. */
static RTSERIALPORT    g_hSerialPortLoopback = NIL_RTSERIALPORT;
/** Number of bytes to transmit for read/write tests. */
static size_t          g_cbTx                = _1M;
/** The config used. */
static RTSERIALPORTCFG g_SerialPortCfg =
{
    /* uBaudRate */
    115200,
    /* enmParity */
    RTSERIALPORTPARITY_NONE,
    /* enmDataBitCount */
    RTSERIALPORTDATABITS_8BITS,
    /* enmStopBitCount */
    RTSERIALPORTSTOPBITS_ONE
};


/**
 * Initializes a TX buffer.
 *
 * @returns nothing.
 * @param   pSerBuf             The serial buffer to initialize.
 * @param   cbTx                Maximum number of bytes to transmit.
 */
static void serialTestTxBufInit(PSERIALTESTTXRXBUFCNT pSerBuf, size_t cbTx)
{
    pSerBuf->iCnt      = 0;
    pSerBuf->offBuf    = 0;
    pSerBuf->cbTxRxMax = 0;
    pSerBuf->cbTxRxLeft = cbTx;
    RT_ZERO(pSerBuf->abBuf);
}


/**
 * Initializes a RX buffer.
 *
 * @returns nothing.
 * @param   pSerBuf             The serial buffer to initialize.
 * @param   cbRx                Maximum number of bytes to receive.
 */
static void serialTestRxBufInit(PSERIALTESTTXRXBUFCNT pSerBuf, size_t cbRx)
{
    pSerBuf->iCnt      = 0;
    pSerBuf->offBuf    = 0;
    pSerBuf->cbTxRxMax = sizeof(pSerBuf->abBuf);
    pSerBuf->cbTxRxLeft = cbRx;
    RT_ZERO(pSerBuf->abBuf);
}


/**
 * Prepares the given TX buffer with data for sending it out.
 *
 * @returns nothing.
 * @param   pSerBuf             The TX buffer pointer.
 */
static void serialTestTxBufPrepare(PSERIALTESTTXRXBUFCNT pSerBuf)
{
    /* Move the data to the front to make room at the end to fill. */
    if (pSerBuf->offBuf)
    {
        memmove(&pSerBuf->abBuf[0], &pSerBuf->abBuf[pSerBuf->offBuf], sizeof(pSerBuf->abBuf) - pSerBuf->offBuf);
        pSerBuf->offBuf = 0;
    }

    /* Fill up with data. */
    uint32_t offData = 0;
    while (pSerBuf->cbTxRxMax + sizeof(uint32_t) <= sizeof(pSerBuf->abBuf))
    {
        pSerBuf->iCnt++;
        *(uint32_t *)&pSerBuf->abBuf[pSerBuf->offBuf + offData] = pSerBuf->iCnt;
        pSerBuf->cbTxRxMax += sizeof(uint32_t);
        offData            += sizeof(uint32_t);
    }
}


/**
 * Sends a new batch of data from the TX buffer preapring new data if required.
 *
 * @returns IPRT status code.
 * @param   hSerialPort         The serial port handle to send the data to.
 * @param   pSerBuf             The TX buffer pointer.
 */
static int serialTestTxBufSend(RTSERIALPORT hSerialPort, PSERIALTESTTXRXBUFCNT pSerBuf)
{
    int rc = VINF_SUCCESS;

    if (pSerBuf->cbTxRxLeft)
    {
        if (!pSerBuf->cbTxRxMax)
            serialTestTxBufPrepare(pSerBuf);

        size_t cbToWrite = RT_MIN(pSerBuf->cbTxRxMax, pSerBuf->cbTxRxLeft);
        size_t cbWritten = 0;
        rc = RTSerialPortWriteNB(hSerialPort, &pSerBuf->abBuf[pSerBuf->offBuf], cbToWrite, &cbWritten);
        if (RT_SUCCESS(rc))
        {
            pSerBuf->cbTxRxMax  -= cbWritten;
            pSerBuf->offBuf     += cbWritten;
            pSerBuf->cbTxRxLeft -= cbWritten;
        }
    }

    return rc;
}


/**
 * Receives dat from the given serial port into the supplied RX buffer and does some validity checking.
 *
 * @returns IPRT status code.
 * @param   hSerialPort         The serial port handle to receive data from.
 * @param   pSerBuf             The RX buffer pointer.
 */
static int serialTestRxBufRecv(RTSERIALPORT hSerialPort, PSERIALTESTTXRXBUFCNT pSerBuf)
{
    int rc = VINF_SUCCESS;

    if (pSerBuf->cbTxRxLeft)
    {
        size_t cbToRead = RT_MIN(pSerBuf->cbTxRxMax, pSerBuf->cbTxRxLeft);
        size_t cbRead = 0;
        rc = RTSerialPortReadNB(hSerialPort, &pSerBuf->abBuf[pSerBuf->offBuf], cbToRead, &cbRead);
        if (RT_SUCCESS(rc))
        {
            pSerBuf->offBuf     += cbRead;
            pSerBuf->cbTxRxMax  -= cbRead;
            pSerBuf->cbTxRxLeft -= cbRead;
        }
    }

    return rc;
}


/**
 * Verifies the data in the given RX buffer for correct transmission.
 *
 * @returns nothing.
 * @param   hTest               The test handle to report errors to.
 * @param   pSerBuf             The RX buffer pointer.
 * @param   iCntTx              The current TX counter value the RX buffer should never get ahead of.
 */
static void serialTestRxBufVerify(RTTEST hTest, PSERIALTESTTXRXBUFCNT pSerBuf, uint32_t iCntTx)
{
    uint32_t offRx = 0;

    while (offRx + sizeof(uint32_t) < pSerBuf->offBuf)
    {
        uint32_t u32Val = *(uint32_t *)&pSerBuf->abBuf[offRx];
        offRx += sizeof(uint32_t);

        if (RT_UNLIKELY(u32Val != ++pSerBuf->iCnt))
            RTTestFailed(hTest, "Data corruption/loss detected, expected counter value %u got %u\n",
                         pSerBuf->iCnt, u32Val);
    }

    if (RT_UNLIKELY(pSerBuf->iCnt > iCntTx))
        RTTestFailed(hTest, "Overtook the send buffer, expected maximum counter value %u got %u\n",
                     iCntTx, pSerBuf->iCnt);

    /* Remove processed data from the buffer and move the rest to the front. */
    if (offRx)
    {
        memmove(&pSerBuf->abBuf[0], &pSerBuf->abBuf[offRx], sizeof(pSerBuf->abBuf) - offRx);
        pSerBuf->offBuf    -= offRx;
        pSerBuf->cbTxRxMax += offRx;
    }
}


/**
 * Runs a simple read/write test.
 *
 * @returns IPRT status code.
 * @param   pSerialTest         The serial test configuration.
 */
static DECLCALLBACK(int) serialTestRunReadWrite(PSERIALTEST pSerialTest)
{
    uint64_t tsStart = RTTimeMilliTS();
    SERIALTESTTXRXBUFCNT SerBufTx;
    SERIALTESTTXRXBUFCNT SerBufRx;

    serialTestTxBufInit(&SerBufTx, g_cbTx);
    serialTestRxBufInit(&SerBufRx, g_cbTx);

    int rc = serialTestTxBufSend(pSerialTest->hSerialPort, &SerBufTx);
    while (   RT_SUCCESS(rc)
           && (   SerBufTx.cbTxRxLeft
               || SerBufRx.cbTxRxLeft))
    {
        uint32_t fEvts = 0;
        uint32_t fEvtsQuery = 0;
        if (SerBufTx.cbTxRxLeft)
            fEvtsQuery |= RTSERIALPORT_EVT_F_DATA_TX;
        if (SerBufRx.cbTxRxLeft)
            fEvtsQuery |= RTSERIALPORT_EVT_F_DATA_RX;

        rc = RTSerialPortEvtPoll(pSerialTest->hSerialPort, fEvtsQuery, &fEvts, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            break;

        if (fEvts & RTSERIALPORT_EVT_F_DATA_RX)
        {
            rc = serialTestRxBufRecv(pSerialTest->hSerialPort, &SerBufRx);
            if (RT_FAILURE(rc))
                break;

            serialTestRxBufVerify(pSerialTest->hTest, &SerBufRx, SerBufTx.iCnt);
        }
        if (   RT_SUCCESS(rc)
            && (fEvts & RTSERIALPORT_EVT_F_DATA_TX))
            rc = serialTestTxBufSend(pSerialTest->hSerialPort, &SerBufTx);
    }

    uint64_t tsRuntime = RTTimeMilliTS() - tsStart;
    tsRuntime /= 1000; /* Seconds */
    RTTestValue(pSerialTest->hTest, "Throughput", g_cbTx / tsRuntime, RTTESTUNIT_BYTES_PER_SEC);

    return rc;
}


/**
 * Runs a simple write test without doing any verification.
 *
 * @returns IPRT status code.
 * @param   pSerialTest         The serial test configuration.
 */
static DECLCALLBACK(int) serialTestRunWrite(PSERIALTEST pSerialTest)
{
    uint64_t tsStart = RTTimeMilliTS();
    SERIALTESTTXRXBUFCNT SerBufTx;

    serialTestTxBufInit(&SerBufTx, g_cbTx);

    int rc = serialTestTxBufSend(pSerialTest->hSerialPort, &SerBufTx);
    while (   RT_SUCCESS(rc)
           && SerBufTx.cbTxRxLeft)
    {
        uint32_t fEvts = 0;

        rc = RTSerialPortEvtPoll(pSerialTest->hSerialPort, RTSERIALPORT_EVT_F_DATA_TX, &fEvts, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            break;

        if (fEvts & RTSERIALPORT_EVT_F_DATA_TX)
            rc = serialTestTxBufSend(pSerialTest->hSerialPort, &SerBufTx);
    }

    uint64_t tsRuntime = RTTimeMilliTS() - tsStart;
    tsRuntime /= 1000; /* Seconds */
    RTTestValue(pSerialTest->hTest, "Throughput", g_cbTx / tsRuntime, RTTESTUNIT_BYTES_PER_SEC);

    return rc;
}


/**
 * Returns an array of test descriptors get from the given string.
 *
 * @returns Pointer to the array of test descriptors.
 * @param   pszTests            The string containing the tests separated with ':'.
 */
static PSERIALTESTDESC serialTestSelectFromCmdLine(const char *pszTests)
{
    size_t cTests = 1;

    const char *pszNext = strchr(pszTests, ':');
    while (pszNext)
    {
        pszNext++;
        cTests++;
        pszNext = strchr(pszNext, ':');
    }

    PSERIALTESTDESC paTests = (PSERIALTESTDESC)RTMemAllocZ((cTests + 1) * sizeof(SERIALTESTDESC));
    if (RT_LIKELY(paTests))
    {
        uint32_t iTest = 0;

        pszNext = strchr(pszTests, ':');
        while (pszNext)
        {
            bool fFound = false;

            pszNext++; /* Skip : character. */

            for (unsigned i = 0; i < RT_ELEMENTS(g_aSerialTests); i++)
            {
                if (!RTStrNICmp(pszTests, g_aSerialTests[i].pszId, pszNext - pszTests - 1))
                {
                    memcpy(&paTests[iTest], &g_aSerialTests[i], sizeof(SERIALTESTDESC));
                    fFound = true;
                    break;
                }
            }

            if (RT_UNLIKELY(!fFound))
            {
                RTPrintf("Testcase \"%.*s\" not known\n", pszNext - pszTests - 1, pszTests);
                RTMemFree(paTests);
                return NULL;
            }

            pszTests = pszNext;
            pszNext = strchr(pszTests, ':');
        }

        /* Fill last descriptor. */
        bool fFound = false;
        for (unsigned i = 0; i < RT_ELEMENTS(g_aSerialTests); i++)
        {
            if (!RTStrICmp(pszTests, g_aSerialTests[i].pszId))
            {
                memcpy(&paTests[iTest], &g_aSerialTests[i], sizeof(SERIALTESTDESC));
                fFound = true;
                break;
            }
        }

        if (RT_UNLIKELY(!fFound))
        {
            RTPrintf("Testcase \"%s\" not known\n", pszTests);
            RTMemFree(paTests);
            paTests = NULL;
        }
    }
    else
        RTPrintf("Failed to allocate test descriptors for %u selected tests\n", cTests);

    return paTests;
}


/**
 * Shows tool usage text.
 */
static void serialTestUsage(PRTSTREAM pStrm)
{
    char szExec[RTPATH_MAX];
    RTStrmPrintf(pStrm, "usage: %s [options]\n",
                 RTPathFilename(RTProcGetExecutablePath(szExec, sizeof(szExec))));
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "options: \n");


    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdOptions); i++)
    {
        const char *pszHelp;
        switch (g_aCmdOptions[i].iShort)
        {
            case 'h':
                pszHelp = "Displays this help and exit";
                break;
            case 'd':
                pszHelp = "Use the specified serial port device";
                break;
            case 'b':
                pszHelp = "Use the given baudrate";
                break;
            case 'p':
                pszHelp = "Use the given parity, valid modes are: none, even, odd, mark, space";
                break;
            case 'c':
                pszHelp = "Use the given data bitcount, valid are: 5, 6, 7, 8";
                break;
            case 's':
                pszHelp = "Use the given stop bitcount, valid are: 1, 1.5, 2";
                break;
            case 'l':
                pszHelp = "Use the given serial port device as the loopback device";
                break;
            case 't':
                pszHelp = "The tests to run separated by ':'";
                break;
            case 'x':
                pszHelp = "Number of bytes to transmit during read/write tests";
                break;
            default:
                pszHelp = "Option undocumented";
                break;
        }
        char szOpt[256];
        RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
        RTStrmPrintf(pStrm, "  %-30s%s\n", szOpt, pszHelp);
    }
}


int main(int argc, char *argv[])
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("SerialTest", &g_hTest);
    if (rc)
        return rc;

    /*
     * Default values.
     */
    const char *pszDevice = NULL;
    const char *pszDeviceLoopback = NULL;
    PSERIALTESTDESC paTests = NULL;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'h':
                serialTestUsage(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case 'd':
                pszDevice = ValueUnion.psz;
                break;
            case 'l':
                pszDeviceLoopback = ValueUnion.psz;
                break;
            case 'b':
                g_SerialPortCfg.uBaudRate = ValueUnion.u32;
                break;
            case 'p':
                if (!RTStrICmp(ValueUnion.psz, "none"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_NONE;
                else if (!RTStrICmp(ValueUnion.psz, "even"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_EVEN;
                else if (!RTStrICmp(ValueUnion.psz, "odd"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_ODD;
                else if (!RTStrICmp(ValueUnion.psz, "mark"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_MARK;
                else if (!RTStrICmp(ValueUnion.psz, "space"))
                    g_SerialPortCfg.enmParity = RTSERIALPORTPARITY_SPACE;
                else
                {
                    RTPrintf("Unknown parity \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            case 'c':
                if (!RTStrICmp(ValueUnion.psz, "5"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_5BITS;
                else if (!RTStrICmp(ValueUnion.psz, "6"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_6BITS;
                else if (!RTStrICmp(ValueUnion.psz, "7"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_7BITS;
                else if (!RTStrICmp(ValueUnion.psz, "8"))
                    g_SerialPortCfg.enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
                else
                {
                    RTPrintf("Unknown data bitcount \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            case 's':
                if (!RTStrICmp(ValueUnion.psz, "1"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;
                else if (!RTStrICmp(ValueUnion.psz, "1.5"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONEPOINTFIVE;
                else if (!RTStrICmp(ValueUnion.psz, "2"))
                    g_SerialPortCfg.enmStopBitCount = RTSERIALPORTSTOPBITS_TWO;
                else
                {
                    RTPrintf("Unknown stop bitcount \"%s\" given\n", ValueUnion.psz);
                    return RTEXITCODE_FAILURE;
                }
                break;
            case 't':
                paTests = serialTestSelectFromCmdLine(ValueUnion.psz);
                if (!paTests)
                    return RTEXITCODE_FAILURE;
                break;
            case 'x':
                g_cbTx = ValueUnion.u32;
                break;
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    if (!paTests)
    {
        /* Select all. */
        paTests = (PSERIALTESTDESC)RTMemAllocZ((RT_ELEMENTS(g_aSerialTests) + 1) * sizeof(SERIALTESTDESC));
        if (RT_UNLIKELY(!paTests))
        {
            RTPrintf("Failed to allocate memory for test descriptors\n");
            return RTEXITCODE_FAILURE;
        }
        memcpy(paTests, &g_aSerialTests[0], RT_ELEMENTS(g_aSerialTests) * sizeof(SERIALTESTDESC));
    }

    /*
     * Start testing.
     */
    RTTestBanner(g_hTest);

    if (pszDevice)
    {
        uint32_t fFlags =   RTSERIALPORT_OPEN_F_READ
                          | RTSERIALPORT_OPEN_F_WRITE
                          | RTSERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING;

        RTTestSub(g_hTest, "Opening device");
        rc = RTSerialPortOpen(&g_hSerialPort, pszDevice, fFlags);
        if (RT_SUCCESS(rc))
        {
            if (pszDeviceLoopback)
            {
                RTTestSub(g_hTest, "Opening loopback device");
                rc = RTSerialPortOpen(&g_hSerialPortLoopback, pszDeviceLoopback, fFlags);
                if (RT_FAILURE(rc))
                    RTTestFailed(g_hTest, "Opening loopback device \"%s\" failed with %Rrc\n", pszDevice, rc);
            }

            if (RT_SUCCESS(rc))
            {
                RTTestSub(g_hTest, "Setting serial port configuration");

                rc = RTSerialPortCfgSet(g_hSerialPort, &g_SerialPortCfg ,NULL);
                if (RT_SUCCESS(rc))
                {
                    if (pszDeviceLoopback)
                    {
                        RTTestSub(g_hTest, "Setting serial port configuration for loopback device");
                        rc = RTSerialPortCfgSet(g_hSerialPortLoopback, &g_SerialPortCfg, NULL);
                        if (RT_FAILURE(rc))
                            RTTestFailed(g_hTest, "Setting configuration of loopback device \"%s\" failed with %Rrc\n", pszDevice, rc);
                    }

                    if (RT_SUCCESS(rc))
                    {
                        SERIALTEST Test;
                        PSERIALTESTDESC pTest = &paTests[0];

                        Test.hTest       = g_hTest;
                        Test.hSerialPort = g_hSerialPort;
                        Test.pSerialCfg  = &g_SerialPortCfg;

                        while (pTest->pszId)
                        {
                            RTTestSub(g_hTest, pTest->pszDesc);
                            rc = pTest->pfnRun(&Test);
                            if (RT_FAILURE(rc))
                                RTTestFailed(g_hTest, "Running test \"%s\" failed with %Rrc\n", pTest->pszId, rc);

                            RTTestSubDone(g_hTest);
                            pTest++;
                        }
                    }
                }
                else
                    RTTestFailed(g_hTest, "Setting configuration of device \"%s\" failed with %Rrc\n", pszDevice, rc);

                RTSerialPortClose(g_hSerialPort);
            }
        }
        else
            RTTestFailed(g_hTest, "Opening device \"%s\" failed with %Rrc\n", pszDevice, rc);
    }
    else
        RTTestFailed(g_hTest, "No device given on command line\n");

    RTMemFree(paTests);
    RTEXITCODE rcExit = RTTestSummaryAndDestroy(g_hTest);
    return rcExit;
}

