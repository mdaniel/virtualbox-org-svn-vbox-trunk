/* $Id$ */
/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation.
 */

/*
 * Copyright (C) 2014-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_cocoa_h
#define VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_cocoa_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/VBoxCocoa.h>

RT_C_DECLS_BEGIN

#ifndef ___renderspu_cocoa_helper_h
ADD_COCOA_NATIVE_REF(NSView);
ADD_COCOA_NATIVE_REF(NSOpenGLContext);
#endif

#ifdef IN_VMSVGA3DCOCOA
# define VMSVGA3DCOCOA_DECL(type)  DECLEXPORT(type)
#else
# define VMSVGA3DCOCOA_DECL(type)  DECLIMPORT(type)
#endif

VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaServiceRunLoop(void);
VMSVGA3DCOCOA_DECL(bool) vmsvga3dCocoaCreateViewAndContext(NativeNSViewRef *ppView, NativeNSOpenGLContextRef *ppCtx,
                                                           NativeNSViewRef pParentView, uint32_t cx, uint32_t cy,
                                                           NativeNSOpenGLContextRef pSharedCtx, bool fOtherProfile);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaDestroyViewAndContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewInfo(PCDBGFINFOHLP pHlp, NativeNSViewRef pView);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int w, int h);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewUpdateViewport(NativeNSViewRef pView);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);
VMSVGA3DCOCOA_DECL(void) vmsvga3dCocoaSwapBuffers(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);

int ExplicitlyLoadVBoxSVGA3DObjC(bool fResolveAllImports, PRTERRINFO pErrInfo);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_cocoa_h */

