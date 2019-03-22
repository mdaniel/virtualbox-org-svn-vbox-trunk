# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Test VMs
"""

__copyright__ = \
"""
Copyright (C) 2010-2017 Oracle Corporation

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
import copy;
import os;
import re;
import random;
import socket;
import string;

# Validation Kit imports.
from testdriver import base;
from testdriver import reporter;
from testdriver import vboxcon;
from common import utils;


# All virtualization modes.
g_asVirtModes      = ['hwvirt', 'hwvirt-np', 'raw',];
# All virtualization modes except for raw-mode.
g_asVirtModesNoRaw = ['hwvirt', 'hwvirt-np',];
# Dictionary mapping the virtualization mode mnemonics to a little less cryptic
# strings used in test descriptions.
g_dsVirtModeDescs  = {
    'raw'       : 'Raw-mode',
    'hwvirt'    : 'HwVirt',
    'hwvirt-np' : 'NestedPaging'
};

## @name VM grouping flags
## @{
g_kfGrpSmoke     = 0x0001;                          ##< Smoke test VM.
g_kfGrpStandard  = 0x0002;                          ##< Standard test VM.
g_kfGrpStdSmoke  = g_kfGrpSmoke | g_kfGrpStandard;  ##< shorthand.
g_kfGrpWithGAs   = 0x0004;                          ##< The VM has guest additions installed.
g_kfGrpNoTxs     = 0x0008;                          ##< The VM lacks test execution service.
g_kfGrpAncient   = 0x1000;                          ##< Ancient OS.
g_kfGrpExotic    = 0x2000;                          ##< Exotic OS.
## @}


## @name Flags.
## @{
g_k32           = 32;                   # pylint: disable=C0103
g_k64           = 64;                   # pylint: disable=C0103
g_k32_64        = 96;                   # pylint: disable=C0103
g_kiArchMask    = 96;
g_kiNoRaw       = 128;                  ##< No raw mode.
## @}

# Array indexes.
g_iGuestOsType = 0;
g_iKind        = 1;
g_iFlags       = 2;
g_iMinCpu      = 3;
g_iMaxCpu      = 4;
g_iRegEx       = 5;

# Table translating from VM name core to a more detailed guest info.
# pylint: disable=C0301
g_aaNameToDetails = \
[
    [ 'WindowsNT3x',    'WindowsNT3x',           g_k32,    1,  32, ['nt3',    'nt3[0-9]*']],                              # max cpus??
    [ 'WindowsNT4',     'WindowsNT4',            g_k32,    1,  32, ['nt4',    'nt4sp[0-9]']],                             # max cpus??
    [ 'Windows2000',    'Windows2000',           g_k32,    1,  32, ['w2k',    'w2ksp[0-9]', 'win2k', 'win2ksp[0-9]']],    # max cpus??
    [ 'WindowsXP',      'WindowsXP',             g_k32,    1,  32, ['xp',     'xpsp[0-9]']],
    [ 'WindowsXP_64',   'WindowsXP_64',          g_k64,    1,  32, ['xp64',   'xp64sp[0-9]']],
    [ 'Windows2003',    'Windows2003',           g_k32,    1,  32, ['w2k3',   'w2k3sp[0-9]', 'win2k3', 'win2k3sp[0-9]']],
    [ 'WindowsVista',   'WindowsVista',          g_k32,    1,  32, ['vista',  'vistasp[0-9]']],
    [ 'WindowsVista_64','WindowsVista_64',       g_k64,    1,  64, ['vista-64', 'vistasp[0-9]-64',]],  # max cpus/cores??
    [ 'Windows2008',    'Windows2008',           g_k32,    1,  64, ['w2k8',   'w2k8sp[0-9]', 'win2k8', 'win2k8sp[0-9]']],     # max cpus/cores??
    [ 'Windows2008_64', 'Windows2008_64',        g_k64,    1,  64, ['w2k8r2', 'w2k8r2sp[0-9]', 'win2k8r2', 'win2k8r2sp[0-9]']], # max cpus/cores??
    [ 'Windows7',       'Windows7',              g_k32,    1,  32, ['w7',     'w7sp[0-9]', 'win7',]],        # max cpus/cores??
    [ 'Windows7_64',    'Windows7_64',           g_k64,    1,  64, ['w7-64',  'w7sp[0-9]-64', 'win7-64',]],  # max cpus/cores??
    [ 'Windows8',       'Windows8',     g_k32 | g_kiNoRaw, 1,  32, ['w8',     'w8sp[0-9]', 'win8',]],        # max cpus/cores??
    [ 'Windows8_64',    'Windows8_64',           g_k64,    1,  64, ['w8-64',  'w8sp[0-9]-64', 'win8-64',]],  # max cpus/cores??
    [ 'Windows81',      'Windows81',    g_k32 | g_kiNoRaw, 1,  32, ['w81',    'w81sp[0-9]', 'win81',]],       # max cpus/cores??
    [ 'Windows81_64',   'Windows81_64',          g_k64,    1,  64, ['w81-64', 'w81sp[0-9]-64', 'win81-64',]], # max cpus/cores??
    [ 'Windows10',      'Windows10',    g_k32 | g_kiNoRaw, 1,  32, ['w10',    'w10sp[0-9]', 'win10',]],       # max cpus/cores??
    [ 'Windows10_64',   'Windows10_64',          g_k64,    1,  64, ['w10-64', 'w10sp[0-9]-64', 'win10-64',]], # max cpus/cores??
    [ 'Linux',          'Debian',                g_k32,    1, 256, ['deb[0-9]*', 'debian[0-9]*', ]],
    [ 'Linux_64',       'Debian_64',             g_k64,    1, 256, ['deb[0-9]*-64', 'debian[0-9]*-64', ]],
    [ 'Linux',          'RedHat',                g_k32,    1, 256, ['rhel',   'rhel[0-9]', 'rhel[0-9]u[0-9]']],
    [ 'Linux',          'Fedora',                g_k32,    1, 256, ['fedora', 'fedora[0-9]*', ]],
    [ 'Linux_64',       'Fedora_64',             g_k64,    1, 256, ['fedora-64', 'fedora[0-9]*-64', ]],
    [ 'Linux',          'Oracle',                g_k32,    1, 256, ['ols[0-9]*', 'oel[0-9]*', ]],
    [ 'Linux_64',       'Oracle_64',             g_k64,    1, 256, ['ols[0-9]*-64', 'oel[0-9]*-64', ]],
    [ 'Linux',          'OpenSUSE',              g_k32,    1, 256, ['opensuse[0-9]*', 'suse[0-9]*', ]],
    [ 'Linux_64',       'OpenSUSE_64',           g_k64,    1, 256, ['opensuse[0-9]*-64', 'suse[0-9]*-64', ]],
    [ 'Linux',          'Ubuntu',                g_k32,    1, 256, ['ubuntu[0-9]*', ]],
    [ 'Linux_64',       'Ubuntu_64',             g_k64,    1, 256, ['ubuntu[0-9]*-64', ]],
    [ 'Linux',          'ArchLinux',             g_k32,    1, 256, ['arch[0-9]*', ]],
    [ 'Linux_64',       'ArchLinux_64',          g_k64,    1, 256, ['arch[0-9]*-64', ]],
    [ 'Solaris',        'Solaris',               g_k32,    1, 256, ['sol10',  'sol10u[0-9]']],
    [ 'Solaris_64',     'Solaris_64',            g_k64,    1, 256, ['sol10-64', 'sol10u-64[0-9]']],
    [ 'Solaris_64',     'Solaris11_64',          g_k64,    1, 256, ['sol11u1']],
    [ 'BSD',            'FreeBSD_64',            g_k32_64, 1, 1,   ['bs-.*']], # boot sectors, wanted 64-bit type.
    [ 'DOS',            'DOS',                   g_k32,    1, 1,   ['bs-.*']],
];


