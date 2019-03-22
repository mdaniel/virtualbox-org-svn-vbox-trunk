/* $Id$ */
/** @file
 * MM - Memory Manager.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_mm     MM - The Memory Manager
 *
 * The memory manager is in charge of the following memory:
 *      - Hypervisor Memory Area (HMA) - Address space management.
 *      - Hypervisor Heap - A memory heap that lives in all contexts.
 *      - Tagged ring-3 heap.
 *      - Page pools - Primarily used by PGM for shadow page tables.
 *      - Locked process memory - Guest RAM and other. (reduce/obsolete this)
 *      - Physical guest memory (RAM & ROM) - Moving to PGM. (obsolete this)
 *
 * The global memory manager (GMM) is the global counter part / partner of MM.
 * MM will provide therefore ring-3 callable interfaces for some of the GMM APIs
 * related to resource tracking (PGM is the user).
 *
 * @see grp_mm
 *
 *
 * @section sec_mm_hma  Hypervisor Memory Area
 *
 * The HMA is used when executing in raw-mode. We borrow, with the help of
 * PGMMap, some unused space (one or more page directory entries to be precise)
 * in the guest's virtual memory context. PGM will monitor the guest's virtual
 * address space for changes and relocate the HMA when required.
 *
 * To give some idea what's in the HMA, study the 'info hma' output:
 * @verbatim
VBoxDbg> info hma
Hypervisor Memory Area (HMA) Layout: Base 00000000a0000000, 0x00800000 bytes
00000000a05cc000-00000000a05cd000                  DYNAMIC                  fence
00000000a05c4000-00000000a05cc000                  DYNAMIC                  Dynamic mapping
00000000a05c3000-00000000a05c4000                  DYNAMIC                  fence
00000000a05b8000-00000000a05c3000                  DYNAMIC                  Paging
00000000a05b6000-00000000a05b8000                  MMIO2   0000000000000000 PCNetShMem
00000000a0536000-00000000a05b6000                  MMIO2   0000000000000000 VGA VRam
00000000a0523000-00000000a0536000 00002aaab3d0c000 LOCKED  autofree         alloc once (PDM_DEVICE)
00000000a0522000-00000000a0523000                  DYNAMIC                  fence
00000000a051e000-00000000a0522000 00002aaab36f5000 LOCKED  autofree         VBoxDD2RC.rc
00000000a051d000-00000000a051e000                  DYNAMIC                  fence
00000000a04eb000-00000000a051d000 00002aaab36c3000 LOCKED  autofree         VBoxDDRC.rc
00000000a04ea000-00000000a04eb000                  DYNAMIC                  fence
00000000a04e9000-00000000a04ea000 00002aaab36c2000 LOCKED  autofree         ram range (High ROM Region)
00000000a04e8000-00000000a04e9000                  DYNAMIC                  fence
00000000a040e000-00000000a04e8000 00002aaab2e6d000 LOCKED  autofree         VMMRC.rc
00000000a0208000-00000000a040e000 00002aaab2c67000 LOCKED  autofree         alloc once (PATM)
00000000a01f7000-00000000a0208000 00002aaaab92d000 LOCKED  autofree         alloc once (SELM)
00000000a01e7000-00000000a01f7000 00002aaaab5e8000 LOCKED  autofree         alloc once (SELM)
00000000a01e6000-00000000a01e7000                  DYNAMIC                  fence
00000000a01e5000-00000000a01e6000 00002aaaab5e7000 HCPHYS  00000000c363c000 Core Code
00000000a01e4000-00000000a01e5000                  DYNAMIC                  fence
00000000a01e3000-00000000a01e4000 00002aaaaab26000 HCPHYS  00000000619cf000 GIP
00000000a01a2000-00000000a01e3000 00002aaaabf32000 LOCKED  autofree         alloc once (PGM_PHYS)
00000000a016b000-00000000a01a2000 00002aaab233f000 LOCKED  autofree         alloc once (PGM_POOL)
00000000a016a000-00000000a016b000                  DYNAMIC                  fence
00000000a0165000-00000000a016a000                  DYNAMIC                  CR3 mapping
00000000a0164000-00000000a0165000                  DYNAMIC                  fence
00000000a0024000-00000000a0164000 00002aaab215f000 LOCKED  autofree         Heap
00000000a0023000-00000000a0024000                  DYNAMIC                  fence
00000000a0001000-00000000a0023000 00002aaab1d24000 LOCKED  pages            VM
00000000a0000000-00000000a0001000                  DYNAMIC                  fence
 @endverbatim
 *
 *
 * @section sec_mm_hyperheap    Hypervisor Heap
 *
 * The heap is accessible from ring-3, ring-0 and the raw-mode context. That
 * said, it's not necessarily mapped into ring-0 on if that's possible since we
 * don't wish to waste kernel address space without a good reason.
 *
 * Allocations within the heap are always in the same relative position in all
 * contexts, so, it's possible to use offset based linking. In fact, the heap is
 * internally using offset based linked lists tracking heap blocks. We use
 * offset linked AVL trees and lists in a lot of places where share structures
 * between RC, R3 and R0, so this is a strict requirement of the heap. However
 * this means that we cannot easily extend the heap since the extension won't
 * necessarily be in the continuation of the current heap memory in all (or any)
 * context.
 *
 * All allocations are tagged. Per tag allocation statistics will be maintaining
 * and exposed thru STAM when VBOX_WITH_STATISTICS is defined.
 *
 *
 * @section sec_mm_r3heap   Tagged Ring-3 Heap
 *
 * The ring-3 heap is a wrapper around the RTMem API adding allocation
 * statistics and automatic cleanup on VM destruction.
 *
 * Per tag allocation statistics will be maintaining and exposed thru STAM when
 * VBOX_WITH_STATISTICS is defined.
 *
 *
 * @section sec_mm_page     Page Pool
 *
 * The MM manages a page pool from which other components can allocate locked,
 * page aligned and page sized memory objects. The pool provides facilities to
 * convert back and forth between (host) physical and virtual addresses (within
 * the pool of course). Several specialized interfaces are provided for the most
 * common allocations and conversions to save the caller from bothersome casting
 * and extra parameter passing.
 *
 *
 * @section sec_mm_locked   Locked Process Memory
 *
 * MM manages the locked process memory. This is used for a bunch of things
 * (count the LOCKED entries in the 'info hma' output found in @ref sec_mm_hma),
 * but the main consumer of memory is currently for guest RAM. There is an
 * ongoing rewrite that will move all the guest RAM allocation to PGM and
 * GMM.
 *
 * The locking of memory is something doing in cooperation with the VirtualBox
 * support driver, SUPDrv (aka. VBoxDrv), thru the support library API,
 * SUPR3 (aka. SUPLib).
 *
 *
 * @section sec_mm_phys     Physical Guest Memory
 *
 * MM is currently managing the physical memory for the guest. It relies heavily
 * on PGM for this. There is an ongoing rewrite that will move this to PGM. (The
 * rewrite is driven by the need for more flexible guest ram allocation, but
 * also motivated by the fact that MMPhys is just adding stupid bureaucracy and
 * that MMR3PhysReserve is a totally weird artifact that must go away.)
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MM
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/gmm.h>
#include "MMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current saved state version of MM. */
#define MM_SAVED_STATE_VERSION      2


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) mmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) mmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);




