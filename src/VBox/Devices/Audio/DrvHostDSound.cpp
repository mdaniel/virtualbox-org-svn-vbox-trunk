/* $Id$ */
/** @file
 * Windows host backend driver using DirectSound.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <iprt/win/windows.h>
#include <dsound.h>

#include <iprt/alloc.h>
#include <iprt/system.h>
#include <iprt/uuid.h>
#include <iprt/utf16.h>

#include "DrvAudio.h"
#include "VBoxDD.h"
#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
# include <new> /* For bad_alloc. */
# include "VBoxMMNotificationClient.h"
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/*
 * IDirectSound* interface uses HRESULT status codes and the driver callbacks use
 * the IPRT status codes. To minimize HRESULT->IPRT conversion most internal functions
 * in the driver return HRESULT and conversion is done in the driver callbacks.
 *
 * Naming convention:
 * 'dsound*' functions return IPRT status code;
 * 'directSound*' - return HRESULT.
 */

/*
 * Optional release logging, which a user can turn on with the
 * 'VBoxManage debugvm' command.
 * Debug logging still uses the common Log* macros from IPRT.
 * Messages which always should go to the release log use LogRel.
 */
/* General code behavior. */
#define DSLOG(a)    do { LogRel2(a); } while(0)
/* Something which produce a lot of logging during playback/recording. */
#define DSLOGF(a)   do { LogRel3(a); } while(0)
/* Important messages like errors. Limited in the default release log to avoid log flood. */
#define DSLOGREL(a) \
    do {  \
        static int8_t s_cLogged = 0; \
        if (s_cLogged < 8) { \
            ++s_cLogged; \
            LogRel(a); \
        } else DSLOG(a); \
    } while (0)

/** Maximum number of attempts to restore the sound buffer before giving up. */
#define DRV_DSOUND_RESTORE_ATTEMPTS_MAX         3
/** Default input latency (in ms). */
#define DRV_DSOUND_DEFAULT_LATENCY_MS_IN        50
/** Default output latency (in ms). */
#define DRV_DSOUND_DEFAULT_LATENCY_MS_OUT       50

/** Makes DRVHOSTDSOUND out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface) \
    ( (PDRVHOSTDSOUND)((uintptr_t)pInterface - RT_UOFFSETOF(DRVHOSTDSOUND, IHostAudio)) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/* Dynamically load dsound.dll. */
typedef HRESULT WINAPI FNDIRECTSOUNDENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
typedef FNDIRECTSOUNDENUMERATEW *PFNDIRECTSOUNDENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTUREENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
typedef FNDIRECTSOUNDCAPTUREENUMERATEW *PFNDIRECTSOUNDCAPTUREENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTURECREATE8(LPCGUID lpcGUID, LPDIRECTSOUNDCAPTURE8 *lplpDSC, LPUNKNOWN pUnkOuter);
typedef FNDIRECTSOUNDCAPTURECREATE8 *PFNDIRECTSOUNDCAPTURECREATE8;

#define VBOX_DSOUND_MAX_EVENTS 3

typedef enum DSOUNDEVENT
{
    DSOUNDEVENT_NOTIFY = 0,
    DSOUNDEVENT_INPUT,
    DSOUNDEVENT_OUTPUT,
} DSOUNDEVENT;

typedef struct DSOUNDHOSTCFG
{
    RTUUID          uuidPlay;
    LPCGUID         pGuidPlay;
    RTUUID          uuidCapture;
    LPCGUID         pGuidCapture;
} DSOUNDHOSTCFG, *PDSOUNDHOSTCFG;

typedef struct DSOUNDSTREAM
{
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG  Cfg;
    /** Buffer alignment. */
    uint8_t            uAlign;
    /** Whether this stream is in an enable state on the DirectSound side. */
    bool               fEnabled;
    /** The stream's critical section for synchronizing access. */
    RTCRITSECT         CritSect;
    /** The internal playback / capturing buffer. */
    PRTCIRCBUF         pCircBuf;
    /** Size (in bytes) of the DirectSound buffer.
     *  Note: This in *not* the size of the circular buffer above! */
    DWORD              cbBufSize;
    union
    {
        struct
        {
            /** The actual DirectSound Buffer (DSB) used for the capturing.
             *  This is a secondary buffer and is used as a streaming buffer. */
            LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB;
            /** Current read offset (in bytes) within the DSB. */
            DWORD                       offReadPos;
            /** Number of buffer overruns happened. Used for logging. */
            uint8_t                     cOverruns;
        } In;
        struct
        {
            /** The actual DirectSound Buffer (DSB) used for playback.
             *  This is a secondary buffer and is used as a streaming buffer. */
            LPDIRECTSOUNDBUFFER8        pDSB;
            /** Current write offset (in bytes) within the DSB. */
            DWORD                       offWritePos;
            /** Offset of last play cursor within the DSB when checked for pending. */
            DWORD                       offPlayCursorLastPending;
            /** Offset of last play cursor within the DSB when last played. */
            DWORD                       offPlayCursorLastPlayed;
            /** Total amount (in bytes) written to our internal ring buffer. */
            uint64_t                    cbWritten;
            /** Total amount (in bytes) played (to the DirectSound buffer). */
            uint64_t                    cbTransferred;
            /** Flag indicating whether playback was just (re)started. */
            bool                        fFirstTransfer;
            /** Flag indicating whether this stream is in draining mode, e.g. no new
             *  data is being written to it but DirectSound still needs to be able to
             *  play its remaining (buffered) data. */
            bool                        fDrain;
            /** How much (in bytes) the last transfer from the internal buffer
             *  to the DirectSound buffer was. */
            uint32_t                    cbLastTransferred;
            /** Timestamp (in ms) of the last transfer from the internal buffer
             *  to the DirectSound buffer. */
            uint64_t                    tsLastTransferredMs;
            /** Number of buffer underruns happened. Used for logging. */
            uint8_t                     cUnderruns;
        } Out;
    };
#ifdef LOG_ENABLED
    struct
    {
        uint64_t tsLastTransferredMs;
    } Dbg;
#endif
} DSOUNDSTREAM, *PDSOUNDSTREAM;

typedef struct DRVHOSTDSOUND
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Our audio host audio interface. */
    PDMIHOSTAUDIO               IHostAudio;
    /** Critical section to serialize access. */
    RTCRITSECT                  CritSect;
    /** List of found host input devices. */
    RTLISTANCHOR                lstDevInput;
    /** List of found host output devices. */
    RTLISTANCHOR                lstDevOutput;
    /** DirectSound configuration options. */
    DSOUNDHOSTCFG               Cfg;
    /** Whether this backend supports any audio input. */
    bool                        fEnabledIn;
    /** Whether this backend supports any audio output. */
    bool                        fEnabledOut;
    /** The Direct Sound playback interface. */
    LPDIRECTSOUND8              pDS;
    /** The Direct Sound capturing interface. */
    LPDIRECTSOUNDCAPTURE8       pDSC;
#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    VBoxMMNotificationClient   *m_pNotificationClient;
#endif
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /** Callback function to the upper driver.
     *  Can be NULL if not being used / registered. */
    PFNPDMHOSTAUDIOCALLBACK     pfnCallback;
#endif
    /** Pointer to the input stream. */
    PDSOUNDSTREAM               pDSStrmIn;
    /** Pointer to the output stream. */
    PDSOUNDSTREAM               pDSStrmOut;
} DRVHOSTDSOUND, *PDRVHOSTDSOUND;

/** No flags specified. */
#define DSOUNDENUMCBFLAGS_NONE          0
/** (Release) log found devices. */
#define DSOUNDENUMCBFLAGS_LOG           RT_BIT(0)

/**
 * Callback context for enumeration callbacks
 */
typedef struct DSOUNDENUMCBCTX
{
    /** Pointer to host backend driver. */
    PDRVHOSTDSOUND      pDrv;
    /** Enumeration flags. */
    uint32_t            fFlags;
    /** Number of found input devices. */
    uint8_t             cDevIn;
    /** Number of found output devices. */
    uint8_t             cDevOut;
} DSOUNDENUMCBCTX, *PDSOUNDENUMCBCTX;

typedef struct DSOUNDDEV
{
    RTLISTNODE  Node;
    char       *pszName;
    GUID        Guid;
} DSOUNDDEV, *PDSOUNDDEV;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static HRESULT  directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB);
static HRESULT  directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS);
static HRESULT  directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush);
static HRESULT  directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush);

static void     dsoundDeviceRemove(PDSOUNDDEV pDev);
static int      dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg);

static int      dsoundStreamEnable(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fEnable);
static void     dsoundStreamReset(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS);
static void     dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis);
static void     dsoundUpdateStatusInternalEx(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum);


