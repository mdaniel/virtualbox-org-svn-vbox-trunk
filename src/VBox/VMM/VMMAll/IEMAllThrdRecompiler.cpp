/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Threaded Recompilation.
 *
 * Logging group IEM_RE_THREADED assignments:
 *      - Level 1  (Log)  : Errors, exceptions, interrupts and such major events. [same as IEM]
 *      - Flow  (LogFlow) : TB calls being emitted.
 *      - Level 2  (Log2) : Basic instruction execution state info. [same as IEM]
 *      - Level 3  (Log3) : More detailed execution state info. [same as IEM]
 *      - Level 4  (Log4) : Decoding mnemonics w/ EIP. [same as IEM]
 *      - Level 5  (Log5) : Decoding details. [same as IEM]
 *      - Level 6  (Log6) : TB opcode range management.
 *      - Level 7  (Log7) : TB obsoletion.
 *      - Level 8  (Log8) : TB compilation.
 *      - Level 9  (Log9) : TB exec.
 *      - Level 10 (Log10): TB block lookup.
 *      - Level 11 (Log11): TB block lookup details.
 *      - Level 12 (Log12): TB insertion.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
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
#ifndef LOG_GROUP /* defined when included by tstIEMCheckMc.cpp */
# define LOG_GROUP LOG_GROUP_IEM_RE_THREADED
#endif
#define IEM_WITH_CODE_TLB_AND_OPCODE_BUF  /* A bit hackish, but its all in IEMInline.h. */
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/gim.h>
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# include <VBox/vmm/em.h>
# include <VBox/vmm/hm_svm.h>
#endif
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
# include <VBox/vmm/hmvmxinline.h>
#endif
#include <VBox/vmm/tm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgftrace.h>
#ifndef TST_IEM_CHECK_MC
# include "IEMInternal.h"
#endif
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/disopcode-x86-amd64.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/sort.h>
#include <iprt/x86.h>

#ifndef TST_IEM_CHECK_MC
# include "IEMInline.h"
# include "IEMOpHlp.h"
# include "IEMMc.h"
#endif

#include "IEMThreadedFunctions.h"


/*
 * Narrow down configs here to avoid wasting time on unused configs here.
 */

#ifndef IEM_WITH_CODE_TLB
# error The code TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_DATA_TLB
# error The data TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_SETJMP
# error The setjmp approach must be enabled for the recompiler.
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void         iemTbAllocatorFree(PVMCPUCC pVCpu, PIEMTB pTb);


/**
 * Calculates the effective address of a ModR/M memory operand, extended version
 * for use in the recompilers.
 *
 * Meant to be used via IEM_MC_CALC_RM_EFF_ADDR.
 *
 * May longjmp on internal error.
 *
 * @return  The effective address.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   bRm                 The ModRM byte.
 * @param   cbImmAndRspOffset   - First byte: The size of any immediate
 *                                following the effective address opcode bytes
 *                                (only for RIP relative addressing).
 *                              - Second byte: RSP displacement (for POP [ESP]).
 * @param   puInfo              Extra info: 32-bit displacement (bits 31:0) and
 *                              SIB byte (bits 39:32).
 *
 * @note This must be defined in a source file with matching
 *       IEM_WITH_CODE_TLB_AND_OPCODE_BUF define till the define is made default
 *       or implemented differently...
 */
RTGCPTR iemOpHlpCalcRmEffAddrJmpEx(PVMCPUCC pVCpu, uint8_t bRm, uint32_t cbImmAndRspOffset, uint64_t *puInfo) IEM_NOEXCEPT_MAY_LONGJMP
{
    Log5(("iemOpHlpCalcRmEffAddrJmp: bRm=%#x\n", bRm));
# define SET_SS_DEF() \
    do \
    { \
        if (!(pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SEG_MASK)) \
            pVCpu->iem.s.iEffSeg = X86_SREG_SS; \
    } while (0)

    if (!IEM_IS_64BIT_CODE(pVCpu))
    {
/** @todo Check the effective address size crap! */
        if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_16BIT)
        {
            uint16_t u16EffAddr;

            /* Handle the disp16 form with no registers first. */
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
            {
                IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);
                *puInfo = u16EffAddr;
            }
            else
            {
                /* Get the displacment. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:  u16EffAddr = 0;                             break;
                    case 1:  IEM_OPCODE_GET_NEXT_S8_SX_U16(&u16EffAddr); break;
                    case 2:  IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);       break;
                    default: AssertFailedStmt(IEM_DO_LONGJMP(pVCpu, VERR_IEM_IPE_1)); /* (caller checked for these) */
                }
                *puInfo = u16EffAddr;

                /* Add the base and index registers to the disp. */
                switch (bRm & X86_MODRM_RM_MASK)
                {
                    case 0: u16EffAddr += pVCpu->cpum.GstCtx.bx + pVCpu->cpum.GstCtx.si; break;
                    case 1: u16EffAddr += pVCpu->cpum.GstCtx.bx + pVCpu->cpum.GstCtx.di; break;
                    case 2: u16EffAddr += pVCpu->cpum.GstCtx.bp + pVCpu->cpum.GstCtx.si; SET_SS_DEF(); break;
                    case 3: u16EffAddr += pVCpu->cpum.GstCtx.bp + pVCpu->cpum.GstCtx.di; SET_SS_DEF(); break;
                    case 4: u16EffAddr += pVCpu->cpum.GstCtx.si;            break;
                    case 5: u16EffAddr += pVCpu->cpum.GstCtx.di;            break;
                    case 6: u16EffAddr += pVCpu->cpum.GstCtx.bp;            SET_SS_DEF(); break;
                    case 7: u16EffAddr += pVCpu->cpum.GstCtx.bx;            break;
                }
            }

            Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#06RX16 uInfo=%#RX64\n", u16EffAddr, *puInfo));
            return u16EffAddr;
        }

        Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT);
        uint32_t u32EffAddr;
        uint64_t uInfo;

        /* Handle the disp32 form with no registers first. */
        if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
        {
            IEM_OPCODE_GET_NEXT_U32(&u32EffAddr);
            uInfo = u32EffAddr;
        }
        else
        {
            /* Get the register (or SIB) value. */
            uInfo = 0;
            switch ((bRm & X86_MODRM_RM_MASK))
            {
                case 0: u32EffAddr = pVCpu->cpum.GstCtx.eax; break;
                case 1: u32EffAddr = pVCpu->cpum.GstCtx.ecx; break;
                case 2: u32EffAddr = pVCpu->cpum.GstCtx.edx; break;
                case 3: u32EffAddr = pVCpu->cpum.GstCtx.ebx; break;
                case 4: /* SIB */
                {
                    uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);
                    uInfo = (uint64_t)bSib << 32;

                    /* Get the index and scale it. */
                    switch ((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK)
                    {
                        case 0: u32EffAddr = pVCpu->cpum.GstCtx.eax; break;
                        case 1: u32EffAddr = pVCpu->cpum.GstCtx.ecx; break;
                        case 2: u32EffAddr = pVCpu->cpum.GstCtx.edx; break;
                        case 3: u32EffAddr = pVCpu->cpum.GstCtx.ebx; break;
                        case 4: u32EffAddr = 0; /*none */ break;
                        case 5: u32EffAddr = pVCpu->cpum.GstCtx.ebp; break;
                        case 6: u32EffAddr = pVCpu->cpum.GstCtx.esi; break;
                        case 7: u32EffAddr = pVCpu->cpum.GstCtx.edi; break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                    }
                    u32EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* add base */
                    switch (bSib & X86_SIB_BASE_MASK)
                    {
                        case 0: u32EffAddr += pVCpu->cpum.GstCtx.eax; break;
                        case 1: u32EffAddr += pVCpu->cpum.GstCtx.ecx; break;
                        case 2: u32EffAddr += pVCpu->cpum.GstCtx.edx; break;
                        case 3: u32EffAddr += pVCpu->cpum.GstCtx.ebx; break;
                        case 4: u32EffAddr += pVCpu->cpum.GstCtx.esp + (cbImmAndRspOffset >> 8); SET_SS_DEF(); break;
                        case 5:
                            if ((bRm & X86_MODRM_MOD_MASK) != 0)
                            {
                                u32EffAddr += pVCpu->cpum.GstCtx.ebp;
                                SET_SS_DEF();
                            }
                            else
                            {
                                uint32_t u32Disp;
                                IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                                u32EffAddr += u32Disp;
                                uInfo      |= u32Disp;
                            }
                            break;
                        case 6: u32EffAddr += pVCpu->cpum.GstCtx.esi; break;
                        case 7: u32EffAddr += pVCpu->cpum.GstCtx.edi; break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                    }
                    break;
                }
                case 5: u32EffAddr = pVCpu->cpum.GstCtx.ebp; SET_SS_DEF(); break;
                case 6: u32EffAddr = pVCpu->cpum.GstCtx.esi; break;
                case 7: u32EffAddr = pVCpu->cpum.GstCtx.edi; break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
            }

            /* Get and add the displacement. */
            switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
            {
                case 0:
                    break;
                case 1:
                {
                    int8_t i8Disp; IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                    u32EffAddr += i8Disp;
                    uInfo      |= (uint32_t)(int32_t)i8Disp;
                    break;
                }
                case 2:
                {
                    uint32_t u32Disp; IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                    u32EffAddr += u32Disp;
                    uInfo      |= u32Disp;
                    break;
                }
                default:
                    AssertFailedStmt(IEM_DO_LONGJMP(pVCpu, VERR_IEM_IPE_2)); /* (caller checked for these) */
            }
        }

        *puInfo = uInfo;
        Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RX32 uInfo=%#RX64\n", u32EffAddr, uInfo));
        return u32EffAddr;
    }

    uint64_t u64EffAddr;
    uint64_t uInfo;

    /* Handle the rip+disp32 form with no registers first. */
    if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
    {
        IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64EffAddr);
        uInfo = (uint32_t)u64EffAddr;
        u64EffAddr += pVCpu->cpum.GstCtx.rip + IEM_GET_INSTR_LEN(pVCpu) + (cbImmAndRspOffset & UINT32_C(0xff));
    }
    else
    {
        /* Get the register (or SIB) value. */
        uInfo = 0;
        switch ((bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB)
        {
            case  0: u64EffAddr = pVCpu->cpum.GstCtx.rax; break;
            case  1: u64EffAddr = pVCpu->cpum.GstCtx.rcx; break;
            case  2: u64EffAddr = pVCpu->cpum.GstCtx.rdx; break;
            case  3: u64EffAddr = pVCpu->cpum.GstCtx.rbx; break;
            case  5: u64EffAddr = pVCpu->cpum.GstCtx.rbp; SET_SS_DEF(); break;
            case  6: u64EffAddr = pVCpu->cpum.GstCtx.rsi; break;
            case  7: u64EffAddr = pVCpu->cpum.GstCtx.rdi; break;
            case  8: u64EffAddr = pVCpu->cpum.GstCtx.r8;  break;
            case  9: u64EffAddr = pVCpu->cpum.GstCtx.r9;  break;
            case 10: u64EffAddr = pVCpu->cpum.GstCtx.r10; break;
            case 11: u64EffAddr = pVCpu->cpum.GstCtx.r11; break;
            case 13: u64EffAddr = pVCpu->cpum.GstCtx.r13; break;
            case 14: u64EffAddr = pVCpu->cpum.GstCtx.r14; break;
            case 15: u64EffAddr = pVCpu->cpum.GstCtx.r15; break;
            /* SIB */
            case 4:
            case 12:
            {
                uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);
                uInfo = (uint64_t)bSib << 32;

                /* Get the index and scale it. */
                switch (((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK) | pVCpu->iem.s.uRexIndex)
                {
                    case  0: u64EffAddr = pVCpu->cpum.GstCtx.rax; break;
                    case  1: u64EffAddr = pVCpu->cpum.GstCtx.rcx; break;
                    case  2: u64EffAddr = pVCpu->cpum.GstCtx.rdx; break;
                    case  3: u64EffAddr = pVCpu->cpum.GstCtx.rbx; break;
                    case  4: u64EffAddr = 0; /*none */ break;
                    case  5: u64EffAddr = pVCpu->cpum.GstCtx.rbp; break;
                    case  6: u64EffAddr = pVCpu->cpum.GstCtx.rsi; break;
                    case  7: u64EffAddr = pVCpu->cpum.GstCtx.rdi; break;
                    case  8: u64EffAddr = pVCpu->cpum.GstCtx.r8;  break;
                    case  9: u64EffAddr = pVCpu->cpum.GstCtx.r9;  break;
                    case 10: u64EffAddr = pVCpu->cpum.GstCtx.r10; break;
                    case 11: u64EffAddr = pVCpu->cpum.GstCtx.r11; break;
                    case 12: u64EffAddr = pVCpu->cpum.GstCtx.r12; break;
                    case 13: u64EffAddr = pVCpu->cpum.GstCtx.r13; break;
                    case 14: u64EffAddr = pVCpu->cpum.GstCtx.r14; break;
                    case 15: u64EffAddr = pVCpu->cpum.GstCtx.r15; break;
                    IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                }
                u64EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                /* add base */
                switch ((bSib & X86_SIB_BASE_MASK) | pVCpu->iem.s.uRexB)
                {
                    case  0: u64EffAddr += pVCpu->cpum.GstCtx.rax; break;
                    case  1: u64EffAddr += pVCpu->cpum.GstCtx.rcx; break;
                    case  2: u64EffAddr += pVCpu->cpum.GstCtx.rdx; break;
                    case  3: u64EffAddr += pVCpu->cpum.GstCtx.rbx; break;
                    case  4: u64EffAddr += pVCpu->cpum.GstCtx.rsp + (cbImmAndRspOffset >> 8); SET_SS_DEF(); break;
                    case  6: u64EffAddr += pVCpu->cpum.GstCtx.rsi; break;
                    case  7: u64EffAddr += pVCpu->cpum.GstCtx.rdi; break;
                    case  8: u64EffAddr += pVCpu->cpum.GstCtx.r8;  break;
                    case  9: u64EffAddr += pVCpu->cpum.GstCtx.r9;  break;
                    case 10: u64EffAddr += pVCpu->cpum.GstCtx.r10; break;
                    case 11: u64EffAddr += pVCpu->cpum.GstCtx.r11; break;
                    case 12: u64EffAddr += pVCpu->cpum.GstCtx.r12; break;
                    case 14: u64EffAddr += pVCpu->cpum.GstCtx.r14; break;
                    case 15: u64EffAddr += pVCpu->cpum.GstCtx.r15; break;
                    /* complicated encodings */
                    case 5:
                    case 13:
                        if ((bRm & X86_MODRM_MOD_MASK) != 0)
                        {
                            if (!pVCpu->iem.s.uRexB)
                            {
                                u64EffAddr += pVCpu->cpum.GstCtx.rbp;
                                SET_SS_DEF();
                            }
                            else
                                u64EffAddr += pVCpu->cpum.GstCtx.r13;
                        }
                        else
                        {
                            uint32_t u32Disp;
                            IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                            u64EffAddr += (int32_t)u32Disp;
                            uInfo      |= u32Disp;
                        }
                        break;
                    IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
                }
                break;
            }
            IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX);
        }

        /* Get and add the displacement. */
        switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
        {
            case 0:
                break;
            case 1:
            {
                int8_t i8Disp;
                IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                u64EffAddr += i8Disp;
                uInfo      |= (uint32_t)(int32_t)i8Disp;
                break;
            }
            case 2:
            {
                uint32_t u32Disp;
                IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                u64EffAddr += (int32_t)u32Disp;
                uInfo      |= u32Disp;
                break;
            }
            IEM_NOT_REACHED_DEFAULT_CASE_RET2(RTGCPTR_MAX); /* (caller checked for these) */
        }

    }

    *puInfo = uInfo;
    if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
    {
        Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RGv uInfo=%#RX64\n", u64EffAddr, uInfo));
        return u64EffAddr;
    }
    Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT);
    Log5(("iemOpHlpCalcRmEffAddrJmp: EffAddr=%#010RGv uInfo=%#RX64\n", u64EffAddr & UINT32_MAX, uInfo));
    return u64EffAddr & UINT32_MAX;
}


