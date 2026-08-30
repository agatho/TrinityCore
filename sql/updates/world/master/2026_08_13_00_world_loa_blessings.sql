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

-- ---------------------------------------------------------------------------
-- FULL major x minor MATRIX (MinorLoa 2..10) — reconstructed from DB2 @68887
-- with NO capture. Each "Blessing:*" spell's Spell.Description_lang references a
-- hidden description spell whose text opens with two icon+name lines naming BOTH
-- loa (e.g. "Akil'zon, Loa of Victory" + "Kulzi, Loa of Medicine"); the icon ids
-- are themselves SpellName rows. So each pairing is triple-confirmed (family
-- SpellName word + description major icon + description minor icon/prose).
-- See C:\dumps\LOA_MATRIX_RECONSTRUCTION.md for the full evidence table.
--
-- DB2 CORRECTION: Akil'zon = Loa of VICTORY (not War), Nalorakk = Loa of WAR
-- (not Victory). Halazzi = the Hunt, Jan'alai = Fire/Magic. MajorLoa slots below
-- keep the established numbering (1 Akil'zon / 2 Halazzi / 3 Jan'alai / 4 Nalorakk).
--
-- MINOR loa slots (icon-SpellName confirmed): NINE, not eight —
--   2 Kulzi(Medicine) 3 Filo(Childhood) 4 Pahk(Depths) 5 Mot'amra(Pestilence)
--   6 Wila'ma(Travelers="Packpeddle of X") 7 Shadra(Subterfuge)
--   8 Dundun(Abundance) 9 Puul(Peril) 10 Oe(Growth)
-- ---------------------------------------------------------------------------
INSERT INTO `loa_blessing_option` (`MajorLoa`,`MinorLoa`,`SpellId`,`UnlockConditionId`,`Name`) VALUES
-- Akil'zon (Victory) x minor
(1,  2, 1243161, 0, 'Akil''zon (Victory) + Kulzi (Medicine): Victorious Health'),
(1,  3, 1243171, 0, 'Akil''zon (Victory) + Filo (Childhood): Victory of Relaxation'),
(1,  4, 1243173, 0, 'Akil''zon (Victory) + Pahk (Depths): Victorious Waves'),
(1,  5, 1243174, 0, 'Akil''zon (Victory) + Mot''amra (Pestilence): Victor''s Presence'),
(1,  6, 1243175, 0, 'Akil''zon (Victory) + Wila''ma (Travelers): Packpeddle of Victory'),
(1,  7, 1243176, 0, 'Akil''zon (Victory) + Shadra (Subterfuge): Victory Through Subterfuge'),
(1,  8, 1243177, 0, 'Akil''zon (Victory) + Dundun (Abundance): Abundant Rush'),
(1,  9, 1243178, 0, 'Akil''zon (Victory) + Puul (Peril): Victory at Any Cost'),
(1, 10, 1243179, 0, 'Akil''zon (Victory) + Oe (Growth): Victorious Growth'),
-- Halazzi (the Hunt) x minor
(2,  2, 1243184, 0, 'Halazzi (Hunt) + Kulzi (Medicine): Neverending Hunt'),
(2,  3, 1243185, 0, 'Halazzi (Hunt) + Filo (Childhood): Hunt for Simplicity'),
(2,  4, 1243186, 0, 'Halazzi (Hunt) + Pahk (Depths): Devoted Adaptation'),
(2,  5, 1243187, 0, 'Halazzi (Hunt) + Mot''amra (Pestilence): Guaranteed Yield'),
(2,  6, 1243189, 0, 'Halazzi (Hunt) + Wila''ma (Travelers): Packpeddle of the Hunt'),
(2,  7, 1243190, 0, 'Halazzi (Hunt) + Shadra (Subterfuge): The Cunning Hunt'),
(2,  8, 1243191, 0, 'Halazzi (Hunt) + Dundun (Abundance): Guide of the Harvest'),
(2,  9, 1243192, 0, 'Halazzi (Hunt) + Puul (Peril): Predator''s Gamble'),
(2, 10, 1243193, 0, 'Halazzi (Hunt) + Oe (Growth): Patient Hunter'),
-- Jan'alai (Fire/Magic) x minor
(3,  2, 1243194, 0, 'Jan''alai (Fire) + Kulzi (Medicine): Flame of Vitality'),
(3,  3, 1243195, 0, 'Jan''alai (Fire) + Filo (Childhood): Flowing Flame'),
(3,  4, 1243196, 0, 'Jan''alai (Fire) + Pahk (Depths): Buoyant Flame'),
(3,  5, 1243197, 0, 'Jan''alai (Fire) + Mot''amra (Pestilence): Ensorcelling Flame'),
(3,  6, 1243198, 0, 'Jan''alai (Fire) + Wila''ma (Travelers): Packpeddle of Fire'),
(3,  7, 1243200, 0, 'Jan''alai (Fire) + Shadra (Subterfuge): Creeping Flame'),
(3,  8, 1243201, 0, 'Jan''alai (Fire) + Dundun (Abundance): Harvest Haste'),
(3,  9, 1243202, 0, 'Jan''alai (Fire) + Puul (Peril): Ravenous Glass'),
(3, 10, 1243203, 0, 'Jan''alai (Fire) + Oe (Growth): Everthrive Flame'),
-- Nalorakk (War) x minor
(4,  2, 1243204, 0, 'Nalorakk (War) + Kulzi (Medicine): War of Attrition'),
(4,  3, 1243205, 0, 'Nalorakk (War) + Filo (Childhood): War Against Threats'),
(4,  4, 1243206, 0, 'Nalorakk (War) + Pahk (Depths): War on the Currents'),
(4,  5, 1243207, 0, 'Nalorakk (War) + Mot''amra (Pestilence): War-Spoiled'),
(4,  6, 1243208, 0, 'Nalorakk (War) + Wila''ma (Travelers): Packpeddle of War'),
(4,  7, 1243209, 0, 'Nalorakk (War) + Shadra (Subterfuge): Wartime Preparations'),
(4,  8, 1243210, 0, 'Nalorakk (War) + Dundun (Abundance): Abundance Permanence'),
(4,  9, 1243211, 0, 'Nalorakk (War) + Puul (Peril): Reckless Feast'),
(4, 10, 1243213, 0, 'Nalorakk (War) + Oe (Growth): Command Experience');

-- STILL CAPTURE-BLOCKED (not seeded):
--   * 1243214 "Blessing: Eagle-Pangolin (WIP)" — empty DB2 Description_lang, name
--     literally "(WIP)". Unfinished dev spell; no resolvable (major,minor) pair.
--   * 1229266 "Blessing of Abundance" — NOT a major x minor combo; a separate
--     tiered progression buff for the Abundance harvest event (Phase 3 tie-in).
--   * Per-option UnlockConditionId (minor-loa side-quest PlayerConditions) — all
--     seeded 0 (always shown); real gates still need the unlock-chain capture.

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
