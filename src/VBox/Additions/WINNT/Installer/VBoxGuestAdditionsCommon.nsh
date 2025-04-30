; $Id$
;; @file
; VBoxGuestAdditionsCommon.nsh - Common / shared utility functions.
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
; Macro which emits an OS selection switch.
;
; Input:
;   None
; Output:
;   None
;
; The following labels must be implemented by the caller:
; - osselswitch_case_w2k_xp_w2k3
; - osselswitch_case_vista_and_later
; - osselswitch_case_nt4 (x86 only)
; - osselswitch_case_unsupported
;
; Note: When adding new OS support, this is the place to add it.
;       Used for installation and uninstallation routines.
;
!macro Common_EmitOSSelectionSwitch
  ${If}     "$g_strWinVersion" == "2000"
  ${OrIf}   "$g_strWinVersion" == "XP"
  ${OrIf}   "$g_strWinVersion" == "203"
    goto osselswitch_case_w2k_xp_w2k3
  ${ElseIf} "$g_strWinVersion" == "2000"
  ${OrIf}   "$g_strWinVersion" == "Vista"
  ${OrIf}   "$g_strWinVersion" == "7"
  ${OrIf}   "$g_strWinVersion" == "8"
  ${OrIf}   "$g_strWinVersion" == "8_1"
  ${OrIf}   "$g_strWinVersion" == "10"
    goto osselswitch_case_vista_and_later
!if $%KBUILD_TARGET_ARCH% == "x86" ; 32-bit only.
  ${ElseIf} "$g_strWinVersion" == "NT4"
    goto osselswitch_case_nt4
!endif
  ${Else}
    goto osselswitch_case_unsupported
  ${EndIf}
!macroend


;;
; Extracts all files to $INSTDIR.
;
; Input:
;   None
; Output:
;   None
;
; Note: This is a worker function for the actual installation process to extract all required
;       files beforehand into $INSTDIR before the actual (driver) installation, as well as
;       when only extracting the installer files into a directory (via /extract).
;
Function Common_ExtractFiles

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ${LogVerbose} "Extracting files to: $INSTDIR"

!ifdef VBOX_WITH_LICENSE_INSTALL_RTF
  ; Copy license file (if any) into the installation directory.
  FILE "/oname=${LICENSE_FILE_RTF}" "$%VBOX_BRAND_LICENSE_RTF%"
!endif

  ;
  ; Guest driver
  ;
  SetOutPath "$INSTDIR\VBoxGuest"
  FILE "$%PATH_OUT%\bin\additions\VBoxGuest.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxGuest.inf"
  FILE "$%PATH_OUT%\bin\additions\VBoxTray.exe"
  FILE "$%PATH_OUT%\bin\additions\VBoxControl.exe"
  FILE "$%PATH_OUT%\bin\additions\VBoxHook.dll"
!if $%KBUILD_TARGET_ARCH% == "x86"
  ${If} $g_strEarlyNTDrvInfix != ""
    FILE "$%PATH_OUT%\bin\additions\VBoxGuestEarlyNT.inf"
  !ifdef VBOX_SIGN_ADDITIONS
    FILE "$%PATH_OUT%\bin\additions\VBoxGuestEarlyNT.cat"
  !endif
  ${EndIf}
!endif
!ifdef VBOX_SIGN_ADDITIONS
  !if $%KBUILD_TARGET_ARCH% == "arm64"
    FILE "$%PATH_OUT%\bin\additions\VBoxGuest.cat"
  !else
  ${If} $g_strWinVersion == "10"
    FILE "$%PATH_OUT%\bin\additions\VBoxGuest.cat"
  ${Else}
    FILE "/oname=VBoxGuest.cat" "$%PATH_OUT%\bin\additions\VBoxGuest-PreW10.cat"
  ${EndIf}
  !endif
!endif

  ;
  ; Mouse driver
  ;
  SetOutPath "$INSTDIR\VBoxMouse"
  FILE "$%PATH_OUT%\bin\additions\VBoxMouse.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxMouse.inf"
!ifdef VBOX_SIGN_ADDITIONS
  !if $%KBUILD_TARGET_ARCH% == "arm64"
    FILE "$%PATH_OUT%\bin\additions\VBoxMouse.cat"
  !else
    ${If} $g_strWinVersion == "10"
      FILE "$%PATH_OUT%\bin\additions\VBoxMouse.cat"
    ${Else}
      FILE "/oname=VBoxMouse.cat" "$%PATH_OUT%\bin\additions\VBoxMouse-PreW10.cat"
    ${EndIf}
  !endif
