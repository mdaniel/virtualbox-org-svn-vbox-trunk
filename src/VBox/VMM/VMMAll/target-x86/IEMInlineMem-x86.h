/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Inlined Memory Functions, x86 target.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineMem_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineMem_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/errcore.h>




/** @name   Memory access.
 *
 * @{
 */

/**
 * Checks whether alignment checks are enabled or not.
 *
 * @returns true if enabled, false if not.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(bool) iemMemAreAlignmentChecksEnabled(PVMCPUCC pVCpu) RT_NOEXCEPT
{
#if 0
    AssertCompile(X86_CR0_AM == X86_EFL_AC);
    return IEM_GET_CPL(pVCpu) == 3
        && (((uint32_t)pVCpu->cpum.GstCtx.cr0 & pVCpu->cpum.GstCtx.eflags.u) & X86_CR0_AM);
#else
    return RT_BOOL(pVCpu->iem.s.fExec & IEM_F_X86_AC);
#endif
}

/**
 * Checks if the given segment can be written to, raise the appropriate
 * exception if not.
 *
 * @returns VBox strict status code.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pHid                Pointer to the hidden register.
 * @param   iSegReg             The register number.
 * @param   pu64BaseAddr        Where to return the base address to use for the
 *                              segment. (In 64-bit code it may differ from the
 *                              base in the hidden segment.)
 */
DECLINLINE(VBOXSTRICTRC) iemMemSegCheckWriteAccessEx(PVMCPUCC pVCpu, PCCPUMSELREGHID pHid,
                                                     uint8_t iSegReg, uint64_t *pu64BaseAddr) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));

    if (IEM_IS_64BIT_CODE(pVCpu))
        *pu64BaseAddr = iSegReg < X86_SREG_FS ? 0 : pHid->u64Base;
    else
    {
        if (!pHid->Attr.n.u1Present)
        {
            uint16_t    uSel = iemSRegFetchU16(pVCpu, iSegReg);
            AssertRelease(uSel == 0);
            LogEx(LOG_GROUP_IEM,("iemMemSegCheckWriteAccessEx: %#x (index %u) - bad selector -> #GP\n", uSel, iSegReg));
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        if (   (   (pHid->Attr.n.u4Type & X86_SEL_TYPE_CODE)
                || !(pHid->Attr.n.u4Type & X86_SEL_TYPE_WRITE) )
            && !IEM_IS_64BIT_CODE(pVCpu) )
            return iemRaiseSelectorInvalidAccess(pVCpu, iSegReg, IEM_ACCESS_DATA_W);
        *pu64BaseAddr = pHid->u64Base;
    }
    return VINF_SUCCESS;
}


/**
 * Checks if the given segment can be read from, raise the appropriate
 * exception if not.
 *
 * @returns VBox strict status code.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pHid                Pointer to the hidden register.
 * @param   iSegReg             The register number.
 * @param   pu64BaseAddr        Where to return the base address to use for the
 *                              segment. (In 64-bit code it may differ from the
 *                              base in the hidden segment.)
 */