/**
 * Initializes the MM members of the UVM.
 *
 * This is currently only the ring-3 heap.
 *
 * @returns VBox status code.
 * @param   pUVM    Pointer to the user mode VM structure.
 */
VMMR3DECL(int) MMR3InitUVM(PUVM pUVM)
{
    /*
     * Assert sizes and order.
     */
    AssertCompile(sizeof(pUVM->mm.s) <= sizeof(pUVM->mm.padding));
    AssertRelease(sizeof(pUVM->mm.s) <= sizeof(pUVM->mm.padding));
    Assert(!pUVM->mm.s.pHeap);

    /*
     * Init the heap.
     */
    int rc = mmR3HeapCreateU(pUVM, &pUVM->mm.s.pHeap);
    if (RT_SUCCESS(rc))
    {
        rc = mmR3UkHeapCreateU(pUVM, &pUVM->mm.s.pUkHeap);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        mmR3HeapDestroy(pUVM->mm.s.pHeap);
        pUVM->mm.s.pHeap = NULL;
    }
    return rc;
}


/**
 * Initializes the MM.
 *
 * MM is managing the virtual address space (among other things) and
 * setup the hypervisor memory area mapping in the VM structure and
 * the hypervisor alloc-only-heap. Assuming the current init order
 * and components the hypervisor memory area looks like this:
 *      -# VM Structure.
 *      -# Hypervisor alloc only heap (also call Hypervisor memory region).
 *      -# Core code.
 *
 * MM determines the virtual address of the hypervisor memory area by
 * checking for location at previous run. If that property isn't available
 * it will choose a default starting location, currently 0xa0000000.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) MMR3Init(PVM pVM)
{
    LogFlow(("MMR3Init\n"));

    /*
     * Assert alignment, sizes and order.
     */
    AssertRelease(!(RT_OFFSETOF(VM, mm.s) & 31));
    AssertRelease(sizeof(pVM->mm.s) <= sizeof(pVM->mm.padding));
    AssertMsg(pVM->mm.s.offVM == 0, ("Already initialized!\n"));

    /*
     * Init the structure.
     */
    pVM->mm.s.offVM = RT_OFFSETOF(VM, mm);
    pVM->mm.s.offLookupHyper = NIL_OFFSET;

    /*
     * Init the page pool.
     */
    int rc = mmR3PagePoolInit(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Init the hypervisor related stuff.
         */
        rc = mmR3HyperInit(pVM);
        if (RT_SUCCESS(rc))
        {
            /*
             * Register the saved state data unit.
             */
            rc = SSMR3RegisterInternal(pVM, "mm", 1, MM_SAVED_STATE_VERSION, sizeof(uint32_t) * 2,
                                       NULL, NULL, NULL,
                                       NULL, mmR3Save, NULL,
                                       NULL, mmR3Load, NULL);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Statistics.
                 */
                STAM_REG(pVM, &pVM->mm.s.cBasePages,   STAMTYPE_U64, "/MM/Reserved/cBasePages",   STAMUNIT_PAGES, "Reserved number of base pages, ROM and Shadow ROM included.");
                STAM_REG(pVM, &pVM->mm.s.cHandyPages,  STAMTYPE_U32, "/MM/Reserved/cHandyPages",  STAMUNIT_PAGES, "Reserved number of handy pages.");
                STAM_REG(pVM, &pVM->mm.s.cShadowPages, STAMTYPE_U32, "/MM/Reserved/cShadowPages", STAMUNIT_PAGES, "Reserved number of shadow paging pages.");
                STAM_REG(pVM, &pVM->mm.s.cFixedPages,  STAMTYPE_U32, "/MM/Reserved/cFixedPages",  STAMUNIT_PAGES, "Reserved number of fixed pages (MMIO2).");
                STAM_REG(pVM, &pVM->mm.s.cbRamBase,    STAMTYPE_U64, "/MM/cbRamBase",             STAMUNIT_BYTES, "Size of the base RAM.");

                return rc;
            }

            /* .... failure .... */
        }
    }
    MMR3Term(pVM);
    return rc;
}


