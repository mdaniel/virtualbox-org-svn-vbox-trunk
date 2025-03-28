/* $Id$ */
/** @file
 * IEM - Internal header file, x86 target specifics.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInternal_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInternal_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


RT_C_DECLS_BEGIN


/** @defgroup grp_iem_int_x86   X86 Target Internals
 * @ingroup grp_iem_int
 * @internal
 * @{
 */

VBOXSTRICTRC iemOpcodeFetchPrefetch(PVMCPUCC pVCpu) RT_NOEXCEPT;


/** @name Prefix constants (IEMCPU::fPrefixes)
 * @note x86 specific
 * @{ */
#define IEM_OP_PRF_SEG_CS               RT_BIT_32(0)  /**< CS segment prefix (0x2e). */
#define IEM_OP_PRF_SEG_SS               RT_BIT_32(1)  /**< SS segment prefix (0x36). */
#define IEM_OP_PRF_SEG_DS               RT_BIT_32(2)  /**< DS segment prefix (0x3e). */
#define IEM_OP_PRF_SEG_ES               RT_BIT_32(3)  /**< ES segment prefix (0x26). */
#define IEM_OP_PRF_SEG_FS               RT_BIT_32(4)  /**< FS segment prefix (0x64). */
#define IEM_OP_PRF_SEG_GS               RT_BIT_32(5)  /**< GS segment prefix (0x65). */
#define IEM_OP_PRF_SEG_MASK             UINT32_C(0x3f)

#define IEM_OP_PRF_SIZE_OP              RT_BIT_32(8)  /**< Operand size prefix (0x66). */
#define IEM_OP_PRF_SIZE_REX_W           RT_BIT_32(9)  /**< REX.W prefix (0x48-0x4f). */
#define IEM_OP_PRF_SIZE_ADDR            RT_BIT_32(10) /**< Address size prefix (0x67). */

#define IEM_OP_PRF_LOCK                 RT_BIT_32(16) /**< Lock prefix (0xf0). */
#define IEM_OP_PRF_REPNZ                RT_BIT_32(17) /**< Repeat-not-zero prefix (0xf2). */
#define IEM_OP_PRF_REPZ                 RT_BIT_32(18) /**< Repeat-if-zero prefix (0xf3). */

#define IEM_OP_PRF_REX                  RT_BIT_32(24) /**< Any REX prefix (0x40-0x4f). */
#define IEM_OP_PRF_REX_B                RT_BIT_32(25) /**< REX.B prefix (0x41,0x43,0x45,0x47,0x49,0x4b,0x4d,0x4f). */
#define IEM_OP_PRF_REX_X                RT_BIT_32(26) /**< REX.X prefix (0x42,0x43,0x46,0x47,0x4a,0x4b,0x4e,0x4f). */
#define IEM_OP_PRF_REX_R                RT_BIT_32(27) /**< REX.R prefix (0x44,0x45,0x46,0x47,0x4c,0x4d,0x4e,0x4f). */
/** Mask with all the REX prefix flags.
 * This is generally for use when needing to undo the REX prefixes when they
 * are followed legacy prefixes and therefore does not immediately preceed
 * the first opcode byte.
 * For testing whether any REX prefix is present, use  IEM_OP_PRF_REX instead. */
#define IEM_OP_PRF_REX_MASK  (IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W )

#define IEM_OP_PRF_VEX                  RT_BIT_32(28) /**< Indiciates VEX prefix. */
#define IEM_OP_PRF_EVEX                 RT_BIT_32(29) /**< Indiciates EVEX prefix. */
#define IEM_OP_PRF_XOP                  RT_BIT_32(30) /**< Indiciates XOP prefix. */
/** @} */

/** @name IEMOPFORM_XXX - Opcode forms
 * @note These are ORed together with IEMOPHINT_XXX.
 * @note x86 specific
 * @{ */
/** ModR/M: reg, r/m */
#define IEMOPFORM_RM            0
/** ModR/M: reg, r/m (register) */
#define IEMOPFORM_RM_REG        (IEMOPFORM_RM | IEMOPFORM_MOD3)
/** ModR/M: reg, r/m (memory)   */
#define IEMOPFORM_RM_MEM        (IEMOPFORM_RM | IEMOPFORM_NOT_MOD3)
/** ModR/M: reg, r/m, imm */
#define IEMOPFORM_RMI           1
/** ModR/M: reg, r/m (register), imm */
#define IEMOPFORM_RMI_REG       (IEMOPFORM_RMI | IEMOPFORM_MOD3)
/** ModR/M: reg, r/m (memory), imm   */
#define IEMOPFORM_RMI_MEM       (IEMOPFORM_RMI | IEMOPFORM_NOT_MOD3)
/** ModR/M: reg, r/m, xmm0 */
#define IEMOPFORM_RM0           2
/** ModR/M: reg, r/m (register), xmm0 */
#define IEMOPFORM_RM0_REG       (IEMOPFORM_RM0 | IEMOPFORM_MOD3)
/** ModR/M: reg, r/m (memory), xmm0   */
#define IEMOPFORM_RM0_MEM       (IEMOPFORM_RM0 | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, reg */
#define IEMOPFORM_MR            3
/** ModR/M: r/m (register), reg */
#define IEMOPFORM_MR_REG        (IEMOPFORM_MR | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), reg */
#define IEMOPFORM_MR_MEM        (IEMOPFORM_MR | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, reg, imm */
#define IEMOPFORM_MRI           4
/** ModR/M: r/m (register), reg, imm */
#define IEMOPFORM_MRI_REG       (IEMOPFORM_MRI | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), reg, imm */
#define IEMOPFORM_MRI_MEM       (IEMOPFORM_MRI | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m only */
#define IEMOPFORM_M             5
/** ModR/M: r/m only (register). */
#define IEMOPFORM_M_REG         (IEMOPFORM_M | IEMOPFORM_MOD3)
/** ModR/M: r/m only (memory). */
#define IEMOPFORM_M_MEM         (IEMOPFORM_M | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, imm */
#define IEMOPFORM_MI            6
/** ModR/M: r/m (register), imm */
#define IEMOPFORM_MI_REG        (IEMOPFORM_MI | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), imm */
#define IEMOPFORM_MI_MEM        (IEMOPFORM_MI | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, 1  (shift and rotate instructions) */
#define IEMOPFORM_M1            7
/** ModR/M: r/m (register), 1. */
#define IEMOPFORM_M1_REG        (IEMOPFORM_M1 | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), 1. */
#define IEMOPFORM_M1_MEM        (IEMOPFORM_M1 | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, CL  (shift and rotate instructions)
 * @todo This should just've been a generic fixed register. But the python
 *       code doesn't needs more convincing. */
#define IEMOPFORM_M_CL          8
/** ModR/M: r/m (register), CL. */
#define IEMOPFORM_M_CL_REG      (IEMOPFORM_M_CL | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), CL. */
#define IEMOPFORM_M_CL_MEM      (IEMOPFORM_M_CL | IEMOPFORM_NOT_MOD3)
/** ModR/M: reg only */
#define IEMOPFORM_R             9

/** VEX+ModR/M: reg, r/m */
#define IEMOPFORM_VEX_RM        16
/** VEX+ModR/M: reg, r/m (register) */
#define IEMOPFORM_VEX_RM_REG    (IEMOPFORM_VEX_RM | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m (memory)   */
#define IEMOPFORM_VEX_RM_MEM    (IEMOPFORM_VEX_RM | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m, reg */
#define IEMOPFORM_VEX_MR        17
/** VEX+ModR/M: r/m (register), reg */
#define IEMOPFORM_VEX_MR_REG    (IEMOPFORM_VEX_MR | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m (memory), reg */
#define IEMOPFORM_VEX_MR_MEM    (IEMOPFORM_VEX_MR | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m, reg, imm8 */
#define IEMOPFORM_VEX_MRI       18
/** VEX+ModR/M: r/m (register), reg, imm8 */
#define IEMOPFORM_VEX_MRI_REG   (IEMOPFORM_VEX_MRI | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m (memory), reg, imm8 */
#define IEMOPFORM_VEX_MRI_MEM   (IEMOPFORM_VEX_MRI | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m only */
#define IEMOPFORM_VEX_M         19
/** VEX+ModR/M: r/m only (register). */
#define IEMOPFORM_VEX_M_REG     (IEMOPFORM_VEX_M | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m only (memory). */
#define IEMOPFORM_VEX_M_MEM     (IEMOPFORM_VEX_M | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg only */
#define IEMOPFORM_VEX_R         20
/** VEX+ModR/M: reg, vvvv, r/m */
#define IEMOPFORM_VEX_RVM       21
/** VEX+ModR/M: reg, vvvv, r/m (register). */
#define IEMOPFORM_VEX_RVM_REG   (IEMOPFORM_VEX_RVM | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m (memory). */
#define IEMOPFORM_VEX_RVM_MEM   (IEMOPFORM_VEX_RVM | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m, imm */
#define IEMOPFORM_VEX_RVMI      22
/** VEX+ModR/M: reg, vvvv, r/m (register), imm. */
#define IEMOPFORM_VEX_RVMI_REG  (IEMOPFORM_VEX_RVMI | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m (memory), imm. */
#define IEMOPFORM_VEX_RVMI_MEM  (IEMOPFORM_VEX_RVMI | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m, imm(reg) */
#define IEMOPFORM_VEX_RVMR      23
/** VEX+ModR/M: reg, vvvv, r/m (register), imm(reg). */
#define IEMOPFORM_VEX_RVMR_REG  (IEMOPFORM_VEX_RVMI | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m (memory), imm(reg). */
#define IEMOPFORM_VEX_RVMR_MEM  (IEMOPFORM_VEX_RVMI | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, r/m, vvvv */
#define IEMOPFORM_VEX_RMV       24
/** VEX+ModR/M: reg, r/m, vvvv (register). */
#define IEMOPFORM_VEX_RMV_REG   (IEMOPFORM_VEX_RMV | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m, vvvv (memory). */
#define IEMOPFORM_VEX_RMV_MEM   (IEMOPFORM_VEX_RMV | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, r/m, imm8 */
#define IEMOPFORM_VEX_RMI       25
/** VEX+ModR/M: reg, r/m, imm8 (register). */
#define IEMOPFORM_VEX_RMI_REG   (IEMOPFORM_VEX_RMI | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m, imm8 (memory). */
#define IEMOPFORM_VEX_RMI_MEM   (IEMOPFORM_VEX_RMI | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m, vvvv, reg */
#define IEMOPFORM_VEX_MVR       26
/** VEX+ModR/M: r/m, vvvv, reg (register) */
#define IEMOPFORM_VEX_MVR_REG   (IEMOPFORM_VEX_MVR | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m, vvvv, reg (memory) */
#define IEMOPFORM_VEX_MVR_MEM   (IEMOPFORM_VEX_MVR | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m */
#define IEMOPFORM_VEX_VM        27
/** VEX+ModR/M+/n: vvvv, r/m (register) */
#define IEMOPFORM_VEX_VM_REG    (IEMOPFORM_VEX_VM | IEMOPFORM_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m (memory) */
#define IEMOPFORM_VEX_VM_MEM    (IEMOPFORM_VEX_VM | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m, imm8 */
#define IEMOPFORM_VEX_VMI       28
/** VEX+ModR/M+/n: vvvv, r/m, imm8 (register) */
#define IEMOPFORM_VEX_VMI_REG   (IEMOPFORM_VEX_VMI | IEMOPFORM_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m, imm8 (memory) */
#define IEMOPFORM_VEX_VMI_MEM   (IEMOPFORM_VEX_VMI | IEMOPFORM_NOT_MOD3)

/** Fixed register instruction, no R/M. */
#define IEMOPFORM_FIXED         32

/** The r/m is a register. */
#define IEMOPFORM_MOD3          RT_BIT_32(8)
/** The r/m is a memory access. */
#define IEMOPFORM_NOT_MOD3      RT_BIT_32(9)
/** @} */

/** @name IEMOPHINT_XXX - Additional Opcode Hints
 * @note These are ORed together with IEMOPFORM_XXX.
 * @note x86 specific
 * @{ */
/** Ignores the operand size prefix (66h). */
#define IEMOPHINT_IGNORES_OZ_PFX    RT_BIT_32(10)
/** Ignores REX.W (aka WIG). */
#define IEMOPHINT_IGNORES_REXW      RT_BIT_32(11)
/** Both the operand size prefixes (66h + REX.W) are ignored. */
#define IEMOPHINT_IGNORES_OP_SIZES  (IEMOPHINT_IGNORES_OZ_PFX | IEMOPHINT_IGNORES_REXW)
/** Allowed with the lock prefix. */
#define IEMOPHINT_LOCK_ALLOWED      RT_BIT_32(11)
/** The VEX.L value is ignored (aka LIG). */
#define IEMOPHINT_VEX_L_IGNORED     RT_BIT_32(12)
/** The VEX.L value must be zero (i.e. 128-bit width only). */
#define IEMOPHINT_VEX_L_ZERO        RT_BIT_32(13)
/** The VEX.L value must be one (i.e. 256-bit width only). */
#define IEMOPHINT_VEX_L_ONE         RT_BIT_32(14)
/** The VEX.V value must be zero. */
#define IEMOPHINT_VEX_V_ZERO        RT_BIT_32(15)
/** The REX.W/VEX.V value must be zero. */
#define IEMOPHINT_REX_W_ZERO        RT_BIT_32(16)
#define IEMOPHINT_VEX_W_ZERO        IEMOPHINT_REX_W_ZERO
/** The REX.W/VEX.V value must be one. */
#define IEMOPHINT_REX_W_ONE         RT_BIT_32(17)
#define IEMOPHINT_VEX_W_ONE         IEMOPHINT_REX_W_ONE

/** Hint to IEMAllInstructionPython.py that this macro should be skipped.  */
#define IEMOPHINT_SKIP_PYTHON       RT_BIT_32(31)
/** @} */

/**
 * Possible hardware task switch sources - iemTaskSwitch(), iemVmxVmexitTaskSwitch().
 * @note x86 specific
 */
typedef enum IEMTASKSWITCH
{
    /** Task switch caused by an interrupt/exception. */
    IEMTASKSWITCH_INT_XCPT = 1,
    /** Task switch caused by a far CALL. */
    IEMTASKSWITCH_CALL,
    /** Task switch caused by a far JMP. */
    IEMTASKSWITCH_JUMP,
    /** Task switch caused by an IRET. */
    IEMTASKSWITCH_IRET
} IEMTASKSWITCH;
AssertCompileSize(IEMTASKSWITCH, 4);

/**
 * Possible CrX load (write) sources - iemCImpl_load_CrX().
 * @note x86 specific
 */
typedef enum IEMACCESSCRX
{
    /** CrX access caused by 'mov crX' instruction. */
    IEMACCESSCRX_MOV_CRX,
    /** CrX (CR0) write caused by 'lmsw' instruction. */
    IEMACCESSCRX_LMSW,
    /** CrX (CR0) write caused by 'clts' instruction. */
    IEMACCESSCRX_CLTS,
    /** CrX (CR0) read caused by 'smsw' instruction. */
    IEMACCESSCRX_SMSW
} IEMACCESSCRX;

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/** @name IEM_SLAT_FAIL_XXX - Second-level address translation failure information.
 *
 * These flags provide further context to SLAT page-walk failures that could not be
 * determined by PGM (e.g, PGM is not privy to memory access permissions).
 *
 * @{
 */
/** Translating a nested-guest linear address failed accessing a nested-guest
 *  physical address. */
# define IEM_SLAT_FAIL_LINEAR_TO_PHYS_ADDR          RT_BIT_32(0)
/** Translating a nested-guest linear address failed accessing a
 *  paging-structure entry or updating accessed/dirty bits. */
# define IEM_SLAT_FAIL_LINEAR_TO_PAGE_TABLE         RT_BIT_32(1)
/** @} */

DECLCALLBACK(FNPGMPHYSHANDLER)      iemVmxApicAccessPageHandler;
# ifndef IN_RING3
DECLCALLBACK(FNPGMRZPHYSPFHANDLER)  iemVmxApicAccessPagePfHandler;
# endif
#endif

/**
 * Indicates to the verifier that the given flag set is undefined.
 *
 * Can be invoked again to add more flags.
 *
 * This is a NOOP if the verifier isn't compiled in.
 *
 * @note We're temporarily keeping this until code is converted to new
 *       disassembler style opcode handling.
 */
#define IEMOP_VERIFICATION_UNDEFINED_EFLAGS(a_fEfl) do { } while (0)


/** Defined in IEMAllAImplC.cpp but also used by IEMAllAImplA.asm. */
RT_C_DECLS_BEGIN
extern uint8_t const g_afParity[256];
RT_C_DECLS_END


/** @name Arithmetic assignment operations on bytes (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINU8, (uint32_t fEFlagsIn, uint8_t  *pu8Dst,  uint8_t  u8Src));
typedef FNIEMAIMPLBINU8  *PFNIEMAIMPLBINU8;
FNIEMAIMPLBINU8 iemAImpl_add_u8, iemAImpl_add_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_adc_u8, iemAImpl_adc_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_sub_u8, iemAImpl_sub_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_sbb_u8, iemAImpl_sbb_u8_locked;
FNIEMAIMPLBINU8  iemAImpl_or_u8,  iemAImpl_or_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_xor_u8, iemAImpl_xor_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_and_u8, iemAImpl_and_u8_locked;
/** @} */

/** @name Arithmetic assignment operations on words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINU16, (uint32_t fEFlagsIn, uint16_t *pu16Dst, uint16_t u16Src));
typedef FNIEMAIMPLBINU16  *PFNIEMAIMPLBINU16;
FNIEMAIMPLBINU16 iemAImpl_add_u16, iemAImpl_add_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_adc_u16, iemAImpl_adc_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_sub_u16, iemAImpl_sub_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_sbb_u16, iemAImpl_sbb_u16_locked;
FNIEMAIMPLBINU16  iemAImpl_or_u16,  iemAImpl_or_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_xor_u16, iemAImpl_xor_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_and_u16, iemAImpl_and_u16_locked;
/** @}  */


/** @name Arithmetic assignment operations on double words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINU32, (uint32_t fEFlagsIn, uint32_t *pu32Dst, uint32_t u32Src));
typedef FNIEMAIMPLBINU32 *PFNIEMAIMPLBINU32;
FNIEMAIMPLBINU32 iemAImpl_add_u32, iemAImpl_add_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_adc_u32, iemAImpl_adc_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_sub_u32, iemAImpl_sub_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_sbb_u32, iemAImpl_sbb_u32_locked;
FNIEMAIMPLBINU32  iemAImpl_or_u32,  iemAImpl_or_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_xor_u32, iemAImpl_xor_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_and_u32, iemAImpl_and_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_blsi_u32, iemAImpl_blsi_u32_fallback;
FNIEMAIMPLBINU32 iemAImpl_blsr_u32, iemAImpl_blsr_u32_fallback;
FNIEMAIMPLBINU32 iemAImpl_blsmsk_u32, iemAImpl_blsmsk_u32_fallback;
/** @}  */

/** @name Arithmetic assignment operations on quad words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINU64, (uint32_t fEFlagsIn, uint64_t *pu64Dst, uint64_t u64Src));
typedef FNIEMAIMPLBINU64 *PFNIEMAIMPLBINU64;
FNIEMAIMPLBINU64 iemAImpl_add_u64, iemAImpl_add_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_adc_u64, iemAImpl_adc_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_sub_u64, iemAImpl_sub_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_sbb_u64, iemAImpl_sbb_u64_locked;
FNIEMAIMPLBINU64  iemAImpl_or_u64,  iemAImpl_or_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_xor_u64, iemAImpl_xor_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_and_u64, iemAImpl_and_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_blsi_u64, iemAImpl_blsi_u64_fallback;
FNIEMAIMPLBINU64 iemAImpl_blsr_u64, iemAImpl_blsr_u64_fallback;
FNIEMAIMPLBINU64 iemAImpl_blsmsk_u64, iemAImpl_blsmsk_u64_fallback;
/** @}  */

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINROU8, (uint32_t fEFlagsIn, uint8_t const *pu8Dst, uint8_t u8Src));
typedef FNIEMAIMPLBINROU8 *PFNIEMAIMPLBINROU8;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINROU16,(uint32_t fEFlagsIn, uint16_t const *pu16Dst, uint16_t u16Src));
typedef FNIEMAIMPLBINROU16 *PFNIEMAIMPLBINROU16;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINROU32,(uint32_t fEFlagsIn, uint32_t const *pu32Dst, uint32_t u32Src));
typedef FNIEMAIMPLBINROU32 *PFNIEMAIMPLBINROU32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLBINROU64,(uint32_t fEFlagsIn, uint64_t const *pu64Dst, uint64_t u64Src));
typedef FNIEMAIMPLBINROU64 *PFNIEMAIMPLBINROU64;

/** @name Compare operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINROU8  iemAImpl_cmp_u8;
FNIEMAIMPLBINROU16 iemAImpl_cmp_u16;
FNIEMAIMPLBINROU32 iemAImpl_cmp_u32;
FNIEMAIMPLBINROU64 iemAImpl_cmp_u64;
/** @}  */

/** @name Test operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINROU8  iemAImpl_test_u8;
FNIEMAIMPLBINROU16 iemAImpl_test_u16;
FNIEMAIMPLBINROU32 iemAImpl_test_u32;
FNIEMAIMPLBINROU64 iemAImpl_test_u64;
/** @}  */

/** @name Bit operations operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINROU16 iemAImpl_bt_u16;
FNIEMAIMPLBINROU32 iemAImpl_bt_u32;
FNIEMAIMPLBINROU64 iemAImpl_bt_u64;
FNIEMAIMPLBINU16 iemAImpl_btc_u16, iemAImpl_btc_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_btc_u32, iemAImpl_btc_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_btc_u64, iemAImpl_btc_u64_locked;
FNIEMAIMPLBINU16 iemAImpl_btr_u16, iemAImpl_btr_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_btr_u32, iemAImpl_btr_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_btr_u64, iemAImpl_btr_u64_locked;
FNIEMAIMPLBINU16 iemAImpl_bts_u16, iemAImpl_bts_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_bts_u32, iemAImpl_bts_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_bts_u64, iemAImpl_bts_u64_locked;
/** @}  */

