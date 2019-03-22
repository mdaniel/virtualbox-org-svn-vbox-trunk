; $Id$
;; @file
; BS3Kit - Bs3A20Enable.
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

%include "bs3kit-template-header.mac"


BS3_EXTERN_CMN Bs3KbdWait
BS3_EXTERN_CMN Bs3KbdRead
BS3_EXTERN_CMN Bs3KbdWrite


;;
; Enables the A20 gate.
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20Enable, BS3_PBC_HYBRID_0_ARGS
        push    xBP
        mov     xBP, xSP
BONLY64 sub     rsp, 20h

        call    BS3_CMN_NM(Bs3A20EnableViaPortA)
;; @todo real 286 support
;        call    BS3_CMN_NM(Bs3A20EnableViaKbd)

        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3A20Enable


;;
; Enables the A20 gate via the keyboard controller.
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20EnableViaKbd, BS3_PBC_HYBRID_0_ARGS
        push    xBP
        mov     xBP, xSP
        push    xAX
        pushf
        cli
BONLY64 sub     rsp, 20h

        call    Bs3KbdWait
        push    0d0h                    ; KBD_CCMD_READ_OUTPORT
        call    Bs3KbdRead

        or      al, 002h
        push    xAX
        push    0d1h                    ; KBD_CCMD_WRITE_OUTPORT
        call    Bs3KbdWrite

        add     xSP, xCB*3              ; both the above calls

        mov     al, 0ffh                ; KBD_CMD_RESET
        out     64h, al
        call    Bs3KbdWait

BONLY64 add     rsp, 20h
        popf
        pop     xAX
        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3A20EnableViaKbd


;;
; Enables the A20 gate via control port A (PS/2 style).
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20EnableViaPortA, BS3_PBC_HYBRID_0_ARGS
        push    xBP
        mov     xBP, xSP
        push    xAX

        ; Use Control port A, assuming a PS/2 style system.
        in      al, 092h
        test    al, 02h
        jnz     .done                   ; avoid trouble writing back the same value.
        or      al, 2                   ; enable the A20 gate.
        out     092h, al

.done:
        pop     xAX
        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3A20EnableViaPortA

