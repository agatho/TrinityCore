-- ============================================================================
-- Arathi Catch-Up / RPE -- quest-chain audit fixes  (2026-08-22)
-- ============================================================================
-- Swept every objective on the whole 908xx/909xx chain for the fabricated-duplicate pattern that has
-- already been fixed piecemeal on 90882/90883/90885/90886/90887/90895 (ids in the 90NNN00 range,
-- authored alongside the real Blizzard objectives and colliding with them at the same StorageIndex).
-- Three more quests were affected, one of them a hard blocker on the rest of the questline.

-- (1) 90893 "Repelling the Siege" -- TEN fabricated objectives (9089300-9089390), every one Amount=100,
--     colliding with the real objectives at StorageIndex 0/1/2/3/4 and inventing 5-9 on top. In game
--     that is ~15 tasks, most of them reading "0/100". The real Blizzard set is 461752 (progress bar)
--     + 461753/461754/461755/461881 (kills) + 461756 (gameobject); keep exactly those.
DELETE FROM `quest_objectives` WHERE `ID` IN
 (9089300,9089310,9089320,9089330,9089340,9089350,9089360,9089370,9089380,9089390);

-- (2) 90896 "One Last Ogre" -- fabricated 9089600 duplicates the real 461768 (both ObjectID 244709,
--     Ro'grok) at a different StorageIndex, so the single boss kill showed as two tasks.
DELETE FROM `quest_objectives` WHERE `ID`=9089600;

-- (3) 90888 "Saving Stromgarde Keep" -- BLOCKER. Its ONLY objective was a fabricated 9088800 of type
--     QUEST_OBJECTIVE_AREATRIGGER (10) with ObjectID=0, i.e. an areatrigger objective wired to no
--     areatrigger at all. It can never be satisfied, so the quest can never complete and the entire
--     chain past Go'shek Farm is stuck. (This is the long-standing "90883/90888 travel objectives need
--     a real areatrigger id" gap; 90883 was already solved a different way, via the mount kill-credit
--     239009.) Our own captures record NO objective row for 90888 at all, and its Horde counterpart
--     90898 "Back to Hammerfall" already ships with ZERO objectives -- so a plain travel quest is the
--     blizzlike shape here: it is started by Jaina at the farm (244655, phase 1959) and handed in at
--     Stromgarde (244657/244658, phase 1610). With no objectives TC marks it complete immediately, so
--     the player simply travels and turns it in -- exactly the intended beat, and no invented data.
DELETE FROM `quest_objectives` WHERE `ID`=9088800;

-- (4) 90911 "Your Next Adventure" -- fabricated 9091100 (type QUEST_OBJECTIVE_CRITERIA_TREE 14,
--     ObjectID=0, unwired) sits at the SAME StorageIndex 0 as the real 461829 and carries the same
--     text, so the finale shows "Next Adventure Chosen" twice. Not a hard blocker -- the finale's
--     PlayerChoice handler calls CompleteQuest() directly -- but it is the same display bug. The real
--     objective is the kill-credit NPC 244885 "[DNT] Kill Credit:", which is granted programmatically
--     and correctly has no world spawn.
DELETE FROM `quest_objectives` WHERE `ID`=9091100;

-- LEFT ALONE deliberately: 90897 "Back to Stromgarde" keeps 9089700 (type TALKTO 3 -> 244714, the
-- Stromgarde hub Jaina, who is also its ender) -- it is a fabricated id but it is correctly wired and
-- completable, and removing it would leave the quest with no beat at all.
