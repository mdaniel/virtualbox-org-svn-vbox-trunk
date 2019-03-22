; $Id$
; @file
; VBoxGuestAdditions.nsi - Main file for Windows Guest Additions installation.
;

;
; Copyright (C) 2012-2019 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

!if $%BUILD_TYPE% == "debug"
  !define _DEBUG     ; Turn this on to get extra output
!endif

!ifdef _DEBUG
  ; Scratch directory for plugin tests
  !addincludedir .\PluginTest
  !addplugindir .\PluginTest
!endif

; Defines for special functions
!define WHQL_FAKE                   ; Enables faking of non WHQL signed / approved drivers
                                    ; Needs the VBoxWHQLFake.exe in the additions output directory!
!define WFP_FILE_EXCEPTION          ; Enables setting a temporary file exception for WFP proctected files

!define VENDOR_ROOT_KEY             "SOFTWARE\$%VBOX_VENDOR_SHORT%"

; Product defines
!define PRODUCT_NAME                "$%VBOX_PRODUCT% Guest Additions"
!define PRODUCT_DESC                "$%VBOX_PRODUCT% Guest Additions"
!define PRODUCT_VERSION             "$%VBOX_VERSION_MAJOR%.$%VBOX_VERSION_MINOR%.$%VBOX_VERSION_BUILD%.0"
!define PRODUCT_PUBLISHER           "$%VBOX_VENDOR%"
!define PRODUCT_COPYRIGHT           "(C) $%VBOX_C_YEAR% $%VBOX_VENDOR%"
!define PRODUCT_OUTPUT              "VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%.exe"
!define PRODUCT_WEB_SITE            "http://www.virtualbox.org"
!define PRODUCT_INSTALL_KEY         "${VENDOR_ROOT_KEY}\VirtualBox Guest Additions"
!define PRODUCT_UNINST_KEY          "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY     "HKLM"

!define LICENSE_FILE_RTF            "license.rtf"

; Needed for InstallLib macro: Install libraries in every case
!define LIBRARY_IGNORE_VERSION

VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "FileVersion"       "$%VBOX_VERSION_STRING%"
VIAddVersionKey "ProductName"       "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion"    "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName"       "${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription"   "${PRODUCT_DESC}"
VIAddVersionKey "LegalCopyright"    "${PRODUCT_COPYRIGHT}"
VIAddVersionKey "InternalName"      "${PRODUCT_OUTPUT}"

; This registry key will hold the mouse driver path before install (NT4 only)
!define ORG_MOUSE_PATH "MousePath"

; If we have our guest install helper DLL, add the
; plugin path so that NSIS can find it when compiling the installer
; Note: NSIS plugins *always* have to be compiled in 32-bit!
!if $%VBOX_WITH_GUEST_INSTALL_HELPER% == "1"
  !addplugindir "$%PATH_TARGET_X86%\VBoxGuestInstallHelper"
!endif

!include "LogicLib.nsh"
!include "FileFunc.nsh"
  !insertmacro GetParameters
  !insertmacro GetOptions
!include "WordFunc.nsh"
  !insertmacro WordFind
  !insertmacro StrFilter

!include "nsProcess.nsh"
!include "Library.nsh"
!include "Sections.nsh"
!include "strstr.nsh"         ; Function "strstr"
!include "servicepack.nsh"    ; Function "GetServicePack"
!include "winver.nsh"         ; Function for determining Windows version
!define REPLACEDLL_NOREGISTER ; Replace in use DLL function
!include "ReplaceDLL.nsh"

!if $%BUILD_TARGET_ARCH% == "amd64"
  !include "x64.nsh"
!endif

; Set Modern UI (MUI) as default
!define USE_MUI

!ifdef USE_MUI
  ; Use modern UI, version 2
  !include "MUI2.nsh"

  ; MUI Settings
  !define MUI_WELCOMEFINISHPAGE_BITMAP "$%VBOX_BRAND_WIN_ADD_INST_DLGBMP%"
  !define MUI_ABORTWARNING
  !define MUI_WELCOMEPAGE_TITLE_3LINES "Welcome to the ${PRODUCT_NAME} Additions Setup"

  ; API defines
  !define SM_CLEANBOOT 67

  ; Icons
  !if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
    !define MUI_ICON "$%VBOX_NSIS_ICON_FILE%"
    !define MUI_UNICON "$%VBOX_NSIS_ICON_FILE%"
  !else   ; 64-bit
    !define MUI_ICON "$%VBOX_WINDOWS_ADDITIONS_ICON_FILE%"
    !define MUI_UNICON "$%VBOX_WINDOWS_ADDITIONS_ICON_FILE%"
  !endif

  ; Welcome page
  !insertmacro MUI_PAGE_WELCOME
!ifdef VBOX_WITH_LICENSE_DISPLAY
  ; License page
  !insertmacro MUI_PAGE_LICENSE "$(VBOX_LICENSE)"
  !define MUI_LICENSEPAGE_RADIOBUTTONS
!endif
  ; Directory page
  !insertmacro MUI_PAGE_DIRECTORY
  ; Components Page
  !insertmacro MUI_PAGE_COMPONENTS
  ; Instfiles page
  !insertmacro MUI_PAGE_INSTFILES

  !ifndef _DEBUG
    !define MUI_FINISHPAGE_TITLE_3LINES   ; Have a bit more vertical space for text
    !insertmacro MUI_PAGE_FINISH          ; Only show in release mode - useful information for debugging!
  !endif

  ; Uninstaller pages
  !insertmacro MUI_UNPAGE_INSTFILES

  ; Define languages we will use
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"

  ; Set branding text which appears on the horizontal line at the bottom
!ifdef _DEBUG
  BrandingText "VirtualBox Windows Additions $%VBOX_VERSION_STRING% (r$%VBOX_SVN_REV%) - Debug Build"
!else
  BrandingText "VirtualBox Windows Additions $%VBOX_VERSION_STRING% r$%VBOX_SVN_REV%"
!endif

