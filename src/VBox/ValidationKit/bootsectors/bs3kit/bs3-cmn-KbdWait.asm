; $Id$
;; @file
; BS3Kit - Bs3KbdWait.
;

;
; Copyright (C) 2007-2024 Oracle and/or its affiliates.
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

%include "bs3kit-template-header.mac"

;;
; Waits for the keyboard controller to become ready.
;
; @cproto   BS3_DECL(void) Bs3KbdWait_c16(void);
;
BS3_PROC_BEGIN_CMN Bs3KbdWait, BS3_PBC_HYBRID_0_ARGS
        push    xBP
        mov     xBP, xSP
        push    xAX

.check_status:
        in      al, 64h
        test    al, 1                   ; KBD_STAT_OBF
        jnz     .read_data_and_status
        test    al, 2                   ; KBD_STAT_IBF
        jnz     .check_status

        pop     xAX
        pop     xBP
        BS3_HYBRID_RET

.read_data_and_status:
        in      al, 60h
        jmp     .check_status
BS3_PROC_END_CMN   Bs3KbdWait

