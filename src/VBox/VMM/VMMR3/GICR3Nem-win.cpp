/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GIC) - Hyper-V interface.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_GIC
#include <iprt/nt/nt-and-windows.h>
#include <iprt/nt/hyperv.h>
#include <iprt/mem.h>
#include <WinHvPlatform.h>

#include <VBox/log.h>
#include "GICInternal.h"
#include "NEMInternal.h" /* Need access to the partition handle. */
#include <VBox/vmm/pdmgic.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>

#include <iprt/armv8.h>

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GIC saved state version. */
#define GIC_NEM_SAVED_STATE_VERSION               1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * GICHv PDM instance data (per-VM).
 */
typedef struct GICHVDEV
{
    /** Pointer to the PDM device instance. */
    PPDMDEVINSR3         pDevIns;
    /** The partition handle grabbed from NEM. */
    WHV_PARTITION_HANDLE hPartition;
} GICHVDEV;
/** Pointer to a GIC Hyper-V device. */
typedef GICHVDEV *PGICHVDEV;
/** Pointer to a const GIC Hyper-V device. */
typedef GICHVDEV const *PCGICHVDEV;


/*
 * The following definitions appeared in build 27744 allow interacting with the GIC controller,
 * since 27813 the API is public with some changes and available under:
 *      https://github.com/MicrosoftDocs/Virtualization-Documentation/blob/main/virtualization/api/hypervisor-platform/headers/WinHvPlatformDefs.h .
 */
/** @todo Better way of defining these which doesn't require casting later on when calling APIs. */
#define MY_WHV_ARM64_IINTERRUPT_TYPE_FIXED UINT32_C(0)

typedef union MY_WHV_INTERRUPT_CONTROL2
{
    UINT64 AsUINT64;
    struct
    {
        uint32_t InterruptType;
        UINT32 Reserved1:2;
        UINT32 Asserted:1;
        UINT32 Retarget:1;
        UINT32 Reserved2:28;
    };
} MY_WHV_INTERRUPT_CONTROL2;


typedef struct MY_WHV_INTERRUPT_CONTROL
{
    UINT64                  TargetPartition;
    MY_WHV_INTERRUPT_CONTROL2  InterruptControl;
    UINT64                  DestinationAddress;
    UINT32                  RequestedVector;
    UINT8                   TargetVtl;
    UINT8                   ReservedZ0;
    UINT16                  ReservedZ1;
} MY_WHV_INTERRUPT_CONTROL;
AssertCompileSize(MY_WHV_INTERRUPT_CONTROL, 32);


typedef struct MY_WHV_INTERRUPT_STATE
{
    uint8_t                         fState;
    uint8_t                         bIPriorityCfg;
    uint8_t                         bIPriorityActive;
    uint8_t                         bRsvd0;
} MY_WHV_INTERRUPT_STATE;
AssertCompileSize(MY_WHV_INTERRUPT_STATE, sizeof(uint32_t));

#define WHV_INTERRUPT_STATE_F_ENABLED           RT_BIT(0)
#define WHV_INTERRUPT_STATE_F_EDGE_TRIGGERED    RT_BIT(1)
#define WHV_INTERRUPT_STATE_F_ASSERTED          RT_BIT(2)
#define WHV_INTERRUPT_STATE_F_SET_PENDING       RT_BIT(3)
#define WHV_INTERRUPT_STATE_F_ACTIVE            RT_BIT(4)
#define WHV_INTERRUPT_STATE_F_DIRECT            RT_BIT(5)


typedef struct MY_WHV_GLOBAL_INTERRUPT_STATE
{
    uint32_t                        u32IntId;
    uint32_t                        idActiveVp;
    uint32_t                        u32TargetMpidrOrVpIndex;
    MY_WHV_INTERRUPT_STATE          State;
} MY_WHV_GLOBAL_INTERRUPT_STATE;
AssertCompileSize(MY_WHV_GLOBAL_INTERRUPT_STATE, 4 * sizeof(uint32_t));


