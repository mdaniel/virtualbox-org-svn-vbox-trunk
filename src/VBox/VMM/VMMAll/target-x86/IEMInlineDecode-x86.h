/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Inlined Decoding related Functions, x86 target.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineDecode_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineDecode_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>


#ifndef IEM_WITH_OPAQUE_DECODER_STATE

# ifndef IEM_WITH_SETJMP

/**
 * Fetches the first opcode byte.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   pu8                 Where to return the opcode byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetFirstU8(PVMCPUCC pVCpu, uint8_t *pu8) RT_NOEXCEPT
{
    /*
     * Check for hardware instruction breakpoints.
     * Note! Guest breakpoints are only checked after POP SS or MOV SS on AMD CPUs.
     */
    if (RT_LIKELY(!(pVCpu->iem.s.fExec & IEM_F_PENDING_BRK_INSTR)))
    { /* likely */ }
    else
    {
        VBOXSTRICTRC rcStrict = DBGFBpCheckInstruction(pVCpu->CTX_SUFF(pVM), pVCpu,
                                                       pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base,
                                                          !(pVCpu->cpum.GstCtx.rflags.uBoth & CPUMCTX_INHIBIT_SHADOW_SS)
                                                       || IEM_IS_GUEST_CPU_AMD(pVCpu));
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */ }
        else
        {
            *pu8 = 0xff; /* shut up gcc. sigh */
            if (rcStrict == VINF_EM_RAW_GUEST_TRAP)
                return iemRaiseDebugException(pVCpu);
            return rcStrict;
        }
    }

    /*
     * Fetch the first opcode byte.
     */
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode < pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 1;
        *pu8 = pVCpu->iem.s.abOpcode[offOpcode];
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU8Slow(pVCpu, pu8);
}

# else  /* IEM_WITH_SETJMP */

/**
 * Fetches the first opcode byte, longjmp on error.
 *
 * @returns The opcode byte.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECL_INLINE_THROW(uint8_t) iemOpcodeGetFirstU8Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
    /*
     * Check for hardware instruction breakpoints.
    * Note! Guest breakpoints are only checked after POP SS or MOV SS on AMD CPUs.
     */
    if (RT_LIKELY(!(pVCpu->iem.s.fExec & IEM_F_PENDING_BRK_INSTR)))
    { /* likely */ }
    else
    {
        VBOXSTRICTRC rcStrict = DBGFBpCheckInstruction(pVCpu->CTX_SUFF(pVM), pVCpu,
                                                       pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base,
                                                          !(pVCpu->cpum.GstCtx.rflags.uBoth & CPUMCTX_INHIBIT_SHADOW_SS)
                                                       || IEM_IS_GUEST_CPU_AMD(pVCpu));
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */ }
        else
        {
            if (rcStrict == VINF_EM_RAW_GUEST_TRAP)
                rcStrict = iemRaiseDebugException(pVCpu);
            IEM_DO_LONGJMP(pVCpu, VBOXSTRICTRC_VAL(rcStrict));
        }
    }

    /*
     * Fetch the first opcode byte.
     */
#  ifdef IEM_WITH_CODE_TLB
    uint8_t         bRet;
    uintptr_t       offBuf = pVCpu->iem.s.offInstrNextByte;
    uint8_t const  *pbBuf  = pVCpu->iem.s.pbInstrBuf;
    if (RT_LIKELY(   pbBuf != NULL
                  && offBuf < pVCpu->iem.s.cbInstrBuf))
    {
        pVCpu->iem.s.offInstrNextByte = (uint32_t)offBuf + 1;
        bRet = pbBuf[offBuf];
    }
    else
        bRet = iemOpcodeGetNextU8SlowJmp(pVCpu);
#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    Assert(pVCpu->iem.s.offOpcode == 0);
    pVCpu->iem.s.abOpcode[pVCpu->iem.s.offOpcode++] = bRet;
#   endif
    return bRet;

