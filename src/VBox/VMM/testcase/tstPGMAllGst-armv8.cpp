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
 * Named value.
 */
typedef struct TSTCFGNAMEDVALUE
{
    /** The name of the value. */
    const char *pszName;
    /** The integer value. */
    uint64_t   u64Val;
} TSTCFGNAMEDVALUE;
typedef TSTCFGNAMEDVALUE *PTSTCFGNAMEDVALUE;
typedef const TSTCFGNAMEDVALUE *PCTSTCFGNAMEDVALUE;


/**
 * A config bitfield.
 */
typedef struct TSTCFGBITFIELD
{
    /** The bitfield name. */
    const char          *pszName;
    /** The bitfield offset. */
    uint8_t             offBitfield;
    /** Number of bits for the bitfield. */
    uint8_t             cBits;
    /** Optional array of named values. */
    PCTSTCFGNAMEDVALUE  paNamedValues;
} TSTCFGBITFIELD;
typedef TSTCFGBITFIELD *PTSTCFGBITFIELD;
typedef const TSTCFGBITFIELD *PCTSTCFGBITFIELD;


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
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;

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


static int tstMmuCfgReadS64(RTTEST hTest, RTJSONVAL hObj, const char *pszName, PCTSTCFGNAMEDVALUE paNamedValues, int64_t *pi64Result)
{
    RTJSONVAL hValue = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hObj, pszName, &hValue);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"%s\" with %Rrc", pszName, rc);
        return rc;
    }

    RTJSONVALTYPE enmType = RTJsonValueGetType(hValue);
    switch (enmType)
    {
        case RTJSONVALTYPE_INTEGER:
            rc = RTJsonValueQueryInteger(hValue, pi64Result);
            break;
        case RTJSONVALTYPE_STRING:
        {
            if (paNamedValues)
            {
                const char *pszNamedValue = RTJsonValueGetString(hValue);
                PCTSTCFGNAMEDVALUE pNamedValue = &paNamedValues[0];
                while (pNamedValue->pszName)
                {
                    if (!RTStrICmp(pszNamedValue, pNamedValue->pszName))
                    {
                        *pi64Result = (int64_t)pNamedValue->u64Val;
                        break;
                    }
                    pNamedValue++;
                }
                if (!pNamedValue->pszName)
                {
                    RTTestFailed(hTest, "\"%s\" ist not a known named value for '%s'", pszNamedValue, pszName);
                    rc = VERR_NOT_FOUND;
                }
            }
            else
            {
                RTTestFailed(hTest, "Integer \"%s\" doesn't support named values", pszName);
                rc = VERR_NOT_SUPPORTED;
            }
            break;
        }
        default:
            rc = VERR_NOT_SUPPORTED;
            RTTestFailed(hTest, "JSON value type %d is not supported\n", enmType);
            break;
    }

    RTJsonValueRelease(hValue);
    return rc;
}


