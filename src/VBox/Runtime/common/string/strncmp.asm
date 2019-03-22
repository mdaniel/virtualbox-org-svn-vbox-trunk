; $Id$
;; @file
; IPRT - No-CRT strncmp - AMD64 & X86.
;

;
; Copyright (C) 2006-2017 Oracle Corporation
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
; @param    psz1   gcc: rdi  msc: rcx  x86:[esp+4]   wcall: eax
; @param    psz2   gcc: rsi  msc: rdx  x86:[esp+8]   wcall: edx
; @param    cch    gcc: rdx  msc: r8   x86:[esp+12]  wcall: ebx
RT_NOCRT_BEGINPROC strncmp
        ; input
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
  %define psz1 rcx
  %define psz2 rdx
  %define cch  r8
 %else
  %define psz1 rdi
  %define psz2 rsi
  %define cch  rdx
 %endif
%elifdef ASM_CALL32_WATCOM
        mov    ecx, eax
  %define psz1 ecx
  %define psz2 edx
  %define cch  ebx

%elifdef RT_ARCH_X86
        mov     ecx, [esp + 4]
        mov     edx, [esp + 8]
        push    ebx
        mov     ebx, [esp + 12+4]
  %define psz1 ecx
  %define psz2 edx
  %define cch  ebx
%else
 %error "Unknown arch"
%endif

        ;
        ; The loop.
        ;
        test    cch, cch
        jz      .equal
.next:
        mov     al, [psz1]
        mov     ah, [psz2]
        cmp     al, ah
        jne     .not_equal
        test    al, al
        jz      .equal
        dec     cch
        jz      .equal

        mov     al, [psz1 + 1]
        mov     ah, [psz2 + 1]
        cmp     al, ah
        jne     .not_equal
        test    al, al
        jz      .equal
        dec     cch
        jz      .equal

        mov     al, [psz1 + 2]
        mov     ah, [psz2 + 2]
        cmp     al, ah
        jne     .not_equal
        test    al, al
        jz      .equal
        dec     cch
        jz      .equal

        mov     al, [psz1 + 3]
        mov     ah, [psz2 + 3]
        cmp     al, ah
        jne     .not_equal
        test    al, al
        jz      .equal
        dec     cch
        jz      .equal

        add     psz1, 4
        add     psz2, 4
        jmp     .next

.equal:
        xor     eax, eax
%ifndef ASM_CALL32_WATCOM
 %ifdef RT_ARCH_X86
        pop     ebx
 %endif
%endif
        ret

.not_equal:
        movzx   ecx, ah
        and     eax, 0ffh
        sub     eax, ecx
%ifndef ASM_CALL32_WATCOM
 %ifdef RT_ARCH_X86
        pop     ebx
 %endif
%endif
        ret
ENDPROC RT_NOCRT(strncmp)

