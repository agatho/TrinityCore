ALTER TABLE `characters`
  ADD COLUMN `timerunningSeasonId` int unsigned NOT NULL DEFAULT '0' AFTER `chromieTimeExpansionId`;
