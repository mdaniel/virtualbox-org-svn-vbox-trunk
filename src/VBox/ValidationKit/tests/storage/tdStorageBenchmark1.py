#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Storage benchmark.
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


# Standard Python imports.
import os;
import socket;
import sys;
import StringIO;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from common     import constants;
from common     import utils;
from testdriver import reporter;
from testdriver import base;
from testdriver import vbox;
from testdriver import vboxcon;

import remoteexecutor;
import storagecfg;

def _ControllerTypeToName(eControllerType):
    """ Translate a controller type to a name. """
    if eControllerType == vboxcon.StorageControllerType_PIIX3 or eControllerType == vboxcon.StorageControllerType_PIIX4:
        sType = "IDE Controller";
    elif eControllerType == vboxcon.StorageControllerType_IntelAhci:
        sType = "SATA Controller";
    elif eControllerType == vboxcon.StorageControllerType_LsiLogicSas:
        sType = "SAS Controller";
    elif eControllerType == vboxcon.StorageControllerType_LsiLogic or eControllerType == vboxcon.StorageControllerType_BusLogic:
        sType = "SCSI Controller";
    elif eControllerType == vboxcon.StorageControllerType_NVMe:
        sType = "NVMe Controller";
    else:
        sType = "Storage Controller";
    return sType;

