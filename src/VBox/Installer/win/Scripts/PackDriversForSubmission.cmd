@echo off
rem $Id$
rem rem @file
rem Windows NT batch script for preparing for signing submission.
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
rem Parse arguments.
rem
set _MY_OPT_BINDIR=..\bin
set _MY_OPT_PDBDIR=
set _MY_OPT_WITH_PDB=1
set _MY_OPT_OUTPUT=
set _MY_OPT_DDF_FILE=

:argument_loop
if ".%1" == "."             goto no_more_arguments

if ".%1" == ".-h"           goto opt_h
if ".%1" == ".-?"           goto opt_h
if ".%1" == "./h"           goto opt_h
if ".%1" == "./H"           goto opt_h
if ".%1" == "./?"           goto opt_h
if ".%1" == ".-help"        goto opt_h
if ".%1" == ".--help"       goto opt_h

if ".%1" == ".-b"           goto opt_b
if ".%1" == ".--bindir"     goto opt_b
if ".%1" == ".-o"           goto opt_o
if ".%1" == ".--output"     goto opt_o
if ".%1" == ".-d"           goto opt_d
if ".%1" == ".--ddf"        goto opt_d
if ".%1" == ".-p"           goto opt_p
if ".%1" == ".--pdb"        goto opt_p
if ".%1" == ".-n"           goto opt_n
if ".%1" == ".--no-pdb"     goto opt_n
echo syntax error: Unknown option: %1
echo               Try --help to list valid options.
goto end_failed

:argument_loop_next_with_value
shift
shift
goto argument_loop

:opt_b
if ".%2" == "."             goto syntax_error_missing_value
set _MY_OPT_BINDIR=%2
goto argument_loop_next_with_value

:opt_d
if ".%2" == "."             goto syntax_error_missing_value
set _MY_OPT_DDF_FILE=%2
goto argument_loop_next_with_value

:opt_h
echo This script creates a .cab file containing all drivers needing blessing from
echo Microsoft to run on recent Windows 10 installations.
echo .
echo Usage: PackDriversForSubmission.cmd [-b bindir] [-p pdbdir] [-n/--no-pdb] [-o output.cab] [-p output.ddf]
echo .
echo Warning! This script should normally be invoked from the repack directory w/o any parameters.
goto end_failed

:opt_n
set _MY_OPT_WITH_PDB=0
shift
goto argument_loop

:opt_p
if ".%2" == "."             goto syntax_error_missing_value
set _MY_OPT_PDBDIR=%2
goto argument_loop_next_with_value

:opt_o
if ".%2" == "."             goto syntax_error_missing_value
set _MY_OPT_OUTPUT=%2
goto argument_loop_next_with_value

:syntax_error_missing_value
echo syntax error: missing or empty option value after %1
goto end_failed

:error_bindir_does_not_exist
echo syntax error: Specified BIN directory does not exist: "%_MY_OPT_BINDIR%"
goto end_failed

:error_pdbdir_does_not_exist
echo syntax error: Specified PDB directory does not exist: "%_MY_OPT_PDBDIR%"
goto end_failed

:error_output_exists
echo error: The output file already exist: "%_MY_OPT_OUTPUT%"
goto end_failed

:error_ddf_exists
echo error: The DDF file already exist: "%_MY_OPT_DDF_FILE%"
goto end_failed

:no_more_arguments
rem validate specified options
if not exist "%_MY_OPT_BINDIR%"     goto error_bindir_does_not_exist

if "%_MY_OPT_WITH_PDB" == "0"       goto no_pdbdir_validation
if ".%_MY_OPT_PDBDIR%" == "."       set _MY_OPT_PDBDIR=%_MY_OPT_BINDIR%\..\stage\debug\bin
if not exist "%_MY_OPT_PDBDIR%"     goto error_pdbdir_does_not_exist
:no_pdbdir_validation

if ".%_MY_OPT_OUTPUT%" == "." if exist "%_MY_OPT_BINDIR%\x86" set _MY_OPT_OUTPUT=VBoxDrivers-amd64.cab
if ".%_MY_OPT_OUTPUT%" == "."       set _MY_OPT_OUTPUT=VBoxDrivers-x86.cab
if exist "%_MY_OPT_OUTPUT%"         goto error_output_exists

if ".%_MY_OPT_DDF_FILE%" == "."     set _MY_OPT_DDF_FILE=%_MY_OPT_OUTPUT%.ddf
if exist "%_MY_OPT_DDF_FILE%"       goto error_ddf_exists


