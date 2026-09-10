--
-- CMSG_SET_PET_FAVORITE: persist the stable "favorite" star per pet.
-- Mirrored at runtime into ActivePlayerData.PetStable.Pets[].PetFlags (PET_STABLE_FAVORITE).
--
-- Guarded: the base dump already carries the column, so a fresh install must skip the ALTER.
--
SET @col_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_pet' AND COLUMN_NAME = 'favorite');
SET @query = IF(@col_exists = 0,
    'ALTER TABLE `character_pet` ADD COLUMN `favorite` TINYINT UNSIGNED NOT NULL DEFAULT ''0'' AFTER `specialization`',
    'SELECT 1');
PREPARE stmt FROM @query;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
