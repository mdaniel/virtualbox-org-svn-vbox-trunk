/* $Id$ */
/** @file
 * VBox audio devices: Pulse Audio audio driver.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#include <VBox/vmm/pdmaudioifs.h>

#include <stdio.h>

#include <iprt/alloc.h>
#include <iprt/mem.h>
#include <iprt/uuid.h>

RT_C_DECLS_BEGIN
 #include "pulse_mangling.h"
 #include "pulse_stubs.h"
RT_C_DECLS_END

#include <pulse/pulseaudio.h>

#include "DrvAudio.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
#define VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS 32 /** @todo Make this configurable thru driver options. */

#ifndef PA_STREAM_NOFLAGS
# define PA_STREAM_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

#ifndef PA_CONTEXT_NOFLAGS
# define PA_CONTEXT_NOFLAGS (pa_context_flags_t)0x0000U /* since 0.9.19 */
#endif

/** No flags specified. */
#define PULSEAUDIOENUMCBFLAGS_NONE          0
/** (Release) log found devices. */
#define PULSEAUDIOENUMCBFLAGS_LOG           RT_BIT(0)

/** Makes DRVHOSTPULSEAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface) \
    ( (PDRVHOSTPULSEAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTPULSEAUDIO, IHostAudio)) )


/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/

/**
 * Host Pulse audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTPULSEAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS            pDrvIns;
    /** Pointer to PulseAudio's threaded main loop. */
    pa_threaded_mainloop *pMainLoop;
    /**
    * Pointer to our PulseAudio context.
    * Note: We use a pMainLoop in a separate thread (pContext).
    *       So either use callback functions or protect these functions
    *       by pa_threaded_mainloop_lock() / pa_threaded_mainloop_unlock().
    */
    pa_context           *pContext;
    /** Shutdown indicator. */
    volatile bool         fAbortLoop;
    /** Abort wait loop during enumeration. */
    volatile bool         fAbortEnumLoop;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO         IHostAudio;
    /** Error count for not flooding the release log.
     *  Specify UINT32_MAX for unlimited logging. */
    uint32_t              cLogErrors;
} DRVHOSTPULSEAUDIO, *PDRVHOSTPULSEAUDIO;

typedef struct PULSEAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG     pCfg;
    /** Pointer to driver instance. */
    PDRVHOSTPULSEAUDIO     pDrv;
    /** Pointer to opaque PulseAudio stream. */
    pa_stream             *pStream;
    /** Pulse sample format and attribute specification. */
    pa_sample_spec         SampleSpec;
    /** Pulse playback and buffer metrics. */
    pa_buffer_attr         BufAttr;
    int                    fOpSuccess;
    /** Pointer to Pulse sample peeking buffer. */
    const uint8_t         *pu8PeekBuf;
    /** Current size (in bytes) of peeking data in
     *  buffer. */
    size_t                 cbPeekBuf;
    /** Our offset (in bytes) in peeking buffer. */
    size_t                 offPeekBuf;
    pa_operation          *pDrainOp;
} PULSEAUDIOSTREAM, *PPULSEAUDIOSTREAM;

/* The desired buffer length in milliseconds. Will be the target total stream
 * latency on newer version of pulse. Apparent latency can be less (or more.)
 */
typedef struct PULSEAUDIOCFG
{
    RTMSINTERVAL buffer_msecs_out;
    RTMSINTERVAL buffer_msecs_in;
} PULSEAUDIOCFG, *PPULSEAUDIOCFG;

static PULSEAUDIOCFG s_pulseCfg =
{
    100, /* buffer_msecs_out */
    100  /* buffer_msecs_in */
};

/**
 * Callback context for server enumeration callbacks.
 */
typedef struct PULSEAUDIOENUMCBCTX
{
    /** Pointer to host backend driver. */
    PDRVHOSTPULSEAUDIO  pDrv;
    /** Enumeration flags. */
    uint32_t            fFlags;
    /** Number of found input devices. */
    uint8_t             cDevIn;
    /** Number of found output devices. */
    uint8_t             cDevOut;
    /** Name of default sink being used. Must be free'd using RTStrFree(). */
    char               *pszDefaultSink;
    /** Name of default source being used. Must be free'd using RTStrFree(). */
    char               *pszDefaultSource;
} PULSEAUDIOENUMCBCTX, *PPULSEAUDIOENUMCBCTX;

#ifndef PA_CONTEXT_IS_GOOD /* To allow running on systems with PulseAudio < 0.9.11. */
static inline int PA_CONTEXT_IS_GOOD(pa_context_state_t x) {
    return
        x == PA_CONTEXT_CONNECTING ||
        x == PA_CONTEXT_AUTHORIZING ||
        x == PA_CONTEXT_SETTING_NAME ||
        x == PA_CONTEXT_READY;
}
#endif /* !PA_CONTEXT_IS_GOOD */

#ifndef PA_STREAM_IS_GOOD /* To allow running on systems with PulseAudio < 0.9.11. */
static inline int PA_STREAM_IS_GOOD(pa_stream_state_t x) {
    return
        x == PA_STREAM_CREATING ||
        x == PA_STREAM_READY;
}
#endif /* !PA_STREAM_IS_GOOD */

/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/

static int  paEnumerate(PDRVHOSTPULSEAUDIO pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum);
static int  paError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg);
static void paStreamCbSuccess(pa_stream *pStream, int fSuccess, void *pvContext);



/**
 * Signal the main loop to abort. Just signalling isn't sufficient as the
 * mainloop might not have been entered yet.
 */