DECLINLINE(VBOXSTRICTRC) iemMemSegCheckReadAccessEx(PVMCPUCC pVCpu, PCCPUMSELREGHID pHid,
                                                    uint8_t iSegReg, uint64_t *pu64BaseAddr) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));

    if (IEM_IS_64BIT_CODE(pVCpu))
        *pu64BaseAddr = iSegReg < X86_SREG_FS ? 0 : pHid->u64Base;
    else
    {
        if (!pHid->Attr.n.u1Present)
        {
            uint16_t    uSel = iemSRegFetchU16(pVCpu, iSegReg);
            AssertRelease(uSel == 0);
            LogEx(LOG_GROUP_IEM,("iemMemSegCheckReadAccessEx: %#x (index %u) - bad selector -> #GP\n", uSel, iSegReg));
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        if ((pHid->Attr.n.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
            return iemRaiseSelectorInvalidAccess(pVCpu, iSegReg, IEM_ACCESS_DATA_R);
        *pu64BaseAddr = pHid->u64Base;
    }
    return VINF_SUCCESS;
}


#ifdef IEM_WITH_SETJMP

/** @todo slim this down   */
DECL_INLINE_THROW(RTGCPTR) iemMemApplySegmentToReadJmp(PVMCPUCC pVCpu, uint8_t iSegReg,
                                                       size_t cbMem, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
    Assert(cbMem >= 1);
    Assert(iSegReg < X86_SREG_COUNT);

    /*
     * 64-bit mode is simpler.
     */
    if (IEM_IS_64BIT_CODE(pVCpu))
    {
        if (iSegReg >= X86_SREG_FS && iSegReg != UINT8_MAX)
        {
            IEM_CTX_IMPORT_JMP(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));
            PCPUMSELREGHID const pSel = iemSRegGetHid(pVCpu, iSegReg);
            GCPtrMem += pSel->u64Base;
        }

        if (RT_LIKELY(X86_IS_CANONICAL(GCPtrMem) && X86_IS_CANONICAL(GCPtrMem + cbMem - 1)))
            return GCPtrMem;
        iemRaiseGeneralProtectionFault0Jmp(pVCpu);
    }
    /*
     * 16-bit and 32-bit segmentation.
     */
    else if (iSegReg != UINT8_MAX)
    {
        /** @todo Does this apply to segments with 4G-1 limit? */
        uint32_t const GCPtrLast32 = (uint32_t)GCPtrMem + (uint32_t)cbMem - 1;
        if (RT_LIKELY(GCPtrLast32 >= (uint32_t)GCPtrMem))
        {
            IEM_CTX_IMPORT_JMP(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));
            PCPUMSELREGHID const pSel = iemSRegGetHid(pVCpu, iSegReg);
            switch (pSel->Attr.u & (  X86DESCATTR_P     | X86DESCATTR_UNUSABLE
                                    | X86_SEL_TYPE_READ | X86_SEL_TYPE_WRITE /* same as read */
                                    | X86_SEL_TYPE_DOWN | X86_SEL_TYPE_CONF  /* same as down */
                                    | X86_SEL_TYPE_CODE))
            {
                case X86DESCATTR_P:                                         /* readonly data, expand up */
                case X86DESCATTR_P | X86_SEL_TYPE_WRITE:                    /* writable data, expand up */
                case X86DESCATTR_P | X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ: /* code, read-only */
                case X86DESCATTR_P | X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_CONF: /* conforming code, read-only */
                    /* expand up */
                    if (RT_LIKELY(GCPtrLast32 <= pSel->u32Limit))
                        return (uint32_t)GCPtrMem + (uint32_t)pSel->u64Base;
                    Log10(("iemMemApplySegmentToReadJmp: out of bounds %#x..%#x vs %#x\n",
                           (uint32_t)GCPtrMem, GCPtrLast32, pSel->u32Limit));
                    break;

                case X86DESCATTR_P | X86_SEL_TYPE_DOWN:                         /* readonly data, expand down */
                case X86DESCATTR_P | X86_SEL_TYPE_DOWN | X86_SEL_TYPE_WRITE:    /* writable data, expand down */
                    /* expand down */
                    if (RT_LIKELY(   (uint32_t)GCPtrMem > pSel->u32Limit
                                  && (   pSel->Attr.n.u1DefBig
                                      || GCPtrLast32 <= UINT32_C(0xffff)) ))
                        return (uint32_t)GCPtrMem + (uint32_t)pSel->u64Base;
                    Log10(("iemMemApplySegmentToReadJmp: expand down out of bounds %#x..%#x vs %#x..%#x\n",
                           (uint32_t)GCPtrMem, GCPtrLast32, pSel->u32Limit, pSel->Attr.n.u1DefBig ? UINT32_MAX : UINT16_MAX));
                    break;

                default:
                    Log10(("iemMemApplySegmentToReadJmp: bad selector %#x\n", pSel->Attr.u));
                    iemRaiseSelectorInvalidAccessJmp(pVCpu, iSegReg, IEM_ACCESS_DATA_R);
                    break;
            }
        }
        Log10(("iemMemApplySegmentToReadJmp: out of bounds %#x..%#x\n",(uint32_t)GCPtrMem, GCPtrLast32));
        iemRaiseSelectorBoundsJmp(pVCpu, iSegReg, IEM_ACCESS_DATA_R);
    }
    /*
     * 32-bit flat address.
     */
    else
        return GCPtrMem;
}


