--
-- MERGE NOTE (integration/all-systems, 2026-08-09): this filename was independently used by TWO
-- branches with completely unrelated content -- feature/club-finder (RBAC 1001) and feature/commerce
-- (the BattlePay purchase ledger). Resolved as the UNION of both sides' statements; no statement is
-- duplicated between the two blocks. Because the file content changed, the auth updates tracker will
-- see a new hash for an already-applied file and re-run it. Both blocks are safe to re-run: the RBAC
-- block is DELETE+INSERT, and account_battlepay_purchase has never been created on this realm's auth
-- DB (see handover 3d.3), so the DROP TABLE IF EXISTS destroys nothing.

--
-- Block 1 (feature/club-finder): Command: clubfinder RBAC permission.
-- Delete the dependent rows before the permission they reference: rbac_linked_permissions has a
-- foreign key onto rbac_permissions, so removing the parent first fails on any database where the
-- permission already exists.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1001);
DELETE FROM `rbac_permissions` WHERE `id` IN (1001);

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1001, "Command: clubfinder");

INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1001);

--
-- Block 2 (feature/commerce): In-game Shop / BattlePay purchase ledger (account level). Shared home
-- for BOTH the WoW Token branch and the in-game Shop branch: it answers
-- CMSG_BATTLE_PAY_GET_PURCHASE_LIST from real history and gives purchases a PurchaseID that survives
-- restarts. The PurchaseID (`id`) is allocated with the realm id in its high 32 bits so two realms
-- sharing this auth DB never collide.
DROP TABLE IF EXISTS `account_battlepay_purchase`;
CREATE TABLE `account_battlepay_purchase` (
  `id` bigint unsigned NOT NULL COMMENT 'Persistent monotonic PurchaseID sent on the wire; high 32 bits = realm id',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Owning game account',
  `productId` int unsigned NOT NULL DEFAULT '0' COMMENT '0 is a VALID value (e.g. web-checkout purchases)',
  `status` int NOT NULL DEFAULT '0' COMMENT 'BattlepayPurchaseStatus: 6 = done, 4 = failed',
  `resultCode` int NOT NULL DEFAULT '0' COMMENT 'PurchaseResult: 0 = ok',
  `basePrice` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Copper',
  `userPrice` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Copper',
  `timeCreated` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds',
  `walletName` varchar(32) NOT NULL DEFAULT '' COMMENT 'Wallet label; empty on this core, sent record-final',
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop / BattlePay purchase ledger';
