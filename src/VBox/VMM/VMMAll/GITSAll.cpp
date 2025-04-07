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
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/dbgf.h>
#include <iprt/errcore.h>       /* VINF_SUCCESS */
#include <iprt/string.h>        /* RT_ZERO */
#include <iprt/mem.h>           /* RTMemAllocZ, RTMemFree */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GITS saved state version. */
#define GITS_SAVED_STATE_VERSION                        1

/** Gets whether the given register offset is within the specified range. */
#define GITS_IS_REG_IN_RANGE(a_offReg, a_offFirst, a_cbRegion)    ((uint32_t)(a_offReg) - (a_offFirst) < (a_cbRegion))

/** Acquire the device critical section. */
#define GITS_CRIT_SECT_ENTER(a_pDevIns) \
    do \
    { \
        int const rcLock = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VINF_SUCCESS); \
        PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock); \
    } while(0)

/** Release the device critical section. */
#define GITS_CRIT_SECT_LEAVE(a_pDevIns)         PDMDevHlpCritSectLeave((a_pDevIns), (a_pDevIns)->CTX_SUFF(pCritSectRo))

/** Returns whether the critical section is held. */
#define GITS_CRIT_SECT_IS_OWNER(a_pDevIns)      PDMDevHlpCritSectIsOwner((a_pDevIns), (a_pDevIns)->CTX_SUFF(pCritSectRo))


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

/** GITS diagnostic enum description expansion.
 * The below construct ensures typos in the input to this macro are caught
 * during compile time. */
#define GITSDIAG_DESC(a_Name)        RT_CONCAT(kGitsDiag_, a_Name) < kGitsDiag_End ? RT_STR(a_Name) : "Ignored"
/** GITS diagnostics description for members in GITSDIAG. */
static const char *const g_apszGitsDiagDesc[] =
{
    GITSDIAG_DESC(None),
    GITSDIAG_DESC(CmdQueue_PhysAddr_Invalid),
    /* kGitsDiag_End */
};
AssertCompile(RT_ELEMENTS(g_apszGitsDiagDesc) == kGitsDiag_End);
#undef GITSDIAG_DESC


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

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


static const char * gitsGetCommandName(uint8_t uCmdId)
{
    switch (uCmdId)
    {
        case GITS_CMD_ID_CLEAR:     return "CLEAR";
        case GITS_CMD_ID_DISCARD:   return "DISCARD";
        case GITS_CMD_ID_INT:       return "INT";
        case GITS_CMD_ID_INV:       return "INV";
        case GITS_CMD_ID_INVALL:    return "INVALL";
        case GITS_CMD_ID_INVDB:     return "INVDB";
        case GITS_CMD_ID_MAPC:      return "MAPC";
        case GITS_CMD_ID_MAPD:      return "MAPD";
        case GITS_CMD_ID_MAPI:      return "MAPI";
        case GITS_CMD_ID_MAPTI:     return "MAPTI";
        case GITS_CMD_ID_MOVALL:    return "MOVALL";
        case GITS_CMD_ID_MOVI:      return "MOVI";
        case GITS_CMD_ID_SYNC:      return "SYNC";
        case GITS_CMD_ID_VINVALL:   return "VINVALL";
        case GITS_CMD_ID_VMAPI:     return "VMAPI";
        case GITS_CMD_ID_VMAPP:     return "VMAPP";
        case GITS_CMD_ID_VMAPTI:    return "VMAPTI";
        case GITS_CMD_ID_VMOVI:     return "VMOVI";
        case GITS_CMD_ID_VMOVP:     return "VMOVP";
        case GITS_CMD_ID_VSGI:      return "VSGI";
        case GITS_CMD_ID_VSYNC:     return "VSYNC";
        default:
            return "<UNKNOWN>";
    }
}


