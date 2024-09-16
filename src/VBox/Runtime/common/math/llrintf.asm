; $Id$
;; @file
; IPRT - No-CRT llrintf - AMD64 & X86.
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


%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"


BEGINCODE

;;
; Round rf to the nearest integer value, rounding according to the current rounding direction.
; @returns 32-bit: edx:eax  64-bit: rax
; @param    rf     32-bit: [esp + 4h]  64-bit: xmm0
RT_NOCRT_BEGINPROC llrintf
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
%ifdef RT_ARCH_X86
        sub     xSP, 10h
        SEH64_ALLOCATE_STACK 10h
%endif
        SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
        cvtss2si rax, xmm0
%else
        fld     dword [ebp + 8h]
        fistp   qword [esp]
        fwait
        mov     eax, [esp]
        mov     edx, [esp + 4]
%endif

        leave
        ret
ENDPROC   RT_NOCRT(llrintf)

