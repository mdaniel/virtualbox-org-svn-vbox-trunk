/* $Id$ */
/** @file
 * HDAStream.cpp - Stream functions for HD Audio.
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include <iprt/mem.h>
#include <iprt/semaphore.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"

#include "DevHDA.h"
#include "HDAStream.h"


#ifdef IN_RING3

/**
 * Creates an HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to create.
 * @param   pThis               HDA state to assign the HDA stream to.
 * @param   u8SD                Stream descriptor number to assign.
 */
int hdaStreamCreate(PHDASTREAM pStream, PHDASTATE pThis, uint8_t u8SD)
{
    RT_NOREF(pThis);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    pStream->u8SD           = u8SD;
    pStream->pMixSink       = NULL;
    pStream->pHDAState      = pThis;

    pStream->State.fInReset = false;
#ifdef HDA_USE_DMA_ACCESS_HANDLER
    RTListInit(&pStream->State.lstDMAHandlers);
#endif

    int rc = RTCircBufCreate(&pStream->State.pCircBuf, _64K); /** @todo Make this configurable. */
    if (RT_SUCCESS(rc))
    {
        rc = hdaStreamPeriodCreate(&pStream->State.Period);
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&pStream->State.CritSect);
    }

    int rc2;

#ifdef DEBUG
    rc2 = RTCritSectInit(&pStream->Dbg.CritSect);
    AssertRC(rc2);
#endif

    pStream->Dbg.Runtime.fEnabled = pThis->Dbg.fEnabled;

    if (pStream->Dbg.Runtime.fEnabled)
    {
        char szFile[64];

        if (pStream->State.Cfg.enmDir == PDMAUDIODIR_IN)
            RTStrPrintf(szFile, sizeof(szFile), "hdaStreamWriteSD%RU8", pStream->u8SD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "hdaStreamReadSD%RU8", pStream->u8SD);

        char szPath[RTPATH_MAX + 1];
        rc2 = DrvAudioHlpGetFileName(szPath, sizeof(szPath), pThis->Dbg.szOutPath, szFile,
                                     0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAG_NONE);
        AssertRC(rc2);
        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAG_NONE, &pStream->Dbg.Runtime.pFileStream);
        AssertRC(rc2);

        if (pStream->State.Cfg.enmDir == PDMAUDIODIR_IN)
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMAWriteSD%RU8", pStream->u8SD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMAReadSD%RU8", pStream->u8SD);

        rc2 = DrvAudioHlpGetFileName(szPath, sizeof(szPath), pThis->Dbg.szOutPath, szFile,
                                     0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAG_NONE);
        AssertRC(rc2);

        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAG_NONE, &pStream->Dbg.Runtime.pFileDMA);
        AssertRC(rc2);

        /* Delete stale debugging files from a former run. */
        DrvAudioHlpFileDelete(pStream->Dbg.Runtime.pFileStream);
        DrvAudioHlpFileDelete(pStream->Dbg.Runtime.pFileDMA);
    }

    return rc;
}

/**
 * Destroys an HDA stream.
 *
 * @param   pStream             HDA stream to destroy.
 */
void hdaStreamDestroy(PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);

    LogFlowFunc(("[SD%RU8]: Destroying ...\n", pStream->u8SD));

    hdaStreamMapDestroy(&pStream->State.Mapping);

    int rc2;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    rc2 = hdaStreamAsyncIODestroy(pStream);
    AssertRC(rc2);
#endif

    rc2 = RTCritSectDelete(&pStream->State.CritSect);
    AssertRC(rc2);

    if (pStream->State.pCircBuf)
    {
        RTCircBufDestroy(pStream->State.pCircBuf);
        pStream->State.pCircBuf = NULL;
    }

    hdaStreamPeriodDestroy(&pStream->State.Period);

#ifdef DEBUG
    rc2 = RTCritSectDelete(&pStream->Dbg.CritSect);
    AssertRC(rc2);
#endif

    if (pStream->Dbg.Runtime.fEnabled)
    {
        DrvAudioHlpFileDestroy(pStream->Dbg.Runtime.pFileStream);
        pStream->Dbg.Runtime.pFileStream = NULL;

        DrvAudioHlpFileDestroy(pStream->Dbg.Runtime.pFileDMA);
        pStream->Dbg.Runtime.pFileDMA = NULL;
    }

    LogFlowFuncLeave();
}

/**
 * Initializes an HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to initialize.
 * @param   uSD                 SD (stream descriptor) number to assign the HDA stream to.
 */