## @name Guest OS type string constants.
## @{
g_ksGuestOsTypeDarwin  = 'darwin';
g_ksGuestOsTypeDOS     = 'dos';
g_ksGuestOsTypeFreeBSD = 'freebsd';
g_ksGuestOsTypeLinux   = 'linux';
g_ksGuestOsTypeOS2     = 'os2';
g_ksGuestOsTypeSolaris = 'solaris';
g_ksGuestOsTypeWindows = 'windows';
## @}

## @name String constants for paravirtualization providers.
## @{
g_ksParavirtProviderNone    = 'none';
g_ksParavirtProviderDefault = 'default';
g_ksParavirtProviderLegacy  = 'legacy';
g_ksParavirtProviderMinimal = 'minimal';
g_ksParavirtProviderHyperV  = 'hyperv';
g_ksParavirtProviderKVM     = 'kvm';
## @}

## Valid paravirtualization providers.
g_kasParavirtProviders = ( g_ksParavirtProviderNone, g_ksParavirtProviderDefault, g_ksParavirtProviderLegacy,
                           g_ksParavirtProviderMinimal, g_ksParavirtProviderHyperV, g_ksParavirtProviderKVM );

# Mapping for support of paravirtualisation providers per guest OS.
#g_kdaParavirtProvidersSupported = {
#    g_ksGuestOsTypeDarwin  : ( g_ksParavirtProviderMinimal, ),
#    g_ksGuestOsTypeFreeBSD : ( g_ksParavirtProviderNone, g_ksParavirtProviderMinimal, ),
#    g_ksGuestOsTypeLinux   : ( g_ksParavirtProviderNone, g_ksParavirtProviderMinimal, g_ksParavirtProviderHyperV, g_ksParavirtProviderKVM),
#    g_ksGuestOsTypeOS2     : ( g_ksParavirtProviderNone, ),
#    g_ksGuestOsTypeSolaris : ( g_ksParavirtProviderNone, ),
#    g_ksGuestOsTypeWindows : ( g_ksParavirtProviderNone, g_ksParavirtProviderMinimal, g_ksParavirtProviderHyperV, )
#}
# Temporary tweak:
#   since for the most guests g_ksParavirtProviderNone is almost the same as g_ksParavirtProviderMinimal,
#   g_ksParavirtProviderMinimal is removed from the list in order to get maximum number of unique choices
#   during independent test runs when paravirt provider is taken randomly.
g_kdaParavirtProvidersSupported = {
    g_ksGuestOsTypeDarwin  : ( g_ksParavirtProviderMinimal, ),
    g_ksGuestOsTypeDOS     : ( g_ksParavirtProviderNone, ),
    g_ksGuestOsTypeFreeBSD : ( g_ksParavirtProviderNone, ),
    g_ksGuestOsTypeLinux   : ( g_ksParavirtProviderNone, g_ksParavirtProviderHyperV, g_ksParavirtProviderKVM),
    g_ksGuestOsTypeOS2     : ( g_ksParavirtProviderNone, ),
    g_ksGuestOsTypeSolaris : ( g_ksParavirtProviderNone, ),
    g_ksGuestOsTypeWindows : ( g_ksParavirtProviderNone, g_ksParavirtProviderHyperV, )
}


# pylint: enable=C0301

def _intersects(asSet1, asSet2):
    """
    Checks if any of the strings in set 1 matches any of the regular
    expressions in set 2.
    """
    for sStr1 in asSet1:
        for sRx2 in asSet2:
            if re.match(sStr1, sRx2 + '$'):
                return True;
    return False;


