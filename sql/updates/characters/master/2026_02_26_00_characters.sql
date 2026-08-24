ALTER TABLE `characters`
  ADD COLUMN `chromieTimeExpansionId` tinyint unsigned NOT NULL DEFAULT '0' AFTER `lastLoginBuild`;