static void gitsCmdQueueSetError(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, GITSDIAG enmError, bool fStallQueue)
{
    Assert(GITS_CRIT_SECT_IS_OWNER(pDevIns));
    NOREF(pDevIns);

    /* Record the error and stall the queue. */
    pGitsDev->uCmdQueueError = enmError;
    if (fStallQueue)
        pGitsDev->uCmdReadReg |= GITS_BF_CTRL_REG_CREADR_STALLED_MASK;

    /* Since we don't support SEIs, so there should be nothing more to do here. */
    Assert(!RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_SEIS));
}


DECL_FORCE_INLINE(bool) gitsCmdQueueIsEmptyEx(PCGITSDEV pGitsDev, uint32_t *poffRead, uint32_t *poffWrite)
{
    *poffRead  = pGitsDev->uCmdReadReg  & GITS_BF_CTRL_REG_CREADR_OFFSET_MASK;
    *poffWrite = pGitsDev->uCmdWriteReg & GITS_BF_CTRL_REG_CWRITER_OFFSET_MASK;
    return *poffRead == *poffWrite;
}


DECL_FORCE_INLINE(bool) gitsCmdQueueIsEmpty(PCGITSDEV pGitsDev)
{
    uint32_t offRead;
    uint32_t offWrite;
    return gitsCmdQueueIsEmptyEx(pGitsDev, &offRead, &offWrite);
}


DECL_FORCE_INLINE(bool) gitsCmdQueueCanProcessRequests(PCGITSDEV pGitsDev)
{
    if (    (pGitsDev->uTypeReg.u    & GITS_BF_CTRL_REG_CTLR_ENABLED_MASK)
        &&  (pGitsDev->uCmdBaseReg.u & GITS_BF_CTRL_REG_CBASER_VALID_MASK)
        && !(pGitsDev->uCmdReadReg   & GITS_BF_CTRL_REG_CREADR_STALLED_MASK))
        return true;
    return false;
}


static void gitsCmdQueueThreadWakeUpIfNeeded(PPDMDEVINS pDevIns, PGITSDEV pGitsDev)
{
    Assert(GITS_CRIT_SECT_IS_OWNER(pDevIns));
    if (    gitsCmdQueueCanProcessRequests(pGitsDev)
        && !gitsCmdQueueIsEmpty(pGitsDev))
    {
        int const rc = PDMDevHlpSUPSemEventSignal(pDevIns, pGitsDev->hEvtCmdQueue);
        AssertRC(rc);
    }
}


DECL_HIDDEN_CALLBACK(uint64_t) gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb)
{
    Assert(cb == 4 || cb == 8);
    Assert(!(offReg & 3));
    RT_NOREF(cb);

    /*
     * GITS_BASER<n>.
     */
    uint64_t uReg;
    if (GITS_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg  = sizeof(uint64_t);
        uint16_t const idxReg = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        uReg = pGitsDev->aItsTableRegs[idxReg].u >> ((offReg & 7) << 3 /* to bits */);
        return uReg;
    }

    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            Assert(cb == 4);
            uReg = pGitsDev->uCtrlReg;
            break;

        case GITS_CTRL_REG_PIDR2_OFF:
        {
            Assert(cb == 4);
            Assert(pGitsDev->uArchRev <= GITS_CTRL_REG_PIDR2_ARCHREV_GICV4);
            uint8_t const uIdCodeDes1 = GIC_JEDEC_JEP10_DES_1(GIC_JEDEC_JEP106_IDENTIFICATION_CODE);
            uReg = RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_DES_1,   uIdCodeDes1)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_JEDEC,   1)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_ARCHREV, pGitsDev->uArchRev);
            break;
        }

        case GITS_CTRL_REG_IIDR_OFF:
            Assert(cb == 4);
            uReg = RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_ID_CODE,   GIC_JEDEC_JEP106_IDENTIFICATION_CODE)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_CONT_CODE, GIC_JEDEC_JEP106_CONTINUATION_CODE);
            break;

        case GITS_CTRL_REG_TYPER_OFF:
        case GITS_CTRL_REG_TYPER_OFF + 4:
        {
            uReg = pGitsDev->uTypeReg.u >> ((offReg & 7) << 3 /* to bits */);
            break;
        }

        case GITS_CTRL_REG_CBASER_OFF:
            uReg = pGitsDev->uCmdBaseReg.u;
            break;

        case GITS_CTRL_REG_CBASER_OFF + 4:
            Assert(cb == 4);
            uReg = pGitsDev->uCmdBaseReg.s.Hi;
            break;

        case GITS_CTRL_REG_CREADR_OFF:
            uReg = pGitsDev->uCmdReadReg;
            break;

        case GITS_CTRL_REG_CREADR_OFF + 4:
            uReg = 0;   /* Upper 32-bits are reserved, MBZ. */
            break;

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s)\n", offReg, gitsGetCtrlRegDescription(offReg)));
            uReg = 0;
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uReg=%#RX64 [%u-bit]\n", offReg, gitsGetCtrlRegDescription(offReg), uReg, cb << 3));
    return uReg;
}


