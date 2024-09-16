# $Id$
## @file
# WebService - SED script for extracting inline functions from soapH.h
#              for putting them in a C++ source file.
#

#
# Copyright (C) 2020-2024 Oracle and/or its affiliates.
#
# This file is part of VirtualBox base platform packages, as
# available from https://www.virtualbox.org.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, in version 3 of the
# License.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses>.
#
# SPDX-License-Identifier: GPL-3.0-only
#

/^inline /,/^}/ {
    /^inline /,/^{/ {
        s/^inline/\n\/\*noinline\*\//
        s/ *= *\([^,)]*\)\([),]\)/ \/\*=\1\*\/\2/
    }
    p
}
1 c #include "soapH.h"
d

