"""
Copyright (C) 2009-2016 Oracle Corporation

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

import os,sys
from distutils.core import setup

def cleanupComCache():
    import shutil
    from distutils.sysconfig import get_python_lib
    comCache1 = os.path.join(get_python_lib(), 'win32com', 'gen_py')
    comCache2 = os.path.join(os.environ.get("TEMP", "c:\\tmp"), 'gen_py')
    print("Cleaning COM cache at",comCache1,"and",comCache2)
    shutil.rmtree(comCache1, True)
    shutil.rmtree(comCache2, True)

def patchWith(file,install,sdk):
    newFile=file + ".new"
    install=install.replace("\\", "\\\\")
    try:
        os.remove(newFile)
    except:
        pass
    oldF = open(file, 'r')
    newF = open(newFile, 'w')
    for line in oldF:
        line = line.replace("%VBOX_INSTALL_PATH%", install)
        line = line.replace("%VBOX_SDK_PATH%", sdk)
        newF.write(line)
    newF.close()
    oldF.close()
    try:
        os.remove(file)
    except:
        pass
    os.rename(newFile, file)

# See http://docs.python.org/distutils/index.html
def main(argv):
    vboxDest = os.environ.get("VBOX_MSI_INSTALL_PATH", None)
    if vboxDest is None:
        vboxDest = os.environ.get('VBOX_INSTALL_PATH', None)
        if vboxDest is None:
            raise Exception("No VBOX_INSTALL_PATH defined, exiting")

    vboxVersion = os.environ.get("VBOX_VERSION", None)
    if vboxVersion is None:
        # Should we use VBox version for binding module versioning?
        vboxVersion = "1.0"

    import platform

    if platform.system() == 'Windows':
        cleanupComCache()

    # Darwin: Patched before installation. Modifying bundle is not allowed, breaks signing and upsets gatekeeper.
    if platform.system() != 'Darwin':
        vboxSdkDest = os.path.join(vboxDest, "sdk")
        patchWith(os.path.join(os.path.dirname(sys.argv[0]), 'vboxapi', '__init__.py'), vboxDest, vboxSdkDest)

    setup(name='vboxapi',
          version=vboxVersion,
          description='Python interface to VirtualBox',
          author='Oracle Corp.',
          author_email='vbox-dev@virtualbox.org',
          url='http://www.virtualbox.org',
          packages=['vboxapi']
          )

if __name__ == '__main__':
    main(sys.argv)