int hdaStreamInit(PHDASTREAM pStream, uint8_t uSD)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PHDASTATE pThis = pStream->pHDAState;
    AssertPtr(pThis);

    pStream->u8SD       = uSD;
    pStream->u64BDLBase = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, pStream->u8SD),
                                      HDA_STREAM_REG(pThis, BDPU, pStream->u8SD));
    pStream->u16LVI     = HDA_STREAM_REG(pThis, LVI, pStream->u8SD);
    pStream->u32CBL     = HDA_STREAM_REG(pThis, CBL, pStream->u8SD);
    pStream->u16FIFOS   = HDA_STREAM_REG(pThis, FIFOS, pStream->u8SD) + 1;

    /* Make sure to also update the stream's DMA counter (based on its current LPIB value). */
    hdaStreamSetPosition(pStream, HDA_STREAM_REG(pThis, LPIB, pStream->u8SD));

    PPDMAUDIOSTREAMCFG pCfg = &pStream->State.Cfg;

    int rc = hdaSDFMTToPCMProps(HDA_STREAM_REG(pThis, FMT, uSD), &pCfg->Props);
    if (RT_FAILURE(rc))
    {
        LogRel(("HDA: Warning: Format 0x%x for stream #%RU8 not supported\n", HDA_STREAM_REG(pThis, FMT, uSD), uSD));
        return rc;
    }

    /* Set the stream's direction. */
    pCfg->enmDir = hdaGetDirFromSD(pStream->u8SD);

    /* The the stream's name, based on the direction. */
    switch (pCfg->enmDir)
    {
        case PDMAUDIODIR_IN:
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
#  error "Implement me!"
# else
            pCfg->DestSource.Source = PDMAUDIORECSOURCE_LINE;
            pCfg->enmLayout         = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
            RTStrCopy(pCfg->szName, sizeof(pCfg->szName), "Line In");
# endif
            break;

        case PDMAUDIODIR_OUT:
            /* Destination(s) will be set in hdaAddStreamOut(),
             * based on the channels / stream layout. */
            break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    /*
     * Initialize the stream mapping in any case, regardless if
     * we support surround audio or not. This is needed to handle
     * the supported channels within a single audio stream, e.g. mono/stereo.
     *
     * In other words, the stream mapping *always* knows the real
     * number of channels in a single audio stream.
     */
    rc = hdaStreamMapInit(&pStream->State.Mapping, &pCfg->Props);
    AssertRCReturn(rc, rc);

    LogFunc(("[SD%RU8] DMA @ 0x%x (%RU32 bytes), LVI=%RU16, FIFOS=%RU16, rc=%Rrc\n",
             pStream->u8SD, pStream->u64BDLBase, pStream->u32CBL, pStream->u16LVI, pStream->u16FIFOS, rc));

    if (   pStream->u32CBL
        && pStream->u16LVI)
    {
        /* Make sure that the chosen Hz rate dividable by the stream's rate. */
        if (pStream->State.Cfg.Props.uHz % pThis->u16TimerHz != 0)
            LogRel(("HDA: Device timer (%RU32) does not fit to stream #RU8 timing (%RU32)\n",
                    pThis->u16TimerHz, pStream->State.Cfg.Props.uHz));

        /** @todo Use a more dynamic fragment size? */
        uint8_t cFragments = pStream->u16LVI;
        if (cFragments <= 1)
            cFragments = 2; /* At least two fragments (BDLEs) must be present. */

        /* Calculate the fragment size the guest OS expects interrupt delivery at. */
        pStream->State.cbTransferSize = pStream->u32CBL / cFragments;
        Assert(pStream->State.cbTransferSize);
        Assert(pStream->State.cbTransferSize % HDA_FRAME_SIZE == 0);

        /* Calculate the bytes we need to transfer to / from the stream's DMA per iteration.
         * This is bound to the device's Hz rate and thus to the (virtual) timing the device expects. */
        pStream->State.cbTransferChunk = (pStream->State.Cfg.Props.uHz / pThis->u16TimerHz) * HDA_FRAME_SIZE;
        Assert(pStream->State.cbTransferChunk);
        Assert(pStream->State.cbTransferChunk % HDA_FRAME_SIZE == 0);

        /* Make sure that the transfer chunk does not exceed the overall transfer size. */
        if (pStream->State.cbTransferChunk > pStream->State.cbTransferSize)
            pStream->State.cbTransferChunk = pStream->State.cbTransferSize;

        pStream->State.cbTransferProcessed        = 0;
        pStream->State.cTransferPendingInterrupts = 0;

        const uint64_t cTicksPerHz = TMTimerGetFreq(pThis->pTimer) / pThis->u16TimerHz;

        /* Calculate the timer ticks per byte for this stream. */
        pStream->State.cTicksPerByte = cTicksPerHz / pStream->State.cbTransferChunk;
        Assert(pStream->State.cTicksPerByte);

        /* Calculate timer ticks per transfer. */
        pStream->State.cTransferTicks     = pStream->State.cbTransferChunk * pStream->State.cTicksPerByte;

        /* Initialize the transfer timestamps. */
        pStream->State.tsTransferLast     = 0;
        pStream->State.tsTransferNext     = 0;

        LogFunc(("[SD%RU8] Timer %uHz (%RU64 ticks per Hz), ticks per byte: %RU64 (%RU64 bytes per iteration = %RU64 ticks)," \
                 " transfer size = %RU64\n",
                 pStream->u8SD, pThis->u16TimerHz, cTicksPerHz, pStream->State.cTicksPerByte,
                 pStream->State.cbTransferChunk, pStream->State.cTransferTicks, pStream->State.cbTransferSize));
        /*
         * Handle the stream's position adjustment.
         */
        uint32_t cfPosAdjust = 0;

        if (pThis->fPosAdjustEnabled) /* Is the position adjustment enabled at all? */
        {
            /* If no custom set position adjustment is set, apply some
             * simple heuristics to detect the appropriate position adjustment. */
            if (   pStream->u64BDLBase
                && !pThis->cPosAdjustFrames)
            {
                HDABDLE BDLE;
                int rc2 = hdaBDLEFetch(pThis, &BDLE, pStream->u64BDLBase, 0 /* Entry */);
                AssertRC(rc2);

                /** @todo Implement / use a (dynamic) table once this gets more complicated. */

#ifdef VBOX_WITH_INTEL_HDA
                /* Intel ICH / PCH: 1 frame. */
                if (BDLE.Desc.u32BufSize == 1 * HDA_FRAME_SIZE)
                {
                    cfPosAdjust = 1;
                }
                /* Intel Baytrail / Braswell: 32 frames. */
                else if (BDLE.Desc.u32BufSize == 32 * HDA_FRAME_SIZE)
                {
                    cfPosAdjust = 32;
                }
                else
#endif
                    cfPosAdjust = pThis->cPosAdjustFrames;
            }
            else /* Go with the set default. */
                cfPosAdjust = pThis->cPosAdjustFrames;

            if (cfPosAdjust)
            {
                /* Initialize position adjustment counter. */
                pStream->State.cPosAdjustFramesLeft = cfPosAdjust;
                LogRel2(("HDA: Position adjustment for stream #%RU8 active (%RU32 frames)\n", pStream->u8SD, cfPosAdjust));
            }
        }

        LogFunc(("[SD%RU8] cfPosAdjust=%RU32\n", pStream->u8SD, cfPosAdjust));
    }

    return rc;
}

/**
 * Resets an HDA stream.
 *
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to reset.
 * @param   uSD                 Stream descriptor (SD) number to use for this stream.
 */
void hdaStreamReset(PHDASTATE pThis, PHDASTREAM pStream, uint8_t uSD)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStream);
    AssertReturnVoid(uSD < HDA_MAX_STREAMS);

# ifdef VBOX_STRICT
    AssertReleaseMsg(!RT_BOOL(HDA_STREAM_REG(pThis, CTL, uSD) & HDA_SDCTL_RUN),
                     ("[SD%RU8] Cannot reset stream while in running state\n", uSD));
# endif

    LogFunc(("[SD%RU8]: Reset\n", uSD));

    /*
     * Set reset state.
     */
    Assert(ASMAtomicReadBool(&pStream->State.fInReset) == false); /* No nested calls. */
    ASMAtomicXchgBool(&pStream->State.fInReset, true);

    /*
     * Second, initialize the registers.
     */
    HDA_STREAM_REG(pThis, STS,   uSD) = HDA_SDSTS_FIFORDY;
    /* According to the ICH6 datasheet, 0x40000 is the default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRST bit. */
    HDA_STREAM_REG(pThis, CTL,   uSD) = 0x40000 | (HDA_STREAM_REG(pThis, CTL, uSD) & HDA_SDCTL_SRST);
    /* ICH6 defines default values (120 bytes for input and 192 bytes for output descriptors) of FIFO size. 18.2.39. */
    HDA_STREAM_REG(pThis, FIFOS, uSD) = hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN ? HDA_SDIFIFO_120B : HDA_SDOFIFO_192B;
    /* See 18.2.38: Always defaults to 0x4 (32 bytes). */
    HDA_STREAM_REG(pThis, FIFOW, uSD) = HDA_SDFIFOW_32B;
    HDA_STREAM_REG(pThis, LPIB,  uSD) = 0;
    HDA_STREAM_REG(pThis, CBL,   uSD) = 0;
    HDA_STREAM_REG(pThis, LVI,   uSD) = 0;
    HDA_STREAM_REG(pThis, FMT,   uSD) = 0;
    HDA_STREAM_REG(pThis, BDPU,  uSD) = 0;
    HDA_STREAM_REG(pThis, BDPL,  uSD) = 0;

#ifdef HDA_USE_DMA_ACCESS_HANDLER
    hdaStreamUnregisterDMAHandlers(pThis, pStream);
