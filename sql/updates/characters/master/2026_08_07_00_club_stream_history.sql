-- Club stream history + real view markers (BNET_AND_BACKLOG_PLAN §D5, P1 #13).
--
-- Guild chat and officer chat are club streams (club id == guild id, stream id 1 = Guild, 2 = Officer).
-- Until now nothing was stored, so club.v1 GetStreamHistory had nothing to answer with and the
-- Communities scrollback was empty on every relog, while AdvanceStreamViewTime / SetStreamFocus
-- returned a bare ERROR_OK so unread badges never survived a session.
--
-- These tables live in the CHARACTER database because every guild table already does (guild,
-- guild_member, club_finder_posting, ...) and both the club id and the member ids are character-schema
-- keys. The auth database is shared between realms and holds no guild data, so a club id would be
-- ambiguous there.
--
-- Time units: epoch is bgs.protocol.MessageId.epoch, MICROSECONDS since the unix epoch - the unit
-- ClubService has always minted for MessageId.epoch and ContentChain.edit_time. lastViewTime is
-- bgs.protocol.ViewMarker.last_read_time and uses the same unit so the two are directly comparable.
-- createdTime is plain unix SECONDS and exists only so age based retention can be a cheap indexed scan.

CREATE TABLE IF NOT EXISTS `club_message` (
  `clubId`          BIGINT UNSIGNED NOT NULL,                -- guild id
  `streamId`        BIGINT UNSIGNED NOT NULL,                -- ClubStreamType: 1 = Guild, 2 = Officer
  `epoch`           BIGINT UNSIGNED NOT NULL,                -- MessageId.epoch, microseconds since unix epoch
  `position`        BIGINT UNSIGNED NOT NULL,                -- MessageId.position, monotonic per (clubId, streamId)
  `authorAccountId` INT UNSIGNED    NOT NULL DEFAULT 0,      -- club.v1.MemberId.account_id of the author
  `authorGuid`      BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- characters.guid of the author
  `content`         TEXT            NOT NULL,
  `createdTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- unix seconds, retention only
  PRIMARY KEY (`clubId`, `streamId`, `epoch`, `position`),
  KEY `idx_created` (`createdTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Club (guild/officer) stream scrollback - ClubStreamHistoryMgr';

CREATE TABLE IF NOT EXISTS `club_stream_view_marker` (
  `clubId`       BIGINT UNSIGNED NOT NULL,
  `streamId`     BIGINT UNSIGNED NOT NULL,
  `memberGuid`   BIGINT UNSIGNED NOT NULL,                   -- characters.guid of the member
  `lastViewTime` BIGINT UNSIGNED NOT NULL DEFAULT 0,         -- ViewMarker.last_read_time, microseconds
  PRIMARY KEY (`clubId`, `streamId`, `memberGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per member per stream read marker - ClubStreamHistoryMgr';

-- The mention view time is a single value per member, not one per club or stream, because the client's
-- club_membership.v1 AdvanceStreamMentionViewTime request carries no fields at all and
-- ClubMembershipState.mention_view is likewise a single ViewMarker for the whole subscription.
CREATE TABLE IF NOT EXISTS `club_mention_view_marker` (
  `memberGuid`   BIGINT UNSIGNED NOT NULL,
  `lastViewTime` BIGINT UNSIGNED NOT NULL DEFAULT 0,         -- ViewMarker.last_read_time, microseconds
  PRIMARY KEY (`memberGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Per member @mention read marker - ClubStreamHistoryMgr';

CREATE TABLE IF NOT EXISTS `club_member_mention` (
  `clubId`          BIGINT UNSIGNED NOT NULL,
  `streamId`        BIGINT UNSIGNED NOT NULL,
  `memberGuid`      BIGINT UNSIGNED NOT NULL,                -- the mentioned member
  `epoch`           BIGINT UNSIGNED NOT NULL,                -- identifies the club_message row
  `position`        BIGINT UNSIGNED NOT NULL,
  `authorGuid`      BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `authorAccountId` INT UNSIGNED    NOT NULL DEFAULT 0,
  `createdTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- unix seconds, retention only
  PRIMARY KEY (`clubId`, `streamId`, `memberGuid`, `epoch`, `position`),
  KEY `idx_member` (`memberGuid`, `epoch`),
  KEY `idx_created` (`createdTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Resolved @name mentions - ClubStreamHistoryMgr';
