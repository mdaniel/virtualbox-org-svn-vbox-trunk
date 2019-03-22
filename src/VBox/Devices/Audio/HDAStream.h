/* $Id$ */
/** @file
 * HDAStream.h - Stream functions for HD Audio.
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
 */

#ifndef HDA_STREAM_H
#define HDA_STREAM_H

#include "DevHDACommon.h"

#include "HDAStreamMap.h"
#include "HDAStreamPeriod.h"


typedef struct HDAMIXERSINK *PHDAMIXERSINK;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Structure keeping the HDA stream's state for asynchronous I/O.
 */
typedef struct HDASTREAMSTATEAIO
{
    /** Thread handle for the actual I/O thread. */
    RTTHREAD              Thread;
    /** Event for letting the thread know there is some data to process. */
    RTSEMEVENT            Event;
    /** Critical section for synchronizing access. */
    RTCRITSECT            CritSect;
    /** Started indicator. */
    volatile bool         fStarted;
    /** Shutdown indicator. */
    volatile bool         fShutdown;
    /** Whether the thread should do any data processing or not. */
    volatile bool         fEnabled;
    uint32_t              Padding1;
} HDASTREAMSTATEAIO, *PHDASTREAMSTATEAIO;
#endif

/**
 * Structure containing HDA stream debug stuff, configurable at runtime.
 */
typedef struct HDASTREAMDBGINFORT
{
    /** Whether debugging is enabled or not. */
    bool                     fEnabled;
    uint8_t                  Padding[7];
    /** File for dumping stream reads / writes.
     *  For input streams, this dumps data being written to the device FIFO,
     *  whereas for output streams this dumps data being read from the device FIFO. */
    R3PTRTYPE(PPDMAUDIOFILE) pFileStream;
    /** File for dumping DMA reads / writes.
     *  For input streams, this dumps data being written to the device DMA,
     *  whereas for output streams this dumps data being read from the device DMA. */
    R3PTRTYPE(PPDMAUDIOFILE) pFileDMA;
} HDASTREAMDBGINFORT, *PHDASTREAMDBGINFORT;

/**
 * Structure containing HDA stream debug information.
 */
typedef struct HDASTREAMDBGINFO
{
#ifdef DEBUG
    /** Critical section to serialize access if needed. */
    RTCRITSECT              CritSect;
    uint32_t                Padding0[2];
    /** Number of total read accesses. */
    uint64_t                cReadsTotal;
    /** Number of total DMA bytes read. */
    uint64_t                cbReadTotal;
    /** Timestamp (in ns) of last read access. */
    uint64_t                tsLastReadNs;
    /** Number of total write accesses. */
    uint64_t                cWritesTotal;
    /** Number of total DMA bytes written. */
    uint64_t                cbWrittenTotal;
    /** Number of total write accesses since last iteration (Hz). */
    uint64_t                cWritesHz;
    /** Number of total DMA bytes written since last iteration (Hz). */
    uint64_t                cbWrittenHz;
    /** Timestamp (in ns) of beginning a new write slot. */
    uint64_t                tsWriteSlotBegin;
    /** Number of current silence samples in a (consecutive) row. */
    uint64_t                csSilence;
    /** Number of silent samples in a row to consider an audio block as audio gap (silence). */
    uint64_t                cSilenceThreshold;
    /** How many bytes to skip in an audio stream before detecting silence.
     *  (useful for intros and silence at the beginning of a song). */
    uint64_t                cbSilenceReadMin;
#endif
    /** Runtime debug info. */
    HDASTREAMDBGINFORT      Runtime;
} HDASTREAMDBGINFO ,*PHDASTREAMDBGINFO;

/**
 * Internal state of a HDA stream.
 */
