/* $Id$ */
/** @file
 * VBox Video drivers, logging helper
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXVIDEOLOG_H
#define VBOXVIDEOLOG_H

#ifndef VBOX_VIDEO_LOG_NAME
# error VBOX_VIDEO_LOG_NAME should be defined!
#endif

#ifndef VBOX_VIDEO_LOG_LOGGER
# define VBOX_VIDEO_LOG_LOGGER Log
#endif

#ifndef VBOX_VIDEO_LOGREL_LOGGER
# define VBOX_VIDEO_LOGREL_LOGGER LogRel
#endif

#ifndef VBOX_VIDEO_LOGFLOW_LOGGER
# define VBOX_VIDEO_LOGFLOW_LOGGER LogFlow
#endif

#ifndef VBOX_VIDEO_LOG_FN_FMT
# define VBOX_VIDEO_LOG_FN_FMT LOG_FN_FMT
#endif

#ifndef VBOX_VIDEO_LOG_FORMATTER
# define VBOX_VIDEO_LOG_FORMATTER(_logger, _severity, _a)                     \
    do                                                                      \
    {                                                                       \
        _logger((VBOX_VIDEO_LOG_PREFIX_FMT _severity, VBOX_VIDEO_LOG_PREFIX_PARMS));  \
        _logger(_a);                                                        \
        _logger((VBOX_VIDEO_LOG_SUFFIX_FMT  VBOX_VIDEO_LOG_SUFFIX_PARMS));  \
    } while (0)
#endif

/* Uncomment to show file/line info in the log */
/*#define VBOX_VIDEO_LOG_SHOWLINEINFO*/

#define VBOX_VIDEO_LOG_PREFIX_FMT VBOX_VIDEO_LOG_NAME"::"VBOX_VIDEO_LOG_FN_FMT": "
#define VBOX_VIDEO_LOG_PREFIX_PARMS __FUNCTION__

#ifdef VBOX_VIDEO_LOG_SHOWLINEINFO
# define VBOX_VIDEO_LOG_SUFFIX_FMT " (%s:%d)\n"
# define VBOX_VIDEO_LOG_SUFFIX_PARMS ,__FILE__, __LINE__
#else
# define VBOX_VIDEO_LOG_SUFFIX_FMT "\n"
# define VBOX_VIDEO_LOG_SUFFIX_PARMS
#endif

#ifdef DEBUG_misha
# define BP_WARN() AssertFailed()
#else
# define BP_WARN() do {} while(0)
#endif

#define _LOGMSG_EXACT(_logger, _a)                                          \
    do                                                                      \
    {                                                                       \
        _logger(_a);                                                        \
    } while (0)

#define _LOGMSG(_logger, _severity, _a)                                     \
    do                                                                      \
    {                                                                       \
        VBOX_VIDEO_LOG_FORMATTER(_logger, _severity, _a);                   \
    } while (0)

/* we can not print paged strings to RT logger, do it this way */
#define _LOGMSG_STR(_logger, _a, _f) do {\
        int _i = 0; \
        for (;(_a)[_i];++_i) { \
            _logger(("%"_f, (_a)[_i])); \
        }\
        _logger(("\n")); \
    } while (0)

#ifdef VBOX_WDDM_MINIPORT
# define _WARN_LOGGER VBOX_VIDEO_LOGREL_LOGGER
#else
# define _WARN_LOGGER VBOX_VIDEO_LOG_LOGGER
#endif

#define WARN_NOBP(_a) _LOGMSG(_WARN_LOGGER, "WARNING! :", _a)
#define WARN(_a)           \
    do                     \
    {                      \
        WARN_NOBP(_a);     \
        BP_WARN();         \
    } while (0)

#define ASSERT_WARN(_a, _w) do {\
        if(!(_a)) { \
            WARN(_w); \
        }\
    } while (0)

#define STOP_FATAL() do {      \
        AssertReleaseFailed(); \
    } while (0)
#define ERR(_a) do { \
        _LOGMSG(VBOX_VIDEO_LOGREL_LOGGER, "FATAL! :", _a); \
        STOP_FATAL();                             \
    } while (0)

#define _DBGOP_N_TIMES(_count, _op) do {    \
        static int fDoWarnCount = (_count); \
        if (fDoWarnCount) { \
            --fDoWarnCount; \
            _op; \
        } \
    } while (0)

#define WARN_ONCE(_a) do {    \
        _DBGOP_N_TIMES(1, WARN(_a)); \
    } while (0)


#define LOG(_a) _LOGMSG(VBOX_VIDEO_LOG_LOGGER, "", _a)
#define LOGREL(_a) _LOGMSG(VBOX_VIDEO_LOGREL_LOGGER, "", _a)
#define LOGF(_a) _LOGMSG(VBOX_VIDEO_LOGFLOW_LOGGER, "", _a)
#define LOGF_ENTER() LOGF(("ENTER"))
#define LOGF_LEAVE() LOGF(("LEAVE"))
#define LOG_EXACT(_a) _LOGMSG_EXACT(VBOX_VIDEO_LOG_LOGGER, _a)
#define LOGREL_EXACT(_a) _LOGMSG_EXACT(VBOX_VIDEO_LOGREL_LOGGER, _a)
#define LOGF_EXACT(_a) _LOGMSG_EXACT(VBOX_VIDEO_LOGFLOW_LOGGER, _a)
/* we can not print paged strings to RT logger, do it this way */
#define LOG_STRA(_a) do {\
        _LOGMSG_STR(VBOX_VIDEO_LOG_LOGGER, _a, "c"); \
    } while (0)
#define LOG_STRW(_a) do {\
        _LOGMSG_STR(VBOX_VIDEO_LOG_LOGGER, _a, "c"); \
    } while (0)
#define LOGREL_STRA(_a) do {\
        _LOGMSG_STR(VBOX_VIDEO_LOGREL_LOGGER, _a, "c"); \
    } while (0)
#define LOGREL_STRW(_a) do {\
        _LOGMSG_STR(VBOX_VIDEO_LOGREL_LOGGER, _a, "c"); \
    } while (0)


#endif /*VBOXVIDEOLOG_H*/
