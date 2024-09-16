; $Id$
;; @file
; IPRT - ASMAtomicXchgU64().
;

;
; Copyright (C) 2006-2024 Oracle and/or its affiliates.
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Atomically Exchange an unsigned 64-bit value, ordered.
;
; @param    pu64     x86:ebp+8   gcc:rdi  msc:rcx
; @param    u64New   x86:ebp+c   gcc:rsi  msc:rdx
;
; @returns Current (i.e. old) *pu64 value (x86:eax:edx, 64-bit: rax)
;
RT_BEGINPROC ASMAtomicXchgU64
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        xchg    [rcx], rdx
 %else
        xchg    [rdi], rsi
 %endif
        ret
%endif
%ifdef RT_ARCH_X86
        push    ebp
        mov     ebp, esp
        push    ebx
        push    edi

.try_again:
        mov     ebx, dword [ebp+0ch]
        mov     ecx, dword [ebp+0ch + 4]
        mov     edi, [ebp+08h]
        lock cmpxchg8b [edi]
        jnz     .try_again

        pop     edi
        pop     ebx
        leave
        ret
%endif
ENDPROC ASMAtomicXchgU64