!ifdef VBOX_WITH_LICENSE_DISPLAY
  ; Set license language
  LicenseLangString VBOX_LICENSE ${LANG_ENGLISH} "$%VBOX_BRAND_LICENSE_RTF%"

  ; If license files not available (OSE / PUEL) build, then use the English one as default
  !ifdef VBOX_BRAND_fr_FR_LICENSE_RTF
    LicenseLangString VBOX_LICENSE ${LANG_FRENCH} "$%VBOX_BRAND_fr_FR_LICENSE_RTF%"
  !else
    LicenseLangString VBOX_LICENSE ${LANG_FRENCH} "$%VBOX_BRAND_LICENSE_RTF%"
  !endif
  !ifdef VBOX_BRAND_de_DE_LICENSE_RTF
    LicenseLangString VBOX_LICENSE ${LANG_GERMAN} "$%VBOX_BRAND_de_DE_LICENSE_RTF%"
  !else
    LicenseLangString VBOX_LICENSE ${LANG_GERMAN} "$%VBOX_BRAND_LICENSE_RTF%"
  !endif
!endif

  !insertmacro MUI_RESERVEFILE_LANGDLL
!else ; !USE_MUI
    XPStyle on
!ifdef VBOX_WITH_LICENSE_DISPLAY
    Page license
!endif
    Page components
    Page directory
    Page instfiles
!endif

; Must come after MUI includes to have certain defines set for DumpLog
!include "dumplog.nsh"        ; Dump log to file function

; Language files
!include "Languages\English.nsh"
!include "Languages\French.nsh"
!include "Languages\German.nsh"

; Variables and output files
Name "${PRODUCT_NAME} $%VBOX_VERSION_STRING%"
!ifdef UNINSTALLER_ONLY
  !echo "Uninstaller only!"
  OutFile "$%PATH_TARGET%\VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%-uninst.exe"
!else
  OutFile "VBoxWindowsAdditions-$%BUILD_TARGET_ARCH%.exe"
!endif ; UNINSTALLER_ONLY

; Define default installation directory
!if $%BUILD_TARGET_ARCH% == "x86" ; 32-bit
  InstallDir  "$PROGRAMFILES32\$%VBOX_VENDOR_SHORT%\VirtualBox Guest Additions"
!else       ; 64-bit
  InstallDir  "$PROGRAMFILES64\$%VBOX_VENDOR_SHORT%\VirtualBox Guest Additions"
!endif

InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel highest

; Whether to use the Unicode version of NSIS
; Note: Using Unicode will result in the installer not working on a Windows 95/98/ME guest
;Unicode true

; Internal parameters
Var g_iSystemMode                       ; Current system mode (0 = Normal boot, 1 = Fail-safe boot, 2 = Fail-safe with network boot)
Var g_strSystemDir                      ; Windows system directory
Var g_strSysWow64                       ; The SysWow64 directory on 64-bit systems
Var g_strCurUser                        ; Current user using the system
Var g_strAddVerMaj                      ; Installed Guest Additions: Major version
Var g_strAddVerMin                      ; Installed Guest Additions: Minor version
Var g_strAddVerBuild                    ; Installed Guest Additions: Build number
Var g_strAddVerRev                      ; Installed Guest Additions: SVN revision
Var g_strWinVersion                     ; Current Windows version we're running on
Var g_bLogEnable                        ; Do logging when installing? "true" or "false"
Var g_bWithWDDM                         ; Install the WDDM graphics driver instead of the XPDM one
Var g_bCapDllCache                      ; Capability: Does the (Windows) guest have have a DLL cache which needs to be taken care of?
Var g_bCapWDDM                          ; Capability: Is the guest able to handle/use our WDDM driver?