/*********************************************************************************************************************************
*   Translation Block Cache.                                                                                                     *
*********************************************************************************************************************************/

/** @callback_method_impl{FNRTSORTCMP, Compare two TBs for pruning sorting purposes.}  */
static DECLCALLBACK(int) iemTbCachePruneCmpTb(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    PCIEMTB const  pTb1 = (PCIEMTB)pvElement1;
    PCIEMTB const  pTb2 = (PCIEMTB)pvElement2;
    uint32_t const cMsSinceUse1 = (uint32_t)(uintptr_t)pvUser - pTb1->msLastUsed;
    uint32_t const cMsSinceUse2 = (uint32_t)(uintptr_t)pvUser - pTb2->msLastUsed;
    if (cMsSinceUse1 != cMsSinceUse2)
        return cMsSinceUse1 < cMsSinceUse2 ? -1 : 1;
    if (pTb1->cUsed != pTb2->cUsed)
        return pTb1->cUsed > pTb2->cUsed ? -1 : 1;
    if ((pTb1->fFlags & IEMTB_F_TYPE_MASK) != (pTb2->fFlags & IEMTB_F_TYPE_MASK))
        return (pTb1->fFlags & IEMTB_F_TYPE_MASK) == IEMTB_F_TYPE_NATIVE ? -1 : 1;
    return 0;
}

#ifdef VBOX_STRICT
/**
 * Assertion helper that checks a collisions list count.
 */
static void iemTbCacheAssertCorrectCount(PIEMTBCACHE pTbCache, uint32_t idxHash, const char *pszOperation)
{
    PIEMTB pTb   = IEMTBCACHE_PTR_GET_TB(pTbCache->apHash[idxHash]);
    int    cLeft = IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]);
    while (pTb)
    {
        pTb = pTb->pNext;
        cLeft--;
    }
    AssertMsg(cLeft == 0,
              ("idxHash=%#x cLeft=%d; entry count=%d; %s\n",
               idxHash, cLeft, IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]), pszOperation));
}
#endif


DECL_NO_INLINE(static, void) iemTbCacheAddWithPruning(PVMCPUCC pVCpu, PIEMTBCACHE pTbCache, PIEMTB pTb, uint32_t idxHash)
{
    STAM_PROFILE_START(&pTbCache->StatPrune, a);

    /*
     * First convert the collision list to an array.
     */
    PIEMTB    apSortedTbs[IEMTBCACHE_PTR_MAX_COUNT];
    uintptr_t cInserted    = 0;
    PIEMTB    pTbCollision = IEMTBCACHE_PTR_GET_TB(pTbCache->apHash[idxHash]);

    pTbCache->apHash[idxHash] = NULL; /* Must NULL the entry before trying to free anything. */

    while (pTbCollision && cInserted < RT_ELEMENTS(apSortedTbs))
    {
        apSortedTbs[cInserted++] = pTbCollision;
        pTbCollision = pTbCollision->pNext;
    }

    /* Free any excess (impossible). */
    if (RT_LIKELY(!pTbCollision))
        Assert(cInserted == RT_ELEMENTS(apSortedTbs));
    else
        do
        {
            PIEMTB pTbToFree = pTbCollision;
            pTbCollision = pTbToFree->pNext;
            iemTbAllocatorFree(pVCpu, pTbToFree);
        } while (pTbCollision);

    /*
     * Sort it by most recently used and usage count.
     */
    RTSortApvShell((void **)apSortedTbs, cInserted, iemTbCachePruneCmpTb, (void *)(uintptr_t)pVCpu->iem.s.msRecompilerPollNow);

    /* We keep half the list for now. Perhaps a bit aggressive... */
    uintptr_t const cKeep = cInserted / 2;

    /* First free up the TBs we don't wish to keep (before creating the new
       list because otherwise the free code will scan the list for each one
       without ever finding it). */
    for (uintptr_t idx = cKeep; idx < cInserted; idx++)
        iemTbAllocatorFree(pVCpu, apSortedTbs[idx]);

    /* Then chain the new TB together with the ones we like to keep of the
       existing ones and insert this list into the hash table. */
    pTbCollision = pTb;
    for (uintptr_t idx = 0; idx < cKeep; idx++)
        pTbCollision = pTbCollision->pNext = apSortedTbs[idx];
    pTbCollision->pNext = NULL;

    pTbCache->apHash[idxHash] = IEMTBCACHE_PTR_MAKE(pTb, cKeep + 1);
#ifdef VBOX_STRICT
    iemTbCacheAssertCorrectCount(pTbCache, idxHash, "add w/ pruning");
#endif

    STAM_PROFILE_STOP(&pTbCache->StatPrune, a);
}


static void iemTbCacheAdd(PVMCPUCC pVCpu, PIEMTBCACHE pTbCache, PIEMTB pTb)
{
    uint32_t const idxHash    = IEMTBCACHE_HASH(pTbCache, pTb->fFlags, pTb->GCPhysPc);
    PIEMTB const   pTbOldHead = pTbCache->apHash[idxHash];
    if (!pTbOldHead)
    {
        pTb->pNext = NULL;
        pTbCache->apHash[idxHash] = IEMTBCACHE_PTR_MAKE(pTb, 1);  /** @todo could make 1 implicit... */
    }
    else
    {
        STAM_REL_COUNTER_INC(&pTbCache->cCollisions);
        uintptr_t cCollisions = IEMTBCACHE_PTR_GET_COUNT(pTbOldHead);
        if (cCollisions < IEMTBCACHE_PTR_MAX_COUNT)
        {
            pTb->pNext = IEMTBCACHE_PTR_GET_TB(pTbOldHead);
            pTbCache->apHash[idxHash] = IEMTBCACHE_PTR_MAKE(pTb, cCollisions + 1);
#ifdef VBOX_STRICT
            iemTbCacheAssertCorrectCount(pTbCache, idxHash, "add");
#endif
        }
        else
            iemTbCacheAddWithPruning(pVCpu, pTbCache, pTb, idxHash);
    }
}


/**
 * Unlinks @a pTb from the hash table if found in it.
 *
 * @returns true if unlinked, false if not present.
 * @param   pTbCache    The hash table.
 * @param   pTb         The TB to remove.
 */
static bool iemTbCacheRemove(PIEMTBCACHE pTbCache, PIEMTB pTb)
{
    uint32_t const idxHash = IEMTBCACHE_HASH(pTbCache, pTb->fFlags, pTb->GCPhysPc);
    PIEMTB         pTbHash = IEMTBCACHE_PTR_GET_TB(pTbCache->apHash[idxHash]);
    uint32_t volatile cLength = IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]); RT_NOREF(cLength);

    /*
     * At the head of the collision list?
     */
    if (pTbHash == pTb)
    {
        if (!pTb->pNext)
            pTbCache->apHash[idxHash] = NULL;
        else
        {
            pTbCache->apHash[idxHash] = IEMTBCACHE_PTR_MAKE(pTb->pNext,
                                                            IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]) - 1);
#ifdef VBOX_STRICT
            iemTbCacheAssertCorrectCount(pTbCache, idxHash, "remove #1");
#endif
        }
        return true;
    }

    /*
     * Search the collision list.
     */
    PIEMTB const pTbHead = pTbHash;
    while (pTbHash)
    {
        PIEMTB const pNextTb = pTbHash->pNext;
        if (pNextTb == pTb)
        {
            pTbHash->pNext = pTb->pNext;
            pTbCache->apHash[idxHash] = IEMTBCACHE_PTR_MAKE(pTbHead, IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]) - 1);
#ifdef VBOX_STRICT
            iemTbCacheAssertCorrectCount(pTbCache, idxHash, "remove #2");
#endif
            return true;
        }
        pTbHash = pNextTb;
    }
    return false;
}


/**
 * Looks up a TB for the given PC and flags in the cache.
 *
 * @returns Pointer to TB on success, NULL if not found.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling thread.
 * @param   pTbCache        The translation block cache.
 * @param   GCPhysPc        The PC to look up a TB for.
 * @param   fExtraFlags     The extra flags to join with IEMCPU::fExec for
 *                          the lookup.
 * @thread  EMT(pVCpu)
 */
