/* $Id$ */
/** @file
 * GITS - Generic Interrupt Controller Interrupt Translation Service (ITS) - All Contexts.
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
#include <iprt/errcore.h>       /* VINF_SUCCESS */
#include <iprt/string.h>        /* RT_ZERO */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GITS saved state version. */
#define GITS_SAVED_STATE_VERSION                        1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifdef LOG_ENABLED
DECL_HIDDEN_CALLBACK(const char *) gitsGetCtrlRegDescription(uint16_t offReg)
{
    if (offReg - GITS_CTRL_REG_BASER_OFF_FIRST < GITS_CTRL_REG_BASER_RANGE_SIZE)
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

#endif


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue)
{
    Log4Func(("offReg=%#RX16\n",  offReg));

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            *puValue = pGitsDev->fUnmappedMsiReporting;
            break;
        default:
            AssertReleaseFailed();
    }

    return rcStrict;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue)
{
    RT_NOREF(pGitsDev, offReg, puValue);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteCtrl(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue)
{
    RT_NOREF(pGitsDev, offReg, uValue);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue)
{
    RT_NOREF(pGitsDev, offReg, uValue);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CALLBACK(void) gitsInit(PGITSDEV pGitsDev)
{
    Log4Func(("\n"));
    pGitsDev->fEnabled              = false;
    pGitsDev->fUnmappedMsiReporting = false;
    pGitsDev->fQuiescent            = true;
    RT_ZERO(pGitsDev->aBases);
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

