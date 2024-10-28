/* $Id$ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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

.private_extern _TestProcA64
_TestProcA64:

        ; Miscellaneous instructions without a parameter
        nop
        yield
        wfe
        wfi
        sev
        sevl
        dgh
        xpaclri

        pacia1716
        pacib1716
        autia1716
        autib1716
        esb
        ;psb csync - Requires FEAT_SPE
        tsb csync
        ;gcsb dsync - Not supported by buildbox toolchain
        csdb
        ;clrbhb - Not supported by buildbox toolchain
        paciaz
        paciasp
        pacibz
        pacibsp
        autiaz
        autiasp
        autibz
        autibsp

        bti
        bti j
        bti c
        bti jc

        ;chkfeat x16 - Not supported by buildbox toolchain

        ; Barrier instructions

        isb #0
        isb #8
        isb #15

        sb

        ;tcommit - Requires FEAT_TME

        ; Control flow instructions
        svc #0xfefe
        hvc #0xdead
        smc #0xcafe
        brk #0xd0d0
        hlt #0xc0de
;        tcancel #0xd00f Requires FEAT_TME
        dcps1   #0xdeca
        dcps2   #0xdec0
        dcps3   #0xfeed
        b #0x100
        b #-0x100
        bl #0x100
        bl #-0x100
        b.ne #+0x1000
        b.eq #-0x1000
;        bc.ne #+0x1000 Requires FEAT_HBC
;        bc.eq #-0x1000 Requires FEAT_HBC
        cbz x0, #+0x100
        cbz x0, #-0x100
        cbz w0, #+0x100
        cbnz x0, #+0x100
        cbnz x0, #-0x100
        cbnz w0, #+0x100
        tbz w0, #13, #+0x100
        tbz x0, #63, #-0x100
        tbz w0, #8,  #+0x100
        ret x30
        ret x1
        ret x2
        ret x15
        retaa
        retab
        br  x15
        braaz x15
        brabz xzr
        blr x15
        blraaz x15
        blrabz xzr
        eret
        eretaa
        eretab
        drps
        braa xzr, sp
        braa x0, x1
        brab xzr, sp
        brab x0, x1
        blraa xzr, sp
        blraa x0, x1
        blrab xzr, sp
        blrab x0, x1

        ; System register access instructions
        msr ttbr0_el1, x0
        mrs x0, ttbr0_el1

        ; Arithmetic instructions
        add  x0, x0, #0x0
        add  x0, x1, #0x10000
        add  x0, x1, #65536
        add  x0, x0, x0
        add  x0, x1, x29
        add  x0, x1, x28, LSL #1
        add  x0, x1, x28, LSL #63
        add  x0, x1, x28, LSR #1
        add  x0, x1, x28, LSR #63
        add  x0, x1, x28, ASR #1
        add  x0, x1, x28, ASR #63
        ; ROR is reserved

        add  w0, w1, #0x0
        add  w0, w1, #0x10000
        add  w0, w1, #65536
        add  w0, w1, w29
        add  w0, w1, w28, LSL #1
        add  w0, w1, w28, LSL #31
        add  w0, w1, w28, LSR #1
        add  w0, w1, w28, LSR #31
        add  w0, w1, w28, ASR #1
        add  w0, w1, w28, ASR #31
        ; ROR is reserved

        adds x0, x0, #0x0
        adds x0, x1, #0x10000
        adds x0, x1, #65536
        adds x0, x0, x0
        adds x0, x1, x29
        adds x0, x1, x28, LSL #1
        adds x0, x1, x28, LSL #63
        adds x0, x1, x28, LSR #1
        adds x0, x1, x28, LSR #63
        adds x0, x1, x28, ASR #1
        adds x0, x1, x28, ASR #63
        ; ROR is reserved

        adds w0, w1, #0x0
        adds w0, w1, #0x10000
        adds w0, w1, #65536
        adds w0, w1, w29
        adds w0, w1, w28, LSL #1
        adds w0, w1, w28, LSL #31
        adds w0, w1, w28, LSR #1
        adds w0, w1, w28, LSR #31
        adds w0, w1, w28, ASR #1
        adds w0, w1, w28, ASR #31
        ; ROR is reserved

        sub  x0, x0, #0x0
        sub  x0, x1, #0x10000
        sub  x0, x1, #65536
        sub  x0, x0, x0
        sub  x0, x1, x29
        sub  x0, x1, x28, LSL #1
        sub  x0, x1, x28, LSL #63
        sub  x0, x1, x28, LSR #1
        sub  x0, x1, x28, LSR #63
        sub  x0, x1, x28, ASR #1
        sub  x0, x1, x28, ASR #63
        ; ROR is reserved

        sub  w0, w1, #0x0
        sub  w0, w1, #0x10000
        sub  w0, w1, #65536
        sub  w0, w1, w29
        sub  w0, w1, w28, LSL #1
        sub  w0, w1, w28, LSL #31
        sub  w0, w1, w28, LSR #1
        sub  w0, w1, w28, LSR #31
        sub  w0, w1, w28, ASR #1
        sub  w0, w1, w28, ASR #31
        ; ROR is reserved

        subs x0, x0, #0x0
        subs x0, x1, #0x10000
        subs x0, x1, #65536
        subs x0, x0, x0
        subs x0, x1, x29
        subs x0, x1, x28, LSL #1
        subs x0, x1, x28, LSL #63
        subs x0, x1, x28, LSR #1
        subs x0, x1, x28, LSR #63
        subs x0, x1, x28, ASR #1
        subs x0, x1, x28, ASR #63
        ; ROR is reserved

        subs w0, w1, #0x0
        subs w0, w1, #0x10000
        subs w0, w1, #65536
        subs w0, w1, w29
        subs w0, w1, w28, LSL #1
        subs w0, w1, w28, LSL #31
        subs w0, w1, w28, LSR #1
        subs w0, w1, w28, LSR #31
        subs w0, w1, w28, ASR #1
        subs w0, w1, w28, ASR #31
        ; ROR is reserved

        ; Aliases of subs -> cmp
        cmp x0, x1
        cmp w0, w1
        cmp x0, x1, LSL #1
        cmp w0, w1, LSL #1

        ; Logical instructions
        and  x0,  x0,  #0xffff
        and  w0,  wzr, #0xffff

        ands x0,  x0,  #0x00ffff00
        ands w10, w23, #0x55555555

        orr  x0,  x0,  #0xffff
        orr  w0,  wzr, #0xffff

        mov x0, x1 ; Alias of orr
        mov w0, w1 ; Alias of orr

        eor  x0,  x0,  #0x00ffff00
        eor w10, w23,  #0x55555555

        sbfm x0, x0, #0x1, #0x2
        sbfm w0, w0, #0xf, #0x9
        bfm  x0, x0, #0x1, #0x2
        bfm  w0, w0, #0xf, #0x9
        ubfm x0, x0, #0x1, #0x2
        ubfm w0, w0, #0xf, #0x9

        movn x0, #0xffff
        movn x0, #0xffff, LSL #16
        movn w0, #0xffff
        movn w0, #0xffff, LSL #16

        movz x0, #0xffff
        movz x0, #0xffff, LSL #48
        movz w0, #0xffff
        movz w0, #0xffff, LSL #16

        movk x0, #0xffff
        movk x0, #0xffff, LSL #32
        movk w0, #0xffff
        movk w0, #0xffff, LSL #16

        ; Logical instructions with a shifted register
        and  w0, w0, w27
        and  w0, w1, w28, LSL #1
        and  w0, w1, w28, LSL #31
        and  w0, w1, w28, LSR #1
        and  w0, w1, w28, LSR #31
        and  w0, w1, w28, ASR #1
        and  w0, w1, w28, ASR #31
        and  w0, w1, w28, ROR #1
        and  w0, w1, w28, ROR #31

        and  x0, x0, x27
        and  x0, x1, x28, LSL #1
        and  x0, x1, x28, LSL #63
        and  x0, x1, x28, LSR #1
        and  x0, x1, x28, LSR #63
        and  x0, x1, x28, ASR #1
        and  x0, x1, x28, ASR #63
        and  x0, x1, x28, ROR #1
        and  x0, x1, x28, ROR #63

        orr  w0, w0, w27
        orr  w0, w1, w28, LSL #1
        orr  w0, w1, w28, LSL #31
        orr  w0, w1, w28, LSR #1
        orr  w0, w1, w28, LSR #31
        orr  w0, w1, w28, ASR #1
        orr  w0, w1, w28, ASR #31
        orr  w0, w1, w28, ROR #1
        orr  w0, w1, w28, ROR #31

        orr  x0, x0, x27
        orr  x0, x1, x28, LSL #1
        orr  x0, x1, x28, LSL #63
        orr  x0, x1, x28, LSR #1
        orr  x0, x1, x28, LSR #63
        orr  x0, x1, x28, ASR #1
        orr  x0, x1, x28, ASR #63
        orr  x0, x1, x28, ROR #1
        orr  x0, x1, x28, ROR #63

        eor  w0, w0, w27
        eor  w0, w1, w28, LSL #1
        eor  w0, w1, w28, LSL #31
        eor  w0, w1, w28, LSR #1
        eor  w0, w1, w28, LSR #31
        eor  w0, w1, w28, ASR #1
        eor  w0, w1, w28, ASR #31
        eor  w0, w1, w28, ROR #1
        eor  w0, w1, w28, ROR #31

        eor  x0, x0, x27
        eor  x0, x1, x28, LSL #1
        eor  x0, x1, x28, LSL #63
        eor  x0, x1, x28, LSR #1
        eor  x0, x1, x28, LSR #63
        eor  x0, x1, x28, ASR #1
        eor  x0, x1, x28, ASR #63
        eor  x0, x1, x28, ROR #1
        eor  x0, x1, x28, ROR #63

        ands x0, x0, x27
        ands x0, x1, x28, LSL #1
        ands x0, x1, x28, LSL #63
        ands x0, x1, x28, LSR #1
        ands x0, x1, x28, LSR #63
        ands x0, x1, x28, ASR #1
        ands x0, x1, x28, ASR #63
        ands x0, x1, x28, ROR #1
        ands x0, x1, x28, ROR #63

        bic  w0, w0, w27
        bic  w0, w1, w28, LSL #1
        bic  w0, w1, w28, LSL #31
        bic  w0, w1, w28, LSR #1
        bic  w0, w1, w28, LSR #31
        bic  w0, w1, w28, ASR #1
        bic  w0, w1, w28, ASR #31
        bic  w0, w1, w28, ROR #1
        bic  w0, w1, w28, ROR #31

        bic  wzr, wzr, wzr
        bic  wzr, wzr, wzr, LSL #1
        bic  wzr, wzr, wzr, LSL #31
        bic  wzr, wzr, wzr, LSR #1
        bic  wzr, wzr, wzr, LSR #31
        bic  wzr, wzr, wzr, ASR #1
        bic  wzr, wzr, wzr, ASR #31
        bic  wzr, wzr, wzr, ROR #1
        bic  wzr, wzr, wzr, ROR #31

        bic  x0, x0, x27
        bic  x0, x1, x28, LSL #1
        bic  x0, x1, x28, LSL #63
        bic  x0, x1, x28, LSR #1
        bic  x0, x1, x28, LSR #63
        bic  x0, x1, x28, ASR #1
        bic  x0, x1, x28, ASR #63
        bic  x0, x1, x28, ROR #1
        bic  x0, x1, x28, ROR #63

        bic  xzr, xzr, xzr
        bic  xzr, xzr, xzr, LSL #1
        bic  xzr, xzr, xzr, LSL #63
        bic  xzr, xzr, xzr, LSR #1
        bic  xzr, xzr, xzr, LSR #63
        bic  xzr, xzr, xzr, ASR #1
        bic  xzr, xzr, xzr, ASR #63
        bic  xzr, xzr, xzr, ROR #1
        bic  xzr, xzr, xzr, ROR #63

        orn  w0, w0, w27
        orn  w0, w1, w28, LSL #1
        orn  w0, w1, w28, LSL #31
        orn  w0, w1, w28, LSR #1
        orn  w0, w1, w28, LSR #31
        orn  w0, w1, w28, ASR #1
        orn  w0, w1, w28, ASR #31
        orn  w0, w1, w28, ROR #1
        orn  w0, w1, w28, ROR #31

        orn  wzr, wzr, wzr
        orn  wzr, wzr, wzr, LSL #1
        orn  wzr, wzr, wzr, LSL #31
        orn  wzr, wzr, wzr, LSR #1
        orn  wzr, wzr, wzr, LSR #31
        orn  wzr, wzr, wzr, ASR #1
        orn  wzr, wzr, wzr, ASR #31
        orn  wzr, wzr, wzr, ROR #1
        orn  wzr, wzr, wzr, ROR #31

        orn  x0, x0, x27
        orn  x0, x1, x28, LSL #1
        orn  x0, x1, x28, LSL #63
        orn  x0, x1, x28, LSR #1
        orn  x0, x1, x28, LSR #63
        orn  x0, x1, x28, ASR #1
        orn  x0, x1, x28, ASR #63
        orn  x0, x1, x28, ROR #1
        orn  x0, x1, x28, ROR #63

        orn  xzr, xzr, xzr
        orn  xzr, xzr, xzr, LSL #1
        orn  xzr, xzr, xzr, LSL #63
        orn  xzr, xzr, xzr, LSR #1
        orn  xzr, xzr, xzr, LSR #63
        orn  xzr, xzr, xzr, ASR #1
        orn  xzr, xzr, xzr, ASR #63
        orn  xzr, xzr, xzr, ROR #1
        orn  xzr, xzr, xzr, ROR #63

        eon  w0, w0, w27
        eon  w0, w1, w28, LSL #1
        eon  w0, w1, w28, LSL #31
        eon  w0, w1, w28, LSR #1
        eon  w0, w1, w28, LSR #31
        eon  w0, w1, w28, ASR #1
        eon  w0, w1, w28, ASR #31
        eon  w0, w1, w28, ROR #1
        eon  w0, w1, w28, ROR #31

        eon  wzr, wzr, wzr
        eon  wzr, wzr, wzr, LSL #1
        eon  wzr, wzr, wzr, LSL #31
        eon  wzr, wzr, wzr, LSR #1
        eon  wzr, wzr, wzr, LSR #31
        eon  wzr, wzr, wzr, ASR #1
        eon  wzr, wzr, wzr, ASR #31
        eon  wzr, wzr, wzr, ROR #1
        eon  wzr, wzr, wzr, ROR #31

        eon  x0, x0, x27
        eon  x0, x1, x28, LSL #1
        eon  x0, x1, x28, LSL #63
        eon  x0, x1, x28, LSR #1
        eon  x0, x1, x28, LSR #63
        eon  x0, x1, x28, ASR #1
        eon  x0, x1, x28, ASR #63
        eon  x0, x1, x28, ROR #1
        eon  x0, x1, x28, ROR #63

        eon  xzr, xzr, xzr
        eon  xzr, xzr, xzr, LSL #1
        eon  xzr, xzr, xzr, LSL #63
        eon  xzr, xzr, xzr, LSR #1
        eon  xzr, xzr, xzr, LSR #63
        eon  xzr, xzr, xzr, ASR #1
        eon  xzr, xzr, xzr, ASR #63
        eon  xzr, xzr, xzr, ROR #1
        eon  xzr, xzr, xzr, ROR #63

        bics w0, w0, w27
        bics w0, w1, w28, LSL #1
        bics w0, w1, w28, LSL #31
        bics w0, w1, w28, LSR #1
        bics w0, w1, w28, LSR #31
        bics w0, w1, w28, ASR #1
        bics w0, w1, w28, ASR #31
        bics w0, w1, w28, ROR #1
        bics w0, w1, w28, ROR #31

        bics wzr, wzr, wzr
        bics wzr, wzr, wzr, LSL #1
        bics wzr, wzr, wzr, LSL #31
        bics wzr, wzr, wzr, LSR #1
        bics wzr, wzr, wzr, LSR #31
        bics wzr, wzr, wzr, ASR #1
        bics wzr, wzr, wzr, ASR #31
        bics wzr, wzr, wzr, ROR #1
        bics wzr, wzr, wzr, ROR #31

        bics x0, x0, x27
        bics x0, x1, x28, LSL #1
        bics x0, x1, x28, LSL #63
        bics x0, x1, x28, LSR #1
        bics x0, x1, x28, LSR #63
        bics x0, x1, x28, ASR #1
        bics x0, x1, x28, ASR #63
        bics x0, x1, x28, ROR #1
        bics x0, x1, x28, ROR #63

        bics xzr, xzr, xzr
        bics xzr, xzr, xzr, LSL #1
        bics xzr, xzr, xzr, LSL #63
        bics xzr, xzr, xzr, LSR #1
        bics xzr, xzr, xzr, LSR #63
        bics xzr, xzr, xzr, ASR #1
        bics xzr, xzr, xzr, ASR #63
        bics xzr, xzr, xzr, ROR #1
        bics xzr, xzr, xzr, ROR #63

        extr wzr, w0, w1, #0
        extr wzr, w0, w1, #31

        extr xzr, x0, x1, #0
        extr xzr, x0, x1, #63

        csel xzr, x1, x2, eq
        csel wzr, w1, w2, eq

        csinc xzr, x1, x2, eq
        csinc wzr, w1, w2, eq

        csinv xzr, x1, x2, eq
        csinv wzr, w1, w2, eq

        csneg xzr, x1, x2, eq
        csneg wzr, w1, w2, eq

        madd wzr, w0, w1, w2
        madd xzr, x0, x1, x2

        msub wzr, w0, w1, w2
        msub xzr, x0, x1, x2

        smaddl xzr, w0, w1, x2
        smsubl xzr, w0, w1, x2
        umaddl xzr, w0, w1, x2
        umsubl xzr, w0, w1, x2

        smulh  xzr, x1, x2
        umulh  xzr, x1, x2


        ; Memory loads
        ldrb w0, [x28]
        ldrb w0, [x28, #1]
        ldrb w0, [x28, #4095]

        ldrb w0, [sp]
        ldrb w0, [sp, #1]
        ldrb w0, [sp, #4095]

        ldrsb w0, [x28]
        ldrsb w0, [x28, #1]
        ldrsb w0, [x28, #4095]

        ldrsb w0, [sp]
        ldrsb w0, [sp, #1]
        ldrsb w0, [sp, #4095]

        ldrsb x0, [x28]
        ldrsb x0, [x28, #1]
        ldrsb x0, [x28, #4095]

        ldrsb x0, [sp]
        ldrsb x0, [sp, #1]
        ldrsb x0, [sp, #4095]

        ldrh w0, [x28]
        ldrh w0, [x28, #2]
        ldrh w0, [x28, #1024]

        ldrh w0, [sp]
        ldrh w0, [sp, #2]
        ldrh w0, [sp, #1024]

        ldrsh w0, [x28]
        ldrsh w0, [x28, #2]
        ldrsh w0, [x28, #1024]

        ldrsh w0, [sp]
        ldrsh w0, [sp, #2]
        ldrsh w0, [sp, #1024]

        ldrsh x0, [x28]
        ldrsh x0, [x28, #2]
        ldrsh x0, [x28, #1024]

        ldrsh x0, [sp]
        ldrsh x0, [sp, #2]
        ldrsh x0, [sp, #1024]

        ldr x0, [x28]
        ldr x0, [x28, #8]
        ldr x0, [x28, #32760]

        ldr x0, [sp]
        ldr x0, [sp, #8]
        ldr x0, [sp, #32760]

        ldr w0, [x28]
        ldr w0, [x28, #4]
        ldr w0, [x28, #16380]

        ldr w0, [sp]
        ldr w0, [sp, #4]
        ldr w0, [sp, #16380]

        ldrsw x0, [x28]
        ldrsw x0, [x28, #4]
        ldrsw x0, [x28, #16380]

        ldrsw x0, [sp]
        ldrsw x0, [sp, #4]
        ldrsw x0, [sp, #16380]

        ldrb w0, [x28, #0]!
        ldrb w0, [x28, #-256]!
        ldrb w0, [x28, #255]!

        ldrb w0, [sp, #0]!
        ldrb w0, [sp, #-256]!
        ldrb w0, [sp, #255]!

        ldrsb w0, [x28, #0]!
        ldrsb w0, [x28, #-256]!
        ldrsb w0, [x28, #255]!

        ldrsb w0, [sp, #0]!
        ldrsb w0, [sp, #-256]!
        ldrsb w0, [sp, #255]!

        ldrsb x0, [x28, #0]!
        ldrsb x0, [x28, #-256]!
        ldrsb x0, [x28, #255]!

        ldrsb x0, [sp, #0]!
        ldrsb x0, [sp, #-256]!
        ldrsb x0, [sp, #255]!

        ldrh w0, [x28, #0]!
        ldrh w0, [x28, #-256]!
        ldrh w0, [x28, #255]!

        ldrh w0, [sp, #0]!
        ldrh w0, [sp, #-256]!
        ldrh w0, [sp, #255]!

        ldrsh w0, [x28, #0]!
        ldrsh w0, [x28, #-256]!
        ldrsh w0, [x28, #255]!

        ldrsh w0, [sp, #0]!
        ldrsh w0, [sp, #-256]!
        ldrsh w0, [sp, #255]!

        ldrsh x0, [x28, #0]!
        ldrsh x0, [x28, #-256]!
        ldrsh x0, [x28, #255]!

        ldrsh x0, [sp, #0]!
        ldrsh x0, [sp, #-256]!
        ldrsh x0, [sp, #255]!

        ldr x0, [x28, #0]!
        ldr x0, [x28, #-256]!
        ldr x0, [x28, #255]!

        ldr x0, [sp, #0]!
        ldr x0, [sp, #-256]!
        ldr x0, [sp, #255]!

        ldr w0, [x28, #0]!
        ldr w0, [x28, #-256]!
        ldr w0, [x28, #255]!

        ldr w0, [sp, #0]!
        ldr w0, [sp, #-256]!
        ldr w0, [sp, #255]!

        ldrsw x0, [x28, #0]!
        ldrsw x0, [x28, #-256]!
        ldrsw x0, [x28, #255]!

        ldrsw x0, [sp, #0]!
        ldrsw x0, [sp, #-256]!
        ldrsw x0, [sp, #255]!

        ldrb w0, [x28], #0
        ldrb w0, [x28], #-256
        ldrb w0, [x28], #255

        ldrb w0, [sp], #0
        ldrb w0, [sp], #-256
        ldrb w0, [sp], #255

        ldrsb w0, [x28], #0
        ldrsb w0, [x28], #-256
        ldrsb w0, [x28], #255

        ldrsb w0, [sp], #0
        ldrsb w0, [sp], #-256
        ldrsb w0, [sp], #255

        ldrsb x0, [x28], #0
        ldrsb x0, [x28], #-256
        ldrsb x0, [x28], #255

        ldrsb x0, [sp], #0
        ldrsb x0, [sp], #-256
        ldrsb x0, [sp], #255

        ldrh w0, [x28], #0
        ldrh w0, [x28], #-256
        ldrh w0, [x28], #255

        ldrh w0, [sp], #0
        ldrh w0, [sp], #-256
        ldrh w0, [sp], #255

        ldrsh w0, [x28], #0
        ldrsh w0, [x28], #-256
        ldrsh w0, [x28], #255

        ldrsh w0, [sp], #0
        ldrsh w0, [sp], #-256
        ldrsh w0, [sp], #255

        ldrsh x0, [x28], #0
        ldrsh x0, [x28], #-256
        ldrsh x0, [x28], #255

        ldrsh x0, [sp], #0
        ldrsh x0, [sp], #-256
        ldrsh x0, [sp], #255

        ldr x0, [x28], #0
        ldr x0, [x28], #-256
        ldr x0, [x28], #255

        ldr x0, [sp], #0
        ldr x0, [sp], #-256
        ldr x0, [sp], #255

        ldr w0, [x28], #0
        ldr w0, [x28], #-256
        ldr w0, [x28], #255

        ldr w0, [sp], #0
        ldr w0, [sp], #-256
        ldr w0, [sp], #255

        ldrsw x0, [x28], #0
        ldrsw x0, [x28], #-256
        ldrsw x0, [x28], #255

        ldrsw x0, [sp], #0
        ldrsw x0, [sp], #-256
        ldrsw x0, [sp], #255


        ldurb w0, [x28]
        ldurb w0, [x28, #-256]
        ldurb w0, [x28, #255]

        ldurb w0, [sp]
        ldurb w0, [sp, #-256]
        ldurb w0, [sp, #255]

        ldursb w0, [x28]
        ldursb w0, [x28, #-256]
        ldursb w0, [x28, #255]

        ldursb w0, [sp]
        ldursb w0, [sp, #-256]
        ldursb w0, [sp, #255]

        ldursb x0, [x28]
        ldursb x0, [x28, #-256]
        ldursb x0, [x28, #255]

        ldursb x0, [sp]
        ldursb x0, [sp, #-256]
        ldursb x0, [sp, #255]

        ldurh w0, [x28]
        ldurh w0, [x28, #-256]
        ldurh w0, [x28, #255]

        ldurh w0, [sp]
        ldurh w0, [sp, #-256]
        ldurh w0, [sp, #255]

        ldursh w0, [x28]
        ldursh w0, [x28, #-256]
        ldursh w0, [x28, #255]

        ldursh w0, [sp]
        ldursh w0, [sp, #-256]
        ldursh w0, [sp, #255]

        ldursh x0, [x28]
        ldursh x0, [x28, #-256]
        ldursh x0, [x28, #255]

        ldursh x0, [sp]
        ldursh x0, [sp, #-256]
        ldursh x0, [sp, #255]

        ldur x0, [x28]
        ldur x0, [x28, #-256]
        ldur x0, [x28, #255]

        ldur x0, [sp]
        ldur x0, [sp, #-256]
        ldur x0, [sp, #255]

        ldur w0, [x28]
        ldur w0, [x28, #-256]
        ldur w0, [x28, #255]

        ldur w0, [sp]
        ldur w0, [sp, #-256]
        ldur w0, [sp, #255]

        ldursw x0, [x28]
        ldursw x0, [x28, #-256]
        ldursw x0, [x28, #255]

        ldursw x0, [sp]
        ldursw x0, [sp, #-256]
        ldursw x0, [sp, #255]


        ldtrb w0, [x28]
        ldtrb w0, [x28, #-256]
        ldtrb w0, [x28, #255]

        ldtrb w0, [sp]
        ldtrb w0, [sp, #-256]
        ldtrb w0, [sp, #255]

        ldtrsb w0, [x28]
        ldtrsb w0, [x28, #-256]
        ldtrsb w0, [x28, #255]

        ldtrsb w0, [sp]
        ldtrsb w0, [sp, #-256]
        ldtrsb w0, [sp, #255]

        ldtrsb x0, [x28]
        ldtrsb x0, [x28, #-256]
        ldtrsb x0, [x28, #255]

        ldtrsb x0, [sp]
        ldtrsb x0, [sp, #-256]
        ldtrsb x0, [sp, #255]

        ldtrh w0, [x28]
        ldtrh w0, [x28, #-256]
        ldtrh w0, [x28, #255]

        ldtrh w0, [sp]
        ldtrh w0, [sp, #-256]
        ldtrh w0, [sp, #255]

        ldtrsh w0, [x28]
        ldtrsh w0, [x28, #-256]
        ldtrsh w0, [x28, #255]

        ldtrsh w0, [sp]
        ldtrsh w0, [sp, #-256]
        ldtrsh w0, [sp, #255]

        ldtrsh x0, [x28]
        ldtrsh x0, [x28, #-256]
        ldtrsh x0, [x28, #255]

        ldtrsh x0, [sp]
        ldtrsh x0, [sp, #-256]
        ldtrsh x0, [sp, #255]

        ldtr x0, [x28]
        ldtr x0, [x28, #-256]
        ldtr x0, [x28, #255]

        ldtr x0, [sp]
        ldtr x0, [sp, #-256]
        ldtr x0, [sp, #255]

        ldtr w0, [x28]
        ldtr w0, [x28, #-256]
        ldtr w0, [x28, #255]

        ldtr w0, [sp]
        ldtr w0, [sp, #-256]
        ldtr w0, [sp, #255]

        ldtrsw x0, [x28]
        ldtrsw x0, [x28, #-256]
        ldtrsw x0, [x28, #255]

        ldtrsw x0, [sp]
        ldtrsw x0, [sp, #-256]
        ldtrsw x0, [sp, #255]


        ldp w0, w1, [x28]
        ldp w0, w1, [x28, #4]
        ldp w0, w1, [x28, #-256]
        ldp w0, w1, [x28, #252]

        ldp w0, w1, [sp]
        ldp w0, w1, [sp, #4]
        ldp w0, w1, [sp, #-256]
        ldp w0, w1, [sp, #252]

        ldp x0, x1, [x28]
        ldp x0, x1, [x28, #8]
        ldp x0, x1, [x28, #-512]
        ldp x0, x1, [x28, #504]

        ldp x0, x1, [sp]
        ldp x0, x1, [sp, #8]
        ldp x0, x1, [sp, #-512]
        ldp x0, x1, [sp, #504]

        ldp w0, w1, [x28, #4]!
        ldp w0, w1, [x28, #-256]!
        ldp w0, w1, [x28, #252]!

        ldp w0, w1, [sp, #4]!
        ldp w0, w1, [sp, #-256]!
        ldp w0, w1, [sp, #252]!

        ldp x0, x1, [x28, #8]!
        ldp x0, x1, [x28, #-512]!
        ldp x0, x1, [x28, #504]!

        ldp x0, x1, [sp, #8]!
        ldp x0, x1, [sp, #-512]!
        ldp x0, x1, [sp, #504]!

        ldp w0, w1, [x28], #4
        ldp w0, w1, [x28], #-256
        ldp w0, w1, [x28], #252

        ldp w0, w1, [sp], #4
        ldp w0, w1, [sp], #-256
        ldp w0, w1, [sp], #252

        ldp x0, x1, [x28], #8
        ldp x0, x1, [x28], #-512
        ldp x0, x1, [x28], #504

        ldp x0, x1, [sp], #8
        ldp x0, x1, [sp], #-512
        ldp x0, x1, [sp], #504

        ldnp w0, w1, [x28]
        ldnp w0, w1, [x28, #4]
        ldnp w0, w1, [x28, #-256]
        ldnp w0, w1, [x28, #252]

        ldnp w0, w1, [sp]
        ldnp w0, w1, [sp, #4]
        ldnp w0, w1, [sp, #-256]
        ldnp w0, w1, [sp, #252]

        ldnp x0, x1, [x28]
        ldnp x0, x1, [x28, #8]
        ldnp x0, x1, [x28, #-512]
        ldnp x0, x1, [x28, #504]

        ldnp x0, x1, [sp]
        ldnp x0, x1, [sp, #8]
        ldnp x0, x1, [sp, #-512]
        ldnp x0, x1, [sp, #504]

        ldr  x0, [x1, x2]
        ldr  w0, [x1, x2]
        ldr  x0, [x1, x2, SXTX #0]
        ldr  x0, [x1, x2, LSL  #3]  ; UXTX
        ldr  x0, [x1, x2, SXTX #3]
        ldr  w0, [x1, w2, UXTW #0]
        ldr  w0, [x1, w2, SXTW #0]
        ldr  w0, [x1, w2, UXTW #2]
        ldr  w0, [x1, w2, SXTW #2]

        ldr  x0, [sp, x2]
        ldr  w0, [sp, x2]
        ldr  x0, [sp, x2, SXTX #0]
        ldr  x0, [sp, x2, LSL  #3]  ; UXTX
        ldr  x0, [sp, x2, SXTX #3]
        ldr  w0, [sp, w2, UXTW #0]
        ldr  w0, [sp, w2, SXTW #0]
        ldr  w0, [sp, w2, UXTW #2]
        ldr  w0, [sp, w2, SXTW #2]

        ldrb w0, [x1, x2]
        ldrb w0, [x1, x2, LSL  #0]  ; UXTX
        ldrb w0, [x1, x2, SXTX #0]
        ldrb w0, [x1, w2, UXTW #0]
        ldrb w0, [x1, w2, SXTW #0]

        ldrb w0, [sp, x2]
        ldrb w0, [sp, x2, LSL  #0]  ; UXTX
        ldrb w0, [sp, x2, SXTX #0]
        ldrb w0, [sp, w2, UXTW #0]
        ldrb w0, [sp, w2, SXTW #0]

        ldrsb w0, [x1, x2]
        ldrsb w0, [x1, x2, LSL  #0] ; UXTX
        ldrsb w0, [x1, x2, SXTX #0]
        ldrsb w0, [x1, w2, UXTW #0]
        ldrsb w0, [x1, w2, SXTW #0]

        ldrsb w0, [sp, x2]
        ldrsb w0, [sp, x2, LSL  #0] ; UXTX
        ldrsb w0, [sp, x2, SXTX #0]
        ldrsb w0, [sp, w2, UXTW #0]
        ldrsb w0, [sp, w2, SXTW #0]

        ldrh w0, [x1, x2]
        ;ldrh w0, [x1, x2, LSL  #0]  ; UXTX
        ldrh w0, [x1, x2, SXTX #0]
        ldrh w0, [x1, x2, LSL  #1]   ; UXTX
        ldrh w0, [x1, x2, SXTX #1]
        ldrh w0, [x1, w2, UXTW #0]
        ldrh w0, [x1, w2, SXTW #0]
        ldrh w0, [x1, w2, UXTW #1]
        ldrh w0, [x1, w2, SXTW #1]

        ldrh w0, [sp, x2]
        ;ldrh w0, [sp, x2, LSL  #0]  ; UXTX
        ldrh w0, [sp, x2, SXTX #0]
        ldrh w0, [sp, x2, LSL  #1]   ; UXTX
        ldrh w0, [sp, x2, SXTX #1]
        ldrh w0, [sp, w2, UXTW #0]
        ldrh w0, [sp, w2, SXTW #0]
        ldrh w0, [sp, w2, UXTW #1]
        ldrh w0, [sp, w2, SXTW #1]

        ldrsh w0, [x1, x2]
        ;ldrsh w0, [x1, x2, LSL  #0] ; UXTX
        ldrsh w0, [x1, x2, SXTX #0]
        ldrsh w0, [x1, x2, LSL  #1]   ; UXTX
        ldrsh w0, [x1, x2, SXTX #1]
        ldrsh w0, [x1, w2, UXTW #0]
        ldrsh w0, [x1, w2, SXTW #0]
        ldrsh w0, [x1, w2, UXTW #1]
        ldrsh w0, [x1, w2, SXTW #1]

        ldrsh w0, [sp, x2]
        ;ldrsh w0, [sp, x2, LSL  #0] ; UXTX
        ldrsh w0, [sp, x2, SXTX #0]
        ldrsh w0, [sp, x2, LSL  #1]   ; UXTX
        ldrsh w0, [sp, x2, SXTX #1]
        ldrsh w0, [sp, w2, UXTW #0]
        ldrsh w0, [sp, w2, SXTW #0]
        ldrsh w0, [sp, w2, UXTW #1]
        ldrsh w0, [sp, w2, SXTW #1]

        ldrsw x0, [x1, x2]
        ;ldrsw x0, [x1, x2, LSL  #0] ; UXTX
        ldrsw x0, [x1, x2, SXTX #0]
        ldrsw x0, [x1, x2, LSL  #2]  ; UXTX
        ldrsw x0, [x1, x2, SXTX #2]
        ldrsw x0, [x1, w2, UXTW #0]
        ldrsw x0, [x1, w2, SXTW #0]
        ldrsw x0, [x1, w2, UXTW #2]
        ldrsw x0, [x1, w2, SXTW #2]

        ldrsw x0, [sp, x2]
        ;ldrsw x0, [sp, x2, LSL  #0] ; UXTX
        ldrsw x0, [sp, x2, SXTX #0]
        ldrsw x0, [sp, x2, LSL  #2]  ; UXTX
        ldrsw x0, [sp, x2, SXTX #2]
        ldrsw x0, [sp, w2, UXTW #0]
        ldrsw x0, [sp, w2, SXTW #0]
        ldrsw x0, [sp, w2, UXTW #2]
        ldrsw x0, [sp, w2, SXTW #2]

        ; Memory stores
        strb w0, [x28]
        strb w0, [x28, #1]
        strb w0, [x28, #4095]

        strb w0, [sp]
        strb w0, [sp, #1]
        strb w0, [sp, #4095]

        strh w0, [x28]
        strh w0, [x28, #2]
        strh w0, [x28, #1024]

        strh w0, [sp]
        strh w0, [sp, #2]
        strh w0, [sp, #1024]

        str x0, [x28]
        str x0, [x28, #8]
        str x0, [x28, #32760]

        str x0, [sp]
        str x0, [sp, #8]
        str x0, [sp, #32760]

        str w0, [x28]
        str w0, [x28, #4]
        str w0, [x28, #16380]

        str w0, [sp]
        str w0, [sp, #4]
        str w0, [sp, #16380]

        strb w0, [x28, #0]!
        strb w0, [x28, #-256]!
        strb w0, [x28, #255]!

        strb w0, [sp, #0]!
        strb w0, [sp, #-256]!
        strb w0, [sp, #255]!

        strh w0, [x28, #0]!
        strh w0, [x28, #-256]!
        strh w0, [x28, #255]!

        strh w0, [sp, #0]!
        strh w0, [sp, #-256]!
        strh w0, [sp, #255]!

        str x0, [x28, #0]!
        str x0, [x28, #-256]!
        str x0, [x28, #255]!

        str x0, [sp, #0]!
        str x0, [sp, #-256]!
        str x0, [sp, #255]!

        str w0, [x28, #0]!
        str w0, [x28, #-256]!
        str w0, [x28, #255]!

        str w0, [sp, #0]!
        str w0, [sp, #-256]!
        str w0, [sp, #255]!


        strb w0, [x28], #0
        strb w0, [x28], #-256
        strb w0, [x28], #255

        strb w0, [sp], #0
        strb w0, [sp], #-256
        strb w0, [sp], #255

        strh w0, [x28], #0
        strh w0, [x28], #-256
        strh w0, [x28], #255

        strh w0, [sp], #0
        strh w0, [sp], #-256
        strh w0, [sp], #255

        str x0, [x28], #0
        str x0, [x28], #-256
        str x0, [x28], #255

        str x0, [sp], #0
        str x0, [sp], #-256
        str x0, [sp], #255

        str w0, [x28], #0
        str w0, [x28], #-256
        str w0, [x28], #255

        str w0, [sp], #0
        str w0, [sp], #-256
        str w0, [sp], #255

        sturb w0, [x28]
        sturb w0, [x28, #-256]
        sturb w0, [x28, #255]

        sturb w0, [sp]
        sturb w0, [sp, #-256]
        sturb w0, [sp, #255]

        sturh w0, [x28]
        sturh w0, [x28, #-256]
        sturh w0, [x28, #255]

        sturh w0, [sp]
        sturh w0, [sp, #-256]
        sturh w0, [sp, #255]

        stur x0, [x28]
        stur x0, [x28, #-256]
        stur x0, [x28, #255]

        stur x0, [sp]
        stur x0, [sp, #-256]
        stur x0, [sp, #255]

        stur w0, [x28]
        stur w0, [x28, #-256]
        stur w0, [x28, #255]

        stur w0, [sp]
        stur w0, [sp, #-256]
        stur w0, [sp, #255]


        sttrb w0, [x28]
        sttrb w0, [x28, #-256]
        sttrb w0, [x28, #255]

        sttrb w0, [sp]
        sttrb w0, [sp, #-256]
        sttrb w0, [sp, #255]

        sttrh w0, [x28]
        sttrh w0, [x28, #-256]
        sttrh w0, [x28, #255]

        sttrh w0, [sp]
        sttrh w0, [sp, #-256]
        sttrh w0, [sp, #255]

        sttr x0, [x28]
        sttr x0, [x28, #-256]
        sttr x0, [x28, #255]

        sttr x0, [sp]
        sttr x0, [sp, #-256]
        sttr x0, [sp, #255]

        sttr w0, [x28]
        sttr w0, [x28, #-256]
        sttr w0, [x28, #255]

        sttr w0, [sp]
        sttr w0, [sp, #-256]
        sttr w0, [sp, #255]


        stp w0, w1, [x28]
        stp w0, w1, [x28, #4]
        stp w0, w1, [x28, #-256]
        stp w0, w1, [x28, #252]

        stp w0, w1, [sp]
        stp w0, w1, [sp, #4]
        stp w0, w1, [sp, #-256]
        stp w0, w1, [sp, #252]

        stp x0, x1, [x28]
        stp x0, x1, [x28, #8]
        stp x0, x1, [x28, #-512]
        stp x0, x1, [x28, #504]

        stp x0, x1, [sp]
        stp x0, x1, [sp, #8]
        stp x0, x1, [sp, #-512]
        stp x0, x1, [sp, #504]

        stp w0, w1, [x28, #4]!
        stp w0, w1, [x28, #-256]!
        stp w0, w1, [x28, #252]!

        stp w0, w1, [sp, #4]!
        stp w0, w1, [sp, #-256]!
        stp w0, w1, [sp, #252]!

        stp x0, x1, [x28, #8]!
        stp x0, x1, [x28, #-512]!
        stp x0, x1, [x28, #504]!

        stp x0, x1, [sp, #8]!
        stp x0, x1, [sp, #-512]!
        stp x0, x1, [sp, #504]!

        stp w0, w1, [x28], #4
        stp w0, w1, [x28], #-256
        stp w0, w1, [x28], #252

        stp w0, w1, [sp], #4
        stp w0, w1, [sp], #-256
        stp w0, w1, [sp], #252

        stp x0, x1, [x28], #8
        stp x0, x1, [x28], #-512
        stp x0, x1, [x28], #504

        stp x0, x1, [sp], #8
        stp x0, x1, [sp], #-512
        stp x0, x1, [sp], #504


        stnp w0, w1, [x28]
        stnp w0, w1, [x28, #4]
        stnp w0, w1, [x28, #-256]
        stnp w0, w1, [x28, #252]

        stnp w0, w1, [sp]
        stnp w0, w1, [sp, #4]
        stnp w0, w1, [sp, #-256]
        stnp w0, w1, [sp, #252]

        stnp x0, x1, [x28]
        stnp x0, x1, [x28, #8]
        stnp x0, x1, [x28, #-512]
        stnp x0, x1, [x28, #504]

        stnp x0, x1, [sp]
        stnp x0, x1, [sp, #8]
        stnp x0, x1, [sp, #-512]
        stnp x0, x1, [sp, #504]


        str  x0, [x1, x2]
        str  w0, [x1, x2]
        str  x0, [x1, x2, SXTX #0]
        str  x0, [x1, x2, LSL  #3]  ; UXTX
        str  x0, [x1, x2, SXTX #3]
        str  w0, [x1, w2, UXTW #0]
        str  w0, [x1, w2, SXTW #0]
        str  w0, [x1, w2, UXTW #2]
        str  w0, [x1, w2, SXTW #2]

        str  x0, [sp, x2]
        str  w0, [sp, x2]
        str  x0, [sp, x2, SXTX #0]
        str  x0, [sp, x2, LSL  #3]  ; UXTX
        str  x0, [sp, x2, SXTX #3]
        str  w0, [sp, w2, UXTW #0]
        str  w0, [sp, w2, SXTW #0]
        str  w0, [sp, w2, UXTW #2]
        str  w0, [sp, w2, SXTW #2]

        strb w0, [x1, x2]
        strb w0, [x1, x2, LSL  #0x0]
        strb w0, [x1, x2, SXTX #0x0]
        strb w0, [x1, w2, UXTW #0x0]
        strb w0, [x1, w2, SXTW #0x0]

        strb w0, [sp, x2]
        strb w0, [sp, x2, LSL  #0x0]
        strb w0, [sp, x2, SXTX #0x0]
        strb w0, [sp, w2, UXTW #0x0]
        strb w0, [sp, w2, SXTW #0x0]

        strh w0, [x1, x2]
        ;strh w0, [x1, x2, LSL  #0x0] ; UXTX
        strh w0, [x1, x2, SXTX #0x0]
        strh w0, [x1, x2, LSL  #1]   ; UXTX
        strh w0, [x1, x2, SXTX #1]
        strh w0, [x1, w2, UXTW #0x0]
        strh w0, [x1, w2, SXTW #0x0]
        strh w0, [x1, w2, UXTW #1]
        strh w0, [x1, w2, SXTW #1]

        strh w0, [sp, x2]
        ;strh w0, [sp, x2, LSL  #0x0] ; UXTX
        strh w0, [sp, x2, SXTX #0x0]
        strh w0, [sp, x2, LSL  #1]   ; UXTX
        strh w0, [sp, x2, SXTX #1]
        strh w0, [sp, w2, UXTW #0x0]
        strh w0, [sp, w2, SXTW #0x0]
        strh w0, [sp, w2, UXTW #1]
        strh w0, [sp, w2, SXTW #1]


        ; SIMD memory loads
        ldr b0, [x0]
        ldr b0, [x0, #4095]

        ldr b31, [sp]
        ldr b31, [sp, #4095]

        ldr h0, [x0]
        ldr h0, [x0, #8190]

        ldr h31, [sp]
        ldr h31, [sp, #8190]

        ldr s0, [x0]
        ldr s0, [x0, #16380]

        ldr s31, [sp]
        ldr s31, [sp, #16380]

        ldr d0, [x0]
        ldr d0, [x0, #32760]

        ldr d31, [sp]
        ldr d31, [sp, #32760]

        ldr q0, [x0]
        ldr q0, [x0, #65520]

        ldr q31, [sp]
        ldr q31, [sp, #65520]


        ldr  b0,  [x1, x2]
        ldr  b31, [x1, x2]
        ldr  b0,  [x1, x2, SXTX #0]
        ldr  b0,  [x1, x2, LSL  #0]  ; UXTX
        ldr  b0,  [x1, x2, SXTX #0]
        ldr  b31, [x1, w2, UXTW #0]
        ldr  b31, [x1, w2, SXTW #0]
        ldr  b31, [x1, w2, UXTW #0]
        ldr  b31, [x1, w2, SXTW #0]

        ldr  b0,  [sp, x2]
        ldr  b31, [sp, x2]
        ldr  b0,  [sp, x2, SXTX #0]
        ldr  b0,  [sp, x2, LSL  #0]  ; UXTX
        ldr  b0,  [sp, x2, SXTX #0]
        ldr  b31, [sp, w2, UXTW #0]
        ldr  b31, [sp, w2, SXTW #0]
        ldr  b31, [sp, w2, UXTW #0]
        ldr  b31, [sp, w2, SXTW #0]


        ldr  h0,  [x1, x2]
        ldr  h31, [x1, x2]
        ldr  h0,  [x1, x2, SXTX #1]
        ldr  h0,  [x1, x2, LSL  #1]  ; UXTX
        ldr  h0,  [x1, x2, SXTX #1]
        ldr  h31, [x1, w2, UXTW #0]
        ldr  h31, [x1, w2, SXTW #0]
        ldr  h31, [x1, w2, UXTW #1]
        ldr  h31, [x1, w2, SXTW #1]

        ldr  h0,  [sp, x2]
        ldr  h31, [sp, x2]
        ldr  h0,  [sp, x2, SXTX #1]
        ldr  h0,  [sp, x2, LSL  #1]  ; UXTX
        ldr  h0,  [sp, x2, SXTX #1]
        ldr  h31, [sp, w2, UXTW #0]
        ldr  h31, [sp, w2, SXTW #0]
        ldr  h31, [sp, w2, UXTW #1]
        ldr  h31, [sp, w2, SXTW #1]


        ldr  s0,  [x1, x2]
        ldr  s31, [x1, x2]
        ldr  s0,  [x1, x2, SXTX #2]
        ldr  s0,  [x1, x2, LSL  #2]  ; UXTX
        ldr  s0,  [x1, x2, SXTX #2]
        ldr  s31, [x1, w2, UXTW #0]
        ldr  s31, [x1, w2, SXTW #0]
        ldr  s31, [x1, w2, UXTW #2]
        ldr  s31, [x1, w2, SXTW #2]

        ldr  s0,  [sp, x2]
        ldr  s31, [sp, x2]
        ldr  s0,  [sp, x2, SXTX #2]
        ldr  s0,  [sp, x2, LSL  #2]  ; UXTX
        ldr  s0,  [sp, x2, SXTX #2]
        ldr  s31, [sp, w2, UXTW #0]
        ldr  s31, [sp, w2, SXTW #0]
        ldr  s31, [sp, w2, UXTW #2]
        ldr  s31, [sp, w2, SXTW #2]


        ldr  d0,  [x1, x2]
        ldr  d31, [x1, x2]
        ldr  d0,  [x1, x2, SXTX #3]
        ldr  d0,  [x1, x2, LSL  #3]  ; UXTX
        ldr  d0,  [x1, x2, SXTX #3]
        ldr  d31, [x1, w2, UXTW #0]
        ldr  d31, [x1, w2, SXTW #0]
        ldr  d31, [x1, w2, UXTW #3]
        ldr  d31, [x1, w2, SXTW #3]

        ldr  d0,  [sp, x2]
        ldr  d31, [sp, x2]
        ldr  d0,  [sp, x2, SXTX #3]
        ldr  d0,  [sp, x2, LSL  #3]  ; UXTX
        ldr  d0,  [sp, x2, SXTX #3]
        ldr  d31, [sp, w2, UXTW #0]
        ldr  d31, [sp, w2, SXTW #0]
        ldr  d31, [sp, w2, UXTW #3]
        ldr  d31, [sp, w2, SXTW #3]


        ldr  q0,  [x1, x2]
        ldr  q31, [x1, x2]
        ldr  q0,  [x1, x2, SXTX #4]
        ldr  q0,  [x1, x2, LSL  #4]  ; UXTX
        ldr  q0,  [x1, x2, SXTX #4]
        ldr  q31, [x1, w2, UXTW #0]
        ldr  q31, [x1, w2, SXTW #0]
        ldr  q31, [x1, w2, UXTW #4]
        ldr  q31, [x1, w2, SXTW #4]

        ldr  q0,  [sp, x2]
        ldr  q31, [sp, x2]
        ldr  q0,  [sp, x2, SXTX #4]
        ldr  q0,  [sp, x2, LSL  #4]  ; UXTX
        ldr  q0,  [sp, x2, SXTX #4]
        ldr  q31, [sp, w2, UXTW #0]
        ldr  q31, [sp, w2, SXTW #0]
        ldr  q31, [sp, w2, UXTW #4]
        ldr  q31, [sp, w2, SXTW #4]


        ldur b0,  [x0]
        ldur b0,  [x0, #-256]
        ldur b31, [x0, #255]

        ldur b0,  [sp]
        ldur b0,  [sp, #-256]
        ldur b31, [sp, #255]

        ldur h0,  [x0]
        ldur h0,  [x0, #-256]
        ldur h31, [x0, #255]

        ldur h0,  [sp]
        ldur h0,  [sp, #-256]
        ldur h31, [sp, #255]

        ldur s0,  [x0]
        ldur s0,  [x0, #-256]
        ldur s31, [x0, #255]

        ldur s0,  [sp]
        ldur s0,  [sp, #-256]
        ldur s31, [sp, #255]

        ldur d0,  [x0]
        ldur d0,  [x0, #-256]
        ldur d31, [x0, #255]

        ldur d0,  [sp]
        ldur d0,  [sp, #-256]
        ldur d31, [sp, #255]

        ldur q0,  [x0]
        ldur q0,  [x0, #-256]
        ldur q31, [x0, #255]

        ldur q0,  [sp]
        ldur q0,  [sp, #-256]
        ldur q31, [sp, #255]


        ldnp s0,   s1, [x0]
        ldnp s30, s31, [x0, #-256]
        ldnp s23, s24, [x0, #252]

        ldnp d0,   d1, [x0]
        ldnp d30, d31, [x0, #-512]
        ldnp d23, d24, [x0, #504]

        ldnp q0,   q1, [x0]
        ldnp q30, q31, [x0, #-1024]
        ldnp q23, q24, [x0, #1008]


        ldr b0, [x28], #0
        ldr b0, [x28], #-256
        ldr b0, [x28], #255

        ldr b31, [sp], #0
        ldr b31, [sp], #-256
        ldr b31, [sp], #255

        ldr h0, [x28], #0
        ldr h0, [x28], #-256
        ldr h0, [x28], #255

        ldr h31, [sp], #0
        ldr h31, [sp], #-256
        ldr h31, [sp], #255

        ldr s0, [x28], #0
        ldr s0, [x28], #-256
        ldr s0, [x28], #255

        ldr s31, [sp], #0
        ldr s31, [sp], #-256
        ldr s31, [sp], #255

        ldr d0, [x28], #0
        ldr d0, [x28], #-256
        ldr d0, [x28], #255

        ldr d31, [sp], #0
        ldr d31, [sp], #-256
        ldr d31, [sp], #255

        ldr q0, [x28], #0
        ldr q0, [x28], #-256
        ldr q0, [x28], #255

        ldr q31, [sp], #0
        ldr q31, [sp], #-256
        ldr q31, [sp], #255


        ldr b0, [x28, #0]!
        ldr b0, [x28, #-256]!
        ldr b0, [x28, #255]!

        ldr b31, [sp, #0]!
        ldr b31, [sp, #-256]!
        ldr b31, [sp, #255]!

        ldr h0, [x28, #0]!
        ldr h0, [x28, #-256]!
        ldr h0, [x28, #255]!

        ldr h31, [sp, #0]!
        ldr h31, [sp, #-256]!
        ldr h31, [sp, #255]!

        ldr s0, [x28, #0]!
        ldr s0, [x28, #-256]!
        ldr s0, [x28, #255]!

        ldr s31, [sp, #0]!
        ldr s31, [sp, #-256]!
        ldr s31, [sp, #255]!

        ldr d0, [x28, #0]!
        ldr d0, [x28, #-256]!
        ldr d0, [x28, #255]!

        ldr d31, [sp, #0]!
        ldr d31, [sp, #-256]!
        ldr d31, [sp, #255]!

        ldr q0, [x28, #0]!
        ldr q0, [x28, #-256]!
        ldr q0, [x28, #255]!

        ldr q31, [sp, #0]!
        ldr q31, [sp, #-256]!
        ldr q31, [sp, #255]!


        ; SIMD memory stores
        str b0, [x0]
        str b0, [x0, #4095]

        str b31, [sp]
        str b31, [sp, #4095]

        str h0, [x0]
        str h0, [x0, #8190]

        str h31, [sp]
        str h31, [sp, #8190]

        str s0, [x0]
        str s0, [x0, #16380]

        str s31, [sp]
        str s31, [sp, #16380]

        str d0, [x0]
        str d0, [x0, #32760]

        str d31, [sp]
        str d31, [sp, #32760]

        str q0, [x0]
        str q0, [x0, #65520]

        str q31, [sp]
        str q31, [sp, #65520]


        str  b0,  [x1, x2]
        str  b31, [x1, x2]
        str  b0,  [x1, x2, SXTX #0]
        str  b0,  [x1, x2, LSL  #0]  ; UXTX
        str  b0,  [x1, x2, SXTX #0]
        str  b31, [x1, w2, UXTW #0]
        str  b31, [x1, w2, SXTW #0]
        str  b31, [x1, w2, UXTW #0]
        str  b31, [x1, w2, SXTW #0]

        str  b0,  [sp, x2]
        str  b31, [sp, x2]
        str  b0,  [sp, x2, SXTX #0]
        str  b0,  [sp, x2, LSL  #0]  ; UXTX
        str  b0,  [sp, x2, SXTX #0]
        str  b31, [sp, w2, UXTW #0]
        str  b31, [sp, w2, SXTW #0]
        str  b31, [sp, w2, UXTW #0]
        str  b31, [sp, w2, SXTW #0]


        str  h0,  [x1, x2]
        str  h31, [x1, x2]
        str  h0,  [x1, x2, SXTX #1]
        str  h0,  [x1, x2, LSL  #1]  ; UXTX
        str  h0,  [x1, x2, SXTX #1]
        str  h31, [x1, w2, UXTW #0]
        str  h31, [x1, w2, SXTW #0]
        str  h31, [x1, w2, UXTW #1]
        str  h31, [x1, w2, SXTW #1]

        str  h0,  [sp, x2]
        str  h31, [sp, x2]
        str  h0,  [sp, x2, SXTX #1]
        str  h0,  [sp, x2, LSL  #1]  ; UXTX
        str  h0,  [sp, x2, SXTX #1]
        str  h31, [sp, w2, UXTW #0]
        str  h31, [sp, w2, SXTW #0]
        str  h31, [sp, w2, UXTW #1]
        str  h31, [sp, w2, SXTW #1]


        str  s0,  [x1, x2]
        str  s31, [x1, x2]
        str  s0,  [x1, x2, SXTX #2]
        str  s0,  [x1, x2, LSL  #2]  ; UXTX
        str  s0,  [x1, x2, SXTX #2]
        str  s31, [x1, w2, UXTW #0]
        str  s31, [x1, w2, SXTW #0]
        str  s31, [x1, w2, UXTW #2]
        str  s31, [x1, w2, SXTW #2]

        str  s0,  [sp, x2]
        str  s31, [sp, x2]
        str  s0,  [sp, x2, SXTX #2]
        str  s0,  [sp, x2, LSL  #2]  ; UXTX
        str  s0,  [sp, x2, SXTX #2]
        str  s31, [sp, w2, UXTW #0]
        str  s31, [sp, w2, SXTW #0]
        str  s31, [sp, w2, UXTW #2]
        str  s31, [sp, w2, SXTW #2]


        str  d0,  [x1, x2]
        str  d31, [x1, x2]
        str  d0,  [x1, x2, SXTX #3]
        str  d0,  [x1, x2, LSL  #3]  ; UXTX
        str  d0,  [x1, x2, SXTX #3]
        str  d31, [x1, w2, UXTW #0]
        str  d31, [x1, w2, SXTW #0]
        str  d31, [x1, w2, UXTW #3]
        str  d31, [x1, w2, SXTW #3]

        str  d0,  [sp, x2]
        str  d31, [sp, x2]
        str  d0,  [sp, x2, SXTX #3]
        str  d0,  [sp, x2, LSL  #3]  ; UXTX
        str  d0,  [sp, x2, SXTX #3]
        str  d31, [sp, w2, UXTW #0]
        str  d31, [sp, w2, SXTW #0]
        str  d31, [sp, w2, UXTW #3]
        str  d31, [sp, w2, SXTW #3]


        str  q0,  [x1, x2]
        str  q31, [x1, x2]
        str  q0,  [x1, x2, SXTX #4]
        str  q0,  [x1, x2, LSL  #4]  ; UXTX
        str  q0,  [x1, x2, SXTX #4]
        str  q31, [x1, w2, UXTW #0]
        str  q31, [x1, w2, SXTW #0]
        str  q31, [x1, w2, UXTW #4]
        str  q31, [x1, w2, SXTW #4]

        str  q0,  [sp, x2]
        str  q31, [sp, x2]
        str  q0,  [sp, x2, SXTX #4]
        str  q0,  [sp, x2, LSL  #4]  ; UXTX
        str  q0,  [sp, x2, SXTX #4]
        str  q31, [sp, w2, UXTW #0]
        str  q31, [sp, w2, SXTW #0]
        str  q31, [sp, w2, UXTW #4]
        str  q31, [sp, w2, SXTW #4]


        stur b0,  [x0]
        stur b0,  [x0, #-256]
        stur b31, [x0, #255]

        stur b0,  [sp]
        stur b0,  [sp, #-256]
        stur b31, [sp, #255]

        stur h0,  [x0]
        stur h0,  [x0, #-256]
        stur h31, [x0, #255]

        stur h0,  [sp]
        stur h0,  [sp, #-256]
        stur h31, [sp, #255]

        stur s0,  [x0]
        stur s0,  [x0, #-256]
        stur s31, [x0, #255]

        stur s0,  [sp]
        stur s0,  [sp, #-256]
        stur s31, [sp, #255]

        stur d0,  [x0]
        stur d0,  [x0, #-256]
        stur d31, [x0, #255]

        stur d0,  [sp]
        stur d0,  [sp, #-256]
        stur d31, [sp, #255]

        stur q0,  [x0]
        stur q0,  [x0, #-256]
        stur q31, [x0, #255]

        stur q0,  [sp]
        stur q0,  [sp, #-256]
        stur q31, [sp, #255]


        stnp s0,   s1, [x0]
        stnp s30, s31, [x0, #-256]
        stnp s23, s24, [x0, #252]

        stnp d0,   d1, [x0]
        stnp d30, d31, [x0, #-512]
        stnp d23, d24, [x0, #504]

        stnp q0,   q1, [x0]
        stnp q30, q31, [x0, #-1024]
        stnp q23, q24, [x0, #1008]


        str b0, [x28], #0
        str b0, [x28], #-256
        str b0, [x28], #255

        str b31, [sp], #0
        str b31, [sp], #-256
        str b31, [sp], #255

        str h0, [x28], #0
        str h0, [x28], #-256
        str h0, [x28], #255

        str h31, [sp], #0
        str h31, [sp], #-256
        str h31, [sp], #255

        str s0, [x28], #0
        str s0, [x28], #-256
        str s0, [x28], #255

        str s31, [sp], #0
        str s31, [sp], #-256
        str s31, [sp], #255

        str d0, [x28], #0
        str d0, [x28], #-256
        str d0, [x28], #255

        str d31, [sp], #0
        str d31, [sp], #-256
        str d31, [sp], #255

        str q0, [x28], #0
        str q0, [x28], #-256
        str q0, [x28], #255

        str q31, [sp], #0
        str q31, [sp], #-256
        str q31, [sp], #255


        str b0, [x28, #0]!
        str b0, [x28, #-256]!
        str b0, [x28, #255]!

        str b31, [sp, #0]!
        str b31, [sp, #-256]!
        str b31, [sp, #255]!

        str h0, [x28, #0]!
        str h0, [x28, #-256]!
        str h0, [x28, #255]!

        str h31, [sp, #0]!
        str h31, [sp, #-256]!
        str h31, [sp, #255]!

        str s0, [x28, #0]!
        str s0, [x28, #-256]!
        str s0, [x28, #255]!

        str s31, [sp, #0]!
        str s31, [sp, #-256]!
        str s31, [sp, #255]!

        str d0, [x28, #0]!
        str d0, [x28, #-256]!
        str d0, [x28, #255]!

        str d31, [sp, #0]!
        str d31, [sp, #-256]!
        str d31, [sp, #255]!

        str q0, [x28, #0]!
        str q0, [x28, #-256]!
        str q0, [x28, #255]!

        str q31, [sp, #0]!
        str q31, [sp, #-256]!
        str q31, [sp, #255]!


        ; Add/subtract with carry

        adc w0, w1, wzr
        adc x0, x1, xzr

        adcs w0, w1, wzr
        adcs x0, x1, xzr

        sbc w0, w1, wzr
        sbc x0, x1, xzr

        sbcs w0, w1, wzr
        sbcs x0, x1, xzr


        rmif x0,  #0,  #0
        rmif xzr, #63, #0xf


        setf8 w0
        setf8 wzr

        setf16 w0
        setf16 wzr


        ; Conditional compare
        ccmp x0, x1, #0x3, eq
        ccmp w0, w1, #0xf, eq
        ccmp x0, x1, #0x3, ne
        ccmp w0, w1, #0xf, ne
        ccmp x0, x1, #0x3, cs
        ccmp w0, w1, #0xf, cc
        ccmp x0, x1, #0x3, mi
        ccmp w0, w1, #0xf, mi
        ccmp x0, x1, #0x3, pl
        ccmp w0, w1, #0xf, vs
        ccmp x0, x1, #0x3, vc
        ccmp w0, w1, #0xf, vc
        ccmp x0, x1, #0x3, hi
        ccmp w0, w1, #0xf, hi
        ccmp x0, x1, #0x3, ls
        ccmp w0, w1, #0xf, ls
        ccmp x0, x1, #0x3, ge
        ccmp w0, w1, #0xf, ge
        ccmp x0, x1, #0x3, lt
        ccmp w0, w1, #0xf, lt
        ccmp x0, x1, #0x3, gt
        ccmp w0, w1, #0xf, gt
        ccmp x0, x1, #0x3, le
        ccmp w0, w1, #0xf, le
        ccmp x0, x1, #0x3, al
        ccmp w0, w1, #0xf, al

        ccmn x0, x1, #0x3, eq
        ccmn w0, w1, #0xf, eq
        ccmn x0, x1, #0x3, ne
        ccmn w0, w1, #0xf, ne
        ccmn x0, x1, #0x3, cs
        ccmn w0, w1, #0xf, cc
        ccmn x0, x1, #0x3, mi
        ccmn w0, w1, #0xf, mi
        ccmn x0, x1, #0x3, pl
        ccmn w0, w1, #0xf, vs
        ccmn x0, x1, #0x3, vc
        ccmn w0, w1, #0xf, vc
        ccmn x0, x1, #0x3, hi
        ccmn w0, w1, #0xf, hi
        ccmn x0, x1, #0x3, ls
        ccmn w0, w1, #0xf, ls
        ccmn x0, x1, #0x3, ge
        ccmn w0, w1, #0xf, ge
        ccmn x0, x1, #0x3, lt
        ccmn w0, w1, #0xf, lt
        ccmn x0, x1, #0x3, gt
        ccmn w0, w1, #0xf, gt
        ccmn x0, x1, #0x3, le
        ccmn w0, w1, #0xf, le
        ccmn x0, x1, #0x3, al
        ccmn w0, w1, #0xf, al

        ccmp x0, #31, #0x3, eq
        ccmp w0, #30, #0xf, eq
        ccmp x0, #29, #0x3, ne
        ccmp w0, #28, #0xf, ne
        ccmp x0, #27, #0x3, cs
        ccmp w0, #26, #0xf, cc
        ccmp x0, #25, #0x3, mi
        ccmp w0, #24, #0xf, mi
        ccmp x0, #23, #0x3, pl
        ccmp w0, #22, #0xf, vs
        ccmp x0, #21, #0x3, vc
        ccmp w0, #20, #0xf, vc
        ccmp x0, #19, #0x3, hi
        ccmp w0, #18, #0xf, hi
        ccmp x0, #17, #0x3, ls
        ccmp w0, #16, #0xf, ls
        ccmp x0, #15, #0x3, ge
        ccmp w0, #14, #0xf, ge
        ccmp x0, #13, #0x3, lt
        ccmp w0, #12, #0xf, lt
        ccmp x0, #11, #0x3, gt
        ccmp w0, #10, #0xf, gt
        ccmp x0, #9,  #0x3, le
        ccmp w0, #8,  #0xf, le
        ccmp x0, #7,  #0x3, al
        ccmp w0, #0,  #0xf, al

        ccmn x0, #31, #0x3, eq
        ccmn w0, #30, #0xf, eq
        ccmn x0, #29, #0x3, ne
        ccmn w0, #28, #0xf, ne
        ccmn x0, #27, #0x3, cs
        ccmn w0, #26, #0xf, cc
        ccmn x0, #25, #0x3, mi
        ccmn w0, #24, #0xf, mi
        ccmn x0, #23, #0x3, pl
        ccmn w0, #22, #0xf, vs
        ccmn x0, #21, #0x3, vc
        ccmn w0, #20, #0xf, vc
        ccmn x0, #19, #0x3, hi
        ccmn w0, #18, #0xf, hi
        ccmn x0, #17, #0x3, ls
        ccmn w0, #16, #0xf, ls
        ccmn x0, #15, #0x3, ge
        ccmn w0, #8,  #0xf, ge
        ccmn x0, #7,  #0x3, lt
        ccmn w0, #6,  #0xf, lt
        ccmn x0, #5,  #0x3, gt
        ccmn w0, #4,  #0xf, gt
        ccmn x0, #3,  #0x3, le
        ccmn w0, #2,  #0xf, le
        ccmn x0, #1,  #0x3, al
        ccmn w0, #0,  #0xf, al

        msr spsel,   #1
        msr spsel,   #0
        msr daifset, #0
        msr daifset, #15
        msr daifclr, #0
        msr daifclr, #15
        msr uao,     #1
        msr uao,     #0
        msr pan,     #1
        msr pan,     #0
        ; msr allint,  #1 Not supported by the toolchain
        ; msr allint,  #0 Not supported by the toolchain
        ;msr pm,      #1 Not supported by the maca1 toolchain
        ;msr pm,      #0 Not supported by the maca1 toolchain
        msr ssbs,    #1
        msr ssbs,    #0
        msr dit,     #1
        msr dit,     #0
        ; msr tco,     #1 Not supported by the toolchain
        ; msr tco,     #0 Not supported by the toolchain
        ; msr svcrsm,  #1 Not supported by the toolchain
        ; msr svcrsm,  #0 Not supported by the toolchain
        ; msr svcrza,  #1 Not supported by the toolchain
        ; msr svcrza,  #0 Not supported by the toolchain
        ; msr svcrsma, #1 Not supported by the toolchain
        ; msr svcrsma, #0 Not supported by the toolchain

        ; Add/subtract immediate with tags

        ;addg x0, x1, #0, #0       - Requires FEAT_MTE
        ;addg sp, sp, #1008, #0x0f - Requires FEAT_MTE

        ;subg x0, x1, #0, #0       - Requires FEAT_MTE
        ;subg sp, sp, #1008, #0x0f - Requires FEAT_MTE

        ; Floating Point instructions.
        fmadd  s0, s1, s3, s31
        fmadd  d0, d1, d3, d31
        fmadd  h0, h1, h3, h31

        fmsub  s0, s1, s3, s31
        fmsub  d0, d1, d3, d31
        fmsub  h0, h1, h3, h31

        fnmadd s0, s1, s3, s31
        fnmadd d0, d1, d3, d31
        fnmadd h0, h1, h3, h31

        fnmsub s0, s1, s3, s31
        fnmsub d0, d1, d3, d31
        fnmsub h0, h1, h3, h31

        fcvtzu x0, s0, #1
        fcvtzu x0, s0, #31
        fcvtzu x0, s0, #63
        fcvtzu w0, s0, #1
        fcvtzu w0, s0, #32

        fcvtzu x0, d0, #1
        fcvtzu x0, d0, #31
        fcvtzu x0, d0, #63
        fcvtzu w0, d0, #1
        fcvtzu w0, d0, #32

        fcvtzu x0, h0, #1
        fcvtzu x0, h0, #31
        fcvtzu x0, h0, #63
        fcvtzu w0, h0, #1
        fcvtzu w0, h0, #32

        fcvtzs x0, s0, #1
        fcvtzs x0, s0, #31
        fcvtzs x0, s0, #63
        fcvtzs w0, s0, #1
        fcvtzs w0, s0, #32

        fcvtzs x0, d0, #1
        fcvtzs x0, d0, #31
        fcvtzs x0, d0, #63
        fcvtzs w0, d0, #1
        fcvtzs w0, d0, #32

        fcvtzs x0, h0, #1
        fcvtzs x0, h0, #31
        fcvtzs x0, h0, #63
        fcvtzs w0, h0, #1
        fcvtzs w0, h0, #32

        ucvtf  s0, x0, #1
        ucvtf  s0, x0, #31
        ucvtf  s0, x0, #63
        ucvtf  s0, w0, #1
        ucvtf  s0, w0, #32

        ucvtf  d0, x0, #1
        ucvtf  d0, x0, #31
        ucvtf  d0, x0, #63
        ucvtf  d0, w0, #1
        ucvtf  d0, w0, #32

        ucvtf  h0, x0, #1
        ucvtf  h0, x0, #31
        ucvtf  h0, x0, #63
        ucvtf  h0, w0, #1
        ucvtf  h0, w0, #32

        scvtf  s0, x0, #1
        scvtf  s0, x0, #31
        scvtf  s0, x0, #63
        scvtf  s0, w0, #1
        scvtf  s0, w0, #32

        scvtf  d0, x0, #1
        scvtf  d0, x0, #31
        scvtf  d0, x0, #63
        scvtf  d0, w0, #1
        scvtf  d0, w0, #32

        scvtf  h0, x0, #1
        scvtf  h0, x0, #31
        scvtf  h0, x0, #63
        scvtf  h0, w0, #1
        scvtf  h0, w0, #32

        fcsel  s0, s1, s2, eq
        fcsel  d0, d1, d2, eq
        fcsel  h0, h1, h2, eq

        fmul   s0, s1, s2
        fmul   d0, d1, d2
        fmul   h0, h1, h2

        fdiv   s0, s1, s2
        fdiv   d0, d1, d2
        fdiv   h0, h1, h2

        fadd   s0, s1, s2
        fadd   d0, d1, d2
        fadd   h0, h1, h2

        fsub   s0, s1, s2
        fsub   d0, d1, d2
        fsub   h0, h1, h2

        fmax   s0, s1, s2
        fmax   d0, d1, d2
        fmax   h0, h1, h2

        fmin   s0, s1, s2
        fmin   d0, d1, d2
        fmin   h0, h1, h2

        fmaxnm s0, s1, s2
        fmaxnm d0, d1, d2
        fmaxnm h0, h1, h2

        fminnm s0, s1, s2
        fminnm d0, d1, d2
        fminnm h0, h1, h2

        fnmul  s0, s1, s2
        fnmul  d0, d1, d2
        fnmul  h0, h1, h2

        fccmp  s0, s1, #0,  eq
        fccmp  s0, s1, #15, ne
        fccmp  d0, d1, #0,  eq
        fccmp  d0, d1, #15, ne
        fccmp  h0, h1, #0,  eq
        fccmp  h0, h1, #15, ne

        fccmpe s0, s1, #0,  eq
        fccmpe s0, s1, #15, ne
        fccmpe d0, d1, #0,  eq
        fccmpe d0, d1, #15, ne
        fccmpe h0, h1, #0,  eq
        fccmpe h0, h1, #15, ne

        ;fmov   s0, #1 @todo Needs FP immediate parsing

        fcmp s0, s1
        ;fcmp s0, #0.0
        fcmp d0, d1
        ;fcmp d0, #0.0
        fcmp h0, h1
        ;fcmp h0, #0.0

        fcmpe s0, s1
        ;fcmpe s0, #0.0
        fcmpe d0, d1
        ;fcmpe d0, #0.0
        fcmpe h0, h1
        ;fcmpe h0, #0.0

        fmov  s0, s1
        fmov  d0, d1
        fmov  h0, h1

        fabs  s0, s1
        fabs  d0, d1
        fabs  h0, h1

        fneg  s0, s1
        fneg  d0, d1
        fneg  h0, h1

        fsqrt s0, s1
        fsqrt d0, d1
        fsqrt h0, h1

        fcvt s0, d1
        fcvt s0, h1
        fcvt d0, s1
        fcvt d0, h1
        fcvt h0, s1
        fcvt h0, d1

        frintn s0, s1
        frintn d0, d1
        frintn h0, h1

        frintp s0, s1
        frintp d0, d1
        frintp h0, h1

        frintm s0, s1
        frintm d0, d1
        frintm h0, h1

        frintz s0, s1
        frintz d0, d1
        frintz h0, h1

        frinta s0, s1
        frinta d0, d1
        frinta h0, h1

        frintx s0, s1
        frintx d0, d1
        frintx h0, h1

        frinti s0, s1
        frinti d0, d1
        frinti h0, h1

        frint32z s0, s1
        frint32z d0, d1

        frint32x s0, s1
        frint32x d0, d1

        frint64z s0, s1
        frint64z d0, d1

        frint64x s0, s1
        frint64x d0, d1

        sshr d0, d1, #1
        sshr d0, d1, #64

        ssra d0, d1, #1
        ssra d0, d1, #64

        srshr d0, d1, #1
        srshr d0, d1, #64

        srsra d0, d1, #1
        srsra d0, d1, #64

        ; @todo
        ;shl   d0, d1, #0
        ;shl   d0, d1, #63

        ;sqshl d0, d1, #0
        ;sqshl d0, d1, #63

        ;sqshrn q0, q1, #1
        ;sqshrn q0, q1, #64

        ;sqrshrn q0, dq, #1
        ;sqrshrn q0, q1, #64

        dsb #0
        dsb #1
        dsb #2
        dsb #3
        dsb #4
        dsb #5
        dsb #6
        dsb #7
        dsb #8
        dsb #9
        dsb #0xa
        dsb #0xb
        dsb #0xc
        dsb #0xd
        dsb #0xe
        dsb #0xf

        ;subps x0, x1, x3 - Requires FEAT_MTE
        ;subps x0, sp, sp - Requires FEAT_MTE

        ;subp  x0, x1, x3 - Requires FEAT_MTE
        ;subp  x0, sp, sp - Requires FEAT_MTE

        ;gmi   x0, x1, x3  - Requires FEAT_MTE
        ;gmi   x0, sp, x3  - Requires FEAT_MTE

        ;irg   x0, x1, x3  - Requires FEAT_MTE
        ;irg   x0, x1, xzr - Requires FEAT_MTE
        ;irg   sp, sp, x3  - Requires FEAT_MTE
        ;irg   sp, sp, xzr - Requires FEAT_MTE

        pacga  xzr, x0, x1
        pacga  x28, x0, sp


        udiv w0, w1, wzr
        udiv x0, x1, xzr

        sdiv w0, w1, wzr
        sdiv x0, x1, xzr

        lslv w0, w1, wzr
        lslv x0, x1, xzr

        lsrv w0, w1, wzr
        lsrv x0, x1, xzr

        asrv w0, w1, wzr
        asrv x0, x1, xzr

        rorv w0, w1, wzr
        rorv x0, x1, xzr

        crc32b w0, w1, wzr
        crc32w w0, w1, wzr
        crc32h w0, w1, wzr

        crc32cb w0, w1, wzr
        crc32cw w0, w1, wzr
        crc32ch w0, w1, wzr

        crc32x w0, w1, x3
        crc32x w0, w1, xzr

        crc32cx w0, w1, x3
        crc32cx w0, w1, xzr

        ;umax w0, w1, wzr - Requires FEAT_CSSC
        ;umax x0, x1, xzr - Requires FEAT_CSSC

        ;smax w0, w1, wzr - Requires FEAT_CSSC
        ;smax x0, x1, xzr - Requires FEAT_CSSC

        ;umin w0, w1, wzr - Requires FEAT_CSSC
        ;umin x0, x1, xzr - Requires FEAT_CSSC

        ;smin w0, w1, wzr - Requires FEAT_CSSC
        ;smin x0, x1, xzr - Requires FEAT_CSSC

        rbit w0, wzr
        rbit w0, w1
        rbit x0, xzr
        rbit x0, x1

        rev16 w0, wzr
        rev16 w0, w1
        rev16 x0, xzr
        rev16 x0, x1

        rev   w0, wzr
        rev   w0, w1
        rev   x0, xzr
        rev   x0, x1

        ; @todo rev32

        clz   w0, wzr
        clz   w0, w1
        clz   x0, xzr
        clz   x0, x1

        cls   w0, wzr
        cls   w0, w1
        cls   x0, xzr
        cls   x0, x1

        ;ctz   w0, wzr - Requires FEAT_CSSC
        ;ctz   w0, w1  - Requires FEAT_CSSC
        ;ctz   x0, xzr - Requires FEAT_CSSC
        ;ctz   x0, x1  - Requires FEAT_CSSC

        ;cnt   w0, wzr - Requires FEAT_CSSC
        ;cnt   w0, w1  - Requires FEAT_CSSC
        ;cnt   x0, xzr - Requires FEAT_CSSC
        ;cnt   x0, x1  - Requires FEAT_CSSC

        ;
        ; Keep last so the testcase can catch errors in
        ; the disassembly of the last instruction.
        ;
        nop

.private_extern _TestProcA64_EndProc
_TestProcA64_EndProc:
