/* $Id$ */
/** @file
 * IPRT - Serial Port API, Windows Implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
#include <iprt/serialport.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include "internal/magics.h"

#include <iprt/win/windows.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal serial port state.
 */
typedef struct RTSERIALPORTINTERNAL
{
    /** Magic value (RTSERIALPORT_MAGIC). */
    uint32_t            u32Magic;
    /** Flags given while opening the serial port. */
    uint32_t            fOpenFlags;
    /** The device handle. */
    HANDLE              hDev;
    /** The overlapped I/O structure. */
    OVERLAPPED          Overlapped;
    /** The event handle to wait on for the overlapped I/O operations of the device. */
    HANDLE              hEvtDev;
    /** The event handle to wait on for waking up waiting threads externally. */
    HANDLE              hEvtIntr;
    /** Events currently waited for. */
    uint32_t            fEvtMask;
    /** Flag whether a write is currently pending. */
    bool                fWritePending;
    /** Bounce buffer for writes. */
    uint8_t            *pbBounceBuf;
    /** Amount of used buffer space. */
    size_t              cbBounceBufUsed;
    /** Amount of allocated buffer space. */
    size_t              cbBounceBufAlloc;
    /** The current active port config. */
    DCB                 PortCfg;
} RTSERIALPORTINTERNAL;
/** Pointer to the internal serial port state. */
typedef RTSERIALPORTINTERNAL *PRTSERIALPORTINTERNAL;



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The pipe buffer size we prefer. */
#define RTSERIALPORT_NT_SIZE      _32K



/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Updatest the current event mask to wait for.
 *
 * @returns IPRT status code.
 * @param   pThis                   The internal serial port instance data.
 * @param   fEvtMask                The new event mask to change to.
 */
static int rtSerialPortWinUpdateEvtMask(PRTSERIALPORTINTERNAL pThis, uint32_t fEvtMask)
{
    DWORD dwEvtMask = 0;

    if (fEvtMask & RTSERIALPORT_EVT_F_DATA_RX)
        dwEvtMask |= EV_RXCHAR;
    if (fEvtMask & RTSERIALPORT_EVT_F_DATA_TX)
        dwEvtMask |= EV_TXEMPTY;
    if (fEvtMask & RTSERIALPORT_EVT_F_BREAK_DETECTED)
        dwEvtMask |= EV_BREAK;
    if (fEvtMask & RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED)
        dwEvtMask |= EV_CTS | EV_DSR | EV_RING | EV_RLSD;

    int rc = VINF_SUCCESS;
    if (!SetCommMask(pThis->hDev, dwEvtMask))
        rc = RTErrConvertFromWin32(GetLastError());
    else
        pThis->fEvtMask = fEvtMask;

    return rc;
}


/**
 * Tries to set the default config on the given serial port.
 *
 * @returns IPRT status code.
 * @param   pThis                   The internal serial port instance data.
 */
