-- Relocated from 2026_09_09_00_auth.sql: upstream TrinityCore claimed that filename for TDB
-- 1210.26091 housekeeping in this merge, so the fork's club-finder RBAC moves to _01.

--
-- Club Finder GM command permission.
--
-- cs_clubfinder.cpp has always referenced rbac::RBAC_PERM_COMMAND_CLUB_FINDER, but this branch never
-- declared it and never shipped the row, so the branch did not build standalone. Both halves were
-- being added by hand in the integration branches instead, which is why the same build break kept
-- reappearing on every integration line that merged this branch. Declared here now, on the owning
-- branch, together with the row that makes it grantable.
--
-- Id 1003, checked against the 12.1 integration line before choosing: 886-890, 1000 and 1001 are
-- taken there by other features' auth updates, and feature/commerce holds 1002 (Command: shop) and
-- 1004 (Command: reload shop_catalog).
--
-- Linked into group 196, the reload / GM command group, like the neighbouring command permissions.
-- Idempotent, and scoped strictly to 1003 so it cannot touch anyone else's permission.

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1003, 'Command: clubfinder')
  ON DUPLICATE KEY UPDATE `name` = 'Command: clubfinder';

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 1003;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(196, 1003);
