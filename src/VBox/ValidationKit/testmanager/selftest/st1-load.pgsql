-- $Id$
--- @file
-- VBox Test Manager - Self Test #1 Database Load File.
--

--
-- Copyright (C) 2012-2017 Oracle Corporation
--
-- This file is part of VirtualBox Open Source Edition (OSE), as
-- available from http://www.virtualbox.org. This file is free software;
-- you can redistribute it and/or modify it under the terms of the GNU
-- General Public License (GPL) as published by the Free Software
-- Foundation, in version 2 as it comes in the "COPYING" file of the
-- VirtualBox OSE distribution. VirtualBox OSE is distributed in the
-- hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
--
-- The contents of this file may alternatively be used under the terms
-- of the Common Development and Distribution License Version 1.0
-- (CDDL) only, as it comes in the "COPYING.CDDL" file of the
-- VirtualBox OSE distribution, in which case the provisions of the
-- CDDL are applicable instead of those of the GPL.
--
-- You may elect to license modified versions of this file under the
-- terms and conditions of either the GPL or the CDDL or both.
--



\set ON_ERROR_STOP 1
\connect testmanager;

BEGIN WORK;


INSERT INTO Users (uid, sUsername, sEmail, sFullName, sLoginName)
    VALUES (1112223331, 'st1', 'st1@example.org', 'self test #1', 'st1');

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test1', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest1.py', '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');

INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test1'), 1112223331, '');

INSERT INTO TestGroups (uidAuthor, sName)
    VALUES (1112223331, 'st1-testgroup');

INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test1'),
            1112223331);

INSERT INTO BuildSources (uidAuthor, sName, sProduct, sBranch, asTypes, asOsArches)
    VALUES (1112223331, 'st1-src', 'st1', 'trunk',
            ARRAY['release', 'strict'],
            ARRAY['win.x86', 'linux.noarch', 'solaris.amd64', 'os-agnostic.sparc64', 'os-agnostic.noarch']);

INSERT INTO BuildCategories (sProduct, sBranch, sType, asOsArches)
    VALUES ('st1', 'trunk', 'release', ARRAY['os-agnostic.noarch']);

INSERT INTO Builds (uidAuthor, idBuildCategory, iRevision, sVersion, sBinaries)
    VALUES (1112223331,
            (SELECT idBuildCategory FROM BuildCategories WHERE sProduct = 'st1' AND sBranch = 'trunk'),
            1234, '1.0', '');

INSERT INTO SchedGroups (uidAuthor, sName, sDescription, fEnabled, idBuildSrc)
    VALUES (1112223331, 'st1-group', 'test test #1', TRUE,
            (SELECT idBuildSrc   FROM BuildSources WHERE sName = 'st1-src') );

INSERT INTO SchedGroupMembers (idSchedGroup, idTestGroup, uidAuthor)
    VALUES ((SELECT idSchedGroup FROM SchedGroups  WHERE sName = 'st1-group'),
            (SELECT idTestGroup  FROM TestGroups   WHERE sName = 'st1-testgroup'),
            1112223331);


-- The second test

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test2', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest2.py', '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');

INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test2'), 1112223331, '');

INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test2'),
            1112223331);

-- The third test

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test3', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest3.py', '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');

INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test3'), 1112223331, '');

INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test3'),
            1112223331);

-- The fourth thru eight tests

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test4-neg', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest4.py --test immediate-sub-tests',
            '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test4-neg'), 1112223331, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test4-neg'),
            1112223331);

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test5-neg', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest4.py --test total-sub-tests',
            '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test5-neg'), 1112223331, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test5-neg'),
            1112223331);

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test6-neg', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest4.py --test immediate-values',
            '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test6-neg'), 1112223331, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test6-neg'),
            1112223331);

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test7-neg', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest4.py --test total-values',
            '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test7-neg'), 1112223331, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test7-neg'),
            1112223331);

INSERT INTO TestCases (uidAuthor, sName, fEnabled, cSecTimeout, sBaseCmd, sTestSuiteZips)
    VALUES (1112223331, 'st1-test8-neg', TRUE, 3600,  'validationkit/tests/selftests/tdSelfTest4.py --test immediate-messages',
            '@DOWNLOAD_BASE_URL@/VBoxValidationKit.zip');
INSERT INTO TestCaseArgs (idTestCase, uidAuthor, sArgs)
    VALUES ((SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test8-neg'), 1112223331, '');
INSERT INTO TestGroupMembers (idTestGroup, idTestCase, uidAuthor)
    VALUES ((SELECT idTestGroup FROM TestGroups WHERE sName = 'st1-testgroup'),
            (SELECT idTestCase  FROM TestCases  WHERE sName = 'st1-test8-neg'),
            1112223331);

COMMIT WORK;

