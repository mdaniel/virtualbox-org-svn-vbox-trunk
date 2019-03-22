/** @file
 * Guest property service - Common header for host service and guest clients.
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

#ifndef ___VBox_HostService_GuestPropertyService_h
#define ___VBox_HostService_GuestPropertyService_h

#include <VBox/VMMDevCoreTypes.h>
#include <VBox/VBoxGuestCoreTypes.h>
#include <VBox/log.h>
#include <VBox/hgcmsvc.h>
#include <iprt/assertcompile.h>
#include <iprt/string.h>


/** Maximum length for property names */
#define GUEST_PROP_MAX_NAME_LEN             64
/** Maximum length for property values */
#define GUEST_PROP_MAX_VALUE_LEN            128
/** Maximum number of properties per guest */
#define GUEST_PROP_MAX_PROPS                256
/** Maximum size for enumeration patterns */
#define GUEST_PROP_MAX_PATTERN_LEN          1024
/** Maximum number of changes we remember for guest notifications */
#define GUEST_PROP_MAX_GUEST_NOTIFICATIONS  256


/** @name GUEST_PROP_F_XXX - The guest property flag values which are currently accepted.
 * @{
 */
#define GUEST_PROP_F_NILFLAG          UINT32_C(0)
/** Transient until VM gets shut down. */
#define GUEST_PROP_F_TRANSIENT        RT_BIT_32(1)
#define GUEST_PROP_F_RDONLYGUEST      RT_BIT_32(2)
#define GUEST_PROP_F_RDONLYHOST       RT_BIT_32(3)
/** Transient until VM gets a reset / restarts.
 *  Implies TRANSIENT. */
#define GUEST_PROP_F_TRANSRESET       RT_BIT_32(4)
#define GUEST_PROP_F_READONLY         (GUEST_PROP_F_RDONLYGUEST | GUEST_PROP_F_RDONLYHOST)
#define GUEST_PROP_F_ALLFLAGS         (GUEST_PROP_F_TRANSIENT | GUEST_PROP_F_READONLY | GUEST_PROP_F_TRANSRESET)
/** @} */

/**
 * Get the name of a flag as a string.
 * @returns the name, or NULL if fFlag is invalid.
 * @param   fFlag  the flag.  Must be a value from the ePropFlags enumeration
 *                 list.
 */
DECLINLINE(const char *) GuestPropFlagName(uint32_t fFlag)
{
    switch (fFlag)
    {
        case GUEST_PROP_F_TRANSIENT:
            return "TRANSIENT";
        case GUEST_PROP_F_RDONLYGUEST:
            return "RDONLYGUEST";
        case GUEST_PROP_F_RDONLYHOST:
            return "RDONLYHOST";
        case GUEST_PROP_F_READONLY:
            return "READONLY";
        case GUEST_PROP_F_TRANSRESET:
            return "TRANSRESET";
        default:
            break;
    }
    return NULL;
}

/**
 * Get the length of a flag name as returned by flagName.
 * @returns the length, or 0 if fFlag is invalid.
 * @param   fFlag  the flag.  Must be a value from the ePropFlags enumeration
 *                 list.
 */
DECLINLINE(size_t) GuestPropFlagNameLen(uint32_t fFlag)
{
    const char *pcszName = GuestPropFlagName(fFlag);
    return RT_LIKELY(pcszName != NULL) ? strlen(pcszName) : 0;
}

/**
 * Maximum length for the property flags field.  We only ever return one of
 * RDONLYGUEST, RDONLYHOST and RDONLY
 */
#define GUEST_PROP_MAX_FLAGS_LEN    sizeof("TRANSIENT, RDONLYGUEST, TRANSRESET")

/**
 * Parse a guest properties flags string for flag names and make sure that
 * there is no junk text in the string.
 *
 * @returns  IPRT status code
 * @retval   VERR_INVALID_PARAMETER if the flag string is not valid
 * @param    pcszFlags  the flag string to parse
 * @param    pfFlags    where to store the parse result.  May not be NULL.
 * @note     This function is also inline because it must be accessible from
 *           several modules and it does not seem reasonable to put it into
 *           its own library.
 */
