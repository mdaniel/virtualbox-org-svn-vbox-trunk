/* $Id$ */
/** @file
 * PGM page table walking testcase - ARMv8 variant.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VMInternal.h"
#include <VBox/vmm/cpum.h>
#include "../include/CPUMInternal-armv8.h"
#include "../include/PGMInternal.h"

#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/avl.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/json.h>
#include <iprt/message.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/test.h>
#include <iprt/zero.h>

#include "tstPGMAllGst-armv8-tests.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Chunk of physical memory containing data.
 */
typedef struct TSTMEMCHUNK
{
    /** AVL tree code. */
    /** @todo Too lazy to introduce support for a ranged RT_GCPHYS based AVL tree right now, so just use uint64_t. */
    AVLRU64NODECORE Core;
    /** The memory - variable in size. */
    uint8_t         abMem[1];
} TSTMEMCHUNK;
/** Pointer to a physical memory chunk. */
typedef TSTMEMCHUNK *PTSTMEMCHUNK;
/** Pointer to a const physical memory chunk. */
typedef const TSTMEMCHUNK *PCTSTMEMCHUNK;


/**
 * The current testcase data.
 */
typedef struct TSTPGMARMV8MMU
{
    /** The address space layout. */
    AVLRU64TREE     TreeMem;
    /** The fake VM structure. */
    PVM             pVM;
    /** TTBR0 value. */
    uint64_t        u64RegTtbr0;
    /** TTBR1 value. */
    uint64_t        u64RegTtbr1;
    /** The current exception level. */
    uint8_t         bEl;
} TSTPGMARMV8MMU;
typedef TSTPGMARMV8MMU *PTSTPGMARMV8MMU;
typedef const TSTPGMARMV8MMU *PCTSTPGMARMV8MMU;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;
/** The currently executing testcase config. */
static TSTPGMARMV8MMU g_MmuCfg;


static int pgmPhysGCPhys2CCPtr(RTGCPHYS GCPhys, void **ppv)
{
    PCTSTMEMCHUNK pChunk = (PCTSTMEMCHUNK)RTAvlrU64RangeGet(&g_MmuCfg.TreeMem, GCPhys);
    if (!pChunk)
    {
        *ppv = (void *)&g_abRTZero64K[0]; /* This ASSUMES that the page table walking code will never access beyond the end of this page. */
        return VINF_SUCCESS;
    }

    uint64_t const off = GCPhys - pChunk->Core.Key;
    *ppv = (void *)&pChunk->abMem[off];
    return VINF_SUCCESS;
}


int pgmPhysGCPhys2CCPtrLockless(PVMCPUCC pVCpu, RTGCPHYS GCPhys, void **ppv)
{
    RT_NOREF(pVCpu, GCPhys, ppv);
    return pgmPhysGCPhys2CCPtr(GCPhys, ppv);
}


int pgmPhysGCPhys2R3Ptr(PVMCC pVM, RTGCPHYS GCPhys, PRTR3PTR pR3Ptr)
{
    RT_NOREF(pVM, GCPhys, pR3Ptr);
    return pgmPhysGCPhys2CCPtr(GCPhys, (void **)pR3Ptr);
}


VMM_INT_DECL(uint8_t) CPUMGetGuestEL(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
    return g_MmuCfg.bEl;
}


VMM_INT_DECL(RTGCPHYS) CPUMGetEffectiveTtbr(PVMCPUCC pVCpu, RTGCPTR GCPtr)
{
    RT_NOREF(pVCpu);
    return   (GCPtr & RT_BIT_64(55))
           ? ARMV8_TTBR_EL1_AARCH64_BADDR_GET(g_MmuCfg.u64RegTtbr1)
           : ARMV8_TTBR_EL1_AARCH64_BADDR_GET(g_MmuCfg.u64RegTtbr0);
}


/** Include and instantiate the page table walking code. */
#include "../VMMAll/PGMAllGst-armv8.cpp.h"


/**
 * Creates a mockup VM structure for testing SSM.
 *
 * @returns 0 on success, 1 on failure.
 * @param   pMmuCfg         The MMU config to initialize.
 */