; Command line parameters - these can be set/modified
; on the command line
Var g_bFakeWHQL                         ; Cmd line: Fake Windows to install non WHQL certificated drivers (only for W2K and XP currently!!) ("/unsig_drv")
Var g_bForceInstall                     ; Cmd line: Force installation on unknown Windows OS version
Var g_bUninstall                        ; Cmd line: Just uninstall any previous Guest Additions and exit
Var g_bRebootOnExit                     ; Cmd line: Auto-Reboot on successful installation. Good for unattended installations ("/reboot")
Var g_iScreenBpp                        ; Cmd line: Screen depth ("/depth=X")
Var g_iScreenX                          ; Cmd line: Screen resolution X ("/resx=X")
Var g_iScreenY                          ; Cmd line: Screen resolution Y ("/resy=Y")
Var g_iSfOrder                          ; Cmd line: Order of Shared Folders network provider (0=first, 1=second, ...)
Var g_bIgnoreUnknownOpts                ; Cmd line: Ignore unknown options (don't display the help)
Var g_bNoVBoxServiceExit                ; Cmd line: Do not quit VBoxService before updating - install on next reboot
Var g_bNoVBoxTrayExit                   ; Cmd line: Do not quit VBoxTray before updating - install on next reboot
Var g_bNoVideoDrv                       ; Cmd line: Do not install the VBoxVideo driver
Var g_bNoGuestDrv                       ; Cmd line: Do not install the VBoxGuest driver
Var g_bNoMouseDrv                       ; Cmd line: Do not install the VBoxMouse driver
Var g_bNoStartMenuEntries               ; Cmd line: Do not create start menu entries
Var g_bWithAutoLogon                    ; Cmd line: Install VBoxGINA / VBoxCredProv for auto logon support
Var g_bWithD3D                          ; Cmd line: Install Direct3D support
Var g_bOnlyExtract                      ; Cmd line: Only extract all files, do *not* install them. Only valid with param "/D" (target directory)
Var g_bPostInstallStatus                ; Cmd line: Post the overall installation status to some external program (VBoxTray)

; Platform parts of this installer
!include "VBoxGuestAdditionsLog.nsh"
!include "VBoxGuestAdditionsExternal.nsh"
!include "VBoxGuestAdditionsCommon.nsh"
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit only
  !include "VBoxGuestAdditionsNT4.nsh"
!endif
!include "VBoxGuestAdditionsW2KXP.nsh"
!include "VBoxGuestAdditionsVista.nsh"
!include "VBoxGuestAdditionsUninstall.nsh"    ; Product uninstallation
!include "VBoxGuestAdditionsUninstallOld.nsh" ; Uninstallation of deprecated versions which must be removed first

Function HandleCommandLine

  Push $0                                     ; Command line (without process name)
  Push $1                                     ; Number of parameters
  Push $2                                     ; Current parameter index
  Push $3                                     ; Current parameter pair (name=value)
  Push $4                                     ; Current parameter name
  Push $5                                     ; Current parameter value (if present)

  StrCpy $1 "0"                               ; Init param counter
  StrCpy $2 "1"                               ; Init current param counter

  ${GetParameters} $0                         ; Extract command line
  ${If} $0 == ""                              ; If no parameters at all exit
    Goto exit
  ${EndIf}

  ; Enable for debugging
  ;MessageBox MB_OK "CmdLine: $0"

  ${WordFind} $0 " " "#" $1                   ; Get number of parameters in cmd line
  ${If} $0 == $1                              ; If result matches the input then
    StrCpy $1 "1"                             ; no delimiter was found. Correct to 1 word total
  ${EndIf}

  ${While} $2 <= $1                           ; Loop through all params

    ${WordFind} $0 " " "+$2" $3               ; Get current name=value pair
    ${WordFind} $3 "=" "+1" $4                ; Get current param name
    ${WordFind} $3 "=" "+2" $5                ; Get current param value

    ${StrFilter} $4 "-" "" "" $4              ; Transfer param name to lowercase

    ; Enable for debugging
    ;MessageBox MB_OK "#$2 of #$1, param='$3', name=$4, val=$5"

    ${Switch} $4

      ${Case} '/d' ; NSIS: /D=<instdir> switch, skip
        ${Break}

      ${Case} '/depth'
      ${Case} 'depth'
        StrCpy $g_iScreenBpp $5
        ${Break}

      ${Case} '/extract'
        StrCpy $g_bOnlyExtract "true"
        ${Break}

      ${Case} '/force'
        StrCpy $g_bForceInstall "true"
        ${Break}

      ${Case} '/help'
      ${Case} '/H'
      ${Case} '/h'
      ${Case} '/?'
        Goto usage
        ${Break}

      ${Case} '/ignore_unknownopts' ; Not officially documented
        StrCpy $g_bIgnoreUnknownOpts "true"
        ${Break}

      ${Case} '/l'
      ${Case} '/log'
      ${Case} '/logging'
        StrCpy $g_bLogEnable "true"
        ${Break}

      ${Case} '/ncrc' ; NSIS: /NCRC switch, skip
        ${Break}

      ${Case} '/no_vboxservice_exit' ; Not officially documented
        StrCpy $g_bNoVBoxServiceExit "true"
        ${Break}

      ${Case} '/no_vboxtray_exit' ; Not officially documented
        StrCpy $g_bNoVBoxTrayExit "true"
        ${Break}

      ${Case} '/no_videodrv' ; Not officially documented
        StrCpy $g_bNoVideoDrv "true"
        ${Break}

      ${Case} '/no_guestdrv' ; Not officially documented
        StrCpy $g_bNoGuestDrv "true"
        ${Break}

      ${Case} '/no_mousedrv' ; Not officially documented
        StrCpy $g_bNoMouseDrv "true"
        ${Break}

      ${Case} '/no_startmenuentries' ; Not officially documented
        StrCpy $g_bNoStartMenuEntries "true"
        ${Break}

!if $%VBOX_WITH_GUEST_INSTALL_HELPER% == "1"
      ; This switch tells our installer that it
      ; - should not quit VBoxTray during the update, because ...
      ; - ... it should show the overall installation status
      ;   using VBoxTray's balloon message feature (since VBox 4.0)
      ${Case} '/post_installstatus' ; Not officially documented
        StrCpy $g_bNoVBoxTrayExit "true"
        StrCpy $g_bPostInstallStatus "true"
        ${Break}
!endif

      ${Case} '/reboot'
        StrCpy $g_bRebootOnExit "true"
        ${Break}

      ${Case} '/s' ; NSIS: /S switch, skip
        ${Break}

      ${Case} '/sforder'
      ${Case} 'sforder'
        StrCpy $g_iSfOrder $5
        ${Break}

!ifdef WHQL_FAKE
      ${Case} '/unsig_drv'
        StrCpy $g_bFakeWHQL "true"
      ${Break}
!endif

      ${Case} '/uninstall'
        StrCpy $g_bUninstall "true"
        ${Break}

      ${Case} '/with_autologon'
        StrCpy $g_bWithAutoLogon "true"
        ${Break}

!if $%VBOX_WITH_CROGL% == "1"
      ${Case} '/with_d3d'
      ${Case} '/with_direct3d'
        StrCpy $g_bWithD3D "true"
        ${Break}
!endif

!if $%VBOX_WITH_WDDM% == "1"
      ${Case} '/with_wddm'
        StrCpy $g_bWithWDDM "true"
        ${Break}
!endif

      ${Case} '/xres'
      ${Case} 'xres'
        StrCpy $g_iScreenX $5
        ${Break}

      ${Case} '/yres'
      ${Case} 'yres'
        StrCpy $g_iScreenY $5
        ${Break}

      ${Default} ; Unknown parameter, print usage message
        ; Prevent popping up usage message on (yet) unknown parameters
        ; in silent mode, just skip
        IfSilent +1 +2
          ${Break}
        goto usage
        ${Break}

    ${EndSwitch}

next_param:

    IntOp $2 $2 + 1

  ${EndWhile}
  Goto exit

usage:

  ; If we were told to ignore unknown (invalid) options, just return to
  ; the parsing loop ...
  ${If} $g_bIgnoreUnknownOpts == "true"
    Goto next_param
  ${EndIf}
  MessageBox MB_OK "${PRODUCT_NAME} Installer$\r$\n$\r$\n \
                    Usage: VBoxWindowsAdditions-$%BUILD_TARGET_ARCH% [OPTIONS] [/l] [/S] [/D=<PATH>]$\r$\n$\r$\n \
                    Options:$\r$\n \
                    /depth=BPP$\tSets the guest's display color depth (bits per pixel)$\r$\n \
                    /extract$\t$\tOnly extract installation files$\r$\n \
                    /force$\t$\tForce installation on unknown/undetected Windows versions$\r$\n \
                    /uninstall$\t$\tJust uninstalls the Guest Additions and exits$\r$\n \
                    /with_autologon$\tInstalls auto-logon support$\r$\n \
                    /with_d3d$\tInstalls D3D support$\r$\n \
                    /with_wddm$\tInstalls the WDDM instead of the XPDM graphics driver$\r$\n \
                    /xres=X$\t$\tSets the guest's display resolution (width in pixels)$\r$\n \
                    /yres=Y$\t$\tSets the guest's display resolution (height in pixels)$\r$\n \
                    $\r$\n \
                    Installer parameters:$\r$\n \
                    /l$\t$\tEnables logging$\r$\n \
                    /S$\t$\tSilent install$\r$\n \
                    /D=<PATH>$\tSets the default install path$\r$\n \
                    $\r$\n \
                    Note: Order of options and installer parameters is fixed, options first." /SD IDOK

  ; No stack restore needed, we're about to quit
  Quit

done:

!ifdef _DEBUG
  ${LogVerbose} "Property: XRes: $g_iScreenX"
  ${LogVerbose} "Property: YRes: $g_iScreenY"
  ${LogVerbose} "Property: BPP: $g_iScreenBpp"
  ${LogVerbose} "Property: Logging enabled: $g_bLogEnable"
!endif

exit:

  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0

FunctionEnd

Function CheckForOldGuestAdditions

  Push $0
  Push $1
  Push $2

begin:

  ${LogVerbose} "Checking for old Guest Additions ..."

sun_check:

  ; Check for old "Sun VirtualBox Guest Additions"
  ; - before rebranding to Oracle
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" sun_xvm_check ; If string is empty, Sun additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_uninstall:

  Call Uninstall_Sun
  Goto success

sun_xvm_check:

  ; Check for old "Sun xVM VirtualBox Guest Additions"
  ; - before getting rid of the "xVM" namespace
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Sun xVM VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" innotek_check ; If string is empty, Sun xVM additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_SUN_FOUND) /SD IDYES IDYES sun_xvm_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_SUN_ABORTED) /SD IDOK
    Quit

sun_xvm_uninstall:

  Call Uninstall_SunXVM
  Goto success

innotek_check:

  ; Check for old "innotek" Guest Additions" before rebranding to "Sun"
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\innotek VirtualBox Guest Additions" "UninstallString"
  StrCmp $0 "" exit ; If string is empty, innotek Guest Additions are probably not installed (anymore)

  MessageBox MB_YESNO $(VBOX_INNOTEK_FOUND) /SD IDYES IDYES innotek_uninstall
    Pop $2
    Pop $1
    Pop $0
    MessageBox MB_ICONSTOP $(VBOX_INNOTEK_ABORTED) /SD IDOK
    Quit

innotek_uninstall:

  Call Uninstall_Innotek
  Goto success

success:

  ; Nothing to do here yet

exit:

  Pop $2
  Pop $1
  Pop $0

FunctionEnd

Function CheckForInstalledComponents

  Push $0
  Push $1

  ${LogVerbose} "Checking for installed components ..."
  StrCpy $1 ""

  Call SetAppMode64

  ; VBoxGINA already installed? So we need to update the installed version as well,
  ; regardless whether the user used "/with_autologon" or not
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${If} $0 == "VBoxGINA.dll"
    StrCpy $1 "GINA"
  ${Else}
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}" ""
    ${If} $0 == "VBoxCredProv"
      StrCpy $1 "Credential Provider"
    ${EndIf}
  ${EndIf}

