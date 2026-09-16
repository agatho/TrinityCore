--
-- Prey: weekly hunt rotation world states and the Hunt Table in Astalor's Sanctum (Silvermoon City)
-- Source: retail 12.1 captures (builds 69273 .. 69814) and the 12.1 client DB2
--

-- The registry of the placeholder prey implementation is replaced by PreyMgr, which derives every hunt from
-- AdventureMapPOI / PlayerCondition / WorldStateExpression.
DROP TABLE IF EXISTS `prey_hunt_template`;

-- One realm-wide world state per hunt target; its value is the Hunt Table slot of the target this week (0 = not offered).
DELETE FROM `world_state` WHERE `ID` BETWEEN 28970 AND 28999 OR `ID` BETWEEN 30629 AND 30632;
INSERT INTO `world_state` (`ID`,`DefaultValue`,`MapIDs`,`AreaIDs`,`ScriptName`,`Comment`) VALUES
(28970,0,NULL,NULL,'','Prey hunt rotation: Magister Sunbreaker'),
(28971,0,NULL,NULL,'','Prey hunt rotation: Magistrix Emberlash'),
(28972,0,NULL,NULL,'','Prey hunt rotation: Senior Tinker Ozwold'),
(28973,0,NULL,NULL,'','Prey hunt rotation: L-N-0R the Recycler'),
(28974,0,NULL,NULL,'','Prey hunt rotation: Mordril Shadowfell'),
(28975,0,NULL,NULL,'','Prey hunt rotation: Deliah Gloomsong'),
(28976,0,NULL,NULL,'','Prey hunt rotation: Phaseblade Talasha'),
(28977,0,NULL,NULL,'','Prey hunt rotation: Nexus-Edge Hadim'),
(28978,0,NULL,NULL,'','Prey hunt rotation: Jo''zolo the Breaker'),
(28979,0,NULL,NULL,'','Prey hunt rotation: Zadu, Fist of Nalorakk'),
(28980,0,NULL,NULL,'','Prey hunt rotation: The Talon of Jan''alai'),
(28981,0,NULL,NULL,'','Prey hunt rotation: The Wing of Akil''zon'),
(28982,0,NULL,NULL,'','Prey hunt rotation: Ranger Swiftglade'),
(28983,0,NULL,NULL,'','Prey hunt rotation: Lieutenant Blazewing'),
(28984,0,NULL,NULL,'','Prey hunt rotation: Petyoll the Razorleaf'),
(28985,0,NULL,NULL,'','Prey hunt rotation: Lamyne of the Undercroft'),
(28986,0,NULL,NULL,'','Prey hunt rotation: High Vindicator Vureem'),
(28987,0,NULL,NULL,'','Prey hunt rotation: Crusader Luxia Maxwell'),
(28988,0,NULL,NULL,'','Prey hunt rotation: Praetor Singularis'),
(28989,0,NULL,NULL,'','Prey hunt rotation: Consul Nebulor'),
(28990,0,NULL,NULL,'','Prey hunt rotation: Executor Kaenius'),
(28991,0,NULL,NULL,'','Prey hunt rotation: Imperator Enigmalia'),
(28992,0,NULL,NULL,'','Prey hunt rotation: Knight-Errant Bloodshatter'),
(28993,0,NULL,NULL,'','Prey hunt rotation: Vylenna the Defector'),
(28994,0,NULL,NULL,'','Prey hunt rotation: Lost Theldrin'),
(28995,0,NULL,NULL,'','Prey hunt rotation: Neydra the Starving'),
(28996,0,NULL,NULL,'','Prey hunt rotation: Thornspeaker Edgath'),
(28997,0,NULL,NULL,'','Prey hunt rotation: Thorn-Witch Liset'),
(28998,0,NULL,NULL,'','Prey hunt rotation: Grothoz, the Burning Shadow'),
(28999,0,NULL,NULL,'','Prey hunt rotation: Dengzag, the Darkened Blaze'),
(30629,0,NULL,NULL,'','Prey hunt rotation: Janoa the Fang (special Nightmare)'),
(30630,0,NULL,NULL,'','Prey hunt rotation: Kursak the Coiled (special Nightmare)'),
(30631,0,NULL,NULL,'','Prey hunt rotation: Batani the Scaled (special Nightmare)'),
(30632,0,NULL,NULL,'','Prey hunt rotation: Kadani the Claw (special Nightmare)');

