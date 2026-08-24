--
-- Midnight 12.0.7 small activities: Ritual Sites
--
-- SHIPS ON feature/midnight-small-activities. NOT applied to the shared realm by
-- this session (central realm is untouchable). Apply via the central update path.
--
-- ritual_site_template drives RitualSiteMgr. Each row is one open-world Ritual Site
-- (a "dangerous ritual") keyed by its DB2 AreaPOI id. Presence of >=1 row ENABLES the
-- reward seam (renown currency 3428 + faction-2792 rep + title 1291 "Ritual Breaker").
-- The ritual encounter itself (creatures, dark obelisks, Lady Darkglen's device, the
-- quests "Dark Obelisk Investigation" / "Manifested Density" / "Thin Their Ranks" /
-- "Raising Magical Alarms") is world-DB content whose ids are RESEARCH-BLOCKED and are
-- NOT seeded here — this table seeds only the site identities the reward seam keys on.
--
DROP TABLE IF EXISTS `ritual_site_template`;
CREATE TABLE `ritual_site_template` (
  `AreaPoiId`    INT UNSIGNED NOT NULL COMMENT 'DB2 AreaPOI id of the site',
  `ZoneId`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Midnight zone id (0 = unscoped)',
  `WorldStateId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'optional worldstate for the site banner',
  PRIMARY KEY (`AreaPoiId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Midnight Ritual Sites — DB2 @68887 anchors';

-- Seed the two named Ritual Site AreaPOIs confirmed in DB2 @68887:
--   8614 "Ritual Site: Broken Throne"    ("A dangerous ritual is being conducted nearby.")
--   8615 "Ritual Site: Daggerspine Point" ("A dangerous ritual is being conducted nearby.")
-- ZoneId left 0 (DB2 AreaPOI carries a map/area atlas, not a zone id; fill when known).
INSERT INTO `ritual_site_template` (`AreaPoiId`, `ZoneId`, `WorldStateId`) VALUES
(8614, 0, 0),
(8615, 0, 0);
