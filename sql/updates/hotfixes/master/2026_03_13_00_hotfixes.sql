--
-- Rename misnamed columns in battle_pet_ability_effect to match WoWDBDefs 12.0 (idempotent)
-- Old (swapped) names were: Aura -> BattlePetEffectPropertiesID, BattlePetEffectPropertiesID -> AuraBattlePetAbilityID, VisualID -> BattlePetVisualID
-- Fresh databases created from 2026_02_20_00_hotfixes already use the corrected names, so this is a
-- no-op there; it only rewrites databases that still carry the old swapped column names.
--

SET @needs_rename = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'battle_pet_ability_effect' AND COLUMN_NAME = 'Aura');

SET @sql = IF(@needs_rename = 1,
    'ALTER TABLE `battle_pet_ability_effect`
        CHANGE COLUMN `BattlePetEffectPropertiesID` `AuraBattlePetAbilityID` smallint unsigned NOT NULL DEFAULT ''0'',
        CHANGE COLUMN `Aura` `BattlePetEffectPropertiesID` smallint unsigned NOT NULL DEFAULT ''0'',
        CHANGE COLUMN `VisualID` `BattlePetVisualID` smallint unsigned NOT NULL DEFAULT ''0''',
    'SELECT 1');

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