static int tstMmuCfgInit(PTSTPGMARMV8MMU pMmuCfg)
{
    /*
     * Allocate and init the UVM structure.
     */
    PUVM pUVM = (PUVM)RTMemPageAllocZ(sizeof(*pUVM));
    AssertReturn(pUVM, 1);
    pUVM->u32Magic = UVM_MAGIC;
    pUVM->vm.s.idxTLS = RTTlsAlloc();
    int rc = RTTlsSet(pUVM->vm.s.idxTLS, &pUVM->aCpus[0]);
    if (RT_SUCCESS(rc))
    {
        pUVM->aCpus[0].pUVM = pUVM;
        pUVM->aCpus[0].vm.s.NativeThreadEMT = RTThreadNativeSelf();

        /*
         * Allocate and init the VM structure.
         */
        PVM pVM = (PVM)RTMemPageAllocZ(sizeof(VM) + sizeof(VMCPU));
        rc = pVM ? VINF_SUCCESS : VERR_NO_PAGE_MEMORY;
        if (RT_SUCCESS(rc))
        {
            pVM->enmVMState = VMSTATE_CREATED;
            pVM->pVMR3 = pVM;
            pVM->pUVM = pUVM;
            pVM->cCpus = 1;

            PVMCPU pVCpu = (PVMCPU)(pVM + 1);
            pVCpu->pVMR3 = pVM;
            pVCpu->hNativeThread = RTThreadNativeSelf();
            pVM->apCpusR3[0] = pVCpu;

            pUVM->pVM = pVM;
            pMmuCfg->pVM = pVM;
            return VINF_SUCCESS;
        }

        RTTestIFailed("Fatal error: failed to allocated pages for the VM structure, rc=%Rrc\n", rc);
    }
    else
        RTTestIFailed("Fatal error: RTTlsSet failed, rc=%Rrc\n", rc);

    return rc;
}


static DECLCALLBACK(int) tstZeroChunk(PAVLRU64NODECORE pCore, void *pvParam)
{
    RT_NOREF(pvParam);
    PTSTMEMCHUNK pChunk = (PTSTMEMCHUNK)pCore;
    memset(&pChunk->abMem, 0, _64K);
    return VINF_SUCCESS;
}


static void tstMmuCfgReset(PTSTPGMARMV8MMU pMmuCfg)
{
    RTAvlrU64DoWithAll(&pMmuCfg->TreeMem, true /*fFromLeft*/, tstZeroChunk, NULL);
}


static DECLCALLBACK(int) tstDestroyChunk(PAVLRU64NODECORE pCore, void *pvParam)
{
    RT_NOREF(pvParam);
    RTMemPageFree(pCore, _64K);
    return VINF_SUCCESS;
}


/**
 * Destroy the VM structure.
 *
 * @param   pVM     Pointer to the VM.
 */
static void tstMmuCfgDestroy(PTSTPGMARMV8MMU pMmuCfg)
{
    RTMemPageFree(pMmuCfg->pVM->pUVM, sizeof(*pMmuCfg->pVM->pUVM));
    RTMemPageFree(pMmuCfg->pVM, sizeof(VM) + sizeof(VMCPU));
    RTAvlrU64Destroy(&pMmuCfg->TreeMem, tstDestroyChunk, NULL);
}


static int tstTestcaseMmuMemoryWrite(RTTEST hTest, PTSTPGMARMV8MMU pMmuCfg, uint64_t GCPhysAddr, const void *pvData, size_t cbData)
{
    size_t cbLeft = cbData;
    const uint8_t *pbData = (const uint8_t *)pvData;
    while (cbLeft)
    {
        PTSTMEMCHUNK pChunk = (PTSTMEMCHUNK)RTAvlrU64RangeGet(&pMmuCfg->TreeMem, GCPhysAddr);
        if (!pChunk)
        {
            /* Allocate a new chunk (64KiB chunks). */
            pChunk = (PTSTMEMCHUNK)RTMemPageAllocZ(_64K);
            if (!pChunk)
            {
                RTTestFailed(hTest, "Failed to allocate 64KiB of memory for memory chunk at %#RX64\n", GCPhysAddr);
                return VERR_NO_MEMORY;
            }

            pChunk->Core.Key = GCPhysAddr & ~((uint64_t)_64K - 1);
            pChunk->Core.KeyLast = pChunk->Core.Key + _64K - 1;
            bool fInsert = RTAvlrU64Insert(&pMmuCfg->TreeMem, &pChunk->Core);
            AssertRelease(fInsert);
        }

        uint64_t const off        = GCPhysAddr - pChunk->Core.Key;
        size_t   const cbThisCopy = RT_MIN(cbLeft, pChunk->Core.KeyLast - off + 1);
        memcpy(&pChunk->abMem[off], pbData, cbThisCopy);
        cbLeft     -= cbThisCopy;
        GCPhysAddr += cbThisCopy;
        pbData     += cbThisCopy;
    }
    return VINF_SUCCESS;
}