!ifdef _DEBUG
  ${LogVerbose} "Auto-logon module: $0"
!endif

  ${IfNot} $1 == ""
    ${LogVerbose} "Auto-logon support ($1) was installed previously"
    StrCpy $g_bWithAutoLogon "true" ; Force update
  ${Else}
    ${LogVerbose} "Auto-logon support was not installed previously"
  ${EndIf}

  Pop $1
  Pop $0

FunctionEnd

; Main Files
Section $(VBOX_COMPONENT_MAIN) SEC01

  SectionIn RO ; Section cannot be unselected (read-only)
  ${If} $g_bPostInstallStatus == "true"
    ${LogToVBoxTray} "0" "${PRODUCT_NAME} update started, please wait ..."
  ${EndIf}

  IfSilent +1 +2
    StrCpy $g_bLogEnable "true" ; Force logging in silent mode

  ${LogEnable} "$g_bLogEnable"
  IfSilent +1 +2 ; NSIS will expand ${LogVerbose} before doing relative jumps!
    LogText "Installer runs in silent mode"

  SetOutPath "$INSTDIR"
  SetOverwrite on

  Call SetAppMode64

  StrCpy $g_strSystemDir "$SYSDIR"

  ${LogVerbose} "Version: $%VBOX_VERSION_STRING% (Rev $%VBOX_SVN_REV%)"
  ${If} $g_strAddVerMaj != ""
    ${LogVerbose} "Previous version: $g_strAddVerMaj.$g_strAddVerMin.$g_strAddVerBuild (Rev $g_strAddVerRev)"
  ${Else}
    ${LogVerbose} "No previous version of ${PRODUCT_NAME} detected"
  ${EndIf}
!if $%BUILD_TARGET_ARCH% == "amd64"
  ${LogVerbose} "Detected OS: Windows $g_strWinVersion (64-bit)"
!else
  ${LogVerbose} "Detected OS: Windows $g_strWinVersion (32-bit)"
!endif
  ${LogVerbose} "System Directory: $g_strSystemDir"

!ifdef _DEBUG
  ${LogVerbose} "Installer runs in debug mode"
!endif

  ;
  ; Here starts the main dispatcher (based on guest OS)
  ;

  ; Which OS are we using?
  ; @todo Use logic lib here
!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
  StrCmp $g_strWinVersion "NT4" nt4     ; Windows NT 4.0
!endif
  StrCmp $g_strWinVersion "2000" w2k    ; Windows 2000
  StrCmp $g_strWinVersion "XP" w2k      ; Windows XP
  StrCmp $g_strWinVersion "2003" w2k    ; Windows 2003 Server
  StrCmp $g_strWinVersion "Vista" vista ; Windows Vista
  StrCmp $g_strWinVersion "7" vista     ; Windows 7
  StrCmp $g_strWinVersion "8" vista     ; Windows 8
  StrCmp $g_strWinVersion "8_1" vista   ; Windows 8.1 / Windows 2012 Server R2
  StrCmp $g_strWinVersion "10" vista    ; Windows 10

  ${If} $g_bForceInstall == "true"
    Goto vista ; Assume newer OS than we know of ...
  ${EndIf}

  Goto notsupported

