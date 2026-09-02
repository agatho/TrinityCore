--
-- Crafting Orders (work-order system) persistent storage
--
DROP TABLE IF EXISTS `crafting_orders`;
CREATE TABLE `crafting_orders` (
  `OrderID` bigint unsigned NOT NULL,
  `SkillLineAbilityID` int NOT NULL DEFAULT '0',
  `OrderState` tinyint NOT NULL DEFAULT '0',
  `OrderType` tinyint unsigned NOT NULL DEFAULT '0',
  `MinQuality` int unsigned NOT NULL DEFAULT '0',
  `EndDate` bigint NOT NULL DEFAULT '0',
  `ClaimEndDate` bigint NOT NULL DEFAULT '0',
  `TipAmount` bigint unsigned NOT NULL DEFAULT '0',
  `HouseCutAmount` bigint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `CustomerGuid` bigint unsigned NOT NULL DEFAULT '0',
  `CrafterGuid` bigint unsigned NOT NULL DEFAULT '0',
  `CustomerAccountId` int unsigned NOT NULL DEFAULT '0',
  `CustomerNotes` text,
  PRIMARY KEY (`OrderID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `crafting_order_reagents`;
CREATE TABLE `crafting_order_reagents` (
  `OrderID` bigint unsigned NOT NULL,
  `Slot` tinyint unsigned NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `CurrencyID` int NOT NULL DEFAULT '0',
  `Quantity` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`OrderID`,`Slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
