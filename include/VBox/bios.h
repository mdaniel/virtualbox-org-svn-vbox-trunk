/** @file
 * X86 (and AMD64) Local APIC registers (VMM,++).
 *
 * bios.mac is generated from this file by running 'kmk -f Maintenance.kmk incs'.
 */

/*
 * Copyright (C) 2017-2024 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_bios_h
#define VBOX_INCLUDED_bios_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** The BIOS control port.
 * Write "Shutdown" byte by byte to shutdown the VM.
 * Write "Bootfail" to indicate no bootable device.
 * Write "Prochalt" to execute alternative CPU halt.
 * @sa VBOX_BIOS_OLD_SHUTDOWN_PORT  */
#define VBOX_BIOS_SHUTDOWN_PORT                 0x040f

/** Write this value to shut down VM. */
#define VBOX_BIOS_CTL_SHUTDOWN                  0x8001
/** Write this value to report boot failure. */
#define VBOX_BIOS_CTL_BOOTFAIL                  0x8002
/** Write this value to halt CPU. */
#define VBOX_BIOS_CTL_PROCHALT                  0x8003

/** The old shutdown port number.
 * Older versions of VirtualBox uses this as does Bochs.
 * @sa VBOX_BIOS_CONTROL_PORT  */
#define VBOX_BIOS_OLD_SHUTDOWN_PORT             0x8900


#endif /* !VBOX_INCLUDED_bios_h */

