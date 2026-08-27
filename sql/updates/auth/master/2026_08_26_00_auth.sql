--
-- chat_2C_4A: two custom RBAC permissions (custom range starts at 1000, see RBAC.h).
--
-- 1000 RBAC_PERM_CHAT_MUTED_PARENTAL_CONTROLS
--      Granted per account. While an account has it, WorldSession::HandleChatMessage answers with
--      SMSG_CHAT_IGNORED_ACCOUNT_MUTED (0x4A0000), whose client consumer hardwires GameError 946
--      ERR_PARENTAL_CONTROLS_CHAT_MUTED. Deliberately NOT in any default security group: the
--      ordinary mutetime / GM mute is a different message (SMSG_CHAT_RESTRICTED, ERR_USER_SQUELCHED).
--
-- 1001 RBAC_PERM_COMMAND_RELOAD_CHAT_SPAM_RECORD
--      .reload chat_spam_record. Linked to 196 'Role: Administrator Commands', which is where the
--      reload permissions live: 98 of the 100 'Command: reload ...' permissions in the tree hang
--      there (the two exceptions, 850 scenes and 851 areatrigger_templates, hang directly on 192).
--      196 is not itself a default group - it reaches secId 3 = Administrator through (192,196)
--      in rbac_linked_permissions, and 192 is what rbac_default_permissions gives secId 3.
--      Linking to 192 instead would be effect-equivalent today but would put a command into a
--      sec-level role rather than into the command role.
--
-- INSERT ... ON DUPLICATE KEY UPDATE, deliberately NOT `DELETE` + `INSERT` on `rbac_permissions`.
-- UpdateFetcher reapplies an update file of its own accord once its hash changes
-- (src/server/database/Updater/UpdateFetcher.cpp:276-279, "Reapplying update ... (it changed)"),
-- and this file has already been edited in place once. `rbac_account_permissions`.`permissionId`
-- is a foreign key onto `rbac_permissions`.`id` with ON DELETE CASCADE
-- (sql/base/auth_database.sql:2284), so `DELETE FROM rbac_permissions WHERE id IN (1000, 1001)`
-- takes every per-account grant of 1000 with it. That table is the only carrier of 1000 - the
-- permission sits in no security group by design - so the delete would silently drop exactly the
-- realm state that SMSG_CHAT_IGNORED_ACCOUNT_MUTED exists for. The row is therefore upserted, never
-- recreated; the name is refreshed so a later rename in this file still takes effect.
--
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1000, 'Chat muted by parental controls')
  ON DUPLICATE KEY UPDATE `name` = 'Chat muted by parental controls';

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1001, 'Command: reload chat_spam_record')
  ON DUPLICATE KEY UPDATE `name` = 'Command: reload chat_spam_record';

--
-- Group membership, unlike the account grants above, is owned by this file and may be rebuilt: the
-- only link row any revision of this file has ever written is (192, 1001), replaced here by
-- (196, 1001). The delete is scoped to `linkedId` = 1001 for exactly that reason - 1000 is never
-- linked from here, so a row for it could only be a realm's own addition and is left alone.
--
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 1001;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 1001);