!endif

  ;
  ; VBoxVideo driver
  ;
!if $%KBUILD_TARGET_ARCH% != "arm64" ;; @todo win.arm64: Make VBoxVideo and friends build on arm.
  SetOutPath "$INSTDIR\VBoxVideo"
  FILE "$%PATH_OUT%\bin\additions\VBoxVideo.sys"
  FILE "$%PATH_OUT%\bin\additions\VBoxDisp.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxVideo.inf"
  !if $%KBUILD_TARGET_ARCH% == "x86"
    ${If} $g_strEarlyNTDrvInfix != ""
      FILE "$%PATH_OUT%\bin\additions\VBoxVideoEarlyNT.inf"
      !ifdef VBOX_SIGN_ADDITIONS
        FILE "$%PATH_OUT%\bin\additions\VBoxVideoEarlyNT.cat"
      !endif
    ${EndIf}
  !endif ; $%KBUILD_TARGET_ARCH% == "x86"
!endif ; $%KBUILD_TARGET_ARCH% != "arm64"
!ifdef VBOX_SIGN_ADDITIONS
  !if $%KBUILD_TARGET_ARCH% != "arm64" ;; @todo win.arm64: Ditto.
    ${If} $g_strWinVersion == "10"
      FILE "$%PATH_OUT%\bin\additions\VBoxVideo.cat"
    ${Else}
      FILE "/oname=VBoxVideo.cat" "$%PATH_OUT%\bin\additions\VBoxVideo-PreW10.cat"
    ${EndIf}
  !endif
!endif ; VBOX_SIGN_ADDITIONS

  ;
  ; VBoxWddm driver
  ;
!if $%VBOX_WITH_WDDM% == "1"
  ${If}   $g_bWithWDDM == "true"
  ${OrIf} $g_bOnlyExtract == "true"
    ; WDDM Video driver
    SetOutPath "$INSTDIR\VBoxWddm"
    !ifdef VBOX_SIGN_ADDITIONS
      !if $%KBUILD_TARGET_ARCH% == "arm64"
        FILE "$%PATH_OUT%\bin\additions\VBoxWddm.cat"
      !else
      ${If} $g_strWinVersion == "10"
        FILE "$%PATH_OUT%\bin\additions\VBoxWddm.cat"
      ${Else}
        FILE "/oname=VBoxWddm.cat" "$%PATH_OUT%\bin\additions\VBoxWddm-PreW10.cat"
      ${EndIf}
      !endif
    !endif
    FILE "$%PATH_OUT%\bin\additions\VBoxWddm.sys"
    FILE "$%PATH_OUT%\bin\additions\VBoxWddm.inf"
    FILE "$%PATH_OUT%\bin\additions\VBoxDispD3D.dll"
    !if $%VBOX_WITH_WDDM_DX% == "1"
      FILE "$%PATH_OUT%\bin\additions\VBoxDX.dll"
    !endif
    !if $%VBOX_WITH_MESA3D% == "1"
      FILE "$%PATH_OUT%\bin\additions\VBoxNine.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxSVGA.dll"
      FILE "$%PATH_OUT%\bin\additions\VBoxGL.dll"
    !endif

    !if $%KBUILD_TARGET_ARCH% == "amd64"
      FILE "$%PATH_OUT%\bin\additions\VBoxDispD3D-x86.dll"
      !if $%VBOX_WITH_WDDM_DX% == "1"
        FILE "$%PATH_OUT%\bin\additions\VBoxDX-x86.dll"
      !endif
      !if $%VBOX_WITH_MESA3D% == "1"
        FILE "$%PATH_OUT%\bin\additions\VBoxNine-x86.dll"
        FILE "$%PATH_OUT%\bin\additions\VBoxSVGA-x86.dll"
        FILE "$%PATH_OUT%\bin\additions\VBoxGL-x86.dll"
      !endif
    !endif ; $%KBUILD_TARGET_ARCH% == "amd64"
  ${EndIf} ; $g_bWithWDDM == "true"
