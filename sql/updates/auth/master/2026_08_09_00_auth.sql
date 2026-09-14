--
-- In-game Shop catalog administration RBAC permissions.
--   1004 = Command: reload shop_catalog  (.reload shop_catalog)
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
-- Idempotent, and scoped strictly to 1002/1004 so it cannot touch anyone else's permission.
--
-- 2026-09-14: moved off 887/888 again. On the 12.1 integration line those ids are already taken
-- by RBAC_PERM_CHANGE_TURN_RATE and RBAC_PERM_COMMAND_CHEAT_DIMINISHINGRETURNS, and 2026_08_26_02
-- inserts 888 = 'Command: cheat diminishingreturns'. Whichever update ran last won, and the other
-- feature's permission silently became the wrong thing. 1002/1003 are free on both lines.

DELETE FROM `rbac_permissions` WHERE `id` IN (1002,1004);
INSERT INTO `rbac_permissions` (`id`,`name`) VALUES
(1004,'Command: reload shop_catalog'),
(1002,'Command: shop');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1002,1004);
INSERT INTO `rbac_linked_permissions` (`id`,`linkedId`) VALUES
(196,1004),
(197,1002);