static int tstTestcaseMmuMemoryAdd(RTTEST hTest, PTSTPGMARMV8MMU pMmuCfg, uint64_t GCPhysAddr, RTJSONVAL hMemObj)
{
    int rc;
    RTJSONVALTYPE enmType = RTJsonValueGetType(hMemObj);
    switch (enmType)
    {
        case RTJSONVALTYPE_ARRAY:
        {
            RTJSONIT hIt = NIL_RTJSONIT;
            rc = RTJsonIteratorBeginArray(hMemObj, &hIt);
            if (RT_SUCCESS(rc))
            {
                for (;;)
                {
                    RTJSONVAL hData = NIL_RTJSONVAL;
                    rc = RTJsonIteratorQueryValue(hIt, &hData, NULL /*ppszName*/);
                    if (RT_SUCCESS(rc))
                    {
                        if (RTJsonValueGetType(hData) == RTJSONVALTYPE_INTEGER)
                        {
                            int64_t i64Data = 0;
                            rc = RTJsonValueQueryInteger(hData, &i64Data);
                            if (RT_SUCCESS(rc))
                            {
                                if (i64Data >= 0 && i64Data <= 255)
                                {
                                    uint8_t bVal = (uint8_t)i64Data;
                                    rc = tstTestcaseMmuMemoryWrite(hTest, pMmuCfg, GCPhysAddr, &bVal, sizeof(bVal));
                                }
                                else
                                {
                                    RTTestFailed(hTest, "Data %#RX64 for address %#RX64 is not a valid byte value", i64Data, GCPhysAddr);
                                    break;
                                }
                            }
                            else
                            {
                                RTTestFailed(hTest, "Failed to query byte value for address %#RX64", GCPhysAddr);
                                break;
                            }
                        }
                        else
                        {
                            RTTestFailed(hTest, "Data for address %#RX64 contains an invalid value", GCPhysAddr);
                            break;
                        }

                        RTJsonValueRelease(hData);
                    }
                    else
                        RTTestFailed(hTest, "Failed to retrieve byte value with %Rrc", rc);

                    rc = RTJsonIteratorNext(hIt);
                    if (RT_FAILURE(rc))
                        break;

                    GCPhysAddr++;
                }
                if (rc == VERR_JSON_ITERATOR_END)
                    rc = VINF_SUCCESS;
                RTJsonIteratorFree(hIt);
            }
            else  /* An empty array is also an error */
                RTTestFailed(hTest, "Failed to traverse JSON array with %Rrc", rc);
            break;
        }
        case RTJSONVALTYPE_INTEGER:
        {
            uint64_t u64Val = 0;
            rc = RTJsonValueQueryInteger(hMemObj, (int64_t *)&u64Val);
            if (RT_SUCCESS(rc))
                rc = tstTestcaseMmuMemoryWrite(hTest, pMmuCfg, GCPhysAddr, &u64Val, sizeof(u64Val));
            else
                RTTestFailed(hTest, "Querying data for address %#RX64 failed with %Rrc\n", GCPhysAddr, u64Val);
            break;
        }
        default:
            RTTestFailed(hTest, "Memory object has an invalid type %d\n", enmType);
            rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}


static int tstTestcaseAddressSpacePrepare(RTTEST hTest, RTJSONVAL hTestcase)
{
    /* Prepare the memory space. */
    RTJSONVAL hVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hTestcase, "AddressSpace", &hVal);
    if (RT_SUCCESS(rc))
    {
        RTJSONIT hIt = NIL_RTJSONIT;
        rc = RTJsonIteratorBeginObject(hVal, &hIt);
        if (RT_SUCCESS(rc))
        {
            for (;;)
            {
                RTJSONVAL hMemObj = NIL_RTJSONVAL;
                const char *pszAddress = NULL;
                rc = RTJsonIteratorQueryValue(hIt, &hMemObj, &pszAddress);
                if (RT_SUCCESS(rc))
                {
                    uint64_t GCPhysAddr = 0;
                    rc = RTStrToUInt64Full(pszAddress, 0, &GCPhysAddr);
                    if (rc == VINF_SUCCESS)
                        rc = tstTestcaseMmuMemoryAdd(hTest, &g_MmuCfg, GCPhysAddr, hMemObj);
                    else
                    {
                        RTTestFailed(hTest, "Address '%s' is not a valid 64-bit physical address", pszAddress);
                        break;
                    }

                    RTJsonValueRelease(hMemObj);
                }
                else
                    RTTestFailed(hTest, "Failed to retrieve memory range with %Rrc", rc);

                rc = RTJsonIteratorNext(hIt);
                if (RT_FAILURE(rc))
                    break;
            }
            if (rc == VERR_JSON_ITERATOR_END)
                rc = VINF_SUCCESS;
            RTJsonIteratorFree(hIt);
        }
        else if (rc == VERR_JSON_IS_EMPTY) /* Empty address space is valid. */
            rc = VINF_SUCCESS;
        else
            RTTestFailed(hTest, "Failed to traverse JSON object with %Rrc", rc);

        RTJsonValueRelease(hVal);
    }
    else
        RTTestFailed(hTest, "Failed to query \"AddressSpace\" containing the address space layout %Rrc", rc);

    return rc;
}