class TestVm(object):
    """
    A Test VM - name + VDI/whatever.

    This is just a data object.
    """

    def __init__(self, # pylint: disable=R0913
                 sVmName,                                   # type: str
                 fGrouping = 0,                             # type: int
                 oSet = None,                               # type: TestVmSet
                 sHd = None,                                # type: str
                 sKind = None,                              # type: str
                 acCpusSup = None,                          # type: List[int]
                 asVirtModesSup = None,                     # type: List[str]
                 fIoApic = None,                            # type: bool
                 fNstHwVirt = False,                        # type: bool
                 fPae = None,                               # type: bool
                 sNic0AttachType = None,                    # type: str
                 sFloppy = None,                            # type: str
                 fVmmDevTestingPart = None,                 # type: bool
                 fVmmDevTestingMmio = False,                # type: bool
                 asParavirtModesSup = None,                 # type: List[str]
                 fRandomPvPMode = False,                    # type: bool
                 sFirmwareType = 'bios',                    # type: str
                 sChipsetType = 'piix3',                    # type: str
                 sHddControllerType = 'IDE Controller',     # type: str
                 sDvdControllerType = 'IDE Controller'      # type: str
                 ):
        self.oSet                    = oSet;
        self.sVmName                 = sVmName;
        self.fGrouping               = fGrouping;
        self.sHd                     = sHd;          # Relative to the testrsrc root.
        self.acCpusSup               = acCpusSup;
        self.asVirtModesSup          = asVirtModesSup;
        self.asParavirtModesSup      = asParavirtModesSup;
        self.asParavirtModesSupOrg   = asParavirtModesSup; # HACK ALERT! Trick to make the 'effing random mess not get in the
                                                           # way of actively selecting virtualization modes.
        self.sKind                   = sKind;
        self.sGuestOsType            = None;
        self.sDvdImage               = None;         # Relative to the testrsrc root.
        self.sDvdControllerType      = sDvdControllerType;
        self.fIoApic                 = fIoApic;
        self.fNstHwVirt              = fNstHwVirt;
        self.fPae                    = fPae;
        self.sNic0AttachType         = sNic0AttachType;
        self.sHddControllerType      = sHddControllerType;
        self.sFloppy                 = sFloppy;      # Relative to the testrsrc root, except when it isn't...
        self.fVmmDevTestingPart      = fVmmDevTestingPart;
        self.fVmmDevTestingMmio      = fVmmDevTestingMmio;
        self.sFirmwareType           = sFirmwareType;
        self.sChipsetType            = sChipsetType;
        self.fCom1RawFile            = False;

        self.fSnapshotRestoreCurrent = False;        # Whether to restore execution on the current snapshot.
        self.fSkip                   = False;        # All VMs are included in the configured set by default.
        self.aInfo                   = None;
        self.sCom1RawFile            = None;         # Set by createVmInner and getReconfiguredVm if fCom1RawFile is set.
        self._guessStuff(fRandomPvPMode);

    def _mkCanonicalGuestOSType(self, sType):
        """
        Convert guest OS type into constant representation.
        Raise exception if specified @param sType is unknown.
        """
        if sType.lower().startswith('darwin'):
            return g_ksGuestOsTypeDarwin
        if sType.lower().startswith('bsd'):
            return g_ksGuestOsTypeFreeBSD
        if sType.lower().startswith('dos'):
            return g_ksGuestOsTypeDOS
        if sType.lower().startswith('linux'):
            return g_ksGuestOsTypeLinux
        if sType.lower().startswith('os2'):
            return g_ksGuestOsTypeOS2
        if sType.lower().startswith('solaris'):
            return g_ksGuestOsTypeSolaris
        if sType.lower().startswith('windows'):
            return g_ksGuestOsTypeWindows
        raise base.GenError(sWhat="unknown guest OS kind: %s" % str(sType))

    def _guessStuff(self, fRandomPvPMode):
        """
        Used by the constructor to guess stuff.
        """

        sNm     = self.sVmName.lower().strip();
        asSplit = sNm.replace('-', ' ').split(' ');

        if self.sKind is None:
            # From name.
            for aInfo in g_aaNameToDetails:
                if _intersects(asSplit, aInfo[g_iRegEx]):
                    self.aInfo        = aInfo;
                    self.sGuestOsType = self._mkCanonicalGuestOSType(aInfo[g_iGuestOsType])
                    self.sKind        = aInfo[g_iKind];
                    break;
            if self.sKind is None:
                reporter.fatal('The OS of test VM "%s" cannot be guessed' % (self.sVmName,));

            # Check for 64-bit, if required and supported.
            if (self.aInfo[g_iFlags] & g_kiArchMask) == g_k32_64  and  _intersects(asSplit, ['64', 'amd64']):
                self.sKind = self.sKind + '_64';
        else:
            # Lookup the kind.
            for aInfo in g_aaNameToDetails:
                if self.sKind == aInfo[g_iKind]:
                    self.aInfo = aInfo;
                    break;
            if self.aInfo is None:
                reporter.fatal('The OS of test VM "%s" with sKind="%s" cannot be guessed' % (self.sVmName, self.sKind));

        # Translate sKind into sGuest OS Type.
        if self.sGuestOsType is None:
            if self.aInfo is not None:
                self.sGuestOsType = self._mkCanonicalGuestOSType(self.aInfo[g_iGuestOsType])
            elif self.sKind.find("Windows") >= 0:
                self.sGuestOsType = g_ksGuestOsTypeWindows
            elif self.sKind.find("Linux") >= 0:
                self.sGuestOsType = g_ksGuestOsTypeLinux;
            elif self.sKind.find("Solaris") >= 0:
                self.sGuestOsType = g_ksGuestOsTypeSolaris;
            elif self.sKind.find("DOS") >= 0:
                self.sGuestOsType = g_ksGuestOsTypeDOS;
            else:
                reporter.fatal('The OS of test VM "%s", sKind="%s" cannot be guessed' % (self.sVmName, self.sKind));

        # Restrict modes and such depending on the OS.
        if self.asVirtModesSup is None:
            self.asVirtModesSup = list(g_asVirtModes);
            if   self.sGuestOsType in (g_ksGuestOsTypeOS2, g_ksGuestOsTypeDarwin) \
              or self.sKind.find('_64') > 0 \
              or (self.aInfo is not None and (self.aInfo[g_iFlags] & g_kiNoRaw)):
                self.asVirtModesSup = [sVirtMode for sVirtMode in self.asVirtModesSup if sVirtMode != 'raw'];
            # TEMPORARY HACK - START
            sHostName = socket.getfqdn();
            if sHostName.startswith('testboxpile1'):
                self.asVirtModesSup = [sVirtMode for sVirtMode in self.asVirtModesSup if sVirtMode != 'raw'];
            # TEMPORARY HACK - END

        # Restrict the CPU count depending on the OS and/or percieved SMP readiness.
        if self.acCpusSup is None:
            if _intersects(asSplit, ['uni']):
                self.acCpusSup = [1];
            elif self.aInfo is not None:
                self.acCpusSup = [i for i in range(self.aInfo[g_iMinCpu], self.aInfo[g_iMaxCpu]) ];
            else:
                self.acCpusSup = [1];

        # Figure relevant PV modes based on the OS.
        if self.asParavirtModesSup is None:
            self.asParavirtModesSup = g_kdaParavirtProvidersSupported[self.sGuestOsType];
            ## @todo Remove this hack as soon as we've got around to explictly configure test variations
            ## on the server side. Client side random is interesting but not the best option.
            self.asParavirtModesSupOrg = self.asParavirtModesSup;
            if fRandomPvPMode:
                random.seed();
                self.asParavirtModesSup = (random.choice(self.asParavirtModesSup),);

        return True;

    def getMissingResources(self, sTestRsrc):
        """
        Returns a list of missing resources (paths, stuff) that the VM needs.
        """
        asRet = [];
        for sPath in [ self.sHd, self.sDvdImage, self.sFloppy]:
            if sPath is not None:
                if not os.path.isabs(sPath):
                    sPath = os.path.join(sTestRsrc, sPath);
                if not os.path.exists(sPath):
                    asRet.append(sPath);
        return asRet;

    def createVm(self, oTestDrv, eNic0AttachType = None, sDvdImage = None):
        """
        Creates the VM with defaults and the few tweaks as per the arguments.

        Returns same as vbox.TestDriver.createTestVM.
        """
        if sDvdImage is not None:
            sMyDvdImage = sDvdImage;
        else:
            sMyDvdImage = self.sDvdImage;

        if eNic0AttachType is not None:
            eMyNic0AttachType = eNic0AttachType;
        elif self.sNic0AttachType is None:
            eMyNic0AttachType = None;
        elif self.sNic0AttachType == 'nat':
            eMyNic0AttachType = vboxcon.NetworkAttachmentType_NAT;
        elif self.sNic0AttachType == 'bridged':
            eMyNic0AttachType = vboxcon.NetworkAttachmentType_Bridged;
        else:
            assert False, self.sNic0AttachType;

        return self.createVmInner(oTestDrv, eMyNic0AttachType, sMyDvdImage);

    def _generateRawPortFilename(self, oTestDrv, sInfix, sSuffix):
        """ Generates a raw port filename. """
        random.seed();
        sRandom = ''.join(random.choice(string.ascii_lowercase + string.digits) for _ in range(10));
        return os.path.join(oTestDrv.sScratchPath, self.sVmName + sInfix + sRandom + sSuffix);

    def createVmInner(self, oTestDrv, eNic0AttachType, sDvdImage):
        """
        Same as createVm but parameters resolved.

        Returns same as vbox.TestDriver.createTestVM.
        """
        reporter.log2('');
        reporter.log2('Calling createTestVM on %s...' % (self.sVmName,))
        if self.fCom1RawFile:
            self.sCom1RawFile = self._generateRawPortFilename(oTestDrv, '-com1-', '.out');
        return oTestDrv.createTestVM(self.sVmName,
                                     1,                 # iGroup
                                     sHd                = self.sHd,
                                     sKind              = self.sKind,
                                     fIoApic            = self.fIoApic,
                                     fNstHwVirt         = self.fNstHwVirt,
                                     fPae               = self.fPae,
                                     eNic0AttachType    = eNic0AttachType,
                                     sDvdImage          = sDvdImage,
                                     sDvdControllerType = self.sDvdControllerType,
                                     sHddControllerType = self.sHddControllerType,
                                     sFloppy            = self.sFloppy,
                                     fVmmDevTestingPart = self.fVmmDevTestingPart,
                                     fVmmDevTestingMmio = self.fVmmDevTestingPart,
                                     sFirmwareType      = self.sFirmwareType,
                                     sChipsetType       = self.sChipsetType,
                                     sCom1RawFile       = self.sCom1RawFile if self.fCom1RawFile else None
                                     );

    def getReconfiguredVm(self, oTestDrv, cCpus, sVirtMode, sParavirtMode = None):
        """
        actionExecute worker that finds and reconfigure a test VM.

        Returns (fRc, oVM) where fRc is True, None or False and oVM is a
        VBox VM object that is only present when rc is True.
        """

        fRc = False;
        oVM = oTestDrv.getVmByName(self.sVmName);
        if oVM is not None:
            if self.fSnapshotRestoreCurrent is True:
                fRc = True;
            else:
                fHostSupports64bit = oTestDrv.hasHostLongMode();
                if self.is64bitRequired() and not fHostSupports64bit:
                    fRc = None; # Skip the test.
                elif self.isViaIncompatible() and oTestDrv.isHostCpuVia():
                    fRc = None; # Skip the test.
                elif self.isP4Incompatible() and oTestDrv.isHostCpuP4():
                    fRc = None; # Skip the test.
                else:
                    oSession = oTestDrv.openSession(oVM);
                    if oSession is not None:
                        fRc =         oSession.enableVirtEx(sVirtMode != 'raw');
                        fRc = fRc and oSession.enableNestedPaging(sVirtMode == 'hwvirt-np');
                        fRc = fRc and oSession.setCpuCount(cCpus);
                        if cCpus > 1:
                            fRc = fRc and oSession.enableIoApic(True);

                        if sParavirtMode is not None and oSession.fpApiVer >= 5.0:
                            adParavirtProviders = {
                                g_ksParavirtProviderNone   : vboxcon.ParavirtProvider_None,
                                g_ksParavirtProviderDefault: vboxcon.ParavirtProvider_Default,
                                g_ksParavirtProviderLegacy : vboxcon.ParavirtProvider_Legacy,
                                g_ksParavirtProviderMinimal: vboxcon.ParavirtProvider_Minimal,
                                g_ksParavirtProviderHyperV : vboxcon.ParavirtProvider_HyperV,
                                g_ksParavirtProviderKVM    : vboxcon.ParavirtProvider_KVM,
                            };
                            fRc = fRc and oSession.setParavirtProvider(adParavirtProviders[sParavirtMode]);

                        fCfg64Bit = self.is64bitRequired() or (self.is64bit() and fHostSupports64bit and sVirtMode != 'raw');
                        fRc = fRc and oSession.enableLongMode(fCfg64Bit);
                        if fCfg64Bit: # This is to avoid GUI pedantic warnings in the GUI. Sigh.
                            oOsType = oSession.getOsType();
                            if oOsType is not None:
                                if oOsType.is64Bit and sVirtMode == 'raw':
                                    assert(oOsType.id[-3:] == '_64');
                                    fRc = fRc and oSession.setOsType(oOsType.id[:-3]);
                                elif not oOsType.is64Bit and sVirtMode != 'raw':
                                    fRc = fRc and oSession.setOsType(oOsType.id + '_64');

                        # New serial raw file.
                        if fRc and self.fCom1RawFile:
                            self.sCom1RawFile = self._generateRawPortFilename(oTestDrv, '-com1-', '.out');
                            utils.noxcptDeleteFile(self.sCom1RawFile);
                            fRc = oSession.setupSerialToRawFile(0, self.sCom1RawFile);

                        # Make life simpler for child classes.
                        if fRc:
                            fRc = self._childVmReconfig(oTestDrv, oVM, oSession);

                        fRc = fRc and oSession.saveSettings();
                        if not oSession.close():
                            fRc = False;
            if fRc is True:
                return (True, oVM);
        return (fRc, None);

    def _childVmReconfig(self, oTestDrv, oVM, oSession):
        """ Hook into getReconfiguredVm() for children. """
        _ = oTestDrv; _ = oVM; _ = oSession;
        return True;

    def isWindows(self):
        """ Checks if it's a Windows VM. """
        return self.sGuestOsType == g_ksGuestOsTypeWindows;

    def isOS2(self):
        """ Checks if it's an OS/2 VM. """
        return self.sGuestOsType == g_ksGuestOsTypeOS2;

    def isLinux(self):
        """ Checks if it's an Linux VM. """
        return self.sGuestOsType == g_ksGuestOsTypeLinux;

    def is64bit(self):
        """ Checks if it's a 64-bit VM. """
        return self.sKind.find('_64') >= 0;

    def is64bitRequired(self):
        """ Check if 64-bit is required or not. """
        return (self.aInfo[g_iFlags] & g_k64) != 0;

    def isLoggedOntoDesktop(self):
        """ Checks if the test VM is logging onto a graphical desktop by default. """
        if self.isWindows():
            return True;
        if self.isOS2():
            return True;
        if self.sVmName.find('-desktop'):
            return True;
        return False;

    def isViaIncompatible(self):
        """
        Identifies VMs that doesn't work on VIA.

        Returns True if NOT supported on VIA, False if it IS supported.
        """
        # Oracle linux doesn't like VIA in our experience
        if self.aInfo[g_iKind] in ['Oracle', 'Oracle_64']:
            return True;
        # OS/2: "The system detected an internal processing error at location
        # 0168:fff1da1f - 000e:ca1f. 0a8606fd
        if self.isOS2():
            return True;
        # Windows NT4 before SP4 won't work because of cmpxchg8b not being
        # detected, leading to a STOP 3e(80,0,0,0).
        if self.aInfo[g_iKind] == 'WindowsNT4':
            if self.sVmName.find('sp') < 0:
                return True; # no service pack.
            if   self.sVmName.find('sp0') >= 0 \
              or self.sVmName.find('sp1') >= 0 \
              or self.sVmName.find('sp2') >= 0 \
              or self.sVmName.find('sp3') >= 0:
                return True;
        # XP x64 on a physical VIA box hangs exactly like a VM.
        if self.aInfo[g_iKind] in ['WindowsXP_64', 'Windows2003_64']:
            return True;
        # Vista 64 throws BSOD 0x5D (UNSUPPORTED_PROCESSOR)
        if self.aInfo[g_iKind] in ['WindowsVista_64']:
            return True;
        # Solaris 11 hangs on VIA, tested on a physical box (testboxvqc)
        if self.aInfo[g_iKind] in ['Solaris11_64']:
            return True;
        return False;

    def isP4Incompatible(self):
        """
        Identifies VMs that doesn't work on Pentium 4 / Pentium D.

        Returns True if NOT supported on P4, False if it IS supported.
        """
        # Stupid 1 kHz timer. Too much for antique CPUs.
        if self.sVmName.find('rhel5') >= 0:
            return True;
        # Due to the boot animation the VM takes forever to boot.
        if self.aInfo[g_iKind] == 'Windows2000':
            return True;
        return False;


