--
-- AccountStore refund integrity. AccountStore ownership is account-wide, but the currency spent is per-character.
-- Record which character paid (payerGuid) so a refund can only credit that same character - refunding on a different
-- character would mint currency here that was destroyed on another character (an account-wide-ownership +
-- per-character-wallet transfer exploit). Record whether the purchase actually granted the collectible (granted) so a
-- refund never strips a mount/spell the payer already owned from another source. Legacy rows: payerGuid = 0 (exempt
-- from the payer-scope check), granted = 1 (assumed to have granted, matching the prior always-revoke behaviour).
--
ALTER TABLE `battlenet_account_store_purchases`
    ADD COLUMN `payerGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `purchaseTime`,
    ADD COLUMN `granted` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `payerGuid`;
