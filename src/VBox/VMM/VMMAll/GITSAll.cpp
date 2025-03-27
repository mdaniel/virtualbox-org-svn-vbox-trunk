/* $Id$ */
/** @file
 * GITS - GIC Interrupt Translation Service (ITS) - All Contexts.
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
#define LOG_GROUP LOG_GROUP_DEV_GIC
#include "GITSInternal.h"

#include <VBox/log.h>
#include <VBox/gic.h>
#include <iprt/errcore.h>       /* VINF_SUCCESS */
#include <iprt/string.h>        /* RT_ZERO */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GITS saved state version. */
#define GITS_SAVED_STATE_VERSION                        1
#define GITS_IS_REG_IN_RANGE(a_offReg, a_offFirst, a_cbRegion)    ((uint32_t)(a_offReg) - (a_offFirst) < (a_cbRegion))


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Interrupt Table Entry (ITE).
 */
typedef struct GITSITE
{
    bool        fValid;
    uint8_t     uType;
    uint16_t    uIntrCollectId;
    VMCPUID     idTargetCpu;
} GITSITE;
AssertCompileSize(GITSITE, 8);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifdef LOG_ENABLED
DECL_HIDDEN_CALLBACK(const char *) gitsGetCtrlRegDescription(uint16_t offReg)
{
    if (GITS_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
        return "GITS_BASER<n>";
    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:    return "GITS_CTLR";
        case GITS_CTRL_REG_IIDR_OFF:    return "GITS_IIDR";
        case GITS_CTRL_REG_TYPER_OFF:   return "GITS_TYPER";
        case GITS_CTRL_REG_MPAMIDR_OFF: return "GITS_MPAMIDR";
        case GITS_CTRL_REG_PARTIDR_OFF: return "GITS_PARTIDR";
        case GITS_CTRL_REG_MPIDR_OFF:   return "GITS_MPIDR";
        case GITS_CTRL_REG_STATUSR_OFF: return "GITS_STATUSR";
        case GITS_CTRL_REG_UMSIR_OFF:   return "GITS_UMSIR";
        case GITS_CTRL_REG_CBASER_OFF:  return "GITS_CBASER";
        case GITS_CTRL_REG_CWRITER_OFF: return "GITS_CWRITER";
        case GITS_CTRL_REG_CREADR_OFF:  return "GITS_CREADR";
        default:
            return "<UNKNOWN>";
    }
}

