--
-- `.reload cache_info` - reloads the world table `cache_info`, whose rows WorldSession::SendCacheInfo
-- sends in SMSG_CACHE_INFO. Without the permission the command exists but nobody can run it.
--
DELETE FROM `rbac_permissions` WHERE `id`=886;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(886, 'Command: reload cache_info');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId`=886;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 886);
