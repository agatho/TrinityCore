--
-- Delve Nemesis layer (Midnight 12.0.7) - shipped, NOT applied to the central realm.
-- Additive + tolerant: NemesisMgr::LoadFromDB() treats an absent table or zero
-- rows as "no Pactsworn packs" (idle no-op), so a realm without this SQL is safe.
--
-- The Pactsworn pack ENTRIES (creature_template) and their in-delve SPAWN COORDS
-- are CAPTURE-BLOCKED (Pactsworn are world-DB creatures, absent from Creature.db2
-- @68887; their delve spawn points need a tester capture). This table therefore
-- ships EMPTY - it is the shape the spawn spine reads once content is authored.
--
DROP TABLE IF EXISTS `nemesis_pactsworn_pack`;
CREATE TABLE `nemesis_pactsworn_pack` (
  `Id`          INT UNSIGNED NOT NULL,
  `MinTier`     TINYINT UNSIGNED NOT NULL DEFAULT 4  COMMENT 'Pactsworn appear at Tier 4+ (DELVE_TIER_ENDGAME_START)',
  `MapId`       INT UNSIGNED NOT NULL DEFAULT 0       COMMENT '0 = any delve map',
  `Entry`       INT UNSIGNED NOT NULL DEFAULT 0       COMMENT 'creature_template entry of the Pactsworn elite',
  `PackSize`    TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `PosX`        FLOAT NOT NULL DEFAULT 0,
  `PosY`        FLOAT NOT NULL DEFAULT 0,
  `PosZ`        FLOAT NOT NULL DEFAULT 0,
  `Orientation` FLOAT NOT NULL DEFAULT 0,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci
  COMMENT='Delve Nemesis: Pactsworn elite packs spawned in Tier 4+ delves (ships empty - CAPTURE-BLOCKED content)';

-- Deferred / listed-only (NOT written this pass, all CAPTURE-BLOCKED):
--   * gameobject_template + *_loot_template rows for the Nemesis Strongbox chest
--     (grantable via config Nemesis.Strongbox.LootId once authored).
--   * creature_template rows for the Pactsworn elites (world-DB, entries TBD).
--   * delve_template row for Torment's Rise (Map 2966) - lives on feature/delves,
--     needs its Scenario id + entry/exit coords captured (cf. Darkway=3184 pattern).
--   * achievement_reward rows delivering item 263413 (Nullaeus Domaneye) /
--     263222 (Arcanovoid Construct) / 264413 (Dominating Victory) for ach 61797/61799.
