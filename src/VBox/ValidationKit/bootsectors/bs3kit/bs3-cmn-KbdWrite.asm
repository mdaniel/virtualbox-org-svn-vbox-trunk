; $Id$
;; @file
; BS3Kit - Bs3KbdRead.
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


BS3_EXTERN_CMN Bs3KbdWait


;;
; Sends a write command to the keyboard controller and then sends the data.
;
; The caller is responsible for making sure the keyboard controller is ready
; for a command (call Bs3KbdWait if unsure).
;
; @param        bCmd        The write command.
; @param        bData       The data to write.
; @uses         Nothing.
;
; @todo         Return status?
;
; @cproto   BS3_DECL(void) Bs3KbdWait_c16(uint8_t bCmd, uint8_t bData);
;
BS3_PROC_BEGIN_CMN Bs3KbdWrite, BS3_PBC_NEAR
        push    xBP
        mov     xBP, xSP
        push    xAX
BONLY64 sub     rsp, 20h

        mov     al, [xBP + xCB*2]
        out     64h, al                 ; Write the command.
        call    Bs3KbdWait

        mov     al, [xBP + xCB*3]
        out     60h, al                 ; Write the data
        call    Bs3KbdWait

BONLY64 add     rsp, 20h
        pop     xAX
        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3KbdWrite

;
; We may be using the near code in some critical code paths, so don't
; penalize it.
;
BS3_CMN_FAR_STUB   Bs3KbdWrite, 4