static DWORD dsoundRingDistance(DWORD offEnd, DWORD offBegin, DWORD cSize)
{
    AssertReturn(offEnd <= cSize,   0);
    AssertReturn(offBegin <= cSize, 0);

    return offEnd >= offBegin ? offEnd - offBegin : cSize - offBegin + offEnd;
}

static int dsoundWaveFmtFromCfg(PPDMAUDIOSTREAMCFG pCfg, PWAVEFORMATEX pFmt)
{
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(pFmt, VERR_INVALID_POINTER);

    RT_BZERO(pFmt, sizeof(WAVEFORMATEX));

    pFmt->wFormatTag      = WAVE_FORMAT_PCM;
    pFmt->nChannels       = pCfg->Props.cChannels;
    pFmt->wBitsPerSample  = pCfg->Props.cBytes * 8;
    pFmt->nSamplesPerSec  = pCfg->Props.uHz;
    pFmt->nBlockAlign     = pFmt->nChannels * pFmt->wBitsPerSample / 8;
    pFmt->nAvgBytesPerSec = pFmt->nSamplesPerSec * pFmt->nBlockAlign;
    pFmt->cbSize          = 0; /* No extra data specified. */

    return VINF_SUCCESS;
}

/**
 * Retrieves the number of free bytes available for writing to a DirectSound output stream.
 *
 * @return  IPRT status code. VERR_NOT_AVAILABLE if unable to determine or the buffer was not recoverable.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           DirectSound output stream to retrieve number for.
 * @param   pdwFree             Where to return the free amount on success.
 */
static int dsoundGetFreeOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, DWORD *pdwFree)
{
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pStreamDS, VERR_INVALID_POINTER);
    AssertPtrReturn(pdwFree,   VERR_INVALID_POINTER);

    Assert(pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT); /* Paranoia. */

    LPDIRECTSOUNDBUFFER8 pDSB = pStreamDS->Out.pDSB;
    if (!pDSB)
    {
        AssertPtr(pDSB);
        return VERR_INVALID_POINTER;
    }

    HRESULT hr = S_OK;

    /* Get the current play position which is used for calculating the free space in the buffer. */
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        DWORD cbPlayCursor, cbWriteCursor;
        hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayCursor, &cbWriteCursor);
        if (SUCCEEDED(hr))
        {
            int32_t cbDiff = cbWriteCursor - cbPlayCursor;
            if (cbDiff < 0)
                cbDiff += pStreamDS->cbBufSize;

            int32_t cbFree = cbPlayCursor - pStreamDS->Out.offWritePos;
            if (cbFree < 0)
                cbFree += pStreamDS->cbBufSize;

            if (cbFree > (int32_t)pStreamDS->cbBufSize - cbDiff)
            {
                pStreamDS->Out.offWritePos = cbWriteCursor;
                cbFree = pStreamDS->cbBufSize - cbDiff;
            }

            /* When starting to use a DirectSound buffer, cbPlayCursor and cbWriteCursor
             * both point at position 0, so we won't be able to detect how many bytes
             * are writable that way.
             *
             * So use our per-stream written indicator to see if we just started a stream. */
            if (pStreamDS->Out.cbWritten == 0)
                cbFree = pStreamDS->cbBufSize;

            DSLOGREL(("DSound: cbPlayCursor=%RU32, cbWriteCursor=%RU32, offWritePos=%RU32 -> cbFree=%RI32\n",
                      cbPlayCursor, cbWriteCursor, pStreamDS->Out.offWritePos, cbFree));

            *pdwFree = cbFree;

            return VINF_SUCCESS;
        }

        if (hr != DSERR_BUFFERLOST) /** @todo MSDN doesn't state this error for GetCurrentPosition(). */
            break;

        LogFunc(("Getting playing position failed due to lost buffer, restoring ...\n"));

        directSoundPlayRestore(pThis, pDSB);
    }

    if (hr != DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
        DSLOGREL(("DSound: Getting current playback position failed with %Rhrc\n", hr));

    LogFunc(("Failed with %Rhrc\n", hr));

    return VERR_NOT_AVAILABLE;
}

static char *dsoundGUIDToUtf8StrA(LPCGUID pGUID)
{
    if (pGUID)
    {
        LPOLESTR lpOLEStr;
        HRESULT hr = StringFromCLSID(*pGUID, &lpOLEStr);
        if (SUCCEEDED(hr))
        {
            char *pszGUID;
            int rc = RTUtf16ToUtf8(lpOLEStr, &pszGUID);
            CoTaskMemFree(lpOLEStr);

            return RT_SUCCESS(rc) ? pszGUID : NULL;
        }
    }

    return RTStrDup("{Default device}");
}

/**
 * Clears the list of the host's playback + capturing devices.
 *
 * @param   pThis               Host audio driver instance.
 */
static void dsoundDevicesClear(PDRVHOSTDSOUND pThis)
{
    AssertPtrReturnVoid(pThis);

    PDSOUNDDEV pDev, pDevNext;
    RTListForEachSafe(&pThis->lstDevInput, pDev, pDevNext, DSOUNDDEV, Node)
        dsoundDeviceRemove(pDev);

    Assert(RTListIsEmpty(&pThis->lstDevInput));

    RTListForEachSafe(&pThis->lstDevOutput, pDev, pDevNext, DSOUNDDEV, Node)
        dsoundDeviceRemove(pDev);

    Assert(RTListIsEmpty(&pThis->lstDevOutput));
}

