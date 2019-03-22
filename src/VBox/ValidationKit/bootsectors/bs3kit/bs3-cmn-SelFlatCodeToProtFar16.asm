; $Id$
;; @file
; BS3Kit - Bs3SelFlatCodeToProtFar16.
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


;*********************************************************************************************************************************
;*      Header Files                                                                                                             *
;*********************************************************************************************************************************
%include "bs3kit-template-header.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_EXTERN_CMN Bs3SelFlatCodeToRealMode
BS3_EXTERN_CMN Bs3SelRealModeCodeToProtMode
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_CMN_PROTO(uint32_t, Bs3SelRealModeCodeToProtMode,(uint32_t uFlatAddr), false);
;
; @uses     Only return registers (ax:dx, eax, eax)
;
BS3_PROC_BEGIN_CMN Bs3SelFlatCodeToProtFar16, BS3_PBC_NEAR
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP

        ;
        ; Call Bs3SelFlatCodeToRealMode and then Bs3SelRealModeCodeToProtMode.
        ; This avoid some code duplication.
        ;
%if TMPL_BITS == 16
        push    word [xBP + xCB + cbCurRetAddr + 2]
        push    word [xBP + xCB + cbCurRetAddr]
        call    Bs3SelFlatCodeToRealMode
        add     sp, 4h

        push    ax                      ; save the offset as it will be the same.

        push    dx
        call    Bs3SelRealModeCodeToProtMode
        add     sp, 2h

        mov     dx, ax                  ; The protected mode selector.
        pop     ax                      ; The offset.

%elif TMPL_BITS == 32
        push    dword [xBP + xCB + cbCurRetAddr]
        call    Bs3SelFlatCodeToRealMode
        add     esp, 4h

        push    eax                     ; save the result.

        shr     eax, 16
        push    eax
        call    Bs3SelRealModeCodeToProtMode
        add     esp, 4h

        mov     [esp + 2], ax           ; Update the selector before popping the result.
        pop     eax

%elif TMPL_BITS == 64
        push    xCX                     ; Preserve RCX to make the behaviour uniform.
        sub     xSP, 28h                ; 20h bytes of calling convention scratch and 8 byte for saving the result.

        mov     ecx, [xBP + xCB + cbCurRetAddr] ; move straight to parameter 0 register.
        call    Bs3SelFlatCodeToRealMode

        mov     [xBP - xCB*2], eax      ; Save the result.

        shr     eax, 16
        mov     ecx, eax                ; Move straight to parameter 0 register.
        call    Bs3SelRealModeCodeToProtMode

        shl     eax, 16                 ; Shift prot mode selector into result position.
        mov     ax, [xBP - xCB*2]       ; The segment offset from the previous call.

        add     xSP, 28h
        pop     xCX
%else
 %error "TMPL_BITS=" TMPL_BITS "!"
%endif
        pop     xBP
        BS3_CALL_CONV_EPILOG 1
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3SelFlatCodeToProtFar16


;
; We may be using the near code in some critical code paths, so don't
; penalize it.
;
BS3_CMN_FAR_STUB   Bs3SelFlatCodeToProtFar16, 4