!if $%BUILD_TARGET_ARCH% == "x86"       ; 32-bit
nt4: ; Windows NT4

  Call GetServicePack
  Pop $R0   ; Major version
  Pop $R1   ; Minor version

  ; At least Service Pack 6 installed?
  ${If} $R0 <> "6"
    MessageBox MB_YESNO $(VBOX_NT4_NO_SP6) /SD IDYES IDYES +2
    Quit
  ${EndIf}

  ; Copy some common files ...
  Call Common_CopyFiles

  Call NT4_Main
  goto success
!endif

w2k: ; Windows 2000 and XP ...

  ; Copy some common files ...
  Call Common_CopyFiles

  Call W2K_Main
  goto success

vista: ; Windows Vista / Windows 7 / Windows 8(.1)

  ; Check requirments; this function can abort the installation if necessary!
  Call Vista_CheckForRequirements

  ; Copy some common files ...
  Call Common_CopyFiles

  Call W2K_Main     ; First install stuff from Windows 2000 / XP
  Call Vista_Main   ; ... and some specific stuff for Vista / Windows 7
  goto success

notsupported:

  MessageBox MB_ICONSTOP $(VBOX_PLATFORM_UNSUPPORTED) /SD IDOK
  goto exit

success:

  ; Write a registry key with version and installation path for later lookup
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "Version" "$%VBOX_VERSION_STRING_RAW%"
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "VersionExt" "$%VBOX_VERSION_STRING%"
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "Revision" "$%VBOX_SVN_REV%"
  WriteRegStr HKLM "${PRODUCT_INSTALL_KEY}" "InstallDir" "$INSTDIR"

  ; Set the reboot flag to tell the finish page that is should
  ; default to the "reboot now" entry
  SetRebootFlag true

exit:

SectionEnd

; Auto-logon support (section is hidden at the moment -- only can be enabled via command line switch)
Section /o -$(VBOX_COMPONENT_AUTOLOGON) SEC02

  Call SetAppMode64

  Call GetWindowsVersion
  Pop $R0 ; Windows Version

  ${LogVerbose} "Installing auto-logon support ..."

  ; Another GINA already is installed? Check if this is ours, otherwise let the user decide (unless it's a silent setup)
  ; whether to replace it with the VirtualBox one or not
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL"
  ${If} $0 != ""
    ${If} $0 != "VBoxGINA.dll"
      ${LogVerbose} "Found another already installed GINA module: $0"
      MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON1 $(VBOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY) /SD IDYES IDYES install
      ${LogVerbose} "Skipping GINA installation, keeping: $0"
      goto skip
    ${EndIf}
  ${EndIf}

install:

  ; Do we need VBoxCredProv or VBoxGINA?
  ${If}   $R0 == 'Vista' ; Windows Vista.
  ${OrIf} $R0 == '7'     ; Windows 7.
  ${OrIf} $R0 == '8'     ; Windows 8.
  ${OrIf} $R0 == '8_1'   ; Windows 8.1 / Windows Server 2012 R2.
  ${OrIf} $R0 == '10'    ; Windows 10.
    ; Use VBoxCredProv on Vista and up.
    ${LogVerbose} "Installing VirtualBox credential provider ..."
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxCredProv.dll" "$g_strSystemDir\VBoxCredProv.dll" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}" "" "VBoxCredProv" ; adding to (default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}" "" "VBoxCredProv"                       ; adding to (Default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}\InprocServer32" "" "VBoxCredProv.dll"    ; adding to (Default) key
    WriteRegStr HKCR "CLSID\{275D3BCC-22BB-4948-A7F6-3A3054EBA92B}\InprocServer32" "ThreadingModel" "Apartment"
  ${Else} ; Use VBoxGINA on older Windows OSes (< Vista)
    ${LogVerbose} "Installing VirtualBox GINA ..."
    !insertmacro ReplaceDLL "$%PATH_OUT%\bin\additions\VBoxGINA.dll" "$g_strSystemDir\VBoxGINA.dll" "$INSTDIR"
    WriteRegStr HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon" "GinaDLL" "VBoxGINA.dll"
    ; Add Windows notification package callbacks for VBoxGINA
    WriteRegStr   HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\VBoxGINA" "DLLName" "VBoxGINA.dll"
    WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\VBoxGINA" "Impersonate" 0
    WriteRegStr   HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinLogon\Notify\VBoxGINA" "StopScreenSaver" "WnpScreenSaverStop"
  ${EndIf}

skip:

  ; Nothing to do here right now

exit:

SectionEnd

; Direct3D support
Section /o $(VBOX_COMPONENT_D3D) SEC03

!if $%VBOX_WITH_WDDM% == "1"
  ${If} $g_bWithWDDM == "true"
    ${LogVerbose} "Installing WDDM Direct3D support ..."

    ; Do we need to restore the original d3d8.dll/d3d9.dll files because the guest
    ; installation was upgraded from XPDM to WDDM driver? In a XPDM installation
    ; those DLLs were replaced by our own stub files.
    Call RestoreFilesDirect3D
    Return
  ${EndIf}
!endif

  Call SetAppMode64
  SetOverwrite on

  ${If} $g_strSystemDir == ''
    StrCpy $g_strSystemDir "$SYSDIR"
  ${EndIf}

  SetOutPath $g_strSystemDir
  ${LogVerbose} "Installing XPDM Direct3D support ..."
  FILE "$%PATH_OUT%\bin\additions\VBoxD3D8.dll"
  FILE "$%PATH_OUT%\bin\additions\VBoxD3D9.dll"
  FILE "$%PATH_OUT%\bin\additions\wined3d.dll"

  ;
  ; Update DLL cache
  ;
  ${If} $g_bCapDllCache == "true"
    ${If} ${FileExists} "$g_strSystemDir\dllcache"
      SetOutPath "$g_strSystemDir\dllcache"
      ${CopyFileEx} "" "$g_strSystemDir\dllcache\d3d8.dll" "$g_strSystemDir\dllcache\msd3d8.dll" "Microsoft Corporation" "$%BUILD_TARGET_ARCH%"
      ${CopyFileEx} "" "$g_strSystemDir\dllcache\d3d9.dll" "$g_strSystemDir\dllcache\msd3d9.dll" "Microsoft Corporation" "$%BUILD_TARGET_ARCH%"

      ; Exchange DLLs
      ${PrepareWRPFileEx} "" "$g_strSystemDir\dllcache\d3d8.dll"
      ${InstallFileEx} "" "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\dllcache\d3d8.dll" "$TEMP"
      ${PrepareWRPFileEx} "" "$g_strSystemDir\dllcache\d3d9.dll"
      ${InstallFileEx} "" "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\dllcache\d3d9.dll" "$TEMP"
    ${Else}
        ${LogVerbose} "DLL cache does not exist, skipping"
    ${EndIf}
  ${EndIf}

  ;
  ; Save original DLLs (only if msd3d*.dll does not exist) ...
  ;
  SetOutPath $g_strSystemDir
  ${CopyFileEx} "" "$g_strSystemDir\d3d8.dll" "$g_strSystemDir\msd3d8.dll" "Microsoft Corporation" "$%BUILD_TARGET_ARCH%"
  ${CopyFileEx} "" "$g_strSystemDir\d3d9.dll" "$g_strSystemDir\msd3d9.dll" "Microsoft Corporation" "$%BUILD_TARGET_ARCH%"

  ; Exchange DLLs
  ${PrepareWRPFileEx} "" "$g_strSystemDir\d3d8.dll"
  ${InstallFileEx} "" "$%PATH_OUT%\bin\additions\d3d8.dll" "$g_strSystemDir\d3d8.dll" "$TEMP"
  ${PrepareWRPFileEx} "" "$g_strSystemDir\d3d9.dll"
  ${InstallFileEx} "" "$%PATH_OUT%\bin\additions\d3d9.dll" "$g_strSystemDir\d3d9.dll" "$TEMP"

!if $%BUILD_TARGET_ARCH% == "amd64"
    ; Only 64-bit installer:
    ; Also copy 32-bit DLLs on 64-bit Windows in SysWOW64 node
    SetOutPath $g_strSysWow64
    ${LogVerbose} "Installing Direct3D support for 32-bit applications (SysWOW64: $g_strSysWow64) ..."
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxD3D8.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\VBoxD3D9.dll"
    FILE "$%VBOX_PATH_ADDITIONS_WIN_X86%\wined3d.dll"

    ;
    ; Update DLL cache
    ;
    ${If} $g_bCapDllCache == "true"
      ${If} ${FileExists} "$g_strSysWow64\dllcache"
        SetOutPath "$g_strSysWow64\dllcache"
        ${CopyFileEx} "" "$g_strSysWow64\dllcache\d3d8.dll" "$g_strSysWow64\dllcache\msd3d8.dll" "Microsoft Corporation" "x86"
        ${CopyFileEx} "" "$g_strSysWow64\dllcache\d3d9.dll" "$g_strSysWow64\dllcache\msd3d9.dll" "Microsoft Corporation" "x86"

        ; Exchange DLLs
        ${PrepareWRPFileEx} "" "$g_strSysWow64\dllcache\d3d8.dll"
        ${InstallFileEx} "" "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$g_strSysWow64\dllcache\d3d8.dll" "$TEMP"
        ${PrepareWRPFileEx} "" "$g_strSysWow64\dllcache\d3d9.dll"
        ${InstallFileEx} "" "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$g_strSysWow64\dllcache\d3d9.dll" "$TEMP"
      ${Else}
        ${LogVerbose} "DLL cache does not exist, skipping"
      ${EndIf}
    ${EndIf}

    ;
    ; Update original DLLs
    ;

    ; Save original DLLs (only if msd3d*.dll does not exist) ...
    ${CopyFileEx} "" "$g_strSysWow64\d3d8.dll" "$g_strSysWow64\msd3d8.dll" "Microsoft Corporation" "x86"
    ${CopyFileEx} "" "$g_strSysWow64\d3d9.dll" "$g_strSysWow64\msd3d9.dll" "Microsoft Corporation" "x86"

    ${PrepareWRPFileEx} "" "$g_strSysWow64\d3d8.dll"
    ${InstallFileEx} "" "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d8.dll" "$g_strSysWow64\d3d8.dll" "$TEMP"
    ${PrepareWRPFileEx} "" "$g_strSysWow64\d3d9.dll"
    ${InstallFileEx} "" "$%VBOX_PATH_ADDITIONS_WIN_X86%\d3d9.dll" "$g_strSysWow64\d3d9.dll" "$TEMP"

!endif ; amd64
  Goto done

error:
  ; @todo
  Goto exit

done:

!ifndef WFP_FILE_EXCEPTION
  MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_WFP_WARN_REPLACE) /SD IDOK
