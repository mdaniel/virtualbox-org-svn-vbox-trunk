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
 *   Roland Mainz <roland mainz@informatik.med.uni-giessen.de>
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

#ifndef _plstr_h
#define _plstr_h

/*
 * plstr.h
 *
 * This header file exports the API to the NSPR portable library or string-
 * handling functions.  
 * 
 * This API was not designed as an "optimal" or "ideal" string library; it 
 * was based on the good ol' unix string.3 functions, and was written to
 *
 *  1) replace the libc functions, for cross-platform consistancy, 
 *  2) complete the API on platforms lacking common functions (e.g., 
 *     strcase*), and
 *  3) to implement some obvious "closure" functions that I've seen
 *     people hacking around in our code.
 *
 * Point number three largely means that most functions have an "strn"
 * limited-length version, and all comparison routines have a non-case-
 * sensitive version available.
 */

#include "prtypes.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PL_strlen VBoxNsplPL_strlen
#define PL_strcmp VBoxNsplPL_strcmp
#define PL_strncmp VBoxNsplPL_strncmp
#define PL_strcasecmp VBoxNsplPL_strcasecmp
#define PL_strncasecmp VBoxNsplPL_strncasecmp
#define PL_strdup VBoxNsplPL_strdup
#define PL_strfree VBoxNsplPL_strfree
#define PL_strncpy VBoxNsplPL_strncpy
#define PL_strncpyz VBoxNsplPL_strncpyz
#define PL_strcaserstr VBoxNsplPL_strcaserstr
#define PL_strcasestr VBoxNsplPL_strcasestr
#define PL_strndup VBoxNsplPL_strndup
#define PL_strnlen VBoxNsplPL_strnlen
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C
/*
 * PL_strlen
 *
 * Returns the length of the provided string, not including the trailing '\0'.
 */

PR_EXTERN(PRUint32)
PL_strlen(const char *str);

/*
 * PL_strnlen
 *
 * Returns the length of the provided string, not including the trailing '\0',
 * up to the indicated maximum.  The string will not be examined beyond the
 * maximum; if no terminating '\0' is found, the maximum will be returned.
 */

PR_EXTERN(PRUint32)
PL_strnlen(const char *str, PRUint32 max);

/*
 * PL_strncpy
 *
 * Copies the source string into the destination buffer, up to and including
 * the trailing '\0' or up to and including the max'th character, whichever
 * comes first.  It does not (can not) verify that the destination buffer is
 * large enough.  If the source string is longer than the maximum length,
 * the result will *not* be null-terminated (JLRU).
 */

PR_EXTERN(char *)
PL_strncpy(char *dest, const char *src, PRUint32 max);

/*
 * PL_strncpyz
 *
 * Copies the source string into the destination buffer, up to and including 
 * the trailing '\0' or up but not including the max'th character, whichever 
 * comes first.  It does not (can not) verify that the destination buffer is
 * large enough.  The destination string is always terminated with a '\0',
 * unlike the traditional libc implementation.  It returns the "dest" argument.
 *
 * NOTE: If you call this with a source "abcdefg" and a max of 5, the 
 * destination will end up with "abcd\0" (i.e., it's strlen length will be 4)!
 *
 * This means you can do this:
 *
 *     char buffer[ SOME_SIZE ];
 *     PL_strncpyz(buffer, src, sizeof(buffer));
 *
 * and the result will be properly terminated.
 */

PR_EXTERN(char *)
PL_strncpyz(char *dest, const char *src, PRUint32 max);

/*
 * PL_strcmp
 *
 * Returns an integer, the sign of which -- positive, zero, or negative --
 * reflects the lexical sorting order of the two strings indicated.  The
 * result is positive if the first string comes after the second.  The
 * NSPR implementation is not i18n.
 */

PR_EXTERN(PRIntn)
PL_strcmp(const char *a, const char *b);

/*
 * PL_strncmp
 * 
 * Returns an integer, the sign of which -- positive, zero, or negative --
 * reflects the lexical sorting order of the two strings indicated, up to
 * the maximum specified.  The result is positive if the first string comes 
 * after the second.  The NSPR implementation is not i18n.  If the maximum
 * is zero, only the existance or non-existance (pointer is null) of the
 * strings is compared.
 */

PR_EXTERN(PRIntn)
PL_strncmp(const char *a, const char *b, PRUint32 max);

/*
 * PL_strcasecmp
 *
 * Returns an integer, the sign of which -- positive, zero or negative --
 * reflects the case-insensitive lexical sorting order of the two strings
 * indicated.  The result is positive if the first string comes after the 
 * second.  The NSPR implementation is not i18n.
 */

PR_EXTERN(PRIntn)
PL_strcasecmp(const char *a, const char *b);

/*
 * PL_strncasecmp
 *
 * Returns an integer, the sign of which -- positive, zero or negative --
 * reflects the case-insensitive lexical sorting order of the first n characters
 * of the two strings indicated.  The result is positive if the first string comes 
 * after the second.  The NSPR implementation is not i18n.
 */

PR_EXTERN(PRIntn)
PL_strncasecmp(const char *a, const char *b, PRUint32 max);

PR_END_EXTERN_C

#endif /* _plstr_h */