DECL_HIDDEN_CALLBACK(uint64_t) gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb)
{
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));
    RT_NOREF(pGitsDev, cb);

    uint64_t uReg = 0;
    AssertReleaseMsgFailed(("offReg=%#x (%s) uReg=%#RX64 [%u-bit]\n", offReg, gitsGetTranslationRegDescription(offReg), uReg, cb << 3));
    return uReg;
}


DECL_HIDDEN_CALLBACK(void) gitsMmioWriteCtrl(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb)
{
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));

    /*
     * GITS_BASER<n>.
     */
    if (GITS_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg  = sizeof(uint64_t);
        uint16_t const idxReg = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        if (!(offReg & 7))
        {
            if (cb == 8)
                pGitsDev->aItsTableRegs[idxReg].u = uValue;
            else
                pGitsDev->aItsTableRegs[idxReg].s.Lo = uValue;
        }
        else
        {
            Assert(cb == 4);
            pGitsDev->aItsTableRegs[idxReg].s.Hi = uValue;
        }
        return;
    }

    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            Assert(cb == 4);
            Assert(!(pGitsDev->uTypeReg.u & GITS_BF_CTRL_REG_TYPER_UMSI_IRQ_MASK));
            pGitsDev->uCtrlReg = uValue & GITS_BF_CTRL_REG_CTLR_RW_MASK;
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CBASER_OFF:
            uValue &= GITS_CTRL_REG_CBASER_RW_MASK;
            if (cb == 8)
                pGitsDev->uCmdBaseReg.u = uValue;
            else
                pGitsDev->uCmdBaseReg.s.Lo = (uint32_t)uValue;
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CBASER_OFF + 4:
            Assert(cb == 4);
            pGitsDev->uCmdBaseReg.s.Hi = uValue & RT_HI_U32(GITS_CTRL_REG_CBASER_RW_MASK);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CWRITER_OFF:
            pGitsDev->uCmdWriteReg = uValue & RT_LO_U32(GITS_CTRL_REG_CWRITER_RW_MASK);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CWRITER_OFF + 4:
            /* Upper 32-bits are all reserved, ignore write. Fedora 40 arm64 guests (and probably others) do this. */
            Assert(uValue == 0);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s) uValue=%#RX32\n", offReg, gitsGetCtrlRegDescription(offReg), uValue));
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uValue=%#RX32 [%u-bit]\n", offReg, gitsGetCtrlRegDescription(offReg), uValue, cb << 3));
}


DECL_HIDDEN_CALLBACK(void) gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb)
{
    RT_NOREF(pGitsDev);
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));
    AssertReleaseMsgFailed(("offReg=%#x uValue=%#RX64 [%u-bit]\n", offReg, uValue, cb << 3));
}


