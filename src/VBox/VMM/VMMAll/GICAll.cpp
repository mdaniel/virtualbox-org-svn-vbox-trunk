/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GIC) - All Contexts.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include "GICInternal.h"
#include <VBox/vmm/pdmgic.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vmcpuset.h>
#ifdef IN_RING0
# include <VBox/vmm/gvmm.h>
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define GIC_IDLE_PRIORITY                   0xff
#define GIC_IS_INTR_SGI(a_uIntId)           (a_uIntId - GIC_INTID_RANGE_SGI_START < GIC_INTID_SGI_RANGE_SIZE)
#define GIC_IS_INTR_PPI(a_uIntId)           (a_uIntId - GIC_INTID_RANGE_PPI_START < GIC_INTID_PPI_RANGE_SIZE)
#define GIC_IS_INTR_SGI_OR_PPI(a_uIntId)    (a_uIntId - GIC_INTID_RANGE_SGI_START < GIC_INTID_PPI_RANGE_SIZE)
#define GIC_IS_INTR_SPI(a_uIntId)           (a_uIntId - GIC_INTID_RANGE_SPI_START < GIC_INTID_SPI_RANGE_SIZE)
#define GIC_IS_INTR_SPECIAL(a_uIntId)       (a_uIntId - GIC_INTID_RANGE_SPECIAL_START < GIC_INTID_EXT_PPI_RANGE_SIZE)
#define GIC_IS_INTR_EXT_PPI(a_uIntId)       (a_uIntId - GIC_INTID_RANGE_EXT_PPI_START < GIC_INTID_EXT_PPI_RANGE_SIZE)
#define GIC_IS_INTR_EXT_SPI(a_uIntId)       (a_uIntId - GIC_INTID_RANGE_EXT_SPI_START < GIC_INTID_EXT_SPI_RANGE_SIZE)


#ifdef LOG_ENABLED
/**
 * Gets the description of a CPU interface register.
 *
 * @returns The description.
 * @param   u32Reg  The CPU interface register offset.
 */
static const char *gicIccGetRegDescription(uint32_t u32Reg)
{
    switch (u32Reg)
    {
#define GIC_ICC_REG_CASE(a_Reg) case ARMV8_AARCH64_SYSREG_ ## a_Reg: return #a_Reg
        GIC_ICC_REG_CASE(ICC_PMR_EL1);
        GIC_ICC_REG_CASE(ICC_IAR0_EL1);
        GIC_ICC_REG_CASE(ICC_EOIR0_EL1);
        GIC_ICC_REG_CASE(ICC_HPPIR0_EL1);
        GIC_ICC_REG_CASE(ICC_BPR0_EL1);
        GIC_ICC_REG_CASE(ICC_AP0R0_EL1);
        GIC_ICC_REG_CASE(ICC_AP0R1_EL1);
        GIC_ICC_REG_CASE(ICC_AP0R2_EL1);
        GIC_ICC_REG_CASE(ICC_AP0R3_EL1);
        GIC_ICC_REG_CASE(ICC_AP1R0_EL1);
        GIC_ICC_REG_CASE(ICC_AP1R1_EL1);
        GIC_ICC_REG_CASE(ICC_AP1R2_EL1);
        GIC_ICC_REG_CASE(ICC_AP1R3_EL1);
        GIC_ICC_REG_CASE(ICC_DIR_EL1);
        GIC_ICC_REG_CASE(ICC_RPR_EL1);
        GIC_ICC_REG_CASE(ICC_SGI1R_EL1);
        GIC_ICC_REG_CASE(ICC_ASGI1R_EL1);
        GIC_ICC_REG_CASE(ICC_SGI0R_EL1);
        GIC_ICC_REG_CASE(ICC_IAR1_EL1);
        GIC_ICC_REG_CASE(ICC_EOIR1_EL1);
        GIC_ICC_REG_CASE(ICC_HPPIR1_EL1);
        GIC_ICC_REG_CASE(ICC_BPR1_EL1);
        GIC_ICC_REG_CASE(ICC_CTLR_EL1);
        GIC_ICC_REG_CASE(ICC_SRE_EL1);
        GIC_ICC_REG_CASE(ICC_IGRPEN0_EL1);
        GIC_ICC_REG_CASE(ICC_IGRPEN1_EL1);
#undef GIC_ICC_REG_CASE
        default:
            break;
    }

    return "<UNKNOWN>";
}


/**
 * Gets the description of a distributor register given it's register offset.
 *
 * @returns The register description.
 * @param   offReg  The distributor register offset.
 */
static const char *gicDistGetRegDescription(uint16_t offReg)
{
   if (offReg - GIC_DIST_REG_IGROUPRn_OFF_START     < GIC_DIST_REG_IGROUPRn_RANGE_SIZE)     return "GICD_IGROUPRn";
   if (offReg - GIC_DIST_REG_IGROUPRnE_OFF_START    < GIC_DIST_REG_IGROUPRnE_RANGE_SIZE)    return "GICD_IGROUPRnE";
   if (offReg - GIC_DIST_REG_IROUTERn_OFF_START     < GIC_DIST_REG_IROUTERn_RANGE_SIZE)     return "GICD_IROUTERn";
   if (offReg - GIC_DIST_REG_IROUTERnE_OFF_START    < GIC_DIST_REG_IROUTERnE_RANGE_SIZE)    return "GICD_IROUTERnE";
   if (offReg - GIC_DIST_REG_ISENABLERn_OFF_START   < GIC_DIST_REG_ISENABLERn_RANGE_SIZE)   return "GICD_ISENABLERn";
   if (offReg - GIC_DIST_REG_ISENABLERnE_OFF_START  < GIC_DIST_REG_ISENABLERnE_RANGE_SIZE)  return "GICD_ISENABLERnE";
   if (offReg - GIC_DIST_REG_ICENABLERn_OFF_START   < GIC_DIST_REG_ICENABLERn_RANGE_SIZE)   return "GICD_ICENABLERn";
   if (offReg - GIC_DIST_REG_ICENABLERnE_OFF_START  < GIC_DIST_REG_ICENABLERnE_RANGE_SIZE)  return "GICD_ICENABLERnE";
   if (offReg - GIC_DIST_REG_ISACTIVERn_OFF_START   < GIC_DIST_REG_ISACTIVERn_RANGE_SIZE)   return "GICD_ISACTIVERn";
   if (offReg - GIC_DIST_REG_ISACTIVERnE_OFF_START  < GIC_DIST_REG_ISACTIVERnE_RANGE_SIZE)  return "GICD_ISACTIVERnE";
   if (offReg - GIC_DIST_REG_ICACTIVERn_OFF_START   < GIC_DIST_REG_ICACTIVERn_RANGE_SIZE)   return "GICD_ICACTIVERn";
   if (offReg - GIC_DIST_REG_ICACTIVERnE_OFF_START  < GIC_DIST_REG_ICACTIVERnE_RANGE_SIZE)  return "GICD_ICACTIVERnE";
   if (offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START  < GIC_DIST_REG_IPRIORITYRn_RANGE_SIZE)  return "GICD_IPRIORITYRn";
   if (offReg - GIC_DIST_REG_IPRIORITYRnE_OFF_START < GIC_DIST_REG_IPRIORITYRnE_RANGE_SIZE) return "GICD_IPRIORITYRnE";
   if (offReg - GIC_DIST_REG_ISPENDRn_OFF_START     < GIC_DIST_REG_ISPENDRn_RANGE_SIZE)     return "GICD_ISPENDRn";
   if (offReg - GIC_DIST_REG_ISPENDRnE_OFF_START    < GIC_DIST_REG_ISPENDRnE_RANGE_SIZE)    return "GICD_ISPENDRnE";
   if (offReg - GIC_DIST_REG_ICPENDRn_OFF_START     < GIC_DIST_REG_ICPENDRn_RANGE_SIZE)     return "GICD_ICPENDRn";
   if (offReg - GIC_DIST_REG_ICPENDRnE_OFF_START    < GIC_DIST_REG_ICPENDRnE_RANGE_SIZE)    return "GICD_ICPENDRnE";
   if (offReg - GIC_DIST_REG_ICFGRn_OFF_START       < GIC_DIST_REG_ICFGRn_RANGE_SIZE)       return "GICD_ICFGRn";
   if (offReg - GIC_DIST_REG_ICFGRnE_OFF_START      < GIC_DIST_REG_ICFGRnE_RANGE_SIZE)      return "GICD_ICFGRnE";
   switch (offReg)
   {
       case GIC_DIST_REG_CTLR_OFF:             return "GICD_CTLR";
       case GIC_DIST_REG_TYPER_OFF:            return "GICD_TYPER";
       case GIC_DIST_REG_STATUSR_OFF:          return "GICD_STATUSR";
       case GIC_DIST_REG_ITARGETSRn_OFF_START: return "GICD_ITARGETSRn";
       case GIC_DIST_REG_IGRPMODRn_OFF_START:  return "GICD_IGRPMODRn";
       case GIC_DIST_REG_NSACRn_OFF_START:     return "GICD_NSACRn";
       case GIC_DIST_REG_SGIR_OFF:             return "GICD_SGIR";
       case GIC_DIST_REG_CPENDSGIRn_OFF_START: return "GICD_CSPENDSGIRn";
       case GIC_DIST_REG_SPENDSGIRn_OFF_START: return "GICD_SPENDSGIRn";
       case GIC_DIST_REG_INMIn_OFF_START:      return "GICD_INMIn";
       case GIC_DIST_REG_PIDR2_OFF:            return "GICD_PIDR2";
       case GIC_DIST_REG_IIDR_OFF:             return "GICD_IIDR";
       case GIC_DIST_REG_TYPER2_OFF:           return "GICD_TYPER2";
       default:
           return "<UNKNOWN>";
   }
}


/**
 * Gets the description of a redistributor register given it's register offset.
 *
 * @returns The register description.
 * @param   offReg  The redistributor register offset.
 */
static const char *gicReDistGetRegDescription(uint16_t offReg)
{
    if (offReg - GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF          < GIC_REDIST_SGI_PPI_REG_IGROUPRnE_RANGE_SIZE)    return "GICR_IGROUPn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF        < GIC_REDIST_SGI_PPI_REG_ISENABLERnE_RANGE_SIZE)  return "GICR_ISENABLERn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF        < GIC_REDIST_SGI_PPI_REG_ICENABLERnE_RANGE_SIZE)  return "GICR_ICENABLERn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF        < GIC_REDIST_SGI_PPI_REG_ISACTIVERnE_RANGE_SIZE)  return "GICR_ISACTIVERn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF        < GIC_REDIST_SGI_PPI_REG_ICACTIVERnE_RANGE_SIZE)  return "GICR_ICACTIVERn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF          < GIC_REDIST_SGI_PPI_REG_ISPENDRnE_RANGE_SIZE)    return "GICR_ISPENDRn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF          < GIC_REDIST_SGI_PPI_REG_ICPENDRnE_RANGE_SIZE)    return "GICR_ICPENDRn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START < GIC_REDIST_SGI_PPI_REG_IPRIORITYRnE_RANGE_SIZE) return "GICR_IPREIORITYn";
    if (offReg - GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF            < GIC_REDIST_SGI_PPI_REG_ICFGRnE_RANGE_SIZE)      return "GICR_ICFGRn";
    switch (offReg)
    {
        case GIC_REDIST_REG_TYPER_OFF:          return "GICR_TYPER";
        case GIC_REDIST_REG_IIDR_OFF:           return "GICR_IIDR";
        case GIC_REDIST_REG_TYPER_AFFINITY_OFF: return "GICR_TYPER_AFF";
        case GIC_REDIST_REG_PIDR2_OFF:          return "GICR_PIDR2";
        default:
            return "<UNKNOWN>";
    }
}
#endif


/**
 * Gets the interrupt ID given a distributor interrupt index.
 *
 * @returns The interrupt ID.
 * @param   idxIntr     The distributor interrupt index.
 * @remarks A distributor interrupt is an interrupt type that belong in the
 *          distributor (e.g. SPIs, extended SPIs).
 */
DECLHIDDEN(uint16_t) gicDistGetIntIdFromIndex(uint16_t idxIntr)
{
    /*
     * Distributor interrupts bits to interrupt ID mapping:
     * +--------------------------------------------------------+
     * | Range (incl) | SGI    | PPI    | SPI      | Ext SPI    |
     * |--------------+--------+--------+----------+------------|
     * | Bit          | 0..15  | 16..31 | 32..1023 | 1024..2047 |
     * | Int Id       | 0..15  | 16..31 | 32..1023 | 4096..5119 |
     * +--------------------------------------------------------+
     */
    uint16_t uIntId;
    /* SGIs, PPIs, SPIs and specials. */
    if (idxIntr < 1024)
        uIntId = idxIntr;
    /* Extended SPIs. */
    else if (idxIntr < 2048)
        uIntId = GIC_INTID_RANGE_EXT_SPI_START + idxIntr - 1024;
    else
    {
        uIntId = 0;
        AssertReleaseFailed();
    }
    Assert(   GIC_IS_INTR_SGI_OR_PPI(uIntId)
           || GIC_IS_INTR_SPI(uIntId)
           || GIC_IS_INTR_SPECIAL(uIntId)
           || GIC_IS_INTR_EXT_SPI(uIntId));
    return uIntId;
}


/**
 * Gets the distributor interrupt index given an interrupt ID.
 *
 * @returns The distributor interrupt index.
 * @param   uIntId  The interrupt ID.
 * @remarks A distributor interrupt is an interrupt type that belong in the
 *          distributor (e.g. SPIs, extended SPIs).
 */
static uint16_t gicDistGetIndexFromIntId(uint16_t uIntId)
{
    uint16_t idxIntr;
    /* SGIs, PPIs, SPIs and specials. */
    if (uIntId <= GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT)
        idxIntr = uIntId;
    /* Extended SPIs. */
    else if (uIntId - GIC_INTID_RANGE_EXT_SPI_START < GIC_INTID_EXT_SPI_RANGE_SIZE)
        idxIntr = 1024 + uIntId - GIC_INTID_RANGE_EXT_SPI_START;
    else
    {
        idxIntr = 0;
        AssertReleaseFailed();
    }
    Assert(idxIntr < sizeof(GICDEV::bmIntrPending) * 8);
    return idxIntr;
}


/**
 * Gets the interrupt ID given a redistributor interrupt index.
 *
 * @returns The interrupt ID.
 * @param   idxIntr     The redistributor interrupt index.
 * @remarks A redistributor interrupt is an interrupt type that belong in the
 *          redistributor (e.g. SGIs, PPIs, extended PPIs).
 */
DECLHIDDEN(uint16_t) gicReDistGetIntIdFromIndex(uint16_t idxIntr)
{
    /*
     * Redistributor interrupts bits to interrupt ID mapping:
     * +---------------------------------------------+
     * | Range (incl) | SGI    | PPI    | Ext PPI    |
     * +---------------------------------------------+
     * | Bit          | 0..15  | 16..31 |   32..95   |
     * | Int Id       | 0..15  | 16..31 | 1056..1119 |
     * +---------------------------------------------+
     */
    uint16_t uIntId;
    /* SGIs and PPIs. */
    if (idxIntr < 32)
        uIntId = idxIntr;
    /* Extended PPIs. */
    else if (idxIntr < 96)
        uIntId = GIC_INTID_RANGE_EXT_PPI_START + idxIntr - 32;
    else
    {
        uIntId = 0;
        AssertReleaseFailed();
    }
    Assert(GIC_IS_INTR_SGI_OR_PPI(uIntId) || GIC_IS_INTR_EXT_PPI(uIntId));
    return uIntId;
}


/**
 * Gets the redistributor interrupt index given an interrupt ID.
 *
 * @returns The interrupt ID.
 * @param   uIntId      The interrupt ID.
 * @remarks A redistributor interrupt is an interrupt type that belong in the
 *          redistributor (e.g. SGIs, PPIs, extended PPIs).
 */
static uint16_t gicReDistGetIndexFromIntId(uint16_t uIntId)
{
    /* SGIs and PPIs. */
    uint16_t idxIntr;
    if (uIntId <= GIC_INTID_RANGE_PPI_LAST)
        idxIntr = uIntId;
    /* Extended PPIs. */
    else if (uIntId - GIC_INTID_RANGE_EXT_PPI_START < GIC_INTID_EXT_PPI_RANGE_SIZE)
        idxIntr = 32 + uIntId - GIC_INTID_RANGE_EXT_PPI_START;
    else
    {
        idxIntr = 0;
        AssertReleaseFailed();
    }
    Assert(idxIntr < sizeof(GICCPU::bmIntrPending) * 8);
    return idxIntr;
}


/**
 * Sets the interrupt pending force-flag and pokes the EMT if required.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fIrq            Flag whether to assert the IRQ line or leave it alone.
 * @param   fFiq            Flag whether to assert the FIQ line or leave it alone.
 */
static void gicSetInterruptFF(PVMCPUCC pVCpu, bool fIrq, bool fFiq)
{
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} fIrq=%RTbool fFiq=%RTbool\n",
                 pVCpu, pVCpu->idCpu, fIrq, fFiq));

    Assert(fIrq || fFiq);

#ifdef IN_RING3
    /* IRQ state should be loaded as-is by "LoadExec". Changes can be made from LoadDone. */
    Assert(pVCpu->pVMR3->enmVMState != VMSTATE_LOADING || PDMR3HasLoadedState(pVCpu->pVMR3));
#endif

    if (fIrq)
        VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ);
    if (fFiq)
        VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_FIQ);

    /*
     * We need to wake up the target CPU if we're not on EMT.
     */
    /** @todo We could just use RTThreadNativeSelf() here, couldn't we? */