static int tstTestcaseMmuConfigPrepare(RTTEST hTest, PTSTPGMARMV8MMU pMmuCfg, RTJSONVAL hTestcase)
{
    PVMCPUCC pVCpu = pMmuCfg->pVM->apCpusR3[0];

    /* Set MMU config (SCTLR, TCR, TTBR, etc.). */
    int64_t i64Tmp = 0;
    int rc = RTJsonValueQueryIntegerByName(hTestcase, "SCTLR_EL1", &i64Tmp);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"SCTLR_EL1\" with %Rrc", rc);
        return rc;
    }
    uint64_t const u64RegSctlrEl1 = (uint64_t)i64Tmp;

    rc = RTJsonValueQueryIntegerByName(hTestcase, "TCR_EL1", &i64Tmp);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"TCR_EL1\" with %Rrc", rc);
        return rc;
    }
    uint64_t const u64RegTcrEl1 = (uint64_t)i64Tmp;

    rc = RTJsonValueQueryIntegerByName(hTestcase, "TTBR0_EL1", &i64Tmp);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"TTBR0_EL1\" with %Rrc", rc);
        return rc;
    }
    pVCpu->cpum.s.Guest.Ttbr0.u64 = (uint64_t)i64Tmp;

    rc = RTJsonValueQueryIntegerByName(hTestcase, "TTBR1_EL1", &i64Tmp);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"TTBR1_EL1\" with %Rrc", rc);
        return rc;
    }
    pVCpu->cpum.s.Guest.Ttbr1.u64 = (uint64_t)i64Tmp;


    uintptr_t const idxNewGstTtbr0 = pgmR3DeduceTypeFromTcr<ARMV8_TCR_EL1_AARCH64_T0SZ_SHIFT, ARMV8_TCR_EL1_AARCH64_TG0_SHIFT,
                                                            ARMV8_TCR_EL1_AARCH64_TBI0_BIT, ARMV8_TCR_EL1_AARCH64_EPD0_BIT, false>
                                                            (u64RegSctlrEl1, u64RegTcrEl1, &pVCpu->pgm.s.afLookupMaskTtbr0[1]);
    uintptr_t const idxNewGstTtbr1 = pgmR3DeduceTypeFromTcr<ARMV8_TCR_EL1_AARCH64_T1SZ_SHIFT, ARMV8_TCR_EL1_AARCH64_TG1_SHIFT,
                                                            ARMV8_TCR_EL1_AARCH64_TBI1_BIT, ARMV8_TCR_EL1_AARCH64_EPD1_BIT, true>
                                                            (u64RegSctlrEl1, u64RegTcrEl1, &pVCpu->pgm.s.afLookupMaskTtbr1[1]);
    Assert(idxNewGstTtbr0 != 0 && idxNewGstTtbr1 != 0);

    /*
     * Change the paging mode data indexes.
     */
    AssertReturn(idxNewGstTtbr0 < RT_ELEMENTS(g_aPgmGuestModeData), VERR_PGM_MODE_IPE);
    AssertReturn(g_aPgmGuestModeData[idxNewGstTtbr0].uType == idxNewGstTtbr0, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr0].pfnGetPage, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr0].pfnModifyPage, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr0].pfnExit, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr0].pfnEnter, VERR_PGM_MODE_IPE);

    AssertReturn(idxNewGstTtbr1 < RT_ELEMENTS(g_aPgmGuestModeData), VERR_PGM_MODE_IPE);
    AssertReturn(g_aPgmGuestModeData[idxNewGstTtbr1].uType == idxNewGstTtbr1, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr1].pfnGetPage, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr1].pfnModifyPage, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr1].pfnExit, VERR_PGM_MODE_IPE);
    AssertPtrReturn(g_aPgmGuestModeData[idxNewGstTtbr1].pfnEnter, VERR_PGM_MODE_IPE);

        rc  = g_aPgmGuestModeData[idxNewGstTtbr0].pfnEnter(pVCpu);
    int rc2 = g_aPgmGuestModeData[idxNewGstTtbr1].pfnEnter(pVCpu);

    /* status codes. */
    AssertRC(rc);
    AssertRC(rc2);
    if (RT_SUCCESS(rc))
    {
        rc = rc2;
        if (RT_SUCCESS(rc)) /* no informational status codes. */
            rc = VINF_SUCCESS;
    }

    pVCpu->pgm.s.aidxGuestModeDataTtbr0[1] = idxNewGstTtbr0;
    pVCpu->pgm.s.aidxGuestModeDataTtbr1[1] = idxNewGstTtbr1;

    /* Also set the value for EL0, saves us an if condition in the hot paths later on. */
    pVCpu->pgm.s.aidxGuestModeDataTtbr0[0] = idxNewGstTtbr0;
    pVCpu->pgm.s.aidxGuestModeDataTtbr1[0] = idxNewGstTtbr1;

    pVCpu->pgm.s.afLookupMaskTtbr0[0] = pVCpu->pgm.s.afLookupMaskTtbr0[1];
    pVCpu->pgm.s.afLookupMaskTtbr1[0] = pVCpu->pgm.s.afLookupMaskTtbr1[1];

    pVCpu->pgm.s.aenmGuestMode[1] = (u64RegSctlrEl1 & ARMV8_SCTLR_EL1_M) ? PGMMODE_VMSA_V8_64 : PGMMODE_NONE;
    return rc;
}


