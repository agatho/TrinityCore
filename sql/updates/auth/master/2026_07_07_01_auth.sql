--
DELETE FROM `rbac_permissions` WHERE `id` IN (886);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(886, "Use commentator mode");

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (886);
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 886);