typedef struct MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE
{
    uint8_t                         bVersion;
    uint8_t                         bGicVersion;
    uint8_t                         abPad[2];

    uint32_t                        cInterrupts;
    uint64_t                        u64RegGicdCtrlEnableGrp1A;

    MY_WHV_GLOBAL_INTERRUPT_STATE   aSpis[1]; /* Flexible */
} MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE;
AssertCompileSize(MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE, 2 * sizeof(uint32_t) + sizeof(uint64_t) + sizeof(MY_WHV_GLOBAL_INTERRUPT_STATE));

#define MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE_VERSION 1


typedef struct MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE
{
    uint8_t                         bVersion;
    uint8_t                         bGicVersion;
    uint8_t                         abPad[6];

    uint64_t                        u64RegIccIGrpEn1El1;
    uint64_t                        u64RegGicrCtrlEnableLpis;
    uint64_t                        u64RegIccBprEl1;
    uint64_t                        u64RegIccPmrEl1;
    uint64_t                        u64RegGicrPropBase;
    uint64_t                        u64RegGicrPendBase;

    uint32_t                        au32RegIchAp1REl2[4];

    MY_WHV_INTERRUPT_STATE          aPpiStates[32];
} MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE;
AssertCompileSize(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, 7 * sizeof(uint64_t) + 4 * sizeof(uint32_t) + 32 * sizeof(MY_WHV_INTERRUPT_STATE));

#define MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE_VERSION 1

#define WHV_VIRTUAL_PROCESSOR_STATE_TYPE_PFN    RT_BIT_32(31)
#define WHV_VIRTUAL_PROCESSOR_STATE_TYPE_ANY_VP RT_BIT_32(30)

#define WHvVirtualProcessorStateTypeInterruptControllerState  (WHV_VIRTUAL_PROCESSOR_STATE_TYPE)(0 | WHV_VIRTUAL_PROCESSOR_STATE_TYPE_PFN)
#define WHvVirtualProcessorStateTypeGlobalInterruptState      (WHV_VIRTUAL_PROCESSOR_STATE_TYPE)(6 | WHV_VIRTUAL_PROCESSOR_STATE_TYPE_PFN | WHV_VIRTUAL_PROCESSOR_STATE_TYPE_ANY_VP)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern decltype(WHvGetVirtualProcessorState) * g_pfnWHvGetVirtualProcessorState;
extern decltype(WHvSetVirtualProcessorState) * g_pfnWHvSetVirtualProcessorState;
extern decltype(WHvRequestInterrupt) *         g_pfnWHvRequestInterrupt;

/*
 * Let the preprocessor alias the APIs to import variables for better autocompletion.
 */
#ifndef IN_SLICKEDIT
# define WHvGetVirtualProcessorState           g_pfnWHvGetVirtualProcessorState
# define WHvSetVirtualProcessorState           g_pfnWHvSetVirtualProcessorState
# define WHvRequestInterrupt                   g_pfnWHvRequestInterrupt
#endif


/** Saved state field descriptors for the global interrupt state. */
static const SSMFIELD g_aWHvGicGlobalInterruptState[] =
{
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, u32IntId),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, idActiveVp),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, u32TargetMpidrOrVpIndex),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, State.fState),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, State.bIPriorityCfg),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_STATE, State.bIPriorityActive),
    SSMFIELD_ENTRY_TERM()
};


/** Saved state field descriptors for the global GIC state (sans the flexible interrupts array. */
static const SSMFIELD g_aWHvGicGlobalState[] =
{
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE, bGicVersion),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE, cInterrupts),
    SSMFIELD_ENTRY(MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE, u64RegGicdCtrlEnableGrp1A),
    SSMFIELD_ENTRY_TERM()
};


#define GIC_NEM_HV_PPI_STATE(a_idx) \
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, aPpiStates[a_idx].fState), \
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, aPpiStates[a_idx].bIPriorityCfg), \
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, aPpiStates[a_idx].bIPriorityActive)