-- Hunt Table (245824) and Astalor Bloodsworn (246231), Murder Row
SET @CGUID := 13400000;
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+0 AND @CGUID+1;
INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnDifficulties`,`phaseUseFlags`,`PhaseId`,`PhaseGroup`,`terrainSwapMap`,`modelid`,`equipment_id`,`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`wander_distance`,`currentwaypoint`,`curHealthPct`,`MovementType`,`npcflag`,`unit_flags`,`unit_flags2`,`unit_flags3`,`ScriptName`,`StringId`,`VerifiedBuild`) VALUES
(@CGUID+0, 245824, 0, 15969, 16634, '0', 0, 0, 0, -1, 0, 0, 8550.509, -4929.4204, 25.383518, 1.605724692344665527, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, '', NULL, 69273), -- Hunt Table
(@CGUID+1, 246231, 0, 15969, 16634, '0', 0, 0, 0, -1, 0, 0, 8548.431, -4927.0435, 25.384737, 1.031758666038513183, 120, 0, 0, 100, 0, NULL, NULL, NULL, NULL, '', NULL, 69273); -- Astalor Bloodsworn

DELETE FROM `creature_template_gossip` WHERE `CreatureID` IN (245824, 246231);
INSERT INTO `creature_template_gossip` (`CreatureID`,`MenuID`,`VerifiedBuild`) VALUES
(245824, 39565, 69273),
(246231, 39662, 69273);

DELETE FROM `npc_text` WHERE `ID` IN (39565, 39662);
INSERT INTO `npc_text` (`ID`,`Probability0`,`Probability1`,`Probability2`,`Probability3`,`Probability4`,`Probability5`,`Probability6`,`Probability7`,`BroadcastTextID0`,`BroadcastTextID1`,`BroadcastTextID2`,`BroadcastTextID3`,`BroadcastTextID4`,`BroadcastTextID5`,`BroadcastTextID6`,`BroadcastTextID7`,`VerifiedBuild`) VALUES
(39565, 1, 0, 0, 0, 0, 0, 0, 0, 98814, 0, 0, 0, 0, 0, 0, 0, 69273),
(39662, 1, 0, 0, 0, 0, 0, 0, 0, 292259, 0, 0, 0, 0, 0, 0, 0, 69273);

DELETE FROM `gossip_menu` WHERE `MenuID` IN (39565, 39662);
INSERT INTO `gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES
(39565, 39565, 69273),
(39662, 39662, 69273);

-- The Hunt Table's only option opens the Adventure Map (OptionNpc 31, GossipNPCOption 59105). Astalor's three
-- explanations were not selected in the capture, so they carry no follow-up menu.
DELETE FROM `gossip_menu_option` WHERE `MenuID` IN (39565, 39662);
INSERT INTO `gossip_menu_option` (`MenuID`,`GossipOptionID`,`OptionID`,`OptionNpc`,`OptionText`,`OptionBroadcastTextID`,`Language`,`Flags`,`ActionMenuID`,`ActionPoiID`,`GossipNpcOptionID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`SpellID`,`OverrideIconID`,`VerifiedBuild`) VALUES
(39565, 134182, 0, 31, 'Show me what missions you have prepared.', 0, 0, 0, 0, 0, 59105, 0, 0, NULL, 0, NULL, NULL, 69273),
(39662, 139543, 4, 0, 'How do I gain progress toward my hunt?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69273),
(39662, 139542, 5, 0, 'How do I know when I''m close to finding my prey?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69273),
(39662, 139541, 6, 0, 'How do I abandon a hunt or start a new one?', 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, NULL, NULL, 69273);
