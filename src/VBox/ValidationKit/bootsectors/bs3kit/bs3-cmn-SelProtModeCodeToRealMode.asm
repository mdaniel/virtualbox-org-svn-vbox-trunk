; $Id$
;; @file
; BS3Kit - Bs3SelProtModeCodeToRealMode.
;

;
; Copyright (C) 2007-2016 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;

%include "bs3kit-template-header.mac"

;
; Make sure we can get at all the segments.
;
BS3_BEGIN_TEXT16
BS3_BEGIN_RMTEXT16
BS3_BEGIN_X0TEXT16
BS3_BEGIN_X1TEXT16
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_CMN_PROTO(uint16_t, Bs3SelProtModeCodeToRealMode,(uint16_t uRealSel), false);
; @uses     ax (return register)
;
BS3_PROC_BEGIN_CMN Bs3SelProtModeCodeToRealMode, BS3_PBC_NEAR
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP

        ; Load it and mask off the RPL.
        mov     ax, [xBP + xCB + cbCurRetAddr]
        and     ax, X86_SEL_MASK_OFF_RPL

        ; We're allowed to use the real-mode segment value.
        cmp     ax, CGROUP16
        je      .bs3text16

        ; Check for typical code segments.
        cmp     ax, BS3_SEL_R0_CS16
        je      .bs3text16
        cmp     ax, BS3_SEL_RMTEXT16_CS
        je      .bs3rmtext16
        cmp     ax, BS3_SEL_X0TEXT16_CS
        je      .bs3x0text16
        cmp     ax, BS3_SEL_X1TEXT16_CS
        je      .bs3x1text16

        ; Check for less common BS3_SEL_R*_CS16_* values.
        cmp     ax, BS3_SEL_R0_FIRST
        jb      .panic
        cmp     ax, BS3_SEL_R3_FIRST + (1 << BS3_SEL_RING_SHIFT)
        jae     .panic

        ; Since the relevant bits now are the lower 8 ones, we skip the
        ;       AND AX, BS3_SEL_RING_SHIFT
        ;       ADD AX, BS3_SEL_R0_FIRST
        ; bits and compare AL with lower 8-bit of the BS3_SEL_R0_CS16* values.
AssertCompile(BS3_SEL_RING_SHIFT == 8)
        cmp     al, BS3_SEL_R0_CS16 & 0xff
        je      .bs3text16
        cmp     al, BS3_SEL_R0_CS16_EO & 0xff
        je      .bs3text16
        cmp     ax, BS3_SEL_R0_CS16_CNF & 0xff
        je      .bs3text16
        cmp     ax, BS3_SEL_R0_CS16_CNF_EO & 0xff
        je      .bs3text16
.panic:
        extern  BS3_CMN_NM(Bs3Panic)
        call    BS3_CMN_NM(Bs3Panic)
        jmp     .return

.bs3x1text16:
        mov     ax, BS3GROUPX1TEXT16
        jmp     .return
.bs3x0text16:
        mov     ax, BS3GROUPX0TEXT16
        jmp     .return
.bs3rmtext16:
        mov     ax, BS3GROUPRMTEXT16
        jmp     .return
.bs3text16:
        mov     ax, CGROUP16
.return:
        pop     xBP
        BS3_CALL_CONV_EPILOG 1
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3SelProtModeCodeToRealMode

;
; We may be using the near code in some critical code paths, so don't
; penalize it.
;
BS3_CMN_FAR_STUB   Bs3SelProtModeCodeToRealMode, 2

