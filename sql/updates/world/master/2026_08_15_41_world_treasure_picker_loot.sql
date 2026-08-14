--
-- Exile's Reach Expeditionary starter kit -- multi-class TreasurePicker pool.
--
-- Reference pool for the SPELL_EFFECT_LOOT pickers behind the Exile's Reach starter kit
-- (spell 1258196 -> picker 136844, spell 1272256 -> picker 138199). Both factions and all four
-- armor skills live in a single picker; the core narrows the list per player through
-- ObjectMgr::IsTreasurePickerItemEligibleForPlayer:
--   * armor rows carry AllowableClass = -1 and are separated by ItemTemplate::GetSkill()
--     (SKILL_CLOTH / SKILL_LEATHER / SKILL_MAIL / SKILL_PLATE_MAIL),
--   * the Alliance/Horde armor twins are separated by AllowableRace,
--   * the race-any faction pairs (mounts 25470/25476, necks 178941/178942) are separated by
--     ITEM_FLAG2_FACTION_ALLIANCE / ITEM_FLAG2_FACTION_HORDE.
-- This is what the armor-skill and race/faction filters in the engine exist for; without them a
-- character would be offered every faction's and every armor class's row.
--
-- 136844 is deliberately seeded empty: only one of the two LOOT spells was observed granting the
-- kit, and giving both a pool would double-grant it. Fill it once a capture shows distinct loot.
-- 178942 carries BonusListID 13617 as captured.
--
-- Evidence: retail 12.0.7.68887 capture (item push burst after the LOOT casts), ItemSparse
-- 12.0.7.67808 for AllowableClass / AllowableRace / Flags, QuestPackageItem for the Exile's Reach
-- armor packages.
--
-- NOTE: nothing in the tree queries these two pickers yet -- no quest references them and
-- SPELL_EFFECT_LOOT does not go through the picker path. They are reference data for the
-- eligibility filter and are inert until that consumer exists.
--

DELETE FROM `treasure_picker` WHERE `TreasurePickerID` IN (136844, 138199);
INSERT INTO `treasure_picker` (`TreasurePickerID`, `Flags`, `IsChoice`, `Gold`, `VerifiedBuild`) VALUES
(136844, 0, 0, 0, 68887), -- 1258196 Effect LOOT (empty -- avoid double kit)
(138199, 0, 0, 0, 68887); -- 1272256 Effect LOOT -- starter kit

