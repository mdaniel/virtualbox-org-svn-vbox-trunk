# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - Database object cache.
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


# Validation Kit imports.
from testmanager.core.base              import ModelLogicBase;


class DatabaseObjCache(ModelLogicBase):
    """
    Database object cache.

    This is mainly for reports and test results where we wish to get further
    information on a data series or similar.  The cache should reduce database
    lookups as well as pyhon memory footprint.

    Note! Dependecies are imported when needed to avoid potential cylic dependency issues.
    """

    ## @name Cache object types.
    ## @{
    ksObjType_TestResultStrTab_idStrName        = 0;
    ksObjType_BuildCategory_idBuildCategory     = 1;
    ksObjType_TestBox_idTestBox                 = 2;
    ksObjType_TestBox_idGenTestBox              = 3;
    ksObjType_TestCase_idTestCase               = 4;
    ksObjType_TestCase_idGenTestCase            = 5;
    ksObjType_TestCaseArgs_idTestCaseArgs       = 6;
    ksObjType_TestCaseArgs_idGenTestCaseArgs    = 7;
    ksObjType_VcsRevision_sRepository_iRevision = 8;
    ksObjType_End                               = 9;
    ## @}

    def __init__(self, oDb, tsNow = None, sPeriodBack = None, cHoursBack = None):
        ModelLogicBase.__init__(self, oDb);

        self.tsNow       = tsNow;
        self.sPeriodBack = sPeriodBack;
        if sPeriodBack is None and cHoursBack is not None:
            self.sPeriodBack = '%u hours' % cHoursBack;

        self._adCache    = (
            dict(), dict(), dict(), dict(),
            dict(), dict(), dict(), dict(),
            dict(),
        );
        assert(len(self._adCache) == self.ksObjType_End);

    def _handleDbException(self):
        """ Deals with database exceptions. """
        #self._oDb.rollback();
        return False;

    def getTestResultString(self, idStrName):
        """ Gets a string from the TestResultStrTab. """
        sRet = self._adCache[self.ksObjType_TestResultStrTab_idStrName].get(idStrName);
        if sRet is None:
            # Load cache entry.
            self._oDb.execute('SELECT sValue FROM TestResultStrTab WHERE idStr = %s', (idStrName,));
            sRet = self._oDb.fetchOne()[0];
            self._adCache[self.ksObjType_TestResultStrTab_idStrName][idStrName] = sRet
        return sRet;

    def getBuildCategory(self, idBuildCategory):
        """ Gets the corresponding BuildCategoryData object. """
        oRet = self._adCache[self.ksObjType_BuildCategory_idBuildCategory].get(idBuildCategory);
        if oRet is None:
            # Load cache entry.
            from testmanager.core.build import BuildCategoryData;
            oRet = BuildCategoryData();
            try:    oRet.initFromDbWithId(self._oDb, idBuildCategory);
            except: self._handleDbException(); raise;
            self._adCache[self.ksObjType_BuildCategory_idBuildCategory][idBuildCategory] = oRet;
        return oRet;

    def getTestBox(self, idTestBox):
        """ Gets the corresponding TestBoxData object. """
        oRet = self._adCache[self.ksObjType_TestBox_idTestBox].get(idTestBox);
        if oRet is None:
            # Load cache entry.
            from testmanager.core.testbox import TestBoxData;
            oRet = TestBoxData();
            try:    oRet.initFromDbWithId(self._oDb, idTestBox, self.tsNow, self.sPeriodBack);
            except: self._handleDbException(); raise;
            else:   self._adCache[self.ksObjType_TestBox_idGenTestBox][oRet.idGenTestBox] = oRet;
            self._adCache[self.ksObjType_TestBox_idTestBox][idTestBox] = oRet;
        return oRet;

    def getTestCase(self, idTestCase):
        """ Gets the corresponding TestCaseData object. """
        oRet = self._adCache[self.ksObjType_TestCase_idTestCase].get(idTestCase);
        if oRet is None:
            # Load cache entry.
            from testmanager.core.testcase import TestCaseData;
            oRet = TestCaseData();
            try:    oRet.initFromDbWithId(self._oDb, idTestCase, self.tsNow, self.sPeriodBack);
            except: self._handleDbException(); raise;
            else:   self._adCache[self.ksObjType_TestCase_idGenTestCase][oRet.idGenTestCase] = oRet;
            self._adCache[self.ksObjType_TestCase_idTestCase][idTestCase] = oRet;
        return oRet;

    def getTestCaseArgs(self, idTestCaseArgs):
        """ Gets the corresponding TestCaseArgsData object. """
        oRet = self._adCache[self.ksObjType_TestCaseArgs_idTestCaseArgs].get(idTestCaseArgs);
        if oRet is None:
            # Load cache entry.
            from testmanager.core.testcaseargs import TestCaseArgsData;
            oRet = TestCaseArgsData();
            try:    oRet.initFromDbWithId(self._oDb, idTestCaseArgs, self.tsNow, self.sPeriodBack);
            except: self._handleDbException(); raise;
            else:   self._adCache[self.ksObjType_TestCaseArgs_idGenTestCaseArgs][oRet.idGenTestCaseArgs] = oRet;
            self._adCache[self.ksObjType_TestCaseArgs_idTestCaseArgs][idTestCaseArgs] = oRet;
        return oRet;

    def preloadVcsRevInfo(self, sRepository, aiRevisions):
        """
        Preloads VCS revision information.
        ASSUMES aiRevisions does not contain duplicate keys.
        """
        from testmanager.core.vcsrevisions import VcsRevisionData;
        dRepo = self._adCache[self.ksObjType_VcsRevision_sRepository_iRevision].get(sRepository);
        if dRepo is None:
            dRepo = dict();
            self._adCache[self.ksObjType_VcsRevision_sRepository_iRevision][sRepository] = dRepo;
            aiFiltered = aiRevisions;
        else:
            aiFiltered = [];
            for iRevision in aiRevisions:
                if iRevision not in dRepo:
                    aiFiltered.append(iRevision);
        if aiFiltered:
            self._oDb.execute('SELECT *\n'
                              'FROM   VcsRevisions\n'
                              'WHERE  sRepository = %s\n'
                              '   AND iRevision IN (' + ','.join([str(i) for i in aiFiltered]) + ')'
                              , ( sRepository, ));
            for aoRow in self._oDb.fetchAll():
                oInfo = VcsRevisionData().initFromDbRow(aoRow);
                dRepo[oInfo.iRevision] = oInfo;
        return True;

    def getVcsRevInfo(self, sRepository, iRevision):
        """
        Gets the corresponding VcsRevisionData object.
        May return a default (all NULLs) VcsRevisionData object if the revision
        information isn't available in the database yet.
        """
        dRepo = self._adCache[self.ksObjType_VcsRevision_sRepository_iRevision].get(sRepository);
        if dRepo is not None:
            oRet = dRepo.get(iRevision);
        else:
            dRepo = dict();
            self._adCache[self.ksObjType_VcsRevision_sRepository_iRevision][sRepository] = dRepo;
            oRet = None;
        if oRet is None:
            from testmanager.core.vcsrevisions import VcsRevisionLogic;
            oRet = VcsRevisionLogic(self._oDb).tryFetch(sRepository, iRevision);
            if oRet is None:
                from testmanager.core.vcsrevisions import VcsRevisionData;
                oRet = VcsRevisionData();
            dRepo[iRevision] = oRet;
        return oRet;

