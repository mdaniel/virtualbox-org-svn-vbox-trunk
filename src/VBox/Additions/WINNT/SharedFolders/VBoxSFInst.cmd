@echo off
rem $Id$
rem rem @file
rem Windows NT batch script for manually installing the shared folders guest addition driver.
rem

rem
rem Copyright (C) 2009-2019 Oracle Corporation
rem
rem This file is part of VirtualBox Open Source Edition (OSE), as
rem available from http://www.virtualbox.org. This file is free software;
rem you can redistribute it and/or modify it under the terms of the GNU
rem General Public License (GPL) as published by the Free Software
rem Foundation, in version 2 as it comes in the "COPYING" file of the
rem VirtualBox OSE distribution. VirtualBox OSE is distributed in the
rem hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
rem


setlocal ENABLEEXTENSIONS
setlocal

rem
rem VBoxSF.sys should be in the same directory as this script or in the additions output dir.
rem
set MY_VBOXSF_SYS=%~dp0VBoxSF.sys
if exist "%MY_VBOXSF_SYS%" goto found_vboxsf
set MY_VBOXSF_SYS=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VBoxSF.sys
if exist "%MY_VBOXSF_SYS%" goto found_vboxsf
echo VBoxSFInst.cmd: failed to find VBoxSF.sys in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vboxsf

rem
rem VBoxMRXNP.dll should be in the same directory as this script or in the additions output dir.
rem
set MY_VBOXMRXNP_DLL=%~dp0VBoxMRXNP.dll
if exist "%MY_VBOXMRXNP_DLL%" goto found_vboxmrxnp
set MY_VBOXMRXNP_DLL=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VBoxMRXNP.dll
if exist "%MY_VBOXMRXNP_DLL%" goto found_vboxmrxnp
echo VBoxSFInst.cmd: failed to find VBoxMRXNP.dll in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vboxmrxnp

rem 32-bit version of same.
if not "%PROCESSOR_ARCHITECTURE%" == "AMD64" goto found_vboxmrxnp_x86
set MY_VBOXMRXNP_X86_DLL=%~dp0VBoxMRXNP-x86.dll
if exist "%MY_VBOXMRXNP_X86_DLL%" goto found_vboxmrxnp_x86
set MY_VBOXMRXNP_X86_DLL=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VBoxMRXNP-x86.dll
if exist "%MY_VBOXMRXNP_X86_DLL%" goto found_vboxmrxnp_x86
echo VBoxSFInst.cmd: failed to find VBoxMRXNP-x86.dll in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vboxmrxnp_x86

rem
rem VBoxDrvInst.exe should be in the same directory as this script or in the additions output dir.
rem
set MY_VBOXDRVINST=%~dp0VBoxDrvInst.exe
if exist "%MY_VBOXDRVINST%" goto found_vboxdrvinst
set MY_VBOXDRVINST=%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\VBoxDrvInst.exe
if exist "%MY_VBOXDRVINST%" goto found_vboxdrvinst
echo VBoxSFInst.cmd: failed to find VBoxDrvInst.exe in either "%~dp0" or "%~dp0..\..\..\..\..\out\win.%KBUILD_TARGET_ARCH%\%KBUILD_TYPE%\bin\additions\".
goto end
:found_vboxdrvinst

rem
rem Deregister the service, provider and delete old files.
rem
echo "Uninstalling..."
sc stop VBoxSF
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "DeviceName"
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "Name"
reg delete /f "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "ProviderPath"

"%MY_VBOXDRVINST%" service delete VBoxSF

del "%SYSTEMROOT%\system32\drivers\VBoxSF.sys"
del "%SYSTEMROOT%\system32\VBoxMRXNP.dll"
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" del "%SYSTEMROOT%\SysWOW64\VBoxMRXNP.dll"


rem
rem Install anything?
rem
if "%1" == "-u" goto end
if "%1" == "--uninstall" goto end

rem
rem Copy the new files to the system dir.
rem
echo "Copying files..."
copy "%MY_VBOXSF_SYS%"    "%SYSTEMROOT%\system32\drivers\"
copy "%MY_VBOXMRXNP_DLL%" "%SYSTEMROOT%\system32\"
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" copy "%MY_VBOXMRXNP_X86_DLL%" "%SYSTEMROOT%\SysWow64\VBoxMRXNP.dll"

rem
rem Register the service.
rem
echo "Installing service..."
"%MY_VBOXDRVINST%" service create VBoxSF "VirtualBox Shared Folders" 2 1 "%SYSTEMROOT%\System32\drivers\VBoxSF.sys" NetworkProvider

echo "Configuring network provider..."
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "DeviceName"    /d "\Device\VBoxMiniRdr"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "Name"          /d "VirtualBox Shared Folders"
reg add "HKLM\SYSTEM\CurrentControlSet\Services\VBoxSF\NetworkProvider" /v "ProviderPath"  /d "%SYSTEMROOT%\System32\VBoxMRXNP.dll"

"%MY_VBOXDRVINST%" netprovider add VBoxSF 0

rem
rem Start the service?
rem
if "%1" == "-n" goto end
if "%1" == "--no-start" goto end
sc start VBoxSF

:end
endlocal
endlocal

