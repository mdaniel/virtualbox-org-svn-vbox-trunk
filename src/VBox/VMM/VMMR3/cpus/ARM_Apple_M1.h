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
 * System/Id Register values for Apple M1.
 */
static SUPARMSYSREGVAL const g_aSysRegVals_ARM_Apple_M1[] =
{
    { UINT64_C(0x00000000611f0221), ARMV8_AARCH64_SYSREG_MIDR_EL1 },
    { UINT64_C(0x0000000080000000), ARMV8_AARCH64_SYSREG_MPIDR_EL1 },
    { UINT64_C(0x0000000010305f09), ARMV8_AARCH64_SYSREG_ID_AA64DFR0_EL1 },
    { UINT64_C(0x0221100110212120), ARMV8_AARCH64_SYSREG_ID_AA64ISAR0_EL1 },
    { UINT64_C(0x0000011110211202), ARMV8_AARCH64_SYSREG_ID_AA64ISAR1_EL1 },
};


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
    /*.bImplementer     = */ 0x61,
    /*.bRevision        = */ 0x01,
    /*.uPartNum         = */ 0x0022,
    /*.cSysRegVals      = */ RT_ELEMENTS(g_aSysRegVals_ARM_Apple_M1),
    /*.paSysRegVals     = */ g_aSysRegVals_ARM_Apple_M1,
};

#endif /* !VBOX_CPUDB_ARM_Apple_M1_h */

