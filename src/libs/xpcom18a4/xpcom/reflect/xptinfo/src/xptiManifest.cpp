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

/* Implementation of xptiManifest. */

#include <iprt/file.h>
#include <iprt/stream.h>

#include "xptiprivate.h"
#include "nsManifestLineReader.h"
#include "nsString.h"

static const char g_Disclaimer[] = "# Generated file. ** DO NOT EDIT! **";

static const char g_TOKEN_Files[]          = "Files";
static const char g_TOKEN_ArchiveItems[]   = "ArchiveItems";
static const char g_TOKEN_Interfaces[]     = "Interfaces";
static const char g_TOKEN_Header[]         = "Header";
static const char g_TOKEN_Version[]        = "Version";
static const char g_TOKEN_AppDir[]         = "AppDir";
static const char g_TOKEN_Directories[]    = "Directories";

static const int  g_VERSION_MAJOR          = 2;
static const int  g_VERSION_MINOR          = 0;

/***************************************************************************/

static PRBool
GetCurrentAppDirString(xptiInterfaceInfoManager* aMgr, nsACString &aStr)
{
    nsCOMPtr<nsILocalFile> appDir;
    aMgr->GetApplicationDir(getter_AddRefs(appDir));
    if(appDir)
        return NS_SUCCEEDED(appDir->GetPersistentDescriptor(aStr));
    return PR_FALSE;
}

static PRBool
CurrentAppDirMatchesPersistentDescriptor(xptiInterfaceInfoManager* aMgr,
                                         const char *inStr)
{
    nsCOMPtr<nsILocalFile> appDir;
    aMgr->GetApplicationDir(getter_AddRefs(appDir));

    nsCOMPtr<nsILocalFile> descDir;
    nsresult rv = NS_NewNativeLocalFile(EmptyCString(), PR_FALSE, getter_AddRefs(descDir));
    if(NS_FAILED(rv))
        return PR_FALSE;

    rv = descDir->SetPersistentDescriptor(nsDependentCString(inStr));
    if(NS_FAILED(rv))
        return PR_FALSE;

    PRBool matches;
    rv = appDir->Equals(descDir, &matches);
    return NS_SUCCEEDED(rv) && matches;
}

PR_STATIC_CALLBACK(PLDHashOperator)
xpti_InterfaceWriter(PLDHashTable *table, PLDHashEntryHdr *hdr,
                     PRUint32 number, void *arg)
{
    xptiInterfaceEntry* entry = ((xptiHashEntry*)hdr)->value;
    PRTSTREAM fd = (PRTSTREAM)  arg;

    char* iidStr = entry->GetTheIID()->ToString();
    if(!iidStr)
        return PL_DHASH_STOP;

    const xptiTypelib& typelib = entry->GetTypelibRecord();

    PRBool success =  RTStrmPrintf(fd, "%d,%s,%s,%d,%d,%d\n",
                                   (int) number,
                                   entry->GetTheName(),
                                   iidStr,
                                   (int) typelib.GetFileIndex(),
                                   (int) (typelib.IsZip() ?
                                   typelib.GetZipItemIndex() : -1),
                                   (int) entry->GetScriptableFlag()) > 0;

    nsCRT::free(iidStr);

    return success ? PL_DHASH_NEXT : PL_DHASH_STOP;
}


