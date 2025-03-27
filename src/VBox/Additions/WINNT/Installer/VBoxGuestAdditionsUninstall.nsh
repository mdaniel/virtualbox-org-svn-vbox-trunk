; $Id$
;; @file
; VBoxGuestAdditionsUninstall.nsh - Guest Additions uninstallation.
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


!macro Uninstall un
;;
; Main uninstallation function.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_Perform

  ${LogVerbose} "Uninstalling ..."
!ifdef _DEBUG
  ${LogVerbose} "Detected OS version: Windows $g_strWinVersion"
  ${LogVerbose} "System Directory: $g_strSystemDir"
  ${LogVerbose} "Temp Directory: $TEMP"
!endif

  ; Create temp directory where we can store uninstallation logs.
  CreateDirectory "$TEMP\${PRODUCT_NAME}"

  !insertmacro Common_EmitOSSelectionSwitch
!if $%KBUILD_TARGET_ARCH% == "x86" ; 32-bit only
osselswitch_case_nt4:
  Call ${un}NT4_CallbackUninstall
  goto common
!endif
osselswitch_case_w2k_xp_w2k3:
  Call ${un}W2K_CallbackUninstall
  goto common
osselswitch_case_vista_and_later:
  Call ${un}W2K_CallbackUninstall
  Call ${un}Vista_CallbackUninstall
  goto common
osselswitch_case_unsupported:
  ${If} $g_bForceInstall == "true"
    Goto osselswitch_case_vista_and_later ; Assume newer OS than we know of ...
  ${EndIf}
  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  Goto exit

common:
exit:

  ; Delete Guest Additions directory (only if completely empty).
  RMDir /REBOOTOK "$INSTDIR"

  ; Delete desktop & start menu entries
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.url"
  RMDir  "$SMPROGRAMS\${PRODUCT_NAME}"

  ; Delete vendor installation directory (only if completely empty).
!if $%KBUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\$%VBOX_VENDOR_SHORT%"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\$%VBOX_VENDOR_SHORT%"
!endif

  ; Delete version information.
  DeleteRegValue HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "Version"
  DeleteRegValue HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "VersionExt"
  DeleteRegValue HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "Revision"
  DeleteRegValue HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "InstallDir"

  ; Delete registry keys.
  DeleteRegKey /ifempty HKLM "${REGISTRY_KEY_PRODUCT_ROOT}"
  DeleteRegKey /ifempty HKLM "${REGISTRY_KEY_VENDOR_ROOT}"
  DeleteRegKey "${REGISTRY_KEY_UNINST_ROOT}" "${REGISTRY_KEY_UNINST_PRODUCT}" ; Uninstaller.

  ;
  ; Dump UI log to on success too. Only works with non-silent installs.
  ; (This has to be done here rather than in .onUninstSuccess, because by
  ; then the log is no longer visible in the UI).
  ;
  ${IfNot} ${Silent}
  !if $%VBOX_WITH_GUEST_INSTALL_HELPER% == "1"
    VBoxGuestInstallHelper::DumpLog "$TEMP\vbox_uninstall_ui.log"
  !else
    StrCpy $0 "$TEMP\vbox_uninstall_ui.log"
    Push $0
    Call DumpLog
  !endif
  ${EndIf}

FunctionEnd
!macroend
!ifndef UNINSTALLER_ONLY
  !insertmacro Uninstall ""
!endif
!insertmacro Uninstall "un."


