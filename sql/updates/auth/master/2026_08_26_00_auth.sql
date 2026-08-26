--
-- Familie 0x67 (Zauber & Kampflog), Phase A: drei neue Befehlsberechtigungen.
--   886  .cheat diminishingreturns  -> SMSG_CHEAT_IGNORE_DIMISHING_RETURNS (0x670002)
--   887  .debug scriptcast          -> SMSG_SCRIPT_CAST (0x670049)
--   888  .debug worldtext           -> SMSG_DISPLAY_WORLD_TEXT_ON_TARGET (0x670055)
-- Alle drei haengen an Gruppe 196 (Gamemaster), wie die benachbarten cheat-/debug-Befehle.
DELETE FROM `rbac_account_permissions` WHERE `permissionId` IN (886, 887, 888);
DELETE FROM `rbac_default_permissions` WHERE `permissionId` IN (886, 887, 888);
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (886, 887, 888);
DELETE FROM `rbac_permissions` WHERE `id` IN (886, 887, 888);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(886, 'Command: cheat diminishingreturns'),
(887, 'Command: debug scriptcast'),
(888, 'Command: debug worldtext');

INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 886),
(196, 887),
(196, 888);
