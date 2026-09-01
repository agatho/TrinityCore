-- Remove fabricated "For Sale" sign (entry 417487, displayId 8206)
-- This entry does not exist in retail. Retail uses only the Cornerstone GO (entry 457142,
-- displayId 110660, type 48/UILink) for ALL plots, with GOState toggling ownership:
--   GOState 0 (ACTIVE) = For Sale / unoccupied
--   GOState 1 (READY) = Owned / occupied
-- DisplayID 8206 is a vanilla-era model that renders as a flat brown rectangle.

DELETE FROM `gameobject_template` WHERE `entry` = 417487;