/** @todo slim this down   */
DECL_INLINE_THROW(RTGCPTR) iemMemApplySegmentToWriteJmp(PVMCPUCC pVCpu, uint8_t iSegReg, size_t cbMem,
                                                        RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP
{
    Assert(cbMem >= 1);
    Assert(iSegReg < X86_SREG_COUNT);

    /*
     * 64-bit mode is simpler.
     */
    if (IEM_IS_64BIT_CODE(pVCpu))
    {
        if (iSegReg >= X86_SREG_FS)
        {
            IEM_CTX_IMPORT_JMP(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));
            PCPUMSELREGHID pSel = iemSRegGetHid(pVCpu, iSegReg);
            GCPtrMem += pSel->u64Base;
        }

        if (RT_LIKELY(X86_IS_CANONICAL(GCPtrMem) && X86_IS_CANONICAL(GCPtrMem + cbMem - 1)))
            return GCPtrMem;
    }
    /*
     * 16-bit and 32-bit segmentation.
     */
    else
    {
        Assert(GCPtrMem <= UINT32_MAX);
        IEM_CTX_IMPORT_JMP(pVCpu, CPUMCTX_EXTRN_SREG_FROM_IDX(iSegReg));
        PCPUMSELREGHID pSel           = iemSRegGetHid(pVCpu, iSegReg);
        uint32_t const fRelevantAttrs = pSel->Attr.u & (  X86DESCATTR_P     | X86DESCATTR_UNUSABLE
                                                        | X86_SEL_TYPE_CODE | X86_SEL_TYPE_WRITE | X86_SEL_TYPE_DOWN);
        if (   fRelevantAttrs == (X86DESCATTR_P | X86_SEL_TYPE_WRITE) /* data, expand up */
               /** @todo explore exactly how the CS stuff works in real mode. See also
                *        http://www.rcollins.org/Productivity/DescriptorCache.html and
                *        http://www.rcollins.org/ddj/Aug98/Aug98.html for some insight. */
            || (iSegReg == X86_SREG_CS && IEM_IS_REAL_OR_V86_MODE(pVCpu)) ) /* Ignored for CS. */ /** @todo testcase! */
        {
            /* expand up */
            uint32_t const GCPtrLast32 = (uint32_t)GCPtrMem + (uint32_t)cbMem - 1;
            if (RT_LIKELY(   GCPtrLast32 <= pSel->u32Limit
                          && GCPtrLast32 >= (uint32_t)GCPtrMem))
                return (uint32_t)GCPtrMem + (uint32_t)pSel->u64Base;
            iemRaiseSelectorBoundsJmp(pVCpu, iSegReg, IEM_ACCESS_DATA_W);
        }
        else if (fRelevantAttrs == (X86DESCATTR_P | X86_SEL_TYPE_WRITE | X86_SEL_TYPE_DOWN)) /* data, expand up */
        {
            /* expand down - the uppger boundary is defined by the B bit, not G. */
            uint32_t GCPtrLast32 = (uint32_t)GCPtrMem + (uint32_t)cbMem - 1;
            if (RT_LIKELY(   (uint32_t)GCPtrMem >= pSel->u32Limit
                          && (pSel->Attr.n.u1DefBig || GCPtrLast32 <= UINT32_C(0xffff))
                          && GCPtrLast32 >= (uint32_t)GCPtrMem))
                return (uint32_t)GCPtrMem + (uint32_t)pSel->u64Base;
            iemRaiseSelectorBoundsJmp(pVCpu, iSegReg, IEM_ACCESS_DATA_W);
        }
        else
            iemRaiseSelectorInvalidAccessJmp(pVCpu, iSegReg, IEM_ACCESS_DATA_W);
    }
    iemRaiseGeneralProtectionFault0Jmp(pVCpu);
}

#endif /* IEM_WITH_SETJMP */

