/* $Id$ */
/** @file
 * IEM - Native Recompiler, Liveness Analysis.
 */

/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_IEM
#define IEM_WITH_OPAQUE_DECODER_STATE
#include <VBox/vmm/iem.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>

#include "IEMN8veRecompiler.h"
#include "IEMThreadedFunctions.h"
#include "IEMNativeFunctions.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define NOP() ((void)0)


/*
 * BEGIN & END as well as internal workers.
 */
#define IEM_MC_BEGIN(a_cArgs, a_cLocals, a_fMcFlags, a_fCImplFlags) \
    { \
        /* Define local variables that we use to accumulate the liveness state changes in. */ \
        IEMLIVENESSPART1 LiveStatePart1  = { 0 }; \
        IEMLIVENESSPART2 LiveStatePart2  = { 0 }; \
        IEMLIVENESSPART1 LiveMaskPart1   = { 0 }; \
        IEMLIVENESSPART2 LiveMaskPart2   = { 0 }; \
        bool             fDoneXpctOrCall = false


#define IEM_LIVENESS_MARK_XCPT_OR_CALL() do { \
            if (!fDoneXpctOrCall) \
            { \
                uint64_t uTmp0 = pIncoming->s1.bm64 & ~LiveMaskPart1.bm64; \
                uTmp0 = uTmp0 & (uTmp0 >> 1); \
                LiveStatePart1.bm64 |= uTmp0 | IEMLIVENESSPART1_XCPT_OR_CALL; \
                \
                uint64_t uTmp1 = pIncoming->s2.bm64 & ~LiveMaskPart2.bm64; \
                uTmp1 = uTmp1 & (uTmp1 >> 1); \
                LiveStatePart2.bm64 |= uTmp1 | IEMLIVENESSPART2_XCPT_OR_CALL; \
                \
                LiveMaskPart1.bm64  |= IEMLIVENESSPART1_MASK; /* could also use UINT64_MAX here, but makes no difference */ \
                LiveMaskPart2.bm64  |= IEMLIVENESSPART2_MASK; /* when compiling with gcc and cl.exe on x86 - may on arm, though. */ \
                fDoneXpctOrCall      = true; \
            } \
        } while (0)


AssertCompile(IEMLIVENESSPART1_ALL_EFL_MASK == 0);
#define IEM_LIVENESS_ALL_EFLAGS_CLOBBERED() do { \
            LiveMaskPart2.bm64  |= IEMLIVENESSPART2_ALL_EFL_MASK; \
        } while (0)
#define IEM_LIVENESS_ALL_EFLAGS_INPUT() do { \
            LiveMaskPart2.bm64  |= IEMLIVENESSPART2_ALL_EFL_MASK; \
            LiveStatePart2.bm64 |= IEMLIVENESSPART2_ALL_EFL_INPUT; \
        } while (0)


AssertCompile(IEMLIVENESS_STATE_CLOBBERED == 0);
#define IEM_LIVENESS_ONE_EFLAG_CLOBBERED(a_Name) do { \
            LiveMaskPart2.a_Name  |= IEMLIVENESS_STATE_MASK; \
        } while (0)
#define IEM_LIVENESS_ONE_EFLAG_INPUT(a_Name) do { \
            LiveMaskPart2.a_Name  |= IEMLIVENESS_STATE_MASK; \
            LiveStatePart2.a_Name |= IEMLIVENESS_STATE_INPUT; \
        } while (0)


#define IEM_LIVENESS_GPR_CLOBBERED(a_idxGpr) do { \
            LiveMaskPart1.bmGprs  |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_idxGpr) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)
#define IEM_LIVENESS_GPR_INPUT(a_idxGpr) do { \
            LiveMaskPart1.bmGprs  |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_idxGpr) * IEMLIVENESS_STATE_BIT_COUNT); \
            LiveStatePart1.bmGprs |= (uint32_t)IEMLIVENESS_STATE_INPUT << ((a_idxGpr) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)


#define IEM_LIVENESS_SEG_BASE_CLOBBERED(a_iSeg) do { \
            LiveMaskPart1.bmSegBase    |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)
#define IEM_LIVENESS_SEG_BASE_INPUT(a_iSeg) do { \
            LiveMaskPart1.bmSegBase    |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
            LiveStatePart1.bmSegBase   |= (uint32_t)IEMLIVENESS_STATE_INPUT << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)


#define IEM_LIVENESS_SEG_ATTRIB_CLOBBERED(a_iSeg) do { \
            LiveMaskPart1.bmSegAttrib  |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)
#define IEM_LIVENESS_SEG_ATTRIB_INPUT(a_iSeg) do { \
            LiveMaskPart1.bmSegAttrib  |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
            LiveStatePart1.bmSegAttrib |= (uint32_t)IEMLIVENESS_STATE_INPUT << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)


#define IEM_LIVENESS_SEG_LIMIT_CLOBBERED(a_iSeg) do { \
            LiveMaskPart2.bmSegLimit   |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)
#define IEM_LIVENESS_SEG_LIMIT_INPUT(a_iSeg) do { \
            LiveMaskPart2.bmSegLimit   |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
            LiveStatePart2.bmSegLimit  |= (uint32_t)IEMLIVENESS_STATE_INPUT << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)


#define IEM_LIVENESS_SEG_SEL_CLOBBERED(a_iSeg) do { \
            LiveMaskPart2.bmSegSel     |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)
#define IEM_LIVENESS_SEG_SEL_INPUT(a_iSeg) do { \
            LiveMaskPart2.bmSegSel     |= (uint32_t)IEMLIVENESS_STATE_MASK  << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
            LiveStatePart2.bmSegSel    |= (uint32_t)IEMLIVENESS_STATE_INPUT << ((a_iSeg) * IEMLIVENESS_STATE_BIT_COUNT); \
        } while (0)


#define IEM_LIVENESS_MEM(a_iSeg) do { \
        IEM_LIVENESS_MARK_XCPT_OR_CALL(); \
        IEM_LIVENESS_SEG_ATTRIB_INPUT(a_iSeg); \
        IEM_LIVENESS_SEG_BASE_INPUT(a_iSeg); \
        IEM_LIVENESS_SEG_LIMIT_INPUT(a_iSeg); \
    } while (0)

#define IEM_LIVENESS_MEM_FLAT() IEM_LIVENESS_MARK_XCPT_OR_CALL()

#define IEM_LIVENESS_STACK() do { \
        IEM_LIVENESS_MEM(X86_GREG_xSP); \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xSP); \
    } while (0)

#define IEM_LIVENESS_STACK_FLAT() do { \
        IEM_LIVENESS_MEM_FLAT(); \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xSP); \
    } while (0)


#define IEM_LIVENESS_PC_NO_FLAGS() NOP()

#define IEM_LIVENESS_PC_WITH_FLAGS() IEM_LIVENESS_MARK_XCPT_OR_CALL()


#define IEM_MC_END() \
        /* Combine the incoming state with what we've accumulated in this block. */ \
        /* We can help the compiler by skipping OR'ing when having applied XPCT_OR_CALL, */ \
        /* since that already imports all the incoming state. Saves a lot with cl.exe. */ \
        if (!fDoneXpctOrCall) \
        { \
            pOutgoing->s1.bm64 = LiveStatePart1.bm64 | (~LiveMaskPart1.bm64 & pIncoming->s1.bm64); \
            pOutgoing->s2.bm64 = LiveStatePart2.bm64 | (~LiveMaskPart2.bm64 & pIncoming->s2.bm64); \
        } \
        else \
        { \
            pOutgoing->s1.bm64 = LiveStatePart1.bm64; \
            pOutgoing->s2.bm64 = LiveStatePart2.bm64; \
        } \
    }

