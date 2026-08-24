-- ============================================================================
-- Omnium Folio unlock + weekly questlines (server-side quest_template content)
-- Midnight 12.0.7 "Revelations" Season 1  |  feature/omnium-folio
-- ============================================================================
-- PURPOSE: seed the DB2-confirmed Omnium quests so the already-built Omnium
--   eligibility engine (keys on achievement 62606) can engage. The client
--   auto-credits achievement 62606 via Criteria 113887 (Type 27 COMPLETE_QUEST,
--   quest 96233); that criterion can only fire if quest 96233 is completable
--   server-side. This file makes 96233 (and its whole chain + the weekly line)
--   EXIST and be turn-in-able.
--
-- EVIDENCE LINE (read the shipped report for the full per-field table):
--   * Quest IDs, questline membership (QuestLine 6275 / 6307) and chain order
--     are DB2-CONFIRMED @68887 (see C:\dumps\GAP_DATAMINE_68974.md).
--   * Titles, givers/enders, objectives, rewards, chain prev/next are
--     WOWHEAD-SOURCED (Midnight beta data, lower confidence, user-approved).
--   * DB-VERIFIED (read-only, integ_world): the ONLY Omnium-line NPCs that
--     exist server-side are Grand Magister Rommath (237504) and Magister
--     Umbric (246025). Every wowhead-datamined giver id (264066/264070/265205/
--     265565/etc.) and every objective entity (GO 265046, items 274576+, the
--     'Omnium Anomaly' creatures, etc.) is NOT seeded in integ_world.
--
-- COMPLETABILITY MODEL: each quest is shipped as a completable talk/auto-
--   complete shell (QuestType 2, ZERO quest_objectives) with a real giver +
--   ender mapped to the existing named NPC. This guarantees the chain is
--   walkable to 96233 so achievement 62606 fires. The REAL wowhead objectives
--   are recorded as TODO comments below; they are NOT shipped as required
--   quest_objectives because every entity they reference is unseeded (a dangling
--   required objective would make the quest uncompletable and 62606 could never
--   fire). Wire them once the referenced creatures/gameobjects/items are seeded.
--
-- REALM SAFETY: ship-only. DO NOT apply to the shared integration realm.
-- VerifiedBuild = 0 on every row => flags this as authored (not sniff-verified).
-- ============================================================================

-- ---- REAL (wowhead) objectives, giver/ender mapping & confidence, per quest ----
-- UNLOCK line 6275 "The Sunstrider Omnium" (order 0..11 -> 96223..96238):
--  96223 The Magisters' Call        giver TODO(blueprint:Umbric) INFERRED->246025 | obj: travel to Thalassian University (areatrigger, no entity)
--  96224 The Magisters' Conundrum   giver Umbric264063/Rommath264066, ender Rommath264066 -> starter/ender 237504 | obj: portal 265005 + meet 265006 (DANGLING)
--  96225 The Magisters' Conundrum   giver Rommath264064->237504, ender Umbric264065->246025 | obj: portal 265005 + meet 265006 (DANGLING)
--  96226 Omnium Anomalies           giver/ender not shown INFERRED->237504 | obj: kill 264481 x3, 264357, 264361, 264309 (DANGLING creatures)
--  96227 Lycaneum Chaos             giver not shown, ender Rommath INFERRED->237504 | obj: 264523 x25, 264525 x13, 264695 x17 (DANGLING)
--  96228 The Shadowed Spire         giver not shown INFERRED->246025, ender Umbric264722->246025 | obj: speak 264722 + anchors 264730/264727/264728/264729 (DANGLING)
--  96229 The Void Reveals           giver not shown INFERRED->246025, ender Umbric->246025 | obj: search 264777 + item 274036 + kill 264791 (DANGLING)
--  96230 Unraveling the Wards       giver Rommath264907->237504, ender not shown INFERRED->237504 | obj: 264912, speak 264907, wards 264860/264888/264889, 265100 (DANGLING)
--  96231 The Grand Magister's Key-Cipher giver not shown INFERRED->237504, ender Rommath265064->237504 | obj: approach 265064 + GO 264914 + item 274261 (DANGLING)
--  96232 Return to the Omnium        giver/ender not shown INFERRED->237504 | obj: portal 265005 + return 265565 (DANGLING)
--  96233 The Omnium Reawakens  ***FIRES ACH 62606*** giver/ender Rommath264066->237504 | obj: speak Rommath + interact GO 265046 'Sunstrider Omnium Activated' (DANGLING GO) | reward currency Voidlight Marl 3316 (qty TODO) + RewardSpell 1302265
--  96238 Return to the Omnium        giver not shown INFERRED->237504, ender 265565(absent) INFERRED->237504 | obj: portal 265005 + return 265565 (DANGLING)
-- WEEKLY line 6307 "The Empowered Folio" (fires achs 62607-62610):
--  96410 Week1 The Omnium Folio       giver Rommath264070->237504, ender Rommath265205->237504 | obj: speak+receive folio+empower rune (flavor) | RewardSpell 1299503 (Simulacrum)
--  96441 Week2 Ritualized Arcana  (ach 62607) giver/ender not shown INFERRED->237504 | obj: collect 8x item 274576 + disrupt ritual site (DANGLING) | reward Voidlight Marl 3316 (qty TODO) + spell 1294322
--  96442 Week3 Ley Line Assaults  (ach 62608) giver/ender not shown INFERRED->237504 | obj: collect 5x item 274577 via Void Assault (DANGLING) | reward 3316 (qty TODO) + spell 1294322
--  96443 Week4 Magical Primessence (ach 62609) giver not shown INFERRED->237504, ender Umbric->246025 | obj: 1x primessence item 274580/274581/274582/274583/274584/274585 (DANGLING) | reward 3316 + spell 1294322
--  96444 Week5 Off-World Magic    (ach 62610) giver not shown INFERRED->237504, ender Rommath->237504 | obj: recover alien-magic fragment (DANGLING) | reward items 274620 + 274640 (DANGLING) + spell 1294322
--  NOTE: weekly-reset flag (QUEST_FLAGS_WEEKLY 0x8000) intentionally NOT set -> shipped one-time so completion reliably fires the achs in test. TODO: add weekly reset once cadence wanted.

