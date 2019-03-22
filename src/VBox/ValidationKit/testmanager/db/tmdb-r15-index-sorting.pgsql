-- $Id$
--- @file
-- VBox Test Manager Database - Index tuning effort.
--

--
-- Copyright (C) 2015-2017 Oracle Corporation
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


--
-- Reordered, modified and new indexes.
--
\d UsersLoginNameIdx;
DROP   INDEX UsersLoginNameIdx;
CREATE INDEX UsersLoginNameIdx ON Users (sLoginName, tsExpire DESC);
\d UsersLoginNameIdx;
ANALYZE VERBOSE Users;


\d TestCaseArgsLookupIdx;
DROP   INDEX TestCaseArgsLookupIdx;
CREATE INDEX TestCaseArgsLookupIdx ON TestCaseArgs (idTestCase, tsExpire DESC, tsEffective ASC);
\d TestCaseArgsLookupIdx;
ANALYZE VERBOSE TestCaseArgs;


\d TestGroups_id_index;
DROP   INDEX TestGroups_id_index;
CREATE INDEX TestGroups_id_index ON TestGroups (idTestGroup, tsExpire DESC, tsEffective ASC);
\d TestGroups_id_index;
ANALYZE VERBOSE TestGroups;


\d TestBoxesUuidIdx;
DROP          INDEX TestBoxesUuidIdx;
CREATE UNIQUE INDEX TestBoxesUuidIdx ON TestBoxes (uuidSystem, tsExpire DESC);
\d TestBoxesUuidIdx;
DROP INDEX IF EXISTS TestBoxesExpireEffectiveIdx;
CREATE INDEX TestBoxesExpireEffectiveIdx ON TestBoxes (tsExpire DESC, tsEffective ASC);
\d TestBoxesExpireEffectiveIdx;
ANALYZE VERBOSE TestBoxes;


DROP INDEX IF EXISTS BuildBlacklistIdx;
CREATE INDEX BuildBlacklistIdx ON BuildBlacklist (iLastRevision DESC, iFirstRevision ASC, sProduct, sBranch,
                                                  tsExpire DESC, tsEffective ASC);
\d BuildBlacklist;
ANALYZE VERBOSE BuildBlacklist;


\d TestResultsNameIdx;
DROP INDEX TestResultsNameIdx;
CREATE INDEX TestResultsNameIdx ON TestResults (idStrName, tsCreated DESC);
\d TestResultsNameIdx;
DROP INDEX IF EXISTS TestResultsNameIdx2;
CREATE INDEX TestResultsNameIdx2 ON TestResults (idTestResult, idStrName);
\d TestResultsNameIdx2;
ANALYZE VERBOSE TestResults;


\d TestSetsCreatedDoneIdx;
DROP   INDEX TestSetsCreatedDoneIdx;
DROP INDEX IF EXISTS TestSetsDoneCreatedBuildCatIdx;
CREATE INDEX TestSetsDoneCreatedBuildCatIdx ON TestSets (tsDone DESC NULLS FIRST, tsCreated ASC, idBuildCategory);
\d TestSetsDoneCreatedBuildCatIdx;
\d TestSetsGraphBoxIdx;
DROP   INDEX TestSetsGraphBoxIdx;
CREATE INDEX TestSetsGraphBoxIdx    ON TestSets (idTestBox, tsCreated DESC, tsDone ASC NULLS LAST, idBuildCategory, idTestCase);
\d TestSetsGraphBoxIdx;
ANALYZE VERBOSE TestSets;


DROP INDEX IF EXISTS SchedQueuesItemIdx;
CREATE INDEX SchedQueuesItemIdx          ON SchedQueues(idItem);
\d SchedQueuesItemIdx;
DROP INDEX IF EXISTS SchedQueuesSchedGroupIdx;
CREATE INDEX SchedQueuesSchedGroupIdx    ON SchedQueues(idSchedGroup);
\d SchedQueuesSchedGroupIdx;
ANALYZE VERBOSE SchedQueues;

