--
-- In-game Shop catalog administration RBAC permissions.
--   1003 = Command: reload shop_catalog  (.reload shop_catalog)
--   1002 = Command: shop                 (.shop list/enable/disable/price/window/feature/preview)
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
-- Idempotent, and scoped strictly to 1002/1003 so it cannot touch anyone else's permission.
--
-- 2026-09-14: moved off 887/888 for the third and last time. The two earlier moves picked ids
-- that were free on THIS branch and occupied on the integration line, which is why the clash
-- kept coming back. Checked against the 12.1 line before choosing: 887, 888, 889, 890, 1000 and
-- 1001 are all inserted there by other features' auth updates; 1002/1003 are free on both.

DELETE FROM `rbac_permissions` WHERE `id` IN (1002,1003);
INSERT INTO `rbac_permissions` (`id`,`name`) VALUES
(1003,'Command: reload shop_catalog'),
(1002,'Command: shop');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1002,1003);
INSERT INTO `rbac_linked_permissions` (`id`,`linkedId`) VALUES
(196,1003),
(197,1002);
