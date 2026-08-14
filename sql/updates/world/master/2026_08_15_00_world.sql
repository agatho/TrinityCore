--
-- TreasurePicker quest rewards (server-authoritative picker contents).
--
-- There is no TreasurePicker*.db2 in the client or in WoWDBDefs: the client asks the server for the
-- contents with CMSG_QUERY_TREASURE_PICKER and renders whatever SMSG_TREASURE_PICKER_RESPONSE says,
-- so the pool has to live in world. `treasure_picker` holds the picker header (Flags / IsChoice /
-- Gold), `treasure_picker_items` the offered rows. Quests point at a picker through the existing
-- `quest_treasure_pickers` table.
--
-- The core filters the stored rows per player (ItemSparse.AllowableClass, weapon/armor proficiency
-- skill when AllowableClass = -1, AllowableRace, ITEM_FLAG2_FACTION_*), so these lists are the full
-- unfiltered pools and are expected to be wider than what any single character is shown.
--
-- Evidence: retail 12.0.7.68453 captures (Shaman turn-in for the picker headers and the granted row;
-- Priest/Hunter/Warrior responses for the per-class filtering), cross-checked against the full
-- unfiltered reward lists for quests 90882/90883/90885/90886/90887 and ItemSparse 12.0.7.67808.
--