/** @name Arithmetic three operand operations on double words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU32, (uint32_t *pu32Dst, uint32_t u32Src1, uint32_t u32Src2, uint32_t *pEFlags));
typedef FNIEMAIMPLBINVEXU32 *PFNIEMAIMPLBINVEXU32;
FNIEMAIMPLBINVEXU32 iemAImpl_andn_u32, iemAImpl_andn_u32_fallback;
FNIEMAIMPLBINVEXU32 iemAImpl_bextr_u32, iemAImpl_bextr_u32_fallback;
FNIEMAIMPLBINVEXU32 iemAImpl_bzhi_u32, iemAImpl_bzhi_u32_fallback;
/** @}  */

/** @name Arithmetic three operand operations on quad words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU64, (uint64_t *pu64Dst, uint64_t u64Src1, uint64_t u64Src2, uint32_t *pEFlags));
typedef FNIEMAIMPLBINVEXU64 *PFNIEMAIMPLBINVEXU64;
FNIEMAIMPLBINVEXU64 iemAImpl_andn_u64, iemAImpl_andn_u64_fallback;
FNIEMAIMPLBINVEXU64 iemAImpl_bextr_u64, iemAImpl_bextr_u64_fallback;
FNIEMAIMPLBINVEXU64 iemAImpl_bzhi_u64, iemAImpl_bzhi_u64_fallback;
/** @}  */

/** @name Arithmetic three operand operations on double words w/o EFLAGS (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU32NOEFL, (uint32_t *pu32Dst, uint32_t u32Src1, uint32_t u32Src2));
typedef FNIEMAIMPLBINVEXU32NOEFL *PFNIEMAIMPLBINVEXU32NOEFL;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_pdep_u32, iemAImpl_pdep_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_pext_u32, iemAImpl_pext_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_sarx_u32, iemAImpl_sarx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_shlx_u32, iemAImpl_shlx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_shrx_u32, iemAImpl_shrx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_rorx_u32;
/** @}  */

/** @name Arithmetic three operand operations on quad words w/o EFLAGS (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU64NOEFL, (uint64_t *pu64Dst, uint64_t u64Src1, uint64_t u64Src2));
typedef FNIEMAIMPLBINVEXU64NOEFL *PFNIEMAIMPLBINVEXU64NOEFL;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_pdep_u64, iemAImpl_pdep_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_pext_u64, iemAImpl_pext_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_sarx_u64, iemAImpl_sarx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_shlx_u64, iemAImpl_shlx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_shrx_u64, iemAImpl_shrx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_rorx_u64;
/** @}  */

/** @name MULX 32-bit and 64-bit.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMULXVEXU32, (uint32_t *puDst1, uint32_t *puDst2, uint32_t uSrc1, uint32_t uSrc2));
typedef FNIEMAIMPLMULXVEXU32 *PFNIEMAIMPLMULXVEXU32;
FNIEMAIMPLMULXVEXU32 iemAImpl_mulx_u32, iemAImpl_mulx_u32_fallback;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMULXVEXU64, (uint64_t *puDst1, uint64_t *puDst2, uint64_t uSrc1, uint64_t uSrc2));
typedef FNIEMAIMPLMULXVEXU64 *PFNIEMAIMPLMULXVEXU64;
FNIEMAIMPLMULXVEXU64 iemAImpl_mulx_u64, iemAImpl_mulx_u64_fallback;
/** @}  */


/** @name Exchange memory with register operations.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_locked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_locked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_locked,(uint64_t *pu64Mem, uint64_t *pu64Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_unlocked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_unlocked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_unlocked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_unlocked,(uint64_t *pu64Mem, uint64_t *pu64Reg));
/** @}  */

/** @name Exchange and add operations.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8, (uint8_t  *pu8Dst,  uint8_t  *pu8Reg,  uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16,(uint16_t *pu16Dst, uint16_t *pu16Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32,(uint32_t *pu32Dst, uint32_t *pu32Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64,(uint64_t *pu64Dst, uint64_t *pu64Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8_locked, (uint8_t  *pu8Dst,  uint8_t  *pu8Reg,  uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16_locked,(uint16_t *pu16Dst, uint16_t *pu16Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32_locked,(uint32_t *pu32Dst, uint32_t *pu32Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64_locked,(uint64_t *pu64Dst, uint64_t *pu64Reg, uint32_t *pEFlags));
/** @}  */

/** @name Compare and exchange.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8,        (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8_locked, (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16,       (uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16_locked,(uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32,       (uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32_locked,(uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags));
#if ARCH_BITS == 32
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags));
#else
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags));
#endif
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
                                            uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b_locked,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
                                                   uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                             uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_locked,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                                    uint32_t *pEFlags));
#ifndef RT_ARCH_ARM64
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_fallback,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx,
                                                      PRTUINT128U pu128RbxRcx, uint32_t *pEFlags));
#endif
/** @} */

/** @name Memory ordering
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEMFENCE,(void));
typedef FNIEMAIMPLMEMFENCE *PFNIEMAIMPLMEMFENCE;
IEM_DECL_IMPL_DEF(void, iemAImpl_mfence,(void));
IEM_DECL_IMPL_DEF(void, iemAImpl_sfence,(void));
IEM_DECL_IMPL_DEF(void, iemAImpl_lfence,(void));
#ifndef RT_ARCH_ARM64
IEM_DECL_IMPL_DEF(void, iemAImpl_alt_mem_fence,(void));
#endif
/** @} */

/** @name Double precision shifts
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU16,(uint16_t *pu16Dst, uint16_t u16Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU16  *PFNIEMAIMPLSHIFTDBLU16;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU32,(uint32_t *pu32Dst, uint32_t u32Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU32  *PFNIEMAIMPLSHIFTDBLU32;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU64,(uint64_t *pu64Dst, uint64_t u64Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU64  *PFNIEMAIMPLSHIFTDBLU64;
FNIEMAIMPLSHIFTDBLU16 iemAImpl_shld_u16, iemAImpl_shld_u16_amd, iemAImpl_shld_u16_intel;
FNIEMAIMPLSHIFTDBLU32 iemAImpl_shld_u32, iemAImpl_shld_u32_amd, iemAImpl_shld_u32_intel;
FNIEMAIMPLSHIFTDBLU64 iemAImpl_shld_u64, iemAImpl_shld_u64_amd, iemAImpl_shld_u64_intel;
FNIEMAIMPLSHIFTDBLU16 iemAImpl_shrd_u16, iemAImpl_shrd_u16_amd, iemAImpl_shrd_u16_intel;
FNIEMAIMPLSHIFTDBLU32 iemAImpl_shrd_u32, iemAImpl_shrd_u32_amd, iemAImpl_shrd_u32_intel;
FNIEMAIMPLSHIFTDBLU64 iemAImpl_shrd_u64, iemAImpl_shrd_u64_amd, iemAImpl_shrd_u64_intel;
/** @}  */


/** @name Bit search operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_bsf_u16, iemAImpl_bsf_u16_amd, iemAImpl_bsf_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_bsf_u32, iemAImpl_bsf_u32_amd, iemAImpl_bsf_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_bsf_u64, iemAImpl_bsf_u64_amd, iemAImpl_bsf_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_bsr_u16, iemAImpl_bsr_u16_amd, iemAImpl_bsr_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_bsr_u32, iemAImpl_bsr_u32_amd, iemAImpl_bsr_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_bsr_u64, iemAImpl_bsr_u64_amd, iemAImpl_bsr_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_lzcnt_u16, iemAImpl_lzcnt_u16_amd, iemAImpl_lzcnt_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_lzcnt_u32, iemAImpl_lzcnt_u32_amd, iemAImpl_lzcnt_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_lzcnt_u64, iemAImpl_lzcnt_u64_amd, iemAImpl_lzcnt_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_tzcnt_u16, iemAImpl_tzcnt_u16_amd, iemAImpl_tzcnt_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_tzcnt_u32, iemAImpl_tzcnt_u32_amd, iemAImpl_tzcnt_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_tzcnt_u64, iemAImpl_tzcnt_u64_amd, iemAImpl_tzcnt_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_popcnt_u16, iemAImpl_popcnt_u16_fallback;
FNIEMAIMPLBINU32 iemAImpl_popcnt_u32, iemAImpl_popcnt_u32_fallback;
FNIEMAIMPLBINU64 iemAImpl_popcnt_u64, iemAImpl_popcnt_u64_fallback;
/** @}  */

/** @name Signed multiplication operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_imul_two_u16, iemAImpl_imul_two_u16_amd, iemAImpl_imul_two_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_imul_two_u32, iemAImpl_imul_two_u32_amd, iemAImpl_imul_two_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_imul_two_u64, iemAImpl_imul_two_u64_amd, iemAImpl_imul_two_u64_intel;
/** @}  */

/** @name Arithmetic assignment operations on bytes (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU8,  (uint8_t  *pu8Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU8  *PFNIEMAIMPLUNARYU8;
FNIEMAIMPLUNARYU8 iemAImpl_inc_u8, iemAImpl_inc_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_dec_u8, iemAImpl_dec_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_not_u8, iemAImpl_not_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_neg_u8, iemAImpl_neg_u8_locked;
/** @} */

/** @name Arithmetic assignment operations on words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU16,  (uint16_t  *pu16Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU16  *PFNIEMAIMPLUNARYU16;
FNIEMAIMPLUNARYU16 iemAImpl_inc_u16, iemAImpl_inc_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_dec_u16, iemAImpl_dec_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_not_u16, iemAImpl_not_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_neg_u16, iemAImpl_neg_u16_locked;
/** @} */

/** @name Arithmetic assignment operations on double words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU32,  (uint32_t  *pu32Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU32  *PFNIEMAIMPLUNARYU32;
FNIEMAIMPLUNARYU32 iemAImpl_inc_u32, iemAImpl_inc_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_dec_u32, iemAImpl_dec_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_not_u32, iemAImpl_not_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_neg_u32, iemAImpl_neg_u32_locked;
/** @} */

/** @name Arithmetic assignment operations on quad words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU64,  (uint64_t  *pu64Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU64  *PFNIEMAIMPLUNARYU64;
FNIEMAIMPLUNARYU64 iemAImpl_inc_u64, iemAImpl_inc_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_dec_u64, iemAImpl_dec_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_not_u64, iemAImpl_not_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_neg_u64, iemAImpl_neg_u64_locked;
/** @} */


/** @name Shift operations on bytes (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSHIFTU8,(uint32_t fEFlagsIn, uint8_t *pu8Dst, uint8_t cShift));
typedef FNIEMAIMPLSHIFTU8  *PFNIEMAIMPLSHIFTU8;
FNIEMAIMPLSHIFTU8 iemAImpl_rol_u8, iemAImpl_rol_u8_amd, iemAImpl_rol_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_ror_u8, iemAImpl_ror_u8_amd, iemAImpl_ror_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_rcl_u8, iemAImpl_rcl_u8_amd, iemAImpl_rcl_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_rcr_u8, iemAImpl_rcr_u8_amd, iemAImpl_rcr_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_shl_u8, iemAImpl_shl_u8_amd, iemAImpl_shl_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_shr_u8, iemAImpl_shr_u8_amd, iemAImpl_shr_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_sar_u8, iemAImpl_sar_u8_amd, iemAImpl_sar_u8_intel;
/** @} */

/** @name Shift operations on words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSHIFTU16,(uint32_t fEFlagsIn, uint16_t *pu16Dst, uint8_t cShift));
typedef FNIEMAIMPLSHIFTU16  *PFNIEMAIMPLSHIFTU16;
FNIEMAIMPLSHIFTU16 iemAImpl_rol_u16, iemAImpl_rol_u16_amd, iemAImpl_rol_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_ror_u16, iemAImpl_ror_u16_amd, iemAImpl_ror_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_rcl_u16, iemAImpl_rcl_u16_amd, iemAImpl_rcl_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_rcr_u16, iemAImpl_rcr_u16_amd, iemAImpl_rcr_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_shl_u16, iemAImpl_shl_u16_amd, iemAImpl_shl_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_shr_u16, iemAImpl_shr_u16_amd, iemAImpl_shr_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_sar_u16, iemAImpl_sar_u16_amd, iemAImpl_sar_u16_intel;
/** @} */

/** @name Shift operations on double words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSHIFTU32,(uint32_t fEFlagsIn, uint32_t *pu32Dst, uint8_t cShift));
typedef FNIEMAIMPLSHIFTU32  *PFNIEMAIMPLSHIFTU32;
FNIEMAIMPLSHIFTU32 iemAImpl_rol_u32, iemAImpl_rol_u32_amd, iemAImpl_rol_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_ror_u32, iemAImpl_ror_u32_amd, iemAImpl_ror_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_rcl_u32, iemAImpl_rcl_u32_amd, iemAImpl_rcl_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_rcr_u32, iemAImpl_rcr_u32_amd, iemAImpl_rcr_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_shl_u32, iemAImpl_shl_u32_amd, iemAImpl_shl_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_shr_u32, iemAImpl_shr_u32_amd, iemAImpl_shr_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_sar_u32, iemAImpl_sar_u32_amd, iemAImpl_sar_u32_intel;
/** @} */

/** @name Shift operations on words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSHIFTU64,(uint32_t fEFlagsIn, uint64_t *pu64Dst, uint8_t cShift));
typedef FNIEMAIMPLSHIFTU64  *PFNIEMAIMPLSHIFTU64;
FNIEMAIMPLSHIFTU64 iemAImpl_rol_u64, iemAImpl_rol_u64_amd, iemAImpl_rol_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_ror_u64, iemAImpl_ror_u64_amd, iemAImpl_ror_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_rcl_u64, iemAImpl_rcl_u64_amd, iemAImpl_rcl_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_rcr_u64, iemAImpl_rcr_u64_amd, iemAImpl_rcr_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_shl_u64, iemAImpl_shl_u64_amd, iemAImpl_shl_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_shr_u64, iemAImpl_shr_u64_amd, iemAImpl_shr_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_sar_u64, iemAImpl_sar_u64_amd, iemAImpl_sar_u64_intel;
/** @} */

/** @name Multiplication and division operations.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMULDIVU8,(uint16_t *pu16AX, uint8_t u8FactorDivisor, uint32_t fEFlags));
typedef FNIEMAIMPLMULDIVU8  *PFNIEMAIMPLMULDIVU8;
FNIEMAIMPLMULDIVU8 iemAImpl_mul_u8,  iemAImpl_mul_u8_amd,  iemAImpl_mul_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_imul_u8, iemAImpl_imul_u8_amd, iemAImpl_imul_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_div_u8,  iemAImpl_div_u8_amd,  iemAImpl_div_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_idiv_u8, iemAImpl_idiv_u8_amd, iemAImpl_idiv_u8_intel;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMULDIVU16,(uint16_t *pu16AX, uint16_t *pu16DX, uint16_t u16FactorDivisor, uint32_t fEFlags));
typedef FNIEMAIMPLMULDIVU16  *PFNIEMAIMPLMULDIVU16;
FNIEMAIMPLMULDIVU16 iemAImpl_mul_u16,  iemAImpl_mul_u16_amd,  iemAImpl_mul_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_imul_u16, iemAImpl_imul_u16_amd, iemAImpl_imul_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_div_u16,  iemAImpl_div_u16_amd,  iemAImpl_div_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_idiv_u16, iemAImpl_idiv_u16_amd, iemAImpl_idiv_u16_intel;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMULDIVU32,(uint32_t *pu32EAX, uint32_t *pu32EDX, uint32_t u32FactorDivisor, uint32_t fEFlags));
typedef FNIEMAIMPLMULDIVU32  *PFNIEMAIMPLMULDIVU32;
FNIEMAIMPLMULDIVU32 iemAImpl_mul_u32,  iemAImpl_mul_u32_amd,  iemAImpl_mul_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_imul_u32, iemAImpl_imul_u32_amd, iemAImpl_imul_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_div_u32,  iemAImpl_div_u32_amd,  iemAImpl_div_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_idiv_u32, iemAImpl_idiv_u32_amd, iemAImpl_idiv_u32_intel;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMULDIVU64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64FactorDivisor, uint32_t fEFlags));
typedef FNIEMAIMPLMULDIVU64  *PFNIEMAIMPLMULDIVU64;
FNIEMAIMPLMULDIVU64 iemAImpl_mul_u64,  iemAImpl_mul_u64_amd,  iemAImpl_mul_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_imul_u64, iemAImpl_imul_u64_amd, iemAImpl_imul_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_div_u64,  iemAImpl_div_u64_amd,  iemAImpl_div_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_idiv_u64, iemAImpl_idiv_u64_amd, iemAImpl_idiv_u64_intel;
/** @} */

/** @name Byte Swap.
 * @{  */
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u16,(uint32_t *pu32Dst)); /* Yes, 32-bit register access. */
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u32,(uint32_t *pu32Dst));
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u64,(uint64_t *pu64Dst));
/** @}  */

/** @name Misc.
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_arpl;
/** @} */

/** @name RDRAND and RDSEED
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLRDRANDSEEDU16,(uint16_t *puDst, uint32_t *pEFlags));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLRDRANDSEEDU32,(uint32_t *puDst, uint32_t *pEFlags));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLRDRANDSEEDU64,(uint64_t *puDst, uint32_t *pEFlags));
typedef FNIEMAIMPLRDRANDSEEDU16  *PFNIEMAIMPLRDRANDSEEDU16;
typedef FNIEMAIMPLRDRANDSEEDU32  *PFNIEMAIMPLRDRANDSEEDU32;
typedef FNIEMAIMPLRDRANDSEEDU64  *PFNIEMAIMPLRDRANDSEEDU64;

FNIEMAIMPLRDRANDSEEDU16 iemAImpl_rdrand_u16, iemAImpl_rdrand_u16_fallback;
FNIEMAIMPLRDRANDSEEDU32 iemAImpl_rdrand_u32, iemAImpl_rdrand_u32_fallback;
FNIEMAIMPLRDRANDSEEDU64 iemAImpl_rdrand_u64, iemAImpl_rdrand_u64_fallback;
FNIEMAIMPLRDRANDSEEDU16 iemAImpl_rdseed_u16, iemAImpl_rdseed_u16_fallback;
FNIEMAIMPLRDRANDSEEDU32 iemAImpl_rdseed_u32, iemAImpl_rdseed_u32_fallback;
FNIEMAIMPLRDRANDSEEDU64 iemAImpl_rdseed_u64, iemAImpl_rdseed_u64_fallback;
/** @} */

/** @name ADOX and ADCX
 * @{ */
FNIEMAIMPLBINU32 iemAImpl_adcx_u32, iemAImpl_adcx_u32_fallback;
FNIEMAIMPLBINU64 iemAImpl_adcx_u64, iemAImpl_adcx_u64_fallback;
FNIEMAIMPLBINU32 iemAImpl_adox_u32, iemAImpl_adox_u32_fallback;
FNIEMAIMPLBINU64 iemAImpl_adox_u64, iemAImpl_adox_u64_fallback;
/** @} */


/**
 * A FPU result.
 * @note x86 specific
 */
typedef struct IEMFPURESULT
{
    /** The output value. */
    RTFLOAT80U      r80Result;
    /** The output status. */
    uint16_t        FSW;
} IEMFPURESULT;
AssertCompileMemberOffset(IEMFPURESULT, FSW, 10);
/** Pointer to a FPU result. */
typedef IEMFPURESULT *PIEMFPURESULT;
/** Pointer to a const FPU result. */
typedef IEMFPURESULT const *PCIEMFPURESULT;

/** @name FPU operations taking a 32-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR32FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2));
typedef FNIEMAIMPLFPUR32FSW *PFNIEMAIMPLFPUR32FSW;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2));
typedef FNIEMAIMPLFPUR32    *PFNIEMAIMPLFPUR32;

FNIEMAIMPLFPUR32FSW iemAImpl_fcom_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fadd_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fmul_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fsub_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fsubr_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fdiv_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fdivr_r80_by_r32;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT32U pr32Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r32,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT32U pr32Val, PCRTFLOAT80U pr80Val));
/** @} */

/** @name FPU operations taking a 64-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR64FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2));
typedef FNIEMAIMPLFPUR64FSW *PFNIEMAIMPLFPUR64FSW;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2));
typedef FNIEMAIMPLFPUR64   *PFNIEMAIMPLFPUR64;

FNIEMAIMPLFPUR64FSW iemAImpl_fcom_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fadd_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fmul_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fsub_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fsubr_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fdiv_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fdivr_r80_by_r64;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT64U pr64Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r64,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT64U pr32Val, PCRTFLOAT80U pr80Val));
/** @} */

/** @name FPU operations taking a 80-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80    *PFNIEMAIMPLFPUR80;
FNIEMAIMPLFPUR80            iemAImpl_fadd_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fmul_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fsub_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fsubr_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fdiv_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fdivr_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fprem_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fprem1_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fscale_r80_by_r80;

FNIEMAIMPLFPUR80            iemAImpl_fpatan_r80_by_r80,  iemAImpl_fpatan_r80_by_r80_amd,  iemAImpl_fpatan_r80_by_r80_intel;
FNIEMAIMPLFPUR80            iemAImpl_fyl2x_r80_by_r80,   iemAImpl_fyl2x_r80_by_r80_amd,   iemAImpl_fyl2x_r80_by_r80_intel;
FNIEMAIMPLFPUR80            iemAImpl_fyl2xp1_r80_by_r80, iemAImpl_fyl2xp1_r80_by_r80_amd, iemAImpl_fyl2xp1_r80_by_r80_intel;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80FSW *PFNIEMAIMPLFPUR80FSW;
FNIEMAIMPLFPUR80FSW         iemAImpl_fcom_r80_by_r80;
FNIEMAIMPLFPUR80FSW         iemAImpl_fucom_r80_by_r80;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPUR80EFL,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw,
                                                          PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80EFL *PFNIEMAIMPLFPUR80EFL;
FNIEMAIMPLFPUR80EFL         iemAImpl_fcomi_r80_by_r80;
FNIEMAIMPLFPUR80EFL         iemAImpl_fucomi_r80_by_r80;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARY,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARY *PFNIEMAIMPLFPUR80UNARY;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fabs_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fchs_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_f2xm1_r80, iemAImpl_f2xm1_r80_amd, iemAImpl_f2xm1_r80_intel;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fsqrt_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_frndint_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fsin_r80, iemAImpl_fsin_r80_amd, iemAImpl_fsin_r80_intel;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fcos_r80, iemAImpl_fcos_r80_amd, iemAImpl_fcos_r80_intel;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARYFSW,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw, PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARYFSW *PFNIEMAIMPLFPUR80UNARYFSW;
FNIEMAIMPLFPUR80UNARYFSW    iemAImpl_ftst_r80;
FNIEMAIMPLFPUR80UNARYFSW    iemAImpl_fxam_r80;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80LDCONST,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes));
typedef FNIEMAIMPLFPUR80LDCONST *PFNIEMAIMPLFPUR80LDCONST;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fld1;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldl2t;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldl2e;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldpi;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldlg2;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldln2;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldz;

/**
 * A FPU result consisting of two output values and FSW.
 * @note x86 specific
 */
