--
-- Delete the dependent rows before the permission they reference: rbac_linked_permissions has a
-- foreign key onto rbac_permissions, so removing the parent first fails on any database where the
-- permission already exists.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1001);
DELETE FROM `rbac_permissions` WHERE `id` IN (1001);

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1001, "Command: clubfinder");

INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1001);
