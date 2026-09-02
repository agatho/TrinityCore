--
-- Account-wide favorite transmog sets (ItemCollectionType::TransmogSetFavorite).
-- Mirrors `battlenet_item_favorite_appearances`; feeds SMSG_ACCOUNT_TRANSMOG_SET_FAVORITES_UPDATE.
--
DROP TABLE IF EXISTS `battlenet_item_favorite_transmog_sets`;
CREATE TABLE `battlenet_item_favorite_transmog_sets` (
  `battlenetAccountId` int unsigned NOT NULL,
  `transmogSetId` int unsigned NOT NULL,
  PRIMARY KEY (`battlenetAccountId`,`transmogSetId`),
  CONSTRAINT `fk_battlenet_item_favorite_transmog_sets` FOREIGN KEY (`battlenetAccountId`) REFERENCES `battlenet_accounts` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
