/* $Id$ */
/** @file
 * VirtualBox bus slots assignment manager
 */

/*
 * Copyright (C) 2010-2024 Oracle and/or its affiliates.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN
#include "LoggingNew.h"

#include "ResourceAssignmentManager.h"

#include <VBox/com/array.h>

#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Address space range type.
 */
typedef enum RESOURCEREGIONTYPE
{
    /** Free region. */
    kResourceRegionType_Free = 0,
    /** RAM. */
    kResourceRegionType_Ram,
    /** ROM. */
    kResourceRegionType_Rom,
    /** MMIO. */
    kResourceRegionType_Mmio,
    /** 32bit hack. */
    kResourceRegionType_32Bit_Hack = 0x7fffffff
} RESOURCEREGIONTYPE;


/**
 * Address space range descriptor.
 */
typedef struct RESOURCEREGION
{
    /** Region name. */
    char                szName[32];
    /** Physical start address of the region. */
    RTGCPHYS            GCPhysStart;
    /** Physical end address of the region. */
    RTGCPHYS            GCPhysEnd;
    /** Region type. */
    RESOURCEREGIONTYPE  enmType;
    /** Padding. */
    uint32_t            u32Pad;
} RESOURCEREGION;
AssertCompileSize(RESOURCEREGION, 56);
/** Pointer to a resource region. */
typedef RESOURCEREGION *PRESOURCEREGION;
/** Pointer to a const resource region. */
typedef const RESOURCEREGION *PCRESOURCEREGION;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/**
 * Resource assignment manage state data.
 * @internal
 */
struct ResourceAssignmentManager::State
{
    ChipsetType_T    mChipsetType;
    IommuType_T      mIommuType;

    /** Pointer to the array of regions (sorted by address, not overlapping, adjacent). */
    PRESOURCEREGION  m_paRegions;
    /** Number of used regions. */
    uint32_t         m_cRegions;
    /** Number of regions the allocated array can hold. */
    uint32_t         m_cRegionsMax;

    uint32_t         mcInterrupts;
    uint32_t         miInterrupt;

    State()
        : mChipsetType(ChipsetType_Null)
        , mIommuType(IommuType_None)
        , m_paRegions(NULL)
        , m_cRegions(0)
        , m_cRegionsMax(0)
        , mcInterrupts(0)
        , miInterrupt(0)
    {}
    ~State()
    {}

    HRESULT init(ChipsetType_T chipsetType, IommuType_T iommuType, uint32_t cInterrupts);

    HRESULT ensureAdditionalRegions(uint32_t cRegions);
    void setRegion(PRESOURCEREGION pRegion, const char *pszName, RESOURCEREGIONTYPE enmType,
                   RTGCPHYS GCPhysStart, RTGCPHYS GCPhysEnd);
    HRESULT addAddrRange(const char *pszName, RESOURCEREGIONTYPE enmType, RTGCPHYS GCPhysStart, RTGCPHYS GCPhysEnd);
    HRESULT findNextFreeAddrRange(RTGCPHYS cbRegion, RTGCPHYS cbAlignment, RTGCPHYS GCPhysMax, RTGCPHYS GCPhysHint,
                                  PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion);

    void dumpToReleaseLog(void);
};


HRESULT ResourceAssignmentManager::State::init(ChipsetType_T chipsetType, IommuType_T iommuType,
                                               uint32_t cInterrupts)
{
    Assert(chipsetType == ChipsetType_ARMv8Virtual);
    Assert(iommuType == IommuType_None); /* For now no IOMMU support on ARMv8. */

    mChipsetType           = chipsetType;
    mIommuType             = iommuType;
    mcInterrupts           = cInterrupts;
    miInterrupt            = 0;

    m_paRegions = (PRESOURCEREGION)RTMemRealloc(m_paRegions, 32 * sizeof(*m_paRegions));
    if (!m_paRegions)
        return E_OUTOFMEMORY;

    m_paRegions[m_cRegions].enmType     = kResourceRegionType_Free;
    m_paRegions[m_cRegions].GCPhysStart = 0;
    m_paRegions[m_cRegions].GCPhysEnd   = UINT64_MAX;
    strcpy(&m_paRegions[m_cRegions].szName[0], "Free");
    m_cRegions++;

    return S_OK;
}


