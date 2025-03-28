/* $Id$ */
/** @file
 * IEM - Internal header file, ARMv8 target specifics.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInternal_armv8_h
#define VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInternal_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


RT_C_DECLS_BEGIN


/** @defgroup grp_iem_int_arm   ARM Target Internals
 * @ingroup grp_iem_int
 * @internal
 * @{
 */

VBOXSTRICTRC iemOpcodeFetchPrefetch(PVMCPUCC pVCpu) RT_NOEXCEPT;


/** @name Misc Helpers
 * @{  */

/** @} */


/** @name  Raising Exceptions.
 * @{ */
//VBOXSTRICTRC            iemRaiseXcptOrInt(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector, uint32_t fFlags,
//                                          uint16_t uErr, uint64_t uCr2) RT_NOEXCEPT;
//DECL_NO_RETURN(void)    iemRaiseXcptOrIntJmp(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector,
//                                             uint32_t fFlags, uint16_t uErr, uint64_t uCr2) IEM_NOEXCEPT_MAY_LONGJMP;

#define IEM_RAISE_PROTOS(a_Name, ...) \
    VBOXSTRICTRC a_Name(__VA_ARGS__) RT_NOEXCEPT; \
    DECL_NO_RETURN(void) RT_CONCAT(a_Name,Jmp)(__VA_ARGS__) IEM_NOEXCEPT_MAY_LONGJMP

IEM_RAISE_PROTOS(iemRaiseDataAbortFromWalk,
                 PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t cbMem, uint32_t fAccess, int rc, PCPGMPTWALKFAST pWalkFast);

IEM_RAISE_PROTOS(iemRaiseDebugDataAccessOrInvokeDbgf,
                 PVMCPUCC pVCpu, uint32_t fDataBps, RTGCPTR GCPtrMem, size_t cbMem, uint32_t fAccess);


IEM_CIMPL_PROTO_0(iemCImplRaiseInvalidOpcode);

/**
 * Macro for calling iemCImplRaiseInvalidOpcode() for decode/static \#UDs.
 *
 * This is for things that will _always_ decode to an \#UD, taking the
 * recompiler into consideration and everything.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_OPCODE_RET()        IEM_MC_DEFER_TO_CIMPL_0_RET(IEM_CIMPL_F_XCPT, 0, iemCImplRaiseInvalidOpcode)

/** @} */


/** @name Register Access.
 * @{ */
VBOXSTRICTRC    iemRegRipRelativeJumpS8AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                           IEMMODE enmEffOpSize) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int16_t offNextInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int32_t offNextInstr,
                                                            IEMMODE enmEffOpSize) RT_NOEXCEPT;
/** @} */


/** @name   Memory access.
 * @{ */

/** XXX */
#define IEM_MEMMAP_F_ALIGN_XXX      RT_BIT_32(16)

VBOXSTRICTRC    iemMemMap(PVMCPUCC pVCpu, void **ppvMem, uint8_t *pbUnmapInfo, size_t cbMem, RTGCPTR GCPtrMem,
                          uint32_t fAccess, uint32_t uAlignCtl) RT_NOEXCEPT;

#ifdef IEM_WITH_CODE_TLB
void            iemOpcodeFetchBytesJmp(PVMCPUCC pVCpu, size_t cbDst, void *pvDst) IEM_NOEXCEPT_MAY_LONGJMP;
#else
VBOXSTRICTRC    iemOpcodeFetchMoreBytes(PVMCPUCC pVCpu, size_t cbMin) RT_NOEXCEPT;
#endif
uint16_t        iemOpcodeGetNextU16SlowJmp(PVMCPUCC pVCpu) IEM_NOEXCEPT_MAY_LONGJMP;

#if 0 /* rework this later */
VBOXSTRICTRC    iemMemFetchDataU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32NoAc(PVMCPUCC pVCpu, uint32_t *pu32Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32_ZX_U64(PVMCPUCC pVCpu, uint64_t *pu64Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64NoAc(PVMCPUCC pVCpu, uint64_t *pu64Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64AlignedU128(PVMCPUCC pVCpu, uint64_t *pu64Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataR80(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataD80(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128NoAc(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
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

VBOXSTRICTRC    iemMemFetchSysU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU128(PVMCPUCC pVCpu, uint128_t *pu128Dst, RTGCPTR GCPtrMem) RT_NOEXCEPT;

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

#endif /* later */
/** @} */

/** @name IEMAllCImpl.cpp
 * @note sed -e '/IEM_CIMPL_DEF_/!d' -e 's/IEM_CIMPL_DEF_/IEM_CIMPL_PROTO_/' -e 's/$/;/'
 * @{ */

/** @} */

/*
 * Recompiler related stuff.
 */


IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_Nop);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_LogCpuState);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_DeferToCImpl0);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckIrq);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckTimers);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckTimersAndIrq);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckMode);
IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckHwInstrBps);

IEM_DECL_IEMTHREADEDFUNC_PROTO(iemThreadedFunc_BltIn_CheckOpcodes);

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


#endif /* !VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInternal_armv8_h */

