/** @file
 * Shared Clipboard - Common guest and host Code.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/critsect.h>
#include <iprt/types.h>
#include <iprt/list.h>

/** @name VBOX_SHCL_FMT_XXX - Data formats (flags) for Shared Clipboard.
 * @{
 */
/** No format set. */
#define VBOX_SHCL_FMT_NONE          0
/** Shared Clipboard format is an Unicode text. */
#define VBOX_SHCL_FMT_UNICODETEXT   RT_BIT(0)
/** Shared Clipboard format is bitmap (BMP / DIB). */
#define VBOX_SHCL_FMT_BITMAP        RT_BIT(1)
/** Shared Clipboard format is HTML. */
#define VBOX_SHCL_FMT_HTML          RT_BIT(2)
/** Shared Clipboard format is a transfer list.
 *
 *  When requesting (reading) data with this format, the following happens:
 *  - Acts as a beacon for transfer negotiation / handshake.
 *  - The receiving side (source) initializes a transfer locally.
 *  - The receiving side reports the transfer status (INIT) to the sending side (target).
 *  - The sending side proceeds initializing the transfer locally.
 *  - The sending side reports its transfer status (INIT) to the receiving side.
 *
 *  Note: When receiving an error via a transfer status, the transfer must be destroyed and
 *  is considered as being failed wholesale.
 *
 *  @since 7.1
 */
#define VBOX_SHCL_FMT_URI_LIST      RT_BIT(3)
/** Shared Clipboard format valid mask. */
#define VBOX_SHCL_FMT_VALID_MASK    0xf
/** Maximum number of Shared Clipboard formats.
 *  This currently ASSUMES that there are no gaps in the bit mask. */
#define VBOX_SHCL_FMT_MAX           VBOX_SHCL_FMT_VALID_MASK
/** @}  */


/** A single Shared Clipboard format (VBOX_SHCL_FMT_XXX). */
typedef uint32_t SHCLFORMAT;
/** Pointer to a single Shared Clipboard format (VBOX_SHCL_FMT_XXX). */
typedef SHCLFORMAT *PSHCLFORMAT;

/** Bit map (flags) of Shared Clipboard formats (VBOX_SHCL_FMT_XXX). */
typedef uint32_t SHCLFORMATS;
/** Pointer to a bit map of Shared Clipboard formats (VBOX_SHCL_FMT_XXX). */
typedef SHCLFORMATS *PSHCLFORMATS;

/** Defines the default timeout (in ms) to use for clipboard single wait operations.
 *  Not being used for lenghtly operations as a whole!
 *  Note: Don't set this too high, otherwise the UI feels sluggish. */
#define SHCL_TIMEOUT_DEFAULT_MS                 RT_MS_5SEC


/**
 * Shared Clipboard transfer direction.
 */
typedef enum SHCLTRANSFERDIR
{
    /** Unknown transfer directory. */
    SHCLTRANSFERDIR_UNKNOWN = 0,
    /** Read transfer (from source). */
    SHCLTRANSFERDIR_FROM_REMOTE,
    /** Write transfer (to target). */
    SHCLTRANSFERDIR_TO_REMOTE,
    /** The usual 32-bit hack. */
    SHCLTRANSFERDIR_32BIT_HACK = 0x7fffffff
} SHCLTRANSFERDIR;
/** Pointer to a shared clipboard transfer direction. */
typedef SHCLTRANSFERDIR *PSHCLTRANSFERDIR;


/**
 * Shared Clipboard data read request.
 */
typedef struct SHCLDATAREQ
{
    /** In which format the data needs to be sent. */
    SHCLFORMAT uFmt;
    /** Read flags; currently unused. */
    uint32_t   fFlags;
    /** Maximum data (in byte) can be sent. */
    uint32_t   cbSize;
} SHCLDATAREQ;
/** Pointer to a shared clipboard data request. */
typedef SHCLDATAREQ *PSHCLDATAREQ;

/**
 * Shared Clipboard event payload (optional).
 */