#endif

    pStream->State.tsTransferLast = 0;
    pStream->State.tsTransferNext = 0;

    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    if (pStream->State.pCircBuf)
        RTCircBufReset(pStream->State.pCircBuf);

    /* Reset stream map. */
    hdaStreamMapReset(&pStream->State.Mapping);

    /* (Re-)initialize the stream with current values. */
    int rc2 = hdaStreamInit(pStream, uSD);
    AssertRC(rc2);

    /* Reset the stream's period. */
    hdaStreamPeriodReset(&pStream->State.Period);

#ifdef DEBUG
    pStream->Dbg.cReadsTotal      = 0;
    pStream->Dbg.cbReadTotal      = 0;
    pStream->Dbg.tsLastReadNs     = 0;
    pStream->Dbg.cWritesTotal     = 0;
    pStream->Dbg.cbWrittenTotal   = 0;
    pStream->Dbg.cWritesHz        = 0;
    pStream->Dbg.cbWrittenHz      = 0;
    pStream->Dbg.tsWriteSlotBegin = 0;
#endif

    /* Report that we're done resetting this stream. */
    HDA_STREAM_REG(pThis, CTL,   uSD) = 0;

    LogFunc(("[SD%RU8] Reset\n", uSD));

    /* Exit reset mode. */
    ASMAtomicXchgBool(&pStream->State.fInReset, false);
}

/**
 * Enables or disables an HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to enable or disable.
 * @param   fEnable             Whether to enable or disble the stream.
 */
int hdaStreamEnable(PHDASTREAM pStream, bool fEnable)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFunc(("[SD%RU8]: fEnable=%RTbool, pMixSink=%p\n", pStream->u8SD, fEnable, pStream->pMixSink));

    int rc = VINF_SUCCESS;

    if (pStream->pMixSink) /* Stream attached to a sink? */
    {
        AUDMIXSINKCMD enmCmd = fEnable
                             ? AUDMIXSINKCMD_ENABLE : AUDMIXSINKCMD_DISABLE;

        /* First, enable or disable the stream and the stream's sink, if any. */
        if (pStream->pMixSink->pMixSink)
            rc = AudioMixerSinkCtl(pStream->pMixSink->pMixSink, enmCmd);

        if (   RT_SUCCESS(rc)
            && pStream->Dbg.Runtime.fEnabled)
        {
            if (fEnable)
            {
                int rc2 = DrvAudioHlpFileOpen(pStream->Dbg.Runtime.pFileStream, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStream->State.Cfg.Props);
                AssertRC(rc2);

                rc2 = DrvAudioHlpFileOpen(pStream->Dbg.Runtime.pFileDMA, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                          &pStream->State.Cfg.Props);
                AssertRC(rc2);
            }
            else
            {
                int rc2 = DrvAudioHlpFileClose(pStream->Dbg.Runtime.pFileStream);
                AssertRC(rc2);

                rc2 = DrvAudioHlpFileClose(pStream->Dbg.Runtime.pFileDMA);
                AssertRC(rc2);
            }
        }
    }

    LogFunc(("[SD%RU8] rc=%Rrc\n", pStream->u8SD, rc));
    return rc;
}

uint32_t hdaStreamGetPosition(PHDASTATE pThis, PHDASTREAM pStream)
{
    return HDA_STREAM_REG(pThis, LPIB, pStream->u8SD);
}

/**
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * updating its associated LPIB register and DMA position buffer (if enabled).
 *
 * @param   pStream             HDA stream to update read / write position for.
 * @param   u32LPIB             Absolute position (in bytes) to set current read / write position to.
 */