/** Saved state field descriptors for the local interrupt controller state. */
static const SSMFIELD g_aWHvGicLocalInterruptState[] =
{
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, bGicVersion),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegIccIGrpEn1El1),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegGicrCtrlEnableLpis),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegIccBprEl1),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegIccPmrEl1),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegGicrPropBase),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, u64RegGicrPendBase),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, au32RegIchAp1REl2[0]),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, au32RegIchAp1REl2[1]),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, au32RegIchAp1REl2[2]),
    SSMFIELD_ENTRY(MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE, au32RegIchAp1REl2[3]),
    GIC_NEM_HV_PPI_STATE(0),
    GIC_NEM_HV_PPI_STATE(1),
    GIC_NEM_HV_PPI_STATE(2),
    GIC_NEM_HV_PPI_STATE(3),
    GIC_NEM_HV_PPI_STATE(4),
    GIC_NEM_HV_PPI_STATE(5),
    GIC_NEM_HV_PPI_STATE(6),
    GIC_NEM_HV_PPI_STATE(7),
    GIC_NEM_HV_PPI_STATE(8),
    GIC_NEM_HV_PPI_STATE(9),
    GIC_NEM_HV_PPI_STATE(10),
    GIC_NEM_HV_PPI_STATE(11),
    GIC_NEM_HV_PPI_STATE(12),
    GIC_NEM_HV_PPI_STATE(13),
    GIC_NEM_HV_PPI_STATE(14),
    GIC_NEM_HV_PPI_STATE(15),
    GIC_NEM_HV_PPI_STATE(16),
    GIC_NEM_HV_PPI_STATE(17),
    GIC_NEM_HV_PPI_STATE(18),
    GIC_NEM_HV_PPI_STATE(19),
    GIC_NEM_HV_PPI_STATE(20),
    GIC_NEM_HV_PPI_STATE(21),
    GIC_NEM_HV_PPI_STATE(22),
    GIC_NEM_HV_PPI_STATE(23),
    GIC_NEM_HV_PPI_STATE(24),
    GIC_NEM_HV_PPI_STATE(25),
    GIC_NEM_HV_PPI_STATE(26),
    GIC_NEM_HV_PPI_STATE(27),
    GIC_NEM_HV_PPI_STATE(28),
    GIC_NEM_HV_PPI_STATE(29),
    GIC_NEM_HV_PPI_STATE(30),
    GIC_NEM_HV_PPI_STATE(31),
    SSMFIELD_ENTRY_TERM()
};


