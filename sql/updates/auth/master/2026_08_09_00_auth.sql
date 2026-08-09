--
-- In-game Shop catalog administration RBAC permissions.
--   887 = Command: reload shop_catalog  (.reload shop_catalog)
--   888 = Command: shop                 (.shop list/enable/disable/price/window/feature/preview)
-- Linked into the same groups as the neighbouring reload / GM command permissions so the default
-- GM roles pick them up (196 = reload group, 197 = GM command group).
--
-- MERGE NOTE (integration/all-systems, 2026-08-09): feature/commerce authored these as 886/887.
-- 886 was already taken by "Use commentator mode" (feature/commentator, 2026_07_07_01_auth.sql,
-- already applied to the live auth DB) -- the original form of this file would have DELETEd and
-- overwritten it. Renumbered to 887/888 and RBAC.h renumbered to match.
-- BACK-PORT THIS RENUMBER TO feature/commerce.

DELETE FROM `rbac_permissions` WHERE `id` IN (887,888);
INSERT INTO `rbac_permissions` (`id`,`name`) VALUES
(887,'Command: reload shop_catalog'),
(888,'Command: shop');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (887,888);
INSERT INTO `rbac_linked_permissions` (`id`,`linkedId`) VALUES
(196,887),
(197,888);