CREATE TABLE IF NOT EXISTS `treasure_picker` (
  `TreasurePickerID` int unsigned NOT NULL,
  `Flags` int NOT NULL DEFAULT '0',
  `IsChoice` tinyint unsigned NOT NULL DEFAULT '0',
  `Gold` bigint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`TreasurePickerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `treasure_picker_items` (
  `TreasurePickerID` int unsigned NOT NULL,
  `Idx` int unsigned NOT NULL,
  `ItemID` int unsigned NOT NULL DEFAULT '0',
  `ItemQuantity` int unsigned NOT NULL DEFAULT '1',
  `BonusListID` int NOT NULL DEFAULT '0',
  `Context` tinyint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`TreasurePickerID`,`Idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `treasure_picker` WHERE `TreasurePickerID` IN (4025, 4026, 4027, 4028, 4029);
INSERT INTO `treasure_picker` (`TreasurePickerID`, `Flags`, `IsChoice`, `Gold`, `VerifiedBuild`) VALUES
(4025, 128, 0, 0, 68453), -- 90882 Gnoll Way
(4026, 0,   0, 0, 68453), -- 90883 To Go'shek Farm
(4027, 128, 0, 0, 68453), -- 90885 My Beautiful Pumpkins
(4028, 128, 0, 0, 68453), -- 90887 Farmer's Nemesis
(4029, 128, 0, 0, 68453); -- 90886 Best Laid Plans...

DELETE FROM `treasure_picker_items` WHERE `TreasurePickerID` IN (4025, 4026, 4027, 4028, 4029);
INSERT INTO `treasure_picker_items` (`TreasurePickerID`, `Idx`, `ItemID`, `ItemQuantity`, `BonusListID`, `Context`, `VerifiedBuild`) VALUES
-- 4025 / 90882 -- 28 weapons; the granted row is picked from the class-filtered subset
(4025, 0, 153726, 1, 4790, 17, 68453),
(4025, 1, 153747, 1, 4790, 17, 68453),
(4025, 2, 153773, 1, 4790, 17, 68453),
(4025, 3, 153792, 1, 4790, 17, 68453),
(4025, 4, 153814, 1, 4790, 17, 68453),
(4025, 5, 153830, 1, 4790, 17, 68453),
(4025, 6, 153835, 1, 4790, 17, 68453),
(4025, 7, 153856, 1, 4790, 17, 68453),
(4025, 8, 153859, 1, 4790, 17, 68453),
(4025, 9, 153889, 1, 4790, 17, 68453),
(4025, 10, 153891, 1, 4790, 17, 68453),
(4025, 11, 153892, 1, 4790, 17, 68453),
(4025, 12, 153893, 1, 4790, 17, 68453),
(4025, 13, 153934, 1, 4790, 17, 68453),
(4025, 14, 153944, 1, 4790, 17, 68453),
(4025, 15, 153959, 1, 4790, 17, 68453),
(4025, 16, 153960, 1, 4790, 17, 68453),
(4025, 17, 153961, 1, 4790, 17, 68453),
(4025, 18, 153973, 1, 4790, 17, 68453),
(4025, 19, 153983, 1, 4790, 17, 68453),
(4025, 20, 154005, 1, 4790, 17, 68453),
(4025, 21, 154024, 1, 4790, 17, 68453),
(4025, 22, 154025, 1, 4790, 17, 68453),
(4025, 23, 154035, 1, 4790, 17, 68453),
(4025, 24, 154036, 1, 4790, 17, 68453),
(4025, 25, 160513, 1, 4790, 17, 68453),
(4025, 26, 194522, 1, 4790, 17, 68453),
(4025, 27, 231839, 1, 4790, 17, 68453),
-- 4026 / 90883 -- bags; Idx 0 = 249773 matches the captured grant
(4026, 0, 249773, 1, 0, 0, 68453),
(4026, 1, 249772, 1, 0, 0, 68453),
(4026, 2, 249771, 1, 0, 0, 68453),
(4026, 3, 188213, 1, 0, 0, 68453),
-- 4027 / 90885 -- rings
(4027, 0, 153741, 1, 4790, 17, 68453),
(4027, 1, 153742, 1, 4790, 17, 68453),
(4027, 2, 153796, 1, 4790, 17, 68453),
(4027, 3, 153797, 1, 4790, 17, 68453),
(4027, 4, 153802, 1, 4790, 17, 68453),
(4027, 5, 153803, 1, 4790, 17, 68453),
(4027, 6, 153817, 1, 4790, 17, 68453),
(4027, 7, 153818, 1, 4790, 17, 68453),
(4027, 8, 153862, 1, 4790, 17, 68453),
(4027, 9, 153863, 1, 4790, 17, 68453),
(4027, 10, 153908, 1, 4790, 17, 68453),
(4027, 11, 153909, 1, 4790, 17, 68453),
(4027, 12, 153927, 1, 4790, 17, 68453),
(4027, 13, 153928, 1, 4790, 17, 68453),
(4027, 14, 153948, 1, 4790, 17, 68453),
(4027, 15, 153949, 1, 4790, 17, 68453),
(4027, 16, 153995, 1, 4790, 17, 68453),
(4027, 17, 153996, 1, 4790, 17, 68453),
(4027, 18, 154011, 1, 4790, 17, 68453),
(4027, 19, 154012, 1, 4790, 17, 68453),
(4027, 20, 154114, 1, 4790, 17, 68453),
(4027, 21, 154115, 1, 4790, 17, 68453),
(4027, 22, 154745, 1, 4790, 17, 68453),
(4027, 23, 154746, 1, 4790, 17, 68453),
(4027, 24, 194533, 1, 4790, 17, 68453),
(4027, 25, 194534, 1, 4790, 17, 68453),
-- 4028 / 90887 -- chest/cloak
(4028, 0, 153718, 1, 4790, 17, 68453),
(4028, 1, 153733, 1, 4790, 17, 68453),
(4028, 2, 153734, 1, 4790, 17, 68453),
(4028, 3, 153793, 1, 4790, 17, 68453),
(4028, 4, 153799, 1, 4790, 17, 68453),
(4028, 5, 153805, 1, 4790, 17, 68453),
(4028, 6, 153829, 1, 4790, 17, 68453),
(4028, 7, 153837, 1, 4790, 17, 68453),
(4028, 8, 153865, 1, 4790, 17, 68453),
(4028, 9, 153866, 1, 4790, 17, 68453),
(4028, 10, 153867, 1, 4790, 17, 68453),
(4028, 11, 153875, 1, 4790, 17, 68453),
(4028, 12, 153900, 1, 4790, 17, 68453),
(4028, 13, 153901, 1, 4790, 17, 68453),
(4028, 14, 153935, 1, 4790, 17, 68453),
(4028, 15, 153945, 1, 4790, 17, 68453),
(4028, 16, 153951, 1, 4790, 17, 68453),
(4028, 17, 153998, 1, 4790, 17, 68453),
(4028, 18, 154023, 1, 4790, 17, 68453),
(4028, 19, 154026, 1, 4790, 17, 68453),
(4028, 20, 154037, 1, 4790, 17, 68453),
(4028, 21, 154119, 1, 4790, 17, 68453),
(4028, 22, 154739, 1, 4790, 17, 68453),
(4028, 23, 154748, 1, 4790, 17, 68453),
(4028, 24, 194526, 1, 4790, 17, 68453),
(4028, 25, 194535, 1, 4790, 17, 68453),
-- 4029 / 90886 -- boots/gloves
(4029, 0, 153735, 1, 4790, 17, 68453),
(4029, 1, 153736, 1, 4790, 17, 68453),
(4029, 2, 153785, 1, 4790, 17, 68453),
(4029, 3, 153786, 1, 4790, 17, 68453),
(4029, 4, 153806, 1, 4790, 17, 68453),
(4029, 5, 153807, 1, 4790, 17, 68453),
(4029, 6, 153820, 1, 4790, 17, 68453),
(4029, 7, 153821, 1, 4790, 17, 68453),
(4029, 8, 153845, 1, 4790, 17, 68453),
(4029, 9, 153846, 1, 4790, 17, 68453),
(4029, 10, 153902, 1, 4790, 17, 68453),
(4029, 11, 153903, 1, 4790, 17, 68453),
(4029, 12, 153936, 1, 4790, 17, 68453),
(4029, 13, 153937, 1, 4790, 17, 68453),
(4029, 14, 153952, 1, 4790, 17, 68453),
(4029, 15, 153953, 1, 4790, 17, 68453),
(4029, 16, 154001, 1, 4790, 17, 68453),
(4029, 17, 154002, 1, 4790, 17, 68453),
(4029, 18, 154014, 1, 4790, 17, 68453),
(4029, 19, 154015, 1, 4790, 17, 68453),
(4029, 20, 154039, 1, 4790, 17, 68453),
(4029, 21, 154040, 1, 4790, 17, 68453),
(4029, 22, 154738, 1, 4790, 17, 68453),
(4029, 23, 154741, 1, 4790, 17, 68453),
(4029, 24, 194524, 1, 4790, 17, 68453),
(4029, 25, 194527, 1, 4790, 17, 68453);

DELETE FROM `quest_treasure_pickers` WHERE `QuestID` IN (90882, 90883, 90885, 90886, 90887);
INSERT INTO `quest_treasure_pickers` (`QuestID`, `TreasurePickerID`, `OrderIndex`) VALUES
(90882, 4025, 0),
(90883, 4026, 0),
(90885, 4027, 0),
(90886, 4029, 0),
(90887, 4028, 0);