!endif
  Goto exit

exit:

SectionEnd

!ifdef USE_MUI
  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT   ${SEC01} $(VBOX_COMPONENT_MAIN_DESC)
    !insertmacro MUI_DESCRIPTION_TEXT   ${SEC02} $(VBOX_COMPONENT_AUTOLOGON_DESC)
    !if $%VBOX_WITH_CROGL% == "1"
      !insertmacro MUI_DESCRIPTION_TEXT ${SEC03} $(VBOX_COMPONENT_D3D_DESC)
    !endif
    !insertmacro MUI_DESCRIPTION_TEXT   ${SEC04} $(VBOX_COMPONENT_STARTMENU_DESC)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
!endif ; USE_MUI

Section -Content

  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"

SectionEnd

; Start menu entries. Enabled by default and can be disabled by the user.
Section /o $(VBOX_COMPONENT_STARTMENU) SEC04

  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url" "" "$INSTDIR\iexplore.ico"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"

SectionEnd

; This section is called after all the files are in place
Section -Post

!ifdef _DEBUG
  ${LogVerbose} "Doing post install ..."
!endif

!ifdef EXTERNAL_UNINSTALLER
  SetOutPath "$INSTDIR"
  FILE "$%PATH_TARGET%\uninst.exe"
!else
  WriteUninstaller "$INSTDIR\uninst.exe"
!endif

  ; Write uninstaller in "Add / Remove programs"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  ; Tune TcpWindowSize for a better network throughput
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" "TcpWindowSize" 64240

!ifdef _DEBUG
  ${LogVerbose} "Enable Backdoor logging for debug build."
  WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\VBoxGuest" "LoggingEnabled" 255
!endif

  ; Add Sun Ray  client info keys
  ; Note: We only need 32-bit keys (HKLM\Software / HKLM\Software\Wow6432Node)