static int tstMmuCfgReadRc(RTTEST hTest, RTJSONVAL hObj, const char *pszName, int32_t *pi32Result)
{
    static const TSTCFGNAMEDVALUE s_aRc[] =
    {
#define CREATE_RC(a_Rc) \
        {#a_Rc, (uint64_t)a_Rc}
        CREATE_RC(VINF_SUCCESS),
        CREATE_RC(VERR_RESERVED_PAGE_TABLE_BITS),
        CREATE_RC(VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS),
        CREATE_RC(VERR_PAGE_TABLE_NOT_PRESENT),
        CREATE_RC(VERR_ACCESS_DENIED),
        { NULL,   0 }
    };


    int64_t i64 = 0;
    int rc = tstMmuCfgReadS64(hTest, hObj, pszName, &s_aRc[0], &i64);
    if (RT_SUCCESS(rc))
    {
        if (i64 >= INT32_MIN && i64 <= INT32_MAX)
            *pi32Result = (int32_t)i64;
        else
            RTTestFailed(hTest, "%RI64 for '%s' is out of range for a 32-bit signed integer", i64, pszName);
    }

    return rc;
}


static int tstMmuCfgReadBitfieldU64(RTTEST hTest, RTJSONVAL hObj, PCTSTCFGBITFIELD paBitfields, uint64_t *pu64Result)
{
    uint64_t u64 = 0;

    uint32_t idx = 0;
    while (paBitfields[idx].pszName)
    {
        RTJSONVAL hValue = NIL_RTJSONVAL;
        int rc = RTJsonValueQueryByName(hObj, paBitfields[idx].pszName, &hValue);
        if (rc == VERR_NOT_FOUND)
        {
            idx++;
            continue;
        }
        if (RT_FAILURE(rc))
        {
            RTTestFailed(hTest, "Failed to query \"%s\" with %Rrc", paBitfields[idx].pszName, rc);
            return rc;
        }

        RTJSONVALTYPE enmType = RTJsonValueGetType(hValue);
        switch (enmType)
        {
            case RTJSONVALTYPE_TRUE:
            case RTJSONVALTYPE_FALSE:
            {
                if (paBitfields[idx].cBits == 1)
                    u64 |= (enmType == RTJSONVALTYPE_TRUE ? 1 : 0) << paBitfields[idx].offBitfield;
                else
                    RTTestFailed(hTest, "Bitfield '%s' is more than 1 bit wide", paBitfields[idx].pszName);

                break;
            }
            case RTJSONVALTYPE_INTEGER:
            {
                int64_t i64Tmp = 0;
                rc = RTJsonValueQueryInteger(hValue, &i64Tmp);
                if (RT_FAILURE(rc))
                {
                    RTTestFailed(hTest, "Failed to query \"%s\" as an integer with %Rrc", paBitfields[idx].pszName, rc);
                    break;
                }
                else if (   i64Tmp < 0
                         || (   paBitfields[idx].cBits < 64
                             && (uint64_t)i64Tmp > (RT_BIT_64(paBitfields[idx].cBits) - 1)))
                {
                    RTTestFailed(hTest, "Value of \"%s\" is out of bounds, got %#RX64, maximum is %#RX64",
                                 paBitfields[idx].pszName, (uint64_t)i64Tmp, (RT_BIT_64(paBitfields[idx].cBits) - 1));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                    u64 |= ((uint64_t)i64Tmp) << paBitfields[idx].offBitfield;
                break;
            }
            case RTJSONVALTYPE_STRING:
            {
                if (paBitfields[idx].paNamedValues)
                {
                    const char *pszNamedValue = RTJsonValueGetString(hValue);
                    PCTSTCFGNAMEDVALUE pNamedValue = &paBitfields[idx].paNamedValues[0];
                    while (pNamedValue->pszName)
                    {
                        if (!RTStrICmp(pszNamedValue, pNamedValue->pszName))
                        {
                            u64 |= pNamedValue->u64Val << paBitfields[idx].offBitfield;
                            break;
                        }
                        pNamedValue++;
                    }
                    if (!pNamedValue->pszName)
                    {
                        RTTestFailed(hTest, "\"%s\" ist not a known named value for bitfield '%s'", pszNamedValue, paBitfields[idx].pszName);
                        rc = VERR_NOT_FOUND;
                    }
                }
                else
                {
                    RTTestFailed(hTest, "Bitfield \"%s\" doesn't support named values", paBitfields[idx].pszName);
                    rc = VERR_NOT_SUPPORTED;
                }
                break;
            }
            default:
                rc = VERR_NOT_SUPPORTED;
                RTTestFailed(hTest, "JSON value type %d is not supported\n", enmType);
                break;
        }
        RTJsonValueRelease(hValue);

        if (RT_FAILURE(rc))
            return rc;

        idx++;
    }

    *pu64Result = u64;
    return VINF_SUCCESS;
}


static int tstMmuCfgReadU64(RTTEST hTest, RTJSONVAL hObj, const char *pszName, PCTSTCFGBITFIELD paBitfields, uint64_t *pu64Result)
{
    RTJSONVAL hValue = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hObj, pszName, &hValue);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed to query \"%s\" with %Rrc", pszName, rc);
        return rc;
    }

    RTJSONVALTYPE enmType = RTJsonValueGetType(hValue);
    switch (enmType)
    {
        case RTJSONVALTYPE_INTEGER:
            rc = RTJsonValueQueryInteger(hValue, (int64_t *)pu64Result);
            break;
        case RTJSONVALTYPE_OBJECT:
            rc = tstMmuCfgReadBitfieldU64(hTest, hValue, paBitfields, pu64Result);
            break;
        default:
            rc = VERR_NOT_SUPPORTED;
            RTTestFailed(hTest, "JSON value type %d is not supported\n", enmType);
            break;
    }

    RTJsonValueRelease(hValue);
    return rc;
}