static HRESULT directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB)
{
    RT_NOREF(pThis);
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (FAILED(hr))
        DSLOG(("DSound: Restoring playback buffer\n"));
    else
        DSLOGREL(("DSound: Restoring playback buffer failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundPlayUnlock(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB,
                                     PVOID pv1, PVOID pv2,
                                     DWORD cb1, DWORD cb2)
{
    RT_NOREF(pThis);
    HRESULT hr = IDirectSoundBuffer8_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking playback buffer failed with %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureUnlock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB,
                                        PVOID pv1, PVOID pv2,
                                        DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer8_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking capture buffer failed with %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayLock(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                   DWORD dwOffset, DWORD dwBytes,
                                   PVOID *ppv1, PVOID *ppv2,
                                   DWORD *pcb1, DWORD *pcb2,
                                   DWORD dwFlags)
{
    AssertReturn(dwBytes, VERR_INVALID_PARAMETER);

    HRESULT hr = E_FAIL;
    AssertCompile(DRV_DSOUND_RESTORE_ATTEMPTS_MAX > 0);
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        PVOID pv1, pv2;
        DWORD cb1, cb2;
        hr = IDirectSoundBuffer8_Lock(pStreamDS->Out.pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
        if (SUCCEEDED(hr))
        {
            if (   (!pv1 || !(cb1 & pStreamDS->uAlign))
                && (!pv2 || !(cb2 & pStreamDS->uAlign)))
            {
                if (ppv1)
                    *ppv1 = pv1;
                if (ppv2)
                    *ppv2 = pv2;
                if (pcb1)
                    *pcb1 = cb1;
                if (pcb2)
                    *pcb2 = cb2;
                return S_OK;
            }
            DSLOGREL(("DSound: Locking playback buffer returned misaligned buffer: cb1=%#RX32, cb2=%#RX32 (alignment: %#RX32)\n",
                      *pcb1, *pcb2, pStreamDS->uAlign));
            directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, pv2, cb1, cb2);
            return E_FAIL;
        }

        if (hr != DSERR_BUFFERLOST)
            break;

        LogFlowFunc(("Locking failed due to lost buffer, restoring ...\n"));
        directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
    }

    DSLOGREL(("DSound: Locking playback buffer failed with %Rhrc (dwOff=%ld, dwBytes=%ld)\n", hr, dwOffset, dwBytes));
    return hr;
}

static HRESULT directSoundCaptureLock(PDSOUNDSTREAM pStreamDS,
                                      DWORD dwOffset, DWORD dwBytes,
                                      PVOID *ppv1, PVOID *ppv2,
                                      DWORD *pcb1, DWORD *pcb2,
                                      DWORD dwFlags)
{
    PVOID pv1 = NULL;
    PVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundCaptureBuffer8_Lock(pStreamDS->In.pDSCB, dwOffset, dwBytes,
                                                 &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Locking capture buffer failed with %Rhrc\n", hr));
        return hr;
    }

    if (   (pv1 && (cb1 & pStreamDS->uAlign))
        || (pv2 && (cb2 & pStreamDS->uAlign)))
    {
        DSLOGREL(("DSound: Locking capture buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                  cb1, cb2, pStreamDS->uAlign));
        directSoundCaptureUnlock(pStreamDS->In.pDSCB, pv1, pv2, cb1, cb2);
        return E_FAIL;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return S_OK;
}

/*
 * DirectSound playback
 */

static void directSoundPlayInterfaceDestroy(PDRVHOSTDSOUND pThis)
{
    if (pThis->pDS)
    {
        LogFlowFuncEnter();

        IDirectSound8_Release(pThis->pDS);
        pThis->pDS = NULL;
    }
}

static HRESULT directSoundPlayInterfaceCreate(PDRVHOSTDSOUND pThis)
{
    if (pThis->pDS != NULL)
        return S_OK;

    LogFlowFuncEnter();

    HRESULT hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_ALL,
                                  IID_IDirectSound8, (void **)&pThis->pDS);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating playback instance failed with %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pThis->pDS, pThis->Cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pThis->pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                DSLOGREL(("DSound: Setting cooperative level for window %p failed with %Rhrc\n", hWnd, hr));
        }

        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no playback devices are attached. */
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSound playback initialization failed with %Rhrc\n", hr));

            directSoundPlayInterfaceDestroy(pThis);
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    LogFlowFuncEnter();

    HRESULT hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
    if (FAILED(hr))
        return hr;

    DSLOG(("DSound: Closing playback stream\n"));

    if (pStreamDS->pCircBuf)
        Assert(RTCircBufUsed(pStreamDS->pCircBuf) == 0);

    if (SUCCEEDED(hr))
    {
        RTCritSectEnter(&pThis->CritSect);

        if (pStreamDS->pCircBuf)
        {
            RTCircBufDestroy(pStreamDS->pCircBuf);
            pStreamDS->pCircBuf = NULL;
        }

        if (pStreamDS->Out.pDSB)
        {
            IDirectSoundBuffer8_Release(pStreamDS->Out.pDSB);
            pStreamDS->Out.pDSB = NULL;
        }

        pThis->pDSStrmOut = NULL;

        RTCritSectLeave(&pThis->CritSect);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: Stopping playback stream %p failed with %Rhrc\n", pStreamDS, hr));

    return hr;
}

static HRESULT directSoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                   PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);
    AssertPtrReturn(pCfgReq,   E_POINTER);
    AssertPtrReturn(pCfgAcq,   E_POINTER);

    LogFlowFuncEnter();

    Assert(pStreamDS->Out.pDSB == NULL);

    DSLOG(("DSound: Opening playback stream (uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool)\n",
           pCfgReq->Props.uHz,
           pCfgReq->Props.cChannels,
           pCfgReq->Props.cBytes * 8,
           pCfgReq->Props.fSigned));

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(pCfgReq, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    DSLOG(("DSound: Requested playback format:\n"
           "  wFormatTag      = %RI16\n"
           "  nChannels       = %RI16\n"
           "  nSamplesPerSec  = %RU32\n"
           "  nAvgBytesPerSec = %RU32\n"
           "  nBlockAlign     = %RI16\n"
           "  wBitsPerSample  = %RI16\n"
           "  cbSize          = %RI16\n",
           wfx.wFormatTag,
           wfx.nChannels,
           wfx.nSamplesPerSec,
           wfx.nAvgBytesPerSec,
           wfx.nBlockAlign,
           wfx.wBitsPerSample,
           wfx.cbSize));

    dsoundUpdateStatusInternal(pThis);

    HRESULT hr = directSoundPlayInterfaceCreate(pThis);
    if (FAILED(hr))
        return hr;

    do /* To use breaks. */
    {
        LPDIRECTSOUNDBUFFER pDSB = NULL;

        DSBUFFERDESC bd;
        RT_ZERO(bd);
        bd.dwSize      = sizeof(bd);
        bd.lpwfxFormat = &wfx;

        /*
         * As we reuse our (secondary) buffer for playing out data as it comes in,
         * we're using this buffer as a so-called streaming buffer.
         *
         * See https://msdn.microsoft.com/en-us/library/windows/desktop/ee419014(v=vs.85).aspx
         *
         * However, as we do not want to use memory on the sound device directly
         * (as most modern audio hardware on the host doesn't have this anyway),
         * we're *not* going to use DSBCAPS_STATIC for that.
         *
         * Instead we're specifying DSBCAPS_LOCSOFTWARE, as this fits the bill
         * of copying own buffer data to our secondary's Direct Sound buffer.
         */
        bd.dwFlags       = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_LOCSOFTWARE;
        bd.dwBufferBytes = DrvAudioHlpFramesToBytes(pCfgReq->Backend.cfBufferSize, &pCfgReq->Props);

        DSLOG(("DSound: Requested playback buffer is %RU64ms (%ld bytes)\n",
               DrvAudioHlpBytesToMilli(bd.dwBufferBytes, &pCfgReq->Props), bd.dwBufferBytes));

        hr = IDirectSound8_CreateSoundBuffer(pThis->pDS, &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Creating playback sound buffer failed with %Rhrc\n", hr));
            break;
        }

        /* "Upgrade" to IDirectSoundBuffer8 interface. */
        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (PVOID *)&pStreamDS->Out.pDSB);
        IDirectSoundBuffer_Release(pDSB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying playback sound buffer interface failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters set for this stream.
         * Those might be different than the initially requested parameters.
         */
        RT_ZERO(wfx);
        hr = IDirectSoundBuffer8_GetFormat(pStreamDS->Out.pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback format failed with %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);

        hr = IDirectSoundBuffer8_GetCaps(pStreamDS->Out.pDSB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Acquired playback buffer is %RU64ms (%ld bytes)\n",
               DrvAudioHlpBytesToMilli(bc.dwBufferBytes, &pCfgReq->Props), bc.dwBufferBytes));

        DSLOG(("DSound: Acquired playback format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  dwFlags         = 0x%x\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               bc.dwFlags,
               wfx.wFormatTag,
               wfx.nChannels,
               wfx.nSamplesPerSec,
               wfx.nAvgBytesPerSec,
               wfx.nBlockAlign,
               wfx.wBitsPerSample,
               wfx.cbSize));

        if (bc.dwBufferBytes & pStreamDS->uAlign)
            DSLOGREL(("DSound: Playback capabilities returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pStreamDS->uAlign + 1));

        /*
         * Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pStreamDS->cbBufSize = bc.dwBufferBytes;

        rc = RTCircBufCreate(&pStreamDS->pCircBuf, pStreamDS->cbBufSize) * 2; /* Use "double buffering" */
        AssertRC(rc);

        pThis->pDSStrmOut = pStreamDS;

        const uint32_t cfBufSize = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, pStreamDS->cbBufSize);

        pCfgAcq->Backend.cfBufferSize = cfBufSize;
        pCfgAcq->Backend.cfPeriod     = cfBufSize / 4;
        pCfgAcq->Backend.cfPreBuf     = pCfgAcq->Backend.cfPeriod * 2;

    } while (0);

    if (FAILED(hr))
        directSoundPlayClose(pThis, pStreamDS);

    return hr;
}

static void dsoundPlayClearBuffer(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturnVoid(pStreamDS);

    PPDMAUDIOPCMPROPS pProps = &pStreamDS->Cfg.Props;

    HRESULT hr = IDirectSoundBuffer_SetCurrentPosition(pStreamDS->Out.pDSB, 0 /* Position */);
    if (FAILED(hr))
        DSLOGREL(("DSound: Setting current position to 0 when clearing buffer failed with %Rhrc\n", hr));

    PVOID pv1;
    hr = directSoundPlayLock(pThis, pStreamDS,
                             0 /* dwOffset */, pStreamDS->cbBufSize,
                             &pv1, NULL, 0, 0, DSBLOCK_ENTIREBUFFER);
    if (SUCCEEDED(hr))
    {
        DrvAudioHlpClearBuf(pProps, pv1, pStreamDS->cbBufSize, PDMAUDIOPCMPROPS_B2F(pProps, pStreamDS->cbBufSize));

        directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, NULL, 0, 0);

        /* Make sure to get the last playback position and current write position from DirectSound again.
         * Those positions in theory could have changed, re-fetch them to be sure. */
        hr = IDirectSoundBuffer_GetCurrentPosition(pStreamDS->Out.pDSB,
                                                   &pStreamDS->Out.offPlayCursorLastPlayed, &pStreamDS->Out.offWritePos);
        if (FAILED(hr))
            DSLOGREL(("DSound: Re-fetching current position when clearing buffer failed with %Rhrc\n", hr));
    }
}

/**
 * Transfers audio data from the internal buffer to the DirectSound playback instance.
 * Due to internal accounting and querying DirectSound, this function knows how much it can transfer at once.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to transfer playback data for.
 */
static int dsoundPlayTransfer(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    if (!pStreamDS->fEnabled)
    {
        Log2Func(("Stream disabled, skipping\n"));
        return VINF_SUCCESS;
    }

    uint32_t cbTransferred = 0;

    PRTCIRCBUF pCircBuf = pStreamDS->pCircBuf;
    AssertPtr(pCircBuf);

    LPDIRECTSOUNDBUFFER8 pDSB = pStreamDS->Out.pDSB;
    AssertPtr(pDSB);

    int rc = VINF_SUCCESS;

    DWORD offPlayCursor, offWriteCursor;
    HRESULT hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &offPlayCursor, &offWriteCursor);
    if (FAILED(hr))
    {
        rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
        return rc;
    }

    DWORD cbFree, cbRemaining;
    if (pStreamDS->Out.fFirstTransfer)
    {
        cbRemaining = 0;
        cbFree      = pStreamDS->cbBufSize;
    }
    else
    {
        cbFree      = dsoundRingDistance(offPlayCursor, pStreamDS->Out.offWritePos, pStreamDS->cbBufSize);
        cbRemaining = dsoundRingDistance(pStreamDS->Out.offWritePos, offPlayCursor, pStreamDS->cbBufSize);
    }

    uint32_t cbAvail      = (uint32_t)RTCircBufUsed(pCircBuf);
    uint32_t cbToTransfer = RT_MIN(cbFree, cbAvail);

#ifdef LOG_ENABLED
    if (!pStreamDS->Dbg.tsLastTransferredMs)
        pStreamDS->Dbg.tsLastTransferredMs = RTTimeMilliTS();
    Log3Func(("offPlay=%RU32, offWrite=%RU32, tsLastTransferredMs=%RU64ms, cbAvail=%RU32, cbFree=%RU32 -> cbToTransfer=%RU32 "
              "(fFirst=%RTbool, fDrain=%RTbool)\n",
              offPlayCursor, offWriteCursor, RTTimeMilliTS() - pStreamDS->Dbg.tsLastTransferredMs, cbAvail, cbFree, cbToTransfer,
              pStreamDS->Out.fFirstTransfer, pStreamDS->Out.fDrain));
    pStreamDS->Dbg.tsLastTransferredMs = RTTimeMilliTS();
#endif

    while (cbToTransfer)
    {
        DWORD cb1 = 0;
        DWORD cb2 = 0;

        void  *pvBuf;
        size_t cbBuf;
        RTCircBufAcquireReadBlock(pCircBuf, cbToTransfer, &pvBuf, &cbBuf);

        if (cbBuf)
        {
            PVOID pv1, pv2;
            hr = directSoundPlayLock(pThis, pStreamDS, pStreamDS->Out.offWritePos, (DWORD)cbBuf,
                                     &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
            if (FAILED(hr))
            {
                rc = VERR_ACCESS_DENIED;
                break;
            }

            AssertPtr(pv1);
            Assert(cb1);

            memcpy(pv1, pvBuf, cb1);

            if (pv2 && cb2) /* Buffer wrap-around? Write second part. */
                memcpy(pv2, (uint8_t *)pvBuf + cb1, cb2);

            directSoundPlayUnlock(pThis, pDSB, pv1, pv2, cb1, cb2);

            pStreamDS->Out.offWritePos = (pStreamDS->Out.offWritePos + cb1 + cb2) % pStreamDS->cbBufSize;

            Assert(cbToTransfer >= cbBuf);
            cbToTransfer -= (uint32_t)cbBuf;

            cbTransferred += cb1 + cb2;
        }

        RTCircBufReleaseReadBlock(pCircBuf, cb1 + cb2);
    }

    pStreamDS->Out.cbTransferred += cbTransferred;

    if (   pStreamDS->Out.fFirstTransfer
        && pStreamDS->Out.cbTransferred >= DrvAudioHlpFramesToBytes(pStreamDS->Cfg.Backend.cfPreBuf, &pStreamDS->Cfg.Props))
    {
        hr = directSoundPlayStart(pThis, pStreamDS);
        if (SUCCEEDED(hr))
        {
            pStreamDS->Out.fFirstTransfer = false;
        }
        else
            rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
    }

    cbAvail = (uint32_t)RTCircBufUsed(pCircBuf);
    if (   !cbAvail
        && cbTransferred)
    {
        pStreamDS->Out.cbLastTransferred   = cbTransferred;
        pStreamDS->Out.tsLastTransferredMs = RTTimeMilliTS();

        LogFlowFunc(("cbLastTransferred=%RU32, tsLastTransferredMs=%RU64\n",
                     pStreamDS->Out.cbLastTransferred, pStreamDS->Out.tsLastTransferredMs));
    }

    LogFlowFunc(("cbTransferred=%RU32, cbAvail=%RU32, rc=%Rrc\n", cbTransferred, cbAvail, rc));
    return rc;
}

static HRESULT directSoundPlayGetStatus(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pdwStatus)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSB,  E_POINTER);

    AssertPtrNull(pdwStatus);

    DWORD dwStatus = 0;
    HRESULT hr = E_FAIL;
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
        if (   hr == DSERR_BUFFERLOST
            || (   SUCCEEDED(hr)
                && (dwStatus & DSBSTATUS_BUFFERLOST)))
        {
            LogFlowFunc(("Getting status failed due to lost buffer, restoring ...\n"));
            directSoundPlayRestore(pThis, pDSB);
        }
        else
            break;
    }

    if (SUCCEEDED(hr))
    {
        if (pdwStatus)
            *pdwStatus = dwStatus;
    }
    else
        DSLOGREL(("DSound: Retrieving playback status failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    HRESULT hr = S_OK;

    if (pStreamDS->Out.pDSB)
    {
        DSLOG(("DSound: Stopping playback\n"));
        hr = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
        if (FAILED(hr))
        {
            hr = directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
            if (FAILED(hr))
                hr = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
        }
    }

    if (SUCCEEDED(hr))
    {
        if (fFlush)
            dsoundStreamReset(pThis, pStreamDS);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: %s playback failed with %Rhrc\n", fFlush ? "Stopping" : "Pausing", hr));

    return hr;
}

/**
 * Enables or disables a stream.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to enable / disable.
 * @param   fEnable             Whether to enable or disable the stream.
 */
static int dsoundStreamEnable(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fEnable)
{
    RT_NOREF(pThis);

    LogFunc(("%s %s\n",
             fEnable ? "Enabling" : "Disabling",
             pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN ? "capture" : "playback"));

    pStreamDS->fEnabled = fEnable;

    return VINF_SUCCESS;
}


/**
 * Resets the state of a DirectSound stream.
 *
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to reset state for.
 */
static void dsoundStreamReset(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    RT_NOREF(pThis);

    LogFunc(("Resetting %s\n",
             pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN ? "capture" : "playback"));

    if (pStreamDS->pCircBuf)
        RTCircBufReset(pStreamDS->pCircBuf);

    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        pStreamDS->In.offReadPos = 0;
        pStreamDS->In.cOverruns  = 0;
    }
    else if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        pStreamDS->Out.fFirstTransfer = true;
        pStreamDS->Out.fDrain         = false;
        pStreamDS->Out.cUnderruns     = 0;

        pStreamDS->Out.cbLastTransferred   = 0;
        pStreamDS->Out.tsLastTransferredMs = 0;

        pStreamDS->Out.cbTransferred = 0;
        pStreamDS->Out.cbWritten = 0;

        pStreamDS->Out.offWritePos = 0;
        pStreamDS->Out.offPlayCursorLastPending = 0;
        pStreamDS->Out.offPlayCursorLastPlayed = 0;
    }

#ifdef LOG_ENABLED
    pStreamDS->Dbg.tsLastTransferredMs = 0;
#endif
}