#if defined(IN_RING0)
# error "Implement me!"
#elif defined(IN_RING3)
    PVMCC   pVM   = pVCpu->CTX_SUFF(pVM);
    VMCPUID idCpu = pVCpu->idCpu;
    if (VMMGetCpuId(pVM) != idCpu)
    {
        Log7Func(("idCpu=%u enmState=%d\n", idCpu, pVCpu->enmState));
        VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_POKE);
    }
#endif
}


#if 0
/**
 * Sets the update interrupt force-flag and pokes the EMT if required.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
static void gicSetUpdateInterruptFF(PVMCPUCC pVCpu)
{
#ifdef IN_RING3
    /* IRQ state should be loaded as-is by "LoadExec". Changes can be made from LoadDone. */
    Assert(pVCpu->pVMR3->enmVMState != VMSTATE_LOADING || PDMR3HasLoadedState(pVCpu->pVMR3));
#endif

    /* Set the update-interrupt force-flag. */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_UPDATE_GIC);

    /* Wake up the target CPU if we're not currently on the target EMT. */
#if defined(IN_RING0)
# error "Implement me"
#elif defined(IN_RING3)
    if (pVCpu->hNativeThread != RTThreadNativeSelf())
        VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_POKE);
#endif
}
#endif


/**
 * Clears the interrupt pending force-flag.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fIrq    Flag whether to clear the IRQ flag.
 * @param   fFiq    Flag whether to clear the FIQ flag.
 */
DECLINLINE(void) gicClearInterruptFF(PVMCPUCC pVCpu, bool fIrq, bool fFiq)
{
    Assert(fIrq || fFiq);
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} fIrq=%RTbool fFiq=%RTbool\n", pVCpu, pVCpu->idCpu, fIrq, fFiq));

#ifdef IN_RING3
    /* IRQ state should be loaded as-is by "LoadExec". Changes can be made from LoadDone. */
    Assert(pVCpu->pVMR3->enmVMState != VMSTATE_LOADING || PDMR3HasLoadedState(pVCpu->pVMR3));
#endif

    if (fIrq)
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_IRQ);
    if (fFiq)
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_FIQ);
}


/**
 * Updates the interrupt force-flag.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fIrq    Flag whether to clear the IRQ flag.
 * @param   fFiq    Flag whether to clear the FIQ flag.
 */
DECLINLINE(void) gicUpdateInterruptFF(PVMCPUCC pVCpu, bool fIrq, bool fFiq)
{
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} fIrq=%RTbool fFiq=%RTbool\n", pVCpu, pVCpu->idCpu, fIrq, fFiq));

    if (fIrq || fFiq)
        gicSetInterruptFF(pVCpu, fIrq, fFiq);

    if (!fIrq || !fFiq)
        gicClearInterruptFF(pVCpu, !fIrq, !fFiq);
}


/**
 * Gets whether the redistributor has pending interrupts with sufficient priority to
 * be signalled to the PE.
 *
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   pfIrq       Where to store whether IRQs can be signalled.
 * @param   pfFiq       Where to store whether FIQs can be signalled.
 */
DECLINLINE(void) gicReDistHasIrqPending(PCGICCPU pGicCpu, bool *pfIrq, bool *pfFiq)
{
    LogFlowFunc(("\n"));
#if 0
    /* Read the interrupt state. */
    uint32_t u32RegIGrp0  = ASMAtomicReadU32(&pThis->u32RegIGrp0);
    uint32_t bmIntEnabled = ASMAtomicReadU32(&pThis->bmIntEnabled);
    uint32_t bmIntPending = ASMAtomicReadU32(&pThis->bmIntPending);
    uint32_t bmIntActive  = ASMAtomicReadU32(&pThis->bmIntActive);
    bool fIrqGrp0Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp0Enabled);
    bool fIrqGrp1Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp1Enabled);

    /* Only allow interrupts with higher priority than the current configured and running one. */
    uint8_t bPriority = RT_MIN(pThis->bInterruptPriority, pThis->abRunningPriorities[pThis->idxRunningPriority]);

    /* Is anything enabled at all? */
    uint32_t bmIntForward = (bmIntPending & bmIntEnabled) & ~bmIntActive; /* Exclude the currently active interrupt. */
    if (bmIntForward)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(pThis->abIntPriority); i++)
        {
            Log4(("SGI/PPI %u, configured priority %u, running priority %u\n", i, pThis->abIntPriority[i], bPriority));
            if (   (bmIntForward & RT_BIT_32(i))
                && pThis->abIntPriority[i] < bPriority)
                break;
            else
                bmIntForward &= ~RT_BIT_32(i);

            if (!bmIntForward)
                break;
        }
    }

    if (bmIntForward)
    {
        /* Determine whether we have to assert the IRQ or FIQ line. */
        *pfIrq = RT_BOOL(bmIntForward & u32RegIGrp0) && fIrqGrp1Enabled;
        *pfFiq = RT_BOOL(bmIntForward & ~u32RegIGrp0) && fIrqGrp0Enabled;
    }
    else
    {
        *pfIrq = false;
        *pfFiq = false;
    }

    LogFlowFunc(("pThis=%p bPriority=%u bmIntEnabled=%#x bmIntPending=%#x bmIntActive=%#x fIrq=%RTbool fFiq=%RTbool\n",
                 pThis, bPriority, bmIntEnabled, bmIntPending, bmIntActive, *pfIrq, *pfFiq));
#else
    bool const fIsGroup1Enabled = pGicCpu->fIntrGroup1Enabled;
    bool const fIsGroup0Enabled = pGicCpu->fIntrGroup0Enabled;
    LogFlowFunc(("fIsGroup0Enabled=%RTbool fIsGroup1Enabled=%RTbool\n", fIsGroup0Enabled, fIsGroup1Enabled));

    uint32_t bmIntrs[3];
    for (uint8_t i = 0; i < RT_ELEMENTS(bmIntrs); i++)
    {
        /* Collect interrupts that are pending, enabled and inactive. */
        bmIntrs[i] = (pGicCpu->bmIntrPending[i] & pGicCpu->bmIntrEnabled[i]) & ~pGicCpu->bmIntrActive[i];

        /* Discard interrupts if the group they belong to is disabled. */
        if (!fIsGroup1Enabled)
            bmIntrs[i] &= ~pGicCpu->bmIntrGroup[i];
        if (!fIsGroup0Enabled)
            bmIntrs[i] &= pGicCpu->bmIntrGroup[i];
    }

    /* Only allow interrupts with higher priority than the current configured and running one. */
    uint8_t const bPriority = RT_MIN(pGicCpu->bIntrPriorityMask, pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority]);

    uint32_t const cIntrs  = sizeof(bmIntrs) * 8;
    int32_t        idxIntr = ASMBitFirstSet(&bmIntrs[0], cIntrs);
    AssertCompile(!(cIntrs % 32));
    if (idxIntr >= 0)
    {
        do
        {
            Assert((uint32_t)idxIntr < RT_ELEMENTS(pGicCpu->abIntrPriority));
            if (pGicCpu->abIntrPriority[idxIntr] < bPriority)
            {
                bool const fInGroup1 = ASMBitTest(&pGicCpu->bmIntrGroup[0], idxIntr);
                bool const fInGroup0 = !fInGroup1;
                *pfIrq = fInGroup1 && fIsGroup1Enabled;
                *pfFiq = fInGroup0 && fIsGroup0Enabled;
                return;
            }
            idxIntr = ASMBitNextSet(&bmIntrs[0], cIntrs, idxIntr);
        } while (idxIntr != -1);
    }
    *pfIrq = false;
    *pfFiq = false;
#endif
}


/**
 * Gets whether the distributor has pending interrupts with sufficient priority to
 * be signalled to the PE.
 *
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idCpu       The ID of the virtual CPU.
 * @param   pfIrq       Where to store whether there are IRQs can be signalled.
 * @param   pfFiq       Where to store whether there are FIQs can be signalled.
 */
DECLINLINE(void) gicDistHasIrqPendingForVCpu(PCGICDEV pGicDev, PCVMCPUCC pVCpu, VMCPUID idCpu, bool *pfIrq, bool *pfFiq)
{
    LogFlowFunc(("\n"));
#if 0
    /* Read the interrupt state. */
    uint32_t u32RegIGrp0  = ASMAtomicReadU32(&pThis->u32RegIGrp0);
    uint32_t bmIntEnabled = ASMAtomicReadU32(&pThis->bmIntEnabled);
    uint32_t bmIntPending = ASMAtomicReadU32(&pThis->bmIntPending);
    uint32_t bmIntActive  = ASMAtomicReadU32(&pThis->bmIntActive);
    bool fIrqGrp0Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp0Enabled);
    bool fIrqGrp1Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp1Enabled);

    /* Only allow interrupts with higher priority than the current configured and running one. */
    uint8_t bPriority = RT_MIN(pGicVCpu->bInterruptPriority, pGicVCpu->abRunningPriorities[pGicVCpu->idxRunningPriority]);

    /* Is anything enabled at all? */
    uint32_t bmIntForward = (bmIntPending & bmIntEnabled) & ~bmIntActive; /* Exclude the currently active interrupt. */
    if (bmIntForward)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(pThis->abIntPriority); i++)
        {
            Log4(("SPI %u, configured priority %u (routing %#x), running priority %u\n", i + GIC_INTID_RANGE_SPI_START, pThis->abIntPriority[i],
                                                                                         pThis->au32IntRouting[i], bPriority));
            if (   (bmIntForward & RT_BIT_32(i))
                && pThis->abIntPriority[i] < bPriority
                && pThis->au32IntRouting[i] == idCpu)
                break;
            else
                bmIntForward &= ~RT_BIT_32(i);

            if (!bmIntForward)
                break;
        }
    }

    if (bmIntForward)
    {
        /* Determine whether we have to assert the IRQ or FIQ line. */
        *pfIrq = RT_BOOL(bmIntForward & u32RegIGrp0) && fIrqGrp1Enabled;
        *pfFiq = RT_BOOL(bmIntForward & ~u32RegIGrp0) && fIrqGrp0Enabled;
    }
    else
    {
        *pfIrq = false;
        *pfFiq = false;
    }

    LogFlowFunc(("pThis=%p bPriority=%u bmIntEnabled=%#x bmIntPending=%#x bmIntActive=%#x fIrq=%RTbool fFiq=%RTbool\n",
                 pThis, bPriority, bmIntEnabled, bmIntPending, bmIntActive, *pfIrq, *pfFiq));
#else
    bool const fIsGroup1Enabled = pGicDev->fIntrGroup1Enabled;
    bool const fIsGroup0Enabled = pGicDev->fIntrGroup0Enabled;
    LogFlowFunc(("fIsGroup1Enabled=%RTbool fIsGroup0Enabled=%RTbool\n", fIsGroup1Enabled, fIsGroup0Enabled));

    uint32_t bmIntrs[64];
    for (uint8_t i = 0; i < RT_ELEMENTS(bmIntrs); i++)
    {
        /* Collect interrupts that are pending, enabled and inactive. */
        bmIntrs[i] = (pGicDev->bmIntrPending[i] & pGicDev->bmIntrEnabled[i]) & ~pGicDev->bmIntrActive[i];

        /* Discard interrupts if the group they belong to is disabled. */
        if (!fIsGroup1Enabled)
            bmIntrs[i] &= ~pGicDev->bmIntrGroup[i];
        if (!fIsGroup0Enabled)
            bmIntrs[i] &= pGicDev->bmIntrGroup[i];
    }

    /* Only allow interrupts with higher priority than the current configured and running one. */
    PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    uint8_t const bPriority = RT_MIN(pGicCpu->bIntrPriorityMask, pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority]);

    /*
     * The distributor's interrupt pending/enabled/active bitmaps have 2048 bits which map
     * SGIs (16), PPIs (16), SPIs (988), reserved SPIs (4) and extended SPIs (1024).
     * Of these, the first 16 bits corresponding to SGIs and PPIs are RAZ/WI when affinity
     * routing is enabled (which it always is in our implementation).
     */
    Assert(pGicDev->fAffRoutingEnabled);
    uint32_t const cIntrs  = sizeof(bmIntrs) * 8;
    int32_t        idxIntr = ASMBitFirstSet(&bmIntrs[0], cIntrs);
    AssertCompile(!(cIntrs % 32));
    Assert(bmIntrs[0] == 0);
    if (idxIntr >= 0)
    {
        Assert(idxIntr > GIC_INTID_RANGE_PPI_LAST);
        do
        {
            AssertCompile(RT_ELEMENTS(pGicDev->abIntrPriority) == RT_ELEMENTS(pGicDev->au32IntrRouting));
            Assert((uint32_t)idxIntr < RT_ELEMENTS(pGicDev->abIntrPriority));
            Assert(idxIntr < GIC_INTID_RANGE_SPECIAL_START || idxIntr > GIC_INTID_RANGE_SPECIAL_LAST);
            if (   pGicDev->abIntrPriority[idxIntr] < bPriority
                && pGicDev->au32IntrRouting[idxIntr] == idCpu)
            {
                bool const fInGroup1 = ASMBitTest(&pGicDev->bmIntrGroup[0], idxIntr);
                bool const fInGroup0 = !fInGroup1;
                *pfFiq = fInGroup0 && fIsGroup0Enabled;
                *pfIrq = fInGroup1 && fIsGroup1Enabled;
                return;
            }
            idxIntr = ASMBitNextSet(&bmIntrs[0], cIntrs, idxIntr);
        } while (idxIntr != -1);
    }
    *pfIrq = false;
    *pfFiq = false;
#endif
}


/**
 * Updates the internal IRQ state and sets or clears the appropriate force action
 * flags.
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static VBOXSTRICTRC gicReDistUpdateIrqState(PCGICDEV pGicDev, PVMCPUCC pVCpu)
{
    LogFlowFunc(("\n"));
    bool fIrq;
    bool fFiq;
    gicReDistHasIrqPending(VMCPU_TO_GICCPU(pVCpu), &fIrq, &fFiq);
    LogFlowFunc(("fIrq=%RTbool fFiq=%RTbool\n", fIrq, fFiq));

    bool fIrqDist;
    bool fFiqDist;
    gicDistHasIrqPendingForVCpu(pGicDev, pVCpu, pVCpu->idCpu, &fIrqDist, &fFiqDist);
    LogFlowFunc(("fIrqDist=%RTbool fFiqDist=%RTbool\n", fIrqDist, fFiqDist));

    fIrq |= fIrqDist;
    fFiq |= fFiqDist;
    gicUpdateInterruptFF(pVCpu, fIrq, fFiq);
    return VINF_SUCCESS;
}


/**
 * Updates the internal IRQ state of the distributor and sets or clears the appropirate force action flags.
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM state.
 * @param   pGicDev     The GIC distributor state.
 */