HRESULT ResourceAssignmentManager::State::ensureAdditionalRegions(uint32_t cRegions)
{
    if (m_cRegions + cRegions == m_cRegionsMax)
    {
        uint32_t const cRegionsNew = m_cRegionsMax + 32;
        PRESOURCEREGION paDescsNew = (PRESOURCEREGION)RTMemRealloc(m_paRegions, cRegionsNew * sizeof(*paDescsNew));
        if (!paDescsNew)
            return E_OUTOFMEMORY;

        m_paRegions   = paDescsNew;
        m_cRegionsMax = cRegionsNew;
    }

    return S_OK;
}


void ResourceAssignmentManager::State::setRegion(PRESOURCEREGION pRegion, const char *pszName, RESOURCEREGIONTYPE enmType,
                                                 RTGCPHYS GCPhysStart, RTGCPHYS GCPhysEnd)
{
    strncpy(&pRegion->szName[0], pszName, sizeof(pRegion->szName));
    pRegion->szName[sizeof(pRegion->szName) - 1] = '\0';
    pRegion->enmType     = enmType;
    pRegion->GCPhysStart = GCPhysStart;
    pRegion->GCPhysEnd   = GCPhysEnd;
}


HRESULT ResourceAssignmentManager::State::addAddrRange(const char *pszName, RESOURCEREGIONTYPE enmType,
                                                       RTGCPHYS GCPhysStart, RTGCPHYS cbRegion)
{
    RTGCPHYS GCPhysEnd = GCPhysStart + cbRegion - 1;

    /* Find the right spot in the array where to insert the range, it must not overlap with an existing used range. */
    uint32_t iRegion = 0;
    while (   iRegion < m_cRegions
           && GCPhysStart > m_paRegions[iRegion].GCPhysEnd)
        iRegion++;

    Assert(   iRegion == m_cRegions - 1
           || m_paRegions[iRegion].enmType != m_paRegions[iRegion + 1].enmType);

    /* Check that there is sufficient free space. */
    if (   (m_paRegions[iRegion].enmType != kResourceRegionType_Free)
        || GCPhysEnd > m_paRegions[iRegion].GCPhysEnd)
        return E_INVALIDARG;

    Assert(m_paRegions[iRegion].enmType == kResourceRegionType_Free);

    /* Requested region fits exactly into the free region. */
    if (   GCPhysStart == m_paRegions[iRegion].GCPhysStart
        && GCPhysEnd   == m_paRegions[iRegion].GCPhysEnd)
    {
        setRegion(&m_paRegions[iRegion], pszName, enmType, GCPhysStart, GCPhysEnd);
        return S_OK;
    }

    HRESULT hrc = ensureAdditionalRegions(2 /*cRegions*/); /* Need two additional region descriptors max. */
    if (FAILED(hrc))
        return hrc;

    /* Need to split the region into two or three depending on where the requested region lies. */
    if (GCPhysStart == m_paRegions[iRegion].GCPhysStart)
    {
        /* At the start, move the free regions and everything at the end. */
        memmove(&m_paRegions[iRegion + 1], &m_paRegions[iRegion], (m_cRegions - iRegion) * sizeof(*m_paRegions));
        setRegion(&m_paRegions[iRegion], pszName, enmType, GCPhysStart, GCPhysEnd);

        /* Adjust the free region. */
        m_paRegions[iRegion + 1].GCPhysStart = GCPhysStart + cbRegion;
        m_cRegions++;
    }
    else if (GCPhysStart + cbRegion - 1 == m_paRegions[iRegion].GCPhysEnd)
    {
        /* At the end of the region, move the remaining regions and adjust ranges. */
        if (iRegion < m_cRegions)
            memmove(&m_paRegions[iRegion + 2], &m_paRegions[iRegion + 1], (m_cRegions - iRegion) * sizeof(*m_paRegions));
        setRegion(&m_paRegions[iRegion + 1], pszName, enmType, GCPhysStart, GCPhysEnd);
        m_paRegions[iRegion].GCPhysEnd = GCPhysStart - 1;
        m_cRegions++;
    }
    else
    {
        /* Somewhere in the middle, split into three regions. */
        if (iRegion < m_cRegions)
            memmove(&m_paRegions[iRegion + 3], &m_paRegions[iRegion + 1], (m_cRegions - iRegion) * sizeof(*m_paRegions));
        setRegion(&m_paRegions[iRegion + 1], pszName, enmType, GCPhysStart, GCPhysEnd);
        setRegion(&m_paRegions[iRegion + 2], "Free", kResourceRegionType_Free, GCPhysStart + cbRegion, m_paRegions[iRegion].GCPhysEnd);
        m_paRegions[iRegion].GCPhysEnd = GCPhysStart - 1;
        m_cRegions += 2;
    }

    return S_OK;
}


