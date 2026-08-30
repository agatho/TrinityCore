--
-- Season 1 raid-season systems (Midnight fork) -- Chiming Void Curio omni-token + Sporefall flex-Mythic.
--
-- LISTED / SHIPS ON THE BRANCH. Realm-safe: additive, tolerated-if-absent by RaidSeasonS1Mgr::Initialize()
-- (an absent/empty table => idle no-op). Applied only to a disposable test DB, NEVER to the central realm.
--
-- Evidence-vs-invention: every DB2 id referenced below is source-anchored to build 12.0.7.68887.
-- No token / vendor / ExtendedCost id is invented -- those are CAPTURE-BLOCKED and left for a follow-up.
--

-- ---------------------------------------------------------------------------
-- Chiming Void Curio -> Tier-35 token reward map (server content; NOT in any client DB2).
-- Keyed loosely: ClassID/InventoryType 0 = wildcard, DifficultyID 0 = any. Matched most-specific first.
-- SHIPS EMPTY: the Tier-35 token item ids per (class, slot, difficulty) are CAPTURE-BLOCKED at 68887.
-- The ATTESTED redemption surface is the vendor trade at NPC Kirana (see the npc_vendor block below),
-- which needs no core code; this table only feeds the optional server-driven redemption seam.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `raid_season_curio_reward`;
CREATE TABLE `raid_season_curio_reward` (
  `ClassID`       TINYINT UNSIGNED  NOT NULL DEFAULT 0 COMMENT '0 = any class',
  `InventoryType` TINYINT UNSIGNED  NOT NULL DEFAULT 0 COMMENT '0 = any slot',
  `DifficultyID`  SMALLINT          NOT NULL DEFAULT 0 COMMENT '0 = any difficulty; else DB2 Difficulty id (e.g. 233 flex-Mythic)',
  `TokenItemID`   INT UNSIGNED      NOT NULL DEFAULT 0 COMMENT 'Tier-35 token granted -- CAPTURE-BLOCKED',
  PRIMARY KEY (`ClassID`,`InventoryType`,`DifficultyID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='S1 Chiming Void Curio (item 249367) redemption map';
-- (no rows: token ids CAPTURE-BLOCKED)

-- ---------------------------------------------------------------------------
-- DEFERRED / LISTED-ONLY (NOT written this pass -- all pending capture):
--
--  * npc_vendor rows at NPC Kirana selling the Tier-35 class-set tokens, each costing 1x Chiming Void Curio
--    (item 249367) via item_extended_cost. Needs: Kirana's creature entry, the Tier-35 token item ids per
--    (class, slot), and the ItemExtendedCost id(s) that price a token at 1 Curio -- all CAPTURE-BLOCKED.
--
--  * Curio difficulty bonus variants (Normal ilvl 256 / Heroic 269 / Mythic 282 per research) so a Mythic
--    Curio buys a Mythic token -- decode the item bonus ids -- CAPTURE-BLOCKED.
--
--  * instance_template / creature spawns / gameobject spawns for Sporefall (Map 1592) and boss Rotmire
--    (DungeonEncounter 3159). Map 1592 is Directory "DevMapE" (a DEV map shell) -- its world data may not
--    ship at 68887; CONFIRM the map has navigable data before authoring spawns -- CAPTURE-BLOCKED.
--
-- NOTE: the Sporefall flex-Mythic difficulty itself needs NO DB2 hotfix -- Difficulty 233 + MapDifficulty 6168
-- (Map 1592 -> Difficulty 233, MaxPlayers 25, ContentTuning 6119) are already present in the client DB2 @68887,
-- which TrinityCore loads directly. The only core change is DIFFICULTY_MYTHIC_RAID_FLEX = 233 (DBCEnums.h).
-- ---------------------------------------------------------------------------