/**
 * Initializes the MM parts which depends on PGM being initialized.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @remark  No cleanup necessary since MMR3Term() will be called on failure.
 */
VMMR3DECL(int) MMR3InitPaging(PVM pVM)
{
    LogFlow(("MMR3InitPaging:\n"));

    /*
     * Query the CFGM values.
     */
    int rc;
    PCFGMNODE pMMCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "MM");
    if (!pMMCfg)
    {
        rc = CFGMR3InsertNode(CFGMR3GetRoot(pVM), "MM", &pMMCfg);
        AssertRCReturn(rc, rc);
    }

    /** @cfgm{/RamSize, uint64_t, 0, 16TB, 0}
     * Specifies the size of the base RAM that is to be set up during
     * VM initialization.
     */
    uint64_t cbRam;
    rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cbRam = 0;
    else
        AssertMsgRCReturn(rc, ("Configuration error: Failed to query integer \"RamSize\", rc=%Rrc.\n", rc), rc);
    AssertLogRelMsg(!(cbRam & ~X86_PTE_PAE_PG_MASK), ("%RGp X86_PTE_PAE_PG_MASK=%RX64\n", cbRam, X86_PTE_PAE_PG_MASK));
    AssertLogRelMsgReturn(cbRam <= GMM_GCPHYS_LAST, ("cbRam=%RGp GMM_GCPHYS_LAST=%RX64\n", cbRam, GMM_GCPHYS_LAST), VERR_OUT_OF_RANGE);
    cbRam &= X86_PTE_PAE_PG_MASK;
    pVM->mm.s.cbRamBase = cbRam;

    /** @cfgm{/RamHoleSize, uint32_t, 0, 4032MB, 512MB}
     * Specifies the size of the memory hole. The memory hole is used
     * to avoid mapping RAM to the range normally used for PCI memory regions.
     * Must be aligned on a 4MB boundary. */
    uint32_t cbRamHole;
    rc = CFGMR3QueryU32Def(CFGMR3GetRoot(pVM), "RamHoleSize", &cbRamHole, MM_RAM_HOLE_SIZE_DEFAULT);
    AssertLogRelMsgRCReturn(rc, ("Configuration error: Failed to query integer \"RamHoleSize\", rc=%Rrc.\n", rc), rc);
    AssertLogRelMsgReturn(cbRamHole <= 4032U * _1M,
                          ("Configuration error: \"RamHoleSize\"=%#RX32 is too large.\n", cbRamHole), VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(cbRamHole > 16 * _1M,
                          ("Configuration error: \"RamHoleSize\"=%#RX32 is too large.\n", cbRamHole), VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(!(cbRamHole & (_4M - 1)),
                          ("Configuration error: \"RamHoleSize\"=%#RX32 is misaligned.\n", cbRamHole), VERR_OUT_OF_RANGE);
    uint64_t const offRamHole = _4G - cbRamHole;
    if (cbRam < offRamHole)
        Log(("MM: %RU64 bytes of RAM\n", cbRam));
    else
        Log(("MM: %RU64 bytes of RAM with a hole at %RU64 up to 4GB.\n", cbRam, offRamHole));

    /** @cfgm{/MM/Policy, string, no overcommitment}
     * Specifies the policy to use when reserving memory for this VM. The recognized
     * value is 'no overcommitment' (default). See GMMPOLICY.
     */
    GMMOCPOLICY enmOcPolicy;
    char sz[64];
    rc = CFGMR3QueryString(CFGMR3GetRoot(pVM), "Policy", sz, sizeof(sz));
    if (RT_SUCCESS(rc))
    {
        if (    !RTStrICmp(sz, "no_oc")
            ||  !RTStrICmp(sz, "no overcommitment"))
            enmOcPolicy = GMMOCPOLICY_NO_OC;
        else
            return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, "Unknown \"MM/Policy\" value \"%s\"", sz);
    }
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        enmOcPolicy = GMMOCPOLICY_NO_OC;
    else
        AssertMsgFailedReturn(("Configuration error: Failed to query string \"MM/Policy\", rc=%Rrc.\n", rc), rc);

    /** @cfgm{/MM/Priority, string, normal}
     * Specifies the memory priority of this VM. The priority comes into play when the
     * system is overcommitted and the VMs needs to be milked for memory. The recognized
     * values are 'low', 'normal' (default) and 'high'. See GMMPRIORITY.
     */
    GMMPRIORITY enmPriority;
    rc = CFGMR3QueryString(CFGMR3GetRoot(pVM), "Priority", sz, sizeof(sz));
    if (RT_SUCCESS(rc))
    {
        if (!RTStrICmp(sz, "low"))
            enmPriority = GMMPRIORITY_LOW;
        else if (!RTStrICmp(sz, "normal"))
            enmPriority = GMMPRIORITY_NORMAL;
        else if (!RTStrICmp(sz, "high"))
            enmPriority = GMMPRIORITY_HIGH;
        else
            return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, "Unknown \"MM/Priority\" value \"%s\"", sz);
    }
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        enmPriority = GMMPRIORITY_NORMAL;
    else
        AssertMsgFailedReturn(("Configuration error: Failed to query string \"MM/Priority\", rc=%Rrc.\n", rc), rc);

    /*
     * Make the initial memory reservation with GMM.
     */
    uint64_t cBasePages = (cbRam >> PAGE_SHIFT) + pVM->mm.s.cBasePages;
    rc = GMMR3InitialReservation(pVM,
                                 RT_MAX(cBasePages + pVM->mm.s.cHandyPages, 1),
                                 RT_MAX(pVM->mm.s.cShadowPages, 1),
                                 RT_MAX(pVM->mm.s.cFixedPages, 1),
                                 enmOcPolicy,
                                 enmPriority);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_GMM_MEMORY_RESERVATION_DECLINED)
            return VMSetError(pVM, rc, RT_SRC_POS,
                              N_("Insufficient free memory to start the VM (cbRam=%#RX64 enmOcPolicy=%d enmPriority=%d)"),
                              cbRam, enmOcPolicy, enmPriority);
        return VMSetError(pVM, rc, RT_SRC_POS, "GMMR3InitialReservation(,%#RX64,0,0,%d,%d)",
                          cbRam >> PAGE_SHIFT, enmOcPolicy, enmPriority);
    }

    /*
     * If RamSize is 0 we're done now.
     */
    if (cbRam < PAGE_SIZE)
    {
        Log(("MM: No RAM configured\n"));
        return VINF_SUCCESS;
    }

    /*
     * Setup the base ram (PGM).
     */
    pVM->mm.s.cbRamHole = cbRamHole;
    if (cbRam > offRamHole)
    {
        pVM->mm.s.cbRamBelow4GB = offRamHole;
        rc = PGMR3PhysRegisterRam(pVM, 0, offRamHole, "Base RAM");
        if (RT_SUCCESS(rc))
        {
            pVM->mm.s.cbRamAbove4GB = cbRam - offRamHole;
            rc = PGMR3PhysRegisterRam(pVM, _4G, cbRam - offRamHole, "Above 4GB Base RAM");
        }
    }
    else
    {
        pVM->mm.s.cbRamBelow4GB = cbRam;
        pVM->mm.s.cbRamAbove4GB = 0;
        rc = PGMR3PhysRegisterRam(pVM, 0, cbRam, "Base RAM");
    }

    /*
     * Enabled mmR3UpdateReservation here since we don't want the
     * PGMR3PhysRegisterRam calls above mess things up.
     */
    pVM->mm.s.fDoneMMR3InitPaging = true;
    AssertMsg(pVM->mm.s.cBasePages == cBasePages || RT_FAILURE(rc), ("%RX64 != %RX64\n", pVM->mm.s.cBasePages, cBasePages));

    LogFlow(("MMR3InitPaging: returns %Rrc\n", rc));
    return rc;
}