DECLINLINE(int) tstResultQueryBoolDef(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, bool *pf, bool fDef)
{
    int rc = RTJsonValueQueryBooleanByName(hMemResult, pszName, pf);
    if (rc == VERR_NOT_FOUND)
    {
        *pf = fDef;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);

    return rc;
}


DECLINLINE(int) tstResultQueryGCPhysDef(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, RTGCPHYS *pGCPhys, RTGCPHYS GCPhysDef)
{
    int64_t i64 = 0;
    int rc = RTJsonValueQueryIntegerByName(hMemResult, pszName, &i64);
    if (rc == VERR_NOT_FOUND)
    {
        *pGCPhys = GCPhysDef;
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);
    else
        *pGCPhys = (RTGCPHYS)i64;

    return rc;
}


DECLINLINE(int) tstResultQueryGCPhys(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, RTGCPHYS *pGCPhys)
{
    int64_t i64 = 0;
    int rc = RTJsonValueQueryIntegerByName(hMemResult, pszName, &i64);
    if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);
    else
        *pGCPhys = (RTGCPHYS)i64;

    return rc;
}


DECLINLINE(int) tstResultQueryU8(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, uint8_t *pu8)
{
    int64_t i64 = 0;
    int rc = RTJsonValueQueryIntegerByName(hMemResult, pszName, &i64);
    if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);
    else if (i64 < 0 || i64 > UINT8_MAX)
        RTTestFailed(hTest, "Value %#RI64 for '%s' is out of bounds", i64, pszName);
    else
        *pu8 = (uint8_t)i64;

    return rc;
}


DECLINLINE(int) tstResultQueryU32(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, uint32_t *pu32)
{
    int64_t i64 = 0;
    int rc = RTJsonValueQueryIntegerByName(hMemResult, pszName, &i64);
    if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);
    else if (i64 < 0 || i64 > UINT32_MAX)
        RTTestFailed(hTest, "Value %#RI64 for '%s' is out of bounds", i64, pszName);
    else
        *pu32 = (uint32_t)i64;

    return rc;
}


DECLINLINE(int) tstResultQueryU64(RTTEST hTest, RTJSONVAL hMemResult, const char *pszName, uint64_t *pu64)
{
    int64_t i64 = 0;
    int rc = RTJsonValueQueryIntegerByName(hMemResult, pszName, &i64);
    if (RT_FAILURE(rc))
        RTTestFailed(hTest, "Querying '%s' failed with %Rrc", pszName, rc);
    else
        *pu64 = (uint64_t)i64;

    return rc;
}


