# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager - Best-Effort-Continuous-Integration (BECI) scheduler.
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
from testmanager.core.schedulerbase  import SchedulerBase, SchedQueueData;


class SchdulerBeci(SchedulerBase): # pylint: disable=R0903
    """
    The best-effort-continuous-integration scheduler, BECI for short.
    """

    def __init__(self, oDb, oSchedGrpData, iVerbosity, tsSecStart):
        SchedulerBase.__init__(self, oDb, oSchedGrpData, iVerbosity, tsSecStart);

    def _recreateQueueItems(self, oData):
        #
        # Prepare the input data for the loop below.  We compress the priority
        # to reduce the number of loops we need to executes below.
        #
        # Note! For BECI test group priority only applies to the ordering of
        #       test groups, which has been resolved by the done sorting in the
        #       base class.
        #
        iMinPriority = 0x7fff;
        iMaxPriority = 0;
        for oTestGroup in oData.aoTestGroups:
            for oTestCase in oTestGroup.aoTestCases:
                iPrio = oTestCase.iSchedPriority;
                assert iPrio in range(32);
                iPrio = iPrio / 4;
                assert iPrio in range(8);
                if iPrio > iMaxPriority:
                    iMaxPriority = iPrio;
                if iPrio < iMinPriority:
                    iMinPriority = iPrio;

                oTestCase.iBeciPrio      = iPrio;
                oTestCase.iNextVariation = -1;

        assert iMinPriority in range(8);
        assert iMaxPriority in range(8);
        assert iMinPriority <= iMaxPriority;

        #
        # Generate the
        #
        cMaxItems = len(oData.aoArgsVariations) * 64;
        if cMaxItems > 1048576:
            cMaxItems = 1048576;

        aoItems   = list();
        cNotAtEnd = len(oData.aoTestCases);
        while len(aoItems) < cMaxItems:
            self.msgDebug('outer loop: %s items' % (len(aoItems),));
            for iPrio in range(iMaxPriority, iMinPriority - 1, -1):
                #self.msgDebug('prio loop: %s' % (iPrio,));
                for oTestGroup in oData.aoTestGroups:
                    #self.msgDebug('testgroup loop: %s' % (oTestGroup,));
                    for oTestCase in oTestGroup.aoTestCases:
                        #self.msgDebug('testcase loop: idTestCase=%s' % (oTestCase.idTestCase,));
                        if iPrio <= oTestCase.iBeciPrio  and  oTestCase.aoArgsVariations:
                            # Get variation.
                            iNext = oTestCase.iNextVariation;
                            if iNext != 0:
                                if iNext == -1: iNext = 0;
                                cNotAtEnd -= 1;
                            oArgsVariation = oTestCase.aoArgsVariations[iNext];

                            # Update next variation.
                            iNext = (iNext + 1) % len(oTestCase.aoArgsVariations);
                            cNotAtEnd += iNext != 0;
                            oTestCase.iNextVariation = iNext;

                            # Create queue item and append it.
                            oItem = SchedQueueData();
                            oItem.initFromValues(idSchedGroup        = self._oSchedGrpData.idSchedGroup,
                                                 idGenTestCaseArgs   = oArgsVariation.idGenTestCaseArgs,
                                                 idTestGroup         = oTestGroup.idTestGroup,
                                                 aidTestGroupPreReqs = oTestGroup.aidTestGroupPreReqs,
                                                 bmHourlySchedule    = oTestGroup.bmHourlySchedule,
                                                 cMissingGangMembers = oArgsVariation.cGangMembers,
                                                 offQueue            = len(aoItems));
                            aoItems.append(oItem);

                            # Done?
                            if cNotAtEnd == 0:
                                self.msgDebug('returns %s items' % (len(aoItems),));
                                return aoItems;
        return aoItems;

