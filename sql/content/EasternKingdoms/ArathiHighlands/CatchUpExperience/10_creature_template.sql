-- ADAPTED to integ_world 12.x schema (Arathi Catch-Up / RPE).
-- Schema mapping applied:
--   rank            -> Classification  (12.x renamed the column)
--   minlevel/maxlevel -> DROPPED        (no such columns in 12.x creature_template;
--                                        creatures already carry correct levels from base)
--   gossip_menu_id  -> moved OUT to creature_template_gossip (CreatureID, MenuID)
-- Creatures already exist in base creature_template, so ON DUPLICATE KEY UPDATE only
-- touches the RPE-relevant fields (faction, npcflag, Classification) -- base data preserved.
-- npcflag: content value OR'd with UNIT_NPC_FLAG_QUESTGIVER(2) for every creature that is
-- a quest starter/ender in 33_creature_quest_links.sql (fixes step-4 blocker: 244643).

INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (229955, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (230004, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (230248, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232019, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232022, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232023, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232028, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232030, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232035, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (232038, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244642, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244643, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244655, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244656, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244657, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244658, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244666, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244667, 35, 2, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244669, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244670, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244671, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244672, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244674, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244675, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244676, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244677, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244682, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244683, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244685, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244690, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244691, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244695, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244709, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244711, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244714, 35, 3, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244715, 35, 3, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- questgiver ok
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244729, 35, 2, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);  -- +questgiver(2) [quest starter/ender]
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244785, 14, 0, 1) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244786, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244923, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (244956, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (245026, 35, 129, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (245027, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249254, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249255, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (249269, 35, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);
INSERT INTO `creature_template` (`entry`, `faction`, `npcflag`, `Classification`) VALUES (257072, 14, 0, 0) ON DUPLICATE KEY UPDATE `faction`=VALUES(`faction`), `npcflag`=VALUES(`npcflag`), `Classification`=VALUES(`Classification`);

-- gossip links moved from creature_template.gossip_menu_id -> creature_template_gossip
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(244714, 39348, 69382),
(245026, 39386, 69382)
ON DUPLICATE KEY UPDATE `VerifiedBuild`=VALUES(`VerifiedBuild`);