DECL_HIDDEN_CALLBACK(const char *) gitsGetTranslationRegDescription(uint16_t offReg)
{
    switch (offReg)
    {
        case GITS_TRANSLATION_REG_TRANSLATER:   return "GITS_TRANSLATERR";
        default:
            return "<UNKNOWN>";
    }
}
#endif /* LOG_ENABLED */


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue)
{
    /*
     * GITS_BASER<n>.
     */
    if (GITS_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST,  GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg  = sizeof(uint64_t);
        uint16_t const idxReg = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        if (!(offReg & 7))
            *puValue = pGitsDev->aItsTableRegs[idxReg].s.Lo;
        else
            *puValue = pGitsDev->aItsTableRegs[idxReg].s.Hi;
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            *puValue = RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_ENABLED,   pGitsDev->fEnabled)
                     | RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_QUIESCENT, pGitsDev->fQuiescent);
            break;

        case GITS_CTRL_REG_PIDR2_OFF:
        {
            Assert(pGitsDev->uArchRev <= GITS_CTRL_REG_PIDR2_ARCHREV_GICV4);
            uint8_t const uIdCodeDes1 = GIC_JEDEC_JEP10_DES_1(GIC_JEDEC_JEP106_IDENTIFICATION_CODE);
            *puValue = RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_DES_1,   uIdCodeDes1)
                     | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_JEDEC,   1)
                     | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_ARCHREV, pGitsDev->uArchRev);
            break;
        }

        case GITS_CTRL_REG_IIDR_OFF:
            *puValue = RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_ID_CODE,   GIC_JEDEC_JEP106_IDENTIFICATION_CODE)
                     | RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_CONT_CODE, GIC_JEDEC_JEP106_CONTINUATION_CODE);
            break;

        case GITS_CTRL_REG_TYPER_OFF:
        {
            uint64_t uLo = RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PHYSICAL,       1)               /* Physical LPIs supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VIRTUAL,        0) */            /* Virtual LPIs not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CCT,            0)               /* Collections in memory not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE, sizeof(GITSITE)) /* ITE size in bytes. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ID_BITS,        31)              /* 32-bit event IDs. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_DEV_BITS,       31)              /* 32-bit device IDs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SEIS,           0) */            /** @todo SEI support. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PTA,            0) */            /* Target is VCPU ID not address. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_HCC,            255);            /* Collection count. */
            *puValue = RT_LO_U32(uLo);
            break;
        }

        case GITS_CTRL_REG_TYPER_OFF + 4:
        {
            uint64_t uHi = 0
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CID_BITS, 0) */   /* CIL specifies collection ID size. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CIL,      0) */   /* 16-bit collection IDs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMOVP,    0) */   /* VMOVP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_MPAM,     0) */   /* MPAM no supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VSGI,     0) */   /* VSGI not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMAPP,    0) */   /* VMAPP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SVPET,    0) */   /* SVPET not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_NID,      0) */   /* NID (doorbell) not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI,     0) */   /** @todo Support reporting receipt of unmapped MSIs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI_IRQ, 0) */   /** @todo Support generating interrupt on unmapped MSI. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_INV,      1);     /* ITS caches are invalidated when clearing
                                                                                  GITS_CTLR.Enabled and GITS_BASER<n>.Valid. */
            *puValue = RT_HI_U32(uHi);
            break;
        }

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s)\n", offReg, gitsGetCtrlRegDescription(offReg)));
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uValue=%#RX32\n", offReg, gitsGetCtrlRegDescription(offReg), *puValue));
    return rcStrict;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue)
{
    RT_NOREF(pGitsDev, offReg, puValue);
    AssertReleaseMsgFailed(("offReg=%#x\n", offReg));
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteCtrl(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue)
{
    /*
     * GITS_BASER<n>.
     */
    if (GITS_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST,  GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg  = sizeof(uint64_t);
        uint16_t const idxReg = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        if (!(offReg & 7))
            pGitsDev->aItsTableRegs[idxReg].s.Lo = uValue;
        else
            pGitsDev->aItsTableRegs[idxReg].s.Hi = uValue;
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            pGitsDev->fEnabled = RT_BF_GET(uValue, GITS_BF_CTRL_REG_CTLR_ENABLED);
            break;

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s) uValue=%#RX32\n", offReg, gitsGetCtrlRegDescription(offReg), uValue));
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uValue=%#RX32\n", offReg, gitsGetCtrlRegDescription(offReg), uValue));
    return rcStrict;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue)
{
    RT_NOREF(pGitsDev, offReg, uValue);
    AssertReleaseMsgFailed(("offReg=%#x uValue=%#RX32\n", offReg, uValue));
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CALLBACK(void) gitsInit(PGITSDEV pGitsDev)
{
    Log4Func(("\n"));
    pGitsDev->fEnabled              = false;
    pGitsDev->fUnmappedMsiReporting = false;
    pGitsDev->fQuiescent            = true;
    RT_ZERO(pGitsDev->aItsTableRegs);
}


#ifdef IN_RING3
DECL_HIDDEN_CALLBACK(void) gitsR3DbgInfo(PCGITSDEV pGitsDev, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pGitsDev, pHlp, pszArgs);
    /** @todo Debug info dump. */
}
#endif /* IN_RING3 */


/**
 * @interface_method_impl{PDMGICBACKEND,pfnSendMsi}
 */
DECL_HIDDEN_CALLBACK(int) gitsSendMsi(PVMCC pVM, PCIBDF uBusDevFn, PCMSIMSG pMsi, uint32_t uEventId, uint32_t uTagSrc)
{
    RT_NOREF(pVM, uBusDevFn, pMsi, uEventId, uTagSrc);
    return VERR_NOT_IMPLEMENTED;
}


#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

