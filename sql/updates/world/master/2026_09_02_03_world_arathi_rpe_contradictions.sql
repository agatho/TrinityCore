-- Corrective SQL for feature/arathi-rpe, from the contradiction sweep (tools/contradiction_sweep.py).
-- Each row below is a HAND-VERIFIED disagreement between authored content and captured retail wire
-- data. Every one was confirmed by 2 independent captures unless noted. REVIEW BEFORE APPLYING; per
-- the golden-source rule this belongs on feature/arathi-rpe as a new dated migration, not on the
-- shared realm.

-- 1. LogDescription / QuestDescription are SWAPPED (quests 90882, 90883, 90898).
--    TrinityCore convention: LogDescription = the short objective line in the quest log;
--    QuestDescription = the questgiver's prose. The branch has them inverted. Direction confirmed
--    twice inside this repo (locale_route.py:110-111, wdb/decode/quest.py:118-119), by BOTH the
--    addon and the WDB cache in 2 captures, and by direct inspection of the realm.
--    NOTE on the idiom: MySQL evaluates SET assignments LEFT TO RIGHT and later terms see the
--    ALREADY-UPDATED value, so `SET a = b, b = a` does NOT swap -- it copies b into both. Each
--    quest is therefore read into session variables first.
SELECT `LogDescription`, `QuestDescription` INTO @l, @d FROM `quest_template` WHERE `ID` = 90882;
UPDATE `quest_template` SET `LogDescription` = @d, `QuestDescription` = @l WHERE `ID` = 90882;
SELECT `LogDescription`, `QuestDescription` INTO @l, @d FROM `quest_template` WHERE `ID` = 90883;
UPDATE `quest_template` SET `LogDescription` = @d, `QuestDescription` = @l WHERE `ID` = 90883;
SELECT `LogDescription`, `QuestDescription` INTO @l, @d FROM `quest_template` WHERE `ID` = 90898;
UPDATE `quest_template` SET `LogDescription` = @d, `QuestDescription` = @l WHERE `ID` = 90898;

-- 2. creature_text.Type 0 -> 12 (CHAT_MSG_MONSTER_SAY) for the Gnoll Way send-off lines.
--    The addon derives Type from the real CHAT_MSG_MONSTER_SAY event. Every other authored RPE row
--    uses 12; the realm holds 34924 rows at 12 against 168 at 0. Introduced by
--    2026_08_22_00_world_arathi_rpe_gnoll_way_sendoff.sql:25.
UPDATE `creature_text` SET `Type` = 12 WHERE `CreatureID` IN (244642, 244643) AND `Type` = 0;

-- 3. creature_template 244669 Scavenging Hyena: Classification 0 -> 6.
--    WDB (SMSG_QUERY_CREATURE_RESPONSE) and the addon agree across 2 captures; the realm already
--    holds 3516 rows at 6.
UPDATE `creature_template` SET `Classification` = 6 WHERE `entry` = 244669 AND `Classification` = 0;

-- 4. quest_poi 90882 ObjectiveIndex 0 -> -1.
--    -1 is retail's convention (41766 realm rows use it). The authored file's own comment says
--    "TODO Phase K: refine against a real capture" -- this is that capture. SINGLE-SOURCE evidence.
UPDATE `quest_poi` SET `ObjectiveIndex` = -1
 WHERE `QuestID` = 90882 AND `BlobIndex` = 1 AND `Idx1` = 1 AND `ObjectiveIndex` = 0;

-- NOT INCLUDED, deliberately:
--   * quest_template 90882/90883 RewardNextQuest -> 0. Genuine divergence but INTENTIONAL: retail
--     chains via the questline DB2, and both authored writes are guarded `AND RewardNextQuest=0`.
--   * quest_offer_reward 90897 "Back to Stromgarde<text>" -- a DECODER artifact: the sniff quest
--     channel concatenates the title with no separator (documented in questevents.py). Two captures
--     agreed and were still wrong; the branch text is confirmed correct by two other captures.
--   * quest_offer_reward 90882 Horde-POV text -- a faction coverage gap, not a defect.
