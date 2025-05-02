; $Id$
;; @file
; VBoxGuestAdditionsUninstallOld.nsh - Guest Additions uninstallation and migration handling
; for older Guest Additions and legacy (Sun [xVM] / innotek) packages.
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

!if $%KBUILD_TARGET_ARCH% != "arm64" ; Not needed for arm64.
!macro Uninstall_WipeInstallationDirectory un
;;
; Wipes the installation directory (recursively).
;
; Only used for wiping (Intel-based) legacy installations (innotek, Sun, Sun xVM).
;
; Input:
;   Stack[0]: Installation directory to wipe.
; Output:
;   Stack[0}: Return code. 0 means success.
;
Function ${un}Uninstall_WipeInstallationDirectory

  Pop  $0
  Push $1
  Push $2

  ; Do some basic sanity checks for not screwing up too fatal ...
  ${LogVerbose} "Removing old installation directory ($0) ..."
  ${If} $0    != $PROGRAMFILES
  ${AndIf} $0 != $PROGRAMFILES32
  ${AndIf} $0 != $PROGRAMFILES64
  ${AndIf} $0 != $COMMONFILES32
  ${AndIf} $0 != $COMMONFILES64
  ${AndIf} $0 != $WINDIR
  ${AndIf} $0 != $SYSDIR
    ${LogVerbose} "Wiping ($0) ..."
    Goto wipe
  ${EndIf}
  Goto wipe_abort

wipe:

  RMDir /r /REBOOTOK "$0"
  StrCpy $0 0 ; All went well.
  Goto exit

wipe_abort:

  ${LogVerbose} "Won't remove directory ($0)!"
  StrCpy $0 1 ; Signal some failure.
  Goto exit

exit:

  Pop $2
  Pop $1
  Push $0

FunctionEnd
!macroend
!insertmacro Uninstall_WipeInstallationDirectory ""


!ifndef UNINSTALLER_ONLY
!macro Uninstall_Before7_2 un
;;
; Uninstalls all files of a < 7.2 Guest Additions installation.
;
; These were stored in a "flat" hierarchy directly in the installation directory,
; which made it impossible to tell which files belong to which component.
;
; This also will be run when installing 7.2 Guest Additions to clean things up.
; Only needed for Intel-based installations (arm64 got introduced in 7.2).
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_Before7_2

  Delete /REBOOTOK "$INSTDIR\VBoxVideo.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideo.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoEarlyNT.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoEarlyNT.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxDisp.dll"

  Delete /REBOOTOK "$INSTDIR\VBoxMouse.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxMouse.cat"

  Delete /REBOOTOK "$INSTDIR\VBoxTray.exe"

  Delete /REBOOTOK "$INSTDIR\VBoxGuest.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxGuest.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxGuestEarlyNT.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxGuestEarlyNT.cat"

  Delete /REBOOTOK "$INSTDIR\VBCoInst.dll"    ; Deprecated, does not get installed anymore.
  Delete /REBOOTOK "$INSTDIR\VBoxControl.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxService.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxWHQLFake.exe"; Removed in r152293 (runup to 7.0).
  Delete /REBOOTOK "$INSTDIR\VBoxICD.dll"     ; Removed in r151892 (runup to 7.0).

