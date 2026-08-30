--
DELETE FROM `rbac_permissions` WHERE `id` = 887;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES (887, "Can change the turn rate with CMSG_MOVE_SET_TURN_RATE_CHEAT");

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 887;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(194, 887);