class FioTest(object):
    """
    Flexible I/O tester testcase.
    """

    kdHostIoEngine = {
        'solaris': ('solarisaio', False),
        'linux':   ('libaio', True)
    };

    def __init__(self, oExecutor, dCfg = None):
        self.oExecutor  = oExecutor;
        self.sCfgFileId = None;
        self.dCfg       = dCfg;
        self.sError     = None;
        self.sResult    = None;

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """

        sTargetOs = self.dCfg.get('TargetOs', 'linux');
        sIoEngine, fDirectIo = self.kdHostIoEngine.get(sTargetOs);
        if sIoEngine is None:
            return False;

        cfgBuf = StringIO.StringIO();
        cfgBuf.write('[global]\n');
        cfgBuf.write('bs=' + self.dCfg.get('RecordSize', '4k') + '\n');
        cfgBuf.write('ioengine=' + sIoEngine + '\n');
        cfgBuf.write('iodepth=' + self.dCfg.get('QueueDepth', '32') + '\n');
        cfgBuf.write('size=' + self.dCfg.get('TestsetSize', '2g') + '\n');
        if fDirectIo:
            cfgBuf.write('direct=1\n');
        else:
            cfgBuf.write('direct=0\n');
        cfgBuf.write('directory=' + self.dCfg.get('FilePath', '/mnt') + '\n');
        cfgBuf.write('filename=fio.test.file')

        cfgBuf.write('[seq-write]\n');
        cfgBuf.write('rw=write\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[rand-write]\n');
        cfgBuf.write('rw=randwrite\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[seq-read]\n');
        cfgBuf.write('rw=read\n');
        cfgBuf.write('stonewall\n');

        cfgBuf.write('[rand-read]\n');
        cfgBuf.write('rw=randread\n');
        cfgBuf.write('stonewall\n');

        self.sCfgFileId = self.oExecutor.copyString(cfgBuf.getvalue(), 'aio-test', cMsTimeout);
        return self.sCfgFileId is not None;

    def run(self, cMsTimeout = 30000):
        """ Runs the testcase """
        _ = cMsTimeout
        fRc, sOutput, sError = self.oExecutor.execBinary('fio', (self.sCfgFileId,), cMsTimeout = cMsTimeout);
        if fRc:
            self.sResult = sOutput;
        else:
            self.sError = ('Binary: fio\n' +
                           '\nOutput:\n\n' +
                           sOutput +
                           '\nError:\n\n' +
                           sError);
        return fRc;

    def cleanup(self):
        """ Cleans up any leftovers from the testcase. """

    def reportResult(self):
        """
        Reports the test results to the test manager.
        """
        return True;

    def getErrorReport(self):
        """
        Returns the error report in case the testcase failed.
        """
        return self.sError;

class IozoneTest(object):
    """
    I/O zone testcase.
    """
    def __init__(self, oExecutor, dCfg = None):
        self.oExecutor = oExecutor;
        self.sResult = None;
        self.sError = None;
        self.lstTests = [ ('initial writers', 'FirstWrite'),
                          ('rewriters',       'Rewrite'),
                          ('re-readers',      'ReRead'),
                          ('stride readers',  'StrideRead'),
                          ('reverse readers', 'ReverseRead'),
                          ('random readers',  'RandomRead'),
                          ('mixed workload',  'MixedWorkload'),
                          ('random writers',  'RandomWrite'),
                          ('pwrite writers',  'PWrite'),
                          ('pread readers',   'PRead'),
                          ('fwriters',        'FWrite'),
                          ('freaders',        'FRead'),
                          ('readers',         'FirstRead')];
        self.sRecordSize  = dCfg.get('RecordSize',  '4k');
        self.sTestsetSize = dCfg.get('TestsetSize', '2g');
        self.sQueueDepth  = dCfg.get('QueueDepth',  '32');
        self.sFilePath    = dCfg.get('FilePath',    '/mnt/iozone');
        self.fDirectIo    = True;

        sTargetOs = dCfg.get('TargetOs');
        if sTargetOs == 'solaris':
            self.fDirectIo = False;

    def prepare(self, cMsTimeout = 30000):
        """ Prepares the testcase """
        _ = cMsTimeout;
        return True; # Nothing to do.

    def run(self, cMsTimeout = 30000):
        """ Runs the testcase """
        tupArgs = ('-r', self.sRecordSize, '-s', self.sTestsetSize, \
                   '-t', '1', '-T', '-F', self.sFilePath + '/iozone.tmp');
        if self.fDirectIo:
            tupArgs += ('-I',);
        fRc, sOutput, sError = self.oExecutor.execBinary('iozone', tupArgs, cMsTimeout = cMsTimeout);
        if fRc:
            self.sResult = sOutput;
        else:
            self.sError = ('Binary: iozone\n' +
                           '\nOutput:\n\n' +
                           sOutput +
                           '\nError:\n\n' +
                           sError);

        _ = cMsTimeout;
        return fRc;

    def cleanup(self):
        """ Cleans up any leftovers from the testcase. """
        return True;

    def reportResult(self):
        """
        Reports the test results to the test manager.
        """

        fRc = True;
        if self.sResult is not None:
            try:
                asLines = self.sResult.splitlines();
                for sLine in asLines:
                    sLine = sLine.strip();
                    if sLine.startswith('Children') is True:
                        # Extract the value
                        idxValue = sLine.rfind('=');
                        if idxValue == -1:
                            raise Exception('IozoneTest: Invalid state');

                        idxValue += 1;
                        while sLine[idxValue] == ' ':
                            idxValue += 1;

                        # Get the reported value, cut off after the decimal point
                        # it is not supported by the testmanager yet and is not really
                        # relevant anyway.
                        idxValueEnd = idxValue;
                        while sLine[idxValueEnd].isdigit():
                            idxValueEnd += 1;

                        for sNeedle, sTestVal in self.lstTests:
                            if sLine.rfind(sNeedle) != -1:
                                reporter.testValue(sTestVal, sLine[idxValue:idxValueEnd],
                                                   constants.valueunit.g_asNames[constants.valueunit.KILOBYTES_PER_SEC]);
                                break;
            except:
                fRc = False;
        else:
            fRc = False;

        return fRc;

    def getErrorReport(self):
        """
        Returns the error report in case the testcase failed.
        """
        return self.sError;

class StorTestCfgMgr(object):
    """
    Manages the different testcases.
    """

    def __init__(self, aasTestLvls, aasTestsBlacklist, fnIsCfgSupported = None):
        self.aasTestsBlacklist = aasTestsBlacklist;
        self.at3TestLvls       = [];
        self.iTestLvl          = 0;
        self.fnIsCfgSupported  = fnIsCfgSupported;
        for asTestLvl in aasTestLvls:
            if isinstance(asTestLvl, tuple):
                asTestLvl, fnTestFmt = asTestLvl;
                self.at3TestLvls.append((0, fnTestFmt, asTestLvl));
            else:
                self.at3TestLvls.append((0, None, asTestLvl));

        self.at3TestLvls.reverse();

        # Get the first non blacklisted test.
        asTestCfg = self.getCurrentTestCfg();
        while asTestCfg and self.isTestCfgBlacklisted(asTestCfg):
            asTestCfg = self.advanceTestCfg();

        iLvl = 0;
        for sCfg in asTestCfg:
            reporter.testStart('%s' % (self.getTestIdString(sCfg, iLvl)));
            iLvl += 1;

    def __del__(self):
        # Make sure the tests are marked as done.
        while self.iTestLvl < len(self.at3TestLvls):
            reporter.testDone();
            self.iTestLvl += 1;

    def getTestIdString(self, oCfg, iLvl):
        """
        Returns a potentially formatted string for the test name.
        """

        # The order of the test levels is reversed so get the level starting
        # from the end.
        _, fnTestFmt, _ = self.at3TestLvls[len(self.at3TestLvls) - 1 - iLvl];
        if fnTestFmt is not None:
            return fnTestFmt(oCfg);
        return oCfg;

    def isTestCfgBlacklisted(self, asTestCfg):
        """
        Returns whether the given test config is black listed.
        """
        fBlacklisted = False;

        for asTestBlacklist in self.aasTestsBlacklist:
            iLvl = 0;
            fBlacklisted = True;
            while iLvl < len(asTestBlacklist) and iLvl < len(asTestCfg):
                if asTestBlacklist[iLvl] != asTestCfg[iLvl] and asTestBlacklist[iLvl] != '*':
                    fBlacklisted = False;
                    break;

                iLvl += 1;

        if not fBlacklisted and self.fnIsCfgSupported is not None:
            fBlacklisted = not self.fnIsCfgSupported(asTestCfg);

        return fBlacklisted;

    def advanceTestCfg(self):
        """
        Advances to the next test config and returns it as an
        array of strings or an empty config if there is no test left anymore.
        """
        iTestCfg, fnTestFmt, asTestCfg = self.at3TestLvls[self.iTestLvl];
        iTestCfg += 1;
        self.at3TestLvls[self.iTestLvl] = (iTestCfg, fnTestFmt, asTestCfg);
        while iTestCfg == len(asTestCfg) and self.iTestLvl < len(self.at3TestLvls):
            self.at3TestLvls[self.iTestLvl] = (0, fnTestFmt, asTestCfg);
            self.iTestLvl += 1;
            if self.iTestLvl < len(self.at3TestLvls):
                iTestCfg, fnTestFmt, asTestCfg = self.at3TestLvls[self.iTestLvl];
                iTestCfg += 1;
                self.at3TestLvls[self.iTestLvl] = (iTestCfg, fnTestFmt, asTestCfg);
                if iTestCfg < len(asTestCfg):
                    self.iTestLvl = 0;
                    break;
            else:
                break; # We reached the end of our tests.

        return self.getCurrentTestCfg();

    def getCurrentTestCfg(self):
        """
        Returns the current not black listed test config as an array of strings.
        """
        asTestCfg = [];

        if self.iTestLvl < len(self.at3TestLvls):
            for t3TestLvl in self.at3TestLvls:
                iTestCfg, _, asTestLvl = t3TestLvl;
                asTestCfg.append(asTestLvl[iTestCfg]);

            asTestCfg.reverse()

        return asTestCfg;

    def getNextTestCfg(self, fSkippedLast = False):
        """
        Returns the next not blacklisted test config or an empty list if
        there is no test left.
        """
        asTestCfgCur = self.getCurrentTestCfg();

        asTestCfg = self.advanceTestCfg();
        while asTestCfg and self.isTestCfgBlacklisted(asTestCfg):
            asTestCfg = self.advanceTestCfg();

        # Compare the current and next config and close the approriate test
        # categories.
        reporter.testDone(fSkippedLast);
        if asTestCfg:
            idxSame = 0;
            while asTestCfgCur[idxSame] == asTestCfg[idxSame]:
                idxSame += 1;

            for i in range(idxSame, len(asTestCfg) - 1):
                reporter.testDone();

            for i in range(idxSame, len(asTestCfg)):
                reporter.testStart('%s' % (self.getTestIdString(asTestCfg[i], i)));

        else:
            # No more tests, mark all tests as done
            for i in range(0, len(asTestCfgCur) - 1):
                reporter.testDone();

        return asTestCfg;

class tdStorageBenchmark(vbox.TestDriver):                                      # pylint: disable=R0902
    """
    Storage benchmark.
    """

    # Global storage configs for the testbox
    kdStorageCfgs = {
        'testboxstor1.de.oracle.com': r'c[3-9]t\dd0\Z',
        'adaris': [ '/dev/sda' ]
    };

    # Available test sets.
    kdTestSets = {
        # Mostly for developing and debugging the testcase.
        'Fast': {
            'RecordSize':  '64k',
            'TestsetSize': '100m',
            'QueueDepth':  '32',
            'DiskSizeGb':  2
        },
        # For quick functionality tests where benchmark results are not required.
        'Functionality': {
            'RecordSize':  '64k',
            'TestsetSize': '2g',
            'QueueDepth':  '32',
            'DiskSizeGb':  10
        },
        # For benchmarking the I/O stack.
        'Benchmark': {
            'RecordSize':  '64k',
            'TestsetSize': '20g',
            'QueueDepth':  '32',
            'DiskSizeGb':  30
        },
        # For stress testing which takes a lot of time.
        'Stress': {
            'RecordSize':  '64k',
            'TestsetSize': '2t',
            'QueueDepth':  '32',
            'DiskSizeGb':  10000
        },
    };

    # Dictionary mapping the virtualization mode mnemonics to a little less cryptic
    # strings used in test descriptions.
    kdVirtModeDescs = {
        'raw'       : 'Raw-mode',
        'hwvirt'    : 'HwVirt',
        'hwvirt-np' : 'NestedPaging'
    };

    kdHostIoCacheDescs = {
        'default'        : 'HostCacheDef',
        'hostiocache'    : 'HostCacheOn',
        'no-hostiocache' : 'HostCacheOff'
    };

    # Array indexes for the test configs.
    kiVmName      = 0;
    kiStorageCtrl = 1;
    kiHostIoCache = 2;
    kiDiskFmt     = 3;
    kiDiskVar     = 4;
    kiCpuCount    = 5;
    kiVirtMode    = 6;
    kiIoTest      = 7;
    kiTestSet     = 8;

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs                 = None;
        self.asTestVMsDef            = ['tst-storage', 'tst-storage32'];
        self.asTestVMs               = self.asTestVMsDef;
        self.asSkipVMs               = [];
        self.asVirtModesDef          = ['hwvirt', 'hwvirt-np', 'raw',]
        self.asVirtModes             = self.asVirtModesDef;
        self.acCpusDef               = [1, 2];
        self.acCpus                  = self.acCpusDef;
        self.asStorageCtrlsDef       = ['AHCI', 'IDE', 'LsiLogicSAS', 'LsiLogic', 'BusLogic', 'NVMe'];
        self.asStorageCtrls          = self.asStorageCtrlsDef;
        self.asHostIoCacheDef        = ['default', 'hostiocache', 'no-hostiocache'];
        self.asHostIoCache           = self.asHostIoCacheDef;
        self.asDiskFormatsDef        = ['VDI', 'VMDK', 'VHD', 'QED', 'Parallels', 'QCOW', 'iSCSI'];
        self.asDiskFormats           = self.asDiskFormatsDef;
        self.asDiskVariantsDef       = ['Dynamic', 'Fixed', 'DynamicSplit2G', 'FixedSplit2G', 'Network'];
        self.asDiskVariants          = self.asDiskVariantsDef;
        self.asTestsDef              = ['iozone', 'fio'];
        self.asTests                 = self.asTestsDef;
        self.asTestSetsDef           = ['Fast', 'Functionality', 'Benchmark', 'Stress'];
        self.asTestSets              = self.asTestSetsDef;
        self.asIscsiTargetsDef       = [ ]; # @todo: Configure one target for basic iSCSI testing
        self.asIscsiTargets          = self.asIscsiTargetsDef;
        self.cDiffLvlsDef            = 0;
        self.cDiffLvls               = self.cDiffLvlsDef;
        self.fTestHost               = False;
        self.fUseScratch             = False;
        self.fRecreateStorCfg        = True;
        self.fReportBenchmarkResults = True;
        self.oStorCfg                = None;
        self.sIoLogPathDef           = self.sScratchPath;
        self.sIoLogPath              = self.sIoLogPathDef;
        self.fIoLog                  = False;
        self.fUseRamDiskDef          = False;
        self.fUseRamDisk             = self.fUseRamDiskDef;

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdStorageBenchmark1 Options:');
        reporter.log('  --virt-modes    <m1[:m2[:]]');
        reporter.log('      Default: %s' % (':'.join(self.asVirtModesDef)));
        reporter.log('  --cpu-counts    <c1[:c2[:]]');
        reporter.log('      Default: %s' % (':'.join(str(c) for c in self.acCpusDef)));
        reporter.log('  --storage-ctrls <type1[:type2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asStorageCtrlsDef)));
        reporter.log('  --host-io-cache <setting1[:setting2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asHostIoCacheDef)));
        reporter.log('  --disk-formats  <type1[:type2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asDiskFormatsDef)));
        reporter.log('  --disk-variants <variant1[:variant2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asDiskVariantsDef)));
        reporter.log('  --iscsi-targets     <target1[:target2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asIscsiTargetsDef)));
        reporter.log('  --tests         <test1[:test2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asTestsDef)));
        reporter.log('  --test-sets     <set1[:set2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asTestSetsDef)));
        reporter.log('  --diff-levels   <number of diffs>');
        reporter.log('      Default: %s' % (self.cDiffLvlsDef));
        reporter.log('  --test-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Test the specified VMs in the given order. Use this to change');
        reporter.log('      the execution order or limit the choice of VMs');
        reporter.log('      Default: %s  (all)' % (':'.join(self.asTestVMsDef)));
        reporter.log('  --skip-vms      <vm1[:vm2[:...]]>');
        reporter.log('      Skip the specified VMs when testing.');
        reporter.log('  --test-host');
        reporter.log('      Do all configured tests on the host first and report the results');
        reporter.log('      to get a baseline');
        reporter.log('  --use-scratch');
        reporter.log('      Use the scratch directory for testing instead of setting up');
        reporter.log('      fresh volumes on dedicated disks (for development)');
        reporter.log('  --always-wipe-storage-cfg');
        reporter.log('      Recreate the host storage config before each test');
        reporter.log('  --dont-wipe-storage-cfg');
        reporter.log('      Don\'t recreate the host storage config before each test');
        reporter.log('  --report-benchmark-results');
        reporter.log('      Report all benchmark results');
        reporter.log('  --dont-report-benchmark-results');
        reporter.log('      Don\'t report any benchmark results');
        reporter.log('  --io-log-path <path>');
        reporter.log('      Default: %s' % (self.sIoLogPathDef));
        reporter.log('  --enable-io-log');
        reporter.log('      Whether to enable I/O logging for each test');
        reporter.log('  --use-ramdisk');
        reporter.log('      Default: %s' % (self.fUseRamDiskDef));
        return rc;

    def parseOption(self, asArgs, iArg):                                        # pylint: disable=R0912,R0915
        if asArgs[iArg] == '--virt-modes':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--virt-modes" takes a colon separated list of modes');
            self.asVirtModes = asArgs[iArg].split(':');
            for s in self.asVirtModes:
                if s not in self.asVirtModesDef:
                    raise base.InvalidOption('The "--virt-modes" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asVirtModesDef)));
        elif asArgs[iArg] == '--cpu-counts':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--cpu-counts" takes a colon separated list of cpu counts');
            self.acCpus = [];
            for s in asArgs[iArg].split(':'):
                try: c = int(s);
                except: raise base.InvalidOption('The "--cpu-counts" value "%s" is not an integer' % (s,));
                if c <= 0:  raise base.InvalidOption('The "--cpu-counts" value "%s" is zero or negative' % (s,));
                self.acCpus.append(c);
        elif asArgs[iArg] == '--storage-ctrls':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--storage-ctrls" takes a colon separated list of Storage controller types');
            self.asStorageCtrls = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--host-io-cache':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--host-io-cache" takes a colon separated list of I/O cache settings');
            self.asHostIoCache = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--disk-formats':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--disk-formats" takes a colon separated list of disk formats');
            self.asDiskFormats = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--disk-variants':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--disk-variants" takes a colon separated list of disk variants');
            self.asDiskVariants = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--iscsi-targets':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--iscsi-targets" takes a colon separated list of iscsi targets');
            self.asIscsiTargets = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--tests':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--tests" takes a colon separated list of tests to run');
            self.asTests = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--test-sets':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--test-sets" takes a colon separated list of test sets');
            self.asTestSets = asArgs[iArg].split(':');
        elif asArgs[iArg] == '--diff-levels':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--diff-levels" takes an integer');
            try: self.cDiffLvls = int(asArgs[iArg]);
            except: raise base.InvalidOption('The "--diff-levels" value "%s" is not an integer' % (asArgs[iArg],));
        elif asArgs[iArg] == '--test-vms':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--test-vms" takes colon separated list');
            self.asTestVMs = asArgs[iArg].split(':');
            for s in self.asTestVMs:
                if s not in self.asTestVMsDef:
                    raise base.InvalidOption('The "--test-vms" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asTestVMsDef)));
        elif asArgs[iArg] == '--skip-vms':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--skip-vms" takes colon separated list');
            self.asSkipVMs = asArgs[iArg].split(':');
            for s in self.asSkipVMs:
                if s not in self.asTestVMsDef:
                    reporter.log('warning: The "--test-vms" value "%s" does not specify any of our test VMs.' % (s));
        elif asArgs[iArg] == '--test-host':
            self.fTestHost = True;
        elif asArgs[iArg] == '--use-scratch':
            self.fUseScratch = True;
        elif asArgs[iArg] == '--always-wipe-storage-cfg':
            self.fRecreateStorCfg = True;
        elif asArgs[iArg] == '--dont-wipe-storage-cfg':
            self.fRecreateStorCfg = False;
        elif asArgs[iArg] == '--report-benchmark-results':
            self.fReportBenchmarkResults = True;
        elif asArgs[iArg] == '--dont-report-benchmark-results':
            self.fReportBenchmarkResults = False;
        elif asArgs[iArg] == '--io-log-path':
            if iArg >= len(asArgs): raise base.InvalidOption('The "--io-log-path" takes a path argument');
            self.sIoLogPath = asArgs[iArg];
        elif asArgs[iArg] == '--enable-io-log':
            self.fIoLog = True;
        elif asArgs[iArg] == '--use-ramdisk':
            self.fUseRamDisk = True;
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def completeOptions(self):
        # Remove skipped VMs from the test list.
        for sVM in self.asSkipVMs:
            try:    self.asTestVMs.remove(sVM);
            except: pass;

        return vbox.TestDriver.completeOptions(self);

    def getResourceSet(self):
        # Construct the resource list the first time it's queried.
        if self.asRsrcs is None:
            self.asRsrcs = [];
            if 'tst-storage' in self.asTestVMs:
                self.asRsrcs.append('5.0/storage/tst-storage.vdi');
            if 'tst-storage32' in self.asTestVMs:
                self.asRsrcs.append('5.0/storage/tst-storage32.vdi');

        return self.asRsrcs;

    def actionConfig(self):

        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        #
        # Configure the VMs we're going to use.
        #

        # Linux VMs
        if 'tst-storage' in self.asTestVMs:
            oVM = self.createTestVM('tst-storage', 1, '5.0/storage/tst-storage.vdi', sKind = 'ArchLinux_64', fIoApic = True, \
                                    eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
                                    eNic0Type = vboxcon.NetworkAdapterType_Am79C973);
            if oVM is None:
                return False;

        if 'tst-storage32' in self.asTestVMs:
            oVM = self.createTestVM('tst-storage32', 1, '5.0/storage/tst-storage32.vdi', sKind = 'ArchLinux', fIoApic = True, \
                                    eNic0AttachType = vboxcon.NetworkAttachmentType_NAT, \
                                    eNic0Type = vboxcon.NetworkAdapterType_Am79C973);
            if oVM is None:
                return False;

        return True;

    def actionExecute(self):
        """
        Execute the testcase.
        """
        fRc = self.test1();
        return fRc;


    #
    # Test execution helpers.
    #

    def prepareStorage(self, oStorCfg, fRamDisk = False, cbPool = None):
        """
        Prepares the host storage for disk images or direct testing on the host.
        """
        # Create a basic pool with the default configuration.
        sMountPoint = None;
        fRc, sPoolId = oStorCfg.createStoragePool(cbPool = cbPool, fRamDisk = fRamDisk);
        if fRc:
            fRc, sMountPoint = oStorCfg.createVolume(sPoolId);
            if not fRc:
                sMountPoint = None;
                oStorCfg.cleanup();

        return sMountPoint;

    def cleanupStorage(self, oStorCfg):
        """
        Cleans up any created storage space for a test.
        """
        return oStorCfg.cleanup();

    def getGuestDisk(self, oSession, oTxsSession, eStorageController):
        """
        Gets the path of the disk in the guest to use for testing.
        """
        lstDisks = None;

        # The naming scheme for NVMe is different and we don't have
        # to query the guest for unformatted disks here because the disk with the OS
        # is not attached to a NVMe controller.
        if eStorageController == vboxcon.StorageControllerType_NVMe:
            lstDisks = [ '/dev/nvme0n1' ];
        else:
            # Find a unformatted disk (no partition).
            # @todo: This is a hack because LIST and STAT are not yet implemented
            #        in TXS (get to this eventually)
            lstBlkDev = [ '/dev/sda', '/dev/sdb' ];
            for sBlkDev in lstBlkDev:
                fRc = oTxsSession.syncExec('/usr/bin/ls', ('ls', sBlkDev + '1'));
                if not fRc:
                    lstDisks = [ sBlkDev ];
                    break;

        _ = oSession;
        return lstDisks;

    def getDiskFormatVariantsForTesting(self, sDiskFmt, asVariants):
        """
        Returns a list of disk variants for testing supported by the given
        disk format and selected for testing.
        """
        lstDskFmts = self.oVBoxMgr.getArray(self.oVBox.systemProperties, 'mediumFormats');
        for oDskFmt in lstDskFmts:
            if oDskFmt.id == sDiskFmt:
                lstDskVariants = [];
                lstCaps = self.oVBoxMgr.getArray(oDskFmt, 'capabilities');

                if     vboxcon.MediumFormatCapabilities_CreateDynamic in lstCaps \
                   and 'Dynamic' in asVariants:
                    lstDskVariants.append('Dynamic');

                if     vboxcon.MediumFormatCapabilities_CreateFixed in lstCaps \
                   and 'Fixed' in asVariants:
                    lstDskVariants.append('Fixed');

                if     vboxcon.MediumFormatCapabilities_CreateSplit2G in lstCaps \
                   and vboxcon.MediumFormatCapabilities_CreateDynamic in lstCaps \
                   and 'DynamicSplit2G' in asVariants:
                    lstDskVariants.append('DynamicSplit2G');

                if     vboxcon.MediumFormatCapabilities_CreateSplit2G in lstCaps \
                   and vboxcon.MediumFormatCapabilities_CreateFixed in lstCaps \
                   and 'FixedSplit2G' in asVariants:
                    lstDskVariants.append('FixedSplit2G');

                if     vboxcon.MediumFormatCapabilities_TcpNetworking in lstCaps \
                   and 'Network' in asVariants:
                    lstDskVariants.append('Network'); # Solely for iSCSI to get a non empty list

                return lstDskVariants;

        return [];

    def convDiskToMediumVariant(self, sDiskVariant):
        """
        Returns a tuple of medium variant flags matching the given disk variant.
        """
        tMediumVariant = None;
        if sDiskVariant == 'Dynamic':
            tMediumVariant = (vboxcon.MediumVariant_Standard, );
        elif sDiskVariant == 'Fixed':
            tMediumVariant = (vboxcon.MediumVariant_Fixed, );
        elif sDiskVariant == 'DynamicSplit2G':
            tMediumVariant = (vboxcon.MediumVariant_Standard, vboxcon.MediumVariant_VmdkSplit2G);
        elif sDiskVariant == 'FixedSplit2G':
            tMediumVariant = (vboxcon.MediumVariant_Fixed, vboxcon.MediumVariant_VmdkSplit2G);

        return tMediumVariant;

    def getStorageCtrlFromName(self, sStorageCtrl):
        """
        Resolves the storage controller string to the matching constant.
        """
        eStorageCtrl = None;

        if sStorageCtrl == 'AHCI':
            eStorageCtrl = vboxcon.StorageControllerType_IntelAhci;
        elif sStorageCtrl == 'IDE':
            eStorageCtrl = vboxcon.StorageControllerType_PIIX4;
        elif sStorageCtrl == 'LsiLogicSAS':
            eStorageCtrl = vboxcon.StorageControllerType_LsiLogicSas;
        elif sStorageCtrl == 'LsiLogic':
            eStorageCtrl = vboxcon.StorageControllerType_LsiLogic;
        elif sStorageCtrl == 'BusLogic':
            eStorageCtrl = vboxcon.StorageControllerType_BusLogic;
        elif sStorageCtrl == 'NVMe':
            eStorageCtrl = vboxcon.StorageControllerType_NVMe;

        return eStorageCtrl;

    def getStorageDriverFromEnum(self, eStorageCtrl, fHardDisk):
        """
        Returns the appropriate driver name for the given storage controller
        and a flag whether the driver has the generic SCSI driver attached.
        """
        if eStorageCtrl == vboxcon.StorageControllerType_IntelAhci:
            if fHardDisk:
                return ('ahci', False);
            return ('ahci', True);
        if eStorageCtrl == vboxcon.StorageControllerType_PIIX4:
            return ('piix3ide', False);
        if eStorageCtrl == vboxcon.StorageControllerType_LsiLogicSas:
            return ('lsilogicsas', True);
        if eStorageCtrl == vboxcon.StorageControllerType_LsiLogic:
            return ('lsilogicscsi', True);
        if eStorageCtrl == vboxcon.StorageControllerType_BusLogic:
            return ('buslogic', True);
        if eStorageCtrl == vboxcon.StorageControllerType_NVMe:
            return ('nvme', False);

        return ('<invalid>', False);

    def isTestCfgSupported(self, asTestCfg):
        """
        Returns whether a specific test config is supported.
        """

        # Check whether the disk variant is supported by the selected format.
        asVariants = self.getDiskFormatVariantsForTesting(asTestCfg[self.kiDiskFmt], [ asTestCfg[self.kiDiskVar] ]);
        if not asVariants:
            return False;

        # For iSCSI check whether we have targets configured.
        if asTestCfg[self.kiDiskFmt] == 'iSCSI' and not self.asIscsiTargets:
            return False;

        # Check for virt mode, CPU count and selected VM.
        if     asTestCfg[self.kiVirtMode] == 'raw' \
           and (asTestCfg[self.kiCpuCount] > 1 or asTestCfg[self.kiVmName] == 'tst-storage'):
            return False;

        # IDE does not support the no host I/O cache setting
        if     asTestCfg[self.kiHostIoCache] == 'no-hostiocache' \
           and asTestCfg[self.kiStorageCtrl] == 'IDE':
            return False;

        return True;

    def fnFormatCpuString(self, cCpus):
        """
        Formats the CPU count to be readable.
        """
        if cCpus == 1:
            return '1 cpu';
        return '%u cpus' % (cCpus);

    def fnFormatVirtMode(self, sVirtMode):
        """
        Formats the virtualization mode to be a little less cryptic for use in test
        descriptions.
        """
        return self.kdVirtModeDescs[sVirtMode];

    def fnFormatHostIoCache(self, sHostIoCache):
        """
        Formats the host I/O cache mode to be a little less cryptic for use in test
        descriptions.
        """
        return self.kdHostIoCacheDescs[sHostIoCache];

    def testBenchmark(self, sTargetOs, sBenchmark, sMountpoint, oExecutor, dTestSet, \
                      cMsTimeout = 3600000):
        """
        Runs the given benchmark on the test host.
        """

        dTestSet['FilePath'] = sMountpoint;
        dTestSet['TargetOs'] = sTargetOs;

        oTst = None;
        if sBenchmark == 'iozone':
            oTst = IozoneTest(oExecutor, dTestSet);
        elif sBenchmark == 'fio':
            oTst = FioTest(oExecutor, dTestSet); # pylint: disable=R0204

        if oTst is not None:
            fRc = oTst.prepare();
            if fRc:
                fRc = oTst.run(cMsTimeout);
                if fRc:
                    if self.fReportBenchmarkResults:
                        fRc = oTst.reportResult();
                else:
                    reporter.testFailure('Running the testcase failed');
                    reporter.addLogString(oTst.getErrorReport(), sBenchmark + '.log',
                                          'log/release/client', 'Benchmark raw output');
            else:
                reporter.testFailure('Preparing the testcase failed');

        oTst.cleanup();

        return fRc;

    def createHd(self, oSession, sDiskFormat, sDiskVariant, iDiffLvl, oHdParent, \
                 sDiskPath, cbDisk):
        """
        Creates a new disk with the given parameters returning the medium object
        on success.
        """

        oHd = None;
        if sDiskFormat == "iSCSI" and iDiffLvl == 0:
            listNames = [];
            listValues = [];
            listValues = self.asIscsiTargets[0].split('|');
            listNames.append('TargetAddress');
            listNames.append('TargetName');
            listNames.append('LUN');

            if self.fpApiVer >= 5.0:
                oHd = oSession.oVBox.createMedium(sDiskFormat, sDiskPath, vboxcon.AccessMode_ReadWrite, \
                                                  vboxcon.DeviceType_HardDisk);
            else:
                oHd = oSession.oVBox.createHardDisk(sDiskFormat, sDiskPath);
            oHd.type = vboxcon.MediumType_Normal;
            oHd.setProperties(listNames, listValues);
        else:
            if iDiffLvl == 0:
                tMediumVariant = self.convDiskToMediumVariant(sDiskVariant);
                oHd = oSession.createBaseHd(sDiskPath + '/base.disk', sDiskFormat, cbDisk, \
                                            cMsTimeout = 3600 * 1000, tMediumVariant = tMediumVariant);
            else:
                sDiskPath = sDiskPath + '/diff_%u.disk' % (iDiffLvl);
                oHd = oSession.createDiffHd(oHdParent, sDiskPath, None);

        return oHd;

    def testOneCfg(self, sVmName, eStorageController, sHostIoCache, sDiskFormat, # pylint: disable=R0913,R0914,R0915
                   sDiskVariant, sDiskPath, cCpus, sIoTest, sVirtMode, sTestSet):
        """
        Runs the specified VM thru test #1.

        Returns a success indicator on the general test execution. This is not
        the actual test result.
        """
        oVM = self.getVmByName(sVmName);

        dTestSet      = self.kdTestSets.get(sTestSet);
        cbDisk        = dTestSet.get('DiskSizeGb') * 1024*1024*1024;
        fHwVirt       = sVirtMode != 'raw';
        fNestedPaging = sVirtMode == 'hwvirt-np';

        fRc = True;
        if sDiskFormat == 'iSCSI':
            sDiskPath = self.asIscsiTargets[0];
        elif self.fUseScratch:
            sDiskPath = self.sScratchPath;
        else:
            # If requested recreate the storage space to start with a clean config
            # for benchmarks
            if self.fRecreateStorCfg:
                sMountPoint = self.prepareStorage(self.oStorCfg, self.fUseRamDisk, cbDisk);
                if sMountPoint is not None:
                    # Create a directory where every normal user can write to.
                    self.oStorCfg.mkDirOnVolume(sMountPoint, 'test', 0777);
                    sDiskPath = sMountPoint + '/test';
                else:
                    fRc = False;
                    reporter.testFailure('Failed to prepare storage for VM');

        if not fRc:
            return fRc;

        lstDisks = []; # List of disks we have to delete afterwards.

        for iDiffLvl in range(self.cDiffLvls + 1):
            sIoLogFile = None;

            if iDiffLvl == 0:
                reporter.testStart('Base');
            else:
                reporter.testStart('Diff %u' % (iDiffLvl));

            # Reconfigure the VM
            oSession = self.openSession(oVM);
            if oSession is not None:
                # Attach HD
                fRc = oSession.ensureControllerAttached(_ControllerTypeToName(eStorageController));
                fRc = fRc and oSession.setStorageControllerType(eStorageController, _ControllerTypeToName(eStorageController));

                if sHostIoCache == 'hostiocache':
                    fRc = fRc and oSession.setStorageControllerHostIoCache(_ControllerTypeToName(eStorageController), True);
                elif sHostIoCache == 'no-hostiocache':
                    fRc = fRc and oSession.setStorageControllerHostIoCache(_ControllerTypeToName(eStorageController), False);

                iDevice = 0;
                if eStorageController == vboxcon.StorageControllerType_PIIX3 or \
                   eStorageController == vboxcon.StorageControllerType_PIIX4:
                    iDevice = 1; # Master is for the OS.

                oHdParent = None;
                if iDiffLvl > 0:
                    oHdParent = lstDisks[0];
                oHd = self.createHd(oSession, sDiskFormat, sDiskVariant, iDiffLvl, oHdParent, sDiskPath, cbDisk);
                if oHd is not None:
                    lstDisks.insert(0, oHd);
                    try:
                        if oSession.fpApiVer >= 4.0:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            0, iDevice, vboxcon.DeviceType_HardDisk, oHd);
                        else:
                            oSession.o.machine.attachDevice(_ControllerTypeToName(eStorageController), \
                                                            0, iDevice, vboxcon.DeviceType_HardDisk, oHd.id);
                    except:
                        reporter.errorXcpt('attachDevice("%s",%s,%s,HardDisk,"%s") failed on "%s"' \
                                           % (_ControllerTypeToName(eStorageController), 1, 0, oHd.id, oSession.sName) );
                        fRc = False;
                    else:
                        reporter.log('attached "%s" to %s' % (sDiskPath, oSession.sName));
                else:
                    fRc = False;

                # Set up the I/O logging config if enabled
                if fRc and self.fIoLog:
                    try:
                        oSession.o.machine.setExtraData('VBoxInternal2/EnableDiskIntegrityDriver', '1');

                        iLun = 0;
                        if eStorageController == vboxcon.StorageControllerType_PIIX3 or \
                           eStorageController == vboxcon.StorageControllerType_PIIX4:
                            iLun = 1
                        sDrv, fDrvScsi = self.getStorageDriverFromEnum(eStorageController, True);
                        if fDrvScsi:
                            sCfgmPath = 'VBoxInternal/Devices/%s/0/LUN#%u/AttachedDriver/Config' % (sDrv, iLun);
                        else:
                            sCfgmPath = 'VBoxInternal/Devices/%s/0/LUN#%u/Config' % (sDrv, iLun);

                        sIoLogFile = '%s/%s.iolog' % (self.sIoLogPath, sDrv);
                        print sCfgmPath;
                        print sIoLogFile;
                        oSession.o.machine.setExtraData('%s/IoLog' % (sCfgmPath,), sIoLogFile);
                    except:
                        reporter.logXcpt();

                fRc = fRc and oSession.enableVirtEx(fHwVirt);
                fRc = fRc and oSession.enableNestedPaging(fNestedPaging);
                fRc = fRc and oSession.setCpuCount(cCpus);
                fRc = fRc and oSession.saveSettings();
                fRc = oSession.close() and fRc and True; # pychecker hack.
                oSession = None;
            else:
                fRc = False;

            # Start up.
            if fRc is True:
                self.logVmInfo(oVM);
                oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(sVmName, fCdWait = False, fNatForwardingForTxs = True);
                if oSession is not None:
                    self.addTask(oSession);

                    # Fudge factor - Allow the guest to finish starting up.
                    self.sleep(5);

                    # Prepare the storage on the guest
                    lstBinaryPaths = ['/bin', '/sbin', '/usr/bin', '/usr/sbin' ];
                    oExecVm = remoteexecutor.RemoteExecutor(oTxsSession, lstBinaryPaths, '${SCRATCH}');
                    oStorCfgVm = storagecfg.StorageCfg(oExecVm, 'linux', self.getGuestDisk(oSession, oTxsSession, \
                                                                                           eStorageController));

                    sMountPoint = self.prepareStorage(oStorCfgVm);
                    if sMountPoint is not None:
                        self.testBenchmark('linux', sIoTest, sMountPoint, oExecVm, dTestSet, \
                                           cMsTimeout = 3 * 3600 * 1000); # 3 hours max (Benchmark and QED takes a lot of time)
                        self.cleanupStorage(oStorCfgVm);
                    else:
                        reporter.testFailure('Failed to prepare storage for the guest benchmark');

                    # cleanup.
                    self.removeTask(oTxsSession);
                    self.terminateVmBySession(oSession);

                    # Add the I/O log if it exists and the test failed
                    if reporter.testErrorCount() > 0 \
                       and sIoLogFile is not None \
                       and os.path.exists(sIoLogFile):
                        reporter.addLogFile(sIoLogFile, 'misc/other', 'I/O log');
                        os.remove(sIoLogFile);

                else:
                    fRc = False;

                # Remove disk
                oSession = self.openSession(oVM);
                if oSession is not None:
                    try:
                        oSession.o.machine.detachDevice(_ControllerTypeToName(eStorageController), 0, iDevice);

                        # Remove storage controller if it is not an IDE controller.
                        if     eStorageController is not vboxcon.StorageControllerType_PIIX3 \
                           and eStorageController is not vboxcon.StorageControllerType_PIIX4:
                            oSession.o.machine.removeStorageController(_ControllerTypeToName(eStorageController));

                        oSession.saveSettings();
                        oSession.saveSettings();
                        oSession.close();
                        oSession = None;
                    except:
                        reporter.errorXcpt('failed to detach/delete disk %s from storage controller' % (sDiskPath));
                else:
                    fRc = False;

            reporter.testDone();

        # Delete all disks
        for oHd in lstDisks:
            self.oVBox.deleteHdByMedium(oHd);

        # Cleanup storage area
        if sDiskFormat != 'iSCSI' and not self.fUseScratch and self.fRecreateStorCfg:
            self.cleanupStorage(self.oStorCfg);

        return fRc;

    def testStorage(self, sDiskPath = None):
        """
        Runs the storage testcase through the selected configurations
        """

        aasTestCfgs = [];
        aasTestCfgs.insert(self.kiVmName,      self.asTestVMs);
        aasTestCfgs.insert(self.kiStorageCtrl, self.asStorageCtrls);
        aasTestCfgs.insert(self.kiHostIoCache, (self.asHostIoCache, self.fnFormatHostIoCache));
        aasTestCfgs.insert(self.kiDiskFmt,     self.asDiskFormats);
        aasTestCfgs.insert(self.kiDiskVar,     self.asDiskVariants);
        aasTestCfgs.insert(self.kiCpuCount,    (self.acCpus, self.fnFormatCpuString));
        aasTestCfgs.insert(self.kiVirtMode,    (self.asVirtModes, self.fnFormatVirtMode));
        aasTestCfgs.insert(self.kiIoTest,      self.asTests);
        aasTestCfgs.insert(self.kiTestSet,     self.asTestSets);

        aasTestsBlacklist = [];
        aasTestsBlacklist.append(['tst-storage', 'BusLogic']); # 64bit Linux is broken with BusLogic

        oTstCfgMgr = StorTestCfgMgr(aasTestCfgs, aasTestsBlacklist, self.isTestCfgSupported);

        fRc = True;
        asTestCfg = oTstCfgMgr.getCurrentTestCfg();
        while asTestCfg:
            fRc = self.testOneCfg(asTestCfg[self.kiVmName], self.getStorageCtrlFromName(asTestCfg[self.kiStorageCtrl]), \
                                  asTestCfg[self.kiHostIoCache], asTestCfg[self.kiDiskFmt], asTestCfg[self.kiDiskVar],
                                  sDiskPath, asTestCfg[self.kiCpuCount], asTestCfg[self.kiIoTest], \
                                  asTestCfg[self.kiVirtMode], asTestCfg[self.kiTestSet]) and fRc and True; # pychecker hack.

            asTestCfg = oTstCfgMgr.getNextTestCfg();

        return fRc;

    def test1(self):
        """
        Executes test #1.
        """

        fRc = True;
        oDiskCfg = self.kdStorageCfgs.get(socket.gethostname().lower());

        # Test the host first if requested
        if oDiskCfg is not None or self.fUseScratch:
            lstBinaryPaths = ['/bin', '/sbin', '/usr/bin', '/usr/sbin', \
                              '/opt/csw/bin', '/usr/ccs/bin', '/usr/sfw/bin'];
            oExecutor = remoteexecutor.RemoteExecutor(None, lstBinaryPaths, self.sScratchPath);
            if not self.fUseScratch:
                self.oStorCfg = storagecfg.StorageCfg(oExecutor, utils.getHostOs(), oDiskCfg);

                # Try to cleanup any leftovers from a previous run first.
                fRc = self.oStorCfg.cleanupLeftovers();
                if not fRc:
                    reporter.error('Failed to cleanup any leftovers from a previous run');

            if self.fTestHost:
                reporter.testStart('Host');
                if self.fUseScratch:
                    sMountPoint = self.sScratchPath;
                else:
                    sMountPoint = self.prepareStorage(self.oStorCfg);
                if sMountPoint is not None:
                    for sIoTest in self.asTests:
                        reporter.testStart(sIoTest);
                        for sTestSet in self.asTestSets:
                            reporter.testStart(sTestSet);
                            dTestSet = self.kdTestSets.get(sTestSet);
                            self.testBenchmark(utils.getHostOs(), sIoTest, sMountPoint, oExecutor, dTestSet);
                            reporter.testDone();
                        reporter.testDone();
                    self.cleanupStorage(self.oStorCfg);
                else:
                    reporter.testFailure('Failed to prepare host storage');
                    fRc = False;
                reporter.testDone();
            else:
                # Create the storage space first if it is not done before every test.
                sMountPoint = None;
                if self.fUseScratch:
                    sMountPoint = self.sScratchPath;
                elif not self.fRecreateStorCfg:
                    reporter.testStart('Create host storage');
                    sMountPoint = self.prepareStorage(self.oStorCfg);
                    if sMountPoint is None:
                        reporter.testFailure('Failed to prepare host storage');
                        fRc = False;
                    self.oStorCfg.mkDirOnVolume(sMountPoint, 'test', 0777);
                    sMountPoint = sMountPoint + '/test';
                    reporter.testDone();

                if fRc:
                    # Run the storage tests.
                    if not self.testStorage(sMountPoint):
                        fRc = False;

                if not self.fRecreateStorCfg and not self.fUseScratch:
                    self.cleanupStorage(self.oStorCfg);
        else:
            fRc = False;

        return fRc;

if __name__ == '__main__':
    sys.exit(tdStorageBenchmark().main(sys.argv));