void hdaStreamSetPosition(PHDASTREAM pStream, uint32_t u32LPIB)
{
    AssertPtrReturnVoid(pStream);

    Assert(u32LPIB % HDA_FRAME_SIZE == 0);

    Log3Func(("[SD%RU8] LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",
              pStream->u8SD, u32LPIB, pStream->pHDAState->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pStream->pHDAState, LPIB, pStream->u8SD) = u32LPIB;

    /* Do we need to tell the current DMA position? */
    if (pStream->pHDAState->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pStream->pHDAState->CTX_SUFF(pDevIns),
                                        pStream->pHDAState->u64DPBase + (pStream->u8SD * 2 * sizeof(uint32_t)),
                                        (void *)&u32LPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }
}

/**
 * Retrieves the available size of (buffered) audio data (in bytes) of a given HDA stream.
 *
 * @returns Available data (in bytes).
 * @param   pStream             HDA stream to retrieve size for.
 */
uint32_t hdaStreamGetUsed(PHDASTREAM pStream)
{
    AssertPtrReturn(pStream, 0);

    if (!pStream->State.pCircBuf)
        return 0;

    return (uint32_t)RTCircBufUsed(pStream->State.pCircBuf);
}

/**
 * Retrieves the free size of audio data (in bytes) of a given HDA stream.
 *
 * @returns Free data (in bytes).
 * @param   pStream             HDA stream to retrieve size for.
 */
uint32_t hdaStreamGetFree(PHDASTREAM pStream)
{
    AssertPtrReturn(pStream, 0);

    if (!pStream->State.pCircBuf)
        return 0;

    return (uint32_t)RTCircBufFree(pStream->State.pCircBuf);
}

/**
 * Returns whether a next transfer for a given stream is scheduled or not.
 *
 * @returns True if a next transfer is scheduled, false if not.
 * @param   pStream             HDA stream to retrieve schedule status for.
 */
bool hdaStreamTransferIsScheduled(PHDASTREAM pStream)
{
    AssertPtrReturn(pStream,            false);
    AssertPtrReturn(pStream->pHDAState, false);

    const bool fScheduled = pStream->State.tsTransferNext > TMTimerGet(pStream->pHDAState->pTimer);

    Log3Func(("[SD%RU8] %RU64 -> %RTbool\n", pStream->u8SD, pStream->State.tsTransferNext, fScheduled));

    return fScheduled;
}

/**
 * Returns the (virtual) clock timestamp of the next transfer, if any.
 * Will return 0 if no new transfer is scheduled.
 *
 * @returns The (virtual) clock timestamp of the next transfer.
 * @param   pStream             HDA stream to retrieve timestamp for.
 */
uint64_t hdaStreamTransferGetNext(PHDASTREAM pStream)
{
    return pStream->State.tsTransferNext;
}

/**
 * Writes audio data from a mixer sink into an HDA stream's DMA buffer.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to write to.
 * @param   pvBuf               Data buffer to write.
 *                              If NULL, silence will be written.
 * @param   cbBuf               Number of bytes of data buffer to write.
 * @param   pcbWritten          Number of bytes written. Optional.
 */
int hdaStreamWrite(PHDASTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /* pvBuf is optional. */
    AssertReturn(cbBuf,      VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    int rc = VINF_SUCCESS;

    uint32_t cbWrittenTotal = 0;
    uint32_t cbLeft         = RT_MIN(cbBuf, (uint32_t)RTCircBufFree(pCircBuf));

    while (cbLeft)
    {
        void *pvDst;
        size_t cbDst;

        RTCircBufAcquireWriteBlock(pCircBuf, cbLeft, &pvDst, &cbDst);

        if (cbDst)
        {
            if (pvBuf)
            {
                memcpy(pvDst, (uint8_t *)pvBuf + cbWrittenTotal, cbDst);
            }
            else /* Send silence. */
            {
                /** @todo Use a sample spec for "silence" based on the PCM parameters.
                 *        For now we ASSUME that silence equals NULLing the data. */
                RT_BZERO(pvDst, cbDst);
            }

            if (pStream->Dbg.Runtime.fEnabled)
                DrvAudioHlpFileWrite(pStream->Dbg.Runtime.pFileStream, pvDst, cbDst, 0 /* fFlags */);
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbDst);

        if (RT_FAILURE(rc))
            break;

        Assert(cbLeft  >= (uint32_t)cbDst);
        cbLeft         -= (uint32_t)cbDst;

        cbWrittenTotal += (uint32_t)cbDst;
    }

    if (pcbWritten)
        *pcbWritten = cbWrittenTotal;

    return rc;
}


/**
 * Reads audio data from an HDA stream's DMA buffer and writes into a specified mixer sink.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to read audio data from.
 * @param   cbToRead            Number of bytes to read.
 * @param   pcbRead             Number of bytes read. Optional.
 */
int hdaStreamRead(PHDASTREAM pStream, uint32_t cbToRead, uint32_t *pcbRead)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertReturn(cbToRead,   VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PHDAMIXERSINK pSink = pStream->pMixSink;
    if (!pSink)
    {
        AssertMsgFailed(("[SD%RU8]: Can't read from a stream with no sink attached\n", pStream->u8SD));

        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;
    uint32_t cbLeft      = RT_MIN(cbToRead, (uint32_t)RTCircBufUsed(pCircBuf));

    while (cbLeft)
    {
        void *pvSrc;
        size_t cbSrc;

        uint32_t cbWritten = 0;

        RTCircBufAcquireReadBlock(pCircBuf, cbLeft, &pvSrc, &cbSrc);

        if (cbSrc)
        {
            if (pStream->Dbg.Runtime.fEnabled)
                DrvAudioHlpFileWrite(pStream->Dbg.Runtime.pFileStream, pvSrc, cbSrc, 0 /* fFlags */);

            rc = AudioMixerSinkWrite(pSink->pMixSink, AUDMIXOP_COPY, pvSrc, (uint32_t)cbSrc, &cbWritten);
            AssertRC(rc);

            Assert(cbSrc >= cbWritten);
            Log2Func(("[SD%RU8]: %zu/%zu bytes read\n", pStream->u8SD, cbWritten, cbSrc));
        }

        RTCircBufReleaseReadBlock(pCircBuf, cbWritten);

        if (RT_FAILURE(rc))
            break;

        Assert(cbLeft  >= cbWritten);
        cbLeft         -= cbWritten;

        cbReadTotal    += cbWritten;
    }

    if (pcbRead)
        *pcbRead = cbReadTotal;

    return rc;
}

/**
 * Transfers data of an HDA stream according to its usage (input / output).
 *
 * For an SDO (output) stream this means reading DMA data from the device to
 * the HDA stream's internal FIFO buffer.
 *
 * For an SDI (input) stream this is reading audio data from the HDA stream's
 * internal FIFO buffer and writing it as DMA data to the device.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to update.
 * @param   cbToProcessMax      How much data (in bytes) to process as maximum.
 */
int hdaStreamTransfer(PHDASTREAM pStream, uint32_t cbToProcessMax)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    hdaStreamLock(pStream);

    PHDASTATE pThis = pStream->pHDAState;
    AssertPtr(pThis);

    PHDASTREAMPERIOD pPeriod = &pStream->State.Period;
    int rc = hdaStreamPeriodLock(pPeriod);
    AssertRC(rc);

    bool fProceed = true;

    /* Stream not running? */
    if (!(HDA_STREAM_REG(pThis, CTL, pStream->u8SD) & HDA_SDCTL_RUN))
    {
        Log3Func(("[SD%RU8] RUN bit not set\n", pStream->u8SD));
        fProceed = false;
    }
    else if (HDA_STREAM_REG(pThis, STS, pStream->u8SD) & HDA_SDSTS_BCIS)
    {
        Log3Func(("[SD%RU8] BCIS bit set\n", pStream->u8SD));
        fProceed = false;
    }

    if (!fProceed)
    {
        hdaStreamPeriodUnlock(pPeriod);
        hdaStreamUnlock(pStream);
        return VINF_SUCCESS;
    }

    const uint64_t tsNow = TMTimerGet(pThis->pTimer);

    if (!pStream->State.tsTransferLast)
        pStream->State.tsTransferLast = tsNow;

#ifdef DEBUG
    const int64_t iTimerDelta = tsNow - pStream->State.tsTransferLast;
    Log3Func(("[SD%RU8] Time now=%RU64, last=%RU64 -> %RI64 ticks delta\n",
              pStream->u8SD, tsNow, pStream->State.tsTransferLast, iTimerDelta));
#endif

    pStream->State.tsTransferLast = tsNow;

    /* Sanity checks. */
    Assert(pStream->u8SD < HDA_MAX_STREAMS);
    Assert(pStream->u64BDLBase);
    Assert(pStream->u32CBL);
    Assert(pStream->u16FIFOS);

    /* State sanity checks. */
    Assert(ASMAtomicReadBool(&pStream->State.fInReset) == false);

    /* Fetch first / next BDL entry. */
    PHDABDLE pBDLE = &pStream->State.BDLE;
    if (hdaBDLEIsComplete(pBDLE))
    {
        rc = hdaBDLEFetch(pThis, pBDLE, pStream->u64BDLBase, pStream->State.uCurBDLE);
        AssertRC(rc);
    }

    uint32_t cbToProcess = RT_MIN(pStream->State.cbTransferSize - pStream->State.cbTransferProcessed,
                                  pStream->State.cbTransferChunk);

    Log3Func(("[SD%RU8] cbToProcess=%RU32, cbToProcessMax=%RU32\n", pStream->u8SD, cbToProcess, cbToProcessMax));

    if (cbToProcess > cbToProcessMax)
    {
        if (pStream->State.Cfg.enmDir == PDMAUDIODIR_IN)
            LogRelMax2(64, ("HDA: Warning: FIFO underflow for stream #%RU8 (still %RU32 bytes needed)\n",
                            pStream->u8SD, cbToProcess - cbToProcessMax));
        else
            LogRelMax2(64, ("HDA: Warning: FIFO overflow for stream #%RU8 (%RU32 bytes outstanding)\n",
                            pStream->u8SD, cbToProcess - cbToProcessMax));

        LogFunc(("[SD%RU8] Warning: Limiting transfer (cbToProcess=%RU32, cbToProcessMax=%RU32)\n",
                 pStream->u8SD, cbToProcess, cbToProcessMax));

        /* Never process more than a stream currently can handle. */
        cbToProcess = cbToProcessMax;
    }

    uint32_t cbProcessed = 0;
    uint32_t cbLeft      = cbToProcess;
    Assert(cbLeft % HDA_FRAME_SIZE == 0);

    uint8_t abChunk[HDA_FIFO_MAX + 1];
    while (cbLeft)
    {
        /* Limit the chunk to the stream's FIFO size and what's left to process. */
        uint32_t cbChunk = RT_MIN(cbLeft, pStream->u16FIFOS);

        /* Limit the chunk to the remaining data of the current BDLE. */
        cbChunk = RT_MIN(cbChunk, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

        /* If there are position adjustment frames left to be processed,
         * make sure that we process them first as a whole. */
        if (pStream->State.cPosAdjustFramesLeft)
            cbChunk = RT_MIN(cbChunk, uint32_t(pStream->State.cPosAdjustFramesLeft * HDA_FRAME_SIZE));

        Log3Func(("[SD%RU8] cbChunk=%RU32, cPosAdjustFramesLeft=%RU16\n",
                  pStream->u8SD, cbChunk, pStream->State.cPosAdjustFramesLeft));

        if (!cbChunk)
            break;

        uint32_t   cbDMA    = 0;
        PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;

        if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_IN) /* Input (SDI). */
        {
            STAM_PROFILE_START(&pThis->StatIn, a);

            uint32_t cbDMAWritten = 0;
            uint32_t cbDMAToWrite = cbChunk;

            while (cbDMAToWrite)
            {
                void *pvBuf; size_t cbBuf;
                RTCircBufAcquireReadBlock(pCircBuf, cbDMAToWrite, &pvBuf, &cbBuf);

                if (   !cbBuf
                    && !RTCircBufUsed(pCircBuf))
                    break;

                memcpy(abChunk + cbDMAWritten, pvBuf, cbBuf);

                RTCircBufReleaseReadBlock(pCircBuf, cbBuf);

                Assert(cbDMAToWrite >= cbBuf);
                cbDMAToWrite -= (uint32_t)cbBuf;
                cbDMAWritten += (uint32_t)cbBuf;
                Assert(cbDMAWritten <= cbChunk);
            }

            if (cbDMAToWrite)
            {
                Assert(cbChunk == cbDMAWritten + cbDMAToWrite);
                memset((uint8_t *)abChunk + cbDMAWritten, 0, cbDMAToWrite);
                cbDMAWritten = cbChunk;
            }

            rc = hdaDMAWrite(pThis, pStream, abChunk, cbDMAWritten, &cbDMA /* pcbWritten */);
            if (RT_FAILURE(rc))
                LogRel(("HDA: Writing to stream #%RU8 DMA failed with %Rrc\n", pStream->u8SD, rc));

            STAM_PROFILE_STOP(&pThis->StatIn, a);
        }
        else if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
        {
            STAM_PROFILE_START(&pThis->StatOut, a);

            rc = hdaDMARead(pThis, pStream, abChunk, cbChunk, &cbDMA /* pcbRead */);
            if (RT_SUCCESS(rc))
            {
                uint32_t cbDMAWritten = 0;
                uint32_t cbDMALeft    = cbDMA;

                if (cbDMALeft > RTCircBufFree(pCircBuf))
                {
                    /* Try to read as much as possible. */
                    cbDMALeft = (uint32_t)RTCircBufFree(pCircBuf);

                    rc = VERR_BUFFER_OVERFLOW;
                }

                while (cbDMALeft)
                {
                    void *pvBuf; size_t cbBuf;
                    RTCircBufAcquireWriteBlock(pCircBuf, cbDMALeft, &pvBuf, &cbBuf);

                    if (cbBuf)
                        memcpy(pvBuf, abChunk + cbDMAWritten, cbBuf);

                    RTCircBufReleaseWriteBlock(pCircBuf, cbBuf);

                    cbDMALeft    -= (uint32_t)cbBuf;
                    cbDMAWritten += (uint32_t)cbBuf;
                }
            }
            else
                LogRel(("HDA: Reading from stream #%RU8 DMA failed with %Rrc\n", pStream->u8SD, rc));

            STAM_PROFILE_STOP(&pThis->StatOut, a);
        }

        else /** @todo Handle duplex streams? */
            AssertFailed();

        if (cbDMA)
        {
            Assert(cbDMA % HDA_FRAME_SIZE == 0);

            /* We always increment the position of DMA buffer counter because we're always reading
             * into an intermediate buffer. */
            pBDLE->State.u32BufOff += (uint32_t)cbDMA;
            Assert(pBDLE->State.u32BufOff <= pBDLE->Desc.u32BufSize);

            /* Are we done doing the position adjustment?
             * Only then do the transfer accounting .*/
            if (pStream->State.cPosAdjustFramesLeft == 0)
            {
                Assert(cbLeft >= cbDMA);
                cbLeft        -= cbDMA;

                cbProcessed   += cbDMA;
            }

            /**
             * Update the stream's current position.
             * Do this as accurate and close to the actual data transfer as possible.
             * All guetsts rely on this, depending on the mechanism they use (LPIB register or DMA counters).
             */
            uint32_t cbStreamPos = hdaStreamGetPosition(pThis, pStream);
            if (cbStreamPos == pStream->u32CBL)
                cbStreamPos = 0;

            hdaStreamSetPosition(pStream, cbStreamPos + cbDMA);
        }

        if (hdaBDLEIsComplete(pBDLE))
        {
            Log3Func(("[SD%RU8] Complete: %R[bdle]\n", pStream->u8SD, pBDLE));

                /* Does the current BDLE require an interrupt to be sent? */
            if (   hdaBDLENeedsInterrupt(pBDLE)
                /* Are we done doing the position adjustment?
                 * It can happen that a BDLE which is handled while doing the
                 * position adjustment requires an interrupt on completion (IOC) being set.
                 *
                 * In such a case we need to skip such an interrupt and just move on. */
                && pStream->State.cPosAdjustFramesLeft == 0)
            {
                /* If the IOCE ("Interrupt On Completion Enable") bit of the SDCTL register is set
                 * we need to generate an interrupt.
                 */
                if (HDA_STREAM_REG(pThis, CTL, pStream->u8SD) & HDA_SDCTL_IOCE)
                {
                    pStream->State.cTransferPendingInterrupts++;

                    AssertMsg(pStream->State.cTransferPendingInterrupts <= 32,
                              ("Too many pending interrupts (%RU8) for stream #%RU8\n",
                               pStream->State.cTransferPendingInterrupts, pStream->u8SD));
                }
            }

            if (pStream->State.uCurBDLE == pStream->u16LVI)
            {
                pStream->State.uCurBDLE = 0;
            }
            else
                pStream->State.uCurBDLE++;

            /* Fetch the next BDLE entry. */
            hdaBDLEFetch(pThis, pBDLE, pStream->u64BDLBase, pStream->State.uCurBDLE);
        }

        /* Do the position adjustment accounting. */
        pStream->State.cPosAdjustFramesLeft -= RT_MIN(pStream->State.cPosAdjustFramesLeft, cbDMA / HDA_FRAME_SIZE);

        if (RT_FAILURE(rc))
            break;
    }

    Log3Func(("[SD%RU8] cbToProcess=%RU32, cbProcessed=%RU32, cbLeft=%RU32, %R[bdle], rc=%Rrc\n",
              pStream->u8SD, cbToProcess, cbProcessed, cbLeft, pBDLE, rc));

    /* Sanity. */
    Assert(cbProcessed % HDA_FRAME_SIZE == 0);
    Assert(cbProcessed == cbToProcess);
    Assert(cbLeft      == 0);

    /* Only do the data accounting if we don't have to do any position
     * adjustment anymore. */
    if (pStream->State.cPosAdjustFramesLeft == 0)
    {
        hdaStreamPeriodInc(pPeriod, RT_MIN(cbProcessed / HDA_FRAME_SIZE, hdaStreamPeriodGetRemainingFrames(pPeriod)));

        pStream->State.cbTransferProcessed += cbProcessed;
    }

    /* Make sure that we never report more stuff processed than initially announced. */
    if (pStream->State.cbTransferProcessed > pStream->State.cbTransferSize)
        pStream->State.cbTransferProcessed = pStream->State.cbTransferSize;

    uint32_t cbTransferLeft     = pStream->State.cbTransferSize - pStream->State.cbTransferProcessed;
    bool     fTransferComplete  = !cbTransferLeft;
    uint64_t tsTransferNext     = 0;

    if (fTransferComplete)
    {
        /**
         * Try updating the wall clock.
         *
         * Note 1) Only certain guests (like Linux' snd_hda_intel) rely on the WALCLK register
         *         in order to determine the correct timing of the sound device. Other guests
         *         like Windows 7 + 10 (or even more exotic ones like Haiku) will completely
         *         ignore this.
         *
         * Note 2) When updating the WALCLK register too often / early (or even in a non-monotonic
         *         fashion) this *will* upset guest device drivers and will completely fuck up the
         *         sound output. Running VLC on the guest will tell!
         */
        const bool fWalClkSet = hdaWalClkSet(pThis,
                                               hdaWalClkGetCurrent(pThis)
                                             + hdaStreamPeriodFramesToWalClk(pPeriod, pStream->State.cbTransferProcessed / HDA_FRAME_SIZE),
                                             false /* fForce */);
        RT_NOREF(fWalClkSet);
    }

    /* Does the period have any interrupts outstanding? */
    if (pStream->State.cTransferPendingInterrupts)
    {
        Log3Func(("[SD%RU8] Scheduling interrupt\n", pStream->u8SD));

        /**
         * Set the stream's BCIS bit.
         *
         * Note: This only must be done if the whole period is complete, and not if only
         * one specific BDL entry is complete (if it has the IOC bit set).
         *
         * This will otherwise confuses the guest when it 1) deasserts the interrupt,
         * 2) reads SDSTS (with BCIS set) and then 3) too early reads a (wrong) WALCLK value.
         *
         * snd_hda_intel on Linux will tell.
         */
        HDA_STREAM_REG(pThis, STS, pStream->u8SD) |= HDA_SDSTS_BCIS;

        /* Trigger an interrupt first and let hdaRegWriteSDSTS() deal with
         * ending / beginning a period. */
#ifndef DEBUG
        hdaProcessInterrupt(pThis);
#else
        hdaProcessInterrupt(pThis, __FUNCTION__);
#endif
        pStream->State.cTransferPendingInterrupts--;
    }
    else /* Transfer still in-flight -- schedule the next timing slot. */
    {
        uint32_t cbTransferNext = cbTransferLeft;

        /* No data left to transfer anymore or do we have more data left
         * than we can transfer per timing slot? Clamp. */
        if (   !cbTransferNext
            || cbTransferNext > pStream->State.cbTransferChunk)
        {
            cbTransferNext = pStream->State.cbTransferChunk;
        }

        tsTransferNext = tsNow + (cbTransferNext * pStream->State.cTicksPerByte);
    }

    /* If we need to do another transfer, (re-)arm the device timer.  */
    if (tsTransferNext) /* Can be 0 if no next transfer is needed. */
    {
        Log3Func(("[SD%RU8] Scheduling timer\n", pStream->u8SD));

        TMTimerUnlock(pThis->pTimer);

        hdaTimerSet(pThis, tsTransferNext, false /* fForce */);

        TMTimerLock(pThis->pTimer, VINF_SUCCESS);

        pStream->State.tsTransferNext = tsTransferNext;
    }

    pStream->State.tsTransferLast = tsNow;

    Log3Func(("[SD%RU8] cbTransferLeft=%RU32 -- %RU64/%RU64\n",
              pStream->u8SD, cbTransferLeft, pStream->State.cbTransferProcessed, pStream->State.cbTransferSize));
    Log3Func(("[SD%RU8] fTransferComplete=%RTbool, cTransferPendingInterrupts=%RU8\n",
              pStream->u8SD, fTransferComplete, pStream->State.cTransferPendingInterrupts));
    Log3Func(("[SD%RU8] tsNow=%RU64, tsTransferNext=%RU64 (in %RU64 ticks)\n",
              pStream->u8SD, tsNow, tsTransferNext, tsTransferNext - tsNow));

    hdaStreamPeriodUnlock(pPeriod);
    hdaStreamUnlock(pStream);

    return VINF_SUCCESS;
}

/**
 * Updates a HDA stream by doing its required data transfers.
 * The host sink(s) set the overall pace.
 *
 * This routine is called by both, the synchronous and the asynchronous, implementations.
 *
 * @param   pStream             HDA stream to update.
 * @param   fInTimer            Whether to this function was called from the timer
 *                              context or an asynchronous I/O stream thread (if supported).
 */
void hdaStreamUpdate(PHDASTREAM pStream, bool fInTimer)
{
    PAUDMIXSINK pSink = NULL;
    if (   pStream->pMixSink
        && pStream->pMixSink->pMixSink)
    {
        pSink = pStream->pMixSink->pMixSink;
    }

    if (!AudioMixerSinkIsActive(pSink)) /* No sink available? Bail out. */
        return;

    int rc2;

    if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
    {
        /* Is the HDA stream ready to be written (guest output data) to? If so, by how much? */
        const uint32_t cbFree = hdaStreamGetFree(pStream);

        if (   fInTimer
            && cbFree)
        {
            Log3Func(("[SD%RU8] cbFree=%RU32\n", pStream->u8SD, cbFree));

            /* Do the DMA transfer. */
            rc2 = hdaStreamTransfer(pStream, cbFree);
            AssertRC(rc2);
        }

        /* How much (guest output) data is available at the moment for the HDA stream? */
        uint32_t cbUsed = hdaStreamGetUsed(pStream);

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (   fInTimer
            && cbUsed)
        {
            rc2 = hdaStreamAsyncIONotify(pStream);
            AssertRC(rc2);
        }
        else
        {
#endif
            const uint32_t cbSinkWritable = AudioMixerSinkGetWritable(pSink);

            /* Do not write more than the sink can hold at the moment.
             * The host sets the overall pace. */
            if (cbUsed > cbSinkWritable)
                cbUsed = cbSinkWritable;

            if (cbUsed)
            {
                /* Read (guest output) data and write it to the stream's sink. */
                rc2 = hdaStreamRead(pStream, cbUsed, NULL /* pcbRead */);
                AssertRC(rc2);
            }

            /* When running synchronously, update the associated sink here.
             * Otherwise this will be done in the stream's dedicated async I/O thread. */
            rc2 = AudioMixerSinkUpdate(pSink);
            AssertRC(rc2);

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        }
#endif
    }
    else /* Input (SDI). */
    {
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (fInTimer)
        {
#endif
            rc2 = AudioMixerSinkUpdate(pSink);
            AssertRC(rc2);

            /* Is the sink ready to be read (host input data) from? If so, by how much? */
            uint32_t cbReadable = AudioMixerSinkGetReadable(pSink);

            Log3Func(("[SD%RU8] cbReadable=%RU32\n", pStream->u8SD, cbReadable));

            if (cbReadable)
            {
                uint8_t abFIFO[HDA_FIFO_MAX + 1];
                while (cbReadable)
                {
                    uint32_t cbRead;
                    rc2 = AudioMixerSinkRead(pSink, AUDMIXOP_COPY,
                                             abFIFO, RT_MIN(cbReadable, (uint32_t)sizeof(abFIFO)), &cbRead);
                    AssertRCBreak(rc2);

                    /* Write (guest input) data to the stream which was read from stream's sink before. */
                    rc2 = hdaStreamWrite(pStream, abFIFO, cbRead, NULL /* pcbWritten */);
                    AssertRCBreak(rc2);

                    Assert(cbReadable >= cbRead);
                    cbReadable -= cbRead;
                }
            }
        #if 0
            else /* Send silence as input. */
            {
                cbReadable = pStream->State.cbTransferSize - pStream->State.cbTransferProcessed;

                Log3Func(("[SD%RU8] Sending silence (%RU32 bytes)\n", pStream->u8SD, cbReadable));

                if (cbReadable)
                {
                    rc2 = hdaStreamWrite(pStream, NULL /* Silence */, cbReadable, NULL /* pcbWritten */);
                    AssertRC(rc2);
                }
            }
        #endif

            const uint32_t cbToTransfer = hdaStreamGetUsed(pStream);
            if (cbToTransfer)
            {
                rc2 = hdaStreamTransfer(pStream, cbToTransfer);
                AssertRC(rc2);
            }
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        } /* fInTimer */
#endif
    }
}

/**
 * Locks an HDA stream for serialized access.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to lock.
 */
void hdaStreamLock(PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);
    int rc2 = RTCritSectEnter(&pStream->State.CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks a formerly locked HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to unlock.
 */
void hdaStreamUnlock(PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);
    int rc2 = RTCritSectLeave(&pStream->State.CritSect);
    AssertRC(rc2);
}

/**
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * updating its associated LPIB register and DMA position buffer (if enabled).
 *
 * @returns Set LPIB value.
 * @param   pStream             HDA stream to update read / write position for.
 * @param   u32LPIB             New LPIB (position) value to set.
 */
uint32_t hdaStreamUpdateLPIB(PHDASTREAM pStream, uint32_t u32LPIB)
{
    AssertPtrReturn(pStream, 0);

    AssertMsg(u32LPIB <= pStream->u32CBL,
              ("[SD%RU8] New LPIB (%RU32) exceeds CBL (%RU32)\n", pStream->u8SD, u32LPIB, pStream->u32CBL));

    const PHDASTATE pThis = pStream->pHDAState;

    u32LPIB = RT_MIN(u32LPIB, pStream->u32CBL);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",
                 pStream->u8SD, u32LPIB, pThis->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pThis, LPIB, pStream->u8SD) = u32LPIB;

    /* Do we need to tell the current DMA position? */
    if (pThis->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns),
                                        pThis->u64DPBase + (pStream->u8SD * 2 * sizeof(uint32_t)),
                                        (void *)&u32LPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }

    return u32LPIB;
}

