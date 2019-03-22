; $Id$
;; @file
; VirtualBox Support Driver - Windows NT specific assembly parts.
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

%ifdef VBOX_WITH_HARDENING

 %ifdef RT_ARCH_X86
;
; Faking up ZwQueryVirtualMemory on XP and W2K3 where it's not exported.
; Using ZwOpenFile as a helper as it has the name number of parameters.
;
extern  IMPNAME(ZwOpenFile@24)

BEGINPROC supdrvNtQueryVirtualMemory_Xxx
  %macro NtQueryVirtualMemorySyscall 1
  GLOBALNAME supdrvNtQueryVirtualMemory_ %+ %1
        mov     eax, %1
        jmp     supdrvNtQueryVirtualMemory_Jump
  %endm
    NtQueryVirtualMemorySyscall 0xAF
    NtQueryVirtualMemorySyscall 0xB0
    NtQueryVirtualMemorySyscall 0xB1
    NtQueryVirtualMemorySyscall 0xB2
    NtQueryVirtualMemorySyscall 0xB3
    NtQueryVirtualMemorySyscall 0xB4
    NtQueryVirtualMemorySyscall 0xB5
    NtQueryVirtualMemorySyscall 0xB6
    NtQueryVirtualMemorySyscall 0xB7
    NtQueryVirtualMemorySyscall 0xB8
    NtQueryVirtualMemorySyscall 0xB9
    NtQueryVirtualMemorySyscall 0xBA
    NtQueryVirtualMemorySyscall 0xBB
    NtQueryVirtualMemorySyscall 0xBC
    NtQueryVirtualMemorySyscall 0xBD
    NtQueryVirtualMemorySyscall 0xBE

supdrvNtQueryVirtualMemory_Jump:
        mov     edx, IMP2(ZwOpenFile@24)
        lea     edx, [edx + 5]
        jmp     edx
ENDPROC   supdrvNtQueryVirtualMemory_Xxx

 %endif

 %ifdef RT_ARCH_AMD64
;
; Faking up ZwQueryVirtualMemory on XP64 and W2K3-64 where it's not exported.
; The C code locates and verifies the essentials in ZwRequestWaitReplyPort.
;
extern NAME(g_pfnKiServiceLinkage)
extern NAME(g_pfnKiServiceInternal)
BEGINPROC supdrvNtQueryVirtualMemory_Xxx
  %macro NtQueryVirtualMemorySyscall 1
  GLOBALNAME supdrvNtQueryVirtualMemory_ %+ %1
        mov     eax, %1
        jmp     supdrvNtQueryVirtualMemory_Jump
  %endm

    NtQueryVirtualMemorySyscall 0x1F
    NtQueryVirtualMemorySyscall 0x20
    NtQueryVirtualMemorySyscall 0x21
    NtQueryVirtualMemorySyscall 0x22
    NtQueryVirtualMemorySyscall 0x23

supdrvNtQueryVirtualMemory_Jump:
        cli
        mov     r10, rsp                ; save call frame pointer.
        mov     r11, [NAME(g_pfnKiServiceLinkage) wrt rip]
        push    0
        push    0
        push    r10                     ; call frame pointer (incoming rsp).
        pushfq
        push    10h
        push    r11                     ; r11 = KiServiceLinkage (ret w/ unwind info)
        jmp     qword [NAME(g_pfnKiServiceInternal) wrt rip]
ENDPROC   supdrvNtQueryVirtualMemory_Xxx
 %endif

%endif ; VBOX_WITH_HARDENING