/**
 * Terminates the MM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) MMR3Term(PVM pVM)
{
    /*
     * Destroy the page pool. (first as it used the hyper heap)
     */
    mmR3PagePoolTerm(pVM);

    /* Clean up the hypervisor heap. */
    mmR3HyperTerm(pVM);

    /*
     * Zero stuff to detect after termination use of the MM interface
     */
    pVM->mm.s.offLookupHyper = NIL_OFFSET;
    pVM->mm.s.pHyperHeapR3   = NULL;        /* freed above. */
    pVM->mm.s.pHyperHeapR0   = NIL_RTR0PTR; /* freed above. */
    pVM->mm.s.pHyperHeapRC   = NIL_RTRCPTR; /* freed above. */
    pVM->mm.s.offVM          = 0;           /* init assertion on this */

    /*
     * Destroy the User-kernel heap here since the support driver session
     * may have been terminated by the time we get to MMR3TermUVM.
     */
    mmR3UkHeapDestroy(pVM->pUVM->mm.s.pUkHeap);
    pVM->pUVM->mm.s.pUkHeap = NULL;

    return VINF_SUCCESS;
}


/**
 * Terminates the UVM part of MM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pUVM        Pointer to the user mode VM structure.
 */
VMMR3DECL(void) MMR3TermUVM(PUVM pUVM)
{
    /*
     * Destroy the heaps.
     */
    if (pUVM->mm.s.pUkHeap)
    {
        mmR3UkHeapDestroy(pUVM->mm.s.pUkHeap);
        pUVM->mm.s.pUkHeap = NULL;
    }
    mmR3HeapDestroy(pUVM->mm.s.pHeap);
    pUVM->mm.s.pHeap = NULL;
}


