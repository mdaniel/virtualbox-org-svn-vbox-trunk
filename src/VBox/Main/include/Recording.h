/* $Id$ */
/** @file
 * Recording code header.
 */

/*
 * Copyright (C) 2012-2023 Oracle and/or its affiliates.
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

#ifndef MAIN_INCLUDED_Recording_h
#define MAIN_INCLUDED_Recording_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>
#include <VBox/settings.h>

#include "RecordingStream.h"

class Console;

/** No flags specified. */
#define VBOX_RECORDING_CURSOR_F_NONE       0
/** Cursor is visible. */
#define VBOX_RECORDING_CURSOR_F_VISIBLE    RT_BIT(0)
/** Cursor shape contains an alpha mask. */
#define VBOX_RECORDING_CURSOR_F_ALPHA      RT_BIT(1)
/** Cursor state flags valid mask. */
#define VBOX_RECORDING_CURSOR_F_VALID_MASK 0x3

/**
 * Class for keeping a recording cursor state.
 */
class RecordingCursorState
{
public:

    RecordingCursorState();
    virtual ~RecordingCursorState();

    void Destroy();

    int CreateOrUpdate(bool fAlpha, uint32_t uWidth, uint32_t uHeight, const uint8_t *pu8Shape, size_t cbShape);

    int Move(int32_t iX, int32_t iY);

    /** Cursor state flags. */
    uint32_t            m_fFlags;
    /** The current cursor shape. */
    RECORDINGVIDEOFRAME m_Shape;
};

/**
 * Class for managing a recording context.
 */
class RecordingContext
{
public:

    RecordingContext();

    RecordingContext(Console *ptrConsole, const settings::RecordingSettings &Settings);

    virtual ~RecordingContext(void);

public:

    const settings::RecordingSettings &GetConfig(void) const;
    RecordingStream *GetStream(unsigned uScreen) const;
    size_t GetStreamCount(void) const;
#ifdef VBOX_WITH_AUDIO_RECORDING
    PRECORDINGCODEC GetCodecAudio(void) { return &this->m_CodecAudio; }
#endif

    int Create(Console *pConsole, const settings::RecordingSettings &Settings);
    void Destroy(void);

    int Start(void);
    int Stop(void);

    int SendAudioFrame(const void *pvData, size_t cbData, uint64_t uTimestampMs);
    int SendVideoFrame(uint32_t uScreen, PRECORDINGVIDEOFRAME pFrame, uint64_t msTimestamp);
    int SendCursorPositionChange(uint32_t uScreen, int32_t x, int32_t y, uint64_t msTimestamp);
    int SendCursorShapeChange(bool fVisible, bool fAlpha, uint32_t xHot, uint32_t yHot, uint32_t uWidth, uint32_t uHeight, const uint8_t *pu8Shape, size_t cbShape, uint64_t msTimestamp);
    int SendScreenChange(uint32_t uScreen, PRECORDINGSURFACEINFO pInfo, uint64_t uTimestampMs);

public:

    uint64_t GetCurrentPTS(void) const;
    bool IsFeatureEnabled(RecordingFeature_T enmFeature);
    bool IsReady(void);
    bool IsReady(uint32_t uScreen, uint64_t msTimestamp);
    bool IsStarted(void);
    bool IsLimitReached(void);
    bool IsLimitReached(uint32_t uScreen, uint64_t msTimestamp);
    bool NeedsUpdate(uint32_t uScreen, uint64_t msTimestamp);

    DECLCALLBACK(int) OnLimitReached(uint32_t uScreen, int vrc);

    /** The state mouse cursor state.
     *  We currently only support one mouse cursor at a time. */
    RecordingCursorState         m_Cursor;


protected:

    int createInternal(Console *ptrConsole, const settings::RecordingSettings &Settings);
    int startInternal(void);
    int stopInternal(void);

    void destroyInternal(void);

    RecordingStream *getStreamInternal(unsigned uScreen) const;

    int processCommonData(RecordingBlockMap &mapCommon, RTMSINTERVAL msTimeout);
    int writeCommonData(RecordingBlockMap &mapCommon, PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags);

    int lock(void);
    int unlock(void);

    static DECLCALLBACK(int) threadMain(RTTHREAD hThreadSelf, void *pvUser);

    int threadNotify(void);

protected:

    int audioInit(const settings::RecordingScreenSettings &screenSettings);

    static DECLCALLBACK(int) audioCodecWriteDataCallback(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags, void *pvUser);

protected:

    /**
     * Enumeration for a recording context state.
     */
    enum RECORDINGSTS
    {
        /** Context not initialized. */
        RECORDINGSTS_UNINITIALIZED = 0,
        /** Context was created. */
        RECORDINGSTS_CREATED       = 1,
        /** Context was started. */
        RECORDINGSTS_STARTED       = 2,
        /** The usual 32-bit hack. */
        RECORDINGSTS_32BIT_HACK    = 0x7fffffff
    };

    /** Pointer to the console object. */
    Console                     *m_pConsole;
    /** Used recording configuration. */
    settings::RecordingSettings  m_Settings;
    /** The current state. */
    RECORDINGSTS                 m_enmState;
    /** Critical section to serialize access. */
    RTCRITSECT                   m_CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT                   m_WaitEvent;
    /** Shutdown indicator. */
    bool                         m_fShutdown;
    /** Encoding worker thread. */
    RTTHREAD                     m_Thread;
    /** Vector of current recording streams.
     *  Per VM screen (display) one recording stream is being used. */
    RecordingStreams             m_vecStreams;
    /** Number of streams in vecStreams which currently are enabled for recording. */
    uint16_t                     m_cStreamsEnabled;
    /** Timestamp (in ms) of when recording has been started.
     *  Set to 0 if not started (yet). */
    uint64_t                     m_tsStartMs;
#ifdef VBOX_WITH_AUDIO_RECORDING
    /** Audio codec to use.
     *
     *  We multiplex audio data from this recording context to all streams,
     *  to avoid encoding the same audio data for each stream. We ASSUME that
     *  all audio data of a VM will be the same for each stream at a given
     *  point in time. */
    RECORDINGCODEC               m_CodecAudio;
#endif /* VBOX_WITH_AUDIO_RECORDING */
    /** Block map of raw common data blocks which need to get encoded first. */
    RecordingBlockMap            m_mapBlocksRaw;
    /** Block map of encoded common blocks.
     *
     *  Only do the encoding of common data blocks only once and then multiplex
     *  the encoded data to all affected recording streams.
     *
     *  This avoids doing the (expensive) encoding + multiplexing work in other
     *  threads like EMT / audio async I/O..
     *
     *  For now this only affects audio, e.g. all recording streams
     *  need to have the same audio data at a specific point in time. */
    RecordingBlockMap            m_mapBlocksEncoded;
};
#endif /* !MAIN_INCLUDED_Recording_h */

