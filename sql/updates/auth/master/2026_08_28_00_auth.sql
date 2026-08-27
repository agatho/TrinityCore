--
-- CMSG_CLEAR_NEW_APPEARANCE (12.1 value 0x2A0005) needs a send side to have anything to clear.
--
-- The client keeps a set of "new" transmog source ids and draws the NEW label plus glow over every wardrobe
-- entry in it (Blizzard_Wardrobe.lua:1126-1131). That set is filled ONLY from the second int vector of
-- SMSG_ACCOUNT_TRANSMOG_UPDATE, and it is emptied one id at a time by CMSG_CLEAR_NEW_APPEARANCE, which the
-- client sends when the player hovers the entry (Blizzard_Wardrobe.lua:1439-1440,
-- Blizzard_TransmogTemplates.lua:1010-1011). The badge therefore has to survive a relog, so the pending set
-- is account state and lives here next to battlenet_item_favorite_appearances, whose shape it copies.
--
-- Ids are ItemModifiedAppearance.ID (the "source id"), the same key space as the favourites table and as the
-- appearance bitmask in battlenet_item_appearances.
--
DROP TABLE IF EXISTS `battlenet_item_new_appearances`;
CREATE TABLE `battlenet_item_new_appearances` (
  `battlenetAccountId` int unsigned NOT NULL,
  `itemModifiedAppearanceId` int unsigned NOT NULL,
  PRIMARY KEY (`battlenetAccountId`,`itemModifiedAppearanceId`),
  CONSTRAINT `fk_battlenet_item_new_appearances` FOREIGN KEY (`battlenetAccountId`) REFERENCES `battlenet_accounts` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