/**
 * Checks if the both VM and UVM parts of MM have been initialized.
 *
 * @returns true if initialized, false if not.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(bool) MMR3IsInitialized(PVM pVM)
{
    return pVM->mm.s.pHyperHeapR3 != NULL;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) mmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("mmR3Save:\n"));

    /* (PGM saves the physical memory.) */
    SSMR3PutU64(pSSM, pVM->mm.s.cBasePages);
    return SSMR3PutU64(pSSM, pVM->mm.s.cbRamBase);
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion       Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) mmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    LogFlow(("mmR3Load:\n"));
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Validate version.
     */
    if (    SSM_VERSION_MAJOR_CHANGED(uVersion, MM_SAVED_STATE_VERSION)
        ||  !uVersion)
    {
        AssertMsgFailed(("mmR3Load: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Check the cBasePages and cbRamBase values.
     */
    int rc;
    RTUINT cb1;

    /* cBasePages (ignored) */
    uint64_t cPages;
    if (uVersion >= 2)
        rc = SSMR3GetU64(pSSM, &cPages);
    else
    {
        rc = SSMR3GetUInt(pSSM, &cb1);
        cPages = cb1 >> PAGE_SHIFT;
    }
    if (RT_FAILURE(rc))
        return rc;

    /* cbRamBase */
    uint64_t cb;
    if (uVersion != 1)
        rc = SSMR3GetU64(pSSM, &cb);
    else
    {
        rc = SSMR3GetUInt(pSSM, &cb1);
        cb = cb1;
    }
    if (RT_FAILURE(rc))
        return rc;
    AssertLogRelMsgReturn(cb == pVM->mm.s.cbRamBase,
                          ("Memory configuration has changed. cbRamBase=%#RX64 save=%#RX64\n", pVM->mm.s.cbRamBase, cb),
                          VERR_SSM_LOAD_MEMORY_SIZE_MISMATCH);

    /* (PGM restores the physical memory.) */
    return rc;
}


/**
 * Updates GMM with memory reservation changes.
 *
 * Called when MM::cbRamRegistered, MM::cShadowPages or MM::cFixedPages changes.
 *
 * @returns VBox status code - see GMMR0UpdateReservation.
 * @param   pVM             The cross context VM structure.
 */
int mmR3UpdateReservation(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    if (pVM->mm.s.fDoneMMR3InitPaging)
        return GMMR3UpdateReservation(pVM,
                                      RT_MAX(pVM->mm.s.cBasePages + pVM->mm.s.cHandyPages, 1),
                                      RT_MAX(pVM->mm.s.cShadowPages, 1),
                                      RT_MAX(pVM->mm.s.cFixedPages, 1));
    return VINF_SUCCESS;
}


/**
 * Interface for PGM to increase the reservation of RAM and ROM pages.
 *
 * This can be called before MMR3InitPaging.
 *
 * @returns VBox status code. Will set VM error on failure.
 * @param   pVM             The cross context VM structure.
 * @param   cAddBasePages   The number of pages to add.
 */
VMMR3DECL(int) MMR3IncreaseBaseReservation(PVM pVM, uint64_t cAddBasePages)
{
    uint64_t cOld = pVM->mm.s.cBasePages;
    pVM->mm.s.cBasePages += cAddBasePages;
    LogFlow(("MMR3IncreaseBaseReservation: +%RU64 (%RU64 -> %RU64\n", cAddBasePages, cOld, pVM->mm.s.cBasePages));
    int rc = mmR3UpdateReservation(pVM);
    if (RT_FAILURE(rc))
    {
        VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to reserved physical memory for the RAM (%#RX64 -> %#RX64 + %#RX32)"),
                   cOld, pVM->mm.s.cBasePages, pVM->mm.s.cHandyPages);
        pVM->mm.s.cBasePages = cOld;
    }
    return rc;
}


/**
 * Interface for PGM to make reservations for handy pages in addition to the
 * base memory.
 *
 * This can be called before MMR3InitPaging.
 *
 * @returns VBox status code. Will set VM error on failure.
 * @param   pVM             The cross context VM structure.
 * @param   cHandyPages     The number of handy pages.
 */
VMMR3DECL(int) MMR3ReserveHandyPages(PVM pVM, uint32_t cHandyPages)
{
    AssertReturn(!pVM->mm.s.cHandyPages, VERR_WRONG_ORDER);

    pVM->mm.s.cHandyPages = cHandyPages;
    LogFlow(("MMR3ReserveHandyPages: %RU32 (base %RU64)\n", pVM->mm.s.cHandyPages, pVM->mm.s.cBasePages));
    int rc = mmR3UpdateReservation(pVM);
    if (RT_FAILURE(rc))
    {
        VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to reserved physical memory for the RAM (%#RX64 + %#RX32)"),
                   pVM->mm.s.cBasePages, pVM->mm.s.cHandyPages);
        pVM->mm.s.cHandyPages = 0;
    }
    return rc;
}


/**
 * Interface for PGM to adjust the reservation of fixed pages.
 *
 * This can be called before MMR3InitPaging.
 *
 * @returns VBox status code. Will set VM error on failure.
 * @param   pVM                 The cross context VM structure.
 * @param   cDeltaFixedPages    The number of pages to add (positive) or subtract (negative).
 * @param   pszDesc             Some description associated with the reservation.
 */
VMMR3DECL(int) MMR3AdjustFixedReservation(PVM pVM, int32_t cDeltaFixedPages, const char *pszDesc)
{
    const uint32_t cOld = pVM->mm.s.cFixedPages;
    pVM->mm.s.cFixedPages += cDeltaFixedPages;
    LogFlow(("MMR3AdjustFixedReservation: %d (%u -> %u)\n", cDeltaFixedPages, cOld, pVM->mm.s.cFixedPages));
    int rc = mmR3UpdateReservation(pVM);
    if (RT_FAILURE(rc))
    {
        VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to reserve physical memory (%#x -> %#x; %s)"),
                   cOld, pVM->mm.s.cFixedPages, pszDesc);
        pVM->mm.s.cFixedPages = cOld;
    }
    return rc;
}


/**
 * Interface for PGM to update the reservation of shadow pages.
 *
 * This can be called before MMR3InitPaging.
 *
 * @returns VBox status code. Will set VM error on failure.
 * @param   pVM             The cross context VM structure.
 * @param   cShadowPages    The new page count.
 */
VMMR3DECL(int) MMR3UpdateShadowReservation(PVM pVM, uint32_t cShadowPages)
{
    const uint32_t cOld = pVM->mm.s.cShadowPages;
    pVM->mm.s.cShadowPages = cShadowPages;
    LogFlow(("MMR3UpdateShadowReservation: %u -> %u\n", cOld, pVM->mm.s.cShadowPages));
    int rc = mmR3UpdateReservation(pVM);
    if (RT_FAILURE(rc))
    {
        VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to reserve physical memory for shadow page tables (%#x -> %#x)"), cOld, pVM->mm.s.cShadowPages);
        pVM->mm.s.cShadowPages = cOld;
    }
    return rc;
}


/**
 * Convert HC Physical address to HC Virtual address.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   HCPhys      The host context virtual address.
 * @param   ppv         Where to store the resulting address.
 * @thread  The Emulation Thread.
 *
 * @remarks Avoid whenever possible.
 *          Intended for the debugger facility only.
 * @todo    Rename to indicate the special usage.
 */
VMMR3DECL(int) MMR3HCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    /*
     * Try page tables.
     */
    int rc = MMPagePhys2PageTry(pVM, HCPhys, ppv);
    if (RT_SUCCESS(rc))
        return rc;

    /*
     * Iterate thru the lookup records for HMA.
     */
    uint32_t off = HCPhys & PAGE_OFFSET_MASK;
    HCPhys &= X86_PTE_PAE_PG_MASK;
    PMMLOOKUPHYPER pCur = (PMMLOOKUPHYPER)((uint8_t *)pVM->mm.s.CTX_SUFF(pHyperHeap) + pVM->mm.s.offLookupHyper);
    for (;;)
    {
        switch (pCur->enmType)
        {
            case MMLOOKUPHYPERTYPE_LOCKED:
            {
                PCRTHCPHYS  paHCPhysPages = pCur->u.Locked.paHCPhysPages;
                size_t      iPage         = pCur->cb >> PAGE_SHIFT;
                while (iPage-- > 0)
                    if (paHCPhysPages[iPage] == HCPhys)
                    {
                        *ppv = (char *)pCur->u.Locked.pvR3 + (iPage << PAGE_SHIFT) + off;
                        return VINF_SUCCESS;
                    }
                break;
            }

            case MMLOOKUPHYPERTYPE_HCPHYS:
                if (pCur->u.HCPhys.HCPhys - HCPhys < pCur->cb)
                {
                    *ppv = (uint8_t *)pCur->u.HCPhys.pvR3 + pCur->u.HCPhys.HCPhys - HCPhys + off;
                    return VINF_SUCCESS;
                }
                break;

            case MMLOOKUPHYPERTYPE_GCPHYS:  /* (for now we'll not allow these kind of conversions) */
            case MMLOOKUPHYPERTYPE_MMIO2:
            case MMLOOKUPHYPERTYPE_DYNAMIC:
                break;

            default:
                AssertMsgFailed(("enmType=%d\n", pCur->enmType));
                break;
        }

        /* next */
        if (pCur->offNext ==  (int32_t)NIL_OFFSET)
            break;
        pCur = (PMMLOOKUPHYPER)((uint8_t *)pCur + pCur->offNext);
    }
    /* give up */
    return VERR_INVALID_POINTER;
}



/**
 * Get the size of the base RAM.
 * This usually means the size of the first contiguous block of physical memory.
 *
 * @returns The guest base RAM size.
 * @param   pVM         The cross context VM structure.
 * @thread  Any.
 *
 * @deprecated
 */
VMMR3DECL(uint64_t) MMR3PhysGetRamSize(PVM pVM)
{
    return pVM->mm.s.cbRamBase;
}


/**
 * Get the size of RAM below 4GB (starts at address 0x00000000).
 *
 * @returns The amount of RAM below 4GB in bytes.
 * @param   pVM         The cross context VM structure.
 * @thread  Any.
 */
VMMR3DECL(uint32_t) MMR3PhysGetRamSizeBelow4GB(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, UINT32_MAX);
    return pVM->mm.s.cbRamBelow4GB;
}


/**
 * Get the size of RAM above 4GB (starts at address 0x000100000000).
 *
 * @returns The amount of RAM above 4GB in bytes.
 * @param   pVM         The cross context VM structure.
 * @thread  Any.
 */
VMMR3DECL(uint64_t) MMR3PhysGetRamSizeAbove4GB(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, UINT64_MAX);
    return pVM->mm.s.cbRamAbove4GB;
}


/**
 * Get the size of the RAM hole below 4GB.
 *
 * @returns Size in bytes.
 * @param   pVM         The cross context VM structure.
 * @thread  Any.
 */
VMMR3DECL(uint32_t) MMR3PhysGet4GBRamHoleSize(PVM pVM)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, UINT32_MAX);
    return pVM->mm.s.cbRamHole;
}