/*
 * The native MC variants.
 */
#define IEM_MC_FREE_LOCAL(a_Name)   NOP()
#define IEM_MC_FREE_ARG(a_Name)     NOP()


/*
 * The THREADED MC variants.
 */

/* We don't track RIP (PC) liveness. */
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16(a_cbInstr, a_rcNormal)                                  IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32(a_cbInstr, a_rcNormal)                                  IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64(a_cbInstr, a_rcNormal)                                  IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_cbInstr, a_rcNormal)                       IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_cbInstr, a_rcNormal)                       IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_cbInstr, a_rcNormal)                       IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16(a_i8, a_cbInstr, a_rcNormal)                             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal)             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal)             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i8, a_cbInstr, a_rcNormal)                  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal)  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal)  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16(a_i16, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32(a_i16, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64(a_i16, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC16(a_i32, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32(a_i32, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64(a_i32, a_cbInstr, a_rcNormal)                           IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal)                IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC16(a_u16NewIP)                                             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC32(a_u16NewIP)                                             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC64(a_u16NewIP)                                             IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_u16NewIP)                                  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_u16NewIP)                                  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u16NewIP)                                  IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC32(a_u32NewEIP)                                            IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC64(a_u32NewEIP)                                            IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_u32NewEIP)                                 IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u32NewEIP)                                 IEM_LIVENESS_PC_WITH_FLAGS()
#define IEM_MC_SET_RIP_U64_AND_FINISH_THREADED_PC64(a_u32NewEIP)                                            IEM_LIVENESS_PC_NO_FLAGS()
#define IEM_MC_SET_RIP_U64_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u32NewEIP)                                 IEM_LIVENESS_PC_WITH_FLAGS()

/* Effective address stuff is rather complicated... */
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_16(a_GCPtrEff, a_bRm, a_u16Disp) do { \
    } while (0)
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_32(a_GCPtrEff, a_bRm, a_uSibAndRspOffset, a_u32Disp) do { \
    } while (0)
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) do { \
    } while (0)
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_FSGS(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) \
    IEM_MC_CALC_RM_EFF_ADDR_THREADED_64(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm)
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_ADDR32(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) \
    IEM_MC_CALC_RM_EFF_ADDR_THREADED_64(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm)

/* At present we don't know what any CIMPL may require as input, so we do XPCT/CALL. */
#define IEM_MC_CALL_CIMPL_1_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0) \
    IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_CALL_CIMPL_2_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1) \
    IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_CALL_CIMPL_3_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2) \
    IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_CALL_CIMPL_4_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3) \
    IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_CALL_CIMPL_5_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3, a4) \
    IEM_LIVENESS_MARK_XCPT_OR_CALL()

#define IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)
#define IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)
#define IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)
#define IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)
#define IEM_MC_DEFER_TO_CIMPL_4_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)
#define IEM_MC_DEFER_TO_CIMPL_5_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3, a4) \
    IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(pOutgoing, pIncoming)

/* Any 8-bit register fetch, store or modification only works on part of the register
   and must therefore be considered INPUTs. */
#define IEM_MC_FETCH_GREG_U8_THREADED(a_u8Dst, a_iGRegEx)           IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_ZX_U16_THREADED(a_u16Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_ZX_U32_THREADED(a_u32Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_ZX_U64_THREADED(a_u64Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_SX_U16_THREADED(a_u16Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_SX_U32_THREADED(a_u32Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_FETCH_GREG_U8_SX_U64_THREADED(a_u64Dst, a_iGRegEx)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_STORE_GREG_U8_THREADED(a_iGRegEx, a_u8Value)         IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_STORE_GREG_U8_CONST_THREADED(a_iGRegEx, a_u8Value)   IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_REF_GREG_U8_THREADED(a_pu8Dst, a_iGRegEx)            IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_ADD_GREG_U8_TO_LOCAL_THREADED(a_u8Value, a_iGRegEx)  IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_AND_GREG_U8_THREADED(a_iGRegEx, a_u8Value)           IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)
#define IEM_MC_OR_GREG_U8_THREADED(a_iGRegEx, a_u8Value)            IEM_LIVENESS_GPR_INPUT(a_iGRegEx & 15)


/*
 * The other MCs.
 */

#define IEM_MC_NO_NATIVE_RECOMPILE()                                NOP()

#define IEM_MC_RAISE_DIVIDE_ERROR()                                 NOP()
#define IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE()                   IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_WAIT_DEVICE_NOT_AVAILABLE()              IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_FPU_XCPT()                               IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT()                       IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT()                       IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT()                       IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO()                          IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_RAISE_GP0_IF_EFF_ADDR_UNALIGNED(a_EffAddr, a_cbAlign) IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_FSGSBASE_XCPT()                          IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_NON_CANONICAL_ADDR_GP0(a_u64Addr)        IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_MAYBE_RAISE_SSE_AVX_SIMD_FP_OR_UD_XCPT()             IEM_LIVENESS_MARK_XCPT_OR_CALL()
#define IEM_MC_RAISE_SSE_AVX_SIMD_FP_OR_UD_XCPT()                   IEM_LIVENESS_MARK_XCPT_OR_CALL()

#define IEM_MC_LOCAL(a_Type, a_Name)                                NOP()
#define IEM_MC_LOCAL_ASSIGN(a_Type, a_Name, a_Value)                NOP()
#define IEM_MC_LOCAL_CONST(a_Type, a_Name, a_Value)                 NOP()
#define IEM_MC_NOREF(a_Name)                                        NOP()
#define IEM_MC_ARG(a_Type, a_Name, a_iArg)                          NOP()
#define IEM_MC_ARG_CONST(a_Type, a_Name, a_Value, a_iArg)           NOP()
#define IEM_MC_ARG_LOCAL_REF(a_Type, a_Name, a_Local, a_iArg)       NOP()

#define IEM_MC_COMMIT_EFLAGS(a_EFlags)                              IEM_LIVENESS_ALL_EFLAGS_CLOBBERED()

#define IEM_MC_ASSIGN_TO_SMALLER(a_VarDst, a_VarSrcEol)             NOP()

#define IEM_MC_FETCH_GREG_U16(a_u16Dst, a_iGReg)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U16_ZX_U32(a_u32Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U16_ZX_U64(a_u64Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U16_SX_U32(a_u32Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U16_SX_U64(a_u64Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U32(a_u32Dst, a_iGReg)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U32_ZX_U64(a_u64Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U32_SX_U64(a_u64Dst, a_iGReg)             IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U64(a_u64Dst, a_iGReg)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_FETCH_GREG_U64_ZX_U64                                IEM_MC_FETCH_GREG_U64
#define IEM_MC_FETCH_GREG_PAIR_U32(a_u64Dst, a_iGRegLo, a_iGRegHi) \
    do { IEM_LIVENESS_GPR_INPUT(a_iGRegLo); IEM_LIVENESS_GPR_INPUT(a_iGRegHi); } while(0)