!endif ; $%VBOX_WITH_WDDM% == "1"

  ;
  ; Shared Folders driver
  ;
  ${If}   $g_strWinVersion <> "NT4" ; Only available for > NT4.
  ${OrIf} $g_bOnlyExtract == "true"
    SetOutPath "$INSTDIR\VBoxSF"
    FILE "$%PATH_OUT%\bin\additions\VBoxSF.sys"
    !if $%KBUILD_TARGET_ARCH% == "x86"
      FILE "$%PATH_OUT%\bin\additions\VBoxSFW2K.sys"
    !endif
    FILE "$%PATH_OUT%\bin\additions\VBoxMRXNP.dll"
    !if $%KBUILD_TARGET_ARCH% == "amd64" ; Not available on arm64.
      FILE "$%PATH_OUT%\bin\additions\VBoxMRXNP-x86.dll"
    !endif
  ${EndIf}

  ;
  ; Credential providers
  ;
  SetOutPath "$INSTDIR\AutoLogon"
  FILE "$%PATH_OUT%\bin\additions\VBoxGINA.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxCredProv.dll"

  ;
  ; Certificate stuff
  ;
!ifdef VBOX_SIGN_ADDITIONS
  ${If}   $g_strWinVersion <> "NT4" ; Only required for > NT4.
  ${OrIf} $g_bOnlyExtract == "true"
    SetOutPath "$INSTDIR\Cert"
    FILE "$%PATH_OUT%\bin\additions\VBoxCertUtil.exe"
    AccessControl::SetOnFile "$INSTDIR\Cert\VBoxCertUtil.exe" "(BU)" "GenericRead"
  ${Endif}
!endif

  ;
  ; Misc binaries
  ;
  SetOutPath "$INSTDIR\Bin"
  ; Technically not needed, as VBoxService gets installed into System32, but
  ; keep it in the installation directory as well for completeness.
  FILE "$%PATH_OUT%\bin\additions\VBoxService.exe"
  AccessControl::SetOnFile "$INSTDIR\Bin\VBoxService.exe" "(BU)" "GenericRead"

  ;
  ; Tools
  ;
  SetOutPath "$INSTDIR\Tools"
${If} $g_strWinVersion <> "NT4" ; VBoxDrvInst only works with > NT4.
  FILE "$%PATH_OUT%\bin\additions\VBoxDrvInst.exe"
  AccessControl::SetOnFile "$INSTDIR\VBoxDrvInst.exe" "(BU)" "GenericRead"
${EndIf}
  FILE "$%PATH_OUT%\bin\additions\VBoxGuestInstallHelper.exe"
  AccessControl::SetOnFile "$INSTDIR\VBoxGuestInstallHelper.exe" "(BU)" "GenericRead"
!ifdef VBOX_WITH_ADDITIONS_SHIPPING_AUDIO_TEST
  FILE "$%PATH_OUT%\bin\additions\VBoxAudioTest.exe"
  AccessControl::SetOnFile "$INSTDIR\VBoxAudioTest.exe" "(BU)" "GenericRead"
!endif

FunctionEnd


;;
; Macro for retrieving the Windows version this installer is running on.
;
; @return  Stack: Windows version string. Empty on error /
;                 if not able to identify.
;
!macro GetWindowsVersionEx un
Function ${un}GetWindowsVersionEx

  Push $0
  Push $1

  ; Check if we are running on Windows 2000 or above.
  ; For other windows versions (> XP) it may be necessary to change winver.nsh.
  Call ${un}GetWindowsVersion
  Pop $0         ; Windows Version.

  ; Param "$1"        ; Result string.
  ; Param "$0"        ; The windows version string.
  ; Param "NT"        ; String to search for. W2K+ returns no string containing "NT".
  ${${un}StrStr} "$1" "$0" "NT"

  ${If} $1 == "" ; If empty -> not NT 3.XX or 4.XX.
    ; $0 contains the original version string.
  ${Else}
    ; Ok we know it is NT. Must be a string like NT X.XX.
    ; Param "$1"      ; Result string.
    ; Param "$0"      ; The windows version string.
    ; Param "4."      ; String to search for.
    ${${un}StrStr} "$1" "$0" "4."
    ${If} $1 == "" ; If empty -> not NT 4.
      ;; @todo NT <= 3.x ?
      ; $0 contains the original version string.
    ${Else}
      StrCpy $0 "NT4"
    ${EndIf}
  ${EndIf}

  Pop $1
  Exch $0

FunctionEnd
!macroend
!ifndef UNINSTALLER_ONLY
  !insertmacro GetWindowsVersionEx ""
!endif
!insertmacro GetWindowsVersionEx "un."


