--
-- SMSG_CACHE_INFO (client 0x49000F, build 12.1.0.69382) - realm defined cache stamps.
--
-- For every row the server sends "<Key>" / "<Value>" inside the packet for "<Prefix>", and the
-- client stores it in the CVar "CACHE-<Prefix>-<Key>" (format string RVA 0x3B6AA20). On the first
-- value that differs from what it stored last time - compared with _stricmp - it throws away the
-- whole cache behind that prefix and re-queries it (handler RVA 0x341AD0).
--
-- The Key space is defined by the server alone: there is exactly one "CACHE-" string in the entire
-- client binary, namely that format string, and no CVar list carries a CACHE- entry. So any stable
-- name works; what matters is only that the Value changes when the data behind the domain changes.
--
-- Prefix must be one of the five the client actually matches (MatchesPrefix bodies at RVA
-- 0x331100..0x331580); every other prefix only writes a CVar and discards no cache at all:
--   WGOB  GameObject cache      WNPC  Creature cache     WQST  Quest cache
--   WPTX  PageText cache        WPTN  Petition cache
--
-- Rows here are sent in ADDITION to the counts the core derives by itself (DB2 row counts, hotfix
-- counts, and the world table row counts for creatures, gameobjects, quests and page texts). Use
-- them where the core has no count of its own - petitions above all - or to force an invalidation
-- after a change the counts cannot see, such as editing an existing row instead of adding one.
--
-- To make a changed Value take effect: bump it, then run `.reload cache_info`. The table is read by
-- ObjectMgr::LoadCacheInfoStamps, which the worldserver calls once at startup and the reload command
-- calls again; without the reload the realm keeps sending the old stamp until it is restarted.
-- SMSG_CACHE_INFO goes out on login, so the new stamp reaches every client that logs in after the
-- reload - already connected sessions keep their cache until their next login either way.
--
-- Key and Value are limited to 63 characters each; the wire field is six bits wide.
--
DROP TABLE IF EXISTS `cache_info`;
CREATE TABLE `cache_info` (
  `Prefix` VARCHAR(4) NOT NULL COMMENT 'Cache domain: WGOB, WNPC, WQST, WPTX or WPTN',
  `Key` VARCHAR(63) NOT NULL COMMENT 'Free choice - becomes the CVar CACHE-<Prefix>-<Key>',
  `Value` VARCHAR(63) NOT NULL DEFAULT '1' COMMENT 'Change this to make clients discard the domain',
  `Comment` VARCHAR(255) DEFAULT NULL,
  PRIMARY KEY (`Prefix`, `Key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='Realm defined SMSG_CACHE_INFO stamps';

DELETE FROM `cache_info` WHERE `Prefix`='WPTN';
INSERT INTO `cache_info` (`Prefix`, `Key`, `Value`, `Comment`) VALUES
('WPTN', 'PetitionGeneration', '1', 'Petitions live in the characters database and change per player, so the core has no static count for this domain. Bump the value by hand to make clients drop their petition cache.');
