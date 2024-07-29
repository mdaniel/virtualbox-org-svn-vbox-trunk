/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#ifndef VBOX
#define DBG_CALL (1 << 0)
#define DBG_MISC (1 << 1)
#define DBG_ERROR (1 << 2)
#define DBG_TFTP (1 << 3)
#define DBG_VERBOSE_CALL (1 << 4)

extern int slirp_debug;

#define DEBUG_CALL(name)                          \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_CALL)) { \
            g_debug(name "...");                  \
        }                                         \
    } while (0)

#define DEBUG_VERBOSE_CALL(name)                          \
    do {                                                  \
        if (G_UNLIKELY(slirp_debug & DBG_VERBOSE_CALL)) { \
            g_debug(name "...");                          \
        }                                                 \
    } while (0)

#define DEBUG_RAW_CALL(...)                       \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_CALL)) { \
            g_debug(__VA_ARGS__);                 \
        }                                         \
    } while (0)

#define DEBUG_ARG(...)                            \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_CALL)) { \
            g_debug(" " __VA_ARGS__);             \
        }                                         \
    } while (0)

#define DEBUG_MISC(...)                           \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_MISC)) { \
            g_debug(__VA_ARGS__);                 \
        }                                         \
    } while (0)

#define DEBUG_ERROR(...)                           \
    do {                                           \
        if (G_UNLIKELY(slirp_debug & DBG_ERROR)) { \
            g_debug(__VA_ARGS__);                  \
        }                                          \
    } while (0)

#define DEBUG_TFTP(...)                           \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_TFTP)) { \
            g_debug(__VA_ARGS__);                 \
        }                                         \
    } while (0)

#else  /* VBOX */
/*
 * Map these onto logging and make them compatible with Visual C++.
 */
# ifndef LOG_GROUP
#  define LOG_GROUP LOG_GROUP_DRV_NAT
#  include <VBox/log.h>
# endif
# define DEBUG_ERROR(...)           do { Log2((__VA_ARGS__)); Log2(("\n")); } while (0)
# define DEBUG_CALL(name)           do { Log3((name)); Log3(("\n")); } while (0)
# define DEBUG_RAW_CALL(...)        do { Log((__VA_ARGS__)); Log3(("\n")); } while (0)
# define DEBUG_VERBOSE_CALL(name)   do { Log7((name)); Log7(("\n")); } while (0)
# define DEBUG_ARG(...)             do { Log5((__VA_ARGS__)); Log5(("\n")); } while (0)
# define DEBUG_MISC(...)            do { Log6((__VA_ARGS__)); Log6(("\n")); } while (0)
# define DEBUG_IS_MISC_ENABLED()    LogIs6Enabled()
# define DEBUG_TFTP(...)            do { Log7((__VA_ARGS__)); Log7(("\n")); } while (0)
#endif /* VBOX */

#endif /* DEBUG_H_ */
