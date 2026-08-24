--
-- Battle.net Friends v2 (bgs.protocol.friends.v2.client.FriendsService, original hash 0x5869BE8C).
--
-- The durable half of the friend graph. The live half is in-process in the worldserver
-- (BnetFriendsMgr), which loads these tables at startup and writes back through them.
--
-- Both the bnetserver socket and the game socket tunnel (CMSG_BATTLENET_REQUEST -> Battlenet::
-- WorldserverServiceDispatcher) reach this service, and the client asks for the friend list at
-- STATUS_AUTHED - i.e. at character select, before any Player exists - so the graph must live in
-- the auth database, not in characters.
--
-- 1) `battlenet_accounts`.`battle_tag` / `battle_tag_disc`
--    A BattleTag is the only handle the client can use to address another account
--    (friends.v2 SendInvitationTarget.battle_tag). TrinityCore has never had one. Stored split so
--    the (name, discriminator) pair can be uniquely constrained and so a free discriminator can be
--    allocated per name; the wire form is composed as "Name#DDDD".
--    Accounts without a BattleTag are given one derived from their email local part on first load.
--
-- 2) `battlenet_account_friend`
--    One row per direction of a friendship. Two rows exist for every accepted invitation.
--    `note` is genuinely per-direction in friends.v2 (UpdateFriendStateOptions.note is applied to
--    the caller's own view of the friend), which is why the note is not on a single shared edge.
--
-- 3) `battlenet_account_friend_invite`
--    Pending invitations. Ids are allocated by the worldserver rather than by AUTO_INCREMENT
--    because friends.v2 requires the invitation id synchronously, inside the RPC handler, to fill
--    SentInvitationAddedNotification / ReceivedInvitationAddedNotification.
--
-- Field-name provenance: every column that carries a protobuf value is annotated with the exact
-- generated field it maps to, taken from
--   src/server/proto/Client/api/client/v2/friends_types.pb.h
--   src/server/proto/Client/api/client/v2/friends_service.pb.h
--
-- Idempotent: safe to re-run.
--

--
-- 1) BattleTag columns on battlenet_accounts
--

SET @col := (SELECT COUNT(*) FROM information_schema.COLUMNS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battlenet_accounts' AND COLUMN_NAME = 'battle_tag');
SET @sql := IF(@col = 0,
    'ALTER TABLE `battlenet_accounts` ADD COLUMN `battle_tag` VARCHAR(32) NULL DEFAULT NULL',
    'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @col := (SELECT COUNT(*) FROM information_schema.COLUMNS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battlenet_accounts' AND COLUMN_NAME = 'battle_tag_disc');
SET @sql := IF(@col = 0,
    'ALTER TABLE `battlenet_accounts` ADD COLUMN `battle_tag_disc` SMALLINT UNSIGNED NULL DEFAULT NULL',
    'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- The (name, discriminator) pair is what a player types; it must be unique.
SET @idx := (SELECT COUNT(*) FROM information_schema.STATISTICS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battlenet_accounts' AND INDEX_NAME = 'uk_battletag');
SET @sql := IF(@idx = 0,
    'ALTER TABLE `battlenet_accounts` ADD UNIQUE KEY `uk_battletag` (`battle_tag`, `battle_tag_disc`)',
    'DO 0');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

--
-- 2) The friend graph
--

CREATE TABLE IF NOT EXISTS `battlenet_account_friend` (
  `accountId`    INT UNSIGNED    NOT NULL,                  -- owner of this view of the friendship
  `friendId`     INT UNSIGNED    NOT NULL,                  -- friends.v2 Friend.id
  `level`        INT UNSIGNED    NOT NULL DEFAULT 0,        -- friends.v2 Friend.level - echoed from the client's own
                                                            -- SendInvitationOptions.level / AcceptInvitationOptions.level,
                                                            -- never synthesised (the numbering is Blizzard's, not ours)
  `note`         VARCHAR(128)    NOT NULL DEFAULT '',       -- friends.v2 Friend.note / UpdateFriendStateOptions.note
  `titleTags`    VARCHAR(255)    NOT NULL DEFAULT '',       -- friends.v2 Friend.title_tag.ids, comma separated
  `creationTime` BIGINT UNSIGNED NOT NULL DEFAULT 0,        -- friends.v2 Friend.creation_time_s (unix seconds)
  PRIMARY KEY (`accountId`, `friendId`),
  KEY `idx_friend` (`friendId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- 3) Pending invitations
--

CREATE TABLE IF NOT EXISTS `battlenet_account_friend_invite` (
  `id`             BIGINT UNSIGNED NOT NULL,                -- friends.v2 ReceivedInvitation.id / SentInvitation.id
                                                            -- and RevokeInvitationRequest/AcceptInvitationRequest/
                                                            -- IgnoreInvitationRequest.invitation_id
  `senderId`       INT UNSIGNED    NOT NULL,                -- resolves to ReceivedInvitation.inviter (UserDescription)
  `targetId`       INT UNSIGNED    NOT NULL,                -- resolved recipient bnet account id
  `targetTag`      VARCHAR(40)     NOT NULL DEFAULT '',     -- friends.v2 SentInvitation.target_name (what the sender typed)
  `level`          INT UNSIGNED    NOT NULL DEFAULT 0,      -- friends.v2 SendInvitationOptions.level, echoed back on both
                                                            -- ReceivedInvitation.level and SentInvitation.level
  `note`           VARCHAR(128)    NOT NULL DEFAULT '',     -- friends.v2 SendInvitationOptions.note / SentInvitation.note
  `titleTags`      VARCHAR(255)    NOT NULL DEFAULT '',     -- friends.v2 SentInvitation.title_tag.ids, comma separated
                                                            -- (ReceivedInvitation has no title_tag field, so this is
                                                            --  only ever echoed back to the sender)
  `creationTime`   BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- friends.v2 *Invitation.creation_time_s (unix seconds)
  `expirationTime` BIGINT UNSIGNED NOT NULL DEFAULT 0,      -- friends.v2 ReceivedInvitation.expiration_time_s
  PRIMARY KEY (`id`),
  KEY `idx_sender` (`senderId`),
  KEY `idx_target` (`targetId`),
  UNIQUE KEY `uk_pair` (`senderId`, `targetId`)             -- one outstanding invitation per ordered pair
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
