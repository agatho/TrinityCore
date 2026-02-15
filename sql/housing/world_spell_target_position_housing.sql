-- Housing teleport spell destinations
-- Tutorial spells (faction-specific, cast by HandleHousingSvcsStartTutorial)
-- Portal spells (from Portal to Founder's Point / Razorwind Shores game objects in capital cities)

DELETE FROM spell_target_position WHERE ID IN (1258476, 1258484, 1235595, 1235590);
INSERT INTO spell_target_position (ID, EffectIndex, OrderIndex, MapID, PositionX, PositionY, PositionZ, Orientation, VerifiedBuild) VALUES
-- Alliance tutorial spell → Founder's Point (NeighborhoodMap ID=1, MapID=2735)
(1258476, 0, 0, 2735, 3807.76, -160.427, 194.111, 0, 57388),
-- Horde tutorial spell → Razorwind Shores (NeighborhoodMap ID=2, MapID=2736)
(1258484, 0, 0, 2736, 2053.6, 175.468, 175.12, 0, 57388),
-- Portal to Founder's Point (gameobject 543407, GAMEOBJECT_TYPE_SPELLCASTER)
(1235595, 0, 0, 2735, 3807.76, -160.427, 194.111, 0, 57388),
-- Portal to Razorwind Shores (gameobject 543406, GAMEOBJECT_TYPE_SPELLCASTER)
(1235590, 0, 0, 2736, 2053.6, 175.468, 175.12, 0, 57388);