typedef struct HDASTREAMSTATE
{
    /** Current BDLE to use. Wraps around to 0 if
     *  maximum (cBDLE) is reached. */
    uint16_t                uCurBDLE;
    /** Flag indicating whether this stream currently is
     *  in reset mode and therefore not acccessible by the guest. */
    volatile bool           fInReset;
    /** Flag indicating if the stream is in running state or not. */
    volatile bool           fRunning;
    /** Unused, padding. */
    uint8_t                 Padding0[3];
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    /** Asynchronous I/O state members. */
    HDASTREAMSTATEAIO       AIO;
#endif
    /** This stream's data mapping. */
    HDASTREAMMAPPING        Mapping;
    /** Current BDLE (Buffer Descriptor List Entry). */
    HDABDLE                 BDLE;
    /** Circular buffer (FIFO) for holding DMA'ed data. */
    R3PTRTYPE(PRTCIRCBUF)   pCircBuf;
    /** Timestamp of the last DMA data transfer. */
    uint64_t                tsTransferLast;
    /** Timestamp of the next DMA data transfer.
     *  Next for determining the next scheduling window.
     *  Can be 0 if no next transfer is scheduled. */
    uint64_t                tsTransferNext;
    /** Total transfer size (in bytes) of a transfer period. */
    uint32_t                cbTransferSize;
    /** Transfer chunk size (in bytes) of a transfer period. */
    uint32_t                cbTransferChunk;
    /** How many bytes already have been processed in within
     *  the current transfer period. */
    uint32_t                cbTransferProcessed;
    /** How many interrupts are pending due to
     *  BDLE interrupt-on-completion (IOC) bits set. */
    uint8_t                 cTransferPendingInterrupts;
    /** The stream's current audio frame size (in bytes). */
    uint32_t                cbFrameSize;
    /** How many audio data frames are left to be processed
     *  for the position adjustment handling.
     *
     *  0 if position adjustment handling is done or inactive. */
    uint16_t                cPosAdjustFramesLeft;
    uint8_t                 Padding2[2];
    /** (Virtual) clock ticks per byte. */
    uint64_t                cTicksPerByte;
    /** (Virtual) clock ticks per transfer. */
    uint64_t                cTransferTicks;
    /** The stream's period. Need for timing. */
    HDASTREAMPERIOD         Period;
    /** The stream's current configuration.
     *  Should match SDFMT. */
    PDMAUDIOSTREAMCFG       Cfg;
#ifdef HDA_USE_DMA_ACCESS_HANDLER
    /** List of DMA handlers. */
    RTLISTANCHORR3          lstDMAHandlers;
#endif
    /** How much DMA data from a previous transfer is left to be processed (in bytes).
     *  This can happen if the stream's frame size is bigger (e.g. 16 bytes) than
     *  the current DMA FIFO can hold (e.g. 10 bytes). Mostly needed for more complex
     *  stuff like interleaved surround streams. */
    uint16_t                cbDMALeft;
    /** Unused, padding. */
    uint8_t                 Padding3;
} HDASTREAMSTATE, *PHDASTREAMSTATE;

/**
 * Structure for keeping a HDA stream (SDI / SDO).
 *
 * Note: This HDA stream has nothing to do with a regular audio stream handled
 * by the audio connector or the audio mixer. This HDA stream is a serial data in/out
 * stream (SDI/SDO) defined in hardware and can contain multiple audio streams
 * in one single SDI/SDO (interleaving streams).
 *
 * How a specific SDI/SDO is mapped to our internal audio streams relies on the
 * stream channel mappings.
 *
 * Contains only register values which do *not* change until a
 * stream reset occurs.
 */
