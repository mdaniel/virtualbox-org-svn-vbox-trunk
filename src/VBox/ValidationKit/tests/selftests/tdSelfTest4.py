#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager / Suite Self Test #4 - Testing result overflow handling.
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
import os;
import sys;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver         import reporter;
from testdriver.base    import TestDriverBase, InvalidOption;


class tdSelfTest4(TestDriverBase):
    """
    Test Manager / Suite Self Test #4 - Testing result overflow handling.
    """

    ## Valid tests.
    kasValidTests = [ 'immediate-sub-tests', 'total-sub-tests', 'immediate-values', 'total-values', 'immediate-messages'];

    def __init__(self):
        TestDriverBase.__init__(self);
        self.sOptWhich = 'immediate-sub-tests';

    def parseOption(self, asArgs, iArg):
        if asArgs[iArg] == '--test':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            if asArgs[iArg] not in self.kasValidTests:
                raise InvalidOption('Invalid test name "%s". Must be one of: %s'
                                    % (asArgs[iArg], ', '.join(self.kasValidTests),));
            self.sOptWhich = asArgs[iArg];
        else:
            return TestDriverBase.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def actionExecute(self):
        # Too many immediate sub-tests.
        if self.sOptWhich == 'immediate-sub-tests':
            reporter.testStart('Too many immediate sub-tests (negative)');
            for i in range(1024):
                reporter.testStart('subsub%d' % i);
                reporter.testDone();
        # Too many sub-tests in total.
        elif self.sOptWhich == 'total-sub-tests':
            reporter.testStart('Too many sub-tests (negative)');
            # 32 * 256 = 2^(5+8) = 2^13 = 8192.
            for i in range(32):
                reporter.testStart('subsub%d' % i);
                for j in range(256):
                    reporter.testStart('subsubsub%d' % j);
                    reporter.testDone();
                reporter.testDone();
        # Too many immediate values.
        elif self.sOptWhich == 'immediate-values':
            reporter.testStart('Too many immediate values (negative)');
            for i in range(512):
                reporter.testValue('value%d' % i, i, 'times');
        # Too many values in total.
        elif self.sOptWhich == 'total-values':
            reporter.testStart('Too many sub-tests (negative)');
            for i in range(256):
                reporter.testStart('subsub%d' % i);
                for j in range(64):
                    reporter.testValue('value%d' % j, i * 10000 + j, 'times');
                reporter.testDone();
        # Too many failure reasons (only immediate since the limit is extremely low).
        elif self.sOptWhich == 'immediate-messages':
            reporter.testStart('Too many immediate messages (negative)');
            for i in range(16):
                reporter.testFailure('Detail %d' % i);
        else:
            reporter.testStart('Unknown test %s' % (self.sOptWhich,));
            reporter.error('Invalid test selected: %s' % (self.sOptWhich,));
        reporter.testDone();
        return True;


if __name__ == '__main__':
    sys.exit(tdSelfTest4().main(sys.argv));

