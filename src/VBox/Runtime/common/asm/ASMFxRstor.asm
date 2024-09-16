; $Id$
;; @file
; IPRT - ASMFxRstor().
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
%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Loads extended CPU state.
; @param    pFxState    Pointer to the FXRSTOR state area.
;                       msc=rcx, gcc=rdi, x86=[esp+4]
;
RT_BEGINPROC ASMFxRstor
SEH64_END_PROLOGUE
%ifdef ASM_CALL64_MSC
        o64 fxrstor [rcx]
%elifdef ASM_CALL64_GCC
        o64 fxrstor [rdi]
%elif ARCH_BITS == 32
        mov     ecx, [esp + 4]
        fxrstor [ecx]
%elif ARCH_BITS == 16
        push    bp
        mov     bp, sp
        push    es
        push    bx
        les     bx, [bp + 4]
        fxrstor [es:bx]
        pop     bx
        pop     es
        pop     bp
%else
 %error "Undefined arch?"
%endif
        ret
ENDPROC ASMFxRstor

