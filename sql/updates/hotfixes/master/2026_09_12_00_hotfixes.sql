--
-- BattlePetState.db2 (12.1.0.69587 layout 489B22AD). Loaded so the pet-battle engine can tell
-- which weather/aura states the client must be told about (Flags 0x8) -- without it the client
-- never showed Call Blizzard / Call Lightning weather.
--
DROP TABLE IF EXISTS `battle_pet_state`;
CREATE TABLE `battle_pet_state` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `LuaName` text,
  `Flags` int NOT NULL DEFAULT '0',
  `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
