-- Per-surface theme columns for character_housing_rooms.
--
-- Previously a single themeId column was shared by walls/floors/ceilings,
-- so dyeing the ceiling's style overwrote the wall's style. Split into
-- three independent theme IDs. New rows default to 0 and the load path
-- seeds them from the legacy themeId so existing houses keep their look.

ALTER TABLE character_housing_rooms
    ADD COLUMN wallThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER ceilingSlot,
    ADD COLUMN floorThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER wallThemeId,
    ADD COLUMN ceilingThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER floorThemeId;