rem
rem Create the DDF file for makecab.
rem
echo .OPTION EXPLICIT>                                                                  "%_MY_OPT_DDF_FILE%" || goto end_failed
echo .Set CabinetFileCountThreshold=0 >>                                                "%_MY_OPT_DDF_FILE%"
echo .Set FolderFileCountThreshold=0 >>                                                 "%_MY_OPT_DDF_FILE%"
echo .Set FolderSizeThreshold=0 >>                                                      "%_MY_OPT_DDF_FILE%"
echo .Set MaxCabinetSize=0 >>                                                           "%_MY_OPT_DDF_FILE%"
echo .Set MaxDiskFileCount=0 >>                                                         "%_MY_OPT_DDF_FILE%"
echo .Set MaxDiskSize=0 >>                                                              "%_MY_OPT_DDF_FILE%"
echo .Set Cabinet=on>>                                                                  "%_MY_OPT_DDF_FILE%"
echo .Set CompressionType=MSZIP>>                                                       "%_MY_OPT_DDF_FILE%"
echo .Set Compress=on>>                                                                 "%_MY_OPT_DDF_FILE%"
echo .Set DiskDirectoryTemplate= >>                                                     "%_MY_OPT_DDF_FILE%"
echo .Set CabinetNameTemplate=%_MY_OPT_OUTPUT%>>                                        "%_MY_OPT_DDF_FILE%"
echo .Set InfFileName=%_MY_OPT_OUTPUT%.inf>>                                            "%_MY_OPT_DDF_FILE%"
echo .Set RptFileName=%_MY_OPT_OUTPUT%.rpt>>                                            "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VBoxDrv>>                                                      "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxDrv.inf VBoxDrv.inf>>                                         "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxDrv.sys VBoxDrv.sys>>                                         "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxDrv.pdb VBoxDrv.pdb>>           "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VBoxNetAdp6>>                                                  "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxNetAdp6.inf VBoxNetAdp6.inf>>                                 "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxNetAdp6.sys VBoxNetAdp6.sys>>                                 "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxNetAdp6.pdb VBoxNetAdp6.pdb>>   "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VBoxNetLwf>>                                                   "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxNetLwf.inf VBoxNetLwf.inf>>                                   "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxNetLwf.sys VBoxNetLwf.sys>>                                   "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxNetLwf.pdb VBoxNetLwf.pdb>>     "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VBoxUSB>>                                                      "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxUSB.inf VBoxUSB.inf>>                                         "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxUSB.sys VBoxUSB.sys>>                                         "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxUSB.pdb VBoxUSB.pdb>>           "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VBoxUSBMon>>                                                   "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxUSBMon.inf VBoxUSBMon.inf>>                                   "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxUSBMon.sys VBoxUSBMon.sys>>                                   "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxUSBMon.pdb VBoxUSBMon.pdb>>     "%_MY_OPT_DDF_FILE%"

echo .Set DestinationDir=VMMR0>>                                                        "%_MY_OPT_DDF_FILE%"
echo .\VMMR0.inf VMMR0.inf>>                                                            "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VMMR0.r0 VMMR0.r0>>                                               "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VMMR0.pdb VMMR0.pdb>>               "%_MY_OPT_DDF_FILE%"
echo %_MY_OPT_BINDIR%\VBoxDDR0.r0 VBoxDDR0.r0>>                                         "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxDDR0.pdb VBoxDDR0.pdb>>         "%_MY_OPT_DDF_FILE%"
if not exist %_MY_OPT_BINDIR%\VBoxDD2R0.r0 goto no_vboxdd2r0
echo %_MY_OPT_BINDIR%\VBoxDD2R0.r0 VBoxDD2R0.r0>>                                       "%_MY_OPT_DDF_FILE%"
if "%_MY_OPT_WITH_PDB" == "1" echo %_MY_OPT_PDBDIR%\VBoxDD2R0.pdb VBoxDD2R0.pdb>>       "%_MY_OPT_DDF_FILE%"
:no_vboxdd2r0

rem
rem Create the cabient file.
rem Note! MakeCab is shipped on W10, so we ASSUME it's in the PATH.
rem
MakeCab.exe /v2 /F "%_MY_OPT_DDF_FILE%"

goto end

:end_failed
@endlocal
@endlocal
@exit /b 1

:end
@endlocal
@endlocal

