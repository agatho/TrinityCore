--
-- Familie 0x67 (Zauber & Kampflog), Phase A: drei neue Befehlsberechtigungen.
--   886  .cheat diminishingreturns  -> SMSG_CHEAT_IGNORE_DIMISHING_RETURNS (0x670002)
--   887  .debug scriptcast          -> SMSG_SCRIPT_CAST (0x670049)
--   888  .debug worldtext           -> SMSG_DISPLAY_WORLD_TEXT_ON_TARGET (0x670055)
--
-- Gruppenwahl, jeweils nach der Gruppe des NAECHSTEN VERWANDTEN im Basisdump:
--   886 -> 196 'Role: Administrator Commands', wie cheat explore (294), cheat god (295),
--          cheat status (297), cheat taxi (298), cheat waterwalk (299).
--   887, 888 -> 198 'Role: Moderator Commands', wie 'Command: debug' (300) selbst. cs_debug.cpp
--          fuehrt die Mehrzahl seiner Unterbefehle unter RBAC_PERM_COMMAND_DEBUG; wer .debug
--          ausfuehren darf, muss auch diese beiden ausfuehren duerfen.
--
-- 196 ist Administrator, NICHT Gamemaster (197) - RBAC.h:111/112 und auth_database.sql:3114/3115.
-- Die Verknuepfung ist kumulativ: 192 (Sec Level Administrator) -> {193, 196},
-- 193 -> {194, 197}, 194 -> {195, 198}. Was an 198 haengt, erreicht damit auch Gamemaster und
-- Administrator; was an 196 haengt, erreicht nur den Administrator.
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
(198, 887),
(198, 888);
