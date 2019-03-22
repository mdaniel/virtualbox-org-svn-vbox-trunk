# coding=utf-8
#
# Copyright © 2011 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# Compare two files containing IR code.  Ignore formatting differences
# and declaration order.

import os
import os.path
import subprocess
import sys
import tempfile

from sexps import *

if len(sys.argv) != 3:
    print 'Usage: python2 ./compare_ir.py <file1> <file2>'
    exit(1)

with open(sys.argv[1]) as f:
    ir1 = sort_decls(parse_sexp(f.read()))
with open(sys.argv[2]) as f:
    ir2 = sort_decls(parse_sexp(f.read()))

if ir1 == ir2:
    exit(0)
else:
    file1, path1 = tempfile.mkstemp(os.path.basename(sys.argv[1]))
    file2, path2 = tempfile.mkstemp(os.path.basename(sys.argv[2]))
    try:
        os.write(file1, '{0}\n'.format(sexp_to_string(ir1)))
        os.close(file1)
        os.write(file2, '{0}\n'.format(sexp_to_string(ir2)))
        os.close(file2)
        subprocess.call(['diff', '-u', path1, path2])
    finally:
        os.remove(path1)
        os.remove(path2)
    exit(1)
