; $Id$
;; @file
; VBoxGuestAdditionsVista.nsh - Guest Additions installation for Windows Vista/7.
;

;
; Copyright (C) 2006-2024 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; SPDX-License-Identifier: GPL-3.0-only
;


;;
; Callback function installation preparation for Windows >= Vista guests.
;
; Input:
;   None
; Output:
;   None
;
Function Vista_CallbackPrepare

  ${LogVerbose} "Preparing for >= Vista ..."

  ; Try to restore the original Direct3D files in case we're coming from an old(er) Guest Additions
  ; installation, which formerly replaced those system files with our own stubs.
  ; This no longer is needed and thus needs to be reverted in any case.
  Call RestoreFilesDirect3D
  ; Ignore the result in case we had trouble restoring. The system would be in an inconsistent state anyway.
  Call VBoxMMR_Uninstall

FunctionEnd


;;
; Callback function for extracting files for Windows >= Vista guests.
;
; Input:
;   None
; Output:
;   None
;
Function Vista_CallbackExtractFiles

  ${LogVerbose} "Extracting for >= Vista ..."
  ; Nothing to do here yet.

FunctionEnd


;;
; Callback function for installation for Windows >= Vista guests.
;
; Input:
;   None
; Output:
;   None
;
Function Vista_CallbackInstall

  ${LogVerbose} "Installing for >= Vista ..."
  ; Nothing to do here yet.

FunctionEnd


!macro Vista_CallbackDeleteFiles un
;;
; Callback function for deleting files for Windows >= Vista guests.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Vista_CallbackDeleteFiles

  ${LogVerbose} "Deleting files for >= Vista ..."
  ; Nothing to do here.

FunctionEnd
!macroend
!insertmacro Vista_CallbackDeleteFiles "un."


!macro Vista_CallbackUninstall un
;;
; Callback function for uninstallation for Windows >= Vista guests.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Vista_CallbackUninstall

  ${LogVerbose} "Uninstalling for >= Vista ..."

  ; Remove credential provider
  ${LogVerbose} "Removing auto-logon support ..."
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
  DeleteRegKey HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}"
  Delete /REBOOTOK "$g_strSystemDir\VBoxCredProv.dll"

  Call ${un}VBoxMMR_Uninstall

FunctionEnd
!macroend
!ifndef UNINSTALLER_ONLY
  !insertmacro Vista_CallbackUninstall ""
!endif
!insertmacro Vista_CallbackUninstall "un."


!macro VBoxMMR_Uninstall un
;;
; Function for uninstalling the MMR driver.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}VBoxMMR_Uninstall

  ; Remove VBoxMMR always

  DetailPrint "Uninstalling VBoxMMR ..."
  Call ${un}StopVBoxMMR

  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "VBoxMMR"

  Delete /REBOOTOK "$g_strSystemDir\VBoxMMR.exe"

  !if $%KBUILD_TARGET_ARCH% == "amd64"
    Delete /REBOOTOK "$g_strSysWow64\VBoxMMRHook.dll"
    Delete /REBOOTOK "$INSTDIR\VBoxMMR-x86.exe"
    Delete /REBOOTOK "$INSTDIR\VBoxMMRHook-x86.dll"
  !else
    Delete /REBOOTOK "$g_strSystemDir\VBoxMMRHook.dll"
    Delete /REBOOTOK "$INSTDIR\VBoxMMR.exe"
    Delete /REBOOTOK "$INSTDIR\VBoxMMRHook.dll"
  !endif

FunctionEnd
!macroend
!insertmacro VBoxMMR_Uninstall ""
!insertmacro VBoxMMR_Uninstall "un."