static int tstResultInit(RTTEST hTest, RTJSONVAL hMemResult, PPGMPTWALK pWalkResult)
{
    int rc = tstResultQueryBoolDef(hTest, hMemResult, "Succeeded", &pWalkResult->fSucceeded, true);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "IsSlat", &pWalkResult->fIsSlat, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "IsLinearAddrValid", &pWalkResult->fIsLinearAddrValid, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "NotPresent", &pWalkResult->fNotPresent, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "BadPhysAddr", &pWalkResult->fBadPhysAddr, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "RsvdError", &pWalkResult->fRsvdError, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "BigPage", &pWalkResult->fBigPage, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryBoolDef(hTest, hMemResult, "GigantPage", &pWalkResult->fGigantPage, false);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryGCPhys(hTest, hMemResult, "GCPhys", &pWalkResult->GCPhys);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryGCPhysDef(hTest, hMemResult, "GCPhysNested", &pWalkResult->GCPhysNested, 0);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryU8(hTest, hMemResult, "Level", &pWalkResult->uLevel);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryU32(hTest, hMemResult, "fFailed", &pWalkResult->fFailed);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryU64(hTest, hMemResult, "Effective", &pWalkResult->fEffective);

    return rc;
}


static void tstExecute(RTTEST hTest, PVM pVM, RTGCPTR GCPtr, RTJSONVAL hMemResult)
{
    PVMCPUCC pVCpu = pVM->apCpusR3[0];

    /** @todo Incorporate EL (for nested virt and EL3 later on). */
    uintptr_t idx =   (GCPtr & RT_BIT_64(55))
                    ? pVCpu->pgm.s.aidxGuestModeDataTtbr1[1]
                    : pVCpu->pgm.s.aidxGuestModeDataTtbr0[1];

    PGMPTWALK Walk; RT_ZERO(Walk);
    AssertReleaseReturnVoid(idx < RT_ELEMENTS(g_aPgmGuestModeData));
    AssertReleaseReturnVoid(g_aPgmGuestModeData[idx].pfnGetPage);
    int rc = g_aPgmGuestModeData[idx].pfnGetPage(pVCpu, GCPtr, &Walk);
    if (RT_SUCCESS(rc))
    {
        PGMPTWALK WalkResult; RT_ZERO(WalkResult);
        WalkResult.GCPtr = GCPtr;

        rc = tstResultInit(hTest, hMemResult, &WalkResult);
        if (RT_SUCCESS(rc))
        {
            if (memcmp(&Walk, &WalkResult, sizeof(Walk)))
            {
                if (Walk.GCPtr != WalkResult.GCPtr)
                    RTTestFailed(hTest, "Result GCPtr=%RGv != Expected GCPtr=%RGv", Walk.GCPtr, WalkResult.GCPtr);
                if (Walk.GCPhysNested != WalkResult.GCPhysNested)
                    RTTestFailed(hTest, "Result GCPhysNested=%RGp != Expected GCPhysNested=%RGp", Walk.GCPhysNested, WalkResult.GCPhysNested);
                if (Walk.GCPhys != WalkResult.GCPhys)
                    RTTestFailed(hTest, "Result GCPhys=%RGp != Expected GCPhys=%RGp", Walk.GCPhys, WalkResult.GCPhys);
                if (Walk.fSucceeded != WalkResult.fSucceeded)
                    RTTestFailed(hTest, "Result fSucceeded=%RTbool != Expected fSucceeded=%RTbool", Walk.fSucceeded, WalkResult.fSucceeded);
                if (Walk.fIsSlat != WalkResult.fIsSlat)
                    RTTestFailed(hTest, "Result fIsSlat=%RTbool != Expected fIsSlat=%RTbool", Walk.fIsSlat, WalkResult.fIsSlat);
                if (Walk.fIsLinearAddrValid != WalkResult.fIsLinearAddrValid)
                    RTTestFailed(hTest, "Result fIsLinearAddrValid=%RTbool != Expected fIsLinearAddrValid=%RTbool", Walk.fIsLinearAddrValid, WalkResult.fIsLinearAddrValid);
                if (Walk.uLevel != WalkResult.uLevel)
                    RTTestFailed(hTest, "Result uLevel=%RU8 != Expected uLevel=%RU8", Walk.uLevel, WalkResult.uLevel);
                if (Walk.fNotPresent != WalkResult.fNotPresent)
                    RTTestFailed(hTest, "Result fNotPresent=%RTbool != Expected fNotPresent=%RTbool", Walk.fNotPresent, WalkResult.fNotPresent);
                if (Walk.fBadPhysAddr != WalkResult.fBadPhysAddr)
                    RTTestFailed(hTest, "Result fBadPhysAddr=%RTbool != Expected fBadPhysAddr=%RTbool", Walk.fBadPhysAddr, WalkResult.fBadPhysAddr);
                if (Walk.fRsvdError != WalkResult.fRsvdError)
                    RTTestFailed(hTest, "Result fRsvdError=%RTbool != Expected fRsvdError=%RTbool", Walk.fRsvdError, WalkResult.fRsvdError);
                if (Walk.fBigPage != WalkResult.fBigPage)
                    RTTestFailed(hTest, "Result fBigPage=%RTbool != Expected fBigPage=%RTbool", Walk.fBigPage, WalkResult.fBigPage);
                if (Walk.fGigantPage != WalkResult.fGigantPage)
                    RTTestFailed(hTest, "Result fGigantPage=%RTbool != Expected fGigantPage=%RTbool", Walk.fGigantPage, WalkResult.fGigantPage);
                if (Walk.fFailed != WalkResult.fFailed)
                    RTTestFailed(hTest, "Result fFailed=%#RX32 != Expected fFailed=%#RX32", Walk.fFailed, WalkResult.fFailed);
                if (Walk.fEffective != WalkResult.fEffective)
                    RTTestFailed(hTest, "Result fEffective=%#RX64 != Expected fEffective=%#RX64", Walk.fEffective, WalkResult.fEffective);
            }
        }
    }
    else
        RTTestFailed(hTest, "Resolving virtual address %#RX64 to physical address failed with %Rrc", GCPtr, rc);
}


