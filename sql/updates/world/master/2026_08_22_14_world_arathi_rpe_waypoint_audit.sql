-- ============================================================================
-- Arathi Catch-Up / RPE -- waypoint audit fix  (2026-08-22)
-- ============================================================================
-- Audited every waypoint path authored for this zone (owner, seating, shape). One real defect:
--
-- Gnoll Bruiser guid 8000024 was switched to MovementType=2 with PathId 9200002, but never MOVED to
-- its path start. It sits at (-960.1,-3510.1,57.1) while node 1 is at (-895.6,-3517.3,70.5) -- 65
-- yards away and 13 yards higher. On spawn it would immediately trek across Hammerfall (and uphill)
-- to reach the start of its patrol, which reads as a mob sliding off to somewhere for no reason.
-- Every OTHER mover in this zone was seated on its route when its path was assigned; this one was
-- missed because it was a pre-existing spawn rather than a newly inserted row.
UPDATE `creature` SET `position_x` = -895.5868, `position_y` = -3517.2900, `position_z` = 70.4694
 WHERE `guid` = 8000024;

-- ---- Audit notes on everything else (no change needed) ----
-- * Paths 2218835 / 2218842 (the Gnoll Way send-off walks) have no creature_addon owner BY DESIGN --
--   they are driven from C++ via MotionMaster::MovePath in npc_arathi_rpe_leader::OnQuestReward, not
--   by a persistent PathId. Their straight-line shape is correct: the leaders walk away, they do not
--   patrol.
-- * Four paths score below the 1.6 confinement threshold the motion classifier uses to separate a
--   patrol from a combat chase: 9200010 Ogre Destroyer (1.1), 9200011 Kobold Pillager (1.4),
--   9200012 Kobold Firetender (1.2), 9200024 Ogre Basher (1.4). They are KEPT, on the grounds that
--   confinement is a weak signal for a SHORT path: these carry 8-17 captured nodes across only
--   14-31 yards, and a combat chase is 2-3 nodes running straight at the player, never a dense slow
--   crawl. Applied as a repeating waypoint path the creature paces back and forth along the route,
--   which is the behaviour the tester asked for. Flagged here so they are the first thing to revisit
--   if any of those four looks like it is walking somewhere strange in game.
-- * No creature on the map has MovementType=2 without a PathId, and no authored path is missing its
--   owner apart from the two C++-driven send-off walks above.
