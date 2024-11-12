/* $Id$ */
/** @file
 * VirtualBox resource assignment (Address ranges, interrupts) manager.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
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

#ifndef MAIN_INCLUDED_ResourceAssignmentManager_h
#define MAIN_INCLUDED_ResourceAssignmentManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBox/types.h"
#include "VirtualBoxBase.h"

class ResourceAssignmentManager
{
private:
    struct State;
    State *m_pState;
    RTGCPHYS m_GCPhysMmioHint;

    ResourceAssignmentManager();

public:
    static ResourceAssignmentManager *createInstance(PCVMMR3VTABLE pVMM, ChipsetType_T chipsetType, IommuType_T iommuType,
                                                     uint32_t cInterrupts, RTGCPHYS GCPhysMmioHint);

    ~ResourceAssignmentManager();

    /*
     * The allocation order for memory regions should be:
     *     1. All regions which require a fixed address (including RAM regions).
     *     2. The 32-bit MMIO regions.
     *     3. All the other MMIO regions.
     */
    HRESULT assignFixedRomRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion);
    HRESULT assignFixedRamRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion);
    HRESULT assignFixedMmioRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion);

    HRESULT assignMmioRegionAligned(const char *pszName, RTGCPHYS cbRegion, RTGCPHYS cbAlignment, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion,
                                    bool fOnly32Bit);
    HRESULT assignMmioRegion(const char *pszName, RTGCPHYS cbRegion, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion);
    HRESULT assignMmio32Region(const char *pszName, RTGCPHYS cbRegion, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion);

    HRESULT assignInterrupts(const char *pszName, uint32_t cInterrupts, uint32_t *piInterruptFirst);
    HRESULT assignSingleInterrupt(const char *pszName, uint32_t *piInterrupt);

    HRESULT queryMmioRegion(PRTGCPHYS pGCPhysMmioStart, PRTGCPHYS pcbMmio);
    HRESULT queryMmio32Region(PRTGCPHYS pGCPhysMmioStart, PRTGCPHYS pcbMmio);

    void dumpMemoryRegionsToReleaseLog(void);
};

#endif /* !MAIN_INCLUDED_ResourceAssignmentManager_h */
