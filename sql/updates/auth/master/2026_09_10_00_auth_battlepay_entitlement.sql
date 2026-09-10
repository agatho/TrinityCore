--
-- In-game Shop / BattlePay entitlement ledger (account level).
--
-- The C++ side of this table shipped without a schema: LoginDatabase.cpp:32-38 prepares seven
-- statements against `account_battlepay_entitlement` and no CREATE TABLE for it exists anywhere in
-- sql/, so every from-scratch deployment dies at mysql_stmt_prepare with
-- "Table 'auth.account_battlepay_entitlement' doesn't exist".
--
-- An entitlement is one purchased-but-not-yet-delivered product. It is created when a purchase
-- completes (status 1 = available), claimed by a realm/character (status 2, guarded by claimToken),
-- and then consumed (status 3 = pending on that character) until the realm applies it.
--
-- Column types are taken from the prepared statements and their setters in BattlePayMgr.cpp:
--   id              setUInt64  - DistributionID; high 32 bits = realm id, so realms sharing this
--                                auth DB never collide (same scheme as account_battlepay_purchase.id)
--   account         setUInt32  - owning game account
--   productId       setUInt32
--   serviceType     setUInt8
--   status          setUInt8   - 1 available, 2 claimed, 3 pending on targetCharacter
--   purchaseId      setUInt64  - account_battlepay_purchase.id that paid for this entitlement
--   claimToken      setUInt64  - claim-race guard; 0 until claimed
--   realmId         setUInt32  - 0 until claimed
--   targetCharacter setUInt64  - character GUID low part; 0 until claimed
--   createTime      setInt64   - unix seconds
--   updateTime      setInt64   - unix seconds
--
-- The (realmId, targetCharacter) index serves LOGIN_SEL_BATTLEPAY_ENTITLEMENT_PENDING_CHAR, which
-- runs on every character login.
--
DROP TABLE IF EXISTS `account_battlepay_entitlement`;
CREATE TABLE `account_battlepay_entitlement` (
  `id` bigint unsigned NOT NULL COMMENT 'DistributionID sent on the wire; high 32 bits = realm id',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Owning game account',
  `productId` int unsigned NOT NULL DEFAULT '0',
  `serviceType` tinyint unsigned NOT NULL DEFAULT '0',
  `status` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1 available, 2 claimed, 3 pending on targetCharacter',
  `purchaseId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'account_battlepay_purchase.id',
  `claimToken` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Claim-race guard; 0 until claimed',
  `realmId` int unsigned NOT NULL DEFAULT '0' COMMENT '0 until claimed',
  `targetCharacter` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character GUID low part; 0 until claimed',
  `createTime` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds',
  `updateTime` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds',
  PRIMARY KEY (`id`),
  KEY `idx_account_status` (`account`,`status`),
  KEY `idx_pending_char` (`realmId`,`targetCharacter`,`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop / BattlePay entitlement ledger';
