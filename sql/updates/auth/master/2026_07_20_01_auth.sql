--
DELETE FROM `rbac_permissions` WHERE `id` IN (1001);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1001, "Command: clubfinder");

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1001);
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1001);