typedef struct SHCLEVENTPAYLOAD
{
    /** Payload ID; currently unused. */
    uint32_t uID;
    /** Size (in bytes) of actual payload data. */
    uint32_t cbData;
    /** Pointer to actual payload data. */
    void    *pvData;
} SHCLEVENTPAYLOAD;
/** Pointer to a shared clipboard event payload. */
typedef SHCLEVENTPAYLOAD *PSHCLEVENTPAYLOAD;

/** A shared clipboard event source ID. */
typedef uint16_t SHCLEVENTSOURCEID;
/** Pointer to a shared clipboard event source ID. */
typedef SHCLEVENTSOURCEID *PSHCLEVENTSOURCEID;

/** A shared clipboard session ID. */
typedef uint16_t        SHCLSESSIONID;
/** Pointer to a shared clipboard session ID. */
typedef SHCLSESSIONID  *PSHCLSESSIONID;
/** NIL shared clipboard session ID. */
#define NIL_SHCLSESSIONID                        UINT16_MAX

/** A shared clipboard transfer ID. */
typedef uint16_t        SHCLTRANSFERID;
/** Pointer to a shared clipboard transfer ID. */
typedef SHCLTRANSFERID *PSHCLTRANSFERID;
/** NIL shared clipboardtransfer ID. */
#define NIL_SHCLTRANSFERID                       UINT16_MAX

/** A shared clipboard event ID. */
typedef uint32_t        SHCLEVENTID;
/** Pointer to a shared clipboard event source ID. */
typedef SHCLEVENTID    *PSHCLEVENTID;
/** NIL shared clipboard event ID. */
#define NIL_SHCLEVENTID                          UINT32_MAX

/** Pointer to a shared clipboard event source.
 *  Forward declaration, needed for SHCLEVENT. */
typedef struct SHCLEVENTSOURCE *PSHCLEVENTSOURCE;

/**
 * Shared Clipboard event.
 */
typedef struct SHCLEVENT
{
    /** List node. */
    RTLISTNODE          Node;
    /** Parent (source) this event belongs to. */
    PSHCLEVENTSOURCE    pParent;
    /** The event's ID, for self-reference. */
    SHCLEVENTID         idEvent;
    /** Reference count to this event. */
    uint32_t            cRefs;
    /** Event semaphore for signalling the event. */
    RTSEMEVENTMULTI     hEvtMulSem;
    /** Payload to this event, optional (NULL). */
    PSHCLEVENTPAYLOAD   pPayload;
    /** Result code (IPRT-style) to assign. */
    int                 rc;
} SHCLEVENT;
/** Pointer to a shared clipboard event. */
typedef SHCLEVENT *PSHCLEVENT;

/**
 * Shared Clipboard event source.
 *
 * Each event source maintains an own counter for events, so that it can be used
 * in different contexts.
 */
typedef struct SHCLEVENTSOURCE
{
    /** The event source ID. */
    SHCLEVENTSOURCEID uID;
    /** Critical section for serializing access. */
    RTCRITSECT        CritSect;
    /** Next upcoming event ID. */
    SHCLEVENTID       idNextEvent;
    /** List of events (PSHCLEVENT). */
    RTLISTANCHOR      lstEvents;
} SHCLEVENTSOURCE;

/** @name Shared Clipboard data payload functions.
 *  @{
 */
int ShClPayloadInit(uint32_t uID, void *pvData, uint32_t cbData, PSHCLEVENTPAYLOAD *ppPayload);
int ShClPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData, PSHCLEVENTPAYLOAD *ppPayload);
void ShClPayloadFree(PSHCLEVENTPAYLOAD pPayload);
/** @} */

/** @name Shared Clipboard event source functions.
 *  @{
 */
int ShClEventSourceCreate(PSHCLEVENTSOURCE pSource, SHCLEVENTSOURCEID idEvtSrc);
int ShClEventSourceDestroy(PSHCLEVENTSOURCE pSource);
void ShClEventSourceReset(PSHCLEVENTSOURCE pSource);
int ShClEventSourceGenerateAndRegisterEvent(PSHCLEVENTSOURCE pSource, PSHCLEVENT *ppEvent);
PSHCLEVENT ShClEventSourceGetFromId(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);
PSHCLEVENT ShClEventSourceGetLast(PSHCLEVENTSOURCE pSource);
/** @} */