static int tstMmuCfgReadU32(RTTEST hTest, RTJSONVAL hObj, const char *pszName, PCTSTCFGBITFIELD paBitfields, uint32_t *pu32Result)
{
    uint64_t u64 = 0;
    int rc = tstMmuCfgReadU64(hTest, hObj, pszName, paBitfields, &u64);
    if (RT_FAILURE(rc))
        return rc;

    *pu32Result = (uint32_t)u64;

    return VINF_SUCCESS;
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
        case RTJSONVALTYPE_OBJECT:
        {
            static const TSTCFGNAMEDVALUE s_aApPerm[] =
            {
                { "PRW",  0 },
                { "UPRW", 1 },
                { "PR",   2 },
                { "UPR",  3 },
                { NULL,   0 }
            };
            static const TSTCFGBITFIELD s_aTblBitfields[] =
            {
#define BITFIELD_CREATE_BOOL(a_Name, a_offBit) \
                { a_Name, a_offBit, 1, NULL }

                { "Raw",       0, 64, NULL         },
                { "SwUse",    55,  4, NULL         },
                BITFIELD_CREATE_BOOL("UXN", 54),
                BITFIELD_CREATE_BOOL("PXN", 53),
                { "PhysAddr", 0, 64, NULL          },
                BITFIELD_CREATE_BOOL("AF",  10),
                { "AP",       6,  2, &s_aApPerm[0] },
                BITFIELD_CREATE_BOOL("T",    1),
                BITFIELD_CREATE_BOOL("V",    0),
                { NULL,     0, 0, NULL }

#undef BITFIELD_CREATE_BOOL
            };

            uint64_t u64Val = 0;
            rc = tstMmuCfgReadBitfieldU64(hTest, hMemObj, &s_aTblBitfields[0], &u64Val);
            if (RT_SUCCESS(rc))
                rc = tstTestcaseMmuMemoryWrite(hTest, pMmuCfg, GCPhysAddr, &u64Val, sizeof(u64Val));
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
    uint64_t u64RegSctlrEl1 = 0;
    int rc = tstMmuCfgReadU64(hTest, hTestcase, "SCTLR_EL1", NULL, &u64RegSctlrEl1);
    if (RT_FAILURE(rc))
        return rc;

    uint64_t u64RegTcrEl1 = 0;
    static const TSTCFGNAMEDVALUE s_aTgSizes[] =
    {
        { "4K",  2 },
        { "64K", 1 },
        { "16K", 3 },
        { NULL,  0 }
    };
    static const TSTCFGNAMEDVALUE s_aIpsSizes[] =
    {
        { "4G",   0 },
        { "64G",  1 },
        { "1T",   2 },
        { "4T",   3 },
        { "16T",  4 },
        { "256T", 5 },
        { "4P",   6 },
        { "64P",  7 },

        { NULL,  0 }
    };
    static const TSTCFGBITFIELD s_aTcrEl1Bitfields[] =
    {
#define BITFIELD_CREATE_BOOL(a_Name, a_offBit) \
        { a_Name, a_offBit, 1, NULL }

        BITFIELD_CREATE_BOOL("MTX1",   61),
        BITFIELD_CREATE_BOOL("MTX0",   60),
        BITFIELD_CREATE_BOOL("DS",     59),
        BITFIELD_CREATE_BOOL("TCMA1",  58),
        BITFIELD_CREATE_BOOL("TCMA0",  57),
        BITFIELD_CREATE_BOOL("E0PD1",  56),
        BITFIELD_CREATE_BOOL("E0PD0",  55),
        BITFIELD_CREATE_BOOL("NFD1",   54),
        BITFIELD_CREATE_BOOL("NFD0",   53),
        BITFIELD_CREATE_BOOL("TBID1",  52),
        BITFIELD_CREATE_BOOL("TBID0",  51),
        BITFIELD_CREATE_BOOL("HWU162", 50),
        BITFIELD_CREATE_BOOL("HWU161", 49),
        BITFIELD_CREATE_BOOL("HWU160", 48),
        BITFIELD_CREATE_BOOL("HWU159", 47),
        BITFIELD_CREATE_BOOL("HWU062", 46),
        BITFIELD_CREATE_BOOL("HWU061", 45),
        BITFIELD_CREATE_BOOL("HWU060", 44),
        BITFIELD_CREATE_BOOL("HWU059", 43),
        BITFIELD_CREATE_BOOL("HPD1",   42),
        BITFIELD_CREATE_BOOL("HPD0",   41),
        BITFIELD_CREATE_BOOL("HD",     40),
        BITFIELD_CREATE_BOOL("HA",     39),
        BITFIELD_CREATE_BOOL("TBI1",   38),
        BITFIELD_CREATE_BOOL("TBI0",   37),
        { "IPS",   32, 3, &s_aIpsSizes[0] },
        { "TG1",   30, 2, &s_aTgSizes[0]  },
        { "SH1",   28, 2, NULL },
        { "ORGN1", 26, 2, NULL },
        { "IRGN1", 24, 2, NULL },
        BITFIELD_CREATE_BOOL("EPD1",   33),
        BITFIELD_CREATE_BOOL("A1",     22),
        { "T1SZ",  16, 6, NULL },

        { "TG0",   14, 2, &s_aTgSizes[0] },
        { "SH0",   12, 2, NULL },
        { "ORGN0", 10, 2, NULL },
        { "IRGN0",  8, 2, NULL },
        BITFIELD_CREATE_BOOL("EPD0",    7),
        { "T0SZ",   0, 6, NULL },
        { NULL,     0, 0, NULL }

#undef BITFIELD_CREATE_BOOL
    };
    rc = tstMmuCfgReadU64(hTest, hTestcase, "TCR_EL1", &s_aTcrEl1Bitfields[0], &u64RegTcrEl1);
    if (RT_FAILURE(rc))
        return rc;

    rc = tstMmuCfgReadU64(hTest, hTestcase, "TTBR0_EL1", NULL, &pVCpu->cpum.s.Guest.Ttbr0.u64);
    if (RT_FAILURE(rc))
        return rc;

    rc = tstMmuCfgReadU64(hTest, hTestcase, "TTBR1_EL1", NULL, &pVCpu->cpum.s.Guest.Ttbr1.u64);
    if (RT_FAILURE(rc))
        return rc;

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


static int tstResultInit(RTTEST hTest, RTJSONVAL hMemResult, PPGMPTWALKFAST pWalkResult, int *prcQueryPageFast,
                         int *prcGetPage)
{
    int rc = tstMmuCfgReadRc(hTest, hMemResult, "rcQueryPageFast", prcQueryPageFast);
    if (RT_SUCCESS(rc))
        rc = tstMmuCfgReadRc(hTest, hMemResult, "rcGetPage", prcGetPage);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryGCPhys(hTest, hMemResult, "GCPhys", &pWalkResult->GCPhys);
    if (RT_SUCCESS(rc))
        rc = tstResultQueryGCPhysDef(hTest, hMemResult, "GCPhysNested", &pWalkResult->GCPhysNested, 0);
    if (RT_SUCCESS(rc))
    {
        static const TSTCFGBITFIELD s_aInfo[] =
        {
#define BITFIELD_CREATE_BOOL(a_Name, a_offBit) \
            { #a_Name, a_offBit, 1, NULL }
            BITFIELD_CREATE_BOOL(Succeeded,          0),
            BITFIELD_CREATE_BOOL(IsSlat,             1),
            BITFIELD_CREATE_BOOL(BigPage,            7),
            BITFIELD_CREATE_BOOL(GiganticPage,       8),
            BITFIELD_CREATE_BOOL(IsLinearAddrValid, 10),
            { NULL,     0, 0, NULL }
        };
        AssertCompile(PGM_WALKINFO_SUCCEEDED            == RT_BIT_32(0));
        AssertCompile(PGM_WALKINFO_IS_SLAT              == RT_BIT_32(1));
        AssertCompile(PGM_WALKINFO_BIG_PAGE             == RT_BIT_32(7));
        AssertCompile(PGM_WALKINFO_GIGANTIC_PAGE        == RT_BIT_32(8));
        AssertCompile(PGM_WALKINFO_IS_LINEAR_ADDR_VALID == RT_BIT_32(10));

        rc = tstMmuCfgReadU32(hTest, hMemResult, "Info", &s_aInfo[0], &pWalkResult->fInfo);
    }
    if (RT_SUCCESS(rc))
    {
        static const TSTCFGBITFIELD s_aFailed[] =
        {
            BITFIELD_CREATE_BOOL(NotPresent,          0),
            BITFIELD_CREATE_BOOL(ReservedBits,        1),
            BITFIELD_CREATE_BOOL(BadPhysicalAddress,  2),
            BITFIELD_CREATE_BOOL(NotWritable,         6),
            BITFIELD_CREATE_BOOL(NotExecutable,       7),
            BITFIELD_CREATE_BOOL(NotAccessibleByMode, 8),
            { "Level",  11, 5, NULL },
            { NULL,      0, 0, NULL }

#undef BITFIELD_CREATE_BOOL
        };
        AssertCompile(PGM_WALKFAIL_NOT_PRESENT            == RT_BIT_32(0));
        AssertCompile(PGM_WALKFAIL_RESERVED_BITS          == RT_BIT_32(1));
        AssertCompile(PGM_WALKFAIL_BAD_PHYSICAL_ADDRESS   == RT_BIT_32(2));
        AssertCompile(PGM_WALKFAIL_NOT_WRITABLE           == RT_BIT_32(6));
        AssertCompile(PGM_WALKFAIL_NOT_EXECUTABLE         == RT_BIT_32(7));
        AssertCompile(PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE == RT_BIT_32(8));

        rc = tstMmuCfgReadU32(hTest, hMemResult, "Failed", &s_aFailed[0], &pWalkResult->fFailed);
    }
    if (RT_SUCCESS(rc))
    {
        static const TSTCFGBITFIELD s_aPtAttrs[] =
        {
#define BITFIELD_CREATE_BOOL(a_Name) \
            { #a_Name, PGM_PTATTRS_##a_Name##_SHIFT, 1, NULL }

            BITFIELD_CREATE_BOOL(PR),
            BITFIELD_CREATE_BOOL(PW),
            BITFIELD_CREATE_BOOL(PX),
            BITFIELD_CREATE_BOOL(PGCS),
            BITFIELD_CREATE_BOOL(UR),
            BITFIELD_CREATE_BOOL(UW),
            BITFIELD_CREATE_BOOL(UX),
            BITFIELD_CREATE_BOOL(UGCS),
            { NULL,     0, 0, NULL }

#undef BITFIELD_CREATE_BOOL
        };

        rc = tstMmuCfgReadU64(hTest, hMemResult, "Effective", &s_aPtAttrs[0], &pWalkResult->fEffective);
    }

    return rc;
}


static void tstExecuteQueryPageFast(RTTEST hTest, PVM pVM, RTGCPTR GCPtr, uint32_t fFlags, int rcExpected, PPGMPTWALKFAST pWalkResult)
{
    PVMCPUCC pVCpu = pVM->apCpusR3[0];

    /** @todo Incorporate EL (for nested virt and EL3 later on). */
    uintptr_t idx =   (GCPtr & RT_BIT_64(55))
                    ? pVCpu->pgm.s.aidxGuestModeDataTtbr1[1]
                    : pVCpu->pgm.s.aidxGuestModeDataTtbr0[1];

    PGMPTWALKFAST Walk; PGMPTWALKFAST_ZERO(&Walk);
    AssertReleaseReturnVoid(idx < RT_ELEMENTS(g_aPgmGuestModeData));
    AssertReleaseReturnVoid(g_aPgmGuestModeData[idx].pfnQueryPageFast);
    int rc = g_aPgmGuestModeData[idx].pfnQueryPageFast(pVCpu, GCPtr, fFlags, &Walk);
    if (rc != rcExpected)
        RTTestFailed(hTest, "QueryPageFast: Result rc=%Rrc != Expected rc=%Rrc", rc, rcExpected);

    if (memcmp(&Walk, pWalkResult, sizeof(Walk)))
    {
        if (Walk.GCPtr != pWalkResult->GCPtr)
            RTTestFailed(hTest, "QueryPageFast: Result GCPtr=%RGv != Expected GCPtr=%RGv", Walk.GCPtr, pWalkResult->GCPtr);
        if (Walk.GCPhys != pWalkResult->GCPhys)
            RTTestFailed(hTest, "QueryPageFast: Result GCPhys=%RGp != Expected GCPhys=%RGp", Walk.GCPhys, pWalkResult->GCPhys);
        if (Walk.GCPhysNested != pWalkResult->GCPhysNested)
            RTTestFailed(hTest, "QueryPageFast: Result GCPhysNested=%RGp != Expected GCPhysNested=%RGp", Walk.GCPhysNested, pWalkResult->GCPhysNested);
        if (Walk.fInfo != pWalkResult->fInfo)
            RTTestFailed(hTest, "QueryPageFast: Result fInfo=%#RX32 != Expected fInfo=%#RX32", Walk.fInfo, pWalkResult->fInfo);
        if (Walk.fFailed != pWalkResult->fFailed)
            RTTestFailed(hTest, "QueryPageFast: Result fFailed=%#RX32 != Expected fFailed=%#RX32", Walk.fFailed, pWalkResult->fFailed);
        if (Walk.fEffective != pWalkResult->fEffective)
            RTTestFailed(hTest, "QueryPageFast: Result fEffective=%#RX64 != Expected fEffective=%#RX64", Walk.fEffective, pWalkResult->fEffective);
    }
}


static void tstExecuteGetPage(RTTEST hTest, PVM pVM, RTGCPTR GCPtr, uint8_t bEl, int rcExpected, PPGMPTWALKFAST pWalkResult)
{
    PVMCPUCC pVCpu = pVM->apCpusR3[0];

    g_MmuCfg.bEl = bEl;

    /* Need to convert the expected result to the PGMPTWALK format. */
    PGMPTWALK WalkResult; RT_ZERO(WalkResult);
    WalkResult.GCPtr        = pWalkResult->GCPtr;
    WalkResult.GCPhysNested = pWalkResult->GCPhysNested;
    WalkResult.GCPhys       = pWalkResult->GCPhys;
    WalkResult.fEffective   = pWalkResult->fEffective;

    if (pWalkResult->fInfo & PGM_WALKINFO_IS_SLAT)
        WalkResult.fIsSlat = true;
    if (pWalkResult->fInfo & PGM_WALKINFO_BIG_PAGE)
        WalkResult.fBigPage = true;
    if (pWalkResult->fInfo & PGM_WALKINFO_GIGANTIC_PAGE)
        WalkResult.fGigantPage = true;
    if (pWalkResult->fInfo & PGM_WALKINFO_IS_LINEAR_ADDR_VALID)
        WalkResult.fIsLinearAddrValid = true;
    if (pWalkResult->fFailed & PGM_WALKFAIL_NOT_PRESENT)
        WalkResult.fNotPresent = true;
    if (pWalkResult->fFailed & PGM_WALKFAIL_RESERVED_BITS)
        WalkResult.fRsvdError = true;
    if (pWalkResult->fFailed & PGM_WALKFAIL_BAD_PHYSICAL_ADDRESS)
        WalkResult.fBadPhysAddr = true;

    /*
     * QueryPageFast() can return VERR_ACCESS_DENIED, which GetPage() doesn't,
     * so only copy the failed result if GetPage() is expected to fail as well.
     */
    if (RT_FAILURE(rcExpected))
    {
        WalkResult.fFailed    = pWalkResult->fFailed;
        WalkResult.uLevel     = (pWalkResult->fFailed & PGM_WALKFAIL_LEVEL_MASK) >> PGM_WALKFAIL_LEVEL_SHIFT;
        WalkResult.fSucceeded = false;
    } 
    else
        WalkResult.fSucceeded = true;

    /** @todo Incorporate EL (for nested virt and EL3 later on). */
    uintptr_t idx =   (GCPtr & RT_BIT_64(55))
                    ? pVCpu->pgm.s.aidxGuestModeDataTtbr1[1]
                    : pVCpu->pgm.s.aidxGuestModeDataTtbr0[1];

    PGMPTWALK Walk; RT_ZERO(Walk);
    AssertReleaseReturnVoid(idx < RT_ELEMENTS(g_aPgmGuestModeData));
    AssertReleaseReturnVoid(g_aPgmGuestModeData[idx].pfnGetPage);
    int rc = g_aPgmGuestModeData[idx].pfnGetPage(pVCpu, GCPtr, &Walk);
    if (rc != rcExpected)
        RTTestFailed(hTest, "GetPage: Result rc=%Rrc != Expected rc=%Rrc", rc, rcExpected);

    if (memcmp(&Walk, &WalkResult, sizeof(Walk)))
    {
        if (Walk.GCPtr != WalkResult.GCPtr)
            RTTestFailed(hTest, "GetPage: Result GCPtr=%RGv != Expected GCPtr=%RGv", Walk.GCPtr, WalkResult.GCPtr);
        if (Walk.GCPhysNested != WalkResult.GCPhysNested)
            RTTestFailed(hTest, "GetPage: Result GCPhysNested=%RGp != Expected GCPhysNested=%RGp", Walk.GCPhysNested, WalkResult.GCPhysNested);
        if (Walk.GCPhys != WalkResult.GCPhys)
            RTTestFailed(hTest, "GetPage: Result GCPhys=%RGp != Expected GCPhys=%RGp", Walk.GCPhys, WalkResult.GCPhys);
        if (Walk.fSucceeded != WalkResult.fSucceeded)
            RTTestFailed(hTest, "GetPage: Result fSucceeded=%RTbool != Expected fSucceeded=%RTbool", Walk.fSucceeded, WalkResult.fSucceeded);
        if (Walk.fIsSlat != WalkResult.fIsSlat)
            RTTestFailed(hTest, "GetPage: Result fIsSlat=%RTbool != Expected fIsSlat=%RTbool", Walk.fIsSlat, WalkResult.fIsSlat);
        if (Walk.fIsLinearAddrValid != WalkResult.fIsLinearAddrValid)
            RTTestFailed(hTest, "GetPage: Result fIsLinearAddrValid=%RTbool != Expected fIsLinearAddrValid=%RTbool", Walk.fIsLinearAddrValid, WalkResult.fIsLinearAddrValid);
        if (Walk.uLevel != WalkResult.uLevel)
            RTTestFailed(hTest, "GetPage: Result uLevel=%RU8 != Expected uLevel=%RU8", Walk.uLevel, WalkResult.uLevel);
        if (Walk.fNotPresent != WalkResult.fNotPresent)
            RTTestFailed(hTest, "GetPage: Result fNotPresent=%RTbool != Expected fNotPresent=%RTbool", Walk.fNotPresent, WalkResult.fNotPresent);
        if (Walk.fBadPhysAddr != WalkResult.fBadPhysAddr)
            RTTestFailed(hTest, "GetPage: Result fBadPhysAddr=%RTbool != Expected fBadPhysAddr=%RTbool", Walk.fBadPhysAddr, WalkResult.fBadPhysAddr);
        if (Walk.fRsvdError != WalkResult.fRsvdError)
            RTTestFailed(hTest, "GetPage: Result fRsvdError=%RTbool != Expected fRsvdError=%RTbool", Walk.fRsvdError, WalkResult.fRsvdError);
        if (Walk.fBigPage != WalkResult.fBigPage)
            RTTestFailed(hTest, "GetPage: Result fBigPage=%RTbool != Expected fBigPage=%RTbool", Walk.fBigPage, WalkResult.fBigPage);
        if (Walk.fGigantPage != WalkResult.fGigantPage)
            RTTestFailed(hTest, "GetPage: Result fGigantPage=%RTbool != Expected fGigantPage=%RTbool", Walk.fGigantPage, WalkResult.fGigantPage);
        if (Walk.fFailed != WalkResult.fFailed)
            RTTestFailed(hTest, "GetPage: Result fFailed=%#RX32 != Expected fFailed=%#RX32", Walk.fFailed, WalkResult.fFailed);
        if (Walk.fEffective != WalkResult.fEffective)
            RTTestFailed(hTest, "GetPage: Result fEffective=%#RX64 != Expected fEffective=%#RX64", Walk.fEffective, WalkResult.fEffective);
    }
}


static int tstTestcaseMmuRun(RTTEST hTest, RTJSONVAL hTestcase)
{
    RTJSONVAL hVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hTestcase, "Tests", &hVal);
    if (RT_SUCCESS(rc))
    {
        RTJSONIT hIt = NIL_RTJSONIT;
        rc = RTJsonIteratorBeginArray(hVal, &hIt);
        if (RT_SUCCESS(rc))
        {
            for (;;)
            {
                RTJSONVAL hMemObj = NIL_RTJSONVAL;
                rc = RTJsonIteratorQueryValue(hIt, &hMemObj, NULL);
                if (RT_SUCCESS(rc))
                {
                    uint64_t GCPtr = 0;
                    rc = RTJsonValueQueryIntegerByName(hMemObj, "GCPtr", (int64_t *)&GCPtr);
                    if (rc == VINF_SUCCESS)
                    {
                        static const TSTCFGBITFIELD s_aQPage[] =
                        {
#define BITFIELD_CREATE_BOOL(a_Name, a_offBit) \
                            { #a_Name, a_offBit, 1, NULL }

                            BITFIELD_CREATE_BOOL(READ,    0),
                            BITFIELD_CREATE_BOOL(WRITE,   1),
                            BITFIELD_CREATE_BOOL(EXECUTE, 2),
                            BITFIELD_CREATE_BOOL(USER,    3),
                            { NULL,     0, 0, NULL }

#undef BITFIELD_CREATE_BOOL
                        };
                        AssertCompile(PGMQPAGE_F_READ      == RT_BIT_32(0));
                        AssertCompile(PGMQPAGE_F_WRITE     == RT_BIT_32(1));
                        AssertCompile(PGMQPAGE_F_EXECUTE   == RT_BIT_32(2));
                        AssertCompile(PGMQPAGE_F_USER_MODE == RT_BIT_32(3));

                        uint32_t fFlags = 0;
                        rc = tstMmuCfgReadU32(hTest, hMemObj, "Flags", &s_aQPage[0], &fFlags);
                        if (RT_SUCCESS(rc))
                        {
                            RTJSONVAL hMemResult = NIL_RTJSONVAL;
                            rc = RTJsonValueQueryByName(hMemObj, "Result", &hMemResult);
                            if (RT_SUCCESS(rc))
                            {
                                int rcQueryPageFast = VINF_SUCCESS;
                                int rcGetPage       = VINF_SUCCESS;

                                PGMPTWALKFAST WalkResult; PGMPTWALKFAST_ZERO(&WalkResult);
                                WalkResult.GCPtr = GCPtr;

                                rc = tstResultInit(hTest, hMemResult, &WalkResult, &rcQueryPageFast, &rcGetPage);
                                if (RT_SUCCESS(rc))
                                {
                                    tstExecuteQueryPageFast(hTest, g_MmuCfg.pVM, GCPtr, fFlags, rcQueryPageFast, &WalkResult);
                                    tstExecuteGetPage(hTest, g_MmuCfg.pVM, GCPtr,
                                                      (fFlags & PGMQPAGE_F_USER_MODE) ? 0 : 1,
                                                      rcGetPage, &WalkResult);
                                }
                                RTJsonValueRelease(hMemResult);
                            }
                            else
                            {
                                RTTestFailed(hTest, "Querying 'Result' failed with %Rrc", rc);
                                break;
                            }
                        }
                    }
                    else
                    {
                        RTTestFailed(hTest, "Querying 'GCPtr' failed with %Rrc", rc);
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
    else if (rc == VERR_NOT_FOUND)
        rc = VINF_SUCCESS;
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

