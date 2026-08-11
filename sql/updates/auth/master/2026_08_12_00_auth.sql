--
-- In-game Shop / BattlePay entitlement ("distribution") store, account level.
--
-- An entitlement is a purchased-but-not-yet-applied product: the retail "own it now, apply it later"
-- model that backs buying at character select and every service product (boost, rename, faction/race
-- change, transfer) whose grant targets a character chosen AFTER the purchase.
--
-- It lives in the AUTH database for the same reason `account_battlepay_purchase` does: the Shop opens
-- at character select, where no character database context exists yet, and an entitlement is owned by
-- the ACCOUNT, not by a character or a realm. `id` doubles as the wire DistributionID and is allocated
-- with the realm id in its high 32 bits so two realms sharing this auth DB never collide, exactly like
-- the purchase ledger's PurchaseID.
--
-- Lifecycle (see BattlePayEntitlements in BattlePayMgr.h):
--   1 AVAILABLE - owned, unassigned. The only status ever sent to the client (the sole value observed
--                 on the wire, 68275 capture).
--   2 CLAIMED   - transient: an assign is in flight and has won the compare-and-swap on `claimToken`.
--   3 BOUND     - assigned to `targetCharacter` on `realmId`; the payload is delivered the next time
--                 that character logs in.
--   4 FINISHED  - delivered; terminal.
--   5 REVOKED   - withdrawn by an admin / refunded; terminal.
--
-- `claimToken` is what makes assignment safe against a replay or a second realm: the claiming session
-- writes a random token together with status 2 under a `status = 1` guard, then reads the row back and
-- proceeds only if ITS token survived. Two racing assigns can therefore never both succeed, without
-- needing an affected-rows count.
DROP TABLE IF EXISTS `account_battlepay_entitlement`;
CREATE TABLE `account_battlepay_entitlement` (
  `id` bigint unsigned NOT NULL COMMENT 'DistributionID sent on the wire; high 32 bits = realm id',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Owning game account',
  `productId` int unsigned NOT NULL DEFAULT '0' COMMENT 'shop_product.productId this entitlement grants',
  `serviceType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 = deferred delivery of the product payload; else the VAS service type from shop_product_deliverable.id where type = 5',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1 available, 2 claimed, 3 bound to a character, 4 finished, 5 revoked',
  `purchaseId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'account_battlepay_purchase.id that created this entitlement',
  `claimToken` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Compare-and-swap token held by the session currently assigning this entitlement',
  `realmId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Realm the target character lives on; 0 while unassigned',
  `targetCharacter` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character guid counter it was assigned to; 0 while unassigned',
  `createTime` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds',
  `updateTime` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds of the last status change',
  PRIMARY KEY (`id`),
  KEY `idx_account_status` (`account`, `status`),
  KEY `idx_pending_delivery` (`realmId`, `targetCharacter`, `status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop / BattlePay entitlements (distributions)';