/**
 * Starts playing a DirectSound stream.
 *
 * @return  HRESULT
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to start playing.
 */
static HRESULT directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    HRESULT hr = S_OK;

    DWORD fFlags = DSCBSTART_LOOPING;

    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        DSLOG(("DSound: Starting playback\n"));
        hr = IDirectSoundBuffer8_Play(pStreamDS->Out.pDSB, 0, 0, fFlags);
        if (   SUCCEEDED(hr)
            || hr != DSERR_BUFFERLOST)
            break;
        else
        {
            LogFunc(("Restarting playback failed due to lost buffer, restoring ...\n"));
            directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
        }
    }

    return hr;
}


/*
 * DirectSoundCapture
 */

static LPCGUID dsoundCaptureSelectDevice(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, NULL);
    AssertPtrReturn(pCfg,  NULL);

    int rc = VINF_SUCCESS;

    LPCGUID pGUID = pThis->Cfg.pGuidCapture;
    if (!pGUID)
    {
        PDSOUNDDEV pDev = NULL;

        switch (pCfg->DestSource.Source)
        {
            case PDMAUDIORECSOURCE_LINE:
                /*
                 * At the moment we're only supporting line-in in the HDA emulation,
                 * and line-in + mic-in in the AC'97 emulation both are expected
                 * to use the host's mic-in as well.
                 *
                 * So the fall through here is intentional for now.
                 */
            case PDMAUDIORECSOURCE_MIC:
            {
                RTListForEach(&pThis->lstDevInput, pDev, DSOUNDDEV, Node)
                {
                    if (RTStrIStr(pDev->pszName, "Mic")) /** @todo What is with non en_us windows versions? */
                        break;
                }

                if (RTListNodeIsDummy(&pThis->lstDevInput, pDev, DSOUNDDEV, Node))
                    pDev = NULL; /* Found nothing. */

                break;
            }

            default:
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
                break;
        }

        if (   RT_SUCCESS(rc)
            && pDev)
        {
            DSLOG(("DSound: Guest source '%s' is using host recording device '%s'\n",
                   DrvAudioHlpRecSrcToStr(pCfg->DestSource.Source), pDev->pszName));

            pGUID = &pDev->Guid;
        }
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("DSound: Selecting recording device failed with %Rrc\n", rc));
        return NULL;
    }

    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);

    /* This always has to be in the release log. */
    LogRel(("DSound: Guest source '%s' is using host recording device with GUID '%s'\n",
            DrvAudioHlpRecSrcToStr(pCfg->DestSource.Source), pszGUID ? pszGUID: "{?}"));

    if (pszGUID)
    {
        RTStrFree(pszGUID);
        pszGUID = NULL;
    }

    return pGUID;
}

