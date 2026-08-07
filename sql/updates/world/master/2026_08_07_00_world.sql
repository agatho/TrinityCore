--
-- Delve completion trigger: the creature whose death completes the delve (0 = none; scenario path only).
--
ALTER TABLE `delve_template` ADD `finalBossEntry` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Creature entry whose death completes the delve' AFTER `worldState26903`;
