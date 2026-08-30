--
DELETE FROM `rbac_permissions` WHERE `id` IN (891);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(891, "Use commentator mode");

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (891);
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 891);