# ifdef HDA_USE_DMA_ACCESS_HANDLER
/**
 * Registers access handlers for a stream's BDLE DMA accesses.
 *
 * @returns true if registration was successful, false if not.
 * @param   pStream             HDA stream to register BDLE access handlers for.
 */
bool hdaStreamRegisterDMAHandlers(PHDASTREAM pStream)
{
    /* At least LVI and the BDL base must be set. */
    if (   !pStream->u16LVI
        || !pStream->u64BDLBase)
    {
        return false;
    }

    hdaStreamUnregisterDMAHandlers(pStream);

    LogFunc(("Registering ...\n"));

    int rc = VINF_SUCCESS;

    /*
     * Create BDLE ranges.
     */

    struct BDLERANGE
    {
        RTGCPHYS uAddr;
        uint32_t uSize;
    } arrRanges[16]; /** @todo Use a define. */

    size_t cRanges = 0;

    for (uint16_t i = 0; i < pStream->u16LVI + 1; i++)
    {
        HDABDLE BDLE;
        rc = hdaBDLEFetch(pThis, &BDLE, pStream->u64BDLBase, i /* Index */);
        if (RT_FAILURE(rc))
            break;

        bool fAddRange = true;
        BDLERANGE *pRange;

        if (cRanges)
        {
            pRange = &arrRanges[cRanges - 1];

            /* Is the current range a direct neighbor of the current BLDE? */
            if ((pRange->uAddr + pRange->uSize) == BDLE.Desc.u64BufAdr)
            {
                /* Expand the current range by the current BDLE's size. */
                pRange->uSize += BDLE.Desc.u32BufSize;

                /* Adding a new range in this case is not needed anymore. */
                fAddRange = false;

                LogFunc(("Expanding range %zu by %RU32 (%RU32 total now)\n", cRanges - 1, BDLE.Desc.u32BufSize, pRange->uSize));
            }
        }

        /* Do we need to add a new range? */
        if (   fAddRange
            && cRanges < RT_ELEMENTS(arrRanges))
        {
            pRange = &arrRanges[cRanges];

            pRange->uAddr = BDLE.Desc.u64BufAdr;
            pRange->uSize = BDLE.Desc.u32BufSize;

            LogFunc(("Adding range %zu - 0x%x (%RU32)\n", cRanges, pRange->uAddr, pRange->uSize));

            cRanges++;
        }
    }

    LogFunc(("%zu ranges total\n", cRanges));

    /*
     * Register all ranges as DMA access handlers.
     */

    for (size_t i = 0; i < cRanges; i++)
    {
        BDLERANGE *pRange = &arrRanges[i];

        PHDADMAACCESSHANDLER pHandler = (PHDADMAACCESSHANDLER)RTMemAllocZ(sizeof(HDADMAACCESSHANDLER));
        if (!pHandler)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        RTListAppend(&pStream->State.lstDMAHandlers, &pHandler->Node);

        pHandler->pStream = pStream; /* Save a back reference to the owner. */

        char szDesc[32];
        RTStrPrintf(szDesc, sizeof(szDesc), "HDA[SD%RU8 - RANGE%02zu]", pStream->u8SD, i);

        int rc2 = PGMR3HandlerPhysicalTypeRegister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3), PGMPHYSHANDLERKIND_WRITE,
                                                   hdaDMAAccessHandler,
                                                   NULL, NULL, NULL,
                                                   NULL, NULL, NULL,
                                                   szDesc, &pHandler->hAccessHandlerType);
        AssertRCBreak(rc2);

        pHandler->BDLEAddr  = pRange->uAddr;
        pHandler->BDLESize  = pRange->uSize;

        /* Get first and last pages of the BDLE range. */
        RTGCPHYS pgFirst = pRange->uAddr & ~PAGE_OFFSET_MASK;
        RTGCPHYS pgLast  = RT_ALIGN(pgFirst + pRange->uSize, PAGE_SIZE);

        /* Calculate the region size (in pages). */
        RTGCPHYS regionSize = RT_ALIGN(pgLast - pgFirst, PAGE_SIZE);

        pHandler->GCPhysFirst = pgFirst;
        pHandler->GCPhysLast  = pHandler->GCPhysFirst + (regionSize - 1);

        LogFunc(("\tRegistering region '%s': 0x%x - 0x%x (region size: %zu)\n",
                 szDesc, pHandler->GCPhysFirst, pHandler->GCPhysLast, regionSize));
        LogFunc(("\tBDLE @ 0x%x - 0x%x (%RU32)\n",
                 pHandler->BDLEAddr, pHandler->BDLEAddr + pHandler->BDLESize, pHandler->BDLESize));

        rc2 = PGMHandlerPhysicalRegister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3),
                                         pHandler->GCPhysFirst, pHandler->GCPhysLast,
                                         pHandler->hAccessHandlerType, pHandler, NIL_RTR0PTR, NIL_RTRCPTR,
                                         szDesc);
        AssertRCBreak(rc2);

        pHandler->fRegistered = true;
    }

    LogFunc(("Registration ended with rc=%Rrc\n", rc));

    return RT_SUCCESS(rc);
}