#define IEM_MC_FETCH_GREG_PAIR_U64(a_u128Dst, a_iGRegLo, a_iGRegHi) \
    do { IEM_LIVENESS_GPR_INPUT(a_iGRegLo); IEM_LIVENESS_GPR_INPUT(a_iGRegHi); } while(0)
#define IEM_MC_FETCH_SREG_U16(a_u16Dst, a_iSReg)                    IEM_LIVENESS_SEG_SEL_INPUT(a_iSReg)
#define IEM_MC_FETCH_SREG_ZX_U32(a_u32Dst, a_iSReg)                 IEM_LIVENESS_SEG_SEL_INPUT(a_iSReg)
#define IEM_MC_FETCH_SREG_ZX_U64(a_u64Dst, a_iSReg)                 IEM_LIVENESS_SEG_SEL_INPUT(a_iSReg)
#define IEM_MC_FETCH_SREG_BASE_U64(a_u64Dst, a_iSReg)               IEM_LIVENESS_SEG_BASE_INPUT(a_iSReg)
#define IEM_MC_FETCH_SREG_BASE_U32(a_u32Dst, a_iSReg)               IEM_LIVENESS_SEG_BASE_INPUT(a_iSReg)
#define IEM_MC_FETCH_EFLAGS(a_EFlags)                               IEM_LIVENESS_ALL_EFLAGS_INPUT()
#define IEM_MC_FETCH_EFLAGS_U8(a_EFlags) do { \
        IEM_LIVENESS_ONE_EFLAGS_INPUT(u2Cf); \
        IEM_LIVENESS_ONE_EFLAGS_INPUT(u2Pf); \
        IEM_LIVENESS_ONE_EFLAGS_INPUT(u2Af); \
        IEM_LIVENESS_ONE_EFLAGS_INPUT(u2Zf); \
        IEM_LIVENESS_ONE_EFLAGS_INPUT(u2Sf); \
    } while (0)

#define IEM_MC_FETCH_FSW(a_u16Fsw)                                  NOP()
#define IEM_MC_FETCH_FCW(a_u16Fcw)                                  NOP()

#define IEM_MC_STORE_GREG_U16(a_iGReg, a_u16Value)                  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_U32(a_iGReg, a_u32Value)                  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_U64(a_iGReg, a_u64Value)                  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_I64(a_iGReg, a_i64Value)                  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_U16_CONST(a_iGReg, a_u16Const)            IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_U32_CONST(a_iGReg, a_u32Const)            IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_U64_CONST(a_iGReg, a_u32Const)            IEM_LIVENESS_GPR_CLOBBERED(a_iGReg)
#define IEM_MC_STORE_GREG_PAIR_U32(a_iGRegLo, a_iGRegHi, a_u64Value) \
    do { IEM_LIVENESS_GPR_CLOBBERED(a_iGRegLo); IEM_LIVENESS_GPR_CLOBBERED(a_iGRegHi); } while(0)
#define IEM_MC_STORE_GREG_PAIR_U64(a_iGRegLo, a_iGRegHi, a_u128Value) \
    do { IEM_LIVENESS_GPR_CLOBBERED(a_iGRegLo); IEM_LIVENESS_GPR_CLOBBERED(a_iGRegHi); } while(0)
#define IEM_MC_CLEAR_HIGH_GREG_U64(a_iGReg)                         NOP() /* ASSUMES it's never used w/o associated modifications that we flag instead */

#define IEM_MC_STORE_SREG_BASE_U64(a_iSReg, a_u64Value)             IEM_LIVENESS_SEG_BASE_CLOBBERED(a_iSReg)
#define IEM_MC_STORE_SREG_BASE_U32(a_iSReg, a_u32Value)             IEM_LIVENESS_SEG_BASE_CLOBBERED(a_iSReg)
#define IEM_MC_STORE_FPUREG_R80_SRC_REF(a_iSt, a_pr80Src)           NOP()


#define IEM_MC_REF_GREG_U16(a_pu16Dst, a_iGReg)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_U16_CONST(a_pu16Dst, a_iGReg)               IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_U32(a_pu32Dst, a_iGReg)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_U32_CONST(a_pu32Dst, a_iGReg)               IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_I32(a_pi32Dst, a_iGReg)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_I32_CONST(a_pi32Dst, a_iGReg)               IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_U64(a_pu64Dst, a_iGReg)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_U64_CONST(a_pu64Dst, a_iGReg)               IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_I64(a_pi64Dst, a_iGReg)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_GREG_I64_CONST(a_pi64Dst, a_iGReg)               IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_REF_EFLAGS(a_pEFlags)                                IEM_LIVENESS_ALL_EFLAGS_INPUT()
#define IEM_MC_REF_MXCSR(a_pfMxcsr)                                 NOP()


#define IEM_MC_ADD_GREG_U16(a_iGReg, a_u16Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_ADD_GREG_U32(a_iGReg, a_u32Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_ADD_GREG_U64(a_iGReg, a_u64Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)

#define IEM_MC_SUB_GREG_U16(a_iGReg, a_u8Const)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_SUB_GREG_U32(a_iGReg, a_u8Const)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_SUB_GREG_U64(a_iGReg, a_u8Const)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_SUB_LOCAL_U16(a_u16Value, a_u16Const)                NOP()

#define IEM_MC_ADD_GREG_U16_TO_LOCAL(a_u16Value, a_iGReg)           IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_ADD_GREG_U32_TO_LOCAL(a_u32Value, a_iGReg)           IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_ADD_GREG_U64_TO_LOCAL(a_u64Value, a_iGReg)           IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_ADD_LOCAL_S16_TO_EFF_ADDR(a_EffAddr, a_i16)          NOP()
#define IEM_MC_ADD_LOCAL_S32_TO_EFF_ADDR(a_EffAddr, a_i32)          NOP()
#define IEM_MC_ADD_LOCAL_S64_TO_EFF_ADDR(a_EffAddr, a_i64)          NOP()

#define IEM_MC_AND_LOCAL_U8(a_u8Local, a_u8Mask)                    NOP()
#define IEM_MC_AND_LOCAL_U16(a_u16Local, a_u16Mask)                 NOP()
#define IEM_MC_AND_LOCAL_U32(a_u32Local, a_u32Mask)                 NOP()
#define IEM_MC_AND_LOCAL_U64(a_u64Local, a_u64Mask)                 NOP()

#define IEM_MC_AND_ARG_U16(a_u16Arg, a_u16Mask)                     NOP()
#define IEM_MC_AND_ARG_U32(a_u32Arg, a_u32Mask)                     NOP()
#define IEM_MC_AND_ARG_U64(a_u64Arg, a_u64Mask)                     NOP()

#define IEM_MC_OR_LOCAL_U8(a_u8Local, a_u8Mask)                     NOP()
#define IEM_MC_OR_LOCAL_U16(a_u16Local, a_u16Mask)                  NOP()
#define IEM_MC_OR_LOCAL_U32(a_u32Local, a_u32Mask)                  NOP()

#define IEM_MC_SAR_LOCAL_S16(a_i16Local, a_cShift)                  NOP()
#define IEM_MC_SAR_LOCAL_S32(a_i32Local, a_cShift)                  NOP()
#define IEM_MC_SAR_LOCAL_S64(a_i64Local, a_cShift)                  NOP()

