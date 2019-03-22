/* $Id$ */
/** @file
 * IPRT Testcase - Directory listing.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include <iprt/dir.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/path.h>
//#include <iprt/

int main(int argc, char **argv)
{
    int rcRet = 0;
    RTR3InitExe(argc, &argv, 0);

    /*
     * Iterate arguments.
     */
    bool fLong      = false;
    bool fTimes     = false;
    bool fInode     = false;
    bool fShortName = false;
    bool fFiltered  = false;
    bool fQuiet     = false;
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1; argv[i][j]; j++)
            {
                switch (argv[i][j])
                {
                    case 'l':
                        fLong = true;
                        break;
                    case 'i':
                        fLong = fInode = true;
                        break;
                    case 't':
                        fLong = fTimes = true;
                        break;
                    case 's':
                        fLong = fShortName = true;
                        break;
                    case 'f':
                        fFiltered = true;
                        break;
                    case 'q':
                        fQuiet = true;
                        break;
                    default:
                        RTPrintf("Unknown option '%c' ignored!\n", argv[i][j]);
                        break;
                }
            }
        }
        else
        {
            /* open */
            PRTDIR pDir;
            int rc;
            if (!fFiltered)
                rc = RTDirOpen(&pDir, argv[i]);
            else
                rc = RTDirOpenFiltered(&pDir, argv[i], RTDIRFILTER_WINNT, 0);
            if (RT_SUCCESS(rc))
            {
                /* list */
                if (!fLong)
                {
                    for (;;)
                    {
                        RTDIRENTRY DirEntry;
                        rc = RTDirRead(pDir, &DirEntry, NULL);
                        if (RT_FAILURE(rc))
                            break;
                        if (!fQuiet)
                        {
                            switch (DirEntry.enmType)
                            {
                                case RTDIRENTRYTYPE_UNKNOWN:     RTPrintf("u"); break;
                                case RTDIRENTRYTYPE_FIFO:        RTPrintf("f"); break;
                                case RTDIRENTRYTYPE_DEV_CHAR:    RTPrintf("c"); break;
                                case RTDIRENTRYTYPE_DIRECTORY:   RTPrintf("d"); break;
                                case RTDIRENTRYTYPE_DEV_BLOCK:   RTPrintf("b"); break;
                                case RTDIRENTRYTYPE_FILE:        RTPrintf("-"); break;
                                case RTDIRENTRYTYPE_SYMLINK:     RTPrintf("l"); break;
                                case RTDIRENTRYTYPE_SOCKET:      RTPrintf("s"); break;
                                case RTDIRENTRYTYPE_WHITEOUT:    RTPrintf("w"); break;
                                default:
                                    rcRet = 1;
                                    RTPrintf("?");
                                    break;
                            }
                            RTPrintf(" %#18llx  %3d %s\n", (uint64_t)DirEntry.INodeId,
                                     DirEntry.cbName, DirEntry.szName);
                        }
                    }
                }
                else
                {
                    for (;;)
                    {
                        RTDIRENTRYEX DirEntry;
                        rc = RTDirReadEx(pDir, &DirEntry, NULL, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
                        if (RT_FAILURE(rc))
                            break;

                        if (!fQuiet)
                        {
                            RTFMODE fMode = DirEntry.Info.Attr.fMode;
                            switch (fMode & RTFS_TYPE_MASK)
                            {
                                case RTFS_TYPE_FIFO:        RTPrintf("f"); break;
                                case RTFS_TYPE_DEV_CHAR:    RTPrintf("c"); break;
                                case RTFS_TYPE_DIRECTORY:   RTPrintf("d"); break;
                                case RTFS_TYPE_DEV_BLOCK:   RTPrintf("b"); break;
                                case RTFS_TYPE_FILE:        RTPrintf("-"); break;
                                case RTFS_TYPE_SYMLINK:     RTPrintf("l"); break;
                                case RTFS_TYPE_SOCKET:      RTPrintf("s"); break;
                                case RTFS_TYPE_WHITEOUT:    RTPrintf("w"); break;
                                default:
                                    rcRet = 1;
                                    RTPrintf("?");
                                    break;
                            }
                            /** @todo sticy bits++ */
                            RTPrintf("%c%c%c",
                                     fMode & RTFS_UNIX_IRUSR ? 'r' : '-',
                                     fMode & RTFS_UNIX_IWUSR ? 'w' : '-',
                                     fMode & RTFS_UNIX_IXUSR ? 'x' : '-');
                            RTPrintf("%c%c%c",
                                     fMode & RTFS_UNIX_IRGRP ? 'r' : '-',
                                     fMode & RTFS_UNIX_IWGRP ? 'w' : '-',
                                     fMode & RTFS_UNIX_IXGRP ? 'x' : '-');
                            RTPrintf("%c%c%c",
                                     fMode & RTFS_UNIX_IROTH ? 'r' : '-',
                                     fMode & RTFS_UNIX_IWOTH ? 'w' : '-',
                                     fMode & RTFS_UNIX_IXOTH ? 'x' : '-');
                            RTPrintf(" %c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                                     fMode & RTFS_DOS_READONLY          ? 'R' : '-',
                                     fMode & RTFS_DOS_HIDDEN            ? 'H' : '-',
                                     fMode & RTFS_DOS_SYSTEM            ? 'S' : '-',
                                     fMode & RTFS_DOS_DIRECTORY         ? 'D' : '-',
                                     fMode & RTFS_DOS_ARCHIVED          ? 'A' : '-',
                                     fMode & RTFS_DOS_NT_DEVICE         ? 'd' : '-',
                                     fMode & RTFS_DOS_NT_NORMAL         ? 'N' : '-',
                                     fMode & RTFS_DOS_NT_TEMPORARY      ? 'T' : '-',
                                     fMode & RTFS_DOS_NT_SPARSE_FILE    ? 'P' : '-',
                                     fMode & RTFS_DOS_NT_REPARSE_POINT  ? 'J' : '-',
                                     fMode & RTFS_DOS_NT_COMPRESSED     ? 'C' : '-',
                                     fMode & RTFS_DOS_NT_OFFLINE        ? 'O' : '-',
                                     fMode & RTFS_DOS_NT_NOT_CONTENT_INDEXED ? 'I' : '-',
                                     fMode & RTFS_DOS_NT_ENCRYPTED      ? 'E' : '-');
                            RTPrintf(" %d %4d %4d %10lld %10lld",
                                     DirEntry.Info.Attr.u.Unix.cHardlinks,
                                     DirEntry.Info.Attr.u.Unix.uid,
                                     DirEntry.Info.Attr.u.Unix.gid,
                                     DirEntry.Info.cbObject,
                                     DirEntry.Info.cbAllocated);
                            if (fTimes)
                                RTPrintf(" %#llx %#llx %#llx %#llx",
                                         DirEntry.Info.BirthTime,
                                         DirEntry.Info.ChangeTime,
                                         DirEntry.Info.ModificationTime,
                                         DirEntry.Info.AccessTime);

                            if (fInode)
                                RTPrintf(" %#x:%#018llx",
                                         DirEntry.Info.Attr.u.Unix.INodeIdDevice, DirEntry.Info.Attr.u.Unix.INodeId);
                            if (fShortName)
                                RTPrintf(" %2d %-12ls ", DirEntry.cwcShortName, DirEntry.wszShortName);
                            RTPrintf(" %2d %s\n", DirEntry.cbName, DirEntry.szName);
                        }
                        if (rc != VINF_SUCCESS)
                            RTPrintf("^^ %Rrc\n", rc);
                    }
                }

                if (rc != VERR_NO_MORE_FILES)
                {
                    RTPrintf("tstDir: Enumeration failed! rc=%Rrc\n", rc);
                    rcRet = 1;
                }

                /* close up */
                rc = RTDirClose(pDir);
                if (RT_FAILURE(rc))
                {
                    RTPrintf("tstDir: Failed to close dir! rc=%Rrc\n", rc);
                    rcRet = 1;
                }
            }
            else
            {
                RTPrintf("tstDir: Failed to open '%s', rc=%Rrc\n", argv[i], rc);
                rcRet = 1;
            }
        }
    }

    return rcRet;
}
