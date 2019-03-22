# -*- coding: utf-8 -*-
# $Id$

"""
????????
"""

__copyright__ = \
"""
Copyright (C) 2012-2017 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"


import os
import sys
import time
import subprocess
import re
import time

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter
from testdriver import base
from testdriver import vbox
from testdriver import vboxcon
from testdriver import vboxtestvms

class tdGuestHostTimings(vbox.TestDriver):                                         # pylint: disable=R0902

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.sSessionTypeDef = 'gui';

        self.oTestVmSet = self.oTestVmManager.getStandardVmSet('nat')

        # Use the command line "--test-vms mw7x64 execute" to run the only "mw7x64" VM
        oTestVm = vboxtestvms.TestVm(self.oTestVmSet, 'mw7x64', sHd = 'mw7x64.vdi',
                         sKind = 'Windows7', acCpusSup = range(1, 2), fIoApic = True, sFirmwareType = 'bios',
                            asParavirtModesSup = ['hyperv'], asVirtModesSup = ['hwvirt-np'], sHddControllerType = 'SATA Controller');
                            
        self.oTestVmSet.aoTestVms.append(oTestVm);

        self.sVMname = None

    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdGuestHostTimings Options:');
        reporter.log(' --runningvmname <vmname>');
        return rc;

    def parseOption(self, asArgs, iArg):                                        # pylint: disable=R0912,R0915
        if asArgs[iArg] == '--runningvmname':
            iArg += 1
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "----runningvmname" needs VM name')

            self.sVMname = asArgs[iArg]
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg)
        return iArg + 1

    def actionConfig(self):
        return True
  
    def actionExecute(self):
        #self.sTempPathHost  = os.environ.get("IPRT_TMPDIR")
        self.sTempPathHost  = os.path.normpath(os.environ.get("TEMP") + "/VBoxAudioValKit")

        if self.sVMname is None:
            return self.oTestVmSet.actionExecute(self, self.testOneVmConfig)
        else:
            return self.actionExecuteOnRunnigVM()

    def doTest(self, oSession):
        oConsole = oSession.console
        oGuest = oConsole.guest

        sOSTypeId = oGuest.OSTypeId.lower()
        if sOSTypeId.find("win") == -1 :
            reporter.log("Only Windows guests are currently supported")
            reporter.testDone()
            return True

        oGuestSession = oGuest.createSession("Administrator", "password", "", "Audio Validation Kit")
        guestSessionWaitResult = oGuestSession.waitFor(self.oVBoxMgr.constants.GuestSessionWaitResult_Start, 2000)
        reporter.log("guestSessionWaitResult = %d" % guestSessionWaitResult)

        for duration in range(3, 6):
            reporter.testStart("Checking for duration of " + str(duration) + " seconds")
            sPathToPlayer = "D:\\win\\" + ("amd64" if (sOSTypeId.find('_64') >= 0) else "x86") + "\\ntPlayToneWaveX.exe"
            oProcess = oGuestSession.processCreate(sPathToPlayer,  ["xxx0", "--total-duration-in-secs", str(duration)], [], [], 0)
            processWaitResult = oProcess.waitFor(self.oVBoxMgr.constants.ProcessWaitForFlag_Start, 1000)
            reporter.log("Started: pid %d, waitResult %d" % (oProcess.PID, processWaitResult))
            
            processWaitResult = oProcess.waitFor(self.oVBoxMgr.constants.ProcessWaitForFlag_Terminate, 2 * duration * 1000)
            reporter.log("Terminated: pid %d, waitResult %d" % (oProcess.PID, processWaitResult))
            time.sleep(1) # Give audio backend sometime to save a stream to .wav file

            absFileName = self.seekLatestAudioFileName(oGuestSession, duration)
            
            if absFileName is None:
                reporter.testFailure("Unable to find audio file")
                continue
            
            reporter.log("Checking audio file '" + absFileName + "'")
            
            diff = self.checkGuestHostTimings(absFileName + ".timing")
            if diff is not None:
                if diff > 0.0:      # Guest sends data quicker than a host can play
                    if diff > 0.01: # 1% is probably good threshold here
                        reporter.testFailure("Guest sends audio buffers too quickly")
                else:
                    diff = -diff;   # Much worse case: guest sends data very slow, host feels starvation
                    if diff > 0.005: # 0.5% is probably good threshold here
                        reporter.testFailure("Guest sends audio buffers too slowly")
                        
                reporter.testDone()
            else:
                reporter.testFailure("Unable to parse a file with timings")

        oGuestSession.close()

        del oGuest
        del oConsole
        
        return True

    def testOneVmConfig(self, oVM, oTestVm):
        #self.logVmInfo(oVM)
        oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName,
                                                                    fCdWait = True,
                                                                    cMsTimeout = 60 * 1000)
        if oSession is not None and oTxsSession is not None:
            # Wait until guest reported success
            reporter.log('Guest started. Connection to TXS service established.')
            self.doTest(oSessionWrapper.o)

        return True

    def actionExecuteOnRunnigVM(self):
        if not self.importVBoxApi():
            return False;

        oVirtualBox = self.oVBoxMgr.getVirtualBox()
        oMachine = oVirtualBox.findMachine(self.sVMname)

        if oMachine == None:
            reporter.log("Machine '%s' is unknown" % (oMachine.name))
            return False

        if oMachine.state != self.oVBoxMgr.constants.MachineState_Running:
            reporter.log("Machine '%s' is not Running" % (oMachine.name))
            return False

        oSession = self.oVBoxMgr.mgr.getSessionObject(oVirtualBox)
        oMachine.lockMachine(oSession, self.oVBoxMgr.constants.LockType_Shared)
        
        self.doTest(oSession);

        oSession.unlockMachine()   

        del oSession
        del oMachine
        del oVirtualBox
        return True

    def seekLatestAudioFileName(self, guestSession, duration):

        listOfFiles = os.listdir(self.sTempPathHost)
        # Assuming that .wav files are named like 2016-11-15T12_08_27.669573100Z.wav by VBOX audio backend 
        # So that sorting by name = sorting by creation date
        listOfFiles.sort(reverse = True)

        for fileName in listOfFiles:
            if not fileName.endswith(".wav"):
                continue
                
            absFileName = os.path.join(self.sTempPathHost, fileName)
            
            # Ignore too small wav files (usually uncompleted audio streams) 
            statInfo = os.stat(absFileName)
            if statInfo.st_size > 100:
                return absFileName

        return

    def checkGuestHostTimings(self, absFileName):
        with open(absFileName) as f:
            for line_terminated in f:
                line = line_terminated.rstrip('\n')

        reporter.log("Last line is: " + line)
        matchObj = re.match( r'(\d+) (\d+)', line, re.I)
        if matchObj:
            hostTime  = int(matchObj.group(1))
            guestTime = int(matchObj.group(2))

            diff = float(guestTime - hostTime) / hostTime
            return diff
        
        return

if __name__ == '__main__':
    sys.exit(tdGuestHostTimings().main(sys.argv));