static VBOXSTRICTRC gicDistUpdateIrqState(PCVMCC pVM, PCGICDEV pGicDev)
{
    LogFlowFunc(("\n"));
    for (uint32_t i = 0; i < pVM->cCpus; i++)
    {
        PVMCPUCC pVCpu   = pVM->CTX_SUFF(apCpus)[i];
        PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);

        bool fIrq, fFiq;
        gicReDistHasIrqPending(pGicCpu, &fIrq, &fFiq);

        bool fIrqDist, fFiqDist;
        gicDistHasIrqPendingForVCpu(pGicDev, pVCpu, i, &fIrqDist, &fFiqDist);
        fIrq |= fIrqDist;
        fFiq |= fFiqDist;

        gicUpdateInterruptFF(pVCpu, fIrq, fFiq);
    }
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt routing register (GICD_IROUTER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_IROUTER range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrRoutingReg(PCGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is disabled, reads return 0. */
    Assert(pGicDev->fAffRoutingEnabled);

    /* Hardware does not map the first 32 registers (corresponding to SGIs and PPIs). */
    idxReg += GIC_INTID_RANGE_SPI_START;
    AssertReturn(idxReg < RT_ELEMENTS(pGicDev->au32IntrRouting), VERR_BUFFER_OVERFLOW);
    Assert(idxReg < sizeof(pGicDev->bmIntrRoutingMode) * 8);
    if (!(idxReg % 2))
    {
        /* Lower 32-bits. */
        uint8_t const fIrm = ASMBitTest(&pGicDev->bmIntrRoutingMode[0], idxReg);
        *puValue = GIC_DIST_REG_IROUTERn_SET(fIrm, pGicDev->au32IntrRouting[idxReg]);
    }
    else
    {
        /* Upper 32-bits. */
        *puValue = pGicDev->au32IntrRouting[idxReg] >> 24;
    }

    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, *puValue));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt routing register (GICD_IROUTER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   offReg      The offset of the register in the distributor register map.
 * @param   idxReg      The index of the register in the GICD_IROUTER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrRoutingReg(PGICDEV pGicDev, uint16_t offReg, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);

    /* Hardware does not map the first 32 registers (corresponding to SGIs and PPIs). */
    idxReg += GIC_INTID_RANGE_SPI_START;
    AssertReturn(idxReg < RT_ELEMENTS(pGicDev->au32IntrRouting), VERR_BUFFER_OVERFLOW);
    Assert(idxReg < sizeof(pGicDev->bmIntrRoutingMode) * 8);
    if (!(offReg & 4))
    {
        /* Lower 32-bits. */
        bool const fIrm = GIC_DIST_REG_IROUTERn_IRM_GET(uValue);
        if (fIrm)
            ASMBitSet(&pGicDev->bmIntrRoutingMode[0], idxReg);
        else
            ASMBitClear(&pGicDev->bmIntrRoutingMode[0], idxReg);
        uint32_t const fAff3 = pGicDev->au32IntrRouting[idxReg] & 0xff000000;
        pGicDev->au32IntrRouting[idxReg] = fAff3 | (uValue & 0x00ffffff);
    }
    else
    {
        /* Upper 32-bits. */
        uint32_t const fAffOthers = pGicDev->au32IntrRouting[idxReg] & 0x00ffffff;
        pGicDev->au32IntrRouting[idxReg] = (uValue << 24) | fAffOthers;
    }

    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->au32IntrRouting[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt (set/clear) enable register (GICD_ISENABLER and
 * GICD_ICENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISENABLER and
 *                      GICD_ICENABLER range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrEnableReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrEnabled));
    *puValue = pGicDev->bmIntrEnabled[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicDev->bmIntrEnabled[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt set-enable register (GICD_ISENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISENABLER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrSetEnableReg(PVM pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrEnabled));
        pGicDev->bmIntrEnabled[idxReg] |= uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrEnabled[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt clear-enable register (GICD_ICENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICENABLER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrClearEnableReg(PVM pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrEnabled));
        pGicDev->bmIntrEnabled[idxReg] &= ~uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrEnabled[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt active register (GICD_ISACTIVER and
 * GICD_ICACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISACTIVER and
 *                      GICD_ICACTIVER range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrActiveReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrActive));
    *puValue = pGicDev->bmIntrActive[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicDev->bmIntrActive[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt set-active register (GICD_ISACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISACTIVER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrSetActiveReg(PVM pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrActive));
        pGicDev->bmIntrActive[idxReg] |= uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrActive[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt clear-active register (GICD_ICACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICACTIVER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrClearActiveReg(PVM pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrActive));
        pGicDev->bmIntrActive[idxReg] &= ~uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrActive[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt priority register (GICD_IPRIORITYR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_IPRIORITY range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrPriorityReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is enabled, reads to registers 0..7 (pertaining to SGIs and PPIs) return 0. */
    Assert(pGicDev->fAffRoutingEnabled);
    Assert(idxReg < RT_ELEMENTS(pGicDev->abIntrPriority) / sizeof(uint32_t));
    Assert(idxReg != 255);
    if (idxReg > 7)
    {
        uint16_t const idxPriority = idxReg * sizeof(uint32_t);
        AssertReturn(idxPriority < RT_ELEMENTS(pGicDev->abIntrPriority) - sizeof(uint32_t), VERR_BUFFER_OVERFLOW);
        AssertCompile(sizeof(*puValue) == sizeof(uint32_t));
        *puValue = *(uint32_t *)&pGicDev->abIntrPriority[idxPriority];
    }
    else
    {
        AssertReleaseFailed();
        *puValue = 0;
    }
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, *puValue));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt priority register (GICD_IPRIORITYR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_IPRIORITY range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrPriorityReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to registers 0..7 are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    Assert(idxReg < RT_ELEMENTS(pGicDev->abIntrPriority) / sizeof(uint32_t));
    Assert(idxReg != 255);
    if (idxReg > 7)
    {
        uint16_t const idxPriority = idxReg * sizeof(uint32_t);
        AssertReturn(idxPriority < RT_ELEMENTS(pGicDev->abIntrPriority) - sizeof(uint32_t), VERR_BUFFER_OVERFLOW);
        AssertCompile(sizeof(uValue) == sizeof(uint32_t));
        *(uint32_t *)&pGicDev->abIntrPriority[idxPriority] = uValue;
        LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, *(uint32_t *)&pGicDev->abIntrPriority[idxPriority]));
    }
    else
        AssertReleaseFailed();
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt pending register (GICD_ISPENDR and
 * GICD_ICPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISPENDR and
 *                      GICD_ICPENDR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrPendingReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is enabled, reads for SGIs and PPIs return 0. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrPending));
        *puValue = pGicDev->bmIntrPending[idxReg];
    }
    else
    {
        AssertReleaseFailed();
        *puValue = 0;
    }
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicDev->bmIntrPending[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Write's the distributor's interrupt set-pending register (GICD_ISPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ISPENDR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrSetPendingReg(PVMCC pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrPending));
        pGicDev->bmIntrPending[idxReg] |= uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrPending[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Write's the distributor's interrupt clear-pending register (GICD_ICPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICPENDR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrClearPendingReg(PVMCC pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrPending));
        pGicDev->bmIntrPending[idxReg] &= ~uValue;
        return gicDistUpdateIrqState(pVM, pGicDev);
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrPending[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt config register (GICD_ICFGR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICFGR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrConfigReg(PCGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is enabled SGIs and PPIs, reads to SGIs and PPIs return 0. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg >= 2)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrConfig));
        *puValue = pGicDev->bmIntrConfig[idxReg];
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicDev->bmIntrConfig[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt config register (GICD_ICFGR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICFGR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrConfigReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled SGIs and PPIs, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg >= 2)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrConfig));
        pGicDev->bmIntrConfig[idxReg] = uValue & 0xaaaaaaaa;
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrConfig[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the distributor's interrupt config register (GICD_IGROUPR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_IGROUPR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicDistReadIntrGroupReg(PGICDEV pGicDev, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is enabled, reads to SGIs and PPIs return 0. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicDev->bmIntrGroup));
        *puValue = pGicDev->bmIntrGroup[idxReg];
    }
    else
        AssertReleaseFailed();
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, *puValue));
    return VINF_SUCCESS;
}


/**
 * Writes the distributor's interrupt config register (GICD_ICFGR).
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   idxReg      The index of the register in the GICD_ICFGR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicDistWriteIntrGroupReg(PCVM pVM, PGICDEV pGicDev, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is enabled, writes to SGIs and PPIs are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    if (idxReg > 0)
    {
        pGicDev->bmIntrGroup[idxReg] = uValue;
        LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicDev->bmIntrGroup[idxReg]));
    }
    else
        AssertReleaseFailed();
    return gicDistUpdateIrqState(pVM, pGicDev);
}


/**
 * Reads the redistributor's interrupt priority register (GICR_IPRIORITYR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_IPRIORITY range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrPriorityReg(PCGICDEV pGicDev, PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is disabled, reads return 0. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    uint16_t const idxPriority = idxReg * sizeof(uint32_t);
    AssertReturn(idxPriority <= RT_ELEMENTS(pGicCpu->abIntrPriority) - sizeof(uint32_t), VERR_BUFFER_OVERFLOW);
    AssertCompile(sizeof(*puValue) == sizeof(uint32_t));
    *puValue = *(uint32_t *)&pGicCpu->abIntrPriority[idxPriority];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, *puValue));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt priority register (GICR_IPRIORITYR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_IPRIORITY range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrPriorityReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    uint16_t const idxPriority = idxReg * sizeof(uint32_t);
    AssertReturn(idxPriority <= RT_ELEMENTS(pGicCpu->abIntrPriority) - sizeof(uint32_t), VERR_BUFFER_OVERFLOW);
    AssertCompile(sizeof(uValue) == sizeof(uint32_t));
    *(uint32_t *)&pGicCpu->abIntrPriority[idxPriority] = uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, *(uint32_t *)&pGicCpu->abIntrPriority[idxPriority]));
    return VINF_SUCCESS;
}


/**
 * Reads the redistributor's interrupt pending register (GICR_ISPENDR and
 * GICR_ICPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_ISPENDR and
 *                      GICR_ICPENDR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrPendingReg(PCGICDEV pGicDev, PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is disabled, reads return 0. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrPending));
    *puValue = pGicCpu->bmIntrPending[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicCpu->bmIntrPending[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt set-pending register (GICR_ISPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ISPENDR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrSetPendingReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrPending));
    pGicCpu->bmIntrPending[idxReg] |= uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrPending[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Writes the redistributor's interrupt clear-pending register (GICR_ICPENDR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ICPENDR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrClearPendingReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrPending));
    pGicCpu->bmIntrPending[idxReg] &= ~uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrPending[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Reads the redistributor's interrupt enable register (GICR_ISENABLER and
 * GICR_ICENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_ISENABLER and
 *                      GICR_ICENABLER range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrEnableReg(PCGICDEV pGicDev, PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrEnabled));
    *puValue = pGicCpu->bmIntrEnabled[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicCpu->bmIntrEnabled[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt set-enable register (GICR_ISENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ISENABLER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrSetEnableReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    Assert(pGicDev->fAffRoutingEnabled);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrEnabled));
    pGicCpu->bmIntrEnabled[idxReg] |= uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrEnabled[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Writes the redistributor's interrupt clear-enable register (GICR_ICENABLER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ICENABLER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrClearEnableReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrEnabled));
    pGicCpu->bmIntrEnabled[idxReg] &= ~uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrEnabled[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Reads the redistributor's interrupt active register (GICR_ISACTIVER and
 * GICR_ICACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_ISACTIVER and
 *                      GICR_ICACTIVER range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrActiveReg(PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrActive));
    *puValue = pGicCpu->bmIntrActive[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicCpu->bmIntrActive[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt set-active register (GICR_ISACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ISACTIVER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrSetActiveReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrActive));
    pGicCpu->bmIntrActive[idxReg] |= uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrActive[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Writes the redistributor's interrupt clear-active register (GICR_ICACTIVER).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ICACTIVER range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrClearActiveReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrActive));
    pGicCpu->bmIntrActive[idxReg] &= ~uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrActive[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


/**
 * Reads the redistributor's interrupt config register (GICR_ICFGR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_ICFGR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrConfigReg(PCGICDEV pGicDev, PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is disabled, reads return 0. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrConfig));
    *puValue = pGicCpu->bmIntrConfig[idxReg];
    /* Ensure SGIs are read-only and remain configured as edge-triggered. */
    Assert(idxReg > 0 || *puValue == 0xaaaaaaaa);
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, *puValue));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt config register (GICR_ICFGR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_ICFGR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrConfigReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    if (idxReg > 0)
    {
        Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrConfig));
        pGicCpu->bmIntrConfig[idxReg] = uValue & 0xaaaaaaaa;
    }
    else
    {
        /* SGIs are always edge-triggered ignore writes. Windows 11 (24H2) arm64 guests writes these. */
        Assert(uValue == 0xaaaaaaaa);
        Assert(pGicCpu->bmIntrConfig[0] == uValue);
    }
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrConfig[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Reads the redistributor's interrupt group register (GICD_IGROUPR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   idxReg      The index of the register in the GICR_IGROUPR range.
 * @param   puValue     Where to store the register's value.
 */
static VBOXSTRICTRC gicReDistReadIntrGroupReg(PCGICDEV pGicDev, PGICCPU pGicCpu, uint16_t idxReg, uint32_t *puValue)
{
    /* When affinity routing is disabled, reads return 0. */
    Assert(pGicDev->fAffRoutingEnabled); RT_NOREF(pGicDev);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrGroup));
    *puValue = pGicCpu->bmIntrGroup[idxReg];
    LogFlowFunc(("idxReg=%#x read %#x\n", idxReg, pGicCpu->bmIntrGroup[idxReg]));
    return VINF_SUCCESS;
}


/**
 * Writes the redistributor's interrupt group register (GICR_IGROUPR).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxReg      The index of the register in the GICR_IGROUPR range.
 * @param   uValue      The value to write to the register.
 */
static VBOXSTRICTRC gicReDistWriteIntrGroupReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint16_t idxReg, uint32_t uValue)
{
    /* When affinity routing is disabled, writes are ignored. */
    Assert(pGicDev->fAffRoutingEnabled);
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    Assert(idxReg < RT_ELEMENTS(pGicCpu->bmIntrGroup));
    pGicCpu->bmIntrGroup[idxReg] = uValue;
    LogFlowFunc(("idxReg=%#x written %#x\n", idxReg, pGicCpu->bmIntrGroup[idxReg]));
    return gicReDistUpdateIrqState(pGicDev, pVCpu);
}


#if 0
/**
 * Gets the IDB index for a given interrupt ID.
 *
 * @returns UINT16_MAX is the interrupt ID is invalid or does not map to an index,
 *          otherwise returns a valid index.
 * @param   uIntId      The interrupt ID.
 */
static uint16_t gicIntIdToIdbIndex(uint16_t uIntId)
{
    /*
     * Interrupt Delivery Bitmap (IDB) format; bits to interrupt ID mapping:
     * +---------------------------------------------------------------------+
     * | Incl ranges  | SGI    | PPI    | SPI      | Ext PPI    | Ext SPI    |
     * +---------------------------------------------------------------------+
     * | Int Id       | 0..15  | 16..31 | 32..1019 | 1056..1119 | 4096..5119 |
     * | Bit Index    | 0..15  | 16..31 | 32..1019 | 1020..1083 | 1084..2107 |
     * +---------------------------------------------------------------------+
     */
    uint16_t const idxIdb;
    if (uIntId <= GIC_INTID_RANGE_SPI_LAST)
        return uIntId;
    if (uIntId - GIC_INTID_RANGE_EXT_PPI_START < GIC_INTID_RANGE_EXT_PPI_RANGE_SIZE)
        return GIC_INTID_RANGE_EXT_PPI_START + uIntId - GIC_INTID_RANGE_EXT_PPI_START;
    if (uIntId - GIC_INTID_RANGE_EXT_SPI_START < GIC_INTID_RANGE_EXT_SPI_RANGE_SIZE)
        return GIC_INTID_RANGE_EXT_SPI_START + uIntId - GIC_INTID_RANGE_EXT_SPI_START;
    return UINT16_MAX;
}

static bool gicIsIntIdValid(uint16_t uIntId)
{
    if (uIntId <= GIC_INTID_RANGE_SPI_LAST)
        return true;
    if (uIntIt - GIC_INTID_RANGE_EXT_PPI_START < GIC_INTID_RANGE_EXT_PPI_RANGE_SIZE)
        return true;
    if (uIntIt - GIC_INTID_RANGE_EXT_SPI_START < GIC_INTID_RANGE_EXT_SPI_RANGE_SIZE)
        return true;
    return false;
}


static void gicSetIdxInIdb(PGICIDB pGicIdb, uint16_t idxIdb)
{
    if (RT_LIKELY(idxIdb < sizeof(pGicIdb->au64IntIdBitmap) * 8))
        ASMAtomicBitSet(&pGicIdb->au64IntIdBitmap[0], idxIdb);
    else
        AssertMsgFailed(("Invalid IDB index %u for INTID %u\n" idxIdb, uIntId));
}


/**
 * Sets an interrupt ID in an Interrupt-Delivery Bitmap (IDB).
 *
 * @param   pGicIdb     Pointer to the IDB.
 * @param   uIntId      The interrupt ID.
 */
static void gicSetIntIdInIdb(PGICIDB pGicIdb, uint16_t uIntId)
{
    uint16_t const idxIdb =  gicIntIdToIdbIndex(uIntId);
    gicSetIdxInIdb(pGicIdb, idxIdb);
}


/**
 * Clears an interrupt ID in an Interrupt-Delivery Bitmap (IDB).
 *
 * @param   pGicIdb     Pointer to the IDB.
 * @param   uIntId      The interrupt ID.
 */
static void gicClearIntIdInIdb(PGICIDB pGicIdb, uint16_t uIntId)
{
    uint16_t const idxIdb =  gicIntIdToIdbIndex(uIntId);
    if (RT_LIKELY(idxIdb < sizeof(pGicIdb->au64IntIdBitmap) * 8))
        ASMAtomicBitClear(&pGicIdb->au64IntIdBitmap[0], idxIdb);
    else
        AssertMsgFailed(("Invalid IDB index %u for INTID %u\n" idxIdb, uIntId));
}


/**
 * Atomically sets the IDB notification bit.
 *
 * @returns non-zero if the bit was already set, 0 otherwise.
 * @param   pGicIdb         Pointer to the IDB.
 */
static uint32_t gicSetNotificationBitInIdb(PGICIDB pGicIdb)
{
    return ASMAtomicXchgU32(&pGicIdb->fOutstandingNotification, RT_BIT_32(31));
}


/**
 * Atomically tests and clears the IDB notification bit.
 *
 * @returns non-zero if the bit was already set, 0 otherwise.
 * @param   pGicIdb         Pointer to the IDB.
 */
static uint32_t gicClearNotificationBitInIdb(PGICIDB pGicIdb)
{
    return ASMAtomicXchgU32(&pGicIdb->fOutstandingNotification, UINT32_C(0));
}


static VBOXSTRICTRC gicPostInterrupt(PVMCPUCC pVCpu, PVMCPUSET pCpuSet, uint16_t idxIdb)
{
    Assert(gicIntIdToIdbIndex(idxIdb) != UINT16_MAX);
    for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
    {
        if (VMCPUSET_IS_PRESENT(&DestCpuSet, idCpu))
        {
            PVMCPUCC pTargetVCpu = pVCpu->pVMR3->CTX_SUFF(apCpus)[idCpu];
            VMCPU_ASSERT_VALID_EXT_RETURN(pVCpu, VERR_INVALID_HANDLE);

            PGICCPU pTargetGicCpu = VMCPU_TO_GICCPU(pTargetVCpu);
            PGICIDB pTargetGicIdb = &pTargetGicCpu->IntrDeliveryBitmap;
            gicSetIdxInIdb(pTargetGicIdb, idxIdb);
            uint32_t const fAlreadySet = gicSetNotificationBitInIdb(pTargetGicIdb);
            if (!fAlreadySet)
                gicSetUpdateInterruptFF(pVCpu);
        }
    }
}
#endif


/**
 * Gets the virtual CPUID given the affinity values.
 *
 * @returns The virtual CPUID.
 * @param   idCpuInterface  The virtual CPUID within the PE cluster (0..15).
 * @param   uAff1           The affinity 1 value.
 * @param   uAff2           The affinity 2 value.
 * @param   uAff3           The affinity 3 value.
 */
