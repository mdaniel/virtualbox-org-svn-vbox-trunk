#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Medium Move Test #1
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
import os
import sys

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0]
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(g_ksValidationKitDir)

# Validation Kit imports.
from testdriver import base
from testdriver import reporter
from testdriver import vboxcon
from testdriver import vboxwrappers


class SubTstDrvMoveMedium1(base.SubTestDriverBase):
    """
    Sub-test driver for Medium Move Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, 'move-medium', oTstDrv)

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        return  self.testMediumMove()

    #
    # Test execution helpers.
    #

    def setLocation(self, sLocation, aoMediumAttachments):
        for oAttachment in aoMediumAttachments:
            try:
                oMedium = oAttachment.medium
                reporter.log('Move medium "%s" to "%s"' % (oMedium.name, sLocation,))
            except:
                reporter.errorXcpt('failed to get the medium from the IMediumAttachment "%s"' % (oAttachment))

            try:
                oProgress = vboxwrappers.ProgressWrapper(oMedium.setLocation(sLocation), self.oTstDrv.oVBoxMgr, self.oTstDrv,
                                                         'move "%s"' % (oMedium.name,))
            except:
                return reporter.errorXcpt('Medium::setLocation("%s") for medium "%s" failed' % (sLocation, oMedium.name,))

            oProgress.wait()
            if oProgress.logResult() is False:
                return False
        return True

    def checkLocation(self, sLocation, aoMediumAttachments, asFiles):
        fRc = True
        for oAttachment in aoMediumAttachments:
            sFilePath = os.path.join(sLocation, asFiles[oAttachment.port])
            sActualFilePath = oAttachment.medium.location
            if os.path.abspath(sFilePath) != os.path.abspath(sActualFilePath):
                reporter.log('medium location expected to be "%s" but is "%s"' % (sFilePath, sActualFilePath))
                fRc = False;
            if not os.path.exists(sFilePath):
                reporter.log('medium file does not exist at "%s"' % (sFilePath,))
                fRc = False;
        return fRc

    def testMediumMove(self):
        """
        Test medium moving.
        """
        reporter.testStart('medium moving')

        try:
            oVM = self.oTstDrv.createTestVM('test-medium-move', 1, None, 4)
            assert oVM is not None

            # create hard disk images, one for each file-based backend, using the first applicable extension
            fRc = True
            oSession = self.oTstDrv.openSession(oVM)
            aoDskFmts = self.oTstDrv.oVBoxMgr.getArray(self.oTstDrv.oVBox.systemProperties, 'mediumFormats')
            asFiles = []
            for oDskFmt in aoDskFmts:
                aoDskFmtCaps = self.oTstDrv.oVBoxMgr.getArray(oDskFmt, 'capabilities')
                if vboxcon.MediumFormatCapabilities_File not in aoDskFmtCaps \
                    or vboxcon.MediumFormatCapabilities_CreateDynamic not in aoDskFmtCaps:
                    continue
                (asExts, aTypes) = oDskFmt.describeFileExtensions()
                for i in range(0, len(asExts)):
                    if aTypes[i] is vboxcon.DeviceType_HardDisk:
                        sExt = '.' + asExts[i]
                        break
                if sExt is None:
                    fRc = False
                    break
                sFile = 'Test' + str(len(asFiles)) + sExt
                sHddPath = os.path.join(self.oTstDrv.sScratchPath, sFile)
                oHd = oSession.createBaseHd(sHddPath, sFmt=oDskFmt.id, cb=1024*1024)
                if oHd is None:
                    fRc = False
                    break

                # attach HDD, IDE controller exists by default, but we use SATA just in case
                sController='SATA Controller'
                fRc = fRc and oSession.attachHd(sHddPath, sController, iPort = len(asFiles),
                                                fImmutable=False, fForceResource=False)
                if fRc:
                    asFiles.append(sFile)

            fRc = fRc and oSession.saveSettings()

            #create temporary subdirectory in the current working directory
            sOrigLoc = self.oTstDrv.sScratchPath
            sNewLoc = os.path.join(sOrigLoc, 'newLocation')
            os.mkdir(sNewLoc, 0o775)

            aoMediumAttachments = oVM.getMediumAttachmentsOfController(sController)
            #case 1. Only path without file name, with trailing separator
            fRc = self.setLocation(sNewLoc + os.sep, aoMediumAttachments) and fRc
            fRc = self.checkLocation(sNewLoc, aoMediumAttachments, asFiles) and fRc

            #case 2. Only path without file name, without trailing separator
            fRc = self.setLocation(sOrigLoc, aoMediumAttachments) and fRc
            fRc = self.checkLocation(sOrigLoc, aoMediumAttachments, asFiles) and fRc

            #case 3. Path with file name
            #The case supposes that user has passed a destination path with a file name but hasn't added an extension/suffix
            #to this destination file. User supposes that the extension would be added automatically and to be the same as
            #for the original file. Difficult case, apparently this case should follow mv(1) logic
            #and the file name is processed as folder name (aka mv(1) logic).
            #Be discussed.
            fRc = self.setLocation(os.path.join(sNewLoc, 'newName'), aoMediumAttachments) and fRc
            asNewFiles = ['newName' + os.path.splitext(s)[1] for s in asFiles]
            fRc = self.checkLocation(os.path.join(sNewLoc, 'newName'), aoMediumAttachments, asFiles) and fRc

            #after the case the destination path must be corrected
            sNewLoc = os.path.join(sNewLoc, 'newName')

            #case 4. Only file name
            fRc = self.setLocation('onlyMediumName', aoMediumAttachments) and fRc
            asNewFiles = ['onlyMediumName' + os.path.splitext(s)[1] for s in asFiles]
            fRc = self.checkLocation(sNewLoc, aoMediumAttachments, asNewFiles) and fRc

            #case 5. Move all files from a snapshot
            fRc = fRc and oSession.takeSnapshot('Snapshot1')
            if fRc:
                aoMediumAttachments = oVM.getMediumAttachmentsOfController(sController)
                asSnapFiles = [os.path.basename(o.medium.name) for o in aoMediumAttachments]
                fRc = self.setLocation(sOrigLoc, aoMediumAttachments) and fRc
                fRc = self.checkLocation(sOrigLoc, aoMediumAttachments, asSnapFiles) and fRc

            fRc = oSession.close() and fRc

            assert fRc is True
        except:
            reporter.errorXcpt()

        return reporter.testDone()[1] == 0


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1
    sys.exit(tdApi1([SubTstDrvMoveMedium1]).main(sys.argv))

