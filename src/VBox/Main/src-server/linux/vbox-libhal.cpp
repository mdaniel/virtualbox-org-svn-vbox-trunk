/* $Id$ */
/** @file
 *
 * Module to dynamically load libhal and libdbus and load all symbols
 * which are needed by VirtualBox.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vbox-libhal.h"

#include <iprt/err.h>
#include <iprt/ldr.h>

/**
 * Whether we have tried to load libdbus and libhal yet.  This flag should only be set
 * to "true" after we have either loaded both libraries and all symbols which we need,
 * or failed to load something and unloaded and set back to zero pLibDBus and pLibHal.
 */
static bool gCheckedForLibHal = false;
/**
 * Pointer to the libhal shared object.  This should only be set once all needed libraries
 * and symbols have been successfully loaded.
 */
static RTLDRMOD ghLibHal = 0;

/** The following are the symbols which we need from libdbus and libhal. */
void (*gDBusErrorInit)(DBusError *);
DBusConnection *(*gDBusBusGet)(DBusBusType, DBusError *);
void (*gDBusErrorFree)(DBusError *);
void (*gDBusConnectionUnref)(DBusConnection *);
LibHalContext *(*gLibHalCtxNew)(void);
dbus_bool_t (*gLibHalCtxSetDBusConnection)(LibHalContext *, DBusConnection *);
dbus_bool_t (*gLibHalCtxInit)(LibHalContext *, DBusError *);
char **(*gLibHalFindDeviceStringMatch)(LibHalContext *, const char *, const char *, int *,
        DBusError *);
char *(*gLibHalDeviceGetPropertyString)(LibHalContext *, const char *, const char *, DBusError *);
void (*gLibHalFreeString)(char *);
void (*gLibHalFreeStringArray)(char **);
dbus_bool_t (*gLibHalCtxShutdown)(LibHalContext *, DBusError *);
dbus_bool_t (*gLibHalCtxFree)(LibHalContext *);

bool gLibHalCheckPresence(void)
{
    RTLDRMOD hLibHal;

    if (ghLibHal != 0 && gCheckedForLibHal == true)
        return true;
    if (gCheckedForLibHal == true)
        return false;
    if (!RT_SUCCESS(RTLdrLoad(LIB_HAL, &hLibHal)))
    {
        return false;
    }
    if (   RT_SUCCESS(RTLdrGetSymbol(hLibHal, "dbus_error_init", (void **) &gDBusErrorInit))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "dbus_bus_get", (void **) &gDBusBusGet))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "dbus_error_free", (void **) &gDBusErrorFree))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "dbus_connection_unref",
                                     (void **) &gDBusConnectionUnref))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_new", (void **) &gLibHalCtxNew))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_set_dbus_connection",
                                     (void **) &gLibHalCtxSetDBusConnection))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_init", (void **) &gLibHalCtxInit))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_manager_find_device_string_match",
                                     (void **) &gLibHalFindDeviceStringMatch))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_device_get_property_string",
                                     (void **) &gLibHalDeviceGetPropertyString))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_free_string", (void **) &gLibHalFreeString))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_free_string_array",
                                     (void **) &gLibHalFreeStringArray))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_shutdown", (void **) &gLibHalCtxShutdown))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_free", (void **) &gLibHalCtxFree))
       )
    {
        ghLibHal = hLibHal;
        gCheckedForLibHal = true;
        return true;
    }
    else
    {
        RTLdrClose(hLibHal);
        gCheckedForLibHal = true;
        return false;
    }
}