!if $%VBOX_WITH_WDDM% == "1"
  Delete /REBOOTOK "$INSTDIR\VBoxWddm.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxWddm.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxWddm.inf"
  ; Obsolete files begin
  Delete /REBOOTOK "$INSTDIR\VBoxVideoWddm.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoWddm.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoWddm.inf"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoW8.cat"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoW8.sys"
  Delete /REBOOTOK "$INSTDIR\VBoxVideoW8.inf"
  ; Obsolete files end
  Delete /REBOOTOK "$INSTDIR\VBoxDispD3D.dll"
  !if $%VBOX_WITH_WDDM_DX% == "1"
    Delete /REBOOTOK "$INSTDIR\VBoxDX.dll"
  !endif
  !if $%VBOX_WITH_MESA3D% == "1"
    Delete /REBOOTOK "$INSTDIR\VBoxNine.dll"
    Delete /REBOOTOK "$INSTDIR\VBoxSVGA.dll"
    Delete /REBOOTOK "$INSTDIR\VBoxICD.dll"
    Delete /REBOOTOK "$INSTDIR\VBoxGL.dll"
  !endif

    Delete /REBOOTOK "$INSTDIR\VBoxD3D9wddm.dll"
    Delete /REBOOTOK "$INSTDIR\wined3dwddm.dll"
    ; Try to delete libWine in case it is there from old installation.
    Delete /REBOOTOK "$INSTDIR\libWine.dll"

  !if $%KBUILD_TARGET_ARCH% == "amd64"
    Delete /REBOOTOK "$INSTDIR\VBoxDispD3D-x86.dll"
    !if $%VBOX_WITH_WDDM_DX% == "1"
      Delete /REBOOTOK "$INSTDIR\VBoxDX-x86.dll"
    !endif
    !if $%VBOX_WITH_MESA3D% == "1"
      Delete /REBOOTOK "$INSTDIR\VBoxNine-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxSVGA-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxICD-x86.dll"
      Delete /REBOOTOK "$INSTDIR\VBoxGL-x86.dll"
    !endif

      Delete /REBOOTOK "$INSTDIR\VBoxD3D9wddm-x86.dll"
      Delete /REBOOTOK "$INSTDIR\wined3dwddm-x86.dll"
  !endif ; $%KBUILD_TARGET_ARCH% == "amd64"