#define IEM_MC_SHR_LOCAL_U8(a_u8Local, a_cShift)                    NOP()

#define IEM_MC_SHL_LOCAL_S16(a_i16Local, a_cShift)                  NOP()
#define IEM_MC_SHL_LOCAL_S32(a_i32Local, a_cShift)                  NOP()
#define IEM_MC_SHL_LOCAL_S64(a_i64Local, a_cShift)                  NOP()

#define IEM_MC_AND_2LOCS_U32(a_u32Local, a_u32Mask)                 NOP()

#define IEM_MC_OR_2LOCS_U32(a_u32Local, a_u32Mask)                  NOP()

#define IEM_MC_AND_GREG_U16(a_iGReg, a_u16Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_AND_GREG_U32(a_iGReg, a_u32Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_AND_GREG_U64(a_iGReg, a_u64Value)                    IEM_LIVENESS_GPR_INPUT(a_iGReg)

#define IEM_MC_OR_GREG_U16(a_iGReg, a_u16Value)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_OR_GREG_U32(a_iGReg, a_u32Value)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)
#define IEM_MC_OR_GREG_U64(a_iGReg, a_u64Value)                     IEM_LIVENESS_GPR_INPUT(a_iGReg)

#define IEM_MC_BSWAP_LOCAL_U16(a_u16Local)                          NOP()
#define IEM_MC_BSWAP_LOCAL_U32(a_u32Local)                          NOP()
#define IEM_MC_BSWAP_LOCAL_U64(a_u64Local)                          NOP()

#define IEM_MC_SET_EFL_BIT(a_fBit) do { \
        if ((a_fBit) == X86_EFL_CF)      IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); \
        else if ((a_fBit) == X86_EFL_DF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflOther); \
        else { AssertFailed();           IEM_LIVENESS_ALL_EFLAG_INPUT(); } \
    } while (0)
#define IEM_MC_CLEAR_EFL_BIT(a_fBit) do { \
        if ((a_fBit) == X86_EFL_CF)      IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); \
        else if ((a_fBit) == X86_EFL_DF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflOther); \
        else { AssertFailed();           IEM_LIVENESS_ALL_EFLAG_INPUT(); } \
    } while (0)
#define IEM_MC_FLIP_EFL_BIT(a_fBit) do { \
        if ((a_fBit) == X86_EFL_CF)      IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); \
        else { AssertFailed();           IEM_LIVENESS_ALL_EFLAG_INPUT(); } \
    } while (0)

#define IEM_MC_CLEAR_FSW_EX()                                       NOP()
#define IEM_MC_FPU_TO_MMX_MODE()                                    NOP()
#define IEM_MC_FPU_FROM_MMX_MODE()                                  NOP()

#define IEM_MC_FETCH_MREG_U64(a_u64Value, a_iMReg)                  NOP()
#define IEM_MC_FETCH_MREG_U32(a_u32Value, a_iMReg)                  NOP()
#define IEM_MC_STORE_MREG_U64(a_iMReg, a_u64Value)                  NOP()
#define IEM_MC_STORE_MREG_U32_ZX_U64(a_iMReg, a_u32Value)           NOP()
#define IEM_MC_REF_MREG_U64(a_pu64Dst, a_iMReg)                     NOP()
#define IEM_MC_REF_MREG_U64_CONST(a_pu64Dst, a_iMReg)               NOP()
#define IEM_MC_REF_MREG_U32_CONST(a_pu32Dst, a_iMReg)               NOP()
#define IEM_MC_MODIFIED_MREG(a_iMReg)                               NOP()
#define IEM_MC_MODIFIED_MREG_BY_REF(a_pu64Dst)                      NOP()

#define IEM_MC_CLEAR_XREG_U32_MASK(a_iXReg, a_bMask)                NOP()
#define IEM_MC_FETCH_XREG_U128(a_u128Value, a_iXReg)                NOP()
#define IEM_MC_FETCH_XREG_XMM(a_XmmValue, a_iXReg)                  NOP()
#define IEM_MC_FETCH_XREG_U64(a_u64Value, a_iXReg, a_iQWord)        NOP()
#define IEM_MC_FETCH_XREG_U32(a_u32Value, a_iXReg, a_iDWord)        NOP()
#define IEM_MC_FETCH_XREG_U16(a_u16Value, a_iXReg, a_iWord)         NOP()
#define IEM_MC_FETCH_XREG_U8( a_u8Value,  a_iXReg, a_iByte)         NOP()
#define IEM_MC_FETCH_XREG_PAIR_U128(a_Dst, a_iXReg1, a_iXReg2)      NOP()
#define IEM_MC_FETCH_XREG_PAIR_XMM(a_Dst, a_iXReg1, a_iXReg2)       NOP()
#define IEM_MC_FETCH_XREG_PAIR_U128_AND_RAX_RDX_U64(a_Dst, a_iXReg1, a_iXReg2) \
    do { IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)
#define IEM_MC_FETCH_XREG_PAIR_U128_AND_EAX_EDX_U32_SX_U64(a_Dst, a_iXReg1, a_iXReg2) \
    do { IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)
#define IEM_MC_STORE_XREG_U128(a_iXReg, a_u128Value)                NOP()
#define IEM_MC_STORE_XREG_XMM(a_iXReg, a_XmmValue)                  NOP()
#define IEM_MC_STORE_XREG_XMM_U32(a_iXReg, a_iDword, a_XmmValue)    NOP()
#define IEM_MC_STORE_XREG_XMM_U64(a_iXReg, a_iQword, a_XmmValue)    NOP()
#define IEM_MC_STORE_XREG_U64(a_iXReg, a_iQword, a_u64Value)        NOP()
#define IEM_MC_STORE_XREG_U32(a_iXReg, a_iDword, a_u32Value)        NOP()
#define IEM_MC_STORE_XREG_U16(a_iXReg, a_iWord, a_u16Value)         NOP()
#define IEM_MC_STORE_XREG_U8(a_iXReg,  a_iByte, a_u8Value)          NOP()
#define IEM_MC_STORE_XREG_U64_ZX_U128(a_iXReg, a_u64Value)          NOP()
#define IEM_MC_STORE_XREG_U32_U128(a_iXReg, a_iDwDst, a_u128Value, a_iDwSrc) NOP()
#define IEM_MC_STORE_XREG_R32(a_iXReg, a_r32Value)                  NOP()
#define IEM_MC_STORE_XREG_R64(a_iXReg, a_r64Value)                  NOP()
#define IEM_MC_STORE_XREG_U32_ZX_U128(a_iXReg, a_u32Value)          NOP()
#define IEM_MC_STORE_XREG_HI_U64(a_iXReg, a_u64Value)               NOP()

#define IEM_MC_BROADCAST_XREG_U8_ZX_VLMAX(a_iXRegDst, a_u8Src)      NOP()
#define IEM_MC_BROADCAST_XREG_U16_ZX_VLMAX(a_iXRegDst, a_u16Src)    NOP()
#define IEM_MC_BROADCAST_XREG_U32_ZX_VLMAX(a_iXRegDst, a_u32Src)    NOP()
#define IEM_MC_BROADCAST_XREG_U64_ZX_VLMAX(a_iXRegDst, a_u64Src)    NOP()