DECL_FORCE_INLINE(VMCPUID) gicGetCpuIdFromAffinity(uint8_t idCpuInterface, uint8_t uAff1,  uint8_t uAff2,  uint8_t uAff3)
{
    AssertReturn(idCpuInterface < 16, 0);
    return (uAff3 * 1048576) + (uAff2 * 4096) + (uAff1 * 16) + idCpuInterface;
}


#if 0
/**
 * @interface_method_impl{PDMGICBACKEND,pfnUpdatePendingInterrupts}
 */
static DECLCALLBACK(void) gicUpdatePendingInterrupts(PVMCPUCC pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);

    /*
     * Delivery interrupts pending in the interrupt-delivery bitmap to the PE.
     */
    bool    fHasPendingIntrs = false;
    PGICIDB pIdb = (PGICIDB)pGicCpu->IntrDeliveryBitmap;
    for (;;)
    {
        uint32_t const fAlreadySet = gicClearNotificationBitInIdb(pIdb);
        if (!fAlreadySet)
            break;

        /* SGI and PPIs */
        uint32_t const uSgiPpi = ASMAtomicXchgU32(&pIdb->au64IntIdBitmap[0], 0);
        if (uSgiPpi)

    }

    STAM_PROFILE_STOP(&pApicCpu->StatUpdatePendingIntrs, a);
    Log3(("APIC%u: apicUpdatePendingInterrupts: fHasPendingIntrs=%RTbool\n", pVCpu->idCpu, fHasPendingIntrs));

    if (   fHasPendingIntrs
        && !VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC))
        apicSignalNextPendingIntr(pVCpu);
}
#endif


#if 0
/**
 * Gets the highest priority pending distributor interrupt that can be forwarded to
 * the redistributor.
 *
 * @returns The interrupt ID or GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT if no interrupt
 *          is pending or not in a state to be forwarded to the redistributor.
 * @param   pGicDev     The GIC distributor state.
 * @param   fGroup0     Whether to consider group 0 interrupts.
 * @param   fGroup1     Whether to consider group 1 interrupts.
 * @param   pidxIntr    Where to store the distributor interrupt index for the
 *                      returned interrupt ID. UINT16_MAX if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT. Optional, can be
 *                      NULL.
 * @param   pbPriority  Where to store the priority of the returned interrupt ID.
 *                      GIC_IDLE_PRIORITY if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT.
 */
static uint16_t gicDistGetHighestPrioPendingIntr(PCGICDEV pGicDev, bool fGroup0, bool fGroup1, uint16_t *pidxIntr,
                                                 uint8_t *pbPriority)
{
    /*
     * Figure out the highest priority pending interrupt in the distributor.
     * We can skip SGIs, PPIs in the distributor as we don't support legacy operation.
     *
     * See ARM GIC spec. 4.7.2 "Interaction of group and individual interrupt enables".
     * See ARM GIC spec. 1.3.5 "GICv3 with no legacy operation".
     */
    Assert(pGicDev->fAffRoutingEnabled);
    uint32_t bmIntrPending[64];
    for (uint8_t i = 0; i < RT_ELEMENTS(bmIntrPending); i++)
    {
        /* Collect interrupts that are pending, enabled and inactive. */
        bmIntrPending[i] = (pGicDev->bmIntrPending[i] & pGicDev->bmIntrEnabled[i]) & ~pGicDev->bmIntrActive[i];

        /* Discard interrupts if the group they belong to is disabled. */
        if (!fGroup1)
            bmIntrPending[i] &= ~pGicDev->bmIntrGroup[i];
        if (!fGroup0)
            bmIntrPending[i] &= pGicDev->bmIntrGroup[i];
    }

    /* Among the collected interrupts, pick the one with the highest, non-idle priority. */
    uint16_t       uIntId     = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
    uint16_t       idxHighest = UINT16_MAX;
    uint8_t        uPriority  = GIC_IDLE_PRIORITY;
    void const    *pvIntrs    = &bmIntrPending[0];
    uint32_t const cIntrs     = sizeof(bmIntrPending) * 8;
    int32_t        idxIntr    = ASMBitFirstSet(pvIntrs, cIntrs);
    AssertCompile(!(cIntrs % 32));
    if (idxIntr >= 0)
    {
        Assert(idxIntr >= 32);  /* We don't support GICv2 legacy operation. */
        do
        {
            Assert((uint32_t)idxIntr < RT_ELEMENTS(pGicDev->abIntrPriority));
            if (pGicDev->abIntrPriority[idxIntr] < uPriority)
            {
                idxHighest = (uint16_t)idxIntr;
                uPriority  = pGicDev->abIntrPriority[idxIntr];
            }
            idxIntr = ASMBitNextSet(pvIntrs, cIntrs, idxIntr);
        } while (idxIntr != -1);
    }

    *pbPriority = uPriority;
    if (uPriority != GIC_IDLE_PRIORITY)
        uIntId = gicDistGetIntIdFromIndex(idxHighest);
    if (pidxIntr)
        *pidxIntr = idxHighest;

    /* Sanity check if the interrupt ID is within known ranges. */
    Assert(   GIC_IS_INTR_SPI(uIntId)
           || GIC_IS_INTR_EXT_PPI(uIntId)
           || GIC_IS_INTR_EXT_SPI(uIntId)
           || uIntId == GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT);
    /* Ensure that if no interrupt is pending, the priority is appropriate. */
    Assert(uIntId != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT || *pbPriority == GIC_IDLE_PRIORITY);

    LogFlowFunc(("uIntId=%u [idxIntr=%u uPriority=%u]\n", uIntId, idxIntr, uPriority));
    return uIntId;
}


/**
 * Gets the highest priority pending redistributor interrupt that can be signalled
 * to the PE.
 *
 * @returns The interrupt ID or GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT if no interrupt
 *          is pending or not in a state to be signalled to the PE.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   fGroup0     Whether to consider group 0 interrupts.
 * @param   fGroup1     Whether to consider group 1 interrupts.
 * @param   pidxIntr    Where to store the distributor interrupt index for the
 *                      returned interrupt ID. UINT16_MAX if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT. Optional, can be
 *                      NULL.
 * @param   pbPriority  Where to store the priority of the returned interrupt ID.
 *                      GIC_IDLE_PRIORITY if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT.
 */
static uint16_t gicReDistGetHighestPrioPendingIntr(PCGICCPU pGicCpu, bool fGroup0, bool fGroup1, uint16_t *pidxIntr,
                                                   uint8_t *pbPriority)
{
    /*
     * Figure out the highest priority pending interrupt in the redistributor.
     * See ARM GIC spec. 4.7.2 "Interaction of group and individual interrupt enables".
     */
    uint32_t bmIntrPending[3];
    for (uint8_t i = 0 ; i < RT_ELEMENTS(bmIntrPending); i++)
    {
        /* Collect interrupts that are pending, enabled and inactive. */
        bmIntrPending[i] = (pGicCpu->bmIntrPending[i] & pGicCpu->bmIntrEnabled[i]) & ~pGicCpu->bmIntrActive[i];

        /* Discard interrupts if the group they belong to is disabled. */
        if (!fGroup1)
            bmIntrPending[i] &= ~pGicCpu->bmIntrGroup[i];
        if (!fGroup0)
            bmIntrPending[i] &= pGicCpu->bmIntrGroup[i];
    }

    /* Among the collected interrupts, pick the one with the highest, non-idle priority. */
    uint16_t       uIntId     = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
    uint16_t       idxHighest = UINT16_MAX;
    uint8_t        uPriority  = GIC_IDLE_PRIORITY;
    const void    *pvIntrs    = &bmIntrPending[0];
    uint32_t const cIntrs     = sizeof(bmIntrPending) * 8;
    int32_t        idxIntr    = ASMBitFirstSet(pvIntrs, cIntrs);
    AssertCompile(!(cIntrs % 32));
    if (idxIntr >= 0)
    {
        do
        {
            Assert((uint32_t)idxIntr < RT_ELEMENTS(pGicCpu->abIntrPriority));
            if (pGicCpu->abIntrPriority[idxIntr] < uPriority)
            {
                idxHighest = (uint16_t)idxIntr;
                uPriority  = pGicCpu->abIntrPriority[idxIntr];
            }
            idxIntr = ASMBitNextSet(pvIntrs, cIntrs, idxIntr);
        } while (idxIntr != -1);
    }

    *pbPriority = uPriority;
    if (uPriority != GIC_IDLE_PRIORITY)
        uIntId = gicReDistGetIntIdFromIndex(idxHighest);
    if (pidxIntr)
        *pidxIntr = idxHighest;

    /* Sanity check if the interrupt ID is within known ranges. */
    Assert(   GIC_IS_INTR_SGI_OR_PPI(uIntId)
           || GIC_IS_INTR_EXT_PPI(uIntId)
           || uIntId == GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT);
    /* Ensure that if no interrupt is pending, the priority is appropriate. */
    Assert(uIntId != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT || *pbPriority == GIC_IDLE_PRIORITY);

    LogFlowFunc(("uIntId=%u [idxIntr=%u uPriority=%u]\n", uIntId, idxIntr, uPriority));
    return uIntId;
}
#endif


#if 1
/**
 * Gets the highest priority pending interrupt that can be signalled to the PE.
 *
 * @returns The interrupt ID or GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT if no interrupt
 *          is pending or not in a state to be signalled to the PE.
 * @param   pGicDev     The GIC distributor state.
 * @param   pGicCpu     The GIC redistributor and CPU interface state.
 * @param   fGroup0     Whether to consider group 0 interrupts.
 * @param   fGroup1     Whether to consider group 1 interrupts.
 * @param   pidxIntr    Where to store the distributor interrupt index for the
 *                      returned interrupt ID. UINT16_MAX if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT. Optional, can be
 *                      NULL.
 * @param   pbPriority  Where to store the priority of the returned interrupt ID.
 *                      GIC_IDLE_PRIORITY if this function returns
 *                      GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT.
 */
static uint16_t gicGetHighestPriorityPendingIntr(PCGICDEV pGicDev, PCGICCPU pGicCpu, bool fGroup0, bool fGroup1,
                                                 uint16_t *pidxIntr, uint8_t *pbPriority)
{
    /*
     * Collect interrupts that are pending, enabled and inactive.
     * Discard interrupts if the group they belong to is disabled.
     * While collecting the interrupts, pick the one with the highest, non-idle priority.
     */
    uint16_t uIntId    = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
    uint16_t idxIntr   = UINT16_MAX;
    uint8_t  uPriority = GIC_IDLE_PRIORITY;

    /* Redistributor. */
    {
        uint16_t idxHighest = UINT16_MAX;
        for (uint16_t i = 0; i < RT_ELEMENTS(pGicCpu->bmIntrPending); i++)
        {
            uint32_t uIntrPending = (pGicCpu->bmIntrPending[i] & pGicCpu->bmIntrEnabled[i]) & ~pGicCpu->bmIntrActive[i];
            if (!fGroup1)
                uIntrPending &= ~pGicCpu->bmIntrGroup[i];
            if (!fGroup0)
                uIntrPending &= pGicCpu->bmIntrGroup[i];

            uint16_t const idxPending = ASMBitFirstSetU32(uIntrPending);
            if (idxPending > 0)
            {
                uint32_t const idxPriority = 32 * i + idxPending - 1;
                Assert(idxPriority < RT_ELEMENTS(pGicCpu->abIntrPriority));
                if (pGicCpu->abIntrPriority[idxPriority] < uPriority)
                {
                    idxHighest = idxPriority;
                    uPriority  = pGicCpu->abIntrPriority[idxPriority];
                }
            }
        }
        if (idxHighest != UINT16_MAX)
        {
            idxIntr = idxHighest;
            uIntId  = gicReDistGetIntIdFromIndex(idxHighest);
            Assert(   GIC_IS_INTR_SGI_OR_PPI(uIntId)
                   || GIC_IS_INTR_EXT_PPI(uIntId));
            Assert(uPriority != GIC_IDLE_PRIORITY);
        }
    }

    /* Distributor. */
    {
        uint16_t idxHighest = UINT16_MAX;
        for (uint16_t i = 0; i < RT_ELEMENTS(pGicDev->bmIntrPending); i += 2)
        {
            uint32_t uLo = (pGicDev->bmIntrPending[i]     & pGicDev->bmIntrEnabled[i])     & ~pGicDev->bmIntrActive[i];
            uint32_t uHi = (pGicDev->bmIntrPending[i + 1] & pGicDev->bmIntrEnabled[i + 1]) & ~pGicDev->bmIntrActive[i + 1];
            if (!fGroup1)
            {
                uLo &= ~pGicDev->bmIntrGroup[i];
                uHi &= ~pGicDev->bmIntrGroup[i + 1];
            }
            if (!fGroup0)
            {
                uLo &= pGicDev->bmIntrGroup[i];
                uHi &= pGicDev->bmIntrGroup[i + 1];
            }

            uint64_t const uIntrPending = RT_MAKE_U64(uLo, uHi);
            uint16_t const idxPending   = ASMBitFirstSetU64(uIntrPending);
            if (idxPending > 0)
            {
                uint32_t const idxPriority = 64 * i + idxPending - 1;
                if (pGicDev->abIntrPriority[idxPriority] < uPriority)
                {
                    idxHighest = idxPriority;
                    uPriority  = pGicDev->abIntrPriority[idxPriority];
                }
            }
        }
        if (idxHighest != UINT16_MAX)
        {
            idxIntr = idxHighest;
            uIntId  = gicDistGetIntIdFromIndex(idxHighest);
            Assert(   GIC_IS_INTR_SPI(uIntId)
                   || GIC_IS_INTR_EXT_SPI(uIntId));
            Assert(uPriority != GIC_IDLE_PRIORITY);
        }
    }

    /* Ensure that if no interrupt is pending, the idle priority is returned. */
    Assert(uIntId != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT || uPriority == GIC_IDLE_PRIORITY);
    if (pbPriority)
        *pbPriority = uPriority;
    if (pidxIntr)
        *pidxIntr = idxIntr;

    LogFlowFunc(("uIntId=%u [idxIntr=%u uPriority=%u]\n", uIntId, idxIntr, uPriority));
    return uIntId;
}
#else
/**
 * Gets the highest priority pending interrupt that can be signalled to the PE.
 *
 * @returns The interrupt ID or GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT if no interrupt
 *          is pending or not in a state to be signalled to the PE.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pGicDev     The GIC distributor state.
 * @param   fGroup0     Whether to consider group 0 interrupts.
 * @param   fGroup1     Whether to consider group 1 interrupts.
 */
static uint16_t gicGetHighestPrioPendingIntr(PCVMCPUCC pVCpu, PCGICDEV pGicDev, bool fGroup0, bool fGroup1)
{
    /* Get highest priority pending interrupt from the distributor. */
    uint8_t  bPriority;
    uint16_t uIntId = gicDistGetHighestPrioPendingIntr(pGicDev, fGroup0, fGroup1, NULL /*pidxIntr*/, &bPriority);

    /* Compare with the highest priority pending interrupt from the redistributor. */
    PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    uint8_t bPriorityRedist;
    uint16_t const uIntIdRedist = gicReDistGetHighestPrioPendingIntr(pGicCpu, fGroup0, fGroup1, NULL /*pidxIntr*/,
                                                                     &bPriorityRedist);
    if (   uIntIdRedist != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT
        && bPriorityRedist < bPriority)
    {
        bPriority = bPriorityRedist;
        uIntId    = uIntIdRedist;
    }

    /* Sanity check if the interrupt ID is within known ranges. */
    Assert(   GIC_IS_INTR_SGI_OR_PPI(uIntId)
           || GIC_IS_INTR_SPI(uIntId)
           || GIC_IS_INTR_EXT_PPI(uIntId)
           || GIC_IS_INTR_EXT_SPI(uIntId)
           || uIntId == GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT);
    /* Ensure that if no interrupt is pending, the priority is appropriate. */
    Assert(uIntId != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT || bPriority == GIC_IDLE_PRIORITY);

    LogFlowFunc(("uIntId=%u [bPriority=%u]\n", uIntId, bPriority));
    return uIntId;
}
#endif


/**
 * Get and acknowledge the interrupt ID of a signalled interrupt.
 *
 * @returns The interrupt ID or GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT no interrupts
 *          are pending or not in a state to be signalled.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   fGroup0     Whether to consider group 0 interrupts.
 * @param   fGroup1     Whether to consider group 1 interrupts.
 */
