/* $Id$ */
/** @file
 * DevEFI - firmware binaries.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


#ifdef RT_OS_DARWIN
#define NAME(a_Name) _##a_Name
#else
#define NAME(a_Name) a_Name
#endif

#ifdef RT_OS_DARWIN
.section __TEXT,__const
#else
.section .text
#endif

/* 32-bit firmware: */
.p2align 6
.globl NAME(g_abEfiFirmwareX86)
NAME(g_abEfiFirmwareX86):
        .incbin "VBoxEFI-x86.fd"
end_x86_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmwareX86)
NAME(g_cbEfiFirmwareX86):
        .long end_x86_firmware - NAME(g_abEfiFirmwareX86)


/* 64-bit firmware: */
.p2align 6
.globl NAME(g_abEfiFirmwareAmd64)
NAME(g_abEfiFirmwareAmd64):
        .incbin "VBoxEFI-amd64.fd"
end_amd64_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmwareAmd64)
NAME(g_cbEfiFirmwareAmd64):
        .long  end_amd64_firmware - NAME(g_abEfiFirmwareAmd64)

#ifdef ASM_FORMAT_ELF
.size g_abEfiFirmwareX86, end_x86_firmware - NAME(g_abEfiFirmwareX86)
.type g_abEfiFirmwareX86 object
.size g_cbEfiFirmwareX86, 4
.type g_cbEfiFirmwareX86 object

.size g_abEfiFirmwareAmd64, end_amd64_firmware - NAME(g_abEfiFirmwareAmd64)
.type g_abEfiFirmwareAmd64 object
.size g_cbEfiFirmwareAmd64, 4
.type g_cbEfiFirmwareAmd64 object

#elif defined(ASM_FORMAT_PE)
/* Explit exportint: */
.section .drectve
.string "-export:g_abEfiFirmwareX86 -export:g_cbEfiFirmwareX86 -export:g_abEfiFirmwareAmd64 -export:g_cbEfiFirmwareAmd64"
.section .text

#endif

#ifdef VBOX_WITH_VIRT_ARMV8
/*
 * The ARMv8 bits
 */

/* 32-bit firmware: */
.p2align 6
.globl NAME(g_abEfiFirmwareArm32)
NAME(g_abEfiFirmwareArm32):
        .incbin "VBoxEFI-arm32.fd"
end_arm32_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmwareArm32)
NAME(g_cbEfiFirmwareArm32):
        .long end_arm32_firmware - NAME(g_abEfiFirmwareArm32)


/* 64-bit firmware: */
.p2align 6
.globl NAME(g_abEfiFirmwareArm64)
NAME(g_abEfiFirmwareArm64):
        .incbin "VBoxEFI-arm64.fd"
end_arm64_firmware:

.p2align 2
.globl NAME(g_cbEfiFirmwareArm64)
NAME(g_cbEfiFirmwareArm64):
        .long  end_arm64_firmware - NAME(g_abEfiFirmwareArm64)

# ifdef ASM_FORMAT_ELF
.size g_abEfiFirmwareArm32, end_arm32_firmware - NAME(g_abEfiFirmwareArm32)
.type g_abEfiFirmwareArm32 object
.size g_cbEfiFirmwareArm32, 4
.type g_cbEfiFirmwareArm32 object

.size g_abEfiFirmwareArm64, end_arm64_firmware - NAME(g_abEfiFirmwareArm64)
.type g_abEfiFirmwareArm64 object
.size g_cbEfiFirmwareArm64, 4
.type g_cbEfiFirmwareArm64 object

#elif defined(ASM_FORMAT_PE)
/* Explit exportint: */
.section .drectve
.string "-export:g_abEfiFirmwareArm32 -export:g_cbEfiFirmwareArm32 -export:g_abEfiFirmwareArm64 -export:g_cbEfiFirmwareArm64"
.section .text

# endif
#endif /*VBOX_WITH_VIRT_ARMV8 */