#define IEM_MC_REF_XREG_U128(a_pu128Dst, a_iXReg)                   NOP()
#define IEM_MC_REF_XREG_U128_CONST(a_pu128Dst, a_iXReg)             NOP()
#define IEM_MC_REF_XREG_XMM_CONST(a_pXmmDst, a_iXReg)               NOP()
#define IEM_MC_REF_XREG_U32_CONST(a_pu32Dst, a_iXReg)               NOP()
#define IEM_MC_REF_XREG_U64_CONST(a_pu64Dst, a_iXReg)               NOP()
#define IEM_MC_REF_XREG_R32_CONST(a_pr32Dst, a_iXReg)               NOP()
#define IEM_MC_REF_XREG_R64_CONST(a_pr64Dst, a_iXReg)               NOP()
#define IEM_MC_COPY_XREG_U128(a_iXRegDst, a_iXRegSrc)               NOP()

#define IEM_MC_FETCH_YREG_U32(a_u32Dst, a_iYRegSrc)                 NOP()
#define IEM_MC_FETCH_YREG_U64(a_u64Dst, a_iYRegSrc)                 NOP()
#define IEM_MC_FETCH_YREG_2ND_U64(a_u64Dst, a_iYRegSrc)             NOP()
#define IEM_MC_FETCH_YREG_U128(a_u128Dst, a_iYRegSrc)               NOP()
#define IEM_MC_FETCH_YREG_U256(a_u256Dst, a_iYRegSrc)               NOP()

#define IEM_MC_STORE_YREG_U128(a_iYRegDst, a_iDQword, a_u128Value)  NOP()

#define IEM_MC_INT_CLEAR_ZMM_256_UP(a_iXRegDst)                     NOP()
#define IEM_MC_STORE_YREG_U32_ZX_VLMAX(a_iYRegDst, a_u32Src)        NOP()
#define IEM_MC_STORE_YREG_U64_ZX_VLMAX(a_iYRegDst, a_u64Src)        NOP()
#define IEM_MC_STORE_YREG_U128_ZX_VLMAX(a_iYRegDst, a_u128Src)      NOP()
#define IEM_MC_STORE_YREG_U256_ZX_VLMAX(a_iYRegDst, a_u256Src)      NOP()

#define IEM_MC_BROADCAST_YREG_U8_ZX_VLMAX(a_iYRegDst, a_u8Src)      NOP()
#define IEM_MC_BROADCAST_YREG_U16_ZX_VLMAX(a_iYRegDst, a_u16Src)    NOP()
#define IEM_MC_BROADCAST_YREG_U32_ZX_VLMAX(a_iYRegDst, a_u32Src)    NOP()
#define IEM_MC_BROADCAST_YREG_U64_ZX_VLMAX(a_iYRegDst, a_u64Src)    NOP()
#define IEM_MC_BROADCAST_YREG_U128_ZX_VLMAX(a_iYRegDst, a_u128Src)  NOP()

#define IEM_MC_REF_YREG_U128(a_pu128Dst, a_iYReg)                   NOP()
#define IEM_MC_REF_YREG_U128_CONST(a_pu128Dst, a_iYReg)             NOP()
#define IEM_MC_REF_YREG_U64_CONST(a_pu64Dst, a_iYReg)               NOP()
#define IEM_MC_CLEAR_YREG_128_UP(a_iYReg)                           NOP()

#define IEM_MC_COPY_YREG_U256_ZX_VLMAX(a_iYRegDst, a_iYRegSrc)      NOP()
#define IEM_MC_COPY_YREG_U128_ZX_VLMAX(a_iYRegDst, a_iYRegSrc)      NOP()
#define IEM_MC_COPY_YREG_U64_ZX_VLMAX(a_iYRegDst, a_iYRegSrc)       NOP()

#define IEM_MC_MERGE_YREG_U32_U96_ZX_VLMAX(a_iYRegDst, a_iYRegSrc32, a_iYRegSrcHx)      NOP()
#define IEM_MC_MERGE_YREG_U64_U64_ZX_VLMAX(a_iYRegDst, a_iYRegSrc64, a_iYRegSrcHx)      NOP()
#define IEM_MC_MERGE_YREG_U64LO_U64LO_ZX_VLMAX(a_iYRegDst, a_iYRegSrc64, a_iYRegSrcHx)  NOP()
#define IEM_MC_MERGE_YREG_U64HI_U64HI_ZX_VLMAX(a_iYRegDst, a_iYRegSrc64, a_iYRegSrcHx)  NOP()
#define IEM_MC_MERGE_YREG_U64LO_U64LOCAL_ZX_VLMAX(a_iYRegDst, a_iYRegSrcHx, a_u64Local) NOP()
#define IEM_MC_MERGE_YREG_U64LOCAL_U64HI_ZX_VLMAX(a_iYRegDst, a_u64Local, a_iYRegSrcHx) NOP()

#define IEM_MC_FETCH_MEM_U8(a_u8Dst, a_iSeg, a_GCPtrMem)                                        IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM16_U8(a_u8Dst, a_iSeg, a_GCPtrMem16)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM32_U8(a_u8Dst, a_iSeg, a_GCPtrMem32)                                    IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U8(a_u8Dst, a_GCPtrMem)                                           IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM16_FLAT_U8(a_u8Dst, a_GCPtrMem16)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM32_FLAT_U8(a_u8Dst, a_GCPtrMem32)                                       IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U16(a_u16Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U16_DISP(a_u16Dst, a_iSeg, a_GCPtrMem, a_offDisp)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_I16(a_i16Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U16(a_u16Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U16_DISP(a_u16Dst, a_GCPtrMem, a_offDisp)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_I16(a_i16Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U32(a_u32Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U32_DISP(a_u32Dst, a_iSeg, a_GCPtrMem, a_offDisp)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_I32(a_i32Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U32(a_u32Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U32_DISP(a_u32Dst, a_GCPtrMem, a_offDisp)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_I32(a_i32Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U64_DISP(a_u64Dst, a_iSeg, a_GCPtrMem, a_offDisp)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U64_ALIGN_U128(a_u64Dst, a_iSeg, a_GCPtrMem)                           IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_I64(a_i64Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U64(a_u64Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U64_DISP(a_u64Dst, a_GCPtrMem, a_offDisp)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U64_ALIGN_U128(a_u64Dst, a_GCPtrMem)                              IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_I64(a_i64Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_R32(a_r32Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_R64(a_r64Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_R80(a_r80Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_D80(a_d80Dst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_R32(a_r32Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_R64(a_r64Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_R80(a_r80Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_D80(a_d80Dst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U128(a_u128Dst, a_iSeg, a_GCPtrMem)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U128_NO_AC(a_u128Dst, a_iSeg, a_GCPtrMem)                              IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U128_ALIGN_SSE(a_u128Dst, a_iSeg, a_GCPtrMem)                          IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_XMM(a_XmmDst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_XMM_NO_AC(a_XmmDst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_XMM_ALIGN_SSE(a_XmmDst, a_iSeg, a_GCPtrMem)                            IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_XMM_U32(a_XmmDst, a_iDWord, a_iSeg, a_GCPtrMem)                        IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_XMM_U64(a_XmmDst, a_iQWord, a_iSeg, a_GCPtrMem)                        IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U128(a_u128Dst, a_GCPtrMem)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U128_NO_AC(a_u128Dst, a_GCPtrMem)                                 IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U128_ALIGN_SSE(a_u128Dst, a_GCPtrMem)                             IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_FLAT_XMM(a_XmmDst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_XMM_NO_AC(a_XmmDst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_XMM_ALIGN_SSE(a_XmmDst, a_GCPtrMem)                               IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_XMM_U32(a_XmmDst, a_iDWord, a_GCPtrMem)                           IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_XMM_U64(a_XmmDst, a_iQWord, a_GCPtrMem)                           IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U128_AND_XREG_U128(a_Dst, a_iXReg1, a_iSeg2, a_GCPtrMem2)              IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128(a_Dst, a_iXReg1, a_GCPtrMem2)                  IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_XMM_ALIGN_SSE_AND_XREG_XMM(a_Dst, a_iXReg1, a_iSeg2, a_GCPtrMem2)      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_FLAT_XMM_ALIGN_SSE_AND_XREG_XMM(a_Dst, a_iXReg1, a_GCPtrMem2)          IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_XMM_U32_AND_XREG_XMM(a_Dst, a_iXReg1, a_iDWord2, a_iSeg2, a_GCPtrMem2) IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_FLAT_XMM_U32_AND_XREG_XMM(a_Dst, a_iXReg1, a_iDWord2, a_GCPtrMem2)     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_XMM_U64_AND_XREG_XMM(a_Dst, a_iXReg1, a_iQWord2, a_iSeg2, a_GCPtrMem2) IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_FLAT_XMM_U64_AND_XREG_XMM(a_Dst, a_iXReg1, a_iQWord2, a_GCPtrMem2)     IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U128_AND_XREG_U128_AND_RAX_RDX_U64(a_Dst, a_iXReg1, a_iSeg2, a_GCPtrMem2) \
    do { IEM_LIVENESS_MEM(a_iSeg2); IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)