static uint16_t gicAckHighestPriorityPendingIntr(PGICDEV pGicDev, PVMCPUCC pVCpu, bool fGroup0, bool fGroup1)
{
    Assert(fGroup0 || fGroup1);
    LogFlowFunc(("fGroup0=%RTbool fGroup1=%RTbool\n", fGroup0, fGroup1));

    /*
     * Get the pending interrupt with the highest priority for the given group.
     */
    uint8_t  bIntrPriority;
    uint16_t idxIntr;
    PGICCPU  pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    STAM_PROFILE_START(&pGicCpu->CTX_SUFF_Z(StatProfIntrAck), x);
    uint16_t const uIntId = gicGetHighestPriorityPendingIntr(pGicDev, pGicCpu, fGroup0, fGroup1, &idxIntr, &bIntrPriority);
    if (uIntId != GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT)
    {
        /*
         * The interrupt priority must be higher than the priority mask of the CPU interface for the
         * interrupt to be signalled/acknowledged. Here, we must NOT use priority grouping when comparing
         * the priority of a pending interrupt with this priority mask (threshold).
         *
         * See ARM GIC spec. 4.8.6 "Priority masking".
         */
        if (bIntrPriority >= pGicCpu->bIntrPriorityMask)
        {
            STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF_Z(StatProfIntrAck), x);
            return GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
        }

        /*
         * The group priority of the pending interrupt must be higher than that of the running priority.
         * The number of bits for the group priority depends on the the binary point registers.
         * We mask the sub-priority bits and only compare the group priority.
         *
         * When the binary point registers indicates no preemption, we must allow interrupts that have
         * a higher priority than idle. Hence, the use of two different masks below.
         *
         * See ARM GIC spec. 4.8.3 "Priority grouping".
         * See ARM GIC spec. 4.8.5 "Preemption".
         */
        static uint8_t const s_afGroupPriorityMasks[8]   = { 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00 };
        static uint8_t const s_afRunningPriorityMasks[8] = { 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0xff };
        uint8_t const idxPriorityMask       = (fGroup0 || (pGicCpu->uIccCtlr & ARMV8_ICC_CTLR_EL1_AARCH64_CBPR))
                                            ? pGicCpu->bBinaryPtGroup0 & 7
                                            : pGicCpu->bBinaryPtGroup1 & 7;
        uint8_t const bRunningPriority      = pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority];
        uint8_t const bRunningGroupPriority = bRunningPriority & s_afRunningPriorityMasks[idxPriorityMask];
        uint8_t const bIntrGroupPriority    = bIntrPriority    & s_afGroupPriorityMasks[idxPriorityMask];
        if (bIntrGroupPriority >= bRunningGroupPriority)
        {
            STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF_Z(StatProfIntrAck), x);
            return GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
        }

        /*
         * Acknowledge the interrupt.
         */
        bool const fIsRedistIntId = GIC_IS_INTR_SGI_OR_PPI(uIntId) || GIC_IS_INTR_EXT_PPI(uIntId);
        if (fIsRedistIntId)
        {
            /* Mark the interrupt as active. */
            AssertMsg(idxIntr < sizeof(pGicCpu->bmIntrActive) * 8, ("idxIntr=%u\n", idxIntr));
            ASMBitSet(&pGicCpu->bmIntrActive[0], idxIntr);

            /** @todo Duplicate block Id=E5ED12D2-088D-4525-9609-8325C02846C3 (start). */
            /* Update the active priorities bitmap. */
            AssertCompile(sizeof(pGicCpu->bmActivePriorityGroup0) * 8 >= 128);
            AssertCompile(sizeof(pGicCpu->bmActivePriorityGroup1) * 8 >= 128);
            uint8_t const idxPreemptionLevel = bIntrPriority >> 1;
            if (fGroup0)
                ASMBitSet(&pGicCpu->bmActivePriorityGroup0[0], idxPreemptionLevel);
            if (fGroup1)
                ASMBitSet(&pGicCpu->bmActivePriorityGroup1[0], idxPreemptionLevel);

            /* Drop priority. */
            if (RT_LIKELY(pGicCpu->idxRunningPriority < RT_ELEMENTS(pGicCpu->abRunningPriorities) - 1))
            {
                LogFlowFunc(("Dropping interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                             pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority],
                             bIntrPriority,
                             pGicCpu->idxRunningPriority, pGicCpu->idxRunningPriority + 1));
                ++pGicCpu->idxRunningPriority;
                pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority] = bIntrPriority;
            }
            else
                AssertReleaseMsgFailed(("Index of running-interrupt priority out-of-bounds %u\n", pGicCpu->idxRunningPriority));
            /** @todo Duplicate block Id=E5ED12D2-088D-4525-9609-8325C02846C3 (end). */

            /* If it is an edge-triggered interrupt, mark it as no longer pending. */
            AssertRelease(2 * idxIntr + 1 < sizeof(pGicCpu->bmIntrConfig) * 8);
            bool const fEdgeTriggered = ASMBitTest(&pGicCpu->bmIntrConfig[0], 2 * idxIntr + 1);
            if (fEdgeTriggered)
                ASMBitClear(&pGicCpu->bmIntrPending[0], idxIntr);

            /* Update the redistributor IRQ state to reflect change to the active interrupt. */
            gicReDistUpdateIrqState(pGicDev, pVCpu);
        }
        else
        {
            /* Sanity check if the interrupt ID belongs to the distributor. */
            Assert(GIC_IS_INTR_SPI(uIntId) || GIC_IS_INTR_EXT_SPI(uIntId));

            /* Mark the interrupt as active. */
            Assert(idxIntr < sizeof(pGicDev->bmIntrActive) * 8);
            ASMBitSet(&pGicDev->bmIntrActive[0], idxIntr);

            /** @todo Duplicate block Id=E5ED12D2-088D-4525-9609-8325C02846C3 (start). */
            /* Update the active priorities bitmap. */
            AssertCompile(sizeof(pGicCpu->bmActivePriorityGroup0) * 8 >= 128);
            AssertCompile(sizeof(pGicCpu->bmActivePriorityGroup1) * 8 >= 128);
            uint8_t const idxPreemptionLevel = bIntrPriority >> 1;
            if (fGroup0)
                ASMBitSet(&pGicCpu->bmActivePriorityGroup0[0], idxPreemptionLevel);
            if (fGroup1)
                ASMBitSet(&pGicCpu->bmActivePriorityGroup1[0], idxPreemptionLevel);

            /* Drop priority. */
            if (RT_LIKELY(pGicCpu->idxRunningPriority < RT_ELEMENTS(pGicCpu->abRunningPriorities) - 1))
            {
                LogFlowFunc(("Dropping interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                             pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority],
                             bIntrPriority,
                             pGicCpu->idxRunningPriority, pGicCpu->idxRunningPriority + 1));
                ++pGicCpu->idxRunningPriority;
                pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority] = bIntrPriority;
            }
            else
                AssertReleaseMsgFailed(("Index of running-interrupt priority out-of-bounds %u\n", pGicCpu->idxRunningPriority));
            /** @todo Duplicate block Id=E5ED12D2-088D-4525-9609-8325C02846C3 (end). */

            /* If it is an edge-triggered interrupt, mark it as no longer pending. */
            AssertRelease(2 * idxIntr + 1 < sizeof(pGicDev->bmIntrConfig) * 8);
            bool const fEdgeTriggered = ASMBitTest(&pGicDev->bmIntrConfig[0], 2 * idxIntr + 1);
            if (fEdgeTriggered)
                ASMBitClear(&pGicDev->bmIntrPending[0], idxIntr);

            /* Update the distributor IRQ state to reflect change to the active interrupt. */
            gicDistUpdateIrqState(pVCpu->CTX_SUFF(pVM), pGicDev);
        }
    }
    else
        Assert(bIntrPriority == GIC_IDLE_PRIORITY);

    LogFlowFunc(("uIntId=%u\n", uIntId));
    STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF(StatProfIntrAck), x);
    return uIntId;
}


/**
 * Reads a distributor register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicDistReadRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu); RT_NOREF(pVCpu);
    PGICDEV  pGicDev     = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    uint16_t const cbReg = sizeof(uint32_t);

    /*
     * GICD_IGROUPR<n> and GICD_IGROUPR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IGROUPRn_OFF_START < GIC_DIST_REG_IGROUPRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IGROUPRn_OFF_START) / cbReg;
            return gicDistReadIntrGroupReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_IGROUPRnE_OFF_START < GIC_DIST_REG_IGROUPRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrGroup) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IGROUPRnE_OFF_START) / cbReg;
            return gicDistReadIntrGroupReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_IROUTER<n> and GICD_IROUTER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IROUTERn_OFF_START < GIC_DIST_REG_IROUTERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IROUTERn_OFF_START) / cbReg;
            return gicDistReadIntrRoutingReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_IROUTERnE_OFF_START < GIC_DIST_REG_IROUTERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->au32IntrRouting) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IROUTERnE_OFF_START) / cbReg;
            return gicDistReadIntrRoutingReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_ISENABLER<n> and GICD_ISENABLER<n>E.
     * GICD_ICENABLER<n> and GICD_ICENABLER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISENABLERn_OFF_START  < GIC_DIST_REG_ISENABLERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISENABLERn_OFF_START) / cbReg;
            return gicDistReadIntrEnableReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ISENABLERnE_OFF_START < GIC_DIST_REG_ISENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrEnabled) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISENABLERnE_OFF_START) / cbReg;
            return gicDistReadIntrEnableReg(pGicDev, idxReg, puValue);
        }

        if (offReg - GIC_DIST_REG_ICENABLERn_OFF_START  < GIC_DIST_REG_ICENABLERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICENABLERn_OFF_START) / cbReg;
            return gicDistReadIntrEnableReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ICENABLERnE_OFF_START < GIC_DIST_REG_ICENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrEnabled) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICENABLERnE_OFF_START) / cbReg;
            return gicDistReadIntrEnableReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_ISACTIVER<n> and GICD_ISACTIVER<n>E.
     * GICD_ICACTIVER<n> and GICD_ICACTIVER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISACTIVERn_OFF_START < GIC_DIST_REG_ISACTIVERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISACTIVERn_OFF_START) / cbReg;
            return gicDistReadIntrActiveReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ISACTIVERnE_OFF_START < GIC_DIST_REG_ISACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrActive) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISACTIVERnE_OFF_START) / cbReg;
            return gicDistReadIntrActiveReg(pGicDev, idxReg, puValue);
        }

        if (offReg - GIC_DIST_REG_ICACTIVERn_OFF_START < GIC_DIST_REG_ICACTIVERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICENABLERn_OFF_START) / cbReg;
            return gicDistReadIntrActiveReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ICACTIVERnE_OFF_START < GIC_DIST_REG_ICACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrActive) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICACTIVERnE_OFF_START) / cbReg;
            return gicDistReadIntrActiveReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_IPRIORITYR<n> and GICD_IPRIORITYR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START < GIC_DIST_REG_IPRIORITYRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START) / cbReg;
            return gicDistReadIntrPriorityReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_IPRIORITYRnE_OFF_START < GIC_DIST_REG_IPRIORITYRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->abIntrPriority) / (2 * sizeof(uint32_t));
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IPRIORITYRnE_OFF_START) / cbReg;
            return gicDistReadIntrPriorityReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_ISPENDR<n> and GICD_ISPENDR<n>E.
     * GICD_ICPENDR<n> and GICD_ICPENDR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISPENDRn_OFF_START < GIC_DIST_REG_ISPENDRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISPENDRn_OFF_START) / cbReg;
            return gicDistReadIntrPendingReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ISPENDRnE_OFF_START < GIC_DIST_REG_ISPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrPending) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISPENDRnE_OFF_START) / cbReg;
            return gicDistReadIntrPendingReg(pGicDev, idxReg, puValue);
        }

        if (offReg - GIC_DIST_REG_ICPENDRn_OFF_START < GIC_DIST_REG_ICPENDRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICPENDRn_OFF_START) / cbReg;
            return gicDistReadIntrPendingReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ICPENDRnE_OFF_START < GIC_DIST_REG_ICPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrPending) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICPENDRnE_OFF_START) / cbReg;
            return gicDistReadIntrPendingReg(pGicDev, idxReg, puValue);
        }
    }

    /*
     * GICD_ICFGR<n> and GICD_ICFGR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ICFGRn_OFF_START < GIC_DIST_REG_ICFGRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICFGRn_OFF_START) / cbReg;
            return gicDistReadIntrConfigReg(pGicDev, idxReg, puValue);
        }
        if (offReg - GIC_DIST_REG_ICFGRnE_OFF_START < GIC_DIST_REG_ICFGRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrConfig) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICFGRnE_OFF_START) / cbReg;
            return gicDistReadIntrConfigReg(pGicDev, idxReg, puValue);
        }
    }

    switch (offReg)
    {
        case GIC_DIST_REG_CTLR_OFF:
            Assert(pGicDev->fAffRoutingEnabled);
            *puValue = (pGicDev->fIntrGroup0Enabled ? GIC_DIST_REG_CTRL_ENABLE_GRP0    : 0)
                     | (pGicDev->fIntrGroup1Enabled ? GIC_DIST_REG_CTRL_ENABLE_GRP1_NS : 0)
                     | GIC_DIST_REG_CTRL_DS         /* We don't support multiple security states. */
                     | GIC_DIST_REG_CTRL_ARE_S;     /* We don't support GICv2 backwards compatibility, ARE is always enabled. */
            break;
        case GIC_DIST_REG_TYPER_OFF:
        {
            Assert(pGicDev->uMaxSpi > 0 && pGicDev->uMaxSpi <= GIC_DIST_REG_TYPER_NUM_ITLINES);
            Assert(pGicDev->fAffRoutingEnabled);
            *puValue = GIC_DIST_REG_TYPER_NUM_ITLINES_SET(pGicDev->uMaxSpi)
                     | GIC_DIST_REG_TYPER_NUM_PES_SET(0)      /* Affinity routing is always enabled, hence this MBZ. */
                     /*| GIC_DIST_REG_TYPER_NMI*/             /** @todo Support non-maskable interrupts */
                     /*| GIC_DIST_REG_TYPER_SECURITY_EXTN*/   /** @todo Support dual security states. */
                     | (pGicDev->fMbi ? GIC_DIST_REG_TYPER_MBIS : 0)
                     /*| GIC_DIST_REG_TYPER_LPIS*/            /** @todo Support LPIs */
                     | (pGicDev->fRangeSel ? GIC_DIST_REG_TYPER_RSS : 0)
                     | GIC_DIST_REG_TYPER_IDBITS_SET(16)      /* We only support 16-bit interrupt IDs. */
                     | (pGicDev->fAff3Levels ? GIC_DIST_REG_TYPER_A3V : 0);
            if (pGicDev->fExtSpi)
                *puValue |= GIC_DIST_REG_TYPER_ESPI
                         |  GIC_DIST_REG_TYPER_ESPI_RANGE_SET(pGicDev->uMaxExtSpi);
            break;
        }
        case GIC_DIST_REG_STATUSR_OFF:
            AssertReleaseFailed();
            break;
#if 0
        case GIC_DIST_REG_IGROUPRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ISENABLERn_OFF_START + 4: /* Only 32 lines for now. */
        case GIC_DIST_REG_ICENABLERn_OFF_START + 4: /* Only 32 lines for now. */
            *puValue = ASMAtomicReadU32(&pThis->bmIntEnabled);
            break;
        case GIC_DIST_REG_ISPENDRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ICPENDRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ISACTIVERn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ICACTIVERn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_IPRIORITYRn_OFF_START:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 4: /* These are banked for the PEs and access the redistributor. */
        {
            PGICCPU pGicVCpu = VMCPU_TO_GICCPU(pVCpu);

            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));

            uint32_t u32Value = 0;
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
                u32Value |= pGicVCpu->abIntPriority[i] << ((i - idxPrio) * 8);

            *puValue = u32Value;
            break;
        }
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 32: /* Only 32 lines for now. */
        {
            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START - 32;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));

            uint32_t u32Value = 0;
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
                u32Value |= pThis->abIntPriority[i] << ((i - idxPrio) * 8);

            *puValue = u32Value;
            break;
        }
#endif
        case GIC_DIST_REG_ITARGETSRn_OFF_START:
            AssertReleaseFailed();
            break;
#if 0
        case GIC_DIST_REG_ICFGRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
#endif
        case GIC_DIST_REG_IGRPMODRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_NSACRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SGIR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_CPENDSGIRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SPENDSGIRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_INMIn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_PIDR2_OFF:
#if 0
            Assert(pThis->uArchRev <= GIC_DIST_REG_PIDR2_ARCH_REV_GICV4);
            *puValue = GIC_DIST_REG_PIDR2_ARCH_REV_SET(pThis->uArchRev);
#else
            Assert(pGicDev->uArchRev <= GIC_DIST_REG_PIDR2_ARCH_REV_GICV4);
            *puValue = GIC_DIST_REG_PIDR2_ARCH_REV_SET(pGicDev->uArchRev);
#endif
            break;
        case GIC_DIST_REG_IIDR_OFF:
            *puValue = 0x43b;   /* JEP106 code 0x43b is an ARM implementation. */
            break;
        case GIC_DIST_REG_TYPER2_OFF:
            *puValue = 0;
            break;
#if 0
        case GIC_DIST_REG_IROUTERn_OFF_START:
            AssertFailed();
            break;
#endif
        default:
            *puValue = 0;
    }
    return VINF_SUCCESS;
}


