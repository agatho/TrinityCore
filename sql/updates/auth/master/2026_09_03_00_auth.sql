--
-- Discord integration (client build 12.1.0) - per-account Discord OAuth link.
-- Written by the bnetserver Discord link store (external OAuth linker) and read by the
-- worldserver to answer CMSG_DISCORD_REFRESH_AUTH (surfaced through ActivePlayerData::DiscordInfo).
-- accountType mirrors the client Enum.DiscordAccountType (0 = Normal, 1 = Provisional).
--
CREATE TABLE IF NOT EXISTS `account_discord` (
  `id` INT UNSIGNED NOT NULL COMMENT 'Account id (auth.account.id)',
  `discordUserId` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'OpaqueDiscordUserID (64-bit snowflake)',
  `discordUserName` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'Discord user/global name',
  `accountType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DiscordAccountType enum (0 Normal, 1 Provisional)',
  `accessToken` VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'Opaque OAuth access token handle',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per-account Discord OAuth link';