/**
 * Unregisters access handlers of a stream's BDLEs.
 *
 * @param   pStream             HDA stream to unregister BDLE access handlers for.
 */
void hdaStreamUnregisterDMAHandlers(PHDASTREAM pStream)
{
    LogFunc(("\n"));

    PHDADMAACCESSHANDLER pHandler, pHandlerNext;
    RTListForEachSafe(&pStream->State.lstDMAHandlers, pHandler, pHandlerNext, HDADMAACCESSHANDLER, Node)
    {
        if (!pHandler->fRegistered) /* Handler not registered? Skip. */
            continue;

        LogFunc(("Unregistering 0x%x - 0x%x (%zu)\n",
                 pHandler->GCPhysFirst, pHandler->GCPhysLast, pHandler->GCPhysLast - pHandler->GCPhysFirst));

        int rc2 = PGMHandlerPhysicalDeregister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3),
                                               pHandler->GCPhysFirst);
        AssertRC(rc2);

        RTListNodeRemove(&pHandler->Node);

        RTMemFree(pHandler);
        pHandler = NULL;
    }

    Assert(RTListIsEmpty(&pStream->State.lstDMAHandlers));
}
# endif /* HDA_USE_DMA_ACCESS_HANDLER */

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Asynchronous I/O thread for a HDA stream.
 * This will do the heavy lifting work for us as soon as it's getting notified by another thread.
 *
 * @returns IPRT status code.
 * @param   hThreadSelf         Thread handle.
 * @param   pvUser              User argument. Must be of type PHDASTREAMTHREADCTX.
 */