HRESULT ResourceAssignmentManager::State::findNextFreeAddrRange(RTGCPHYS cbRegion, RTGCPHYS cbAlignment, RTGCPHYS GCPhysMax,
                                                                RTGCPHYS GCPhysHint, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion)
{
    AssertReturn(GCPhysMax >= cbRegion, E_FAIL);
    GCPhysMax -= cbRegion;

    uint32_t iRegion = 0;
    if (GCPhysHint)
    {
        /* Look for free address range downwards from the hint first. */
        iRegion = m_cRegions - 1;
        while (iRegion)
        {
            PCRESOURCEREGION pRegion = &m_paRegions[iRegion];

            /* Region must be free and satisfy the alignment++ requirements. */
            RTGCPHYS GCPhysStartAligned = RT_ALIGN_T(pRegion->GCPhysEnd - cbRegion + 1, cbAlignment, RTGCPHYS);
            if (   pRegion->enmType == kResourceRegionType_Free
                && GCPhysHint >= pRegion->GCPhysEnd
                && GCPhysMax >= pRegion->GCPhysEnd
                && GCPhysStartAligned >= pRegion->GCPhysStart
                && pRegion->GCPhysEnd - GCPhysStartAligned + 1 >= cbRegion)
            {
                *pGCPhysStart = GCPhysStartAligned;
                *pcbRegion    = cbRegion;
                return S_OK;
            }

            iRegion--;
        }
    }

    /* Find a free region which satisfies or requirements. */
    while (   iRegion < m_cRegions
           && GCPhysMax >= m_paRegions[iRegion].GCPhysStart)
    {
        PCRESOURCEREGION pRegion = &m_paRegions[iRegion];

        /* Region must be free and satisfy the alignment++ requirements. */
        RTGCPHYS GCPhysStartAligned = RT_ALIGN_T(pRegion->GCPhysStart, cbAlignment, RTGCPHYS);
        if (   pRegion->enmType == kResourceRegionType_Free
            && GCPhysStartAligned >= pRegion->GCPhysStart
            && pRegion->GCPhysEnd - GCPhysStartAligned + 1 >= cbRegion)
        {
            *pGCPhysStart = pRegion->GCPhysStart;
            *pcbRegion    = cbRegion;
            return S_OK;
        }

        iRegion++;
    }

    return E_OUTOFMEMORY;
}


static const char *resourceManagerRegionType2Str(RESOURCEREGIONTYPE enmType)
{
    switch (enmType)
    {
        case kResourceRegionType_Free: return "FREE";
        case kResourceRegionType_Ram:  return "RAM ";
        case kResourceRegionType_Rom:  return "ROM ";
        case kResourceRegionType_Mmio: return "MMIO";
        default: AssertFailed(); return "UNKNOWN";
    }
}


void ResourceAssignmentManager::State::dumpToReleaseLog(void)
{
    LogRel(("Memory Regions:\n"));
    for (uint32_t iRegion = 0; iRegion < m_cRegions; iRegion++)
    {
        LogRel(("    %RGp - %RGp (%zu) %s %s\n",
                m_paRegions[iRegion].GCPhysStart,
                m_paRegions[iRegion].GCPhysEnd,
                m_paRegions[iRegion].GCPhysEnd - m_paRegions[iRegion].GCPhysStart + 1,
                m_paRegions[iRegion].szName));
    }
}


ResourceAssignmentManager::ResourceAssignmentManager()
    : m_pState(NULL)
{
    m_pState = new State();
    Assert(m_pState);
}


ResourceAssignmentManager::~ResourceAssignmentManager()
{
    if (m_pState)
    {
        delete m_pState;
        m_pState = NULL;
    }
}


ResourceAssignmentManager *ResourceAssignmentManager::createInstance(PCVMMR3VTABLE pVMM, ChipsetType_T chipsetType, IommuType_T iommuType,
                                                                     uint32_t cInterrupts, RTGCPHYS GCPhysMmioHint)
{
    RT_NOREF(pVMM);

    ResourceAssignmentManager *pInstance = new ResourceAssignmentManager();

    pInstance->m_GCPhysMmioHint = GCPhysMmioHint;
    pInstance->m_pState->init(chipsetType, iommuType, cInterrupts);
    Assert(pInstance);
    return pInstance;
}


HRESULT ResourceAssignmentManager::assignFixedRomRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion)
{
    return m_pState->addAddrRange(pszName, kResourceRegionType_Rom, GCPhysStart, cbRegion);
}


HRESULT ResourceAssignmentManager::assignFixedRamRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion)
{
    return m_pState->addAddrRange(pszName, kResourceRegionType_Ram, GCPhysStart, cbRegion);
}


