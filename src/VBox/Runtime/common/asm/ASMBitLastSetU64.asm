; $Id$
;; @file
; IPRT - ASMBitLastSetU64().
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
; Finds the last bit which is set in the given 64-bit integer.
;
; Bits are numbered from 1 (least significant) to 64.
;
; @returns (xAX)        index [1..64] of the last set bit.
; @returns (xAX)        0 if all bits are cleared.
; @param   msc:rcx gcc:rdi x86:stack u64  Integer to search for set bits.
;
; @cproto DECLASM(unsigned) ASMBitLastSetU64(uint64_t u64);
;
BEGINPROC_EXPORTED ASMBitLastSetU64
%if ARCH_BITS == 16
        CPU     8086
        push    bp
        mov     bp, sp

        ; 63:48
        mov     ax, 64
        mov     cx, [bp + 2 + 2 + 6]
        test    cx, cx
        jnz     .next_bit

        ; 47:32
        mov     al, 48
        or      cx, [bp + 2 + 2 + 4]
        jnz     .next_bit

        ; 31:16
        mov     al, 32
        or      cx, [bp + 2 + 2 + 2]
        jnz     .next_bit

        ; 16:0
        mov     al, 16
        or      cx, [bp + 2 + 2 + 0]
        jz      .return_zero

        ; find the bit that was set.
.next_bit:
        shl     cx, 1
        jc      .return
        dec     ax
        jmp     .next_bit

.return_zero:
        xor     ax, ax
.return:
        pop     bp
        ret

%else
 %if    ARCH_BITS == 64
  %ifdef ASM_CALL64_GCC
        bsr     rax, rsi
  %else
        bsr     rax, rcx
  %endif
        jz      .return_zero
        inc     eax
.return:
        ret

 %elif ARCH_BITS == 32
        ; Check the 2nd dword then the first one.
        bsr     eax, dword [esp + 4 + 4]
        jz      .check_1st_dword
        add     eax, 32
        ret

.check_1st_dword:
        bsr     eax, dword [esp + 4 + 0]
        jz      .return_zero
        inc     eax
        ret

 %else
  %error "Missing or invalid ARCH_BITS."
 %endif

.return_zero:
        xor     eax, eax
        ret
%endif
ENDPROC ASMBitLastSetU64

