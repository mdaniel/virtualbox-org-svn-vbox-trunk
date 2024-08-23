/** @file
 * Disassembler - Opcodes
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_disopcode_armv8_h
#define VBOX_INCLUDED_disopcode_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>

/** @defgroup grp_dis_opcodes_armv8 Opcodes (DISOPCODE::uOpCode)
 * @ingroup grp_dis
 * @{
 */
enum OPCODESARMV8
{
    /** @name Full ARMv8 AArch64 opcode list.
     * @{ */
    OP_ARMV8_INVALID = 0,
    OP_ARMV8_A64_ADC,
    OP_ARMV8_A64_ADCS,
    OP_ARMV8_A64_ADD,
    OP_ARMV8_A64_ADDG,
    OP_ARMV8_A64_ADDS,
    OP_ARMV8_A64_ADR,
    OP_ARMV8_A64_ADRP,
    OP_ARMV8_A64_AND,
    OP_ARMV8_A64_ANDS,
    OP_ARMV8_A64_ASR,
    OP_ARMV8_A64_ASRV,
    OP_ARMV8_A64_AT,
    OP_ARMV8_A64_AUTDA,
    OP_ARMV8_A64_AUTDZA,
    OP_ARMV8_A64_AUTDB,
    OP_ARMV8_A64_AUTDZB,
    OP_ARMV8_A64_AUTIA,
    OP_ARMV8_A64_AUTIA1716,
    OP_ARMV8_A64_AUTIASP,
    OP_ARMV8_A64_AUTIAZ,
    OP_ARMV8_A64_AUTIZA,
    OP_ARMV8_A64_AUTIB,
    OP_ARMV8_A64_AUTIB1716,
    OP_ARMV8_A64_AUTIBSP,
    OP_ARMV8_A64_AUTIBZ,
    OP_ARMV8_A64_AUTIZB,
    OP_ARMV8_A64_AXFLAG,
    OP_ARMV8_A64_B,
    OP_ARMV8_A64_BC,
    OP_ARMV8_A64_BFC,
    OP_ARMV8_A64_BFI,
    OP_ARMV8_A64_BFM,
    OP_ARMV8_A64_BFXIL,
    OP_ARMV8_A64_BIC,
    OP_ARMV8_A64_BICS,
    OP_ARMV8_A64_BL,
    OP_ARMV8_A64_BLR,
    OP_ARMV8_A64_BLRAA,
    OP_ARMV8_A64_BLRAAZ,
    OP_ARMV8_A64_BLRAB,
    OP_ARMV8_A64_BLRABZ,
    OP_ARMV8_A64_BR,
    OP_ARMV8_A64_BRAA,
    OP_ARMV8_A64_BRAAZ,
    OP_ARMV8_A64_BRAB,
    OP_ARMV8_A64_BRABZ,
    OP_ARMV8_A64_BRB,
    OP_ARMV8_A64_BRK,
    OP_ARMV8_A64_BTI,
    OP_ARMV8_A64_CASB,
    OP_ARMV8_A64_CASAB,
    OP_ARMV8_A64_CASALB,
    OP_ARMV8_A64_CASLB,
    OP_ARMV8_A64_CASH,
    OP_ARMV8_A64_CASAH,
    OP_ARMV8_A64_CASALH,
    OP_ARMV8_A64_CASLH,
    OP_ARMV8_A64_CASP,
    OP_ARMV8_A64_CASPA,
    OP_ARMV8_A64_CASPAL,
    OP_ARMV8_A64_CASPL,
    OP_ARMV8_A64_CAS,
    OP_ARMV8_A64_CASA,
    OP_ARMV8_A64_CASAL,
    OP_ARMV8_A64_CASL,
    OP_ARMV8_A64_CBNZ,
    OP_ARMV8_A64_CBZ,
    OP_ARMV8_A64_CCMN,
    OP_ARMV8_A64_CCMP,
    OP_ARMV8_A64_CFINV,
    OP_ARMV8_A64_CFP,
    OP_ARMV8_A64_CINC,
    OP_ARMV8_A64_CINV,
    OP_ARMV8_A64_CLREX,
    OP_ARMV8_A64_CLS,
    OP_ARMV8_A64_CLZ,
    OP_ARMV8_A64_CMN,
    OP_ARMV8_A64_CMP,
    OP_ARMV8_A64_CMPP,
    OP_ARMV8_A64_CNEG,
    OP_ARMV8_A64_CPP,
    /** @todo FEAT_MOPS instructions (CPYFP and friends). */
    OP_ARMV8_A64_CRC32B,
    OP_ARMV8_A64_CRC32H,
    OP_ARMV8_A64_CRC32W,
    OP_ARMV8_A64_CRC32X,
    OP_ARMV8_A64_CRC32CB,
    OP_ARMV8_A64_CRC32CH,
    OP_ARMV8_A64_CRC32CW,
    OP_ARMV8_A64_CRC32CX,
    OP_ARMV8_A64_CSDB,
    OP_ARMV8_A64_CSEL,
    OP_ARMV8_A64_CSET,
    OP_ARMV8_A64_CSETM,
    OP_ARMV8_A64_CSINC,
    OP_ARMV8_A64_CSNEG,
    OP_ARMV8_A64_DC,
    OP_ARMV8_A64_DCPS1,
    OP_ARMV8_A64_DCPS2,
    OP_ARMV8_A64_DCPS3,
    OP_ARMV8_A64_DGH,
    OP_ARMV8_A64_DMB,
    OP_ARMV8_A64_DRPS,
    OP_ARMV8_A64_DSB,
    OP_ARMV8_A64_DVP,
    OP_ARMV8_A64_EON,
    OP_ARMV8_A64_EOR,
    OP_ARMV8_A64_ERET,
    OP_ARMV8_A64_ERETAA,
    OP_ARMV8_A64_ERETAB,
    OP_ARMV8_A64_ESB,
    OP_ARMV8_A64_EXTR,
    OP_ARMV8_A64_GMI,
    OP_ARMV8_A64_HINT,
    OP_ARMV8_A64_HLT,
    OP_ARMV8_A64_HVC,
    OP_ARMV8_A64_IC,
    OP_ARMV8_A64_IRG,
    OP_ARMV8_A64_ISB,
    OP_ARMV8_A64_LD64B,
    OP_ARMV8_A64_LDADDB,
    OP_ARMV8_A64_LDADDAB,
    OP_ARMV8_A64_LDADDALB,
    OP_ARMV8_A64_LDADDLB,
    OP_ARMV8_A64_LDADDH,
    OP_ARMV8_A64_LDADDAH,
    OP_ARMV8_A64_LDADDALH,
    OP_ARMV8_A64_LDADDLH,
    OP_ARMV8_A64_LDADD,
    OP_ARMV8_A64_LDADDA,
    OP_ARMV8_A64_LDADDAL,
    OP_ARMV8_A64_LDADDL,
    OP_ARMV8_A64_LDAPR,
    OP_ARMV8_A64_LDAPRB,
    OP_ARMV8_A64_LDAPRH,
    OP_ARMV8_A64_LDAPUR,
    OP_ARMV8_A64_LDAPURB,
    OP_ARMV8_A64_LDAPURH,
    OP_ARMV8_A64_LDAPURSB,
    OP_ARMV8_A64_LDAPURSH,
    OP_ARMV8_A64_LDAPURSW,
    OP_ARMV8_A64_LDAR,
    OP_ARMV8_A64_LDARB,
    OP_ARMV8_A64_LDARH,
    OP_ARMV8_A64_LDAXP,
    OP_ARMV8_A64_LDAXRB,
    OP_ARMV8_A64_LDAXRH,
    OP_ARMV8_A64_LDCLRB,
    OP_ARMV8_A64_LDCLRAB,
    OP_ARMV8_A64_LDCLRALB,
    OP_ARMV8_A64_LDCLRLB,
    OP_ARMV8_A64_LDCLRH,
    OP_ARMV8_A64_LDCLRAH,
    OP_ARMV8_A64_LDCLRALH,
    OP_ARMV8_A64_LDCLRLH,
    OP_ARMV8_A64_LDCLR,
    OP_ARMV8_A64_LDCLRA,
    OP_ARMV8_A64_LDCLRAL,
    OP_ARMV8_A64_LDCLRL,
    OP_ARMV8_A64_LDEORB,
    OP_ARMV8_A64_LDEORAB,
    OP_ARMV8_A64_LDEORALB,
    OP_ARMV8_A64_LDEORLB,
    OP_ARMV8_A64_LDEORH,
    OP_ARMV8_A64_LDEORAH,
    OP_ARMV8_A64_LDEORALH,
    OP_ARMV8_A64_LDEORLH,
    OP_ARMV8_A64_LDEOR,
    OP_ARMV8_A64_LDEORA,
    OP_ARMV8_A64_LDEORAL,
    OP_ARMV8_A64_LDEORL,
    OP_ARMV8_A64_LDG,
    OP_ARMV8_A64_LDGM,
    OP_ARMV8_A64_LDLARB,
    OP_ARMV8_A64_LDLARH,
    OP_ARMV8_A64_LDLAR,
    OP_ARMV8_A64_LDNP,
    OP_ARMV8_A64_LDP,
    OP_ARMV8_A64_LDPSW,
    OP_ARMV8_A64_LDR,
    OP_ARMV8_A64_LDRAA,
    OP_ARMV8_A64_LDRAB,
    OP_ARMV8_A64_LDRB,
    OP_ARMV8_A64_LDRH,
    OP_ARMV8_A64_LDRSB,
    OP_ARMV8_A64_LDRSH,
    OP_ARMV8_A64_LDRSW,
    OP_ARMV8_A64_LDSETB,
    OP_ARMV8_A64_LDSETAB,
    OP_ARMV8_A64_LDSETALB,
    OP_ARMV8_A64_LDSETLB,
    OP_ARMV8_A64_LDSETH,
    OP_ARMV8_A64_LDSETAH,
    OP_ARMV8_A64_LDSETALH,
    OP_ARMV8_A64_LDSETLH,
    OP_ARMV8_A64_LDSET,
    OP_ARMV8_A64_LDSETA,
    OP_ARMV8_A64_LDSETAL,
    OP_ARMV8_A64_LDSETL,
    OP_ARMV8_A64_LDSMAB,
    OP_ARMV8_A64_LDSMAXAB,
    OP_ARMV8_A64_LDSMAXALB,
    OP_ARMV8_A64_LDSMAXLB,
    OP_ARMV8_A64_LDSMAXH,
    OP_ARMV8_A64_LDSMAXAH,
    OP_ARMV8_A64_LDSMAXALH,
    OP_ARMV8_A64_LDSMAXLH,
    OP_ARMV8_A64_LDSMAX,
    OP_ARMV8_A64_LDSMAXA,
    OP_ARMV8_A64_LDSMAXAL,
    OP_ARMV8_A64_LDSMAXL,
    OP_ARMV8_A64_LDSMINB,
    OP_ARMV8_A64_LDSMINAB,
    OP_ARMV8_A64_LDSMINALB,
    OP_ARMV8_A64_LDSMINLB,
    OP_ARMV8_A64_LDSMINH,
    OP_ARMV8_A64_LDSMINAH,
    OP_ARMV8_A64_LDSMINALH,
    OP_ARMV8_A64_LDSMINLH,
    OP_ARMV8_A64_LDSMIN,
    OP_ARMV8_A64_LDSMINA,
    OP_ARMV8_A64_LDSMINAL,
    OP_ARMV8_A64_LDSMINL,
    OP_ARMV8_A64_LDTR,
    OP_ARMV8_A64_LDTRB,
    OP_ARMV8_A64_LDTRH,
    OP_ARMV8_A64_LDTRSB,
    OP_ARMV8_A64_LDTRSH,
    OP_ARMV8_A64_LDTRSW,
    OP_ARMV8_A64_LDUMAXB,
    OP_ARMV8_A64_LDUMAXAB,
    OP_ARMV8_A64_LDUMAXALB,
    OP_ARMV8_A64_LDUMAXLB,
    OP_ARMV8_A64_LDUMAXH,
    OP_ARMV8_A64_LDUMAXAH,
    OP_ARMV8_A64_LDUMAXALH,
    OP_ARMV8_A64_LDUMAXLH,
    OP_ARMV8_A64_LDUMAX,
    OP_ARMV8_A64_LDUMAXA,
    OP_ARMV8_A64_LDUMAXAL,
    OP_ARMV8_A64_LDUMAXL,
    OP_ARMV8_A64_LDUMINB,
    OP_ARMV8_A64_LDUMINAB,
    OP_ARMV8_A64_LDUMINALB,
    OP_ARMV8_A64_LDUMINLB,
    OP_ARMV8_A64_LDUMINH,
    OP_ARMV8_A64_LDUMINAH,
    OP_ARMV8_A64_LDUMINALH,
    OP_ARMV8_A64_LDUMINLH,
    OP_ARMV8_A64_LDUMIN,
    OP_ARMV8_A64_LDUMINA,
    OP_ARMV8_A64_LDUMINAL,
    OP_ARMV8_A64_LDUMINL,
    OP_ARMV8_A64_LDUR,
    OP_ARMV8_A64_LDURB,
    OP_ARMV8_A64_LDURH,
    OP_ARMV8_A64_LDURSB,
    OP_ARMV8_A64_LDURSH,
    OP_ARMV8_A64_LDURSW,
    OP_ARMV8_A64_LDXP,
    OP_ARMV8_A64_LDXR,
    OP_ARMV8_A64_LDXRB,
    OP_ARMV8_A64_LDXRH,
    OP_ARMV8_A64_LSL,
    OP_ARMV8_A64_LSLV,
    OP_ARMV8_A64_LSR,
    OP_ARMV8_A64_LSRV,
    OP_ARMV8_A64_MADD,
    OP_ARMV8_A64_MNEG,
    OP_ARMV8_A64_MOV,
    OP_ARMV8_A64_MOVK,
    OP_ARMV8_A64_MOVN,
    OP_ARMV8_A64_MOVZ,
    OP_ARMV8_A64_MRS,
    OP_ARMV8_A64_MSR,
    OP_ARMV8_A64_MSUB,
    OP_ARMV8_A64_MUL,
    OP_ARMV8_A64_MVN,
    OP_ARMV8_A64_NEG,
    OP_ARMV8_A64_NEGS,
    OP_ARMV8_A64_NGC,
    OP_ARMV8_A64_NGCS,
    OP_ARMV8_A64_NOP,
    OP_ARMV8_A64_ORR,
    OP_ARMV8_A64_ORN,
    OP_ARMV8_A64_PACDA,
    OP_ARMV8_A64_PACDZA,
    OP_ARMV8_A64_PACDB,
    OP_ARMV8_A64_PACDZB,
    OP_ARMV8_A64_PACGA,
    OP_ARMV8_A64_PACIA,
    OP_ARMV8_A64_PACIA1716,
    OP_ARMV8_A64_PACIASP,
    OP_ARMV8_A64_PACIAZ,
    OP_ARMV8_A64_PACIZA,
    OP_ARMV8_A64_PACIB,
    OP_ARMV8_A64_PACIB1716,
    OP_ARMV8_A64_PACIBSP,
    OP_ARMV8_A64_PACIBZ,
    OP_ARMV8_A64_PACIBZB,
    OP_ARMV8_A64_PRFM,
    OP_ARMV8_A64_PRFUM,
    OP_ARMV8_A64_PSBSYNC,
    OP_ARMV8_A64_PSSBB,
    OP_ARMV8_A64_RBIT,
    OP_ARMV8_A64_RET,
    OP_ARMV8_A64_RETAA,
    OP_ARMV8_A64_RETAB,
    OP_ARMV8_A64_REV,
    OP_ARMV8_A64_REV16,
    OP_ARMV8_A64_REV32,
    OP_ARMV8_A64_RMIF,
    OP_ARMV8_A64_ROR,
    OP_ARMV8_A64_RORV,
    OP_ARMV8_A64_SB,
    OP_ARMV8_A64_SBC,
    OP_ARMV8_A64_SBCS,
    OP_ARMV8_A64_SBFIZ,
    OP_ARMV8_A64_SBFM,
    OP_ARMV8_A64_SBFX,
    OP_ARMV8_A64_SDIV,
    OP_ARMV8_A64_SETF8,
    OP_ARMV8_A64_SETF16,
    OP_ARMV8_A64_SETGP,
    OP_ARMV8_A64_SETGM,
    OP_ARMV8_A64_SETGE,
    OP_ARMV8_A64_SETGPN,
    OP_ARMV8_A64_SETGMN,
    OP_ARMV8_A64_SETGEN,
    OP_ARMV8_A64_SETGPT,
    OP_ARMV8_A64_SETGMT,
    OP_ARMV8_A64_SETGET,
    OP_ARMV8_A64_SETGPTN,
    OP_ARMV8_A64_SETGMTN,
    OP_ARMV8_A64_SETGETN,
    OP_ARMV8_A64_SETP,
    OP_ARMV8_A64_SETM,
    OP_ARMV8_A64_SETE,
    OP_ARMV8_A64_SETPN,
    OP_ARMV8_A64_SETMNM,
    OP_ARMV8_A64_SETEN,
    OP_ARMV8_A64_SETPT,
    OP_ARMV8_A64_SETMT,
    OP_ARMV8_A64_SETET,
    OP_ARMV8_A64_SETPTN,
    OP_ARMV8_A64_SETMTN,
    OP_ARMV8_A64_SETETN,
    OP_ARMV8_A64_SEV,
    OP_ARMV8_A64_SEVL,
    OP_ARMV8_A64_SMADDL,
    OP_ARMV8_A64_SMC,
    OP_ARMV8_A64_SMNEGL,
    OP_ARMV8_A64_SMSTART,
    OP_ARMV8_A64_SMSTOP,
    OP_ARMV8_A64_SMSUBL,
    OP_ARMV8_A64_SMULH,
    OP_ARMV8_A64_SMULL,
    OP_ARMV8_A64_SSBB,
    OP_ARMV8_A64_ST2G,
    OP_ARMV8_A64_ST64B,
    OP_ARMV8_A64_ST64BV0,
    OP_ARMV8_A64_STADDB,
    OP_ARMV8_A64_STADDLB,
    OP_ARMV8_A64_STADDH,
    OP_ARMV8_A64_STADDLH,
    OP_ARMV8_A64_STADD,
    OP_ARMV8_A64_STADDL,
    OP_ARMV8_A64_STCLRB,
    OP_ARMV8_A64_STCLRLB,
    OP_ARMV8_A64_STCLRH,
    OP_ARMV8_A64_STCLRLH,
    OP_ARMV8_A64_STCLR,
    OP_ARMV8_A64_STCLRL,
    OP_ARMV8_A64_STEORB,
    OP_ARMV8_A64_STEROLB,
    OP_ARMV8_A64_STEORH,
    OP_ARMV8_A64_STEORLH,
    OP_ARMV8_A64_STEOR,
    OP_ARMV8_A64_STEORL,
    OP_ARMV8_A64_STG,
    OP_ARMV8_A64_STGM,
    OP_ARMV8_A64_STGP,
    OP_ARMV8_A64_STLLRB,
    OP_ARMV8_A64_STLLRH,
    OP_ARMV8_A64_STLLR,
    OP_ARMV8_A64_STLR,
    OP_ARMV8_A64_STLRB,
    OP_ARMV8_A64_STLRH,
    OP_ARMV8_A64_STLUR,
    OP_ARMV8_A64_STLURB,
    OP_ARMV8_A64_STLURH,
    OP_ARMV8_A64_STLXP,
    OP_ARMV8_A64_STLXR,
    OP_ARMV8_A64_STLXRB,
    OP_ARMV8_A64_STLXRH,
    OP_ARMV8_A64_STNP,
    OP_ARMV8_A64_STP,
    OP_ARMV8_A64_STR,
    OP_ARMV8_A64_STRB,
    OP_ARMV8_A64_STRH,
    OP_ARMV8_A64_STTR,
    OP_ARMV8_A64_STTRB,
    OP_ARMV8_A64_STTRH,
    OP_ARMV8_A64_STUR,
    OP_ARMV8_A64_STURB,
    OP_ARMV8_A64_STURH,
    OP_ARMV8_A64_STXP,
    OP_ARMV8_A64_STXR,
    OP_ARMV8_A64_STXRB,
    OP_ARMV8_A64_STXRH,
    OP_ARMV8_A64_STZ2G,
    OP_ARMV8_A64_STZG,
    OP_ARMV8_A64_STZGM,
    OP_ARMV8_A64_SUB,
    OP_ARMV8_A64_SUBG,
    OP_ARMV8_A64_SUBPS,
    OP_ARMV8_A64_SUBS,
    OP_ARMV8_A64_SVC,
    OP_ARMV8_A64_SWPB,
    OP_ARMV8_A64_SWPAB,
    OP_ARMV8_A64_SWPALB,
    OP_ARMV8_A64_SWPLB,
    OP_ARMV8_A64_SWPH,
    OP_ARMV8_A64_SWPAH,
    OP_ARMV8_A64_SWPALH,
    OP_ARMV8_A64_SWPLH,
    OP_ARMV8_A64_SWP,
    OP_ARMV8_A64_SWPA,
    OP_ARMV8_A64_SWPAL,
    OP_ARMV8_A64_SWPL,
    OP_ARMV8_A64_SXTB,
    OP_ARMV8_A64_SXTH,
    OP_ARMV8_A64_SXTW,
    OP_ARMV8_A64_SYS,
    OP_ARMV8_A64_SYSL,
    OP_ARMV8_A64_TBNZ,
    OP_ARMV8_A64_TBZ,
    OP_ARMV8_A64_TCANCEL,
    OP_ARMV8_A64_TCOMMIT,
    OP_ARMV8_A64_TLBI,
    OP_ARMV8_A64_TSTART,
    OP_ARMV8_A64_TTEST,
    OP_ARMV8_A64_TSBCSYNC,
    OP_ARMV8_A64_TST,
    OP_ARMV8_A64_UBFIZ,
    OP_ARMV8_A64_UBFM,
    OP_ARMV8_A64_UBFX,
    OP_ARMV8_A64_UDF,
    OP_ARMV8_A64_UDIV,
    OP_ARMV8_A64_UMADDL,
    OP_ARMV8_A64_UMNEGL,
    OP_ARMV8_A64_UMSUBL,
    OP_ARMV8_A64_UMULH,
    OP_ARMV8_A64_UMULL,
    OP_ARMV8_A64_UXTB,
    OP_ARMV8_A64_UXTH,
    OP_ARMV8_A64_WFE,
    OP_ARMV8_A64_WFET,
    OP_ARMV8_A64_WFI,
    OP_ARMV8_A64_WFIT,
    OP_ARMV8_A64_XAFLAG,
    OP_ARMV8_A64_XPACD,
    OP_ARMV8_A64_XPACI,
    OP_ARMV8_A64_XPACLRI,
    OP_ARMV8_A64_YIELD,
    /** @} */

