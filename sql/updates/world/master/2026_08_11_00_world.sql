--
-- Housing: cornerstones are just "Cornerstone" - drop the invented faction/plot suffix.
--
-- A tester on Founder's Point (Alliance) reported the cornerstone tooltip reading "(Horde)".
--
-- Our 110 cornerstone templates were named "Cornerstone - Plot N (Alliance)" / "(Horde)" locally, and the
-- suffix was assigned from the OLD integ_hotfixes.neighborhood_plot rows, which had NeighborhoodMapID 1 and 2
-- swapped (see 2026_08_10 work: those stale VerifiedBuild=65617 rows were deleted so the client's own
-- NeighborhoodPlot.db2 is used). Once the correct assignment took effect, every suffix was inverted: all 55
-- entries now on map 1 (Founder's Point, Alliance) read "(Horde)" and all 55 on map 2 read "(Alliance)".
--
-- Rather than swap the suffixes, remove them: retail has no suffix at all. From an actual retail
-- SMSG_QUERY_GAME_OBJECT_RESPONSE (Allow: True, DataSize 182) for a map-2735 cornerstone, in
-- C:/sniff/alliance_housing_start/dumps/dump_12.0.1.65940_2026-02-19_10-35-38_parsed.txt:
--
--     Entry 457142   Type: 48 (UILink)   Display ID: 110660
--     [0] Name: Cornerstone      Icon Name: buy
--     Data: [0]=4 [2]=1 [4]=10 [7]=70 [8]=1266097
--
-- Every other column already matches that response exactly (verified across all 55 map-1 entries: IconName
-- 'buy', Data0 4, Data2 1, Data4 10, Data7 70, Data8 1266097). Only the name was ours.
--
-- Keeping a per-plot suffix would also re-break the moment plot data is re-imported, since the name would have
-- to be kept in sync with an assignment that lives in DB2.
--
UPDATE `gameobject_template` SET `name` = 'Cornerstone'
  WHERE `name` LIKE 'Cornerstone - Plot%' AND `type` = 48 AND `displayId` = 110660;

-- Entry 457142 is the template retail actually spawns for every plot on map 2735 (see the query response
-- above). We already have the row - IconName 'buy', Data0 4, Data8 1266097 all match - but its name was left
-- empty, and no plot currently references it because our spawner uses the per-plot
-- NeighborhoodPlot.CornerstoneGameObjectID as the GO entry instead. Name it correctly either way.
--
-- NOTE for later: retail spawns ONE shared template (457142) per plot and carries the per-plot cornerstone id
-- in a separate field of the create block, identifying the plot through the FJamHousingCornerstone_C fragment
-- (which holds Cost and PlotIndex). Ours works and renders identically; the difference is only noted here.
UPDATE `gameobject_template` SET `name` = 'Cornerstone'
  WHERE `entry` = 457142 AND `type` = 48 AND `displayId` = 110660 AND (`name` IS NULL OR `name` = '');