typedef struct IEMFPURESULTTWO
{
    /** The first output value. */
    RTFLOAT80U      r80Result1;
    /** The output status. */
    uint16_t        FSW;
    /** The second output value. */
    RTFLOAT80U      r80Result2;
} IEMFPURESULTTWO;
AssertCompileMemberOffset(IEMFPURESULTTWO, FSW, 10);
AssertCompileMemberOffset(IEMFPURESULTTWO, r80Result2, 12);
/** Pointer to a FPU result consisting of two output values and FSW. */
typedef IEMFPURESULTTWO *PIEMFPURESULTTWO;
/** Pointer to a const FPU result consisting of two output values and FSW. */
typedef IEMFPURESULTTWO const *PCIEMFPURESULTTWO;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARYTWO,(PCX86FXSTATE pFpuState, PIEMFPURESULTTWO pFpuResTwo,
                                                           PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARYTWO *PFNIEMAIMPLFPUR80UNARYTWO;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fptan_r80_r80, iemAImpl_fptan_r80_r80_amd, iemAImpl_fptan_r80_r80_intel;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fxtract_r80_r80;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fsincos_r80_r80, iemAImpl_fsincos_r80_r80_amd, iemAImpl_fsincos_r80_r80_intel;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT80U pr80Dst, PCRTFLOAT80U pr80Src));

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_d80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTPBCD80U pd80Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_d80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTPBCD80U pd80Dst, PCRTFLOAT80U pr80Src));

/** @} */

/** @name FPU operations taking a 16-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2));
typedef FNIEMAIMPLFPUI16 *PFNIEMAIMPLFPUI16;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI16,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int16_t *pi16Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI16 *PFNIEMAIMPLFPUSTR80TOI16;

FNIEMAIMPLFPUI16    iemAImpl_fiadd_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fimul_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fisub_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fisubr_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fidiv_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fidivr_r80_by_i16;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI16FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2));
typedef FNIEMAIMPLFPUI16FSW *PFNIEMAIMPLFPUI16FSW;
FNIEMAIMPLFPUI16FSW     iemAImpl_ficom_r80_by_i16;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int16_t const *pi16Val));
FNIEMAIMPLFPUSTR80TOI16 iemAImpl_fist_r80_to_i16;
FNIEMAIMPLFPUSTR80TOI16 iemAImpl_fistt_r80_to_i16, iemAImpl_fistt_r80_to_i16_amd, iemAImpl_fistt_r80_to_i16_intel;
/** @}  */

/** @name FPU operations taking a 32-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2));
typedef FNIEMAIMPLFPUI32 *PFNIEMAIMPLFPUI32;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI32,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int32_t *pi32Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI32 *PFNIEMAIMPLFPUSTR80TOI32;

FNIEMAIMPLFPUI32    iemAImpl_fiadd_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fimul_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fisub_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fisubr_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fidiv_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fidivr_r80_by_i32;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI32FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2));
typedef FNIEMAIMPLFPUI32FSW *PFNIEMAIMPLFPUI32FSW;
FNIEMAIMPLFPUI32FSW     iemAImpl_ficom_r80_by_i32;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int32_t const *pi32Val));
FNIEMAIMPLFPUSTR80TOI32 iemAImpl_fist_r80_to_i32;
FNIEMAIMPLFPUSTR80TOI32 iemAImpl_fistt_r80_to_i32;
/** @}  */

/** @name FPU operations taking a 64-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI64,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int64_t *pi64Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI64 *PFNIEMAIMPLFPUSTR80TOI64;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int64_t const *pi64Val));
FNIEMAIMPLFPUSTR80TOI64 iemAImpl_fist_r80_to_i64;
FNIEMAIMPLFPUSTR80TOI64 iemAImpl_fistt_r80_to_i64;
/** @} */


/** Temporary type representing a 256-bit vector register. */
typedef struct { uint64_t au64[4]; } IEMVMM256;
/** Temporary type pointing to a 256-bit vector register. */
typedef IEMVMM256 *PIEMVMM256;
/** Temporary type pointing to a const 256-bit vector register. */
typedef IEMVMM256 *PCIEMVMM256;


/** @name Media (SSE/MMX/AVX) operations: full1 + full2 -> full1.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAF2U64,(PCX86FXSTATE pFpuState, uint64_t *puDst, uint64_t const *puSrc));
typedef FNIEMAIMPLMEDIAF2U64   *PFNIEMAIMPLMEDIAF2U64;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF2U128,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
typedef FNIEMAIMPLMEDIAF2U128  *PFNIEMAIMPLMEDIAF2U128;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF2U256,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86YMMREG puSrc));
typedef FNIEMAIMPLMEDIAF2U256  *PFNIEMAIMPLMEDIAF2U256;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF3U128,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc1, PCX86XMMREG puSrc2));
typedef FNIEMAIMPLMEDIAF3U128  *PFNIEMAIMPLMEDIAF3U128;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF3U256,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86YMMREG puSrc1, PCX86YMMREG puSrc2));
typedef FNIEMAIMPLMEDIAF3U256  *PFNIEMAIMPLMEDIAF3U256;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U64,(uint64_t *puDst, uint64_t const *puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U64   *PFNIEMAIMPLMEDIAOPTF2U64;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U128,(PRTUINT128U puDst, PCRTUINT128U puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U128  *PFNIEMAIMPLMEDIAOPTF2U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U128,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2));
typedef FNIEMAIMPLMEDIAOPTF3U128  *PFNIEMAIMPLMEDIAOPTF3U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U256,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2));
typedef FNIEMAIMPLMEDIAOPTF3U256  *PFNIEMAIMPLMEDIAOPTF3U256;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U256,(PRTUINT256U puDst, PCRTUINT256U puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U256  *PFNIEMAIMPLMEDIAOPTF2U256;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pshufb_u64, iemAImpl_pshufb_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pand_u64, iemAImpl_pandn_u64, iemAImpl_por_u64, iemAImpl_pxor_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pcmpeqb_u64,  iemAImpl_pcmpeqw_u64,  iemAImpl_pcmpeqd_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pcmpgtb_u64,  iemAImpl_pcmpgtw_u64,  iemAImpl_pcmpgtd_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_paddb_u64, iemAImpl_paddsb_u64, iemAImpl_paddusb_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_paddw_u64, iemAImpl_paddsw_u64, iemAImpl_paddusw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_paddd_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_paddq_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psubb_u64, iemAImpl_psubsb_u64, iemAImpl_psubusb_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psubw_u64, iemAImpl_psubsw_u64, iemAImpl_psubusw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psubd_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psubq_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmaddwd_u64, iemAImpl_pmaddwd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmullw_u64, iemAImpl_pmulhw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pminub_u64, iemAImpl_pmaxub_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pminsw_u64, iemAImpl_pmaxsw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pabsb_u64, iemAImpl_pabsb_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pabsw_u64, iemAImpl_pabsw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pabsd_u64, iemAImpl_pabsd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psignb_u64, iemAImpl_psignb_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psignw_u64, iemAImpl_psignw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psignd_u64, iemAImpl_psignd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phaddw_u64, iemAImpl_phaddw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phaddd_u64, iemAImpl_phaddd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phsubw_u64, iemAImpl_phsubw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phsubd_u64, iemAImpl_phsubd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phaddsw_u64, iemAImpl_phaddsw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_phsubsw_u64, iemAImpl_phsubsw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmaddubsw_u64, iemAImpl_pmaddubsw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmulhrsw_u64, iemAImpl_pmulhrsw_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmuludq_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psllw_u64, iemAImpl_psrlw_u64, iemAImpl_psraw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pslld_u64, iemAImpl_psrld_u64, iemAImpl_psrad_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psllq_u64, iemAImpl_psrlq_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_packsswb_u64, iemAImpl_packuswb_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_packssdw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmulhuw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pavgb_u64, iemAImpl_pavgw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psadbw_u64;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pshufb_u128, iemAImpl_pshufb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pand_u128, iemAImpl_pandn_u128, iemAImpl_por_u128, iemAImpl_pxor_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pcmpeqb_u128, iemAImpl_pcmpeqw_u128, iemAImpl_pcmpeqd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pcmpeqq_u128, iemAImpl_pcmpeqq_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pcmpgtb_u128, iemAImpl_pcmpgtw_u128, iemAImpl_pcmpgtd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pcmpgtq_u128, iemAImpl_pcmpgtq_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_paddb_u128, iemAImpl_paddsb_u128, iemAImpl_paddusb_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_paddw_u128, iemAImpl_paddsw_u128, iemAImpl_paddusw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_paddd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_paddq_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psubb_u128, iemAImpl_psubsb_u128, iemAImpl_psubusb_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psubw_u128, iemAImpl_psubsw_u128, iemAImpl_psubusw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psubd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psubq_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmullw_u128, iemAImpl_pmullw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmulhw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmulld_u128, iemAImpl_pmulld_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaddwd_u128, iemAImpl_pmaddwd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminub_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminud_u128, iemAImpl_pminud_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminuw_u128, iemAImpl_pminuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminsb_u128, iemAImpl_pminsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminsd_u128, iemAImpl_pminsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pminsw_u128, iemAImpl_pminsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxub_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxud_u128, iemAImpl_pmaxud_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxuw_u128, iemAImpl_pmaxuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxsb_u128, iemAImpl_pmaxsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxsw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaxsd_u128, iemAImpl_pmaxsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pabsb_u128, iemAImpl_pabsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pabsw_u128, iemAImpl_pabsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pabsd_u128, iemAImpl_pabsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psignb_u128, iemAImpl_psignb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psignw_u128, iemAImpl_psignw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psignd_u128, iemAImpl_psignd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phaddw_u128, iemAImpl_phaddw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phaddd_u128, iemAImpl_phaddd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phsubw_u128, iemAImpl_phsubw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phsubd_u128, iemAImpl_phsubd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phaddsw_u128, iemAImpl_phaddsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phsubsw_u128, iemAImpl_phsubsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaddubsw_u128, iemAImpl_pmaddubsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmulhrsw_u128, iemAImpl_pmulhrsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmuludq_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmaddwd_u128, iemAImpl_pmaddwd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_packsswb_u128, iemAImpl_packuswb_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_packssdw_u128, iemAImpl_packusdw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psllw_u128, iemAImpl_psrlw_u128, iemAImpl_psraw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pslld_u128, iemAImpl_psrld_u128, iemAImpl_psrad_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psllq_u128, iemAImpl_psrlq_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmulhuw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pavgb_u128, iemAImpl_pavgw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psadbw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmuldq_u128, iemAImpl_pmuldq_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_unpcklps_u128, iemAImpl_unpcklpd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_unpckhps_u128, iemAImpl_unpckhpd_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_phminposuw_u128, iemAImpl_phminposuw_u128_fallback;

FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpshufb_u128,    iemAImpl_vpshufb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpand_u128,      iemAImpl_vpand_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpandn_u128,     iemAImpl_vpandn_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpor_u128,       iemAImpl_vpor_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpxor_u128,      iemAImpl_vpxor_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpeqb_u128,   iemAImpl_vpcmpeqb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpeqw_u128,   iemAImpl_vpcmpeqw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpeqd_u128,   iemAImpl_vpcmpeqd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpeqq_u128,   iemAImpl_vpcmpeqq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpgtb_u128,   iemAImpl_vpcmpgtb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpgtw_u128,   iemAImpl_vpcmpgtw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpgtd_u128,   iemAImpl_vpcmpgtd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpcmpgtq_u128,   iemAImpl_vpcmpgtq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddb_u128,     iemAImpl_vpaddb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddw_u128,     iemAImpl_vpaddw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddd_u128,     iemAImpl_vpaddd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddq_u128,     iemAImpl_vpaddq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubb_u128,     iemAImpl_vpsubb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubw_u128,     iemAImpl_vpsubw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubd_u128,     iemAImpl_vpsubd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubq_u128,     iemAImpl_vpsubq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminub_u128,    iemAImpl_vpminub_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminuw_u128,    iemAImpl_vpminuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminud_u128,    iemAImpl_vpminud_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminsb_u128,    iemAImpl_vpminsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminsw_u128,    iemAImpl_vpminsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpminsd_u128,    iemAImpl_vpminsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxub_u128,    iemAImpl_vpmaxub_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxuw_u128,    iemAImpl_vpmaxuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxud_u128,    iemAImpl_vpmaxud_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxsb_u128,    iemAImpl_vpmaxsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxsw_u128,    iemAImpl_vpmaxsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaxsd_u128,    iemAImpl_vpmaxsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpacksswb_u128,  iemAImpl_vpacksswb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackssdw_u128,  iemAImpl_vpackssdw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackuswb_u128,  iemAImpl_vpackuswb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackusdw_u128,  iemAImpl_vpackusdw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmullw_u128,    iemAImpl_vpmullw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulld_u128,    iemAImpl_vpmulld_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulhw_u128,    iemAImpl_vpmulhw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulhuw_u128,   iemAImpl_vpmulhuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpavgb_u128,     iemAImpl_vpavgb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpavgw_u128,     iemAImpl_vpavgw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignb_u128,    iemAImpl_vpsignb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignw_u128,    iemAImpl_vpsignw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignd_u128,    iemAImpl_vpsignd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphaddw_u128,    iemAImpl_vphaddw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphaddd_u128,    iemAImpl_vphaddd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphsubw_u128,    iemAImpl_vphsubw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphsubd_u128,    iemAImpl_vphsubd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphaddsw_u128,   iemAImpl_vphaddsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphsubsw_u128,   iemAImpl_vphsubsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaddubsw_u128, iemAImpl_vpmaddubsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulhrsw_u128,  iemAImpl_vpmulhrsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsadbw_u128,    iemAImpl_vpsadbw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmuldq_u128,    iemAImpl_vpmuldq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmuludq_u128,   iemAImpl_vpmuludq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubsb_u128,    iemAImpl_vpsubsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubsw_u128,    iemAImpl_vpsubsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubusb_u128,   iemAImpl_vpsubusb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsubusw_u128,   iemAImpl_vpsubusw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddusb_u128,   iemAImpl_vpaddusb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddusw_u128,   iemAImpl_vpaddusw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddsb_u128,    iemAImpl_vpaddsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpaddsw_u128,    iemAImpl_vpaddsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsllw_u128,     iemAImpl_vpsllw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpslld_u128,     iemAImpl_vpslld_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsllq_u128,     iemAImpl_vpsllq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsraw_u128,     iemAImpl_vpsraw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsrad_u128,     iemAImpl_vpsrad_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsrlw_u128,     iemAImpl_vpsrlw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsrld_u128,     iemAImpl_vpsrld_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsrlq_u128,     iemAImpl_vpsrlq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmaddwd_u128, iemAImpl_vpmaddwd_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsb_u128,     iemAImpl_vpabsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsw_u128,     iemAImpl_vpabsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsd_u128,     iemAImpl_vpabsw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vphminposuw_u128, iemAImpl_vphminposuw_u128_fallback;

FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpshufb_u256,    iemAImpl_vpshufb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpand_u256,      iemAImpl_vpand_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpandn_u256,     iemAImpl_vpandn_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpor_u256,       iemAImpl_vpor_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpxor_u256,      iemAImpl_vpxor_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpeqb_u256,   iemAImpl_vpcmpeqb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpeqw_u256,   iemAImpl_vpcmpeqw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpeqd_u256,   iemAImpl_vpcmpeqd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpeqq_u256,   iemAImpl_vpcmpeqq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpgtb_u256,   iemAImpl_vpcmpgtb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpgtw_u256,   iemAImpl_vpcmpgtw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpgtd_u256,   iemAImpl_vpcmpgtd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpcmpgtq_u256,   iemAImpl_vpcmpgtq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddb_u256,     iemAImpl_vpaddb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddw_u256,     iemAImpl_vpaddw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddd_u256,     iemAImpl_vpaddd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddq_u256,     iemAImpl_vpaddq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubb_u256,     iemAImpl_vpsubb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubw_u256,     iemAImpl_vpsubw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubd_u256,     iemAImpl_vpsubd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubq_u256,     iemAImpl_vpsubq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminub_u256,    iemAImpl_vpminub_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminuw_u256,    iemAImpl_vpminuw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminud_u256,    iemAImpl_vpminud_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminsb_u256,    iemAImpl_vpminsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminsw_u256,    iemAImpl_vpminsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpminsd_u256,    iemAImpl_vpminsd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxub_u256,    iemAImpl_vpmaxub_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxuw_u256,    iemAImpl_vpmaxuw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxud_u256,    iemAImpl_vpmaxud_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxsb_u256,    iemAImpl_vpmaxsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxsw_u256,    iemAImpl_vpmaxsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaxsd_u256,    iemAImpl_vpmaxsd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpacksswb_u256,  iemAImpl_vpacksswb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackssdw_u256,  iemAImpl_vpackssdw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackuswb_u256,  iemAImpl_vpackuswb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackusdw_u256,  iemAImpl_vpackusdw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmullw_u256,    iemAImpl_vpmullw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulld_u256,    iemAImpl_vpmulld_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulhw_u256,    iemAImpl_vpmulhw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulhuw_u256,   iemAImpl_vpmulhuw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpavgb_u256,     iemAImpl_vpavgb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpavgw_u256,     iemAImpl_vpavgw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignb_u256,    iemAImpl_vpsignb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignw_u256,    iemAImpl_vpsignw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignd_u256,    iemAImpl_vpsignd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphaddw_u256,    iemAImpl_vphaddw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphaddd_u256,    iemAImpl_vphaddd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphsubw_u256,    iemAImpl_vphsubw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphsubd_u256,    iemAImpl_vphsubd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphaddsw_u256,   iemAImpl_vphaddsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphsubsw_u256,   iemAImpl_vphsubsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaddubsw_u256, iemAImpl_vpmaddubsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulhrsw_u256,  iemAImpl_vpmulhrsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsadbw_u256,    iemAImpl_vpsadbw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmuldq_u256,    iemAImpl_vpmuldq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmuludq_u256,   iemAImpl_vpmuludq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubsb_u256,    iemAImpl_vpsubsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubsw_u256,    iemAImpl_vpsubsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubusb_u256,   iemAImpl_vpsubusb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsubusw_u256,   iemAImpl_vpsubusw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddusb_u256,   iemAImpl_vpaddusb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddusw_u256,   iemAImpl_vpaddusw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddsb_u256,    iemAImpl_vpaddsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpaddsw_u256,    iemAImpl_vpaddsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsllw_u256,     iemAImpl_vpsllw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpslld_u256,     iemAImpl_vpslld_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsllq_u256,     iemAImpl_vpsllq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsraw_u256,     iemAImpl_vpsraw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsrad_u256,     iemAImpl_vpsrad_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsrlw_u256,     iemAImpl_vpsrlw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsrld_u256,     iemAImpl_vpsrld_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsrlq_u256,     iemAImpl_vpsrlq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmaddwd_u256, iemAImpl_vpmaddwd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpermps_u256,    iemAImpl_vpermps_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpermd_u256,     iemAImpl_vpermd_u256_fallback;

FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsb_u256,     iemAImpl_vpabsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsw_u256,     iemAImpl_vpabsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsd_u256,     iemAImpl_vpabsd_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operations: lowhalf1 + lowhalf1 -> full1.
 * @{ */
FNIEMAIMPLMEDIAOPTF2U64   iemAImpl_punpcklbw_u64,  iemAImpl_punpcklwd_u64,  iemAImpl_punpckldq_u64;
FNIEMAIMPLMEDIAOPTF2U128  iemAImpl_punpcklbw_u128, iemAImpl_punpcklwd_u128, iemAImpl_punpckldq_u128, iemAImpl_punpcklqdq_u128;
FNIEMAIMPLMEDIAOPTF3U128  iemAImpl_vpunpcklbw_u128,  iemAImpl_vpunpcklbw_u128_fallback,
                          iemAImpl_vpunpcklwd_u128,  iemAImpl_vpunpcklwd_u128_fallback,
                          iemAImpl_vpunpckldq_u128,  iemAImpl_vpunpckldq_u128_fallback,
                          iemAImpl_vpunpcklqdq_u128, iemAImpl_vpunpcklqdq_u128_fallback,
                          iemAImpl_vunpcklps_u128, iemAImpl_vunpcklps_u128_fallback,
                          iemAImpl_vunpcklpd_u128, iemAImpl_vunpcklpd_u128_fallback,
                          iemAImpl_vunpckhps_u128, iemAImpl_vunpckhps_u128_fallback,
                          iemAImpl_vunpckhpd_u128, iemAImpl_vunpckhpd_u128_fallback;

