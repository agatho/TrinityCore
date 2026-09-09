-- 2026-09-09: feature/commerce RBAC_PERM_COMMAND_SHOP renumbered 1000 -> 1002 at integration
-- (1000/1001 are taken by parental-controls chat mute / reload chat_spam_record).
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1002, 'Command: shop')
  ON DUPLICATE KEY UPDATE `name` = 'Command: shop';

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 1002;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 1002);
