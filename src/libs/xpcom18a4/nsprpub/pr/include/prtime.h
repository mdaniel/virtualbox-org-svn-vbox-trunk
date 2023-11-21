/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 *----------------------------------------------------------------------
 *
 * prtime.h --
 *
 *     NSPR date and time functions
 *
 *-----------------------------------------------------------------------
 */

#ifndef prtime_h___
#define prtime_h___

#include "prlong.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_Now VBoxNsprPR_Now
#define PR_ExplodeTime VBoxNsprPR_ExplodeTime
#define PR_ImplodeTime VBoxNsprPR_ImplodeTime
#define PR_NormalizeTime VBoxNsprPR_NormalizeTime
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

#define PR_MSEC_PER_SEC		1000UL
#define PR_USEC_PER_SEC		1000000UL
#define PR_NSEC_PER_SEC		1000000000UL
#define PR_USEC_PER_MSEC	1000UL
#define PR_NSEC_PER_MSEC	1000000UL

/*
 * PRTime --
 *
 *     NSPR represents basic time as 64-bit signed integers relative
 *     to midnight (00:00:00), January 1, 1970 Greenwich Mean Time (GMT).
 *     (GMT is also known as Coordinated Universal Time, UTC.)
 *     The units of time are in microseconds. Negative times are allowed
 *     to represent times prior to the January 1970 epoch. Such values are
 *     intended to be exported to other systems or converted to human
 *     readable form.
 *
 *     Notes on porting: PRTime corresponds to time_t in ANSI C.  NSPR 1.0
 *     simply uses PRInt64.
 */

typedef PRInt64 PRTime;

/*
 * Time zone and daylight saving time corrections applied to GMT to
 * obtain the local time of some geographic location
 */

typedef struct PRTimeParameters {
    PRInt32 tp_gmt_offset;     /* the offset from GMT in seconds */
    PRInt32 tp_dst_offset;     /* contribution of DST in seconds */
} PRTimeParameters;

/*
 * PRExplodedTime --
 *
 *     Time broken down into human-readable components such as year, month,
 *     day, hour, minute, second, and microsecond.  Time zone and daylight
 *     saving time corrections may be applied.  If they are applied, the
 *     offsets from the GMT must be saved in the 'tm_params' field so that
 *     all the information is available to reconstruct GMT.
 *
 *     Notes on porting: PRExplodedTime corrresponds to struct tm in
 *     ANSI C, with the following differences:
 *       - an additional field tm_usec;
 *       - replacing tm_isdst by tm_params;
 *       - the month field is spelled tm_month, not tm_mon;
 *       - we use absolute year, AD, not the year since 1900.
 *     The corresponding type in NSPR 1.0 is called PRTime.  Below is
 *     a table of date/time type correspondence in the three APIs:
 *         API          time since epoch          time in components
 *       ANSI C             time_t                  struct tm
 *       NSPR 1.0           PRInt64                   PRTime
 *       NSPR 2.0           PRTime                  PRExplodedTime
 */

typedef struct PRExplodedTime {
    PRInt32 tm_usec;		    /* microseconds past tm_sec (0-99999)  */
    PRInt32 tm_sec;             /* seconds past tm_min (0-61, accomodating
                                   up to two leap seconds) */	
    PRInt32 tm_min;             /* minutes past tm_hour (0-59) */
    PRInt32 tm_hour;            /* hours past tm_day (0-23) */
    PRInt32 tm_mday;            /* days past tm_mon (1-31, note that it
				                starts from 1) */
    PRInt32 tm_month;           /* months past tm_year (0-11, Jan = 0) */
    PRInt16 tm_year;            /* absolute year, AD (note that we do not
				                count from 1900) */

    PRInt8 tm_wday;		        /* calculated day of the week
				                (0-6, Sun = 0) */
    PRInt16 tm_yday;            /* calculated day of the year 
				                (0-365, Jan 1 = 0) */

    PRTimeParameters tm_params;  /* time parameters used by conversion */
} PRExplodedTime;

/*
 * PRTimeParamFn --
 *
 *     A function of PRTimeParamFn type returns the time zone and
 *     daylight saving time corrections for some geographic location,
 *     given the current time in GMT.  The input argument gmt should
 *     point to a PRExplodedTime that is in GMT, i.e., whose
 *     tm_params contains all 0's.
 *
 *     For any time zone other than GMT, the computation is intended to
 *     consist of two steps:
 *       - Figure out the time zone correction, tp_gmt_offset.  This number
 *         usually depends on the geographic location only.  But it may
 *         also depend on the current time.  For example, all of China
 *         is one time zone right now.  But this situation may change
 *         in the future.
 *       - Figure out the daylight saving time correction, tp_dst_offset.
 *         This number depends on both the geographic location and the
 *         current time.  Most of the DST rules are expressed in local
 *         current time.  If so, one should apply the time zone correction
 *         to GMT before applying the DST rules.
 */

typedef PRTimeParameters (PR_CALLBACK *PRTimeParamFn)(const PRExplodedTime *gmt);

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/*
 * The PR_Now routine returns the current time relative to the
 * epoch, midnight, January 1, 1970 UTC. The units of the returned
 * value are microseconds since the epoch.
 *
 * The values returned are not guaranteed to advance in a linear fashion
 * due to the application of time correction protocols which synchronize
 * computer clocks to some external time source. Consequently it should
 * not be depended on for interval timing.
 *
 * The implementation is machine dependent.
 * Cf. time_t time(time_t *tp) in ANSI C.
 */
#if defined(HAVE_WATCOM_BUG_2)
PRTime __pascal __export __loadds
#else
NSPR_API(PRTime) 
#endif
PR_Now(void);

/*
 * Expand time binding it to time parameters provided by PRTimeParamFn.
 * The calculation is envisoned to proceed in the following steps:
 *   - From given PRTime, calculate PRExplodedTime in GMT
 *   - Apply the given PRTimeParamFn to the GMT that we just calculated
 *     to obtain PRTimeParameters.
 *   - Add the PRTimeParameters offsets to GMT to get the local time
 *     as PRExplodedTime.
 */

NSPR_API(void) PR_ExplodeTime(
    PRTime usecs, PRTimeParamFn params, PRExplodedTime *exploded);

/* Reverse operation of PR_ExplodeTime */
#if defined(HAVE_WATCOM_BUG_2)
PRTime __pascal __export __loadds
#else
NSPR_API(PRTime) 
#endif
PR_ImplodeTime(const PRExplodedTime *exploded);

/*
 * Adjust exploded time to normalize field overflows after manipulation.
 * Note that the following fields of PRExplodedTime should not be
 * manipulated:
 *   - tm_month and tm_year: because the number of days in a month and
 *     number of days in a year are not constant, it is ambiguous to
 *     manipulate the month and year fields, although one may be tempted
 *     to.  For example, what does "a month from January 31st" mean?
 *   - tm_wday and tm_yday: these fields are calculated by NSPR.  Users
 *     should treat them as "read-only".
 */

NSPR_API(void) PR_NormalizeTime(
    PRExplodedTime *exploded, PRTimeParamFn params);

PR_END_EXTERN_C

#endif /* prtime_h___ */
