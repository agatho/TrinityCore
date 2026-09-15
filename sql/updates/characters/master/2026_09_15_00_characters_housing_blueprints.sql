-- Housing blueprints (12.1): saved house, room, interior and exterior layouts, one collection per Battle.net account.
-- `uuid` is what the client turns into the shareable code; `content` is the layout (HousingBlueprintMgr serialization).
DROP TABLE IF EXISTS `account_housing_blueprint`;
CREATE TABLE `account_housing_blueprint` (
    `id` BIGINT UNSIGNED NOT NULL,
    `uuid` CHAR(36) NOT NULL,
    `bnetAccountId` INT UNSIGNED NOT NULL,
    `exporterGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Character that exported it',
    `name` VARCHAR(64) NOT NULL DEFAULT '',
    `type` TINYINT UNSIGNED NOT NULL COMMENT 'HousingBlueprintType: 1 House, 2 Room, 3 Interior, 4 Exterior',
    `flags` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'HousingBlueprintFlag: 1 AutomaticBackup',
    `createTime` BIGINT NOT NULL DEFAULT 0,
    `content` MEDIUMBLOB NOT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_uuid` (`uuid`),
    KEY `idx_bnetAccountId` (`bnetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
