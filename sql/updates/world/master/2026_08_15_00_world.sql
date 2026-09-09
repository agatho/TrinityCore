--
-- In-game Shop: the Character Boost product.
--
-- A boost is expressed in the existing catalog vocabulary, not a new one: a `shop_product` with a
-- single type-5 (service) deliverable whose `id` is 1 (CharacterBoost, the client's own deliverable
-- type). BattlePayMgr::NeedsEntitlement already routes any service product's purchase into an
-- entitlement instead of an immediate grant, and CMSG_CHARACTER_UPGRADE_START is what spends it.
--
-- SHIPPED DISABLED (`enabled` = 0), on purpose. Applying a boost needs two worldserver.conf options
-- that both default to 0 - Shop.Entitlements.Enabled and Shop.Entitlements.AssignEnabled - plus a
-- deliberate choice of Shop.CharacterBoost.Level. An enabled card on a realm that has not opted in
-- would appear in the Shop and then refuse every purchase with PurchaseResult 57, which is worse than
-- not being there. Turn it on with:
--
--     UPDATE `shop_product` SET `enabled` = 1 WHERE `productId` = 1161;
--     .reload shop_catalog
--
-- PRICE. 1,200,000,000 copper (120,000 gold), following the same retail-dollar mapping the curated
-- catalog already uses throughout - 20,000,000 copper per USD, so the retail $60 boost lands here the
-- way the $10 pets (200,000,000) and the $20 WoW Token (400,000,000) do. Note that a priced product
-- can only be bought by a LOGGED-IN character: at character select there is no gold to take, and the
-- purchase is refused with the client's own ErrorBuyingProductNotAllowedInGlueScreen. Set currency 0
-- and price 0 if the boost should be obtainable from the glue screen as well.
--
-- The id 1161 is retail's own Character Boost id, and it is also the id of the CharacterBoost
-- deliverable that the shipped 12.0.7.68275 catalog template carries (type = 1, boostID = 11) - the
-- same record BattlePayPackets.h documents. Using it keeps our DeliverableID identical to the one the
-- client has already seen for this product rather than inventing a parallel one.
--

DELETE FROM `shop_product_deliverable` WHERE `productId` = 1161;
DELETE FROM `shop_product` WHERE `productId` = 1161;

INSERT INTO `shop_product`
  (`productId`,`enabled`,`name`,`description`,`currency`,`price`,`priceItemId`,`priceItemCount`,`displayPrice`,`displayFlags`,`groupId`,`ordering`,`featured`,`availableFrom`,`availableUntil`,`reqLevel`,`reqFaction`,`hideIfOwned`,`playerConditionId`,`comment`) VALUES
(1161,0,'Character Boost','Instantly raise one character to the boost level, equipped and ready to play.',1,1200000000,0,0,1200000000,0,0,0,0,NULL,NULL,0,-1,0,0,'Services | retail $60.00 | service 1 CharacterBoost | disabled until Shop.Entitlements.* are enabled');

INSERT INTO `shop_product_deliverable` (`productId`,`seq`,`type`,`id`,`count`) VALUES
(1161,0,5,1,1);