DECLCALLBACK(int) hdaStreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PHDASTREAMTHREADCTX pCtx = (PHDASTREAMTHREADCTX)pvUser;
    AssertPtr(pCtx);

    PHDASTREAM pStream = pCtx->pStream;
    AssertPtr(pStream);

    PHDASTREAMSTATEAIO pAIO = &pCtx->pStream->State.AIO;

    ASMAtomicXchgBool(&pAIO->fStarted, true);

    RTThreadUserSignal(hThreadSelf);

    LogFunc(("[SD%RU8]: Started\n", pStream->u8SD));

    for (;;)
    {
        int rc2 = RTSemEventWait(pAIO->Event, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc2))
            break;

        if (ASMAtomicReadBool(&pAIO->fShutdown))
            break;

        rc2 = RTCritSectEnter(&pAIO->CritSect);
        if (RT_SUCCESS(rc2))
        {
            if (!pAIO->fEnabled)
            {
                RTCritSectLeave(&pAIO->CritSect);
                continue;
            }

            hdaStreamUpdate(pStream, false /* fInTimer */);

            int rc3 = RTCritSectLeave(&pAIO->CritSect);
            AssertRC(rc3);
        }

        AssertRC(rc2);
    }

    LogFunc(("[SD%RU8]: Ended\n", pStream->u8SD));

    ASMAtomicXchgBool(&pAIO->fStarted, false);

    return VINF_SUCCESS;
}

