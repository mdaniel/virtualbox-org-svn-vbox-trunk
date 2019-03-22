; $Id$
;; @file
; BS3Kit - 16-bit Watcom C/C++, 32-bit unsigned integer division.
;

;
; Copyright (C) 2007-2017 Oracle Corporation
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

%include "bs3kit-template-header.mac"



;;
; 32-bit unsigned integer division.
;
; @returns  DX:AX quotient, CX:BX remainder.
; @param    DX:AX           Dividend.
; @param    CX:BX           Divisor
;
; @uses     Nothing.
;
%ifdef BS3KIT_WITH_REAL_WATCOM_INTRINSIC_NAMES
global __U4D
__U4D:
%endif
global $_?U4D
$_?U4D:
%if TMPL_BITS >= 32
        ; Move dividend into EDX:EAX
        shl     eax, 10h
        mov     ax, dx
        xor     edx, edx

        ; Move divisor into ebx.
        shl     ebx, 10h
        mov     bx, cx

        ; Do it!
        div     ebx

        ; Reminder in to CX:BX
        mov     bx, dx
        shr     edx, 10h
        mov     cx, dx

        ; Quotient into DX:AX
        mov     edx, eax
        shr     edx, 10h
%else
        push    ds
        push    es

        ;
        ; Convert to a C __cdecl call - too lazy to do this in assembly.
        ;

        ; Set up a frame of sorts, allocating 8 bytes for the result buffer.
        push    bp
        sub     sp, 08h
        mov     bp, sp

        ; Pointer to the return buffer.
        push    ss
        push    bp
        add     bp, 08h                 ; Correct bp.

        ; The divisor.
        push    cx
        push    bx

        ; The dividend.
        push    dx
        push    ax

        BS3_EXTERN_CMN Bs3UInt32Div
        call    Bs3UInt32Div

        ; Load the reminder.
        mov     cx, [bp - 08h + 6]
        mov     bx, [bp - 08h + 4]
        ; Load the quotient.
        mov     dx, [bp - 08h + 2]
        mov     ax, [bp - 08h]

        mov     sp, bp
        pop     bp
        pop     es
        pop     ds

%endif
%ifdef ASM_MODEL_FAR_CODE
        retf
%else
        ret
%endif

