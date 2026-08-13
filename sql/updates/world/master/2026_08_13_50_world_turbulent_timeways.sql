-- ============================================================================
-- Turbulent Timeways — rotation definition table (feature/turbulent-timeways)
--
-- Ships the server-wide weekly rotation of featured Timewalking expansions that
-- TurbulentTimewaysMgr loads. Realm-safe: the manager tolerates this table being
-- absent or empty (it simply idles). LISTED, NOT APPLIED to the central realm.
--
-- Every numeric id below is DB2-anchored @ 12.0.7.68887 (see
-- C:\dumps\TURBULENT_TIMEWAYS_BLUEPRINT.md §2). Fields left 0 are
-- CAPTURE/RESEARCH-BLOCKED, not fabricated:
--   * HolidayId    — only TBC/WotLK/Cata confirmed via SharedDefines HolidayIds
--                    (559/562/587); the MoP+ Holidays.db2 rows are not yet pinned.
--   * GateWorldStateId — decoded from PlayerCondition.WorldStateExpressionID bytes
--                    for TBC(10276)/WotLK(10279)/MoP(12941)/DF(30129) only.
--   * WeeklyQuestId — only DF(93495)/BfA(88808)/SL(92647) confirmed in QuestV2.
--   * OrderIndex    — canonical expansion order placeholder; the live retail
--                    weekly sequence is RESEARCH-only (not a wire/DB2 proof).
-- ChromieExpansionRecId = sUIChromieTimeExpansionInfoStore id (from feature/chromie-time).
-- ============================================================================

DROP TABLE IF EXISTS `turbulent_timeways_rotation`;
CREATE TABLE `turbulent_timeways_rotation` (
  `OrderIndex`            INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rotation position (0..N-1)',
  `ChromieExpansionRecId` INT          NOT NULL DEFAULT -1 COMMENT 'UIChromieTimeExpansionInfo.db2 id; -1 = none',
  `HolidayId`             INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Holiday.db2 timewalking-weekend id (0 = unconfirmed)',
  `RandomLfgDungeonId`    INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'LFGDungeons.db2 Random Timewalking (X)',
  `GateWorldStateId`      INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'worldstate the client PlayerCondition/WSE reads (0 = unconfirmed)',
  `WeeklyQuestId`         INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'QuestV2 weekly quest id (0 = not captured)',
  `Name`                  VARCHAR(64)  NOT NULL DEFAULT '' COMMENT 'human label',
  PRIMARY KEY (`OrderIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Turbulent Timeways weekly rotation (feature/turbulent-timeways)';

-- OrderIndex, ChromieRecId, HolidayId, RandomLfgDungeonId, GateWorldStateId, WeeklyQuestId, Name
INSERT INTO `turbulent_timeways_rotation`
  (`OrderIndex`,`ChromieExpansionRecId`,`HolidayId`,`RandomLfgDungeonId`,`GateWorldStateId`,`WeeklyQuestId`,`Name`) VALUES
  (0, -1,   0, 2634,     0,     0, 'Classic'),               -- [DB2 LFGDungeons 2634]
  (1,  6, 559,  744, 10276,     0, 'Burning Crusade'),       -- [DB2 LFGDungeons 744]  holiday 559 [SRC]
  (2,  7, 562,  995, 10279,     0, 'Wrath of the Lich King'),-- [DB2 LFGDungeons 995]  holiday 562 [SRC]
  (3,  5, 587, 1146,     0,     0, 'Cataclysm'),             -- [DB2 LFGDungeons 1146] holiday 587 [SRC]
  (4,  8,   0, 1453, 12941,     0, 'Mists of Pandaria'),     -- [DB2 LFGDungeons 1453]
  (5,  9,   0, 1971,     0,     0, 'Warlords of Draenor'),   -- [DB2 LFGDungeons 1971]
  (6, 10,   0, 2274,     0,     0, 'Legion'),                -- [DB2 LFGDungeons 2274]
  (7, 15,   0, 2874,     0, 88808, 'Battle for Azeroth'),    -- [DB2 LFGDungeons 2874] quest 88808 [DB2 QuestV2]
  (8, 14,   0, 3076,     0, 92647, 'Shadowlands'),           -- [DB2 LFGDungeons 3076] quest 92647 [DB2 QuestV2]
  (9, 16,   0, 3143, 30129, 93495, 'Dragonflight');          -- [DB2 LFGDungeons 3143] quest 93495 [DB2 QuestV2]

-- ----------------------------------------------------------------------------
-- Deferred / listed-only (NOT written this pass — needs capture, see blueprint §7):
--   * Timewalking vendor npc_vendor rows (Timewarped Badge 1166 purchases) at the
--     per-expansion vendors (Xydan 255019 / Churbro 239840 / Ta'steld 252687 — world-DB
--     creature ids, RESEARCH-only, unverifiable in DB2).
--   * game_event rows binding HOLIDAY_TURBULENT_TIMEWAYS (1425) + the per-expansion
--     dungeon-event holidays to a calendar window (GameEventMgr holiday scheduling).
--   * The remaining MoP+ Holidays.db2 ids and the WoD/Legion/BfA/SL/Classic gate
--     worldstates (decode + confirm from a live rotation capture).
-- ----------------------------------------------------------------------------
