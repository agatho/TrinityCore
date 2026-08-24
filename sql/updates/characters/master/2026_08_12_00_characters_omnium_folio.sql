-- Omnium Folio (Midnight 12.0.7 Season 1) -- per-character season bookkeeping
-- ---------------------------------------------------------------------------
-- The folio's actual power/selection state persists through the EXISTING trait
-- tables (character_trait_config / character_trait_entry) because it is a
-- generic TraitConfig (TraitSystem 48 / TraitTree 1186). This table only tracks
-- which season a character has already been reset/minted for, so the seasonal
-- rollover seam (OmniumFolioMgr::ResetForNewSeason) can re-mint exactly once.
--
-- Realm-safety: purely additive; nothing reads it until the seasonal seam is
-- enabled. Ship-only; DO NOT apply to the shared integration realm.
CREATE TABLE IF NOT EXISTS `character_omnium_folio` (
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `LastSeasonMinted` int unsigned NOT NULL DEFAULT '0' COMMENT 'Season the generic folio config was last minted/reset for',
  `UnlockedAt` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix time the folio unlock spell was applied',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Omnium Folio per-character season bookkeeping';