DECL_HIDDEN_CALLBACK(void) gitsInit(PGITSDEV pGitsDev)
{
    Log4Func(("\n"));

    pGitsDev->uCtrlReg   = RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_QUIESCENT, 1);
    pGitsDev->uTypeReg.u = RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PHYSICAL,  1)     /* Physical LPIs supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VIRTUAL,   0) */  /* Virtual LPIs not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CCT,       0)     /* Collections in memory not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE, sizeof(GITSITE)) /* ITE size in bytes. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ID_BITS,   31)    /* 32-bit event IDs. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_DEV_BITS,  31)    /* 32-bit device IDs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SEIS,      0) */  /* Locally generated errors not recommended. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PTA,       0) */  /* Target is VCPU ID not address. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_HCC,       255)   /* Collection count. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CID_BITS,  0) */  /* CIL specifies collection ID size. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CIL,       0) */  /* 16-bit collection IDs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMOVP,     0) */  /* VMOVP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_MPAM,      0) */  /* MPAM no supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VSGI,      0) */  /* VSGI not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMAPP,     0) */  /* VMAPP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SVPET,     0) */  /* SVPET not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_NID,       0) */  /* NID (doorbell) not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI,      0) */  /** @todo Reporting receipt of unmapped MSIs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI_IRQ,  0) */  /** @todo Generating interrupt on unmapped MSI. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_INV,       1);    /* ITS caches invalidated when clearing
                                                                                  GITS_CTLR.Enabled and GITS_BASER<n>.Valid. */
    RT_ZERO(pGitsDev->aItsTableRegs);
    pGitsDev->uCmdBaseReg.u      = 0;
    pGitsDev->uCmdReadReg        = 0;
    pGitsDev->uCmdWriteReg       = 0;
}


