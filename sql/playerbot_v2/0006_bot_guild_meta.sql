-- Migration: 0006_bot_guild_meta
-- Date:    2026-05-16
-- Purpose: Schema for the bot guild ecosystem (see
--          src/modules/PlayerbotV2/docs/GUILD_PLAN.md).
--          - bot_guild_meta: maps TC's `guild.guildid` to BotGuildMgr
--            metadata (faction, theme, founder, rival, member cap).
--            Distinguishes bot-founded guilds from operator/player
--            guilds so the manager never modifies guilds it didn't
--            create.
--          - bot_guild_name_reserved: in-flight name reservations
--            while a charter FSM is collecting signatures. Prevents
--            two parallel founders from picking the same name.
-- Reverts: yes (DROP TABLE both).

CREATE TABLE IF NOT EXISTS bot_guild_meta (
    guild_id        BIGINT UNSIGNED NOT NULL,
    faction         TINYINT UNSIGNED NOT NULL,
    theme           VARCHAR(32) NOT NULL DEFAULT 'adventurers',
    founder_low     BIGINT UNSIGNED NOT NULL,
    rival_low       BIGINT UNSIGNED DEFAULT NULL,
    member_cap      SMALLINT UNSIGNED NOT NULL DEFAULT 75,
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_event_at   TIMESTAMP NULL DEFAULT NULL,
    PRIMARY KEY (guild_id),
    KEY idx_bot_guild_meta_faction (faction),
    KEY idx_bot_guild_meta_founder (founder_low)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Column docs:
--   guild_id      FK to guild.guildid (CharacterDatabase). Not a real
--                 FK constraint because TC's guild table doesn't carry
--                 one for player rosters and we mirror the convention.
--   faction       0 = Alliance, 1 = Horde. Matches
--                 BotGuildMgr::Faction enum.
--   theme         'adventurers' for the default Phase A.2 guild kind;
--                 'crafters' / 'raiders' for Phase E themed guilds.
--   founder_low   Character guid_low of the bot that founded the
--                 guild. NOT necessarily the current GM (GM rotates
--                 via Phase B hygiene). Used by recently-disbanded
--                 cooldowns and ownership lineage.
--   rival_low     Optional rival guild_id (intra-faction). Set at
--                 creation (Phase A.2 picks the longest-active
--                 existing bot guild of same faction; NULL if first).
--                 Phase E rivalries read this.
--   member_cap    Per-guild membership ceiling. Defaults to 75 from
--                 BotGuildMgr::kDefaultMaxMembersPerGuild but
--                 configurable per guild for special cases.
--   created_at    Insertion time = guild submission time.
--   last_event_at Last scheduled-event start (Phase D). NULL until
--                 first event runs.

CREATE TABLE IF NOT EXISTS bot_guild_name_reserved (
    name            VARCHAR(64) NOT NULL,
    faction         TINYINT UNSIGNED NOT NULL,
    founder_low     BIGINT UNSIGNED NOT NULL,
    reserved_at     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (name),
    KEY idx_bot_guild_name_reserved_founder (founder_low)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Column docs:
--   name          The candidate guild name reserved by the charter
--                 FSM at phase 2 (buy_charter). Released on success
--                 (row deleted when bot_guild_meta row is INSERTed)
--                 OR on FSM abort (founder lost the charter, exceeded
--                 the 30-min total time-to-found budget, etc.).
--   faction       Founder's faction (sanity check + queryability).
--   founder_low   Owner of the reservation. Hygiene cron sweeps
--                 reservations older than 60 min as orphans.
--   reserved_at   For 60-min sweep cutoff.

CREATE TABLE IF NOT EXISTS bot_guild_member_meta (
    guild_id        BIGINT UNSIGNED NOT NULL,
    char_guid_low   BIGINT UNSIGNED NOT NULL,
    joined_at       TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_promoted_at TIMESTAMP NULL DEFAULT NULL,
    PRIMARY KEY (guild_id, char_guid_low),
    KEY idx_bot_gmm_char (char_guid_low),
    KEY idx_bot_gmm_joined (joined_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Column docs:
--   guild_id        FK to guild.guildid (CharacterDatabase).
--   char_guid_low   Bot character guid_low (FK to characters.guid).
--   joined_at       When the bot joined this guild — populated by the
--                   manager whenever Guild::AddMember succeeds for a
--                   bot (charter-signer turn-in, organic recruitment,
--                   etc). Drives the rank ladder hygiene cron
--                   (Initiate <7d, Member <30d, Veteran >30d).
--   last_promoted_at Last rank change; null until first promotion.

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (6, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
