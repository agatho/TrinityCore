-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr intro 64864: retail 12.0.7 sniff has QuestFlags[2] = 8 (NOT_REPLAYABLE).
UPDATE `quest_template` SET `FlagsEx2` = `FlagsEx2` | 8 WHERE `ID` = 64864 AND (`FlagsEx2` & 8) = 0;
