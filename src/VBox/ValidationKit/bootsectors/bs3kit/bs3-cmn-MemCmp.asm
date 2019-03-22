; $Id$
;; @file
; BS3Kit - Bs3MemCmp.
;

;
; Copyright (C) 2007-2017 Oracle Corporation
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
; @cproto   BS3_CMN_PROTO_NOSB(int, Bs3MemCmp,(void const BS3_FAR *pv1, void const BS3_FAR *pv2, size_t cb));
;
BS3_PROC_BEGIN_CMN Bs3MemCmp, BS3_PBC_HYBRID
TONLY16 CPU 8086
        push    xBP
        mov     xBP, xSP
        push    xDI
        push    xSI
TNOT64  push    es
TONLY16 push    ds
        cld

        ;
        ; To save complexity and space, do straight forward byte compares.
        ;
%if TMPL_BITS == 16
        mov     di, [bp + 2 + cbCurRetAddr]         ; pv1.off
        mov     es, [bp + 2 + cbCurRetAddr + 2]     ; pv1.sel
        mov     si, [bp + 2 + cbCurRetAddr + 4]     ; pv2.off
        mov     ds, [bp + 2 + cbCurRetAddr + 6]     ; pv2.sel
        mov     cx, [bp + 2 + cbCurRetAddr + 8]     ; cbDst
        xor     ax, ax
        repe cmpsb
        je      .return

        mov     al, [es:di - 1]
        xor     dx, dx
        mov     dl, [esi - 1]
        sub     ax, dx

%else
 %if TMPL_BITS == 64
        mov     rdi, rcx                            ; rdi = pv1
        mov     rsi, rdx                            ; rdi = pv2
        mov     rcx, r8                             ; rcx = cbDst
 %else
        mov     ax, ds
        mov     es, ax                              ; paranoia
        mov     edi, [ebp + 4 + cbCurRetAddr]       ; pv1
        mov     esi, [ebp + 4 + cbCurRetAddr + 4]   ; pv2
        mov     ecx, [ebp + 4 + cbCurRetAddr + 8]   ; cbDst
 %endif
        xor     eax, eax
        repe cmpsb
        je      .return

        mov     al, [xDI - 1]
        movzx   edx, byte [xSI - 1]
        sub     eax, edx
%endif

.return:
TONLY16 pop     ds
TNOT64  pop     es
        pop     xSI
        pop     xDI
        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3MemCmp

