; $Id$
;; @file
; BS3Kit - Bs3SwitchFromV86To16BitAndCallC
;

;
; Copyright (C) 2007-2024 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
; in the VirtualBox distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;
; SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
;

;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "bs3kit-template-header.mac"
%if TMPL_BITS != 16
 %error "16-bit only"
%endif


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
%ifdef BS3_STRICT
BS3_EXTERN_DATA16 g_bBs3CurrentMode
TMPL_BEGIN_TEXT
BS3_EXTERN_CMN    Bs3Panic
%endif
BS3_EXTERN_CMN    Bs3SwitchTo16Bit
BS3_EXTERN_CMN    Bs3SwitchTo16BitV86
BS3_EXTERN_CMN    Bs3SelRealModeCodeToProtMode


;;
; @cproto   BS3_CMN_PROTO_STUB(int, Bs3SwitchFromV86To16BitAndCallC,(FPFNBS3FAR fpfnCall, unsigned cbParams, ...));
;
BS3_PROC_BEGIN_CMN Bs3SwitchFromV86To16BitAndCallC, BS3_PBC_HYBRID
        inc     bp
        push    bp
        mov     bp, sp

        ;
        ; Push the arguments first.
        ;
        mov     ax, si                  ; save si
        mov     si, [bp + 2 + cbCurRetAddr + 4]
%ifdef BS3_STRICT
        test    si, 1
        jz      .cbParams_ok
        call    Bs3Panic
.cbParams_ok:
        test    byte [g_bBs3CurrentMode], BS3_MODE_CODE_V86
        jnz     .mode_ok
        call    Bs3Panic
.mode_ok:
%endif

.push_more:
        push    word [bp + 2 + cbCurRetAddr + 4 + 2 + si - 2]
        sub     si, 2
        jnz     .push_more
        mov     si, ax                  ; restore si

        ;
        ; Convert the code segment to a 16-bit prot mode selector
        ;
        push    word [bp + 2 + cbCurRetAddr + 2]
        call    Bs3SelRealModeCodeToProtMode
        mov     [bp + 2 + cbCurRetAddr + 2], ax
        add     sp, 2

        ;
        ; Switch mode.
        ;
        call    Bs3SwitchTo16Bit
        call far [bp + 2 + cbCurRetAddr]
        call    Bs3SwitchTo16BitV86

        mov     sp, bp
        pop     bp
        dec     bp
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3SwitchFromV86To16BitAndCallC