/** @name Shared Clipboard event functions.
 *  @{
 */
uint32_t ShClEventGetRefs(PSHCLEVENT pEvent);
uint32_t ShClEventRetain(PSHCLEVENT pEvent);
uint32_t ShClEventRelease(PSHCLEVENT pEvent);
int ShClEventSignalEx(PSHCLEVENT pEvent, int rc, PSHCLEVENTPAYLOAD pPayload);
int ShClEventSignal(PSHCLEVENT pEvent, PSHCLEVENTPAYLOAD pPayload);
int ShClEventWait(PSHCLEVENT pEvent, RTMSINTERVAL uTimeoutMs, PSHCLEVENTPAYLOAD *ppPayload);
int ShClEventWaitEx(PSHCLEVENT pEvent, RTMSINTERVAL uTimeoutMs, int *pRc, PSHCLEVENTPAYLOAD *ppPayload);
/** @} */

/**
 * Shared Clipboard transfer source type.
 * @note Part of saved state!
 */
typedef enum SHCLSOURCE
{
    /** Invalid source type. */
    SHCLSOURCE_INVALID = 0,
    /** Source is local. */
    SHCLSOURCE_LOCAL,
    /** Source is remote. */
    SHCLSOURCE_REMOTE,
    /** The usual 32-bit hack. */
    SHCLSOURCE_32BIT_HACK = 0x7fffffff
} SHCLSOURCE;

/** @name Shared Clipboard caching.
 *  @{
 */
/**
 * A single Shared Clipboard cache entry.
 *
 * One entry marks exactly one clipboard format at a time.
 */
typedef struct _SHCLCACHEENTRY
{
    /** Entry data.
     *  Acts as a beacon for entry validation. */
    void  *pvData;
    /** Entry data size (in bytes). */
    size_t cbData;
} SHCLCACHEENTRY;
/** Pointer to a Shared Clipboard cache entry. */
typedef SHCLCACHEENTRY *PSHCLCACHEENTRY;

/**
 * A (very simple) Shared Clipboard cache.
 */
typedef struct _SHCLCACHE
{
    /** Entries for all formats.
     *  Right now this is static to keep it simple. */
    SHCLCACHEENTRY aEntries[VBOX_SHCL_FMT_MAX];
} SHCLCACHE;
/** Pointer to a Shared Clipboard cache. */
typedef SHCLCACHE *PSHCLCACHE;

void ShClCacheEntryGet(PSHCLCACHEENTRY pCacheEntry, void **pvData, size_t *pcbData);

void ShClCacheInit(PSHCLCACHE pCache);
void ShClCacheDestroy(PSHCLCACHE pCache);
void ShClCacheInvalidate(PSHCLCACHE pCache);
void ShClCacheInvalidateEntry(PSHCLCACHE pCache, SHCLFORMAT uFmt);
PSHCLCACHEENTRY ShClCacheGet(PSHCLCACHE pCache, SHCLFORMAT uFmt);
int ShClCacheSet(PSHCLCACHE pCache, SHCLFORMAT uFmt, const void *pvData, size_t cbData);
int ShClCacheSetMultiple(PSHCLCACHE pCache, SHCLFORMATS uFmts, const void *pvData, size_t cbData);
/** @}  */

/** Opaque data structure for the X11/VBox frontend/glue code.
 * @{ */
struct SHCLCONTEXT;
typedef struct SHCLCONTEXT SHCLCONTEXT;
/** @} */
/** Pointer to opaque data structure the X11/VBox frontend/glue code. */
typedef SHCLCONTEXT *PSHCLCONTEXT;

/**
 * @name Shared Clipboard callback table.
 *
 * This table gets used by
 *   - the backends on the host (where required)
 *   - guest side implementations (e.g. VBoxClient)
 *   - by the underlying core code (e.g. X11 backend -> X11 common code -> callback)
 *
 * Some clipboard mechanisms (e.g. X11) require asynchronous and/or event-driven handling
 * of clipboard data, making it hard to control our program flow when testing stuff.
 *
 * So overriding required callbacks on runtime for testing purposes makes this approach much
 * more flexible without implementing separate code paths for production code and test units.
 *
 * @{
 */
