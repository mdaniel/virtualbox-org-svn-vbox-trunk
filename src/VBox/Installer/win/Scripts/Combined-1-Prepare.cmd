@echo off
rem $Id$
rem rem @file
rem Windows NT batch script for preparing both amd64 and x86 for signing submission.
rem

rem
rem Copyright (C) 2018 Oracle Corporation
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
rem Globals and checks for required enviornment variables.
rem
if ".%KBUILD_DEVTOOLS%" == "." (echo KBUILD_DEVTOOLS is not set & goto end_failed)
if ".%KBUILD_BIN_PATH%" == "." (echo KBUILD_BIN_PATH is not set & goto end_failed)
set _MY_SCRIPT_DIR=%~dp0
set _MY_SAVED_CD=%CD%
set _MY_VER_REV=@VBOX_VERSION_STRING@r@VBOX_SVN_REV@

rem
rem Parse arguments.
rem
set _MY_OPT_UNTAR_DIR=%_MY_SCRIPT_DIR%\..\..\..\
for %%i in (%_MY_OPT_UNTAR_DIR%) do set _MY_OPT_UNTAR_DIR=%%~fi
set _MY_OPT_EXTPACK=%_MY_OPT_UNTAR_DIR%\Oracle_VM_VirtualBox_Extension_Pack-%_MY_VER_REV%.vbox-extpack
set _MY_OPT_EXTPACK_ENTERPRISE=%_MY_OPT_UNTAR_DIR%\Oracle_VM_VirtualBox_Extension_Pack-%_MY_VER_REV%-ENTERPRISE.vbox-extpack
set _MY_OPT_BUILD_TYPE=@KBUILD_TYPE@
set _MY_OPT_OUTDIR=%_MY_OPT_UNTAR_DIR%\output

:argument_loop
if ".%1" == "."             goto no_more_arguments

if ".%1" == ".-h"           goto opt_h
if ".%1" == ".-?"           goto opt_h
if ".%1" == "./h"           goto opt_h
if ".%1" == "./H"           goto opt_h
if ".%1" == "./?"           goto opt_h
if ".%1" == ".-help"        goto opt_h
if ".%1" == ".--help"       goto opt_h

if ".%1" == ".-e"                   goto opt_e
if ".%1" == ".--extpack"            goto opt_e
if ".%1" == ".-o"                   goto opt_o
if ".%1" == ".--outdir"             goto opt_o
if ".%1" == ".-s"                   goto opt_s
if ".%1" == ".--extpack-enterprise" goto opt_s
if ".%1" == ".-t"                   goto opt_t
if ".%1" == ".--build-type"         goto opt_t
if ".%1" == ".-u"                   goto opt_u
if ".%1" == ".--vboxall-untar-dir"  goto opt_u
echo syntax error: Unknown option: %1
echo               Try --help to list valid options.
goto end_failed

:argument_loop_next_with_value
shift
shift
goto argument_loop

:opt_e
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_EXTPACK=%~f2
goto argument_loop_next_with_value

:opt_h
echo Toplevel combined package: Prepare both x86 and amd64 for submission.
echo .
echo Usage: Combined-1-Prepare.cmd [-o output-dir] [-e/--extpack puel.vbox-extpack]
echo            [-s/--extpack-enterprise puel-enterprise.vbox-extpack]
echo            [-u/--vboxall-dir unpacked-vboxall-dir] [-t build-type]
echo .
echo Default -e/--extpack value:            %_MY_OPT_EXTPACK%
echo Default -s/--extpack-enterprise value: %_MY_OPT_EXTPACK_ENTERPRISE%
echo Default -u/--vboxall-untar-dir value:  %_MY_OPT_UNTAR_DIR%
echo Default -o/--outdir value:             %_MY_OPT_OUTDIR%
echo Default -t/--build-type value:         %_MY_OPT_BUILD_TYPE%
echo .
goto end_failed

:opt_o
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_OUTDIR=%~f2
goto argument_loop_next_with_value

:opt_s
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_EXTPACK_ENTERPRISE=%~f2
goto argument_loop_next_with_value

:opt_t
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_BUILD_TYPE=%~2
goto argument_loop_next_with_value

:opt_u
if ".%~2" == "."            goto syntax_error_missing_value
set _MY_OPT_UNTAR_DIR=%~f2
goto argument_loop_next_with_value


:syntax_error_missing_value
echo syntax error: missing or empty option value after %1
goto end_failed


:error_vboxall_untar_dir_not_found
echo syntax error: The VBoxAll untar directory was not found: "%_MY_OPT_UNTAR_DIR%"
goto end_failed

:error_amd64_bindir_not_found
echo syntax error: The AMD64 bin directory was not found: "%_MY_BINDIR_AMD64%"
goto end_failed

:error_x86_bindir_not_found
echo syntax error: The AMD64 bin directory was not found: "%_MY_BINDIR_X86%"
goto end_failed

:error_amd64_repack_dir_not_found
echo syntax error: The AMD64 bin directory was not found: "%_MY_REPACK_DIR_AMD64%"
goto end_failed

:error_x86_repack_dir_not_found
echo syntax error: The AMD64 bin directory was not found: "%_MY_REPACK_DIR_X86%"
goto end_failed

:error_extpack_not_found
echo syntax error: Specified extension pack not found: "%_MY_OPT_EXTPACK%"
goto end_failed

