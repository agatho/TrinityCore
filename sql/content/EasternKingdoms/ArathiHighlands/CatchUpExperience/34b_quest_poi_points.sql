-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_poi_points (pin coords)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)   client uiMapID 2451 (see 34_quest_poi.sql banner for source/caveats
--   -- these points back the pins authored there, one (Idx1=1, Idx2=0) point per quest).
-- X/Y/Z are integer world coordinates (schema: `X`/`Y`/`Z` int, matching the captured
--   oracle rows' convention of plain truncated-int world coords, NOT a UiMap 0-100 scale).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is (QuestID,Idx1,Idx2)).
-- ============================================================================
--
-- Coordinate sources (plan Part 1.7 POIs + Part 1.3 objective clusters; Z authored from
-- Part 1.7 where given, else approximated from the nearest sourced Z -- flagged inline):
--   Hammerfall                    (-991.6, -3528.3,  56.6)  Part 1.7
--   Go'shek/Dabyrie's Farmstead   (-1090.3, -2843.2, 42.2)  Part 1.7
--   Stromgarde Keep               (-1675.4, -1802.7, 80.0)  Part 1.7
--   Catapults cluster             (-1874, -1339, [Z not captured -- approximated 80 from
--                                  the nearest sourced Z, Stromgarde Keep])  Part 1.3
--   Jaina @ siege battlefield     (-1812, -1568, [Z not captured -- approximated 80])  Part 1.3

INSERT INTO `quest_poi_points` (`QuestID`, `Idx1`, `Idx2`, `X`, `Y`, `Z`) VALUES
 (90882, 1, 0, -992,  -3528, 57),  -- Hammerfall
 (90883, 1, 0, -1090, -2843, 42),  -- Go'shek/Dabyrie's Farmstead
 (90885, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90886, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90887, 1, 0, -1090, -2843, 42),  -- Go'shek farm
 (90888, 1, 0, -1675, -1803, 80),  -- Stromgarde Keep
 (90893, 1, 0, -1675, -1803, 80),  -- Stromgarde siege battlefield (Keep coords reused, no finer capture)
 (90895, 1, 0, -1874, -1339, 80),  -- Catapults cluster (Z approximated)
 (90896, 1, 0, -1675, -1803, 80),  -- siege battlefield (Keep coords reused, no finer capture)
 (90897, 1, 0, -1812, -1568, 80),  -- Jaina @ siege battlefield cluster (Z approximated)
 (90911, 1, 0, -1812, -1568, 80)   -- Jaina hub, same spot as 90897
ON DUPLICATE KEY UPDATE `X`=VALUES(`X`), `Y`=VALUES(`Y`), `Z`=VALUES(`Z`);