    OP_ARMV8_END_OF_OPCODES
};


/** Armv8 Condition codes.    */
typedef enum DISARMV8INSTRCOND
{
    kDisArmv8InstrCond_Eq = 0,                     /**< 0 - Equal - Zero set. */
    kDisArmv8InstrCond_Ne,                         /**< 1 - Not equal - Zero clear. */

    kDisArmv8InstrCond_Cs,                         /**< 2 - Carry set (also known as 'HS'). */
    kDisArmv8InstrCond_Hs = kDisArmv8InstrCond_Cs, /**< 2 - Unsigned higher or same. */
    kDisArmv8InstrCond_Cc,                         /**< 3 - Carry clear (also known as 'LO'). */
    kDisArmv8InstrCond_Lo = kDisArmv8InstrCond_Cc, /**< 3 - Unsigned lower. */

    kDisArmv8InstrCond_Mi,                         /**< 4 - Negative result (minus). */
    kDisArmv8InstrCond_Pl,                         /**< 5 - Positive or zero result (plus). */

    kDisArmv8InstrCond_Vs,                         /**< 6 - Overflow set. */
    kDisArmv8InstrCond_Vc,                         /**< 7 - Overflow clear. */

    kDisArmv8InstrCond_Hi,                         /**< 8 - Unsigned higher. */
    kDisArmv8InstrCond_Ls,                         /**< 9 - Unsigned lower or same. */

    kDisArmv8InstrCond_Ge,                         /**< a - Signed greater or equal. */
    kDisArmv8InstrCond_Lt,                         /**< b - Signed less than. */

    kDisArmv8InstrCond_Gt,                         /**< c - Signed greater than. */
    kDisArmv8InstrCond_Le,                         /**< d - Signed less or equal. */

    kDisArmv8InstrCond_Al,                         /**< e - Condition is always true. */
    kDisArmv8InstrCond_Al1                         /**< f - Condition is always true. */
} DISARMV8INSTRCOND;


