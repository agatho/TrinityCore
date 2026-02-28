-- Create hotfix tables for RoomComponentOptionTexture and RoomComponentTexture DB2 stores
-- These link room component options to their texture/material variants

CREATE TABLE IF NOT EXISTS `room_component_option_texture` (
    `ID` int unsigned NOT NULL DEFAULT '0',
    `RoomComponentOptionID` int NOT NULL DEFAULT '0',
    `RoomComponentTextureID` int NOT NULL DEFAULT '0',
    `VerifiedBuild` int NOT NULL DEFAULT '0',
    PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `room_component_texture` (
    `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    `ID` int unsigned NOT NULL DEFAULT '0',
    `Type` int NOT NULL DEFAULT '0',
    `FileDataID` int NOT NULL DEFAULT '0',
    `Flags` int NOT NULL DEFAULT '0',
    `UiOrder` int NOT NULL DEFAULT '0',
    `RoomComponentID` int unsigned NOT NULL DEFAULT '0',
    `VerifiedBuild` int NOT NULL DEFAULT '0',
    PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `room_component_texture_locale` (
    `ID` int unsigned NOT NULL DEFAULT '0',
    `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
    `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
    `VerifiedBuild` int NOT NULL DEFAULT '0',
    PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;