#  else /* !IEM_WITH_CODE_TLB */
    uintptr_t offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode < pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 1;
        return pVCpu->iem.s.abOpcode[offOpcode];
    }
    return iemOpcodeGetNextU8SlowJmp(pVCpu);
#  endif
}

# endif /* IEM_WITH_SETJMP */

/**
 * Fetches the first opcode byte, returns/throws automatically on failure.
 *
 * @param   a_pu8               Where to return the opcode byte.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_FIRST_U8(a_pu8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetFirstU8(pVCpu, (a_pu8)); \
        if (rcStrict2 == VINF_SUCCESS) \
        { /* likely */ } \
        else \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_FIRST_U8(a_pu8) (*(a_pu8) = iemOpcodeGetFirstU8Jmp(pVCpu))
# endif /* IEM_WITH_SETJMP */


# ifndef IEM_WITH_SETJMP

/**
 * Fetches the next opcode byte.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   pu8                 Where to return the opcode byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU8(PVMCPUCC pVCpu, uint8_t *pu8) RT_NOEXCEPT
{
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode < pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 1;
        *pu8 = pVCpu->iem.s.abOpcode[offOpcode];
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU8Slow(pVCpu, pu8);
}

# else  /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode byte, longjmp on error.
 *
 * @returns The opcode byte.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECL_INLINE_THROW(uint8_t) iemOpcodeGetNextU8Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  ifdef IEM_WITH_CODE_TLB
    uint8_t         bRet;
    uintptr_t       offBuf = pVCpu->iem.s.offInstrNextByte;
    uint8_t const  *pbBuf  = pVCpu->iem.s.pbInstrBuf;
    if (RT_LIKELY(   pbBuf != NULL
                  && offBuf < pVCpu->iem.s.cbInstrBuf))
    {
        pVCpu->iem.s.offInstrNextByte = (uint32_t)offBuf + 1;
        bRet = pbBuf[offBuf];
    }
    else
        bRet = iemOpcodeGetNextU8SlowJmp(pVCpu);
#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    Assert(pVCpu->iem.s.offOpcode < sizeof(pVCpu->iem.s.abOpcode));
    pVCpu->iem.s.abOpcode[pVCpu->iem.s.offOpcode++] = bRet;
#   endif
    return bRet;

#  else /* !IEM_WITH_CODE_TLB */
    uintptr_t offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode < pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 1;
        return pVCpu->iem.s.abOpcode[offOpcode];
    }
    return iemOpcodeGetNextU8SlowJmp(pVCpu);
#  endif
}

