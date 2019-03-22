"""
Copyright (C) 2018 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
"""

import sys

def GeneratePfns():

    # Get list of functions.
    exports_file = open(sys.argv[1], "r")
    if not exports_file:
        print("Error: couldn't open %s file!" % filename)
        sys.exit()

    names = []
    for line in exports_file.readlines():
        line = line.strip()
        if len(line) > 0 and line[0] != ';' and line != 'EXPORTS':
            names.append(line)

    exports_file.close()


    #
    # C loader data
    #
    c_file = open(sys.argv[2], "w")
    if not c_file:
        print("Error: couldn't open %s file!" % filename)
        sys.exit()

    c_file.write('#include <iprt/win/windows.h>\n')
    c_file.write('#include <VBoxWddmUmHlp.h>\n')
    c_file.write('\n')

    for index in range(len(names)):
        fn = names[index]
        c_file.write('FARPROC pfn_%s;\n' % fn)
    c_file.write('\n')

    c_file.write("struct VBOXWDDMDLLPROC aIcdProcs[] =\n")
    c_file.write('{\n')
    for index in range(len(names)):
        fn = names[index]
        c_file.write('    { "%s",  &pfn_%s },\n' % (fn, fn) )
    c_file.write('    { NULL, NULL }\n')
    c_file.write('};\n')

    c_file.close()

GeneratePfns()
