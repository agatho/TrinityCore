--
-- INTEGRATION NOTE: this filename is deliberately shared. The Discord block below belongs to
-- feature/discord, the In-game Shop boost block to feature/commerce. The integration branch must
-- carry the UNION of both halves; taking one side at a merge silently deletes the other feature's
-- schema. Both blocks are CREATE TABLE IF NOT EXISTS, so applying them together is idempotent.
--

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

--
-- In-game Shop character boost: which characters this realm has boosted.
--
-- The boost itself is applied to an OFFLINE character from the glue screen, so `characters` already
-- carries everything the boost changed (level, xp, primarySpecialization, inventory, the
-- character-select equipment cache). What that leaves nowhere to live is the FACT of the boost, and
-- the character-enumeration packet needs it: a boosted character must carry
-- CHARACTER_FLAG_4_USED_MAX_LEVEL_BOOST so the client stops offering the boost again, and a character
-- created through "Try New Class" must carry CHARACTER_RESTRICTION_FLAG_TRIAL_BOOST until it is
-- boosted so the client draws the class-trial plate.
--
-- This is a CHARACTERS-database table, unlike the entitlement ledger it is fed from
-- (account_battlepay_entitlement, auth DB). The entitlement belongs to the account and outlives any
-- realm; this row is a property of one character on one realm and dies with it, which is why
-- CHAR_DEL_SHOP_BOOST is issued from the character-delete path.
--
--   trial = 1  character was created through Try New Class and has NOT been boosted yet
--   trial = 0  character has been boosted; the entitlement that paid for it is consumed
--
-- `distributionId` is the entitlement (wire DistributionID) that paid for the boost, kept so a
-- support query can walk a boosted character back to the purchase that produced it. It is 0 for a
-- trial row, because a trial does not consume an entitlement.
--

CREATE TABLE IF NOT EXISTS `character_shop_boost` (
  `guid`             BIGINT UNSIGNED  NOT NULL,
  `productId`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `distributionId`   BIGINT UNSIGNED  NOT NULL DEFAULT 0,
  `specializationId` INT UNSIGNED     NOT NULL DEFAULT 0,
  `trial`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `boostedAt`        BIGINT           NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='In-game Shop: boosted and class-trial characters';
