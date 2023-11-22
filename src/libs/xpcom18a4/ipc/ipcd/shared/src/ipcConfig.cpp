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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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
#define LOG_GROUP LOG_GROUP_IPC

#if defined(XP_WIN)
#elif defined(XP_OS2) && defined(XP_OS2_NATIVEIPC)
#else
#include <string.h>
#ifdef XP_UNIX
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#include "ipcConfig.h"
#include "plstr.h"

#include <iprt/env.h>
#include <VBox/log.h>

#if defined(XP_OS2) && !defined(XP_OS2_NATIVEIPC)
#ifdef VBOX
static const char kDefaultSocketPrefix[] = "\\socket\\vbox-";
#else
static const char kDefaultSocketPrefix[] = "\\socket\\mozilla-";
#endif
static const char kDefaultSocketSuffix[] = "-ipc\\ipcd";
#else
#ifdef VBOX
static const char kDefaultSocketPrefix[] = "/tmp/.vbox-";
#else
static const char kDefaultSocketPrefix[] = "/tmp/.mozilla-";
#endif
static const char kDefaultSocketSuffix[] = "-ipc/ipcd";
#endif

void IPC_GetDefaultSocketPath(char *buf, PRUint32 bufLen)
{
    const char *logName;
    int len;

    PL_strncpyz(buf, kDefaultSocketPrefix, bufLen);
    buf    += (sizeof(kDefaultSocketPrefix) - 1);
    bufLen -= (sizeof(kDefaultSocketPrefix) - 1);

    logName = RTEnvGet("VBOX_IPC_SOCKETID");
#if defined(VBOX) && defined(XP_UNIX)
    if (!logName || !logName[0]) {
        struct passwd *passStruct = getpwuid(getuid());
        if (passStruct)
            logName = passStruct->pw_name;
    }
#endif
    if (!logName || !logName[0]) {
        logName = RTEnvGet("LOGNAME");
        if (!logName || !logName[0]) {
            logName = RTEnvGet("USER");
            if (!logName || !logName[0]) {
                Log(("could not determine username from environment\n"));
                goto end;
            }
        }
    }
    PL_strncpyz(buf, logName, bufLen);
    len = strlen(logName);
    buf    += len;
    bufLen -= len;

end:
    PL_strncpyz(buf, kDefaultSocketSuffix, bufLen);
}

#endif
