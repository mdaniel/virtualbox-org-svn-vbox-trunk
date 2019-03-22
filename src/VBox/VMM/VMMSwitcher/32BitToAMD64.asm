; $Id$
;; @file
; VMM - World Switchers, 32-Bit to AMD64 intermediate context.
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

;*******************************************************************************
;*   Defined Constants And Macros                                              *
;*******************************************************************************
%define SWITCHER_TYPE               VMMSWITCHER_32_TO_AMD64
%define SWITCHER_DESCRIPTION        "32-bit to/from AMD64 intermediate context"
%define NAME_OVERLOAD(name)         vmmR3Switcher32BitToAMD64_ %+ name
%define SWITCHER_FIX_INTER_CR3_HC   FIX_INTER_32BIT_CR3

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "VMMSwitcher/LegacyandAMD64.mac"

