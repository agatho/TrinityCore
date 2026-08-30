-- Omnium Folio (Midnight 12.0.7 "Revelations" Season 1 rune-ledger power system)
-- ---------------------------------------------------------------------------
-- Optional per-fork seasonal schedule for the folio. The folio's *definition*
-- data lives entirely in client DB2s already loaded by TraitMgr:
--   TraitTree 1186 / TraitSystem 48 / TraitCurrency 4230 ("Mote of Omnial Inquiry")
--   sourced from achievements 62606..62610 (weekly gates) + level-1 base.
-- This table only tells OmniumFolioMgr which season is live and whether the
-- server-side eligibility/reset seam is enabled.
--
-- Realm-safety: OmniumFolioMgr::LoadFromDB() tolerates this table being ABSENT
-- (idle no-op). Ship-only; DO NOT apply to the shared integration realm.
CREATE TABLE IF NOT EXISTS `omnium_folio_season` (
  `SeasonId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Midnight seasonal index',
  `Enabled` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1 = server-side folio eligibility/reset seam active',
  `TraitTreeId` int NOT NULL DEFAULT '1186' COMMENT 'DB2 TraitTree.ID (client 12.0.7.68887)',
  `TraitSystemId` int NOT NULL DEFAULT '48' COMMENT 'DB2 TraitSystem.ID',
  `UnlockSpellId` int unsigned NOT NULL DEFAULT '1279717' COMMENT 'SPELL_EFFECT_CREATE_TRAIT_TREE_CONFIG, MiscValue=TraitTreeId',
  `WeeklyResetHour` tinyint unsigned NOT NULL DEFAULT '15' COMMENT 'UTC hour of weekly mote gate',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`SeasonId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Omnium Folio seasonal schedule (Midnight S1)';

-- Seed Midnight Season 1 as ACTIVE. This is the intended live state: only the
-- integration session applies branch SQL (never the shared realm), so seeding it
-- here makes the folio eligibility/mint seam engage the moment this SQL is applied
-- to a disposable test DB. With Enabled=1, an eligible character (achievement 62606,
-- no existing TraitSystem-48 config) is minted a folio config on login.
DELETE FROM `omnium_folio_season` WHERE `SeasonId` = 1;
INSERT INTO `omnium_folio_season`
  (`SeasonId`,`Enabled`,`TraitTreeId`,`TraitSystemId`,`UnlockSpellId`,`WeeklyResetHour`,`Comment`) VALUES
  (1, 1, 1186, 48, 1279717, 15, 'Midnight Season 1 - Omnium Folio');
