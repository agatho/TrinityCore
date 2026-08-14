-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr B4: Scalecommander Azurathel (181056) speak + quest 64864 turn-in after 183380 morph
-- Sniff: creature_text broadcast 213820; creature_template_addon anim kit 24297, no stasis aura
-- Gossip menu pattern from Kethahn (27444); 181056 has no gossip row in 11.0.7 sniff world SQL

SET @NPCTEXT_AZURATHEL := 6486402;
SET @MENU_AZURATHEL := 27445;

-- Interact flags (gossip + quest giver) and sniff-aligned template fields
UPDATE `creature_template` SET `faction`=35, `npcflag`=3, `BaseAttackTime`=2000, `unit_flags2`=0x800 WHERE `entry`=181056;

-- Post-awaken addon: anim kit only, no stasis (sniff 11.0.7)
DELETE FROM `creature_template_addon` WHERE `entry`=181056;
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvpFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(181056, 0, 0, 0, 0, 0, 1, 0, 0, 24297, 0, 0, 0, '');

-- Gossip (broadcast 213820 — awaken line from sniff creature_text)
DELETE FROM `creature_template_gossip` WHERE `CreatureID`=181056 AND `MenuID`=@MENU_AZURATHEL;
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(181056, @MENU_AZURATHEL, 64978);

DELETE FROM `npc_text` WHERE `ID`=@NPCTEXT_AZURATHEL;
INSERT INTO `npc_text` (`ID`, `Probability0`, `Probability1`, `Probability2`, `Probability3`, `Probability4`, `Probability5`, `Probability6`, `Probability7`, `BroadcastTextId0`, `BroadcastTextId1`, `BroadcastTextId2`, `BroadcastTextId3`, `BroadcastTextId4`, `BroadcastTextId5`, `BroadcastTextId6`, `BroadcastTextId7`, `VerifiedBuild`) VALUES
(@NPCTEXT_AZURATHEL, 1, 0, 0, 0, 0, 0, 0, 0, 213820, 0, 0, 0, 0, 0, 0, 0, 64978);

DELETE FROM `gossip_menu` WHERE `MenuID`=@MENU_AZURATHEL;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES
(@MENU_AZURATHEL, @NPCTEXT_AZURATHEL, 64978);

-- Quest turn-in: see 2026_08_15_06_world_dracthyr_lower_war_creche.sql (creature_questender 181056 / 64864)