// static
PRBool xptiManifest::Write(xptiInterfaceInfoManager* aMgr,
                           xptiWorkingSet*           aWorkingSet)
{

    PRBool succeeded = PR_FALSE;
    PRUint32 i;
    PRUint32 size32;
    PRIntn interfaceCount = 0;
    nsCAutoString appDirString;

    nsCOMPtr<nsILocalFile> tempFile;
    if(!aMgr->GetCloneOfManifestLocation(getter_AddRefs(tempFile)) || !tempFile)
        return PR_FALSE;

    nsCAutoString originalLeafName;
    tempFile->GetNativeLeafName(originalLeafName);

    nsCAutoString leafName;
    leafName.Assign(originalLeafName + NS_LITERAL_CSTRING(".tmp"));

    tempFile->SetNativeLeafName(leafName);

    nsCAutoString pathName;
    nsresult rv = tempFile->GetNativePath(pathName);
    if (NS_FAILED(rv))
        return PR_FALSE;

    // All exits via "goto out;" from here on...
    RTFILE hFile      = NIL_RTFILE;
    PRTSTREAM pStream = NULL;
    int vrc = RTFileOpen(&hFile, pathName.get(),
                         RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE
                         | (0600 << RTFILE_O_CREATE_MODE_SHIFT));
    if (RT_SUCCESS(vrc))
    {
        vrc = RTStrmOpenFileHandle(hFile, "at", 0 /*fFlags*/, &pStream);
        if (RT_FAILURE(vrc))
        {
            RTFileClose(hFile);
            hFile = NIL_RTFILE;
            goto out;
        }
    }
    else
        goto out;

    // write file header comments

    if(RTStrmPrintf(pStream, "%s\n", g_Disclaimer) <= 0)
        goto out;

    // write the [Header] block, version number, and appdir.

    if(RTStrmPrintf(pStream, "\n[%s,%d]\n", g_TOKEN_Header, 2) <= 0)
        goto out;

    if(RTStrmPrintf(pStream, "%d,%s,%d,%d\n",
                       0, g_TOKEN_Version, g_VERSION_MAJOR, g_VERSION_MINOR) <= 0)
        goto out;

    GetCurrentAppDirString(aMgr, appDirString);
    if(appDirString.IsEmpty())
        goto out;

    if(RTStrmPrintf(pStream, "%d,%s,%s\n",
                       1, g_TOKEN_AppDir, appDirString.get()) <= 0)
        goto out;

    // write Directories list

    if(RTStrmPrintf(pStream, "\n[%s,%d]\n",
                       g_TOKEN_Directories,
                       (int) aWorkingSet->GetDirectoryCount()) <= 0)
        goto out;

    for(i = 0; i < aWorkingSet->GetDirectoryCount(); i++)
    {
        nsCOMPtr<nsILocalFile> dir;
        nsCAutoString str;

        aWorkingSet->GetDirectoryAt(i, getter_AddRefs(dir));
        if(!dir)
            goto out;

        dir->GetPersistentDescriptor(str);
        if(str.IsEmpty())
            goto out;

        if(RTStrmPrintf(pStream, "%d,%s\n", (int) i, str.get()) <= 0)
            goto out;
    }

    // write Files list

    if(RTStrmPrintf(pStream, "\n[%s,%d]\n",
                       g_TOKEN_Files,
                       (int) aWorkingSet->GetFileCount()) <= 0)
        goto out;

    for(i = 0; i < aWorkingSet->GetFileCount(); i++)
    {
        const xptiFile& file = aWorkingSet->GetFileAt(i);

        LL_L2UI(size32, file.GetSize());

        if(RTStrmPrintf(pStream, "%d,%s,%d,%u,%lld\n",
                           (int) i,
                           file.GetName(),
                           (int) file.GetDirectory(),
                           size32, PRInt64(file.GetDate())) <= 0)
        goto out;
    }

    // write ArchiveItems list

    if(RTStrmPrintf(pStream, "\n[%s,%d]\n",
                       g_TOKEN_ArchiveItems,
                       (int) aWorkingSet->GetZipItemCount()) <= 0)
        goto out;

    for(i = 0; i < aWorkingSet->GetZipItemCount(); i++)
    {
        if(RTStrmPrintf(pStream, "%d,%s\n",
                           (int) i,
                           aWorkingSet->GetZipItemAt(i).GetName()) <= 0)
        goto out;
    }

    // write the Interfaces list

    interfaceCount = aWorkingSet->mNameTable->entryCount;

    if(RTStrmPrintf(pStream, "\n[%s,%d]\n",
                       g_TOKEN_Interfaces,
                       (int) interfaceCount) <= 0)
        goto out;

    if(interfaceCount != (PRIntn)
        PL_DHashTableEnumerate(aWorkingSet->mNameTable,
                               xpti_InterfaceWriter, pStream))
        goto out;


    if (RT_SUCCESS(RTStrmClose(pStream)))
    {
        succeeded = PR_TRUE;
    }
    pStream = NULL;

out:
    if (pStream)
        RTStrmClose(pStream);

    if(succeeded)
    {
        // delete the old file and rename this
        nsCOMPtr<nsILocalFile> mainFile;
        if(!aMgr->GetCloneOfManifestLocation(getter_AddRefs(mainFile)) || !mainFile)
            return PR_FALSE;

        PRBool exists;
        if(NS_FAILED(mainFile->Exists(&exists)))
            return PR_FALSE;

        if(exists && NS_FAILED(mainFile->Remove(PR_FALSE)))
            return PR_FALSE;

        nsCOMPtr<nsIFile> parent;
        mainFile->GetParent(getter_AddRefs(parent));

        // MoveTo means rename.
        if(NS_FAILED(tempFile->MoveToNative(parent, originalLeafName)))
            return PR_FALSE;
    }

    return succeeded;
}

