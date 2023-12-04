/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Implementation of misc. xpti stuff. */

#include <iprt/file.h>
#include <iprt/time.h>
#include <iprt/string.h>

#include "xptiprivate.h"

struct xptiFileTypeEntry
{
    const char*         name;
    int                 len;
    xptiFileType::Type  type;
};

static const xptiFileTypeEntry g_Entries[] = 
    {
        {".xpt", 4, xptiFileType::XPT},            
        {".zip", 4, xptiFileType::ZIP},            
        {".jar", 4, xptiFileType::ZIP},            
        {nsnull, 0, xptiFileType::UNKNOWN}            
    };

// static 
xptiFileType::Type xptiFileType::GetType(const char* name)
{
    NS_ASSERTION(name, "loser!");
    int len = strlen(name);
    for(const xptiFileTypeEntry* p = g_Entries; p->name; p++)
    {
        if(len > p->len && 0 == RTStrICmp(p->name, &(name[len - p->len])))
            return p->type;
    }
    return UNKNOWN;        
}        

/***************************************************************************/

MOZ_DECL_CTOR_COUNTER(xptiAutoLog)

xptiAutoLog::xptiAutoLog(xptiInterfaceInfoManager* mgr, 
                         const char *logfile, PRBool append)
    : mMgr(nsnull), mOldFileDesc(nsnull)
{
    MOZ_COUNT_CTOR(xptiAutoLog);

    if(mgr && logfile)
    {
        RTFILE hFile      = NIL_RTFILE;
        PRTSTREAM pStream = NULL;
        int vrc = RTFileOpen(&hFile, logfile,
                             RTFILE_O_OPEN_CREATE | RTFILE_O_WRITE | RTFILE_O_APPEND | RTFILE_O_DENY_NONE
                             | (append ? 0 : RTFILE_O_TRUNCATE)
                             | (0600 << RTFILE_O_CREATE_MODE_SHIFT));
        if (RT_SUCCESS(vrc))
            vrc = RTStrmOpenFileHandle(hFile, "at", 0 /*fFlags*/, &pStream);

        if (RT_SUCCESS(vrc))
        {
#ifdef DEBUG
            m_DEBUG_FileDesc = pStream;
#endif
            mMgr = mgr;
            mOldFileDesc = mMgr->SetOpenLogFile(pStream);
            if (append)
                RTStrmSeek(pStream, 0, RTFILE_SEEK_END);
            WriteTimestamp(pStream, "++++ start logging ");

        }
        else
        {
#ifdef DEBUG
            printf("xpti failed to open log file for writing\n");
#endif
            if (hFile != NIL_RTFILE)
                RTFileClose(hFile);
            hFile = NIL_RTFILE;
        }
    }
}

xptiAutoLog::~xptiAutoLog()
{
    MOZ_COUNT_DTOR(xptiAutoLog);

    if(mMgr)
    {
        PRTSTREAM fd = mMgr->SetOpenLogFile(mOldFileDesc);
        NS_ASSERTION(fd == m_DEBUG_FileDesc, "bad unravel");
        if(fd)
        {
            WriteTimestamp(fd, "---- end logging   ");
            RTStrmClose(fd);
        }
    }
}

void xptiAutoLog::WriteTimestamp(PRTSTREAM pStream, const char* msg)
{
    RTTIMESPEC TimeSpec;
    RTTimeLocalNow(&TimeSpec);

    RTTIME Time;
    RTTimeExplode(&Time, &TimeSpec);
    RTTimeNormalize(&Time);
    char time[128];
    RTTimeToString(&Time, time, sizeof(time));
    RTStrmPrintf(pStream, "\n%s %s\n\n", msg, time);
}

/***************************************************************************/

nsresult 
xptiCloneLocalFile(nsILocalFile*  aLocalFile,
                   nsILocalFile** aCloneLocalFile)
{
    nsresult rv;
    nsCOMPtr<nsIFile> cloneRaw;
 
    rv = aLocalFile->Clone(getter_AddRefs(cloneRaw));
    if(NS_FAILED(rv))
        return rv;

    return CallQueryInterface(cloneRaw, aCloneLocalFile);
}                        


nsresult 
xptiCloneElementAsLocalFile(nsISupportsArray* aArray, PRUint32 aIndex,
                            nsILocalFile** aLocalFile)
{
    nsresult rv;
    nsCOMPtr<nsILocalFile> original;

    rv = aArray->QueryElementAt(aIndex, NS_GET_IID(nsILocalFile), 
                                getter_AddRefs(original));
    if(NS_FAILED(rv))
        return rv;

    return xptiCloneLocalFile(original, aLocalFile);
}       
