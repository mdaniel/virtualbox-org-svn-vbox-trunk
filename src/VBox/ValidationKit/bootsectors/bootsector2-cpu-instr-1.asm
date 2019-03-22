; $Id$
;; @file
; Bootsector test for misc instructions.
;
; Recommended (but not necessary):
;   VBoxManage setextradata bs-cpu-instr-1 VBoxInternal/Devices/VMMDev/0/Config/TestingEnabled  1
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


;*******************************************************************************
;*      Header Files                                                           *
;*******************************************************************************
%include "iprt/asmdefs.mac"
%include "iprt/x86.mac"
%include "VBox/VMMDevTesting.mac"

; Include and execute the init code.
%define BS2_INIT_RM
%define BS2_WITH_TRAPS
%define BS2_INC_RM
%define BS2_INC_PE32
%define BS2_INC_PP32
%define BS2_INC_PAE32
%define BS2_INC_LM32
%define BS2_INC_LM64
%define BS2_WITH_TRAPRECS
%include "bootsector2-common-init-code.mac"


;
; The main() function.
;
BEGINPROC main
        BITS 16
        ;
        ; Test prologue.
        ;
        mov     ax, .s_szTstName
        call    TestInit_r86
        call    Bs2EnableA20_r86


        ;
        ; Execute the tests
        ;
%if 1
        call    NAME(DoTestsForMode_rm_pe32)
%endif
%if 1
        call    NAME(DoTestsForMode_rm_pp32)
%endif
%if 1
        call    NAME(DoTestsForMode_rm_pae32)
%endif
%if 1
        call    NAME(DoTestsForMode_rm_lm64)
%endif

        ;
        ; We're done.
        ;
        call    TestTerm_r86
        ret

.s_szTstName:
        db      'tstCpuInstr1', 0
ENDPROC   main


;
; Instantiate the template code.
;
%include "bootsector2-template-footer.mac"  ; reset the initial environemnt.

%define TMPL_PE32
%include "bootsector2-cpu-instr-1-template.mac"
%define TMPL_PP32
%include "bootsector2-cpu-instr-1-template.mac"
%define TMPL_PAE32
%include "bootsector2-cpu-instr-1-template.mac"
%define TMPL_LM64
%include "bootsector2-cpu-instr-1-template.mac"


;
; End sections and image.
;
%include "bootsector2-common-end.mac"