static void paSignalWaiter(PDRVHOSTPULSEAUDIO pThis)
{
    if (!pThis)
        return;

    pThis->fAbortLoop = true;
    pThis->fAbortEnumLoop = true;
    pa_threaded_mainloop_signal(pThis->pMainLoop, 0);
}


static pa_sample_format_t paAudioPropsToPulse(PPDMAUDIOPCMPROPS pProps)
{
    switch (pProps->cBits)
    {
        case 8:
            if (!pProps->fSigned)
                return PA_SAMPLE_U8;
            break;

        case 16:
            if (pProps->fSigned)
                return PA_SAMPLE_S16LE;
            break;

#ifdef PA_SAMPLE_S32LE
        case 32:
            if (pProps->fSigned)
                return PA_SAMPLE_S32LE;
            break;
#endif

        default:
            break;
    }

    AssertMsgFailed(("%RU8%s not supported\n", pProps->cBits, pProps->fSigned ? "S" : "U"));
    return PA_SAMPLE_INVALID;
}


static int paPulseToAudioProps(pa_sample_format_t pulsefmt, PPDMAUDIOPCMPROPS pProps)
{
    switch (pulsefmt)
    {
        case PA_SAMPLE_U8:
            pProps->cBits   = 8;
            pProps->fSigned = false;
            break;

        case PA_SAMPLE_S16LE:
            pProps->cBits   = 16;
            pProps->fSigned = true;
            break;

        case PA_SAMPLE_S16BE:
            pProps->cBits   = 16;
            pProps->fSigned = true;
            /** @todo Handle Endianess. */
            break;

#ifdef PA_SAMPLE_S32LE
        case PA_SAMPLE_S32LE:
            pProps->cBits   = 32;
            pProps->fSigned = true;
            break;
#endif

#ifdef PA_SAMPLE_S32BE
        case PA_SAMPLE_S32BE:
            pProps->cBits   = 32;
            pProps->fSigned = true;
            /** @todo Handle Endianess. */
            break;
#endif

        default:
            AssertLogRelMsgFailed(("PulseAudio: Format (%ld) not supported\n", pulsefmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}


/**
 * Synchronously wait until an operation completed.
 */
static int paWaitForEx(PDRVHOSTPULSEAUDIO pThis, pa_operation *pOP, RTMSINTERVAL cMsTimeout, bool fDebug)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pOP,   VERR_INVALID_POINTER);
    NOREF(fDebug);

    int rc = VINF_SUCCESS;
    pThis->fAbortEnumLoop = false;

    uint64_t u64StartMs = RTTimeMilliTS();
    while (pa_operation_get_state(pOP) == PA_OPERATION_RUNNING)
    {
        if (!pThis->fAbortLoop)
        {
            AssertPtr(pThis->pMainLoop);
//            if (fDebug)
//                LogRel(("PulseAudio: pa_threaded_mainloop_wait\n"));
            pa_threaded_mainloop_wait(pThis->pMainLoop);
            if (fDebug)
                LogRel(("PulseAudio: pa_threaded_mainloop_wait done\n"));
            if (pThis->fAbortEnumLoop)
                break;
            if (   !pThis->pContext
                || pa_context_get_state(pThis->pContext) != PA_CONTEXT_READY)
            {
                LogRel(("PulseAudio: pa_context_get_state context not ready\n"));
                break;
            }
        }
        pThis->fAbortLoop = false;

        uint64_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
        if (u64ElapsedMs >= cMsTimeout)
        {
            rc = VERR_TIMEOUT;
            break;
        }
    }

    pa_operation_unref(pOP);

    return rc;
}


static int paWaitFor(PDRVHOSTPULSEAUDIO pThis, pa_operation *pOP, bool fDebug = false)
{
    return paWaitForEx(pThis, pOP, 10 * 1000 /* 10s timeout */, fDebug);
}


/**
 * Context status changed.
 */
static void paContextCbStateChanged(pa_context *pCtx, void *pvUser)
{
    AssertPtrReturnVoid(pCtx);

    PDRVHOSTPULSEAUDIO pThis = (PDRVHOSTPULSEAUDIO)pvUser;
    AssertPtrReturnVoid(pThis);

    switch (pa_context_get_state(pCtx))
    {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            paSignalWaiter(pThis);
            break;

        default:
            LogRel(("PulseAudio: ignoring state change %d\n", pa_context_get_state(pCtx)));
            break;
    }
}


/**
 * Callback called when our pa_stream_drain operation was completed.
 */
static void paStreamCbDrain(pa_stream *pStream, int fSuccess, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvUser;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;
    if (fSuccess)
    {
        pa_operation_unref(pa_stream_cork(pStream, 1,
                                          paStreamCbSuccess, pvUser));
    }
    else
        paError(pStrm->pDrv, "Failed to drain stream");

    if (pStrm->pDrainOp)
    {
        pa_operation_unref(pStrm->pDrainOp);
        pStrm->pDrainOp = NULL;
    }
}


/**
 * Stream status changed.
 */
static void paStreamCbStateChanged(pa_stream *pStream, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PDRVHOSTPULSEAUDIO pThis = (PDRVHOSTPULSEAUDIO)pvUser;
    AssertPtrReturnVoid(pThis);

    switch (pa_stream_get_state(pStream))
    {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            paSignalWaiter(pThis);
            break;

        default:
            break;
    }
}


static void paStreamCbSuccess(pa_stream *pStream, int fSuccess, void *pvUser)
{
    AssertPtrReturnVoid(pStream);

    PPULSEAUDIOSTREAM pStrm = (PPULSEAUDIOSTREAM)pvUser;
    AssertPtrReturnVoid(pStrm);

    pStrm->fOpSuccess = fSuccess;

    if (fSuccess)
        paSignalWaiter(pStrm->pDrv);
    else
        paError(pStrm->pDrv, "Failed to finish stream operation");
}


static int paStreamOpen(PDRVHOSTPULSEAUDIO pThis, bool fIn, const char *pszName,
                        pa_sample_spec *pSampleSpec, pa_buffer_attr *pBufAttr,
                        pa_stream **ppStream)
{
    AssertPtrReturn(pThis,       VERR_INVALID_POINTER);
    AssertPtrReturn(pszName,     VERR_INVALID_POINTER);
    AssertPtrReturn(pSampleSpec, VERR_INVALID_POINTER);
    AssertPtrReturn(pBufAttr,    VERR_INVALID_POINTER);
    AssertPtrReturn(ppStream,    VERR_INVALID_POINTER);

    if (!pa_sample_spec_valid(pSampleSpec))
    {
        LogRel(("PulseAudio: Unsupported sample specification for stream '%s'\n",
                pszName));
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    pa_stream *pStream = NULL;
    uint32_t   flags = PA_STREAM_NOFLAGS;

    LogFunc(("Opening '%s', rate=%dHz, channels=%d, format=%s\n",
             pszName, pSampleSpec->rate, pSampleSpec->channels,
             pa_sample_format_to_string(pSampleSpec->format)));

    pa_threaded_mainloop_lock(pThis->pMainLoop);

    do
    {
        /** @todo r=andy Use pa_stream_new_with_proplist instead. */
        if (!(pStream = pa_stream_new(pThis->pContext, pszName, pSampleSpec,
                                      NULL /* pa_channel_map */)))
        {
            LogRel(("PulseAudio: Could not create stream '%s'\n", pszName));
            rc = VERR_NO_MEMORY;
            break;
        }

        pa_stream_set_state_callback(pStream, paStreamCbStateChanged, pThis);

#if PA_API_VERSION >= 12
        /* XXX */
        flags |= PA_STREAM_ADJUST_LATENCY;
#endif

#if 0
        /* Not applicable as we don't use pa_stream_get_latency() and pa_stream_get_time(). */
        flags |= PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
#endif
        /* No input/output right away after the stream was started. */
        flags |= PA_STREAM_START_CORKED;

        if (fIn)
        {
            LogFunc(("Input stream attributes: maxlength=%d fragsize=%d\n",
                     pBufAttr->maxlength, pBufAttr->fragsize));

            if (pa_stream_connect_record(pStream, /*dev=*/NULL, pBufAttr, (pa_stream_flags_t)flags) < 0)
            {
                LogRel(("PulseAudio: Could not connect input stream '%s': %s\n",
                        pszName, pa_strerror(pa_context_errno(pThis->pContext))));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }
        else
        {
            LogFunc(("Output buffer attributes: maxlength=%d tlength=%d prebuf=%d minreq=%d\n",
                     pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

            if (pa_stream_connect_playback(pStream, /*dev=*/NULL, pBufAttr, (pa_stream_flags_t)flags,
                                           /*cvolume=*/NULL, /*sync_stream=*/NULL) < 0)
            {
                LogRel(("PulseAudio: Could not connect playback stream '%s': %s\n",
                        pszName, pa_strerror(pa_context_errno(pThis->pContext))));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }

        /* Wait until the stream is ready. */
        for (;;)
        {
            if (!pThis->fAbortLoop)
                pa_threaded_mainloop_wait(pThis->pMainLoop);
            pThis->fAbortLoop = false;

            pa_stream_state_t streamSt = pa_stream_get_state(pStream);
            if (streamSt == PA_STREAM_READY)
                break;
            else if (   streamSt == PA_STREAM_FAILED
                     || streamSt == PA_STREAM_TERMINATED)
            {
                LogRel(("PulseAudio: Failed to initialize stream '%s' (state %ld)\n", pszName, streamSt));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }

        if (RT_FAILURE(rc))
            break;

        const pa_buffer_attr *pBufAttrObtained = pa_stream_get_buffer_attr(pStream);
        AssertPtr(pBufAttrObtained);
        memcpy(pBufAttr, pBufAttrObtained, sizeof(pa_buffer_attr));

        if (fIn)
            LogFunc(("Obtained record buffer attributes: maxlength=%RU32, fragsize=%RU32\n",
                     pBufAttr->maxlength, pBufAttr->fragsize));
        else
            LogFunc(("Obtained playback buffer attributes: maxlength=%d, tlength=%d, prebuf=%d, minreq=%d\n",
                     pBufAttr->maxlength, pBufAttr->tlength, pBufAttr->prebuf, pBufAttr->minreq));

    } while (0);

    if (   RT_FAILURE(rc)
        && pStream)
        pa_stream_disconnect(pStream);

    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_FAILURE(rc))
    {
        if (pStream)
            pa_stream_unref(pStream);
    }
    else
        *ppStream = pStream;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostPulseAudioInit(PPDMIHOSTAUDIO pInterface)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    LogFlowFuncEnter();

    int rc = audioLoadPulseLib();
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Failed to load the PulseAudio shared library! Error %Rrc\n", rc));
        return rc;
    }

    pThis->fAbortLoop = false;
    pThis->pMainLoop = NULL;

    bool fLocked = false;

    do
    {
        if (!(pThis->pMainLoop = pa_threaded_mainloop_new()))
        {
            LogRel(("PulseAudio: Failed to allocate main loop: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (!(pThis->pContext = pa_context_new(pa_threaded_mainloop_get_api(pThis->pMainLoop), "VirtualBox")))
        {
            LogRel(("PulseAudio: Failed to allocate context: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_NO_MEMORY;
            break;
        }

        if (pa_threaded_mainloop_start(pThis->pMainLoop) < 0)
        {
            LogRel(("PulseAudio: Failed to start threaded mainloop: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        /* Install a global callback to known if something happens to our acquired context. */
        pa_context_set_state_callback(pThis->pContext, paContextCbStateChanged, pThis /* pvUserData */);

        pa_threaded_mainloop_lock(pThis->pMainLoop);
        fLocked = true;

        if (pa_context_connect(pThis->pContext, NULL /* pszServer */,
                               PA_CONTEXT_NOFLAGS, NULL) < 0)
        {
            LogRel(("PulseAudio: Failed to connect to server: %s\n",
                     pa_strerror(pa_context_errno(pThis->pContext))));
            rc = VERR_AUDIO_BACKEND_INIT_FAILED;
            break;
        }

        /* Wait until the pThis->pContext is ready. */
        for (;;)
        {
            if (!pThis->fAbortLoop)
                pa_threaded_mainloop_wait(pThis->pMainLoop);
            pThis->fAbortLoop = false;

            pa_context_state_t cstate = pa_context_get_state(pThis->pContext);
            if (cstate == PA_CONTEXT_READY)
                break;
            else if (   cstate == PA_CONTEXT_TERMINATED
                     || cstate == PA_CONTEXT_FAILED)
            {
                LogRel(("PulseAudio: Failed to initialize context (state %d)\n", cstate));
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
                break;
            }
        }
    }
    while (0);

    if (fLocked)
        pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_FAILURE(rc))
    {
        if (pThis->pMainLoop)
            pa_threaded_mainloop_stop(pThis->pMainLoop);

        if (pThis->pContext)
        {
            pa_context_disconnect(pThis->pContext);
            pa_context_unref(pThis->pContext);
            pThis->pContext = NULL;
        }

        if (pThis->pMainLoop)
        {
            pa_threaded_mainloop_free(pThis->pMainLoop);
            pThis->pMainLoop = NULL;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int paCreateStreamOut(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA,
                             PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    pStreamPA->pDrainOp            = NULL;

    pStreamPA->SampleSpec.format   = paAudioPropsToPulse(&pCfgReq->Props);
    pStreamPA->SampleSpec.rate     = pCfgReq->Props.uHz;
    pStreamPA->SampleSpec.channels = pCfgReq->Props.cChannels;

    /* Note that setting maxlength to -1 does not work on PulseAudio servers
     * older than 0.9.10. So use the suggested value of 3/2 of tlength */
    pStreamPA->BufAttr.tlength     =   (pa_bytes_per_second(&pStreamPA->SampleSpec)
                                        * s_pulseCfg.buffer_msecs_out) / 1000;
    pStreamPA->BufAttr.maxlength   = (pStreamPA->BufAttr.tlength * 3) / 2;
    pStreamPA->BufAttr.prebuf      = -1; /* Same as tlength */
    pStreamPA->BufAttr.minreq      = -1;

    /* Note that the struct BufAttr is updated to the obtained values after this call! */
    int rc = paStreamOpen(pThis, false /* fIn */, "PulseAudio (Out)",
                          &pStreamPA->SampleSpec, &pStreamPA->BufAttr, &pStreamPA->pStream);
    if (RT_FAILURE(rc))
        return rc;

    rc = paPulseToAudioProps(pStreamPA->SampleSpec.format, &pCfgAcq->Props);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio output format %ld\n", pStreamPA->SampleSpec.format));
        return rc;
    }

    pCfgAcq->Props.uHz       = pStreamPA->SampleSpec.rate;
    pCfgAcq->Props.cChannels = pStreamPA->SampleSpec.channels;
    pCfgAcq->Props.cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pCfgAcq->Props.cBits, pCfgAcq->Props.cChannels);

    uint32_t cbBuf = RT_MIN(pStreamPA->BufAttr.tlength * 2,
                            pStreamPA->BufAttr.maxlength); /** @todo Make this configurable! */
    if (cbBuf)
    {
        pCfgAcq->cSampleBufferHint = PDMAUDIOSTREAMCFG_B2S(pCfgAcq, cbBuf);

        pStreamPA->pDrv = pThis;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}


static int paCreateStreamIn(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM  pStreamPA,
                            PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    pStreamPA->SampleSpec.format   = paAudioPropsToPulse(&pCfgReq->Props);
    pStreamPA->SampleSpec.rate     = pCfgReq->Props.uHz;
    pStreamPA->SampleSpec.channels = pCfgReq->Props.cChannels;

    /** @todo Check these values! */
    pStreamPA->BufAttr.fragsize    = (pa_bytes_per_second(&pStreamPA->SampleSpec) * s_pulseCfg.buffer_msecs_in) / 1000;
    pStreamPA->BufAttr.maxlength   = (pStreamPA->BufAttr.fragsize * 3) / 2;

    /* Note: Other members of BufAttr are ignored for record streams. */
    int rc = paStreamOpen(pThis, true /* fIn */, "PulseAudio (In)", &pStreamPA->SampleSpec, &pStreamPA->BufAttr,
                          &pStreamPA->pStream);
    if (RT_FAILURE(rc))
        return rc;

    rc = paPulseToAudioProps(pStreamPA->SampleSpec.format, &pCfgAcq->Props);
    if (RT_FAILURE(rc))
    {
        LogRel(("PulseAudio: Cannot find audio capture format %ld\n", pStreamPA->SampleSpec.format));
        return rc;
    }

    pStreamPA->pDrv       = pThis;
    pStreamPA->pu8PeekBuf = NULL;

    pCfgAcq->Props.uHz         = pStreamPA->SampleSpec.rate;
    pCfgAcq->Props.cChannels   = pStreamPA->SampleSpec.channels;
    pCfgAcq->cSampleBufferHint = PDMAUDIOSTREAMCFG_B2S(pCfgAcq,
                                                       RT_MIN(pStreamPA->BufAttr.fragsize * 10, pStreamPA->BufAttr.maxlength));

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamCapture(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOBACKENDSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pvBuf, cbBuf);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    /* We should only call pa_stream_readable_size() once and trust the first value. */
    pa_threaded_mainloop_lock(pThis->pMainLoop);
    size_t cbAvail = pa_stream_readable_size(pStreamPA->pStream);
    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (cbAvail == (size_t)-1)
        return paError(pStreamPA->pDrv, "Failed to determine input data size");

    /* If the buffer was not dropped last call, add what remains. */
    if (pStreamPA->pu8PeekBuf)
    {
        Assert(pStreamPA->cbPeekBuf >= pStreamPA->offPeekBuf);
        cbAvail += (pStreamPA->cbPeekBuf - pStreamPA->offPeekBuf);
    }

    Log3Func(("cbAvail=%zu\n", cbAvail));

    if (!cbAvail) /* No data? Bail out. */
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;

    size_t cbToRead = RT_MIN(cbAvail, cbBuf);

    Log3Func(("cbToRead=%zu, cbAvail=%zu, offPeekBuf=%zu, cbPeekBuf=%zu\n",
              cbToRead, cbAvail, pStreamPA->offPeekBuf, pStreamPA->cbPeekBuf));

    uint32_t cbReadTotal = 0;

    while (cbToRead)
    {
        /* If there is no data, do another peek. */
        if (!pStreamPA->pu8PeekBuf)
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            pa_stream_peek(pStreamPA->pStream,
                           (const void**)&pStreamPA->pu8PeekBuf, &pStreamPA->cbPeekBuf);
            pa_threaded_mainloop_unlock(pThis->pMainLoop);

            pStreamPA->offPeekBuf = 0;

            /* No data anymore?
             * Note: If there's a data hole (cbPeekBuf then contains the length of the hole)
             *       we need to drop the stream lateron. */
            if (   !pStreamPA->pu8PeekBuf
                && !pStreamPA->cbPeekBuf)
            {
                break;
            }
        }

        Assert(pStreamPA->cbPeekBuf >= pStreamPA->offPeekBuf);
        size_t cbToWrite = RT_MIN(pStreamPA->cbPeekBuf - pStreamPA->offPeekBuf, cbToRead);

        Log3Func(("cbToRead=%zu, cbToWrite=%zu, offPeekBuf=%zu, cbPeekBuf=%zu, pu8PeekBuf=%p\n",
                  cbToRead, cbToWrite,
                  pStreamPA->offPeekBuf, pStreamPA->cbPeekBuf, pStreamPA->pu8PeekBuf));

        if (cbToWrite)
        {
            memcpy((uint8_t *)pvBuf + cbReadTotal, pStreamPA->pu8PeekBuf + pStreamPA->offPeekBuf, cbToWrite);

            Assert(cbToRead >= cbToWrite);
            cbToRead          -= cbToWrite;
            cbReadTotal       += cbToWrite;

            pStreamPA->offPeekBuf += cbToWrite;
            Assert(pStreamPA->offPeekBuf <= pStreamPA->cbPeekBuf);
        }

        if (/* Nothing to write anymore? Drop the buffer. */
               !cbToWrite
            /* Was there a hole in the peeking buffer? Drop it. */
            || !pStreamPA->pu8PeekBuf
            /* If the buffer is done, drop it. */
            || pStreamPA->offPeekBuf == pStreamPA->cbPeekBuf)
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            pa_stream_drop(pStreamPA->pStream);
            pa_threaded_mainloop_unlock(pThis->pMainLoop);

            pStreamPA->pu8PeekBuf = NULL;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbReadTotal;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                     PPDMAUDIOBACKENDSTREAM pStream, const void *pvBuf, uint32_t cbBuf,
                                                     uint32_t *pcbWritten)
{
    RT_NOREF(pvBuf, cbBuf);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pPAStream = (PPULSEAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;

    pa_threaded_mainloop_lock(pThis->pMainLoop);

    do
    {
        size_t cbWriteable = pa_stream_writable_size(pPAStream->pStream);
        if (cbWriteable == (size_t)-1)
        {
            rc = paError(pPAStream->pDrv, "Failed to determine output data size");
            break;
        }

        size_t cbToWrite = RT_MIN(cbWriteable, cbBuf);

        uint32_t cbWritten;
        while (cbToWrite)
        {
            cbWritten = cbToWrite;
            if (pa_stream_write(pPAStream->pStream, (uint8_t *)pvBuf + cbWrittenTotal, cbWritten, NULL /* Cleanup callback */,
                                0, PA_SEEK_RELATIVE) < 0)
            {
                rc = paError(pPAStream->pDrv, "Failed to write to output stream");
                break;
            }

            Assert(cbToWrite >= cbWritten);
            cbToWrite      -= cbWritten;
            cbWrittenTotal += cbWritten;
        }

    } while (0);

    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWrittenTotal;
    }

    return rc;
}


/** @todo Implement va handling. */
static int paError(PDRVHOSTPULSEAUDIO pThis, const char *szMsg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(szMsg, VERR_INVALID_POINTER);

    if (pThis->cLogErrors++ < VBOX_PULSEAUDIO_MAX_LOG_REL_ERRORS)
    {
        int rc2 = pa_context_errno(pThis->pContext);
        LogRel2(("PulseAudio: %s: %s\n", szMsg, pa_strerror(rc2)));
    }

    /** @todo Implement some PulseAudio -> IPRT mapping here. */
    return VERR_GENERAL_FAILURE;
}


static void paEnumSinkCb(pa_context *pCtx, const pa_sink_info *pInfo, int eol, void *pvUserData)
{
    LogRel(("PulseAudio: entering paEnumSinkCb\n"));
    if (eol > 0)
    {
        LogRel(("PulseAudio: paEnumSinkCb: eol=%d\n", eol));
        return;
    }

    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);
    PDRVHOSTPULSEAUDIO pThis = pCbCtx->pDrv;
    AssertPtrReturnVoid(pThis);
    if (eol < 0)
    {
        pThis->fAbortEnumLoop = true;
        pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
        LogRel(("PulseAudio: paEnumSinkCb eol=%d, signalling\n", eol));
        return;
    }

    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pInfo);

    LogRel2(("PulseAudio: Using output sink '%s'\n", pInfo->name));

    /** @todo Store sinks + channel mapping in callback context as soon as we have surround support. */
    pCbCtx->cDevOut++;

    LogRel(("PulseAudio: paEnumSinkCb signalling\n"));
    pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
}


static void paEnumSourceCb(pa_context *pCtx, const pa_source_info *pInfo, int eol, void *pvUserData)
{
    LogRel(("PulseAudio: entering paEnumSourceCb\n"));
    if (eol > 0)
    {
        LogRel(("PulseAudio: paEnumSourceCb: eol=%d\n", eol));
        return;
    }

    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);
    PDRVHOSTPULSEAUDIO pThis = pCbCtx->pDrv;
    AssertPtrReturnVoid(pThis);
    if (eol < 0)
    {
        pThis->fAbortEnumLoop = true;
        pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
        LogRel(("PulseAudio: paEnumSourceCb eol=%d, signalling\n", eol));
        return;
    }

    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pInfo);

    LogRel2(("PulseAudio: Using input source '%s'\n", pInfo->name));

    /** @todo Store sources + channel mapping in callback context as soon as we have surround support. */
    pCbCtx->cDevIn++;

    LogRel(("PulseAudio: paEnumSourceCb signalling\n"));
    pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
}


static void paEnumServerCb(pa_context *pCtx, const pa_server_info *pInfo, void *pvUserData)
{
    LogRel(("PulseAudio: entering paEnumServerCb\n"));

    AssertPtrReturnVoid(pCtx);
    PPULSEAUDIOENUMCBCTX pCbCtx = (PPULSEAUDIOENUMCBCTX)pvUserData;
    AssertPtrReturnVoid(pCbCtx);
    PDRVHOSTPULSEAUDIO pThis = pCbCtx->pDrv;
    AssertPtrReturnVoid(pThis);

    if (!pInfo)
    {
        pThis->fAbortEnumLoop = true;
        LogRel(("PulseAudio: paEnumServerCb no info, signalling\n"));
        pa_threaded_mainloop_signal(pCbCtx->pDrv->pMainLoop, 0);
        return;
    }

    if (pInfo->default_sink_name)
    {
        Assert(RTStrIsValidEncoding(pInfo->default_sink_name));
        pCbCtx->pszDefaultSink   = RTStrDup(pInfo->default_sink_name);
    }

    if (pInfo->default_sink_name)
    {
        Assert(RTStrIsValidEncoding(pInfo->default_source_name));
        pCbCtx->pszDefaultSource = RTStrDup(pInfo->default_source_name);
    }

    LogRel(("PulseAudio: paEnumServerCb signalling\n"));
    pa_threaded_mainloop_signal(pThis->pMainLoop, 0);
}


static int paEnumerate(PDRVHOSTPULSEAUDIO pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    PDMAUDIOBACKENDCFG Cfg;
    RT_ZERO(Cfg);

    Cfg.cbStreamOut    = sizeof(PULSEAUDIOSTREAM);
    Cfg.cbStreamIn     = sizeof(PULSEAUDIOSTREAM);
    Cfg.cMaxStreamsOut = UINT32_MAX;
    Cfg.cMaxStreamsIn  = UINT32_MAX;

    PULSEAUDIOENUMCBCTX CbCtx;
    RT_ZERO(CbCtx);

    CbCtx.pDrv   = pThis;
    CbCtx.fFlags = fEnum;

    bool fLog = (fEnum & PULSEAUDIOENUMCBFLAGS_LOG);

    LogRel(("PulseAudio: starting server enumeration\n"));
    int rc = paWaitFor(pThis, pa_context_get_server_info(pThis->pContext, paEnumServerCb, &CbCtx), true);
    LogRel(("PulseAudio: server enumeration done rc=%Rrc abort=%d\n", rc, pThis->fAbortEnumLoop));
    if (   RT_SUCCESS(rc)
        && pThis->fAbortEnumLoop)
        rc = VERR_AUDIO_BACKEND_INIT_FAILED; /* error code does not matter */
    if (RT_SUCCESS(rc))
    {
        if (CbCtx.pszDefaultSink)
        {
            if (fLog)
                LogRel2(("PulseAudio: Default output sink is '%s'\n", CbCtx.pszDefaultSink));

            LogRel(("PulseAudio: starting sink enumeration\n"));
            rc = paWaitFor(pThis, pa_context_get_sink_info_by_name(pThis->pContext, CbCtx.pszDefaultSink,
                                                                   paEnumSinkCb, &CbCtx), true);
            LogRel(("PulseAudio: sink enumeration done rc=%Rrc abort=%d\n", rc, pThis->fAbortEnumLoop));
            if (   RT_SUCCESS(rc)
                && pThis->fAbortEnumLoop)
                rc = VERR_AUDIO_BACKEND_INIT_FAILED; /* error code does not matter */
            if (   RT_FAILURE(rc)
                && fLog)
            {
                LogRel(("PulseAudio: Error enumerating properties for default output sink '%s'\n", CbCtx.pszDefaultSink));
            }
        }
        else if (fLog)
            LogRel2(("PulseAudio: No default output sink found\n"));

        if (RT_SUCCESS(rc))
        {
            if (CbCtx.pszDefaultSource)
            {
                if (fLog)
                    LogRel2(("PulseAudio: Default input source is '%s'\n", CbCtx.pszDefaultSource));

                LogRel(("PulseAudio: starting source enumeration\n"));
                rc = paWaitFor(pThis, pa_context_get_source_info_by_name(pThis->pContext, CbCtx.pszDefaultSource,
                                                                         paEnumSourceCb, &CbCtx), true);
                LogRel(("PulseAudio: source enumeration done rc=%Rrc abort=%d\n", rc, pThis->fAbortEnumLoop));
                if (   RT_FAILURE(rc)
                    && fLog)
                {
                    LogRel(("PulseAudio: Error enumerating properties for default input source '%s'\n", CbCtx.pszDefaultSource));
                }
            }
            else if (fLog)
                LogRel2(("PulseAudio: No default input source found\n"));
        }

        if (RT_SUCCESS(rc))
        {
            if (fLog)
            {
                LogRel2(("PulseAudio: Found %RU8 host playback device(s)\n",  CbCtx.cDevOut));
                LogRel2(("PulseAudio: Found %RU8 host capturing device(s)\n", CbCtx.cDevIn));
            }

            if (pCfg)
                memcpy(pCfg, &Cfg, sizeof(PDMAUDIOBACKENDCFG));
        }

        if (CbCtx.pszDefaultSink)
        {
            RTStrFree(CbCtx.pszDefaultSink);
            CbCtx.pszDefaultSink = NULL;
        }

        if (CbCtx.pszDefaultSource)
        {
            RTStrFree(CbCtx.pszDefaultSource);
            CbCtx.pszDefaultSource = NULL;
        }
    }
    else if (fLog)
        LogRel(("PulseAudio: Error enumerating PulseAudio server properties\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int paDestroyStreamIn(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA)
{
    LogFlowFuncEnter();

    if (pStreamPA->pStream)
    {
        pa_threaded_mainloop_lock(pThis->pMainLoop);

        pa_stream_disconnect(pStreamPA->pStream);
        pa_stream_unref(pStreamPA->pStream);

        pStreamPA->pStream = NULL;

        pa_threaded_mainloop_unlock(pThis->pMainLoop);
    }

    return VINF_SUCCESS;
}


static int paDestroyStreamOut(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA)
{
    if (pStreamPA->pStream)
    {
        pa_threaded_mainloop_lock(pThis->pMainLoop);

        /* Make sure to cancel a pending draining operation, if any. */
        if (pStreamPA->pDrainOp)
        {
            pa_operation_cancel(pStreamPA->pDrainOp);
            pStreamPA->pDrainOp = NULL;
        }

        pa_stream_disconnect(pStreamPA->pStream);
        pa_stream_unref(pStreamPA->pStream);

        pStreamPA->pStream = NULL;

        pa_threaded_mainloop_unlock(pThis->pMainLoop);
    }

    return VINF_SUCCESS;
}


static int paControlStreamOut(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);

            if (   pStreamPA->pDrainOp
                && pa_operation_get_state(pStreamPA->pDrainOp) != PA_OPERATION_DONE)
            {
                pa_operation_cancel(pStreamPA->pDrainOp);
                pa_operation_unref(pStreamPA->pDrainOp);

                pStreamPA->pDrainOp = NULL;
            }
            else
            {
                rc = paWaitFor(pThis, pa_stream_cork(pStreamPA->pStream, 0, paStreamCbSuccess, pStreamPA));
            }

            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /* Pause audio output (the Pause bit of the AC97 x_CR register is set).
             * Note that we must return immediately from here! */
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            if (!pStreamPA->pDrainOp)
            {
                rc = paWaitFor(pThis, pa_stream_trigger(pStreamPA->pStream, paStreamCbSuccess, pStreamPA));
                if (RT_SUCCESS(rc))
                    pStreamPA->pDrainOp = pa_stream_drain(pStreamPA->pStream, paStreamCbDrain, pStreamPA);
            }
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int paControlStreamIn(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            rc = paWaitFor(pThis, pa_stream_cork(pStreamPA->pStream, 0 /* Play / resume */, paStreamCbSuccess, pStreamPA));
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            pa_threaded_mainloop_lock(pThis->pMainLoop);
            if (pStreamPA->pu8PeekBuf) /* Do we need to drop the peek buffer?*/
            {
                pa_stream_drop(pStreamPA->pStream);
                pStreamPA->pu8PeekBuf = NULL;
            }

            rc = paWaitFor(pThis, pa_stream_cork(pStreamPA->pStream, 1 /* Stop / pause */, paStreamCbSuccess, pStreamPA));
            pa_threaded_mainloop_unlock(pThis->pMainLoop);
            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostPulseAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    AssertPtrReturnVoid(pInterface);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    LogFlowFuncEnter();

    if (pThis->pMainLoop)
        pa_threaded_mainloop_stop(pThis->pMainLoop);

    if (pThis->pContext)
    {
        pa_context_disconnect(pThis->pContext);
        pa_context_unref(pThis->pContext);
        pThis->pContext = NULL;
    }

    if (pThis->pMainLoop)
    {
        pa_threaded_mainloop_free(pThis->pMainLoop);
        pThis->pMainLoop = NULL;
    }

    LogFlowFuncLeave();
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostPulseAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    return paEnumerate(pThis, pBackendCfg, PULSEAUDIOENUMCBFLAGS_LOG /* fEnum */);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostPulseAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = paCreateStreamIn (pThis, pStreamPA, pCfgReq, pCfgAcq);
    else if (pCfgReq->enmDir == PDMAUDIODIR_OUT)
        rc = paCreateStreamOut(pThis, pStreamPA, pCfgReq, pCfgAcq);
    else
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);

    if (RT_SUCCESS(rc))
    {
        pStreamPA->pCfg = DrvAudioHlpStreamCfgDup(pCfgAcq);
        if (!pStreamPA->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    if (!pStreamPA->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = paDestroyStreamIn (pThis, pStreamPA);
    else if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_OUT)
        rc = paDestroyStreamOut(pThis, pStreamPA);
    else
        AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

    if (RT_SUCCESS(rc))
    {
        DrvAudioHlpStreamCfgFree(pStreamPA->pCfg);
        pStreamPA->pCfg = NULL;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamControl(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    if (!pStreamPA->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = paControlStreamIn (pThis, pStreamPA, enmStreamCmd);
    else if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_OUT)
        rc = paControlStreamOut(pThis, pStreamPA, enmStreamCmd);
    else
        AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

    return rc;
}


static uint32_t paStreamGetAvail(PDRVHOSTPULSEAUDIO pThis, PPULSEAUDIOSTREAM pStreamPA)
{
    pa_threaded_mainloop_lock(pThis->pMainLoop);

    uint32_t cbAvail = 0;

    if (PA_STREAM_IS_GOOD(pa_stream_get_state(pStreamPA->pStream)))
    {
        if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_IN)
        {
            cbAvail = (uint32_t)pa_stream_readable_size(pStreamPA->pStream);
            Log3Func(("cbReadable=%RU32\n", cbAvail));
        }
        else if (pStreamPA->pCfg->enmDir == PDMAUDIODIR_OUT)
        {
            size_t cbWritable = pa_stream_writable_size(pStreamPA->pStream);
            Log3Func(("cbWritable=%zu, cbMinReq=%RU32\n", cbWritable, pStreamPA->BufAttr.minreq));

            if (cbWritable >= pStreamPA->BufAttr.minreq)
                cbAvail = (uint32_t)cbWritable;
        }
        else
            AssertFailed();
    }

    pa_threaded_mainloop_unlock(pThis->pMainLoop);

    return cbAvail;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostPulseAudioStreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    return paStreamGetAvail(pThis, pStreamPA);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostPulseAudioStreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVHOSTPULSEAUDIO pThis     = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);
    PPULSEAUDIOSTREAM  pStreamPA = (PPULSEAUDIOSTREAM)pStream;

    return paStreamGetAvail(pThis, pStreamPA);
}



/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostPulseAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    RT_NOREF(pStream);

    PDRVHOSTPULSEAUDIO pThis = PDMIHOSTAUDIO_2_DRVHOSTPULSEAUDIO(pInterface);

    PDMAUDIOSTRMSTS strmSts  = PDMAUDIOSTRMSTS_FLAG_NONE;

    /* Check PulseAudio's general status. */
    if (   pThis->pContext
        && PA_CONTEXT_IS_GOOD(pa_context_get_state(pThis->pContext)))
    {
       strmSts = PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED;
    }

    return strmSts;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostPulseAudioStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for PulseAudio. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostPulseAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    AssertPtrReturn(pInterface, NULL);
    AssertPtrReturn(pszIID, NULL);

    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTPULSEAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPULSEAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}


/**
 * Destructs a PulseAudio Audio driver instance.
 *
 * @copydoc FNPDMDRVDESTRUCT
 */
static DECLCALLBACK(void) drvHostPulseAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    LogFlowFuncEnter();
}


/**
 * Constructs a PulseAudio Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostPulseAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);

    PDRVHOSTPULSEAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTPULSEAUDIO);
    LogRel(("Audio: Initializing PulseAudio driver\n"));

    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostPulseAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostPulseAudio);

    return VINF_SUCCESS;
}


/**
 * Pulse audio driver registration record.
 */
const PDMDRVREG g_DrvHostPulseAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "PulseAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Pulse Audio host driver",
    /* fFlags */
     PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTPULSEAUDIO),
    /* pfnConstruct */
    drvHostPulseAudioConstruct,
    /* pfnDestruct */
    drvHostPulseAudioDestruct,
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

#if 0 /* unused */
static struct audio_option pulse_options[] =
{
    {"DAC_MS", AUD_OPT_INT, &s_pulseCfg.buffer_msecs_out,
     "DAC period size in milliseconds", NULL, 0},
    {"ADC_MS", AUD_OPT_INT, &s_pulseCfg.buffer_msecs_in,
     "ADC period size in milliseconds", NULL, 0}
};
#endif