static int rtSerialPortSetDefaultCfg(PRTSERIALPORTINTERNAL pThis)
{
    if (!PurgeComm(pThis->hDev, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR))
        return RTErrConvertFromWin32(GetLastError());

    pThis->PortCfg.DCBlength = sizeof(pThis->PortCfg);
    if (!GetCommState(pThis->hDev, &pThis->PortCfg))
        return RTErrConvertFromWin32(GetLastError());

    pThis->PortCfg.BaudRate    = CBR_9600;
    pThis->PortCfg.fBinary     = TRUE;
    pThis->PortCfg.fParity     = TRUE;
    pThis->PortCfg.fDtrControl = DTR_CONTROL_DISABLE;
    pThis->PortCfg.ByteSize    = 8;
    pThis->PortCfg.Parity      = NOPARITY;

    int rc = VINF_SUCCESS;
    if (!SetCommState(pThis->hDev, &pThis->PortCfg))
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


/**
 * Common worker for handling I/O completion.
 *
 * This is used by RTSerialPortClose, RTSerialPortWrite and RTPipeSerialPortNB.
 *
 * @returns IPRT status code.
 * @param   pThis               The pipe instance handle.
 */
static int rtSerialPortWriteCheckCompletion(PRTSERIALPORTINTERNAL pThis)
{
    int rc = VINF_SUCCESS;
    DWORD dwRc = WaitForSingleObject(pThis->Overlapped.hEvent, 0);
    if (dwRc == WAIT_OBJECT_0)
    {
        DWORD cbWritten = 0;
        if (GetOverlappedResult(pThis->hDev, &pThis->Overlapped, &cbWritten, TRUE))
        {
            for (;;)
            {
                if (cbWritten >= pThis->cbBounceBufUsed)
                {
                    pThis->fWritePending = false;
                    rc = VINF_SUCCESS;
                    break;
                }

                /* resubmit the remainder of the buffer - can this actually happen? */
                memmove(&pThis->pbBounceBuf[0], &pThis->pbBounceBuf[cbWritten], pThis->cbBounceBufUsed - cbWritten);
                rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                if (!WriteFile(pThis->hDev, pThis->pbBounceBuf, (DWORD)pThis->cbBounceBufUsed,
                               &cbWritten, &pThis->Overlapped))
                {
                    if (GetLastError() == ERROR_IO_PENDING)
                        rc = VINF_TRY_AGAIN;
                    else
                    {
                        pThis->fWritePending = false;
                        rc = RTErrConvertFromWin32(GetLastError());
                    }
                    break;
                }
                Assert(cbWritten > 0);
            }
        }
        else
        {
            pThis->fWritePending = false;
            rc = RTErrConvertFromWin32(GetLastError());
        }
    }
    else if (dwRc == WAIT_TIMEOUT)
        rc = VINF_TRY_AGAIN;
    else
    {
        pThis->fWritePending = false;
        if (dwRc == WAIT_ABANDONED)
            rc = VERR_INVALID_HANDLE;
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    return rc;
}


RTDECL(int)  RTSerialPortOpen(PRTSERIALPORT phSerialPort, const char *pszPortAddress, uint32_t fFlags)
{
    AssertPtrReturn(phSerialPort, VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszPortAddress) && *pszPortAddress != '\0', VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RTSERIALPORT_OPEN_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & RTSERIALPORT_OPEN_F_READ) || (fFlags & RTSERIALPORT_OPEN_F_WRITE),
                 VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PRTSERIALPORTINTERNAL pThis = (PRTSERIALPORTINTERNAL)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic         = RTSERIALPORT_MAGIC;
        pThis->fOpenFlags       = fFlags;
        pThis->fEvtMask         = 0;
        pThis->fWritePending    = false;
        pThis->pbBounceBuf      = NULL;
        pThis->cbBounceBufUsed  = 0;
        pThis->cbBounceBufAlloc = 0;
        pThis->hEvtDev          = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (pThis->hEvtDev)
        {
            pThis->Overlapped.hEvent = pThis->hEvtDev,
            pThis->hEvtIntr = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (pThis->hEvtIntr)
            {
                DWORD fWinFlags = 0;

                if (fFlags & RTSERIALPORT_OPEN_F_WRITE)
                    fWinFlags |= GENERIC_WRITE;
                if (fFlags & RTSERIALPORT_OPEN_F_READ)
                    fWinFlags |= GENERIC_READ;

                pThis->hDev = CreateFile(pszPortAddress,
                                         fWinFlags,
                                         0, /* Must be opened with exclusive access. */
                                         NULL, /* No SECURITY_ATTRIBUTES structure. */
                                         OPEN_EXISTING, /* Must use OPEN_EXISTING. */
                                         FILE_FLAG_OVERLAPPED, /* Overlapped I/O. */
                                         NULL);
                if (pThis->hDev)
                    rc = rtSerialPortSetDefaultCfg(pThis);
                else
                    rc = RTErrConvertFromWin32(GetLastError());

                CloseHandle(pThis->hEvtDev);
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());

            CloseHandle(pThis->hEvtDev);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)  RTSerialPortClose(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    if (pThis == NIL_RTSERIALPORT)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the cleanup.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSERIALPORT_MAGIC_DEAD, RTSERIALPORT_MAGIC), VERR_INVALID_HANDLE);

    if (pThis->fWritePending)
        rtSerialPortWriteCheckCompletion(pThis);

    CloseHandle(pThis->hDev);
    CloseHandle(pThis->hEvtDev);
    CloseHandle(pThis->hEvtIntr);
    pThis->hDev     = NULL;
    pThis->hEvtDev  = NULL;
    pThis->hEvtIntr = NULL;
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(RTHCINTPTR) RTSerialPortToNative(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, -1);

    return (RTHCINTPTR)pThis->hDev;
}