#define IEM_MC_FETCH_MEM_U128_AND_XREG_U128_AND_EAX_EDX_U32_SX_U64(a_Dst, a_iXReg1, a_iSeg2, a_GCPtrMem2) \
    do { IEM_LIVENESS_MEM(a_iSeg2); IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)

#define IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128_AND_RAX_RDX_U64(a_Dst, a_iXReg1, a_GCPtrMem2) \
    do { IEM_LIVENESS_MEM_FLAT(); IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)
#define IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128_AND_EAX_EDX_U32_SX_U64(a_Dst, a_iXReg1, a_GCPtrMem2) \
    do { IEM_LIVENESS_MEM_FLAT(); IEM_LIVENESS_GPR_INPUT(X86_GREG_xAX); IEM_LIVENESS_GPR_INPUT(X86_GREG_xDX); } while (0)


#define IEM_MC_FETCH_MEM_U256(a_u256Dst, a_iSeg, a_GCPtrMem)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U256_NO_AC(a_u256Dst, a_iSeg, a_GCPtrMem)                              IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U256_ALIGN_AVX(a_u256Dst, a_iSeg, a_GCPtrMem)                          IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_YMM(a_YmmDst, a_iSeg, a_GCPtrMem)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_YMM_NO_AC(a_YmmDst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_YMM_ALIGN_AVX(a_YmmDst, a_iSeg, a_GCPtrMem)                            IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U256(a_u256Dst, a_GCPtrMem)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U256_NO_AC(a_u256Dst, a_GCPtrMem)                                 IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U256_ALIGN_AVX(a_u256Dst, a_GCPtrMem)                             IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_FLAT_YMM(a_YmmDst, a_GCPtrMem)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_YMM_NO_AC(a_YmmDst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_YMM_ALIGN_AVX(a_YmmDst, a_GCPtrMem)                               IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U8_ZX_U16(a_u16Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U8_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U8_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U16_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U16_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U32_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U16(a_u16Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U32(a_u32Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U64(a_u64Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U16_ZX_U32(a_u32Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U16_ZX_U64(a_u64Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U32_ZX_U64(a_u64Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_FETCH_MEM_U8_SX_U16(a_u16Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U8_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U8_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U16_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U16_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_FETCH_MEM_U32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem)                               IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U16(a_u16Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U32(a_u32Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U64(a_u64Dst, a_GCPtrMem)                                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U16_SX_U32(a_u32Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U16_SX_U64(a_u64Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_FETCH_MEM_FLAT_U32_SX_U64(a_u64Dst, a_GCPtrMem)                                  IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_STORE_MEM_U8(a_iSeg, a_GCPtrMem, a_u8Value)                                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U16(a_iSeg, a_GCPtrMem, a_u16Value)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U32(a_iSeg, a_GCPtrMem, a_u32Value)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U64(a_iSeg, a_GCPtrMem, a_u64Value)                                    IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_STORE_MEM_FLAT_U8(a_GCPtrMem, a_u8Value)                                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U16(a_GCPtrMem, a_u16Value)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U32(a_GCPtrMem, a_u32Value)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U64(a_GCPtrMem, a_u64Value)                                       IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_STORE_MEM_U8_CONST(a_iSeg, a_GCPtrMem, a_u8C)                                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U16_CONST(a_iSeg, a_GCPtrMem, a_u16C)                                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U32_CONST(a_iSeg, a_GCPtrMem, a_u32C)                                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U64_CONST(a_iSeg, a_GCPtrMem, a_u64C)                                  IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_STORE_MEM_FLAT_U8_CONST(a_GCPtrMem, a_u8C)                                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U16_CONST(a_GCPtrMem, a_u16C)                                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U32_CONST(a_GCPtrMem, a_u32C)                                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U64_CONST(a_GCPtrMem, a_u64C)                                     IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_STORE_MEM_I8_CONST_BY_REF( a_pi8Dst,  a_i8C)                                     NOP()
#define IEM_MC_STORE_MEM_I16_CONST_BY_REF(a_pi16Dst, a_i16C)                                    NOP()
#define IEM_MC_STORE_MEM_I32_CONST_BY_REF(a_pi32Dst, a_i32C)                                    NOP()
#define IEM_MC_STORE_MEM_I64_CONST_BY_REF(a_pi64Dst, a_i64C)                                    NOP()
#define IEM_MC_STORE_MEM_NEG_QNAN_R32_BY_REF(a_pr32Dst)                                         NOP()
#define IEM_MC_STORE_MEM_NEG_QNAN_R64_BY_REF(a_pr64Dst)                                         NOP()
#define IEM_MC_STORE_MEM_NEG_QNAN_R80_BY_REF(a_pr80Dst)                                         NOP()
#define IEM_MC_STORE_MEM_INDEF_D80_BY_REF(a_pd80Dst)                                            NOP()

#define IEM_MC_STORE_MEM_U128(a_iSeg, a_GCPtrMem, a_u128Value)                                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U128_ALIGN_SSE(a_iSeg, a_GCPtrMem, a_u128Value)                        IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_STORE_MEM_FLAT_U128(a_GCPtrMem, a_u128Value)                                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U128_ALIGN_SSE(a_GCPtrMem, a_u128Value)                           IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_STORE_MEM_U256(a_iSeg, a_GCPtrMem, a_u256Value)                                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_STORE_MEM_U256_ALIGN_AVX(a_iSeg, a_GCPtrMem, a_u256Value)                        IEM_LIVENESS_MEM(a_iSeg)

#define IEM_MC_STORE_MEM_FLAT_U256(a_GCPtrMem, a_u256Value)                                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_STORE_MEM_FLAT_U256_ALIGN_AVX(a_GCPtrMem, a_u256Value)                           IEM_LIVENESS_MEM_FLAT()

