--
-- Arathi Returning Player Experience - scale the keep gnolls.
--
-- They shipped with ContentTuningID 0, which collapses SelectLevel to level 1 and makes them
-- trivial for any real returning character. Quests 90882-90887 use ContentTuningID 4306.
--
-- UNVERIFIED: creatures 244669 / 244670 / 244671 / 244672, ContentTuningID 4306.
--

UPDATE `creature_template_difficulty`
SET `ContentTuningID`=4306
WHERE `Entry` IN (244669, 244670, 244671, 244672)
  AND `DifficultyID`=0
  AND `ContentTuningID`=0;
