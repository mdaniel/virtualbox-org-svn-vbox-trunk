#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0301

"""
Admin job for checking detecting deleted builds.

This is necessary when the tinderbox <-> test manager interface was
busted and the build info in is out of sync.  The result is generally
a lot of skipped tests because of missing builds, typically during
bisecting problems.
"""

from __future__ import print_function;

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

# Standard python imports
import sys;
import os;
from optparse import OptionParser;  # pylint: disable=deprecated-module

# Add Test Manager's modules path
g_ksTestManagerDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksTestManagerDir);

# Test Manager imports
from testmanager.core.db    import TMDatabaseConnection;
from testmanager.core.build import BuildLogic;



class BuildChecker(object): # pylint: disable=R0903
    """
    Add build info into Test Manager database.
    """

    def __init__(self):
        """
        Parse command line.
        """

        oParser = OptionParser();
        oParser.add_option('-q', '--quiet', dest = 'fQuiet', action = 'store_true',  default = False,
                           help = 'Quiet execution');
        oParser.add_option('--dry-run', dest = 'fRealRun',   action = 'store_false', default = False,
                           help = 'Dry run');
        oParser.add_option('--real-run', dest = 'fRealRun',  action = 'store_true',  default = False,
                           help = 'Real run');

        (self.oConfig, _) = oParser.parse_args();
        if not self.oConfig.fQuiet:
            if not self.oConfig.fRealRun:
                print('Dry run.');
            else:
                print('Real run! Will commit findings!');


    def checkBuilds(self):
        """
        Add build data record into database.
        """
        oDb = TMDatabaseConnection();
        oBuildLogic = BuildLogic(oDb);

        tsNow    = oDb.getCurrentTimestamp();
        cMaxRows = 1024;
        iStart   = 0;
        while True:
            aoBuilds = oBuildLogic.fetchForListing(iStart, cMaxRows, tsNow);
            if not self.oConfig.fQuiet and aoBuilds:
                print('Processing builds #%s thru #%s' % (aoBuilds[0].idBuild, aoBuilds[-1].idBuild));

            for oBuild in aoBuilds:
                if oBuild.fBinariesDeleted is False:
                    rc = oBuild.areFilesStillThere();
                    if rc is False:
                        if not self.oConfig.fQuiet:
                            print('missing files for build #%s / r%s / %s / %s / %s / %s / %s'
                                  % (oBuild.idBuild, oBuild.iRevision, oBuild.sVersion, oBuild.oCat.sType,
                                     oBuild.oCat.sBranch, oBuild.oCat.sProduct, oBuild.oCat.asOsArches,));
                            print('  %s' % (oBuild.sBinaries,));
                        if self.oConfig.fRealRun is True:
                            oBuild.fBinariesDeleted = True;
                            oBuildLogic.editEntry(oBuild, fCommit = True);
                    elif rc is True and not self.oConfig.fQuiet:
                        print('build #%s still have its files' % (oBuild.idBuild,));
                    elif rc is None and not self.oConfig.fQuiet:
                        print('Unable to determine state of build #%s' % (oBuild.idBuild,));

            # advance
            if len(aoBuilds) < cMaxRows:
                break;
            iStart += len(aoBuilds);

        oDb.close();
        return 0;

if __name__ == '__main__':
    sys.exit(BuildChecker().checkBuilds());

