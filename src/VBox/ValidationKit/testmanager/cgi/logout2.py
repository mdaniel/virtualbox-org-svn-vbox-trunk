#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - CGI - Log out page for Safari.
"""

__copyright__ = \
"""
Copyright (C) 2012-2024 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
"""
__version__ = "$Revision$"


# Standard python imports.
import os
import sys

# Only the main script needs to modify the path.
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testmanager.core.webservergluecgi import WebServerGlueCgi;


def main():
    """
    Main function a la C/C++. Returns exit code.
    """

    oSrvGlue = WebServerGlueCgi(g_ksValidationKitDir, fHtmlOutput = True);
    sUserAgent = oSrvGlue.getUserAgent();
    oSrvGlue.setHeaderField('Status', '401 Unauthorized to access the document');
    oSrvGlue.setHeaderField('WWW-authenticate', 'Basic realm="Test Manager"');
    if sUserAgent.startswith('Mozilla/') and sUserAgent.find('AppleWebKit/') > 0:
        oSrvGlue.write('<p>Attempting to log out an Apple browser...</p>');
    else:
        oSrvGlue.write('<p>Sorry, not sure this will work...</p>');
        oSrvGlue.write('<p>User-Agent:' + sUserAgent + '</p>');

    oSrvGlue.write('<p><a href="%sadmin.py">Log in</a> under another user name.</p>' %
                   (oSrvGlue.getBaseUrl(),))

    oSrvGlue.write('<hr/><p>debug info:</p>');
    oSrvGlue.debugInfoPage();
    oSrvGlue.flush();

    return 0;

if __name__ == '__main__':
    sys.exit(main());

