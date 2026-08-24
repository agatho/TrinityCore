--
-- Columns added by integrated features that were only ever written into
-- sql/base/characters_database.sql. The updater applies the base schema solely when creating a
-- database from scratch, so upgraded databases never received them and the affected prepared
-- statements failed at startup.
--
ALTER TABLE `character_covenant` ADD COLUMN `covenantId` int unsigned NOT NULL DEFAULT '0' AFTER `guid`;
ALTER TABLE `character_covenant` ADD COLUMN `soulbindId` int unsigned NOT NULL DEFAULT '0';
