; $Id$
;; @file
; IPRT - ASMAtomicCmpXchgU8().
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

;;
; Atomically compares and exchanges an unsigned 8-bit int.
;
; @param    pu8     x86:esp+4  msc:rcx  gcc:rdi
; @param    u8New   x86:esp+8  msc:dl   gcc:sil
; @param    u8Old   x86:esp+c  msc:r8l  gcc:dl
;
; @returns  bool result: true if successfully exchanged, false if not.
;           x86:al
;
BEGINPROC_EXPORTED ASMAtomicCmpXchgU8
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        mov     al, r8b
        lock cmpxchg [rcx], dl
 %else
        mov     al, dl
        lock cmpxchg [rdi], sil
 %endif
%else
        mov     ecx, [esp + 04h]
        mov     dl,  [esp + 08h]
        mov     al,  [esp + 0ch]
        lock cmpxchg [ecx], dl
%endif
        setz    al
        movzx   eax, al
        ret
ENDPROC ASMAtomicCmpXchgU8

