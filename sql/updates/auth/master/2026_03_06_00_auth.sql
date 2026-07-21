-- Add RBAC permission for .chromietime GM command
--
-- Delete the dependent rows before the permission they reference: rbac_default_permissions has a
-- foreign key onto rbac_permissions, so removing the parent first fails on any database where the
-- permission already exists. That made this migration succeed on a fresh database and fail on a
-- re-run or an upgrade.
DELETE FROM `rbac_default_permissions` WHERE `permissionId` = 1000;
DELETE FROM `rbac_permissions` WHERE `id` = 1000;

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES (1000, 'Command: chromietime');

-- Grant to GM role (secLevel 1 = moderator)
INSERT INTO `rbac_default_permissions` (`secId`, `permissionId`) VALUES (2, 1000);
