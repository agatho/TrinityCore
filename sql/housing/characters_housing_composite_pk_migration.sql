-- ============================================================================
-- Migration: Change PRIMARY KEY from (id) to (ownerGuid, id)
-- ============================================================================
-- The original schema used `id` as the sole PRIMARY KEY, requiring global
-- uniqueness across ALL players. This caused [1062] Duplicate entry errors
-- when two players' room/decor IDs collided.
--
-- This migration changes to a composite PRIMARY KEY (ownerGuid, id) so that
-- IDs only need to be unique PER PLAYER, not globally.
-- ============================================================================

-- 1. Fix character_housing_decor: change PK from (id) to (ownerGuid, id)
ALTER TABLE `character_housing_decor`
    MODIFY COLUMN `id` BIGINT UNSIGNED NOT NULL COMMENT 'Decor instance ID (unique per owner)',
    DROP PRIMARY KEY,
    ADD PRIMARY KEY (`ownerGuid`, `id`);

-- 2. Fix character_housing_rooms: change PK from (id) to (ownerGuid, id)
ALTER TABLE `character_housing_rooms`
    MODIFY COLUMN `id` BIGINT UNSIGNED NOT NULL COMMENT 'Room instance ID (unique per owner)',
    DROP PRIMARY KEY,
    ADD PRIMARY KEY (`ownerGuid`, `id`);

SELECT 'Schema migration complete: composite primary keys applied.' AS status;