:error_enterprise_extpack_not_found
echo syntax error: Specified enterprise extension pack not found: "%_MY_OPT_EXTPACK_ENTERPRISE%"
goto end_failed



:no_more_arguments
rem
rem Validate and adjust specified options.
rem

if not exist "%_MY_OPT_UNTAR_DIR%"      goto error_vboxall_untar_dir_not_found

set _MY_BINDIR_AMD64=%_MY_OPT_UNTAR_DIR%\win.amd64\%_MY_OPT_BUILD_TYPE%\bin
set _MY_BINDIR_X86=%_MY_OPT_UNTAR_DIR%\win.x86\%_MY_OPT_BUILD_TYPE%\bin
if not exist "%_MY_BINDIR_AMD64%"       goto error_amd64_bindir_not_found
if not exist "%_MY_BINDIR_X86%"         goto error_x86_bindir_not_found

set _MY_REPACK_DIR_AMD64=%_MY_OPT_UNTAR_DIR%\win.amd64\%_MY_OPT_BUILD_TYPE%\repack
set _MY_REPACK_DIR_X86=%_MY_OPT_UNTAR_DIR%\win.x86\%_MY_OPT_BUILD_TYPE%\repack
if not exist "%_MY_REPACK_DIR_AMD64%"   goto error_amd64_repack_dir_not_found
if not exist "%_MY_REPACK_DIR_X86%"     goto error_x86_repack_dir_not_found

if not exist "%_MY_OPT_EXTPACK%"        goto error_extpack_not_found
if not ".%_MY_OPT_EXTPACK_ENTERPRISE%" == "." if not exist "%_MY_OPT_EXTPACK_ENTERPRISE%" goto error_enterprise_extpack_not_found


rem Make sure the output dir exists.
if not exist "%_MY_OPT_OUTDIR%"     (mkdir "%_MY_OPT_OUTDIR%" || goto end_failed)

rem
rem Install the extpack in the bin directories.
rem Note! Not really necessary, but whatever.
rem
echo on
copy /y "%_MY_OPT_EXTPACK%" "%_MY_BINDIR_AMD64%\Oracle_VM_VirtualBox_Extension_Pack.vbox-extpack" || goto end_failed
copy /y "%_MY_OPT_EXTPACK%"   "%_MY_BINDIR_X86%\Oracle_VM_VirtualBox_Extension_Pack.vbox-extpack" || goto end_failed
@echo off

rem
rem Do the packing.
rem
@echo **************************************************************************
@echo Packing AMD64 drivers
@echo **************************************************************************
cd /d "%_MY_REPACK_DIR_AMD64%" || goto end_failed
call "%_MY_REPACK_DIR_AMD64%\PackDriversForSubmission.cmd" -b "%_MY_BINDIR_AMD64%" -a amd64 -e "%_MY_OPT_EXTPACK%" ^
    -o "%_MY_OPT_OUTDIR%\VBoxDrivers-%_MY_VER_REV%-amd64.cab" || goto end_failed
@echo .
@echo **************************************************************************
@echo Packing X86 drivers
@echo **************************************************************************
cd /d "%_MY_REPACK_DIR_X86%" || goto end_failed
call "%_MY_REPACK_DIR_X86%\PackDriversForSubmission.cmd" -b "%_MY_BINDIR_X86%" -a x86 -e "%_MY_OPT_EXTPACK%" ^
    -o "%_MY_OPT_OUTDIR%\VBoxDrivers-%_MY_VER_REV%-x86.cab" || goto end_failed
@echo off
echo .
cd /d "%_MY_SAVED_CD%"

rem
rem Generate script for taking the next step.
rem
set _MY_NEXT_SCRIPT=%_MY_OPT_OUTDIR%\Combined-3-Repack.cmd
echo cd /d "%cd%" > "%_MY_NEXT_SCRIPT%"
echo call "%_MY_SCRIPT_DIR%\Combined-3-Repack.cmd" --extpack "%_MY_OPT_EXTPACK%" ^
    --extpack-enterprise "%_MY_OPT_EXTPACK_ENTERPRISE%" ^
    --vboxall-untar-dir "%_MY_OPT_UNTAR_DIR%" ^
    --outdir "%_MY_OPT_OUTDIR%" ^
    --build-type "%_MY_OPT_BUILD_TYPE%" %%* >> "%_MY_NEXT_SCRIPT%"

rem
rem Instructions on what to do next.
rem
echo **************************************************************************
echo * First step is done.
echo *
echo * Created:
echo *     %_MY_OPT_OUTDIR%\VBoxDrivers-%_MY_VER_REV%-amd64.cab
echo *     %_MY_OPT_OUTDIR%\VBoxDrivers-%_MY_VER_REV%-x86.cab
echo *
echo * Next steps:
echo *   1. Sign the two files using the Oracle EV certificate.
echo *   2. Submit the signed files to Microsoft for attestation signing.
echo *   3. Download the signed result.
echo *   4. "%_MY_NEXT_SCRIPT%" --signed-x86 {zip} --signed-amd64 {zip}

goto end


:end_failed
@cd /d "%_MY_SAVED_CD%"
@endlocal
@endlocal
@echo * Failed!
@exit /b 1

:end
@cd /d "%_MY_SAVED_CD%"
@endlocal
@endlocal