/**
 * Fakes a long mode stack selector for SS = 0.
 *
 * @param   pDescSs             Where to return the fake stack descriptor.
 * @param   uDpl                The DPL we want.
 */
DECLINLINE(void) iemMemFakeStackSelDesc(PIEMSELDESC pDescSs, uint32_t uDpl) RT_NOEXCEPT
{
    pDescSs->Long.au64[0] = 0;
    pDescSs->Long.au64[1] = 0;
    pDescSs->Long.Gen.u4Type     = X86_SEL_TYPE_RW_ACC;
    pDescSs->Long.Gen.u1DescType = 1; /* 1 = code / data, 0 = system. */
    pDescSs->Long.Gen.u2Dpl      = uDpl;
    pDescSs->Long.Gen.u1Present  = 1;
    pDescSs->Long.Gen.u1Long     = 1;
}


/*
 * Instantiate R/W inline templates.
 */

/** @def TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK
 * Used to check if an unaligned access is if within the page and won't
 * trigger an \#AC.
 *
 * This can also be used to deal with misaligned accesses on platforms that are
 * senstive to such if desires.
 */
#if 1
# define TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(a_pVCpu, a_GCPtrEff, a_TmplMemType) \
    (   ((a_GCPtrEff) & GUEST_PAGE_OFFSET_MASK) <= GUEST_PAGE_SIZE - sizeof(a_TmplMemType) \
     && !((a_pVCpu)->iem.s.fExec & IEM_F_X86_AC) )
#else
# define TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(a_pVCpu, a_GCPtrEff, a_TmplMemType) 0
#endif

#define TMPL_MEM_WITH_ATOMIC_MAPPING

#define TMPL_MEM_TYPE       uint8_t
#define TMPL_MEM_TYPE_ALIGN 0
#define TMPL_MEM_TYPE_SIZE  1
#define TMPL_MEM_FN_SUFF    U8
#define TMPL_MEM_FMT_TYPE   "%#04x"
#define TMPL_MEM_FMT_DESC   "byte"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_MEM_WITH_STACK

#define TMPL_MEM_TYPE       uint16_t
#define TMPL_MEM_TYPE_ALIGN 1
#define TMPL_MEM_TYPE_SIZE  2
#define TMPL_MEM_FN_SUFF    U16
#define TMPL_MEM_FMT_TYPE   "%#06x"
#define TMPL_MEM_FMT_DESC   "word"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_WITH_PUSH_SREG
#define TMPL_MEM_TYPE       uint32_t
#define TMPL_MEM_TYPE_ALIGN 3
#define TMPL_MEM_TYPE_SIZE  4
#define TMPL_MEM_FN_SUFF    U32
#define TMPL_MEM_FMT_TYPE   "%#010x"
#define TMPL_MEM_FMT_DESC   "dword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef  TMPL_WITH_PUSH_SREG

#define TMPL_MEM_TYPE       uint64_t
#define TMPL_MEM_TYPE_ALIGN 7
#define TMPL_MEM_TYPE_SIZE  8
#define TMPL_MEM_FN_SUFF    U64
#define TMPL_MEM_FMT_TYPE   "%#018RX64"
#define TMPL_MEM_FMT_DESC   "qword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#undef TMPL_MEM_WITH_STACK
#undef TMPL_MEM_WITH_ATOMIC_MAPPING

#define TMPL_MEM_NO_MAPPING /* currently sticky */

#define TMPL_MEM_NO_STORE
#define TMPL_MEM_TYPE       uint32_t
#define TMPL_MEM_TYPE_ALIGN 0
#define TMPL_MEM_TYPE_SIZE  4
#define TMPL_MEM_FN_SUFF    U32NoAc
#define TMPL_MEM_FMT_TYPE   "%#010x"
#define TMPL_MEM_FMT_DESC   "dword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_MEM_NO_STORE
#define TMPL_MEM_TYPE       uint64_t
#define TMPL_MEM_TYPE_ALIGN 0
#define TMPL_MEM_TYPE_SIZE  8
#define TMPL_MEM_FN_SUFF    U64NoAc
#define TMPL_MEM_FMT_TYPE   "%#018RX64"
#define TMPL_MEM_FMT_DESC   "qword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_MEM_NO_STORE
#define TMPL_MEM_TYPE       uint64_t
#define TMPL_MEM_TYPE_ALIGN 15
#define TMPL_MEM_TYPE_SIZE  8
#define TMPL_MEM_FN_SUFF    U64AlignedU128
#define TMPL_MEM_FMT_TYPE   "%#018RX64"
#define TMPL_MEM_FMT_DESC   "qword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#undef TMPL_MEM_NO_MAPPING

