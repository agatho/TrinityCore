-- Remove static cornerstone spawns; now spawned dynamically by HousingMap::LoadGridObjects
-- Housing maps 2735 (Alliance Founder's Point), 2736 (Horde Razorwind Shores)
-- GO entries: 457142 (Cornerstone), 417487 (For Sale Sign / Plot marker)
DELETE FROM gameobject WHERE map IN (2735, 2736) AND id IN (457142, 417487)
  AND guid BETWEEN 9000000 AND 9999999;