!endif ; $%VBOX_WITH_WDDM% == "1"

  Delete /REBOOTOK "$INSTDIR\RegCleanup.exe" ; Obsolete since r165894 (see #10799).
  Delete /REBOOTOK "$INSTDIR\VBoxDrvInst.exe"
  Delete /REBOOTOK "$INSTDIR\VBoxGuestInstallHelper.exe"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" ; Old name. Changed to Website.url in r153663.
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"            ; Obsolete. We don't install a desktop link any more.

FunctionEnd
!macroend
!insertmacro Uninstall_Before7_2 ""
!endif ; UNINSTALLER_ONLY
!endif ; $%KBUILD_TARGET_ARCH% != "arm64"


!if $%KBUILD_TARGET_ARCH% != "arm64" ; Not needed for arm64.
!macro Uninstall_Sun un
;;
; Function to clean  up an old Sun (pre-Oracle) installation.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_Sun

  Push $0
  Push $1
  Push $2

  ; Get current installation path.
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path.
  ; Param "$1"       ; Result string
  ; Param "$0"       ; String
  ; Param "\"        ; SubString
  ; Param "<"        ; SearchDirection
  ; Param "<"        ; StrInclusionDirection
  ; Param "0"        ; IncludeSubString
  ; Param "0"        ; Loops
  ; Param "0"        ; CaseSensitive
  ${${un}StrStrAdv} "$1" "$0" "\" "<" "<" "0" "0" "0"

  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path.
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions" "${REGISTRY_VAL_ORG_MOUSE_PATH}"
  WriteRegStr HKLM "${REGISTRY_KEY_UNINST_PRODUCT}" "${REGISTRY_VAL_ORG_MOUSE_PATH}" $0

  ; Try to wipe current installation directory.
  Push $1 ; Push uninstaller path to stack.
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit.

common:

  ; Make sure everything is cleaned up in case the old uninstaller did forget something.
  DeleteRegKey HKLM "SOFTWARE\Sun\VirtualBox Guest Additions"
  DeleteRegKey /ifempty HKLM "SOFTWARE\Sun"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions"
  Delete /REBOOTOK "$1\netamd.inf"
  Delete /REBOOTOK "$1\pcntpci5.cat"
  Delete /REBOOTOK "$1\PCNTPCI5.sys"

  ; Try to remove old installation directory if empty.
  RMDir /r /REBOOTOK "$SMPROGRAMS\Sun VirtualBox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Get original mouse driver info and restore it.
  ;ReadRegStr $0 "${REGISTRY_KEY_UNINST_ROOT}" "${REGISTRY_KEY_UNINST_PRODUCT}" "${REGISTRY_VAL_ORG_MOUSE_PATH}"
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VBoxMouseNT.sys"

  ; Delete vendor installation directory (only if completely empty)
!if $%KBUILD_TARGET_ARCH% == "x86"
  RMDir /REBOOTOK "$PROGRAMFILES32\Sun"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\Sun"
!endif

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_Sun ""


!macro Uninstall_SunXVM un
;;
; Function to clean  up an old Sun xVM installation.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_SunXVM

  Push $0
  Push $1
  Push $2

  ; Get current installation path.
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path.
  ; Param "$1"       ; Result string
  ; Param "$0"       ; String
  ; Param "\"        ; SubString
  ; Param "<"        ; SearchDirection
  ; Param "<"        ; StrInclusionDirection
  ; Param "0"        ; IncludeSubString
  ; Param "0"        ; Loops
  ; Param "0"        ; CaseSensitive
  ${${un}StrStrAdv} "$1" "$0" "\" "<" "<" "0" "0" "0"
  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path.
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions" ${REGISTRY_VAL_ORG_MOUSE_PATH}
  WriteRegStr HKLM "${REGISTRY_KEY_UNINST_PRODUCT}" ${REGISTRY_VAL_ORG_MOUSE_PATH} $0

  ; Try to wipe current installation directory.
  Push $1 ; Push uninstaller path to stack.
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack.
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit.

common:

  ; Make sure everything is cleaned up in case the old uninstaller did forget something.
  DeleteRegKey HKLM "SOFTWARE\Sun\xVM VirtualBox Guest Additions"
  DeleteRegKey /ifempty HKLM "SOFTWARE\Sun"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions"
  Delete /REBOOTOK "$1\netamd.inf"
  Delete /REBOOTOK "$1\pcntpci5.cat"
  Delete /REBOOTOK "$1\PCNTPCI5.sys"

  ; Try to remove old installation directory if empty.
  RMDir /r /REBOOTOK "$SMPROGRAMS\Sun xVM VirtualBox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Delete vendor installation directory (only if completely empty).
!if $%KBUILD_TARGET_ARCH% == "x86"
  RMDir /REBOOTOK "$PROGRAMFILES32\Sun"
!else
  RMDir /REBOOTOK "$PROGRAMFILES64\Sun"
!endif

  ; Get original mouse driver info and restore it.
  ;ReadRegStr $0 "${REGISTRY_KEY_UNINST_ROOT}" "${REGISTRY_KEY_UNINST_PRODUCT}" "${REGISTRY_VAL_ORG_MOUSE_PATH}"
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VBoxMouseNT.sys"

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_SunXVM ""


!macro Uninstall_Innotek un
;;
; Function to clean  up an old innotek installation.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}Uninstall_Innotek

  Push $0
  Push $1
  Push $2

  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" exit

  ; Extract path.
  ; Param "$1"       ; Result string
  ; Param "$0"       ; String
  ; Param "\"        ; SubString
  ; Param "<"        ; SearchDirection
  ; Param "<"        ; StrInclusionDirection
  ; Param "0"        ; IncludeSubString
  ; Param "0"        ; Loops
  ; Param "0"        ; CaseSensitive
  ${${un}StrStrAdv} "$1" "$0" "\" "<" "<" "0" "0" "0"
  StrCmp $1 "" exit

  ; Save current i8042prt info to new uninstall registry path.
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions" "${REGISTRY_VAL_ORG_MOUSE_PATH}"
  WriteRegStr HKLM "${REGISTRY_KEY_UNINST_PRODUCT}" "${REGISTRY_VAL_ORG_MOUSE_PATH}" $0

  ; Try to wipe current installation directory.
  Push $1 ; Push uninstaller path to stack.
  Call ${un}Uninstall_WipeInstallationDirectory
  Pop $2  ; Get uninstaller exit code from stack.
  StrCmp $2 0 common exit ; Only process common part if exit code is 0, otherwise exit.

