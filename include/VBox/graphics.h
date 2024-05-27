/** @file
 * Common defines which are being used by various graphics implementations
 * (VBoxVGA, VBoxSVGA, VMSVGA) and Main.
 */

/*
 * Copyright (C) 2017-2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_graphics_h
#define VBOX_INCLUDED_graphics_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** The default amount of VGA VRAM (in bytes). */
# define VGA_VRAM_DEFAULT           (_4M)
/** The minimum amount of VGA VRAM (in bytes). */
# define VGA_VRAM_MIN               (_1M)
/** The maximum amount of VGA VRAM (in bytes). Limited by VBOX_MAX_ALLOC_PAGE_COUNT. */
# define VGA_VRAM_MAX               (256 * _1M)

/** The minimum amount of SVGA VRAM (in bytes). */
#define SVGA_VRAM_MIN_SIZE          (4 * 640 * 480)
/** The maximum amount of SVGA VRAM (in bytes) when 3D acceleration is enabled. */
#define SVGA_VRAM_MIN_SIZE_3D       (16 * 1024 * 1024)
/** The maximum amount of SVGA VRAM (in bytes). */
#define SVGA_VRAM_MAX_SIZE          (128 * 1024 * 1024)

#endif /* !VBOX_INCLUDED_graphics_h */