/**
 * Creates the async I/O thread for a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA audio stream to create the async I/O thread for.
 */
int hdaStreamAsyncIOCreate(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    int rc;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
    {
        pAIO->fShutdown = false;

        rc = RTSemEventCreate(&pAIO->Event);
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pAIO->CritSect);
            if (RT_SUCCESS(rc))
            {
                HDASTREAMTHREADCTX Ctx = { pStream->pHDAState, pStream };

                char szThreadName[64];
                RTStrPrintf2(szThreadName, sizeof(szThreadName), "hdaAIO%RU8", pStream->u8SD);

                rc = RTThreadCreate(&pAIO->Thread, hdaStreamAsyncIOThread, &Ctx,
                                    0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, szThreadName);
                if (RT_SUCCESS(rc))
                    rc = RTThreadUserWait(pAIO->Thread, 10 * 1000 /* 10s timeout */);
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    LogFunc(("[SD%RU8]: Returning %Rrc\n", pStream->u8SD, rc));
    return rc;
}

/**
 * Destroys the async I/O thread of a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA audio stream to destroy the async I/O thread for.
 */
int hdaStreamAsyncIODestroy(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return VINF_SUCCESS;

    ASMAtomicWriteBool(&pAIO->fShutdown, true);

    int rc = hdaStreamAsyncIONotify(pStream);
    AssertRC(rc);

    int rcThread;
    rc = RTThreadWait(pAIO->Thread, 30 * 1000 /* 30s timeout */, &rcThread);
    LogFunc(("Async I/O thread ended with %Rrc (%Rrc)\n", rc, rcThread));

    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectDelete(&pAIO->CritSect);
        AssertRC(rc);

        rc = RTSemEventDestroy(pAIO->Event);
        AssertRC(rc);

        pAIO->fStarted  = false;
        pAIO->fShutdown = false;
        pAIO->fEnabled  = false;
    }

    LogFunc(("[SD%RU8]: Returning %Rrc\n", pStream->u8SD, rc));
    return rc;
}

/**
 * Lets the stream's async I/O thread know that there is some data to process.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to notify async I/O thread for.
 */
int hdaStreamAsyncIONotify(PHDASTREAM pStream)
{
    return RTSemEventSignal(pStream->State.AIO.Event);
}

/**
 * Locks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStream             HDA stream to lock async I/O thread for.
 */
void hdaStreamAsyncIOLock(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectEnter(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStream             HDA stream to unlock async I/O thread for.
 */
void hdaStreamAsyncIOUnlock(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectLeave(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Enables (resumes) or disables (pauses) the async I/O thread.
 *
 * @param   pStream             HDA stream to enable/disable async I/O thread for.
 * @param   fEnable             Whether to enable or disable the I/O thread.
 *
 * @remarks Does not do locking.
 */
void hdaStreamAsyncIOEnable(PHDASTREAM pStream, bool fEnable)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;
    ASMAtomicXchgBool(&pAIO->fEnabled, fEnable);
}
# endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */

#endif /* IN_RING3 */

