/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) AST handling.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_ACPI
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/list.h>
#include <iprt/mem.h>

#include <iprt/formats/acpi-aml.h>

#include "internal/acpi.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

DECLHIDDEN(PRTACPIASTNODE) rtAcpiAstNodeAlloc(RTACPIASTNODEOP enmOp, uint32_t fFlags, uint8_t cArgs)
{
    PRTACPIASTNODE pAstNd = (PRTACPIASTNODE)RTMemAllocZ(RT_UOFFSETOF_DYN(RTACPIASTNODE, aArgs[cArgs]));
    if (pAstNd)
    {
        pAstNd->enmOp  = enmOp;
        pAstNd->fFlags = fFlags;
        pAstNd->cArgs  = cArgs;
        RTListInit(&pAstNd->LstScopeNodes);
    }

    return pAstNd;
}


DECLHIDDEN(void) rtAcpiAstNodeFree(PRTACPIASTNODE pAstNd)
{
    /** @todo */
    RTMemFree(pAstNd);
}


static int rtAcpiAstDumpAstList(PCRTLISTANCHOR pLst, RTACPITBL hAcpiTbl)
{
    PCRTACPIASTNODE pIt;
    RTListForEach(pLst, pIt, RTACPIASTNODE, NdAst)
    {
        int rc = rtAcpiAstDumpToTbl(pIt, hAcpiTbl);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtAcpiAstDumpToTbl(PCRTACPIASTNODE pAstNd, RTACPITBL hAcpiTbl)
{
    int rc;

    switch (pAstNd->enmOp)
    {
        case kAcpiAstNodeOp_Scope:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblScopeStart(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblScopeFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Processor:
        {
            AssertBreakStmt(   pAstNd->cArgs == 4
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_U8
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_U32
                            && pAstNd->aArgs[3].enmType == kAcpiAstArgType_U8,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblProcessorStart(hAcpiTbl,
                                         pAstNd->aArgs[0].u.pszNameString,
                                         pAstNd->aArgs[1].u.u8,
                                         pAstNd->aArgs[2].u.u32,
                                         pAstNd->aArgs[3].u.u8);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblProcessorFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_External:
        {
            AssertBreakStmt(   pAstNd->cArgs == 3
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_ObjType
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_U8,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblExternalAppend(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString, pAstNd->aArgs[1].u.enmObjType, pAstNd->aArgs[2].u.u8);
            break;
        }
        case kAcpiAstNodeOp_Method:
        {
            AssertBreakStmt(   pAstNd->cArgs == 4
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_U8
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_Bool
                            && pAstNd->aArgs[3].enmType == kAcpiAstArgType_U8,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblMethodStart(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString,
                                      pAstNd->aArgs[1].u.u8,
                                      pAstNd->aArgs[1].u.f ? RTACPI_METHOD_F_SERIALIZED : RTACPI_METHOD_F_NOT_SERIALIZED,
                                      pAstNd->aArgs[1].u.u8);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblMethodFinalize(hAcpiTbl);
            }
            break;
        }
        default:
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
    }

    return rc;
}