!ifndef UNINSTALLER_ONLY
!macro GetAdditionsVersion un
;;
; Retrieves the installed Guest Additions version.
;
; Input:
;   None
; Output:
;   Will store the results in $g_strAddVerMaj, $g_strAddVerMin, $g_strAddVerBuild.
;
Function ${un}GetAdditionsVersion

  Push $0
  Push $1

  ; Get additions version.
  ReadRegStr $0 HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "Version"

  ; Get revision.
  ReadRegStr $g_strAddVerRev HKLM "${REGISTRY_KEY_PRODUCT_ROOT}" "Revision"

  ; Extract major version.
  ; Param "$g_strAddVerMaj"   ; Result string
  ; Param "$0"                ; String to search for
  ; Param "."                 ; SubString
  ; Param ">"                 ; SearchDirection
  ; Param "<"                 ; StrInclusionDirection
  ; Param "0"                 ; IncludeSubString
  ; Param "0"                 ; Loops
  ; Param "0"                 ; CaseSensitive
  ${${un}StrStrAdv} "$g_strAddVerMaj" "$0" "." ">" "<" "0" "0" "0"

  ; Extract minor version.
  ; Param "$1"                ; Result string
  ; Param "$0"                ; String
  ; Param "."                 ; SubString
  ; Param ">"                 ; SearchDirection
  ; Param ">"                 ; StrInclusionDirection
  ; Param "0"                 ; IncludeSubString
  ; Param "0"                 ; Loops
  ; Param "0"                 ; CaseSensitive
  ${${un}StrStrAdv} "$1" "$0" "." ">" ">" "0" "0" "0"

  ; Param "$g_strAddVerMin"   ; Result string
  ; Param "$1"                ; String
  ; Param "."                 ; SubString
  ; Param ">"                 ; SearchDirection
  ; Param "<"                 ; StrInclusionDirection
  ; Param "0"                 ; IncludeSubString
  ; Param "0"                 ; Loops
  ; Param "0"                 ; CaseSensitive
  ${${un}StrStrAdv} "$g_strAddVerMin" "$1" "." ">" "<" "0" "0" "0"

  ; Extract build number.
  ; Param "$g_strAddVerBuild" ; Result string
  ; Param "$0"                ; String
  ; Param "."                 ; SubString
  ; Param "<"                 ; SearchDirection
  ; Param ">"                 ; StrInclusionDirection
  ; Param "0"                 ; IncludeSubString
  ; Param "0"                 ; Loops
  ; Param "0"                 ; CaseSensitive
  ${${un}StrStrAdv} "$g_strAddVerBuild" "$0" "." "<" ">" "0" "0" "0"

  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro GetAdditionsVersion ""
!endif ; UNINSTALLER_ONLY


!macro StopVBoxService un
;;
; Stops VBoxService.
;
; Input:
;   None
; Output:
;   None
;
Function ${un}StopVBoxService

  Push $0   ; Temp results
  Push $1
  Push $2   ; Image name of VBoxService
  Push $3   ; Safety counter

  StrCpy $3 "0" ; Init counter.
  ${LogVerbose} "Stopping VBoxService ..."

  ${LogVerbose} "Stopping VBoxService via SCM ..."
  ${If} $g_strWinVersion == "NT4"
    nsExec::Exec '"$SYSDIR\net.exe" stop VBoxService'
  ${Else}
    nsExec::Exec '"$SYSDIR\sc.exe" stop VBoxService'
  ${EndIf}
  Sleep "1000"           ; Wait a bit

!ifdef _DEBUG
  ${LogVerbose} "Stopping VBoxService (as exe) ..."
!endif

exe_stop_loop:

  IntCmp $3 10 exit      ; Only try this loop 10 times max.
  IntOp  $3 $3 + 1       ; Increment.

!ifdef _DEBUG
  ${LogVerbose} "Stopping attempt #$3"
!endif

  StrCpy $2 "VBoxService.exe"

  ${nsProcess::FindProcess} $2 $0
  StrCmp $0 0 0 exit

  ${nsProcess::KillProcess} $2 $0
  Sleep "1000" ; Wait a bit
  Goto exe_stop_loop

exit:

  ${LogVerbose} "Stopping VBoxService done"

  Pop $3
  Pop $2
  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro StopVBoxService ""
!insertmacro StopVBoxService "un."


!macro StopVBoxTray un
Function ${un}StopVBoxTray

  Push $0   ; Temp results
  Push $1   ; Safety counter

  StrCpy $1 "0" ; Init counter
  ${LogVerbose} "Stopping VBoxTray ..."

