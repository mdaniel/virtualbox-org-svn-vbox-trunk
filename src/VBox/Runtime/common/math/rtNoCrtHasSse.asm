; $Id$
;; @file
; IPRT - No-CRT rtNoCrtHasSse - X86.
;

;
; Copyright (C) 2022-2024 Oracle and/or its affiliates.
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


%include "iprt/asmdefs.mac"
%include "iprt/x86.mac"


BEGINDATA
g_frtNoCrtHasSse:   db  0x80


BEGINCODE

;;
; Checks if SSE is supported.
; @returns  1 if supported, 0 if not.  Entire eax/rax is set.
; @uses     rax only
;
BEGINPROC rtNoCrtHasSse
        mov     al, [g_frtNoCrtHasSse]
        test    al, 0x80
        jnz     .detect_sse
        ret

.detect_sse:
        push    ebx
        push    ecx
        push    edx

        mov     eax, 1
        cpuid

        mov     eax, 1
        test    edx, X86_CPUID_FEATURE_EDX_SSE
        jz      .no_supported
        xor     eax, eax
.no_supported:

        pop     edx
        pop     ecx
        pop     ebx
        ret
ENDPROC   rtNoCrtHasSse

