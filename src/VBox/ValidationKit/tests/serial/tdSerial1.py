#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Serial port testing #1.
"""

__copyright__ = \
"""
Copyright (C) 2018 Oracle Corporation

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


# Standard Python imports.
import os;
import random;
import string;
import struct;
import sys;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import base;
from testdriver import reporter;
from testdriver import vbox;
from testdriver import vboxcon;

import loopback;

class tdSerial1(vbox.TestDriver):
    """
    VBox serial port testing #1.
    """

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs          = None;
        self.oTestVmSet       = self.oTestVmManager.selectSet(self.oTestVmManager.kfGrpStdSmoke);
        self.asSerialModesDef = ['RawFile', 'Tcp', 'TcpServ', 'NamedPipe', 'NamedPipeServ', 'HostDev'];
        self.asSerialModes    = self.asSerialModesDef;
        self.asSerialTestsDef = ['Write', 'ReadWrite'];
        self.asSerialTests    = self.asSerialTestsDef;
        self.oLoopback        = None;
        self.sLocation        = None;

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdSerial1 Options:');
        reporter.log('  --serial-modes    <m1[:m2[:]]');
        reporter.log('      Default: %s' % (':'.join(self.asSerialModesDef)));
        reporter.log('  --serial-tests    <t1[:t2[:]]');
        reporter.log('      Default: %s' % (':'.join(self.asSerialTestsDef)));
        return rc;

    def parseOption(self, asArgs, iArg):
        if asArgs[iArg] == '--serial-modes':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--serial-modes" takes a colon separated list of serial port modes to test');
            self.asSerialModes = asArgs[iArg].split(':');
            for s in self.asSerialModes:
                if s not in self.asSerialModesDef:
                    reporter.log('warning: The "--serial-modes" value "%s" is not a valid serial port mode.' % (s));
        elif asArgs[iArg] == '--serial-tests':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--serial-tests" takes a colon separated list of serial port tests');
            self.asSerialTests = asArgs[iArg].split(':');
            for s in self.asSerialTests:
                if s not in self.asSerialTestsDef:
                    reporter.log('warning: The "--serial-tests" value "%s" is not a valid serial port test.' % (s));
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);

        return iArg + 1;

    def actionVerify(self):
        if self.sVBoxValidationKitIso is None or not os.path.isfile(self.sVBoxValidationKitIso):
            reporter.error('Cannot find the VBoxValidationKit.iso! (%s)'
                           'Please unzip a Validation Kit build in the current directory or in some parent one.'
                           % (self.sVBoxValidationKitIso,) );
            return False;
        return vbox.TestDriver.actionVerify(self);

    def actionConfig(self):
        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        assert self.sVBoxValidationKitIso is not None;
        return self.oTestVmSet.actionConfig(self, sDvdImage = self.sVBoxValidationKitIso);

    def actionExecute(self):
        """
        Execute the testcase.
        """
        return self.oTestVmSet.actionExecute(self, self.testOneVmConfig)


    #
    # Test execution helpers.
    #

    def _generateRawPortFilename(self, oTestDrv, oTestVm, sInfix, sSuffix):
        """ Generates a raw port filename. """
        random.seed();
        sRandom = ''.join(random.choice(string.ascii_lowercase + string.digits) for _ in range(10));
        return os.path.join(oTestDrv.sScratchPath, oTestVm.sVmName + sInfix + sRandom + sSuffix);

    def setupSerialMode(self, oSession, oTestVm, sMode):
        """
        Sets up the serial mode.
        """
        fRc = True;
        fServer = False;
        sLocation = None;
        ePortMode = vboxcon.PortMode_Disconnected;
        if sMode == 'RawFile':
            sLocation = self._generateRawPortFilename(self, oTestVm, '-com1-', '.out');
            ePortMode = vboxcon.PortMode_RawFile;
        elif sMode == 'Tcp':
            sLocation = '127.0.0.1:1234';
            self.oLoopback = loopback.SerialLoopback(loopback.g_ksLoopbackTcpServ, sLocation);
            ePortMode = vboxcon.PortMode_TCP;
        elif sMode == 'TcpServ':
            fServer   = True;
            sLocation = '1234';
            ePortMode = vboxcon.PortMode_TCP;
            self.oLoopback = loopback.SerialLoopback(loopback.g_ksLoopbackTcpClient, '127.0.0.1:1234');
        elif sMode == 'NamedPipe':
            sLocation = self._generateRawPortFilename(self, oTestVm, '-com1-', '.out');
            ePortMode = vboxcon.PortMode_HostPipe;
            self.oLoopback = loopback.SerialLoopback(loopback.g_ksLoopbackNamedPipeServ, sLocation);
        elif sMode == 'NamedPipeServ':
            fServer   = True;
            sLocation = self._generateRawPortFilename(self, oTestVm, '-com1-', '.out');
            ePortMode = vboxcon.PortMode_HostPipe;
            self.oLoopback = loopback.SerialLoopback(loopback.g_ksLoopbackNamedPipeClient, sLocation);
        elif sMode == 'HostDev':
            sLocation = '/dev/ttyUSB0';
            ePortMode = vboxcon.PortMode_HostDevice;
        else:
            reporter.log('warning, invalid mode %s given' % (sMode, ));
            fRc = False;

        if fRc:
            fRc = oSession.changeSerialPortAttachment(0, ePortMode, sLocation, fServer);
            if fRc and (sMode == 'TcpServ' or sMode == 'NamedPipeServ'):
                self.sleep(2); # Fudge to allow the TCP server to get started.
                fRc = self.oLoopback.connect();
                if not fRc:
                    reporter.log('Failed to connect to %s' % (sLocation, ));
            self.sLocation = sLocation;

        return fRc;

    def testWrite(self, oSession, oTxsSession, oTestVm, sMode):
        """
        Does a simple write test verifying the output.
        """
        _ = oSession;

        reporter.testStart('Write');
        tupCmdLine = ('SerialTest', '--tests', 'write', '--txbytes', '1048576', '--device');
        if oTestVm.isWindows():
            tupCmdLine += (r'\\.\COM1',);
        elif oTestVm.isLinux():
            tupCmdLine += (r'/dev/ttyS0',);

        fRc = self.txsRunTest(oTxsSession, 'SerialTest', 3600 * 1000, \
            '${CDROM}/${OS/ARCH}/SerialTest${EXESUFF}', tupCmdLine);
        if not fRc:
            reporter.testFailure('Running serial test utility failed');
        elif sMode == 'RawFile':
            # Open serial port and verify
            cLast = 0;
            try:
                oFile = open(self.sLocation, 'rb');
                sFmt = '=I';
                cBytes = 4;
                for i in xrange(1048576 / 4):
                    _ = i;
                    sData = oFile.read(cBytes);
                    tupUnpacked = struct.unpack(sFmt, sData);
                    cLast = cLast + 1;
                    if tupUnpacked[0] != cLast:
                        reporter.testFailure('Corruption detected, expected counter value %s, got %s'
                                             % (cLast + 1, tupUnpacked[0]));
                        break;
                oFile.close();
            except:
                reporter.testFailure('Verifying the written data failed');
        reporter.testDone();
        return fRc;

    def testReadWrite(self, oSession, oTxsSession, oTestVm):
        """
        Does a simple write test verifying the output.
        """
        _ = oSession;

        reporter.testStart('ReadWrite');
        tupCmdLine = ('SerialTest', '--tests', 'readwrite', '--txbytes', '1048576', '--device');
        if oTestVm.isWindows():
            tupCmdLine += (r'\\.\COM1',);
        elif oTestVm.isLinux():
            tupCmdLine += (r'/dev/ttyS0',);

        fRc = self.txsRunTest(oTxsSession, 'SerialTest', 600 * 1000, \
            '${CDROM}/${OS/ARCH}/SerialTest${EXESUFF}', tupCmdLine);
        if not fRc:
            reporter.testFailure('Running serial test utility failed');

        reporter.testDone();
        return fRc;

    def isModeCompatibleWithTest(self, sMode, sTest):
        """
        Returns whether the given port mode and test combination is
        supported for testing.
        """
        if sMode == 'RawFile' and sTest == 'ReadWrite':
            return False;
        elif sMode != 'RawFile' and sTest == 'Write':
            return False;

        return True;

    def testOneVmConfig(self, oVM, oTestVm):
        """
        Runs the specified VM thru test #1.
        """

        # Reconfigure the VM
        fRc = True;
        oSession = self.openSession(oVM);
        if oSession is not None:
            fRc = oSession.enableSerialPort(0);

            fRc = fRc and oSession.saveSettings();
            fRc = oSession.close() and fRc;
            oSession = None;
        else:
            fRc = False;

        if fRc is True:
            self.logVmInfo(oVM);
            oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = True);
            if oSession is not None:
                self.addTask(oTxsSession);

                for sMode in self.asSerialModes:
                    reporter.testStart(sMode);
                    fRc = self.setupSerialMode(oSession, oTestVm, sMode);
                    if fRc:
                        for sTest in self.asSerialTests:
                            # Skip tests which don't work with the current mode.
                            if self.isModeCompatibleWithTest(sMode, sTest):
                                if sTest == 'Write':
                                    fRc = self.testWrite(oSession, oTxsSession, oTestVm, sMode);
                                if sTest == 'ReadWrite':
                                    fRc = self.testReadWrite(oSession, oTxsSession, oTestVm);
                        if self.oLoopback is not None:
                            self.oLoopback.shutdown();
                            self.oLoopback = None;

                    reporter.testDone();

                self.removeTask(oTxsSession);
                self.terminateVmBySession(oSession);
        return fRc;

if __name__ == '__main__':
    sys.exit(tdSerial1().main(sys.argv));

