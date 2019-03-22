; $Id$
;; @file
; BS3Kit - First Object, calling 16-bit protected-mode main().
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


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************

;
; Segment defs, grouping and related variables.
; Defines the entry point 'start' as well, leaving us in BS3TEXT16.
;
%include "bs3-first-common.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_BEGIN_DATA16

BS3_BEGIN_RMTEXT16
extern _Bs3CpuDetect_rm_far
extern _Bs3InitMemory_rm_far

BS3_BEGIN_TEXT16
BS3_EXTERN_CMN Bs3PicMaskAll
BS3_EXTERN_CMN Bs3Trap16Init
extern _Bs3SwitchToPE16_rm
extern _Main_pe16
BS3_EXTERN_CMN Bs3Shutdown


BS3_BEGIN_TEXT16
    ;
    ; Zero return address and zero caller BP.
    ;
    xor     ax, ax
    push    ax
    push    ax
    mov     bp, sp

    ;
    ; Load DS and ES with data selectors.
    ;
    mov     ax, BS3KIT_GRPNM_DATA16
    mov     ds, ax
    mov     es, ax


    ;
    ; Make sure interrupts are disabled as we cannot (don't want to) service
    ; BIOS interrupts once we switch mode.
    ;
    cli
    call    Bs3PicMaskAll

    ;
    ; Initialize 16-bit protected mode.
    ;
    call far _Bs3CpuDetect_rm_far
    call far _Bs3InitMemory_rm_far
    call    Bs3Trap16Init

    ;
    ; Switch to PE16 and call main.
    ;
    call    _Bs3SwitchToPE16_rm
    call    _Main_pe16

    ; Try shutdown if it returns.
    call    Bs3Shutdown

