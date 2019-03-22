; $Id$
;; @file
; Bootsector that benchmarks I/O and MMIO roundtrip time.
;   VBoxManage setextradata bs-test1 VBoxInternal/Devices/VMMDev/0/Config/TestingEnabled  1
;   VBoxManage setextradata bs-test1 VBoxInternal/Devices/VMMDev/0/Config/TestingMMIO  1
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
%include "iprt/x86.mac"
%include "VBox/VMMDevTesting.mac"

;; The number of instructions to test.
%define TEST_INSTRUCTION_COUNT_IO       2000000

;; The number of RDTSC instructions to test.
%define TEST_INSTRUCTION_COUNT_RDTSC    4000000

;; The number of RDTSC instructions to test.
%define TEST_INSTRUCTION_COUNT_READCR4  1000000

;; The number of instructions to test.
%define TEST_INSTRUCTION_COUNT_MMIO     750000

;; Define this to drop unnecessary test variations.
%define QUICK_TEST

;
; Include and execute the init code.
;
        %define BS2_INIT_RM
        %define BS2_INC_PE16
        %define BS2_INC_PE32
        %define BS2_INC_PP32
        %define BS2_INC_PAE32
        %define BS2_INC_LM64
        %include "bootsector2-common-init-code.mac"


;
; The benchmark driver
;
BEGINPROC main
        ;
        ; Test prologue.
        ;
        mov     ax, .s_szTstName
        call    TestInit_r86
        call    Bs2EnableA20_r86
        call    Bs2PanicIfVMMDevTestingIsMissing_r86

        ;
        ; CPUID.
        ;
        mov     ax, .s_szTstCpuId
        call    TestSub_r86
        call    BenchmarkCpuId_rm_pp32
        call    BenchmarkCpuId_rm_pae32
        call    BenchmarkCpuId_rm_lm64
        call    BenchmarkCpuId_rm_pe16
        call    BenchmarkCpuId_rm_pe32
        call    BenchmarkCpuId_rm_rm

        ;
        ; RDTSC.
        ;
        mov     ax, .s_szTstRdTsc
        call    TestSub_r86
        call    BenchmarkRdTsc_rm_pp32
        call    BenchmarkRdTsc_rm_pae32
        call    BenchmarkRdTsc_rm_lm64
        call    BenchmarkRdTsc_rm_pe16
        call    BenchmarkRdTsc_rm_pe32
        call    BenchmarkRdTsc_rm_rm

        ;
        ; Read CR4
        ;
        mov     ax, .s_szTstRdCr4
        call    TestSub_r86
        call    BenchmarkRdCr4_rm_pp32
        call    BenchmarkRdCr4_rm_pae32
        call    BenchmarkRdCr4_rm_lm64
        call    BenchmarkRdCr4_rm_pe16
        call    BenchmarkRdCr4_rm_pe32
        call    BenchmarkRdCr4_rm_rm

        ;
        ; I/O port access.
        ;
        mov     ax, .s_szTstNopIoPort
        call    TestSub_r86
        call    BenchmarkIoPortNop_rm_rm
        call    BenchmarkIoPortNop_rm_pe16
        call    BenchmarkIoPortNop_rm_pe32
        call    BenchmarkIoPortNop_rm_pp32
        call    BenchmarkIoPortNop_rm_pae32
        call    BenchmarkIoPortNop_rm_lm64

        ;
        ; MMIO access.
        ;
        mov     ax, .s_szTstNopMmio
        call    TestSub_r86
        call    BenchmarkMmioNop_rm_pp32
        call    BenchmarkMmioNop_rm_pae32
        call    BenchmarkMmioNop_rm_lm64
        call    BenchmarkMmioNop_rm_pe16
        call    BenchmarkMmioNop_rm_pe32
        call    BenchmarkMmioNop_rm_rm

        ;
        ; We're done.
        ;
        call    TestTerm_r86
        call    Bs2Panic

.s_szTstName:
        db      'tstIOIntr', 0
.s_szTstCpuId:
        db      'CPUID EAX=1', 0
.s_szTstRdTsc:
        db      'RDTSC', 0
.s_szTstRdCr4:
        db      'Read CR4', 0
.s_szTstNopIoPort:
        db      'NOP I/O Port Access', 0
.s_szTstNopMmio:
        db      'NOP MMIO Access', 0
ENDPROC   main


;
; Instantiate the template code.
;
%include "bootsector2-template-footer.mac"  ; reset the initial environemnt.

%define TMPL_RM
%include "bootsector2-test1-template.mac"
;%define TMPL_CMN_V86
;%include "bootsector2-test1-template.mac"
%define TMPL_PE16
%include "bootsector2-test1-template.mac"
%define TMPL_PE32
%include "bootsector2-test1-template.mac"
;%define TMPL_PP16
;%include "bootsector2-test1-template.mac"
%define TMPL_PP32
%include "bootsector2-test1-template.mac"
;%define TMPL_PAE16
;%include "bootsector2-test1-template.mac"
%define TMPL_PAE32
%include "bootsector2-test1-template.mac"
;%define TMPL_LM16
;%include "bootsector2-test1-template.mac"
;%define TMPL_LM32
;%include "bootsector2-test1-template.mac"
%define TMPL_LM64
%include "bootsector2-test1-template.mac"


;
; End sections and image.
;
%include "bootsector2-common-end.mac"