DELETE FROM `quest_template` WHERE `ID` IN (96223, 96224, 96225, 96226, 96227, 96228, 96229, 96230, 96231, 96232, 96233, 96238, 96410, 96441, 96442, 96443, 96444);
INSERT INTO `quest_template` (`ID`, `QuestType`, `QuestPackageID`, `ContentTuningID`, `QuestSortID`, `QuestInfoID`, `SuggestedGroupNum`, `RewardNextQuest`, `RewardXPDifficulty`, `RewardXPMultiplier`, `RewardMoneyDifficulty`, `RewardMoneyMultiplier`, `RewardSpell`, `RewardHonor`, `RewardKillHonor`, `RewardFavor`, `StartItem`, `RewardArtifactXPDifficulty`, `RewardArtifactXPMultiplier`, `RewardArtifactCategoryID`, `Flags`, `FlagsEx`, `FlagsEx2`, `FlagsEx3`, `RewardSkillLineID`, `RewardNumSkillUps`, `PortraitGiver`, `PortraitGiverMount`, `PortraitGiverModelSceneID`, `PortraitTurnIn`, `RewardItem1`, `RewardItem2`, `RewardItem3`, `RewardItem4`, `RewardAmount1`, `RewardAmount2`, `RewardAmount3`, `RewardAmount4`, `ItemDrop1`, `ItemDrop2`, `ItemDrop3`, `ItemDrop4`, `ItemDropQuantity1`, `ItemDropQuantity2`, `ItemDropQuantity3`, `ItemDropQuantity4`, `RewardChoiceItemID1`, `RewardChoiceItemID2`, `RewardChoiceItemID3`, `RewardChoiceItemID4`, `RewardChoiceItemID5`, `RewardChoiceItemID6`, `RewardChoiceItemQuantity1`, `RewardChoiceItemQuantity2`, `RewardChoiceItemQuantity3`, `RewardChoiceItemQuantity4`, `RewardChoiceItemQuantity5`, `RewardChoiceItemQuantity6`, `RewardChoiceItemDisplayID1`, `RewardChoiceItemDisplayID2`, `RewardChoiceItemDisplayID3`, `RewardChoiceItemDisplayID4`, `RewardChoiceItemDisplayID5`, `RewardChoiceItemDisplayID6`, `POIContinent`, `POIx`, `POIy`, `POIPriority`, `RewardTitle`, `RewardArenaPoints`, `RewardFactionID1`, `RewardFactionID2`, `RewardFactionID3`, `RewardFactionID4`, `RewardFactionID5`, `RewardFactionValue1`, `RewardFactionValue2`, `RewardFactionValue3`, `RewardFactionValue4`, `RewardFactionValue5`, `RewardFactionCapIn1`, `RewardFactionCapIn2`, `RewardFactionCapIn3`, `RewardFactionCapIn4`, `RewardFactionCapIn5`, `RewardFactionOverride1`, `RewardFactionOverride2`, `RewardFactionOverride3`, `RewardFactionOverride4`, `RewardFactionOverride5`, `RewardFactionFlags`, `AreaGroupID`, `TimeAllowed`, `AllowableRaces`, `Expansion`, `ManagedWorldStateID`, `QuestSessionBonus`, `LogTitle`, `LogDescription`, `QuestDescription`, `AreaDescription`, `QuestCompletionLog`, `RewardCurrencyID1`, `RewardCurrencyID2`, `RewardCurrencyID3`, `RewardCurrencyID4`, `RewardCurrencyQty1`, `RewardCurrencyQty2`, `RewardCurrencyQty3`, `RewardCurrencyQty4`, `PortraitGiverText`, `PortraitGiverName`, `PortraitTurnInText`, `PortraitTurnInName`, `AcceptedSoundKitID`, `CompleteSoundKitID`, `VerifiedBuild`) VALUES
(96223, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Magisters\' Call', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Magisters' Call
(96224, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Magisters\' Conundrum', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Magisters' Conundrum
(96225, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Magisters\' Conundrum', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Magisters' Conundrum
(96226, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Omnium Anomalies', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Omnium Anomalies
(96227, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Lycaneum Chaos', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Lycaneum Chaos
(96228, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Shadowed Spire', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Shadowed Spire
(96229, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Void Reveals', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Void Reveals
(96230, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Unraveling the Wards', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Unraveling the Wards
(96231, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Grand Magister\'s Key-Cipher', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Grand Magister's Key-Cipher
(96232, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Return to the Omnium', '', 'Well, now that we\'ve recovered the piece Dar\'Khan stole, we are one step closer to restoring the Omnium.', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Return to the Omnium
(96233, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1302265, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'The Omnium Reawakens', '', 'For years, as Silvermoon and its people changed, this relic sat in a vault beneath the Terrace, unresponsive and dormant.', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- The Omnium Reawakens
(96238, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Return to the Omnium', '', 'With Belo\'vir\'s key-cipher recovered and Umbric in possession of the Void Magicule, we should return to the Lycaneum. Hopefully we have all that we need to recalibrate the Sunstrider Omnium.', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Return to the Omnium
(96410, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1299503, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Seeking Knowledge Week 1 of 5: The Omnium Folio', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Seeking Knowledge Week 1 of 5: The Omnium Folio
(96441, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1294322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Seeking Knowledge Week 2 of 5: Ritualized Arcana', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Seeking Knowledge Week 2 of 5: Ritualized Arcana
(96442, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1294322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Seeking Knowledge Week 3 of 5: Ley Line Assaults', '', 'Collect 5 Dark-Ley Coalescence by completing any Void Assault in Eversong or Zul\'Aman.', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Seeking Knowledge Week 3 of 5: Ley Line Assaults
(96443, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1294322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Seeking Knowledge Week 4 of 5: Magical Primessence', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0), -- Seeking Knowledge Week 4 of 5: Magical Primessence
(96444, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1294322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 'Seeking Knowledge Week 5 of 5: Off-World Magic', '', '', '', '', 0, 0, 0, 0, 0, 0, 0, 0, '', '', '', '', 0, 0, 0); -- Seeking Knowledge Week 5 of 5: Off-World Magic

DELETE FROM `quest_template_addon` WHERE `ID` IN (96223, 96224, 96225, 96226, 96227, 96228, 96229, 96230, 96231, 96232, 96233, 96238, 96410, 96441, 96442, 96443, 96444);
INSERT INTO `quest_template_addon` (`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`, `BreadcrumbForQuestId`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`, `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`, `ProvidedItemCount`, `SpecialFlags`, `ScriptName`) VALUES
(96223, 0, 0, 0, 0, 96224, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Magisters' Call
(96224, 0, 0, 0, 96223, 96225, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Magisters' Conundrum
(96225, 0, 0, 0, 96224, 96226, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Magisters' Conundrum
(96226, 0, 0, 0, 96225, 96227, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Omnium Anomalies
(96227, 0, 0, 0, 96226, 96228, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Lycaneum Chaos
(96228, 0, 0, 0, 96227, 96229, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Shadowed Spire
(96229, 0, 0, 0, 96228, 96230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Void Reveals
(96230, 0, 0, 0, 96229, 96231, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Unraveling the Wards
(96231, 0, 0, 0, 96230, 96232, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Grand Magister's Key-Cipher
(96232, 0, 0, 0, 96231, 96233, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Return to the Omnium
(96233, 0, 0, 0, 96232, 96238, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- The Omnium Reawakens
(96238, 0, 0, 0, 96233, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Return to the Omnium
(96410, 0, 0, 0, 0, 96441, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Seeking Knowledge Week 1 of 5: The Omnium Folio
(96441, 0, 0, 0, 96410, 96442, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Seeking Knowledge Week 2 of 5: Ritualized Arcana
(96442, 0, 0, 0, 96441, 96443, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Seeking Knowledge Week 3 of 5: Ley Line Assaults
(96443, 0, 0, 0, 96442, 96444, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''), -- Seeking Knowledge Week 4 of 5: Magical Primessence
(96444, 0, 0, 0, 96443, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ''); -- Seeking Knowledge Week 5 of 5: Off-World Magic

DELETE FROM `creature_queststarter` WHERE `quest` IN (96223, 96224, 96225, 96226, 96227, 96228, 96229, 96230, 96231, 96232, 96233, 96238, 96410, 96441, 96442, 96443, 96444);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(246025, 96223, 0), -- The Magisters' Call (starter Umbric)
(237504, 96224, 0), -- The Magisters' Conundrum (starter Rommath)
(237504, 96225, 0), -- The Magisters' Conundrum (starter Rommath)
(237504, 96226, 0), -- Omnium Anomalies (starter Rommath)
(237504, 96227, 0), -- Lycaneum Chaos (starter Rommath)
(246025, 96228, 0), -- The Shadowed Spire (starter Umbric)
(246025, 96229, 0), -- The Void Reveals (starter Umbric)
(237504, 96230, 0), -- Unraveling the Wards (starter Rommath)
(237504, 96231, 0), -- The Grand Magister's Key-Cipher (starter Rommath)
(237504, 96232, 0), -- Return to the Omnium (starter Rommath)
(237504, 96233, 0), -- The Omnium Reawakens (starter Rommath)
(237504, 96238, 0), -- Return to the Omnium (starter Rommath)
(237504, 96410, 0), -- Seeking Knowledge Week 1 of 5: The Omnium Folio (starter Rommath)
(237504, 96441, 0), -- Seeking Knowledge Week 2 of 5: Ritualized Arcana (starter Rommath)
(237504, 96442, 0), -- Seeking Knowledge Week 3 of 5: Ley Line Assaults (starter Rommath)
(237504, 96443, 0), -- Seeking Knowledge Week 4 of 5: Magical Primessence (starter Rommath)
(237504, 96444, 0); -- Seeking Knowledge Week 5 of 5: Off-World Magic (starter Rommath)

DELETE FROM `creature_questender` WHERE `quest` IN (96223, 96224, 96225, 96226, 96227, 96228, 96229, 96230, 96231, 96232, 96233, 96238, 96410, 96441, 96442, 96443, 96444);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES
(246025, 96223, 0), -- The Magisters' Call (ender Umbric)
(237504, 96224, 0), -- The Magisters' Conundrum (ender Rommath)
(246025, 96225, 0), -- The Magisters' Conundrum (ender Umbric)
(237504, 96226, 0), -- Omnium Anomalies (ender Rommath)
(237504, 96227, 0), -- Lycaneum Chaos (ender Rommath)
(246025, 96228, 0), -- The Shadowed Spire (ender Umbric)
(246025, 96229, 0), -- The Void Reveals (ender Umbric)
(237504, 96230, 0), -- Unraveling the Wards (ender Rommath)
(237504, 96231, 0), -- The Grand Magister's Key-Cipher (ender Rommath)
(237504, 96232, 0), -- Return to the Omnium (ender Rommath)
(237504, 96233, 0), -- The Omnium Reawakens (ender Rommath)
(237504, 96238, 0), -- Return to the Omnium (ender Rommath)
(237504, 96410, 0), -- Seeking Knowledge Week 1 of 5: The Omnium Folio (ender Rommath)
(237504, 96441, 0), -- Seeking Knowledge Week 2 of 5: Ritualized Arcana (ender Rommath)
(237504, 96442, 0), -- Seeking Knowledge Week 3 of 5: Ley Line Assaults (ender Rommath)
(246025, 96443, 0), -- Seeking Knowledge Week 4 of 5: Magical Primessence (ender Umbric)
(237504, 96444, 0); -- Seeking Knowledge Week 5 of 5: Off-World Magic (ender Rommath)