FNIEMAIMPLMEDIAOPTF3U256  iemAImpl_vpunpcklbw_u256,  iemAImpl_vpunpcklbw_u256_fallback,
                          iemAImpl_vpunpcklwd_u256,  iemAImpl_vpunpcklwd_u256_fallback,
                          iemAImpl_vpunpckldq_u256,  iemAImpl_vpunpckldq_u256_fallback,
                          iemAImpl_vpunpcklqdq_u256, iemAImpl_vpunpcklqdq_u256_fallback,
                          iemAImpl_vunpcklps_u256, iemAImpl_vunpcklps_u256_fallback,
                          iemAImpl_vunpcklpd_u256, iemAImpl_vunpcklpd_u256_fallback,
                          iemAImpl_vunpckhps_u256, iemAImpl_vunpckhps_u256_fallback,
                          iemAImpl_vunpckhpd_u256, iemAImpl_vunpckhpd_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operations: hihalf1 + hihalf2 -> full1.
 * @{ */
FNIEMAIMPLMEDIAOPTF2U64   iemAImpl_punpckhbw_u64,  iemAImpl_punpckhwd_u64,  iemAImpl_punpckhdq_u64;
FNIEMAIMPLMEDIAOPTF2U128  iemAImpl_punpckhbw_u128, iemAImpl_punpckhwd_u128, iemAImpl_punpckhdq_u128, iemAImpl_punpckhqdq_u128;
FNIEMAIMPLMEDIAOPTF3U128  iemAImpl_vpunpckhbw_u128,  iemAImpl_vpunpckhbw_u128_fallback,
                          iemAImpl_vpunpckhwd_u128,  iemAImpl_vpunpckhwd_u128_fallback,
                          iemAImpl_vpunpckhdq_u128,  iemAImpl_vpunpckhdq_u128_fallback,
                          iemAImpl_vpunpckhqdq_u128, iemAImpl_vpunpckhqdq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256  iemAImpl_vpunpckhbw_u256,  iemAImpl_vpunpckhbw_u256_fallback,
                          iemAImpl_vpunpckhwd_u256,  iemAImpl_vpunpckhwd_u256_fallback,
                          iemAImpl_vpunpckhdq_u256,  iemAImpl_vpunpckhdq_u256_fallback,
                          iemAImpl_vpunpckhqdq_u256, iemAImpl_vpunpckhqdq_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Packed Shuffle Stuff (evil)
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHUFU128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAPSHUFU128 *PFNIEMAIMPLMEDIAPSHUFU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHUFU256,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAPSHUFU256 *PFNIEMAIMPLMEDIAPSHUFU256;
IEM_DECL_IMPL_DEF(void, iemAImpl_pshufw_u64,(uint64_t *puDst, uint64_t const *puSrc, uint8_t bEvil));
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_pshufhw_u128, iemAImpl_pshuflw_u128, iemAImpl_pshufd_u128;
#ifndef IEM_WITHOUT_ASSEMBLY
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpshufhw_u256, iemAImpl_vpshuflw_u256, iemAImpl_vpshufd_u256;
#endif
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpshufhw_u256_fallback, iemAImpl_vpshuflw_u256_fallback, iemAImpl_vpshufd_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Shift Immediate Stuff (evil)
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU64,(uint64_t *puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU64 *PFNIEMAIMPLMEDIAPSHIFTU64;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU128,(PRTUINT128U puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU128 *PFNIEMAIMPLMEDIAPSHIFTU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU256,(PRTUINT256U puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU256 *PFNIEMAIMPLMEDIAPSHIFTU256;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psllw_imm_u64,  iemAImpl_pslld_imm_u64,  iemAImpl_psllq_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psrlw_imm_u64,  iemAImpl_psrld_imm_u64,  iemAImpl_psrlq_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psraw_imm_u64,  iemAImpl_psrad_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psllw_imm_u128, iemAImpl_pslld_imm_u128, iemAImpl_psllq_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psrlw_imm_u128, iemAImpl_psrld_imm_u128, iemAImpl_psrlq_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psraw_imm_u128, iemAImpl_psrad_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_pslldq_imm_u128, iemAImpl_psrldq_imm_u128;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Move Byte Mask
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_maskmovq_u64,(uint64_t *puMem, uint64_t const *puSrc, uint64_t const *puMsk));
IEM_DECL_IMPL_DEF(void, iemAImpl_maskmovdqu_u128,(PRTUINT128U puMem, PCRTUINT128U puSrc, PCRTUINT128U puMsk));
IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u64,(uint64_t *pu64Dst, uint64_t const *puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u128,(uint64_t *pu64Dst, PCRTUINT128U puSrc));
#ifndef IEM_WITHOUT_ASSEMBLY
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovmskb_u256,(uint64_t *pu64Dst, PCRTUINT256U puSrc));
#endif
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovmskb_u256_fallback,(uint64_t *pu64Dst, PCRTUINT256U puSrc));
/** @} */

/** @name Media (SSE/MMX/AVX) operations: Variable Blend Packed Bytes/R32/R64.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBLENDU128,(PRTUINT128U puDst, PCRTUINT128U puSrc, PCRTUINT128U puMask));
typedef FNIEMAIMPLBLENDU128  *PFNIEMAIMPLBLENDU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLAVXBLENDU128,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, PCRTUINT128U puMask));
typedef FNIEMAIMPLAVXBLENDU128  *PFNIEMAIMPLAVXBLENDU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLAVXBLENDU256,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, PCRTUINT256U puMask));
typedef FNIEMAIMPLAVXBLENDU256  *PFNIEMAIMPLAVXBLENDU256;

FNIEMAIMPLBLENDU128 iemAImpl_pblendvb_u128;
FNIEMAIMPLBLENDU128 iemAImpl_pblendvb_u128_fallback;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vpblendvb_u128;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vpblendvb_u128_fallback;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vpblendvb_u256;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vpblendvb_u256_fallback;

FNIEMAIMPLBLENDU128 iemAImpl_blendvps_u128;
FNIEMAIMPLBLENDU128 iemAImpl_blendvps_u128_fallback;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vblendvps_u128;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vblendvps_u128_fallback;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vblendvps_u256;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vblendvps_u256_fallback;

FNIEMAIMPLBLENDU128 iemAImpl_blendvpd_u128;
FNIEMAIMPLBLENDU128 iemAImpl_blendvpd_u128_fallback;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vblendvpd_u128;
FNIEMAIMPLAVXBLENDU128 iemAImpl_vblendvpd_u128_fallback;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vblendvpd_u256;
FNIEMAIMPLAVXBLENDU256 iemAImpl_vblendvpd_u256_fallback;
/** @} */


/** @name Media (SSE/MMX/AVX) operation: Sort this later
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxbw_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbw_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbw_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbw_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbw_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxbd_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbd_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbd_u128_fallback,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbd_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbd_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxbq_u128,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbq_u128,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbq_u128_fallback,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxbq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxwd_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwd_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwd_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwd_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwd_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxwq_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwq_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwq_u128_fallback,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxwq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovsxdq_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxdq_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxdq_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxdq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovsxdq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxbw_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbw_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbw_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbw_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbw_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxbd_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbd_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbd_u128_fallback,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbd_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbd_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxbq_u128,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbq_u128,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbq_u128_fallback,(PRTUINT128U puDst, uint16_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxbq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxwd_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwd_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwd_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwd_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwd_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxwq_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwq_u128,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwq_u128_fallback,(PRTUINT128U puDst, uint32_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxwq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_pmovzxdq_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxdq_u128,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxdq_u128_fallback,(PRTUINT128U puDst, uint64_t uSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxdq_u256,(PRTUINT256U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovzxdq_u256_fallback,(PRTUINT256U puDst, PCRTUINT128U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_shufpd_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufpd_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufpd_u128_fallback,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufpd_u256,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufpd_u256_fallback,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint8_t bEvil));

IEM_DECL_IMPL_DEF(void, iemAImpl_shufps_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufps_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufps_u128_fallback,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufps_u256,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_vshufps_u256_fallback,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint8_t bEvil));

IEM_DECL_IMPL_DEF(void, iemAImpl_palignr_u64,(uint64_t *pu64Dst, uint64_t u64Src, uint8_t bEvil));
IEM_DECL_IMPL_DEF(void, iemAImpl_palignr_u64_fallback,(uint64_t *pu64Dst, uint64_t u64Src, uint8_t bEvil));

IEM_DECL_IMPL_DEF(void, iemAImpl_movmskps_u128,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskps_u128,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskps_u128_fallback,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskps_u256,(uint8_t *pu8Dst, PCRTUINT256U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskps_u256_fallback,(uint8_t *pu8Dst, PCRTUINT256U puSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_movmskpd_u128,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskpd_u128,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskpd_u128_fallback,(uint8_t *pu8Dst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskpd_u256,(uint8_t *pu8Dst, PCRTUINT256U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovmskpd_u256_fallback,(uint8_t *pu8Dst, PCRTUINT256U puSrc));


typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U128IMM8,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAOPTF2U128IMM8 *PFNIEMAIMPLMEDIAOPTF2U128IMM8;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U256IMM8,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAOPTF2U256IMM8 *PFNIEMAIMPLMEDIAOPTF2U256IMM8;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U128IMM8,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAOPTF3U128IMM8 *PFNIEMAIMPLMEDIAOPTF3U128IMM8;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U256IMM8,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAOPTF3U256IMM8 *PFNIEMAIMPLMEDIAOPTF3U256IMM8;

FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_palignr_u128, iemAImpl_palignr_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_pblendw_u128, iemAImpl_pblendw_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_blendps_u128, iemAImpl_blendps_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_blendpd_u128, iemAImpl_blendpd_u128_fallback;

FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vpalignr_u128, iemAImpl_vpalignr_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vpblendw_u128, iemAImpl_vpblendw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vpblendd_u128, iemAImpl_vpblendd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vblendps_u128, iemAImpl_vblendps_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vblendpd_u128, iemAImpl_vblendpd_u128_fallback;

FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vpalignr_u256, iemAImpl_vpalignr_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vpblendw_u256, iemAImpl_vpblendw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vpblendd_u256, iemAImpl_vpblendd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vblendps_u256, iemAImpl_vblendps_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vblendpd_u256, iemAImpl_vblendpd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vperm2i128_u256, iemAImpl_vperm2i128_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vperm2f128_u256, iemAImpl_vperm2f128_u256_fallback;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_aesimc_u128,     iemAImpl_aesimc_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_aesenc_u128,     iemAImpl_aesenc_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_aesenclast_u128, iemAImpl_aesenclast_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_aesdec_u128,     iemAImpl_aesdec_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_aesdeclast_u128, iemAImpl_aesdeclast_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vaesimc_u128,     iemAImpl_vaesimc_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vaesenc_u128,     iemAImpl_vaesenc_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vaesenclast_u128, iemAImpl_vaesenclast_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vaesdec_u128,     iemAImpl_vaesdec_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vaesdeclast_u128, iemAImpl_vaesdeclast_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_aeskeygenassist_u128, iemAImpl_aeskeygenassist_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_vaeskeygenassist_u128, iemAImpl_vaeskeygenassist_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_sha1nexte_u128,       iemAImpl_sha1nexte_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_sha1msg1_u128,        iemAImpl_sha1msg1_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_sha1msg2_u128,        iemAImpl_sha1msg2_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_sha256msg1_u128,      iemAImpl_sha256msg1_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_sha256msg2_u128,      iemAImpl_sha256msg2_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_sha1rnds4_u128,   iemAImpl_sha1rnds4_u128_fallback;
IEM_DECL_IMPL_DEF(void, iemAImpl_sha256rnds2_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc, PCRTUINT128U puXmm0Constants));
IEM_DECL_IMPL_DEF(void, iemAImpl_sha256rnds2_u128_fallback,(PRTUINT128U puDst, PCRTUINT128U puSrc, PCRTUINT128U puXmm0Constants));

FNIEMAIMPLMEDIAOPTF2U256IMM8 iemAImpl_vpermq_u256,      iemAImpl_vpermq_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256IMM8 iemAImpl_vpermpd_u256,     iemAImpl_vpermpd_u256_fallback;

typedef struct IEMPCMPISTRXSRC
{
    RTUINT128U              uSrc1;
    RTUINT128U              uSrc2;
} IEMPCMPISTRXSRC;
typedef IEMPCMPISTRXSRC *PIEMPCMPISTRXSRC;
typedef const IEMPCMPISTRXSRC *PCIEMPCMPISTRXSRC;

typedef struct IEMPCMPESTRXSRC
{
    RTUINT128U              uSrc1;
    RTUINT128U              uSrc2;
    uint64_t                u64Rax;
    uint64_t                u64Rdx;
} IEMPCMPESTRXSRC;
typedef IEMPCMPESTRXSRC *PIEMPCMPESTRXSRC;
typedef const IEMPCMPESTRXSRC *PCIEMPCMPESTRXSRC;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLPCMPISTRIU128IMM8,(uint32_t *pEFlags, PCRTUINT128U pSrc1, PCRTUINT128U pSrc2, uint8_t bEvil));
typedef FNIEMAIMPLPCMPISTRIU128IMM8 *PFNIEMAIMPLPCMPISTRIU128IMM8;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLPCMPESTRIU128IMM8,(uint32_t *pu32Ecx, uint32_t *pEFlags, PCIEMPCMPESTRXSRC pSrc, uint8_t bEvil));
typedef FNIEMAIMPLPCMPESTRIU128IMM8 *PFNIEMAIMPLPCMPESTRIU128IMM8;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLPCMPISTRMU128IMM8,(PRTUINT128U puDst, uint32_t *pEFlags, PCIEMPCMPISTRXSRC pSrc, uint8_t bEvil));
typedef FNIEMAIMPLPCMPISTRMU128IMM8 *PFNIEMAIMPLPCMPISTRMU128IMM8;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLPCMPESTRMU128IMM8,(PRTUINT128U puDst, uint32_t *pEFlags, PCIEMPCMPESTRXSRC pSrc, uint8_t bEvil));
typedef FNIEMAIMPLPCMPESTRMU128IMM8 *PFNIEMAIMPLPCMPESTRMU128IMM8;

FNIEMAIMPLPCMPISTRIU128IMM8 iemAImpl_pcmpistri_u128,  iemAImpl_pcmpistri_u128_fallback;
FNIEMAIMPLPCMPESTRIU128IMM8 iemAImpl_pcmpestri_u128,  iemAImpl_pcmpestri_u128_fallback;
FNIEMAIMPLPCMPISTRMU128IMM8 iemAImpl_pcmpistrm_u128,  iemAImpl_pcmpistrm_u128_fallback;
FNIEMAIMPLPCMPESTRMU128IMM8 iemAImpl_pcmpestrm_u128,  iemAImpl_pcmpestrm_u128_fallback;
FNIEMAIMPLPCMPISTRIU128IMM8 iemAImpl_vpcmpistri_u128, iemAImpl_vpcmpistri_u128_fallback;
FNIEMAIMPLPCMPESTRIU128IMM8 iemAImpl_vpcmpestri_u128, iemAImpl_vpcmpestri_u128_fallback;
FNIEMAIMPLPCMPISTRMU128IMM8 iemAImpl_vpcmpistrm_u128, iemAImpl_vpcmpistrm_u128_fallback;
FNIEMAIMPLPCMPESTRMU128IMM8 iemAImpl_vpcmpestrm_u128, iemAImpl_vpcmpestrm_u128_fallback;


FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_pclmulqdq_u128, iemAImpl_pclmulqdq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vpclmulqdq_u128, iemAImpl_vpclmulqdq_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_mpsadbw_u128, iemAImpl_mpsadbw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128IMM8 iemAImpl_vmpsadbw_u128, iemAImpl_vmpsadbw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256IMM8 iemAImpl_vmpsadbw_u256, iemAImpl_vmpsadbw_u256_fallback;

FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsllw_imm_u128, iemAImpl_vpsllw_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsllw_imm_u256, iemAImpl_vpsllw_imm_u256_fallback;
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpslld_imm_u128, iemAImpl_vpslld_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpslld_imm_u256, iemAImpl_vpslld_imm_u256_fallback;
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsllq_imm_u128, iemAImpl_vpsllq_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsllq_imm_u256, iemAImpl_vpsllq_imm_u256_fallback;
IEM_DECL_IMPL_DEF(void, iemAImpl_vpslldq_imm_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpslldq_imm_u128_fallback,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpslldq_imm_u256,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpslldq_imm_u256_fallback,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t uShift));

FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsraw_imm_u128, iemAImpl_vpsraw_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsraw_imm_u256, iemAImpl_vpsraw_imm_u256_fallback;
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsrad_imm_u128, iemAImpl_vpsrad_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsrad_imm_u256, iemAImpl_vpsrad_imm_u256_fallback;

FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsrlw_imm_u128, iemAImpl_vpsrlw_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsrlw_imm_u256, iemAImpl_vpsrlw_imm_u256_fallback;
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsrld_imm_u128, iemAImpl_vpsrld_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsrld_imm_u256, iemAImpl_vpsrld_imm_u256_fallback;
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_vpsrlq_imm_u128, iemAImpl_vpsrlq_imm_u128_fallback;
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpsrlq_imm_u256, iemAImpl_vpsrlq_imm_u256_fallback;
IEM_DECL_IMPL_DEF(void, iemAImpl_vpsrldq_imm_u128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpsrldq_imm_u128_fallback,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpsrldq_imm_u256,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t uShift));
IEM_DECL_IMPL_DEF(void, iemAImpl_vpsrldq_imm_u256_fallback,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t uShift));

FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpermilps_u128,     iemAImpl_vpermilps_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_vpermilps_imm_u128, iemAImpl_vpermilps_imm_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpermilps_u256,     iemAImpl_vpermilps_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256IMM8 iemAImpl_vpermilps_imm_u256, iemAImpl_vpermilps_imm_u256_fallback;

FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpermilpd_u128,     iemAImpl_vpermilpd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128IMM8 iemAImpl_vpermilpd_imm_u128, iemAImpl_vpermilpd_imm_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpermilpd_u256,     iemAImpl_vpermilpd_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256IMM8 iemAImpl_vpermilpd_imm_u256, iemAImpl_vpermilpd_imm_u256_fallback;

FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpsllvd_u128, iemAImpl_vpsllvd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpsllvd_u256, iemAImpl_vpsllvd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpsllvq_u128, iemAImpl_vpsllvq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpsllvq_u256, iemAImpl_vpsllvq_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpsravd_u128, iemAImpl_vpsravd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpsravd_u256, iemAImpl_vpsravd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpsrlvd_u128, iemAImpl_vpsrlvd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpsrlvd_u256, iemAImpl_vpsrlvd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U128     iemAImpl_vpsrlvq_u128, iemAImpl_vpsrlvq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256     iemAImpl_vpsrlvq_u256, iemAImpl_vpsrlvq_u256_fallback;
/** @} */

/** @name Media Odds and Ends
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U8,(uint32_t *puDst, uint8_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U16,(uint32_t *puDst, uint16_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U32,(uint32_t *puDst, uint32_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U64,(uint32_t *puDst, uint64_t uSrc));
FNIEMAIMPLCR32U8  iemAImpl_crc32_u8,  iemAImpl_crc32_u8_fallback;
FNIEMAIMPLCR32U16 iemAImpl_crc32_u16, iemAImpl_crc32_u16_fallback;
FNIEMAIMPLCR32U32 iemAImpl_crc32_u32, iemAImpl_crc32_u32_fallback;
FNIEMAIMPLCR32U64 iemAImpl_crc32_u64, iemAImpl_crc32_u64_fallback;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLF2EFL128,(PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint32_t *pEFlags));
typedef FNIEMAIMPLF2EFL128 *PFNIEMAIMPLF2EFL128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLF2EFL256,(PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint32_t *pEFlags));
typedef FNIEMAIMPLF2EFL256 *PFNIEMAIMPLF2EFL256;
FNIEMAIMPLF2EFL128 iemAImpl_ptest_u128;
FNIEMAIMPLF2EFL256 iemAImpl_vptest_u256, iemAImpl_vptest_u256_fallback;
FNIEMAIMPLF2EFL128 iemAImpl_vtestps_u128, iemAImpl_vtestps_u128_fallback;
FNIEMAIMPLF2EFL256 iemAImpl_vtestps_u256, iemAImpl_vtestps_u256_fallback;
FNIEMAIMPLF2EFL128 iemAImpl_vtestpd_u128, iemAImpl_vtestpd_u128_fallback;
FNIEMAIMPLF2EFL256 iemAImpl_vtestpd_u256, iemAImpl_vtestpd_u256_fallback;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I32U64,(uint32_t uMxCsrIn, int32_t *pi32Dst, const uint64_t *pu64Src)); /* pu64Src is a double precision floating point. */
typedef FNIEMAIMPLSSEF2I32U64 *PFNIEMAIMPLSSEF2I32U64;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I64U64,(uint32_t uMxCsrIn, int64_t *pi64Dst, const uint64_t *pu64Src)); /* pu64Src is a double precision floating point. */
typedef FNIEMAIMPLSSEF2I64U64 *PFNIEMAIMPLSSEF2I64U64;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I32U32,(uint32_t uMxCsrIn, int32_t *pi32Dst, const uint32_t *pu32Src)); /* pu32Src is a single precision floating point. */
typedef FNIEMAIMPLSSEF2I32U32 *PFNIEMAIMPLSSEF2I32U32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I64U32,(uint32_t uMxCsrIn, int64_t *pi64Dst, const uint32_t *pu32Src)); /* pu32Src is a single precision floating point. */
typedef FNIEMAIMPLSSEF2I64U32 *PFNIEMAIMPLSSEF2I64U32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I32R32,(uint32_t uMxCsrIn, int32_t *pi32Dst, PCRTFLOAT32U pr32Src));
typedef FNIEMAIMPLSSEF2I32R32 *PFNIEMAIMPLSSEF2I32R32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I64R32,(uint32_t uMxCsrIn, int64_t *pi64Dst, PCRTFLOAT32U pr32Src));
typedef FNIEMAIMPLSSEF2I64R32 *PFNIEMAIMPLSSEF2I64R32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I32R64,(uint32_t uMxCsrIn, int32_t *pi32Dst, PCRTFLOAT64U pr64Src));
typedef FNIEMAIMPLSSEF2I32R64 *PFNIEMAIMPLSSEF2I32R64;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2I64R64,(uint32_t uMxCsrIn, int64_t *pi64Dst, PCRTFLOAT64U pr64Src));
typedef FNIEMAIMPLSSEF2I64R64 *PFNIEMAIMPLSSEF2I64R64;

