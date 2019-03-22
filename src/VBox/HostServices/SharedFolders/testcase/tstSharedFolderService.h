/** @file
 * VBox Shared Folders testcase stub redefinitions.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBSF_TEST_SHARED_FOLDER_SERVICE__H
#define __VBSF_TEST_SHARED_FOLDER_SERVICE__H

/* Grumble... if the coding style let us use the anonymous "struct RTTESTINT *"
 * instead of "PRTTEST" here we wouldn't need to unnecessarily include this. */
#include <iprt/test.h>

void testMappingsQuery(RTTEST hTest);
/* Sub-tests for testMappingsQuery(). */
void testMappingsQuerySimple(RTTEST hTest);
void testMappingsQueryTooFewBuffers(RTTEST hTest);
void testMappingsQueryAutoMount(RTTEST hTest);
void testMappingsQueryArrayWrongSize(RTTEST hTest);

void testMappingsQueryName(RTTEST hTest);
/* Sub-tests for testMappingsQueryName(). */
void testMappingsQueryNameValid(RTTEST hTest);
void testMappingsQueryNameInvalid(RTTEST hTest);
void testMappingsQueryNameBadBuffer(RTTEST hTest);

void testMapFolder(RTTEST hTest);
/* Sub-tests for testMapFolder(). */
void testMapFolderValid(RTTEST hTest);
void testMapFolderInvalid(RTTEST hTest);
void testMapFolderTwice(RTTEST hTest);
void testMapFolderDelimiter(RTTEST hTest);
void testMapFolderCaseSensitive(RTTEST hTest);
void testMapFolderCaseInsensitive(RTTEST hTest);
void testMapFolderBadParameters(RTTEST hTest);

void testUnmapFolder(RTTEST hTest);
/* Sub-tests for testUnmapFolder(). */
void testUnmapFolderValid(RTTEST hTest);
void testUnmapFolderInvalid(RTTEST hTest);
void testUnmapFolderBadParameters(RTTEST hTest);

void testCreate(RTTEST hTest);
/* Sub-tests for testCreate(). */
void testCreateFileSimple(RTTEST hTest);
void testCreateFileSimpleCaseInsensitive(RTTEST hTest);
void testCreateDirSimple(RTTEST hTest);
void testCreateBadParameters(RTTEST hTest);

void testClose(RTTEST hTest);
/* Sub-tests for testClose(). */
void testCloseBadParameters(RTTEST hTest);

void testRead(RTTEST hTest);
/* Sub-tests for testRead(). */
void testReadBadParameters(RTTEST hTest);
void testReadFileSimple(RTTEST hTest);

void testWrite(RTTEST hTest);
/* Sub-tests for testWrite(). */
void testWriteBadParameters(RTTEST hTest);
void testWriteFileSimple(RTTEST hTest);

void testLock(RTTEST hTest);
/* Sub-tests for testLock(). */
void testLockBadParameters(RTTEST hTest);
void testLockFileSimple(RTTEST hTest);

void testFlush(RTTEST hTest);
/* Sub-tests for testFlush(). */
void testFlushBadParameters(RTTEST hTest);
void testFlushFileSimple(RTTEST hTest);

void testDirList(RTTEST hTest);
/* Sub-tests for testDirList(). */
void testDirListBadParameters(RTTEST hTest);
void testDirListEmpty(RTTEST hTest);

void testReadLink(RTTEST hTest);
/* Sub-tests for testReadLink(). */
void testReadLinkBadParameters(RTTEST hTest);

void testFSInfo(RTTEST hTest);
/* Sub-tests for testFSInfo(). */
void testFSInfoBadParameters(RTTEST hTest);
void testFSInfoQuerySetFMode(RTTEST hTest);
void testFSInfoQuerySetDirATime(RTTEST hTest);
void testFSInfoQuerySetFileATime(RTTEST hTest);
void testFSInfoQuerySetEndOfFile(RTTEST hTest);

void testRemove(RTTEST hTest);
/* Sub-tests for testRemove(). */
void testRemoveBadParameters(RTTEST hTest);

void testRename(RTTEST hTest);
/* Sub-tests for testRename(). */
void testRenameBadParameters(RTTEST hTest);

void testSymlink(RTTEST hTest);
/* Sub-tests for testSymlink(). */
void testSymlinkBadParameters(RTTEST hTest);

void testMappingsAdd(RTTEST hTest);
/* Sub-tests for testMappingsAdd(). */
void testMappingsAddBadParameters(RTTEST hTest);

void testMappingsRemove(RTTEST hTest);
/* Sub-tests for testMappingsRemove(). */
void testMappingsRemoveBadParameters(RTTEST hTest);

#if 0  /* Where should this go? */
void testSetStatusLed(RTTEST hTest);
/* Sub-tests for testStatusLed(). */
void testSetStatusLedBadParameters(RTTEST hTest);
#endif

#endif /* __VBSF_TEST_SHARED_FOLDER_SERVICE__H */
