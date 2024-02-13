"""
Copyright (C) 2024 Oracle and/or its affiliates.

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

#
# This is just a wrapper for calling vboxapi/setup.py, so that we can keep
# the behavior of vboxapisetup.py as we did in the past.
#
if __name__ == '__main__':
    import sys
    try:
        sys.path.append("vboxapi") # ASSUMES that the CWD is the same as the script directory. This has always been the case.
        import setup
        sys.exit(setup.main())
    except ImportError as oXcpt:
        print("VBox Python API binding sources not found: %s" % (oXcpt,))
    sys.exit(1)