DECLINLINE(int) GuestPropValidateFlags(const char *pcszFlags, uint32_t *pfFlags)
{
    static const uint32_t s_aFlagList[] =
    {
        GUEST_PROP_F_TRANSIENT, GUEST_PROP_F_READONLY, GUEST_PROP_F_RDONLYGUEST, GUEST_PROP_F_RDONLYHOST, GUEST_PROP_F_TRANSRESET
    };
    const char *pcszNext = pcszFlags;
    int rc = VINF_SUCCESS;
    uint32_t fFlags = 0;
    AssertLogRelReturn(VALID_PTR(pfFlags), VERR_INVALID_POINTER);

    if (pcszFlags)
    {
        while (' ' == *pcszNext)
            ++pcszNext;
        while ((*pcszNext != '\0') && RT_SUCCESS(rc))
        {
            unsigned i = 0;
            for (; i < RT_ELEMENTS(s_aFlagList); ++i)
                if (RTStrNICmp(pcszNext, GuestPropFlagName(s_aFlagList[i]), GuestPropFlagNameLen(s_aFlagList[i])) == 0)
                    break;
            if (RT_ELEMENTS(s_aFlagList) == i)
                rc = VERR_PARSE_ERROR;
            else
            {
                fFlags |= s_aFlagList[i];
                pcszNext += GuestPropFlagNameLen(s_aFlagList[i]);
                while (' ' == *pcszNext)
                    ++pcszNext;
                if (',' == *pcszNext)
                    ++pcszNext;
                else if (*pcszNext != '\0')
                    rc = VERR_PARSE_ERROR;
                while (' ' == *pcszNext)
                    ++pcszNext;
            }
        }
    }
    if (RT_SUCCESS(rc))
        *pfFlags = fFlags;
    return rc;
}


/**
 * Write out flags to a string.
 *
 * @returns  IPRT status code
 * @param    fFlags    The flags to write out.
 * @param    pszFlags  Where to write the flags string.
 * @param    cbFlags   The size of the destination buffer (in bytes).
 */