static int tstTestcaseMmuRun(RTTEST hTest, RTJSONVAL hTestcase)
{
    RTJSONVAL hVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hTestcase, "Tests", &hVal);
    if (RT_SUCCESS(rc))
    {
        RTJSONIT hIt = NIL_RTJSONIT;
        rc = RTJsonIteratorBeginObject(hVal, &hIt);
        if (RT_SUCCESS(rc))
        {
            for (;;)
            {
                RTJSONVAL hMemObj = NIL_RTJSONVAL;
                const char *pszAddress = NULL;
                rc = RTJsonIteratorQueryValue(hIt, &hMemObj, &pszAddress);
                if (RT_SUCCESS(rc))
                {
                    uint64_t GCPtr = 0;
                    rc = RTStrToUInt64Full(pszAddress, 0, &GCPtr);
                    if (rc == VINF_SUCCESS)
                        tstExecute(hTest, g_MmuCfg.pVM, GCPtr, hMemObj);
                    else
                    {
                        RTTestFailed(hTest, "Address '%s' is not a valid 64-bit physical address", pszAddress);
                        break;
                    }

                    RTJsonValueRelease(hMemObj);
                }
                else
                    RTTestFailed(hTest, "Failed to retrieve memory range with %Rrc", rc);

                rc = RTJsonIteratorNext(hIt);
                if (RT_FAILURE(rc))
                    break;
            }
            if (rc == VERR_JSON_ITERATOR_END)
                rc = VINF_SUCCESS;
            RTJsonIteratorFree(hIt);
        }
        else
            RTTestFailed(hTest, "Failed to traverse JSON array with %Rrc", rc);


        RTJsonValueRelease(hVal);
    }
    else
        RTTestFailed(hTest, "Failed to query \"Tests\" %Rrc", rc);

    return rc;
}


static void tstExecuteTestcase(RTTEST hTest, RTJSONVAL hTestcase)
{
    RTJSONVAL hVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hTestcase, "Name", &hVal);
    if (RT_SUCCESS(rc))
    {
        const char *pszTestcaseName = RTJsonValueGetString(hVal);
        if (pszTestcaseName)
        {
            RTTestSub(hTest, pszTestcaseName);

            /* Reset the config for each testcase. */
            tstMmuCfgReset(&g_MmuCfg);

            rc = tstTestcaseAddressSpacePrepare(hTest, hTestcase);
            if (RT_SUCCESS(rc))
                rc = tstTestcaseMmuConfigPrepare(hTest, &g_MmuCfg, hTestcase);
            if (RT_SUCCESS(rc))
                rc = tstTestcaseMmuRun(hTest, hTestcase);
        }
        else
            RTTestFailed(hTest, "The testcase name is not a string");
        RTJsonValueRelease(hVal);
    }
    else
        RTTestFailed(hTest, "Failed to query the testcase name with %Rrc", rc);
}


