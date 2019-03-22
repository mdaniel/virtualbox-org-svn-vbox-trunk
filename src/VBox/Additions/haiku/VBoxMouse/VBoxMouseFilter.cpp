/* $Id$ */
/** @file
 * VBoxMouse; input_server filter - Haiku Guest Additions, implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 * Copyright (c) 2011 Mike Smith <mike@scgtrp.net>
 *                    Fran�ois Revol <revol@free.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <Clipboard.h>
#include <Debug.h>
#include <Message.h>
#include <String.h>

#include "VBoxMouseFilter.h"
#include <VBox/VBoxGuestLib.h>
#include <VBoxGuestInternal.h>
#include <VBox/log.h>
#include <iprt/errcore.h>

/** @todo can this be merged with VBoxMouse? */

RTDECL(BInputServerFilter *)
instantiate_input_filter()
{
    return new VBoxMouseFilter();
}

VBoxMouseFilter::VBoxMouseFilter()
     : BInputServerFilter(),
       fDriverFD(-1),
       fServiceThreadID(-1),
       fExiting(false),
       fCurrentButtons(0)
{
}


VBoxMouseFilter::~VBoxMouseFilter()
{
}


filter_result VBoxMouseFilter::Filter(BMessage *message, BList *outList)
{
    switch (message->what)
    {
        case B_MOUSE_UP:
        case B_MOUSE_DOWN:
        {
            printf("click|release\n");
            message->FindInt32("buttons", &fCurrentButtons);
            /** @todo r=ramshankar this looks wrong, no 'break' here? */
        }

        case B_MOUSE_MOVED:
        {
            printf("mouse moved\n");
            message->ReplaceInt32("buttons", fCurrentButtons);
            /** @todo r=ramshankar: 'break' or explicit comment please. */
        }
    }

    return B_DISPATCH_MESSAGE;
}

