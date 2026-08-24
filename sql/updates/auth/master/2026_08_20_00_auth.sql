--
-- WoW Token market: record the character that listed a token so its sale proceeds can be mailed to them
-- when the listing is bought (feature/wow-token B3: sell / buy / redeem legs).
ALTER TABLE `account_wow_token`
  ADD COLUMN `seller_guid` bigint unsigned NOT NULL DEFAULT '0'
  COMMENT 'Character lowguid that listed the token; receives the sale proceeds by mail'
  AFTER `createTime`;
