"""
Copyright (C) 2009-2016 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
"""

from __future__ import print_function
import sys

#Note, this should match the fakedri_glxfuncsList.h order
glx_functions = [
"CopyContext",
"UseXFont",
#"GetDriverConfig",
"GetProcAddress",
"QueryExtension",
"IsDirect",
"DestroyGLXPbufferSGIX",
"QueryGLXPbufferSGIX",
"CreateGLXPixmap",
"CreateGLXPixmapWithConfigSGIX",
"QueryContext",
"CreateContextWithConfigSGIX",
"SwapBuffers",
"CreateNewContext",
"SelectEventSGIX",
"GetCurrentDrawable",
"ChooseFBConfig",
"WaitGL",
"GetFBConfigs",
"CreatePixmap",
"GetSelectedEventSGIX",
"GetCurrentReadDrawable",
"GetCurrentDisplay",
"QueryServerString",
"CreateWindow",
"SelectEvent",
"GetVisualFromFBConfigSGIX",
"GetFBConfigFromVisualSGIX",
"QueryDrawable",
"CreateContext",
"GetConfig",
"CreateGLXPbufferSGIX",
"CreatePbuffer",
"ChooseFBConfigSGIX",
"WaitX",
"GetVisualFromFBConfig",
#"GetScreenDriver",
"GetFBConfigAttrib",
"GetCurrentContext",
"GetClientString",
"DestroyPixmap",
"MakeCurrent",
"DestroyContext",
"GetProcAddressARB",
"GetSelectedEvent",
"DestroyPbuffer",
"DestroyWindow",
"DestroyGLXPixmap",
"QueryVersion",
"ChooseVisual",
"MakeContextCurrent",
"QueryExtensionsString",
"GetFBConfigAttribSGIX",
"FreeMemoryMESA",
"QueryContextInfoEXT",
"ImportContextEXT",
"GetContextIDEXT",
"MakeCurrentReadSGI",
"AllocateMemoryMESA",
"GetMemoryOffsetMESA",
"CreateGLXPixmapMESA",
"GetCurrentDisplayEXT",
"FreeContextEXT"
];

print('%include "iprt/asmdefs.mac"')
print("")
print("%ifdef RT_ARCH_AMD64")
print("extern glxim")
print("%else ; X86")
print("extern glxim")
print("%endif")
print("")

for index in range(len(glx_functions)):
    func_name = glx_functions[index]

    print("BEGINPROC_EXPORTED vbox_glX%s" % func_name)
    print("%ifdef RT_ARCH_AMD64")
    print("\tjmp \t[glxim+%d wrt rip wrt ..gotpcrel]" % (8*index))
    print("%else ; X86")
    print("\tjmp \t[glxim+%d wrt ..gotpc]" % (4*index))
    print("%endif")
    print("ENDPROC vbox_glX%s" % func_name)
    print("")