DELETE FROM `treasure_picker_items` WHERE `TreasurePickerID` IN (136844, 138199);
INSERT INTO `treasure_picker_items` (`TreasurePickerID`, `Idx`, `ItemID`, `ItemQuantity`, `BonusListID`, `Context`, `VerifiedBuild`) VALUES
-- Faction mounts (faction-flag pair 25476 / 25470)
(138199,  0, 25476,  1, 0,     0, 68887), -- Green Wind Rider (Horde)
(138199,  1, 25470,  1, 0,     0, 68887), -- Golden Gryphon (Alliance)
-- Horde Expeditionary armor 175207-175238
(138199,  2, 175220, 1, 0,     0, 68887), -- Head cloth
(138199,  3, 175222, 1, 0,     0, 68887), -- Head leather
(138199,  4, 175221, 1, 0,     0, 68887), -- Head mail
(138199,  5, 175207, 1, 0,     0, 68887), -- Head plate
(138199,  6, 175229, 1, 0,     0, 68887), -- Shoulder cloth
(138199,  7, 175227, 1, 0,     0, 68887), -- Shoulder leather
(138199,  8, 175230, 1, 0,     0, 68887), -- Shoulder mail
(138199,  9, 175228, 1, 0,     0, 68887), -- Shoulder plate
(138199, 10, 175208, 1, 0,     0, 68887), -- Chest cloth
(138199, 11, 175211, 1, 0,     0, 68887), -- Chest leather
(138199, 12, 175210, 1, 0,     0, 68887), -- Chest mail
(138199, 13, 175209, 1, 0,     0, 68887), -- Chest plate
(138199, 14, 175238, 1, 0,     0, 68887), -- Waist cloth
(138199, 15, 175233, 1, 0,     0, 68887), -- Waist leather
(138199, 16, 175232, 1, 0,     0, 68887), -- Waist mail
(138199, 17, 175231, 1, 0,     0, 68887), -- Waist plate
(138199, 18, 175224, 1, 0,     0, 68887), -- Legs cloth
(138199, 19, 175226, 1, 0,     0, 68887), -- Legs leather
(138199, 20, 175225, 1, 0,     0, 68887), -- Legs mail
(138199, 21, 175223, 1, 0,     0, 68887), -- Legs plate
(138199, 22, 175213, 1, 0,     0, 68887), -- Feet cloth
(138199, 23, 175215, 1, 0,     0, 68887), -- Feet leather
(138199, 24, 175214, 1, 0,     0, 68887), -- Feet mail
(138199, 25, 175212, 1, 0,     0, 68887), -- Feet plate
(138199, 26, 175234, 1, 0,     0, 68887), -- Wrist cloth
(138199, 27, 175237, 1, 0,     0, 68887), -- Wrist leather
(138199, 28, 175236, 1, 0,     0, 68887), -- Wrist mail
(138199, 29, 175235, 1, 0,     0, 68887), -- Wrist plate
(138199, 30, 175218, 1, 0,     0, 68887), -- Hands cloth
(138199, 31, 175216, 1, 0,     0, 68887), -- Hands leather
(138199, 32, 175219, 1, 0,     0, 68887), -- Hands mail
(138199, 33, 175217, 1, 0,     0, 68887), -- Hands plate
-- Alliance Expeditionary armor 175175-175206
(138199, 34, 175188, 1, 0,     0, 68887), -- Head cloth
(138199, 35, 175190, 1, 0,     0, 68887), -- Head leather
(138199, 36, 175189, 1, 0,     0, 68887), -- Head mail
(138199, 37, 175175, 1, 0,     0, 68887), -- Head plate
(138199, 38, 175197, 1, 0,     0, 68887), -- Shoulder cloth
(138199, 39, 175195, 1, 0,     0, 68887), -- Shoulder leather
(138199, 40, 175198, 1, 0,     0, 68887), -- Shoulder mail
(138199, 41, 175196, 1, 0,     0, 68887), -- Shoulder plate
(138199, 42, 175176, 1, 0,     0, 68887), -- Chest cloth
(138199, 43, 175179, 1, 0,     0, 68887), -- Chest leather
(138199, 44, 175178, 1, 0,     0, 68887), -- Chest mail
(138199, 45, 175177, 1, 0,     0, 68887), -- Chest plate
(138199, 46, 175200, 1, 0,     0, 68887), -- Waist cloth
(138199, 47, 175202, 1, 0,     0, 68887), -- Waist leather
(138199, 48, 175201, 1, 0,     0, 68887), -- Waist mail
(138199, 49, 175199, 1, 0,     0, 68887), -- Waist plate
(138199, 50, 175192, 1, 0,     0, 68887), -- Legs cloth
(138199, 51, 175194, 1, 0,     0, 68887), -- Legs leather
(138199, 52, 175193, 1, 0,     0, 68887), -- Legs mail
(138199, 53, 175191, 1, 0,     0, 68887), -- Legs plate
(138199, 54, 175181, 1, 0,     0, 68887), -- Feet cloth
(138199, 55, 175183, 1, 0,     0, 68887), -- Feet leather
(138199, 56, 175182, 1, 0,     0, 68887), -- Feet mail
(138199, 57, 175180, 1, 0,     0, 68887), -- Feet plate
(138199, 58, 175203, 1, 0,     0, 68887), -- Wrist cloth
(138199, 59, 175206, 1, 0,     0, 68887), -- Wrist leather
(138199, 60, 175205, 1, 0,     0, 68887), -- Wrist mail
(138199, 61, 175204, 1, 0,     0, 68887), -- Wrist plate
(138199, 62, 175186, 1, 0,     0, 68887), -- Hands cloth
(138199, 63, 175184, 1, 0,     0, 68887), -- Hands leather
(138199, 64, 175187, 1, 0,     0, 68887), -- Hands mail
(138199, 65, 175185, 1, 0,     0, 68887), -- Hands plate
-- Faction necks (faction-flag pair 178942 / 178941; BonusListID 13617 as captured)
(138199, 66, 178942, 1, 13617, 0, 68887), -- War-Chain of the Horde
(138199, 67, 178941, 1, 13617, 0, 68887); -- Alliance Dog Tags
