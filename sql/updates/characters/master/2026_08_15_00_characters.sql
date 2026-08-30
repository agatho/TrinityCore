--
-- Discord integration (client build 12.1.0.69299): per-guild link + settings.
-- DiscordGuildSettings bitmask recovered from the client enum registrar (RVA 0xE3F030);
-- currently one bit: SeparateStream = 0x1.
--
CREATE TABLE IF NOT EXISTS `guild_discord_settings` (
  `guildid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Guild Identificator',
  `settings` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DiscordGuildSettings bitmask',
  `discordGuildId` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Linked Discord server (guild) id',
  `discordChannelId` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Linked Discord channel id',
  PRIMARY KEY (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-guild Discord integration link and settings';