#define IEM_MC_PUSH_U16(a_u16Value)                  IEM_LIVENESS_STACK()
#define IEM_MC_PUSH_U32(a_u32Value)                  IEM_LIVENESS_STACK()
#define IEM_MC_PUSH_U32_SREG(a_uSegVal)              IEM_LIVENESS_STACK()
#define IEM_MC_PUSH_U64(a_u64Value)                  IEM_LIVENESS_STACK()

#define IEM_MC_POP_GREG_U16(a_iGReg)            do { IEM_LIVENESS_STACK();  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)
#define IEM_MC_POP_GREG_U32(a_iGReg)            do { IEM_LIVENESS_STACK();  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)
#define IEM_MC_POP_GREG_U64(a_iGReg)            do { IEM_LIVENESS_STACK();  IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)

/* 32-bit flat stack push and pop: */
#define IEM_MC_FLAT32_PUSH_U16(a_u16Value)           IEM_LIVENESS_STACK_FLAT()
#define IEM_MC_FLAT32_PUSH_U32(a_u32Value)           IEM_LIVENESS_STACK_FLAT()
#define IEM_MC_FLAT32_PUSH_U32_SREG(a_uSegVal)       IEM_LIVENESS_STACK_FLAT()

#define IEM_MC_FLAT32_POP_GREG_U16(a_iGReg)     do { IEM_LIVENESS_STACK_FLAT(); IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)
#define IEM_MC_FLAT32_POP_GREG_U32(a_iGReg)     do { IEM_LIVENESS_STACK_FLAT(); IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)

/* 64-bit flat stack push and pop: */
#define IEM_MC_FLAT64_PUSH_U16(a_u16Value)           IEM_LIVENESS_STACK_FLAT()
#define IEM_MC_FLAT64_PUSH_U64(a_u64Value)           IEM_LIVENESS_STACK_FLAT()

#define IEM_MC_FLAT64_POP_GREG_U16(a_iGReg)     do { IEM_LIVENESS_STACK_FLAT(); IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)
#define IEM_MC_FLAT64_POP_GREG_U64(a_iGReg)     do { IEM_LIVENESS_STACK_FLAT(); IEM_LIVENESS_GPR_CLOBBERED(a_iGReg); } while (0)


#define IEM_MC_MEM_MAP_U8_ATOMIC(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U8_RW(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                        IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U8_WO(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                        IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U8_RO(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                        IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_U8_ATOMIC(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem)                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U8_RW(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem)                           IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U8_WO(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem)                           IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U8_RO(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem)                           IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_U16_ATOMIC(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U16_RW(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U16_WO(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U16_RO(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_U16_ATOMIC(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem)                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U16_RW(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U16_WO(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U16_RO(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_I16_WO(a_pi16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_I16_WO(a_pi16Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_U32_ATOMIC(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U32_RW(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U32_WO(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U32_RO(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_U32_ATOMIC(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem)                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U32_RW(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U32_WO(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U32_RO(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_I32_WO(a_pi32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_I32_WO(a_pi32Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_R32_WO(a_pr32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_R32_WO(a_pr32Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_U64_ATOMIC(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                  IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U64_RW(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U64_WO(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U64_RO(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_U64_ATOMIC(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem)                     IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U64_RW(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U64_WO(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U64_RO(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_I64_WO(a_pi64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_I64_WO(a_pi64Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_R64_WO(a_pr64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_R64_WO(a_pr64Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_U128_ATOMIC(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U128_RW(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U128_WO(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_MAP_U128_RO(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                    IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_U128_ATOMIC(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem)                   IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U128_RW(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem)                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U128_WO(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem)                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_FLAT_MAP_U128_RO(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem)                       IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_R80_WO(a_pr80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_R80_WO(a_pr80Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()
#define IEM_MC_MEM_MAP_D80_WO(a_pd80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem)                      IEM_LIVENESS_MEM(a_iSeg)
#define IEM_MC_MEM_FLAT_MAP_D80_WO(a_pd80Mem, a_bUnmapInfo, a_GCPtrMem)                         IEM_LIVENESS_MEM_FLAT()


#define IEM_MC_MEM_COMMIT_AND_UNMAP_RW(a_bMapInfo)                                              NOP()
#define IEM_MC_MEM_COMMIT_AND_UNMAP_ATOMIC(a_bMapInfo)                                          NOP()
#define IEM_MC_MEM_COMMIT_AND_UNMAP_WO(a_bMapInfo)                                              NOP()
#define IEM_MC_MEM_COMMIT_AND_UNMAP_RO(a_bMapInfo)                                              NOP()
#define IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE_WO(a_bMapInfo, a_u16FSW)                      NOP()
#define IEM_MC_MEM_ROLLBACK_AND_UNMAP_WO(a_bMapInfo)                                            NOP()


#define IEM_MC_CALL_VOID_AIMPL_0(a_pfn)                                                         NOP()
#define IEM_MC_CALL_VOID_AIMPL_1(a_pfn, a0)                                                     NOP()
#define IEM_MC_CALL_VOID_AIMPL_2(a_pfn, a0, a1)                                                 NOP()
#define IEM_MC_CALL_VOID_AIMPL_3(a_pfn, a0, a1, a2)                                             NOP()
#define IEM_MC_CALL_VOID_AIMPL_4(a_pfn, a0, a1, a2, a3)                                         NOP()
#define IEM_MC_CALL_AIMPL_3(a_rc, a_pfn, a0, a1, a2)                                            NOP()
#define IEM_MC_CALL_AIMPL_4(a_rc, a_pfn, a0, a1, a2, a3)                                        NOP()

#define IEM_MC_CALL_FPU_AIMPL_1(a_pfnAImpl, a0)                                                 NOP()
#define IEM_MC_CALL_FPU_AIMPL_2(a_pfnAImpl, a0, a1)                                             NOP()
#define IEM_MC_CALL_FPU_AIMPL_3(a_pfnAImpl, a0, a1, a2)                                         NOP()

#define IEM_MC_SET_FPU_RESULT(a_FpuData, a_FSW, a_pr80Value)                                    NOP()

#define IEM_MC_PUSH_FPU_RESULT(a_FpuData, a_uFpuOpcode)                                         NOP()
#define IEM_MC_PUSH_FPU_RESULT_MEM_OP(a_FpuData, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode)           NOP()
#define IEM_MC_PUSH_FPU_RESULT_TWO(a_FpuDataTwo, a_uFpuOpcode)                                  NOP()

#define IEM_MC_STORE_FPU_RESULT(a_FpuData, a_iStReg, a_uFpuOpcode)                              NOP()
#define IEM_MC_STORE_FPU_RESULT_THEN_POP(a_FpuData, a_iStReg, a_uFpuOpcode)                     NOP()
#define IEM_MC_STORE_FPU_RESULT_MEM_OP(a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode) NOP()
#define IEM_MC_STORE_FPU_RESULT_WITH_MEM_OP_THEN_POP(a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode) NOP()

#define IEM_MC_UPDATE_FPU_OPCODE_IP(a_uFpuOpcode)                                               NOP()
#define IEM_MC_FPU_STACK_FREE(a_iStReg)                                                         NOP()
#define IEM_MC_FPU_STACK_INC_TOP()                                                              NOP()
#define IEM_MC_FPU_STACK_DEC_TOP()                                                              NOP()