RTDECL(int) RTSerialPortRead(RTSERIALPORT hSerialPort, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToRead > 0, VERR_INVALID_PARAMETER);

    /*
     * Kick of an overlapped read.
     */
    int rc = VINF_SUCCESS;
    uint8_t *pbBuf = (uint8_t *)pvBuf;

    while (   cbToRead > 0
           && RT_SUCCESS(rc))
    {
        BOOL fSucc = ResetEvent(pThis->Overlapped.hEvent); Assert(fSucc == TRUE); RT_NOREF(fSucc);
        DWORD cbRead = 0;
        if (ReadFile(pThis->hDev, pbBuf,
                     cbToRead <= ~(DWORD)0 ? (DWORD)cbToRead : ~(DWORD)0,
                     &cbRead, &pThis->Overlapped))
        {
            if (pcbRead)
            {
                *pcbRead = cbRead;
                break;
            }
            rc = VINF_SUCCESS;
        }
        else if (GetLastError() == ERROR_IO_PENDING)
        {
            DWORD dwWait = WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
            if (dwWait == WAIT_OBJECT_0)
            {
                if (GetOverlappedResult(pThis->hDev, &pThis->Overlapped, &cbRead, TRUE /*fWait*/))
                {
                    if (pcbRead)
                    {
                        *pcbRead = cbRead;
                        break;
                    }
                    rc = VINF_SUCCESS;
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
            else
            {
                Assert(dwWait == WAIT_FAILED);
                rc = RTErrConvertFromWin32(GetLastError());
            }
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());

        if (RT_SUCCESS(rc))
        {
            cbToRead -= cbRead;
            pbBuf    += cbRead;
        }
    }

    return rc;
}


RTDECL(int) RTSerialPortReadNB(RTSERIALPORT hSerialPort, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToRead > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);

    *pcbRead = 0;

    /*
     * Kick of an overlapped read.  It should return immediately if
     * there is bytes in the buffer.  If not, we'll cancel it and see
     * what we get back.
     */
    int rc = VINF_SUCCESS;
    BOOL fSucc = ResetEvent(pThis->Overlapped.hEvent); Assert(fSucc == TRUE); RT_NOREF(fSucc);
    DWORD cbRead = 0;
    if (   cbToRead == 0
        || ReadFile(pThis->hDev, pvBuf,
                    cbToRead <= ~(DWORD)0 ? (DWORD)cbToRead : ~(DWORD)0,
                    &cbRead, &pThis->Overlapped))
    {
        *pcbRead = cbRead;
        rc = VINF_SUCCESS;
    }
    else if (GetLastError() == ERROR_IO_PENDING)
    {
        if (!CancelIo(pThis->hDev))
            WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
        if (GetOverlappedResult(pThis->hDev, &pThis->Overlapped, &cbRead, TRUE /*fWait*/))
        {
            *pcbRead = cbRead;
            rc = VINF_SUCCESS;
        }
        else if (GetLastError() == ERROR_OPERATION_ABORTED)
        {
            *pcbRead = 0;
            rc = VINF_TRY_AGAIN;
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


RTDECL(int) RTSerialPortWrite(RTSERIALPORT hSerialPort, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToWrite > 0, VERR_INVALID_PARAMETER);

    /* If I/O is pending, check if it has completed. */
    int rc = VINF_SUCCESS;
    if (pThis->fWritePending)
        rc = rtSerialPortWriteCheckCompletion(pThis);
    if (rc == VINF_SUCCESS)
    {
        const uint8_t *pbBuf = (const uint8_t *)pvBuf;

        while (   cbToWrite > 0
               && RT_SUCCESS(rc))
        {
            BOOL fSucc = ResetEvent(pThis->Overlapped.hEvent); Assert(fSucc == TRUE); RT_NOREF(fSucc);
            DWORD cbWritten = 0;
            if (WriteFile(pThis->hDev, pbBuf,
                          cbToWrite <= ~(DWORD)0 ? (DWORD)cbToWrite : ~(DWORD)0,
                          &cbWritten, &pThis->Overlapped))
            {
                if (pcbWritten)
                {
                    *pcbWritten = cbWritten;
                    break;
                }
                rc = VINF_SUCCESS;
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                DWORD dwWait = WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
                if (dwWait == WAIT_OBJECT_0)
                {
                    if (GetOverlappedResult(pThis->hDev, &pThis->Overlapped, &cbWritten, TRUE /*fWait*/))
                    {
                        if (pcbWritten)
                        {
                            *pcbWritten = cbWritten;
                            break;
                        }
                        rc = VINF_SUCCESS;
                    }
                    else
                        rc = RTErrConvertFromWin32(GetLastError());
                }
                else
                {
                    Assert(dwWait == WAIT_FAILED);
                    rc = RTErrConvertFromWin32(GetLastError());
                }
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());

            if (RT_SUCCESS(rc))
            {
                cbToWrite -= cbWritten;
                pbBuf     += cbWritten;
            }
        }
    }

    return rc;
}


RTDECL(int) RTSerialPortWriteNB(RTSERIALPORT hSerialPort, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToWrite > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);

    /* If I/O is pending, check if it has completed. */
    int rc = VINF_SUCCESS;
    if (pThis->fWritePending)
        rc = rtSerialPortWriteCheckCompletion(pThis);
    if (rc == VINF_SUCCESS)
    {
        Assert(!pThis->fWritePending);

        /* Do the bounce buffering. */
        if (    pThis->cbBounceBufAlloc < cbToWrite
            &&  pThis->cbBounceBufAlloc < RTSERIALPORT_NT_SIZE)
        {
            if (cbToWrite > RTSERIALPORT_NT_SIZE)
                cbToWrite = RTSERIALPORT_NT_SIZE;
            void *pv = RTMemRealloc(pThis->pbBounceBuf, RT_ALIGN_Z(cbToWrite, _1K));
            if (pv)
            {
                pThis->pbBounceBuf = (uint8_t *)pv;
                pThis->cbBounceBufAlloc = RT_ALIGN_Z(cbToWrite, _1K);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else if (cbToWrite > RTSERIALPORT_NT_SIZE)
            cbToWrite = RTSERIALPORT_NT_SIZE;
        if (RT_SUCCESS(rc) && cbToWrite)
        {
            memcpy(pThis->pbBounceBuf, pvBuf, cbToWrite);
            pThis->cbBounceBufUsed = (uint32_t)cbToWrite;

            /* Submit the write. */
            rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
            DWORD cbWritten = 0;
            if (WriteFile(pThis->hDev, pThis->pbBounceBuf, (DWORD)pThis->cbBounceBufUsed,
                          &cbWritten, &pThis->Overlapped))
            {
                *pcbWritten = RT_MIN(cbWritten, cbToWrite); /* paranoia^3 */
                rc = VINF_SUCCESS;
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                *pcbWritten = cbToWrite;
                pThis->fWritePending = true;
                rc = VINF_SUCCESS;
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }
        else if (RT_SUCCESS(rc))
            *pcbWritten = 0;
    }
    else if (RT_SUCCESS(rc))
        *pcbWritten = 0;

    return rc;
}


RTDECL(int) RTSerialPortCfgQueryCurrent(RTSERIALPORT hSerialPort, PRTSERIALPORTCFG pCfg)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    pCfg->uBaudRate = pThis->PortCfg.BaudRate;
    switch (pThis->PortCfg.Parity)
    {
        case NOPARITY:
            pCfg->enmParity = RTSERIALPORTPARITY_NONE;
            break;
        case EVENPARITY:
            pCfg->enmParity = RTSERIALPORTPARITY_EVEN;
            break;
        case ODDPARITY:
            pCfg->enmParity = RTSERIALPORTPARITY_ODD;
            break;
        case MARKPARITY:
            pCfg->enmParity = RTSERIALPORTPARITY_MARK;
            break;
        case SPACEPARITY:
            pCfg->enmParity = RTSERIALPORTPARITY_SPACE;
            break;
        default:
            AssertFailed();
            return VERR_INTERNAL_ERROR;
    }

    switch (pThis->PortCfg.ByteSize)
    {
        case 5:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_5BITS;
            break;
        case 6:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_6BITS;
            break;
        case 7:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_7BITS;
            break;
        case 8:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
            break;
        default:
            AssertFailed();
            return VERR_INTERNAL_ERROR;
    }

    switch (pThis->PortCfg.StopBits)
    {
        case ONESTOPBIT:
            pCfg->enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;
            break;
        case ONE5STOPBITS:
            pCfg->enmStopBitCount = RTSERIALPORTSTOPBITS_ONEPOINTFIVE;
            break;
        case TWOSTOPBITS:
            pCfg->enmStopBitCount = RTSERIALPORTSTOPBITS_TWO;
            break;
        default:
            AssertFailed();
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTSerialPortCfgSet(RTSERIALPORT hSerialPort, PCRTSERIALPORTCFG pCfg, PRTERRINFO pErrInfo)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    RT_NOREF(pErrInfo);

    DCB DcbNew;
    memcpy(&DcbNew, &pThis->PortCfg, sizeof(DcbNew));
    DcbNew.BaudRate = pCfg->uBaudRate;

    switch (pCfg->enmParity)
    {
        case RTSERIALPORTPARITY_NONE:
            DcbNew.Parity = NOPARITY;
            break;
        case RTSERIALPORTPARITY_EVEN:
            DcbNew.Parity = EVENPARITY;
            break;
        case RTSERIALPORTPARITY_ODD:
            DcbNew.Parity = ODDPARITY;
            break;
        case RTSERIALPORTPARITY_MARK:
            DcbNew.Parity = MARKPARITY;
            break;
        case RTSERIALPORTPARITY_SPACE:
            DcbNew.Parity = SPACEPARITY;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    switch (pCfg->enmDataBitCount)
    {
        case RTSERIALPORTDATABITS_5BITS:
            DcbNew.ByteSize = 5;
            break;
        case RTSERIALPORTDATABITS_6BITS:
            DcbNew.ByteSize = 6;
            break;
        case RTSERIALPORTDATABITS_7BITS:
            DcbNew.ByteSize = 7;
            break;
        case RTSERIALPORTDATABITS_8BITS:
            DcbNew.ByteSize = 8;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    switch (pCfg->enmStopBitCount)
    {
        case RTSERIALPORTSTOPBITS_ONE:
            DcbNew.StopBits = ONESTOPBIT;
            break;
        case RTSERIALPORTSTOPBITS_ONEPOINTFIVE:
            AssertReturn(pCfg->enmDataBitCount == RTSERIALPORTDATABITS_5BITS, VERR_INVALID_PARAMETER);
            DcbNew.StopBits = ONE5STOPBITS;
            break;
        case RTSERIALPORTSTOPBITS_TWO:
            AssertReturn(pCfg->enmDataBitCount != RTSERIALPORTDATABITS_5BITS, VERR_INVALID_PARAMETER);
            DcbNew.StopBits = TWOSTOPBITS;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    int rc = VINF_SUCCESS;
    if (!SetCommState(pThis->hDev, &DcbNew))
        rc = RTErrConvertFromWin32(GetLastError());
    else
        memcpy(&pThis->PortCfg, &DcbNew, sizeof(DcbNew));

    return rc;
}


RTDECL(int) RTSerialPortEvtPoll(RTSERIALPORT hSerialPort, uint32_t fEvtMask, uint32_t *pfEvtsRecv,
                                RTMSINTERVAL msTimeout)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fEvtMask & ~RTSERIALPORT_EVT_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfEvtsRecv, VERR_INVALID_POINTER);

    *pfEvtsRecv = 0;

    int rc = VINF_SUCCESS;
    if (fEvtMask != pThis->fEvtMask)
        rc = rtSerialPortWinUpdateEvtMask(pThis, fEvtMask);

    if (RT_SUCCESS(rc))
    {
        DWORD dwEventMask = 0;
        HANDLE ahWait[2];
        ahWait[0] = pThis->hEvtDev;
        ahWait[1] = pThis->hEvtIntr;

        RT_ZERO(pThis->Overlapped);
        pThis->Overlapped.hEvent = pThis->hEvtDev;

        if (!WaitCommEvent(pThis->hDev, &dwEventMask, &pThis->Overlapped))
        {
            DWORD dwRet = GetLastError();
            if (dwRet == ERROR_IO_PENDING)
            {
                dwRet = WaitForMultipleObjects(2, ahWait, FALSE, msTimeout == RT_INDEFINITE_WAIT ? INFINITE : msTimeout);
                if (dwRet == WAIT_TIMEOUT)
                    rc = VERR_TIMEOUT;
                else if (dwRet == WAIT_FAILED)
                    rc = RTErrConvertFromWin32(GetLastError());
                else if (dwRet != WAIT_OBJECT_0)
                    rc = VERR_INTERRUPTED;
            }
            else
                rc = RTErrConvertFromWin32(dwRet);
        }

        if (RT_SUCCESS(rc))
        {
            /* Check the event */
            if (dwEventMask & EV_RXCHAR)
                *pfEvtsRecv |= RTSERIALPORT_EVT_F_DATA_RX;
            if (dwEventMask & EV_TXEMPTY)
            {
                if (pThis->fWritePending)
                {
                    rc = rtSerialPortWriteCheckCompletion(pThis);
                    if (rc == VINF_SUCCESS)
                        *pfEvtsRecv |= RTSERIALPORT_EVT_F_DATA_TX;
                    else
                        rc = VINF_SUCCESS;
                }
                else
                    *pfEvtsRecv |= RTSERIALPORT_EVT_F_DATA_TX;
            }
            if (dwEventMask & EV_BREAK)
                *pfEvtsRecv |= RTSERIALPORT_EVT_F_BREAK_DETECTED;
            if (dwEventMask & (EV_CTS | EV_DSR | EV_RING | EV_RLSD))
                *pfEvtsRecv |= RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED;
        }
    }

    return rc;
}


RTDECL(int) RTSerialPortEvtPollInterrupt(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    if (!SetEvent(pThis->hEvtIntr))
        return RTErrConvertFromWin32(GetLastError());

    return VINF_SUCCESS;
}


RTDECL(int) RTSerialPortChgBreakCondition(RTSERIALPORT hSerialPort, bool fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    BOOL fSucc = FALSE;
    if (fSet)
        fSucc = SetCommBreak(pThis->hDev);
    else
        fSucc = ClearCommBreak(pThis->hDev);

    int rc = VINF_SUCCESS;
    if (!fSucc)
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


RTDECL(int) RTSerialPortChgStatusLines(RTSERIALPORT hSerialPort, uint32_t fClear, uint32_t fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    BOOL fSucc = TRUE;
    if (fSet & RTSERIALPORT_CHG_STS_LINES_F_DTR)
        fSucc = EscapeCommFunction(pThis->hDev, SETDTR);
    if (   fSucc
        && (fSet & RTSERIALPORT_CHG_STS_LINES_F_RTS))
        fSucc = EscapeCommFunction(pThis->hDev, SETRTS);

    if (   fSucc
        && (fClear & RTSERIALPORT_CHG_STS_LINES_F_DTR))
        fSucc = EscapeCommFunction(pThis->hDev, CLRDTR);
    if (   fSucc
        && (fClear & RTSERIALPORT_CHG_STS_LINES_F_RTS))
        fSucc = EscapeCommFunction(pThis->hDev, CLRRTS);

    int rc = VINF_SUCCESS;
    if (!fSucc)
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


RTDECL(int) RTSerialPortQueryStatusLines(RTSERIALPORT hSerialPort, uint32_t *pfStsLines)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfStsLines, VERR_INVALID_POINTER);

    *pfStsLines = 0;

    int rc = VINF_SUCCESS;
    DWORD fStsLinesQueried = 0;

    /* Get the new state */
    if (GetCommModemStatus(pThis->hDev, &fStsLinesQueried))
    {
        *pfStsLines |= (fStsLinesQueried & MS_RLSD_ON) ? RTSERIALPORT_STS_LINE_DCD : 0;
        *pfStsLines |= (fStsLinesQueried & MS_RING_ON) ? RTSERIALPORT_STS_LINE_RI  : 0;
        *pfStsLines |= (fStsLinesQueried & MS_DSR_ON) ? RTSERIALPORT_STS_LINE_DSR : 0;
        *pfStsLines |= (fStsLinesQueried & MS_CTS_ON) ? RTSERIALPORT_STS_LINE_CTS : 0;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}

