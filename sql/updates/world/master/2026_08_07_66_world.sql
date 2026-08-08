--
-- Sniff-mined delve world data (C:\dumps\GULF_FONT_MINE.md).
--
-- SPLIT NOTE: on content/midnight-s1 this file also carries the Font of Power gameobject
-- spawn in Algeth'ar Academy (GO 246779, guid 9000300) — that statement belongs to
-- feature/mythic-plus and is intentionally NOT part of this branch's copy. The filename is
-- kept identical so the updates tracker does not double-apply it at integration time.
--

-- The Gulf of Memory (delve_template 25): full run data from the 12.0.1.66709 capture
-- (values that are stable across builds: scenario, gossip ids, world positions).
-- Entry NEW_WORLD map 2964 (155.129, 634.757, 187.334, o 2.8133); exit NEW_WORLD back to map 2694
-- at (48.420, 816.046, 1110.519, o 1.7356), right beside the entrance. Tier gossip menu 40278,
-- OptionID = 135349 - tier (Tier 1 = 135348 .. Tier 11 = 135338). Scenario 3177 (4 steps
-- 16081/16083/16084/16082). Entrance NPC: creature 212407 on map 2694.
UPDATE `delve_template` SET
    `scenarioId` = 3177, `activeScenarioId` = 3177,
    `gossipMenuId` = 40278, `firstTierGossipOptionId` = 135348,
    `entryX` = 155.129, `entryY` = 634.757, `entryZ` = 187.334, `entryO` = 2.8133,
    `exitX` = 48.420, `exitY` = 816.046, `exitZ` = 1110.519, `exitO` = 1.7356
WHERE `id` = 25;

-- Scenario routing for ScenarioMgr (delves are faction-neutral)
DELETE FROM `scenarios` WHERE `map` = 2964 AND `difficulty` = 208;
INSERT INTO `scenarios` (`map`, `difficulty`, `scenario_A`, `scenario_H`) VALUES
(2964, 208, 3177, 3177);