!if $%BUILD_TARGET_ARCH% == "amd64"
  WriteRegStr HKLM "SOFTWARE\Wow6432Node\Oracle\Sun Ray\ClientInfoAgent\ReconnectActions" "" ""
  WriteRegStr HKLM "SOFTWARE\Wow6432Node\Oracle\Sun Ray\ClientInfoAgent\DisconnectActions" "" ""
!else
  WriteRegStr HKLM "SOFTWARE\Oracle\Sun Ray\ClientInfoAgent\ReconnectActions" "" ""
  WriteRegStr HKLM "SOFTWARE\Oracle\Sun Ray\ClientInfoAgent\DisconnectActions" "" ""
!endif

  ${LogVerbose} "Installation completed."

SectionEnd

; !!! NOTE: This function *has* to be right under the last section; otherwise it does
;           *not* get called! Don't ask me why ... !!!
Function .onSelChange

  Push $0

  ; Handle selection of D3D component
  SectionGetFlags ${SEC03} $0
  ${If} $0 == ${SF_SELECTED}

    StrCpy $g_bWithD3D "true"

!if $%VBOX_WITH_WDDM% == "1"
    ; If we're able to use the WDDM driver just use it instead of the replaced
    ; D3D components below
    ${If} $g_bCapWDDM == "true"
      ;
      ; Temporary solution: Since WDDM is marked as experimental yet we notify the user
      ; that WDDM (Aero) support is available but not recommended for production use. He now
      ; can opt-in for installing WDDM or still go for the old (XPDM) way -- safe mode still required!
      ;
      MessageBox MB_ICONQUESTION|MB_YESNO $(VBOX_COMPONENT_D3D_OR_WDDM) /SD IDNO IDYES d3d_install
      ; Display an unconditional hint about needed VRAM sizes
      ; Note: We also could use the PCI configuration space (WMI: Win32_SystemSlot Class) for querying
      ;       the current VRAM size, but let's keep it simple for now
      MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_COMPONENT_D3D_HINT_VRAM) /SD IDOK
      StrCpy $g_bWithWDDM "true"
      Goto exit
    ${EndIf}

d3d_install:

!endif ; $%VBOX_WITH_WDDM% == "1"

    ${If} $g_bForceInstall != "true"
      ; Do not install on < XP
      ${If}   $g_strWinVersion == "NT4"
      ${OrIf} $g_strWinVersion == "2000"
      ${OrIf} $g_strWinVersion == ""
        MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_COMPONENT_D3D_NOT_SUPPORTED) /SD IDOK
        Goto d3d_disable
      ${EndIf}
    ${EndIf}

    ; If force flag is set skip the safe mode check
    ${If} $g_bForceInstall != "true"
      ; If we're not in safe mode, print a warning and don't install D3D support
      ${If} $g_iSystemMode == '0'
        MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_COMPONENT_D3D_NO_SM) /SD IDOK
        Goto d3d_disable
      ${EndIf}
    ${EndIf}

  ${Else} ; D3D unselected again

    ${If}   $g_strWinVersion != "8"   ; On Windows 8 WDDM is mandatory
    ${AndIf} $g_strWinVersion != "8_1" ; ... also on Windows 8.1 / Windows 2012 Server R2
    ${AndIf} $g_strWinVersion != "10" ; ... also on Windows 10
      StrCpy $g_bWithWDDM "false"
    ${EndIf}

  ${EndIf}
  Goto exit

d3d_disable:

  StrCpy $g_bWithD3D "false"
  IntOp $0 $0 & ${SECTION_OFF} ; Unselect section again
  SectionSetFlags ${SEC03} $0
  Goto exit

exit:

  Pop $0

FunctionEnd

; This function is called when a critical error occurred, caused by
; the Abort command
Function .onInstFailed

  ${LogVerbose} "$(VBOX_ERROR_INST_FAILED)"
  MessageBox MB_ICONSTOP $(VBOX_ERROR_INST_FAILED) /SD IDOK

  ${If} $g_bPostInstallStatus == "true"
    ${LogToVBoxTray} "2" "Error while installing ${PRODUCT_NAME}!"
  ${EndIf}

  ; Set overall exit code
  SetErrorLevel 1

FunctionEnd

; This function is called when installation was successful!
Function .onInstSuccess

  ${LogVerbose} "${PRODUCT_NAME} successfully installed"

  ${If} $g_bPostInstallStatus == "true"
    ${LogToVBoxTray} "0" "${PRODUCT_NAME} successfully updated!"
  ${EndIf}

  SetErrorLevel 0

FunctionEnd

; This function is called at the very beginning of installer execution
Function .onInit

  Push $0

  ; Init values
  StrCpy $g_iSystemMode "0"
  StrCpy $g_strCurUser "<None>"
  StrCpy $g_strAddVerMaj "0"
  StrCpy $g_strAddVerMin "0"
  StrCpy $g_strAddVerBuild "0"
  StrCpy $g_strAddVerRev "0"

  StrCpy $g_bIgnoreUnknownOpts "false"
  StrCpy $g_bLogEnable "false"
  StrCpy $g_bFakeWHQL "false"
  StrCpy $g_bForceInstall "false"
  StrCpy $g_bUninstall "false"
  StrCpy $g_bRebootOnExit "false"
  StrCpy $g_iScreenX "0"
  StrCpy $g_iScreenY "0"
  StrCpy $g_iScreenBpp "0"
  StrCpy $g_iSfOrder "0"
  StrCpy $g_bNoVBoxServiceExit "false"
  StrCpy $g_bNoVBoxTrayExit "false"
  StrCpy $g_bNoVideoDrv "false"
  StrCpy $g_bNoGuestDrv "false"
  StrCpy $g_bNoMouseDrv "false"
  StrCpy $g_bNoStartMenuEntries "false"
  StrCpy $g_bWithAutoLogon "false"
  StrCpy $g_bWithD3D "false"
  StrCpy $g_bOnlyExtract "false"
  StrCpy $g_bWithWDDM "false"
  StrCpy $g_bCapDllCache "false"
  StrCpy $g_bCapWDDM "false"
  StrCpy $g_bPostInstallStatus "false"

  ; We need a special directory set to SysWOW64 because some
  ; shell operations don't support file redirection (yet)
  StrCpy $g_strSysWow64 "$WINDIR\SysWOW64"

  SetErrorLevel 0
  ClearErrors

