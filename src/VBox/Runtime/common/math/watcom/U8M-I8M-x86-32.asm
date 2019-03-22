; $Id$
;; @file
; BS3Kit - 32-bit Watcom C/C++, 64-bit unsigned integer division.
;

;
; Copyright (C) 2007-2019 Oracle Corporation
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
; 64-bit signed & unsigned integer multiplication.
;
; @returns  EDX:EAX product
; @param    EDX:EAX     Factor #1.
; @param    ECX:EBX     Factor #2.
; @uses     ECX, EBX
;
global __U8M
__U8M:
global __I8M
__I8M:
        ;
        ; See if this is a pure 32-bit multiplication.  We might get lucky.
        ;
        test    edx, edx
        jnz     .complicated
        test    ecx, ecx
        jnz     .complicated

        mul     ebx                     ; eax * ebx -> edx:eax
        ret

.complicated:
        push    eax
        push    edx

        ; ecx = F1.lo * F2.hi  (edx contains overflow here can be ignored)
        mul     ecx
        mov     ecx, eax

        ; ecx += F1.hi * F2.lo (edx can be ignored again)
        pop     eax
        mul     ebx
        add     ecx, eax

        ; edx:eax = F1.lo * F2.lo
        pop     eax
        mul     ebx

        ; Add ecx to the high part (edx).
        add     edx, ecx

        ret

