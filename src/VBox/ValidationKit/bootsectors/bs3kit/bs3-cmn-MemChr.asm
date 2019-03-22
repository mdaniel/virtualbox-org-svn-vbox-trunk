; $Id$
;; @file
; BS3Kit - Bs3MemChr.
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
; @cproto   BS3_CMN_PROTO_NOSB(void BS3_FAR *, Bs3MemChr,(void BS3_FAR const *pvHaystack, uint8_t bNeedle, size_t cbHaystack));
;
BS3_PROC_BEGIN_CMN Bs3MemChr, BS3_PBC_HYBRID
        push    xBP
        mov     xBP, xSP
        push    xDI
TONLY16 push    es

%if TMPL_BITS == 64

        mov     rdi, rcx                ; rdi = pvHaystack
        mov     rcx, r8                 ; rcx = cbHaystack
        mov     al, dl                  ; bNeedle
        mov     rcx, r8

%elif TMPL_BITS == 16
        mov     di, [bp + 2 + cbCurRetAddr]     ; pvHaystack.off
        mov     es, [bp + 2 + cbCurRetAddr + 2] ; pvHaystack.sel
        mov     al, [bp + 2 + cbCurRetAddr + 4] ; bNeedle
        mov     cx, [bp + 2 + cbCurRetAddr + 6] ; cbHaystack

%elif TMPL_BITS == 32
        mov     edi, [ebp + 8]                          ; pvHaystack
        mov     al, byte [ebp + 4 + cbCurRetAddr + 4]   ; bNeedle
        mov     ecx, [ebp + 4 + cbCurRetAddr + 8]       ; cbHaystack
%else
 %error "TMPL_BITS!"
%endif

        cld
        repne scasb
        je      .found

        xor     xAX, xAX
TONLY16 xor     dx, dx

.return:
TONLY16 pop     es
        pop     xDI
        pop     xBP
        BS3_HYBRID_RET

.found:
        lea     xAX, [xDI - 1]
TONLY16 mov     dx, es
        jmp     .return

BS3_PROC_END_CMN   Bs3MemChr