/***************************************************************************/
/***************************************************************************/

static char*
ReadManifestIntoMemory(xptiInterfaceInfoManager* aMgr,
                       PRUint32* pLength)
{
    PRInt32 flen;
    PRInt64 fileSize;
    char* whole = nsnull;
    PRBool success = PR_FALSE;

    nsCOMPtr<nsILocalFile> aFile;
    if(!aMgr->GetCloneOfManifestLocation(getter_AddRefs(aFile)) || !aFile)
        return nsnull;

#ifdef DEBUG
    {
        static PRBool shown = PR_FALSE;

        nsCAutoString path;
        if(!shown && NS_SUCCEEDED(aFile->GetNativePath(path)) && !path.IsEmpty())
        {
            fprintf(stderr, "Type Manifest File: %s\n", path.get());
            shown = PR_TRUE;
        }
    }
#endif

    if(NS_FAILED(aFile->GetFileSize(&fileSize)) || !(flen = nsInt64(fileSize)))
        return nsnull;

    whole = new char[flen];
    if (!whole)
        return nsnull;

    // All exits from on here should be via 'goto out'
    int vrc = VINF_SUCCESS;
    size_t cbRead = 0;
    RTFILE hFile = NIL_RTFILE;
    nsCAutoString pathName;
    if (NS_FAILED(aFile->GetNativePath(pathName)))
        goto out;

    vrc = RTFileOpen(&hFile, pathName.get(),
                     RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(vrc))
        goto out;

    vrc = RTFileRead(hFile, whole, flen, &cbRead);
    if(RT_FAILURE(vrc) || cbRead < flen)
        goto out;

    success = PR_TRUE;

 out:
    if (hFile != NIL_RTFILE)
        RTFileClose(hFile);

    if(!success)
    {
        delete [] whole;
        return nsnull;
    }

    *pLength = flen;
    return whole;
}

static
PRBool ReadSectionHeader(nsManifestLineReader& reader,
                         const char *token, int minCount, int* count)
{
    while(1)
    {
        if(!reader.NextLine())
            break;
        if(*reader.LinePtr() == '[')
        {
            char* p = reader.LinePtr() + (reader.LineLength() - 1);
            if(*p != ']')
                break;
            *p = 0;

            char* values[2];
            int lengths[2];
            if(2 != reader.ParseLine(values, lengths, 2))
                break;

            // ignore the leading '['
            if(0 != RTStrCmp(values[0]+1, token))
                break;

            if((*count = atoi(values[1])) < minCount)
                break;

            return PR_TRUE;
        }
    }
    return PR_FALSE;
}


