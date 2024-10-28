# $Id$
## @file
# For defining member offsets for selected struct so the ARM64 assembler can use them.
#
# This script uses the 'pretty' command in the llvm-pdbutil.exe utility to do
# the dumping from IEMAllN8veRecompiler-obj.pdb.  The script ASSUMES a specific
# output format and indentation system to work.
#

#
# Copyright (C) 2024 Oracle and/or its affiliates.
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

#
# The top-level VMCPU members.
#
/^  struct VMCPU .* {$/,/^  }$/ {
    s/^    data [+]\(0x[[:xdigit:]][[:xdigit:]]*\) .* \([a-zA-Z_][a-zA-Z0-9_]*\)$/\#define VMCPU_OFF_\2 \1/p
    s/^    data [+]\(0x[[:xdigit:]][[:xdigit:]]*\) .* \([a-zA-Z_][a-zA-Z0-9_]*\)\[[0-9][0-9]*]$/\#define VMCPU_OFF_\2 \1/p
}

#
# The top-level IEMCPU members.
#
/^  struct IEMCPU .* {$/,/^  }$/ {
    s/^    data [+]\(0x[[:xdigit:]][[:xdigit:]]*\) .* \([a-zA-Z_][a-zA-Z0-9_]*\)$/\#define IEMCPU_OFF_\2 \1/p
    s/^    data [+]\(0x[[:xdigit:]][[:xdigit:]]*\) .* \([a-zA-Z_][a-zA-Z0-9_]*\)\[[0-9][0-9]*]$/\#define IEMCPU_OFF_\2 \1/p
}

