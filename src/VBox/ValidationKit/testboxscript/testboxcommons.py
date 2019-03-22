# -*- coding: utf-8 -*-
# $Id$

"""
TestBox Script - Common Functions and Classes.

This module contains constants and functions that are useful for all
the files in this (testbox) directory.
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


# Standard python imports.
import sys
import traceback

# Validation Kit imports.
from common import utils;

#
# Exceptions.
#

class TestBoxException(Exception):
    """
    Custom exception class
    """
    pass

#
# Logging.
#

def log(sMessage, sCaller = None, sTsPrf = None):
    """
    Print out a message and flush stdout
    """
    if sTsPrf is None: sTsPrf = utils.getTimePrefix();
    print('[%s] %s' % (sTsPrf, sMessage,));
    sys.stdout.flush();
    _ = sCaller;

def log2(sMessage, sCaller = None, sTsPrf = None):
    """
    Debug logging, will later be disabled by default.
    """
    if True is True:
        if sTsPrf is None: sTsPrf = utils.getTimePrefix();
        print('[%s] %s' % (sTsPrf, sMessage,));
        sys.stdout.flush()
        _ = sCaller;

def _logXcptWorker(fnLogger, sPrefix = '', sText = None, cFrames = 1, fnLogger1 = log):
    """
    Log an exception, optionally with a preceeding message and more than one
    call frame.
    """
    ## @todo skip all this if iLevel is too high!

    # Try get exception info.
    sTsPrf = utils.getTimePrefix();
    try:
        oType, oValue, oTraceback = sys.exc_info();
    except:
        oType = oValue = oTraceback = None;
    if oType is not None:

        # Try format the info
        try:
            rc      = 0;
            sCaller = utils.getCallerName(oTraceback.tb_frame);
            if sText is not None:
                rc = fnLogger('%s%s' % (sPrefix, sText), sCaller, sTsPrf);
            asInfo = [];
            try:
                asInfo = asInfo + traceback.format_exception_only(oType, oValue);
                if cFrames is not None and cFrames <= 1:
                    asInfo = asInfo + traceback.format_tb(oTraceback, 1);
                else:
                    asInfo.append('Traceback:')
                    asInfo = asInfo + traceback.format_tb(oTraceback, cFrames);
                    asInfo.append('Stack:')
                    asInfo = asInfo + traceback.format_stack(oTraceback.tb_frame.f_back, cFrames);
            except:
                fnLogger1('internal-error: Hit exception #2! %s' % (traceback.format_exc()), sCaller, sTsPrf);

            if asInfo:
                # Do the logging.
                for sItem in asInfo:
                    asLines = sItem.splitlines();
                    for sLine in asLines:
                        rc = fnLogger('%s%s' % (sPrefix, sLine), sCaller, sTsPrf);

            else:
                fnLogger('No exception info...', sCaller, sTsPrf);
                rc = -3;
        except:
            fnLogger1('internal-error: Hit exception! %s' % (traceback.format_exc()), None, sTsPrf);
            rc = -2;
    else:
        fnLogger1('internal-error: No exception! %s' % (utils.getCallerName(iFrame=3)), utils.getCallerName(iFrame=3), sTsPrf);
        rc = -1;

    return rc;


def log1Xcpt(sText = None, cFrames = 1):
    """Logs an exception."""
    return _logXcptWorker(log, '', sText, cFrames);

def log2Xcpt(sText = None, cFrames = 1):
    """Debug logging of an exception."""
    return _logXcptWorker(log2, '', sText, cFrames);