#ifdef IN_RING3
DECL_HIDDEN_CALLBACK(void) gitsR3DbgInfo(PCGITSDEV pGitsDev, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "GIC ITS:\n");

    /* Basic info, GITS_CTLR and GITS_TYPER. */
    {
        GITSDIAG const    enmDiag  = pGitsDev->enmDiag;
        const char *const pszDiag  = enmDiag < RT_ELEMENTS(g_apszGitsDiagDesc) ? g_apszGitsDiagDesc[enmDiag] : "(Unknown)";
        uint32_t const    uCtrlReg = pGitsDev->uCtrlReg;
        pHlp->pfnPrintf(pHlp, "  Error              = %#RX32 (%s)\n", enmDiag, pszDiag);
        pHlp->pfnPrintf(pHlp, "  uArchRev           = %u\n",      pGitsDev->uArchRev);
        pHlp->pfnPrintf(pHlp, "  uCtrlReg           = %#RX32\n",  uCtrlReg);
        pHlp->pfnPrintf(pHlp, "    Enabled            = %RTbool\n", RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_ENABLED));
        pHlp->pfnPrintf(pHlp, "    UMSI IRQ           = %RTbool\n", RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_UMSI_IRQ));
        pHlp->pfnPrintf(pHlp, "    Quiescent          = %RTbool\n", RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_QUIESCENT));
    }

    /* GITS_BASER<n>. */
    for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aItsTableRegs); i++)
    {
        static uint32_t const s_acbPageSize[] = { _4K, _16K, _64K, _64K };
        static const char* const s_apszType[] = { "UnImpl", "Devices", "vPEs", "Intr Collections" };

        uint64_t const uReg        = pGitsDev->aItsTableRegs[i].u;
        uint16_t const uSize       = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_SIZE);
        uint16_t const cPages      = uSize > 0 ? uSize + 1 : 0;
        uint8_t const  idxPageSize = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_PAGESIZE);
        uint64_t const cbItsTable  = cPages * s_acbPageSize[idxPageSize];
        uint8_t const  uEntrySize  = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_ENTRY_SIZE);
        uint8_t const  idxType     = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_TYPE);
        const char *pszType        = s_apszType[idxType];
        pHlp->pfnPrintf(pHlp, "  aItsTableReg[%u]    = %#RX64\n", i, uReg);
        pHlp->pfnPrintf(pHlp, "    Size               = %#x (pages=%u total=%.Rhcb)\n", uSize, cPages, cbItsTable);
        pHlp->pfnPrintf(pHlp, "    Page size          = %#x (%.Rhcb)\n", idxPageSize, s_acbPageSize[idxPageSize]);
        pHlp->pfnPrintf(pHlp, "    Shareability       = %#x\n", RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_SHAREABILITY));
        pHlp->pfnPrintf(pHlp, "    Phys addr          = %#RX64\n", uReg & GITS_BF_CTRL_REG_BASER_PHYS_ADDR_MASK);
        pHlp->pfnPrintf(pHlp, "    Entry size         = %#x (%u bytes)\n", uEntrySize, uEntrySize > 0 ? uEntrySize + 1 : 0);
        pHlp->pfnPrintf(pHlp, "    Outer cache        = %#x\n", RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_OUTER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Type               = %#x (%s)\n", idxType, pszType);
        pHlp->pfnPrintf(pHlp, "    Inner cache        = %#x\n", RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_INNER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Indirect           = %RTbool\n", RT_BOOL(RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_INDIRECT)));
        pHlp->pfnPrintf(pHlp, "    Valid              = %RTbool\n", RT_BOOL(RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_VALID)));
    }

    /* GITS_CBASER. */
    {
        uint64_t const uReg   = pGitsDev->uCmdBaseReg.u;
        uint8_t const uSize   = RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_SIZE);
        uint16_t const cPages = uSize > 0 ? uSize + 1 : 0;
        pHlp->pfnPrintf(pHlp, "  uCmdBaseReg        = %#RX64\n", uReg);
        pHlp->pfnPrintf(pHlp, "    Size               = %#x (pages=%u total=%.Rhcb)\n", uSize, cPages, _4K * cPages);
        pHlp->pfnPrintf(pHlp, "    Shareability       = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_SHAREABILITY));
        pHlp->pfnPrintf(pHlp, "    Phys addr          = %#RX64\n",   uReg & GITS_BF_CTRL_REG_CBASER_PHYS_ADDR_MASK);
        pHlp->pfnPrintf(pHlp, "    Outer cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_OUTER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Inner cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_INNER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Valid              = %RTbool\n",  RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_VALID));
    }

    /* GITS_CREADR. */
    {
        uint32_t const uReg = pGitsDev->uCmdReadReg;
        pHlp->pfnPrintf(pHlp, "  uCmdReadReg        = 0x%05RX32 (stalled=%RTbool offset=%RU32)\n", uReg,
                        RT_BF_GET(uReg, GITS_BF_CTRL_REG_CREADR_STALLED), uReg & GITS_BF_CTRL_REG_CREADR_OFFSET_MASK);
    }

    /* GITS_CWRITER. */
    {
        uint32_t const uReg = pGitsDev->uCmdWriteReg;
        pHlp->pfnPrintf(pHlp, "  uCmdWriteReg       = 0x%05RX32 (  retry=%RTbool offset=%RU32)\n", uReg,
                        RT_BF_GET(uReg, GITS_BF_CTRL_REG_CWRITER_RETRY), uReg & GITS_BF_CTRL_REG_CWRITER_OFFSET_MASK);
    }
}


