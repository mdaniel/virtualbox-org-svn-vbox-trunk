; $Id$
;; @file
; BS3Kit - Bs3RegGetCr2
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


BS3_EXTERN_CMN Bs3Panic
BS3_EXTERN_CMN Bs3Syscall
%if TMPL_BITS == 16
BS3_EXTERN_DATA16 g_bBs3CurrentMode
%endif
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_CMN_PROTO_STUB(RTCCUINTXREG, Bs3RegGetCr2,(void));
;
; @returns  Register value.
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
; @uses     No GPRs (only return full register(s)).
;
BS3_PROC_BEGIN_CMN Bs3RegGetCr2, BS3_PBC_HYBRID_SAFE
        BS3_CALL_CONV_PROLOG 0
        push    xBP
        mov     xBP, xSP

%if TMPL_BITS == 16
        ; If V8086 mode we have to go thru a syscall.
        test    byte [BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_CODE_V86
        jnz     .via_system_call
        cmp     byte [BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_RM
        je      .direct_access
%endif
        ; If not in ring-0, we have to make a system call.
        mov     ax, ss
        and     ax, X86_SEL_RPL
        jnz     .via_system_call

.direct_access:
        mov     sAX, cr2
TONLY16 mov     edx, eax
TONLY16 shr     edx, 16
        jmp     .return

.via_system_call:
        mov     xAX, BS3_SYSCALL_GET_CRX
        mov     dl, 2
        call    Bs3Syscall

.return:
        pop     xBP
        BS3_CALL_CONV_EPILOG 0
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3RegGetCr2