FNIEMAIMPLSSEF2I32U64 iemAImpl_cvttsd2si_i32_r64;
FNIEMAIMPLSSEF2I32U64 iemAImpl_cvtsd2si_i32_r64;

FNIEMAIMPLSSEF2I64U64 iemAImpl_cvttsd2si_i64_r64;
FNIEMAIMPLSSEF2I64U64 iemAImpl_cvtsd2si_i64_r64;

FNIEMAIMPLSSEF2I32U32 iemAImpl_cvttss2si_i32_r32;
FNIEMAIMPLSSEF2I32U32 iemAImpl_cvtss2si_i32_r32;

FNIEMAIMPLSSEF2I64U32 iemAImpl_cvttss2si_i64_r32;
FNIEMAIMPLSSEF2I64U32 iemAImpl_cvtss2si_i64_r32;

FNIEMAIMPLSSEF2I32R32 iemAImpl_vcvttss2si_i32_r32, iemAImpl_vcvttss2si_i32_r32_fallback;
FNIEMAIMPLSSEF2I64R32 iemAImpl_vcvttss2si_i64_r32, iemAImpl_vcvttss2si_i64_r32_fallback;
FNIEMAIMPLSSEF2I32R32 iemAImpl_vcvtss2si_i32_r32,  iemAImpl_vcvtss2si_i32_r32_fallback;
FNIEMAIMPLSSEF2I64R32 iemAImpl_vcvtss2si_i64_r32,  iemAImpl_vcvtss2si_i64_r32_fallback;

FNIEMAIMPLSSEF2I32R64 iemAImpl_vcvttss2si_i32_r64, iemAImpl_vcvttss2si_i32_r64_fallback;
FNIEMAIMPLSSEF2I64R64 iemAImpl_vcvttss2si_i64_r64, iemAImpl_vcvttss2si_i64_r64_fallback;
FNIEMAIMPLSSEF2I32R64 iemAImpl_vcvtss2si_i32_r64,  iemAImpl_vcvtss2si_i32_r64_fallback;
FNIEMAIMPLSSEF2I64R64 iemAImpl_vcvtss2si_i64_r64,  iemAImpl_vcvtss2si_i64_r64_fallback;

FNIEMAIMPLSSEF2I32R32 iemAImpl_vcvttsd2si_i32_r32, iemAImpl_vcvttsd2si_i32_r32_fallback;
FNIEMAIMPLSSEF2I64R32 iemAImpl_vcvttsd2si_i64_r32, iemAImpl_vcvttsd2si_i64_r32_fallback;
FNIEMAIMPLSSEF2I32R32 iemAImpl_vcvtsd2si_i32_r32,  iemAImpl_vcvtsd2si_i32_r32_fallback;
FNIEMAIMPLSSEF2I64R32 iemAImpl_vcvtsd2si_i64_r32,  iemAImpl_vcvtsd2si_i64_r32_fallback;

FNIEMAIMPLSSEF2I32R64 iemAImpl_vcvttsd2si_i32_r64, iemAImpl_vcvttsd2si_i32_r64_fallback;
FNIEMAIMPLSSEF2I64R64 iemAImpl_vcvttsd2si_i64_r64, iemAImpl_vcvttsd2si_i64_r64_fallback;
FNIEMAIMPLSSEF2I32R64 iemAImpl_vcvtsd2si_i32_r64,  iemAImpl_vcvtsd2si_i32_r64_fallback;
FNIEMAIMPLSSEF2I64R64 iemAImpl_vcvtsd2si_i64_r64,  iemAImpl_vcvtsd2si_i64_r64_fallback;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2R32I32,(uint32_t uMxCsrIn, PRTFLOAT32U pr32Dst, const int32_t *pi32Src));
typedef FNIEMAIMPLSSEF2R32I32 *PFNIEMAIMPLSSEF2R32I32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2R32I64,(uint32_t uMxCsrIn, PRTFLOAT32U pr32Dst, const int64_t *pi64Src));
typedef FNIEMAIMPLSSEF2R32I64 *PFNIEMAIMPLSSEF2R32I64;

FNIEMAIMPLSSEF2R32I32 iemAImpl_cvtsi2ss_r32_i32;
FNIEMAIMPLSSEF2R32I64 iemAImpl_cvtsi2ss_r32_i64;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLAVXF3XMMI32,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc, const int32_t *pi32Src));
typedef FNIEMAIMPLAVXF3XMMI32 *PFNIEMAIMPLAVXF3XMMI32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLAVXF3XMMI64,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc, const int64_t *pi64Src));
typedef FNIEMAIMPLAVXF3XMMI64 *PFNIEMAIMPLAVXF3XMMI64;

FNIEMAIMPLAVXF3XMMI32 iemAImpl_vcvtsi2ss_u128_i32, iemAImpl_vcvtsi2ss_u128_i32_fallback;
FNIEMAIMPLAVXF3XMMI64 iemAImpl_vcvtsi2ss_u128_i64, iemAImpl_vcvtsi2ss_u128_i64_fallback;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2R64I32,(uint32_t uMxCsrIn, PRTFLOAT64U pr64Dst, const int32_t *pi32Src));
typedef FNIEMAIMPLSSEF2R64I32 *PFNIEMAIMPLSSEF2R64I32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLSSEF2R64I64,(uint32_t uMxCsrIn, PRTFLOAT64U pr64Dst, const int64_t *pi64Src));
typedef FNIEMAIMPLSSEF2R64I64 *PFNIEMAIMPLSSEF2R64I64;

FNIEMAIMPLSSEF2R64I32 iemAImpl_cvtsi2sd_r64_i32;
FNIEMAIMPLSSEF2R64I64 iemAImpl_cvtsi2sd_r64_i64;

FNIEMAIMPLAVXF3XMMI32 iemAImpl_vcvtsi2sd_u128_i32, iemAImpl_vcvtsi2sd_u128_i32_fallback;
FNIEMAIMPLAVXF3XMMI64 iemAImpl_vcvtsi2sd_u128_i64, iemAImpl_vcvtsi2sd_u128_i64_fallback;

IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtps2pd_u128_u64,(uint32_t uMxCsrIn, PX86XMMREG puDst, const uint64_t *pu64Src)); /* Actually two single precision floating point values. */
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtps2pd_u128_u64_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst,  const uint64_t *pu64Src)); /* Actually two single precision floating point values. */
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtps2pd_u256_u128,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtps2pd_u256_u128_fallback,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86XMMREG puSrc));


IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtdq2pd_u128_u64,(uint32_t uMxCsrIn, PX86XMMREG puDst, const uint64_t *pu64Src)); /* Actually two single precision floating point values. */
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtdq2pd_u128_u64_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst,  const uint64_t *pu64Src)); /* Actually two single precision floating point values. */
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtdq2pd_u256_u128,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_vcvtdq2pd_u256_u128_fallback,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86XMMREG puSrc));


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLF2EFLMXCSRR32R32,(uint32_t uMxCsrIn, uint32_t *pfEFlags, RTFLOAT32U uSrc1, RTFLOAT32U uSrc2));
typedef FNIEMAIMPLF2EFLMXCSRR32R32 *PFNIEMAIMPLF2EFLMXCSRR32R32;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLF2EFLMXCSRR64R64,(uint32_t uMxCsrIn, uint32_t *pfEFlags, RTFLOAT64U uSrc1, RTFLOAT64U uSrc2));
typedef FNIEMAIMPLF2EFLMXCSRR64R64 *PFNIEMAIMPLF2EFLMXCSRR64R64;

FNIEMAIMPLF2EFLMXCSRR32R32 iemAImpl_ucomiss_u128;
FNIEMAIMPLF2EFLMXCSRR32R32 iemAImpl_vucomiss_u128, iemAImpl_vucomiss_u128_fallback;

FNIEMAIMPLF2EFLMXCSRR64R64 iemAImpl_ucomisd_u128;
FNIEMAIMPLF2EFLMXCSRR64R64 iemAImpl_vucomisd_u128, iemAImpl_vucomisd_u128_fallback;

FNIEMAIMPLF2EFLMXCSRR32R32 iemAImpl_comiss_u128;
FNIEMAIMPLF2EFLMXCSRR32R32 iemAImpl_vcomiss_u128, iemAImpl_vcomiss_u128_fallback;

FNIEMAIMPLF2EFLMXCSRR64R64 iemAImpl_comisd_u128;
FNIEMAIMPLF2EFLMXCSRR64R64 iemAImpl_vcomisd_u128, iemAImpl_vcomisd_u128_fallback;


typedef struct IEMMEDIAF2XMMSRC
{
    X86XMMREG               uSrc1;
    X86XMMREG               uSrc2;
} IEMMEDIAF2XMMSRC;
typedef IEMMEDIAF2XMMSRC *PIEMMEDIAF2XMMSRC;
typedef const IEMMEDIAF2XMMSRC *PCIEMMEDIAF2XMMSRC;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF3XMMIMM8,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCIEMMEDIAF2XMMSRC puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAF3XMMIMM8 *PFNIEMAIMPLMEDIAF3XMMIMM8;


typedef struct IEMMEDIAF2YMMSRC
{
    X86YMMREG               uSrc1;
    X86YMMREG               uSrc2;
} IEMMEDIAF2YMMSRC;
typedef IEMMEDIAF2YMMSRC *PIEMMEDIAF2YMMSRC;
typedef const IEMMEDIAF2YMMSRC *PCIEMMEDIAF2YMMSRC;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF3YMMIMM8,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCIEMMEDIAF2YMMSRC puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAF3YMMIMM8 *PFNIEMAIMPLMEDIAF3YMMIMM8;


FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_cmpps_u128;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_cmppd_u128;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_cmpss_u128;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_cmpsd_u128;

FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_vcmpps_u128, iemAImpl_vcmpps_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_vcmppd_u128, iemAImpl_vcmppd_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_vcmpss_u128, iemAImpl_vcmpss_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_vcmpsd_u128, iemAImpl_vcmpsd_u128_fallback;

FNIEMAIMPLMEDIAF3YMMIMM8 iemAImpl_vcmpps_u256, iemAImpl_vcmpps_u256_fallback;
FNIEMAIMPLMEDIAF3YMMIMM8 iemAImpl_vcmppd_u256, iemAImpl_vcmppd_u256_fallback;

FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_roundss_u128;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_roundsd_u128;

FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_dpps_u128,     iemAImpl_dpps_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8 iemAImpl_dppd_u128,     iemAImpl_dppd_u128_fallback;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF2U128IMM8,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAF2U128IMM8 *PFNIEMAIMPLMEDIAF2U128IMM8;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMEDIAF2U256IMM8,(uint32_t uMxCsrIn, PX86YMMREG puDst, PCX86YMMREG puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAF2U256IMM8 *PFNIEMAIMPLMEDIAF2U256IMM8;


FNIEMAIMPLMEDIAF2U128IMM8 iemAImpl_roundps_u128,  iemAImpl_roundps_u128_fallback;
FNIEMAIMPLMEDIAF2U128IMM8 iemAImpl_roundpd_u128,  iemAImpl_roundpd_u128_fallback;

FNIEMAIMPLMEDIAF2U128IMM8 iemAImpl_vroundps_u128, iemAImpl_vroundps_u128_fallback;
FNIEMAIMPLMEDIAF2U128IMM8 iemAImpl_vroundpd_u128, iemAImpl_vroundpd_u128_fallback;

FNIEMAIMPLMEDIAF2U256IMM8 iemAImpl_vroundps_u256, iemAImpl_vroundps_u256_fallback;
FNIEMAIMPLMEDIAF2U256IMM8 iemAImpl_vroundpd_u256, iemAImpl_vroundpd_u256_fallback;

FNIEMAIMPLMEDIAF3XMMIMM8  iemAImpl_vroundss_u128, iemAImpl_vroundss_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8  iemAImpl_vroundsd_u128, iemAImpl_vroundsd_u128_fallback;

FNIEMAIMPLMEDIAF3XMMIMM8  iemAImpl_vdpps_u128,     iemAImpl_vdpps_u128_fallback;
FNIEMAIMPLMEDIAF3XMMIMM8  iemAImpl_vdppd_u128,     iemAImpl_vdppd_u128_fallback;

FNIEMAIMPLMEDIAF3YMMIMM8  iemAImpl_vdpps_u256,     iemAImpl_vdpps_u256_fallback;


typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMXCSRU64U128,(uint32_t fMxCsrIn, uint64_t *pu64Dst, PCX86XMMREG pSrc));
typedef FNIEMAIMPLMXCSRU64U128 *PFNIEMAIMPLMXCSRU64U128;

FNIEMAIMPLMXCSRU64U128 iemAImpl_cvtpd2pi_u128;
FNIEMAIMPLMXCSRU64U128 iemAImpl_cvttpd2pi_u128;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMXCSRU128U64,(uint32_t fMxCsrIn, PX86XMMREG pDst, uint64_t u64Src));
typedef FNIEMAIMPLMXCSRU128U64 *PFNIEMAIMPLMXCSRU128U64;

FNIEMAIMPLMXCSRU128U64 iemAImpl_cvtpi2ps_u128;
FNIEMAIMPLMXCSRU128U64 iemAImpl_cvtpi2pd_u128;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLMXCSRU64U64,(uint32_t fMxCsrIn, uint64_t *pu64Dst, uint64_t u64Src));
typedef FNIEMAIMPLMXCSRU64U64 *PFNIEMAIMPLMXCSRU64U64;

FNIEMAIMPLMXCSRU64U64 iemAImpl_cvtps2pi_u128;
FNIEMAIMPLMXCSRU64U64 iemAImpl_cvttps2pi_u128;

/** @} */


/** @name Function tables.
 * @{
 */

/**
 * Function table for a binary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPBINSIZES
{
    PFNIEMAIMPLBINU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLBINU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLBINU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLBINU64 pfnNormalU64,   pfnLockedU64;
} IEMOPBINSIZES;
/** Pointer to a binary operator function table. */
typedef IEMOPBINSIZES const *PCIEMOPBINSIZES;


/**
 * Function table for a unary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPUNARYSIZES
{
    PFNIEMAIMPLUNARYU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLUNARYU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLUNARYU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLUNARYU64 pfnNormalU64,   pfnLockedU64;
} IEMOPUNARYSIZES;
/** Pointer to a unary operator function table. */
typedef IEMOPUNARYSIZES const *PCIEMOPUNARYSIZES;


/**
 * Function table for a shift operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPSHIFTSIZES
{
    PFNIEMAIMPLSHIFTU8  pfnNormalU8;
    PFNIEMAIMPLSHIFTU16 pfnNormalU16;
    PFNIEMAIMPLSHIFTU32 pfnNormalU32;
    PFNIEMAIMPLSHIFTU64 pfnNormalU64;
} IEMOPSHIFTSIZES;
/** Pointer to a shift operator function table. */
typedef IEMOPSHIFTSIZES const *PCIEMOPSHIFTSIZES;


/**
 * Function table for a multiplication or division operation.
 */
typedef struct IEMOPMULDIVSIZES
{
    PFNIEMAIMPLMULDIVU8  pfnU8;
    PFNIEMAIMPLMULDIVU16 pfnU16;
    PFNIEMAIMPLMULDIVU32 pfnU32;
    PFNIEMAIMPLMULDIVU64 pfnU64;
} IEMOPMULDIVSIZES;
/** Pointer to a multiplication or division operation function table. */
typedef IEMOPMULDIVSIZES const *PCIEMOPMULDIVSIZES;


/**
 * Function table for a double precision shift operator providing implementation
 * based on operand size.
 */
typedef struct IEMOPSHIFTDBLSIZES
{
    PFNIEMAIMPLSHIFTDBLU16 pfnNormalU16;
    PFNIEMAIMPLSHIFTDBLU32 pfnNormalU32;
    PFNIEMAIMPLSHIFTDBLU64 pfnNormalU64;
} IEMOPSHIFTDBLSIZES;
/** Pointer to a double precision shift function table. */
typedef IEMOPSHIFTDBLSIZES const *PCIEMOPSHIFTDBLSIZES;


/**
 * Function table for media instruction taking two full sized media source
 * registers and one full sized destination register (AVX).
 */
typedef struct IEMOPMEDIAF3
{
    PFNIEMAIMPLMEDIAF3U128 pfnU128;
    PFNIEMAIMPLMEDIAF3U256 pfnU256;
} IEMOPMEDIAF3;
/** Pointer to a media operation function table for 3 full sized ops (AVX). */
typedef IEMOPMEDIAF3 const *PCIEMOPMEDIAF3;

/** @def IEMOPMEDIAF3_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF3 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAF3_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAF3_INIT_VARS_EX */
#define IEMOPMEDIAF3_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAF3_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                              RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))


/**
 * Function table for media instruction taking one full sized media source
 * registers and one full sized destination register (AVX).
 */
typedef struct IEMOPMEDIAF2
{
    PFNIEMAIMPLMEDIAF2U128 pfnU128;
    PFNIEMAIMPLMEDIAF2U256 pfnU256;
} IEMOPMEDIAF2;
/** Pointer to a media operation function table for 2 full sized ops (AVX). */
typedef IEMOPMEDIAF2 const *PCIEMOPMEDIAF2;

/** @def IEMOPMEDIAF2_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAF2_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF2 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAF2_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAF2_INIT_VARS_EX */
#define IEMOPMEDIAF2_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAF2_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                              RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))


/**
 * Function table for media instruction taking two full sized media source
 * registers and one full sized destination register, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF3
{
    PFNIEMAIMPLMEDIAOPTF3U128 pfnU128;
    PFNIEMAIMPLMEDIAOPTF3U256 pfnU256;
} IEMOPMEDIAOPTF3;
/** Pointer to a media operation function table for 3 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF3 const *PCIEMOPMEDIAOPTF3;

/** @def IEMOPMEDIAOPTF3_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF3_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF3_INIT_VARS_EX */
#define IEMOPMEDIAOPTF3_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF3_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                 RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))

/**
 * Function table for media instruction taking one full sized media source
 * registers and one full sized destination register, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF2
{
    PFNIEMAIMPLMEDIAOPTF2U128 pfnU128;
    PFNIEMAIMPLMEDIAOPTF2U256 pfnU256;
} IEMOPMEDIAOPTF2;
/** Pointer to a media operation function table for 2 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF2 const *PCIEMOPMEDIAOPTF2;

/** @def IEMOPMEDIAOPTF2_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF2_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF2_INIT_VARS_EX */
#define IEMOPMEDIAOPTF2_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF2_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                 RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))


/**
 * Function table for media instruction taking one full sized media source
 * register and one full sized destination register and an 8-bit immediate (AVX).
 */
typedef struct IEMOPMEDIAF2IMM8
{
    PFNIEMAIMPLMEDIAF2U128IMM8 pfnU128;
    PFNIEMAIMPLMEDIAF2U256IMM8 pfnU256;
} IEMOPMEDIAF2IMM8;
/** Pointer to a media operation function table for 2 full sized ops (AVX). */
typedef IEMOPMEDIAF2IMM8 const *PCIEMOPMEDIAF2IMM8;

/** @def IEMOPMEDIAF2IMM8_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAF2IMM8_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF2IMM8 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAF2IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAF2IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF2IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAF2IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAF2IMM8_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAF2IMM8_INIT_VARS_EX */
#define IEMOPMEDIAF2IMM8_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAF2IMM8_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                  RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))


/**
 * Function table for media instruction taking one full sized media source
 * register and one full sized destination register and an 8-bit immediate, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF2IMM8
{
    PFNIEMAIMPLMEDIAOPTF2U128IMM8 pfnU128;
    PFNIEMAIMPLMEDIAOPTF2U256IMM8 pfnU256;
} IEMOPMEDIAOPTF2IMM8;
/** Pointer to a media operation function table for 2 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF2IMM8 const *PCIEMOPMEDIAOPTF2IMM8;

/** @def IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2IMM8 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF2IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF2IMM8_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX */
#define IEMOPMEDIAOPTF2IMM8_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF2IMM8_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_imm_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_imm_u256),\
                                     RT_CONCAT3(iemAImpl_,a_InstrNm,_imm_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_imm_u256_fallback))

/**
 * Function table for media instruction taking two full sized media source
 * registers and one full sized destination register and an 8-bit immediate, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF3IMM8
{
    PFNIEMAIMPLMEDIAOPTF3U128IMM8 pfnU128;
    PFNIEMAIMPLMEDIAOPTF3U256IMM8 pfnU256;
} IEMOPMEDIAOPTF3IMM8;
/** Pointer to a media operation function table for 3 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF3IMM8 const *PCIEMOPMEDIAOPTF3IMM8;

/** @def IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3IMM8 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF3IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3IMM8 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF3IMM8_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX */
#define IEMOPMEDIAOPTF3IMM8_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF3IMM8_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                     RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))
/** @} */


/**
 * Function table for blend type instruction taking three full sized media source
 * registers and one full sized destination register, but no additional state
 * (AVX).
 */
typedef struct IEMOPBLENDOP
{
    PFNIEMAIMPLAVXBLENDU128 pfnU128;
    PFNIEMAIMPLAVXBLENDU256 pfnU256;
} IEMOPBLENDOP;
/** Pointer to a media operation function table for 4 full sized ops (AVX). */
typedef IEMOPBLENDOP const *PCIEMOPBLENDOP;

/** @def IEMOPBLENDOP_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPBLENDOP_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPBLENDOP const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPBLENDOP const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPBLENDOP_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPBLENDOP const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPBLENDOP_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPBLENDOP_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPBLENDOP_INIT_VARS_EX */
#define IEMOPBLENDOP_INIT_VARS(a_InstrNm) \
    IEMOPBLENDOP_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                              RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))


