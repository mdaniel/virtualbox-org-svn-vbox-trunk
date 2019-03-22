; $Id$
;; @file
; IPRT - ASMCpuIdExSlow().
;

;
; Copyright (C) 2012-2019 Oracle Corporation
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
; CPUID with EAX input, returning ALL output registers (no NULL checking).
;
; @param    uOperator   8086:bp+4   x86:ebp+8   gcc:rdi  msc:rcx
; @param    pvEAX       8086:bp+8   x86:ebp+0c  gcc:rsi  msc:rdx
; @param    pvEBX       8086:bp+0c  x86:ebp+10  gcc:rdx  msc:r8
; @param    pvECX       8086:bp+10  x86:ebp+14  gcc:rcx  msc:r9
; @param    pvEDX       8086:bp+14  x86:ebp+18  gcc:r8   msc:rbp+30h
;
; DECLASM(void) ASMCpuId(uint32_t uOperator, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX);
;
BEGINPROC_EXPORTED ASMCpuId
        push    xBP
        mov     xBP, xSP
        push    xBX

%ifdef ASM_CALL64_MSC
 %if ARCH_BITS != 64
  %error ARCH_BITS mismatch?
 %endif
        mov     eax, ecx
        mov     r10, rdx
        cpuid
        mov     [r10], eax
        mov     [r8], ebx
        mov     [r9], ecx
        mov     r10, [rbp+30h]
        mov     [r10], edx

%elifdef ASM_CALL64_GCC
        mov     eax, edi
        mov     r10, rdx
        mov     r11, rcx
        cpuid
        mov     [rsi], eax
        mov     [r10], ebx
        mov     [r11], ecx
        mov     [r8], edx

%elif ARCH_BITS == 32
        mov     eax, [xBP + 08h]
        cpuid
        push    edx
        mov     edx, [xBP + 0ch]
        mov     [edx], eax
        mov     edx, [xBP + 10h]
        mov     [edx], ebx
        mov     edx, [xBP + 14h]
        mov     [edx], ecx
        mov     edx, [xBP + 18h]
        pop     dword [edx]

%elif ARCH_BITS == 16
        push    es
        push    di

        mov     eax, [xBP + 04h]
        cpuid
        les     di, [xBP + 08h]
        mov     [di], eax
        les     di, [xBP + 0ch]
        mov     [di], ebx
        les     di, [xBP + 10h]
        mov     [di], ecx
        les     di, [xBP + 14h]
        mov     [di], edx

        pop     di
        pop     es
%else
 %error unsupported arch
%endif

        pop     xBX
        leave
        ret
ENDPROC ASMCpuId