exe_stop:

  IntCmp $1 10 exit      ; Only try this loop 10 times max
  IntOp  $1 $1 + 1       ; Increment

  ${nsProcess::FindProcess} "VBoxTray.exe" $0
  StrCmp $0 0 0 exit

  ${nsProcess::KillProcess} "VBoxTray.exe" $0
  Sleep "1000"           ; Wait a bit
  Goto exe_stop

exit:

  ${LogVerbose} "Stopping VBoxTray done"

  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro StopVBoxTray ""
!insertmacro StopVBoxTray "un."


!macro StopVBoxMMR un
Function ${un}StopVBoxMMR

  Push $0   ; Temp results
  Push $1   ; Safety counter

  StrCpy $1 "0" ; Init counter
  DetailPrint "Stopping VBoxMMR ..."

exe_stop:

  IntCmp $1 10 exit      ; Only try this loop 10 times max
  IntOp  $1 $1 + 1       ; Increment

  ${nsProcess::FindProcess} "VBoxMMR.exe" $0
  StrCmp $0 0 0 exit

  ${nsProcess::KillProcess} "VBoxMMR.exe" $0
  Sleep "1000"           ; Wait a bit
  Goto exe_stop

exit:

  DetailPrint "Stopping VBoxMMR done."

  Pop $1
  Pop $0

FunctionEnd
!macroend
!insertmacro StopVBoxMMR ""
!insertmacro StopVBoxMMR "un."


!macro WriteRegBinR ROOT KEY NAME VALUE
  WriteRegBin "${ROOT}" "${KEY}" "${NAME}" "${VALUE}"
!macroend


;;
; Sets $g_bCapDllCache, $g_bCapXPDM, $g_bWithWDDM and $g_bCapWDDM.
;
; Input:
;   None
; Output:
;   None
;
!macro CheckForCapabilities un
Function ${un}CheckForCapabilities

  Push $0

  ; Retrieve system mode and store result in.
  System::Call 'user32::GetSystemMetrics(i ${SM_CLEANBOOT}) i .r0'
  StrCpy $g_iSystemMode $0

  ; Does the guest have a DLL cache?
  ${If}   $g_strWinVersion == "NT4"     ; bird: NT4 doesn't have a DLL cache, WTP is 5.0 <= NtVersion < 6.0.
  ${OrIf} $g_strWinVersion == "2000"
  ${OrIf} $g_strWinVersion == "XP"
    StrCpy $g_bCapDllCache "true"
    ${LogVerbose}  "OS has a DLL cache"
  ${EndIf}

  ${If}   $g_strWinVersion == "2000"
  ${OrIf} $g_strWinVersion == "XP"
  ${OrIf} $g_strWinVersion == "2003"
  ${OrIf} $g_strWinVersion == "Vista"
  ${OrIf} $g_strWinVersion == "7"
    StrCpy $g_bCapXPDM "true"
    ${LogVerbose} "OS is XPDM driver capable"
  ${EndIf}

!if $%VBOX_WITH_WDDM% == "1"
  ; By default use the WDDM driver on Vista+.
  ${If}   $g_strWinVersion == "Vista"
  ${OrIf} $g_strWinVersion == "7"
  ${OrIf} $g_strWinVersion == "8"
  ${OrIf} $g_strWinVersion == "8_1"
  ${OrIf} $g_strWinVersion == "10"
    StrCpy $g_bWithWDDM "true"
    StrCpy $g_bCapWDDM "true"
    ${LogVerbose} "OS is WDDM driver capable"
  ${EndIf}
!endif

  Pop $0

FunctionEnd
!macroend
!ifndef UNINSTALLER_ONLY
  !insertmacro CheckForCapabilities ""
!endif
!insertmacro CheckForCapabilities "un."


;;
; Switches (back) the path + registry view to 32-bit mode (SysWOW64)
; for 64-bit Intel + ARM guests.
;
; Input:
;   None
; Output:
;   None
;
!macro SetAppMode32 un
Function ${un}SetAppMode32
  !if $%KBUILD_TARGET_ARCH% != "x86" ; amd64 + arm64
    ${LogVerbose} "Setting application mode to 32-bit"
    ${EnableX64FSRedirection}
    SetRegView 32
  !endif
FunctionEnd
!macroend
!ifndef UNINSTALLER_ONLY
  !insertmacro SetAppMode32 ""
  !insertmacro SetAppMode32 "un."
!endif