/** @name SSE/AVX single/double precision floating point operations.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPSSEF2U128,(uint32_t uMxCsrIn, PX86XMMREG pResult, PCX86XMMREG puSrc1, PCX86XMMREG puSrc2));
typedef FNIEMAIMPLFPSSEF2U128  *PFNIEMAIMPLFPSSEF2U128;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPSSEF2U128R32,(uint32_t uMxCsrIn, PX86XMMREG Result, PCX86XMMREG puSrc1, PCRTFLOAT32U pr32Src2));
typedef FNIEMAIMPLFPSSEF2U128R32  *PFNIEMAIMPLFPSSEF2U128R32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPSSEF2U128R64,(uint32_t uMxCsrIn, PX86XMMREG pResult, PCX86XMMREG puSrc1, PCRTFLOAT64U pr64Src2));
typedef FNIEMAIMPLFPSSEF2U128R64  *PFNIEMAIMPLFPSSEF2U128R64;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPAVXF3U128,(uint32_t uMxCsrIn, PX86XMMREG pResult, PCX86XMMREG puSrc1, PCX86XMMREG puSrc2));
typedef FNIEMAIMPLFPAVXF3U128  *PFNIEMAIMPLFPAVXF3U128;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPAVXF3U128R32,(uint32_t uMxCsrIn, PX86XMMREG pResult, PCX86XMMREG puSrc1, PCRTFLOAT32U pr32Src2));
typedef FNIEMAIMPLFPAVXF3U128R32  *PFNIEMAIMPLFPAVXF3U128R32;
typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPAVXF3U128R64,(uint32_t uMxCsrIn, PX86XMMREG pResult, PCX86XMMREG puSrc1, PCRTFLOAT64U pr64Src2));
typedef FNIEMAIMPLFPAVXF3U128R64  *PFNIEMAIMPLFPAVXF3U128R64;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPAVXF3U256,(uint32_t uMxCsrIn, PX86YMMREG pResult, PCX86YMMREG puSrc1, PCX86YMMREG puSrc2));
typedef FNIEMAIMPLFPAVXF3U256  *PFNIEMAIMPLFPAVXF3U256;

FNIEMAIMPLFPSSEF2U128 iemAImpl_addps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_addpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_mulps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_mulpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_subps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_subpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_minps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_minpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_divps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_divpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_maxps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_maxpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_haddps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_haddpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_hsubps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_hsubpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_sqrtps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_rsqrtps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_sqrtpd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_rcpps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_addsubps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_addsubpd_u128;

FNIEMAIMPLFPSSEF2U128 iemAImpl_cvtpd2ps_u128;
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_cvtps2pd_u128,(uint32_t uMxCsrIn, PX86XMMREG pResult, uint64_t const *pu64Src));

FNIEMAIMPLFPSSEF2U128 iemAImpl_cvtdq2ps_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_cvtps2dq_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_cvttps2dq_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_cvttpd2dq_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_cvtdq2pd_u128;
FNIEMAIMPLFPSSEF2U128 iemAImpl_cvtpd2dq_u128;

FNIEMAIMPLFPSSEF2U128R32 iemAImpl_addss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_addsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_mulss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_mulsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_subss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_subsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_minss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_minsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_divss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_divsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_maxss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_maxsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_cvtss2sd_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_cvtsd2ss_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_sqrtss_u128_r32;
FNIEMAIMPLFPSSEF2U128R64 iemAImpl_sqrtsd_u128_r64;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_rsqrtss_u128_r32;
FNIEMAIMPLFPSSEF2U128R32 iemAImpl_rcpss_u128_r32;

FNIEMAIMPLMEDIAF3U128 iemAImpl_vaddps_u128, iemAImpl_vaddps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vaddpd_u128, iemAImpl_vaddpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vmulps_u128, iemAImpl_vmulps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vmulpd_u128, iemAImpl_vmulpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vsubps_u128, iemAImpl_vsubps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vsubpd_u128, iemAImpl_vsubpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vminps_u128, iemAImpl_vminps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vminpd_u128, iemAImpl_vminpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vdivps_u128, iemAImpl_vdivps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vdivpd_u128, iemAImpl_vdivpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vmaxps_u128, iemAImpl_vmaxps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vmaxpd_u128, iemAImpl_vmaxpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vhaddps_u128, iemAImpl_vhaddps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vhaddpd_u128, iemAImpl_vhaddpd_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vhsubps_u128, iemAImpl_vhsubps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vhsubpd_u128, iemAImpl_vhsubpd_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vsqrtps_u128, iemAImpl_vsqrtps_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vsqrtpd_u128, iemAImpl_vsqrtpd_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vrsqrtps_u128,  iemAImpl_vrsqrtps_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vrcpps_u128,    iemAImpl_vrcpps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vaddsubps_u128, iemAImpl_vaddsubps_u128_fallback;
FNIEMAIMPLMEDIAF3U128 iemAImpl_vaddsubpd_u128, iemAImpl_vaddsubpd_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vcvtdq2ps_u128, iemAImpl_vcvtdq2ps_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vcvtps2dq_u128, iemAImpl_vcvtps2dq_u128_fallback;
FNIEMAIMPLMEDIAF2U128 iemAImpl_vcvttps2dq_u128, iemAImpl_vcvttps2dq_u128_fallback;
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2ps_u128_u128,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2ps_u128_u128_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvttpd2dq_u128_u128,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvttpd2dq_u128_u128_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2dq_u128_u128,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2dq_u128_u128_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86XMMREG puSrc));


FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vaddss_u128_r32, iemAImpl_vaddss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vaddsd_u128_r64, iemAImpl_vaddsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vmulss_u128_r32, iemAImpl_vmulss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vmulsd_u128_r64, iemAImpl_vmulsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vsubss_u128_r32, iemAImpl_vsubss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vsubsd_u128_r64, iemAImpl_vsubsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vminss_u128_r32, iemAImpl_vminss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vminsd_u128_r64, iemAImpl_vminsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vdivss_u128_r32, iemAImpl_vdivss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vdivsd_u128_r64, iemAImpl_vdivsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vmaxss_u128_r32, iemAImpl_vmaxss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vmaxsd_u128_r64, iemAImpl_vmaxsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vsqrtss_u128_r32, iemAImpl_vsqrtss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vsqrtsd_u128_r64, iemAImpl_vsqrtsd_u128_r64_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vrsqrtss_u128_r32, iemAImpl_vrsqrtss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vrcpss_u128_r32,   iemAImpl_vrcpss_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R32 iemAImpl_vcvtss2sd_u128_r32, iemAImpl_vcvtss2sd_u128_r32_fallback;
FNIEMAIMPLFPAVXF3U128R64 iemAImpl_vcvtsd2ss_u128_r64, iemAImpl_vcvtsd2ss_u128_r64_fallback;


FNIEMAIMPLFPAVXF3U256 iemAImpl_vaddps_u256, iemAImpl_vaddps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vaddpd_u256, iemAImpl_vaddpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vmulps_u256, iemAImpl_vmulps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vmulpd_u256, iemAImpl_vmulpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vsubps_u256, iemAImpl_vsubps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vsubpd_u256, iemAImpl_vsubpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vminps_u256, iemAImpl_vminps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vminpd_u256, iemAImpl_vminpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vdivps_u256, iemAImpl_vdivps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vdivpd_u256, iemAImpl_vdivpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vmaxps_u256, iemAImpl_vmaxps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vmaxpd_u256, iemAImpl_vmaxpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vhaddps_u256, iemAImpl_vhaddps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vhaddpd_u256, iemAImpl_vhaddpd_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vhsubps_u256, iemAImpl_vhsubps_u256_fallback;
FNIEMAIMPLFPAVXF3U256 iemAImpl_vhsubpd_u256, iemAImpl_vhsubpd_u256_fallback;
FNIEMAIMPLMEDIAF3U256 iemAImpl_vaddsubps_u256, iemAImpl_vaddsubps_u256_fallback;
FNIEMAIMPLMEDIAF3U256 iemAImpl_vaddsubpd_u256, iemAImpl_vaddsubpd_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vsqrtps_u256, iemAImpl_vsqrtps_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vsqrtpd_u256, iemAImpl_vsqrtpd_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vrsqrtps_u256,  iemAImpl_vrsqrtps_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vrcpps_u256,    iemAImpl_vrcpps_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vcvtdq2ps_u256,  iemAImpl_vcvtdq2ps_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vcvtps2dq_u256,  iemAImpl_vcvtps2dq_u256_fallback;
FNIEMAIMPLMEDIAF2U256 iemAImpl_vcvttps2dq_u256, iemAImpl_vcvttps2dq_u256_fallback;
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2ps_u128_u256,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2ps_u128_u256_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvttpd2dq_u128_u256,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvttpd2dq_u128_u256_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2dq_u128_u256,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
IEM_DECL_IMPL_PROTO(uint32_t, iemAImpl_vcvtpd2dq_u128_u256_fallback,(uint32_t uMxCsrIn, PX86XMMREG puDst, PCX86YMMREG puSrc));
/** @} */


/** @name Misc Helpers
 * @{  */

/** @def IEM_GET_INSTR_LEN
 * Gets the instruction length. */
#ifdef IEM_WITH_CODE_TLB
# define IEM_GET_INSTR_LEN(a_pVCpu)         ((a_pVCpu)->iem.s.offInstrNextByte - (uint32_t)(int32_t)(a_pVCpu)->iem.s.offCurInstrStart)
#else
# define IEM_GET_INSTR_LEN(a_pVCpu)         ((a_pVCpu)->iem.s.offOpcode)
#endif

/**
 * Gets the CPU mode (from fExec) as a IEMMODE value.
 *
 * @returns IEMMODE
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_CPU_MODE(a_pVCpu)           ((a_pVCpu)->iem.s.fExec & IEM_F_MODE_X86_CPUMODE_MASK)

/**
 * Check if we're currently executing in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_REAL_OR_V86_MODE(a_pVCpu)    ((  ((a_pVCpu)->iem.s.fExec  ^ IEM_F_MODE_X86_PROT_MASK) \
                                              & (IEM_F_MODE_X86_V86_MASK | IEM_F_MODE_X86_PROT_MASK)) != 0)

/**
 * Check if we're currently executing in virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_V86_MODE(a_pVCpu)            (((a_pVCpu)->iem.s.fExec & IEM_F_MODE_X86_V86_MASK) != 0)

/**
 * Check if we're currently executing in long mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_LONG_MODE(a_pVCpu)           (CPUMIsGuestInLongModeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if we're currently executing in a 16-bit code segment.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_16BIT_CODE(a_pVCpu)          (IEM_GET_CPU_MODE(a_pVCpu) == IEMMODE_16BIT)

/**
 * Check if we're currently executing in a 32-bit code segment.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_32BIT_CODE(a_pVCpu)          (IEM_GET_CPU_MODE(a_pVCpu) == IEMMODE_32BIT)

/**
 * Check if we're currently executing in a 64-bit code segment.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_64BIT_CODE(a_pVCpu)          (IEM_GET_CPU_MODE(a_pVCpu) == IEMMODE_64BIT)

/**
 * Check if we're currently executing in real mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_REAL_MODE(a_pVCpu)           (!((a_pVCpu)->iem.s.fExec & IEM_F_MODE_X86_PROT_MASK))

/**
 * Gets the current protection level (CPL).
 *
 * @returns 0..3
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_CPL(a_pVCpu)                (((a_pVCpu)->iem.s.fExec >> IEM_F_X86_CPL_SHIFT) & IEM_F_X86_CPL_SMASK)

/**
 * Sets the current protection level (CPL).
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_SET_CPL(a_pVCpu, a_uCpl) \
    do { (a_pVCpu)->iem.s.fExec = ((a_pVCpu)->iem.s.fExec & ~IEM_F_X86_CPL_MASK) | ((a_uCpl) << IEM_F_X86_CPL_SHIFT); } while (0)

/**
 * Returns a (const) pointer to the CPUMFEATURES for the guest CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_GUEST_CPU_FEATURES(a_pVCpu) (&((a_pVCpu)->CTX_SUFF(pVM)->cpum.ro.GuestFeatures))

/**
 * Returns a (const) pointer to the CPUMFEATURES for the host CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_HOST_CPU_FEATURES(a_pVCpu)  (&g_CpumHostFeatures.s)

/**
 * Evaluates to true if we're presenting an Intel CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_INTEL(a_pVCpu)     ( (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL )

/**
 * Evaluates to true if we're presenting an AMD CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_AMD(a_pVCpu)       ( (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_AMD || (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_HYGON )

/**
 * Check if the address is canonical.
 */
#define IEM_IS_CANONICAL(a_u64Addr)         X86_IS_CANONICAL(a_u64Addr)

/** Checks if the ModR/M byte is in register mode or not.  */
#define IEM_IS_MODRM_REG_MODE(a_bRm)        ( ((a_bRm) & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT) )
/** Checks if the ModR/M byte is in memory mode or not.  */
#define IEM_IS_MODRM_MEM_MODE(a_bRm)        ( ((a_bRm) & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT) )

/**
 * Gets the register (reg) part of a ModR/M encoding, with REX.R added in.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_REG(a_pVCpu, a_bRm)   ( (((a_bRm) >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | (a_pVCpu)->iem.s.uRexReg )
/**
 * Gets the r/m part of a ModR/M encoding as a register index, with REX.B added in.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_RM(a_pVCpu, a_bRm)    ( ((a_bRm) & X86_MODRM_RM_MASK) | (a_pVCpu)->iem.s.uRexB )

/**
 * Gets the register (reg) part of a ModR/M encoding, without REX.R.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_REG_8(a_bRm)          ( (((a_bRm) >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) )
/**
 * Gets the r/m part of a ModR/M encoding as a register index, without REX.B.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_RM_8(a_bRm)           ( ((a_bRm) & X86_MODRM_RM_MASK) )

/**
 * Gets the register (reg) part of a ModR/M encoding as an extended 8-bit
 * register index, with REX.R added in.
 *
 * For use during decoding.
 *
 * @see iemGRegRefU8Ex, iemGRegFetchU8Ex, iemGRegStoreU8Ex
 */
#define IEM_GET_MODRM_REG_EX8(a_pVCpu, a_bRm) \
    (   (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX) \
     || !((a_bRm) & (4 << X86_MODRM_REG_SHIFT)) /* IEM_GET_MODRM_REG(pVCpu, a_bRm) < 4 */ \
     ? IEM_GET_MODRM_REG(pVCpu, a_bRm) : (((a_bRm) >> X86_MODRM_REG_SHIFT) & 3) | 16)
/**
 * Gets the r/m part of a ModR/M encoding as an extended 8-bit register index,
 * with REX.B added in.
 *
 * For use during decoding.
 *
 * @see iemGRegRefU8Ex, iemGRegFetchU8Ex, iemGRegStoreU8Ex
 */
#define IEM_GET_MODRM_RM_EX8(a_pVCpu, a_bRm) \
    (   (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX) \
     || !((a_bRm) & 4) /* IEM_GET_MODRM_RM(pVCpu, a_bRm) < 4 */ \
     ? IEM_GET_MODRM_RM(pVCpu, a_bRm) : ((a_bRm) & 3) | 16)

/**
 * Combines the prefix REX and ModR/M byte for passing to
 * iemOpHlpCalcRmEffAddrThreadedAddr64().
 *
 * @returns The ModRM byte but with bit 3 set to REX.B and bit 4 to REX.X.
 *          The two bits are part of the REG sub-field, which isn't needed in
 *          iemOpHlpCalcRmEffAddrThreadedAddr64().
 *
 * For use during decoding/recompiling.
 */
#define IEM_GET_MODRM_EX(a_pVCpu, a_bRm) \
    (  ((a_bRm) & ~X86_MODRM_REG_MASK) \
     | (uint8_t)( (pVCpu->iem.s.fPrefixes & (IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X)) >> (25 - 3) ) )
AssertCompile(IEM_OP_PRF_REX_B == RT_BIT_32(25));
AssertCompile(IEM_OP_PRF_REX_X == RT_BIT_32(26));

/**
 * Gets the effective VEX.VVVV value.
 *
 * The 4th bit is ignored if not 64-bit code.
 * @returns effective V-register value.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_EFFECTIVE_VVVV(a_pVCpu) \
    (IEM_IS_64BIT_CODE(a_pVCpu) ? (a_pVCpu)->iem.s.uVex3rdReg : (a_pVCpu)->iem.s.uVex3rdReg & 7)


/**
 * Gets the register (reg) part of a the special 4th register byte used by
 * vblendvps and vblendvpd.
 *
 * For use during decoding.
 */
#define IEM_GET_IMM8_REG(a_pVCpu, a_bRegImm8) \
    (IEM_IS_64BIT_CODE(a_pVCpu) ? (a_bRegImm8) >> 4 : ((a_bRegImm8) >> 4) & 7)


/**
 * Checks if we're executing inside an AMD-V or VT-x guest.
 */
#if defined(VBOX_WITH_NESTED_HWVIRT_VMX) || defined(VBOX_WITH_NESTED_HWVIRT_SVM)
# define IEM_IS_IN_GUEST(a_pVCpu)       RT_BOOL((a_pVCpu)->iem.s.fExec & IEM_F_X86_CTX_IN_GUEST)
#else
# define IEM_IS_IN_GUEST(a_pVCpu)       false
#endif


#ifdef VBOX_WITH_NESTED_HWVIRT_VMX

/**
 * Check if the guest has entered VMX root operation.
 */
# define IEM_VMX_IS_ROOT_MODE(a_pVCpu)      (CPUMIsGuestInVmxRootMode(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if the guest has entered VMX non-root operation.
 */
# define IEM_VMX_IS_NON_ROOT_MODE(a_pVCpu)  (   ((a_pVCpu)->iem.s.fExec & (IEM_F_X86_CTX_VMX | IEM_F_X86_CTX_IN_GUEST)) \
                                             ==                           (IEM_F_X86_CTX_VMX | IEM_F_X86_CTX_IN_GUEST) )

/**
 * Check if the nested-guest has the given Pin-based VM-execution control set.
 */
# define IEM_VMX_IS_PINCTLS_SET(a_pVCpu, a_PinCtl)  (CPUMIsGuestVmxPinCtlsSet(IEM_GET_CTX(a_pVCpu), (a_PinCtl)))

/**
 * Check if the nested-guest has the given Processor-based VM-execution control set.
 */
# define IEM_VMX_IS_PROCCTLS_SET(a_pVCpu, a_ProcCtl) (CPUMIsGuestVmxProcCtlsSet(IEM_GET_CTX(a_pVCpu), (a_ProcCtl)))

/**
 * Check if the nested-guest has the given Secondary Processor-based VM-execution
 * control set.
 */
# define IEM_VMX_IS_PROCCTLS2_SET(a_pVCpu, a_ProcCtl2) (CPUMIsGuestVmxProcCtls2Set(IEM_GET_CTX(a_pVCpu), (a_ProcCtl2)))

/** Gets the guest-physical address of the shadows VMCS for the given VCPU. */
# define IEM_VMX_GET_SHADOW_VMCS(a_pVCpu)           ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysShadowVmcs)

/** Whether a shadow VMCS is present for the given VCPU. */
# define IEM_VMX_HAS_SHADOW_VMCS(a_pVCpu)           RT_BOOL(IEM_VMX_GET_SHADOW_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the VMXON region pointer. */
# define IEM_VMX_GET_VMXON_PTR(a_pVCpu)             ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)

/** Gets the guest-physical address of the current VMCS for the given VCPU. */
# define IEM_VMX_GET_CURRENT_VMCS(a_pVCpu)          ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs)

/** Whether a current VMCS is present for the given VCPU. */
# define IEM_VMX_HAS_CURRENT_VMCS(a_pVCpu)          RT_BOOL(IEM_VMX_GET_CURRENT_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Assigns the guest-physical address of the current VMCS for the given VCPU. */
# define IEM_VMX_SET_CURRENT_VMCS(a_pVCpu, a_GCPhysVmcs) \
    do \
    { \
        Assert((a_GCPhysVmcs) != NIL_RTGCPHYS); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = (a_GCPhysVmcs); \
    } while (0)

/** Clears any current VMCS for the given VCPU. */
# define IEM_VMX_CLEAR_CURRENT_VMCS(a_pVCpu) \
    do \
    { \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = NIL_RTGCPHYS; \
    } while (0)

/**
 * Invokes the VMX VM-exit handler for an instruction intercept.
 */
