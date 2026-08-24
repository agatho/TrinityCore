--
-- Midnight 12.0.7 small activities: Abyss Anglers
--
-- SHIPS ON feature/midnight-small-activities. NOT applied to the shared realm by
-- this session (central realm is untouchable). Apply via the central update path.
--
-- abyss_angler_dive_template gates AbyssAnglersMgr's reward seam. Presence of >=1 row
-- ENABLES the Angler Pearls (currency 3373) payout + the display-currency mirror
-- (3506) that drives the reward toast. The DIVE itself (Depthdiver Jeju gossip -> the
-- "Abyss Anglers - Vehicle" 1253017/1253021 -> underwater scored scenario -> "Surface!"
-- 1260426) is CAPTURE-BLOCKED and NOT seeded here; Depthdiver Jeju / Tu'nakit are
-- world-DB creatures whose ids are RESEARCH-BLOCKED. This table intentionally carries
-- only enough to switch the reward seam on for a disposable test DB.
--
DROP TABLE IF EXISTS `abyss_angler_dive_template`;
CREATE TABLE `abyss_angler_dive_template` (
  `Id`     INT UNSIGNED NOT NULL COMMENT 'dive id (internal)',
  `ZoneId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Zul''Aman Depths zone (0 = unscoped)',
  `Notes`  VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'provenance / TODO',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Midnight Abyss Anglers dives — DB2 @68887 anchors';

-- One placeholder dive so the reward seam is exercisable. AreaPOI 8584 "Abyss Anglers"
-- ("Speak with Depthdiver Jeju to dive with the Abyss Anglers.") is the world anchor.
INSERT INTO `abyss_angler_dive_template` (`Id`, `ZoneId`, `Notes`) VALUES
(1, 0, 'AreaPOI 8584; dive scenario CAPTURE-BLOCKED; reward seam only');
