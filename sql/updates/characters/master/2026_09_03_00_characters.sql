--
-- Guild rename cluster (12.1): per-guild rename history used by GuildRenameMgr for the
-- post-rename cooldown and the time-boxed refund of the last paid rename.
--
DROP TABLE IF EXISTS `guild_rename`;
CREATE TABLE `guild_rename` (
  `guildid` bigint unsigned NOT NULL DEFAULT '0',
  `previousName` varchar(24) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `costPaid` bigint unsigned NOT NULL DEFAULT '0',
  `renameTime` bigint unsigned NOT NULL DEFAULT '0',
  `refunded` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild Rename History';
