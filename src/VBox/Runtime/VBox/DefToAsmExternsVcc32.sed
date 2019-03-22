# $Id$
## @file
# SED script for generating assembly externs from a VBoxRT windows .def file.
#

#
# Copyright (C) 2006-2018 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

#
# Remove uncertain exports.
#
/not-some-systems/d

#
# Remove comments and space. Skip empty lines.
#
s/;.*$//g
s/^[[:space:]][[:space:]]*//g
s/[[:space:]][[:space:]]*$//g
/^$/d

#
# Handle text after EXPORTS
#
/EXPORTS/,//{
s/^EXPORTS$//
/^$/b end

/^?/b cpp_export
/[[:space:]]DATA$/b data

#
# Function export
#
:code
s/^\(.*\)$/EXTERN_IMP2 \1/
b end

#
# Data export
#
:data
s/^\(.*\)[[:space:]]*DATA$/EXTERN_IMP2 \1/
b end

#
# Mangled C++ .
#
:cpp_export
s/^\(.*\)$/extern __imp_\1/
b end

}
d
b end

#
# next expression
#
:end