DECLINLINE(int) GuestPropWriteFlags(uint32_t fFlags, char* pszFlags, size_t cbFlags)
{
    /* Putting READONLY before the other RDONLY flags keeps the result short. */
    static const uint32_t s_aFlagList[] =
    {
        GUEST_PROP_F_TRANSIENT, GUEST_PROP_F_READONLY, GUEST_PROP_F_RDONLYGUEST, GUEST_PROP_F_RDONLYHOST, GUEST_PROP_F_TRANSRESET
    };
    int rc = VINF_SUCCESS;

    AssertLogRelReturn(VALID_PTR(pszFlags), VERR_INVALID_POINTER);
    AssertLogRelReturn(cbFlags,             VERR_INVALID_PARAMETER);

    pszFlags[0] = '\0';

    if ((fFlags & ~GUEST_PROP_F_ALLFLAGS) == GUEST_PROP_F_NILFLAG)
    {
        unsigned i;

        /* TRANSRESET implies TRANSIENT.  For compatability with old clients we
           always set TRANSIENT when TRANSRESET appears. */
        if (fFlags & GUEST_PROP_F_TRANSRESET)
            fFlags |= GUEST_PROP_F_TRANSIENT;

        for (i = 0; i < RT_ELEMENTS(s_aFlagList); ++i)
        {
            if (s_aFlagList[i] == (fFlags & s_aFlagList[i]))
            {
                rc = RTStrCat(pszFlags, cbFlags, GuestPropFlagName(s_aFlagList[i]));
                if (RT_FAILURE(rc))
                    break;

                fFlags &= ~s_aFlagList[i];

                if (fFlags != GUEST_PROP_F_NILFLAG)
                {
                    rc = RTStrCat(pszFlags, cbFlags, ", ");
                    if (RT_FAILURE(rc))
                        break;
                }
            }
        }

        Assert(fFlags == GUEST_PROP_F_NILFLAG); /* bad s_aFlagList */
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}


/** @name The service functions which are callable by host.
 * @{
 */
/** Set properties in a block.
 * The parameters are pointers to NULL-terminated arrays containing the
 * parameters.  These are, in order, name, value, timestamp, flags.  Strings are
 * stored as pointers to mutable utf8 data.  All parameters must be supplied. */
#define GUEST_PROP_FN_HOST_SET_PROPS        1
/** Get the value attached to a guest property.
 * The parameter format matches that of GET_PROP. */
#define GUEST_PROP_FN_HOST_GET_PROP         2
/** Set the value attached to a guest property.
 * The parameter format matches that of SET_PROP. */
#define GUEST_PROP_FN_HOST_SET_PROP         3
/** Set the value attached to a guest property.
 * The parameter format matches that of SET_PROP_VALUE. */
#define GUEST_PROP_FN_HOST_SET_PROP_VALUE   4
/** Remove a guest property.
 * The parameter format matches that of DEL_PROP. */
#define GUEST_PROP_FN_HOST_DEL_PROP         5
/** Enumerate guest properties.
 * The parameter format matches that of ENUM_PROPS. */
#define GUEST_PROP_FN_HOST_ENUM_PROPS       6
/** Set global flags for the service.
 * Currently RDONLYGUEST is supported.  Takes one 32-bit unsigned integer
 * parameter for the flags. */
#define GUEST_PROP_FN_HOST_SET_GLOBAL_FLAGS 7
/** Return the pointer to a debug info function enumerating all guest
 * properties. */
#define GUEST_PROP_FN_HOST_GET_DBGF_INFO    8
/** @} */


/** @name The service functions which are called by guest.
 *
 * @note The numbers may not change!
 * @{
 */
/** Get a guest property */
#define GUEST_PROP_FN_GET_PROP              1
/** Set a guest property */
#define GUEST_PROP_FN_SET_PROP              2
/** Set just the value of a guest property */
#define GUEST_PROP_FN_SET_PROP_VALUE        3
/** Delete a guest property */
#define GUEST_PROP_FN_DEL_PROP              4
/** Enumerate guest properties */
#define GUEST_PROP_FN_ENUM_PROPS            5
/** Poll for guest notifications */
#define GUEST_PROP_FN_GET_NOTIFICATION      6
/** @} */


/**
 * Data structure to pass to the service extension callback.
 * We use this to notify the host of changes to properties.
 */
typedef struct GUESTPROPHOSTCALLBACKDATA
{
    /** Magic number to identify the structure (GUESTPROPHOSTCALLBACKDATA_MAGIC). */
    uint32_t        u32Magic;
    /** The name of the property that was changed */
    const char     *pcszName;
    /** The new property value, or NULL if the property was deleted */
    const char     *pcszValue;
    /** The timestamp of the modification */
    uint64_t       u64Timestamp;
    /** The flags field of the modified property */
    const char     *pcszFlags;
} GUESTPROPHOSTCALLBACKDATA;
/** Poitner to a data structure to pass to the service extension callback. */
typedef GUESTPROPHOSTCALLBACKDATA *PGUESTPROPHOSTCALLBACKDATA;

/** Magic number for sanity checking the HOSTCALLBACKDATA structure */
#define GUESTPROPHOSTCALLBACKDATA_MAGIC     UINT32_C(0x69c87a78)

/**
 * HGCM parameter structures.  Packing is explicitly defined as this is a wire format.
 */
/** The guest is requesting the value of a property */
typedef struct GuestPropMsgGetProperty
{
    VBGLIOCHGCMCALL hdr;

    /**
     * The property name (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The returned string data will be placed here.  (OUT pointer)
     * This call returns two null-terminated strings which will be placed one
     * after another: value and flags.
     */
    HGCMFunctionParameter buffer;

    /**
     * The property timestamp.  (OUT uint64_t)
     */
    HGCMFunctionParameter timestamp;

    /**
     * If the buffer provided was large enough this will contain the size of
     * the returned data.  Otherwise it will contain the size of the buffer
     * needed to hold the data and VERR_BUFFER_OVERFLOW will be returned.
     * (OUT uint32_t)
     */
    HGCMFunctionParameter size;
} GuestPropMsgGetProperty;
AssertCompileSize(GuestPropMsgGetProperty, 40 + 4 * (ARCH_BITS == 64 ? 16 : 12));

/** The guest is requesting to change a property */
typedef struct GuestPropMsgSetProperty
{
    VBGLIOCHGCMCALL hdr;

    /**
     * The property name.  (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The value of the property (IN pointer)
     * Criteria as for the name parameter, but with length less than or equal to
     * MAX_VALUE_LEN.
     */
    HGCMFunctionParameter value;

    /**
     * The property flags (IN pointer)
     * This is a comma-separated list of the format flag=value
     * The length must be less than or equal to MAX_FLAGS_LEN and only
     * known flag names and values will be accepted.
     */
    HGCMFunctionParameter flags;
} GuestPropMsgSetProperty;
AssertCompileSize(GuestPropMsgSetProperty, 40 + 3 * (ARCH_BITS == 64 ? 16 : 12));

/** The guest is requesting to change the value of a property */
typedef struct GuestPropMsgSetPropertyValue
{
    VBGLIOCHGCMCALL hdr;

    /**
     * The property name.  (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The value of the property (IN pointer)
     * Criteria as for the name parameter, but with length less than or equal to
     * MAX_VALUE_LEN.
     */
    HGCMFunctionParameter value;
} GuestPropMsgSetPropertyValue;
AssertCompileSize(GuestPropMsgSetPropertyValue, 40 + 2 * (ARCH_BITS == 64 ? 16 : 12));

/** The guest is requesting to remove a property */
typedef struct GuestPropMsgDelProperty
{
    VBGLIOCHGCMCALL hdr;

    /**
     * The property name.  This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;
} GuestPropMsgDelProperty;
AssertCompileSize(GuestPropMsgDelProperty, 40 + 1 * (ARCH_BITS == 64 ? 16 : 12));

/** The guest is requesting to enumerate properties */
typedef struct GuestPropMsgEnumProperties
{
    VBGLIOCHGCMCALL hdr;

    /**
     * Array of patterns to match the properties against, separated by '|'
     * characters.  For backwards compatibility, '\\0' is also accepted
     * as a separater.
     * (IN pointer)
     * If only a single, empty pattern is given then match all.
     */
    HGCMFunctionParameter patterns;
    /**
     * On success, null-separated array of strings in which the properties are
     * returned.  (OUT pointer)
     * The number of strings in the array is always a multiple of four,
     * and in sequences of name, value, timestamp (hexadecimal string) and the
     * flags as a comma-separated list in the format "name=value".  The list
     * is terminated by an empty string after a "flags" entry (or at the
     * start).
     */
    HGCMFunctionParameter strings;
    /**
     * On success, the size of the returned data.  If the buffer provided is
     * too small, the size of buffer needed.  (OUT uint32_t)
     */
    HGCMFunctionParameter size;
} GuestPropMsgEnumProperties;
AssertCompileSize(GuestPropMsgEnumProperties, 40 + 3 * (ARCH_BITS == 64 ? 16 : 12));

/**
 * The guest is polling for notifications on changes to properties, specifying
 * a set of patterns to match the names of changed properties against and
 * optionally the timestamp of the last notification seen.
 * On success, VINF_SUCCESS will be returned and the buffer will contain
 * details of a property notification.  If no new notification is available
 * which matches one of the specified patterns, the call will block until one
 * is.
 * If the last notification could not be found by timestamp, VWRN_NOT_FOUND
 * will be returned and the oldest available notification will be returned.
 * If a zero timestamp is specified, the call will always wait for a new
 * notification to arrive.
 * If the buffer supplied was not large enough to hold the notification,
 * VERR_BUFFER_OVERFLOW will be returned and the size parameter will contain
 * the size of the buffer needed.
 *
 * The protocol for a guest to obtain notifications is to call
 * GET_NOTIFICATION in a loop.  On the first call, the ingoing timestamp
 * parameter should be set to zero.  On subsequent calls, it should be set to
 * the outgoing timestamp from the previous call.
 */
typedef struct GuestPropMsgGetNotification
{
    VBGLIOCHGCMCALL hdr;

    /**
     * A list of patterns to match the guest event name against, separated by
     * vertical bars (|) (IN pointer)
     * An empty string means match all.
     */
    HGCMFunctionParameter patterns;
    /**
     * The timestamp of the last change seen (IN uint64_t)
     * This may be zero, in which case the oldest available change will be
     * sent.  If the service does not remember an event matching the
     * timestamp, then VWRN_NOT_FOUND will be returned, and the guest should
     * assume that it has missed a certain number of notifications.
     *
     * The timestamp of the change being notified of (OUT uint64_t)
     * Undefined on failure.
     */
    HGCMFunctionParameter timestamp;

    /**
     * The returned data, if any, will be placed here.  (OUT pointer)
     * This call returns three null-terminated strings which will be placed
     * one after another: name, value and flags.  For a delete notification,
     * value and flags will be empty strings.  Undefined on failure.
     */
    HGCMFunctionParameter buffer;

    /**
     * On success, the size of the returned data.  (OUT uint32_t)
     * On buffer overflow, the size of the buffer needed to hold the data.
     * Undefined on failure.
     */
    HGCMFunctionParameter size;
} GuestPropMsgGetNotification;
AssertCompileSize(GuestPropMsgGetNotification, 40 + 4 * (ARCH_BITS == 64 ? 16 : 12));


#endif  /* !___VBox_HostService_GuestPropertySvc_h */