#define TMPL_MEM_TYPE       RTFLOAT80U
#define TMPL_MEM_TYPE_ALIGN 7
#define TMPL_MEM_TYPE_SIZE  10
#define TMPL_MEM_FN_SUFF    R80
#define TMPL_MEM_FMT_TYPE   "%.10Rhxs"
#define TMPL_MEM_FMT_DESC   "tword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_MEM_TYPE       RTPBCD80U
#define TMPL_MEM_TYPE_ALIGN 7           /** @todo RTPBCD80U alignment testcase */
#define TMPL_MEM_TYPE_SIZE  10
#define TMPL_MEM_FN_SUFF    D80
#define TMPL_MEM_FMT_TYPE   "%.10Rhxs"
#define TMPL_MEM_FMT_DESC   "tword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"

#define TMPL_MEM_WITH_ATOMIC_MAPPING
#define TMPL_MEM_TYPE       RTUINT128U
#define TMPL_MEM_TYPE_ALIGN 15
#define TMPL_MEM_TYPE_SIZE  16
#define TMPL_MEM_FN_SUFF    U128
#define TMPL_MEM_FMT_TYPE   "%.16Rhxs"
#define TMPL_MEM_FMT_DESC   "dqword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef  TMPL_MEM_WITH_ATOMIC_MAPPING

#define TMPL_MEM_NO_MAPPING
#define TMPL_MEM_TYPE       RTUINT128U
#define TMPL_MEM_TYPE_ALIGN 0
#define TMPL_MEM_TYPE_SIZE  16
#define TMPL_MEM_FN_SUFF    U128NoAc
#define TMPL_MEM_FMT_TYPE   "%.16Rhxs"
#define TMPL_MEM_FMT_DESC   "dqword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef TMPL_MEM_NO_MAPPING


/* Every template relying on unaligned accesses inside a page not being okay should go below. */
#undef TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK
#define TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK(a_pVCpu, a_GCPtrEff, a_TmplMemType) 0

#define TMPL_MEM_NO_MAPPING
#define TMPL_MEM_TYPE       RTUINT128U
#define TMPL_MEM_TYPE_ALIGN 15
#define TMPL_MEM_TYPE_SIZE  16
#define TMPL_MEM_FN_SUFF    U128AlignedSse
#define TMPL_MEM_FMT_TYPE   "%.16Rhxs"
#define TMPL_MEM_FMT_DESC   "dqword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef  TMPL_MEM_NO_MAPPING

#define TMPL_MEM_NO_MAPPING
#define TMPL_MEM_TYPE       RTUINT256U
#define TMPL_MEM_TYPE_ALIGN 0
#define TMPL_MEM_TYPE_SIZE  32
#define TMPL_MEM_FN_SUFF    U256NoAc
#define TMPL_MEM_FMT_TYPE   "%.32Rhxs"
#define TMPL_MEM_FMT_DESC   "qqword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef TMPL_MEM_NO_MAPPING

#define TMPL_MEM_NO_MAPPING
#define TMPL_MEM_TYPE       RTUINT256U
#define TMPL_MEM_TYPE_ALIGN 31
#define TMPL_MEM_TYPE_SIZE  32
#define TMPL_MEM_FN_SUFF    U256AlignedAvx
#define TMPL_MEM_FMT_TYPE   "%.32Rhxs"
#define TMPL_MEM_FMT_DESC   "qqword"
#include "IEMAllMemRWTmplInline-x86.cpp.h"
#undef TMPL_MEM_NO_MAPPING

#undef TMPL_MEM_CHECK_UNALIGNED_WITHIN_PAGE_OK

/** @} */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineMem_x86_h */