static PIEMTB iemTbCacheLookup(PVMCPUCC pVCpu, PIEMTBCACHE pTbCache,
                               RTGCPHYS GCPhysPc, uint32_t fExtraFlags) IEM_NOEXCEPT_MAY_LONGJMP
{
    uint32_t const fFlags  = ((pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags) & IEMTB_F_KEY_MASK;
    uint32_t const idxHash = IEMTBCACHE_HASH_NO_KEY_MASK(pTbCache, fFlags, GCPhysPc);
    PIEMTB         pTb     = IEMTBCACHE_PTR_GET_TB(pTbCache->apHash[idxHash]);
#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
    int            cLeft   = IEMTBCACHE_PTR_GET_COUNT(pTbCache->apHash[idxHash]);
#endif
    Log10(("TB lookup: fFlags=%#x GCPhysPc=%RGp idxHash=%#x: %p L %d\n", fFlags, GCPhysPc, idxHash, pTb, cLeft));
    while (pTb)
    {
        if (pTb->GCPhysPc == GCPhysPc)
        {
            if ((pTb->fFlags & IEMTB_F_KEY_MASK) == fFlags)
            {
                if (pTb->x86.fAttr == (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u)
                {
                    STAM_COUNTER_INC(&pTbCache->cLookupHits);
                    AssertMsg(cLeft > 0, ("%d\n", cLeft));

                    pTb->msLastUsed = pVCpu->iem.s.msRecompilerPollNow;
                    pTb->cUsed++;
#ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
                    if ((pTb->fFlags & IEMTB_F_TYPE_NATIVE) || pTb->cUsed != 16)
                        return pTb;
                    return iemNativeRecompile(pVCpu, pTb);
#else
                    return pTb;
#endif
                }
                Log11(("TB miss: CS: %#x, wanted %#x\n", pTb->x86.fAttr, (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u));
            }
            else
                Log11(("TB miss: fFlags: %#x, wanted %#x\n", pTb->fFlags, fFlags));
        }
        else
            Log11(("TB miss: GCPhysPc: %#x, wanted %#x\n", pTb->GCPhysPc, GCPhysPc));

        pTb = pTb->pNext;
#ifdef VBOX_STRICT
        cLeft--;
#endif
    }
    AssertMsg(cLeft == 0, ("%d\n", cLeft));
    STAM_REL_COUNTER_INC(&pTbCache->cLookupMisses);
    return pTb;
}


/*********************************************************************************************************************************
*   Translation Block Allocator.
*********************************************************************************************************************************/
/*
 * Translation block allocationmanagement.
 */

#ifdef IEMTB_SIZE_IS_POWER_OF_TWO
# define IEMTBALLOC_IDX_TO_CHUNK(a_pTbAllocator, a_idxTb) \
    ((a_idxTb) >> (a_pTbAllocator)->cChunkShift)
# define IEMTBALLOC_IDX_TO_INDEX_IN_CHUNK(a_pTbAllocator, a_idxTb, a_idxChunk) \
    ((a_idxTb) &  (a_pTbAllocator)->fChunkMask)
# define IEMTBALLOC_IDX_FOR_CHUNK(a_pTbAllocator, a_idxChunk) \
    ((uint32_t)(a_idxChunk) << (a_pTbAllocator)->cChunkShift)
#else
# define IEMTBALLOC_IDX_TO_CHUNK(a_pTbAllocator, a_idxTb) \
    ((a_idxTb) / (a_pTbAllocator)->cTbsPerChunk)
# define IEMTBALLOC_IDX_TO_INDEX_IN_CHUNK(a_pTbAllocator, a_idxTb, a_idxChunk) \
    ((a_idxTb) - (a_idxChunk) * (a_pTbAllocator)->cTbsPerChunk)
# define IEMTBALLOC_IDX_FOR_CHUNK(a_pTbAllocator, a_idxChunk) \
    ((uint32_t)(a_idxChunk) * (a_pTbAllocator)->cTbsPerChunk)
#endif
/** Makes a TB index from a chunk index and TB index within that chunk. */
#define IEMTBALLOC_IDX_MAKE(a_pTbAllocator, a_idxChunk, a_idxInChunk) \
    (IEMTBALLOC_IDX_FOR_CHUNK(a_pTbAllocator, a_idxChunk) + (a_idxInChunk))


/**
 * Initializes the TB allocator and cache for an EMT.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   cInitialTbs     The initial number of translation blocks to
 *                          preallocator.
 * @param   cMaxTbs         The max number of translation blocks allowed.
 * @param   cbInitialExec   The initial size of the executable memory allocator.
 * @param   cbMaxExec       The max size of the executable memory allocator.
 * @param   cbChunkExec     The chunk size for executable memory allocator. Zero
 *                          or UINT32_MAX for automatically determining this.
 * @thread  EMT
 */
DECLCALLBACK(int) iemTbInit(PVMCC pVM, uint32_t cInitialTbs, uint32_t cMaxTbs,
                            uint64_t cbInitialExec, uint64_t cbMaxExec, uint32_t cbChunkExec)
{
    PVMCPUCC pVCpu = VMMGetCpu(pVM);
    Assert(!pVCpu->iem.s.pTbCacheR3);
    Assert(!pVCpu->iem.s.pTbAllocatorR3);

    /*
     * Calculate the chunk size of the TB allocator.
     * The minimum chunk size is 2MiB.
     */
    AssertCompile(!(sizeof(IEMTB) & IEMTBCACHE_PTR_COUNT_MASK));
    uint32_t      cbPerChunk   = _2M;
    uint32_t      cTbsPerChunk = _2M / sizeof(IEMTB);
#ifdef IEMTB_SIZE_IS_POWER_OF_TWO
    uint8_t const cTbShift     = ASMBitFirstSetU32((uint32_t)sizeof(IEMTB)) - 1;
    uint8_t       cChunkShift  = 21 - cTbShift;
    AssertCompile(RT_BIT_32(21) == _2M); Assert(RT_BIT_32(cChunkShift) == cTbsPerChunk);
#endif
    for (;;)
    {
        if (cMaxTbs <= cTbsPerChunk * (uint64_t)RT_ELEMENTS(pVCpu->iem.s.pTbAllocatorR3->aChunks))
            break;
        cbPerChunk  *= 2;
        cTbsPerChunk = cbPerChunk / sizeof(IEMTB);
#ifdef IEMTB_SIZE_IS_POWER_OF_TWO
        cChunkShift += 1;
#endif
    }

    uint32_t cMaxChunks = (cMaxTbs + cTbsPerChunk - 1) / cTbsPerChunk;
    Assert(cMaxChunks * cTbsPerChunk >= cMaxTbs);
    Assert(cMaxChunks <= RT_ELEMENTS(pVCpu->iem.s.pTbAllocatorR3->aChunks));

    cMaxTbs = cMaxChunks * cTbsPerChunk;

    /*
     * Allocate and initalize it.
     */
    uint32_t const        c64BitWords   = RT_ALIGN_32(cMaxTbs, 64) / 64;
    size_t const          cbTbAllocator = RT_UOFFSETOF_DYN(IEMTBALLOCATOR, bmAllocated[c64BitWords]);
    PIEMTBALLOCATOR const pTbAllocator  = (PIEMTBALLOCATOR)RTMemAllocZ(cbTbAllocator);
    if (!pTbAllocator)
        return VMSetError(pVM, VERR_NO_MEMORY, RT_SRC_POS,
                          "Failed to allocate %zu bytes (max %u TBs) for the TB allocator of VCpu #%u",
                          cbTbAllocator, cMaxTbs, pVCpu->idCpu);
    pTbAllocator->uMagic        = IEMTBALLOCATOR_MAGIC;
    pTbAllocator->cMaxChunks    = (uint8_t)cMaxChunks;
    pTbAllocator->cTbsPerChunk  = cTbsPerChunk;
    pTbAllocator->cbPerChunk    = cbPerChunk;
    pTbAllocator->cMaxTbs       = cMaxTbs;
#ifdef IEMTB_SIZE_IS_POWER_OF_TWO
    pTbAllocator->fChunkMask    = cTbsPerChunk - 1;
    pTbAllocator->cChunkShift   = cChunkShift;
    Assert(RT_BIT_32(cChunkShift) == cTbsPerChunk);
#endif

    memset(pTbAllocator->bmAllocated, 0xff, c64BitWords * sizeof(uint64_t)); /* Mark all as allocated, clear as chunks are added. */
    pVCpu->iem.s.pTbAllocatorR3 = pTbAllocator;

    /*
     * Allocate the initial chunks.
     */
    for (uint32_t idxChunk = 0; ; idxChunk++)
    {
        PIEMTB const paTbs = pTbAllocator->aChunks[idxChunk].paTbs = (PIEMTB)RTMemPageAllocZ(cbPerChunk);
        if (!paTbs)
            return VMSetError(pVM, VERR_NO_MEMORY, RT_SRC_POS,
                              "Failed to initial %zu bytes for the #%u chunk of TBs for VCpu #%u",
                              cbPerChunk, idxChunk, pVCpu->idCpu);

        for (uint32_t iTb = 0; iTb < cTbsPerChunk; iTb++)
            paTbs[iTb].idxAllocChunk = idxChunk; /* This is not strictly necessary... */
        ASMBitClearRange(pTbAllocator->bmAllocated, idxChunk * cTbsPerChunk, (idxChunk + 1) * cTbsPerChunk);
        pTbAllocator->cAllocatedChunks = (uint16_t)(idxChunk + 1);
        pTbAllocator->cTotalTbs       += cTbsPerChunk;

        if ((idxChunk + 1) * cTbsPerChunk >= cInitialTbs)
            break;
    }

    /*
     * Calculate the size of the hash table. We double the max TB count and
     * round it up to the nearest power of two.
     */
    uint32_t cCacheEntries = cMaxTbs * 2;
    if (!RT_IS_POWER_OF_TWO(cCacheEntries))
    {
        uint8_t const iBitTop = ASMBitFirstSetU32(cCacheEntries);
        cCacheEntries = RT_BIT_32(iBitTop);
        Assert(cCacheEntries >= cMaxTbs * 2);
    }

    size_t const      cbTbCache = RT_UOFFSETOF_DYN(IEMTBCACHE, apHash[cCacheEntries]);
    PIEMTBCACHE const pTbCache  = (PIEMTBCACHE)RTMemAllocZ(cbTbCache);
    if (!pTbCache)
        return VMSetError(pVM, VERR_NO_MEMORY, RT_SRC_POS,
                          "Failed to allocate %zu bytes (%u entries) for the TB cache of VCpu #%u",
                          cbTbCache, cCacheEntries, pVCpu->idCpu);

    /*
     * Initialize it (assumes zeroed by the allocator).
     */
    pTbCache->uMagic    = IEMTBCACHE_MAGIC;
    pTbCache->cHash     = cCacheEntries;
    pTbCache->uHashMask = cCacheEntries - 1;
    Assert(pTbCache->cHash > pTbCache->uHashMask);
    pVCpu->iem.s.pTbCacheR3 = pTbCache;

    /*
     * Initialize the native executable memory allocator.
     */
#ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
    int rc = iemExecMemAllocatorInit(pVCpu, cbMaxExec, cbInitialExec, cbChunkExec);
    AssertLogRelRCReturn(rc, rc);
#else
    RT_NOREF(cbMaxExec, cbInitialExec, cbChunkExec);
#endif

    return VINF_SUCCESS;
}


/**
 * Inner free worker.
 */
static void iemTbAllocatorFreeInner(PVMCPUCC pVCpu, PIEMTBALLOCATOR pTbAllocator,
                                    PIEMTB pTb, uint32_t idxChunk, uint32_t idxInChunk)
{
    Assert(idxChunk < pTbAllocator->cAllocatedChunks);
    Assert(idxInChunk < pTbAllocator->cTbsPerChunk);
    Assert((uintptr_t)(pTb - pTbAllocator->aChunks[idxChunk].paTbs) == idxInChunk);
    Assert(ASMBitTest(&pTbAllocator->bmAllocated, IEMTBALLOC_IDX_MAKE(pTbAllocator, idxChunk, idxInChunk)));

    /*
     * Unlink the TB from the hash table.
     */
    iemTbCacheRemove(pVCpu->iem.s.pTbCacheR3, pTb);

    /*
     * Free the TB itself.
     */
    switch (pTb->fFlags & IEMTB_F_TYPE_MASK)
    {
        case IEMTB_F_TYPE_THREADED:
            pTbAllocator->cThreadedTbs -= 1;
            RTMemFree(pTb->Thrd.paCalls);
            break;
#ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
        case IEMTB_F_TYPE_NATIVE:
            pTbAllocator->cNativeTbs -= 1;
            iemExecMemAllocatorFree(pVCpu, pTb->Native.paInstructions,
                                    pTb->Native.cInstructions * sizeof(pTb->Native.paInstructions[0]));
            break;
#endif
        default:
            AssertFailed();
    }
    RTMemFree(pTb->pabOpcodes);

    pTb->pNext              = NULL;
    pTb->fFlags             = 0;
    pTb->GCPhysPc           = UINT64_MAX;
    pTb->Gen.uPtr           = 0;
    pTb->Gen.uData          = 0;
    pTb->cbOpcodes          = 0;
    pTb->pabOpcodes         = NULL;

    ASMBitClear(&pTbAllocator->bmAllocated, IEMTBALLOC_IDX_MAKE(pTbAllocator, idxChunk, idxInChunk));
    Assert(pTbAllocator->cInUseTbs > 0);

    pTbAllocator->cInUseTbs -= 1;
    STAM_REL_COUNTER_INC(&pTbAllocator->StatFrees);
}


/**
 * Frees the given TB.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to free.
 * @thread  EMT(pVCpu)
 */
static void iemTbAllocatorFree(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * Validate state.
     */
    PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
    Assert(pTbAllocator && pTbAllocator->uMagic == IEMTBALLOCATOR_MAGIC);
    uint8_t const idxChunk = pTb->idxAllocChunk;
    AssertLogRelReturnVoid(idxChunk < pTbAllocator->cAllocatedChunks);
    uintptr_t const idxInChunk = pTb - pTbAllocator->aChunks[idxChunk].paTbs;
    AssertLogRelReturnVoid(idxInChunk < pTbAllocator->cTbsPerChunk);

    /*
     * Call inner worker.
     */
    iemTbAllocatorFreeInner(pVCpu, pTbAllocator, pTb, idxChunk, (uint32_t)idxInChunk);
}


/**
 * Schedules a native TB for freeing when it's not longer being executed and
 * part of the caller's call stack.
 *
 * The TB will be removed from the translation block cache, though, so it isn't
 * possible to executed it again and the IEMTB::pNext member can be used to link
 * it together with other TBs awaiting freeing.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to schedule for freeing.
 */
static void iemTbAlloctorScheduleForFree(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * Validate state.
     */
    PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
    Assert(pTbAllocator && pTbAllocator->uMagic == IEMTBALLOCATOR_MAGIC);
    Assert(pTb->idxAllocChunk < pTbAllocator->cAllocatedChunks);
    Assert((uintptr_t)(pTb - pTbAllocator->aChunks[pTb->idxAllocChunk].paTbs) < pTbAllocator->cTbsPerChunk);
    Assert(ASMBitTest(&pTbAllocator->bmAllocated,
                      IEMTBALLOC_IDX_MAKE(pTbAllocator, pTb->idxAllocChunk,
                                          (uintptr_t)(pTb - pTbAllocator->aChunks[pTb->idxAllocChunk].paTbs))));
    Assert((pTb->fFlags & IEMTB_F_TYPE_MASK) == IEMTB_F_TYPE_NATIVE);

    /*
     * Remove it from the cache and prepend it to the allocator's todo list.
     */
    iemTbCacheRemove(pVCpu->iem.s.pTbCacheR3, pTb);

    pTb->pNext = pTbAllocator->pDelayedFreeHead;
    pTbAllocator->pDelayedFreeHead = pTb;
}


/**
 * Processes the delayed frees.
 *
 * This is called by the allocator function as well as the native recompile
 * function before making any TB or executable memory allocations respectively.
 */
void iemTbAllocatorProcessDelayedFrees(PVMCPU pVCpu, PIEMTBALLOCATOR pTbAllocator)
{
    PIEMTB pTb = pTbAllocator->pDelayedFreeHead;
    pTbAllocator->pDelayedFreeHead = NULL;
    while (pTb)
    {
        PIEMTB const pTbNext = pTb->pNext;
        Assert(pVCpu->iem.s.pCurTbR3 != pTb);
        iemTbAlloctorScheduleForFree(pVCpu, pTb);
        pTb = pTbNext;
    }
}


/**
 * Grow the translation block allocator with another chunk.
 */
static int iemTbAllocatorGrow(PVMCPUCC pVCpu)
{
    /*
     * Validate state.
     */
    PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
    AssertReturn(pTbAllocator, VERR_WRONG_ORDER);
    AssertReturn(pTbAllocator->uMagic == IEMTBALLOCATOR_MAGIC, VERR_INVALID_MAGIC);
    uint32_t const idxChunk = pTbAllocator->cAllocatedChunks;
    AssertReturn(idxChunk < pTbAllocator->cMaxChunks, VERR_OUT_OF_RESOURCES);

    /*
     * Allocate a new chunk and add it to the allocator.
     */
    PIEMTB const paTbs = (PIEMTB)RTMemPageAllocZ(pTbAllocator->cbPerChunk);
    AssertLogRelReturn(paTbs, VERR_NO_PAGE_MEMORY);
    pTbAllocator->aChunks[idxChunk].paTbs = paTbs;

    uint32_t const cTbsPerChunk = pTbAllocator->cTbsPerChunk;
    for (uint32_t iTb = 0; iTb < cTbsPerChunk; iTb++)
        paTbs[iTb].idxAllocChunk = idxChunk; /* This is not strictly necessary... */
    ASMBitClearRange(pTbAllocator->bmAllocated, idxChunk * cTbsPerChunk, (idxChunk + 1) * cTbsPerChunk);
    pTbAllocator->cAllocatedChunks = (uint16_t)(idxChunk + 1);
    pTbAllocator->cTotalTbs       += cTbsPerChunk;
    pTbAllocator->iStartHint       = idxChunk * cTbsPerChunk;

    return VINF_SUCCESS;
}


/**
 * Allocates a TB from allocator with free block.
 *
 * This is common code to both the fast and slow allocator code paths.
 */
DECL_FORCE_INLINE(PIEMTB) iemTbAllocatorAllocCore(PIEMTBALLOCATOR const pTbAllocator, bool fThreaded)
{
    Assert(pTbAllocator->cInUseTbs < pTbAllocator->cTotalTbs);

    int idxTb;
    if (pTbAllocator->iStartHint < pTbAllocator->cTotalTbs)
        idxTb = ASMBitNextClear(pTbAllocator->bmAllocated,
                                pTbAllocator->cTotalTbs,
                                pTbAllocator->iStartHint & ~(uint32_t)63);
    else
        idxTb = -1;
    if (idxTb < 0)
    {
        idxTb = ASMBitFirstClear(pTbAllocator->bmAllocated, pTbAllocator->cTotalTbs);
        AssertLogRelReturn(idxTb >= 0, NULL);
    }
    Assert((uint32_t)idxTb < pTbAllocator->cTotalTbs);
    ASMBitSet(pTbAllocator->bmAllocated, idxTb);

    /** @todo shift/mask optimization for power of two IEMTB sizes. */
    uint32_t const idxChunk     = IEMTBALLOC_IDX_TO_CHUNK(pTbAllocator, idxTb);
    uint32_t const idxTbInChunk = IEMTBALLOC_IDX_TO_INDEX_IN_CHUNK(pTbAllocator, idxTb, idxChunk);
    PIEMTB const   pTb          = &pTbAllocator->aChunks[idxChunk].paTbs[idxTbInChunk];
    Assert(pTb->idxAllocChunk == idxChunk);

    pTbAllocator->cInUseTbs        += 1;
    if (fThreaded)
        pTbAllocator->cThreadedTbs += 1;
    else
        pTbAllocator->cNativeTbs   += 1;
    STAM_REL_COUNTER_INC(&pTbAllocator->StatAllocs);
    return pTb;
}


/**
 * Slow path for iemTbAllocatorAlloc.
 */
static PIEMTB iemTbAllocatorAllocSlow(PVMCPUCC pVCpu, PIEMTBALLOCATOR const pTbAllocator, bool fThreaded)
{
    /*
     * With some luck we can add another chunk.
     */
    if (pTbAllocator->cAllocatedChunks < pTbAllocator->cMaxChunks)
    {
        int rc = iemTbAllocatorGrow(pVCpu);
        if (RT_SUCCESS(rc))
            return iemTbAllocatorAllocCore(pTbAllocator, fThreaded);
    }

    /*
     * We have to prune stuff. Sigh.
     *
     * This requires scanning for older TBs and kick them out.  Not sure how to
     * best do this as we don't want to maintain any list of TBs ordered by last
     * usage time. But one reasonably simple approach would be that each time we
     * get here we continue a sequential scan of the allocation chunks,
     * considering just a smallish number of TBs and freeing a fixed portion of
     * them.  Say, we consider the next 128 TBs, freeing the least recently used
     * in out of groups of 4 TBs, resulting in 32 free TBs.
     */
    STAM_PROFILE_START(&pTbAllocator->StatPrune, a);
    uint32_t const msNow          = pVCpu->iem.s.msRecompilerPollNow;
    uint32_t const cTbsToPrune    = 128;
    uint32_t const cTbsPerGroup   = 4;
    uint32_t       cFreedTbs      = 0;
#ifdef IEMTB_SIZE_IS_POWER_OF_TWO
    uint32_t       idxTbPruneFrom = pTbAllocator->iPruneFrom & ~(uint32_t)(cTbsToPrune - 1); /* Stay within a chunk! */
#else
    uint32_t       idxTbPruneFrom = pTbAllocator->iPruneFrom;
#endif
    if (idxTbPruneFrom >= pTbAllocator->cMaxTbs)
        idxTbPruneFrom = 0;
    for (uint32_t i = 0; i < cTbsToPrune; i += cTbsPerGroup, idxTbPruneFrom += cTbsPerGroup)
    {
        uint32_t idxChunk   = IEMTBALLOC_IDX_TO_CHUNK(pTbAllocator, idxTbPruneFrom);
        uint32_t idxInChunk = IEMTBALLOC_IDX_TO_INDEX_IN_CHUNK(pTbAllocator, idxTbPruneFrom, idxChunk);
        PIEMTB   pTb        = &pTbAllocator->aChunks[idxChunk].paTbs[idxInChunk];
        uint32_t cMsAge     = msNow - pTb->msLastUsed;
        Assert(pTb->fFlags & IEMTB_F_TYPE_MASK);

        for (uint32_t j = 1, idxChunk2 = idxChunk, idxInChunk2 = idxInChunk + 1; j < cTbsPerGroup; j++, idxInChunk2++)
        {
#ifndef IEMTB_SIZE_IS_POWER_OF_TWO
            if (idxInChunk2 < pTbAllocator->cTbsPerChunk)
            { /* likely */ }
            else
            {
                idxInChunk2 = 0;
                idxChunk2  += 1;
                if (idxChunk2 >= pTbAllocator->cAllocatedChunks)
                    idxChunk2 = 0;
            }
#endif
            PIEMTB   const pTb2    = &pTbAllocator->aChunks[idxChunk2].paTbs[idxInChunk2];
            uint32_t const cMsAge2 = msNow - pTb2->msLastUsed;
            if (   cMsAge2 > cMsAge
                || (cMsAge2 == cMsAge && pTb2->cUsed < pTb->cUsed))
            {
                Assert(pTb2->fFlags & IEMTB_F_TYPE_MASK);
                pTb        = pTb2;
                idxChunk   = idxChunk2;
                idxInChunk = idxInChunk2;
                cMsAge     = cMsAge2;
            }
        }

        /* Free the TB. */
        iemTbAllocatorFreeInner(pVCpu, pTbAllocator, pTb, idxChunk, idxInChunk);
        cFreedTbs++; /* paranoia */
    }
    pTbAllocator->iPruneFrom = idxTbPruneFrom;
    STAM_PROFILE_STOP(&pTbAllocator->StatPrune, a);

    /*
     * Allocate a TB from the ones we've pruned.
     */
    if (cFreedTbs)
        return iemTbAllocatorAllocCore(pTbAllocator, fThreaded);
    return NULL;
}


/**
 * Allocate a translation block.
 *
 * @returns Pointer to block on success, NULL if we're out and is unable to
 *          free up an existing one (very unlikely once implemented).
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   fThreaded   Set if threaded TB being allocated, clear if native TB.
 *                      For statistics.
 */
DECL_FORCE_INLINE(PIEMTB) iemTbAllocatorAlloc(PVMCPUCC pVCpu, bool fThreaded)
{
    PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
    Assert(pTbAllocator && pTbAllocator->uMagic == IEMTBALLOCATOR_MAGIC);

    /* Free any pending TBs before we proceed. */
    if (!pTbAllocator->pDelayedFreeHead)
    { /* probably likely */ }
    else
        iemTbAllocatorProcessDelayedFrees(pVCpu, pTbAllocator);

    /* If the allocator is full, take slow code path.*/
    if (RT_LIKELY(pTbAllocator->cInUseTbs < pTbAllocator->cTotalTbs))
        return iemTbAllocatorAllocCore(pTbAllocator, fThreaded);
    return iemTbAllocatorAllocSlow(pVCpu, pTbAllocator, fThreaded);
}



/*********************************************************************************************************************************
*   Threaded Recompiler Core                                                                                                     *
*********************************************************************************************************************************/

/**
 * Allocate a translation block for threadeded recompilation.
 *
 * This is allocated with maxed out call table and storage for opcode bytes,
 * because it's only supposed to be called once per EMT to allocate the TB
 * pointed to by IEMCPU::pThrdCompileTbR3.
 *
 * @returns Pointer to the translation block on success, NULL on failure.
 * @param   pVM         The cross context virtual machine structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   GCPhysPc    The physical address corresponding to RIP + CS.BASE.
 * @param   fExtraFlags Extra flags (IEMTB_F_XXX).
 */
static PIEMTB iemThreadedTbAlloc(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags)
{
    PIEMTB pTb = (PIEMTB)RTMemAllocZ(sizeof(IEMTB));
    if (pTb)
    {
        unsigned const cCalls = 256;
        pTb->Thrd.paCalls = (PIEMTHRDEDCALLENTRY)RTMemAlloc(sizeof(IEMTHRDEDCALLENTRY) * cCalls);
        if (pTb->Thrd.paCalls)
        {
            pTb->pabOpcodes = (uint8_t *)RTMemAlloc(cCalls * 16);
            if (pTb->pabOpcodes)
            {
                pVCpu->iem.s.cbOpcodesAllocated = cCalls * 16;
                pTb->Thrd.cAllocated        = cCalls;
                pTb->Thrd.cCalls            = 0;
                pTb->cbOpcodes              = 0;
                pTb->pNext                  = NULL;
                pTb->cUsed                  = 0;
                pTb->msLastUsed             = pVCpu->iem.s.msRecompilerPollNow;
                pTb->idxAllocChunk          = UINT8_MAX;
                pTb->GCPhysPc               = GCPhysPc;
                pTb->x86.fAttr              = (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u;
                pTb->fFlags                 = (pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags;
                pTb->cInstructions          = 0;

                /* Init the first opcode range. */
                pTb->cRanges                = 1;
                pTb->aRanges[0].cbOpcodes   = 0;
                pTb->aRanges[0].offOpcodes  = 0;
                pTb->aRanges[0].offPhysPage = GCPhysPc & GUEST_PAGE_OFFSET_MASK;
                pTb->aRanges[0].u2Unused    = 0;
                pTb->aRanges[0].idxPhysPage = 0;
                pTb->aGCPhysPages[0]        = NIL_RTGCPHYS;
                pTb->aGCPhysPages[1]        = NIL_RTGCPHYS;

                return pTb;
            }
            RTMemFree(pTb->Thrd.paCalls);
        }
        RTMemFree(pTb);
    }
    RT_NOREF(pVM);
    return NULL;
}


/**
 * Called on the TB that are dedicated for recompilation before it's reused.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block to reuse.
 * @param   GCPhysPc    The physical address corresponding to RIP + CS.BASE.
 * @param   fExtraFlags Extra flags (IEMTB_F_XXX).
 */
static void iemThreadedTbReuse(PVMCPUCC pVCpu, PIEMTB pTb, RTGCPHYS GCPhysPc, uint32_t fExtraFlags)
{
    pTb->GCPhysPc               = GCPhysPc;
    pTb->fFlags                 = (pVCpu->iem.s.fExec & IEMTB_F_IEM_F_MASK) | fExtraFlags;
    pTb->x86.fAttr              = (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u;
    pTb->Thrd.cCalls            = 0;
    pTb->cbOpcodes              = 0;
    pTb->cInstructions          = 0;

    /* Init the first opcode range. */
    pTb->cRanges                = 1;
    pTb->aRanges[0].cbOpcodes   = 0;
    pTb->aRanges[0].offOpcodes  = 0;
    pTb->aRanges[0].offPhysPage = GCPhysPc & GUEST_PAGE_OFFSET_MASK;
    pTb->aRanges[0].u2Unused    = 0;
    pTb->aRanges[0].idxPhysPage = 0;
    pTb->aGCPhysPages[0]        = NIL_RTGCPHYS;
    pTb->aGCPhysPages[1]        = NIL_RTGCPHYS;
}


/**
 * Used to duplicate a threded translation block after recompilation is done.
 *
 * @returns Pointer to the translation block on success, NULL on failure.
 * @param   pVM         The cross context virtual machine structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTbSrc      The TB to duplicate.
 */
static PIEMTB iemThreadedTbDuplicate(PVMCC pVM, PVMCPUCC pVCpu, PCIEMTB pTbSrc)
{
    /*
     * Just using the heap for now.  Will make this more efficient and
     * complicated later, don't worry. :-)
     */
    PIEMTB pTb = iemTbAllocatorAlloc(pVCpu, true /*fThreaded*/);
    if (pTb)
    {
        uint8_t const idxAllocChunk = pTb->idxAllocChunk;
        memcpy(pTb, pTbSrc, sizeof(*pTb));
        pTb->idxAllocChunk = idxAllocChunk;

        unsigned const cCalls = pTbSrc->Thrd.cCalls;
        Assert(cCalls > 0);
        pTb->Thrd.paCalls = (PIEMTHRDEDCALLENTRY)RTMemDup(pTbSrc->Thrd.paCalls, sizeof(IEMTHRDEDCALLENTRY) * cCalls);
        if (pTb->Thrd.paCalls)
        {
            unsigned const cbOpcodes = pTbSrc->cbOpcodes;
            Assert(cbOpcodes > 0);
            pTb->pabOpcodes = (uint8_t *)RTMemDup(pTbSrc->pabOpcodes, cbOpcodes);
            if (pTb->pabOpcodes)
            {
                pTb->Thrd.cAllocated    = cCalls;
                pTb->pNext              = NULL;
                pTb->cUsed              = 0;
                pTb->msLastUsed         = pVCpu->iem.s.msRecompilerPollNow;
                pTb->fFlags             = pTbSrc->fFlags;

                return pTb;
            }
            RTMemFree(pTb->Thrd.paCalls);
        }
        iemTbAllocatorFree(pVCpu, pTb);
    }
    RT_NOREF(pVM);
    return NULL;

}


/**
 * Adds the given TB to the hash table.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTbCache    The cache to add it to.
 * @param   pTb         The translation block to add.
 */
static void iemThreadedTbAdd(PVMCPUCC pVCpu, PIEMTBCACHE pTbCache, PIEMTB pTb)
{
    iemTbCacheAdd(pVCpu, pTbCache, pTb);

    STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatTbThreadedInstr, pTb->cInstructions);
    STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatTbThreadedCalls, pTb->Thrd.cCalls);
    if (LogIs12Enabled())
    {
        Log12(("TB added: %p %RGp LB %#x fl=%#x idxHash=%#x cRanges=%u cInstr=%u cCalls=%u\n",
               pTb, pTb->GCPhysPc, pTb->cbOpcodes, pTb->fFlags, IEMTBCACHE_HASH(pTbCache, pTb->fFlags, pTb->GCPhysPc),
               pTb->cRanges, pTb->cInstructions, pTb->Thrd.cCalls));
        for (uint8_t idxRange = 0; idxRange < pTb->cRanges; idxRange++)
            Log12((" range#%u: offPg=%#05x offOp=%#04x LB %#04x pg#%u=%RGp\n", idxRange, pTb->aRanges[idxRange].offPhysPage,
                   pTb->aRanges[idxRange].offOpcodes, pTb->aRanges[idxRange].cbOpcodes, pTb->aRanges[idxRange].idxPhysPage,
                   pTb->aRanges[idxRange].idxPhysPage == 0
                   ? pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK
                   : pTb->aGCPhysPages[pTb->aRanges[idxRange].idxPhysPage - 1]));
    }
}


/**
 * Called by opcode verifier functions when they detect a problem.
 */
void iemThreadedTbObsolete(PVMCPUCC pVCpu, PIEMTB pTb, bool fSafeToFree)
{
    /* Unless it's safe, we can only immediately free threaded TB, as we will
       have more code left to execute in native TBs when fSafeToFree == false.  */
    if (fSafeToFree || (pTb->fFlags & IEMTB_F_TYPE_THREADED))
        iemTbAllocatorFree(pVCpu, pTb);
    else
        iemTbAlloctorScheduleForFree(pVCpu, pTb);
}


/*
 * Real code.
 */

#ifdef LOG_ENABLED
/**
 * Logs the current instruction.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pszFunction The IEM function doing the execution.
 * @param   idxInstr    The instruction number in the block.
 */
static void iemThreadedLogCurInstr(PVMCPUCC pVCpu, const char *pszFunction, uint32_t idxInstr) RT_NOEXCEPT
{
# ifdef IN_RING3
    if (LogIs2Enabled())
    {
        char     szInstr[256];
        uint32_t cbInstr = 0;
        DBGFR3DisasInstrEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), &cbInstr);

        PCX86FXSTATE pFpuCtx = &pVCpu->cpum.GstCtx.XState.x87;
        Log2(("**** %s fExec=%x pTb=%p cUsed=%u #%u\n"
              " eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
              " eip=%08x esp=%08x ebp=%08x iopl=%d tr=%04x\n"
              " cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x efl=%08x\n"
              " fsw=%04x fcw=%04x ftw=%02x mxcsr=%04x/%04x\n"
              " %s\n"
              , pszFunction, pVCpu->iem.s.fExec, pVCpu->iem.s.pCurTbR3, pVCpu->iem.s.pCurTbR3 ? pVCpu->iem.s.pCurTbR3->cUsed : 0, idxInstr,
              pVCpu->cpum.GstCtx.eax, pVCpu->cpum.GstCtx.ebx, pVCpu->cpum.GstCtx.ecx, pVCpu->cpum.GstCtx.edx, pVCpu->cpum.GstCtx.esi, pVCpu->cpum.GstCtx.edi,
              pVCpu->cpum.GstCtx.eip, pVCpu->cpum.GstCtx.esp, pVCpu->cpum.GstCtx.ebp, pVCpu->cpum.GstCtx.eflags.Bits.u2IOPL, pVCpu->cpum.GstCtx.tr.Sel,
              pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.ss.Sel, pVCpu->cpum.GstCtx.ds.Sel, pVCpu->cpum.GstCtx.es.Sel,
              pVCpu->cpum.GstCtx.fs.Sel, pVCpu->cpum.GstCtx.gs.Sel, pVCpu->cpum.GstCtx.eflags.u,
              pFpuCtx->FSW, pFpuCtx->FCW, pFpuCtx->FTW, pFpuCtx->MXCSR, pFpuCtx->MXCSR_MASK,
              szInstr));

        if (LogIs3Enabled())
            DBGFR3InfoEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, "cpumguest", "verbose", NULL);
    }
    else
# endif
        LogFlow(("%s: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x\n", pszFunction, pVCpu->cpum.GstCtx.cs.Sel,
                 pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.ss.Sel, pVCpu->cpum.GstCtx.rsp, pVCpu->cpum.GstCtx.eflags.u));
}
#endif /* LOG_ENABLED */


#if 0
static VBOXSTRICTRC iemThreadedCompileLongJumped(PVMCC pVM, PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict)
{
    RT_NOREF(pVM, pVCpu);
    return rcStrict;
}
#endif


/**
 * Initializes the decoder state when compiling TBs.
 *
 * This presumes that fExec has already be initialized.
 *
 * This is very similar to iemInitDecoder() and iemReInitDecoder(), so may need
 * to apply fixes to them as well.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   fReInit     Clear for the first call for a TB, set for subsequent
 *                      calls from inside the compile loop where we can skip a
 *                      couple of things.
 * @param   fExtraFlags The extra translation block flags when @a fReInit is
 *                      true, otherwise ignored.  Only IEMTB_F_INHIBIT_SHADOW is
 *                      checked.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileInitDecoder(PVMCPUCC pVCpu, bool const fReInit, uint32_t const fExtraFlags)
{
    /* ASSUMES: That iemInitExec was already called and that anyone changing
       CPU state affecting the fExec bits since then will have updated fExec!  */
    AssertMsg((pVCpu->iem.s.fExec & ~IEM_F_USER_OPTS) == iemCalcExecFlags(pVCpu),
              ("fExec=%#x iemCalcExecModeFlags=%#x\n", pVCpu->iem.s.fExec, iemCalcExecFlags(pVCpu)));

    IEMMODE const enmMode = IEM_GET_CPU_MODE(pVCpu);

    /* Decoder state: */
    pVCpu->iem.s.enmDefAddrMode     = enmMode;  /** @todo check if this is correct... */
    pVCpu->iem.s.enmEffAddrMode     = enmMode;
    if (enmMode != IEMMODE_64BIT)
    {
        pVCpu->iem.s.enmDefOpSize   = enmMode;  /** @todo check if this is correct... */
        pVCpu->iem.s.enmEffOpSize   = enmMode;
    }
    else
    {
        pVCpu->iem.s.enmDefOpSize   = IEMMODE_32BIT;
        pVCpu->iem.s.enmEffOpSize   = IEMMODE_32BIT;
    }
    pVCpu->iem.s.fPrefixes          = 0;
    pVCpu->iem.s.uRexReg            = 0;
    pVCpu->iem.s.uRexB              = 0;
    pVCpu->iem.s.uRexIndex          = 0;
    pVCpu->iem.s.idxPrefix          = 0;
    pVCpu->iem.s.uVex3rdReg         = 0;
    pVCpu->iem.s.uVexLength         = 0;
    pVCpu->iem.s.fEvexStuff         = 0;
    pVCpu->iem.s.iEffSeg            = X86_SREG_DS;
    pVCpu->iem.s.offModRm           = 0;
    pVCpu->iem.s.iNextMapping       = 0;

    if (!fReInit)
    {
        pVCpu->iem.s.cActiveMappings        = 0;
        pVCpu->iem.s.rcPassUp               = VINF_SUCCESS;
        pVCpu->iem.s.fEndTb                 = false;
        pVCpu->iem.s.fTbCheckOpcodes        = false;
        pVCpu->iem.s.fTbBranched            = IEMBRANCHED_F_NO;
        pVCpu->iem.s.fTbCrossedPage         = false;
        pVCpu->iem.s.cInstrTillIrqCheck     = !(fExtraFlags & IEMTB_F_INHIBIT_SHADOW) ? 32 : 0;
        pVCpu->iem.s.fTbCurInstrIsSti       = false;
        /* Force RF clearing and TF checking on first instruction in the block
           as we don't really know what came before and should assume the worst: */
        pVCpu->iem.s.fTbPrevInstr           = IEM_CIMPL_F_RFLAGS | IEM_CIMPL_F_END_TB;
    }
    else
    {
        Assert(pVCpu->iem.s.cActiveMappings == 0);
        Assert(pVCpu->iem.s.rcPassUp        == VINF_SUCCESS);
        Assert(pVCpu->iem.s.fEndTb          == false);
        Assert(pVCpu->iem.s.fTbCrossedPage  == false);
        pVCpu->iem.s.fTbPrevInstr           = pVCpu->iem.s.fTbCurInstr;
    }
    pVCpu->iem.s.fTbCurInstr                = 0;

#ifdef DBGFTRACE_ENABLED
    switch (IEM_GET_CPU_MODE(pVCpu))
    {
        case IEMMODE_64BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I64/%u %08llx", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.rip);
            break;
        case IEMMODE_32BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I32/%u %04x:%08x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
        case IEMMODE_16BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I16/%u %04x:%04x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
    }
#endif
}