;;
; Sets the installer's application mode.
;
; Because this NSIS installer is always built for x86 (32-bit), we have to
; do some tricks for the Windows paths + registry on Intel + ARM 64-bit guests.
;
; Input:
;   None
; Output:
;   None
;
!macro SetAppMode64 un
Function ${un}SetAppMode64
  !if $%KBUILD_TARGET_ARCH% != "x86" ; amd64 + arm64
    ${LogVerbose} "Setting application mode to 64-bit"
    ${DisableX64FSRedirection}
    SetRegView 64
  !endif
FunctionEnd
!macroend
!insertmacro SetAppMode64 ""
!insertmacro SetAppMode64 "un."


;;
; Retrieves the vendor ("CompanyName" of FILEINFO structure)
; of a given file.
;
; Input:
;   Stack[0]: File name to retrieve vendor for.
; Output:
;   Stack[0]: Company name, or "" on error/if not found.
;
!macro GetFileVendor un
Function ${un}GetFileVendor

  ; Preserve values
  Exch $0 ; Stack: $0 <filename> (Get file name into $0)
  Push $1

  IfFileExists "$0" found
  Goto not_found

found:

  VBoxGuestInstallHelper::FileGetVendor "$0"
  ; Stack: <vendor> $1 $0
  Pop  $0 ; Get vendor
  Pop  $1 ; Restore $1
  Exch $0 ; Restore $0, push vendor on top of stack
  Goto end

not_found:

  Pop $1
  Pop $0
  Push "File not found"
  Goto end

end:

FunctionEnd
!macroend
!insertmacro GetFileVendor ""
!insertmacro GetFileVendor "un."


;;
; Retrieves the architecture of a given file.
;
; Input:
;   Stack[0]: File name to retrieve architecture for.
; Output:
;   Stack[0]: Architecture ("x86", "amd64") or error message.
;
!macro GetFileArchitecture un
Function ${un}GetFileArchitecture

  ; Preserve values
  Exch $0 ; Stack: $0 <filename> (Get file name into $0)
  Push $1

  IfFileExists "$0" found
  Goto not_found

found:

  ${LogVerbose} "Getting architecture of file $\"$0$\" ..."

  VBoxGuestInstallHelper::FileGetArchitecture "$0"

  ; Stack: <architecture> $1 $0
  Pop  $0 ; Get architecture string

  ${LogVerbose} "Architecture is: $0"

  Pop  $1 ; Restore $1
  Exch $0 ; Restore $0, push vendor on top of stack
  Goto end

not_found:

  Pop $1
  Pop $0
  Push "File not found"
  Goto end

end:

FunctionEnd
!macroend
!insertmacro GetFileArchitecture ""
!insertmacro GetFileArchitecture "un."


;;
; Verifies a given file by checking its file vendor and target
; architecture.
;
; Input:
;   Stack[0]: Architecture ("x86" or "amd64").
;   Stack[1]: Vendor.
;   Stack[2]: File name to verify.
; Output:
;   Stack[0]: "0" if valid, "1" if not, "2" on error / not found.
;
!macro VerifyFile un
Function ${un}VerifyFile

  ; Preserve values
  Exch $0 ; File;         S: old$0 vendor arch
  Exch    ;               S: vendor old$0 arch
  Exch $1 ; Vendor;       S: old$1 old$0 arch
  Exch    ;               S: old$0 old$1 arch
  Exch 2  ;               S: arch old$1 old$0
  Exch $2 ; Architecture; S: old$2 old$1 old$0
  Push $3 ;               S: old$3 old$2 old$1 old$0

  ${LogVerbose} "Verifying file $\"$0$\" (vendor: $1, arch: $2) ..."

  IfFileExists "$0" check_arch
  Goto not_found

check_arch:

  ${LogVerbose} "File $\"$0$\" found"

  Push $0
  Call ${un}GetFileArchitecture
  Pop $3

  ${LogVerbose} "Architecture is: $3"

  ${If} $3 == $2
    Goto check_vendor
  ${EndIf}
  Goto invalid

check_vendor:

  Push $0
  Call ${un}GetFileVendor
  Pop $3

  ${LogVerbose} "Vendor is: $3"

  ${If} $3 == $1
    Goto valid
  ${EndIf}

invalid:

  ${LogVerbose} "File $\"$0$\" is invalid"

  StrCpy $3 "1" ; Invalid
  Goto end

valid:

  ${LogVerbose} "File $\"$0$\" is valid"

  StrCpy $3 "0" ; Valid
  Goto end

not_found:

  ${LogVerbose} "File $\"$0$\" was not found"

  StrCpy $3 "2" ; Not found
  Goto end

