; $Id$
;; @file
; BS3Kit - Bs3SwitchToPP16_V86
;

;
; Copyright (C) 2007-2019 Oracle Corporation
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


;;
; Switch to 16-bit paged protected mode with 16-bit sys+tss from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToPP16(void);
;
; @uses     Nothing (except high 32-bit register parts).
;
; @remarks  Obviously returns to v8086 16-bit mode, even if the caller was
;           in 16-bit, 32-bit or 64-bit mode.
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
%if TMPL_BITS == 16
BS3_GLOBAL_NAME_EX TMPL_NM(Bs3SwitchToPP16_V86_Safe), function , 0
%endif
BS3_PROC_BEGIN_MODE Bs3SwitchToPP16_V86, BS3_PBC_NEAR
%ifdef TMPL_PP16_V86
        ret

%else
        ;
        ; Convert the return address and jump to the 16-bit code segment.
        ;
 %if TMPL_BITS != 16
        shl     xPRE [xSP], TMPL_BITS - 16
        add     xSP, (TMPL_BITS - 16) / 8
        jmp     .sixteen_bit_segment
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
BS3_GLOBAL_LOCAL_LABEL .sixteen_bit_segment
 %endif

        ;
        ; Switch to 16-bit PP16 and from there to V8086.
        ;
        extern  TMPL_NM(Bs3SwitchToPP16)
        call    TMPL_NM(Bs3SwitchToPP16)
        BS3_SET_BITS 16

        ;
        ; Switch to v8086 mode (return address is already 16-bit).
        ;
        extern  _Bs3SwitchTo16BitV86_c16
        jmp     _Bs3SwitchTo16BitV86_c16
%endif
BS3_PROC_END_MODE   Bs3SwitchToPP16_V86


%if TMPL_BITS == 16
;;
; Custom far stub.
BS3_BEGIN_TEXT16_FARSTUBS
BS3_PROC_BEGIN_MODE Bs3SwitchToPP16_V86, BS3_PBC_FAR
        inc         bp
        push        bp
        mov         bp, sp

        ; Call the real thing.
        call        TMPL_NM(Bs3SwitchToPP16_V86)

 %if !BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
        ; Jmp to common code for the tedious conversion.
        BS3_EXTERN_CMN Bs3SwitchHlpConvProtModeRetfPopBpDecBpAndReturn
        jmp         Bs3SwitchHlpConvProtModeRetfPopBpDecBpAndReturn
 %else
        pop         bp
        dec         bp
        retf
 %endif
BS3_PROC_END_MODE   Bs3SwitchToPP16_V86

%else
;;
; Safe far return to non-BS3TEXT16 code.
BS3_EXTERN_CMN Bs3SwitchHlpConvFlatRetToRetfProtMode
BS3_BEGIN_TEXT16
BS3_SET_BITS TMPL_BITS
BS3_PROC_BEGIN_MODE Bs3SwitchToPP16_V86_Safe, BS3_PBC_NEAR
        call        Bs3SwitchHlpConvFlatRetToRetfProtMode ; Special internal function.  Uses nothing, but modifies the stack.
        call        TMPL_NM(Bs3SwitchToPP16_V86)
        BS3_SET_BITS 16
        retf
BS3_PROC_END_MODE   Bs3SwitchToPP16_V86_Safe
%endif

