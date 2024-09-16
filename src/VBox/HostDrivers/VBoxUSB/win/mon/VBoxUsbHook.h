/* $Id$ */
/** @file
 * Driver Dispatch Table Hooking API impl
 */
/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_VBoxUSB_win_mon_VBoxUsbHook_h
#define VBOX_INCLUDED_SRC_VBoxUSB_win_mon_VBoxUsbHook_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxUsbMon.h"

typedef struct VBOXUSBHOOK_ENTRY
{
    LIST_ENTRY RequestList;
    KSPIN_LOCK Lock;
    BOOLEAN fIsInstalled;
    PDRIVER_DISPATCH pfnOldHandler;
    VBOXDRVTOOL_REF HookRef;
    PDRIVER_OBJECT pDrvObj;
    UCHAR iMjFunction;
    PDRIVER_DISPATCH pfnHook;
} VBOXUSBHOOK_ENTRY, *PVBOXUSBHOOK_ENTRY;

typedef struct VBOXUSBHOOK_REQUEST
{
    LIST_ENTRY ListEntry;
    PVBOXUSBHOOK_ENTRY pHook;
    IO_STACK_LOCATION OldLocation;
    PDEVICE_OBJECT pDevObj;
    PIRP pIrp;
    BOOLEAN bCompletionStopped;
} VBOXUSBHOOK_REQUEST, *PVBOXUSBHOOK_REQUEST;

DECLINLINE(BOOLEAN) VBoxUsbHookRetain(PVBOXUSBHOOK_ENTRY pHook)
{
    KIRQL Irql;
    KeAcquireSpinLock(&pHook->Lock, &Irql);
    if (!pHook->fIsInstalled)
    {
        KeReleaseSpinLock(&pHook->Lock, Irql);
        return FALSE;
    }

    VBoxDrvToolRefRetain(&pHook->HookRef);
    KeReleaseSpinLock(&pHook->Lock, Irql);
    return TRUE;
}

DECLINLINE(VOID) VBoxUsbHookRelease(PVBOXUSBHOOK_ENTRY pHook)
{
    VBoxDrvToolRefRelease(&pHook->HookRef);
}

VOID VBoxUsbHookInit(PVBOXUSBHOOK_ENTRY pHook, PDRIVER_OBJECT pDrvObj, UCHAR iMjFunction, PDRIVER_DISPATCH pfnHook);
NTSTATUS VBoxUsbHookInstall(PVBOXUSBHOOK_ENTRY pHook);
NTSTATUS VBoxUsbHookUninstall(PVBOXUSBHOOK_ENTRY pHook);
BOOLEAN VBoxUsbHookIsInstalled(PVBOXUSBHOOK_ENTRY pHook);
NTSTATUS VBoxUsbHookRequestPassDownHookCompletion(PVBOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PIO_COMPLETION_ROUTINE pfnCompletion, PVBOXUSBHOOK_REQUEST pRequest);
NTSTATUS VBoxUsbHookRequestPassDownHookSkip(PVBOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS VBoxUsbHookRequestMoreProcessingRequired(PVBOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVBOXUSBHOOK_REQUEST pRequest);
NTSTATUS VBoxUsbHookRequestComplete(PVBOXUSBHOOK_ENTRY pHook, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVBOXUSBHOOK_REQUEST pRequest);
VOID VBoxUsbHookVerifyCompletion(PVBOXUSBHOOK_ENTRY pHook, PVBOXUSBHOOK_REQUEST pRequest, PIRP pIrp);

#endif /* !VBOX_INCLUDED_SRC_VBoxUSB_win_mon_VBoxUsbHook_h */
