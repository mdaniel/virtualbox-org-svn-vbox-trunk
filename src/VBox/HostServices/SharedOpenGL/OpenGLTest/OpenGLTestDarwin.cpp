/* $Id$ */
/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/VBoxOGL.h>

#include <IOKit/IOKitLib.h>
#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/gl.h>
#ifdef VBOX_WITH_COCOA_QT
# include <OpenGL/glu.h>
#endif

#include <iprt/env.h>
#include <iprt/log.h>
#include <iprt/once.h>



/**
 * @callback_method_impl{FNRTONCE,
 *  For determining the cached VBoxOglIsOfflineRenderingAppropriate result.}
 */
static DECLCALLBACK(int32_t) vboxOglIsOfflineRenderingAppropriateOnce(void *pvUser)
{
    bool *pfAppropriate = (bool *)pvUser;

    /* It is assumed that it is makes sense to enable offline rendering
       only in case if host has more than one GPU installed. This routine
       counts all the PCI devices in IORegistry which have IOName property
       set to "display". If the number of such devices is greater than one,
       it sets pfAppropriate to TRUE, otherwise to FALSE. */

    CFStringRef apKeyStrings[]   = { CFSTR(kIOProviderClassKey), CFSTR(kIONameMatchKey) };
    CFStringRef apValueStrings[] = { CFSTR("IOPCIDevice"),       CFSTR("display") };
    Assert(RT_ELEMENTS(apKeyStrings) == RT_ELEMENTS(apValueStrings));

    CFDictionaryRef pMatchingDictionary = CFDictionaryCreate(kCFAllocatorDefault,
                                                             (const void **)apKeyStrings,
                                                             (const void **)apValueStrings,
                                                             RT_ELEMENTS(apKeyStrings),
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    if (pMatchingDictionary)
    {
        /* The reference to pMatchingDictionary is consumed by the function below => no IORelease(pMatchingDictionary)! */
        io_iterator_t matchingServices;
        kern_return_t krc = IOServiceGetMatchingServices(kIOMasterPortDefault, pMatchingDictionary, &matchingServices);
        if (krc == kIOReturnSuccess)
        {
            io_object_t matchingService;
            int         cMatchingServices = 0;

            while ((matchingService = IOIteratorNext(matchingServices)) != 0)
            {
                cMatchingServices++;
                IOObjectRelease(matchingService);
            }

            *pfAppropriate = cMatchingServices > 1;

            IOObjectRelease(matchingServices);
        }
    }

    LogRel(("OpenGL: Offline rendering support is %s (pid=%d)\n", *pfAppropriate ? "ON" : "OFF", (int)getpid()));
    return VINF_SUCCESS;
}


bool RTCALL VBoxOglIsOfflineRenderingAppropriate(void)
{
    /* In order to do not slowdown 3D engine which can ask about offline rendering several times,
       let's cache the result and assume that renderers amount value is constant. Use the IPRT
       execute once construct to make sure there aren't any threading issues. */
    static RTONCE s_Once = RTONCE_INITIALIZER;
    static bool   s_fCached = false;
    int rc = RTOnce(&s_Once, vboxOglIsOfflineRenderingAppropriateOnce, &s_fCached);
    AssertRC(rc);
    return s_fCached;
}


bool RTCALL VBoxOglIs3DAccelerationSupported(void)
{
    if (RTEnvExist("VBOX_CROGL_FORCE_SUPPORTED"))
    {
        LogRel(("VBOX_CROGL_FORCE_SUPPORTED is specified, skipping 3D test, and treating as supported\n"));
        return true;
    }

    CGOpenGLDisplayMask     cglDisplayMask = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());
    CGLPixelFormatAttribute aAttribs[] =
    {
        kCGLPFADisplayMask,
        (CGLPixelFormatAttribute)cglDisplayMask,
        kCGLPFAAccelerated,
        kCGLPFADoubleBuffer,
        VBoxOglIsOfflineRenderingAppropriate() ? kCGLPFAAllowOfflineRenderers : (CGLPixelFormatAttribute)NULL,
        (CGLPixelFormatAttribute)NULL
    };
    CGLPixelFormatObj pPixelFormat = NULL;
    GLint             cPixelFormatsIgnored = 0;
    CGLError rcCgl = CGLChoosePixelFormat(aAttribs, &pPixelFormat, &cPixelFormatsIgnored);
    if (rcCgl != kCGLNoError)
    {
        LogRel(("OpenGL Info: 3D test unable to choose pixel format (rcCgl=0x%X)\n", rcCgl));
        return false;
    }

    if (pPixelFormat)
    {
        CGLContextObj pCglContext = 0;
        rcCgl = CGLCreateContext(pPixelFormat, NULL, &pCglContext);
        CGLDestroyPixelFormat(pPixelFormat);

        if (rcCgl != kCGLNoError)
        {
            LogRel(("OpenGL Info: 3D test unable to create context (rcCgl=0x%X)\n", rcCgl));
            return false;
        }

        if (pCglContext)
        {
            GLboolean isSupported = GL_TRUE;

#ifdef VBOX_WITH_COCOA_QT
            /*
             * In the Cocoa port we depend on the GL_EXT_framebuffer_object &
             * the GL_EXT_texture_rectangle extension. If they are not
             * available, disable 3D support.
             */
            CGLSetCurrentContext(pCglContext);
            const GLubyte *pszExts = glGetString(GL_EXTENSIONS);
            isSupported = gluCheckExtension((const GLubyte *)"GL_EXT_framebuffer_object", pszExts);
            if (isSupported)
            {
                isSupported = gluCheckExtension((const GLubyte *)"GL_EXT_texture_rectangle", pszExts);
                if (!isSupported)
                    LogRel(("OpenGL Info: 3D test found that GL_EXT_texture_rectangle extension not supported.\n"));
            }
            else
                LogRel(("OpenGL Info: 3D test found that GL_EXT_framebuffer_object extension not supported.\n"));
#endif /* VBOX_WITH_COCOA_QT */

            CGLDestroyContext(pCglContext);
            LogRel(("OpenGL Info: 3D test %spassed\n", isSupported == GL_TRUE ? "" : "not "));
            return isSupported == GL_TRUE;
        }

        LogRel(("OpenGL Info: 3D test unable to create context (internal error).\n"));
    }
    else
        LogRel(("OpenGL Info: 3D test unable to choose pixel format (internal error).\n"));

    return false;
}

