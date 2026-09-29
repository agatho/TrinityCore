--
-- TreasurePicker currency rows, and the Prey hunt reward pickers.
--
-- SMSG_TREASURE_PICKER_RESPONSE carries currencies next to items (CurrencyID, Quantity, optional
-- QuestRewardContextFlags). Every currency row is granted on quest reward, independent of the item pick; a
-- FirstCompletionBonus row only when the quest was never rewarded before, a RepeatCompletionBonus row only when it
-- was. A currency bound to a faction (CurrencyTypes.FactionID) is converted into reputation by ModifyCurrency.
--
-- Evidence: retail 12.1.0.69933 capture of "Prey: The Talon of Jan'alai (Normal)" (quest 91105, a character's first
-- hunt): pickers 4887 and 4877 queried at accept and turn-in, and the turn-in granted exactly their contents -
-- 3310 Coffer Key Shards +50, 3442 Adventurer Mistcrest +10, item 275918 Preyhunter's Adventurer Chest, and +1000
-- reputation with Preyhunter's Journey (faction 2808) from 100000 Preyseeker's Journey (3515, FactionID 2808).
-- The quest -> picker link (quest_treasure_pickers 91105 -> 4887, 4877) ships with feature/prey-voidforge.
--

CREATE TABLE IF NOT EXISTS `treasure_picker_currencies` (
  `TreasurePickerID` int unsigned NOT NULL,
  `Idx` int unsigned NOT NULL,
  `CurrencyID` int unsigned NOT NULL DEFAULT '0',
  `Quantity` int unsigned NOT NULL DEFAULT '0',
  `ContextFlags` int DEFAULT NULL COMMENT 'QuestRewardContextFlags: 1 = FirstCompletionBonus, 2 = RepeatCompletionBonus',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`TreasurePickerID`,`Idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `treasure_picker` WHERE `TreasurePickerID` IN (4877, 4887);
INSERT INTO `treasure_picker` (`TreasurePickerID`, `Flags`, `IsChoice`, `Gold`, `VerifiedBuild`) VALUES
(4877, 0, 0, 0, 69933), -- Prey hunt reward (91105)
(4887, 0, 0, 0, 69933); -- Prey hunt first-completion reputation (91105)

DELETE FROM `treasure_picker_items` WHERE `TreasurePickerID` IN (4877, 4887);
INSERT INTO `treasure_picker_items` (`TreasurePickerID`, `Idx`, `ItemID`, `ItemQuantity`, `BonusListID`, `Context`, `VerifiedBuild`) VALUES
(4877, 0, 275918, 1, 0, 11, 69933); -- Preyhunter's Adventurer Chest

DELETE FROM `treasure_picker_currencies` WHERE `TreasurePickerID` IN (4877, 4887);
INSERT INTO `treasure_picker_currencies` (`TreasurePickerID`, `Idx`, `CurrencyID`, `Quantity`, `ContextFlags`, `VerifiedBuild`) VALUES
(4877, 0, 3310, 50, NULL, 69933),   -- Coffer Key Shards
(4877, 1, 3442, 10, NULL, 69933),   -- Adventurer Mistcrest
(4887, 0, 3515, 100000, 1, 69933);  -- Preyseeker's Journey (-> +1000 reputation with faction 2808), FirstCompletionBonus
