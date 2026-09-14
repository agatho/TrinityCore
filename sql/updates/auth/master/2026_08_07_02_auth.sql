--
-- Battle.net presence (bgs.protocol.presence.v1 0xFA0796FF / presence.v2.client 0x138D200C)
-- and account-scope block list (bgs.protocol.block_list.v1.client 0x8E8F5FB0).
--
-- 1) `battlenet_game_account_presence`
--    The durable mirror of who is online, on which character, in which zone. The live half is
--    in-process in the worldserver (BnetPresenceMgr); this table exists so a second process - in
--    particular the bnetserver, which shares nothing with the worldserver except this database -
--    can render the character-select / Battle.net app view without an IPC channel.
--
--    Rewritten on session login, session logout, character login, character logout and zone change.
--    Every row is forced isOnline = 0 at worldserver startup, so a crash cannot leave the table
--    claiming players are online.
--
--    NOTE ON FIELD PROVENANCE: none of these columns is serialised into a presence *field*. Both
--    protocol versions carry presence as {key, Variant} pairs behind presence.v1 FieldKey{program,
--    group, field, unique_id} / presence.v2 PresenceFieldKey{title_id, group, field, unique_id}, and
--    that (group, field) numbering is assigned by Blizzard. It is not derivable from the client
--    binary offline and a wrong number renders as a blank line under the friend's name rather than
--    as a visible failure. The only presence fields this server puts on the wire are the ones the
--    client itself authored through presence Update, echoed back verbatim.
--
-- 2) `battlenet_account_blocked`
--    One row per (blocker, blocked) battlenet account pair. This is one scope above the existing
--    per-character ignore list (`character_social`, SOCIAL_FLAG_IGNORED), which is untouched.
--
--    block_list.v1 BlockPlayerForSession is deliberately not stored here: that block is real for the
--    duration of the account's connection and is dropped when its last session closes.
--
-- Field-name provenance: every column that carries a protobuf value is annotated with the exact
-- generated field it maps to, taken from
--   src/server/proto/Client/presence_types.pb.h
--   src/server/proto/Client/api/client/v2/presence_types.pb.h
--   src/server/proto/Client/api/client/v1/block_list_types.pb.h
--
-- Idempotent: safe to re-run.
--

CREATE TABLE IF NOT EXISTS `battlenet_game_account_presence` (
  `gameAccountId`  INT UNSIGNED    NOT NULL,                -- `account`.`id` - one row per game account
  `bnetAccountId`  INT UNSIGNED    NOT NULL DEFAULT 0,      -- `battlenet_accounts`.`id`
  `isOnline`       TINYINT UNSIGNED NOT NULL DEFAULT 0,     -- a WorldSession exists for this game account
  `realmId`        INT UNSIGNED    NOT NULL DEFAULT 0,      -- virtual realm address of the logged-in character
  `characterGuid`  BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- ObjectGuid low part, 0 when at character select
  `characterName`  VARCHAR(24)     NOT NULL DEFAULT '',
  `level`          TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `raceId`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `classId`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `factionId`      TINYINT UNSIGNED NOT NULL DEFAULT 0,     -- TeamId: 0 alliance, 1 horde, 2 neutral
  `zoneId`         INT UNSIGNED    NOT NULL DEFAULT 0,
  `areaId`         INT UNSIGNED    NOT NULL DEFAULT 0,
  `updateTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- unix seconds of the last change
  PRIMARY KEY (`gameAccountId`),
  KEY `idx_bnet` (`bnetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `battlenet_account_blocked` (
  `accountId`        INT UNSIGNED    NOT NULL,              -- the battlenet account doing the blocking
  `blockedAccountId` INT UNSIGNED    NOT NULL,              -- block_list.v1 BlockedPlayer.id
  `blockedBattleTag` VARCHAR(40)     NOT NULL DEFAULT '',   -- block_list.v1 BlockedPlayer.battle_tag,
                                                            -- snapshotted at block time so the entry stays
                                                            -- renderable if the target later loses its tag
  `creationTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,    -- block_list.v1 BlockedPlayer.creation_time_us / 1000000
  `modifiedTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,    -- block_list.v1 BlockedPlayer.modified_time_us / 1000000
  PRIMARY KEY (`accountId`, `blockedAccountId`),
  KEY `idx_blocked` (`blockedAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
