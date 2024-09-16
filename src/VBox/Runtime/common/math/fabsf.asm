; $Id$
;; @file
; IPRT - No-CRT fabsf - AMD64 & X86.
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
; Compute the absolute value of rf (|rf|).
; @returns 32-bit: st(0)   64-bit: xmm0
; @param    rf      32-bit: [ebp + 8]   64-bit: xmm0
RT_NOCRT_BEGINPROC fabsf
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
        andps   xmm0, [g_r32ClearSignMask xWrtRIP]
%else
        fld     dword [xBP + xCB*2]     ; This turns SNaN into QNaN.
        fabs
%endif

        leave
        ret
ENDPROC   RT_NOCRT(fabsf)

ALIGNCODE(16)
g_r32ClearSignMask:
        dd      07fffffffh
        dd      07fffffffh
        dd      07fffffffh
        dd      07fffffffh

