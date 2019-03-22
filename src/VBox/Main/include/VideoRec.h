/* $Id$ */
/** @file
 * Video recording code header.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIDEOREC
#define ____H_VIDEOREC

#include <VBox/com/array.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/err.h>

using namespace com;

#include "VideoRecInternals.h"
#include "VideoRecStream.h"

/**
 * Enumeration for definining a video / audio
 * profile setting.
 */
typedef enum VIDEORECPROFILE
{
    VIDEORECPROFILE_NONE   = 0,
    VIDEORECPROFILE_LOW,
    VIDEORECPROFILE_MEDIUM,
    VIDEORECPROFILE_HIGH,
    VIDEORECPROFILE_BEST,
    VIDEORECPROFILE_REALTIME
} VIDEORECPROFILE;

/** Stores video recording features. */
typedef uint32_t VIDEORECFEATURES;

/** Video recording is disabled completely. */
#define VIDEORECFEATURE_NONE        0
/** Capturing video is enabled. */
#define VIDEORECFEATURE_VIDEO       RT_BIT(0)
/** Capturing audio is enabled. */
#define VIDEORECFEATURE_AUDIO       RT_BIT(1)

/**
 * Structure for keeping a video recording configuration.
 */
typedef struct VIDEORECCFG
{
    VIDEORECCFG(void)
        :  enmDst(VIDEORECDEST_INVALID)
        , uMaxTimeS(0)
    {
#ifdef VBOX_WITH_AUDIO_VIDEOREC
        RT_ZERO(Audio);
#endif
        RT_ZERO(Video);
    }

    /** Array of all screens containing whether they're enabled
     *  for recording or not.  */
    com::SafeArray<BOOL>    aScreens;
    /** Destination where to write the stream to. */
    VIDEORECDEST            enmDst;

    /**
     * Structure for keeping recording parameters if
     * destination is a file.
     */
    struct
    {
        /** File name (as absolute path). */
        com::Bstr       strName;
        /** Maximum file size (in MB) to record. */
        uint32_t        uMaxSizeMB;
    } File;

#ifdef VBOX_WITH_AUDIO_VIDEOREC
    /**
     * Structure for keeping the audio recording parameters.
     */
    struct
    {
        /** Whether audio recording is enabled or not. */
        bool                fEnabled;
        /** The device LUN the audio driver is attached / configured to. */
        unsigned            uLUN;
        /** Hertz (Hz) rate. */
        uint16_t            uHz;
        /** Bits per sample. */
        uint8_t             cBits;
        /** Number of audio channels. */
        uint8_t             cChannels;
        /** Audio profile which is being used. */
        VIDEORECPROFILE     enmProfile;
    } Audio;
#endif

    /**
     * Structure for keeping the video recording parameters.
     */
    struct
    {
        /** Whether video recording is enabled or not. */
        bool                fEnabled;
        /** Target width (in pixels). */
        uint32_t            uWidth;
        /** Target height (in pixels). */
        uint32_t            uHeight;
        /** Target encoding rate. */
        uint32_t            uRate;
        /** Target FPS. */
        uint32_t            uFPS;

#ifdef VBOX_WITH_LIBVPX
        union
        {
            struct
            {
                /** Encoder deadline. */
                unsigned int uEncoderDeadline;
            } VPX;
        } Codec;
#endif

    } Video;
    /** Maximum time (in s) to record.
     *  Specify 0 to disable this check. */
    uint32_t                uMaxTimeS;

    VIDEORECCFG& operator=(const VIDEORECCFG &that)
    {
        aScreens.resize(that.aScreens.size());
        for (size_t i = 0; i < that.aScreens.size(); ++i)
            aScreens[i] = that.aScreens[i];

        enmDst   = that.enmDst;

        File.strName    = that.File.strName;
        File.uMaxSizeMB = that.File.uMaxSizeMB;
#ifdef VBOX_WITH_AUDIO_VIDEOREC
        Audio           = that.Audio;
#endif
        Video           = that.Video;
        uMaxTimeS       = that.uMaxTimeS;
        return *this;
    }

} VIDEORECCFG, *PVIDEORECCFG;

/**
 * Structure for keeping a video recording context.
 */
typedef struct VIDEORECCONTEXT
{
    /** Used recording configuration. */
    VIDEORECCFG         Cfg;
    /** The current state. */
    uint32_t            enmState;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;
    /** Semaphore to signal the encoding worker thread. */
    RTSEMEVENT          WaitEvent;
    /** Whether this conext is in started state or not. */
    bool                fStarted;
    /** Shutdown indicator. */
    bool                fShutdown;
    /** Worker thread. */
    RTTHREAD            Thread;
    /** Vector of current recording streams.
     *  Per VM screen (display) one recording stream is being used. */
    VideoRecStreams     vecStreams;
    /** Timestamp (in ms) of when recording has been started. */
    uint64_t            tsStartMs;
    /** Block map of common blocks which need to get multiplexed
     *  to all recording streams. This common block maps should help
     *  reducing the time spent in EMT and avoid doing the (expensive)
     *  multiplexing work in there.
     *
     *  For now this only affects audio, e.g. all recording streams
     *  need to have the same audio data at a specific point in time. */
    VideoRecBlockMap    mapBlocksCommon;
} VIDEORECCONTEXT, *PVIDEORECCONTEXT;

int VideoRecContextCreate(uint32_t cScreens, PVIDEORECCFG pVideoRecCfg, PVIDEORECCONTEXT *ppCtx);
int VideoRecContextDestroy(PVIDEORECCONTEXT pCtx);

VIDEORECFEATURES VideoRecGetFeatures(PVIDEORECCFG pCfg);
PVIDEORECSTREAM VideoRecGetStream(PVIDEORECCONTEXT pCtx, uint32_t uScreen);

int VideoRecSendAudioFrame(PVIDEORECCONTEXT pCtx, const void *pvData, size_t cbData, uint64_t uTimestampMs);
int VideoRecSendVideoFrame(PVIDEORECCONTEXT pCtx, uint32_t uScreen,
                            uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP,
                            uint32_t uBytesPerLine, uint32_t uSrcWidth, uint32_t uSrcHeight,
                            uint8_t *puSrcData, uint64_t uTimeStampMs);
bool VideoRecIsReady(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t uTimeStampMs);
bool VideoRecIsStarted(PVIDEORECCONTEXT pCtx);
bool VideoRecIsLimitReached(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t tsNowMs);

#endif /* !____H_VIDEOREC */