# endif /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode byte, returns automatically on failure.
 *
 * @param   a_pu8               Where to return the opcode byte.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U8(a_pu8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU8(pVCpu, (a_pu8)); \
        if (rcStrict2 == VINF_SUCCESS) \
        { /* likely */ } \
        else \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U8(a_pu8) (*(a_pu8) = iemOpcodeGetNextU8Jmp(pVCpu))
# endif /* IEM_WITH_SETJMP */


# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed byte from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pi8                 Where to return the signed byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8(PVMCPUCC pVCpu, int8_t *pi8) RT_NOEXCEPT
{
    return iemOpcodeGetNextU8(pVCpu, (uint8_t *)pi8);
}
# endif /* !IEM_WITH_SETJMP */


/**
 * Fetches the next signed byte from the opcode stream, returning automatically
 * on failure.
 *
 * @param   a_pi8               Where to return the signed byte.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S8(a_pi8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8(pVCpu, (a_pi8)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else /* IEM_WITH_SETJMP */
#  define IEM_OPCODE_GET_NEXT_S8(a_pi8) (*(a_pi8) = (int8_t)iemOpcodeGetNextU8Jmp(pVCpu))

# endif /* IEM_WITH_SETJMP */


# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 16-bit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu16                Where to return the unsigned word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU16(PVMCPUCC pVCpu, uint16_t *pu16) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode >= pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextS8SxU16Slow(pVCpu, pu16);

    *pu16 = (uint16_t)(int16_t)(int8_t)pVCpu->iem.s.abOpcode[offOpcode];
    pVCpu->iem.s.offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu16              Where to return the word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S8_SX_U16(a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU16(pVCpu, (a_pu16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S8_SX_U16(a_pu16) (*(a_pu16) = (uint16_t)(int16_t)(int8_t)iemOpcodeGetNextU8Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 32-bit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu32                Where to return the unsigned dword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU32(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode >= pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextS8SxU32Slow(pVCpu, pu32);

    *pu32 = (uint32_t)(int32_t)(int8_t)pVCpu->iem.s.abOpcode[offOpcode];
    pVCpu->iem.s.offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu32              Where to return the word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S8_SX_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU32(pVCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S8_SX_U32(a_pu32) (*(a_pu32) = (uint32_t)(int32_t)(int8_t)iemOpcodeGetNextU8Jmp(pVCpu))
# endif


# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 64-bit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu64                Where to return the unsigned qword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU64(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode >= pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextS8SxU64Slow(pVCpu, pu64);

    *pu64 = (uint64_t)(int64_t)(int8_t)pVCpu->iem.s.abOpcode[offOpcode];
    pVCpu->iem.s.offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu64              Where to return the word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S8_SX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU64(pVCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S8_SX_U64(a_pu64) (*(a_pu64) = (uint64_t)(int64_t)(int8_t)iemOpcodeGetNextU8Jmp(pVCpu))
# endif


# ifndef IEM_WITH_SETJMP

/**
 * Fetches the next opcode word.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu16                Where to return the opcode word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16(PVMCPUCC pVCpu, uint16_t *pu16) RT_NOEXCEPT
{
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 2 <= pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 2;
#  ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        *pu16 = *(uint16_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#  else
        *pu16 = RT_MAKE_U16(pVCpu->iem.s.abOpcode[offOpcode], pVCpu->iem.s.abOpcode[offOpcode + 1]);
#  endif
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU16Slow(pVCpu, pu16);
}

# else  /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode word, longjmp on error.
 *
 * @returns The opcode word.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECL_INLINE_THROW(uint16_t) iemOpcodeGetNextU16Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  ifdef IEM_WITH_CODE_TLB
    uint16_t        u16Ret;
    uintptr_t       offBuf = pVCpu->iem.s.offInstrNextByte;
    uint8_t const  *pbBuf  = pVCpu->iem.s.pbInstrBuf;
    if (RT_LIKELY(   pbBuf != NULL
                  && offBuf + 2 <= pVCpu->iem.s.cbInstrBuf))
    {
        pVCpu->iem.s.offInstrNextByte = (uint32_t)offBuf + 2;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        u16Ret = *(uint16_t const *)&pbBuf[offBuf];
#   else
        u16Ret = RT_MAKE_U16(pbBuf[offBuf], pbBuf[offBuf + 1]);
#   endif
    }
    else
        u16Ret = iemOpcodeGetNextU16SlowJmp(pVCpu);

#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    Assert(offOpcode + 1 < sizeof(pVCpu->iem.s.abOpcode));
#    ifdef IEM_USE_UNALIGNED_DATA_ACCESS
    *(uint16_t *)&pVCpu->iem.s.abOpcode[offOpcode] = u16Ret;
#    else
    pVCpu->iem.s.abOpcode[offOpcode]     = RT_LO_U8(u16Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 1] = RT_HI_U8(u16Ret);
#    endif
    pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + (uint8_t)2;
#   endif

    return u16Ret;

#  else /* !IEM_WITH_CODE_TLB */
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 2 <= pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 2;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        return *(uint16_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#   else
        return RT_MAKE_U16(pVCpu->iem.s.abOpcode[offOpcode], pVCpu->iem.s.abOpcode[offOpcode + 1]);
#   endif
    }
    return iemOpcodeGetNextU16SlowJmp(pVCpu);
#  endif /* !IEM_WITH_CODE_TLB */
}

# endif /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode word, returns automatically on failure.
 *
 * @param   a_pu16              Where to return the opcode word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U16(a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16(pVCpu, (a_pu16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U16(a_pu16) (*(a_pu16) = iemOpcodeGetNextU16Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next opcode word, zero extending it to a double word.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu32                Where to return the opcode double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16ZxU32(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode + 2 > pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextU16ZxU32Slow(pVCpu, pu32);

    *pu32 = RT_MAKE_U16(pVCpu->iem.s.abOpcode[offOpcode], pVCpu->iem.s.abOpcode[offOpcode + 1]);
    pVCpu->iem.s.offOpcode = offOpcode + 2;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next opcode word and zero extends it to a double word, returns
 * automatically on failure.
 *
 * @param   a_pu32              Where to return the opcode double word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U16_ZX_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16ZxU32(pVCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U16_ZX_U32(a_pu32) (*(a_pu32) = iemOpcodeGetNextU16Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next opcode word, zero extending it to a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16ZxU64(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode + 2 > pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextU16ZxU64Slow(pVCpu, pu64);

    *pu64 = RT_MAKE_U16(pVCpu->iem.s.abOpcode[offOpcode], pVCpu->iem.s.abOpcode[offOpcode + 1]);
    pVCpu->iem.s.offOpcode = offOpcode + 2;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next opcode word and zero extends it to a quad word, returns
 * automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U16_ZX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16ZxU64(pVCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U16_ZX_U64(a_pu64)  (*(a_pu64) = iemOpcodeGetNextU16Jmp(pVCpu))
# endif


# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed word from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pi16                Where to return the signed word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS16(PVMCPUCC pVCpu, int16_t *pi16) RT_NOEXCEPT
{
    return iemOpcodeGetNextU16(pVCpu, (uint16_t *)pi16);
}
# endif /* !IEM_WITH_SETJMP */


/**
 * Fetches the next signed word from the opcode stream, returning automatically
 * on failure.
 *
 * @param   a_pi16              Where to return the signed word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S16(a_pi16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS16(pVCpu, (a_pi16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S16(a_pi16) (*(a_pi16) = (int16_t)iemOpcodeGetNextU16Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP

/**
 * Fetches the next opcode dword.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu32                Where to return the opcode double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU32(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT
{
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 4 <= pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 4;
#  ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        *pu32 = *(uint32_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#  else
        *pu32 = RT_MAKE_U32_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                    pVCpu->iem.s.abOpcode[offOpcode + 1],
                                    pVCpu->iem.s.abOpcode[offOpcode + 2],
                                    pVCpu->iem.s.abOpcode[offOpcode + 3]);
#  endif
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU32Slow(pVCpu, pu32);
}

# else  /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode dword, longjmp on error.
 *
 * @returns The opcode dword.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECL_INLINE_THROW(uint32_t) iemOpcodeGetNextU32Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  ifdef IEM_WITH_CODE_TLB
    uint32_t u32Ret;
    uintptr_t       offBuf = pVCpu->iem.s.offInstrNextByte;
    uint8_t const  *pbBuf  = pVCpu->iem.s.pbInstrBuf;
    if (RT_LIKELY(   pbBuf != NULL
                  && offBuf + 4 <= pVCpu->iem.s.cbInstrBuf))
    {
        pVCpu->iem.s.offInstrNextByte = (uint32_t)offBuf + 4;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        u32Ret = *(uint32_t const *)&pbBuf[offBuf];
#   else
        u32Ret = RT_MAKE_U32_FROM_U8(pbBuf[offBuf],
                                     pbBuf[offBuf + 1],
                                     pbBuf[offBuf + 2],
                                     pbBuf[offBuf + 3]);
#   endif
    }
    else
        u32Ret = iemOpcodeGetNextU32SlowJmp(pVCpu);

#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    Assert(offOpcode + 3 < sizeof(pVCpu->iem.s.abOpcode));
#    ifdef IEM_USE_UNALIGNED_DATA_ACCESS
    *(uint32_t *)&pVCpu->iem.s.abOpcode[offOpcode] = u32Ret;
#    else
    pVCpu->iem.s.abOpcode[offOpcode]     = RT_BYTE1(u32Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 1] = RT_BYTE2(u32Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 2] = RT_BYTE3(u32Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 3] = RT_BYTE4(u32Ret);
#    endif
    pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + (uint8_t)4;
#   endif /* IEM_WITH_CODE_TLB_AND_OPCODE_BUF */

    return u32Ret;

#  else  /* !IEM_WITH_CODE_TLB */
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 4 <= pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 4;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        return *(uint32_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#   else
        return RT_MAKE_U32_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                   pVCpu->iem.s.abOpcode[offOpcode + 1],
                                   pVCpu->iem.s.abOpcode[offOpcode + 2],
                                   pVCpu->iem.s.abOpcode[offOpcode + 3]);
#   endif
    }
    return iemOpcodeGetNextU32SlowJmp(pVCpu);
#  endif
}

# endif /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode dword, returns automatically on failure.
 *
 * @param   a_pu32              Where to return the opcode dword.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU32(pVCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U32(a_pu32) (*(a_pu32) = iemOpcodeGetNextU32Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next opcode dword, zero extending it to a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU32ZxU64(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode + 4 > pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextU32ZxU64Slow(pVCpu, pu64);

    *pu64 = RT_MAKE_U32_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                pVCpu->iem.s.abOpcode[offOpcode + 1],
                                pVCpu->iem.s.abOpcode[offOpcode + 2],
                                pVCpu->iem.s.abOpcode[offOpcode + 3]);
    pVCpu->iem.s.offOpcode = offOpcode + 4;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next opcode dword and zero extends it to a quad word, returns
 * automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U32_ZX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU32ZxU64(pVCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U32_ZX_U64(a_pu64) (*(a_pu64) = iemOpcodeGetNextU32Jmp(pVCpu))
# endif


# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next signed double word from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pi32                Where to return the signed double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS32(PVMCPUCC pVCpu, int32_t *pi32) RT_NOEXCEPT
{
    return iemOpcodeGetNextU32(pVCpu, (uint32_t *)pi32);
}
# endif

/**
 * Fetches the next signed double word from the opcode stream, returning
 * automatically on failure.
 *
 * @param   a_pi32              Where to return the signed double word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S32(a_pi32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS32(pVCpu, (a_pi32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S32(a_pi32)    (*(a_pi32) = (int32_t)iemOpcodeGetNextU32Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP
/**
 * Fetches the next opcode dword, sign extending it into a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS32SxU64(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT
{
    uint8_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_UNLIKELY(offOpcode + 4 > pVCpu->iem.s.cbOpcode))
        return iemOpcodeGetNextS32SxU64Slow(pVCpu, pu64);

    int32_t i32 = RT_MAKE_U32_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                      pVCpu->iem.s.abOpcode[offOpcode + 1],
                                      pVCpu->iem.s.abOpcode[offOpcode + 2],
                                      pVCpu->iem.s.abOpcode[offOpcode + 3]);
    *pu64 = (uint64_t)(int64_t)i32;
    pVCpu->iem.s.offOpcode = offOpcode + 4;
    return VINF_SUCCESS;
}
# endif /* !IEM_WITH_SETJMP */

/**
 * Fetches the next opcode double word and sign extends it to a quad word,
 * returns automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_S32_SX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS32SxU64(pVCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_S32_SX_U64(a_pu64) (*(a_pu64) = (uint64_t)(int64_t)(int32_t)iemOpcodeGetNextU32Jmp(pVCpu))
# endif

# ifndef IEM_WITH_SETJMP

/**
 * Fetches the next opcode qword.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pu64                Where to return the opcode qword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU64(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT
{
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 8 <= pVCpu->iem.s.cbOpcode))
    {
#  ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        *pu64 = *(uint64_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#  else
        *pu64 = RT_MAKE_U64_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                    pVCpu->iem.s.abOpcode[offOpcode + 1],
                                    pVCpu->iem.s.abOpcode[offOpcode + 2],
                                    pVCpu->iem.s.abOpcode[offOpcode + 3],
                                    pVCpu->iem.s.abOpcode[offOpcode + 4],
                                    pVCpu->iem.s.abOpcode[offOpcode + 5],
                                    pVCpu->iem.s.abOpcode[offOpcode + 6],
                                    pVCpu->iem.s.abOpcode[offOpcode + 7]);
#  endif
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 8;
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU64Slow(pVCpu, pu64);
}

# else  /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode qword, longjmp on error.
 *
 * @returns The opcode qword.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECL_INLINE_THROW(uint64_t) iemOpcodeGetNextU64Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP
{
#  ifdef IEM_WITH_CODE_TLB
    uint64_t        u64Ret;
    uintptr_t       offBuf = pVCpu->iem.s.offInstrNextByte;
    uint8_t const  *pbBuf  = pVCpu->iem.s.pbInstrBuf;
    if (RT_LIKELY(   pbBuf != NULL
                  && offBuf + 8 <= pVCpu->iem.s.cbInstrBuf))
    {
        pVCpu->iem.s.offInstrNextByte = (uint32_t)offBuf + 8;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        u64Ret = *(uint64_t const *)&pbBuf[offBuf];
#   else
        u64Ret = RT_MAKE_U64_FROM_U8(pbBuf[offBuf],
                                     pbBuf[offBuf + 1],
                                     pbBuf[offBuf + 2],
                                     pbBuf[offBuf + 3],
                                     pbBuf[offBuf + 4],
                                     pbBuf[offBuf + 5],
                                     pbBuf[offBuf + 6],
                                     pbBuf[offBuf + 7]);
#   endif
    }
    else
        u64Ret = iemOpcodeGetNextU64SlowJmp(pVCpu);

#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    Assert(offOpcode + 7 < sizeof(pVCpu->iem.s.abOpcode));
#    ifdef IEM_USE_UNALIGNED_DATA_ACCESS
    *(uint64_t *)&pVCpu->iem.s.abOpcode[offOpcode] = u64Ret;
#    else
    pVCpu->iem.s.abOpcode[offOpcode]     = RT_BYTE1(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 1] = RT_BYTE2(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 2] = RT_BYTE3(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 3] = RT_BYTE4(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 4] = RT_BYTE5(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 5] = RT_BYTE6(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 6] = RT_BYTE7(u64Ret);
    pVCpu->iem.s.abOpcode[offOpcode + 7] = RT_BYTE8(u64Ret);
#    endif
    pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + (uint8_t)8;
#   endif /* IEM_WITH_CODE_TLB_AND_OPCODE_BUF */

    return u64Ret;

#  else /* !IEM_WITH_CODE_TLB */
    uintptr_t const offOpcode = pVCpu->iem.s.offOpcode;
    if (RT_LIKELY((uint8_t)offOpcode + 8 <= pVCpu->iem.s.cbOpcode))
    {
        pVCpu->iem.s.offOpcode = (uint8_t)offOpcode + 8;
#   ifdef IEM_USE_UNALIGNED_DATA_ACCESS
        return *(uint64_t const *)&pVCpu->iem.s.abOpcode[offOpcode];
#   else
        return RT_MAKE_U64_FROM_U8(pVCpu->iem.s.abOpcode[offOpcode],
                                   pVCpu->iem.s.abOpcode[offOpcode + 1],
                                   pVCpu->iem.s.abOpcode[offOpcode + 2],
                                   pVCpu->iem.s.abOpcode[offOpcode + 3],
                                   pVCpu->iem.s.abOpcode[offOpcode + 4],
                                   pVCpu->iem.s.abOpcode[offOpcode + 5],
                                   pVCpu->iem.s.abOpcode[offOpcode + 6],
                                   pVCpu->iem.s.abOpcode[offOpcode + 7]);
#   endif
    }
    return iemOpcodeGetNextU64SlowJmp(pVCpu);
#  endif /* !IEM_WITH_CODE_TLB */
}

# endif /* IEM_WITH_SETJMP */

/**
 * Fetches the next opcode quad word, returns automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pVCpu.
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_GET_NEXT_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU64(pVCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)
# else
#  define IEM_OPCODE_GET_NEXT_U64(a_pu64)    ( *(a_pu64) = iemOpcodeGetNextU64Jmp(pVCpu) )
# endif

/**
 * For fetching the opcode bytes for an ModR/M effective address, but throw
 * away the result.
 *
 * This is used when decoding undefined opcodes and such where we want to avoid
 * unnecessary MC blocks.
 *
 * @note The recompiler code overrides this one so iemOpHlpCalcRmEffAddrJmpEx is
 *       used instead.  At least for now...
 */
# ifndef IEM_WITH_SETJMP
#  define IEM_OPCODE_SKIP_RM_EFF_ADDR_BYTES(a_bRm) do { \
        RTGCPTR      GCPtrEff; \
        VBOXSTRICTRC rcStrict = iemOpHlpCalcRmEffAddr(pVCpu, bRm, 0, &GCPtrEff); \
        if (rcStrict != VINF_SUCCESS) \
            return rcStrict; \
    } while (0)
# else
#  define IEM_OPCODE_SKIP_RM_EFF_ADDR_BYTES(a_bRm) do { \
        (void)iemOpHlpCalcRmEffAddrJmp(pVCpu, bRm, 0); \
    } while (0)
# endif

#endif /* !IEM_WITH_OPAQUE_DECODER_STATE */


#ifndef IEM_WITH_OPAQUE_DECODER_STATE

/**
 * Recalculates the effective operand size.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemRecalEffOpSize(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    switch (IEM_GET_CPU_MODE(pVCpu))
    {
        case IEMMODE_16BIT:
            pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP ? IEMMODE_32BIT : IEMMODE_16BIT;
            break;
        case IEMMODE_32BIT:
            pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.fPrefixes & IEM_OP_PRF_SIZE_OP ? IEMMODE_16BIT : IEMMODE_32BIT;
            break;
        case IEMMODE_64BIT:
            switch (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP))
            {
                case 0:
                    pVCpu->iem.s.enmEffOpSize = pVCpu->iem.s.enmDefOpSize;
                    break;
                case IEM_OP_PRF_SIZE_OP:
                    pVCpu->iem.s.enmEffOpSize = IEMMODE_16BIT;
                    break;
                case IEM_OP_PRF_SIZE_REX_W:
                case IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP:
                    pVCpu->iem.s.enmEffOpSize = IEMMODE_64BIT;
                    break;
            }
            break;
        default:
            AssertFailed();
    }
}


/**
 * Sets the default operand size to 64-bit and recalculates the effective
 * operand size.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemRecalEffOpSize64Default(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
    if ((pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP)) != IEM_OP_PRF_SIZE_OP)
        pVCpu->iem.s.enmEffOpSize = IEMMODE_64BIT;
    else
        pVCpu->iem.s.enmEffOpSize = IEMMODE_16BIT;
}


/**
 * Sets the default operand size to 64-bit and recalculates the effective
 * operand size, with intel ignoring any operand size prefix (AMD respects it).
 *
 * This is for the relative jumps.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemRecalEffOpSize64DefaultAndIntelIgnoresOpSizePrefix(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    pVCpu->iem.s.enmDefOpSize = IEMMODE_64BIT;
    if (   (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP)) != IEM_OP_PRF_SIZE_OP
        || pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL)
        pVCpu->iem.s.enmEffOpSize = IEMMODE_64BIT;
    else
        pVCpu->iem.s.enmEffOpSize = IEMMODE_16BIT;
}

#endif /* !IEM_WITH_OPAQUE_DECODER_STATE */


#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineDecode_x86_h */
