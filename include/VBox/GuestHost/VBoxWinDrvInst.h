/* $Id$ */
/** @file
 * VBoxWinDrvInst - Header for Windows driver installation handling.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_GuestHost_VBoxWinDrvInst_h
#define VBOX_INCLUDED_GuestHost_VBoxWinDrvInst_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** Windows driver installer handle. */
typedef R3PTRTYPE(struct VBOXWINDRVINSTINTERNAL *) VBOXWINDRVINST;
/** Pointer to a Windows driver installer handle. */
typedef VBOXWINDRVINST                            *PVBOXWINDRVINST;
/** Nil Windows driver installer handle. */
#define NIL_VBOXWINDRVINST                      ((VBOXWINDRVINST)0)

/**
 * Enumeration for the Windows driver installation logging type.
 *
 * Used by the log message callback.
 */
typedef enum VBOXWINDRIVERLOGTYPE
{
    VBOXWINDRIVERLOGTYPE_INVALID = 0,
    VBOXWINDRIVERLOGTYPE_INFO,
    VBOXWINDRIVERLOGTYPE_VERBOSE,
    VBOXWINDRIVERLOGTYPE_WARN,
    VBOXWINDRIVERLOGTYPE_ERROR,
    /** If the (un)installation indicates that a system reboot is required. */
    VBOXWINDRIVERLOGTYPE_REBOOT_NEEDED
} VBOXWINDRIVERLOGTYPE;

/**
 * Log message callback.
 *
 * @param   enmType     Log type.
 * @param   pszMsg      Log message.
 * @param   pvUser      User-supplied pointer. Might be NULL if not being used.
 */
typedef DECLCALLBACKTYPE(void, FNVBOXWINDRIVERLOGMSG,(VBOXWINDRIVERLOGTYPE enmType, const char *pszMsg, void *pvUser));
/** Pointer to message callback function. */
typedef FNVBOXWINDRIVERLOGMSG *PFNVBOXWINDRIVERLOGMSG;

/** No flags specified. */
#define VBOX_WIN_DRIVERINSTALL_F_NONE       0
/** Try a silent installation (if possible). */
#define VBOX_WIN_DRIVERINSTALL_F_SILENT     RT_BIT(0)
/** Force driver installation, even if a newer driver version already is installed (overwrite). */
#define VBOX_WIN_DRIVERINSTALL_F_FORCE      RT_BIT(1)
/** Validation mask. */
#define VBOX_WIN_DRIVERINSTALL_F_VALID_MASK 0x3

int VBoxWinDrvInstCreate(PVBOXWINDRVINST hDrvInst);
int VBoxWinDrvInstCreateEx(PVBOXWINDRVINST phDrvInst, unsigned uVerbosity, PFNVBOXWINDRIVERLOGMSG pfnLog, void *pvUser);
int VBoxWinDrvInstDestroy(VBOXWINDRVINST hDrvInst);
unsigned VBoxWinDrvInstGetWarnings(VBOXWINDRVINST hDrvInst);
unsigned VBoxWinDrvInstGetErrors(VBOXWINDRVINST hDrvInst);
unsigned VBoxWinDrvInstSetVerbosity(VBOXWINDRVINST hDrvInst, uint8_t uVerbosity);
void VBoxWinDrvInstSetLogCallback(VBOXWINDRVINST hDrvInst, PFNVBOXWINDRIVERLOGMSG pfnLog, void *pvUser);
int VBoxWinDrvInstInstallEx(VBOXWINDRVINST hDrvInst, const char *pszInfFile, const char *pszModel, const char *pszPnpId, uint32_t fFlags);
int VBoxWinDrvInstInstall(VBOXWINDRVINST hDrvInst, const char *pszInfFile, uint32_t fFlags);
int VBoxWinDrvInstInstallExecuteInf(VBOXWINDRVINST hDrvInst, const char *pszInfFile, const char *pszSection, uint32_t fFlags);
int VBoxWinDrvInstUninstall(VBOXWINDRVINST hDrvInst, const char *pszInfFile, const char *pszModel, const char *pszPnPId, uint32_t fFlags);
int VBoxWinDrvInstUninstallExecuteInf(VBOXWINDRVINST hDrvInst, const char *pszInfFile, const char *pszSection, uint32_t fFlags);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_GuestHost_VBoxWinDrvInst_h */

