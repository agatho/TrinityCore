--
-- Discord integration (client build 12.1.0) - per-character display-name preference.
-- Backs C_Discord.GetDisplayNameType() / CMSG_DISCORD_SET_DISPLAY_NAME_TYPE. Value is a
-- DiscordDisplayNameType enum (0 = Default, 1 = LastOnline, 2 = GlobalName).
--
CREATE TABLE IF NOT EXISTS `character_discord_settings` (
  `guid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Character global unique identifier',
  `displayNameType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DiscordDisplayNameType enum (0..2)',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-character Discord display-name preference';