/**
 * Writes a distributor register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicDistWriteRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu); RT_NOREF(pVCpu);
    PGICDEV        pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    PVMCC          pVM     = PDMDevHlpGetVM(pDevIns);
    uint16_t const cbReg   = sizeof(uint32_t);

    /*
     * GICD_IGROUPR<n> and GICD_IGROUPR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IGROUPRn_OFF_START < GIC_DIST_REG_IGROUPRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IGROUPRn_OFF_START) / cbReg;
            return gicDistWriteIntrGroupReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_IGROUPRnE_OFF_START < GIC_DIST_REG_IGROUPRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrGroup) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IGROUPRnE_OFF_START) / cbReg;
            return gicDistWriteIntrGroupReg(pVM, pGicDev, idxReg, uValue);
        }
    }

    /*
     * GICD_IROUTER<n> and GICD_IROUTER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IROUTERn_OFF_START < GIC_DIST_REG_IROUTERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IROUTERn_OFF_START) / cbReg;
            return gicDistWriteIntrRoutingReg(pGicDev, offReg, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_IROUTERnE_OFF_START < GIC_DIST_REG_IROUTERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->au32IntrRouting) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IROUTERnE_OFF_START) / cbReg;
            return gicDistWriteIntrRoutingReg(pGicDev, offReg, idxReg, uValue);
        }
    }

    /*
     * GICD_ISENABLER<n> and GICD_ISENABLER<n>E.
     * GICD_ICENABLER<n> and GICD_ICENABLER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISENABLERn_OFF_START  < GIC_DIST_REG_ISENABLERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISENABLERn_OFF_START) / cbReg;
            return gicDistWriteIntrSetEnableReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ISENABLERnE_OFF_START < GIC_DIST_REG_ISENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrEnabled) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISENABLERnE_OFF_START) / cbReg;
            return gicDistWriteIntrSetEnableReg(pVM, pGicDev, idxReg, uValue);
        }

        if (offReg - GIC_DIST_REG_ICENABLERn_OFF_START  < GIC_DIST_REG_ICENABLERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICENABLERn_OFF_START) / cbReg;
            return gicDistWriteIntrClearEnableReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ICENABLERnE_OFF_START < GIC_DIST_REG_ICENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrEnabled) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICENABLERnE_OFF_START) / cbReg;
            return gicDistWriteIntrClearEnableReg(pVM, pGicDev, idxReg, uValue);
        }
    }

    /*
     * GICD_ISACTIVER<n> and GICD_ISACTIVER<n>E.
     * GICD_ICACTIVER<n> and GICD_ICACTIVER<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISACTIVERn_OFF_START < GIC_DIST_REG_ISACTIVERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISACTIVERn_OFF_START) / cbReg;
            return gicDistWriteIntrSetActiveReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ISACTIVERnE_OFF_START < GIC_DIST_REG_ISACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrActive) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISACTIVERnE_OFF_START) / cbReg;
            return gicDistWriteIntrSetActiveReg(pVM, pGicDev, idxReg, uValue);
        }

        if (offReg - GIC_DIST_REG_ICACTIVERn_OFF_START < GIC_DIST_REG_ICACTIVERn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICACTIVERn_OFF_START) / cbReg;
            return gicDistWriteIntrClearActiveReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ICACTIVERnE_OFF_START < GIC_DIST_REG_ICACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrActive) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICACTIVERnE_OFF_START) / cbReg;
            return gicDistWriteIntrClearActiveReg(pVM, pGicDev, idxReg, uValue);
        }
    }

    /*
     * GICD_IPRIORITYR<n> and GICD_IPRIORITYR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START < GIC_DIST_REG_IPRIORITYRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START) / cbReg;
            return gicDistWriteIntrPriorityReg(pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_IPRIORITYRnE_OFF_START < GIC_DIST_REG_IPRIORITYRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->abIntrPriority) / (2 * sizeof(uint32_t));
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_IPRIORITYRnE_OFF_START) / cbReg;
            return gicDistWriteIntrPriorityReg(pGicDev, idxReg, uValue);
        }
    }

    /*
     * GICD_ISPENDR<n> and GICD_ISPENDR<n>E.
     * GICD_ICPENDR<n> and GICD_ICPENDR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ISPENDRn_OFF_START < GIC_DIST_REG_ISPENDRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ISPENDRn_OFF_START) / cbReg;
            return gicDistWriteIntrSetPendingReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ISPENDRnE_OFF_START < GIC_DIST_REG_ISPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrPending) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ISPENDRnE_OFF_START) / cbReg;
            return gicDistWriteIntrSetPendingReg(pVM, pGicDev, idxReg, uValue);
        }

        if (offReg - GIC_DIST_REG_ICPENDRn_OFF_START < GIC_DIST_REG_ICPENDRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICPENDRn_OFF_START) / cbReg;
            return gicDistWriteIntrClearPendingReg(pVM, pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ICPENDRnE_OFF_START < GIC_DIST_REG_ICPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrPending) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICPENDRnE_OFF_START) / cbReg;
            return gicDistWriteIntrClearPendingReg(pVM, pGicDev, idxReg, uValue);
        }
    }

    /*
     * GICD_ICFGR<n> and GICD_ICFGR<n>E.
     */
    {
        if (offReg - GIC_DIST_REG_ICFGRn_OFF_START < GIC_DIST_REG_ICFGRn_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_DIST_REG_ICFGRn_OFF_START) / cbReg;
            return gicDistWriteIntrConfigReg(pGicDev, idxReg, uValue);
        }
        if (offReg - GIC_DIST_REG_ICFGRnE_OFF_START < GIC_DIST_REG_ICFGRnE_RANGE_SIZE)
        {
            uint16_t const idxExt = RT_ELEMENTS(pGicDev->bmIntrConfig) / 2;
            uint16_t const idxReg = idxExt + (offReg - GIC_DIST_REG_ICFGRnE_OFF_START) / cbReg;
            return gicDistWriteIntrConfigReg(pGicDev, idxReg, uValue);
        }
    }

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GIC_DIST_REG_CTLR_OFF:
#if 0
            ASMAtomicWriteBool(&pThis->fIrqGrp0Enabled, RT_BOOL(uValue & GIC_DIST_REG_CTRL_ENABLE_GRP0));
            ASMAtomicWriteBool(&pThis->fIrqGrp1Enabled, RT_BOOL(uValue & GIC_DIST_REG_CTRL_ENABLE_GRP1_NS));
            Assert(!(uValue & GIC_DIST_REG_CTRL_ARE_NS));
            rcStrict = gicDistUpdateIrqState(pVM, pThis);
#else
            Assert(!(uValue & GIC_DIST_REG_CTRL_ARE_NS));
            pGicDev->fIntrGroup0Enabled = RT_BOOL(uValue & GIC_DIST_REG_CTRL_ENABLE_GRP0);
            pGicDev->fIntrGroup1Enabled = RT_BOOL(uValue & GIC_DIST_REG_CTRL_ENABLE_GRP1_NS);
            rcStrict = gicDistUpdateIrqState(pVM, pGicDev);
#endif
            break;
        case GIC_DIST_REG_STATUSR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SETSPI_NSR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_CLRSPI_NSR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SETSPI_SR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_CLRSPI_SR_OFF:
            AssertReleaseFailed();
            break;
#if 0
        case GIC_DIST_REG_IGROUPRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_IGROUPRn_OFF_START + 4: /* Only 32 lines for now. */
            ASMAtomicOrU32(&pThis->u32RegIGrp0, uValue);
            rcStrict = gicDistUpdateIrqState(pVM, pThis);
            break;
        case GIC_DIST_REG_ISENABLERn_OFF_START + 4: /* Only 32 lines for now. */
            ASMAtomicOrU32(&pThis->bmIntEnabled, uValue);
            rcStrict = gicDistUpdateIrqState(pVM, pThis);
            break;
        case GIC_DIST_REG_ICENABLERn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ICENABLERn_OFF_START + 4: /* Only 32 lines for now. */
            ASMAtomicAndU32(&pThis->bmIntEnabled, ~uValue);
            rcStrict = gicDistUpdateIrqState(pVM, pThis);
            break;
        case GIC_DIST_REG_ISPENDRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ICPENDRn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ISACTIVERn_OFF_START: /* Only 32 lines for now. */
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_ICACTIVERn_OFF_START + 4: /* Only 32 lines for now. */
            ASMAtomicAndU32(&pThis->bmIntActive, ~uValue);
            rcStrict = gicDistUpdateIrqState(pVM, pThis);
            break;
        case GIC_DIST_REG_IPRIORITYRn_OFF_START: /* These are banked for the PEs and access the redistributor. */
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 4:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 8:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 12:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 16:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 20:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 24:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 28:
        {
            PGICCPU pGicVCpu = VMCPU_TO_GICCPU(pVCpu);

            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pGicVCpu->abIntPriority) - sizeof(uint32_t));
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
            {
                pGicVCpu->abIntPriority[i] = (uint8_t)(uValue & 0xff);
                uValue >>= 8;
            }
            break;
        }
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 32: /* Only 32 lines for now. */
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 36:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 40:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 44:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 48:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 52:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 56:
        case GIC_DIST_REG_IPRIORITYRn_OFF_START + 60:
        {
            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_DIST_REG_IPRIORITYRn_OFF_START - 32;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
            {
#if 1
                /** @todo r=aeichner This gross hack prevents Windows from hanging during boot because
                 * it tries to set the interrupt priority for PCI interrupt lines to 0 which will cause an interrupt
                 * storm later on because the lowest interrupt priority Windows seems to use is 32 for the per vCPU
                 * timer.
                 */
                if ((uValue & 0xff) == 0)
                {
                    uValue >>= 8;
                    continue;
                }
#endif
                pThis->abIntPriority[i] = (uint8_t)(uValue & 0xff);
                uValue >>= 8;
            }
            break;
        }
#endif
        case GIC_DIST_REG_ITARGETSRn_OFF_START:
            AssertReleaseFailed();
            break;
#if 0
        case GIC_DIST_REG_ICFGRn_OFF_START + 8: /* Only 32 lines for now. */
            ASMAtomicWriteU32(&pThis->u32RegICfg0, uValue);
            break;
        case GIC_DIST_REG_ICFGRn_OFF_START+ 12:
            ASMAtomicWriteU32(&pThis->u32RegICfg1, uValue);
            break;
#endif
        case GIC_DIST_REG_IGRPMODRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_NSACRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SGIR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_CPENDSGIRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_SPENDSGIRn_OFF_START:
            AssertReleaseFailed();
            break;
        case GIC_DIST_REG_INMIn_OFF_START:
            AssertReleaseFailed();
            break;
        default:
        {
            /* Windows 11 arm64 (24H2) writes zeroes into these reserved registers. We ignore them. */
            if (offReg >= 0x7fe0 && offReg <= 0x7ffc)
                LogFlowFunc(("Bad guest writing to reserved GIC distributor register space [0x7fe0..0x7ffc] -- ignoring!"));
            else
                AssertReleaseMsgFailed(("offReg=%#x uValue=%#RX32\n", offReg, uValue));
            break;
        }
    }

    return rcStrict;
}


/**
 * Reads a GIC redistributor register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   idRedist        The redistributor ID.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistReadRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint32_t idRedist, uint16_t offReg, uint32_t *puValue)
{
    PGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    AssertRelease(idRedist == pVCpu->idCpu);
    switch (offReg)
    {
        case GIC_REDIST_REG_TYPER_OFF:
        {
            PCVMCC pVM = pVCpu->CTX_SUFF(pVM);
            *puValue = (pVCpu->idCpu == pVM->cCpus - 1 ? GIC_REDIST_REG_TYPER_LAST : 0)
                     | GIC_REDIST_REG_TYPER_CPU_NUMBER_SET(idRedist)
                     | GIC_REDIST_REG_TYPER_CMN_LPI_AFF_SET(GIC_REDIST_REG_TYPER_CMN_LPI_AFF_ALL)
                     | (pGicDev->fExtPpi ? GIC_REDIST_REG_TYPER_PPI_NUM_SET(pGicDev->uMaxExtPpi) : 0);
            Assert(!pGicDev->fExtPpi || pGicDev->uMaxExtPpi > 0);
            break;
        }
        case GIC_REDIST_REG_IIDR_OFF:
            *puValue = 0x43b;    /* JEP106 code 0x43b is an ARM implementation. */
            break;
        case GIC_REDIST_REG_TYPER_AFFINITY_OFF:
            *puValue = idRedist;
            break;
        case GIC_REDIST_REG_PIDR2_OFF:
            Assert(pGicDev->uArchRev <= GIC_DIST_REG_PIDR2_ARCH_REV_GICV4);
            *puValue = GIC_REDIST_REG_PIDR2_ARCH_REV_SET(pGicDev->uArchRev);
            break;
        default:
            *puValue = 0;
    }
    return VINF_SUCCESS;
}


/**
 * Reads a GIC redistributor SGI/PPI frame register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistReadSgiPpiRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns);

    PGICCPU  pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    PCGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    uint16_t const cbReg = sizeof(uint32_t);

#if 1
    /*
     * GICR_IGROUPR0 and GICR_IGROUPR<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF < GIC_REDIST_SGI_PPI_REG_IGROUPRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF) / cbReg;
            return gicReDistReadIntrGroupReg(pGicDev, pGicCpu, idxReg, puValue);
        }
    }

    /*
     * GICR_ISENABLER0 and GICR_ISENABLER<n>E.
     * GICR_ICENABLER0 and GICR_ICENABLER<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF < GIC_REDIST_SGI_PPI_REG_ISENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF) / cbReg;
            return gicReDistReadIntrEnableReg(pGicDev, pGicCpu, idxReg, puValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF < GIC_REDIST_SGI_PPI_REG_ICENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICENABLERnE_OFF_START) / cbReg;
            return gicReDistReadIntrEnableReg(pGicDev, pGicCpu, idxReg, puValue);
        }
    }

    /*
     * GICR_ISACTIVER0 and GICR_ISACTIVER<n>E.
     * GICR_ICACTIVER0 and GICR_ICACTIVER<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF < GIC_REDIST_SGI_PPI_REG_ISACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF) / cbReg;
            return gicReDistReadIntrActiveReg(pGicCpu, idxReg, puValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF < GIC_REDIST_SGI_PPI_REG_ICACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF) / cbReg;
            return gicReDistReadIntrActiveReg(pGicCpu, idxReg, puValue);
        }
    }

    /*
     * GICR_ISPENDR0 and GICR_ISPENDR<n>E.
     * GICR_ICPENDR0 and GICR_ICPENDR<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF < GIC_REDIST_SGI_PPI_REG_ISPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF) / cbReg;
            return gicReDistReadIntrPendingReg(pGicDev, pGicCpu, idxReg, puValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF < GIC_REDIST_SGI_PPI_REG_ICPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF) / cbReg;
            return gicReDistReadIntrPendingReg(pGicDev, pGicCpu, idxReg, puValue);
        }
    }

    /*
     * GICR_IPRIORITYR<n> and GICR_IPRIORITYR<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START < GIC_REDIST_SGI_PPI_REG_IPRIORITYRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START) / cbReg;
            return gicReDistReadIntrPriorityReg(pGicDev, pGicCpu, idxReg, puValue);
        }
    }

    /*
     * GICR_ICFGR0, GICR_ICFGR1 and GICR_ICFGR<n>E.
     */
    {
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF < GIC_REDIST_SGI_PPI_REG_ICFGRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF) / cbReg;
            return gicReDistReadIntrConfigReg(pGicDev, pGicCpu, idxReg, puValue);
        }
    }

    AssertReleaseFailed();
    *puValue = 0;
    return VINF_SUCCESS;
#else
    switch (offReg)
    {
        case GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntEnabled);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntPending);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntActive);
            break;
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START +  4:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START +  8:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 12:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 16:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 20:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 24:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 28:
        {
            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));

            uint32_t u32Value = 0;
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
                u32Value |= pThis->abIntPriority[i] << ((i - idxPrio) * 8);

            *puValue = u32Value;
            break;
        }
        case GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->u32RegICfg0);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICFGR1_OFF:
            *puValue = ASMAtomicReadU32(&pThis->u32RegICfg1);
            break;
        default:
            AssertReleaseFailed();
            *puValue = 0;
    }

    return VINF_SUCCESS;
#endif
}


/**
 * Writes a GIC redistributor frame register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistWriteRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, uValue);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GIC_REDIST_REG_STATUSR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_WAKER_OFF:
            Assert(uValue == 0);
            break;
        case GIC_REDIST_REG_PARTIDR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_SETLPIR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_CLRLPIR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_PROPBASER_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_PENDBASER_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_INVLPIR_OFF:
            AssertReleaseFailed();
            break;
        case GIC_REDIST_REG_INVALLR_OFF:
            AssertReleaseFailed();
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    return rcStrict;
}


/**
 * Writes a GIC redistributor SGI/PPI frame register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistWriteSgiPpiRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);

#if 1
    PCGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PCGICDEV);

    /*
     * GICR_IGROUPR0 and GICR_IGROUPR<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF < GIC_REDIST_SGI_PPI_REG_IGROUPRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF) / cbReg;
            return gicReDistWriteIntrGroupReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    /*
     * GICR_ISENABLER0 and GICR_ISENABLER<n>E.
     * GICR_ICENABLER0 and GICR_ICENABLER<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF < GIC_REDIST_SGI_PPI_REG_ISENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF) / cbReg;
            return gicReDistWriteIntrSetEnableReg(pGicDev, pVCpu, idxReg, uValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF < GIC_REDIST_SGI_PPI_REG_ICENABLERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF) / cbReg;
            return gicReDistWriteIntrClearEnableReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    /*
     * GICR_ISACTIVER0 and GICR_ISACTIVER<n>E.
     * GICR_ICACTIVER0 and GICR_ICACTIVER<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF < GIC_REDIST_SGI_PPI_REG_ISACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF) / cbReg;
            return gicReDistWriteIntrSetActiveReg(pGicDev, pVCpu, idxReg, uValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF < GIC_REDIST_SGI_PPI_REG_ICACTIVERnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF) / cbReg;
            return gicReDistWriteIntrClearActiveReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    /*
     * GICR_ISPENDR0 and GICR_ISPENDR<n>E.
     * GICR_ICPENDR0 and GICR_ICPENDR<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF < GIC_REDIST_SGI_PPI_REG_ISPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF) / cbReg;
            return gicReDistWriteIntrSetPendingReg(pGicDev, pVCpu, idxReg, uValue);
        }
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF < GIC_REDIST_SGI_PPI_REG_ICPENDRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF) / cbReg;
            return gicReDistWriteIntrClearPendingReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    /*
     * GICR_IPRIORITYR<n> and GICR_IPRIORITYR<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START < GIC_REDIST_SGI_PPI_REG_IPRIORITYRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START) / cbReg;
            return gicReDistWriteIntrPriorityReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    /*
     * GICR_ICFGR0, GIC_ICFGR1 and GICR_ICFGR<n>E.
     */
    {
        uint16_t const cbReg = sizeof(uint32_t);
        if (offReg - GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF < GIC_REDIST_SGI_PPI_REG_ICFGRnE_RANGE_SIZE)
        {
            uint16_t const idxReg = (offReg - GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF) / cbReg;
            return gicReDistWriteIntrConfigReg(pGicDev, pVCpu, idxReg, uValue);
        }
    }

    AssertReleaseMsgFailed(("offReg=%#RX16\n", offReg));
    return VERR_INTERNAL_ERROR_2;