common:

  ; Remove left over files which were not entirely cached by the formerly running uninstaller.
  DeleteRegKey HKLM "SOFTWARE\innotek\VirtualBox Guest Additions"
  DeleteRegKey HKLM "SOFTWARE\innotek"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions"
  Delete /REBOOTOK "$1\VBoxGuestDrvInst.exe"
  Delete /REBOOTOK "$1\VBoxMouseInst.exe"
  Delete /REBOOTOK "$1\VBoxSFDrvInst.exe"
  Delete /REBOOTOK "$1\RegCleanup.exe"
  Delete /REBOOTOK "$1\VBoxService.exe"
  Delete /REBOOTOK "$1\VBoxMouseInst.exe"
  Delete /REBOOTOK "$1\innotek VirtualBox Guest Additions.url"
  Delete /REBOOTOK "$1\uninst.exe"
  Delete /REBOOTOK "$1\iexplore.ico"
  Delete /REBOOTOK "$1\install.log"
  Delete /REBOOTOK "$1\VBCoInst.dll"
  Delete /REBOOTOK "$1\VBoxControl.exe"
  Delete /REBOOTOK "$1\VBoxDisp.dll"
  Delete /REBOOTOK "$1\VBoxGINA.dll"
  Delete /REBOOTOK "$1\VBoxGuest.cat"
  Delete /REBOOTOK "$1\VBoxGuest.inf"
  Delete /REBOOTOK "$1\VBoxGuest.sys"
  Delete /REBOOTOK "$1\VBoxMouse.inf"
  Delete /REBOOTOK "$1\VBoxMouse.sys"
  Delete /REBOOTOK "$1\VBoxVideo.cat"
  Delete /REBOOTOK "$1\VBoxVideo.inf"
  Delete /REBOOTOK "$1\VBoxVideo.sys"

  ; Try to remove old installation directory if empty.
  RMDir /r /REBOOTOK "$SMPROGRAMS\innotek VirtualBox Guest Additions"
  RMDir /REBOOTOK "$1"

  ; Delete vendor installation directory (only if completely empty).
!if $%KBUILD_TARGET_ARCH% == "x86"       ; 32-bit
  RMDir /REBOOTOK "$PROGRAMFILES32\innotek"
!else   ; 64-bit
  RMDir /REBOOTOK "$PROGRAMFILES64\innotek"
!endif

  ; Get original mouse driver info and restore it.
  ;ReadRegStr $0 "${REGISTRY_KEY_UNINST_ROOT}" "${REGISTRY_KEY_UNINST_PRODUCT}" "${REGISTRY_VAL_ORG_MOUSE_PATH}"
  ;WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\i8042prt" "ImagePath" $0
  ;Delete "$SYSDIR\drivers\VBoxMouseNT.sys".

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro Uninstall_Innotek ""


;;
; Handles uninstallation of legacy Guest Additions and/or performs necessary migration steps.
;
; Input:
;   None
; Output:
;   None
;
Function HandleOldGuestAdditions

  Push $0
  Push $1
  Push $2

  ${LogVerbose} "Checking for old Guest Additions ..."

  ; Check for old "Sun VirtualBox Guest Additions"
  ; - before rebranding to Oracle
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions" "UninstallString"
  ${If} $0 == ""
    goto sun_xvm_check ; If string is empty, Sun additions are probably not installed (anymore)
  ${EndIf}

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_uninstall:

  Call Uninstall_Sun
  Goto migration_check

sun_xvm_check:

  ; Check for old "Sun xVM VirtualBox Guest Additions"
  ; - before getting rid of the "xVM" namespace
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions" "UninstallString"
  ${If} $0 == ""
    goto innotek_check ; If string is empty, Sun xVM additions are probably not installed (anymore).
  ${EndIf}

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_xvm_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_xvm_uninstall:

  Call Uninstall_SunXVM
  Goto migration_check

innotek_check:

  ; Check for old "innotek" Guest Additions" before rebranding to "Sun".
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions" "UninstallString"
  ${If} $0 == ""
    goto migration_check ; If string is empty, innotek Guest Additions are probably not installed (anymore).
  ${EndIf}

  MessageBox MB_YESNO $(VBOX_INNOTEK_FOUND) /SD IDYES IDYES innotek_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_INNOTEK_ABORTED) /SD IDOK
    Quit

innotek_uninstall:

  Call Uninstall_Innotek
  Goto migration_check

migration_check:

    ${If} $g_strAddVerMaj != ""
      goto migration_perform
    ${EndIf}
    goto done

migration_perform:

  ${LogVerbose} "Running migration steps ..."

  ; Migrate old(er) installation directories (< 7.2) to new structure.
  Call Uninstall_Before7_2

done:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd

!endif ; $%KBUILD_TARGET_ARCH% != "arm64"