HRESULT ResourceAssignmentManager::assignFixedMmioRegion(const char *pszName, RTGCPHYS GCPhysStart, RTGCPHYS cbRegion)
{
    return m_pState->addAddrRange(pszName, kResourceRegionType_Mmio, GCPhysStart, cbRegion);
}


HRESULT ResourceAssignmentManager::assignMmioRegionAligned(const char *pszName, RTGCPHYS cbRegion, RTGCPHYS cbAlignment, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion,
                                                           bool fOnly32Bit)
{
    RTGCPHYS GCPhysRegionStart;
    RTGCPHYS cbRegionFinal;
    HRESULT hrc = m_pState->findNextFreeAddrRange(cbRegion, cbAlignment, fOnly32Bit ? _4G : RTGCPHYS_MAX,
                                                  m_GCPhysMmioHint, &GCPhysRegionStart, &cbRegionFinal);
    if (SUCCEEDED(hrc))
    {
        *pGCPhysStart = GCPhysRegionStart;
        *pcbRegion    = cbRegionFinal;
        return assignFixedMmioRegion(pszName, GCPhysRegionStart, cbRegion);
    }

    return hrc;
}


HRESULT ResourceAssignmentManager::assignMmioRegion(const char *pszName, RTGCPHYS cbRegion, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion)
{
    return assignMmioRegionAligned(pszName, cbRegion, _4K, pGCPhysStart, pcbRegion, false /*fOnly32Bit*/);
}


HRESULT ResourceAssignmentManager::assignMmio32Region(const char *pszName, RTGCPHYS cbRegion, PRTGCPHYS pGCPhysStart, PRTGCPHYS pcbRegion)
{
    return assignMmioRegionAligned(pszName, cbRegion, _4K, pGCPhysStart, pcbRegion, true /*fOnly32Bit*/);
}


HRESULT ResourceAssignmentManager::assignInterrupts(const char *pszName, uint32_t cInterrupts, uint32_t *piInterruptFirst)
{
    if (m_pState->miInterrupt + cInterrupts > m_pState->mcInterrupts)
    {
        AssertLogRelMsgFailed(("ResourceAssignmentManager: Interrupt count for %s exceeds number of available interrupts\n", pszName));
        return E_INVALIDARG;
    }

    *piInterruptFirst = m_pState->miInterrupt;
    m_pState->miInterrupt += cInterrupts;
    return S_OK;
}

HRESULT ResourceAssignmentManager::queryMmioRegion(PRTGCPHYS pGCPhysMmioStart, PRTGCPHYS pcbMmio)
{
    /*
     * This ASSUMES that there are adjacent MMIO regions above 4GiB which can be combined into one single region
     * (adjacent and no ROM/RAM regions inbetween).
     */
    *pGCPhysMmioStart = 0;
    *pcbMmio          = 0;

    /* Look for the start. */
    for (uint32_t i = 0; i < m_pState->m_cRegions; i++)
    {
        PCRESOURCEREGION pRegion = &m_pState->m_paRegions[i];

        if (   pRegion->GCPhysStart >= _4G
            && pRegion->enmType == kResourceRegionType_Mmio)
        {
            RTGCPHYS cbMmio = pRegion->GCPhysEnd - pRegion->GCPhysStart + 1;

            *pGCPhysMmioStart = pRegion->GCPhysStart;

            /* Add up any remaining MMIO regions adjacent to this one. */
            for (uint32_t j = i; j < m_pState->m_cRegions; j++)
            {
                pRegion = &m_pState->m_paRegions[i];
                if (   pRegion->enmType == kResourceRegionType_Mmio
                    && pRegion->GCPhysStart == *pGCPhysMmioStart + cbMmio)
                    cbMmio += pRegion->GCPhysEnd - pRegion->GCPhysStart + 1;
                else
                    break;
            }

            *pcbMmio = cbMmio;
            return S_OK;
        }
    }

    return S_OK;
}


HRESULT ResourceAssignmentManager::queryMmio32Region(PRTGCPHYS pGCPhysMmioStart, PRTGCPHYS pcbMmio)
{
    RT_NOREF(pGCPhysMmioStart, pcbMmio);
    return S_OK;
}


HRESULT ResourceAssignmentManager::assignSingleInterrupt(const char *pszName, uint32_t *piInterrupt)
{
    return assignInterrupts(pszName, 1, piInterrupt);
}


void ResourceAssignmentManager::dumpMemoryRegionsToReleaseLog(void)
{
    m_pState->dumpToReleaseLog();
}