!macro Common_DeleteFiles un
;;
; Deletes files commonly used by all supported guest OSes in $INSTDIR.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Common_DeleteFiles

  Delete /REBOOTOK "$INSTDIR\${LICENSE_FILE_RTF}"
  Delete /REBOOTOK "$INSTDIR\iexplore.ico" ; Removed in r153662.

  Delete /REBOOTOK "$INSTDIR\install.log"
  Delete /REBOOTOK "$INSTDIR\install*.log"
  Delete /REBOOTOK "$INSTDIR\uninst.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxGAs*.log" ; Debug logs created by VBoxStub.
  Delete /REBOOTOK "$INSTDIR\${PRODUCT_NAME}.url"

  ;
  ; Guest driver
  ;
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxTray.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxControl.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxHook.dll"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxGuest.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxGuest.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxGuest.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxGuestEarlyNT.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest\VBoxGuestEarlyNT.cat"
  RMDir  /REBOOTOK "$INSTDIR\VBoxGuest"

  ;
  ; Mouse driver
  ;
  Delete /REBOOTOK "$INSTDIR\VBoxMouse\VBoxMouse.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse\VBoxMouse.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse\VBoxMouse.cat"
  RMDir  /REBOOTOK "$INSTDIR\VBoxMouse"

  ;
  ; VBoxVideo driver
  ;
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxVideo.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxVideo.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxVideo.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxVideoEarlyNT.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxVideoEarlyNT.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo\VBoxDisp.dll"
  RMDir  /REBOOTOK "$INSTDIR\VBoxVideo"

  ;
  ; VBoxWddm driver
  ;
  ; !if $%VBOX_WITH_WDDM% == "1"
    Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxWddm.cat"
    Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxWddm.sys"
    Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxWddm.inf"
    Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxDispD3D.dll"
    ; !if $%VBOX_WITH_WDDM_DX% == "1"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxDX.dll"
    ; !endif
    ; !if $%VBOX_WITH_MESA3D% == "1"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxNine.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxSVGA.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxICD.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxGL.dll"
    ; !endif
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxD3D9wddm.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\wined3dwddm.dll"
    ; !if $%KBUILD_TARGET_ARCH% == "amd64"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxDispD3D-x86.dll"
      ; !if $%VBOX_WITH_WDDM_DX% == "1"
        Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxDX-x86.dll"
      ; !endif
      ; !if $%VBOX_WITH_MESA3D% == "1"
        Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxNine-x86.dll"
        Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxSVGA-x86.dll"
        Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxICD-x86.dll"
        Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxGL-x86.dll"
      ; !endif
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\VBoxD3D9wddm-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxWddm\wined3dwddm-x86.dll"
    ; !endif ; $%KBUILD_TARGET_ARCH% == "amd64"
    RMDir /REBOOTOK "$INSTDIR\VBoxWddm"
  ; !endif ; $%VBOX_WITH_WDDM% == "1"

  ;
  ; Shared Folders driver
  ;
  Delete /REBOOTOK "$INSTDIR\VBoxSF\VBoxSF.sys"
  ;!if $%BUILD_TARGET_ARCH% == "x86"
    Delete /REBOOTOK "$INSTDIR\VBoxSF\VBoxSFW2K.sys"
  ;!endif
  Delete /REBOOTOK "$INSTDIR\VBoxSF\VBoxMRXNP.dll"
  ;!if $%BUILD_TARGET_ARCH% == "x86"
    Delete /REBOOTOK "$INSTDIR\VBoxSF\VBoxMRXNP-x86.dll"
  ; !endif
  RMDir /REBOOTOK "$INSTDIR\VBoxSF"

  ;
  ; Credential providers
  ;
  Delete /REBOOTOK "$INSTDIR\AutoLogon\VBoxGINA.dll"
  Delete /REBOOTOK "$INSTDIR\AutoLogon\VBoxCredProv.dll"
  RMDir /REBOOTOK "$INSTDIR\AutoLogon"

  ;
  ; Certificate stuff.
  ;
  Delete /REBOOTOK "$INSTDIR\Cert\VBoxGAs*.log" ; Debug logs created by VBoxCertUtil.
  Delete /REBOOTOK "$INSTDIR\Cert\VBoxCertUtil.exe"
  RMDir /REBOOTOK "$INSTDIR\Cert"

  ;
  ; Misc binaries
  ;
  Delete /REBOOTOK "$INSTDIR\Bin\VBoxService.exe"
  RMDir /REBOOTOK "$INSTDIR\Bin"

  ;
  ; Tools
  Delete /REBOOTOK "$INSTDIR\Tools\VBoxDrvInst.exe" ; Does not exist on NT4, but try to remove it anyway.
  Delete /REBOOTOK "$INSTDIR\Tools\VBoxGuestInstallHelper.exe"
  Delete /REBOOTOK "$INSTDIR\Tools\VBoxAudioTest.exe"
  RMDir /REBOOTOK "$INSTDIR\Tools"

FunctionEnd
!macroend
!insertmacro Common_DeleteFiles "un."


!macro Uninstall_DeleteFiles un
;;
; Deletes all previously installed files in $INSTDIR.
; Must be called after ${un}Uninstall_Perform.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_DeleteFiles

  ${LogVerbose} "Deleting files in $\"$INSTDIR$\" ..."
!ifdef _DEBUG
  ${LogVerbose} "Detected OS version: Windows $g_strWinVersion"
  ${LogVerbose} "System Directory: $g_strSystemDir"
!endif

  !insertmacro Common_EmitOSSelectionSwitch
!if $%KBUILD_TARGET_ARCH% == "x86" ; 32-bit only
osselswitch_case_nt4:
  Call ${un}NT4_CallbackDeleteFiles
  goto common
!endif
osselswitch_case_w2k_xp_w2k3:
  Call ${un}W2K_CallbackDeleteFiles
  goto common
osselswitch_case_vista_and_later:
  Call ${un}W2K_CallbackDeleteFiles
  Call ${un}Vista_CallbackDeleteFiles
  goto common
osselswitch_case_unsupported:
  ${If} $g_bForceInstall == "true"
    Goto osselswitch_case_vista_and_later ; Assume newer OS than we know of ...
  ${EndIf}
  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  Goto exit

common:

  Call ${un}Common_DeleteFiles

exit:

FunctionEnd
!macroend
!insertmacro Uninstall_DeleteFiles "un."