/**
 * Initializes the opcode fetcher when starting the compilation.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileInitOpcodeFetching(PVMCPUCC pVCpu)
{
    /* Almost everything is done by iemGetPcWithPhysAndCode() already.  We just need to initialize the index into abOpcode. */
#ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    pVCpu->iem.s.offOpcode          = 0;
#else
    RT_NOREF(pVCpu);
#endif
}


/**
 * Re-initializes the opcode fetcher between instructions while compiling.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(void) iemThreadedCompileReInitOpcodeFetching(PVMCPUCC pVCpu)
{
    if (pVCpu->iem.s.pbInstrBuf)
    {
        uint64_t off = pVCpu->cpum.GstCtx.rip;
        Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu));
        off += pVCpu->cpum.GstCtx.cs.u64Base;
        off -= pVCpu->iem.s.uInstrBufPc;
        if (off < pVCpu->iem.s.cbInstrBufTotal)
        {
            pVCpu->iem.s.offInstrNextByte = (uint32_t)off;
            pVCpu->iem.s.offCurInstrStart = (uint16_t)off;
            if ((uint16_t)off + 15 <= pVCpu->iem.s.cbInstrBufTotal)
                pVCpu->iem.s.cbInstrBuf = (uint16_t)off + 15;
            else
                pVCpu->iem.s.cbInstrBuf = pVCpu->iem.s.cbInstrBufTotal;
        }
        else
        {
            pVCpu->iem.s.pbInstrBuf       = NULL;
            pVCpu->iem.s.offInstrNextByte = 0;
            pVCpu->iem.s.offCurInstrStart = 0;
            pVCpu->iem.s.cbInstrBuf       = 0;
            pVCpu->iem.s.cbInstrBufTotal  = 0;
            pVCpu->iem.s.GCPhysInstrBuf   = NIL_RTGCPHYS;
        }
    }
    else
    {
        pVCpu->iem.s.offInstrNextByte = 0;
        pVCpu->iem.s.offCurInstrStart = 0;
        pVCpu->iem.s.cbInstrBuf       = 0;
        pVCpu->iem.s.cbInstrBufTotal  = 0;
#ifdef VBOX_STRICT
        pVCpu->iem.s.GCPhysInstrBuf   = NIL_RTGCPHYS;
#endif
    }
#ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    pVCpu->iem.s.offOpcode          = 0;
#endif
}


DECLINLINE(void) iemThreadedCopyOpcodeBytesInline(PCVMCPUCC pVCpu, uint8_t *pbDst, uint8_t cbInstr)
{
    switch (cbInstr)
    {
        default: AssertMsgFailed(("%#x\n", cbInstr)); RT_FALL_THROUGH();
        case 15:    pbDst[14] = pVCpu->iem.s.abOpcode[14]; RT_FALL_THROUGH();
        case 14:    pbDst[13] = pVCpu->iem.s.abOpcode[13]; RT_FALL_THROUGH();
        case 13:    pbDst[12] = pVCpu->iem.s.abOpcode[12]; RT_FALL_THROUGH();
        case 12:    pbDst[11] = pVCpu->iem.s.abOpcode[11]; RT_FALL_THROUGH();
        case 11:    pbDst[10] = pVCpu->iem.s.abOpcode[10]; RT_FALL_THROUGH();
        case 10:    pbDst[9]  = pVCpu->iem.s.abOpcode[9];  RT_FALL_THROUGH();
        case 9:     pbDst[8]  = pVCpu->iem.s.abOpcode[8];  RT_FALL_THROUGH();
        case 8:     pbDst[7]  = pVCpu->iem.s.abOpcode[7];  RT_FALL_THROUGH();
        case 7:     pbDst[6]  = pVCpu->iem.s.abOpcode[6];  RT_FALL_THROUGH();
        case 6:     pbDst[5]  = pVCpu->iem.s.abOpcode[5];  RT_FALL_THROUGH();
        case 5:     pbDst[4]  = pVCpu->iem.s.abOpcode[4];  RT_FALL_THROUGH();
        case 4:     pbDst[3]  = pVCpu->iem.s.abOpcode[3];  RT_FALL_THROUGH();
        case 3:     pbDst[2]  = pVCpu->iem.s.abOpcode[2];  RT_FALL_THROUGH();
        case 2:     pbDst[1]  = pVCpu->iem.s.abOpcode[1];  RT_FALL_THROUGH();
        case 1:     pbDst[0]  = pVCpu->iem.s.abOpcode[0];  break;
    }
}


/**
 * Called by IEM_MC2_BEGIN_EMIT_CALLS() under one of these conditions:
 *
 *      - CS LIM check required.
 *      - Must recheck opcode bytes.
 *      - Previous instruction branched.
 *      - TLB load detected, probably due to page crossing.
 *
 * @returns true if everything went well, false if we're out of space in the TB
 *          (e.g. opcode ranges) or needs to start doing CS.LIM checks.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block being compiled.
 */
