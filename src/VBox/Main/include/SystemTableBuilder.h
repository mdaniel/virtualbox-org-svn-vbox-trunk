/* $Id$ */
/** @file
 * VirtualBox system tables builder.
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

#ifndef MAIN_INCLUDED_SystemTableBuilder_h
#define MAIN_INCLUDED_SystemTableBuilder_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/acpi.h>
#include <iprt/vfs.h>

#include <iprt/formats/acpi-tables.h>


typedef enum SYSTEMTABLETYPE
{
    kSystemTableType_Invalid = 0,
    kSystemTableType_Acpi,
    kSystemTableType_Fdt
} SYSTEMTABLETYPE;

class SystemTableBuilder
{
public:
    static SystemTableBuilder *createInstance(SYSTEMTABLETYPE enmTableType);

    virtual ~SystemTableBuilder() { };
    SystemTableBuilder() { };

    virtual int initInstance(void);
    virtual int finishTables(RTGCPHYS GCPhysTblsStart, RTVFSIOSTREAM hVfsIos, PRTGCPHYS pGCPhysTblRoot, size_t *pcbTblRoot, size_t *pcbTbls);

    virtual int addCpu(uint32_t idCpu);
    virtual int addMemory(RTGCPHYS GCPhysStart, RTGCPHYS cbMem);
    virtual int addMmioDeviceNoIrq(const char *pszVBoxName, uint32_t uInstance, RTGCPHYS GCPhysMmio, RTGCPHYS cbMmio);
    virtual int addMmioDevice(const char *pszVBoxName, uint32_t uInstance, RTGCPHYS GCPhysMmio, RTGCPHYS cbMmio, uint32_t u32Irq);
    virtual int configureGic(uint32_t cCpus, RTGCPHYS GCPhysIntcDist, RTGCPHYS cbMmioIntcDist, RTGCPHYS GCPhysIntcReDist, RTGCPHYS cbMmioIntcReDist);
    virtual int configureClock(void);
    virtual int configurePcieRootBus(const char *pszVBoxName, uint32_t aPinIrqs[4], RTGCPHYS GCPhysMmioPio, RTGCPHYS GCPhysMmioEcam, size_t cbPciMmioEcam,
                                     RTGCPHYS GCPhysPciMmioBase, RTGCPHYS cbPciMmio, RTGCPHYS GCPhysPciMmio32Base, RTGCPHYS cbPciMmio32);
    virtual int configureTpm2(bool fCrb, RTGCPHYS GCPhysMmioStart, RTGCPHYS cbMmio, uint32_t u32Irq);


    virtual int dumpTables(const char *pszFilename);
};


class SystemTableBuilderAcpi: public SystemTableBuilder
{
public:
    SystemTableBuilderAcpi() { };
    ~SystemTableBuilderAcpi() { };

    int initInstance(void);
    int finishTables(RTGCPHYS GCPhysTblsStart, RTVFSIOSTREAM hVfsIos, PRTGCPHYS pGCPhysTblRoot, size_t *pcbTblRoot, size_t *pcbTbls);

    int addCpu(uint32_t idCpu);
    int addMemory(RTGCPHYS GCPhysStart, RTGCPHYS cbMem);
    int addMmioDeviceNoIrq(const char *pszVBoxName, uint32_t uInstance, RTGCPHYS GCPhysMmio, RTGCPHYS cbMmio);
    int addMmioDevice(const char *pszVBoxName, uint32_t uInstance, RTGCPHYS GCPhysMmio, RTGCPHYS cbMmio, uint32_t u32Irq);
    int configureGic(uint32_t cCpus, RTGCPHYS GCPhysIntcDist, RTGCPHYS cbMmioIntcDist, RTGCPHYS GCPhysIntcReDist, RTGCPHYS cbMmioIntcReDist);
    int configureClock(void);
    int configurePcieRootBus(const char *pszVBoxName, uint32_t aPinIrqs[4], RTGCPHYS GCPhysMmioPio, RTGCPHYS GCPhysMmioEcam, size_t cbPciMmioEcam,
                             RTGCPHYS GCPhysPciMmioBase, RTGCPHYS cbPciMmio, RTGCPHYS GCPhysPciMmio32Base, RTGCPHYS cbPciMmio32);
    int configureTpm2(bool fCrb, RTGCPHYS GCPhysMmioStart, RTGCPHYS cbMmio, uint32_t u32Irq);

    int dumpTables(const char *pszFilename);

private:
    int buildMadt(RTVFSIOSTREAM hVfsIos, size_t *pcbMadt);
    int buildMcfg(RTVFSIOSTREAM hVfsIos, size_t *pcbMcfg);
    int buildGtdt(RTVFSIOSTREAM hVfsIos, size_t *pcbGtdt);
    int buildFadt(RTVFSIOSTREAM hVfsIos, RTGCPHYS GCPhysXDsdt, size_t *pcbFadt);
    int buildTpm20(RTVFSIOSTREAM hVfsIos, size_t *pcbTpm20);

    RTACPITBL m_hAcpiDsdt;
    RTACPIRES m_hAcpiRes;

    uint32_t m_cCpus;
    RTGCPHYS m_GCPhysIntcDist;
    RTGCPHYS m_cbMmioIntcDist;
    RTGCPHYS m_GCPhysIntcReDist;
    RTGCPHYS m_cbMmioIntcReDist;

    RTGCPHYS m_GCPhysPciMmioEcam;

    bool m_fTpm20;
    bool m_fCrb;
    RTGCPHYS m_GCPhysTpm20Mmio;
};

#endif /* !MAIN_INCLUDED_SystemTableBuilder_h */