/**
 * Transfers audio data from the DirectSound capture instance to the internal buffer.
 * Due to internal accounting and querying DirectSound, this function knows how much it can transfer at once.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to capture audio data for.
 */
static int dsoundCaptureTransfer(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    RT_NOREF(pThis);

    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB = pStreamDS->In.pDSCB;
    AssertPtr(pDSCB);

    DWORD offCaptureCursor;
    HRESULT hr = IDirectSoundCaptureBuffer_GetCurrentPosition(pDSCB, NULL, &offCaptureCursor);
    if (FAILED(hr))
    {
        AssertFailed();
        return VERR_ACCESS_DENIED; /** @todo Find a better rc. */
    }

    DWORD cbUsed = dsoundRingDistance(offCaptureCursor, pStreamDS->In.offReadPos, pStreamDS->cbBufSize);

    PRTCIRCBUF pCircBuf = pStreamDS->pCircBuf;
    AssertPtr(pCircBuf);

    uint32_t cbFree = (uint32_t)RTCircBufFree(pCircBuf);
    if (   !cbFree
        || pStreamDS->In.cOverruns < 32) /** @todo Make this configurable. */
    {
        DSLOG(("DSound: Warning: Capture buffer full, skipping to record data (%RU32 bytes)\n", cbUsed));
        pStreamDS->In.cOverruns++;
    }

    DWORD cbToCapture = RT_MIN(cbUsed, cbFree);

    Log3Func(("cbUsed=%ld, cbToCapture=%ld\n", cbUsed, cbToCapture));

    while (cbToCapture)
    {
        void  *pvBuf;
        size_t cbBuf;
        RTCircBufAcquireWriteBlock(pCircBuf, cbToCapture, &pvBuf, &cbBuf);

        if (cbBuf)
        {
            PVOID pv1, pv2;
            DWORD cb1, cb2;
            hr = directSoundCaptureLock(pStreamDS, pStreamDS->In.offReadPos, (DWORD)cbBuf,
                                        &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
            if (FAILED(hr))
                break;

            AssertPtr(pv1);
            Assert(cb1);

            memcpy(pvBuf, pv1, cb1);

            if (pv2 && cb2) /* Buffer wrap-around? Write second part. */
                memcpy((uint8_t *)pvBuf + cb1, pv2, cb2);

            directSoundCaptureUnlock(pDSCB, pv1, pv2, cb1, cb2);

            pStreamDS->In.offReadPos = (pStreamDS->In.offReadPos + cb1 + cb2) % pStreamDS->cbBufSize;

            Assert(cbToCapture >= cbBuf);
            cbToCapture -= (uint32_t)cbBuf;
        }

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
        if (cbBuf)
        {
            RTFILE fh;
            int rc2 = RTFileOpen(&fh, VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "dsoundCapture.pcm",
                                 RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(rc2))
            {
                RTFileWrite(fh, pvBuf, cbBuf, NULL);
                RTFileClose(fh);
            }
        }
#endif
        RTCircBufReleaseWriteBlock(pCircBuf, cbBuf);
    }

    return VINF_SUCCESS;
}

/**
 * Destroys the DirectSound capturing interface.
 *
 * @return  IPRT status code.
 * @param   pThis               Driver instance to destroy capturing interface for.
 */
static void directSoundCaptureInterfaceDestroy(PDRVHOSTDSOUND pThis)
{
    if (pThis->pDSC)
    {
        LogFlowFuncEnter();

        IDirectSoundCapture_Release(pThis->pDSC);
        pThis->pDSC = NULL;
    }
}

/**
 * Creates the DirectSound capturing interface.
 *
 * @return  IPRT status code.
 * @param   pThis               Driver instance to create the capturing interface for.
 * @param   pCfg                Audio stream to use for creating the capturing interface.
 */
static HRESULT directSoundCaptureInterfaceCreate(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pCfg,      E_POINTER);

    if (pThis->pDSC)
        return S_OK;

    LogFlowFuncEnter();

    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture8, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture8, (void **)&pThis->pDSC);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating capture instance failed with %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pCfg);
        /* pGUID can be NULL when using the default device. */

        hr = IDirectSoundCapture_Initialize(pThis->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no capture devices are attached. */
                DSLOGREL(("DSound: Capture device currently is unavailable\n"));
            else
                DSLOGREL(("DSound: Initializing capturing device failed with %Rhrc\n", hr));

            directSoundCaptureInterfaceDestroy(pThis);
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    LogFlowFuncEnter();

    HRESULT hr = directSoundCaptureStop(pThis, pStreamDS, true /* fFlush */);
    if (FAILED(hr))
        return hr;

    if (   pStreamDS
        && pStreamDS->In.pDSCB)
    {
        DSLOG(("DSound: Closing capturing stream\n"));

        IDirectSoundCaptureBuffer8_Release(pStreamDS->In.pDSCB);
        pStreamDS->In.pDSCB = NULL;
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                      PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);
    AssertPtrReturn(pCfgReq,   E_POINTER);
    AssertPtrReturn(pCfgAcq,   E_POINTER);

    LogFlowFuncEnter();

    Assert(pStreamDS->In.pDSCB == NULL);

    DSLOG(("DSound: Opening capturing stream (uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool)\n",
           pCfgReq->Props.uHz,
           pCfgReq->Props.cChannels,
           pCfgReq->Props.cBytes * 8,
           pCfgReq->Props.fSigned));

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(pCfgReq, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    dsoundUpdateStatusInternalEx(pThis, NULL /* Cfg */, DSOUNDENUMCBFLAGS_LOG /* fEnum */);

    HRESULT hr = directSoundCaptureInterfaceCreate(pThis, pCfgReq);
    if (FAILED(hr))
        return hr;

    do /* To use breaks. */
    {
        DSCBUFFERDESC bd;
        RT_ZERO(bd);

        bd.dwSize        = sizeof(bd);
        bd.lpwfxFormat   = &wfx;
        bd.dwBufferBytes = DrvAudioHlpFramesToBytes(pCfgReq->Backend.cfBufferSize, &pCfgReq->Props);

        DSLOG(("DSound: Requested capture buffer is %RU64ms (%ld bytes)\n",
               DrvAudioHlpBytesToMilli(bd.dwBufferBytes, &pCfgReq->Props), bd.dwBufferBytes));

        LPDIRECTSOUNDCAPTUREBUFFER pDSCB;
        hr = IDirectSoundCapture_CreateCaptureBuffer(pThis->pDSC, &bd, &pDSCB, NULL);
        if (FAILED(hr))
        {
            if (hr == E_ACCESSDENIED)
            {
                DSLOGREL(("DSound: Capturing input from host not possible, access denied\n"));
            }
            else
                DSLOGREL(("DSound: Creating capture buffer failed with %Rhrc\n", hr));
            break;
        }

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pStreamDS->In.pDSCB);
        IDirectSoundCaptureBuffer_Release(pDSCB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying interface for capture buffer failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        DWORD offByteReadPos = 0;
        hr = IDirectSoundCaptureBuffer8_GetCurrentPosition(pStreamDS->In.pDSCB, NULL, &offByteReadPos);
        if (FAILED(hr))
        {
            offByteReadPos = 0;
            DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pStreamDS->In.pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture format failed with %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pStreamDS->In.pDSCB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Acquired capture buffer is %RU64ms (%ld bytes)\n",
               DrvAudioHlpBytesToMilli(bc.dwBufferBytes, &pCfgReq->Props), bc.dwBufferBytes));

        DSLOG(("DSound: Capture format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  dwFlags         = 0x%x\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               bc.dwFlags,
               wfx.wFormatTag,
               wfx.nChannels,
               wfx.nSamplesPerSec,
               wfx.nAvgBytesPerSec,
               wfx.nBlockAlign,
               wfx.wBitsPerSample,
               wfx.cbSize));

        if (bc.dwBufferBytes & pStreamDS->uAlign)
            DSLOGREL(("DSound: Capture GetCaps returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pStreamDS->uAlign + 1));

        /* Initial state: reading at the initial capture position, no error. */
        pStreamDS->In.offReadPos = 0;
        pStreamDS->cbBufSize     = bc.dwBufferBytes;

        rc = RTCircBufCreate(&pStreamDS->pCircBuf, pStreamDS->cbBufSize) * 2; /* Use "double buffering". */
        AssertRC(rc);

        pThis->pDSStrmIn = pStreamDS;

        pCfgAcq->Backend.cfBufferSize = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, pStreamDS->cbBufSize);

    } while (0);

    if (FAILED(hr))
        directSoundCaptureClose(pThis, pStreamDS);

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    RT_NOREF(pThis);

    HRESULT hr = S_OK;

    if (pStreamDS->In.pDSCB)
    {
        DSLOG(("DSound: Stopping capture\n"));
        hr = IDirectSoundCaptureBuffer_Stop(pStreamDS->In.pDSCB);
    }

    if (SUCCEEDED(hr))
    {
        if (fFlush)
             dsoundStreamReset(pThis, pStreamDS);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    HRESULT hr;
    if (pStreamDS->In.pDSCB)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer8_GetStatus(pStreamDS->In.pDSCB, &dwStatus);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Retrieving capture status failed with %Rhrc\n", hr));
        }
        else
        {
            if (dwStatus & DSCBSTATUS_CAPTURING)
            {
                DSLOG(("DSound: Already capturing\n"));
            }
            else
            {
                const DWORD fFlags = DSCBSTART_LOOPING;

                DSLOG(("DSound: Starting to capture\n"));
                hr = IDirectSoundCaptureBuffer8_Start(pStreamDS->In.pDSCB, fFlags);
                if (FAILED(hr))
                    DSLOGREL(("DSound: Starting to capture failed with %Rhrc\n", hr));
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static int dsoundDevAdd(PRTLISTANCHOR pList, LPGUID pGUID, LPCWSTR pwszDescription, PDSOUNDDEV *ppDev)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pGUID, VERR_INVALID_POINTER);
    AssertPtrReturn(pwszDescription, VERR_INVALID_POINTER);

    PDSOUNDDEV pDev = (PDSOUNDDEV)RTMemAlloc(sizeof(DSOUNDDEV));
    if (!pDev)
        return VERR_NO_MEMORY;

    int rc = RTUtf16ToUtf8(pwszDescription, &pDev->pszName);
    if (   RT_SUCCESS(rc)
        && pGUID)
    {
        memcpy(&pDev->Guid, pGUID, sizeof(GUID));
    }

    if (RT_SUCCESS(rc))
        RTListAppend(pList, &pDev->Node);

    if (ppDev)
        *ppDev = pDev;

    return rc;
}

static void dsoundDeviceRemove(PDSOUNDDEV pDev)
{
    if (pDev)
    {
        if (pDev->pszName)
        {
            RTStrFree(pDev->pszName);
            pDev->pszName = NULL;
        }

        RTListNodeRemove(&pDev->Node);

        RTMemFree(pDev);
        pDev = NULL;
    }
}

static void dsoundLogDevice(const char *pszType, LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule)
{
    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);
    /* This always has to be in the release log.
     * Only print this when we're running in verbose (audio debug) mode, as this can generate a lot of content. */
    LogRel2(("DSound: %s: GUID: %s [%ls] (Module: %ls)\n", pszType, pszGUID ? pszGUID : "{?}", pwszDescription, pwszModule));
    RTStrFree(pszGUID);
}

static BOOL CALLBACK dsoundDevicesEnumCbPlayback(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!pGUID)
        return TRUE;

    AssertPtrReturn(pwszDescription, FALSE);
    /* Do not care about pwszModule. */

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Output", pGUID, pwszDescription, pwszModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevOutput,
                          pGUID, pwszDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->cDevOut++;

    return TRUE;
}

