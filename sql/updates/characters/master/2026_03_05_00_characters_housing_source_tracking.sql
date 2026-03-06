-- Add source tracking columns to character_housing_decor and character_housing_catalog
-- SourceType: DecorSourceType enum (0=Standard, 3=Deferred, 5=Spell, 6=Item)
-- SourceValue: Context string (spell ID, item GUID, etc.)

ALTER TABLE `character_housing_decor`
    ADD COLUMN `sourceType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item' AFTER `placementTime`,
    ADD COLUMN `sourceValue` VARCHAR(128) NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)' AFTER `sourceType`;

ALTER TABLE `character_housing_catalog`
    ADD COLUMN `sourceType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item' AFTER `acquiredTime`,
    ADD COLUMN `sourceValue` VARCHAR(128) NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)' AFTER `sourceType`;