end:

  ; S: old$3 old$2 old$1 old$0
  Exch $3 ; S: $3 old$2 old$1 old$0
  Exch    ; S: old$2 $3 old$1
  Pop $2  ; S: $3 old$1 old$0
  Exch    ; S: old$1 $3 old$0
  Pop $1  ; S: $3 old$0
  Exch    ; S: old$0 $3
  Pop $0  ; S: $3

FunctionEnd
!macroend
!insertmacro VerifyFile ""
!insertmacro VerifyFile "un."


;;
; Macro for accessing VerifyFile in a more convenient way by using
; a parameter list.
;
; @return  Stack: "0" if valid, "1" if not, "2" on error / not found.
; @param   Un/Installer prefix; either "" or "un".
; @param   Name of file to verify.
; @param   Vendor to check for.
; @param   Architecture ("x86" or "amd64") to check for.
;
!macro VerifyFileEx un File Vendor Architecture
  Push $0
  Push "${Architecture}"
  Push "${Vendor}"
  Push "${File}"
  Call ${un}VerifyFile
  Pop $0
  ${If} $0 == "0"
    ${LogVerbose} "Verification of file $\"${File}$\" successful (Vendor: ${Vendor}, Architecture: ${Architecture})"
  ${ElseIf} $0 == "1"
    ${LogVerbose} "Verification of file $\"${File}$\" failed (not Vendor: ${Vendor}, and/or not Architecture: ${Architecture})"
  ${Else}
    ${LogVerbose} "Skipping to file $\"${File}$\"; not found"
  ${EndIf}
  ; Push result popped off the stack to stack again
  Push $0
!macroend
!define VerifyFileEx "!insertmacro VerifyFileEx"


;;
; Macro for copying a file only if the source file is verified
; to be from a certain vendor and architecture.
;
; @return  Stack: "0" if copied, "1" if not, "2" on error / not found.
; @param   Un/Installer prefix; either "" or "un".
; @param   Name of file to verify and copy to destination.
; @param   Destination name to copy verified file to.
; @param   Vendor to check for.
; @param   Architecture ("x86" or "amd64") to check for.
;
!macro CopyFileEx un FileSrc FileDest Vendor Architecture
  Push $0
  Push "${Architecture}"
  Push "${Vendor}"
  Push "${FileSrc}"
  Call ${un}VerifyFile
  Pop $0

  Push "${Architecture}"
  Push "Oracle Corporation"
  Push "${FileDest}"
  Call ${un}VerifyFile
  Pop $0

  ${If} $0 == "0"
    ${LogVerbose} "Copying verified file $\"${FileSrc}$\" to $\"${FileDest}$\" ..."
    ClearErrors
    SetOverwrite on
    CopyFiles /SILENT "${FileSrc}" "${FileDest}"
    ${If} ${Errors}
      CreateDirectory "$TEMP\${PRODUCT_NAME}"
      ${GetFileName} "${FileSrc}" $0 ; Get the base name
      CopyFiles /SILENT "${FileSrc}" "$TEMP\${PRODUCT_NAME}\$0"
      ${LogVerbose} "Immediate installation failed, postponing to next reboot (temporary location is: $\"$TEMP\${PRODUCT_NAME}\$0$\") ..."
      ;${InstallFileEx} "${un}" "${FileSrc}" "${FileDest}" "$TEMP" ; Only works with compile time files!
      System::Call "kernel32::MoveFileEx(t '$TEMP\${PRODUCT_NAME}\$0', t '${FileDest}', i 5)"
    ${EndIf}
  ${Else}
    ${LogVerbose} "Skipping to copy file $\"${FileSrc}$\" to $\"${FileDest}$\" (not Vendor: ${Vendor}, Architecture: ${Architecture})"
  ${EndIf}
  ; Push result popped off the stack to stack again
  Push $0
!macroend
!define CopyFileEx "!insertmacro CopyFileEx"


;;
; Macro for installing a library/DLL.
;
; @return  Stack: "0" if copied, "1" if not, "2" on error / not found.
; @param   Un/Installer prefix; either "" or "un".
; @param   Name of lib/DLL to copy to destination.
; @param   Destination name to copy the source file to.
; @param   Temporary folder used for exchanging the (locked) lib/DLL after a reboot.
;
!macro InstallFileEx un FileSrc FileDest DirTemp
  ${LogVerbose} "Installing library $\"${FileSrc}$\" to $\"${FileDest}$\" ..."
  ; Try the gentle way and replace the file instantly
  !insertmacro InstallLib DLL NOTSHARED NOREBOOT_NOTPROTECTED "${FileSrc}" "${FileDest}" "${DirTemp}"
  ; If the above call didn't help, use a (later) reboot to replace the file
  !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED "${FileSrc}" "${FileDest}" "${DirTemp}"