/**
 * Common worker for gicR3HvSetSpi() and gicR3HvSetPpi().
 *
 * @returns VBox status code.
 * @param   pDevIns     The PDM Hyper-V GIC device instance.
 * @param   idCpu       The CPU ID for which the interrupt is updated (only valid for PPIs).
 * @param   fPpi        Flag whether this is a PPI or SPI.
 * @param   uIntId      The interrupt ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
DECLINLINE(int) gicR3HvSetIrq(PPDMDEVINS pDevIns, VMCPUID idCpu, bool fPpi, uint32_t uIntId, bool fAsserted)
{
    LogFlowFunc(("pDevIns=%p idCpu=%u fPpi=%RTbool uIntId=%u fAsserted=%RTbool\n",
                 pDevIns, idCpu, fPpi, uIntId, fAsserted));

    PGICHVDEV pThis = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);

    MY_WHV_INTERRUPT_CONTROL IntrCtrl;
    IntrCtrl.TargetPartition                         = 0;
    IntrCtrl.InterruptControl.InterruptType          = MY_WHV_ARM64_IINTERRUPT_TYPE_FIXED;
    IntrCtrl.InterruptControl.Reserved1              = 0;
    IntrCtrl.InterruptControl.Asserted               = fAsserted ? 1 : 0;
    IntrCtrl.InterruptControl.Retarget               = 0;
    IntrCtrl.InterruptControl.Reserved2              = 0;
    IntrCtrl.DestinationAddress                      = fPpi ? RT_BIT(idCpu) : 0; /* SGI1R_EL1 */
    IntrCtrl.RequestedVector                         = fPpi ? uIntId : uIntId;
    IntrCtrl.TargetVtl                               = 0;
    IntrCtrl.ReservedZ0                              = 0;
    IntrCtrl.ReservedZ1                              = 0;
    HRESULT hrc = WHvRequestInterrupt(pThis->hPartition, (const WHV_INTERRUPT_CONTROL *)&IntrCtrl, sizeof(IntrCtrl));
    if (SUCCEEDED(hrc))
        return VINF_SUCCESS;

    AssertFailed();
    LogFlowFunc(("WHvRequestInterrupt() failed with %Rhrc (Last=%#x/%u)\n", hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_NEM_IPE_9; /** @todo */
}


/**
 * Sets the given SPI inside the in-kernel Hyper-V GIC.
 *
 * @returns VBox status code.
 * @param   pVM         The VM instance.
 * @param   uIntId      The SPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
static DECLCALLBACK(int) gicR3HvSetSpi(PVMCC pVM, uint32_t uIntId, bool fAsserted)
{
    PGIC pGic = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);

    /* idCpu is ignored for SPI interrupts. */
    return gicR3HvSetIrq(pDevIns, 0 /*idCpu*/, false /*fPpi*/,
                         uIntId + GIC_INTID_RANGE_SPI_START, fAsserted);
}