static BOOL CALLBACK dsoundDevicesEnumCbCapture(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!pGUID)
        return TRUE;

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Input", pGUID, pwszDescription, pwszModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevInput,
                          pGUID, pwszDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->cDevIn++;

    return TRUE;
}

/**
 * Does a (Re-)enumeration of the host's playback + capturing devices.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pEnmCtx             Enumeration context to use.
 * @param   fEnum               Enumeration flags.
 */
static int dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PDSOUNDENUMCBCTX pEnmCtx, uint32_t fEnum)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pEnmCtx, VERR_INVALID_POINTER);

    dsoundDevicesClear(pThis);

    RTLDRMOD hDSound = NULL;
    int rc = RTLdrLoadSystem("dsound.dll", true /*fNoUnload*/, &hDSound);
    if (RT_SUCCESS(rc))
    {
        PFNDIRECTSOUNDENUMERATEW pfnDirectSoundEnumerateW = NULL;
        PFNDIRECTSOUNDCAPTUREENUMERATEW pfnDirectSoundCaptureEnumerateW = NULL;

        rc = RTLdrGetSymbol(hDSound, "DirectSoundEnumerateW", (void**)&pfnDirectSoundEnumerateW);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hDSound, "DirectSoundCaptureEnumerateW", (void**)&pfnDirectSoundCaptureEnumerateW);

        if (RT_SUCCESS(rc))
        {
            HRESULT hr = pfnDirectSoundEnumerateW(&dsoundDevicesEnumCbPlayback, pEnmCtx);
            if (FAILED(hr))
                LogRel2(("DSound: Error enumerating host playback devices: %Rhrc\n", hr));

            hr = pfnDirectSoundCaptureEnumerateW(&dsoundDevicesEnumCbCapture, pEnmCtx);
            if (FAILED(hr))
                LogRel2(("DSound: Error enumerating host capturing devices: %Rhrc\n", hr));

            if (fEnum & DSOUNDENUMCBFLAGS_LOG)
            {
                LogRel2(("DSound: Found %RU8 host playback devices\n",  pEnmCtx->cDevOut));
                LogRel2(("DSound: Found %RU8 host capturing devices\n", pEnmCtx->cDevIn));
            }
        }

        RTLdrClose(hDSound);
    }
    else
    {
        /* No dsound.dll on this system. */
        LogRel2(("DSound: Could not load dsound.dll: %Rrc\n", rc));
    }

    return rc;
}

/**
 * Updates this host driver's internal status, according to the global, overall input/output
 * state and all connected (native) audio streams.
 *
 * @param   pThis               Host audio driver instance.
 * @param   pCfg                Where to store the backend configuration. Optional.
 * @param   fEnum               Enumeration flags.
 */
static void dsoundUpdateStatusInternalEx(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum)
{
    AssertPtrReturnVoid(pThis);
    /* pCfg is optional. */

    PDMAUDIOBACKENDCFG Cfg;
    RT_ZERO(Cfg);

    Cfg.cbStreamOut = sizeof(DSOUNDSTREAM);
    Cfg.cbStreamIn  = sizeof(DSOUNDSTREAM);

    DSOUNDENUMCBCTX cbCtx = { pThis, fEnum, 0, 0 };

    int rc = dsoundDevicesEnumerate(pThis, &cbCtx, fEnum);
    if (RT_SUCCESS(rc))
    {
#if 0
        if (   pThis->fEnabledOut != RT_BOOL(cbCtx.cDevOut)
            || pThis->fEnabledIn  != RT_BOOL(cbCtx.cDevIn))
        {
            /** @todo Use a registered callback to the audio connector (e.g "OnConfigurationChanged") to
             *        let the connector know that something has changed within the host backend. */
        }
#endif
        pThis->fEnabledOut = RT_BOOL(cbCtx.cDevOut);
        pThis->fEnabledIn  = RT_BOOL(cbCtx.cDevIn);

        RTStrPrintf2(Cfg.szName, sizeof(Cfg.szName), "DirectSound audio driver");

        Cfg.cMaxStreamsIn  = UINT32_MAX;
        Cfg.cMaxStreamsOut = UINT32_MAX;

        if (pCfg)
            memcpy(pCfg, &Cfg, sizeof(PDMAUDIOBACKENDCFG));
    }

    LogFlowFuncLeaveRC(rc);
}

