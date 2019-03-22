/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Minimal implementation.
 */

/*
 * Copyright (C) 2014-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GIM
#include <VBox/vmm/gim.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/apic.h>
#include "GIMInternal.h"
#include <VBox/vmm/vm.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/**
 * Initializes the Minimal provider.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3MinimalInit(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_MINIMAL, VERR_INTERNAL_ERROR_5);

    /*
     * Expose HVP (Hypervisor Present) bit to the guest.
     */
    CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    /*
     * Insert the hypervisor leaf range.
     */
    CPUMCPUIDLEAF HyperLeaf;
    RT_ZERO(HyperLeaf);
    HyperLeaf.uLeaf = UINT32_C(0x40000000);
    HyperLeaf.uEax  = UINT32_C(0x40000010); /* Maximum leaf we implement. */
    int rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    if (RT_SUCCESS(rc))
    {
        /*
         * Insert missing zero leaves (you never know what missing leaves are
         * going to return when read).
         */
        RT_ZERO(HyperLeaf);
        for (uint32_t uLeaf = UINT32_C(0x40000001); uLeaf <= UINT32_C(0x40000010); uLeaf++)
        {
            HyperLeaf.uLeaf = uLeaf;
            rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
            AssertLogRelRCReturn(rc, rc);
        }
    }
    else
        LogRel(("GIM: Minimal: Failed to insert hypervisor leaf %#RX32. rc=%Rrc\n", HyperLeaf.uLeaf, rc));

    return rc;
}


/**
 * Initializes remaining bits of the Minimal provider.
 * This is called after initializing HM and almost all other VMM components.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3MinimalInitCompleted(PVM pVM)
{
    /*
     * Expose a generic hypervisor-agnostic leaf (originally defined by VMware).
     * The leaves range from  0x40000010 to 0x400000FF.
     *
     * This is done in the init. completed routine as we need PDM to be
     * initialized (otherwise APICGetTimerFreq() would fail).
     */
    CPUMCPUIDLEAF HyperLeaf;
    int rc = CPUMR3CpuIdGetLeaf(pVM, &HyperLeaf, 0x40000000, 0 /* uSubLeaf */);
    if (RT_SUCCESS(rc))
    {
        Assert(HyperLeaf.uEax >= 0x40000010);

        /*
         * Add the timing information hypervisor leaf.
         * MacOS X uses this to determine the TSC, bus frequency. See @bugref{7270}.
         *
         * EAX - TSC frequency in KHz.
         * EBX - APIC frequency in KHz.
         * ECX, EDX - Reserved.
         */
        uint64_t uApicFreq;
        rc = APICGetTimerFreq(pVM, &uApicFreq);
        AssertLogRelRCReturn(rc, rc);

        RT_ZERO(HyperLeaf);
        HyperLeaf.uLeaf        = UINT32_C(0x40000010);
        HyperLeaf.uEax         = TMCpuTicksPerSecond(pVM) / UINT64_C(1000);
        HyperLeaf.uEbx         = (uApicFreq + 500) / UINT64_C(1000);
        rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
        AssertLogRelRCReturn(rc, rc);
    }
    else
        LogRel(("GIM: Minimal: failed to get hypervisor leaf 0x40000000. rc=%Rrc\n", rc));

    return VINF_SUCCESS;
}