#else
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF:
            ASMAtomicOrU32(&pThis->u32RegIGrp0, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF:
            ASMAtomicOrU32(&pThis->bmIntEnabled, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF:
            ASMAtomicAndU32(&pThis->bmIntEnabled, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF:
            ASMAtomicOrU32(&pThis->bmIntPending, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF:
            ASMAtomicAndU32(&pThis->bmIntPending, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF:
            ASMAtomicOrU32(&pThis->bmIntActive, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF:
            ASMAtomicAndU32(&pThis->bmIntActive, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START +  4:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START +  8:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 12:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 16:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 20:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 24:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START + 28:
        {
            /* Figure out the register which is written. */
            uint8_t idxPrio = offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYRn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
            {
                pThis->abIntPriority[i] = (uint8_t)(uValue & 0xff);
                uValue >>= 8;
            }
            break;
        }
        case GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF:
            ASMAtomicWriteU32(&pThis->u32RegICfg0, uValue);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICFGR1_OFF:
            ASMAtomicWriteU32(&pThis->u32RegICfg1, uValue);
            break;
        default:
            //AssertReleaseFailed();
            break;
    }
    return rcStrict;
#endif
}


/**
 * @interface_method_impl{PDMGICBACKEND,pfnSetSpi}
 */
static DECLCALLBACK(int) gicSetSpi(PVMCC pVM, uint32_t uSpiIntId, bool fAsserted)
{
    LogFlowFunc(("pVM=%p uSpiIntId=%u fAsserted=%RTbool\n",
                 pVM, uSpiIntId, fAsserted));

    PGIC       pGic    = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);
    PGICDEV    pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);

#ifdef VBOX_WITH_STATISTICS
    PVMCPU pVCpu = VMMGetCpuById(pVM, 0);
    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatSetSpi));
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
#endif

#if 0
    /* Update the interrupts pending state. */
    if (fAsserted)
        ASMAtomicOrU32(&pThis->bmIntPending, RT_BIT_32(uIntId));
    else
        ASMAtomicAndU32(&pThis->bmIntPending, ~RT_BIT_32(uIntId));

    int rc = VBOXSTRICTRC_VAL(gicDistUpdateIrqState(pVM, pThis));
#else
    STAM_PROFILE_START(&pGicCpu->CTX_SUFF_Z(StatProfSetSpi), a);

    uint16_t const uIntId  = GIC_INTID_RANGE_SPI_START + uSpiIntId;
    uint16_t const idxIntr = gicDistGetIndexFromIntId(uIntId);

    Assert(idxIntr >= GIC_INTID_RANGE_SPI_START);
    AssertMsgReturn(idxIntr < sizeof(pGicDev->bmIntrPending) * 8,
                    ("out-of-range SPI interrupt ID %RU32 (%RU32)\n", uIntId, uSpiIntId),
                    VERR_INVALID_PARAMETER);

    int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /* Update the interrupt pending state. */
    if (fAsserted)
        ASMBitSet(&pGicDev->bmIntrPending[0], idxIntr);
    else
        ASMBitClear(&pGicDev->bmIntrPending[0], idxIntr);

    int const rc = VBOXSTRICTRC_VAL(gicDistUpdateIrqState(pVM, pGicDev));
    STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF_Z(StatProfSetSpi), a);
#endif

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return rc;
}


/**
 * @interface_method_impl{PDMGICBACKEND,pfnSetPpi}
 */
static DECLCALLBACK(int) gicSetPpi(PVMCPUCC pVCpu, uint32_t uPpiIntId, bool fAsserted)
{
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} uPpiIntId=%u fAsserted=%RTbool\n", pVCpu, pVCpu->idCpu, uPpiIntId, fAsserted));

    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);
    PCGICDEV   pGicDev = PDMDEVINS_2_DATA(pDevIns, PCGICDEV);
    PGICCPU    pGicCpu = VMCPU_TO_GICCPU(pVCpu);

#if 0
    AssertReturn(uIntId <= (GIC_INTID_RANGE_PPI_LAST - GIC_INTID_RANGE_PPI_START), VERR_INVALID_PARAMETER);
    int rc = gicReDistInterruptSet(pVCpu, uIntId + GIC_INTID_RANGE_PPI_START, fAsserted);
#else
    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatSetPpi));
    STAM_PROFILE_START(&pGicCpu->CTX_SUFF_Z(StatProfSetPpi), b);

    uint32_t const uIntId  = GIC_INTID_RANGE_PPI_START + uPpiIntId;
    uint16_t const idxIntr = gicReDistGetIndexFromIntId(uIntId);

    Assert(idxIntr >= GIC_INTID_RANGE_PPI_START);
    AssertMsgReturn(idxIntr < sizeof(pGicCpu->bmIntrPending) * 8,
                    ("out-of-range PPI interrupt ID %RU32 (%RU32)\n", uIntId, uPpiIntId),
                    VERR_INVALID_PARAMETER);

    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    /* Update the interrupt pending state. */
    if (fAsserted)
        ASMBitSet(&pGicCpu->bmIntrPending[0], idxIntr);
    else
        ASMBitClear(&pGicCpu->bmIntrPending[0], idxIntr);

    int const rc = VBOXSTRICTRC_VAL(gicReDistUpdateIrqState(pGicDev, pVCpu));
    STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF_Z(StatProfSetPpi), b);
#endif

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return rc;
}


/**
 * Sets the specified software generated interrupt (SGI).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pDestCpuSet     Which CPUs to deliver the SGI to.
 * @param   uIntId          The SGI interrupt ID.
 */
static VBOXSTRICTRC gicSetSgi(PCGICDEV pGicDev, PVMCPUCC pVCpu, PCVMCPUSET pDestCpuSet, uint8_t uIntId)
{
#if 0
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} uIntId=%u fAsserted=%RTbool\n",
                 pVCpu, pVCpu->idCpu, uIntId, fAsserted));

    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);
    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    AssertReturn(uIntId <= (GIC_INTID_RANGE_SGI_LAST - GIC_INTID_RANGE_SGI_START), VERR_INVALID_PARAMETER);
    int rc = gicReDistInterruptSet(pVCpu, uIntId + GIC_INTID_RANGE_SGI_START, fAsserted);
    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return rc;
#else
    LogFlowFunc(("pVCpu=%p{.idCpu=%u} uIntId=%u\n", pVCpu, pVCpu->idCpu, uIntId));

    PPDMDEVINS     pDevIns = VMCPU_TO_DEVINS(pVCpu);
    PCVMCC         pVM     = pVCpu->CTX_SUFF(pVM);
    uint32_t const cCpus   = pVM->cCpus;
    AssertReturn(uIntId <= GIC_INTID_RANGE_SGI_LAST, VERR_INVALID_PARAMETER);
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, pDevIns->pCritSectRoR3)); RT_NOREF_PV(pDevIns);

    for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        if (VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu))
        {
            PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVM->CTX_SUFF(apCpus)[idCpu]);
            pGicCpu->bmIntrPending[0] |= RT_BIT_32(uIntId);
        }

    return gicDistUpdateIrqState(pVM, pGicDev);
#endif
}


/**
 * Writes to the redistributor's SGI group 1 register (ICC_SGI1R_EL1).
 *
 * @returns Strict VBox status code.
 * @param   pGicDev     The GIC distributor state.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uValue      The value being written to the ICC_SGI1R_EL1 register.
 */
static VBOXSTRICTRC gicReDistWriteSgiReg(PCGICDEV pGicDev, PVMCPUCC pVCpu, uint64_t uValue)
{
#ifdef VBOX_WITH_STATISTICS
    PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatSetSgi));
    STAM_PROFILE_START(&pGicCpu->CTX_SUFF_Z(StatProfSetSgi), c);
#else
    PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);
#endif

    VMCPUSET DestCpuSet;
    if (uValue & ARMV8_ICC_SGI1R_EL1_AARCH64_IRM)
    {
        /*
         * Deliver to all VCPUs but this one.
         */
        VMCPUSET_FILL(&DestCpuSet);
        VMCPUSET_DEL(&DestCpuSet, pVCpu->idCpu);
    }
    else
    {
        /*
         * Target specific VCPUs.
         * See ARM GICv3 and GICv4 Software Overview spec 3.3 "Affinity routing".
         */
        VMCPUSET_EMPTY(&DestCpuSet);
        bool const     fRangeSelSupport = RT_BOOL(pGicCpu->uIccCtlr & ARMV8_ICC_CTLR_EL1_AARCH64_RSS);
        uint8_t const  idRangeStart     = ARMV8_ICC_SGI1R_EL1_AARCH64_RS_GET(uValue) * 16;
        uint16_t const bmCpuInterfaces  = ARMV8_ICC_SGI1R_EL1_AARCH64_TARGET_LIST_GET(uValue);
        uint8_t const  uAff1            = ARMV8_ICC_SGI1R_EL1_AARCH64_AFF1_GET(uValue);
        uint8_t const  uAff2            = ARMV8_ICC_SGI1R_EL1_AARCH64_AFF2_GET(uValue);
        uint8_t const  uAff3            = (pGicCpu->uIccCtlr & ARMV8_ICC_CTLR_EL1_AARCH64_A3V)
                                        ? ARMV8_ICC_SGI1R_EL1_AARCH64_AFF3_GET(uValue)
                                        : 0;
        uint32_t const cCpus            = pVCpu->CTX_SUFF(pVM)->cCpus;
        for (uint8_t idCpuInterface = 0; idCpuInterface < 16; idCpuInterface++)
        {
            if (bmCpuInterfaces & RT_BIT(idCpuInterface))
            {
                VMCPUID idCpuTarget;
                if (fRangeSelSupport)
                    idCpuTarget = RT_MAKE_U32_FROM_U8(idRangeStart + idCpuInterface, uAff1, uAff2, uAff3);
                else
                    idCpuTarget = gicGetCpuIdFromAffinity(idCpuInterface, uAff1, uAff2, uAff3);
                if (RT_LIKELY(idCpuTarget < cCpus))
                    VMCPUSET_ADD(&DestCpuSet, idCpuTarget);
                else
                    AssertReleaseFailed();
            }
        }
    }

    if (!VMCPUSET_IS_EMPTY(&DestCpuSet))
    {
        uint8_t const uSgiIntId = ARMV8_ICC_SGI1R_EL1_AARCH64_INTID_GET(uValue);
        Assert(GIC_IS_INTR_SGI(uSgiIntId));
#if 0
        uint16_t const idxIdb = uSgiIntId;
        gicPostInterrupt(pVCpu, &DestCpuSet, idxIdb);
#else
        VBOXSTRICTRC const rcStrict = gicSetSgi(pGicDev, pVCpu, &DestCpuSet, uSgiIntId);
        Assert(RT_SUCCESS(rcStrict)); RT_NOREF_PV(rcStrict);
#endif
    }

    STAM_PROFILE_STOP(&pGicCpu->CTX_SUFF_Z(StatProfSetSgi), c);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMGICBACKEND,pfnReadSysReg}
 */
static DECLCALLBACK(VBOXSTRICTRC) gicReadSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pu64Value);

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatSysRegRead));

    *pu64Value = 0;
    PGICCPU    pGicCpu = VMCPU_TO_GICCPU(pVCpu);
    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);
    PGICDEV    pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);

    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_ICC_PMR_EL1:
            *pu64Value = pGicCpu->bIntrPriorityMask;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR0_EL1:
            *pu64Value = ARMV8_ICC_BPR0_EL1_AARCH64_BINARYPOINT_SET(pGicCpu->bBinaryPtGroup0);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R0_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup0[0];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R1_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup0[1];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R2_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup0[2];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup0[3];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup1[0];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R1_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup1[1];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R2_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup1[2];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R3_EL1:
            AssertReleaseFailed();
            *pu64Value = pGicCpu->bmActivePriorityGroup1[3];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_DIR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_RPR_EL1:
            *pu64Value = pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority];
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_ASGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1:
        {
#if 0
            /** @todo Figure out the highest priority interrupt. */
            uint32_t bmIntActive = ASMAtomicReadU32(&pThis->bmIntActive);
            uint32_t bmIntEnabled = ASMAtomicReadU32(&pThis->bmIntEnabled);
            uint32_t bmPending = (ASMAtomicReadU32(&pThis->bmIntPending) & bmIntEnabled) & ~bmIntActive;
            int32_t idxIntPending = ASMBitFirstSet(&bmPending, sizeof(bmPending) * 8);
            if (idxIntPending > -1)
            {
                /* Mark the interrupt as active. */
                ASMAtomicOrU32(&pThis->bmIntActive, RT_BIT_32(idxIntPending));
                /* Drop priority. */
                Assert((uint32_t)idxIntPending < RT_ELEMENTS(pThis->abIntPriority));
                Assert(pThis->idxRunningPriority < RT_ELEMENTS(pThis->abRunningPriorities) - 1);

                LogFlowFunc(("Dropping interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                             pThis->abRunningPriorities[pThis->idxRunningPriority],
                             pThis->abIntPriority[idxIntPending],
                             pThis->idxRunningPriority, pThis->idxRunningPriority + 1));

                pThis->abRunningPriorities[++pThis->idxRunningPriority] = pThis->abIntPriority[idxIntPending];

                /* Clear edge level interrupts like SGIs as pending. */
                if (idxIntPending <= GIC_INTID_RANGE_SGI_LAST)
                    ASMAtomicBitClear(&pThis->bmIntPending, idxIntPending);
                *pu64Value = idxIntPending;
                gicReDistUpdateIrqState(pThis, pVCpu);
            }
            else
            {
                /** @todo This is wrong as the guest might decide to prioritize PPIs and SPIs differently. */
                bmIntActive = ASMAtomicReadU32(&pGicDev->bmIntActive);
                bmIntEnabled = ASMAtomicReadU32(&pGicDev->bmIntEnabled);
                bmPending = (ASMAtomicReadU32(&pGicDev->bmIntPending) & bmIntEnabled) & ~bmIntActive;
                idxIntPending = ASMBitFirstSet(&bmPending, sizeof(bmPending) * 8);
                if (   idxIntPending > -1
                    && pGicDev->abIntPriority[idxIntPending] < pThis->bInterruptPriority)
                {
                    /* Mark the interrupt as active. */
                    ASMAtomicOrU32(&pGicDev->bmIntActive, RT_BIT_32(idxIntPending));

                    /* Drop priority. */
                    Assert((uint32_t)idxIntPending < RT_ELEMENTS(pGicDev->abIntPriority));
                    Assert(pThis->idxRunningPriority < RT_ELEMENTS(pThis->abRunningPriorities) - 1);

                    LogFlowFunc(("Dropping interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                                 pThis->abRunningPriorities[pThis->idxRunningPriority],
                                 pThis->abIntPriority[idxIntPending],
                                 pThis->idxRunningPriority, pThis->idxRunningPriority + 1));

                    pThis->abRunningPriorities[++pThis->idxRunningPriority] = pGicDev->abIntPriority[idxIntPending];

                    *pu64Value = idxIntPending + GIC_INTID_RANGE_SPI_START;
                    gicReDistUpdateIrqState(pThis, pVCpu);
                }
                else
                    *pu64Value = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
            }
#else
            *pu64Value = gicAckHighestPriorityPendingIntr(pGicDev, pVCpu, false /*fGroup0*/, true /*fGroup1*/);
#endif
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_EOIR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR1_EL1:
        {
#if 0
            /** @todo Figure out the highest priority interrupt. */
            uint32_t bmIntActive = ASMAtomicReadU32(&pThis->bmIntActive);
            uint32_t bmIntEnabled = ASMAtomicReadU32(&pThis->bmIntEnabled);
            uint32_t bmPending = (ASMAtomicReadU32(&pThis->bmIntPending) & bmIntEnabled) & ~bmIntActive;
            int32_t idxIntPending = ASMBitFirstSet(&bmPending, sizeof(bmPending) * 8);
            if (idxIntPending > -1)
                *pu64Value = idxIntPending;
            else
            {
                /** @todo This is wrong as the guest might decide to prioritize PPIs and SPIs differently. */
                bmIntActive = ASMAtomicReadU32(&pGicDev->bmIntActive);
                bmIntEnabled = ASMAtomicReadU32(&pGicDev->bmIntEnabled);
                bmPending = (ASMAtomicReadU32(&pGicDev->bmIntPending) & bmIntEnabled) & ~bmIntActive;
                idxIntPending = ASMBitFirstSet(&bmPending, sizeof(bmPending) * 8);
                if (idxIntPending > -1)
                    *pu64Value = idxIntPending + GIC_INTID_RANGE_SPI_START;
                else
                    *pu64Value = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
            }
#else
            AssertReleaseFailed();
            *pu64Value = gicGetHighestPriorityPendingIntr(pGicDev, pGicCpu, false /*fGroup0*/, true /*fGroup1*/,
                                                          NULL /*pidxIntr*/, NULL /*pbPriority*/);
#endif
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_BPR1_EL1:
            *pu64Value = ARMV8_ICC_BPR1_EL1_AARCH64_BINARYPOINT_SET(pGicCpu->bBinaryPtGroup1);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_CTLR_EL1:
            *pu64Value = pGicCpu->uIccCtlr;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SRE_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN0_EL1:
            *pu64Value = pGicCpu->fIntrGroup0Enabled ? ARMV8_ICC_IGRPEN0_EL1_AARCH64_ENABLE : 0;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1:
            *pu64Value = pGicCpu->fIntrGroup1Enabled ? ARMV8_ICC_IGRPEN1_EL1_AARCH64_ENABLE : 0;
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);

    LogFlowFunc(("pVCpu=%p u32Reg=%#x{%s} pu64Value=%RX64\n", pVCpu, u32Reg, gicIccGetRegDescription(u32Reg), *pu64Value));
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMGICBACKEND,pfnWriteSysReg}
 */