/** @defgroup grp_dis_opparam_armv8 Opcode parameters (DISOPCODE::fParam1,
 *            DISOPCODE::fParam2, DISOPCODE::fParam3)
 * @ingroup grp_dis
 * @{
 */

/**
 * Basic parameter type.
 */
typedef enum DISARMV8OPPARM
{
    /** Parameter is not used. */
    kDisArmv8OpParmNone = 0,
    /** Imediate value. */
    kDisArmv8OpParmImm,
    /** Relative address immediate. */
    kDisArmv8OpParmImmRel,
    /** General purpose register. */
    kDisArmv8OpParmGpr,
    /** System register. */
    kDisArmv8OpParmSysReg,
    /** Accessing memory from address in base register + potential offset. */
    kDisArmv8OpParmAddrInGpr,
    /** Conditional as parameter (CCMN/CCMP). */
    kDisArmv8OpParmCond
} DISARMV8OPPARM;


/**
 * Extend types.
 */
typedef enum DISARMV8OPPARMEXTEND
{
    /** No shift applied. */
    kDisArmv8OpParmExtendNone = 0,
    /** Left shift applied. */
    kDisArmv8OpParmExtendLsl,
    /** Right shift applied. */
    kDisArmv8OpParmExtendLsr,
    /** Arithmetic right shift applied. */
    kDisArmv8OpParmExtendAsr,
    /** Rotation applied. */
    kDisArmv8OpParmExtendRor,
    /** @todo Document. */
    kDisArmv8OpParmExtendUxtB,
    kDisArmv8OpParmExtendUxtH,
    kDisArmv8OpParmExtendUxtW,
    kDisArmv8OpParmExtendUxtX,
    kDisArmv8OpParmExtendSxtB,
    kDisArmv8OpParmExtendSxtH,
    kDisArmv8OpParmExtendSxtW,
    kDisArmv8OpParmExtendSxtX
} DISARMV8OPPARMEXTEND;
/** @} */

/** @} */

#endif /* !VBOX_INCLUDED_disopcode_armv8_h */

