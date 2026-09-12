--
-- Delve exit map (REPORT.md 1.5 / 5, work item 11). The exit coordinates in delve_template had no map: the branch
-- hardcoded map 2552 (Khaz Algar) in go_leave_delve, wrong for every Midnight delve. Retail returns the player to the
-- exit coordinates on the map the delve was entered from: The Gulf of Memory -> Harandar 2694 (gulf NEW_WORLD 1129673:
-- 46.226, 810.912, 1109.843), The Shadow Enclave -> map 0 (eversong 4780.455, -4118.306, 32.133; deatholme
-- 4781.234, -4121.170, 31.018), The Darkway was entered from map 0 (shadowmoon, capture ends inside).
-- DelveMgr::LeaveDelve prefers the map stored per run at entry and falls back to this column; -1 = unknown -> homebind.
--
ALTER TABLE `delve_template`
    ADD COLUMN `exitMapId` int NOT NULL DEFAULT '-1' COMMENT 'overworld map of exitX/Y/Z/O (-1 = unknown)' AFTER `exitO`;

UPDATE `delve_template` SET `exitMapId`=2694 WHERE `mapId`=2964; -- The Gulf of Memory (12.1 gulf)
UPDATE `delve_template` SET `exitMapId`=0    WHERE `mapId`=2952; -- The Shadow Enclave (12.1 eversong, 12.0.1 deatholme)
UPDATE `delve_template` SET `exitMapId`=0    WHERE `mapId`=3003; -- The Darkway (12.0.7 shadowmoon, entered from map 0)
