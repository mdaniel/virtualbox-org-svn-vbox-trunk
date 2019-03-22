; $Id$
;; @file
; IPRT - RTDbgStackDumpSelf assembly wrapper calling rtDbgStackDumpSelfWorker.
;

;
; Copyright (C) 2006-2019 Oracle Corporation
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


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"


;*********************************************************************************************************************************
;*  Extern Symbols                                                                                                               *
;*********************************************************************************************************************************
extern NAME(rtDbgStackDumpSelfWorker)


BEGINCODE

;;
; Collects register state and calls C worker.
;
BEGINPROC_EXPORTED RTDbgStackDumpSelf
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0

        ;
        ; Push all GPRS in reserve order...
        ;
%ifdef RT_ARCH_AMD64
        push    r15
        SEH64_PUSH_GREG r15
        push    r14
        SEH64_PUSH_GREG r14
        push    r13
        SEH64_PUSH_GREG r13
        push    r12
        SEH64_PUSH_GREG r12
        push    r13
        SEH64_PUSH_GREG r13
        push    r12
        SEH64_PUSH_GREG r12
        push    r11
        SEH64_PUSH_GREG r11
        push    r10
        SEH64_PUSH_GREG r10
        push    r9
        SEH64_PUSH_GREG r9
        push    r8
        SEH64_PUSH_GREG r8
%endif
        push    xDI
        SEH64_PUSH_GREG xDI
        push    xSI
        SEH64_PUSH_GREG xSI
%ifdef RT_ARCH_AMD64
        mov     r10, [xBP]                  ; Caller RBP.
        push    r10
        lea     r10, [xBP + xCB * 2]        ; Caller RSP.
        push    r10
%else
        push    dword [xBP]                 ; Caller EBP
        push    xAX
        lea     xAX, [xBP + xCB * 2]        ; Caller ESP.
        xchg    xAX, [xSP]
%endif
        push    xBX
        SEH64_PUSH_GREG xBX
        push    xDX
        SEH64_PUSH_GREG xDX
        push    xCX
        SEH64_PUSH_GREG xCX
        push    xAX
        SEH64_PUSH_GREG xAX

        ;
        ; ... preceeded by the EIP/RIP.
        ;
%ifdef RT_ARCH_AMD64
        mov     r10, [xBP + xCB]
        push    r10
%else
        push    dword [xBP + xCB]
%endif

        ;
        ; Call the worker function passing the register array as the fourth argument.
        ;
%ifdef ASM_CALL64_GCC
        mov     rcx, rsp                    ; 4th parameter = register array.
        sub     rsp, 8h                     ; Align stack.
        SEH64_ALLOCATE_STACK 28h
%elifdef ASM_CALL64_MSC
        mov     r9, rsp                     ; 4th parameter = register array.
        sub     rsp, 28h                    ; Aling stack and allocate spill area.
        SEH64_ALLOCATE_STACK 28h
%else
        mov     eax, esp                    ; Save register array location
        and     xSP, ~15                    ; Align the stack.
%endif
SEH64_END_PROLOGUE

%ifndef RT_ARCH_AMD64
        push    eax
        push    dword [xBP + xCB * 4]
        push    dword [xBP + xCB * 3]
        push    dword [xBP + xCB * 2]
%endif
%ifdef ASM_FORMAT_ELF
 %ifdef PIC
        call    NAME(rtDbgStackDumpSelfWorker) wrt ..plt
 %else
        call    NAME(rtDbgStackDumpSelfWorker)
 %endif
%else
        call    NAME(rtDbgStackDumpSelfWorker)
%endif

        leave
        ret
ENDPROC RTDbgStackDumpSelf