DECL_HIDDEN_CALLBACK(int) gitsR3CmdQueueProcess(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, void *pvBuf, uint32_t cbBuf)
{
    /* Hold the critical section as we could be accessing the device state simultaneously with MMIO accesses. */
    GITS_CRIT_SECT_ENTER(pDevIns);

    if (gitsCmdQueueCanProcessRequests(pGitsDev))
    {
        uint32_t   offRead;
        uint32_t   offWrite;
        bool const fIsEmpty = gitsCmdQueueIsEmptyEx(pGitsDev, &offRead, &offWrite);
        if (!fIsEmpty)
        {
            uint32_t const cCmdQueuePages = (pGitsDev->uCmdBaseReg.u & GITS_BF_CTRL_REG_CBASER_SIZE_MASK) + 1;
            uint32_t const cbCmdQueue     = cCmdQueuePages << GITS_CMD_QUEUE_PAGE_SHIFT;
            AssertRelease(cbCmdQueue <= cbBuf); /** @todo Paranoia; make this a debug assert later. */

            /*
             * Read all the commands from guest memory into our command queue buffer.
             */
            int      rc;
            uint32_t cbCmds;
            RTGCPHYS const GCPhysCmds = pGitsDev->uCmdBaseReg.u & GITS_BF_CTRL_REG_CBASER_PHYS_ADDR_MASK;

            /* Leave the critical section while reading (a potentially large number of) commands from guest memory. */
            GITS_CRIT_SECT_LEAVE(pDevIns);

            if (offWrite > offRead)
            {
                /* The write offset has not wrapped around, read them in one go. */
                cbCmds = offWrite - offRead;
                Assert(cbCmds <= cbBuf);
                rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds, pvBuf, cbCmds);
            }
            else
            {
                /* The write offset has wrapped around, read till end of buffer followed by wrapped-around data. */
                uint32_t const cbForward = cbCmdQueue - offRead;
                uint32_t const cbWrapped = offWrite;
                Assert(cbForward + cbWrapped <= cbBuf);
                rc  = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds, pvBuf, cbForward);
                if (   RT_SUCCESS(rc)
                    && cbWrapped > 0)
                {
                    rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds + cbForward, (void *)((uintptr_t)pvBuf + cbForward),
                                               cbWrapped);
                }
                cbCmds = cbForward + cbWrapped;
            }

            /* Indicate to the guest we've fetched all commands. */
            GITS_CRIT_SECT_ENTER(pDevIns);
            pGitsDev->uCmdReadReg = RT_BF_SET(pGitsDev->uCmdReadReg, GITS_BF_CTRL_REG_CREADR_OFFSET, offWrite);

            /*
             * Process the commands in the buffer.
             */
            if (RT_SUCCESS(rc))
            {
                /* Don't hold the lock while processing commands. */
                GITS_CRIT_SECT_LEAVE(pDevIns);

                uint32_t const cCmds = cbCmds / GITS_CMD_SIZE;
                for (uint32_t idxCmd = 0; idxCmd < cCmds; idxCmd++)
                {
                    PCGITSCMD pCmd = (PCGITSCMD)((uintptr_t)pvBuf + (idxCmd * GITS_CMD_SIZE));
                    uint8_t const uCmdId = pCmd->common.uCmdId;
                    switch (uCmdId)
                    {
                        case GITS_CMD_ID_MAPC:
                        {
                            Assert(!RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_PTA)); /* GITS_TYPER is read-only */
                            /** @todo Implementing me. Figure out interrupt collection, HCC etc. */
                            //uint64_t const uDw2 = pCmd->au64[2].u;
                            //bool const     fValid            = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_VALID);
                            //uint32_t const uTargetCpuId      = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_RDBASE);
                            //uint16_t const uIntrCollectionId = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_IC_ID);
                            break;
                        }

                        default:
                            AssertReleaseMsgFailed(("Cmd=%#x (%s)\n", uCmdId, gitsGetCommandName(uCmdId)));
                            break;
                    }
                }
                return VINF_SUCCESS;
            }

            /* Failed to read command queue from the physical address specified by the guest, stall queue and retry later. */
            gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_PhysAddr_Invalid, true /* fStall */);
            GITS_CRIT_SECT_LEAVE(pDevIns);
            return VINF_TRY_AGAIN;
        }
    }

    GITS_CRIT_SECT_LEAVE(pDevIns);
    return VINF_SUCCESS;
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

