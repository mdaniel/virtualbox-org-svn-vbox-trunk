; $Id$
;; @file
; IPRT - No-CRT memrchr - AMD64 & X86.
;

;
; Copyright (C) 2006-2019 Oracle Corporation
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

%include "iprt/asmdefs.mac"

BEGINCODE

;;
; @param    pv      gcc: rdi  msc: ecx  x86:[esp+4]   wcall: eax
; @param    ch      gcc: esi  msc: edx  x86:[esp+8]   wcall: edx
; @param    cb      gcc: rdx  msc: r8   x86:[esp+0ch] wcall: ebx
RT_NOCRT_BEGINPROC memrchr
        std
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        or      r8, r8
        jz      .not_found_early

        mov     r9, rdi                 ; save rdi
        mov     eax, edx
        lea     rdi, [rcx + r8 - 1]
        mov     rcx, r8
 %else
        mov     rcx, rdx
        jrcxz   .not_found_early

        mov     eax, esi
        lea     rdi, [rdi + rcx - 1]
 %endif

%else
 %ifdef ASM_CALL32_WATCOM
        mov     ecx, ebx
        jecxz   .not_found_early
        xchg    eax, edx
        xchg    edi, edx                ; load + save edi
 %else
        mov     ecx, [esp + 0ch]
        jecxz   .not_found_early
        mov     edx, edi                ; save edi
        mov     eax, [esp + 8]
        mov     edi, [esp + 4]
 %endif
        lea     edi, [edi + ecx - 1]
%endif

        ; do the search
        repne   scasb
        jne     .not_found

        ; found it
        lea     xAX, [xDI + 1]
%ifdef ASM_CALL64_MSC
        mov     rdi, r9
%endif
%ifdef RT_ARCH_X86
        mov     edi, edx
%endif
        cld
        ret

.not_found:
%ifdef ASM_CALL64_MSC
        mov     rdi, r9
%endif
%ifdef RT_ARCH_X86
        mov     edi, edx
%endif
.not_found_early:
        xor     eax, eax
        cld
        ret
ENDPROC RT_NOCRT(memrchr)

