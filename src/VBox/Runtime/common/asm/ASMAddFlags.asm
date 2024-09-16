; $Id$
;; @file
; IPRT - ASMSetFlags().
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
; @param rcx/rdi  eflags to add
RT_BEGINPROC ASMAddFlags
%if    ARCH_BITS == 64
        pushfq
        mov     rax, [rsp]
 %ifdef ASM_CALL64_GCC
        or      rdi, rax
        mov     [rsp], rdi
 %else
        or      rcx, rax
        mov     [rsp], rcx
 %endif
        popfq
%elif  ARCH_BITS == 32
        mov     ecx, [esp + 4]
        pushfd
        mov     eax, [esp]
        or      ecx, eax
        mov     [esp], ecx
        popfd
%elif  ARCH_BITS == 16
        push    bp
        mov     bp, sp
        pushf
        pop     ax
        push    word [bp + 2 + 2]
        or      [bp - 2], ax
        popf
        leave
%else
 %error ARCH_BITS
%endif
        ret
ENDPROC ASMAddFlags