# define IEM_VMX_VMEXIT_INSTR_RET(a_pVCpu, a_uExitReason, a_cbInstr) \
    do { return iemVmxVmexitInstr((a_pVCpu), (a_uExitReason), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for an instruction intercept where the
 * instruction provides additional VM-exit information.
 */
# define IEM_VMX_VMEXIT_INSTR_NEEDS_INFO_RET(a_pVCpu, a_uExitReason, a_uInstrId, a_cbInstr) \
    do { return iemVmxVmexitInstrNeedsInfo((a_pVCpu), (a_uExitReason), (a_uInstrId), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for a task switch.
 */
# define IEM_VMX_VMEXIT_TASK_SWITCH_RET(a_pVCpu, a_enmTaskSwitch, a_SelNewTss, a_cbInstr) \
    do { return iemVmxVmexitTaskSwitch((a_pVCpu), (a_enmTaskSwitch), (a_SelNewTss), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for MWAIT.
 */
# define IEM_VMX_VMEXIT_MWAIT_RET(a_pVCpu, a_fMonitorArmed, a_cbInstr) \
    do { return iemVmxVmexitInstrMwait((a_pVCpu), (a_fMonitorArmed), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for EPT faults.
 */
# define IEM_VMX_VMEXIT_EPT_RET(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr) \
    do { return iemVmxVmexitEpt(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr); } while (0)

/**
 * Invokes the VMX VM-exit handler.
 */
# define IEM_VMX_VMEXIT_TRIPLE_FAULT_RET(a_pVCpu, a_uExitReason, a_uExitQual) \
    do { return iemVmxVmexit((a_pVCpu), (a_uExitReason), (a_uExitQual)); } while (0)

#else
# define IEM_VMX_IS_ROOT_MODE(a_pVCpu)                                          (false)
# define IEM_VMX_IS_NON_ROOT_MODE(a_pVCpu)                                      (false)
# define IEM_VMX_IS_PINCTLS_SET(a_pVCpu, a_cbInstr)                             (false)
# define IEM_VMX_IS_PROCCTLS_SET(a_pVCpu, a_cbInstr)                            (false)
# define IEM_VMX_IS_PROCCTLS2_SET(a_pVCpu, a_cbInstr)                           (false)
# define IEM_VMX_VMEXIT_INSTR_RET(a_pVCpu, a_uExitReason, a_cbInstr)            do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_INSTR_NEEDS_INFO_RET(a_pVCpu, a_uExitReason, a_uInstrId, a_cbInstr)  do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_TASK_SWITCH_RET(a_pVCpu, a_enmTaskSwitch, a_SelNewTss, a_cbInstr)    do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_MWAIT_RET(a_pVCpu, a_fMonitorArmed, a_cbInstr)          do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_EPT_RET(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr)       do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_TRIPLE_FAULT_RET(a_pVCpu, a_uExitReason, a_uExitQual)   do { return VERR_VMX_IPE_1; } while (0)

#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Checks if we're executing a guest using AMD-V.
 */
# define IEM_SVM_IS_IN_GUEST(a_pVCpu) (   (a_pVCpu->iem.s.fExec & (IEM_F_X86_CTX_SVM | IEM_F_X86_CTX_IN_GUEST)) \
                                       ==                         (IEM_F_X86_CTX_SVM | IEM_F_X86_CTX_IN_GUEST))
/**
 * Check if an SVM control/instruction intercept is set.
 */
# define IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmCtrlInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_Intercept)))

/**
 * Check if an SVM read CRx intercept is set.
 */
# define IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, a_uCr) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmReadCRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uCr)))

/**
 * Check if an SVM write CRx intercept is set.
 */
# define IEM_SVM_IS_WRITE_CR_INTERCEPT_SET(a_pVCpu, a_uCr) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmWriteCRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uCr)))

/**
 * Check if an SVM read DRx intercept is set.
 */
# define IEM_SVM_IS_READ_DR_INTERCEPT_SET(a_pVCpu, a_uDr) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmReadDRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uDr)))

/**
 * Check if an SVM write DRx intercept is set.
 */
# define IEM_SVM_IS_WRITE_DR_INTERCEPT_SET(a_pVCpu, a_uDr) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmWriteDRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uDr)))

/**
 * Check if an SVM exception intercept is set.
 */
# define IEM_SVM_IS_XCPT_INTERCEPT_SET(a_pVCpu, a_uVector) \
    (IEM_SVM_IS_IN_GUEST(a_pVCpu) && CPUMIsGuestSvmXcptInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uVector)))

/**
 * Invokes the SVM \#VMEXIT handler for the nested-guest.
 */
# define IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2) \
    do { return iemSvmVmexit((a_pVCpu), (a_uExitCode), (a_uExitInfo1), (a_uExitInfo2)); } while (0)

/**
 * Invokes the 'MOV CRx' SVM \#VMEXIT handler after constructing the
 * corresponding decode assist information.
 */
# define IEM_SVM_CRX_VMEXIT_RET(a_pVCpu, a_uExitCode, a_enmAccessCrX, a_iGReg) \
    do \
    { \
        uint64_t uExitInfo1; \
        if (   IEM_GET_GUEST_CPU_FEATURES(a_pVCpu)->fSvmDecodeAssists \
            && (a_enmAccessCrX) == IEMACCESSCRX_MOV_CRX) \
            uExitInfo1 = SVM_EXIT1_MOV_CRX_MASK | ((a_iGReg) & 7); \
        else \
            uExitInfo1 = 0; \
        IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, uExitInfo1, 0); \
    } while (0)

/** Check and handles SVM nested-guest instruction intercept and updates
 *  NRIP if needed.
 */
# define IEM_SVM_CHECK_INSTR_INTERCEPT(a_pVCpu, a_Intercept, a_uExitCode, a_uExitInfo1, a_uExitInfo2, a_cbInstr) \
    do \
    { \
        if (IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept)) \
        { \
            IEM_SVM_UPDATE_NRIP(a_pVCpu, a_cbInstr); \
            IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2); \
        } \
    } while (0)

/** Checks and handles SVM nested-guest CR0 read intercept. */
# define IEM_SVM_CHECK_READ_CR0_INTERCEPT(a_pVCpu, a_uExitInfo1, a_uExitInfo2, a_cbInstr) \
    do \
    { \
        if (!IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, 0)) \
        { /* probably likely */ } \
        else \
        { \
            IEM_SVM_UPDATE_NRIP(a_pVCpu, a_cbInstr); \
            IEM_SVM_VMEXIT_RET(a_pVCpu, SVM_EXIT_READ_CR0, a_uExitInfo1, a_uExitInfo2); \
        } \
    } while (0)

/**
 * Updates the NextRIP (NRI) field in the nested-guest VMCB.
 */
# define IEM_SVM_UPDATE_NRIP(a_pVCpu, a_cbInstr) \
    do { \
        if (IEM_GET_GUEST_CPU_FEATURES(a_pVCpu)->fSvmNextRipSave) \
            CPUMGuestSvmUpdateNRip(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_cbInstr)); \
    } while (0)

#else
# define IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept)                                (false)
# define IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, a_uCr)                                   (false)
# define IEM_SVM_IS_WRITE_CR_INTERCEPT_SET(a_pVCpu, a_uCr)                                  (false)
# define IEM_SVM_IS_READ_DR_INTERCEPT_SET(a_pVCpu, a_uDr)                                   (false)
# define IEM_SVM_IS_WRITE_DR_INTERCEPT_SET(a_pVCpu, a_uDr)                                  (false)
# define IEM_SVM_IS_XCPT_INTERCEPT_SET(a_pVCpu, a_uVector)                                  (false)
# define IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2)               do { return VERR_SVM_IPE_1; } while (0)
# define IEM_SVM_CRX_VMEXIT_RET(a_pVCpu, a_uExitCode, a_enmAccessCrX, a_iGReg)              do { return VERR_SVM_IPE_1; } while (0)
# define IEM_SVM_CHECK_INSTR_INTERCEPT(a_pVCpu, a_Intercept, a_uExitCode, \
                                       a_uExitInfo1, a_uExitInfo2, a_cbInstr)               do { } while (0)
# define IEM_SVM_CHECK_READ_CR0_INTERCEPT(a_pVCpu, a_uExitInfo1, a_uExitInfo2, a_cbInstr)   do { } while (0)
# define IEM_SVM_UPDATE_NRIP(a_pVCpu, a_cbInstr)                                            do { } while (0)

#endif

/** @} */


/**
 * Selector descriptor table entry as fetched by iemMemFetchSelDesc.
 */
typedef union IEMSELDESC
{
    /** The legacy view. */
    X86DESC     Legacy;
    /** The long mode view. */
    X86DESC64   Long;
} IEMSELDESC;
/** Pointer to a selector descriptor table entry. */
typedef IEMSELDESC *PIEMSELDESC;

/** @name  Raising Exceptions.
 * @{ */
VBOXSTRICTRC            iemTaskSwitch(PVMCPUCC pVCpu, IEMTASKSWITCH enmTaskSwitch, uint32_t uNextEip, uint32_t fFlags,
                                      uint16_t uErr, uint64_t uCr2, RTSEL SelTSS, PIEMSELDESC pNewDescTSS) RT_NOEXCEPT;

