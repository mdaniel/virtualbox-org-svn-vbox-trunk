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
#include <VBox/vmm/dbgf.h>
#include <iprt/errcore.h>       /* VINF_SUCCESS */
#include <iprt/string.h>        /* RT_ZERO */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GITS saved state version. */
#define GITS_SAVED_STATE_VERSION                        1

/** Gets whether the given register offset is within the specified range. */
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
            uReg = RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_ENABLED,   pGitsDev->fEnabled)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_QUIESCENT, pGitsDev->fQuiescent);
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
            uint64_t const uVal = RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PHYSICAL,  1)      /* Physical LPIs supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VIRTUAL,   0) */   /* Virtual LPIs not supported. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CCT,       0)      /* Collections in memory not supported. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE, sizeof(GITSITE)) /* ITE size in bytes. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ID_BITS,   31)     /* 32-bit event IDs. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_DEV_BITS,  31)     /* 32-bit device IDs. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SEIS,      0) */   /** @todo SEI support. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PTA,       0) */   /* Target is VCPU ID not address. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_HCC,       255)    /* Collection count. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CID_BITS,  0) */   /* CIL specifies collection ID size. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CIL,       0) */   /* 16-bit collection IDs. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMOVP,     0) */   /* VMOVP not supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_MPAM,      0) */   /* MPAM no supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VSGI,      0) */   /* VSGI not supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMAPP,     0) */   /* VMAPP not supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SVPET,     0) */   /* SVPET not supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_NID,       0) */   /* NID (doorbell) not supported. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI,      0) */   /** @todo Reporting receipt of unmapped MSIs. */
                              /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI_IRQ,  0) */   /** @todo Generating interrupt on unmapped MSI. */
                                | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_INV,       1);     /* ITS caches invalidated when clearing
                                                                                          GITS_CTLR.Enabled and GITS_BASER<n>.Valid. */
            uReg = uVal >> ((offReg & 7) << 3 /* to bits */);
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


DECL_HIDDEN_CALLBACK(void) gitsMmioWriteCtrl(PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb)
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
            pGitsDev->fEnabled = RT_BF_GET(uValue, GITS_BF_CTRL_REG_CTLR_ENABLED);
            break;

        case GITS_CTRL_REG_CBASER_OFF:
            uValue &= GITS_CTRL_REG_CBASER_RW_MASK;
            if (cb == 8)
                pGitsDev->uCmdBaseReg.u = uValue;
            else
                pGitsDev->uCmdBaseReg.s.Lo = (uint32_t)uValue;
            break;

        case GITS_CTRL_REG_CBASER_OFF + 4:
            Assert(cb == 4);
            pGitsDev->uCmdBaseReg.s.Hi = uValue & RT_HI_U32(GITS_CTRL_REG_CBASER_RW_MASK);
            break;

        case GITS_CTRL_REG_CWRITER_OFF:
            pGitsDev->uCmdWriteReg = uValue & RT_LO_U32(GITS_CTRL_REG_CWRITER_RW_MASK);
            break;

        case GITS_CTRL_REG_CWRITER_OFF + 4:
            /* Upper 32-bits are all reserved, ignore write. Fedora 40 arm64 guests (and probably others) do this. */
            Assert(uValue == 0);
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
    pGitsDev->fEnabled           = false;
    pGitsDev->fUnmappedMsiReport = false;
    pGitsDev->fQuiescent         = true;
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
    pHlp->pfnPrintf(pHlp, "  uArchRev           = %u\n",      pGitsDev->uArchRev);
    pHlp->pfnPrintf(pHlp, "  fEnabled           = %RTbool\n", pGitsDev->fEnabled);
    pHlp->pfnPrintf(pHlp, "  fUnmappedMsiReport = %RTbool\n", pGitsDev->fUnmappedMsiReport);
    pHlp->pfnPrintf(pHlp, "  fQuiescent         = %RTbool\n", pGitsDev->fQuiescent);

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

