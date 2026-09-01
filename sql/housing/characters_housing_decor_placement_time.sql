-- Add placement_time column to character_housing_decor
-- Tracks when each decoration was placed, used for the refund window (2 hours)
-- Existing rows default to 0 (no refund eligibility for pre-existing placements)

ALTER TABLE `character_housing_decor`
    ADD COLUMN `placementTime` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when decor was placed (for refund window)'
    AFTER `locked`;