class BootSectorTestVm(TestVm):
    """
    A Boot Sector Test VM.
    """

    def __init__(self, oSet, sVmName, sFloppy = None, asVirtModesSup = None, f64BitRequired = False):
        self.f64BitRequired = f64BitRequired;
        if asVirtModesSup is None:
            asVirtModesSup = list(g_asVirtModes);
        TestVm.__init__(self, sVmName,
                        oSet = oSet,
                        acCpusSup = [1,],
                        sFloppy = sFloppy,
                        asVirtModesSup = asVirtModesSup,
                        fPae = True,
                        fIoApic = True,
                        fVmmDevTestingPart = True,
                        fVmmDevTestingMmio = True,
                        );

    def is64bitRequired(self):
        return self.f64BitRequired;


class AncientTestVm(TestVm):
    """
    A ancient Test VM, using the serial port for communicating results.

    We're looking for 'PASSED' and 'FAILED' lines in the COM1 output.
    """


    def __init__(self, # pylint: disable=R0913
                 sVmName,                                   # type: str
                 fGrouping = g_kfGrpAncient | g_kfGrpNoTxs, # type: int
                 sHd = None,                                # type: str
                 sKind = None,                              # type: str
                 acCpusSup = None,                          # type: List[int]
                 asVirtModesSup = None,                     # type: List[str]
                 sNic0AttachType = None,                    # type: str
                 sFloppy = None,                            # type: str
                 sFirmwareType = 'bios',                    # type: str
                 sChipsetType = 'piix3',                    # type: str
                 sHddControllerName = 'IDE Controller',     # type: str
                 sDvdControllerName = 'IDE Controller',     # type: str
                 cMBRamMax = None,                          # type: int
                 ):
        TestVm.__init__(self,
                        sVmName,
                        fGrouping = fGrouping,
                        sHd = sHd,
                        sKind = sKind,
                        acCpusSup = [1] if acCpusSup is None else acCpusSup,
                        asVirtModesSup = asVirtModesSup,
                        sNic0AttachType = sNic0AttachType,
                        sFloppy = sFloppy,
                        sFirmwareType = sFirmwareType,
                        sChipsetType = sChipsetType,
                        sHddControllerType = sHddControllerName,
                        sDvdControllerType = sDvdControllerName,
                        asParavirtModesSup = (g_ksParavirtProviderNone,)
                        );
        self.fCom1RawFile = True;
        self.cMBRamMax= cMBRamMax;


    def _childVmReconfig(self, oTestDrv, oVM, oSession):
        _ = oVM; _ = oTestDrv;
        fRc = True;

        # DOS 4.01 doesn't like the default 32MB of memory.
        if fRc and self.cMBRamMax is not None:
            try:
                cMBRam = oSession.o.machine.memorySize;
            except:
                cMBRam = self.cMBRamMax + 4;
            if self.cMBRamMax < cMBRam:
                fRc = oSession.setRamSize(self.cMBRamMax);

        return fRc;


