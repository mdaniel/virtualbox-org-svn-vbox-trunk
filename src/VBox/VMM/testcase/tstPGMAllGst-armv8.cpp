/* $Id$ */
/** @file
 * PDM Queue Testcase.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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
#include "VMInternal.h" /* createFakeVM */
#include "../include/PGMInternal.h"

#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;


int pgmPhysGCPhys2CCPtrLockless(PVMCPUCC pVCpu, RTGCPHYS GCPhys, void **ppv)
{
    RT_NOREF(pVCpu, GCPhys, ppv);
    AssertFailed();
    return VINF_SUCCESS;
}


int pgmPhysGCPhys2R3Ptr(PVMCC pVM, RTGCPHYS GCPhys, PRTR3PTR pR3Ptr)
{
    RT_NOREF(pVM, GCPhys, pR3Ptr);
    AssertFailed();
    return VINF_SUCCESS;
}


VMM_INT_DECL(uint8_t) CPUMGetGuestEL(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
    AssertFailed();
    return 0;
}


VMM_INT_DECL(RTGCPHYS) CPUMGetEffectiveTtbr(PVMCPUCC pVCpu, RTGCPTR GCPtr)
{
    RT_NOREF(pVCpu, GCPtr);
    AssertFailed();
    return 0;
}


/** Include and instantiate the page table walking code. */
#include "../VMMAll/PGMAllGst-armv8.cpp.h"


/**
 * Creates a mockup VM structure for testing SSM.
 *
 * @returns 0 on success, 1 on failure.
 * @param   ppVM    Where to store Pointer to the VM.
 *
 * @todo    Move this to VMM/VM since it's stuff done by several testcases.
 */
static int createFakeVM(PVM *ppVM)
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
            *ppVM = pVM;
            return VINF_SUCCESS;
        }

        RTTestIFailed("Fatal error: failed to allocated pages for the VM structure, rc=%Rrc\n", rc);
    }
    else
        RTTestIFailed("Fatal error: RTTlsSet failed, rc=%Rrc\n", rc);

    *ppVM = NULL;
    return rc;
}


/**
 * Destroy the VM structure.
 *
 * @param   pVM     Pointer to the VM.
 *
 * @todo    Move this to VMM/VM since it's stuff done by several testcases.
 */
static void destroyFakeVM(PVM pVM)
{
    RT_NOREF(pVM);
}


static void tstBasic(void)
{
    /*
     * Create an fake VM structure.
     */
    PVM pVM;
    int rc = createFakeVM(&pVM);
    if (RT_FAILURE(rc))
        return;

    /** @todo */

    destroyFakeVM(pVM);
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
            tstBasic();
            rcExit = RTTestSummaryAndDestroy(g_hTest);
        }
        else
            rcExit = RTMsgErrorExitFailure("RTTestCreate failed: %Rrc", rc);
    }
    else
        rcExit = RTMsgInitFailure(rc);
    return rcExit;
}

