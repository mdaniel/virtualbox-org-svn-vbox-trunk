; $Id$
;; @file
; IPRT - ASMCpuId_Idx_ECX().
;

;
; Copyright (C) 2012-2017 Oracle Corporation
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; CPUID with EAX and ECX inputs, returning ALL output registers.
;
; @param    uOperator   x86:ebp+8   gcc:rdi  msc:rcx
; @param    uIdxECX     x86:ebp+c   gcc:rsi  msc:rdx
; @param    pvEAX       x86:ebp+10  gcc:rdx  msc:r8
; @param    pvEBX       x86:ebp+14  gcc:rcx  msc:r9
; @param    pvECX       x86:ebp+18  gcc:r8   msc:rsp+28h
; @param    pvEDX       x86:ebp+1c  gcc:r9   msc:rsp+30h
;
; @returns  void
;
BEGINPROC_EXPORTED ASMCpuId_Idx_ECX
%ifdef RT_ARCH_AMD64
        mov     r10, rbx

 %ifdef ASM_CALL64_MSC

        mov     eax, ecx
        mov     ecx, edx
        xor     ebx, ebx
        xor     edx, edx

        cpuid

        mov     [r8], eax
        mov     [r9], ebx
        mov     rax, [rsp + 28h]
        mov     rbx, [rsp + 30h]
        mov     [rax], ecx
        mov     [rbx], edx

 %else
        mov     rsi, rdx
        mov     r11, rcx
        mov     eax, edi
        mov     ecx, esi
        xor     ebx, ebx
        xor     edx, edx

        cpuid

        mov     [rsi], eax
        mov     [r11], ebx
        mov     [r8], ecx
        mov     [r9], edx

 %endif

        mov     rbx, r10
        ret

%elifdef RT_ARCH_X86
        push    ebp
        mov     ebp, esp
        push    ebx
        push    edi

        xor     edx, edx
        xor     ebx, ebx
        mov     eax, [ebp + 08h]
        mov     ecx, [ebp + 0ch]

        cpuid

        mov     edi, [ebp + 10h]
        mov     [edi], eax
        mov     edi, [ebp + 14h]
        mov     [edi], ebx
        mov     edi, [ebp + 18h]
        mov     [edi], ecx
        mov     edi, [ebp + 1ch]
        mov     [edi], edx

        pop     edi
        pop     ebx
        leave
        ret
%else
 %error unsupported arch
%endif
ENDPROC ASMCpuId_Idx_ECX

