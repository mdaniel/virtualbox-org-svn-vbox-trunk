/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>

#ifdef VBOX
# include <iprt/assert.h>
# include <iprt/mem.h>
#endif

PRIntervalTime _PR_UNIX_GetInterval()
{
    struct timeval time;
    PRIntervalTime ticks;

    (void)GETTIMEOFDAY(&time);  /* fallicy of course */
    ticks = (PRUint32)time.tv_sec * PR_MSEC_PER_SEC;  /* that's in milliseconds */
    ticks += (PRUint32)time.tv_usec / PR_USEC_PER_MSEC;  /* so's that */
    return ticks;
}  /* _PR_SUNOS_GetInterval */

PRIntervalTime _PR_UNIX_TicksPerSecond()
{
    return 1000;  /* this needs some work :) */
}

/************************************************************************/

#if defined(SOLARIS)
PRIntervalTime _MD_Solaris_TicksPerSecond(void)
{
    /*
     * Ticks have a 10-microsecond resolution.  So there are
     * 100000 ticks per second.
     */
    return 100000UL;
}

/* Interval timers, implemented using gethrtime() */

PRIntervalTime _MD_Solaris_GetInterval(void)
{
    union {
	hrtime_t hrt;  /* hrtime_t is a 64-bit (long long) integer */
	PRInt64 pr64;
    } time;
    PRInt64 resolution;
    PRIntervalTime ticks;

    time.hrt = gethrtime();  /* in nanoseconds */
    /*
     * Convert from nanoseconds to ticks.  A tick's resolution is
     * 10 microseconds, or 10000 nanoseconds.
     */
    LL_I2L(resolution, 10000);
    LL_DIV(time.pr64, time.pr64, resolution);
    LL_L2UI(ticks, time.pr64);
    return ticks;
}
#endif