typedef struct HDASTREAM
{
    /** Stream descriptor number (SDn). */
    uint8_t                  u8SD;
    /** Current channel index.
     *  For a stereo stream, this is u8Channel + 1. */
    uint8_t                  u8Channel;
    uint8_t                  Padding0[6];
    /** DMA base address (SDnBDPU - SDnBDPL). */
    uint64_t                 u64BDLBase;
    /** Cyclic Buffer Length (SDnCBL).
     *  Represents the size of the ring buffer. */
    uint32_t                 u32CBL;
    /** Format (SDnFMT). */
    uint16_t                 u16FMT;
    /** FIFO Size (FIFOS).
     *  Maximum number of bytes that may have been DMA'd into
     *  memory but not yet transmitted on the link. */
    uint16_t                 u16FIFOS;
    /** FIFO Watermark. */
    uint16_t                 u16FIFOW;
    /** Last Valid Index (SDnLVI). */
    uint16_t                 u16LVI;
    uint16_t                 Padding1[2];
    /** Pointer to the HDA state this stream is attached to. */
    R3PTRTYPE(PHDASTATE)     pHDAState;
    /** Pointer to HDA sink this stream is attached to. */
    R3PTRTYPE(PHDAMIXERSINK) pMixSink;
    /** The stream'S critical section to serialize access. */
    RTCRITSECT               CritSect;
    /** Pointer to the stream's timer. */
    PTMTIMERR3               pTimer;
    /** Internal state of this stream. */
    HDASTREAMSTATE           State;
    /** Debug information. */
    HDASTREAMDBGINFO         Dbg;
} HDASTREAM, *PHDASTREAM;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Structure for keeping a HDA stream thread context.
 */
typedef struct HDASTREAMTHREADCTX
{
    PHDASTATE  pThis;
    PHDASTREAM pStream;
} HDASTREAMTHREADCTX, *PHDASTREAMTHREADCTX;
#endif

#ifdef IN_RING3

/** @name Stream functions.
 * @{
 */
int               hdaR3StreamCreate(PHDASTREAM pStream, PHDASTATE pThis, uint8_t u8SD);
void              hdaR3StreamDestroy(PHDASTREAM pStream);
int               hdaR3StreamInit(PHDASTREAM pStream, uint8_t uSD);
void              hdaR3StreamReset(PHDASTATE pThis, PHDASTREAM pStream, uint8_t uSD);
int               hdaR3StreamEnable(PHDASTREAM pStream, bool fEnable);
uint32_t          hdaR3StreamGetPosition(PHDASTATE pThis, PHDASTREAM pStream);
void              hdaR3StreamSetPosition(PHDASTREAM pStream, uint32_t u32LPIB);
uint32_t          hdaR3StreamGetFree(PHDASTREAM pStream);
uint32_t          hdaR3StreamGetUsed(PHDASTREAM pStream);
bool              hdaR3StreamTransferIsScheduled(PHDASTREAM pStream);
uint64_t          hdaR3StreamTransferGetNext(PHDASTREAM pStream);
int               hdaR3StreamTransfer(PHDASTREAM pStream, uint32_t cbToProcessMax);
void              hdaR3StreamLock(PHDASTREAM pStream);
void              hdaR3StreamUnlock(PHDASTREAM pStream);
int               hdaR3StreamRead(PHDASTREAM pStream, uint32_t cbToRead, uint32_t *pcbRead);
int               hdaR3StreamWrite(PHDASTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten);
void              hdaR3StreamUpdate(PHDASTREAM pStream, bool fAsync);
# ifdef HDA_USE_DMA_ACCESS_HANDLER
bool              hdaR3StreamRegisterDMAHandlers(PHDASTREAM pStream);
void              hdaR3StreamUnregisterDMAHandlers(PHDASTREAM pStream);
# endif /* HDA_USE_DMA_ACCESS_HANDLER */
/** @} */

/** @name Timer functions.
 * @{
 */
DECLCALLBACK(void) hdaR3StreamTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);
/** @} */


/** @name Async I/O stream functions.
 * @{
 */
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
DECLCALLBACK(int) hdaR3StreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser);
int               hdaR3StreamAsyncIOCreate(PHDASTREAM pStream);
int               hdaR3StreamAsyncIODestroy(PHDASTREAM pStream);
int               hdaR3StreamAsyncIONotify(PHDASTREAM pStream);
void              hdaR3StreamAsyncIOLock(PHDASTREAM pStream);
void              hdaR3StreamAsyncIOUnlock(PHDASTREAM pStream);
void              hdaR3StreamAsyncIOEnable(PHDASTREAM pStream, bool fEnable);
# endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */
/** @} */

#endif /* IN_RING3 */
#endif /* !HDA_STREAM_H */

