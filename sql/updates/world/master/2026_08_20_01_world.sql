-- Chromie Time terrain: restore STOCK quest/faction/level gates on 5 terrain swaps
-- (1190, 1066, 1074, 736, 719) that 2026_08_09_20_world.sql silently dropped.
-- ============================================================================
-- Root cause (same class of bug fixed for Silithus 1817 in 2026_08_20_00_world.sql):
--   The chromie world data (2026_03_06_05 -> 2026_08_09_20) re-gated each terrain swap
--   with `DELETE FROM conditions WHERE SourceTypeOrReferenceId=25 AND SourceEntry=<map>`
--   followed by INSERTing ONLY the chromie NegativeCondition rows. That DELETE also wiped
--   the STOCK gate (faction / quest-state / PlayerCondition / level) that TC ships for
--   these swaps, so post-chromie the swap fired for the wrong faction / quest progress.
--   This file restores each stock gate AND keeps the chromie suppression, ANDed together.
--
-- Condition model on this branch (unchanged from 2026_08_09_20 / 2026_08_20_00):
--   SourceTypeOrReferenceId=25 = CONDITION_SOURCE_TYPE_TERRAIN_SWAP, SourceEntry = swap map.
--   Rows in the SAME ElseGroup are ANDed; different ElseGroups are ORed (swap applies if
--   ANY ElseGroup is fully satisfied).
--   CONDITION_CHROMIE_TIME=60, comparand = ActivePlayerData::UiChromieTimeExpansionID
--   (UiChromieTimeExpansionInfo.ID DB2 record id): Cata=5, TBC=6, WotLK=7, MoP=8, WoD=9,
--   Legion=10. NegativeCondition=1 => "NOT in that timeline". The chromie negatives are the
--   authoritative record ids from 2026_08_09_20 and are copied VERBATIM here.
--
--   Every stock gate below lives entirely in ElseGroup 0 (a single AND-group). The chromie
--   negatives are added to that SAME ElseGroup 0, so they AND in without breaking any OR.
--   None of these five stock gates used multiple ElseGroups, so no per-group duplication of
--   the negatives is required. (The only OR present is INSIDE condition reference 40002 for
--   map 1074, which is evaluated as a single unit from the swap row's point of view, so
--   AND-ing the negatives at ElseGroup 0 does not disturb that reference's internal OR.)
--
-- 15-column format matches 2026_08_20_00_world.sql. Stock rows recovered from sql/old were
-- 16-column (they carried ConditionStringValue1); that empty string column is dropped here.
--
-- This file applies AFTER 2026_08_09_20 and 2026_08_20_00 (idempotent DELETE+INSERT) and
-- supersedes the 1190/1066/1074/736/719 rows from BOTH 2026_03_06_05 (wrong record ids 1/2/3)
-- and 2026_08_09_20 (correct ids, but stock gate missing).
--
-- Condition reference 40002 (used by map 1074) is NOT re-added: the chromie migrations only
-- ever touched SourceTypeOrReferenceId=25 and =15, never the -40002 reference definition
-- (defined by sql/old/11.x/.../2024_10_27_00_world.sql: quests 29611|29612|49538|49852|60126
-- rewarded, ORed across ElseGroups 0-4). It remains intact in the world DB, so the restored
-- 1074 row can reference it unchanged.
-- ============================================================================

-- ----------------------------------------------------------------------------
-- Blasted Lands WoD Iron-Horde terrain (1190)
--   Stock gate (sql/old/10.x/world/23071_2023_10_05/2023_08_14_01_world.sql:66-67, newest):
--     quest 66560 not taken (CONDITION_QUESTSTATE 47, val2=1=QUEST_STATE_NONE)
--     AND PlayerCondition 81408 satisfied (CONDITION_PLAYER_CONDITION 56).
--   Chromie suppression kept (2026_08_09_20): not active in Cata/TBC/WotLK CT.
--   Combined: swap only when (quest 66560 not taken) AND (PlayerCondition 81408)
--             AND not in Cata/TBC/WotLK Chromie Time.
-- ----------------------------------------------------------------------------
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1190;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- restored stock gate
(25,0,1190,0,0,47,0,66560,1,0,0,0,0,'','Blasted Lands WoD terrain: only if quest 66560 not taken'),
(25,0,1190,0,0,56,0,81408,0,0,0,0,0,'','Blasted Lands WoD terrain: only if PlayerCondition 81408 satisfied'),
-- kept chromie suppression
(25,0,1190,0,0,60,0,6,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in TBC CT'),
(25,0,1190,0,0,60,0,7,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in WotLK CT'),
(25,0,1190,0,0,60,0,5,0,0,1,0,0,'','Blasted Lands WoD terrain: not active in Cata CT');

-- ----------------------------------------------------------------------------
-- Stormwind Gunship Pandaria Start (1066)
--   Stock gate (sql/old/11.x/world/24121_2025_03_29/2025_01_02_25_world.sql:137-138, newest):
--     Alliance (CONDITION_TEAM 6, val1=469)
--     AND quest 29548 taken (CONDITION_QUESTSTATE 47, val2=8=QUEST_STATE_INCOMPLETE, positive).
--   Chromie suppression kept (2026_08_09_20): not active in Cata/TBC/WotLK CT.
--   Combined: swap only for Alliance who have taken quest 29548 AND not in pre-MoP Chromie Time.
-- ----------------------------------------------------------------------------
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1066;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- restored stock gate
(25,0,1066,0,0,6,0,469,0,0,0,0,0,'','SW Gunship MoP terrain: only for Alliance'),
(25,0,1066,0,0,47,0,29548,8,0,0,0,0,'','SW Gunship MoP terrain: only if quest 29548 taken'),
-- kept chromie suppression
(25,0,1066,0,0,60,0,6,0,0,1,0,0,'','SW Gunship MoP terrain: not active in TBC CT'),
(25,0,1066,0,0,60,0,7,0,0,1,0,0,'','SW Gunship MoP terrain: not active in WotLK CT'),
(25,0,1066,0,0,60,0,5,0,0,1,0,0,'','SW Gunship MoP terrain: not active in Cata CT');

-- ----------------------------------------------------------------------------
-- Orgrimmar Gunship Pandaria Start (1074)
--   Stock gate (sql/old/11.x/world/25031_2025_05_31/2025_05_16_00_world.sql:100-102, newest):
--     Horde (CONDITION_TEAM 6, val1=67)
--     AND quest 29690 not rewarded (CONDITION_QUESTSTATE 47, val2=64=QUEST_STATE_REWARDED, neg=1)
--     AND condition reference 40002 (quests 29611|29612|49538|49852|60126 rewarded, ORed).
--   Chromie suppression kept (2026_08_09_20): not active in Cata/TBC/WotLK CT.
--   Combined: swap only for Horde, quest 29690 not yet rewarded, ref-40002 satisfied,
--             AND not in pre-MoP Chromie Time.
-- ----------------------------------------------------------------------------
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=1074;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- restored stock gate (references condition reference 40002, still defined on branch)
(25,0,1074,0,0,6,0,67,0,0,0,0,0,'','Org Gunship MoP terrain: only for Horde'),
(25,0,1074,0,0,47,0,29690,64,0,1,0,0,'','Org Gunship MoP terrain: only if quest 29690 not rewarded'),
(25,0,1074,0,0,-40002,0,0,0,0,0,0,0,'','Org Gunship MoP terrain: only if condition reference 40002 fulfilled'),
-- kept chromie suppression
(25,0,1074,0,0,60,0,6,0,0,1,0,0,'','Org Gunship MoP terrain: not active in TBC CT'),
(25,0,1074,0,0,60,0,7,0,0,1,0,0,'','Org Gunship MoP terrain: not active in WotLK CT'),
(25,0,1074,0,0,60,0,5,0,0,1,0,0,'','Org Gunship MoP terrain: not active in Cata CT');

-- ----------------------------------------------------------------------------
-- Twilight Highlands Dragonmaw Port (736)
--   NO stock CONDITION_SOURCE_TYPE_TERRAIN_SWAP gate exists in TC for this swap (verified:
--   the only SourceEntry=736 SourceType-25 rows anywhere in the branch SQL are the chromie
--   files themselves; no faction/quest/level gate was ever shipped). Nothing to restore, so
--   only the existing chromie suppression is re-inserted unchanged.
--   Chromie suppression kept (2026_08_09_20): not active in TBC/WotLK CT.
-- ----------------------------------------------------------------------------
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=736;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- no stock gate to restore; chromie suppression only
(25,0,736,0,0,60,0,6,0,0,1,0,0,'','TH Dragonmaw terrain: not active in TBC CT'),
(25,0,736,0,0,60,0,7,0,0,1,0,0,'','TH Dragonmaw terrain: not active in WotLK CT');

-- ----------------------------------------------------------------------------
-- Mount Hyjal default terrain (719)
--   Stock gate (sql/old/6.x/world/01_2015_03_21/2015_03_29_01_world.sql:53, newest):
--     quest 25372 (Aessina's Miracle) not rewarded (CONDITION_QUESTREWARDED 8, neg=1).
--   Chromie suppression kept (2026_08_09_20): not active in TBC/WotLK CT.
--   Combined: swap only when quest 25372 not rewarded AND not in TBC/WotLK Chromie Time.
-- ----------------------------------------------------------------------------
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry`=719;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
-- restored stock gate
(25,0,719,0,0,8,0,25372,0,0,1,0,0,'','Mount Hyjal terrain: only if quest 25372 (Aessina''s Miracle) not rewarded'),
-- kept chromie suppression
(25,0,719,0,0,60,0,6,0,0,1,0,0,'','Hyjal terrain: not active in TBC CT'),
(25,0,719,0,0,60,0,7,0,0,1,0,0,'','Hyjal terrain: not active in WotLK CT');
