/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of Linux-specific keyboard functions.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___XKeyboard_h___
#define ___XKeyboard_h___

/* Forward declarations: */
class QString;
typedef struct _XDisplay Display;

/** Initializes the X keyboard subsystem. */
void initMappedX11Keyboard(Display *pDisplay, const QString &remapScancodes);

/** Handles native XKey events. */
unsigned handleXKeyEvent(Display *pDisplay, unsigned int iDetail);

/** Handles log requests from initXKeyboard after release logging is started. */
void doXKeyboardLogging(Display *pDisplay);

/** Wraps for the XkbKeycodeToKeysym(3) API which falls back to the deprecated XKeycodeToKeysym(3) if it is unavailable. */
unsigned long wrapXkbKeycodeToKeysym(Display *pDisplay, unsigned char cCode,
                                     unsigned int cGroup, unsigned int cIndex);

#endif /* !___XKeyboard_h___ */