static void dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis)
{
    dsoundUpdateStatusInternalEx(pThis, NULL /* pCfg */, 0 /* fEnum */);
}

static int dsoundCreateStreamOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                 PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    LogFlowFunc(("pStreamDS=%p, pCfgReq=%p\n", pStreamDS, pCfgReq));

    int rc = VINF_SUCCESS;

    /* Try to open playback in case the device is already there. */
    HRESULT hr = directSoundPlayOpen(pThis, pStreamDS, pCfgReq, pCfgAcq);
    if (SUCCEEDED(hr))
    {
        rc = DrvAudioHlpStreamCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            dsoundStreamReset(pThis, pStreamDS);
    }
    else
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int dsoundControlStreamOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStreamDS=%p, cmd=%d\n", pStreamDS, enmStreamCmd));

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            dsoundStreamEnable(pThis, pStreamDS, true /* fEnable */);
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            hr = directSoundPlayStart(pThis, pStreamDS);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED; /** @todo Fix this. */
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            dsoundStreamEnable(pThis, pStreamDS, false /* fEnable */);
            hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            hr = directSoundPlayStop(pThis, pStreamDS, false /* fFlush */);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DRAIN:
        {
            /* Make sure we transferred everything. */
            pStreamDS->fEnabled = true;
            pStreamDS->Out.fDrain = true;
            rc = dsoundPlayTransfer(pThis, pStreamDS);
            if (   RT_SUCCESS(rc)
                && pStreamDS->Out.fFirstTransfer)
            {
                /* If this was the first transfer ever for this stream, make sure to also play the (short) audio data. */
                DSLOG(("DSound: Started playing output (short sound)\n"));

                pStreamDS->Out.fFirstTransfer    = false;
                pStreamDS->Out.cbLastTransferred = pStreamDS->Out.cbTransferred; /* All transferred audio data must be played. */
                pStreamDS->Out.tsLastTransferredMs = RTTimeMilliTS();

                hr = directSoundPlayStart(pThis, pStreamDS);
                if (FAILED(hr))
                    rc = VERR_ACCESS_DENIED; /** @todo Find a better rc. */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DROP:
        {
            pStreamDS->Out.cbLastTransferred   = 0;
            pStreamDS->Out.tsLastTransferredMs = 0;
            RTCircBufReset(pStreamDS->pCircBuf);
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
int drvHostDSoundStreamPlay(PPDMIHOSTAUDIO pInterface,
                            PPDMAUDIOBACKENDSTREAM pStream, const void *pvBuf, uint32_t cxBuf, uint32_t *pcxWritten)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cxBuf,         VERR_INVALID_PARAMETER);
    /* pcxWritten is optional. */

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;

    uint8_t   *pbBuf    = (uint8_t *)pvBuf;
    PRTCIRCBUF pCircBuf = pStreamDS->pCircBuf;

    uint32_t cbToPlay = RT_MIN(cxBuf, (uint32_t)RTCircBufFree(pCircBuf));
    while (cbToPlay)
    {
        void *pvChunk;
        size_t cbChunk;
        RTCircBufAcquireWriteBlock(pCircBuf, cbToPlay, &pvChunk, &cbChunk);

        if (cbChunk)
        {
            memcpy(pvChunk, pbBuf, cbChunk);

            pbBuf     += cbChunk;
            Assert(cbToPlay >= cbChunk);
            cbToPlay  -= (uint32_t)cbChunk;

            cbWrittenTotal += (uint32_t)cbChunk;
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbChunk);
    }

    Assert(cbWrittenTotal <= cxBuf);
    Assert(cbWrittenTotal == cxBuf);

    pStreamDS->Out.cbWritten += cbWrittenTotal;

    if (RT_SUCCESS(rc))
    {
        if (pcxWritten)
            *pcxWritten = cbWrittenTotal;
    }
    else
        dsoundUpdateStatusInternal(pThis);

    return rc;
}

static int dsoundDestroyStreamOut(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    LogFlowFuncEnter();

    HRESULT hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
    if (SUCCEEDED(hr))
    {
        hr = directSoundPlayClose(pThis, pStreamDS);
        if (FAILED(hr))
            return VERR_GENERAL_FAILURE; /** @todo Fix. */
    }

    return VINF_SUCCESS;
}

static int dsoundCreateStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    LogFunc(("pStreamDS=%p, pCfgReq=%p, enmRecSource=%s\n",
             pStreamDS, pCfgReq, DrvAudioHlpRecSrcToStr(pCfgReq->DestSource.Source)));

    int rc = VINF_SUCCESS;

    /* Try to open capture in case the device is already there. */
    HRESULT hr = directSoundCaptureOpen(pThis, pStreamDS, pCfgReq, pCfgAcq);
    if (SUCCEEDED(hr))
    {
        rc = DrvAudioHlpStreamCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            dsoundStreamReset(pThis, pStreamDS);
    }
    else
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;

    return rc;
}

