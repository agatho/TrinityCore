-- Migration: Rename wallpaperId/materialId to per-component-type texture fields
-- Date: 2026-04-13
-- Reason: Sniff analysis shows wall/floor/ceiling textures are stored independently.
--         Old single wallpaperId applied to all surfaces; now we have per-type fields.
--
-- Old columns: wallpaperId (wall texture), materialId (was misused as shared texture)
-- New columns: wallTextureId, floorTextureId, ceilingTextureId, colorOverride
--
-- Compatible with MySQL 9.4 (no IF NOT EXISTS on columns).
-- Safe to run multiple times: uses stored procedure with column existence checks.

DELIMITER //

DROP PROCEDURE IF EXISTS `housing_texture_migration`//
CREATE PROCEDURE `housing_texture_migration`()
BEGIN
    -- Step 1: Add new columns if they don't exist
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'wallTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `wallTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `themeId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'floorTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `floorTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `wallTextureId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'ceilingTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `ceilingTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `floorTextureId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'colorOverride') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `colorOverride` INT NOT NULL DEFAULT -1 AFTER `ceilingTextureId`;
    END IF;

    -- Step 2: Migrate old data if old columns still exist
    -- wallpaperId was misread (got ColorOverride=-1 due to field-swap bug)
    -- materialId accidentally captured the real texture ID
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'materialId') THEN
        UPDATE `character_housing_rooms`
            SET `wallTextureId` = `materialId`
            WHERE `wallTextureId` = 0 AND `materialId` > 0;
    END IF;

    -- Step 3: Drop old columns if they exist
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'wallpaperId') THEN
        ALTER TABLE `character_housing_rooms` DROP COLUMN `wallpaperId`;
    END IF;

    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'materialId') THEN
        ALTER TABLE `character_housing_rooms` DROP COLUMN `materialId`;
    END IF;
END//

DELIMITER ;

CALL `housing_texture_migration`();
DROP PROCEDURE IF EXISTS `housing_texture_migration`;