static DECLCALLBACK(VBOXSTRICTRC) gicWriteSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    LogFlowFunc(("pVCpu=%p u32Reg=%#x{%s} u64Value=%RX64\n", pVCpu, u32Reg, gicIccGetRegDescription(u32Reg), u64Value));

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatSysRegWrite));

    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);
    PGICDEV    pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    PGICCPU    pGicCpu = VMCPU_TO_GICCPU(pVCpu);

    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_ICC_PMR_EL1:
            LogFlowFunc(("ICC_PMR_EL1: Interrupt priority now %u\n", (uint8_t)u64Value));
#if 0
            ASMAtomicWriteU8(&pThis->bInterruptPriority, (uint8_t)u64Value);
            gicReDistUpdateIrqState(pThis, pVCpu);
#else
            pGicCpu->bIntrPriorityMask = (uint8_t)u64Value;
            rcStrict = gicReDistUpdateIrqState(pGicDev, pVCpu);
#endif
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR0_EL1:
#if 0
            pThis->bBinaryPointGrp0 = (uint8_t)ARMV8_ICC_BPR0_EL1_AARCH64_BINARYPOINT_GET(u64Value);
#else
            pGicCpu->bBinaryPtGroup0 = (uint8_t)ARMV8_ICC_BPR0_EL1_AARCH64_BINARYPOINT_GET(u64Value);
#endif
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R0_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP0R1_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP0R2_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP1R1_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP1R2_EL1:
        case ARMV8_AARCH64_SYSREG_ICC_AP1R3_EL1:
            /* Writes ignored, well behaving guest would write all 0s or the last read value of the register. */
            break;
        case ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_DIR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_RPR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI1R_EL1:
        {
#if 0
            uint32_t uIntId = ARMV8_ICC_SGI1R_EL1_AARCH64_INTID_GET(u64Value) - GIC_INTID_RANGE_SGI_START;
            if (u64Value & ARMV8_ICC_SGI1R_EL1_AARCH64_IRM)
            {
                /* Route to all but this vCPU. */
                for (uint32_t i = 0; i < pVCpu->pVMR3->cCpus; i++)
                {
                    if (i != pVCpu->idCpu)
                    {
                        PVMCPUCC pVCpuDst = VMMGetCpuById(pVCpu->CTX_SUFF(pVM), i);
                        if (pVCpuDst)
                            gicSetSgi(pVCpuDst, uIntId, true /*fAsserted*/);
                        else
                            AssertFailed();
                    }
                }
            }
            else
            {
                /* Examine target list. */
                /** @todo Range selector support. */
                VMCPUID idCpu = 0;
                uint16_t uTgtList = ARMV8_ICC_SGI1R_EL1_AARCH64_TARGET_LIST_GET(u64Value);
                /** @todo rewrite using ASMBitFirstSetU16. */
                while (uTgtList)
                {
                    if (uTgtList & 0x1)
                    {
                        PVMCPUCC pVCpuDst = VMMGetCpuById(pVCpu->CTX_SUFF(pVM), idCpu);
                        if (pVCpuDst)
                            gicSetSgi(pVCpuDst, uIntId, true /*fAsserted*/);
                        else
                            AssertFailed();
                    }
                    uTgtList >>= 1;
                    idCpu++;
                }
            }
#else
            gicReDistWriteSgiReg(pGicDev, pVCpu, u64Value);
#endif
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_ASGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR1_EL1:
        {
#if 0
            /* Mark the interrupt as not active anymore, though it might still be pending. */
            if (u64Value < GIC_INTID_RANGE_SPI_START)
                ASMAtomicAndU32(&pThis->bmIntActive, ~RT_BIT_32((uint32_t)u64Value));
            else
                ASMAtomicAndU32(&pGicDev->bmIntActive, ~RT_BIT_32((uint32_t)(u64Value - GIC_INTID_RANGE_SPI_START)));

            /* Restore previous interrupt priority. */
            Assert(pThis->idxRunningPriority > 0);
            if (RT_LIKELY(pThis->idxRunningPriority))
            {
                LogFlowFunc(("Restoring interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                             pThis->abRunningPriorities[pThis->idxRunningPriority],
                             pThis->abRunningPriorities[pThis->idxRunningPriority - 1],
                             pThis->idxRunningPriority, pThis->idxRunningPriority - 1));
                pThis->idxRunningPriority--;
            }
            gicReDistUpdateIrqState(pThis, pVCpu);
#else
            /*
             * We only support priority drop + interrupt deactivation with writes to this register.
             * This avoids an extra access which would be required by software for deactivation.
             */
            Assert(!(pGicCpu->uIccCtlr & ARMV8_ICC_CTLR_EL1_AARCH64_EOIMODE));

            /*
             * Mark the interrupt as inactive, though it might still be pending.
             * It is up to the guest to ensure the interrupt ID belongs to the right group as
             * failure to do so results in unpredictable behavior.
             *
             * See ARM GIC spec. 12.2.10 "ICC_EOIR1_EL1, Interrupt Controller End Of Interrupt Register 1".
             * NOTE! The order of the 'if' checks below are crucial.
             */
            uint16_t const uIntId = (uint16_t)u64Value;
            if (uIntId <= GIC_INTID_RANGE_PPI_LAST)
            {
                /* SGIs and PPIs. */
                AssertCompile(GIC_INTID_RANGE_PPI_LAST < 8 * sizeof(pGicDev->bmIntrActive[0]));
                Assert(pGicDev->fAffRoutingEnabled);
                pGicCpu->bmIntrActive[0] &= ~RT_BIT_32(uIntId);
            }
            else if (uIntId <= GIC_INTID_RANGE_SPI_LAST)
            {
                /* SPIs. */
                uint16_t const idxIntr = /*gicDistGetIndexFromIntId*/(uIntId);
                AssertReturn(idxIntr < sizeof(pGicDev->bmIntrActive) * 8, VERR_BUFFER_OVERFLOW);
                ASMBitClear(&pGicDev->bmIntrActive[0], idxIntr);
            }
            else if (uIntId <= GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT)
            {
                /* Special interrupt IDs, ignored. */
                Log(("Ignoring write to EOI with special interrupt ID.\n"));
                break;
            }
            else if (uIntId <= GIC_INTID_RANGE_EXT_PPI_LAST)
            {
                /* Extended PPIs. */
                uint16_t const idxIntr = gicReDistGetIndexFromIntId(uIntId);
                AssertReturn(idxIntr < sizeof(pGicCpu->bmIntrActive) * 8, VERR_BUFFER_OVERFLOW);
                ASMBitClear(&pGicCpu->bmIntrActive[0], idxIntr);
            }
            else if (uIntId <= GIC_INTID_RANGE_EXT_SPI_LAST)
            {
                /* Extended SPIs. */
                uint16_t const idxIntr = gicDistGetIndexFromIntId(uIntId);
                AssertReturn(idxIntr < sizeof(pGicDev->bmIntrActive) * 8, VERR_BUFFER_OVERFLOW);
                ASMBitClear(&pGicDev->bmIntrActive[0], idxIntr);
            }
            else
            {
                AssertMsgFailed(("Invalid INTID %u\n", uIntId));
                break;
            }

            /*
             * Drop priority by restoring previous interrupt.
             */
            if (RT_LIKELY(pGicCpu->idxRunningPriority))
            {
                LogFlowFunc(("Restoring interrupt priority from %u -> %u (idxRunningPriority: %u -> %u)\n",
                             pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority],
                             pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority - 1],
                             pGicCpu->idxRunningPriority, pGicCpu->idxRunningPriority - 1));

                /*
                 * Clear the interrupt priority from the active priorities bitmap.
                 * It is up to the guest to ensure that writes to EOI registers are done in the exact
                 * reverse order of the reads from the IAR registers.
                 *
                 * See ARM GIC spec 4.1.1 "Physical CPU interface".
                 */
                uint8_t const idxPreemptionLevel = pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority] >> 1;
                AssertCompile(sizeof(pGicCpu->bmActivePriorityGroup1) * 8 >= 128);
                ASMBitClear(&pGicCpu->bmActivePriorityGroup1[0], idxPreemptionLevel);

                pGicCpu->idxRunningPriority--;
                Assert(pGicCpu->abRunningPriorities[0] == GIC_IDLE_PRIORITY);
            }
            else
                AssertReleaseMsgFailed(("Index of running-priority interrupt out-of-bounds %u\n", pGicCpu->idxRunningPriority));
            rcStrict = gicReDistUpdateIrqState(pGicDev, pVCpu);
#endif
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR1_EL1:
            pGicCpu->bBinaryPtGroup1 = (uint8_t)ARMV8_ICC_BPR1_EL1_AARCH64_BINARYPOINT_GET(u64Value);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_CTLR_EL1:
            pGicCpu->uIccCtlr &= ARMV8_ICC_CTLR_EL1_RW;
            /** @todo */
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SRE_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN0_EL1:
            pGicCpu->fIntrGroup0Enabled = RT_BOOL(u64Value & ARMV8_ICC_IGRPEN0_EL1_AARCH64_ENABLE);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1:
            pGicCpu->fIntrGroup1Enabled = RT_BOOL(u64Value & ARMV8_ICC_IGRPEN1_EL1_AARCH64_ENABLE);
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return rcStrict;
}


/**
 * Initializes the GIC distributor state.
 *
 * @param   pDevIns     The device instance.
 */
static void gicInit(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("\n"));
    PGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    RT_ZERO(pGicDev->bmIntrGroup);
    RT_ZERO(pGicDev->bmIntrConfig);
    RT_ZERO(pGicDev->bmIntrEnabled);
    RT_ZERO(pGicDev->bmIntrPending);
    RT_ZERO(pGicDev->bmIntrActive);
    RT_ZERO(pGicDev->abIntrPriority);
    RT_ZERO(pGicDev->au32IntrRouting);
    RT_ZERO(pGicDev->bmIntrRoutingMode);
    pGicDev->fIntrGroup0Enabled = false;
    pGicDev->fIntrGroup1Enabled = false;
    pGicDev->fAffRoutingEnabled = true; /* GICv2 backwards compatibility is not implemented, so this is RA1/WI. */
}


/**
 * Initialies the GIC redistributor and CPU interface state.
 *
 * @param   pDevIns     The device instance.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static void gicInitCpu(PPDMDEVINS pDevIns, PVMCPUCC pVCpu)
{
    LogFlowFunc(("[%u]\n", pVCpu->idCpu));
    PGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    PGICCPU pGicCpu = &pVCpu->gic.s;

    RT_ZERO(pGicCpu->bmIntrGroup);
    RT_ZERO(pGicCpu->bmIntrConfig);
    /* SGIs are always edge-triggered, writes to GICR_ICFGR0 are to be ignored. */
    pGicCpu->bmIntrConfig[0] = 0xaaaaaaaa;
    RT_ZERO(pGicCpu->bmIntrEnabled);
    RT_ZERO(pGicCpu->bmIntrPending);
    RT_ZERO(pGicCpu->bmIntrActive);
    RT_ZERO(pGicCpu->abIntrPriority);

    pGicCpu->uIccCtlr = ARMV8_ICC_CTLR_EL1_AARCH64_PMHE
                      | ARMV8_ICC_CTLR_EL1_AARCH64_PRIBITS_SET(4)
                      | ARMV8_ICC_CTLR_EL1_AARCH64_IDBITS_SET(ARMV8_ICC_CTLR_EL1_AARCH64_IDBITS_16BITS)
                      | (pGicDev->fRangeSel   ? ARMV8_ICC_CTLR_EL1_AARCH64_RSS : 0)
                      | (pGicDev->fAff3Levels ? ARMV8_ICC_CTLR_EL1_AARCH64_A3V : 0);

    pGicCpu->bIntrPriorityMask  = 0; /* Means no interrupt gets through to the PE. */
    pGicCpu->idxRunningPriority = 0;
    memset((void *)&pGicCpu->abRunningPriorities[0], 0xff, sizeof(pGicCpu->abRunningPriorities));
    RT_ZERO(pGicCpu->bmActivePriorityGroup0);
    RT_ZERO(pGicCpu->bmActivePriorityGroup1);
    pGicCpu->bBinaryPtGroup0    = 0;
    pGicCpu->bBinaryPtGroup1    = 0;
    pGicCpu->fIntrGroup0Enabled = false;
    pGicCpu->fIntrGroup1Enabled = false;
}


/**
 * Initializes per-VM GIC to the state following a power-up or hardware
 * reset.
 *
 * @param   pDevIns     The device instance.
 */
DECLHIDDEN(void) gicReset(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("\n"));
    gicInit(pDevIns);
}


/**
 * Initializes per-VCPU GIC to the state following a power-up or hardware
 * reset.
 *
 * @param   pDevIns     The device instance.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLHIDDEN(void) gicResetCpu(PPDMDEVINS pDevIns, PVMCPUCC pVCpu)
{
    LogFlowFunc(("[%u]\n", pVCpu->idCpu));
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
    gicInitCpu(pDevIns, pVCpu);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu  = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg = off & 0xfffc;
    uint32_t uValue = 0;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    VBOXSTRICTRC rc = VBOXSTRICTRC_VAL(gicDistReadRegister(pDevIns, pVCpu, offReg, &uValue));
    *(uint32_t *)pv = uValue;

    LogFlowFunc(("[%u]: offReg=%#RX16 (%s) uValue=%#RX32\n", pVCpu->idCpu, offReg, gicDistGetRegDescription(offReg), uValue));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu  = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg = off & 0xfffc;
    uint32_t uValue = *(uint32_t *)pv;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));
    LogFlowFunc(("[%u]: offReg=%#RX16 (%s) uValue=%#RX32\n", pVCpu->idCpu, offReg, gicDistGetRegDescription(offReg), uValue));

    return gicDistWriteRegister(pDevIns, pVCpu, offReg, uValue);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    /*
     * Determine the redistributor being targeted. Each redistributor takes
     * GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE bytes
     * and the redistributors are adjacent.
     */
    uint32_t const idReDist = off / (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);
    off %= (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);

    PVMCC pVM = PDMDevHlpGetVM(pDevIns);
    Assert(idReDist < pVM->cCpus);
    PVMCPUCC pVCpu = pVM->CTX_SUFF(apCpus)[idReDist];

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    /* Redistributor or SGI/PPI frame? */
    uint16_t const offReg = off & 0xfffc;
    uint32_t       uValue = 0;
    VBOXSTRICTRC rcStrict;
    if (off < GIC_REDIST_REG_FRAME_SIZE)
        rcStrict = gicReDistReadRegister(pDevIns, pVCpu, idReDist, offReg, &uValue);
    else
        rcStrict = gicReDistReadSgiPpiRegister(pDevIns, pVCpu, offReg, &uValue);

    *(uint32_t *)pv = uValue;
    LogFlowFunc(("[%u]: off=%RGp idReDist=%u offReg=%#RX16 (%s) uValue=%#RX32 -> %Rrc\n", pVCpu->idCpu, off, idReDist, offReg,
                 gicReDistGetRegDescription(offReg), uValue, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    uint32_t uValue = *(uint32_t *)pv;

    /*
     * Determine the redistributor being targeted. Each redistributor takes
     * GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE bytes
     * and the redistributors are adjacent.
     */
    uint32_t const idReDist = off / (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);
    off %= (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);

    PCVMCC pVM = PDMDevHlpGetVM(pDevIns);
    Assert(idReDist < pVM->cCpus);
    PVMCPUCC pVCpu = pVM->CTX_SUFF(apCpus)[idReDist];

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));

    /* Redistributor or SGI/PPI frame? */
    uint16_t const offReg = off & 0xfffc;
    VBOXSTRICTRC rcStrict;
    if (off < GIC_REDIST_REG_FRAME_SIZE)
        rcStrict = gicReDistWriteRegister(pDevIns, pVCpu, offReg, uValue);
    else
        rcStrict = gicReDistWriteSgiPpiRegister(pDevIns, pVCpu, offReg, uValue);

    LogFlowFunc(("[%u]: off=%RGp idReDist=%u offReg=%#RX16 (%s) uValue=%#RX32 -> %Rrc\n", pVCpu->idCpu, off, idReDist, offReg,
                 gicReDistGetRegDescription(offReg), uValue, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


#ifndef IN_RING3
/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) gicRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    AssertReleaseFailed();
    return VINF_SUCCESS;
}
#endif /* !IN_RING3 */


/**
 * GIC device registration structure.
 */
const PDMDEVREG g_DeviceGIC =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "gic",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(GICDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic Interrupt Controller",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           gicR3Construct,
    /* .pfnDestruct = */            gicR3Destruct,
    /* .pfnRelocate = */            gicR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               gicR3Reset,
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
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           gicRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           gicRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};


/**
 * The VirtualBox GIC backend.
 */
const PDMGICBACKEND g_GicBackend =
{
    /* .pfnReadSysReg = */  gicReadSysReg,
    /* .pfnWriteSysReg = */ gicWriteSysReg,
    /* .pfnSetSpi = */      gicSetSpi,
    /* .pfnSetPpi = */      gicSetPpi,
};