!macroend
!define InstallFileEx "!insertmacro InstallFileEx"


;;
; Macro for installing a library/DLL.
;
; @return  Stack: "0" if copied, "1" if not, "2" on error / not found.
; @param   Un/Installer prefix; either "" or "un".
; @param   Name of lib/DLL to verify and copy to destination.
; @param   Destination name to copy verified file to.
; @param   Temporary folder used for exchanging the (locked) lib/DLL after a reboot.
; @param   Vendor to check for.
; @param   Architecture ("x86" or "amd64") to check for.
;
!macro InstallFileVerify un FileSrc FileDest DirTemp Vendor Architecture
  Push $0
  Push "${Architecture}"
  Push "${Vendor}"
  Push "${FileSrc}"
  ${LogVerbose} "Verifying library $\"${FileSrc}$\" ..."
  Call ${un}VerifyFile
  Pop $0
  ${If} $0 == "0"
    ${InstallFileEx} ${un} ${FileSrc} ${FileDest} ${DirTemp}
  ${Else}
    ${LogVerbose} "File $\"${FileSrc}$\" did not pass verification (Vendor: ${Vendor}, Architecture: ${Architecture})"
  ${EndIf}
  ; Push result popped off the stack to stack again.
  Push $0
!macroend
!define InstallFileVerify "!insertmacro InstallFileVerify"


;;
; Function which restores formerly backed up Direct3D original files, which were replaced by
; a VBox XPDM driver installation before. This might be necessary for upgrading a
; XPDM installation to a WDDM one.
;
; @return  Stack: "0" if files were restored successfully; otherwise "1".
;
; Note: We don't ship modified Direct3D files anymore, but we need to (try to)
;       restore the original (backed up) DLLs when upgrading from an old(er)
;       installation.
;
!macro RestoreFilesDirect3D un
Function ${un}RestoreFilesDirect3D
  ${If}  $g_bCapXPDM != "true"
      ${LogVerbose} "RestoreFilesDirect3D: XPDM is not supported"
      Return
  ${EndIf}

  Push $0

  ; We need to switch to 64-bit app mode to handle the "real" 64-bit files in
  ; "system32" on a 64-bit guest.
  Call ${un}SetAppMode64

  ${LogVerbose} "Restoring original D3D files ..."
  ${CopyFileEx} "${un}" "$SYSDIR\msd3d8.dll" "$SYSDIR\d3d8.dll" "Microsoft Corporation" "$%KBUILD_TARGET_ARCH%"
  ${CopyFileEx} "${un}" "$SYSDIR\msd3d9.dll" "$SYSDIR\d3d9.dll" "Microsoft Corporation" "$%KBUILD_TARGET_ARCH%"

  ${If} $g_bCapDllCache == "true"
    ${CopyFileEx} "${un}" "$SYSDIR\dllcache\msd3d8.dll" "$SYSDIR\dllcache\d3d8.dll" "Microsoft Corporation" "$%KBUILD_TARGET_ARCH%"
    ${CopyFileEx} "${un}" "$SYSDIR\dllcache\msd3d9.dll" "$SYSDIR\dllcache\d3d9.dll" "Microsoft Corporation" "$%KBUILD_TARGET_ARCH%"
  ${EndIf}

!if $%KBUILD_TARGET_ARCH% == "amd64"
  ${CopyFileEx} "${un}" "$g_strSysWow64\msd3d8.dll" "$g_strSysWow64\d3d8.dll" "Microsoft Corporation" "x86"
  ${CopyFileEx} "${un}" "$g_strSysWow64\msd3d9.dll" "$g_strSysWow64\d3d9.dll" "Microsoft Corporation" "x86"

  ${If} $g_bCapDllCache == "true"
    ${CopyFileEx} "${un}" "$g_strSysWow64\dllcache\msd3d8.dll" "$g_strSysWow64\dllcache\d3d8.dll" "Microsoft Corporation" "x86"
    ${CopyFileEx} "${un}" "$g_strSysWow64\dllcache\msd3d9.dll" "$g_strSysWow64\dllcache\d3d9.dll" "Microsoft Corporation" "x86"
  ${EndIf}
!endif

  Pop $0

FunctionEnd
!macroend
!insertmacro RestoreFilesDirect3D ""
!insertmacro RestoreFilesDirect3D "un."
