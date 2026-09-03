-- Housing "singleton" opcodes (build 12.1.0.69497):
--   CMSG_HOUSING_DECOR_SET_PET  -> petGuid/petFlag columns on placed decor
--   CMSG_HOUSING_SVCS_HOUSE_FINDER_IGNORE_NEIGHBORHOOD -> per-player ignore list

-- 1. Battle-pet binding on placed decor.
ALTER TABLE `character_housing_decor`
    ADD COLUMN `petGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Battle pet counter bound to this decor slot (0 = none), HighGuid::BattlePet' AFTER `sourceValue`,
    ADD COLUMN `petFlag` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Client-sent flag accompanying the pet binding (CMSG_HOUSING_DECOR_SET_PET)' AFTER `petGuid`;

-- 2. House-finder per-player ignored neighborhood list.
CREATE TABLE IF NOT EXISTS `character_housing_ignored_neighborhood` (
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player character GUID counter',
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Ignored neighborhood GUID counter',
    PRIMARY KEY (`ownerGuid`, `neighborhoodGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