/**
 * Sets the given PPI inside the in-kernel Hyper-V GIC.
 *
 * @returns VBox status code.
 * @param   pVCpu       The vCPU for whih the PPI state is updated.
 * @param   uIntId      The PPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
static DECLCALLBACK(int) gicR3HvSetPpi(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);

    return gicR3HvSetIrq(pDevIns, pVCpu->idCpu, true /*fPpi*/,
                         uIntId + GIC_INTID_RANGE_PPI_START, fAsserted);
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) gicR3HvSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PGICHVDEV       pThis = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);
    PVM             pVM   = PDMDevHlpGetVM(pDevIns);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    AssertReturn(pVM, VERR_INVALID_VM_HANDLE);

    LogFlowFunc(("Enter\n"));

    /*
     * Save the global interrupt state first.
     */
    /** @todo The saved state is not final because it would be great if we could have
     *        a compatible saved state format between all possible GIC variants (no idea whether this is feasible).
     */
    uint32_t cbWritten = 0;
    HRESULT hrc = WHvGetVirtualProcessorState(pThis->hPartition, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,
                                              NULL, 0, &cbWritten);
    AssertLogRelMsgReturn(hrc == WHV_E_INSUFFICIENT_BUFFER,
                          ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);

    /* Allocate a buffer to write the whole state to based on the amount of interrupts indicated. */
    uint32_t const cbState = cbWritten;
    MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE *pState = (MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE *)RTMemTmpAllocZ(cbState);
    AssertLogRelMsgReturn(pState, ("Allocating %u bytes of memory for the global interrupt state buffer failed\n", cbState),
                          VERR_NO_MEMORY);

    hrc = WHvGetVirtualProcessorState(pThis->hPartition, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,
                                      pState, cbState, &cbWritten);
    AssertLogRelMsg(SUCCEEDED(hrc),
                    ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState, %p, %u) -> %Rhrc (Last=%#x/%u)\n",
                    pVM->nem.s.hPartition, pState, cbState, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    AssertLogRelMsgReturn(cbWritten == cbState,
                          ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,) -> cbWritten=%u vs expected=%u\n",
                           pVM->nem.s.hPartition, cbWritten, cbState)
                          , VERR_NEM_GET_REGISTERS_FAILED);
    AssertLogRelMsgReturn(pState->bVersion == MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE_VERSION,
                          ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,) -> bVersion=%u vs expected=%u\n",
                           pVM->nem.s.hPartition, pState->bVersion, MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE_VERSION)
                          , VERR_NEM_GET_REGISTERS_FAILED);

    if (SUCCEEDED(hrc))
    {
        pHlp->pfnSSMPutStruct(pSSM, (const void *)pState, &g_aWHvGicGlobalState[0]);
        for (uint32_t i = 0; i < pState->cInterrupts; i++)
            pHlp->pfnSSMPutStruct(pSSM, (const void *)&pState->aSpis[i], &g_aWHvGicGlobalInterruptState[0]);
    }

    RTMemTmpFree(pState);
    if (FAILED(hrc))
        return VERR_NEM_GET_REGISTERS_FAILED;

    /*
     * Now for the local interrupt state for each vCPU.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE LocalState;

        hrc = WHvGetVirtualProcessorState(pThis->hPartition, idCpu, WHvVirtualProcessorStateTypeInterruptControllerState,
                                          &LocalState, sizeof(LocalState), &cbWritten);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeInterruptControllerState2,) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_GET_REGISTERS_FAILED);
        AssertLogRelMsgReturn(cbWritten == sizeof(LocalState),
                              ("WHvGetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeInterruptControllerState2,) -> cbWritten=%u vs expected=%u\n",
                               pVM->nem.s.hPartition, cbWritten, sizeof(LocalState))
                              , VERR_NEM_GET_REGISTERS_FAILED);
        AssertLogRelMsgReturn(LocalState.bVersion == MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE_VERSION,
                              ("WHvGetVirtualProcessorState(%p, %u, WHvVirtualProcessorStateTypeInterruptControllerState2,) -> bVersion=%u vs expected=%u\n",
                               pVM->nem.s.hPartition, idCpu, LocalState.bVersion, MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE_VERSION)
                              , VERR_NEM_GET_REGISTERS_FAILED);

        pHlp->pfnSSMPutStruct(pSSM, (const void *)&LocalState, &g_aWHvGicLocalInterruptState[0]);

        /*
         * Check that we're still good wrt restored data.
         */
        int rc = pHlp->pfnSSMHandleGetStatus(pSSM);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) gicR3HvLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PGICHVDEV       pThis = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);
    PVM             pVM   = PDMDevHlpGetVM(pDevIns);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    AssertReturn(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(uPass == SSM_PASS_FINAL, VERR_WRONG_ORDER);

    LogFlowFunc(("uVersion=%u uPass=%#x\n", uVersion, uPass));

    /* Weed out invalid versions. */
    if (uVersion != GIC_NEM_SAVED_STATE_VERSION)
    {
        LogRel(("GIC: gicR3HvLoadExec: Invalid/unrecognized saved-state version %u (%#x)\n", uVersion, uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Restore the global state.
     */
    MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE GlobalState; RT_ZERO(GlobalState);
    int rc = pHlp->pfnSSMGetStruct(pSSM, &GlobalState, &g_aWHvGicGlobalState[0]);
    AssertRCReturn(rc, rc);

    if (GlobalState.cInterrupts >= _64K) /* Interrupt IDs are 16-bit. */
        return VERR_INVALID_PARAMETER;

    /* Calculate size of the final buffer and allocate. */
    uint32_t const cbState = RT_UOFFSETOF_DYN(MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE, aSpis[GlobalState.cInterrupts]);
    MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE *pState = (MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE *)RTMemTmpAllocZ(cbState);
    AssertLogRelMsgReturn(pState, ("Allocating %u bytes of memory for the global interrupt state buffer failed\n", cbState),
                          VERR_NO_MEMORY);

    pState->bVersion                  = MY_WHV_GLOBAL_INTERRUPT_CONTROLLER_STATE_VERSION;
    pState->bGicVersion               = GlobalState.bGicVersion;
    pState->cInterrupts               = GlobalState.cInterrupts;
    pState->u64RegGicdCtrlEnableGrp1A = GlobalState.u64RegGicdCtrlEnableGrp1A;
    for (uint32_t i = 0; i < pState->cInterrupts; i++)
    {
        rc = pHlp->pfnSSMGetStruct(pSSM, &pState->aSpis[i], &g_aWHvGicGlobalInterruptState[0]);
        if (RT_FAILURE(rc))
            break;
    }
    AssertRCReturnStmt(rc, RTMemTmpFree(pState), rc);

    HRESULT hrc = WHvSetVirtualProcessorState(pThis->hPartition, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,
                                              pState, cbState);
    RTMemTmpFree(pState);
    pState = NULL;

    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvSetVirtualProcessorState(%p, WHV_ANY_VP, WHvVirtualProcessorStateTypeGlobalInterruptState,,%u) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, cbState, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_SET_REGISTERS_FAILED);

    /*
     * Restore per CPU state.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE LocalState;
        RT_ZERO(LocalState);

        rc = pHlp->pfnSSMGetStruct(pSSM, &LocalState, &g_aWHvGicLocalInterruptState[0]);
        AssertRCReturn(rc, rc);

        LocalState.bVersion = MY_WHV_LOCAL_INTERRUPT_CONTROLLER_STATE_VERSION;

        hrc = WHvSetVirtualProcessorState(pThis->hPartition, idCpu, WHvVirtualProcessorStateTypeInterruptControllerState,
                                          &LocalState, sizeof(LocalState));
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorState(%p, %u, WHvVirtualProcessorStateTypeInterruptControllerState2,) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_SET_REGISTERS_FAILED);
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) gicR3HvReset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    RT_NOREF(pVM);

    LogFlow(("GIC: gicR3HvReset\n"));
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) gicR3HvDestruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) gicR3HvConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PGICHVDEV       pThis    = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    PGIC            pGic     = VM_TO_GIC(pVM);
    Assert(iInstance == 0); NOREF(iInstance);

    /*
     * Init the data.
     */
    pGic->pDevInsR3   = pDevIns;
    pThis->pDevIns    = pDevIns;
    pThis->hPartition = pVM->nem.s.hPartition;

    /*
     * Validate GIC settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DistributorMmioBase|RedistributorMmioBase|ItsMmioBase", "");

    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register the GIC with PDM.
     */
    rc = PDMDevHlpIcRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMGicRegisterBackend(pVM, PDMGICBACKENDTYPE_HYPERV, &g_GicHvBackend);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Query the MMIO ranges.
     */
    RTGCPHYS GCPhysMmioBaseDist = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "DistributorMmioBase", &GCPhysMmioBaseDist);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DistributorMmioBase\" value"));

    RTGCPHYS GCPhysMmioBaseReDist = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "RedistributorMmioBase", &GCPhysMmioBaseReDist);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"RedistributorMmioBase\" value"));

    /*
     * Register saved state callbacks.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, GIC_NEM_SAVED_STATE_VERSION, 0 /*cbGuess*/, gicR3HvSaveExec, gicR3HvLoadExec);
    AssertRCReturn(rc, rc);

    gicR3HvReset(pDevIns);
    return VINF_SUCCESS;
}


/**
 * GIC device registration structure.
 */
const PDMDEVREG g_DeviceGICNem =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "gic-nem",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(GICHVDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic Interrupt Controller",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           gicR3HvConstruct,
    /* .pfnDestruct = */            gicR3HvDestruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               gicR3HvReset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

/**
 * The Hyper-V GIC backend.
 */
const PDMGICBACKEND g_GicHvBackend =
{
    /* .pfnReadSysReg = */  NULL,
    /* .pfnWriteSysReg = */ NULL,
    /* .pfnSetSpi = */      gicR3HvSetSpi,
    /* .pfnSetPpi = */      gicR3HvSetPpi,
    /* .pfnSendMsi = */     NULL,
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

