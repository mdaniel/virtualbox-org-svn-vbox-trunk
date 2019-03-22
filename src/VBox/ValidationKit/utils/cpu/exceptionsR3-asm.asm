; $Id$
;; @file
; exceptionsR3-asm.asm - assembly helpers.
;

;
; Copyright (C) 2009-2017 Oracle Corporation
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


;*******************************************************************************
;*      Defined Constants And Macros                                           *
;*******************************************************************************
%ifdef RT_ARCH_AMD64
 %define TST_XCPT_MAGIC  0123456789abcdef0h
%else
 %define TST_XCPT_MAGIC  012345678h
%endif

%macro tstXcptAsmProlog 0
        push    xBP
        push    xDI
        push    xSI
        push    xBX
 %ifdef RT_ARCH_X86
        push    gs
        push    fs
        push    es
        push    ds
 %endif
 %ifdef RT_ARCH_AMD64
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
 %endif

        mov     xAX, TST_XCPT_MAGIC
        mov     xBX, xAX
        mov     xCX, xAX
        mov     xDX, xAX
        mov     xDI, xAX
        mov     xSI, xAX
        mov     xBP, xAX
 %ifdef RT_ARCH_AMD64
        mov     r8,  xAX
        mov     r9,  xAX
        mov     r10, xAX
        mov     r11, xAX
        mov     r12, xAX
        mov     r13, xAX
        mov     r14, xAX
        mov     r15, xAX
 %endif
%endmacro

%macro tstXcptAsmEpilog 0
 %ifdef RT_ARCH_AMD64
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
 %endif
 %ifdef RT_ARCH_X86
        pop     ds
        pop     es
        pop     fs
        pop     gs
 %endif
        pop     xBX
        pop     xSI
        pop     xDI
        pop     xBP
%endmacro


BEGINCODE

;;
BEGINPROC tstXcptAsmNullPtrRead
;        tstXcptAsmProlog
        xor     eax, eax
GLOBALNAME tstXcptAsmNullPtrRead_PC
        mov     al, [xAX]
;        tstXcptAsmEpilog
        ret
ENDPROC   tstXcptAsmNullPtrRead


;;
BEGINPROC tstXcptAsmNullPtrWrite
        tstXcptAsmProlog
        xor     eax, eax
GLOBALNAME tstXcptAsmNullPtrWrite_PC
        mov     [xAX], al
        tstXcptAsmEpilog
        ret
ENDPROC   tstXcptAsmNullPtrWrite


;;
BEGINPROC tstXcptAsmSysCall
        tstXcptAsmProlog
GLOBALNAME tstXcptAsmSysCall_PC
        syscall
        tstXcptAsmEpilog
        ret
ENDPROC   tstXcptAsmSysCall


;;
BEGINPROC tstXcptAsmSysEnter
        tstXcptAsmProlog
GLOBALNAME tstXcptAsmSysEnter_PC
%ifdef RT_ARCH_AMD64
        db 00fh, 034h                   ; test this on 64-bit, yasm complains...
%else
        sysenter
%endif
        tstXcptAsmEpilog
        ret
ENDPROC   tstXcptAsmSysEnter