// static
PRBool xptiManifest::Read(xptiInterfaceInfoManager* aMgr,
                          xptiWorkingSet*           aWorkingSet)
{
    int i;
    char* whole = nsnull;
    PRBool succeeded = PR_FALSE;
    PRUint32 flen = 0;
    nsManifestLineReader reader;
    xptiHashEntry* hashEntry;
    int headerCount = 0;
    int dirCount = 0;
    int fileCount = 0;
    int zipItemCount = -1;
    int interfaceCount = 0;
    int dir;
    int flags;
    char* values[6];    // 6 is currently the max items we need to parse
    int lengths[6];
    PRUint32 size32;
    PRInt64 size;
    PRInt64 date;

    whole = ReadManifestIntoMemory(aMgr, &flen);
    if(!whole)
        return PR_FALSE;

    reader.Init(whole, flen);

    // All exits from here on should be via 'goto out'

    // Look for "Header" section

    // This version accepts only version 1,0. We also freak if the header
    // has more than one entry. The rationale is that we want to force an
    // autoreg if the xpti.dat file was written by *any* other version of
    // the software. Future versions may wish to support updating older
    // manifests in some interesting way.

    if(!ReadSectionHeader(reader, g_TOKEN_Header, 2, &headerCount))
        goto out;

    if(headerCount != 2)
        goto out;

    // Verify the version number

    if(!reader.NextLine())
        goto out;

    // index,VersionLiteral,major,minor
    if(4 != reader.ParseLine(values, lengths, 4))
        goto out;

    // index
    if(0 != atoi(values[0]))
        goto out;

    // VersionLiteral
    if(0 != RTStrCmp(values[1], g_TOKEN_Version))
        goto out;

    // major
    if(g_VERSION_MAJOR != atoi(values[2]))
        goto out;

    // minor
    if(g_VERSION_MINOR != atoi(values[3]))
        goto out;

    // Verify the application directory

    if(!reader.NextLine())
        goto out;

    // index,AppDirLiteral,directoryname
    if(3 != reader.ParseLine(values, lengths, 3))
        goto out;

    // index
    if(1 != atoi(values[0]))
        goto out;

    // AppDirLiteral
    if(0 != RTStrCmp(values[1], g_TOKEN_AppDir))
        goto out;

    if(!CurrentAppDirMatchesPersistentDescriptor(aMgr, values[2]))
        goto out;

    // Look for "Directories" section

    if(!ReadSectionHeader(reader, g_TOKEN_Directories, 1, &dirCount))
        goto out;
    else
    {
        // To validate that the directory list matches the current search path
        // we first confirm that the list lengths match.

        nsCOMPtr<nsISupportsArray> searchPath;
        aMgr->GetSearchPath(getter_AddRefs(searchPath));

        PRUint32 searchPathCount;
        searchPath->Count(&searchPathCount);

        if(dirCount != (int) searchPathCount)
            goto out;
    }

    // Read the directory records

    for(i = 0; i < dirCount; ++i)
    {
        if(!reader.NextLine())
            goto out;

        // index,directoryname
        if(2 != reader.ParseLine(values, lengths, 2))
            goto out;

        // index
        if(i != atoi(values[0]))
            goto out;

        // directoryname
        if(!aWorkingSet->DirectoryAtMatchesPersistentDescriptor(i, values[1]))
            goto out;
    }

    // Look for "Files" section

    if(!ReadSectionHeader(reader, g_TOKEN_Files, 1, &fileCount))
        goto out;


    // Alloc room in the WorkingSet for the filearray.

    if(!aWorkingSet->NewFileArray(fileCount))
        goto out;

    // Read the file records

    for(i = 0; i < fileCount; ++i)
    {
        if(!reader.NextLine())
            goto out;

        // index,filename,dirIndex,dilesSize,filesDate
        if(5 != reader.ParseLine(values, lengths, 5))
            goto out;

        // index
        if(i != atoi(values[0]))
            goto out;

        // filename
        if(!*values[1])
            goto out;

        // dirIndex
        dir = atoi(values[2]);
        if(dir < 0 || dir > dirCount)
            goto out;

        // fileSize
        size32 = atoi(values[3]);
        if(size32 <= 0)
            goto out;
        LL_UI2L(size, size32);

        // fileDate
        date = nsCRT::atoll(values[4]);
        if(LL_IS_ZERO(date))
            goto out;

        // Append a new file record to the array.

        aWorkingSet->AppendFile(
            xptiFile(nsInt64(size), nsInt64(date), dir, values[1], aWorkingSet));
    }

    // Look for "ZipItems" section

    if(!ReadSectionHeader(reader, g_TOKEN_ArchiveItems, 0, &zipItemCount))
        goto out;

    // Alloc room in the WorkingSet for the zipItemarray.

    if(zipItemCount)
        if(!aWorkingSet->NewZipItemArray(zipItemCount))
            goto out;

    // Read the zipItem records

    for(i = 0; i < zipItemCount; ++i)
    {
        if(!reader.NextLine())
            goto out;

        // index,filename
        if(2 != reader.ParseLine(values, lengths, 2))
            goto out;

        // index
        if(i != atoi(values[0]))
            goto out;

        // filename
        if(!*values[1])
            goto out;

        // Append a new zipItem record to the array.

        aWorkingSet->AppendZipItem(xptiZipItem(values[1], aWorkingSet));
    }

    // Look for "Interfaces" section

    if(!ReadSectionHeader(reader, g_TOKEN_Interfaces, 1, &interfaceCount))
        goto out;

    // Read the interface records

    for(i = 0; i < interfaceCount; ++i)
    {
        int fileIndex;
        int zipItemIndex;
        nsIID iid;
        xptiInterfaceEntry* entry;
        xptiTypelib typelibRecord;

        if(!reader.NextLine())
            goto out;

        // index,interfaceName,iid,fileIndex,zipIndex,flags
        if(6 != reader.ParseLine(values, lengths, 6))
            goto out;

        // index
        if(i != atoi(values[0]))
            goto out;

        // interfaceName
        if(!*values[1])
            goto out;

        // iid
        if(!iid.Parse(values[2]))
            goto out;

        // fileIndex
        fileIndex = atoi(values[3]);
        if(fileIndex < 0 || fileIndex >= fileCount)
            goto out;

        // zipIndex (NOTE: -1 is a valid value)
        zipItemIndex = atoi(values[4]);
        if(zipItemIndex < -1 || zipItemIndex >= zipItemCount)
            goto out;

        // flags
        flags = atoi(values[5]);
        if(flags != 0 && flags != 1)
            goto out;

        // Build an InterfaceInfo and hook it in.

        if(zipItemIndex == -1)
            typelibRecord.Init(fileIndex);
        else
            typelibRecord.Init(fileIndex, zipItemIndex);

        entry = xptiInterfaceEntry::NewEntry(values[1], lengths[1],
                                             iid, typelibRecord,
                                             aWorkingSet);
        if(!entry)
            goto out;

        entry->SetScriptableFlag(flags==1);

        // Add our entry to the iid hashtable.

        hashEntry = (xptiHashEntry*)
            PL_DHashTableOperate(aWorkingSet->mNameTable,
                                 entry->GetTheName(), PL_DHASH_ADD);
        if(hashEntry)
            hashEntry->value = entry;

        // Add our entry to the name hashtable.

        hashEntry = (xptiHashEntry*)
            PL_DHashTableOperate(aWorkingSet->mIIDTable,
                                 entry->GetTheIID(), PL_DHASH_ADD);
        if(hashEntry)
            hashEntry->value = entry;
    }

    // success!

    succeeded = PR_TRUE;

 out:
    if(whole)
        delete [] whole;

    if(!succeeded)
    {
        // Cleanup the WorkingSet on failure.
        aWorkingSet->InvalidateInterfaceInfos();
        aWorkingSet->ClearHashTables();
        aWorkingSet->ClearFiles();
    }
    return succeeded;
}

// static
PRBool xptiManifest::Delete(xptiInterfaceInfoManager* aMgr)
{
    nsCOMPtr<nsILocalFile> aFile;
    if(!aMgr->GetCloneOfManifestLocation(getter_AddRefs(aFile)) || !aFile)
        return PR_FALSE;

    PRBool exists;
    if(NS_FAILED(aFile->Exists(&exists)))
        return PR_FALSE;

    if(exists && NS_FAILED(aFile->Remove(PR_FALSE)))
        return PR_FALSE;

    return PR_TRUE;
}