static void tstLoadAndRun(RTTEST hTest, RTJSONVAL hRoot)
{
    int rc = tstMmuCfgInit(&g_MmuCfg);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to initialize MMU config %Rrc", rc);
        return;
    }

    RTJSONVALTYPE enmType = RTJsonValueGetType(hRoot);
    if (enmType == RTJSONVALTYPE_ARRAY)
    {
        /* Array of testcases. */
        RTJSONIT hIt = NIL_RTJSONIT;
        rc = RTJsonIteratorBeginArray(hRoot, &hIt);
        if (RT_SUCCESS(rc))
        {
            for (;;)
            {
                RTJSONVAL hTestcase = NIL_RTJSONVAL;
                rc = RTJsonIteratorQueryValue(hIt, &hTestcase, NULL /*ppszName*/);
                if (RT_SUCCESS(rc))
                {
                    tstExecuteTestcase(hTest, hTestcase);
                    RTJsonValueRelease(hTestcase);
                }
                else
                    RTTestFailed(hTest, "Failed to retrieve testcase with %Rrc", rc);

                rc = RTJsonIteratorNext(hIt);
                if (RT_FAILURE(rc))
                    break;
            }
            if (rc == VERR_JSON_ITERATOR_END)
                rc = VINF_SUCCESS;
            RTJsonIteratorFree(hIt);
        }
        else  /* An empty array is also an error */
            RTTestFailed(hTest, "Failed to traverse JSON array with %Rrc", rc);
    }
    else if (enmType == RTJSONVALTYPE_OBJECT)
    {
        /* Single testcase. */
        tstExecuteTestcase(hTest, hRoot);
    }
    else
        RTTestFailed(hTest, "JSON root is not an array or object containing a testcase");
    RTJsonValueRelease(hRoot);
    tstMmuCfgDestroy(&g_MmuCfg);
}


static void tstLoadFromFile(RTTEST hTest, const char *pszFilename)
{
    /* Load the configuration from the JSON config file. */
    RTERRINFOSTATIC ErrInfo;
    RTJSONVAL hRoot = NIL_RTJSONVAL;
    int rc = RTJsonParseFromFile(&hRoot, RTJSON_PARSE_F_JSON5, pszFilename, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
        tstLoadAndRun(hTest, hRoot);
    else
    {
        if (RTErrInfoIsSet(&ErrInfo.Core))
            RTTestFailed(hTest, "RTJsonParseFromFile() for \"%s\" failed with %Rrc\n%s",
                         pszFilename, rc, ErrInfo.Core.pszMsg);
        else
            RTTestFailed(hTest, "RTJsonParseFromFile() for \"%s\" failed with %Rrc",
                         pszFilename, rc);
    }
}


static void tstBasic(RTTEST hTest)
{
    RTERRINFOSTATIC ErrInfo;
    RTJSONVAL hRoot = NIL_RTJSONVAL;
    int rc = RTJsonParseFromBuf(&hRoot, RTJSON_PARSE_F_JSON5,
                                g_abtstPGMAllGst_armv8_1, g_cbtstPGMAllGst_armv8_1,
                                RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
        tstLoadAndRun(hTest, hRoot);
    else
    {
        if (RTErrInfoIsSet(&ErrInfo.Core))
            RTTestFailed(hTest, "RTJsonParseFromBuf() failed with %Rrc\n%s", rc, ErrInfo.Core.pszMsg);
        else
            RTTestFailed(hTest, "RTJsonParseFromBuf() failed with %Rrc", rc);
    }
}


int main(int argc, char **argv)
{
    /*
     * We run the VMM in driverless mode to avoid needing to hardened the testcase
     */
    RTEXITCODE rcExit;
    int rc = RTR3InitExe(argc, &argv, SUPR3INIT_F_DRIVERLESS << RTR3INIT_FLAGS_SUPLIB_SHIFT);
    if (RT_SUCCESS(rc))
    {
        rc = RTTestCreate("tstPGMAllGst-armv8", &g_hTest);
        if (RT_SUCCESS(rc))
        {
            RTTestBanner(g_hTest);
            if (argc == 2)
                tstLoadFromFile(g_hTest, argv[1]);
            else
                tstBasic(g_hTest);
            rcExit = RTTestSummaryAndDestroy(g_hTest);
        }
        else
            rcExit = RTMsgErrorExitFailure("RTTestCreate failed: %Rrc", rc);
    }
    else
        rcExit = RTMsgInitFailure(rc);
    return rcExit;
}

