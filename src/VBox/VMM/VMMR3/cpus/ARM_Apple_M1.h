/* $Id$ */
/** @file
 * CPU database entry "Apple M1".
 * Handcrafted placeholder.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef VBOX_CPUDB_ARM_Apple_M1_h
#define VBOX_CPUDB_ARM_Apple_M1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/**
 * Database entry for Apple M1
 */
static CPUMDBENTRYARM const g_Entry_ARM_Apple_M1 =
{
    {
        /*.pszName      = */ "Apple M1",
        /*.pszFullName  = */ "Apple M1",
        /*.enmVendor    = */ CPUMCPUVENDOR_APPLE,
        /*.enmMicroarch = */ kCpumMicroarch_Apple_M1,
        /*.fFlags       = */ 0,
        /*.enmEntryType = */ CPUMDBENTRYTYPE_ARM,
    },
    0 /** @todo */
};

#endif /* !VBOX_CPUDB_ARM_Apple_M1_h */