VBOXSTRICTRC            iemRaiseXcptOrInt(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector, uint32_t fFlags,
                                          uint16_t uErr, uint64_t uCr2) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseXcptOrIntJmp(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector,
                                             uint32_t fFlags, uint16_t uErr, uint64_t uCr2) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseDivideError(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseDivideErrorJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseDebugException(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseBoundRangeExceeded(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseUndefinedOpcode(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseUndefinedOpcodeJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseDeviceNotAvailable(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseDeviceNotAvailableJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultCurrentTSS(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFault0(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
/*VBOXSTRICTRC            iemRaiseSelectorNotPresent(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;*/
VBOXSTRICTRC            iemRaiseSelectorNotPresentWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseSelectorNotPresentBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseStackSelectorNotPresentBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseStackSelectorNotPresentWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseGeneralProtectionFault(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseGeneralProtectionFault0(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseGeneralProtectionFault0Jmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseGeneralProtectionFaultBySelector(PVMCPUCC pVCpu, RTSEL Sel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseNotCanonical(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseSelectorBounds(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseSelectorBoundsJmp(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseSelectorBoundsBySelector(PVMCPUCC pVCpu, RTSEL Sel) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseSelectorBoundsBySelectorJmp(PVMCPUCC pVCpu, RTSEL Sel) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseSelectorInvalidAccess(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseSelectorInvalidAccessJmp(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaisePageFault(PVMCPUCC pVCpu, RTGCPTR GCPtrWhere, uint32_t cbAccess, uint32_t fAccess, int rc) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaisePageFaultJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrWhere, uint32_t cbAccess, uint32_t fAccess, int rc) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseMathFault(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseMathFaultJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseAlignmentCheckException(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseAlignmentCheckExceptionJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
VBOXSTRICTRC            iemRaiseSimdFpException(PVMCPUCC pVCpu) RT_NOEXCEPT;
DECL_NO_RETURN(void)    iemRaiseSimdFpExceptionJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;

void                    iemLogSyscallRealModeInt(PVMCPUCC pVCpu, uint8_t u8Vector, uint8_t cbInstr);
void                    iemLogSyscallProtModeInt(PVMCPUCC pVCpu, uint8_t u8Vector, uint8_t cbInstr);

IEM_CIMPL_PROTO_0(iemCImplRaiseDivideError);
IEM_CIMPL_PROTO_0(iemCImplRaiseInvalidLockPrefix);
IEM_CIMPL_PROTO_0(iemCImplRaiseInvalidOpcode);

/**
 * Macro for calling iemCImplRaiseDivideError().
 *
 * This is for things that will _always_ decode to an \#DE, taking the
 * recompiler into consideration and everything.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_DIVIDE_ERROR_RET()          IEM_MC_DEFER_TO_CIMPL_0_RET(IEM_CIMPL_F_XCPT, 0, iemCImplRaiseDivideError)

/**
 * Macro for calling iemCImplRaiseInvalidLockPrefix().
 *
 * This is for things that will _always_ decode to an \#UD, taking the
 * recompiler into consideration and everything.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_LOCK_PREFIX_RET()   IEM_MC_DEFER_TO_CIMPL_0_RET(IEM_CIMPL_F_XCPT, 0, iemCImplRaiseInvalidLockPrefix)

/**
 * Macro for calling iemCImplRaiseInvalidOpcode() for decode/static \#UDs.
 *
 * This is for things that will _always_ decode to an \#UD, taking the
 * recompiler into consideration and everything.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_OPCODE_RET()        IEM_MC_DEFER_TO_CIMPL_0_RET(IEM_CIMPL_F_XCPT, 0, iemCImplRaiseInvalidOpcode)

/**
 * Macro for calling iemCImplRaiseInvalidOpcode() for runtime-style \#UDs.
 *
 * Using this macro means you've got _buggy_ _code_ and are doing things that
 * belongs exclusively in IEMAllCImpl.cpp during decoding.
 *
 * @return  Strict VBox status code.
 * @see     IEMOP_RAISE_INVALID_OPCODE_RET
 */
#define IEMOP_RAISE_INVALID_OPCODE_RUNTIME_RET() IEM_MC_DEFER_TO_CIMPL_0_RET(IEM_CIMPL_F_XCPT, 0, iemCImplRaiseInvalidOpcode)

/** @} */

/** @name Register Access.
 * @{ */
VBOXSTRICTRC    iemRegRipRelativeJumpS8AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                           IEMMODE enmEffOpSize) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int16_t offNextInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int32_t offNextInstr,
                                                            IEMMODE enmEffOpSize) RT_NOEXCEPT;
/** @} */

/** @name FPU access and helpers.
 * @{ */
void            iemFpuPushResult(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuPushResultWithMemOp(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuPushResultTwo(PVMCPUCC pVCpu, PIEMFPURESULTTWO pResult, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStoreResult(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStoreResultThenPop(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStoreResultWithMemOp(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg,
                                           uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStoreResultWithMemOpThenPop(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg,
                                                  uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateOpcodeAndIp(PVMCPUCC pVCpu, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateFSW(PVMCPUCC pVCpu, uint16_t u16FSW, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateFSWThenPop(PVMCPUCC pVCpu, uint16_t u16FSW, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateFSWWithMemOp(PVMCPUCC pVCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateFSWThenPopPop(PVMCPUCC pVCpu, uint16_t u16FSW, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuUpdateFSWWithMemOpThenPop(PVMCPUCC pVCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackUnderflow(PVMCPUCC pVCpu, uint8_t iStReg, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackUnderflowWithMemOp(PVMCPUCC pVCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackUnderflowThenPop(PVMCPUCC pVCpu, uint8_t iStReg, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackUnderflowWithMemOpThenPop(PVMCPUCC pVCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackUnderflowThenPopPop(PVMCPUCC pVCpu, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackPushUnderflow(PVMCPUCC pVCpu, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackPushUnderflowTwo(PVMCPUCC pVCpu, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackPushOverflow(PVMCPUCC pVCpu, uint16_t uFpuOpcode) RT_NOEXCEPT;
void            iemFpuStackPushOverflowWithMemOp(PVMCPUCC pVCpu, uint8_t iEffSeg, RTGCPTR GCPtrEff, uint16_t uFpuOpcode) RT_NOEXCEPT;
/** @} */

/** @name SSE+AVX SIMD access and helpers.
 * @{ */
void            iemSseUpdateMxcsr(PVMCPUCC pVCpu, uint32_t fMxcsr) RT_NOEXCEPT;
/** @} */

/** @name   Memory access.
 * @{ */

/** Report a \#GP instead of \#AC and do not restrict to ring-3 */
#define IEM_MEMMAP_F_ALIGN_GP       RT_BIT_32(16)
/** SSE access that should report a \#GP instead of \#AC, unless MXCSR.MM=1
 *  when it works like normal \#AC. Always used with IEM_MEMMAP_F_ALIGN_GP. */
#define IEM_MEMMAP_F_ALIGN_SSE      RT_BIT_32(17)
/** If \#AC is applicable, raise it. Always used with IEM_MEMMAP_F_ALIGN_GP.
 * Users include FXSAVE & FXRSTOR. */
#define IEM_MEMMAP_F_ALIGN_GP_OR_AC RT_BIT_32(18)

VBOXSTRICTRC    iemMemMap(PVMCPUCC pVCpu, void **ppvMem, uint8_t *pbUnmapInfo, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem,
                          uint32_t fAccess, uint32_t uAlignCtl) RT_NOEXCEPT;
#ifndef IN_RING3
VBOXSTRICTRC    iemMemCommitAndUnmapPostponeTroubleToR3(PVMCPUCC pVCpu, uint8_t bUnmapInfo) RT_NOEXCEPT;
#endif
VBOXSTRICTRC    iemMemApplySegment(PVMCPUCC pVCpu, uint32_t fAccess, uint8_t iSegReg, size_t cbMem, PRTGCPTR pGCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemMarkSelDescAccessed(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;

void            iemOpcodeFlushLight(PVMCPUCC pVCpu, uint8_t cbInstr);
void            iemOpcodeFlushHeavy(PVMCPUCC pVCpu, uint8_t cbInstr);
#ifdef IEM_WITH_CODE_TLB
void            iemOpcodeFetchBytesJmp(PVMCPUCC pVCpu, size_t cbDst, void *pvDst) IEM_NOEXCEPT_MAY_LONGJMP;
#else
VBOXSTRICTRC    iemOpcodeFetchMoreBytes(PVMCPUCC pVCpu, size_t cbMin) RT_NOEXCEPT;
#endif
uint8_t         iemOpcodeGetNextU8SlowJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t        iemOpcodeGetNextU16SlowJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemOpcodeGetNextU32SlowJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemOpcodeGetNextU64SlowJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;

VBOXSTRICTRC    iemMemFetchDataU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32NoAc(PVMCPUCC pVCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32_ZX_U64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64NoAc(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64AlignedU128(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataR80(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataD80(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128NoAc(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128AlignedSse(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU256(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU256NoAc(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU256AlignedAvx(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataXdtr(PVMCPUCC pVCpu, uint16_t *pcbLimit, PRTGCPTR pGCPtrBase, uint8_t iSegReg,
                                    RTGCPTR GCPtrMem, IEMMODE enmOpSize) RT_NOEXCEPT;
uint8_t         iemMemFetchDataU8SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t        iemMemFetchDataU16SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFetchDataU32SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFetchDataU32NoAcSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFlatFetchDataU32SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchDataU64SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchDataU64NoAcSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchDataU64AlignedU128SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataR80SafeJmp(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataD80SafeJmp(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128SafeJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128NoAcSafeJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128AlignedSseSafeJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU256SafeJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU256NoAcSafeJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU256AlignedAvxSafeJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
#if 0 /* these are inlined now */
uint8_t         iemMemFetchDataU8Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t        iemMemFetchDataU16Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFetchDataU32Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFlatFetchDataU32Jmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchDataU64Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchDataU64AlignedU128Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataR80Jmp(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataD80Jmp(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128Jmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128NoAcJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU128AlignedSseJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU256NoAcJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFetchDataU256AlignedAvxJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
#endif
void            iemMemFetchDataU256Jmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;

VBOXSTRICTRC    iemMemFetchSysU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSelDesc(PVMCPUCC pVCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSelDescWithErr(PVMCPUCC pVCpu, PIEMSELDESC pDesc, uint16_t uSel,
                                          uint8_t uXcpt, uint16_t uErrorCode) RT_NOEXCEPT;

VBOXSTRICTRC    iemMemStoreDataU8(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU16(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU32(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU64(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU128(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU128NoAc(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU128AlignedSse(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU256(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU256NoAc(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU256AlignedAvx(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataXdtr(PVMCPUCC pVCpu, uint16_t cbLimit, RTGCPTR GCPtrBase, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemStoreDataU8SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU16SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU32SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU64SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU128SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT128U u128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU128NoAcSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT128U pu128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU128AlignedSseSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT128U pu128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256NoAcSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256AlignedAvxSafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataR80SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTFLOAT80U pr80Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataD80SafeJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTPBCD80U pd80Value) IEM_NOEXCEPT_MAY_LONGJMP;
#if 0 /* inlined */
void            iemMemStoreDataU8Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU16Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU32Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU64Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU128Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataNoAcU128Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256NoAcJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256AlignedAvxJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;
#endif
void            iemMemStoreDataU128AlignedSseJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreDataU256Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) IEM_NOEXCEPT_MAY_LONGJMP;

uint8_t        *iemMemMapDataU8RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint8_t        *iemMemMapDataU8AtSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint8_t        *iemMemMapDataU8WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint8_t const  *iemMemMapDataU8RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t       *iemMemMapDataU16RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t       *iemMemMapDataU16AtSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t       *iemMemMapDataU16WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint16_t const *iemMemMapDataU16RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t       *iemMemMapDataU32RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t       *iemMemMapDataU32AtSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t       *iemMemMapDataU32WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t const *iemMemMapDataU32RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t       *iemMemMapDataU64RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t       *iemMemMapDataU64AtSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t       *iemMemMapDataU64WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t const *iemMemMapDataU64RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTFLOAT80U     iemMemMapDataR80RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTFLOAT80U     iemMemMapDataR80WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PCRTFLOAT80U    iemMemMapDataR80RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTPBCD80U      iemMemMapDataD80RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTPBCD80U      iemMemMapDataD80WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PCRTPBCD80U     iemMemMapDataD80RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTUINT128U     iemMemMapDataU128RwSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTUINT128U     iemMemMapDataU128AtSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PRTUINT128U     iemMemMapDataU128WoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
PCRTUINT128U    iemMemMapDataU128RoSafeJmp(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, uint8_t iSegReg, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;

VBOXSTRICTRC    iemMemStackPushBeginSpecial(PVMCPUCC pVCpu, size_t cbMem, uint32_t cbAlign,
                                            void **ppvMem, uint8_t *pbUnmapInfo, uint64_t *puNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushCommitSpecial(PVMCPUCC pVCpu, uint8_t bUnmapInfo, uint64_t uNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU16(PVMCPUCC pVCpu, uint16_t u16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32(PVMCPUCC pVCpu, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU64(PVMCPUCC pVCpu, uint64_t u64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU16Ex(PVMCPUCC pVCpu, uint16_t u16Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32Ex(PVMCPUCC pVCpu, uint32_t u32Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU64Ex(PVMCPUCC pVCpu, uint64_t u64Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32SReg(PVMCPUCC pVCpu, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopBeginSpecial(PVMCPUCC pVCpu, size_t cbMem, uint32_t cbAlign,
                                           void const **ppvMem, uint8_t *pbUnmapInfo, uint64_t *puNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopContinueSpecial(PVMCPUCC pVCpu, size_t off, size_t cbMem,
                                              void const **ppvMem, uint8_t *pbUnmapInfo, uint64_t uCurNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopDoneSpecial(PVMCPUCC pVCpu, uint8_t bUnmapInfo) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU16(PVMCPUCC pVCpu, uint16_t *pu16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU32(PVMCPUCC pVCpu, uint32_t *pu32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU64(PVMCPUCC pVCpu, uint64_t *pu64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU16Ex(PVMCPUCC pVCpu, uint16_t *pu16Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU32Ex(PVMCPUCC pVCpu, uint32_t *pu32Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU64Ex(PVMCPUCC pVCpu, uint64_t *pu64Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;

void            iemMemStackPushU16SafeJmp(PVMCPUCC pVCpu, uint16_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPushU32SafeJmp(PVMCPUCC pVCpu, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPushU32SRegSafeJmp(PVMCPUCC pVCpu, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPushU64SafeJmp(PVMCPUCC pVCpu, uint64_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPopGRegU16SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPopGRegU32SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStackPopGRegU64SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;

void            iemMemFlat32StackPushU16SafeJmp(PVMCPUCC pVCpu, uint16_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat32StackPushU32SafeJmp(PVMCPUCC pVCpu, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat32StackPushU32SRegSafeJmp(PVMCPUCC pVCpu, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat32StackPopGRegU16SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat32StackPopGRegU32SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;

void            iemMemFlat64StackPushU16SafeJmp(PVMCPUCC pVCpu, uint16_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat64StackPushU64SafeJmp(PVMCPUCC pVCpu, uint64_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat64StackPopGRegU16SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemFlat64StackPopGRegU64SafeJmp(PVMCPUCC pVCpu, uint8_t iGReg) IEM_NOEXCEPT_MAY_LONGJMP;

void            iemMemStoreStackU16SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint16_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreStackU32SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreStackU32SRegSafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;
void            iemMemStoreStackU64SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint64_t uValue) IEM_NOEXCEPT_MAY_LONGJMP;

uint16_t        iemMemFetchStackU16SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint32_t        iemMemFetchStackU32SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;
uint64_t        iemMemFetchStackU64SafeJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrMem) IEM_NOEXCEPT_MAY_LONGJMP;

/** @} */

/** @name IEMAllCImpl.cpp
 * @note sed -e '/IEM_CIMPL_DEF_/!d' -e 's/IEM_CIMPL_DEF_/IEM_CIMPL_PROTO_/' -e 's/$/;/'
 * @{ */

/**
 * INT instruction types - iemCImpl_int().
 * @note x86 specific
 */
typedef enum IEMINT
{
    /** INT n instruction (opcode 0xcd imm). */
    IEMINT_INTN  = 0,
    /** Single byte INT3 instruction (opcode 0xcc). */
    IEMINT_INT3  = IEM_XCPT_FLAGS_BP_INSTR,
    /** Single byte INTO instruction (opcode 0xce). */
    IEMINT_INTO  = IEM_XCPT_FLAGS_OF_INSTR,
    /** Single byte INT1 (ICEBP) instruction (opcode 0xf1). */
    IEMINT_INT1 = IEM_XCPT_FLAGS_ICEBP_INSTR
} IEMINT;
AssertCompileSize(IEMINT, 4);

IEM_CIMPL_PROTO_2(iemCImpl_pop_mem16, uint16_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_2(iemCImpl_pop_mem32, uint16_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_2(iemCImpl_pop_mem64, uint16_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_0(iemCImpl_popa_16);
IEM_CIMPL_PROTO_0(iemCImpl_popa_32);
IEM_CIMPL_PROTO_0(iemCImpl_pusha_16);
IEM_CIMPL_PROTO_0(iemCImpl_pusha_32);
IEM_CIMPL_PROTO_1(iemCImpl_pushf, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_popf, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_FarJmp, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_callf, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmEffOpSize);
typedef IEM_CIMPL_DECL_TYPE_3(FNIEMCIMPLFARBRANCH, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmEffOpSize);
typedef FNIEMCIMPLFARBRANCH *PFNIEMCIMPLFARBRANCH;
IEM_CIMPL_PROTO_2(iemCImpl_retf, IEMMODE, enmEffOpSize, uint16_t, cbPop);
IEM_CIMPL_PROTO_3(iemCImpl_enter, IEMMODE, enmEffOpSize, uint16_t, cbFrame, uint8_t, cParameters);
IEM_CIMPL_PROTO_1(iemCImpl_leave, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_int, uint8_t, u8Int, IEMINT, enmInt);
IEM_CIMPL_PROTO_1(iemCImpl_iret_real_v8086, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_4(iemCImpl_iret_prot_v8086, uint32_t, uNewEip, uint16_t, uNewCs, uint32_t, uNewFlags, uint64_t, uNewRsp);
IEM_CIMPL_PROTO_1(iemCImpl_iret_prot_NestedTask, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret_prot, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret_64bit, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_0(iemCImpl_loadall286);
IEM_CIMPL_PROTO_0(iemCImpl_syscall);
IEM_CIMPL_PROTO_1(iemCImpl_sysret, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_0(iemCImpl_sysenter);
IEM_CIMPL_PROTO_1(iemCImpl_sysexit, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_LoadSReg, uint8_t, iSegReg, uint16_t, uSel);
IEM_CIMPL_PROTO_2(iemCImpl_load_SReg, uint8_t, iSegReg, uint16_t, uSel);
IEM_CIMPL_PROTO_2(iemCImpl_pop_Sreg, uint8_t, iSegReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_5(iemCImpl_load_SReg_Greg, uint16_t, uSel, uint64_t, offSeg, uint8_t, iSegReg, uint8_t, iGReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_VerX, uint16_t, uSel, bool, fWrite);
IEM_CIMPL_PROTO_3(iemCImpl_LarLsl_u64, uint64_t *, pu64Dst, uint16_t, uSel, bool, fIsLar);
IEM_CIMPL_PROTO_3(iemCImpl_LarLsl_u16, uint16_t *, pu16Dst, uint16_t, uSel, bool, fIsLar);
IEM_CIMPL_PROTO_3(iemCImpl_lgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_lidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_1(iemCImpl_lldt, uint16_t, uNewLdt);
IEM_CIMPL_PROTO_2(iemCImpl_sldt_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sldt_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_1(iemCImpl_ltr, uint16_t, uNewTr);
IEM_CIMPL_PROTO_2(iemCImpl_str_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_str_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Cd, uint8_t, iGReg, uint8_t, iCrReg);
IEM_CIMPL_PROTO_2(iemCImpl_smsw_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_smsw_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_4(iemCImpl_load_CrX, uint8_t, iCrReg, uint64_t, uNewCrX, IEMACCESSCRX, enmAccessCrX, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Cd_Rd, uint8_t, iCrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_lmsw, uint16_t, u16NewMsw, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_0(iemCImpl_clts);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Dd, uint8_t, iGReg, uint8_t, iDrReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Dd_Rd, uint8_t, iDrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Td, uint8_t, iGReg, uint8_t, iTrReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Td_Rd, uint8_t, iTrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_1(iemCImpl_invlpg, RTGCPTR, GCPtrPage);
IEM_CIMPL_PROTO_3(iemCImpl_invpcid, uint8_t, iEffSeg, RTGCPTR, GCPtrInvpcidDesc, uint64_t, uInvpcidType);
IEM_CIMPL_PROTO_0(iemCImpl_invd);
IEM_CIMPL_PROTO_0(iemCImpl_wbinvd);
IEM_CIMPL_PROTO_0(iemCImpl_rsm);
IEM_CIMPL_PROTO_0(iemCImpl_rdtsc);
IEM_CIMPL_PROTO_0(iemCImpl_rdtscp);
IEM_CIMPL_PROTO_0(iemCImpl_rdpmc);
IEM_CIMPL_PROTO_0(iemCImpl_rdmsr);
IEM_CIMPL_PROTO_0(iemCImpl_wrmsr);
IEM_CIMPL_PROTO_3(iemCImpl_in, uint16_t, u16Port, uint8_t, cbReg, uint8_t, bImmAndEffAddrMode);
IEM_CIMPL_PROTO_2(iemCImpl_in_eAX_DX, uint8_t, cbReg, IEMMODE, enmEffAddrMode);
IEM_CIMPL_PROTO_3(iemCImpl_out, uint16_t, u16Port, uint8_t, cbReg, uint8_t, bImmAndEffAddrMode);
IEM_CIMPL_PROTO_2(iemCImpl_out_DX_eAX, uint8_t, cbReg, IEMMODE, enmEffAddrMode);
IEM_CIMPL_PROTO_0(iemCImpl_cli);
IEM_CIMPL_PROTO_0(iemCImpl_sti);
IEM_CIMPL_PROTO_0(iemCImpl_hlt);
IEM_CIMPL_PROTO_1(iemCImpl_monitor, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_mwait);
IEM_CIMPL_PROTO_0(iemCImpl_swapgs);
IEM_CIMPL_PROTO_0(iemCImpl_cpuid);
IEM_CIMPL_PROTO_1(iemCImpl_aad, uint8_t, bImm);
IEM_CIMPL_PROTO_1(iemCImpl_aam, uint8_t, bImm);
IEM_CIMPL_PROTO_0(iemCImpl_daa);
IEM_CIMPL_PROTO_0(iemCImpl_das);
IEM_CIMPL_PROTO_0(iemCImpl_aaa);
IEM_CIMPL_PROTO_0(iemCImpl_aas);
IEM_CIMPL_PROTO_3(iemCImpl_bound_16, int16_t, idxArray, int16_t, idxLowerBound, int16_t, idxUpperBound);
IEM_CIMPL_PROTO_3(iemCImpl_bound_32, int32_t, idxArray, int32_t, idxLowerBound, int32_t, idxUpperBound);
IEM_CIMPL_PROTO_0(iemCImpl_xgetbv);
IEM_CIMPL_PROTO_0(iemCImpl_xsetbv);
IEM_CIMPL_PROTO_5(iemCImpl_cmpxchg16b_fallback_rendezvous, PRTUINT128U, pu128Dst, PRTUINT128U, pu128RaxRdx,
                  PRTUINT128U, pu128RbxRcx, uint32_t *, pEFlags, uint8_t, bUnmapInfo);
IEM_CIMPL_PROTO_2(iemCImpl_clflush_clflushopt, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_1(iemCImpl_finit, bool, fCheckXcpts);
IEM_CIMPL_PROTO_3(iemCImpl_fxsave, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_fxrstor, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_xsave, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_xrstor, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_stmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_2(iemCImpl_vstmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_2(iemCImpl_ldmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_2(iemCImpl_vldmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_3(iemCImpl_fnstenv, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_fnsave, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_fldenv, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_3(iemCImpl_frstor, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_1(iemCImpl_fldcw, uint16_t, u16Fcw);
IEM_CIMPL_PROTO_2(iemCImpl_fxch_underflow, uint8_t, iStReg, uint16_t, uFpuOpcode);
IEM_CIMPL_PROTO_3(iemCImpl_fcomi_fucomi, uint8_t, iStReg, bool, fUCmp, uint32_t, uPopAndFpuOpcode);
IEM_CIMPL_PROTO_2(iemCImpl_rdseed, uint8_t, iReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_rdrand, uint8_t, iReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovps_load_u128, uint8_t, iXRegDst, uint8_t, iXRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovps_load_u256, uint8_t, iYRegDst, uint8_t, iYRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovps_store_u128, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iXRegMsk, uint8_t, iXRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovps_store_u256, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iYRegMsk, uint8_t, iYRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovd_load_u128, uint8_t, iXRegDst, uint8_t, iXRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovd_load_u256, uint8_t, iYRegDst, uint8_t, iYRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovd_store_u128, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iXRegMsk, uint8_t, iXRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovd_store_u256, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iYRegMsk, uint8_t, iYRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovpd_load_u128, uint8_t, iXRegDst, uint8_t, iXRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovpd_load_u256, uint8_t, iYRegDst, uint8_t, iYRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovpd_store_u128, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iXRegMsk, uint8_t, iXRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vmaskmovpd_store_u256, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iYRegMsk, uint8_t, iYRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovq_load_u128, uint8_t, iXRegDst, uint8_t, iXRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovq_load_u256, uint8_t, iYRegDst, uint8_t, iYRegMsk, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovq_store_u128, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iXRegMsk, uint8_t, iXRegSrc);
IEM_CIMPL_PROTO_4(iemCImpl_vpmaskmovq_store_u256, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst, uint8_t, iYRegMsk, uint8_t, iYRegSrc);
IEM_CIMPL_PROTO_2(iemCImpl_vpgather_worker_xx, uint32_t, u32PackedArgs, uint32_t, u32Disp);

/** Packed 32-bit argument for iemCImpl_vpgather_worker_xx. */
typedef union IEMGATHERARGS
{
    /** Integer view. */
    uint32_t u;
    /** Bitfield view. */
    struct
    {
        uint32_t iYRegDst       : 4; /**<  0 - XMM or YMM register number (destination) */
        uint32_t iYRegIdc       : 4; /**<  4 - XMM or YMM register number (indices)     */
        uint32_t iYRegMsk       : 4; /**<  8 - XMM or YMM register number (mask)        */
        uint32_t iGRegBase      : 4; /**< 12 - general register number    (base ptr)    */
        uint32_t iScale         : 2; /**< 16 - scale factor               (1/2/4/8)     */
        uint32_t enmEffOpSize   : 2; /**< 18 - operand size               (16/32/64/--) */
        uint32_t enmEffAddrMode : 2; /**< 20 - addressing  mode           (16/32/64/--) */
        uint32_t iEffSeg        : 3; /**< 22 - effective segment (ES/CS/SS/DS/FS/GS)    */
        uint32_t fVex256        : 1; /**< 25 - overall instruction width (128/256 bits) */
        uint32_t fIdxQword      : 1; /**< 26 - individual index width     (4/8 bytes)   */
        uint32_t fValQword      : 1; /**< 27 - individual value width     (4/8 bytes)   */
    } s;
} IEMGATHERARGS;
AssertCompileSize(IEMGATHERARGS, sizeof(uint32_t));

/** @} */

/** @name IEMAllCImplStrInstr.cpp.h
 * @note sed -e '/IEM_CIMPL_DEF_/!d' -e 's/IEM_CIMPL_DEF_/IEM_CIMPL_PROTO_/' -e 's/$/;/' -e 's/RT_CONCAT4(//' \
 *           -e 's/,ADDR_SIZE)/64/g' -e 's/,OP_SIZE,/64/g' -e 's/,OP_rAX,/rax/g' IEMAllCImplStrInstr.cpp.h
 * @{ */
IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr16, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr16, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr16, uint8_t, iEffSeg, bool, fIoChecked);


IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_rax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_rax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_rax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_rax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op64_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op64_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op64_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op64_addr32, uint8_t, iEffSeg, bool, fIoChecked);


IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_rax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_rax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_rax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_rax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op64_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op64_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op64_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op64_addr64, uint8_t, iEffSeg, bool, fIoChecked);
/** @} */

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
VBOXSTRICTRC    iemVmxVmexit(PVMCPUCC pVCpu, uint32_t uExitReason, uint64_t u64ExitQual) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstr(PVMCPUCC pVCpu, uint32_t uExitReason, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrNeedsInfo(PVMCPUCC pVCpu, uint32_t uExitReason, VMXINSTRID uInstrId, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitTaskSwitch(PVMCPUCC pVCpu, IEMTASKSWITCH enmTaskSwitch, RTSEL SelNewTss, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEvent(PVMCPUCC pVCpu, uint8_t uVector, uint32_t fFlags, uint32_t uErrCode, uint64_t uCr2, uint8_t cbInstr)  RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEventDoubleFault(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEpt(PVMCPUCC pVCpu, PPGMPTWALKFAST pWalk, uint32_t fAccess, uint32_t fSlatFail, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitPreemptTimer(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMwait(PVMCPUCC pVCpu, bool fMonitorHwArmed, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrIo(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint16_t u16Port,
                                    bool fImm, uint8_t cbAccess, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrStrIo(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint16_t u16Port, uint8_t cbAccess,
                                       bool fRep, VMXEXITINSTRINFO ExitInstrInfo, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovDrX(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint8_t iDrReg, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr8(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovFromCr8(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr3(PVMCPUCC pVCpu, uint64_t uNewCr3, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovFromCr3(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr0Cr4(PVMCPUCC pVCpu, uint8_t iCrReg, uint64_t *puNewCrX, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrClts(PVMCPUCC pVCpu, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrLmsw(PVMCPUCC pVCpu, uint32_t uGuestCr0, uint16_t *pu16NewMsw,
                                      RTGCPTR GCPtrEffDst, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrInvlpg(PVMCPUCC pVCpu, RTGCPTR GCPtrPage, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxApicWriteEmulation(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVirtApicAccessUnused(PVMCPUCC pVCpu, PRTGCPHYS pGCPhysAccess, size_t cbAccess, uint32_t fAccess) RT_NOEXCEPT;
uint32_t        iemVmxVirtApicReadRaw32(PVMCPUCC pVCpu, uint16_t offReg) RT_NOEXCEPT;
void            iemVmxVirtApicWriteRaw32(PVMCPUCC pVCpu, uint16_t offReg, uint32_t uReg) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxInvvpid(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPTR GCPtrInvvpidDesc,
                              uint64_t u64InvvpidType, PCVMXVEXITINFO pExitInfo) RT_NOEXCEPT;
bool            iemVmxIsRdmsrWrmsrInterceptSet(PCVMCPU pVCpu, uint32_t uExitReason, uint32_t idMsr) RT_NOEXCEPT;
IEM_CIMPL_PROTO_0(iemCImpl_vmxoff);
IEM_CIMPL_PROTO_2(iemCImpl_vmxon, uint8_t, iEffSeg, RTGCPTR, GCPtrVmxon);
IEM_CIMPL_PROTO_0(iemCImpl_vmlaunch);
IEM_CIMPL_PROTO_0(iemCImpl_vmresume);
IEM_CIMPL_PROTO_2(iemCImpl_vmptrld, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmptrst, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmclear, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmwrite_reg, uint64_t, u64Val, uint64_t, u64VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmwrite_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrVal, uint32_t, u64VmcsField);
IEM_CIMPL_PROTO_2(iemCImpl_vmread_reg64, uint64_t *, pu64Dst, uint64_t, u64VmcsField);
IEM_CIMPL_PROTO_2(iemCImpl_vmread_reg32, uint64_t *, pu32Dst, uint32_t, u32VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmread_mem_reg64, uint8_t, iEffSeg, RTGCPTR, GCPtrDst, uint32_t, u64VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmread_mem_reg32, uint8_t, iEffSeg, RTGCPTR, GCPtrDst, uint32_t, u32VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_invvpid, uint8_t, iEffSeg, RTGCPTR, GCPtrInvvpidDesc, uint64_t, uInvvpidType);
IEM_CIMPL_PROTO_3(iemCImpl_invept, uint8_t, iEffSeg, RTGCPTR, GCPtrInveptDesc, uint64_t, uInveptType);
IEM_CIMPL_PROTO_0(iemCImpl_vmx_pause);
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
VBOXSTRICTRC    iemSvmVmexit(PVMCPUCC pVCpu, uint64_t uExitCode, uint64_t uExitInfo1, uint64_t uExitInfo2) RT_NOEXCEPT;
VBOXSTRICTRC    iemHandleSvmEventIntercept(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector, uint32_t fFlags, uint32_t uErr, uint64_t uCr2) RT_NOEXCEPT;
VBOXSTRICTRC    iemSvmHandleIOIntercept(PVMCPUCC pVCpu, uint16_t u16Port, SVMIOIOTYPE enmIoType, uint8_t cbReg,
                                        uint8_t cAddrSizeBits, uint8_t iEffSeg, bool fRep, bool fStrIo, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemSvmHandleMsrIntercept(PVMCPUCC pVCpu, uint32_t idMsr, bool fWrite, uint8_t cbInstr) RT_NOEXCEPT;
IEM_CIMPL_PROTO_0(iemCImpl_vmrun);
IEM_CIMPL_PROTO_0(iemCImpl_vmload);
IEM_CIMPL_PROTO_0(iemCImpl_vmsave);
IEM_CIMPL_PROTO_0(iemCImpl_clgi);
IEM_CIMPL_PROTO_0(iemCImpl_stgi);
IEM_CIMPL_PROTO_0(iemCImpl_invlpga);
IEM_CIMPL_PROTO_0(iemCImpl_skinit);
IEM_CIMPL_PROTO_0(iemCImpl_svm_pause);
#endif

IEM_CIMPL_PROTO_0(iemCImpl_vmcall);  /* vmx */
IEM_CIMPL_PROTO_0(iemCImpl_vmmcall); /* svm */
IEM_CIMPL_PROTO_1(iemCImpl_Hypercall, uint16_t, uDisOpcode); /* both */

extern const PFNIEMOP g_apfnIemInterpretOnlyOneByteMap[256];
extern const PFNIEMOP g_apfnIemInterpretOnlyTwoByteMap[1024];
extern const PFNIEMOP g_apfnIemInterpretOnlyThreeByte0f3a[1024];
extern const PFNIEMOP g_apfnIemInterpretOnlyThreeByte0f38[1024];
extern const PFNIEMOP g_apfnIemInterpretOnlyVecMap1[1024];
extern const PFNIEMOP g_apfnIemInterpretOnlyVecMap2[1024];
extern const PFNIEMOP g_apfnIemInterpretOnlyVecMap3[1024];

/*
 * Recompiler related stuff.
 */
extern const PFNIEMOP g_apfnIemThreadedRecompilerOneByteMap[256];
extern const PFNIEMOP g_apfnIemThreadedRecompilerTwoByteMap[1024];
extern const PFNIEMOP g_apfnIemThreadedRecompilerThreeByte0f3a[1024];
extern const PFNIEMOP g_apfnIemThreadedRecompilerThreeByte0f38[1024];
extern const PFNIEMOP g_apfnIemThreadedRecompilerVecMap1[1024];
extern const PFNIEMOP g_apfnIemThreadedRecompilerVecMap2[1024];
extern const PFNIEMOP g_apfnIemThreadedRecompilerVecMap3[1024];


IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_Nop);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_LogCpuState);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_DeferToCImpl0);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckIrq);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckTimers);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckTimersAndIrq);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckMode);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckHwInstrBps);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLim);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndOpcodes);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodes);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesConsiderCsLim);

/* Branching: */
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndPcAndOpcodes);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckPcAndOpcodes);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckPcAndOpcodesConsiderCsLim);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesLoadingTlbConsiderCsLim);

/* Natural page crossing: */
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlbConsiderCsLim);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlbConsiderCsLim);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlb);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlbConsiderCsLim);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_Jump);

bool iemThreadedCompileEmitIrqCheckBefore(PVMCPUCC pVCpu, PIEMTB pTb);
bool iemThreadedCompileBeginEmitCallsComplications(PVMCPUCC pVCpu, PIEMTB pTb);
#ifdef IEM_WITH_INTRA_TB_JUMPS
DECLHIDDEN(int)     iemThreadedCompileBackAtFirstInstruction(PVMCPU pVCpu, PIEMTB pTb) RT_NOEXCEPT;
#endif


/** @} */

RT_C_DECLS_END


#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInternal_x86_h */