#define IEM_MC_UPDATE_FSW(a_u16FSW, a_uFpuOpcode)                                               NOP()
#define IEM_MC_UPDATE_FSW_CONST(a_u16FSW, a_uFpuOpcode)                                         NOP()
#define IEM_MC_UPDATE_FSW_WITH_MEM_OP(a_u16FSW, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode)            NOP()
#define IEM_MC_UPDATE_FSW_THEN_POP(a_u16FSW, a_uFpuOpcode)                                      NOP()
#define IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(a_u16FSW, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode)   NOP()
#define IEM_MC_UPDATE_FSW_THEN_POP_POP(a_u16FSW, a_uFpuOpcode)                                  NOP()

#define IEM_MC_FPU_STACK_UNDERFLOW(a_iStDst, a_uFpuOpcode)                                      NOP()
#define IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(a_iStDst, a_uFpuOpcode)                             NOP()
#define IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(a_iStDst, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode)        NOP()
#define IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(a_iStDst, a_iEffSeg, a_GCPtrEff, a_uFpuOpcode) NOP()
#define IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP_POP(a_uFpuOpcode)                                   NOP()
#define IEM_MC_FPU_STACK_PUSH_UNDERFLOW(a_uFpuOpcode)                                           NOP()
#define IEM_MC_FPU_STACK_PUSH_UNDERFLOW_TWO(a_uFpuOpcode)                                       NOP()

#define IEM_MC_FPU_STACK_PUSH_OVERFLOW(a_uFpuOpcode)                                            NOP()
#define IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(a_iEffSeg, a_GCPtrEff, a_uFpuOpcode)              NOP()

#define IEM_MC_PREPARE_FPU_USAGE()                                                              NOP()
#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ()                                                   NOP()
#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE()                                                 NOP()

#define IEM_MC_STORE_SSE_RESULT(a_SseData, a_iXmmReg)                                           NOP()
#define IEM_MC_SSE_UPDATE_MXCSR(a_fMxcsr)                                                       NOP()

#define IEM_MC_PREPARE_SSE_USAGE()                                                              NOP()
#define IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ()                                                   NOP()
#define IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE()                                                 NOP()

#define IEM_MC_PREPARE_AVX_USAGE()                                                              NOP()
#define IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ()                                                   NOP()
#define IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE()                                                 NOP()

#define IEM_MC_CALL_MMX_AIMPL_2(a_pfnAImpl, a0, a1)                                             NOP()
#define IEM_MC_CALL_MMX_AIMPL_3(a_pfnAImpl, a0, a1, a2)                                         NOP()
#define IEM_MC_CALL_SSE_AIMPL_2(a_pfnAImpl, a0, a1)                                             NOP()
#define IEM_MC_CALL_SSE_AIMPL_3(a_pfnAImpl, a0, a1, a2)                                         NOP()
#define IEM_MC_IMPLICIT_AVX_AIMPL_ARGS()                                                        NOP()
#define IEM_MC_CALL_AVX_AIMPL_2(a_pfnAImpl, a1, a2)                                             NOP()
#define IEM_MC_CALL_AVX_AIMPL_3(a_pfnAImpl, a1, a2, a3)                                         NOP()

#define IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit) \
    do { if (     (a_fBit) == X86_EFL_CF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); \
         else if ((a_fBit) == X86_EFL_PF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflPf); \
         else if ((a_fBit) == X86_EFL_AF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflAf); \
         else if ((a_fBit) == X86_EFL_ZF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflZf); \
         else if ((a_fBit) == X86_EFL_SF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflSf); \
         else if ((a_fBit) == X86_EFL_OF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflOf); \
         else if ((a_fBit) == X86_EFL_DF) IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflOther); /* loadsb and friends */ \
         else { AssertMsgFailed(("#s (%#x)\n", #a_fBit, (a_fBit)));  IEM_LIVENESS_ALL_EFLAGS_INPUT(); } \
    } while (0)

#define IEM_MC_IF_EFL_BIT_SET(a_fBit)                   IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit); {
#define IEM_MC_IF_EFL_BIT_NOT_SET(a_fBit)               IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit); {
#define IEM_MC_IF_EFL_ANY_BITS_SET(a_fBits) \
    do { if ((a_fBits) == (X86_EFL_CF | X86_EFL_ZF)) \
         { IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflZf); } \
         else { AssertMsgFailed(("#s (%#x)\n", #a_fBits, (a_fBits)));  IEM_LIVENESS_ALL_EFLAGS_INPUT(); } \
    } while (0);                                        {
#define IEM_MC_IF_EFL_NO_BITS_SET(a_fBits) \
    do { if ((a_fBits) == (X86_EFL_CF | X86_EFL_ZF)) \
         { IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflCf); IEM_LIVENESS_ONE_EFLAG_INPUT(u2EflZf); } \
         else { AssertMsgFailed(("#s (%#x)\n", #a_fBits, (a_fBits)));  IEM_LIVENESS_ALL_EFLAGS_INPUT(); } \
    } while (0);                                        {
#define IEM_MC_IF_EFL_BITS_NE(a_fBit1, a_fBit2) \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit1); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit2);       {
#define IEM_MC_IF_EFL_BITS_EQ(a_fBit1, a_fBit2) \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit1); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit2);       {
#define IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(a_fBit, a_fBit1, a_fBit2) \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit1); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit2);       {
#define IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(a_fBit, a_fBit1, a_fBit2) \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit1); \
    IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit2);       {
#define IEM_MC_IF_CX_IS_NZ()                            IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_ECX_IS_NZ()                           IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_RCX_IS_NZ()                           IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_CX_IS_NOT_ONE()                       IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_ECX_IS_NOT_ONE()                      IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_RCX_IS_NOT_ONE()                      IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); {
#define IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
        IEM_LIVENESS_GPR_INPUT(X86_GREG_xCX); \
        IEM_LIVENESS_ONE_STATUS_EFLAG_INPUT(a_fBit);    {
#define IEM_MC_IF_LOCAL_IS_Z(a_Local)                   {
#define IEM_MC_IF_GREG_BIT_SET(a_iGReg, a_iBitNo)       IEM_LIVENESS_GPR_INPUT(a_iGReg); {

#define IEM_MC_REF_FPUREG(a_pr80Dst, a_iSt)             NOP()
#define IEM_MC_IF_FPUREG_IS_EMPTY(a_iSt)                {
#define IEM_MC_IF_FPUREG_NOT_EMPTY(a_iSt)               {
#define IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(a_pr80Dst, a_iSt) {
#define IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(a_pr80Dst0, a_iSt0, a_pr80Dst1, a_iSt1) {
#define IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(a_pr80Dst0, a_iSt0, a_iSt1) {
#define IEM_MC_IF_FCW_IM()                              {
#define IEM_MC_IF_MXCSR_XCPT_PENDING()                  {

#define IEM_MC_ELSE()                                   } /*else*/ {
#define IEM_MC_ENDIF()                                  } do {} while (0)

#define IEM_MC_HINT_FLUSH_GUEST_SHADOW(g_fGstShwFlush)  NOP()


/*********************************************************************************************************************************
*   The threaded functions.                                                                                                      *
*********************************************************************************************************************************/
#include "IEMNativeLiveness.cpp.h"


