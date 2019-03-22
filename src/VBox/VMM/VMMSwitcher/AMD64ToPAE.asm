; $Id$
;; @file
; VMM - World Switchers, PAE to PAE
;

;
; Copyright (C) 2006-2017 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

;*******************************************************************************
;*   Defined Constants And Macros                                              *
;*******************************************************************************
%define SWITCHER_TO_PAE             1
%undef  SWITCHER_TO_32BIT
%define SWITCHER_TYPE               VMMSWITCHER_AMD64_TO_PAE
%define SWITCHER_DESCRIPTION        "AMD64 to/from PAE"
%define NAME_OVERLOAD(name)         vmmR3SwitcherAMD64ToPAE_ %+ name
;%define SWITCHER_FIX_INTER_CR3_HC   FIX_INTER_AMD64_CR3
%define SWITCHER_FIX_INTER_CR3_GC   FIX_INTER_PAE_CR3


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VMMSwitcher/AMD64andLegacy.mac"

