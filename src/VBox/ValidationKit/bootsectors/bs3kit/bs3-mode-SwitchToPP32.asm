; $Id$
;; @file
; BS3Kit - Bs3SwitchToPP32
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


;;
; Switch to 32-bit paged protected mode from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToPE32(void);
;
; @uses     Nothing (except high 32-bit register parts), upper part of ESP is
;           cleared if caller is in 16-bit mode.
;
; @remarks  Obviously returns to 32-bit mode, even if the caller was
;           in 16-bit or 64-bit mode.  It doesn't not preserve the callers
;           ring, but instead changes to ring-0.
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_GLOBAL_NAME_EX TMPL_NM(Bs3SwitchToPP32_Safe), function, 0
BS3_PROC_BEGIN_MODE Bs3SwitchToPP32, BS3_PBC_NEAR
%ifdef TMPL_PP32
        ret

%elif BS3_MODE_IS_V86(TMPL_MODE)
        ;
        ; V8086 - Switch to 16-bit ring-0 and call worker for that mode.
        ;
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)
        extern %[BS3_MODE_R0_NM_ %+ TMPL_MODE](Bs3SwitchToPP32)
        jmp    %[BS3_MODE_R0_NM_ %+ TMPL_MODE](Bs3SwitchToPP32)

%else
        ;
        ; Switch to real mode.
        ;
 %if TMPL_BITS != 32
  %if TMPL_BITS > 32
        shl     xPRE [xSP], 32          ; Adjust the return address from 64-bit to 32-bit.
        add     rsp, xCB - 4
  %else
        push    word 0                  ; Reserve space to expand the return address.
  %endif
 %endif
 %if TMPL_BITS != 16
        ; Must be in 16-bit segment when calling Bs3SwitchTo16Bit.
        jmp     .sixteen_bit_segment
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
BS3_GLOBAL_LOCAL_LABEL .sixteen_bit_segment
 %endif

        ;
        ; Switch to real mode.
        ;
        extern  TMPL_NM(Bs3SwitchToRM)
        call    TMPL_NM(Bs3SwitchToRM)
        BS3_SET_BITS 16

        push    eax
        push    ecx
        pushfd

        ;
        ; Make sure PAE is really off and that PSE is on when supported.
        ;
BS3_EXTERN_DATA16 g_uBs3CpuDetected
BS3_BEGIN_TEXT16
        test    byte [1 + BS3_DATA16_WRT(g_uBs3CpuDetected)], (BS3CPU_F_CPUID >> 8)
        jz      .cr4_is_fine
        mov     eax, cr4
        mov     ecx, eax
        and     eax, ~(X86_CR4_PAE | X86_CR4_PSE)
        test    byte [1 + BS3_DATA16_WRT(g_uBs3CpuDetected)], (BS3CPU_F_PSE >> 8)
        jz      .no_pse
        or      eax, X86_CR4_PSE
.no_pse:
        cmp     eax, ecx
        je      .cr4_is_fine
        mov     cr4, eax
.cr4_is_fine:

        ;
        ; Get the page directory (returned in eax).
        ; Will lazy init page tables (in 16-bit prot mode).
        ;
        extern NAME(Bs3PagingGetRootForPP32_rm)
        call   NAME(Bs3PagingGetRootForPP32_rm)

        cli
        mov     cr3, eax

        ;
        ; Load the GDT and enable PE32.
        ;
BS3_EXTERN_SYSTEM16 Bs3LgdtDef_Gdt
BS3_EXTERN_SYSTEM16 Bs3Lgdt_Gdt
BS3_BEGIN_TEXT16
        mov     ax, BS3SYSTEM16
        mov     ds, ax
        lgdt    [Bs3LgdtDef_Gdt]        ; Will only load 24-bit base!

        mov     eax, cr0
        or      eax, X86_CR0_PE | X86_CR0_PG
        mov     cr0, eax
        jmp     BS3_SEL_R0_CS32:dword .thirty_two_bit wrt FLAT
BS3_BEGIN_TEXT32
BS3_GLOBAL_LOCAL_LABEL .thirty_two_bit
        ;
        ; Convert the (now) real mode stack pointer to 32-bit flat.
        ;
        xor     eax, eax
        mov     ax, ss
        shl     eax, 4
        and     esp, 0ffffh
        add     esp, eax

        mov     ax, BS3_SEL_R0_SS32
        mov     ss, ax

        ;
        ; Call rountine for doing mode specific setups.
        ;
        extern  NAME(Bs3EnteredMode_pp32)
        call    NAME(Bs3EnteredMode_pp32)

        ; Load full 32-bit GDT base address.
        lgdt    [Bs3Lgdt_Gdt wrt FLAT]

        ;
        ; Restore ecx, eax and flags (IF).
        ;
 %if TMPL_BITS < 32
        movzx   eax, word [esp + 12 + 2] ; Load return address.
        add     eax, BS3_ADDR_BS3TEXT16  ; Convert it to a flat address.
        mov     [esp + 12], eax          ; Store it in the place right for 32-bit returns.
 %endif
        popfd
        pop     ecx
        pop     eax
        ret

 %if TMPL_BITS != 32
TMPL_BEGIN_TEXT
 %endif
%endif
BS3_PROC_END_MODE   Bs3SwitchToPP32


%if TMPL_BITS == 16
;;
; Custom far stub.
BS3_BEGIN_TEXT16_FARSTUBS
BS3_PROC_BEGIN_MODE Bs3SwitchToPP32, BS3_PBC_FAR
        inc         bp
        push        bp
        mov         bp, sp

        ; Call the real thing.
        call        TMPL_NM(Bs3SwitchToPP32)
        BS3_SET_BITS 32

        ; Jmp to common code for the tedious conversion.
 %if BS3_MODE_IS_RM_OR_V86(TMPL_MODE)
        extern      _Bs3SwitchHlpConvRealModeRetfPopBpDecBpAndReturn_c32
        jmp         _Bs3SwitchHlpConvRealModeRetfPopBpDecBpAndReturn_c32
 %else
        extern      _Bs3SwitchHlpConvProtModeRetfPopBpDecBpAndReturn_c32
        jmp         _Bs3SwitchHlpConvProtModeRetfPopBpDecBpAndReturn_c32
 %endif
        BS3_SET_BITS 16
BS3_PROC_END_MODE   Bs3SwitchToPP32
%endif

