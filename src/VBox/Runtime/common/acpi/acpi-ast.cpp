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


DECLHIDDEN(int) rtAcpiAstNodeTransform(PRTACPIASTNODE pAstNd, PRTERRINFO pErrInfo)
{
#if 0
    /* Walk all arguments containing AST nodes first. */
    for (uint8_t i = 0; i < pAstNd->cArgs; i++)
    {
        if (   pAstNd->aArgs[i].enmType == kAcpiAstArgType_AstNode
            && pAstNd->aArgs[i].u.pAstNd)
        {
            int rc = rtAcpiAstNodeTransform(pAstNd->aArgs[i].u.pAstNd, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    if (pAstNd->fFlags & RTACPI_AST_NODE_F_NEW_SCOPE)
    {
        PRTACPIASTNODE pIt/*, pItPrev*/;
        /* Do transformations on the nodes first. */
        RTListForEach(&pAstNd->LstScopeNodes, pIt, RTACPIASTNODE, NdAst)
        {
            int rc = rtAcpiAstNodeTransform(pIt, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;
        }

        /* Now do transformations on our level. */
        RTListForEachReverseSafe(&pAstNd->LstScopeNodes, pIt, pItPrev, RTACPIASTNODE, NdAst)
        {
            /*
             * If there is an If AST node followed by Else we move the Else branch as the last
             * statement in the If because when emitting to AML the Else is enclosed in the If
             * package.
             */
            if (   pIt->enmOp == kAcpiAstNodeOp_Else
                && pItPrev
                && pItPrev->enmOp == kAcpiAstNodeOp_If)
            {
                RTListNodeRemove(&pIt->NdAst);
                RTListAppend(&pItPrev->LstScopeNodes, &pIt->NdAst);
            }
        }
    }
#else
    RT_NOREF(pAstNd, pErrInfo);
#endif

    return VINF_SUCCESS;
}


/**
 * Evaluates the given AST node to an integer if possible.
 *
 * @returns IPRT status code.
 * @param   pAstNd          The AST node to evaluate.
 * @param   pu64            Where to store the integer on success.
 */
static int rtAcpiAstNodeEvaluateToInteger(PCRTACPIASTNODE pAstNd, uint64_t *pu64)
{
    /* Easy way out?. */
    if (pAstNd->enmOp == kAcpiAstNodeOp_Number)
    {
        *pu64 = pAstNd->u64;
        return VINF_SUCCESS;
    }

    if (pAstNd->enmOp == kAcpiAstNodeOp_One)
    {
        *pu64 = 1;
        return VINF_SUCCESS;
    }

    if (pAstNd->enmOp == kAcpiAstNodeOp_Zero)
    {
        *pu64 = 0;
        return VINF_SUCCESS;
    }

    /** @todo */
    return VERR_NOT_IMPLEMENTED;
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
    int rc = VINF_SUCCESS;

    switch (pAstNd->enmOp)
    {
        case kAcpiAstNodeOp_Identifier:
        {
            rc = RTAcpiTblNameStringAppend(hAcpiTbl, pAstNd->pszIde);
            if (RT_SUCCESS(rc))
            {
                for (uint8_t i = 0; i < pAstNd->cArgs; i++)
                {
                    Assert(pAstNd->aArgs[i].enmType == kAcpiAstArgType_AstNode);
                    rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[i].u.pAstNd, hAcpiTbl);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
            break;
        }
        case kAcpiAstNodeOp_StringLiteral:
            rc = RTAcpiTblStringAppend(hAcpiTbl, pAstNd->pszStrLit);
            break;
        case kAcpiAstNodeOp_Number:
            rc = RTAcpiTblIntegerAppend(hAcpiTbl, pAstNd->u64);
            break;
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
                                      pAstNd->aArgs[2].u.f ? RTACPI_METHOD_F_SERIALIZED : RTACPI_METHOD_F_NOT_SERIALIZED,
                                      pAstNd->aArgs[3].u.u8);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblMethodFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Device:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblDeviceStart(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblDeviceFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_If:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblIfStart(hAcpiTbl);
            if (RT_SUCCESS(rc))
            {
                /* Predicate. */
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
                if (RT_SUCCESS(rc))
                {
                    /* Walk all the other AST nodes. */
                    rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                    if (RT_SUCCESS(rc))
                        rc = RTAcpiTblIfFinalize(hAcpiTbl);
                }
            }
            break;
        }
        case kAcpiAstNodeOp_Else:
        {
            AssertBreakStmt(pAstNd->cArgs == 0, rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblElseStart(hAcpiTbl);
            if (RT_SUCCESS(rc))
            {
                /* Walk all the other AST nodes. */
                rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblElseFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_LAnd:
        case kAcpiAstNodeOp_LEqual:
        case kAcpiAstNodeOp_LGreater:
        case kAcpiAstNodeOp_LGreaterEqual:
        case kAcpiAstNodeOp_LLess:
        case kAcpiAstNodeOp_LLessEqual:
        case kAcpiAstNodeOp_LNotEqual:
        {
            AssertBreakStmt(   pAstNd->cArgs == 2
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            RTACPIBINARYOP enmOp;
            switch (pAstNd->enmOp)
            {
                case kAcpiAstNodeOp_LAnd:           enmOp = kAcpiBinaryOp_LAnd; break;
                case kAcpiAstNodeOp_LEqual:         enmOp = kAcpiBinaryOp_LEqual; break;
                case kAcpiAstNodeOp_LGreater:       enmOp = kAcpiBinaryOp_LGreater; break;
                case kAcpiAstNodeOp_LGreaterEqual:  enmOp = kAcpiBinaryOp_LGreaterEqual; break;
                case kAcpiAstNodeOp_LLess:          enmOp = kAcpiBinaryOp_LLess; break;
                case kAcpiAstNodeOp_LLessEqual:     enmOp = kAcpiBinaryOp_LLessEqual; break;
                case kAcpiAstNodeOp_LNotEqual:      enmOp = kAcpiBinaryOp_LNotEqual; break;
                default:
                    AssertReleaseFailed(); /* Impossible */
                    return VERR_INTERNAL_ERROR;
            }

            rc = RTAcpiTblBinaryOpAppend(hAcpiTbl, enmOp);
            if (RT_SUCCESS(rc))
            {
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[1].u.pAstNd, hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_LNot:
            AssertFailed(); /** @todo */
            break;
        case kAcpiAstNodeOp_Zero:
        {
            AssertBreakStmt(pAstNd->cArgs == 0, rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblIntegerAppend(hAcpiTbl, 0);
            break;
        }
        case kAcpiAstNodeOp_One:
        {
            AssertBreakStmt(pAstNd->cArgs == 0, rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblIntegerAppend(hAcpiTbl, 1);
            break;
        }
        case kAcpiAstNodeOp_Return:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Return);
            if (RT_SUCCESS(rc))
            {
                if (pAstNd->aArgs[0].u.pAstNd)
                    rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
                else
                    rc = RTAcpiTblNullNameAppend(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Unicode:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[0].u.pAstNd->enmOp == kAcpiAstNodeOp_StringLiteral,
                            rc = VERR_INTERNAL_ERROR);

            rc = RTAcpiTblStringAppendAsUtf16(hAcpiTbl, pAstNd->aArgs[0].u.pAstNd->pszStrLit);
            break;
        }
        case kAcpiAstNodeOp_OperationRegion:
        {
            AssertBreakStmt(   pAstNd->cArgs == 4
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_RegionSpace
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[3].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);

            rc = RTAcpiTblOpRegionAppendEx(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString, pAstNd->aArgs[1].u.enmRegionSpace);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[2].u.pAstNd, hAcpiTbl);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[3].u.pAstNd, hAcpiTbl);
            break;
        }
        case kAcpiAstNodeOp_Field:
        {
            AssertBreakStmt(   pAstNd->cArgs == 4
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_FieldAcc
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_Bool
                            && pAstNd->aArgs[3].enmType == kAcpiAstArgType_FieldUpdate,
                            rc = VERR_INTERNAL_ERROR);

            rc = RTAcpiTblFieldAppend(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString, pAstNd->aArgs[1].u.enmFieldAcc,
                                      pAstNd->aArgs[2].u.f, pAstNd->aArgs[3].u.enmFieldUpdate, pAstNd->Fields.paFields,
                                      pAstNd->Fields.cFields);
            break;
        }
        case kAcpiAstNodeOp_Name:
        {
            AssertBreakStmt(   pAstNd->cArgs == 2
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_NameString
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);

            rc = RTAcpiTblNameAppend(hAcpiTbl, pAstNd->aArgs[0].u.pszNameString);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[1].u.pAstNd, hAcpiTbl);
            break;
        }
        case kAcpiAstNodeOp_ResourceTemplate:
            rc = RTAcpiTblResourceAppend(hAcpiTbl, pAstNd->hAcpiRes);
            break;
        case kAcpiAstNodeOp_Arg0:
        case kAcpiAstNodeOp_Arg1:
        case kAcpiAstNodeOp_Arg2:
        case kAcpiAstNodeOp_Arg3:
        case kAcpiAstNodeOp_Arg4:
        case kAcpiAstNodeOp_Arg5:
        case kAcpiAstNodeOp_Arg6:
            rc = RTAcpiTblArgOpAppend(hAcpiTbl, pAstNd->enmOp - kAcpiAstNodeOp_Arg0);
            break;
        case kAcpiAstNodeOp_Local0:
        case kAcpiAstNodeOp_Local1:
        case kAcpiAstNodeOp_Local2:
        case kAcpiAstNodeOp_Local3:
        case kAcpiAstNodeOp_Local4:
        case kAcpiAstNodeOp_Local5:
        case kAcpiAstNodeOp_Local6:
        case kAcpiAstNodeOp_Local7:
            rc = RTAcpiTblLocalOpAppend(hAcpiTbl, pAstNd->enmOp - kAcpiAstNodeOp_Local0);
            break;
        case kAcpiAstNodeOp_Package:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);

            /* Try to gather the number of elements. */
            uint64_t cElems = 0;
            if (pAstNd->aArgs[0].u.pAstNd)
            {
                /* Try resolving to a constant expression. */
                rc = rtAcpiAstNodeEvaluateToInteger(pAstNd->aArgs[0].u.pAstNd, &cElems);
                if (RT_FAILURE(rc))
                    break;
            }
            else
            {
                /* Count elements. */
                PRTACPIASTNODE pIt;
                RTListForEach(&pAstNd->LstScopeNodes, pIt, RTACPIASTNODE, NdAst)
                {
                    cElems++;
                }
            }
            if (RT_SUCCESS(rc))
            {
                if (cElems > 255)
                {
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }

                rc = RTAcpiTblPackageStart(hAcpiTbl, (uint8_t)cElems);
                if (RT_SUCCESS(rc))
                    rc = rtAcpiAstDumpAstList(&pAstNd->LstScopeNodes, hAcpiTbl);
                if (RT_SUCCESS(rc))
                    rc = RTAcpiTblPackageFinalize(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Buffer:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);

            /* Try to gather the number of elements. */
            uint64_t cElems = 0;
            if (pAstNd->aArgs[0].u.pAstNd)
            {
                /* Try resolving to a constant expression. */
                rc = rtAcpiAstNodeEvaluateToInteger(pAstNd->aArgs[0].u.pAstNd, &cElems);
                if (RT_FAILURE(rc))
                    break;
            }
            else
            {
                /* Count elements. */
                PRTACPIASTNODE pIt;
                RTListForEach(&pAstNd->LstScopeNodes, pIt, RTACPIASTNODE, NdAst)
                {
                    cElems++;
                }
            }

            if (RT_SUCCESS(rc))
            {
                uint8_t *pb = (uint8_t *)RTMemAlloc(cElems);
                if (pb)
                {
                    uint64_t i = 0;
                    PRTACPIASTNODE pIt;
                    RTListForEach(&pAstNd->LstScopeNodes, pIt, RTACPIASTNODE, NdAst)
                    {
                        /* Try resolving to a constant expression. */
                        uint64_t u64 = 0;
                        rc = rtAcpiAstNodeEvaluateToInteger(pIt, &u64);
                        if (RT_FAILURE(rc))
                            break;
                        if (u64 > UINT8_MAX)
                        {
                            rc = VERR_BUFFER_OVERFLOW;
                            break;
                        }

                        pb[i++] = (uint8_t)u64;
                    }

                    if (RT_SUCCESS(rc))
                        rc = RTAcpiTblBufferAppend(hAcpiTbl, pb, cElems);
                    RTMemFree(pb);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            break;
        }
        case kAcpiAstNodeOp_ToUuid:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[0].u.pAstNd->enmOp == kAcpiAstNodeOp_StringLiteral,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblUuidAppendFromStr(hAcpiTbl, pAstNd->aArgs[0].u.pAstNd->pszStrLit);
            break;
        }
        case kAcpiAstNodeOp_Break:
        {
            AssertBreakStmt(pAstNd->cArgs == 0, rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Break);
            break;
        }
        case kAcpiAstNodeOp_Continue:
        {
            AssertBreakStmt(pAstNd->cArgs == 0, rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Continue);
            break;
        }
        case kAcpiAstNodeOp_DerefOf:
        {
            AssertBreakStmt(   pAstNd->cArgs == 1
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_DerefOf);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
            break;
        }
        case kAcpiAstNodeOp_Store:
        case kAcpiAstNodeOp_Notify:
        {
            AssertBreakStmt(   pAstNd->cArgs == 2
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl,
                                             pAstNd->enmOp == kAcpiAstNodeOp_Store
                                           ? kAcpiStmt_Store
                                           : kAcpiStmt_Notify);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[1].u.pAstNd, hAcpiTbl);
            break;
        }
        case kAcpiAstNodeOp_Not:
        {
            AssertBreakStmt(   pAstNd->cArgs == 2
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);
            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, kAcpiStmt_Store);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
            if (RT_SUCCESS(rc))
            {
                if (pAstNd->aArgs[2].u.pAstNd)
                    rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[1].u.pAstNd, hAcpiTbl);
                else
                    rc = RTAcpiTblNullNameAppend(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Index:
        case kAcpiAstNodeOp_Add:
        case kAcpiAstNodeOp_Subtract:
        case kAcpiAstNodeOp_And:
        case kAcpiAstNodeOp_Nand:
        case kAcpiAstNodeOp_Or:
        case kAcpiAstNodeOp_Xor:
        {
            AssertBreakStmt(   pAstNd->cArgs == 3
                            && pAstNd->aArgs[0].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[1].enmType == kAcpiAstArgType_AstNode
                            && pAstNd->aArgs[2].enmType == kAcpiAstArgType_AstNode,
                            rc = VERR_INTERNAL_ERROR);

            RTACPISTMT enmStmt;
            switch (pAstNd->enmOp)
            {
                case kAcpiAstNodeOp_Index:    enmStmt = kAcpiStmt_Index;    break;
                case kAcpiAstNodeOp_Add:      enmStmt = kAcpiStmt_Add;      break;
                case kAcpiAstNodeOp_Subtract: enmStmt = kAcpiStmt_Subtract; break;
                case kAcpiAstNodeOp_And:      enmStmt = kAcpiStmt_And;      break;
                case kAcpiAstNodeOp_Nand:     enmStmt = kAcpiStmt_Nand;     break;
                case kAcpiAstNodeOp_Or:       enmStmt = kAcpiStmt_Or;       break;
                case kAcpiAstNodeOp_Xor:      enmStmt = kAcpiStmt_Xor;      break;
                default:
                    AssertReleaseFailed(); /* Impossible */
                    return VERR_INTERNAL_ERROR;
            }

            rc = RTAcpiTblStmtSimpleAppend(hAcpiTbl, enmStmt);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[0].u.pAstNd, hAcpiTbl);
            if (RT_SUCCESS(rc))
                rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[1].u.pAstNd, hAcpiTbl);
            if (RT_SUCCESS(rc))
            {
                if (pAstNd->aArgs[2].u.pAstNd)
                    rc = rtAcpiAstDumpToTbl(pAstNd->aArgs[2].u.pAstNd, hAcpiTbl);
                else
                    rc = RTAcpiTblNullNameAppend(hAcpiTbl);
            }
            break;
        }
        case kAcpiAstNodeOp_Ones:
        default:
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
    }

    AssertRC(rc);
    return rc;
}