!ifdef UNINSTALLER_ONLY

  ;
  ; If UNINSTALLER_ONLY is defined, we're only interested in uninst.exe
  ; so we can sign it
  ;
  ; Note that the Quit causes the exit status to be 2 instead of 0
  ;
  WriteUninstaller "$%PATH_TARGET%\uninst.exe"
  Quit

!else

  ; Handle command line
  Call HandleCommandLine

  ; Check if there's already another instance of the installer is running -
  ; important for preventing NT4 to spawn the installer twice
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "VBoxGuestInstaller") ?e'
  Pop $0
  ${If} $0 != 0
    Quit
  ${EndIf}

  ; Retrieve Windows version and store result in $g_strWinVersion
  Call GetWindowsVersionEx
  Pop $g_strWinVersion

  ; Retrieve capabilities
  Call CheckForCapabilities

  ; Get user Name
  AccessControl::GetCurrentUserName
  Pop $g_strCurUser
  ${LogVerbose} "Current user: $g_strCurUser"

  ; Only extract files? This action can be called even from non-Admin users
  ; and non-compatible architectures
  ${If} $g_bOnlyExtract == "true"
    Call ExtractFiles
    MessageBox MB_OK|MB_ICONINFORMATION $(VBOX_EXTRACTION_COMPLETE) /SD IDOK
    Quit
  ${EndIf}

  ; Check for correct architecture
  Call CheckArchitecture
  Pop $0
  ${If} $0 <> 0 ; Wrong architecture? Tell the world
!if $%BUILD_TARGET_ARCH% == "amd64"
    MessageBox MB_ICONSTOP $(VBOX_NOTICE_ARCH_AMD64) /SD IDOK
!else
    MessageBox MB_ICONSTOP $(VBOX_NOTICE_ARCH_X86) /SD IDOK
!endif
    Abort "$(VBOX_NOTICE_ARCH_AMD64)"
  ${EndIf}

  ; Has the user who calls us admin rights?
  UserInfo::GetAccountType
  Pop $0
  ${If} $0 != "Admin"
    MessageBox MB_ICONSTOP $(VBOX_NOADMIN) /SD IDOK
    Abort
  ${EndIf}

  ; Only uninstall?
  ${If} $g_bUninstall == "true"
    Call Uninstall_Innotek
    Call Uninstall
    MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_UNINST_SUCCESS) /SD IDOK
    Quit
  ${EndIf}

  Call CheckForInstalledComponents

  ;
  ; Section 02
  ;
  ${If} $g_bWithAutoLogon == "true" ; Auto-logon support
    !insertmacro SelectSection ${SEC02}
  ${EndIf}

  ;
  ; Section 03
  ;
!if $%VBOX_WITH_CROGL% == "1"
  ${If} $g_bWithD3D == "true" ; D3D support
    !insertmacro SelectSection ${SEC03}
  ${EndIf}
!endif
  ${If} $g_bWithWDDM == "true" ; D3D / WDDM support
    !insertmacro SelectSection ${SEC03}
  ${EndIf}
  ; On Windows 8 / 8.1 / Windows Server 2012 R2 and newer we always select the 3D
  ; section and disable it so that it cannot be deselected again
  ${If}   $g_strWinVersion == "8"
  ${OrIf} $g_strWinVersion == "8_1"
  ${OrIf} $g_strWinVersion == "10"
    IntOp $0 ${SF_SELECTED} | ${SF_RO}
    SectionSetFlags ${SEC03} $0
  ${EndIf}

  ;
  ; Section 04
  ;
  ${If} $g_bNoStartMenuEntries == "false" ; Start menu entries
    !insertmacro SelectSection ${SEC04}
  ${EndIf}

!ifdef USE_MUI
  ; Display language selection dialog (will be hidden in silent mode!)
  !ifdef VBOX_INSTALLER_ADD_LANGUAGES
    !insertmacro MUI_LANGDLL_DISPLAY
  !endif
!endif

  Call SetAppMode64

  ; Check for old additions
  Call CheckForOldGuestAdditions
  Call GetAdditionsVersion

  ; Due to some bug in NSIS the license page won't be displayed if we're in
  ; 64-bit registry view, so as a workaround switch back to 32-bit (Wow6432Node)
  ; mode for now
  Call SetAppMode32

!endif ; UNINSTALLER_ONLY

  Pop $0

FunctionEnd

;
; The uninstaller is built separately when doing code signing
; For some reason NSIS still finds the Uninstall section even
; when EXTERNAL_UNINSTALLER is defined. This causes a silly warning
;
!ifndef EXTERNAL_UNINSTALLER

Function un.onUninstSuccess

  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK $(VBOX_UNINST_SUCCESS) /SD IDOK

FunctionEnd

Function un.onInit

  ; Has the user who calls us admin rights?
  UserInfo::GetAccountType
  Pop $0
  ${If} $0 != "Admin"
    MessageBox MB_ICONSTOP $(VBOX_NOADMIN) /SD IDOK
    Abort
  ${EndIf}

  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 $(VBOX_UNINST_CONFIRM) /SD IDYES IDYES proceed
  Quit

proceed:

  Call un.SetAppMode64

  ; Set system directory
  StrCpy $g_strSystemDir "$SYSDIR"

  ; We need a special directory set to SysWOW64 because some
  ; shell operations don't support file redirection (yet)
  StrCpy $g_strSysWow64 "$WINDIR\SysWOW64"

  ; Retrieve Windows version we're running on and store it in $g_strWinVersion
  Call un.GetWindowsVersionEx
  Pop $g_strWinVersion

  ; Retrieve capabilities
  Call un.CheckForCapabilities

FunctionEnd

Section Uninstall

!ifdef _DEBUG
  ${LogEnable} "true"
!endif

  Call un.SetAppMode64

  ; Call the uninstall main function
  Call un.Uninstall

  ; ... and remove the local install directory
  Call un.UninstallInstDir

!ifndef _DEBUG
  SetAutoClose true
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 $(VBOX_REBOOT_REQUIRED) /SD IDNO IDYES restart
  StrCmp $g_bRebootOnExit "true" restart
!endif

  Goto exit

restart:

  ${LogVerbose} "Rebooting ..."
  Reboot

exit:

SectionEnd

; !EXTERNAL_UNINSTALLER
!endif

;Direct the output to our bin dir
!cd "$%PATH_OUT%\bin\additions"
