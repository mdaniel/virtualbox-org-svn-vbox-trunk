/* $Id$ */
/** @file
 * PMU - Performance Monitoring Unit.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VMM_INCLUDED_SRC_include_PMUInternal_h
#define VMM_INCLUDED_SRC_include_PMUInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdmdev.h>


/** @defgroup grp_pmu_int       Internal
 * @ingroup grp_pmu
 * @internal
 * @{
 */


#ifdef IN_RING3
# define VMCPU_TO_PMU_DEVINS(a_pVCpu)           ((a_pVCpu)->pVMR3->pmu.s.pDevInsR3)
#elif defined(IN_RING0)
# error "Not implemented!"
#endif

/**
 * PMU PDM instance data (per-VM).
 */
typedef struct PMUDEV
{
    /** @todo */
    uint8_t bDummy;

} PMUDEV;
/** Pointer to a PMU device. */
typedef PMUDEV *PPMUDEV;
/** Pointer to a const PMU device. */
typedef PMUDEV const *PCPMUDEV;


/**
 * PMU VM Instance data.
 */
typedef struct PMU
{
    /** The ring-3 device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** Flag whether the in-kernel (KVM/Hyper-V) PMU of the NEM backend is used. */
    bool                        fNemPmu;
} PMU;
/** Pointer to PMU VM instance data. */
typedef PMU *PPMU;
/** Pointer to const PMU VM instance data. */
typedef PMU const *PCPMU;
AssertCompileSizeAlignment(PMU, 8);

/**
 * PMU VMCPU Instance data.
 */
typedef struct PMUCPU
{
    /** @name PMU statistics.
     * @{ */
#ifdef VBOX_WITH_STATISTICS
    /** Number of MSR reads in R3. */
    STAMCOUNTER                 StatSysRegReadR3;
    /** Number of MSR writes in R3. */
    STAMCOUNTER                 StatSysRegWriteR3;
#endif
    /** @} */
} PMUCPU;
/** Pointer to PMU VMCPU instance data. */
typedef PMUCPU *PPMUCPU;
/** Pointer to a const PMU VMCPU instance data. */
typedef PMUCPU const *PCPMUCPU;

DECLCALLBACK(int)                       pmuR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg);
DECLCALLBACK(int)                       pmuR3Destruct(PPDMDEVINS pDevIns);
DECLCALLBACK(void)                      pmuR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
DECLCALLBACK(void)                      pmuR3Reset(PPDMDEVINS pDevIns);

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_PMUInternal_h */

