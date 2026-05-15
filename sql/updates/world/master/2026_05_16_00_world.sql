--
-- Phase 10D - register all post-DF Major Factions as warband-shared.
--
-- Warband Phase 4 (commit c96412ad97) seeded `warband_reputation_faction`
-- with the four TWW 11.0-launch majors (2570, 2590, 2594, 2600). Phase 10
-- extends this to:
--   * the six Dragonflight 10.x majors that retroactively became
--     account-wide in TWW 11.0 (2503/2507/2510/2511/2564/2574),
--   * the four 11.1+ majors (2653/2658/2685/2688),
--   * the five Midnight 12.x majors (2696/2699/2704/2710/2792).
--
-- The Plunderstorm renown faction (2616 Keg Leg Thrasher) is intentionally
-- excluded: it uses a separate renown track (RenownRewardsPlunderstorm.db2)
-- and is mode-bound (Plunderstorm appearances vs Dragonflight-world
-- appearances), so it is never warband-shared.
--
-- Source for retro account-wide list: Warbands rollout, TWW 11.0.0 patch
-- notes (October 2024); per-faction confirmation in
-- C:\dumps\MAJORFACTIONS_DATA_*.md files.
--

INSERT IGNORE INTO `warband_reputation_faction` (`factionId`, `comment`) VALUES
-- Dragonflight 10.x (retroactively account-wide with TWW launch)
(2503, 'Maruuk Centaur'),
(2507, 'Dragonscale Expedition'),
(2510, 'Valdrakken Accord'),
(2511, 'Iskaara Tuskarr'),
(2564, 'Loamm Niffen'),
(2574, 'Dream Wardens'),
-- The War Within 11.1 / 11.2
(2653, 'The Cartels of Undermine'),
(2658, 'The K''aresh Trust'),
(2685, 'Gallagio Loyalty Rewards Club'),
(2688, 'Flame''s Radiance'),
-- Midnight 12.x
(2696, 'Amani Tribe'),
(2699, 'The Singularity'),
(2704, 'Hara''ti'),
(2710, 'Silvermoon Court'),
(2792, 'Ritual Sites');
