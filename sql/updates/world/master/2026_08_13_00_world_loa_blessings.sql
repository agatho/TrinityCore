--
-- Midnight Zul'Aman "Loa Blessings" — Altar of Blessings worship system.
-- SHIPS ON feature/loa-blessings. LISTED, NOT APPLIED to the central realm.
--
-- Every SpellId below is DB2-anchored @ build 12.0.7.68887 (SpellName.db2).
-- The altar creature (256508) and Du'gal (256510) already exist in the world
-- DB (integ_world, verified read-only) but carry no ScriptName/AIName — this
-- update binds the worship script and seeds the selectable blessings.
--

-- ---------------------------------------------------------------------------
-- Selectable altar options: (MajorLoa, MinorLoa) -> blessing aura spell.
--   MajorLoa: 1=Akil'zon(Eagle) 2=Halazzi(Lynx) 3=Jan'alai(Dragonhawk) 4=Nalorakk(Bear)
--   MinorLoa: 0 = base major-only blessing; 1..N = minor-loa combination slot
--   UnlockConditionId: PlayerConditionID gating the option (0 = always shown)
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `loa_blessing_option`;
CREATE TABLE `loa_blessing_option` (
  `MajorLoa`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `MinorLoa`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `SpellId`           INT UNSIGNED     NOT NULL DEFAULT 0,
  `UnlockConditionId` INT UNSIGNED     NOT NULL DEFAULT 0,
  `Name`              TEXT,
  PRIMARY KEY (`MajorLoa`,`MinorLoa`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Loa Blessings altar options (DB2-anchored @68887)';

-- Base major-loa blessings (MinorLoa = 0). Spell ids confirmed in SpellName.db2:
--   1235600 "Akil'zon's Blessing", 1227122 "Halazzi's Blessing",
--   1235170 "Blessing of Jan'alai", 1236349 "Blessing of Nalorakk".
INSERT INTO `loa_blessing_option` (`MajorLoa`,`MinorLoa`,`SpellId`,`UnlockConditionId`,`Name`) VALUES
(1, 0, 1235600, 0, 'Worship Akil''zon, loa of the eagle'),
(2, 0, 1227122, 0, 'Worship Halazzi, loa of the lynx'),
(3, 0, 1235170, 0, 'Worship Jan''alai, loa of the dragonhawk'),
(4, 0, 1236349, 0, 'Worship Nalorakk, loa of the bear');

-- Major-loa signature combination blessings (MinorLoa = 1). Spell ids confirmed:
--   1239164 "Blessing: Akil'zon's Gale", 1239158 "Blessing: Halazzi's Guile",
--   1239171 "Blessing: Jan'alai's Everburn", 1239152 "Blessing: Nalorakk's Pressure".
INSERT INTO `loa_blessing_option` (`MajorLoa`,`MinorLoa`,`SpellId`,`UnlockConditionId`,`Name`) VALUES
(1, 1, 1239164, 0, 'Akil''zon''s Gale'),
(2, 1, 1239158, 0, 'Halazzi''s Guile'),
(3, 1, 1239171, 0, 'Jan''alai''s Everburn'),
(4, 1, 1239152, 0, 'Nalorakk''s Pressure');

-- NOTE (CAPTURE/RESEARCH-BLOCKED): the full minor-loa matrix (DB2 "Blessing: *"
-- 1243161..1243214, grouped Victory/Hunt/Flame/War families) plus the eight
-- minor-loa NAMES and their unlock PlayerConditions are not yet mapped to
-- concrete (major,minor) pairings. Seed them here once in-game worship captures
-- confirm which combination yields which spell.

-- ---------------------------------------------------------------------------
-- Bind the worship script to the existing altar creature.
-- (Realm-safe: no-op if the creature row is absent on a given DB.)
-- ---------------------------------------------------------------------------
UPDATE `creature_template` SET `ScriptName` = 'npc_altar_of_blessings' WHERE `entry` = 256508;

-- LISTED / DEFERRED (not written here — need capture or content import):
--   * gossip_menu / npc_text rows for Du'gal (256510) unlock chatter.
--   * quest 93792 "Blessings of the Loa" reward wiring beyond stock template.
--   * PlayerCondition rows for minor-loa unlocks (UnlockConditionId).
--   * Abundance reward hook (spell 1229266 "Blessing of Abundance" / faction 2696
--     "Amani Tribe" renown) — see LoaBlessingMgr::OnAbundanceHarvest.
