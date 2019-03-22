; $Id$
;; @file
; BS3Kit - Bs3PagingGetRootForLM64
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


%ifdef TMPL_RM
extern TMPL_NM(Bs3SwitchToPE16)
extern NAME(Bs3SwitchToRM_pe16)
%elifdef TMPL_CMN_V86
extern TMPL_NM(Bs3SwitchToRing0)
extern TMPL_NM(Bs3SwitchTo16BitV86)
%endif

BS3_EXTERN_CMN Bs3PagingInitRootForLM

BS3_EXTERN_DATA16 g_PhysPagingRootLM
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_DECL(uint32_t) Bs3PagingGetRootForLM64(void)
;
; @returns  eax
;
; @uses     ax
;
; @remarks  returns value in EAX, not dx:ax!
;
BS3_PROC_BEGIN_MODE Bs3PagingGetRootForLM64, BS3_PBC_NEAR ; Internal function, no far variant necessary.
        mov     eax, [BS3_DATA16_WRT(g_PhysPagingRootLM)]
        cmp     eax, 0ffffffffh
        je      .init_root
%ifdef BS3_STRICT
.return:
        cmp     eax, 1000h
        jnb     .cr3_ok_low
        hlt
.cr3_ok_low:
        cmp     eax, 16*_1M
        jb     .cr3_ok_high
        hlt
.cr3_ok_high:
%endif
        ret

.init_root:
        push    xBP
        mov     xBP, xSP
BONLY16 push    es
        push    sDX
        push    sCX
        push    sBX
%if TMPL_BITS == 64
        push    r8
        push    r9
        push    r10
        push    r11
%endif

%ifdef TMPL_RM
        ;
        ; We don't want to be restricted to real mode addressing, so
        ; temporarily switch to 16-bit protected mode.
        ;
        call    TMPL_NM(Bs3SwitchToPE16)
        call    Bs3PagingInitRootForLM
        call    NAME(Bs3SwitchToRM_pe16)
%elifdef TMPL_CMN_V86
        ;
        ; V8086 mode uses real mode addressing too.  Unlikly that we'll
        ; ever end up here though.
        ;
        call    TMPL_NM(Bs3SwitchToRing0)
        call    Bs3PagingInitRootForLM
        call    TMPL_NM(Bs3SwitchTo16BitV86)
%else
        ;
        ; Not a problematic addressing mode.
        ;
BONLY64 sub     rsp, 20h
        BS3_CALL Bs3PagingInitRootForLM, 0
BONLY64 add     rsp, 20h
%endif

        ;
        ; Load the value and return.
        ;
        mov     eax, [BS3_DATA16_WRT(g_PhysPagingRootLM)]

%if TMPL_BITS == 64
        pop     r11
        pop     r10
        pop     r9
        pop     r8
%endif
        pop     sBX
        pop     sCX
        pop     sDX
BONLY16 pop     es
        leave
%ifdef BS3_STRICT
        jmp     .return
%else
        ret
%endif
BS3_PROC_END_MODE   Bs3PagingGetRootForLM64