class TestVmSet(object):
    """
    A set of Test VMs.
    """

    def __init__(self, oTestVmManager = None, acCpus = None, asVirtModes = None, fIgnoreSkippedVm = False):
        self.oTestVmManager = oTestVmManager;
        if acCpus is None:
            acCpus = [1, 2];
        self.acCpusDef      = acCpus;
        self.acCpus         = acCpus;
        if asVirtModes is None:
            asVirtModes = list(g_asVirtModes);
        self.asVirtModesDef = asVirtModes;
        self.asVirtModes    = asVirtModes;
        self.aoTestVms      = [];
        self.fIgnoreSkippedVm = fIgnoreSkippedVm;
        self.asParavirtModes = None; ##< If None, use the first PV mode of the test VM, otherwise all modes in this list.

    def findTestVmByName(self, sVmName):
        """
        Returns the TestVm object with the given name.
        Returns None if not found.
        """

        # The 'tst-' prefix is optional.
        sAltName = sVmName if sVmName.startswith('tst-') else 'tst-' + sVmName;

        for oTestVm in self.aoTestVms:
            if oTestVm.sVmName == sVmName or oTestVm.sVmName == sAltName:
                return oTestVm;
        return None;

    def getAllVmNames(self, sSep = ':'):
        """
        Returns names of all the test VMs in the set separated by
                sSep (defaults to ':').
        """
        sVmNames = '';
        for oTestVm in self.aoTestVms:
            sName = oTestVm.sVmName;
            if sName.startswith('tst-'):
                sName = sName[4:];
            if sVmNames == '':
                sVmNames = sName;
            else:
                sVmNames = sVmNames + sSep + sName;
        return sVmNames;

    def showUsage(self):
        """
        Invoked by vbox.TestDriver.
        """
        reporter.log('');
        reporter.log('Test VM selection and general config options:');
        reporter.log('  --virt-modes   <m1[:m2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(self.asVirtModesDef)));
        reporter.log('  --skip-virt-modes <m1[:m2[:...]]>');
        reporter.log('      Use this to avoid hwvirt or hwvirt-np when not supported by the host');
        reporter.log('      since we cannot detect it using the main API. Use after --virt-modes.');
        reporter.log('  --cpu-counts   <c1[:c2[:...]]>');
        reporter.log('      Default: %s' % (':'.join(str(c) for c in self.acCpusDef)));
        reporter.log('  --test-vms     <vm1[:vm2[:...]]>');
        reporter.log('      Test the specified VMs in the given order. Use this to change');
        reporter.log('      the execution order or limit the choice of VMs');
        reporter.log('      Default: %s  (all)' % (self.getAllVmNames(),));
        reporter.log('  --skip-vms     <vm1[:vm2[:...]]>');
        reporter.log('      Skip the specified VMs when testing.');
        reporter.log('  --snapshot-restore-current');
        reporter.log('      Restores the current snapshot and resumes execution.');
        reporter.log('  --paravirt-modes   <pv1[:pv2[:...]]>');
        reporter.log('      Set of paravirtualized providers (modes) to tests. Intersected with what the test VM supports.');
        reporter.log('      Default is the first PV mode the test VMs support, generally same as "legacy".');
        reporter.log('  --with-nested-hwvirt-only');
        reporter.log('      Test VMs using nested hardware-virtualization only.');
        reporter.log('  --without-nested-hwvirt-only');
        reporter.log('      Test VMs not using nested hardware-virtualization only.');
        ## @todo Add more options for controlling individual VMs.
        return True;

    def parseOption(self, asArgs, iArg):
        """
        Parses the set test vm set options (--test-vms and --skip-vms), modifying the set
        Invoked by the testdriver method with the same name.

        Keyword arguments:
        asArgs -- The argument vector.
        iArg   -- The index of the current argument.

        Returns iArg if the option was not recognized and the caller should handle it.
        Returns the index of the next argument when something is consumed.

        In the event of a syntax error, a InvalidOption or QuietInvalidOption
        is thrown.
        """

        if asArgs[iArg] == '--virt-modes':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--virt-modes" takes a colon separated list of modes');

            self.asVirtModes = asArgs[iArg].split(':');
            for s in self.asVirtModes:
                if s not in self.asVirtModesDef:
                    raise base.InvalidOption('The "--virt-modes" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asVirtModesDef)));

        elif asArgs[iArg] == '--skip-virt-modes':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--skip-virt-modes" takes a colon separated list of modes');

            for s in asArgs[iArg].split(':'):
                if s not in self.asVirtModesDef:
                    raise base.InvalidOption('The "--virt-modes" value "%s" is not valid; valid values are: %s' \
                        % (s, ' '.join(self.asVirtModesDef)));
                if s in self.asVirtModes:
                    self.asVirtModes.remove(s);

        elif asArgs[iArg] == '--cpu-counts':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--cpu-counts" takes a colon separated list of cpu counts');

            self.acCpus = [];
            for s in asArgs[iArg].split(':'):
                try: c = int(s);
                except: raise base.InvalidOption('The "--cpu-counts" value "%s" is not an integer' % (s,));
                if c <= 0:  raise base.InvalidOption('The "--cpu-counts" value "%s" is zero or negative' % (s,));
                self.acCpus.append(c);

        elif asArgs[iArg] == '--test-vms':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--test-vms" takes colon separated list');

            for oTestVm in self.aoTestVms:
                oTestVm.fSkip = True;

            asTestVMs = asArgs[iArg].split(':');
            for s in asTestVMs:
                oTestVm = self.findTestVmByName(s);
                if oTestVm is None:
                    raise base.InvalidOption('The "--test-vms" value "%s" is not valid; valid values are: %s' \
                                             % (s, self.getAllVmNames(' ')));
                oTestVm.fSkip = False;

        elif asArgs[iArg] == '--skip-vms':
            iArg += 1;
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--skip-vms" takes colon separated list');

            asTestVMs = asArgs[iArg].split(':');
            for s in asTestVMs:
                oTestVm = self.findTestVmByName(s);
                if oTestVm is None:
                    reporter.log('warning: The "--test-vms" value "%s" does not specify any of our test VMs.' % (s,));
                else:
                    oTestVm.fSkip = True;

        elif asArgs[iArg] == '--snapshot-restore-current':
            for oTestVm in self.aoTestVms:
                if oTestVm.fSkip is False:
                    oTestVm.fSnapshotRestoreCurrent = True;
                    reporter.log('VM "%s" will be restored.' % (oTestVm.sVmName));

        elif asArgs[iArg] == '--paravirt-modes':
            iArg += 1
            if iArg >= len(asArgs):
                raise base.InvalidOption('The "--paravirt-modes" takes a colon separated list of modes');

            self.asParavirtModes = asArgs[iArg].split(':')
            for sPvMode in self.asParavirtModes:
                if sPvMode not in g_kasParavirtProviders:
                    raise base.InvalidOption('The "--paravirt-modes" value "%s" is not valid; valid values are: %s'
                                             % (sPvMode, ', '.join(g_kasParavirtProviders),));
            if not self.asParavirtModes:
                self.asParavirtModes = None;

            # HACK ALERT! Reset the random paravirt selection for members.
            for oTestVm in self.aoTestVms:
                oTestVm.asParavirtModesSup = oTestVm.asParavirtModesSupOrg;

        elif asArgs[iArg] == '--with-nested-hwvirt-only':
            for oTestVm in self.aoTestVms:
                if oTestVm.fNstHwVirt is False:
                    oTestVm.fSkip = True;

        elif asArgs[iArg] == '--without-nested-hwvirt-only':
            for oTestVm in self.aoTestVms:
                if oTestVm.fNstHwVirt is True:
                    oTestVm.fSkip = True;

        else:
            return iArg;
        return iArg + 1;

    def getResourceSet(self):
        """
        Implements base.TestDriver.getResourceSet
        """
        asResources = [];
        for oTestVm in self.aoTestVms:
            if not oTestVm.fSkip:
                if oTestVm.sHd is not None:
                    asResources.append(oTestVm.sHd);
                if oTestVm.sDvdImage is not None:
                    asResources.append(oTestVm.sDvdImage);
        return asResources;

    def actionConfig(self, oTestDrv, eNic0AttachType = None, sDvdImage = None):
        """
        For base.TestDriver.actionConfig. Configure the VMs with defaults and
        a few tweaks as per arguments.

        Returns True if successful.
        Returns False if not.
        """

        for oTestVm in self.aoTestVms:
            if oTestVm.fSkip:
                continue;

            if oTestVm.fSnapshotRestoreCurrent:
                # If we want to restore a VM we don't need to create
                # the machine anymore -- so just add it to the test VM list.
                oVM = oTestDrv.addTestMachine(oTestVm.sVmName);
            else:
                oVM = oTestVm.createVm(oTestDrv, eNic0AttachType, sDvdImage);
            if oVM is None:
                return False;

        return True;

    def _removeUnsupportedVirtModes(self, oTestDrv):
        """
        Removes unsupported virtualization modes.
        """
        if 'hwvirt' in self.asVirtModes and not oTestDrv.hasHostHwVirt():
            reporter.log('Hardware assisted virtualization is not available on the host, skipping it.');
            self.asVirtModes.remove('hwvirt');

        if 'hwvirt-np' in self.asVirtModes and not oTestDrv.hasHostNestedPaging():
            reporter.log('Nested paging not supported by the host, skipping it.');
            self.asVirtModes.remove('hwvirt-np');

        if 'raw' in self.asVirtModes and not oTestDrv.hasRawModeSupport():
            reporter.log('Raw-mode virtualization is not available in this build (or perhaps for this host), skipping it.');
            self.asVirtModes.remove('raw');

        return True;

    def actionExecute(self, oTestDrv, fnCallback): # pylint: disable=R0914
        """
        For base.TestDriver.actionExecute.  Calls the callback function for
        each of the VMs and basic configuration variations (virt-mode and cpu
        count).

        Returns True if all fnCallback calls returned True, otherwise False.

        The callback can return True, False or None. The latter is for when the
        test is skipped.  (True is for success, False is for failure.)
        """

        self._removeUnsupportedVirtModes(oTestDrv);
        cMaxCpus = oTestDrv.getHostCpuCount();

        #
        # The test loop.
        #
        fRc = True;
        for oTestVm in self.aoTestVms:
            if oTestVm.fNstHwVirt and not oTestDrv.isHostCpuAmd():
                reporter.log('Ignoring VM %s (Nested hardware-virtualization only supported on AMD CPUs).' % (oTestVm.sVmName,));
                continue;
            if oTestVm.fSkip and self.fIgnoreSkippedVm:
                reporter.log2('Ignoring VM %s (fSkip = True).' % (oTestVm.sVmName,));
                continue;
            reporter.testStart(oTestVm.sVmName);
            if oTestVm.fSkip:
                reporter.testDone(fSkipped = True);
                continue;

            # Intersect the supported modes and the ones being testing.
            asVirtModesSup = [sMode for sMode in oTestVm.asVirtModesSup if sMode in self.asVirtModes];

            # Ditto for CPUs.
            acCpusSup      = [cCpus for cCpus in oTestVm.acCpusSup      if cCpus in self.acCpus];

            # Ditto for paravirtualization modes, except if not specified we got a less obvious default.
            if self.asParavirtModes is not None  and  oTestDrv.fpApiVer >= 5.0:
                asParavirtModes = [sPvMode for sPvMode in oTestVm.asParavirtModesSup if sPvMode in self.asParavirtModes];
                assert None not in asParavirtModes;
            elif oTestDrv.fpApiVer >= 5.0:
                asParavirtModes = (oTestVm.asParavirtModesSup[0],);
                assert asParavirtModes[0] is not None;
            else:
                asParavirtModes = (None,);

            for cCpus in acCpusSup:
                if cCpus == 1:
                    reporter.testStart('1 cpu');
                else:
                    reporter.testStart('%u cpus' % (cCpus));
                    if cCpus > cMaxCpus:
                        reporter.testDone(fSkipped = True);
                        continue;

                cTests = 0;
                for sVirtMode in asVirtModesSup:
                    if sVirtMode == 'raw' and cCpus > 1:
                        continue;
                    reporter.testStart('%s' % ( g_dsVirtModeDescs[sVirtMode], ) );
                    cStartTests = cTests;

                    for sParavirtMode in asParavirtModes:
                        if sParavirtMode is not None:
                            assert oTestDrv.fpApiVer >= 5.0;
                            reporter.testStart('%s' % ( sParavirtMode, ) );

                        # Reconfigure the VM.
                        try:
                            (rc2, oVM) = oTestVm.getReconfiguredVm(oTestDrv, cCpus, sVirtMode, sParavirtMode = sParavirtMode);
                        except KeyboardInterrupt:
                            raise;
                        except:
                            reporter.errorXcpt(cFrames = 9);
                            rc2 = False;
                        if rc2 is True:
                            # Do the testing.
                            try:
                                rc2 = fnCallback(oVM, oTestVm);
                            except KeyboardInterrupt:
                                raise;
                            except:
                                reporter.errorXcpt(cFrames = 9);
                                rc2 = False;
                            if rc2 is False:
                                reporter.maybeErr(reporter.testErrorCount() == 0, 'fnCallback failed');
                        elif rc2 is False:
                            reporter.log('getReconfiguredVm failed');
                        if rc2 is False:
                            fRc = False;

                        cTests = cTests + (rc2 is not None);
                        if sParavirtMode is not None:
                            reporter.testDone(fSkipped = (rc2 is None));

                    reporter.testDone(fSkipped = cTests == cStartTests);

                reporter.testDone(fSkipped = cTests == 0);

            _, cErrors = reporter.testDone();
            if cErrors > 0:
                fRc = False;
        return fRc;

    def enumerateTestVms(self, fnCallback):
        """
        Enumerates all the 'active' VMs.

        Returns True if all fnCallback calls returned True.
        Returns False if any returned False.
        Returns None immediately if fnCallback returned None.
        """
        fRc = True;
        for oTestVm in self.aoTestVms:
            if not oTestVm.fSkip:
                fRc2 = fnCallback(oTestVm);
                if fRc2 is None:
                    return fRc2;
                fRc = fRc and fRc2;
        return fRc;



class TestVmManager(object):
    """
    Test VM manager.
    """

    ## @name VM grouping flags
    ## @{
    kfGrpSmoke     = g_kfGrpSmoke;
    kfGrpStandard  = g_kfGrpStandard;
    kfGrpStdSmoke  = g_kfGrpStdSmoke;
    kfGrpWithGAs   = g_kfGrpWithGAs;
    kfGrpNoTxs     = g_kfGrpNoTxs;
    kfGrpAncient   = g_kfGrpAncient;
    kfGrpExotic    = g_kfGrpExotic;
    ## @}

    kaTestVMs = (
        # Linux
        TestVm('tst-ubuntu-15_10-64-efi',   kfGrpStdSmoke,        sHd = '4.2/efi/ubuntu-15_10-efi-amd64.vdi',
               sKind = 'Ubuntu_64', acCpusSup = range(1, 33), fIoApic = True, sFirmwareType = 'efi',
               asParavirtModesSup = [g_ksParavirtProviderKVM,]),
        TestVm('tst-rhel5',                 kfGrpSmoke,           sHd = '3.0/tcp/rhel5.vdi',
               sKind = 'RedHat', acCpusSup = range(1, 33), fIoApic = True, sNic0AttachType = 'nat'),
        TestVm('tst-arch',                  kfGrpStandard,        sHd = '4.2/usb/tst-arch.vdi',
               sKind = 'ArchLinux_64', acCpusSup = range(1, 33), fIoApic = True, sNic0AttachType = 'nat'),

        # Solaris
        TestVm('tst-sol10',                 kfGrpSmoke,           sHd = '3.0/tcp/solaris10.vdi',
               sKind = 'Solaris', acCpusSup = range(1, 33), fPae = True,  sNic0AttachType = 'bridged'),
        TestVm('tst-sol10-64',              kfGrpSmoke,           sHd = '3.0/tcp/solaris10.vdi',
               sKind = 'Solaris_64', acCpusSup = range(1, 33), sNic0AttachType = 'bridged'),
        TestVm('tst-sol11u1',               kfGrpSmoke,           sHd = '4.2/nat/sol11u1/t-sol11u1.vdi',
               sKind = 'Solaris11_64', acCpusSup = range(1, 33), sNic0AttachType = 'nat', fIoApic = True,
               sHddControllerType = 'SATA Controller'),
        #TestVm('tst-sol11u1-ich9',          kfGrpSmoke,           sHd = '4.2/nat/sol11u1/t-sol11u1.vdi',
        #       sKind = 'Solaris11_64', acCpusSup = range(1, 33), sNic0AttachType = 'nat', fIoApic = True,
        #       sHddControllerType = 'SATA Controller', sChipsetType = 'ich9'),

        # NT 3.x
        TestVm('tst-nt310',                 kfGrpAncient,               sHd = '5.2/great-old-ones/t-nt310/t-nt310.vdi',
               sKind = 'WindowsNT3x', acCpusSup = [1], sHddControllerType = 'BusLogic SCSI Controller',
               sDvdControllerType = 'BusLogic SCSI Controller'),
        TestVm('tst-nt350',                 kfGrpAncient,               sHd = '5.2/great-old-ones/t-nt350/t-nt350.vdi',
               sKind = 'WindowsNT3x', acCpusSup = [1], sHddControllerType = 'BusLogic SCSI Controller',
               sDvdControllerType = 'BusLogic SCSI Controller'),
        TestVm('tst-nt351',                 kfGrpAncient,               sHd = '5.2/great-old-ones/t-nt350/t-nt351.vdi',
               sKind = 'WindowsNT3x', acCpusSup = [1], sHddControllerType = 'BusLogic SCSI Controller',
               sDvdControllerType = 'BusLogic SCSI Controller'),

        # NT 4
        TestVm('tst-nt4sp1',                kfGrpStdSmoke,        sHd = '4.2/nat/nt4sp1/t-nt4sp1.vdi',
               sKind = 'WindowsNT4', acCpusSup = [1], sNic0AttachType = 'nat'),

        TestVm('tst-nt4sp6',                kfGrpStdSmoke,        sHd = '4.2/nt4sp6/t-nt4sp6.vdi',
               sKind = 'WindowsNT4', acCpusSup = range(1, 33)),

        # W2K
        TestVm('tst-2ksp4',                 kfGrpStdSmoke,        sHd = '4.2/win2ksp4/t-win2ksp4.vdi',
               sKind = 'Windows2000', acCpusSup = range(1, 33)),

        # XP
        TestVm('tst-xppro',                 kfGrpStdSmoke,        sHd = '4.2/nat/xppro/t-xppro.vdi',
               sKind = 'WindowsXP', acCpusSup = range(1, 33), sNic0AttachType = 'nat'),
        TestVm('tst-xpsp2',                 kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxpsp2.vdi',
               sKind = 'WindowsXP', acCpusSup = range(1, 33), fIoApic = True),
        TestVm('tst-xpsp2-halaacpi',        kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxp-halaacpi.vdi',
               sKind = 'WindowsXP', acCpusSup = range(1, 33), fIoApic = True),
        TestVm('tst-xpsp2-halacpi',         kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxp-halacpi.vdi',
               sKind = 'WindowsXP', acCpusSup = range(1, 33), fIoApic = True),
        TestVm('tst-xpsp2-halapic',         kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxp-halapic.vdi',
               sKind = 'WindowsXP', acCpusSup = range(1, 33), fIoApic = True),
        TestVm('tst-xpsp2-halmacpi',        kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxp-halmacpi.vdi',
               sKind = 'WindowsXP', acCpusSup = range(2, 33), fIoApic = True),
        TestVm('tst-xpsp2-halmps',          kfGrpStdSmoke,        sHd = '4.2/xpsp2/t-winxp-halmps.vdi',
               sKind = 'WindowsXP', acCpusSup = range(2, 33), fIoApic = True),

        # W2K3
        TestVm('tst-win2k3ent',             kfGrpSmoke,           sHd = '3.0/tcp/win2k3ent-acpi.vdi',
               sKind = 'Windows2003', acCpusSup = range(1, 33), fPae = True, sNic0AttachType = 'bridged'),

        # W7
        TestVm('tst-win7',                  kfGrpStdSmoke,        sHd = '4.2/win7-32/t-win7.vdi',
               sKind = 'Windows7', acCpusSup = range(1, 33), fIoApic = True),

        # W8
        TestVm('tst-win8-64',               kfGrpStdSmoke,        sHd = '4.2/win8-64/t-win8-64.vdi',
               sKind = 'Windows8_64', acCpusSup = range(1, 33), fIoApic = True),
        #TestVm('tst-win8-64-ich9',          kfGrpStdSmoke,         sHd = '4.2/win8-64/t-win8-64.vdi',
        #       sKind = 'Windows8_64', acCpusSup = range(1, 33), fIoApic = True, sChipsetType = 'ich9'),

        # W10
        TestVm('tst-win10-efi',             kfGrpStdSmoke,        sHd = '4.2/efi/win10-efi-x86.vdi',
               sKind = 'Windows10', acCpusSup = range(1, 33), fIoApic = True, sFirmwareType = 'efi'),
        TestVm('tst-win10-64-efi',          kfGrpStdSmoke,        sHd = '4.2/efi/win10-efi-amd64.vdi',
               sKind = 'Windows10_64', acCpusSup = range(1, 33), fIoApic = True, sFirmwareType = 'efi'),
        #TestVm('tst-win10-64-efi-ich9',     kfGrpStdSmoke,         sHd = '4.2/efi/win10-efi-amd64.vdi',
        #       sKind = 'Windows10_64', acCpusSup = range(1, 33), fIoApic = True, sFirmwareType = 'efi', sChipsetType = 'ich9'),

        # Nested hardware-virtualization
        TestVm('tst-nsthwvirt-ubuntu-64',    kfGrpStdSmoke,       sHd = '5.3/nat/nsthwvirt-ubuntu64/t-nsthwvirt-ubuntu64.vdi',
               sKind = 'Ubuntu_64', acCpusSup = range(1, 2), asVirtModesSup = ['hwvirt-np',], fIoApic = True, fNstHwVirt = True,
               sNic0AttachType = 'nat'),

        # DOS and Old Windows.
        AncientTestVm('tst-dos20',              sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos20/t-dos20.vdi'),
        AncientTestVm('tst-dos401-win30me',     sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos401-win30me/t-dos401-win30me.vdi',                 cMBRamMax = 4),
        AncientTestVm('tst-dos401-emm386-win30me', sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos401-emm386-win30me/t-dos401-emm386-win30me.vdi',   cMBRamMax = 4),
        AncientTestVm('tst-dos50-win31',        sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos50-win31/t-dos50-win31.vdi'),
        AncientTestVm('tst-dos50-emm386-win31', sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos50-emm386-win31/t-dos50-emm386-win31.vdi'),
        AncientTestVm('tst-dos622',             sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos622/t-dos622.vdi'),
        AncientTestVm('tst-dos622-emm386',      sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos622-emm386/t-dos622-emm386.vdi'),
        AncientTestVm('tst-dos71',              sKind = 'DOS',
                      sHd = '5.2/great-old-ones/t-dos71/t-dos71.vdi'),

        #AncientTestVm('tst-dos5-win311a',       sKind = 'DOS',  sHd = '5.2/great-old-ones/t-dos5-win311a/t-dos5-win311a.vdi'),
    );


    def __init__(self, sResourcePath):
        self.sResourcePath = sResourcePath;

    def selectSet(self, fGrouping, sTxsTransport = None, fCheckResources = True):
        """
        Returns a VM set with the selected VMs.
        """
        oSet = TestVmSet(oTestVmManager = self);
        for oVm in self.kaTestVMs:
            if oVm.fGrouping & fGrouping:
                if sTxsTransport is None  or  oVm.sNic0AttachType is None  or  sTxsTransport == oVm.sNic0AttachType:
                    if not fCheckResources  or  not oVm.getMissingResources(self.sResourcePath):
                        oCopyVm = copy.deepcopy(oVm);
                        oCopyVm.oSet = oSet;
                        oSet.aoTestVms.append(oCopyVm);
        return oSet;

    def getStandardVmSet(self, sTxsTransport):
        """
        Gets the set of standard test VMs.

        This is supposed to do something seriously clever, like searching the
        testrsrc tree for usable VMs, but for the moment it's all hard coded. :-)
        """
        return self.selectSet(self.kfGrpStandard, sTxsTransport)

    def getSmokeVmSet(self):
        """Gets a representative set of VMs for smoke testing. """
        return self.selectSet(self.kfGrpSmoke);

    def shutUpPyLint(self):
        """ Shut up already! """
        return self.sResourcePath;

