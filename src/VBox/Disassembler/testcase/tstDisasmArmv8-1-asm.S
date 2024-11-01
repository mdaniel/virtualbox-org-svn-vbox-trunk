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

        ; Add/Sub extended register
        add w0, w1, w2

        add x0, x1, x2
        add sp, sp, xzr

        add w0,  w1,  w2,  UXTB #1
        add w0,  w1,  w2,  UXTB #2
        add w0,  w1,  w2,  UXTB #3
        add w0,  w1,  w2,  UXTB #4
        add w28, w29, wzr, UXTB #1
        add w28, w29, wzr, UXTB #2
        add w28, w29, wzr, UXTB #3
        add w28, w29, wzr, UXTB #4

        add x0, x1,  w2, UXTB #1
        add x0, x1,  w2, UXTB #2
        add x0, x1,  w2, UXTB #3
        add x0, x1,  w2, UXTB #4
        add sp, sp, wzr, UXTB #1
        add sp, sp, wzr, UXTB #2
        add sp, sp, wzr, UXTB #3
        add sp, sp, wzr, UXTB #4

        add w0,  w1,  w2,  UXTH #1
        add w0,  w1,  w2,  UXTH #2
        add w0,  w1,  w2,  UXTH #3
        add w0,  w1,  w2,  UXTH #4
        add w28, w29, wzr, UXTH #1
        add w28, w29, wzr, UXTH #2
        add w28, w29, wzr, UXTH #3
        add w28, w29, wzr, UXTH #4

        add x0, x1,  w2, UXTH #1
        add x0, x1,  w2, UXTH #2
        add x0, x1,  w2, UXTH #3
        add x0, x1,  w2, UXTH #4
        add sp, sp, wzr, UXTH #1
        add sp, sp, wzr, UXTH #2
        add sp, sp, wzr, UXTH #3
        add sp, sp, wzr, UXTH #4

        add w0,  w1,  w2,  UXTW #1
        add w0,  w1,  w2,  UXTW #2
        add w0,  w1,  w2,  UXTW #3
        add w0,  w1,  w2,  UXTW #4
        add w28, w29, wzr, UXTW #1
        add w28, w29, wzr, UXTW #2
        add w28, w29, wzr, UXTW #3
        add w28, w29, wzr, UXTW #4

        add x0, x1,  w2, UXTW #1
        add x0, x1,  w2, UXTW #2
        add x0, x1,  w2, UXTW #3
        add x0, x1,  w2, UXTW #4
        add sp, sp, wzr, UXTW #1
        add sp, sp, wzr, UXTW #2
        add sp, sp, wzr, UXTW #3
        add sp, sp, wzr, UXTW #4

        add w0,  w1,  w2,  UXTX #1
        add w0,  w1,  w2,  UXTX #2
        add w0,  w1,  w2,  UXTX #3
        add w0,  w1,  w2,  UXTX #4
        add w28, w29, wzr, UXTX #1
        add w28, w29, wzr, UXTX #2
        add w28, w29, wzr, UXTX #3
        add w28, w29, wzr, UXTX #4

        add x0, x1,  x2, UXTX #1
        add x0, x1,  x2, UXTX #2
        add x0, x1,  x2, UXTX #3
        add x0, x1,  x2, UXTX #4
        add sp, sp, xzr, UXTX #1
        add sp, sp, xzr, UXTX #2
        add sp, sp, xzr, UXTX #3
        add sp, sp, xzr, UXTX #4

        add w0,  w1,  w2,  SXTB #1
        add w0,  w1,  w2,  SXTB #2
        add w0,  w1,  w2,  SXTB #3
        add w0,  w1,  w2,  SXTB #4
        add w28, w29, wzr, SXTB #1
        add w28, w29, wzr, SXTB #2
        add w28, w29, wzr, SXTB #3
        add w28, w29, wzr, SXTB #4

        add x0, x1,  w2, SXTB #1
        add x0, x1,  w2, SXTB #2
        add x0, x1,  w2, SXTB #3
        add x0, x1,  w2, SXTB #4
        add sp, sp, wzr, SXTB #1
        add sp, sp, wzr, SXTB #2
        add sp, sp, wzr, SXTB #3
        add sp, sp, wzr, SXTB #4

        add w0,  w1,  w2,  SXTH #1
        add w0,  w1,  w2,  SXTH #2
        add w0,  w1,  w2,  SXTH #3
        add w0,  w1,  w2,  SXTH #4
        add w28, w29, wzr, SXTH #1
        add w28, w29, wzr, SXTH #2
        add w28, w29, wzr, SXTH #3
        add w28, w29, wzr, SXTH #4

        add x0, x1,  w2, SXTH #1
        add x0, x1,  w2, SXTH #2
        add x0, x1,  w2, SXTH #3
        add x0, x1,  w2, SXTH #4
        add sp, sp, wzr, SXTH #1
        add sp, sp, wzr, SXTH #2
        add sp, sp, wzr, SXTH #3
        add sp, sp, wzr, SXTH #4

        add w0,  w1,  w2,  SXTW #1
        add w0,  w1,  w2,  SXTW #2
        add w0,  w1,  w2,  SXTW #3
        add w0,  w1,  w2,  SXTW #4
        add w28, w29, wzr, SXTW #1
        add w28, w29, wzr, SXTW #2
        add w28, w29, wzr, SXTW #3
        add w28, w29, wzr, SXTW #4

        add x0, x1,  w2, SXTW #1
        add x0, x1,  w2, SXTW #2
        add x0, x1,  w2, SXTW #3
        add x0, x1,  w2, SXTW #4
        add sp, sp, wzr, SXTW #1
        add sp, sp, wzr, SXTW #2
        add sp, sp, wzr, SXTW #3
        add sp, sp, wzr, SXTW #4

        add w0,  w1,  w2,  SXTX #1
        add w0,  w1,  w2,  SXTX #2
        add w0,  w1,  w2,  SXTX #3
        add w0,  w1,  w2,  SXTX #4
        add w28, w29, wzr, SXTX #1
        add w28, w29, wzr, SXTX #2
        add w28, w29, wzr, SXTX #3
        add w28, w29, wzr, SXTX #4

        add x0, x1,  x2, SXTX #1
        add x0, x1,  x2, SXTX #2
        add x0, x1,  x2, SXTX #3
        add x0, x1,  x2, SXTX #4
        add sp, sp, xzr, SXTX #1
        add sp, sp, xzr, SXTX #2
        add sp, sp, xzr, SXTX #3
        add sp, sp, xzr, SXTX #4


        adds w0, w1, w2
        adds x0, x1, x2
        adds xzr, sp, x2

        adds w0,  w1,  w2,  UXTB #1
        adds w0,  w1,  w2,  UXTB #2
        adds w0,  w1,  w2,  UXTB #3
        adds w0,  w1,  w2,  UXTB #4
        adds w28, w29, wzr, UXTB #1
        adds w28, w29, wzr, UXTB #2
        adds w28, w29, wzr, UXTB #3
        adds w28, w29, wzr, UXTB #4

        adds x0, x1,  w2, UXTB #1
        adds x0, x1,  w2, UXTB #2
        adds x0, x1,  w2, UXTB #3
        adds x0, x1,  w2, UXTB #4
        adds xzr, sp, wzr, UXTB #1
        adds xzr, sp, wzr, UXTB #2
        adds xzr, sp, wzr, UXTB #3
        adds xzr, sp, wzr, UXTB #4

        adds w0,  w1,  w2,  UXTH #1
        adds w0,  w1,  w2,  UXTH #2
        adds w0,  w1,  w2,  UXTH #3
        adds w0,  w1,  w2,  UXTH #4
        adds w28, w29, wzr, UXTH #1
        adds w28, w29, wzr, UXTH #2
        adds w28, w29, wzr, UXTH #3
        adds w28, w29, wzr, UXTH #4

        adds x0, x1,  w2, UXTH #1
        adds x0, x1,  w2, UXTH #2
        adds x0, x1,  w2, UXTH #3
        adds x0, x1,  w2, UXTH #4
        adds xzr, sp, wzr, UXTH #1
        adds xzr, sp, wzr, UXTH #2
        adds xzr, sp, wzr, UXTH #3
        adds xzr, sp, wzr, UXTH #4

        adds w0,  w1,  w2,  UXTW #1
        adds w0,  w1,  w2,  UXTW #2
        adds w0,  w1,  w2,  UXTW #3
        adds w0,  w1,  w2,  UXTW #4
        adds w28, w29, wzr, UXTW #1
        adds w28, w29, wzr, UXTW #2
        adds w28, w29, wzr, UXTW #3
        adds w28, w29, wzr, UXTW #4

        adds x0, x1,  w2, UXTW #1
        adds x0, x1,  w2, UXTW #2
        adds x0, x1,  w2, UXTW #3
        adds x0, x1,  w2, UXTW #4
        adds xzr, sp, wzr, UXTW #1
        adds xzr, sp, wzr, UXTW #2
        adds xzr, sp, wzr, UXTW #3
        adds xzr, sp, wzr, UXTW #4

        adds w0,  w1,  w2,  UXTX #1
        adds w0,  w1,  w2,  UXTX #2
        adds w0,  w1,  w2,  UXTX #3
        adds w0,  w1,  w2,  UXTX #4
        adds w28, w29, wzr, UXTX #1
        adds w28, w29, wzr, UXTX #2
        adds w28, w29, wzr, UXTX #3
        adds w28, w29, wzr, UXTX #4

        adds x0, x1,  x2, UXTX #1
        adds x0, x1,  x2, UXTX #2
        adds x0, x1,  x2, UXTX #3
        adds x0, x1,  x2, UXTX #4
        adds xzr, sp, xzr, UXTX #1
        adds xzr, sp, xzr, UXTX #2
        adds xzr, sp, xzr, UXTX #3
        adds xzr, sp, xzr, UXTX #4

        adds w0,  w1,  w2,  SXTB #1
        adds w0,  w1,  w2,  SXTB #2
        adds w0,  w1,  w2,  SXTB #3
        adds w0,  w1,  w2,  SXTB #4
        adds w28, w29, wzr, SXTB #1
        adds w28, w29, wzr, SXTB #2
        adds w28, w29, wzr, SXTB #3
        adds w28, w29, wzr, SXTB #4

        adds x0, x1,  w2, SXTB #1
        adds x0, x1,  w2, SXTB #2
        adds x0, x1,  w2, SXTB #3
        adds x0, x1,  w2, SXTB #4
        adds xzr, sp, wzr, SXTB #1
        adds xzr, sp, wzr, SXTB #2
        adds xzr, sp, wzr, SXTB #3
        adds xzr, sp, wzr, SXTB #4

        adds w0,  w1,  w2,  SXTH #1
        adds w0,  w1,  w2,  SXTH #2
        adds w0,  w1,  w2,  SXTH #3
        adds w0,  w1,  w2,  SXTH #4
        adds w28, w29, wzr, SXTH #1
        adds w28, w29, wzr, SXTH #2
        adds w28, w29, wzr, SXTH #3
        adds w28, w29, wzr, SXTH #4

        adds x0, x1,  w2, SXTH #1
        adds x0, x1,  w2, SXTH #2
        adds x0, x1,  w2, SXTH #3
        adds x0, x1,  w2, SXTH #4
        adds xzr, sp, wzr, SXTH #1
        adds xzr, sp, wzr, SXTH #2
        adds xzr, sp, wzr, SXTH #3
        adds xzr, sp, wzr, SXTH #4

        adds w0,  w1,  w2,  SXTW #1
        adds w0,  w1,  w2,  SXTW #2
        adds w0,  w1,  w2,  SXTW #3
        adds w0,  w1,  w2,  SXTW #4
        adds w28, w29, wzr, SXTW #1
        adds w28, w29, wzr, SXTW #2
        adds w28, w29, wzr, SXTW #3
        adds w28, w29, wzr, SXTW #4

        adds x0, x1,  w2, SXTW #1
        adds x0, x1,  w2, SXTW #2
        adds x0, x1,  w2, SXTW #3
        adds x0, x1,  w2, SXTW #4
        adds xzr, sp, wzr, SXTW #1
        adds xzr, sp, wzr, SXTW #2
        adds xzr, sp, wzr, SXTW #3
        adds xzr, sp, wzr, SXTW #4

        adds w0,  w1,  w2,  SXTX #1
        adds w0,  w1,  w2,  SXTX #2
        adds w0,  w1,  w2,  SXTX #3
        adds w0,  w1,  w2,  SXTX #4
        adds w28, w29, wzr, SXTX #1
        adds w28, w29, wzr, SXTX #2
        adds w28, w29, wzr, SXTX #3
        adds w28, w29, wzr, SXTX #4

        adds x0, x1,  x2, SXTX #1
        adds x0, x1,  x2, SXTX #2
        adds x0, x1,  x2, SXTX #3
        adds x0, x1,  x2, SXTX #4
        adds xzr, sp, xzr, SXTX #1
        adds xzr, sp, xzr, SXTX #2
        adds xzr, sp, xzr, SXTX #3
        adds xzr, sp, xzr, SXTX #4


        sub w0, w1, w2

        sub w0,  w1,  w2,  UXTB #1
        sub w0,  w1,  w2,  UXTB #2
        sub w0,  w1,  w2,  UXTB #3
        sub w0,  w1,  w2,  UXTB #4
        sub w28, w29, wzr, UXTB #1
        sub w28, w29, wzr, UXTB #2
        sub w28, w29, wzr, UXTB #3
        sub w28, w29, wzr, UXTB #4

        sub x0, x1,  w2, UXTB #1
        sub x0, x1,  w2, UXTB #2
        sub x0, x1,  w2, UXTB #3
        sub x0, x1,  w2, UXTB #4
        sub sp, sp, wzr, UXTB #1
        sub sp, sp, wzr, UXTB #2
        sub sp, sp, wzr, UXTB #3
        sub sp, sp, wzr, UXTB #4

        sub w0,  w1,  w2,  UXTH #1
        sub w0,  w1,  w2,  UXTH #2
        sub w0,  w1,  w2,  UXTH #3
        sub w0,  w1,  w2,  UXTH #4
        sub w28, w29, wzr, UXTH #1
        sub w28, w29, wzr, UXTH #2
        sub w28, w29, wzr, UXTH #3
        sub w28, w29, wzr, UXTH #4

        sub x0, x1,  w2, UXTH #1
        sub x0, x1,  w2, UXTH #2
        sub x0, x1,  w2, UXTH #3
        sub x0, x1,  w2, UXTH #4
        sub sp, sp, wzr, UXTH #1
        sub sp, sp, wzr, UXTH #2
        sub sp, sp, wzr, UXTH #3
        sub sp, sp, wzr, UXTH #4

        sub w0,  w1,  w2,  UXTW #1
        sub w0,  w1,  w2,  UXTW #2
        sub w0,  w1,  w2,  UXTW #3
        sub w0,  w1,  w2,  UXTW #4
        sub w28, w29, wzr, UXTW #1
        sub w28, w29, wzr, UXTW #2
        sub w28, w29, wzr, UXTW #3
        sub w28, w29, wzr, UXTW #4

        sub x0, x1,  w2, UXTW #1
        sub x0, x1,  w2, UXTW #2
        sub x0, x1,  w2, UXTW #3
        sub x0, x1,  w2, UXTW #4
        sub sp, sp, wzr, UXTW #1
        sub sp, sp, wzr, UXTW #2
        sub sp, sp, wzr, UXTW #3
        sub sp, sp, wzr, UXTW #4

        sub w0,  w1,  w2,  UXTX #1
        sub w0,  w1,  w2,  UXTX #2
        sub w0,  w1,  w2,  UXTX #3
        sub w0,  w1,  w2,  UXTX #4
        sub w28, w29, wzr, UXTX #1
        sub w28, w29, wzr, UXTX #2
        sub w28, w29, wzr, UXTX #3
        sub w28, w29, wzr, UXTX #4

        sub x0, x1,  x2, UXTX #1
        sub x0, x1,  x2, UXTX #2
        sub x0, x1,  x2, UXTX #3
        sub x0, x1,  x2, UXTX #4
        sub sp, sp, xzr, UXTX #1
        sub sp, sp, xzr, UXTX #2
        sub sp, sp, xzr, UXTX #3
        sub sp, sp, xzr, UXTX #4

        sub w0,  w1,  w2,  SXTB #1
        sub w0,  w1,  w2,  SXTB #2
        sub w0,  w1,  w2,  SXTB #3
        sub w0,  w1,  w2,  SXTB #4
        sub w28, w29, wzr, SXTB #1
        sub w28, w29, wzr, SXTB #2
        sub w28, w29, wzr, SXTB #3
        sub w28, w29, wzr, SXTB #4

        sub x0, x1,  w2, SXTB #1
        sub x0, x1,  w2, SXTB #2
        sub x0, x1,  w2, SXTB #3
        sub x0, x1,  w2, SXTB #4
        sub sp, sp, wzr, SXTB #1
        sub sp, sp, wzr, SXTB #2
        sub sp, sp, wzr, SXTB #3
        sub sp, sp, wzr, SXTB #4

        sub w0,  w1,  w2,  SXTH #1
        sub w0,  w1,  w2,  SXTH #2
        sub w0,  w1,  w2,  SXTH #3
        sub w0,  w1,  w2,  SXTH #4
        sub w28, w29, wzr, SXTH #1
        sub w28, w29, wzr, SXTH #2
        sub w28, w29, wzr, SXTH #3
        sub w28, w29, wzr, SXTH #4

        sub x0, x1,  w2, SXTH #1
        sub x0, x1,  w2, SXTH #2
        sub x0, x1,  w2, SXTH #3
        sub x0, x1,  w2, SXTH #4
        sub sp, sp, wzr, SXTH #1
        sub sp, sp, wzr, SXTH #2
        sub sp, sp, wzr, SXTH #3
        sub sp, sp, wzr, SXTH #4

        sub w0,  w1,  w2,  SXTW #1
        sub w0,  w1,  w2,  SXTW #2
        sub w0,  w1,  w2,  SXTW #3
        sub w0,  w1,  w2,  SXTW #4
        sub w28, w29, wzr, SXTW #1
        sub w28, w29, wzr, SXTW #2
        sub w28, w29, wzr, SXTW #3
        sub w28, w29, wzr, SXTW #4

        sub x0, x1,  w2, SXTW #1
        sub x0, x1,  w2, SXTW #2
        sub x0, x1,  w2, SXTW #3
        sub x0, x1,  w2, SXTW #4
        sub sp, sp, wzr, SXTW #1
        sub sp, sp, wzr, SXTW #2
        sub sp, sp, wzr, SXTW #3
        sub sp, sp, wzr, SXTW #4

        sub w0,  w1,  w2,  SXTX #1
        sub w0,  w1,  w2,  SXTX #2
        sub w0,  w1,  w2,  SXTX #3
        sub w0,  w1,  w2,  SXTX #4
        sub w28, w29, wzr, SXTX #1
        sub w28, w29, wzr, SXTX #2
        sub w28, w29, wzr, SXTX #3
        sub w28, w29, wzr, SXTX #4

        sub x0, x1,  x2, SXTX #1
        sub x0, x1,  x2, SXTX #2
        sub x0, x1,  x2, SXTX #3
        sub x0, x1,  x2, SXTX #4
        sub sp, sp, xzr, SXTX #1
        sub sp, sp, xzr, SXTX #2
        sub sp, sp, xzr, SXTX #3
        sub sp, sp, xzr, SXTX #4


        subs w0, w1, w2
        subs x0, sp, xzr

        subs w0,  w1,  w2,  UXTB #1
        subs w0,  w1,  w2,  UXTB #2
        subs w0,  w1,  w2,  UXTB #3
        subs w0,  w1,  w2,  UXTB #4
        subs w28, w29, wzr, UXTB #1
        subs w28, w29, wzr, UXTB #2
        subs w28, w29, wzr, UXTB #3
        subs w28, w29, wzr, UXTB #4

        subs x0, x1,  w2, UXTB #1
        subs x0, x1,  w2, UXTB #2
        subs x0, x1,  w2, UXTB #3
        subs x0, x1,  w2, UXTB #4
        subs x28, sp, wzr, UXTB #1 ; targetting xzr would produce an cmp alias instruction
        subs x28, sp, wzr, UXTB #2
        subs x28, sp, wzr, UXTB #3
        subs x28, sp, wzr, UXTB #4

        subs w0,  w1,  w2,  UXTH #1
        subs w0,  w1,  w2,  UXTH #2
        subs w0,  w1,  w2,  UXTH #3
        subs w0,  w1,  w2,  UXTH #4
        subs w28, w29, wzr, UXTH #1
        subs w28, w29, wzr, UXTH #2
        subs w28, w29, wzr, UXTH #3
        subs w28, w29, wzr, UXTH #4

        subs x0, x1,  w2, UXTH #1
        subs x0, x1,  w2, UXTH #2
        subs x0, x1,  w2, UXTH #3
        subs x0, x1,  w2, UXTH #4
        subs x28, sp, wzr, UXTH #1
        subs x28, sp, wzr, UXTH #2
        subs x28, sp, wzr, UXTH #3
        subs x28, sp, wzr, UXTH #4

        subs w0,  w1,  w2,  UXTW #1
        subs w0,  w1,  w2,  UXTW #2
        subs w0,  w1,  w2,  UXTW #3
        subs w0,  w1,  w2,  UXTW #4
        subs w28, w29, wzr, UXTW #1
        subs w28, w29, wzr, UXTW #2
        subs w28, w29, wzr, UXTW #3
        subs w28, w29, wzr, UXTW #4

        subs x0, x1,  w2, UXTW #1
        subs x0, x1,  w2, UXTW #2
        subs x0, x1,  w2, UXTW #3
        subs x0, x1,  w2, UXTW #4
        subs x28, sp, wzr, UXTW #1
        subs x28, sp, wzr, UXTW #2
        subs x28, sp, wzr, UXTW #3
        subs x28, sp, wzr, UXTW #4

        subs w0,  w1,  w2,  UXTX #1
        subs w0,  w1,  w2,  UXTX #2
        subs w0,  w1,  w2,  UXTX #3
        subs w0,  w1,  w2,  UXTX #4
        subs w28, w29, wzr, UXTX #1
        subs w28, w29, wzr, UXTX #2
        subs w28, w29, wzr, UXTX #3
        subs w28, w29, wzr, UXTX #4

        subs x0, x1,  x2, UXTX #1
        subs x0, x1,  x2, UXTX #2
        subs x0, x1,  x2, UXTX #3
        subs x0, x1,  x2, UXTX #4
        subs x28, sp, xzr, UXTX #1
        subs x28, sp, xzr, UXTX #2
        subs x28, sp, xzr, UXTX #3
        subs x28, sp, xzr, UXTX #4

        subs w0,  w1,  w2,  SXTB #1
        subs w0,  w1,  w2,  SXTB #2
        subs w0,  w1,  w2,  SXTB #3
        subs w0,  w1,  w2,  SXTB #4
        subs w28, w29, wzr, SXTB #1
        subs w28, w29, wzr, SXTB #2
        subs w28, w29, wzr, SXTB #3
        subs w28, w29, wzr, SXTB #4

        subs x0, x1,  w2, SXTB #1
        subs x0, x1,  w2, SXTB #2
        subs x0, x1,  w2, SXTB #3
        subs x0, x1,  w2, SXTB #4
        subs x28, sp, wzr, SXTB #1
        subs x28, sp, wzr, SXTB #2
        subs x28, sp, wzr, SXTB #3
        subs x28, sp, wzr, SXTB #4

        subs w0,  w1,  w2,  SXTH #1
        subs w0,  w1,  w2,  SXTH #2
        subs w0,  w1,  w2,  SXTH #3
        subs w0,  w1,  w2,  SXTH #4
        subs w28, w29, wzr, SXTH #1
        subs w28, w29, wzr, SXTH #2
        subs w28, w29, wzr, SXTH #3
        subs w28, w29, wzr, SXTH #4

        subs x0, x1,  w2, SXTH #1
        subs x0, x1,  w2, SXTH #2
        subs x0, x1,  w2, SXTH #3
        subs x0, x1,  w2, SXTH #4
        subs x28, sp, wzr, SXTH #1
        subs x28, sp, wzr, SXTH #2
        subs x28, sp, wzr, SXTH #3
        subs x28, sp, wzr, SXTH #4

        subs w0,  w1,  w2,  SXTW #1
        subs w0,  w1,  w2,  SXTW #2
        subs w0,  w1,  w2,  SXTW #3
        subs w0,  w1,  w2,  SXTW #4
        subs w28, w29, wzr, SXTW #1
        subs w28, w29, wzr, SXTW #2
        subs w28, w29, wzr, SXTW #3
        subs w28, w29, wzr, SXTW #4

        subs x0, x1,  w2, SXTW #1
        subs x0, x1,  w2, SXTW #2
        subs x0, x1,  w2, SXTW #3
        subs x0, x1,  w2, SXTW #4
        subs x28, sp, wzr, SXTW #1
        subs x28, sp, wzr, SXTW #2
        subs x28, sp, wzr, SXTW #3
        subs x28, sp, wzr, SXTW #4

        subs w0,  w1,  w2,  SXTX #1
        subs w0,  w1,  w2,  SXTX #2
        subs w0,  w1,  w2,  SXTX #3
        subs w0,  w1,  w2,  SXTX #4
        subs w28, w29, wzr, SXTX #1
        subs w28, w29, wzr, SXTX #2
        subs w28, w29, wzr, SXTX #3
        subs w28, w29, wzr, SXTX #4

        subs x0, x1,  x2, SXTX #1
        subs x0, x1,  x2, SXTX #2
        subs x0, x1,  x2, SXTX #3
        subs x0, x1,  x2, SXTX #4
        subs x28, sp, xzr, SXTX #1
        subs x28, sp, xzr, SXTX #2
        subs x28, sp, xzr, SXTX #3
        subs x28, sp, xzr, SXTX #4


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


        ; FEAT_PAuth memory loads

        ldraa x0, [x1]
        ldraa x0, [x1, #-1024]
        ldraa x0, [x1, #-4096]
        ldraa x0, [x1, #4088]

        ldraa xzr, [sp]
        ldraa xzr, [sp, #-1024]
        ldraa xzr, [sp, #-4096]
        ldraa xzr, [sp, #4088]

        ldraa x0, [x1, #0]!
        ldraa x0, [x1, #-1024]!
        ldraa x0, [x1, #-4096]!
        ldraa x0, [x1, #4088]!

        ldraa xzr, [sp, #0]!
        ldraa xzr, [sp, #-1024]!
        ldraa xzr, [sp, #-4096]!
        ldraa xzr, [sp, #4088]!


        ldrab x0, [x1]
        ldrab x0, [x1, #-1024]
        ldrab x0, [x1, #-4096]
        ldrab x0, [x1, #4088]

        ldrab xzr, [sp]
        ldrab xzr, [sp, #-1024]
        ldrab xzr, [sp, #-4096]
        ldrab xzr, [sp, #4088]

        ldrab x0, [x1, #0]!
        ldrab x0, [x1, #-1024]!
        ldrab x0, [x1, #-4096]!
        ldrab x0, [x1, #4088]!

        ldrab xzr, [sp, #0]!
        ldrab xzr, [sp, #-1024]!
        ldrab xzr, [sp, #-4096]!
        ldrab xzr, [sp, #4088]!


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


        ; Atomic memory operations

        ldaddb w1,  w2,  [x0]
        ldaddb wzr, wzr, [sp]

        ldclrb w1,  w2,  [x0]
        ldclrb wzr, wzr, [sp]

        ldeorb w1,  w2,  [x0]
        ldeorb wzr, wzr, [sp]

        ldsetb w1,  w2,  [x0]
        ldsetb wzr, wzr, [sp]

        ldsmaxb w1,  w2,  [x0]
        ldsmaxb wzr, wzr, [sp]

        ldsminb w1,  w2,  [x0]
        ldsminb wzr, wzr, [sp]

        ldumaxb w1,  w2,  [x0]
        ldumaxb wzr, wzr, [sp]

        lduminb w1,  w2,  [x0]
        lduminb wzr, wzr, [sp]

        swpb w1,  w2,  [x0]
        swpb wzr, wzr, [sp]

        ;rcwclr x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwclr xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwswp x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwswp xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwset x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwset xzr, xzr, [sp] - Requires FEAT_THE

        ldaddlb w1,  w2,  [x0]
        ldaddlb wzr, wzr, [sp]

        ldclrlb w1,  w2,  [x0]
        ldclrlb wzr, wzr, [sp]

        ldeorlb w1,  w2,  [x0]
        ldeorlb wzr, wzr, [sp]

        ldsetlb w1,  w2,  [x0]
        ldsetlb wzr, wzr, [sp]

        ldsmaxlb w1,  w2,  [x0]
        ldsmaxlb wzr, wzr, [sp]

        ldsminlb w1,  w2,  [x0]
        ldsminlb wzr, wzr, [sp]

        ldumaxlb w1,  w2,  [x0]
        ldumaxlb wzr, wzr, [sp]

        lduminlb w1,  w2,  [x0]
        lduminlb wzr, wzr, [sp]

        swplb w1,  w2,  [x0]
        swplb wzr, wzr, [sp]

        ;rcwclrl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwclrl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwswpl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwswpl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsetl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsetl xzr, xzr, [sp] - Requires FEAT_THE

        ldaddab w1,  w2,  [x0]
        ldaddab wzr, wzr, [sp]

        ldclrab w1,  w2,  [x0]
        ldclrab wzr, wzr, [sp]

        ldeorab w1,  w2,  [x0]
        ldeorab wzr, wzr, [sp]

        ldsetab w1,  w2,  [x0]
        ldsetab wzr, wzr, [sp]

        ldsmaxab w1,  w2,  [x0]
        ldsmaxab wzr, wzr, [sp]

        ldsminab w1,  w2,  [x0]
        ldsminab wzr, wzr, [sp]

        ldumaxab w1,  w2,  [x0]
        ldumaxab wzr, wzr, [sp]

        lduminab w1,  w2,  [x0]
        lduminab wzr, wzr, [sp]

        swpab w1,  w2,  [x0]
        swpab wzr, wzr, [sp]

        ;rcwclra x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwclra xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwswpa x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwswpa xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwseta x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwseta xzr, xzr, [sp] - Requires FEAT_THE

        ldaprb w0, [x1]
        ldaprb wzr, [sp]

        ldaddalb w1,  w2,  [x0]
        ldaddalb wzr, wzr, [sp]

        ldclralb w1,  w2,  [x0]
        ldclralb wzr, wzr, [sp]

        ldeoralb w1,  w2,  [x0]
        ldeoralb wzr, wzr, [sp]

        ldsetalb w1,  w2,  [x0]
        ldsetalb wzr, wzr, [sp]

        ldsmaxalb w1,  w2,  [x0]
        ldsmaxalb wzr, wzr, [sp]

        ldsminalb w1,  w2,  [x0]
        ldsminalb wzr, wzr, [sp]

        ldumaxalb w1,  w2,  [x0]
        ldumaxalb wzr, wzr, [sp]

        lduminalb w1,  w2,  [x0]
        lduminalb wzr, wzr, [sp]

        swpalb w1,  w2,  [x0]
        swpalb wzr, wzr, [sp]

        ;rcwclral x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwclral xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwswpal x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwswpal xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsetal x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsetal xzr, xzr, [sp] - Requires FEAT_THE

        ldaddh w1,  w2,  [x0]
        ldaddh wzr, wzr, [sp]

        ldclrh w1,  w2,  [x0]
        ldclrh wzr, wzr, [sp]

        ldeorh w1,  w2,  [x0]
        ldeorh wzr, wzr, [sp]

        ldseth w1,  w2,  [x0]
        ldseth wzr, wzr, [sp]

        ldsmaxh w1,  w2,  [x0]
        ldsmaxh wzr, wzr, [sp]

        ldsminh w1,  w2,  [x0]
        ldsminh wzr, wzr, [sp]

        ldumaxh w1,  w2,  [x0]
        ldumaxh wzr, wzr, [sp]

        lduminh w1,  w2,  [x0]
        lduminh wzr, wzr, [sp]

        swph w1,  w2,  [x0]
        swph wzr, wzr, [sp]

        ;rcwsclr x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsclr xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsswp x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsswp xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsset x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsset xzr, xzr, [sp] - Requires FEAT_THE

        ldaddlh w1,  w2,  [x0]
        ldaddlh wzr, wzr, [sp]

        ldclrlh w1,  w2,  [x0]
        ldclrlh wzr, wzr, [sp]

        ldeorlh w1,  w2,  [x0]
        ldeorlh wzr, wzr, [sp]

        ldsetlh w1,  w2,  [x0]
        ldsetlh wzr, wzr, [sp]

        ldsmaxlh w1,  w2,  [x0]
        ldsmaxlh wzr, wzr, [sp]

        ldsminlh w1,  w2,  [x0]
        ldsminlh wzr, wzr, [sp]

        ldumaxlh w1,  w2,  [x0]
        ldumaxlh wzr, wzr, [sp]

        lduminlh w1,  w2,  [x0]
        lduminlh wzr, wzr, [sp]

        swplh w1,  w2,  [x0]
        swplh wzr, wzr, [sp]

        ;rcwsclrl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsclrl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsswpl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsswpl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwssetl x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwssetl xzr, xzr, [sp] - Requires FEAT_THE

        ldaddah w1,  w2,  [x0]
        ldaddah wzr, wzr, [sp]

        ldclrah w1,  w2,  [x0]
        ldclrah wzr, wzr, [sp]

        ldeorah w1,  w2,  [x0]
        ldeorah wzr, wzr, [sp]

        ldsetah w1,  w2,  [x0]
        ldsetah wzr, wzr, [sp]

        ldsmaxah w1,  w2,  [x0]
        ldsmaxah wzr, wzr, [sp]

        ldsminah w1,  w2,  [x0]
        ldsminah wzr, wzr, [sp]

        ldumaxah w1,  w2,  [x0]
        ldumaxah wzr, wzr, [sp]

        lduminah w1,  w2,  [x0]
        lduminah wzr, wzr, [sp]

        swpah w1,  w2,  [x0]
        swpah wzr, wzr, [sp]

        ;rcwsclra x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsclra xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsswpa x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsswpa xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsseta x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsseta xzr, xzr, [sp] - Requires FEAT_THE

        ldaprh w0, [x1]
        ldaprh wzr, [sp]

        ldaddalh w1,  w2,  [x0]
        ldaddalh wzr, wzr, [sp]

        ldclralh w1,  w2,  [x0]
        ldclralh wzr, wzr, [sp]

        ldeoralh w1,  w2,  [x0]
        ldeoralh wzr, wzr, [sp]

        ldsetalh w1,  w2,  [x0]
        ldsetalh wzr, wzr, [sp]

        ldsmaxalh w1,  w2,  [x0]
        ldsmaxalh wzr, wzr, [sp]

        ldsminalh w1,  w2,  [x0]
        ldsminalh wzr, wzr, [sp]

        ldumaxalh w1,  w2,  [x0]
        ldumaxalh wzr, wzr, [sp]

        lduminalh w1,  w2,  [x0]
        lduminalh wzr, wzr, [sp]

        swpalh w1,  w2,  [x0]
        swpalh wzr, wzr, [sp]

        ;rcwsclral x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsclral xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwsswpal x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwsswpal xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwssetal x1,  x2,  [x0] - Requires FEAT_THE
        ;rcwssetal xzr, xzr, [sp] - Requires FEAT_THE

        ldadd w1,  w2,  [x0]
        ldadd wzr, wzr, [sp]
        ldadd x1,  x2,  [x0]
        ldadd xzr, xzr, [sp]

        ldclr w1,  w2,  [x0]
        ldclr wzr, wzr, [sp]
        ldclr x1,  x2,  [x0]
        ldclr xzr, xzr, [sp]

        ldeor w1,  w2,  [x0]
        ldeor wzr, wzr, [sp]
        ldeor x1,  x2,  [x0]
        ldeor xzr, xzr, [sp]

        ldset w1,  w2,  [x0]
        ldset wzr, wzr, [sp]
        ldset x1,  x2,  [x0]
        ldset xzr, xzr, [sp]

        ldsmax w1,  w2,  [x0]
        ldsmax wzr, wzr, [sp]
        ldsmax x1,  x2,  [x0]
        ldsmax xzr, xzr, [sp]

        ldsmin w1,  w2,  [x0]
        ldsmin wzr, wzr, [sp]
        ldsmin x1,  x2,  [x0]
        ldsmin xzr, xzr, [sp]

        ldumax w1,  w2,  [x0]
        ldumax wzr, wzr, [sp]
        ldumax x1,  x2,  [x0]
        ldumax xzr, xzr, [sp]

        ldumin w1,  w2,  [x0]
        ldumin wzr, wzr, [sp]
        ldumin x1,  x2,  [x0]
        ldumin xzr, xzr, [sp]

        swp w1,  w2,  [x0]
        swp wzr, wzr, [sp]
        swp x1,  x2,  [x0]
        swp xzr, xzr, [sp]

        ldaddl w1,  w2,  [x0]
        ldaddl wzr, wzr, [sp]
        ldaddl x1,  x2,  [x0]
        ldaddl xzr, xzr, [sp]

        ldclrl w1,  w2,  [x0]
        ldclrl wzr, wzr, [sp]
        ldclrl x1,  x2,  [x0]
        ldclrl xzr, xzr, [sp]

        ldeorl w1,  w2,  [x0]
        ldeorl wzr, wzr, [sp]
        ldeorl x1,  x2,  [x0]
        ldeorl xzr, xzr, [sp]

        ldsetl w1,  w2,  [x0]
        ldsetl wzr, wzr, [sp]
        ldsetl x1,  x2,  [x0]
        ldsetl xzr, xzr, [sp]

        ldsmaxl w1,  w2,  [x0]
        ldsmaxl wzr, wzr, [sp]
        ldsmaxl x1,  x2,  [x0]
        ldsmaxl xzr, xzr, [sp]

        ldsminl w1,  w2,  [x0]
        ldsminl wzr, wzr, [sp]
        ldsminl x1,  x2,  [x0]
        ldsminl xzr, xzr, [sp]

        ldumaxl w1,  w2,  [x0]
        ldumaxl wzr, wzr, [sp]
        ldumaxl x1,  x2,  [x0]
        ldumaxl xzr, xzr, [sp]

        lduminl w1,  w2,  [x0]
        lduminl wzr, wzr, [sp]
        lduminl x1,  x2,  [x0]
        lduminl xzr, xzr, [sp]

        swpl w1,  w2,  [x0]
        swpl wzr, wzr, [sp]
        swpl x1,  x2,  [x0]
        swpl xzr, xzr, [sp]

        ldadda w1,  w2,  [x0]
        ldadda wzr, wzr, [sp]
        ldadda x1,  x2,  [x0]
        ldadda xzr, xzr, [sp]

        ldclra w1,  w2,  [x0]
        ldclra wzr, wzr, [sp]
        ldclra x1,  x2,  [x0]
        ldclra xzr, xzr, [sp]

        ldeora w1,  w2,  [x0]
        ldeora wzr, wzr, [sp]
        ldeora x1,  x2,  [x0]
        ldeora xzr, xzr, [sp]

        ldseta w1,  w2,  [x0]
        ldseta wzr, wzr, [sp]
        ldseta x1,  x2,  [x0]
        ldseta xzr, xzr, [sp]

        ldsmaxa w1,  w2,  [x0]
        ldsmaxa wzr, wzr, [sp]
        ldsmaxa x1,  x2,  [x0]
        ldsmaxa xzr, xzr, [sp]

        ldsmina w1,  w2,  [x0]
        ldsmina wzr, wzr, [sp]
        ldsmina x1,  x2,  [x0]
        ldsmina xzr, xzr, [sp]

        ldumaxa w1,  w2,  [x0]
        ldumaxa wzr, wzr, [sp]
        ldumaxa x1,  x2,  [x0]
        ldumaxa xzr, xzr, [sp]

        ldumina w1,  w2,  [x0]
        ldumina wzr, wzr, [sp]
        ldumina x1,  x2,  [x0]
        ldumina xzr, xzr, [sp]

        swpa w1,  w2,  [x0]
        swpa wzr, wzr, [sp]
        swpa x1,  x2,  [x0]
        swpa xzr, xzr, [sp]

        ldapr w0, [x1]
        ldapr wzr, [sp]
        ldapr x0, [x1]
        ldapr xzr, [sp]

        ldaddal w1,  w2,  [x0]
        ldaddal wzr, wzr, [sp]
        ldaddal x1,  x2,  [x0]
        ldaddal xzr, xzr, [sp]

        ldclral w1,  w2,  [x0]
        ldclral wzr, wzr, [sp]
        ldclral x1,  x2,  [x0]
        ldclral xzr, xzr, [sp]

        ldeoral w1,  w2,  [x0]
        ldeoral wzr, wzr, [sp]
        ldeoral x1,  x2,  [x0]
        ldeoral xzr, xzr, [sp]

        ldsetal w1,  w2,  [x0]
        ldsetal wzr, wzr, [sp]
        ldsetal x1,  x2,  [x0]
        ldsetal xzr, xzr, [sp]

        ldsmaxal w1,  w2,  [x0]
        ldsmaxal wzr, wzr, [sp]
        ldsmaxal x1,  x2,  [x0]
        ldsmaxal xzr, xzr, [sp]

        ldsminal w1,  w2,  [x0]
        ldsminal wzr, wzr, [sp]
        ldsminal x1,  x2,  [x0]
        ldsminal xzr, xzr, [sp]

        ldumaxal w1,  w2,  [x0]
        ldumaxal wzr, wzr, [sp]
        ldumaxal x1,  x2,  [x0]
        ldumaxal xzr, xzr, [sp]

        lduminal w1,  w2,  [x0]
        lduminal wzr, wzr, [sp]
        lduminal x1,  x2,  [x0]
        lduminal xzr, xzr, [sp]

        swpal w1,  w2,  [x0]
        swpal wzr, wzr, [sp]
        swpal x1,  x2,  [x0]
        swpal xzr, xzr, [sp]

        ;
        ; LDR literal variants
        ;

        ldr w0, #0
        ldr wzr, #0xffffc
        ldr wzr, #-0xffffc

        ldr x0, #0
        ldr xzr, #0xffffc
        ldr xzr, #-0xffffc

        ldrsw x0, #0
        ldrsw xzr, #0xffffc
        ldrsw xzr, #-0xffffc

        ldr s0, #0
        ldr s31, #0xffffc
        ldr s31, #-0xffffc

        ldr d0, #0
        ldr d31, #0xffffc
        ldr d31, #-0xffffc

        ldr q0, #0
        ldr q31, #0xffffc
        ldr q31, #-0xffffc

        ;
        ; Load/Store memory tags
        ;

        ;.cpu generic+mte

        ;stg x0, [x0]
        ;stg x0, [x0, #-4096]
        ;stg x0, [x0, #4080]
        ;stg sp, [sp]
        ;stg sp, [sp, #-4096]
        ;stg sp, [sp, #4080]

        ;stg x0, [x0], #0
        ;stg x0, [x0], #-4096
        ;stg x0, [x0], #4080
        ;stg sp, [sp], #0
        ;stg sp, [sp], #-4096
        ;stg sp, [sp], #4080

        ;stg x0, [x0, #0]!
        ;stg x0, [x0, #-4096]!
        ;stg x0, [x0, #4080]!
        ;stg sp, [sp, #0]!
        ;stg sp, [sp, #-4096]!
        ;stg sp, [sp, #4080]!

        ;stzgm x1, [x0]
        ;stzgm xzr, [sp]

        ;ldg x0, [x0]
        ;ldg x0, [x0, #-4096]
        ;ldg x0, [x0, #4080]
        ;ldg xzr, [sp]
        ;ldg xzr, [sp, #-4096]
        ;ldg xzr, [sp, #4080]

        ;stzg x0, [x0]
        ;stzg x0, [x0, #-4096]
        ;stzg x0, [x0, #4080]
        ;stzg sp, [sp]
        ;stzg sp, [sp, #-4096]
        ;stzg sp, [sp, #4080]

        ;stzg x0, [x0], #0
        ;stzg x0, [x0], #-4096
        ;stzg x0, [x0], #4080
        ;stzg sp, [sp], #0
        ;stzg sp, [sp], #-4096
        ;stzg sp, [sp], #4080

        ;stzg x0, [x0, #0]!
        ;stzg x0, [x0, #-4096]!
        ;stzg x0, [x0, #4080]!
        ;stzg sp, [sp, #0]!
        ;stzg sp, [sp, #-4096]!
        ;stzg sp, [sp, #4080]!

        ;stgm x1, [x0]
        ;stgm xzr, [sp]

        ;st2g x0, [x0]
        ;st2g x0, [x0, #-4096]
        ;st2g x0, [x0, #4080]
        ;st2g sp, [sp]
        ;st2g sp, [sp, #-4096]
        ;st2g sp, [sp, #4080]

        ;st2g x0, [x0], #0
        ;st2g x0, [x0], #-4096
        ;st2g x0, [x0], #4080
        ;st2g sp, [sp], #0
        ;st2g sp, [sp], #-4096
        ;st2g sp, [sp], #4080

        ;st2g x0, [x0, #0]!
        ;st2g x0, [x0, #-4096]!
        ;st2g x0, [x0, #4080]!
        ;st2g sp, [sp, #0]!
        ;st2g sp, [sp, #-4096]!
        ;st2g sp, [sp, #4080]!

        ;ldgm x1, [x0]
        ;ldgm xzr, [sp]

        ;stz2g x0, [x0]
        ;stz2g x0, [x0, #-4096]
        ;stz2g x0, [x0, #4080]
        ;stz2g sp, [sp]
        ;stz2g sp, [sp, #-4096]
        ;stz2g sp, [sp, #4080]

        ;stz2g x0, [x0], #0
        ;stz2g x0, [x0], #-4096
        ;stz2g x0, [x0], #4080
        ;stz2g sp, [sp], #0
        ;stz2g sp, [sp], #-4096
        ;stz2g sp, [sp], #4080

        ;stz2g x0, [x0, #0]!
        ;stz2g x0, [x0, #-4096]!
        ;stz2g x0, [x0, #4080]!
        ;stz2g sp, [sp, #0]!
        ;stz2g sp, [sp, #-4096]!
        ;stz2g sp, [sp, #4080]!

        ;
        ; Compare and swap
        ;

        casb  w0,  w1, [x0]
        casb wzr, wzr, [sp]

        caslb  w0,  w1, [x0]
        caslb wzr, wzr, [sp]

        casab  w0,  w1, [x0]
        casab wzr, wzr, [sp]

        casalb  w0,  w1, [x0]
        casalb wzr, wzr, [sp]

        cash  w0,  w1, [x0]
        cash wzr, wzr, [sp]

        caslh  w0,  w1, [x0]
        caslh wzr, wzr, [sp]

        casah  w0,  w1, [x0]
        casah wzr, wzr, [sp]

        casalh  w0,  w1, [x0]
        casalh wzr, wzr, [sp]

        cas  w0,  w1, [x0]
        cas wzr, wzr, [sp]

        casl  w0,  w1, [x0]
        casl wzr, wzr, [sp]

        casa  w0,  w1, [x0]
        casa wzr, wzr, [sp]

        casal  w0,  w1, [x0]
        casal wzr, wzr, [sp]

        cas  x0,  x1, [x0]
        cas xzr, xzr, [sp]

        casl  x0,  x1, [x0]
        casl xzr, xzr, [sp]

        casa  x0,  x1, [x0]
        casa xzr, xzr, [sp]

        casal  x0,  x1, [x0]
        casal xzr, xzr, [sp]

        ;
        ; Load/store exclusive register
        ;

        stxrb  w0,   w1, [x2]
        stxrb  w0,  wzr, [sp]
        stxrb  wzr,  w0, [sp]

        stlxrb  w0,   w1, [x2]
        stlxrb  w0,  wzr, [sp]
        stlxrb  wzr,  w0, [sp]

        ldxrb  w0,  [x2]
        ldxrb  wzr, [sp]

        ldaxrb  w0,  [x2]
        ldaxrb  wzr, [sp]

        stxrh  w0,   w1, [x2]
        stxrh  w0,  wzr, [sp]
        stxrh  wzr,  w0, [sp]

        stlxrh  w0,   w1, [x2]
        stlxrh  w0,  wzr, [sp]
        stlxrh  wzr,  w0, [sp]

        ldxrh  w0,  [x2]
        ldxrh  wzr, [sp]

        ldaxrh  w0,  [x2]
        ldaxrh  wzr, [sp]

        stxr  w0,   w1, [x2]
        stxr  w0,  wzr, [sp]
        stxr  wzr,  w0, [sp]

        stlxr  w0,   w1, [x2]
        stlxr  w0,  wzr, [sp]
        stlxr  wzr,  w0, [sp]

        ldxr  w0,  [x2]
        ldxr  wzr, [sp]

        ldaxr  w0,  [x2]
        ldaxr  wzr, [sp]

        stxr  w0,   x1, [x2]
        stxr  w0,  xzr, [sp]
        stxr  wzr,  x0, [sp]

        stlxr  w0,   x1, [x2]
        stlxr  w0,  xzr, [sp]
        stlxr  wzr,  x0, [sp]

        ldxr  x0,  [x2]
        ldxr  xzr, [sp]

        ldaxr  x0,  [x2]
        ldaxr  xzr, [sp]

        ;
        ; Load/store exclusive pair
        ;

        stxp w0,  w1,  w2, [x3]
        stxp w0, wzr, wzr, [sp]
        stxp w0,  x1,  x2, [x3]
        stxp w0, xzr, xzr, [sp]

        stlxp w0,  w1,  w2, [x3]
        stlxp w0, wzr, wzr, [sp]
        stlxp w0,  x1,  x2, [x3]
        stlxp w0, xzr, xzr, [sp]

        ldxp  w1,  w2, [x0]
        ldxp wzr, wzr, [sp]
        ldxp  x1,  x2, [x0]
        ldxp xzr, xzr, [sp]

        ldaxp  w1,  w2, [x0]
        ldaxp wzr, wzr, [sp]
        ldaxp  x1,  x2, [x0]
        ldaxp xzr, xzr, [sp]

        ;
        ; Load/store ordered
        ;

        stllrb w0,  [x1]
        stllrb wzr, [sp]

        stlrb w0,  [x1]
        stlrb wzr, [sp]

        ldlarb w0,  [x1]
        ldlarb wzr, [sp]

        ldarb w0,  [x1]
        ldarb wzr, [sp]

        stllrh w0,  [x1]
        stllrh wzr, [sp]

        stlrh w0,  [x1]
        stlrh wzr, [sp]

        ldlarh w0,  [x1]
        ldlarh wzr, [sp]

        ldarh w0,  [x1]
        ldarh wzr, [sp]

        stllr w0,  [x1]
        stllr wzr, [sp]

        stlr w0,  [x1]
        stlr wzr, [sp]

        ldlar w0,  [x1]
        ldlar wzr, [sp]

        ldar w0,  [x1]
        ldar wzr, [sp]

        stllr x0,  [x1]
        stllr xzr, [sp]

        stlr x0,  [x1]
        stlr xzr, [sp]

        ldlar x0,  [x1]
        ldlar xzr, [sp]

        ldar x0,  [x1]
        ldar xzr, [sp]

        ;
        ; Advanced SIMD load/store multiple structures
        ;
        st4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0]
        st4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp]
        st4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0]
        st4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp]

        st4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0]
        st4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp]
        st4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0]
        st4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp]

        st4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0]
        st4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp]
        st4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0]
        st4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp]

        st4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0]
        st4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp]

        st1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0]
        st1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp]
        st1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0]
        st1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp]

        st1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0]
        st1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp]
        st1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0]
        st1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp]

        st1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0]
        st1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp]
        st1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0]
        st1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp]

        st1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0]
        st1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp]

        st3 { v0.8B,  v1.8B, v2.8B }, [x0]
        st3 { v31.8B, v0.8B, v1.8B }, [sp]
        st3 { v0.16B,  v1.16B, v2.16B }, [x0]
        st3 { v31.16B, v0.16B, v1.16B }, [sp]

        st3 { v0.4H,  v1.4H, v2.4H }, [x0]
        st3 { v31.4H, v0.4H, v1.4H }, [sp]
        st3 { v0.8H,  v1.8H, v2.8H }, [x0]
        st3 { v31.8H, v0.8H, v1.8H }, [sp]

        st3 { v0.2S,  v1.2S, v2.2S }, [x0]
        st3 { v31.2S, v0.2S, v1.2S }, [sp]
        st3 { v0.4S,  v1.4S, v2.4S }, [x0]
        st3 { v31.4S, v0.4S, v1.4S }, [sp]

        st3 { v0.2D,  v1.2D, v2.2D }, [x0]
        st3 { v31.2D, v0.2D, v1.2D }, [sp]

        st1 { v0.8B,  v1.8B, v2.8B }, [x0]
        st1 { v31.8B, v0.8B, v1.8B }, [sp]
        st1 { v0.16B,  v1.16B, v2.16B }, [x0]
        st1 { v31.16B, v0.16B, v1.16B }, [sp]

        st1 { v0.4H,  v1.4H, v2.4H }, [x0]
        st1 { v31.4H, v0.4H, v1.4H }, [sp]
        st1 { v0.8H,  v1.8H, v2.8H }, [x0]
        st1 { v31.8H, v0.8H, v1.8H }, [sp]

        st1 { v0.2S,  v1.2S, v2.2S }, [x0]
        st1 { v31.2S, v0.2S, v1.2S }, [sp]
        st1 { v0.4S,  v1.4S, v2.4S }, [x0]
        st1 { v31.4S, v0.4S, v1.4S }, [sp]

        st1 { v0.2D,  v1.2D, v2.2D }, [x0]
        st1 { v31.2D, v0.2D, v1.2D }, [sp]

        st1 { v0.8B }, [x0]
        st1 { v31.8B }, [sp]
        st1 { v0.16B }, [x0]
        st1 { v31.16B }, [sp]

        st1 { v0.4H }, [x0]
        st1 { v31.4H }, [sp]
        st1 { v0.8H }, [x0]
        st1 { v31.8H }, [sp]

        st1 { v0.2S }, [x0]
        st1 { v31.2S }, [sp]
        st1 { v0.4S }, [x0]
        st1 { v31.4S }, [sp]

        st1 { v0.2D }, [x0]
        st1 { v31.2D }, [sp]

        st2 { v0.8B,  v1.8B }, [x0]
        st2 { v31.8B, v0.8B }, [sp]
        st2 { v0.16B,  v1.16B }, [x0]
        st2 { v31.16B, v0.16B }, [sp]

        st2 { v0.4H,  v1.4H }, [x0]
        st2 { v31.4H, v0.4H }, [sp]
        st2 { v0.8H,  v1.8H }, [x0]
        st2 { v31.8H, v0.8H }, [sp]

        st2 { v0.2S,  v1.2S }, [x0]
        st2 { v31.2S, v0.2S }, [sp]
        st2 { v0.4S,  v1.4S }, [x0]
        st2 { v31.4S, v0.4S }, [sp]

        st2 { v0.2D,  v1.2D }, [x0]
        st2 { v31.2D, v0.2D }, [sp]

        st1 { v0.8B,  v1.8B }, [x0]
        st1 { v31.8B, v0.8B }, [sp]
        st1 { v0.16B,  v1.16B }, [x0]
        st1 { v31.16B, v0.16B }, [sp]

        st1 { v0.4H,  v1.4H }, [x0]
        st1 { v31.4H, v0.4H }, [sp]
        st1 { v0.8H,  v1.8H }, [x0]
        st1 { v31.8H, v0.8H }, [sp]

        st1 { v0.2S,  v1.2S }, [x0]
        st1 { v31.2S, v0.2S }, [sp]
        st1 { v0.4S,  v1.4S }, [x0]
        st1 { v31.4S, v0.4S }, [sp]

        st1 { v0.2D,  v1.2D }, [x0]
        st1 { v31.2D, v0.2D }, [sp]


        ld4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0]
        ld4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp]
        ld4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0]
        ld4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp]

        ld4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0]
        ld4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp]
        ld4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0]
        ld4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp]

        ld4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0]
        ld4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp]
        ld4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0]
        ld4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp]

        ld4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0]
        ld4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp]

        ld1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0]
        ld1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp]
        ld1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0]
        ld1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp]

        ld1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0]
        ld1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp]
        ld1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0]
        ld1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp]

        ld1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0]
        ld1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp]
        ld1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0]
        ld1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp]

        ld1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0]
        ld1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp]

        ld3 { v0.8B,  v1.8B, v2.8B }, [x0]
        ld3 { v31.8B, v0.8B, v1.8B }, [sp]
        ld3 { v0.16B,  v1.16B, v2.16B }, [x0]
        ld3 { v31.16B, v0.16B, v1.16B }, [sp]

        ld3 { v0.4H,  v1.4H, v2.4H }, [x0]
        ld3 { v31.4H, v0.4H, v1.4H }, [sp]
        ld3 { v0.8H,  v1.8H, v2.8H }, [x0]
        ld3 { v31.8H, v0.8H, v1.8H }, [sp]

        ld3 { v0.2S,  v1.2S, v2.2S }, [x0]
        ld3 { v31.2S, v0.2S, v1.2S }, [sp]
        ld3 { v0.4S,  v1.4S, v2.4S }, [x0]
        ld3 { v31.4S, v0.4S, v1.4S }, [sp]

        ld3 { v0.2D,  v1.2D, v2.2D }, [x0]
        ld3 { v31.2D, v0.2D, v1.2D }, [sp]

        ld1 { v0.8B,  v1.8B, v2.8B }, [x0]
        ld1 { v31.8B, v0.8B, v1.8B }, [sp]
        ld1 { v0.16B,  v1.16B, v2.16B }, [x0]
        ld1 { v31.16B, v0.16B, v1.16B }, [sp]

        ld1 { v0.4H,  v1.4H, v2.4H }, [x0]
        ld1 { v31.4H, v0.4H, v1.4H }, [sp]
        ld1 { v0.8H,  v1.8H, v2.8H }, [x0]
        ld1 { v31.8H, v0.8H, v1.8H }, [sp]

        ld1 { v0.2S,  v1.2S, v2.2S }, [x0]
        ld1 { v31.2S, v0.2S, v1.2S }, [sp]
        ld1 { v0.4S,  v1.4S, v2.4S }, [x0]
        ld1 { v31.4S, v0.4S, v1.4S }, [sp]

        ld1 { v0.2D,  v1.2D, v2.2D }, [x0]
        ld1 { v31.2D, v0.2D, v1.2D }, [sp]

        ld1 { v0.8B }, [x0]
        ld1 { v31.8B }, [sp]
        ld1 { v0.16B }, [x0]
        ld1 { v31.16B }, [sp]

        ld1 { v0.4H }, [x0]
        ld1 { v31.4H }, [sp]
        ld1 { v0.8H }, [x0]
        ld1 { v31.8H }, [sp]

        ld1 { v0.2S }, [x0]
        ld1 { v31.2S }, [sp]
        ld1 { v0.4S }, [x0]
        ld1 { v31.4S }, [sp]

        ld1 { v0.2D }, [x0]
        ld1 { v31.2D }, [sp]

        ld2 { v0.8B,  v1.8B }, [x0]
        ld2 { v31.8B, v0.8B }, [sp]
        ld2 { v0.16B,  v1.16B }, [x0]
        ld2 { v31.16B, v0.16B }, [sp]

        ld2 { v0.4H,  v1.4H }, [x0]
        ld2 { v31.4H, v0.4H }, [sp]
        ld2 { v0.8H,  v1.8H }, [x0]
        ld2 { v31.8H, v0.8H }, [sp]

        ld2 { v0.2S,  v1.2S }, [x0]
        ld2 { v31.2S, v0.2S }, [sp]
        ld2 { v0.4S,  v1.4S }, [x0]
        ld2 { v31.4S, v0.4S }, [sp]

        ld2 { v0.2D,  v1.2D }, [x0]
        ld2 { v31.2D, v0.2D }, [sp]

        ld1 { v0.8B,  v1.8B }, [x0]
        ld1 { v31.8B, v0.8B }, [sp]
        ld1 { v0.16B,  v1.16B }, [x0]
        ld1 { v31.16B, v0.16B }, [sp]

        ld1 { v0.4H,  v1.4H }, [x0]
        ld1 { v31.4H, v0.4H }, [sp]
        ld1 { v0.8H,  v1.8H }, [x0]
        ld1 { v31.8H, v0.8H }, [sp]

        ld1 { v0.2S,  v1.2S }, [x0]
        ld1 { v31.2S, v0.2S }, [sp]
        ld1 { v0.4S,  v1.4S }, [x0]
        ld1 { v31.4S, v0.4S }, [sp]

        ld1 { v0.2D,  v1.2D }, [x0]
        ld1 { v31.2D, v0.2D }, [sp]


        ;
        ; Advanced SIMD load/store multiple structures, post-indexed register
        ;

        st4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], x1
        st4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], x30
        st4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], x1
        st4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], x30

        st4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], x1
        st4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], x30
        st4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], x1
        st4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], x30

        st4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], x1
        st4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], x30
        st4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], x1
        st4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], x30

        st4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], x1
        st4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], x30

        st1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], x1
        st1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], x30
        st1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], x1
        st1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], x30

        st1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], x1
        st1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], x30
        st1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], x1
        st1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], x30

        st1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], x1
        st1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], x30
        st1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], x1
        st1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], x30

        st1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], x1
        st1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], x30

        st3 { v0.8B,  v1.8B, v2.8B }, [x0], x1
        st3 { v31.8B, v0.8B, v1.8B }, [sp], x30
        st3 { v0.16B,  v1.16B, v2.16B }, [x0], x1
        st3 { v31.16B, v0.16B, v1.16B }, [sp], x30

        st3 { v0.4H,  v1.4H, v2.4H }, [x0], x1
        st3 { v31.4H, v0.4H, v1.4H }, [sp], x30
        st3 { v0.8H,  v1.8H, v2.8H }, [x0], x1
        st3 { v31.8H, v0.8H, v1.8H }, [sp], x30

        st3 { v0.2S,  v1.2S, v2.2S }, [x0], x1
        st3 { v31.2S, v0.2S, v1.2S }, [sp], x30
        st3 { v0.4S,  v1.4S, v2.4S }, [x0], x1
        st3 { v31.4S, v0.4S, v1.4S }, [sp], x30

        st3 { v0.2D,  v1.2D, v2.2D }, [x0], x1
        st3 { v31.2D, v0.2D, v1.2D }, [sp], x30

        st1 { v0.8B,  v1.8B, v2.8B }, [x0], x1
        st1 { v31.8B, v0.8B, v1.8B }, [sp], x30
        st1 { v0.16B,  v1.16B, v2.16B }, [x0], x1
        st1 { v31.16B, v0.16B, v1.16B }, [sp], x30

        st1 { v0.4H,  v1.4H, v2.4H }, [x0], x1
        st1 { v31.4H, v0.4H, v1.4H }, [sp], x30
        st1 { v0.8H,  v1.8H, v2.8H }, [x0], x1
        st1 { v31.8H, v0.8H, v1.8H }, [sp], x30

        st1 { v0.2S,  v1.2S, v2.2S }, [x0], x1
        st1 { v31.2S, v0.2S, v1.2S }, [sp], x30
        st1 { v0.4S,  v1.4S, v2.4S }, [x0], x1
        st1 { v31.4S, v0.4S, v1.4S }, [sp], x30

        st1 { v0.2D,  v1.2D, v2.2D }, [x0], x1
        st1 { v31.2D, v0.2D, v1.2D }, [sp], x30

        st1 { v0.8B }, [x0], x1
        st1 { v31.8B }, [sp], x30
        st1 { v0.16B }, [x0], x1
        st1 { v31.16B }, [sp], x30

        st1 { v0.4H }, [x0], x1
        st1 { v31.4H }, [sp], x30
        st1 { v0.8H }, [x0], x1
        st1 { v31.8H }, [sp], x30

        st1 { v0.2S }, [x0], x1
        st1 { v31.2S }, [sp], x30
        st1 { v0.4S }, [x0], x1
        st1 { v31.4S }, [sp], x30

        st1 { v0.2D }, [x0], x1
        st1 { v31.2D }, [sp], x30

        st2 { v0.8B,  v1.8B }, [x0], x1
        st2 { v31.8B, v0.8B }, [sp], x30
        st2 { v0.16B,  v1.16B }, [x0], x1
        st2 { v31.16B, v0.16B }, [sp], x30

        st2 { v0.4H,  v1.4H }, [x0], x1
        st2 { v31.4H, v0.4H }, [sp], x30
        st2 { v0.8H,  v1.8H }, [x0], x1
        st2 { v31.8H, v0.8H }, [sp], x30

        st2 { v0.2S,  v1.2S }, [x0], x1
        st2 { v31.2S, v0.2S }, [sp], x30
        st2 { v0.4S,  v1.4S }, [x0], x1
        st2 { v31.4S, v0.4S }, [sp], x30

        st2 { v0.2D,  v1.2D }, [x0], x1
        st2 { v31.2D, v0.2D }, [sp], x30

        st1 { v0.8B,  v1.8B }, [x0], x1
        st1 { v31.8B, v0.8B }, [sp], x30
        st1 { v0.16B,  v1.16B }, [x0], x1
        st1 { v31.16B, v0.16B }, [sp], x30

        st1 { v0.4H,  v1.4H }, [x0], x1
        st1 { v31.4H, v0.4H }, [sp], x30
        st1 { v0.8H,  v1.8H }, [x0], x1
        st1 { v31.8H, v0.8H }, [sp], x30

        st1 { v0.2S,  v1.2S }, [x0], x1
        st1 { v31.2S, v0.2S }, [sp], x30
        st1 { v0.4S,  v1.4S }, [x0], x1
        st1 { v31.4S, v0.4S }, [sp], x30

        st1 { v0.2D,  v1.2D }, [x0], x1
        st1 { v31.2D, v0.2D }, [sp], x30


        ld4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], x1
        ld4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], x30
        ld4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], x1
        ld4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], x30

        ld4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], x1
        ld4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], x30
        ld4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], x1
        ld4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], x30

        ld4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], x1
        ld4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], x30
        ld4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], x1
        ld4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], x30

        ld4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], x1
        ld4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], x30

        ld1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], x1
        ld1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], x30
        ld1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], x1
        ld1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], x30

        ld1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], x1
        ld1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], x30
        ld1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], x1
        ld1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], x30

        ld1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], x1
        ld1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], x30
        ld1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], x1
        ld1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], x30

        ld1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], x1
        ld1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], x30

        ld3 { v0.8B,  v1.8B, v2.8B }, [x0], x1
        ld3 { v31.8B, v0.8B, v1.8B }, [sp], x30
        ld3 { v0.16B,  v1.16B, v2.16B }, [x0], x1
        ld3 { v31.16B, v0.16B, v1.16B }, [sp], x30

        ld3 { v0.4H,  v1.4H, v2.4H }, [x0], x1
        ld3 { v31.4H, v0.4H, v1.4H }, [sp], x30
        ld3 { v0.8H,  v1.8H, v2.8H }, [x0], x1
        ld3 { v31.8H, v0.8H, v1.8H }, [sp], x30

        ld3 { v0.2S,  v1.2S, v2.2S }, [x0], x1
        ld3 { v31.2S, v0.2S, v1.2S }, [sp], x30
        ld3 { v0.4S,  v1.4S, v2.4S }, [x0], x1
        ld3 { v31.4S, v0.4S, v1.4S }, [sp], x30

        ld3 { v0.2D,  v1.2D, v2.2D }, [x0], x1
        ld3 { v31.2D, v0.2D, v1.2D }, [sp], x30

        ld1 { v0.8B,  v1.8B, v2.8B }, [x0], x1
        ld1 { v31.8B, v0.8B, v1.8B }, [sp], x30
        ld1 { v0.16B,  v1.16B, v2.16B }, [x0], x1
        ld1 { v31.16B, v0.16B, v1.16B }, [sp], x30

        ld1 { v0.4H,  v1.4H, v2.4H }, [x0], x1
        ld1 { v31.4H, v0.4H, v1.4H }, [sp], x30
        ld1 { v0.8H,  v1.8H, v2.8H }, [x0], x1
        ld1 { v31.8H, v0.8H, v1.8H }, [sp], x30

        ld1 { v0.2S,  v1.2S, v2.2S }, [x0], x1
        ld1 { v31.2S, v0.2S, v1.2S }, [sp], x30
        ld1 { v0.4S,  v1.4S, v2.4S }, [x0], x1
        ld1 { v31.4S, v0.4S, v1.4S }, [sp], x30

        ld1 { v0.2D,  v1.2D, v2.2D }, [x0], x1
        ld1 { v31.2D, v0.2D, v1.2D }, [sp], x30

        ld1 { v0.8B }, [x0], x1
        ld1 { v31.8B }, [sp], x30
        ld1 { v0.16B }, [x0], x1
        ld1 { v31.16B }, [sp], x30

        ld1 { v0.4H }, [x0], x1
        ld1 { v31.4H }, [sp], x30
        ld1 { v0.8H }, [x0], x1
        ld1 { v31.8H }, [sp], x30

        ld1 { v0.2S }, [x0], x1
        ld1 { v31.2S }, [sp], x30
        ld1 { v0.4S }, [x0], x1
        ld1 { v31.4S }, [sp], x30

        ld1 { v0.2D }, [x0], x1
        ld1 { v31.2D }, [sp], x30

        ld2 { v0.8B,  v1.8B }, [x0], x1
        ld2 { v31.8B, v0.8B }, [sp], x30
        ld2 { v0.16B,  v1.16B }, [x0], x1
        ld2 { v31.16B, v0.16B }, [sp], x30

        ld2 { v0.4H,  v1.4H }, [x0], x1
        ld2 { v31.4H, v0.4H }, [sp], x30
        ld2 { v0.8H,  v1.8H }, [x0], x1
        ld2 { v31.8H, v0.8H }, [sp], x30

        ld2 { v0.2S,  v1.2S }, [x0], x1
        ld2 { v31.2S, v0.2S }, [sp], x30
        ld2 { v0.4S,  v1.4S }, [x0], x1
        ld2 { v31.4S, v0.4S }, [sp], x30

        ld2 { v0.2D,  v1.2D }, [x0], x1
        ld2 { v31.2D, v0.2D }, [sp], x30

        ld1 { v0.8B,  v1.8B }, [x0], x1
        ld1 { v31.8B, v0.8B }, [sp], x30
        ld1 { v0.16B,  v1.16B }, [x0], x1
        ld1 { v31.16B, v0.16B }, [sp], x30

        ld1 { v0.4H,  v1.4H }, [x0], x1
        ld1 { v31.4H, v0.4H }, [sp], x30
        ld1 { v0.8H,  v1.8H }, [x0], x1
        ld1 { v31.8H, v0.8H }, [sp], x30

        ld1 { v0.2S,  v1.2S }, [x0], x1
        ld1 { v31.2S, v0.2S }, [sp], x30
        ld1 { v0.4S,  v1.4S }, [x0], x1
        ld1 { v31.4S, v0.4S }, [sp], x30

        ld1 { v0.2D,  v1.2D }, [x0], x1
        ld1 { v31.2D, v0.2D }, [sp], x30


        ;
        ; Advanced SIMD load/store multiple structures, post-indexed immediate
        ;

        st4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], #32
        st4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], #32
        st4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], #64
        st4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], #64

        st4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], #32
        st4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], #32
        st4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], #64
        st4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], #64

        st4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], #32
        st4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], #32
        st4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], #64
        st4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], #64

        st4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], #64
        st4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], #64

        st1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], #32
        st1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], #32
        st1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], #64
        st1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], #64

        st1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], #32
        st1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], #32
        st1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], #64
        st1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], #64

        st1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], #32
        st1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], #32
        st1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], #64
        st1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], #64

        st1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], #64
        st1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], #64

        st3 { v0.8B,  v1.8B, v2.8B }, [x0], #24
        st3 { v31.8B, v0.8B, v1.8B }, [sp], #24
        st3 { v0.16B,  v1.16B, v2.16B }, [x0], #48
        st3 { v31.16B, v0.16B, v1.16B }, [sp], #48

        st3 { v0.4H,  v1.4H, v2.4H }, [x0], #24
        st3 { v31.4H, v0.4H, v1.4H }, [sp], #24
        st3 { v0.8H,  v1.8H, v2.8H }, [x0], #48
        st3 { v31.8H, v0.8H, v1.8H }, [sp], #48

        st3 { v0.2S,  v1.2S, v2.2S }, [x0], #24
        st3 { v31.2S, v0.2S, v1.2S }, [sp], #24
        st3 { v0.4S,  v1.4S, v2.4S }, [x0], #48
        st3 { v31.4S, v0.4S, v1.4S }, [sp], #48

        st3 { v0.2D,  v1.2D, v2.2D }, [x0], #48
        st3 { v31.2D, v0.2D, v1.2D }, [sp], #48

        st1 { v0.8B,  v1.8B, v2.8B }, [x0], #24
        st1 { v31.8B, v0.8B, v1.8B }, [sp], #24
        st1 { v0.16B,  v1.16B, v2.16B }, [x0], #48
        st1 { v31.16B, v0.16B, v1.16B }, [sp], #48

        st1 { v0.4H,  v1.4H, v2.4H }, [x0], #24
        st1 { v31.4H, v0.4H, v1.4H }, [sp], #24
        st1 { v0.8H,  v1.8H, v2.8H }, [x0], #48
        st1 { v31.8H, v0.8H, v1.8H }, [sp], #48

        st1 { v0.2S,  v1.2S, v2.2S }, [x0], #24
        st1 { v31.2S, v0.2S, v1.2S }, [sp], #24
        st1 { v0.4S,  v1.4S, v2.4S }, [x0], #48
        st1 { v31.4S, v0.4S, v1.4S }, [sp], #48

        st1 { v0.2D,  v1.2D, v2.2D }, [x0], #48
        st1 { v31.2D, v0.2D, v1.2D }, [sp], #48

        st1 { v0.8B }, [x0], #8
        st1 { v31.8B }, [sp], #8
        st1 { v0.16B }, [x0], #16
        st1 { v31.16B }, [sp], #16

        st1 { v0.4H }, [x0], #8
        st1 { v31.4H }, [sp], #8
        st1 { v0.8H }, [x0], #16
        st1 { v31.8H }, [sp], #16

        st1 { v0.2S }, [x0], #8
        st1 { v31.2S }, [sp], #8
        st1 { v0.4S }, [x0], #16
        st1 { v31.4S }, [sp], #16

        st1 { v0.2D }, [x0], #16
        st1 { v31.2D }, [sp], #16

        st2 { v0.8B,  v1.8B }, [x0], #16
        st2 { v31.8B, v0.8B }, [sp], #16
        st2 { v0.16B,  v1.16B }, [x0], #32
        st2 { v31.16B, v0.16B }, [sp], #32

        st2 { v0.4H,  v1.4H }, [x0], #16
        st2 { v31.4H, v0.4H }, [sp], #16
        st2 { v0.8H,  v1.8H }, [x0], #32
        st2 { v31.8H, v0.8H }, [sp], #32

        st2 { v0.2S,  v1.2S }, [x0], #16
        st2 { v31.2S, v0.2S }, [sp], #16
        st2 { v0.4S,  v1.4S }, [x0], #32
        st2 { v31.4S, v0.4S }, [sp], #32

        st2 { v0.2D,  v1.2D }, [x0], #32
        st2 { v31.2D, v0.2D }, [sp], #32

        st1 { v0.8B,  v1.8B }, [x0], #16
        st1 { v31.8B, v0.8B }, [sp], #16
        st1 { v0.16B,  v1.16B }, [x0], #32
        st1 { v31.16B, v0.16B }, [sp], #32

        st1 { v0.4H,  v1.4H }, [x0], #16
        st1 { v31.4H, v0.4H }, [sp], #16
        st1 { v0.8H,  v1.8H }, [x0], #32
        st1 { v31.8H, v0.8H }, [sp], #32

        st1 { v0.2S,  v1.2S }, [x0], #16
        st1 { v31.2S, v0.2S }, [sp], #16
        st1 { v0.4S,  v1.4S }, [x0], #32
        st1 { v31.4S, v0.4S }, [sp], #32

        st1 { v0.2D,  v1.2D }, [x0], #32
        st1 { v31.2D, v0.2D }, [sp], #32


        ld4 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], #32
        ld4 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], #32
        ld4 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], #64
        ld4 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], #64

        ld4 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], #32
        ld4 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], #32
        ld4 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], #64
        ld4 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], #64

        ld4 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], #32
        ld4 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], #32
        ld4 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], #64
        ld4 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], #64

        ld4 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], #64
        ld4 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], #64

        ld1 { v0.8B,  v1.8B, v2.8B, v3.8B }, [x0], #32
        ld1 { v31.8B, v0.8B, v1.8B, v2.8B }, [sp], #32
        ld1 { v0.16B,  v1.16B, v2.16B, v3.16B }, [x0], #64
        ld1 { v31.16B, v0.16B, v1.16B, v2.16B }, [sp], #64

        ld1 { v0.4H,  v1.4H, v2.4H, v3.4H }, [x0], #32
        ld1 { v31.4H, v0.4H, v1.4H, v2.4H }, [sp], #32
        ld1 { v0.8H,  v1.8H, v2.8H, v3.8H }, [x0], #64
        ld1 { v31.8H, v0.8H, v1.8H, v2.8H }, [sp], #64

        ld1 { v0.2S,  v1.2S, v2.2S, v3.2S }, [x0], #32
        ld1 { v31.2S, v0.2S, v1.2S, v2.2S }, [sp], #32
        ld1 { v0.4S,  v1.4S, v2.4S, v3.4S }, [x0], #64
        ld1 { v31.4S, v0.4S, v1.4S, v2.4S }, [sp], #64

        ld1 { v0.2D,  v1.2D, v2.2D, v3.2D }, [x0], #64
        ld1 { v31.2D, v0.2D, v1.2D, v2.2D }, [sp], #64

        ld3 { v0.8B,  v1.8B, v2.8B }, [x0], #24
        ld3 { v31.8B, v0.8B, v1.8B }, [sp], #24
        ld3 { v0.16B,  v1.16B, v2.16B }, [x0], #48
        ld3 { v31.16B, v0.16B, v1.16B }, [sp], #48

        ld3 { v0.4H,  v1.4H, v2.4H }, [x0], #24
        ld3 { v31.4H, v0.4H, v1.4H }, [sp], #24
        ld3 { v0.8H,  v1.8H, v2.8H }, [x0], #48
        ld3 { v31.8H, v0.8H, v1.8H }, [sp], #48

        ld3 { v0.2S,  v1.2S, v2.2S }, [x0], #24
        ld3 { v31.2S, v0.2S, v1.2S }, [sp], #24
        ld3 { v0.4S,  v1.4S, v2.4S }, [x0], #48
        ld3 { v31.4S, v0.4S, v1.4S }, [sp], #48

        ld3 { v0.2D,  v1.2D, v2.2D }, [x0], #48
        ld3 { v31.2D, v0.2D, v1.2D }, [sp], #48

        ld1 { v0.8B,  v1.8B, v2.8B }, [x0], #24
        ld1 { v31.8B, v0.8B, v1.8B }, [sp], #24
        ld1 { v0.16B,  v1.16B, v2.16B }, [x0], #48
        ld1 { v31.16B, v0.16B, v1.16B }, [sp], #48

        ld1 { v0.4H,  v1.4H, v2.4H }, [x0], #24
        ld1 { v31.4H, v0.4H, v1.4H }, [sp], #24
        ld1 { v0.8H,  v1.8H, v2.8H }, [x0], #48
        ld1 { v31.8H, v0.8H, v1.8H }, [sp], #48

        ld1 { v0.2S,  v1.2S, v2.2S }, [x0], #24
        ld1 { v31.2S, v0.2S, v1.2S }, [sp], #24
        ld1 { v0.4S,  v1.4S, v2.4S }, [x0], #48
        ld1 { v31.4S, v0.4S, v1.4S }, [sp], #48

        ld1 { v0.2D,  v1.2D, v2.2D }, [x0], #48
        ld1 { v31.2D, v0.2D, v1.2D }, [sp], #48

        ld1 { v0.8B }, [x0], #8
        ld1 { v31.8B }, [sp], #8
        ld1 { v0.16B }, [x0], #16
        ld1 { v31.16B }, [sp], #16

        ld1 { v0.4H }, [x0], #8
        ld1 { v31.4H }, [sp], #8
        ld1 { v0.8H }, [x0], #16
        ld1 { v31.8H }, [sp], #16

        ld1 { v0.2S }, [x0], #8
        ld1 { v31.2S }, [sp], #8
        ld1 { v0.4S }, [x0], #16
        ld1 { v31.4S }, [sp], #16

        ld1 { v0.2D }, [x0], #16
        ld1 { v31.2D }, [sp], #16

        ld2 { v0.8B,  v1.8B }, [x0], #16
        ld2 { v31.8B, v0.8B }, [sp], #16
        ld2 { v0.16B,  v1.16B }, [x0], #32
        ld2 { v31.16B, v0.16B }, [sp], #32

        ld2 { v0.4H,  v1.4H }, [x0], #16
        ld2 { v31.4H, v0.4H }, [sp], #16
        ld2 { v0.8H,  v1.8H }, [x0], #32
        ld2 { v31.8H, v0.8H }, [sp], #32

        ld2 { v0.2S,  v1.2S }, [x0], #16
        ld2 { v31.2S, v0.2S }, [sp], #16
        ld2 { v0.4S,  v1.4S }, [x0], #32
        ld2 { v31.4S, v0.4S }, [sp], #32

        ld2 { v0.2D,  v1.2D }, [x0], #32
        ld2 { v31.2D, v0.2D }, [sp], #32

        ld1 { v0.8B,  v1.8B }, [x0], #16
        ld1 { v31.8B, v0.8B }, [sp], #16
        ld1 { v0.16B,  v1.16B }, [x0], #32
        ld1 { v31.16B, v0.16B }, [sp], #32

        ld1 { v0.4H,  v1.4H }, [x0], #16
        ld1 { v31.4H, v0.4H }, [sp], #16
        ld1 { v0.8H,  v1.8H }, [x0], #32
        ld1 { v31.8H, v0.8H }, [sp], #32

        ld1 { v0.2S,  v1.2S }, [x0], #16
        ld1 { v31.2S, v0.2S }, [sp], #16
        ld1 { v0.4S,  v1.4S }, [x0], #32
        ld1 { v31.4S, v0.4S }, [sp], #32

        ld1 { v0.2D,  v1.2D }, [x0], #32
        ld1 { v31.2D, v0.2D }, [sp], #32


        ;
        ; RCW compare and swap
        ;
        ;.cpu generic+the+d128

        ;rcwcas x0,   x1, [x3] - Requires FEAT_THE
        ;rcwcas xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwcasl x0,   x1, [x3] - Requires FEAT_THE
        ;rcwcasl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwcasa x0,   x1, [x3] - Requires FEAT_THE
        ;rcwcasa xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwcasal x0,   x1, [x3] - Requires FEAT_THE
        ;rcwcasal xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwscas x0,   x1, [x3] - Requires FEAT_THE
        ;rcwscas xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwscasl x0,   x1, [x3] - Requires FEAT_THE
        ;rcwscasl xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwscasa x0,   x1, [x3] - Requires FEAT_THE
        ;rcwscasa xzr, xzr, [sp] - Requires FEAT_THE

        ;rcwscasal x0,   x1, [x3] - Requires FEAT_THE
        ;rcwscasal xzr, xzr, [sp] - Requires FEAT_THE

        ;
        ; RCW compare and swap pair
        ;

        ;rcwcasp x0, x1, x2, x3, [x4] - Requires FEAT_THE && FEAT_D128
        ;rcwcasp x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwcaspl x0, x1, x2, x3, [x4] - Requires FEAT_THE && FEAT_D128
        ;rcwcaspl x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwcaspa x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwcaspa x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwcaspal x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwcaspal x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwscasp x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwscasp x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwscaspl x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwscaspl x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwscaspa x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwscaspa x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;rcwscaspal x0, x1, x2, x3, [x3] - Requires FEAT_THE && FEAT_D128
        ;rcwscaspal x0, x1, x2, x3, [sp] - Requires FEAT_THE && FEAT_D128

        ;
        ; Keep last so the testcase can catch errors in
        ; the disassembly of the last instruction.
        ;
        nop

.private_extern _TestProcA64_EndProc
_TestProcA64_EndProc:
