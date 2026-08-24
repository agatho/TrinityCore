--
-- Phase 10L - UiTextureKit.db2
-- Resolves Campaign.UiTextureKitID -> KitPrefix string. Used by the
-- Major Faction renown UI to compute textureKit suffixes (atlas member
-- names like "MajorFaction-DragonscaleExpedition-Background"). Loaded
-- so MajorFactionMgr::GetTextureKitPrefix(factionId) can walk the chain
-- faction -> renown campaign -> Campaign.UiTextureKitID -> UiTextureKit.KitPrefix.
--
DROP TABLE IF EXISTS `ui_texture_kit`;
CREATE TABLE `ui_texture_kit` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `KitPrefix` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
