/* $Id$ */
/** @file
 * ConsoleControl.h
 */

/*
 * Copyright (C) 2009-2023 Oracle and/or its affiliates.
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

/*
 * This code is based on:
 *
 * Copyright (c) 2004 - 2006, Intel Corporation
 * All rights reserved. This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 * Module Name:
 *
 * ConsoleControl.h
 *
 * Abstract:
 *
 * Abstraction of a Text mode or GOP/UGA screen
 */

#ifndef __CONSOLE_CONTROL_H__
#define __CONSOLE_CONTROL_H__

#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID \
  { 0xf42f7782, 0x12e, 0x4c12, {0x99, 0x56, 0x49, 0xf9, 0x43, 0x4, 0xf7, 0x21} }

typedef struct _EFI_CONSOLE_CONTROL_PROTOCOL   EFI_CONSOLE_CONTROL_PROTOCOL;


typedef enum {
  EfiConsoleControlScreenText,
  EfiConsoleControlScreenGraphics,
  EfiConsoleControlScreenMaxValue
} EFI_CONSOLE_CONTROL_SCREEN_MODE;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  OUT EFI_CONSOLE_CONTROL_SCREEN_MODE   *Mode,
  OUT BOOLEAN                           *GopUgaExists,  OPTIONAL
  OUT BOOLEAN                           *StdInLocked    OPTIONAL
  )
/*++

  Routine Description:
    Return the current video mode information. Also returns info about existence
    of Graphics Output devices or UGA Draw devices in system, and if the Std In
    device is locked. All the arguments are optional and only returned if a non
    NULL pointer is passed in.

  Arguments:
    This         - Protocol instance pointer.
    Mode         - Are we in text of graphics mode.
    GopUgaExists - TRUE if Console Splitter has found a GOP or UGA device
    StdInLocked  - TRUE if StdIn device is keyboard locked

  Returns:
    EFI_SUCCESS     - Mode information returned.

--*/
;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  OUT EFI_CONSOLE_CONTROL_SCREEN_MODE   Mode
  )
/*++

  Routine Description:
    Set the current mode to either text or graphics. Graphics is
    for Quiet Boot.

  Arguments:
    This  - Protocol instance pointer.
    Mode  - Mode to set the

  Returns:
    EFI_SUCCESS     - Mode information returned.

--*/
;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  IN CHAR16                             *Password
  )
/*++

  Routine Description:
    Lock Std In devices until Password is typed.

  Arguments:
    This     - Protocol instance pointer.
    Password - Password needed to unlock screen. NULL means unlock keyboard

  Returns:
    EFI_SUCCESS      - Mode information returned.
    EFI_DEVICE_ERROR - Std In not locked

--*/
;



struct _EFI_CONSOLE_CONTROL_PROTOCOL {
  EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE           GetMode;
  EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE           SetMode;
  EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN        LockStdIn;
};

extern EFI_GUID gEfiConsoleControlProtocolGuid;

#endif
