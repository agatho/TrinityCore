--
-- In-game Shop catalog administration RBAC permissions.
--   887 = Command: reload shop_catalog  (.reload shop_catalog)
--   888 = Command: shop                 (.shop list/enable/disable/price/window/feature/preview)
--
-- NOT 886/887. Id 886 is "Use commentator mode", claimed first by the commentator feature and already
-- present in the live auth database. The earlier version of this file deleted and re-inserted 886/887,
-- which would have DESTROYED the commentator permission on any realm that had both features. The clash
-- was found and patched twice while assembling the integration lines; it is fixed here, on the owning
-- branch, so it stops coming back.
--
-- Linked into the same groups as the neighbouring reload / GM command permissions so the default GM
-- roles pick them up (196 = reload group, 197 = GM command group).
--
-- Idempotent, and scoped strictly to 887/888 so it cannot touch anyone else's permission.

DELETE FROM `rbac_permissions` WHERE `id` IN (887,888);
INSERT INTO `rbac_permissions` (`id`,`name`) VALUES
(887,'Command: reload shop_catalog'),
(888,'Command: shop');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (887,888);
INSERT INTO `rbac_linked_permissions` (`id`,`linkedId`) VALUES
(196,887),
(197,888);
