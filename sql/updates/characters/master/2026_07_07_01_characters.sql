--
-- Content tracking: persists the set of map content a character is tracking (CMSG_CONTENT_TRACKING_START/STOP_TRACKING)
-- so it survives relog. Mirrored into the ActivePlayer.TrackedCollectableSources update field on login.
--
DROP TABLE IF EXISTS `character_content_tracking`;
CREATE TABLE `character_content_tracking` (
  `ownerGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `targetType` int NOT NULL DEFAULT '0' COMMENT 'ContentTrackingTargetType of the tracked entry',
  `targetId` int NOT NULL DEFAULT '0' COMMENT 'Id of the tracked target within its type',
  `collectableSourceInfoId` int NOT NULL DEFAULT '0' COMMENT 'Client-resolved CollectableSourceInfo id',
  PRIMARY KEY (`ownerGuid`,`targetType`,`targetId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