static int dsoundControlStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStreamDS=%p, enmStreamCmd=%ld\n", pStreamDS, enmStreamCmd));

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            dsoundStreamEnable(pThis, pStreamDS, true /* fEnable */);
            RT_FALL_THROUGH();
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Try to start capture. If it fails, then reopen and try again. */
            hr = directSoundCaptureStart(pThis, pStreamDS);
            if (FAILED(hr))
            {
                hr = directSoundCaptureClose(pThis, pStreamDS);
                if (SUCCEEDED(hr))
                {
                    PDMAUDIOSTREAMCFG CfgAcq;
                    hr = directSoundCaptureOpen(pThis, pStreamDS, &pStreamDS->Cfg /* pCfgReq */, &CfgAcq);
                    if (SUCCEEDED(hr))
                    {
                        rc = DrvAudioHlpStreamCfgCopy(&pStreamDS->Cfg, &CfgAcq);
                        if (RT_FAILURE(rc))
                            break;

                        /** @todo What to do if the format has changed? */

                        hr = directSoundCaptureStart(pThis, pStreamDS);
                    }
                }
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
            dsoundStreamEnable(pThis, pStreamDS, false /* fEnable */);
            RT_FALL_THROUGH();
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            directSoundCaptureStop(pThis, pStreamDS,
                                   enmStreamCmd == PDMAUDIOSTREAMCMD_DISABLE /* fFlush */);

            /* Return success in any case, as stopping the capture can fail if
             * the capture buffer is not around anymore.
             *
             * This can happen if the host's capturing device has been changed suddenly. */
            rc = VINF_SUCCESS;
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
int drvHostDSoundStreamCapture(PPDMIHOSTAUDIO pInterface,
                               PPDMAUDIOBACKENDSTREAM pStream, void *pvBuf, uint32_t cxBuf, uint32_t *pcxRead)
{

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cxBuf,         VERR_INVALID_PARAMETER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;

    uint32_t cbToRead = RT_MIN((uint32_t)RTCircBufUsed(pStreamDS->pCircBuf), cxBuf);
    while (cbToRead)
    {
        void   *pvChunk;
        size_t  cbChunk;
        RTCircBufAcquireReadBlock(pStreamDS->pCircBuf, cbToRead, &pvChunk, &cbChunk);

        if (cbChunk)
        {
            memcpy((uint8_t *)pvBuf + cbReadTotal, pvChunk, cbChunk);
            cbReadTotal += (uint32_t)cbChunk;
            Assert(cbToRead >= cbChunk);
            cbToRead    -= (uint32_t)cbChunk;
        }

        RTCircBufReleaseReadBlock(pStreamDS->pCircBuf, cbChunk);
    }

    if (RT_SUCCESS(rc))
    {
        if (pcxRead)
            *pcxRead = cbReadTotal;
    }
    else
        dsoundUpdateStatusInternal(pThis);

    return rc;
}

static int dsoundDestroyStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    LogFlowFuncEnter();

    directSoundCaptureClose(pThis, pStreamDS);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
int drvHostDSoundGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    dsoundUpdateStatusInternalEx(pThis, pBackendCfg, 0 /* fEnum */);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
void drvHostDSoundShutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    LogFlowFuncEnter();

    RT_NOREF(pThis);

    LogFlowFuncLeave();
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostDSoundInit(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    LogFlowFuncEnter();

    int rc;

    /* Verify that IDirectSound is available. */
    LPDIRECTSOUND pDirectSound = NULL;
    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_ALL, IID_IDirectSound, (void **)&pDirectSound);
    if (SUCCEEDED(hr))
    {
        IDirectSound_Release(pDirectSound);

        rc = VINF_SUCCESS;

        dsoundUpdateStatusInternalEx(pThis, NULL /* pCfg */, DSOUNDENUMCBFLAGS_LOG /* fEnum */);
    }
    else
    {
        DSLOGREL(("DSound: DirectSound not available: %Rhrc\n", hr));
        rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static LPCGUID dsoundConfigQueryGUID(PCFGMNODE pCfg, const char *pszName, RTUUID *pUuid)
{
    LPCGUID pGuid = NULL;

    char *pszGuid = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfg, pszName, &pszGuid);
    if (RT_SUCCESS(rc))
    {
        rc = RTUuidFromStr(pUuid, pszGuid);
        if (RT_SUCCESS(rc))
            pGuid = (LPCGUID)&pUuid;
        else
            DSLOGREL(("DSound: Error parsing device GUID for device '%s': %Rrc\n", pszName, rc));

        RTStrFree(pszGuid);
    }

    return pGuid;
}

static int dsoundConfigInit(PDRVHOSTDSOUND pThis, PCFGMNODE pCfg)
{
    pThis->Cfg.pGuidPlay    = dsoundConfigQueryGUID(pCfg, "DeviceGuidOut", &pThis->Cfg.uuidPlay);
    pThis->Cfg.pGuidCapture = dsoundConfigQueryGUID(pCfg, "DeviceGuidIn",  &pThis->Cfg.uuidCapture);

    DSLOG(("DSound: Configuration: DeviceGuidOut {%RTuuid}, DeviceGuidIn {%RTuuid}\n",
           &pThis->Cfg.uuidPlay,
           &pThis->Cfg.uuidCapture));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostDSoundGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostDSoundStreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                   PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = dsoundCreateStreamIn(pThis,  pStreamDS, pCfgReq, pCfgAcq);
    else
        rc = dsoundCreateStreamOut(pThis, pStreamDS, pCfgReq, pCfgAcq);

    if (RT_SUCCESS(rc))
    {
        rc = DrvAudioHlpStreamCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&pStreamDS->CritSect);
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostDSoundStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = dsoundDestroyStreamIn(pThis, pStreamDS);
    else
        rc = dsoundDestroyStreamOut(pThis, pStreamDS);

    if (RT_SUCCESS(rc))
    {
        if (RTCritSectIsInitialized(&pStreamDS->CritSect))
            rc = RTCritSectDelete(&pStreamDS->CritSect);
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostDSoundStreamControl(PPDMIHOSTAUDIO pInterface,
                                                    PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = dsoundControlStreamIn(pThis,  pStreamDS, enmStreamCmd);
    else
        rc = dsoundControlStreamOut(pThis, pStreamDS, enmStreamCmd);

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundStreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMAUDIOSTREAMSTS_FLAG_NONE);

    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    if (   pStreamDS->fEnabled
        && pStreamDS->pCircBuf)
    {
        return (uint32_t)RTCircBufUsed(pStreamDS->pCircBuf);
    }

    return 0;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundStreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, PDMAUDIOSTREAMSTS_FLAG_NONE);
    AssertPtrReturn(pStream,    PDMAUDIOSTREAMSTS_FLAG_NONE);

    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    if (pStreamDS->fEnabled)
        return (uint32_t)RTCircBufFree(pStreamDS->pCircBuf);

    return 0;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetPending}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundStreamGetPending(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, 0);

    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        uint32_t cbPending = 0;

        /* Any uncommitted data left? */
        if (pStreamDS->pCircBuf)
            cbPending = (uint32_t)RTCircBufUsed(pStreamDS->pCircBuf);

        /* Check if we have committed data which still needs to be played by
         * by DirectSound's streaming buffer. */
        if (!cbPending)
        {
            const uint64_t diffLastTransferredMs  = RTTimeMilliTS() - pStreamDS->Out.tsLastTransferredMs;
            const uint64_t uLastTranserredChunkMs = DrvAudioHlpBytesToMilli(pStreamDS->Out.cbLastTransferred, &pStreamDS->Cfg.Props);
            if (   uLastTranserredChunkMs
                && diffLastTransferredMs < uLastTranserredChunkMs)
                cbPending = 1;

            Log3Func(("diffLastTransferredMs=%RU64ms, uLastTranserredChunkMs=%RU64ms (%RU32 bytes) -> cbPending=%RU32\n",
                      diffLastTransferredMs, uLastTranserredChunkMs, pStreamDS->Out.cbLastTransferred, cbPending));
        }
        else
            Log3Func(("cbPending=%RU32\n", cbPending));

        return cbPending;
    }
    /* Note: For input streams we never have pending data left. */

    return 0;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostDSoundStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMAUDIOSTREAMSTS_FLAG_NONE);

    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    PDMAUDIOSTREAMSTS strmSts = PDMAUDIOSTREAMSTS_FLAG_INITIALIZED;

    if (pStreamDS->fEnabled)
        strmSts |= PDMAUDIOSTREAMSTS_FLAG_ENABLED;

    return strmSts;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostDSoundStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis    = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        return dsoundCaptureTransfer(pThis, pStreamDS);
    }
    else if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        return dsoundPlayTransfer(pThis, pStreamDS);
    }

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnSetCallback}
 */
static DECLCALLBACK(int) drvHostDSoundSetCallback(PPDMIHOSTAUDIO pInterface, PFNPDMHOSTAUDIOCALLBACK pfnCallback)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pfnCallback will be handled below. */

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("pfnCallback=%p\n", pfnCallback));

        if (pfnCallback) /* Register. */
        {
            Assert(pThis->pfnCallback == NULL);
            pThis->pfnCallback = pfnCallback;

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
            if (pThis->m_pNotificationClient)
                pThis->m_pNotificationClient->RegisterCallback(pThis->pDrvIns, pfnCallback);
#endif
        }
        else /* Unregister. */
        {
            if (pThis->pfnCallback)
                pThis->pfnCallback = NULL;

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
            if (pThis->m_pNotificationClient)
                pThis->m_pNotificationClient->UnregisterCallback();
#endif
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return rc;
}
#endif


/*********************************************************************************************************************************
*   PDMDRVINS::IBase Interface                                                                                                   *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostDSoundQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS     pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTDSOUND pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/*********************************************************************************************************************************
*   PDMDRVREG Interface                                                                                                          *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNPDMDRVDESTRUCT, pfnDestruct}
 */
static DECLCALLBACK(void) drvHostDSoundDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTDSOUND pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    LogFlowFuncEnter();

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    if (pThis->m_pNotificationClient)
    {
        pThis->m_pNotificationClient->Dispose();
        pThis->m_pNotificationClient->Release();

        pThis->m_pNotificationClient = NULL;
    }
#endif

    if (pThis->pDrvIns)
        CoUninitialize();

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

    LogFlowFuncLeave();
}

/**
 * @callback_method_impl{FNPDMDRVCONSTRUCT,
 *      Construct a DirectSound Audio driver instance.}
 */
static DECLCALLBACK(int) drvHostDSoundConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDRVHOSTDSOUND pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);

    LogRel(("Audio: Initializing DirectSound audio driver\n"));

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: CoInitializeEx failed with %Rhrc\n", hr));
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Init basic data members and interfaces.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostDSoundQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostDSound);
    pThis->IHostAudio.pfnStreamGetPending  = drvHostDSoundStreamGetPending;

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /* This backend supports host audio callbacks. */
    pThis->IHostAudio.pfnSetCallback = drvHostDSoundSetCallback;
    pThis->pfnCallback               = NULL;
#endif

    /*
     * Init the static parts.
     */
    RTListInit(&pThis->lstDevInput);
    RTListInit(&pThis->lstDevOutput);

    pThis->fEnabledIn  = false;
    pThis->fEnabledOut = false;

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    bool fUseNotificationClient = false;

    char szOSVersion[32];
    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szOSVersion, sizeof(szOSVersion));
    if (RT_SUCCESS(rc))
    {
        /* IMMNotificationClient is available starting at Windows Vista. */
        if (RTStrVersionCompare(szOSVersion, "6.0") >= 0)
            fUseNotificationClient = true;
    }

    if (fUseNotificationClient)
    {
        try
        {
            pThis->m_pNotificationClient = new VBoxMMNotificationClient();

            HRESULT hr = pThis->m_pNotificationClient->Initialize();
            if (FAILED(hr))
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
        }
        catch (std::bad_alloc &ex)
        {
            NOREF(ex);
            rc = VERR_NO_MEMORY;
        }
    }

    LogRel2(("DSound: Notification client is %s\n", fUseNotificationClient ? "enabled" : "disabled"));
#endif

    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize configuration values.
         */
        rc = dsoundConfigInit(pThis, pCfg);
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&pThis->CritSect);
    }

    return rc;
}


/**
 * PDM driver registration.
 */
const PDMDRVREG g_DrvHostDSound =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "DSoundAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "DirectSound Audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTDSOUND),
    /* pfnConstruct */
    drvHostDSoundConstruct,
    /* pfnDestruct */
    drvHostDSoundDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