typedef struct _SHCLCALLBACKS
{
    /**
     * Callback for reporting supported clipoard formats of current clipboard data.
     *
     * @note On X11:
     *         Runs in Xt event thread for the X11 code.
     *
     * @returns VBox status code.
     * @param   pCtx            Opaque context pointer for the glue code.
     * @param   fFormats        The formats available.
     * @param   pvUser          Implementation-dependent pointer to data for fullfilling the request.
     *                          Optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnReportFormats, (PSHCLCONTEXT pCtx, SHCLFORMATS fFormats, void *pvUser));

    /**
     * Callback for reading data from the clipboard.
     * Optional and can be NULL.
     *
     * @note Used for testing X11 clipboard code.
     *
     * @returns VBox status code.
     * @param   pCtx            Opaque context pointer for the glue code.
     * @param   uFmt            The format in which the data should be read
     *                          (VBOX_SHCL_FMT_XXX).
     * @param   ppv             Returns an allocated buffer with data from on success.
     *                          Needs to be free'd with RTMemFree() by the caller.
     * @param   pcb             Returns the amount of data read (in bytes) on success.
     * @param   pvUser          Implementation-dependent pointer to data for fullfilling the request.
     *                          Optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnOnClipboardRead, (PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, size_t *pcb, void *pvUser));

    /**
     * Callback for writing data to the clipboard.
     * Optional and can be NULL.
     *
     * @note Used for testing X11 clipboard code.
     *
     * @returns VBox status code.
     * @param   pCtx            Opaque context pointer for the glue code.
     * @param   uFmt            The format in which the data should be written as
     *                          (VBOX_SHCL_FMT_XXX).
     * @param   pv              The clipboard data to write.
     * @param   cb              The size of the data in @a pv.
     * @param   pvUser          Implementation-dependent pointer to data for fullfilling the request.
     *                          Optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnOnClipboardWrite, (PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void *pv, size_t cb, void *pvUser));

    /**
     * Callback for requesting clipboard data from the source.
     *
     * @note On X11:
     *         The function will be invoked for every single target the clipboard requests.
     *         Runs in Xt event thread for the X11 code.
     *
     * @returns VBox status code.
     * @retval  VERR_NO_DATA if no data available.
     * @param   pCtx            Opaque context pointer for the glue code.
     * @param   uFmt            The format in which the data should be transferred
     *                          (VBOX_SHCL_FMT_XXX).
     * @param   ppv             Returns an allocated buffer with data read from the guest on success.
     *                          Needs to be free'd with RTMemFree() by the caller.
     * @param   pcb             Returns the amount of data read (in bytes) on success.
     * @param   pvUser          Implementation-dependent pointer to data for fullfilling the request.
     *                          Optional and can be NULL.
     *                          On X11: Of type PSHCLX11READDATAREQ; We RTMemFree() this in this function.
     */
    DECLCALLBACKMEMBER(int, pfnOnRequestDataFromSource, (PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb, void *pvUser));

    /**
     * Callback for sending clipboard data to the destination.
     *
     * @returns VBox status code.
     * @param   pCtx            Opaque context pointer for the glue code.
     * @param   pv              The clipboard data returned if the request succeeded.
     * @param   cb              The size of the data in @a pv.
     * @param   pvUser          Implementation-dependent pointer to data for fullfilling the request.
     *                          Optional and can be NUL
     *                          On X11: Of type PSHCLX11READDATAREQ.
     */
    DECLCALLBACKMEMBER(int, pfnOnSendDataToDest, (PSHCLCONTEXT pCtx, void *pv, uint32_t cb, void *pvUser));
} SHCLCALLBACKS;
/** Pointer to a Shared Clipboard callback table. */
typedef SHCLCALLBACKS *PSHCLCALLBACKS;
/** @} */

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_h */