bool iemThreadedCompileBeginEmitCallsComplications(PVMCPUCC pVCpu, PIEMTB pTb)
{
    Log6(("%04x:%08RX64: iemThreadedCompileBeginEmitCallsComplications\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    Assert((pVCpu->iem.s.GCPhysInstrBuf & GUEST_PAGE_OFFSET_MASK) == 0);
#if 0
    if (pVCpu->cpum.GstCtx.rip >= 0xc0000000 && !LogIsEnabled())
        RTLogChangeFlags(NULL, 0, RTLOGFLAGS_DISABLED);
#endif

    /*
     * If we're not in 64-bit mode and not already checking CS.LIM we need to
     * see if it's needed to start checking.
     */
    bool           fConsiderCsLimChecking;
    uint32_t const fMode = pVCpu->iem.s.fExec & IEM_F_MODE_MASK;
    if (   fMode == IEM_F_MODE_X86_64BIT
        || (pTb->fFlags & IEMTB_F_CS_LIM_CHECKS)
        || fMode == IEM_F_MODE_X86_32BIT_PROT_FLAT
        || fMode == IEM_F_MODE_X86_32BIT_FLAT)
        fConsiderCsLimChecking = false; /* already enabled or not needed */
    else
    {
        int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
        if (offFromLim >= GUEST_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
            fConsiderCsLimChecking = true; /* likely */
        else
        {
            Log8(("%04x:%08RX64: Needs CS.LIM checks (%#RX64)\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, offFromLim));
            return false;
        }
    }

    /*
     * Prepare call now, even before we know if can accept the instruction in this TB.
     * This allows us amending parameters w/o making every case suffer.
     */
    uint8_t const             cbInstr   = IEM_GET_INSTR_LEN(pVCpu);
    uint16_t const            offOpcode = pTb->cbOpcodes;
    uint8_t                   idxRange  = pTb->cRanges - 1;

    PIEMTHRDEDCALLENTRY const pCall     = &pTb->Thrd.paCalls[pTb->Thrd.cCalls];
    pCall->idxInstr    = pTb->cInstructions;
    pCall->offOpcode   = offOpcode;
    pCall->idxRange    = idxRange;
    pCall->cbOpcode    = cbInstr;
    pCall->auParams[0] = cbInstr;
    pCall->auParams[1] = idxRange;
    pCall->auParams[2] = offOpcode - pTb->aRanges[idxRange].offOpcodes;

/** @todo check if we require IEMTB_F_CS_LIM_CHECKS for any new page we've
 *        gotten onto.  If we do, stop */

    /*
     * Case 1: We've branched (RIP changed).
     *
     * Sub-case 1a: Same page, no TLB load (fTbCrossedPage is false).
     *         Req: 1 extra range, no extra phys.
     *
     * Sub-case 1b: Different page but no page boundrary crossing, so TLB load
     *              necessary (fTbCrossedPage is true).
     *         Req: 1 extra range, probably 1 extra phys page entry.
     *
     * Sub-case 1c: Different page, so TLB load necessary (fTbCrossedPage is true),
     *              but in addition we cross into the following page and require
     *              another TLB load.
     *         Req: 2 extra ranges, probably 2 extra phys page entries.
     *
     * Sub-case 1d: Same page, so no initial TLB load necessary, but we cross into
     *              the following page (thus fTbCrossedPage is true).
     *         Req: 2 extra ranges, probably 1 extra phys page entry.
     *
     * Note! The setting fTbCrossedPage is done by the iemOpcodeFetchBytesJmp, but
     *       it may trigger "spuriously" from the CPU point of view because of
     *       physical page changes that'll invalid the physical TLB and trigger a
     *       call to the function.  In theory this be a big deal, just a bit
     *       performance loss as we'll pick the LoadingTlb variants.
     *
     * Note! We do not currently optimize branching to the next instruction (sorry
     *       32-bit PIC code).  We could maybe do that in the branching code that
     *       sets (or not) fTbBranched.
     */
    /** @todo Optimize 'jmp .next_instr' and 'call .next_instr'. Seen the jmp
     *        variant in win 3.1 code and the call variant in 32-bit linux PIC
     *        code.  This'll require filtering out far jmps and calls, as they
     *        load CS which should technically be considered indirect since the
     *        GDT/LDT entry's base address can be modified independently from
     *        the code. */
    if (pVCpu->iem.s.fTbBranched != IEMBRANCHED_F_NO)
    {
        if (   !pVCpu->iem.s.fTbCrossedPage       /* 1a */
            || pVCpu->iem.s.offCurInstrStart >= 0 /* 1b */ )
        {
            /* 1a + 1b - instruction fully within the branched to page. */
            Assert(pVCpu->iem.s.offCurInstrStart >= 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr <= GUEST_PAGE_SIZE);

            if (!(pVCpu->iem.s.fTbBranched & IEMBRANCHED_F_ZERO))
            {
                /* Check that we've got a free range. */
                idxRange += 1;
                if (idxRange < RT_ELEMENTS(pTb->aRanges))
                { /* likely */ }
                else
                {
                    Log8(("%04x:%08RX64: out of ranges after branch\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                    return false;
                }
                pCall->idxRange    = idxRange;
                pCall->auParams[1] = idxRange;
                pCall->auParams[2] = 0;

                /* Check that we've got a free page slot. */
                AssertCompile(RT_ELEMENTS(pTb->aGCPhysPages) == 2);
                RTGCPHYS const GCPhysNew = pVCpu->iem.s.GCPhysInstrBuf & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
                if ((pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK) == GCPhysNew)
                    pTb->aRanges[idxRange].idxPhysPage = 0;
                else if (   pTb->aGCPhysPages[0] == NIL_RTGCPHYS
                         || pTb->aGCPhysPages[0] == GCPhysNew)
                {
                    pTb->aGCPhysPages[0] = GCPhysNew;
                    pTb->aRanges[idxRange].idxPhysPage = 1;
                }
                else if (   pTb->aGCPhysPages[1] == NIL_RTGCPHYS
                         || pTb->aGCPhysPages[1] == GCPhysNew)
                {
                    pTb->aGCPhysPages[1] = GCPhysNew;
                    pTb->aRanges[idxRange].idxPhysPage = 2;
                }
                else
                {
                    Log8(("%04x:%08RX64: out of aGCPhysPages entires after branch\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                    return false;
                }

                /* Finish setting up the new range. */
                pTb->aRanges[idxRange].offPhysPage = pVCpu->iem.s.offCurInstrStart;
                pTb->aRanges[idxRange].offOpcodes  = offOpcode;
                pTb->aRanges[idxRange].cbOpcodes   = cbInstr;
                pTb->aRanges[idxRange].u2Unused    = 0;
                pTb->cRanges++;
                Log6(("%04x:%08RX64: new range #%u same page: offPhysPage=%#x offOpcodes=%#x\n",
                      pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, idxRange, pTb->aRanges[idxRange].offPhysPage,
                      pTb->aRanges[idxRange].offOpcodes));
            }
            else
            {
                Log8(("%04x:%08RX64: zero byte jump\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
                pTb->aRanges[idxRange].cbOpcodes += cbInstr;
            }

            /* Determin which function we need to load & check.
               Note! For jumps to a new page, we'll set both fTbBranched and
                     fTbCrossedPage to avoid unnecessary TLB work for intra
                     page branching */
            if (   (pVCpu->iem.s.fTbBranched & (IEMBRANCHED_F_INDIRECT | IEMBRANCHED_F_FAR)) /* Far is basically indirect. */
                || pVCpu->iem.s.fTbCrossedPage)
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesLoadingTlbConsiderCsLim;
            else if (pVCpu->iem.s.fTbBranched & (IEMBRANCHED_F_CONDITIONAL | /* paranoia: */ IEMBRANCHED_F_DIRECT))
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndPcAndOpcodes
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckPcAndOpcodes
                                   : kIemThreadedFunc_BltIn_CheckPcAndOpcodesConsiderCsLim;
            else
            {
                Assert(pVCpu->iem.s.fTbBranched & IEMBRANCHED_F_RELATIVE);
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodes
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodes
                                   : kIemThreadedFunc_BltIn_CheckOpcodesConsiderCsLim;
            }
        }
        else
        {
            /* 1c + 1d - instruction crosses pages. */
            Assert(pVCpu->iem.s.offCurInstrStart < 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr > 0);

            /* Lazy bird: Check that this isn't case 1c, since we've already
                          load the first physical address.  End the TB and
                          make it a case 2b instead.

                          Hmm. Too much bother to detect, so just do the same
                          with case 1d as well. */
#if 0       /** @todo get back to this later when we've got the actual branch code in
             *        place. */
            uint8_t const cbStartPage = (uint8_t)-pVCpu->iem.s.offCurInstrStart;

            /* Check that we've got two free ranges. */
            if (idxRange + 2 < RT_ELEMENTS(pTb->aRanges))
            { /* likely */ }
            else
                return false;
            idxRange += 1;
            pCall->idxRange    = idxRange;
            pCall->auParams[1] = idxRange;
            pCall->auParams[2] = 0;

            /* ... */

#else
            Log8(("%04x:%08RX64: complicated post-branch condition, ending TB.\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
#endif
        }
    }

    /*
     * Case 2: Page crossing.
     *
     * Sub-case 2a: The instruction starts on the first byte in the next page.
     *
     * Sub-case 2b: The instruction has opcode bytes in both the current and
     *              following page.
     *
     * Both cases requires a new range table entry and probably a new physical
     * page entry.  The difference is in which functions to emit and whether to
     * add bytes to the current range.
     */
    else if (pVCpu->iem.s.fTbCrossedPage)
    {
        /* Check that we've got a free range. */
        idxRange += 1;
        if (idxRange < RT_ELEMENTS(pTb->aRanges))
        { /* likely */ }
        else
        {
            Log8(("%04x:%08RX64: out of ranges while crossing page\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
        }

        /* Check that we've got a free page slot. */
        AssertCompile(RT_ELEMENTS(pTb->aGCPhysPages) == 2);
        RTGCPHYS const GCPhysNew = pVCpu->iem.s.GCPhysInstrBuf & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
        if ((pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK) == GCPhysNew)
            pTb->aRanges[idxRange].idxPhysPage = 0;
        else if (   pTb->aGCPhysPages[0] == NIL_RTGCPHYS
                 || pTb->aGCPhysPages[0] == GCPhysNew)
        {
            pTb->aGCPhysPages[0] = GCPhysNew;
            pTb->aRanges[idxRange].idxPhysPage = 1;
        }
        else if (   pTb->aGCPhysPages[1] == NIL_RTGCPHYS
                 || pTb->aGCPhysPages[1] == GCPhysNew)
        {
            pTb->aGCPhysPages[1] = GCPhysNew;
            pTb->aRanges[idxRange].idxPhysPage = 2;
        }
        else
        {
            Log8(("%04x:%08RX64: out of aGCPhysPages entires while crossing page\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
            return false;
        }

        if (((pTb->aRanges[idxRange - 1].offPhysPage + pTb->aRanges[idxRange - 1].cbOpcodes) & GUEST_PAGE_OFFSET_MASK) == 0)
        {
            Assert(pVCpu->iem.s.offCurInstrStart == 0);
            pCall->idxRange    = idxRange;
            pCall->auParams[1] = idxRange;
            pCall->auParams[2] = 0;

            /* Finish setting up the new range. */
            pTb->aRanges[idxRange].offPhysPage = pVCpu->iem.s.offCurInstrStart;
            pTb->aRanges[idxRange].offOpcodes  = offOpcode;
            pTb->aRanges[idxRange].cbOpcodes   = cbInstr;
            pTb->aRanges[idxRange].u2Unused    = 0;
            pTb->cRanges++;
            Log6(("%04x:%08RX64: new range #%u new page (a) %u/%RGp: offPhysPage=%#x offOpcodes=%#x\n",
                  pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, idxRange,  pTb->aRanges[idxRange].idxPhysPage, GCPhysNew,
                  pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].offOpcodes));

            /* Determin which function we need to load & check. */
            pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                               ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb
                               : !fConsiderCsLimChecking
                               ? kIemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlb
                               : kIemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlbConsiderCsLim;
        }
        else
        {
            Assert(pVCpu->iem.s.offCurInstrStart < 0);
            Assert(pVCpu->iem.s.offCurInstrStart + cbInstr > 0);
            uint8_t const cbStartPage = (uint8_t)-pVCpu->iem.s.offCurInstrStart;
            pCall->auParams[0] |= (uint64_t)cbStartPage << 32;

            /* We've good. Split the instruction over the old and new range table entries. */
            pTb->aRanges[idxRange - 1].cbOpcodes += cbStartPage;

            pTb->aRanges[idxRange].offPhysPage    = 0;
            pTb->aRanges[idxRange].offOpcodes     = offOpcode + cbStartPage;
            pTb->aRanges[idxRange].cbOpcodes      = cbInstr   - cbStartPage;
            pTb->aRanges[idxRange].u2Unused       = 0;
            pTb->cRanges++;
            Log6(("%04x:%08RX64: new range #%u new page (b) %u/%RGp: offPhysPage=%#x offOpcodes=%#x\n",
                  pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, idxRange,  pTb->aRanges[idxRange].idxPhysPage, GCPhysNew,
                  pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].offOpcodes));

            /* Determin which function we need to load & check. */
            if (pVCpu->iem.s.fTbCheckOpcodes)
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlbConsiderCsLim;
            else
                pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                                   ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb
                                   : !fConsiderCsLimChecking
                                   ? kIemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlb
                                   : kIemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlbConsiderCsLim;
        }
    }

    /*
     * Regular case: No new range required.
     */
    else
    {
        Assert(pVCpu->iem.s.fTbCheckOpcodes || (pTb->fFlags & IEMTB_F_CS_LIM_CHECKS));
        if (pVCpu->iem.s.fTbCheckOpcodes)
            pCall->enmFunction = pTb->fFlags & IEMTB_F_CS_LIM_CHECKS
                               ? kIemThreadedFunc_BltIn_CheckCsLimAndOpcodes
                               : kIemThreadedFunc_BltIn_CheckOpcodes;
        else
            pCall->enmFunction = kIemThreadedFunc_BltIn_CheckCsLim;

        iemThreadedCopyOpcodeBytesInline(pVCpu, &pTb->pabOpcodes[offOpcode], cbInstr);
        pTb->cbOpcodes                    = offOpcode + cbInstr;
        pTb->aRanges[idxRange].cbOpcodes += cbInstr;
        Assert(pTb->cbOpcodes <= pVCpu->iem.s.cbOpcodesAllocated);
    }

    /*
     * Commit the call.
     */
    pTb->Thrd.cCalls++;

    /*
     * Clear state.
     */
    pVCpu->iem.s.fTbBranched     = IEMBRANCHED_F_NO;
    pVCpu->iem.s.fTbCrossedPage  = false;
    pVCpu->iem.s.fTbCheckOpcodes = false;

    /*
     * Copy opcode bytes.
     */
    iemThreadedCopyOpcodeBytesInline(pVCpu, &pTb->pabOpcodes[offOpcode], cbInstr);
    pTb->cbOpcodes = offOpcode + cbInstr;
    Assert(pTb->cbOpcodes <= pVCpu->iem.s.cbOpcodesAllocated);

    return true;
}


/**
 * Worker for iemThreadedCompileBeginEmitCallsComplications and
 * iemThreadedCompileCheckIrq that checks for pending delivarable events.
 *
 * @returns true if anything is pending, false if not.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 */
DECL_FORCE_INLINE(bool) iemThreadedCompileIsIrqOrForceFlagPending(PVMCPUCC pVCpu)
{
    uint64_t fCpu = pVCpu->fLocalForcedActions;
    fCpu &= VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC | VMCPU_FF_INTERRUPT_NMI | VMCPU_FF_INTERRUPT_SMI;
#if 1
    /** @todo this isn't even close to the NMI/IRQ conditions in EM. */
    if (RT_LIKELY(   !fCpu
                  || (   !(fCpu & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
                      && (   !pVCpu->cpum.GstCtx.rflags.Bits.u1IF
                          || CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx))) ))
        return false;
    return true;
#else
    return false;
#endif

}


/**
 * Called by iemThreadedCompile when a block requires a mode check.
 *
 * @returns true if we should continue, false if we're out of call entries.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block being compiled.
 */
static bool iemThreadedCompileEmitCheckMode(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /* Emit the call. */
    uint32_t const idxCall = pTb->Thrd.cCalls;
    AssertReturn(idxCall < pTb->Thrd.cAllocated, false);
    PIEMTHRDEDCALLENTRY pCall = &pTb->Thrd.paCalls[idxCall];
    pTb->Thrd.cCalls = (uint16_t)(idxCall + 1);
    pCall->enmFunction = kIemThreadedFunc_BltIn_CheckMode;
    pCall->idxInstr    = pTb->cInstructions - 1;
    pCall->uUnused0    = 0;
    pCall->offOpcode   = 0;
    pCall->cbOpcode    = 0;
    pCall->idxRange    = 0;
    pCall->auParams[0] = pVCpu->iem.s.fExec;
    pCall->auParams[1] = 0;
    pCall->auParams[2] = 0;
    LogFunc(("%04x:%08RX64 fExec=%#x\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, pVCpu->iem.s.fExec));
    return true;
}


/**
 * Called by IEM_MC2_BEGIN_EMIT_CALLS() when IEM_CIMPL_F_CHECK_IRQ_BEFORE is
 * set.
 *
 * @returns true if we should continue, false if an IRQ is deliverable or a
 *          relevant force flag is pending.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The translation block being compiled.
 * @sa      iemThreadedCompileCheckIrq
 */
bool iemThreadedCompileEmitIrqCheckBefore(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * Skip this we've already emitted a call after the previous instruction
     * or if it's the first call, as we're always checking FFs between blocks.
     */
    uint32_t const idxCall = pTb->Thrd.cCalls;
    if (   idxCall > 0
        && pTb->Thrd.paCalls[idxCall - 1].enmFunction != kIemThreadedFunc_BltIn_CheckIrq)
    {
        /* Emit the call. */
        AssertReturn(idxCall < pTb->Thrd.cAllocated, false);
        PIEMTHRDEDCALLENTRY pCall = &pTb->Thrd.paCalls[idxCall];
        pTb->Thrd.cCalls = (uint16_t)(idxCall + 1);
        pCall->enmFunction = kIemThreadedFunc_BltIn_CheckIrq;
        pCall->idxInstr    = pTb->cInstructions;
        pCall->uUnused0    = 0;
        pCall->offOpcode   = 0;
        pCall->cbOpcode    = 0;
        pCall->idxRange    = 0;
        pCall->auParams[0] = 0;
        pCall->auParams[1] = 0;
        pCall->auParams[2] = 0;
        LogFunc(("%04x:%08RX64\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));

        /* Reset the IRQ check value. */
        pVCpu->iem.s.cInstrTillIrqCheck = !CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx) ? 32 : 0;

        /*
         * Check for deliverable IRQs and pending force flags.
         */
        return !iemThreadedCompileIsIrqOrForceFlagPending(pVCpu);
    }
    return true; /* continue */
}


/**
 * Emits an IRQ check call and checks for pending IRQs.
 *
 * @returns true if we should continue, false if an IRQ is deliverable or a
 *          relevant force flag is pending.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   pTb         The transation block.
 * @sa      iemThreadedCompileBeginEmitCallsComplications
 */
static bool iemThreadedCompileCheckIrqAfter(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /* Check again in a little bit, unless it is immediately following an STI
       in which case we *must* check immediately after the next instruction
       as well in case it's executed with interrupt inhibition.  We could
       otherwise miss the interrupt window. See the irq2 wait2 varaiant in
       bs3-timers-1 which is doing sti + sti + cli. */
    if (!pVCpu->iem.s.fTbCurInstrIsSti)
        pVCpu->iem.s.cInstrTillIrqCheck = 32;
    else
    {
        pVCpu->iem.s.fTbCurInstrIsSti   = false;
        pVCpu->iem.s.cInstrTillIrqCheck = 0;
    }
    LogFunc(("%04x:%08RX64\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));

    /*
     * Emit the call.
     */
    AssertReturn(pTb->Thrd.cCalls < pTb->Thrd.cAllocated, false);
    PIEMTHRDEDCALLENTRY pCall = &pTb->Thrd.paCalls[pTb->Thrd.cCalls++];
    pCall->enmFunction = kIemThreadedFunc_BltIn_CheckIrq;
    pCall->idxInstr    = pTb->cInstructions;
    pCall->uUnused0    = 0;
    pCall->offOpcode   = 0;
    pCall->cbOpcode    = 0;
    pCall->idxRange    = 0;
    pCall->auParams[0] = 0;
    pCall->auParams[1] = 0;
    pCall->auParams[2] = 0;

    /*
     * Check for deliverable IRQs and pending force flags.
     */
    return !iemThreadedCompileIsIrqOrForceFlagPending(pVCpu);
}


/**
 * Compiles a new TB and executes it.
 *
 * We combine compilation and execution here as it makes it simpler code flow
 * in the main loop and it allows interpreting while compiling if we want to
 * explore that option.
 *
 * @returns Strict VBox status code.
 * @param   pVM         The cross context virtual machine structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   GCPhysPc    The physical address corresponding to the current
 *                      RIP+CS.BASE.
 * @param   fExtraFlags Extra translation block flags: IEMTB_F_INHIBIT_SHADOW,
 *                      IEMTB_F_INHIBIT_NMI, IEMTB_F_CS_LIM_CHECKS.
 */
static VBOXSTRICTRC iemThreadedCompile(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhysPc, uint32_t fExtraFlags) IEM_NOEXCEPT_MAY_LONGJMP
{
    Assert(!(fExtraFlags & IEMTB_F_TYPE_MASK));
    fExtraFlags |= IEMTB_F_TYPE_THREADED;

    /*
     * Get the TB we use for the recompiling.  This is a maxed-out TB so
     * that'll we'll make a more efficient copy of when we're done compiling.
     */
    PIEMTB pTb = pVCpu->iem.s.pThrdCompileTbR3;
    if (pTb)
        iemThreadedTbReuse(pVCpu, pTb, GCPhysPc, fExtraFlags);
    else
    {
        pTb = iemThreadedTbAlloc(pVM, pVCpu, GCPhysPc, fExtraFlags);
        AssertReturn(pTb, VERR_IEM_TB_ALLOC_FAILED);
        pVCpu->iem.s.pThrdCompileTbR3 = pTb;
    }

    /* Set the current TB so iemThreadedCompileLongJumped and the CIMPL
       functions may get at it. */
    pVCpu->iem.s.pCurTbR3 = pTb;

#if 0
    /* Make sure the CheckIrq condition matches the one in EM. */
    iemThreadedCompileCheckIrqAfter(pVCpu, pTb);
    const uint32_t cZeroCalls = 1;
#else
    const uint32_t cZeroCalls = 0;
#endif

    /*
     * Now for the recomplication. (This mimicks IEMExecLots in many ways.)
     */
    iemThreadedCompileInitDecoder(pVCpu, false /*fReInit*/, fExtraFlags);
    iemThreadedCompileInitOpcodeFetching(pVCpu);
    VBOXSTRICTRC rcStrict;
    for (;;)
    {
        /* Process the next instruction. */
#ifdef LOG_ENABLED
        iemThreadedLogCurInstr(pVCpu, "CC", pTb->cInstructions);
        uint16_t const uCsLog  = pVCpu->cpum.GstCtx.cs.Sel;
        uint64_t const uRipLog = pVCpu->cpum.GstCtx.rip;
        Assert(uCsLog != 0 || uRipLog > 0x400 || !IEM_IS_REAL_OR_V86_MODE(pVCpu)); /* Detect executing RM interrupt table. */
#endif
        uint8_t b; IEM_OPCODE_GET_FIRST_U8(&b);
        uint16_t const cCallsPrev = pTb->Thrd.cCalls;

        rcStrict = FNIEMOP_CALL(g_apfnIemThreadedRecompilerOneByteMap[b]);
        if (   rcStrict == VINF_SUCCESS
            && pVCpu->iem.s.rcPassUp == VINF_SUCCESS
            && !pVCpu->iem.s.fEndTb)
        {
            Assert(pTb->Thrd.cCalls > cCallsPrev);
            Assert(cCallsPrev - pTb->Thrd.cCalls < 5);

            pVCpu->iem.s.cInstructions++;

            /* Check for mode change _after_ certain CIMPL calls, so check that
               we continue executing with the same mode value. */
            if (!(pVCpu->iem.s.fTbCurInstr & (IEM_CIMPL_F_MODE | IEM_CIMPL_F_XCPT | IEM_CIMPL_F_VMEXIT)))
            { /* probable */ }
            else if (RT_LIKELY(iemThreadedCompileEmitCheckMode(pVCpu, pTb)))
            { /* extremely likely */ }
            else
                break;
        }
        else
        {
            Log8(("%04x:%08RX64: End TB - %u instr, %u calls, rc=%d\n",
                  uCsLog, uRipLog, pTb->cInstructions, pTb->Thrd.cCalls, VBOXSTRICTRC_VAL(rcStrict)));
            if (rcStrict == VINF_IEM_RECOMPILE_END_TB)
                rcStrict = VINF_SUCCESS;

            if (pTb->Thrd.cCalls > cZeroCalls)
            {
                if (cCallsPrev != pTb->Thrd.cCalls)
                    pVCpu->iem.s.cInstructions++;
                break;
            }

            pVCpu->iem.s.pCurTbR3 = NULL;
            return iemExecStatusCodeFiddling(pVCpu, rcStrict);
        }

        /* Check for IRQs? */
        if (pVCpu->iem.s.cInstrTillIrqCheck > 0)
            pVCpu->iem.s.cInstrTillIrqCheck--;
        else if (!iemThreadedCompileCheckIrqAfter(pVCpu, pTb))
            break;

        /* Still space in the TB? */
        if (   pTb->Thrd.cCalls + 5 < pTb->Thrd.cAllocated
            && pTb->cbOpcodes + 16 <= pVCpu->iem.s.cbOpcodesAllocated)
            iemThreadedCompileInitDecoder(pVCpu, true /*fReInit*/, 0);
        else
        {
            Log8(("%04x:%08RX64: End TB - %u instr, %u calls, %u opcode bytes - full\n",
                  uCsLog, uRipLog, pTb->cInstructions, pTb->Thrd.cCalls, pTb->cbOpcodes));
            break;
        }
        iemThreadedCompileReInitOpcodeFetching(pVCpu);
    }

    /*
     * Duplicate the TB into a completed one and link it.
     */
    pTb = iemThreadedTbDuplicate(pVM, pVCpu, pTb);
    AssertReturn(pTb, VERR_IEM_TB_ALLOC_FAILED);

    iemThreadedTbAdd(pVCpu, pVCpu->iem.s.pTbCacheR3, pTb);

#ifdef IEM_COMPILE_ONLY_MODE
    /*
     * Execute the translation block.
     */
#endif

    return iemExecStatusCodeFiddling(pVCpu, rcStrict);
}



/*********************************************************************************************************************************
*   Recompiled Execution Core                                                                                                    *
*********************************************************************************************************************************/


/**
 * Executes a translation block.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The translation block to execute.
 */
static VBOXSTRICTRC iemTbExec(PVMCPUCC pVCpu, PIEMTB pTb) IEM_NOEXCEPT_MAY_LONGJMP
{
    /*
     * Check the opcodes in the first page before starting execution.
     */
/** @todo this test should take IEMTB_F_CS_LIM_CHECKS into account or something.
 * The 'near jmp+call' test in bs3-cpu-basic-2 triggers the 2nd assertion here by
 * altering the CS limit such that only one or the two instruction bytes are valid.
 * Since it's a CS_LIMT problem, the pbInstrBuf is good for the full length, and
 * the test succeeds if skipped, but we assert in debug builds. */
    Assert(!(pVCpu->iem.s.GCPhysInstrBuf & (RTGCPHYS)GUEST_PAGE_OFFSET_MASK));
    Assert(pTb->aRanges[0].cbOpcodes <= pVCpu->iem.s.cbInstrBufTotal - pVCpu->iem.s.offInstrNextByte);
    if (memcmp(pTb->pabOpcodes, &pVCpu->iem.s.pbInstrBuf[pTb->aRanges[0].offPhysPage], pTb->aRanges[0].cbOpcodes) == 0)
    { /* likely */ }
    else
    {
        Log7(("TB obsolete: %p GCPhys=%RGp\n", pTb, pTb->GCPhysPc));
        iemThreadedTbObsolete(pVCpu, pTb, true /*fSafeToFree*/);
        return VINF_SUCCESS;
    }

    /*
     * Set the current TB so CIMPL functions may get at it.
     */
    pVCpu->iem.s.pCurTbR3 = pTb;

    /*
     * Execute the block.
     */
#ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
    if (pTb->fFlags & IEMTB_F_TYPE_NATIVE)
    {
        pVCpu->iem.s.cTbExecNative++;
# ifdef LOG_ENABLED
        iemThreadedLogCurInstr(pVCpu, "EXn", 0);
# endif
# ifdef RT_ARCH_AMD64
        VBOXSTRICTRC const rcStrict = ((PFNIEMTBNATIVE)pTb->Native.paInstructions)(pVCpu);
# else
        VBOXSTRICTRC const rcStrict = ((PFNIEMTBNATIVE)pTb->Native.paInstructions)(pVCpu, &pVCpu->cpum.GstCtx);
# endif
        if (RT_LIKELY(   rcStrict == VINF_SUCCESS
                      && pVCpu->iem.s.rcPassUp == VINF_SUCCESS /** @todo this isn't great. */))
        { /* likely */ }
        else
        {
            /* pVCpu->iem.s.cInstructions is incremented by iemNativeHlpExecStatusCodeFiddling. */
            pVCpu->iem.s.pCurTbR3 = NULL;
            STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatTbExecBreaks);

            /* VINF_IEM_REEXEC_BREAK should be treated as VINF_SUCCESS as it's
               only to break out of TB execution early. */
            if (rcStrict == VINF_IEM_REEXEC_BREAK)
                return iemExecStatusCodeFiddling(pVCpu, VINF_SUCCESS);

            /* VINF_IEM_REEXEC_WITH_FLAGS needs to receive special treatment
               and converted to VINF_SUCCESS or whatever is appropriate. */
            if (rcStrict == VINF_IEM_REEXEC_FINISH_WITH_FLAGS)
                return iemExecStatusCodeFiddling(pVCpu, iemFinishInstructionWithFlagsSet(pVCpu));

            return iemExecStatusCodeFiddling(pVCpu, rcStrict);
        }
    }
    else
#endif /* VBOX_WITH_IEM_NATIVE_RECOMPILER */
    {
        /*
         * The threaded execution loop.
         */
        pVCpu->iem.s.cTbExecThreaded++;
#ifdef LOG_ENABLED
        uint64_t             uRipPrev   = UINT64_MAX;
#endif
        PCIEMTHRDEDCALLENTRY pCallEntry = pTb->Thrd.paCalls;
        uint32_t             cCallsLeft = pTb->Thrd.cCalls;
        while (cCallsLeft-- > 0)
        {
#ifdef LOG_ENABLED
            if (pVCpu->cpum.GstCtx.rip != uRipPrev)
            {
                uRipPrev = pVCpu->cpum.GstCtx.rip;
                iemThreadedLogCurInstr(pVCpu, "EXt", pTb->Thrd.cCalls - cCallsLeft - 1);
            }
            Log9(("%04x:%08RX64: #%d/%d - %d %s\n", pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
                  pTb->Thrd.cCalls - cCallsLeft - 1, pCallEntry->idxInstr, pCallEntry->enmFunction,
                  g_apszIemThreadedFunctions[pCallEntry->enmFunction]));
#endif
            VBOXSTRICTRC const rcStrict = g_apfnIemThreadedFunctions[pCallEntry->enmFunction](pVCpu,
                                                                                              pCallEntry->auParams[0],
                                                                                              pCallEntry->auParams[1],
                                                                                              pCallEntry->auParams[2]);
            if (RT_LIKELY(   rcStrict == VINF_SUCCESS
                          && pVCpu->iem.s.rcPassUp == VINF_SUCCESS /** @todo this isn't great. */))
                pCallEntry++;
            else
            {
                pVCpu->iem.s.cInstructions += pCallEntry->idxInstr; /* This may be one short, but better than zero. */
                pVCpu->iem.s.pCurTbR3       = NULL;
                STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatTbExecBreaks);

                /* VINF_IEM_REEXEC_BREAK should be treated as VINF_SUCCESS as it's
                   only to break out of TB execution early. */
                if (rcStrict == VINF_IEM_REEXEC_BREAK)
                    return iemExecStatusCodeFiddling(pVCpu, VINF_SUCCESS);
                return iemExecStatusCodeFiddling(pVCpu, rcStrict);
            }
        }
    }

    pVCpu->iem.s.cInstructions += pTb->cInstructions;
    pVCpu->iem.s.pCurTbR3 = NULL;
    return VINF_SUCCESS;
}


/**
 * This is called when the PC doesn't match the current pbInstrBuf.
 *
 * Upon return, we're ready for opcode fetching.  But please note that
 * pbInstrBuf can be NULL iff the memory doesn't have readable backing (i.e.
 * MMIO or unassigned).
 */
static RTGCPHYS iemGetPcWithPhysAndCodeMissed(PVMCPUCC pVCpu)
{
    pVCpu->iem.s.pbInstrBuf       = NULL;
    pVCpu->iem.s.offCurInstrStart = 0;
    pVCpu->iem.s.offInstrNextByte = 0;
    iemOpcodeFetchBytesJmp(pVCpu, 0, NULL);
    return pVCpu->iem.s.GCPhysInstrBuf + pVCpu->iem.s.offCurInstrStart;
}


/** @todo need private inline decl for throw/nothrow matching IEM_WITH_SETJMP? */
DECL_FORCE_INLINE_THROW(RTGCPHYS) iemGetPcWithPhysAndCode(PVMCPUCC pVCpu)
{
    /*
     * Set uCurTbStartPc to RIP and calc the effective PC.
     */
    uint64_t uPc = pVCpu->cpum.GstCtx.rip;
    pVCpu->iem.s.uCurTbStartPc = uPc;
    Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu));
    uPc += pVCpu->cpum.GstCtx.cs.u64Base;

    /*
     * Advance within the current buffer (PAGE) when possible.
     */
    if (pVCpu->iem.s.pbInstrBuf)
    {
        uint64_t off = uPc - pVCpu->iem.s.uInstrBufPc;
        if (off < pVCpu->iem.s.cbInstrBufTotal)
        {
            pVCpu->iem.s.offInstrNextByte = (uint32_t)off;
            pVCpu->iem.s.offCurInstrStart = (uint16_t)off;
            if ((uint16_t)off + 15 <= pVCpu->iem.s.cbInstrBufTotal)
                pVCpu->iem.s.cbInstrBuf = (uint16_t)off + 15;
            else
                pVCpu->iem.s.cbInstrBuf = pVCpu->iem.s.cbInstrBufTotal;

            return pVCpu->iem.s.GCPhysInstrBuf + off;
        }
    }
    return iemGetPcWithPhysAndCodeMissed(pVCpu);
}


/**
 * Determines the extra IEMTB_F_XXX flags.
 *
 * @returns A mix of IEMTB_F_INHIBIT_SHADOW, IEMTB_F_INHIBIT_NMI and
 *          IEMTB_F_CS_LIM_CHECKS (or zero).
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 */
DECL_FORCE_INLINE(uint32_t) iemGetTbFlagsForCurrentPc(PVMCPUCC pVCpu)
{
    uint32_t fRet = 0;

    /*
     * Determine the inhibit bits.
     */
    if (!(pVCpu->cpum.GstCtx.rflags.uBoth & (IEMTB_F_INHIBIT_SHADOW | IEMTB_F_INHIBIT_NMI)))
    { /* typical */ }
    else
    {
        if (CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx))
            fRet |= IEMTB_F_INHIBIT_SHADOW;
        if (CPUMAreInterruptsInhibitedByNmiEx(&pVCpu->cpum.GstCtx))
            fRet |= IEMTB_F_INHIBIT_NMI;
    }

    /*
     * Return IEMTB_F_CS_LIM_CHECKS if the current PC is invalid or if it is
     * likely to go invalid before the end of the translation block.
     */
    if (IEM_IS_64BIT_CODE(pVCpu))
        return fRet;

    int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
    if (offFromLim >= X86_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
        return fRet;
    return fRet | IEMTB_F_CS_LIM_CHECKS;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecRecompiler(PVMCC pVM, PVMCPUCC pVCpu)
{
    /*
     * See if there is an interrupt pending in TRPM, inject it if we can.
     */
    if (!TRPMHasTrap(pVCpu))
    { /* likely */ }
    else
    {
        VBOXSTRICTRC rcStrict = iemExecInjectPendingTrap(pVCpu);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /*likely */ }
        else
            return rcStrict;
    }

    /*
     * Init the execution environment.
     */
#ifdef RT_ARCH_ARM64 /** @todo ARM64: fix unaligned locked instructions properly. @bugreg{10547} */
    if (pVM->cCpus == 1)
        iemInitExec(pVCpu, IEM_F_X86_DISREGARD_LOCK /*fExecOpts*/);
    else
#endif
        iemInitExec(pVCpu, 0 /*fExecOpts*/);
    if (RT_LIKELY(pVCpu->iem.s.msRecompilerPollNow != 0))
    { }
    else
        pVCpu->iem.s.msRecompilerPollNow = (uint32_t)(TMVirtualGetNoCheck(pVM) / RT_NS_1MS);

    /*
     * Run-loop.
     *
     * If we're using setjmp/longjmp we combine all the catching here to avoid
     * having to call setjmp for each block we're executing.
     */
    PIEMTBCACHE const pTbCache = pVCpu->iem.s.pTbCacheR3;
    for (;;)
    {
        PIEMTB       pTb = NULL;
        VBOXSTRICTRC rcStrict;
        IEM_TRY_SETJMP(pVCpu, rcStrict)
        {
            uint32_t const cPollRate = 511; /* EM.cpp passes 4095 to IEMExecLots, so an eigth of that seems reasonable for now. */
            for (uint32_t iIterations = 0; ; iIterations++)
            {
                /* Translate PC to physical address, we'll need this for both lookup and compilation. */
                RTGCPHYS const GCPhysPc    = iemGetPcWithPhysAndCode(pVCpu);
                uint32_t const fExtraFlags = iemGetTbFlagsForCurrentPc(pVCpu);

                pTb = iemTbCacheLookup(pVCpu, pTbCache, GCPhysPc, fExtraFlags);
                if (pTb)
                    rcStrict = iemTbExec(pVCpu, pTb);
                else
                    rcStrict = iemThreadedCompile(pVM, pVCpu, GCPhysPc, fExtraFlags);
                if (rcStrict == VINF_SUCCESS)
                {
                    Assert(pVCpu->iem.s.cActiveMappings == 0);

                    uint64_t fCpu = pVCpu->fLocalForcedActions;
                    fCpu &= VMCPU_FF_ALL_MASK & ~(  VMCPU_FF_PGM_SYNC_CR3
                                                  | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
                                                  | VMCPU_FF_TLB_FLUSH
                                                  | VMCPU_FF_UNHALT );
                    /** @todo this isn't even close to the NMI/IRQ conditions in EM. */
                    if (RT_LIKELY(   (   !fCpu
                                      || (   !(fCpu & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
                                          && (   !pVCpu->cpum.GstCtx.rflags.Bits.u1IF
                                              || CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx) )) )
                                  && !VM_FF_IS_ANY_SET(pVM, VM_FF_ALL_MASK) ))
                    {
                        if (RT_LIKELY(   (iIterations & cPollRate) != 0
                                      || !TMTimerPollBoolWith32BitMilliTS(pVM, pVCpu, &pVCpu->iem.s.msRecompilerPollNow)))
                            pTb = NULL; /* Clear it before looping so iemTbCacheLookup can safely do native recompilation. */
                        else
                            return VINF_SUCCESS;
                    }
                    else
                        return VINF_SUCCESS;
                }
                else
                    return rcStrict;
            }
        }
        IEM_CATCH_LONGJMP_BEGIN(pVCpu, rcStrict);
        {
            pVCpu->iem.s.cLongJumps++;
            if (pVCpu->iem.s.cActiveMappings > 0)
                iemMemRollback(pVCpu);

#if 0 /** @todo do we need to clean up anything?  If not, we can drop the pTb = NULL some lines up and change the scope. */
            /* If pTb isn't NULL we're in iemTbExec. */
            if (!pTb)
            {
                /* If pCurTbR3 is NULL, we're in iemGetPcWithPhysAndCode.*/
                pTb = pVCpu->iem.s.pCurTbR3;
                if (pTb)
                {
                    if (pTb == pVCpu->iem.s.pThrdCompileTbR3)
                        return iemThreadedCompileLongJumped(pVM, pVCpu, rcStrict);
                    Assert(pTb != pVCpu->iem.s.pNativeCompileTbR3);
                }
            }
#endif
            return rcStrict;
        }
        IEM_CATCH_LONGJMP_END(pVCpu);
    }
}

