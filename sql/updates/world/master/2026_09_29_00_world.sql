--
-- Prey hunts: the hunt itself, measured from a complete retail hunt.
--
-- Evidence: 12.1.0.69933 capture of "Prey: The Talon of Jan'alai (Normal)" in Eversong Woods played from
-- CMSG_ADVENTURE_MAP_START_QUEST to the auto-complete turn-in (33 minutes, six ambushes, three world quests, the final
-- fight), cross-checked against the 12.1.0.69273 capture of "Prey: Nexus-Edge Hadim (Normal)" (four ambushes, final
-- reached), the 12.1 client DB2 (SpellEffect, PlayerDataElementCharacter, ModifierTree, AdventureMapPOI, UiWidget*)
-- and the hunt quests' POIs. Everything the client does not carry lives in five `prey_hunt_*` tables read by PreyMgr.
--

DROP TABLE IF EXISTS `prey_hunt_zone`;
CREATE TABLE `prey_hunt_zone` (
  `ZoneIndex` tinyint unsigned NOT NULL COMMENT 'PlayerDataElementCharacter 141',
  `AreaId` int unsigned NOT NULL DEFAULT '0',
  `UiMapId` int unsigned NOT NULL DEFAULT '0' COMMENT 'selects the zone''s blobs in the hunt quest POIs',
  `Name` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`ZoneIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Prey: the four hunt zones';

DROP TABLE IF EXISTS `prey_hunt_slot`;
CREATE TABLE `prey_hunt_slot` (
  `Slot` tinyint unsigned NOT NULL COMMENT 'value of the target''s rotation world state',
  `ZoneIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`Slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Prey: zone of each Hunt Table slot';

DROP TABLE IF EXISTS `prey_hunt_target`;
CREATE TABLE `prey_hunt_target` (
  `WorldStateId` int NOT NULL COMMENT 'rotation world state of the target',
  `TargetIndex` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'PlayerDataElementCharacter 137',
  `PursuitSpellId` int unsigned NOT NULL DEFAULT '0' COMMENT 'summons the fleeing prey after an ambush',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`WorldStateId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Prey: hunt targets';

DROP TABLE IF EXISTS `prey_hunt`;
CREATE TABLE `prey_hunt` (
  `QuestId` int unsigned NOT NULL,
  `PreyEntry` int unsigned NOT NULL DEFAULT '0' COMMENT 'creature fought at the end of the hunt',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`QuestId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Prey: hunt quests';

DROP TABLE IF EXISTS `prey_hunt_spell`;
CREATE TABLE `prey_hunt_spell` (
  `WorldStateId` int NOT NULL,
  `Role` tinyint unsigned NOT NULL COMMENT '0 = Hunted Remnant (ambush), 1 = prey (final fight)',
  `SpellId` int unsigned NOT NULL,
  `InitialMin` int unsigned NOT NULL DEFAULT '0' COMMENT 'ms after the fight starts',
  `InitialMax` int unsigned NOT NULL DEFAULT '0',
  `RepeatMin` int unsigned NOT NULL DEFAULT '0' COMMENT 'ms, 0 = once',
  `RepeatMax` int unsigned NOT NULL DEFAULT '0',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`WorldStateId`,`Role`,`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Prey: ability kits of remnant and prey';

-- Zone index = PlayerDataElementCharacter 141. The hunt quest POIs are gated by ModifierTree type 390 on element 141:
-- 401859 (== 1) Eversong Woods / UiMap 2395, 401861 (== 2) Zul'Aman / 2437, 401863 (== 3) Harandar / 2413,
-- 401865 (== 4) Voidstorm / 2405. Areas from UiMapAssignment.
INSERT INTO `prey_hunt_zone` (`ZoneIndex`,`AreaId`,`UiMapId`,`Name`) VALUES
(1,15968,2395,'Eversong Woods'),
(2,15947,2437,'Zul''Aman'),
(3,15355,2413,'Harandar'),
(4,15458,2405,'Voidstorm');

-- Slots 1..12 run zone by zone, Normal / Hard / Nightmare. Captured: slot 1 = Eversong Woods (both hunts). Slots 4..6 sit
-- on Zul'Aman's own map 0 coordinates on the scouting map; 7..12 follow the zone index order (Harandar, Voidstorm).
-- Slots 13 and 14 are extra Hard positions whose zone is not captured: assigned to the nearest zone group on the
-- scouting map (AdventureMapPOI.WorldPosition 6100,-10600 and 5900,-12150).
INSERT INTO `prey_hunt_slot` (`Slot`,`ZoneIndex`,`Comment`) VALUES
(1,1,'Normal - captured'),(2,1,'Hard'),(3,1,'Nightmare'),
(4,2,'Normal'),(5,2,'Hard'),(6,2,'Nightmare'),
(7,3,'Normal'),(8,3,'Hard'),(9,3,'Nightmare'),
(10,4,'Normal'),(11,4,'Hard'),(12,4,'Nightmare'),
(13,2,'Hard - zone not captured, nearest scouting map group'),
(14,3,'Hard - zone not captured, nearest scouting map group');

-- Target index = PlayerDataElementCharacter 137: captured 8 for Nexus-Edge Hadim (world state 28977) and 11 for The Talon
-- of Jan'alai (28980), i.e. world state - 28969 for the 30 regular targets. The four special Nightmare targets continue
-- the count (not captured). Pursuit spell: the "Pursuit" summon (SpellEffect 28, SummonProperties 6522) whose creature
-- carries the target's name; the specials' four Pursuit spells 1302329..1302332 are assigned in world state order.
INSERT INTO `prey_hunt_target` (`WorldStateId`,`TargetIndex`,`PursuitSpellId`,`Comment`) VALUES
(28970,1,1260235,'Magister Sunbreaker'),
(28971,2,1260393,'Magistrix Emberlash'),
(28972,3,1260394,'Senior Tinker Ozwold'),
(28973,4,1260395,'L-N-0R the Recycler'),
(28974,5,1260396,'Mordril Shadowfell'),
(28975,6,1260397,'Deliah Gloomsong'),
(28976,7,1260399,'Phaseblade Talasha'),
(28977,8,1260400,'Nexus-Edge Hadim - captured'),
(28978,9,1260401,'Jo''zolo the Breaker'),
(28979,10,1260402,'Zadu, Fist of Nalorakk'),
(28980,11,1260403,'The Talon of Jan''alai - captured'),
(28981,12,1260404,'The Wing of Akil''zon'),
(28982,13,1260405,'Ranger Swiftglade'),
(28983,14,1260406,'Lieutenant Blazewing'),
(28984,15,1260407,'Petyoll the Razorleaf'),
(28985,16,1260408,'Lamyne of the Undercroft'),
(28986,17,1260409,'High Vindicator Vureem'),
(28987,18,1260410,'Crusader Luxia Maxwell'),
(28988,19,1260412,'Praetor Singularis'),
(28989,20,1260413,'Consul Nebulor'),
(28990,21,1260414,'Executor Kaenius'),
(28991,22,1260415,'Imperator Enigmalia'),
(28992,23,1260416,'Knight-Errant Bloodshatter'),
(28993,24,1260417,'Vylenna the Defector'),
(28994,25,1260418,'Lost Theldrin'),
(28995,26,1260419,'Neydra the Starving'),
(28996,27,1260420,'Thornspeaker Edgath'),
(28997,28,1260421,'Thorn-Witch Liset'),
(28998,29,1260422,'Grothoz, the Burning Shadow'),
(28999,30,1260423,'Dengzag, the Darkened Blaze'),
(30629,31,1302329,'Janoa the Fang - special Nightmare target, index and pursuit extrapolated'),
(30630,32,1302330,'Kursak the Coiled - special Nightmare target, index and pursuit extrapolated'),
(30631,33,1302331,'Batani the Scaled - special Nightmare target, index and pursuit extrapolated'),
(30632,34,1302332,'Kadani the Claw - special Nightmare target, index and pursuit extrapolated');

-- Prey = the target's QuestBoss creature of the hunt's difficulty: every target has three, whose HealthModifier rises
-- Normal < Hard < Nightmare (captured: The Talon of Jan'alai Normal = 246501). The four special Nightmare hunts
-- (95021..95024) have no quest template in the world DB yet and are left out.
INSERT INTO `prey_hunt` (`QuestId`,`PreyEntry`,`Comment`) VALUES
(91095,246434,'Prey: Magister Sunbreaker (Normal)'),
(91096,246491,'Prey: Magistrix Emberlash (Normal)'),
(91097,246492,'Prey: Senior Tinker Ozwold (Normal)'),
(91098,246493,'Prey: L-N-0R the Recycler (Normal)'),
(91099,246494,'Prey: Mordril Shadowfell (Normal)'),
(91100,246495,'Prey: Deliah Gloomsong (Normal)'),
(91101,246496,'Prey: Phaseblade Talasha (Normal)'),
(91102,246498,'Prey: Nexus-Edge Hadim (Normal)'),
(91103,246499,'Prey: Jo''zolo the Breaker (Normal)'),
(91104,246500,'Prey: Zadu, Fist of Nalorakk (Normal)'),
(91105,246501,'Prey: The Talon of Jan''alai (Normal)'),
(91106,246502,'Prey: The Wing of Akil''zon (Normal)'),
(91107,246503,'Prey: Ranger Swiftglade (Normal)'),
(91108,246504,'Prey: Lieutenant Blazewing (Normal)'),
(91109,246505,'Prey: Petyoll the Razorleaf (Normal)'),
(91110,246506,'Prey: Lamyne of the Undercroft (Normal)'),
(91111,246507,'Prey: High Vindicator Vureem (Normal)'),
(91112,246508,'Prey: Crusader Luxia Maxwell (Normal)'),
(91113,246509,'Prey: Praetor Singularis (Normal)'),
(91114,246510,'Prey: Consul Nebulor (Normal)'),
(91115,246511,'Prey: Executor Kaenius (Normal)'),
(91116,246512,'Prey: Imperator Enigmalia (Normal)'),
(91117,246514,'Prey: Knight-Errant Bloodshatter (Normal)'),
(91118,246515,'Prey: Vylenna the Defector (Normal)'),
(91119,246516,'Prey: Lost Theldrin (Normal)'),
(91120,246517,'Prey: Neydra the Starving (Normal)'),
(91121,246518,'Prey: Thornspeaker Edgath (Normal)'),
(91122,246519,'Prey: Thorn-Witch Liset (Normal)'),
(91123,246520,'Prey: Grothoz, the Burning Shadow (Normal)'),
(91124,246521,'Prey: Dengzag, the Darkened Blaze (Normal)'),
(91210,246438,'Prey: Magister Sunbreaker (Hard)'),
(91211,246439,'Prey: Magister Sunbreaker (Nightmare)'),
(91212,246925,'Prey: Magistrix Emberlash (Hard)'),
(91213,246926,'Prey: Magistrix Emberlash (Nightmare)'),
(91214,246927,'Prey: Senior Tinker Ozwold (Hard)'),
(91215,246928,'Prey: Senior Tinker Ozwold (Nightmare)'),
(91216,246929,'Prey: L-N-0R the Recycler (Hard)'),
(91217,246930,'Prey: L-N-0R the Recycler (Nightmare)'),
(91218,246932,'Prey: Mordril Shadowfell (Hard)'),
(91219,246933,'Prey: Mordril Shadowfell (Nightmare)'),
(91220,246934,'Prey: Deliah Gloomsong (Hard)'),
(91221,246935,'Prey: Deliah Gloomsong (Nightmare)'),
(91222,246936,'Prey: Phaseblade Talasha (Hard)'),
(91223,246937,'Prey: Phaseblade Talasha (Nightmare)'),
(91224,246938,'Prey: Nexus-Edge Hadim (Hard)'),
(91225,246939,'Prey: Nexus-Edge Hadim (Nightmare)'),
(91226,246940,'Prey: Jo''zolo the Breaker (Hard)'),
(91227,246941,'Prey: Jo''zolo the Breaker (Nightmare)'),
(91228,246942,'Prey: Zadu, Fist of Nalorakk (Hard)'),
(91229,246943,'Prey: Zadu, Fist of Nalorakk (Nightmare)'),
(91230,246944,'Prey: The Talon of Jan''alai (Hard)'),
(91231,246945,'Prey: The Talon of Jan''alai (Nightmare)'),
(91232,246946,'Prey: The Wing of Akil''zon (Hard)'),
(91233,246947,'Prey: The Wing of Akil''zon (Nightmare)'),
(91234,246948,'Prey: Ranger Swiftglade (Hard)'),
(91235,246949,'Prey: Ranger Swiftglade (Nightmare)'),
(91236,246950,'Prey: Lieutenant Blazewing (Hard)'),
(91237,246951,'Prey: Lieutenant Blazewing (Nightmare)'),
(91238,246952,'Prey: Petyoll the Razorleaf (Hard)'),
(91239,246953,'Prey: Petyoll the Razorleaf (Nightmare)'),
(91240,246954,'Prey: Lamyne of the Undercroft (Hard)'),
(91241,246955,'Prey: Lamyne of the Undercroft (Nightmare)'),
(91242,246956,'Prey: High Vindicator Vureem (Hard)'),
(91243,246958,'Prey: Crusader Luxia Maxwell (Hard)'),
(91244,246960,'Prey: Praetor Singularis (Hard)'),
(91245,246963,'Prey: Consul Nebulor (Hard)'),
(91246,246965,'Prey: Executor Kaenius (Hard)'),
(91247,246968,'Prey: Imperator Enigmalia (Hard)'),
(91248,246972,'Prey: Knight-Errant Bloodshatter (Hard)'),
(91249,246974,'Prey: Vylenna the Defector (Hard)'),
(91250,246976,'Prey: Lost Theldrin (Hard)'),
(91251,246978,'Prey: Neydra the Starving (Hard)'),
(91252,246980,'Prey: Thornspeaker Edgath (Hard)'),
(91253,246982,'Prey: Thorn-Witch Liset (Hard)'),
(91254,246985,'Prey: Grothoz, the Burning Shadow (Hard)'),
(91255,246987,'Prey: Dengzag, the Darkened Blaze (Hard)'),
(91256,246957,'Prey: High Vindicator Vureem (Nightmare)'),
(91257,246959,'Prey: Crusader Luxia Maxwell (Nightmare)'),
(91258,246961,'Prey: Praetor Singularis (Nightmare)'),
(91259,246964,'Prey: Consul Nebulor (Nightmare)'),
(91260,246966,'Prey: Executor Kaenius (Nightmare)'),
(91261,246969,'Prey: Imperator Enigmalia (Nightmare)'),
(91262,246973,'Prey: Knight-Errant Bloodshatter (Nightmare)'),
(91263,246975,'Prey: Vylenna the Defector (Nightmare)'),
(91264,246977,'Prey: Lost Theldrin (Nightmare)'),
(91265,246979,'Prey: Neydra the Starving (Nightmare)'),
(91266,246981,'Prey: Thornspeaker Edgath (Nightmare)'),
(91267,246984,'Prey: Thorn-Witch Liset (Nightmare)'),
(91268,246986,'Prey: Grothoz, the Burning Shadow (Nightmare)'),
(91269,246988,'Prey: Dengzag, the Darkened Blaze (Nightmare)');

-- Ability kits (ms, measured from the fixate / engage of each captured fight).
-- The Talon of Jan'alai remnant: Earthgrab Totem +1.3 s, every 17.4..17.8 s; Lava Burst +5.2..6.9 s, every 9.8..10.2 s.
-- The Talon of Jan'alai prey: Chain Lightning +10.0 s, Earthgrab Totem +15.3 s after engage; repeats not captured (the
-- prey died 22 s into the fight) - Earthgrab taken over from the remnant, Chain Lightning 10..12 s.
-- Nexus-Edge Hadim remnant: Aether Tricks +1.2 s, every 20 s; Void Laceration +4.8..6.1 s, every 11 s; Umbral Daggers
-- +10.9..11.0 s.
INSERT INTO `prey_hunt_spell` (`WorldStateId`,`Role`,`SpellId`,`InitialMin`,`InitialMax`,`RepeatMin`,`RepeatMax`,`Comment`) VALUES
(28980,0,1254818,1200,1400,17400,17800,'The Talon of Jan''alai - remnant - Earthgrab Totem'),
(28980,0,1254777,5200,6900,9800,10200,'The Talon of Jan''alai - remnant - Lava Burst'),
(28980,1,1254779,9500,10500,10000,12000,'The Talon of Jan''alai - prey - Chain Lightning'),
(28980,1,1254818,15000,15500,17400,17800,'The Talon of Jan''alai - prey - Earthgrab Totem'),
(28977,0,1254074,1100,1300,19900,20100,'Nexus-Edge Hadim - remnant - Aether Tricks'),
(28977,0,1254093,4800,6100,10900,11100,'Nexus-Edge Hadim - remnant - Void Laceration'),
(28977,0,1254163,10900,11000,0,0,'Nexus-Edge Hadim - remnant - Umbral Daggers');

-- Accepting a hunt: the quests carry QUEST_FLAGS_PLAYER_CAST_ACCEPT; the player casts 1241010 "UI Toast [DNT]" (captured
-- first, UI event toast 307) which leads into 1244011 "Target Accept Shared [DNT]".
INSERT IGNORE INTO `quest_template_addon` (`ID`) VALUES (91095),(91096),(91097),(91098),(91099),(91100),(91101),(91102),(91103),(91104),(91105),(91106),(91107),(91108),(91109),(91110),(91111),(91112),(91113),(91114),(91115),(91116),(91117),(91118),(91119),(91120),(91121),(91122),(91123),(91124),(91210),(91211),(91212),(91213),(91214),(91215),(91216),(91217),(91218),(91219),(91220),(91221),(91222),(91223),(91224),(91225),(91226),(91227),(91228),(91229),(91230),(91231),(91232),(91233),(91234),(91235),(91236),(91237),(91238),(91239),(91240),(91241),(91242),(91243),(91244),(91245),(91246),(91247),(91248),(91249),(91250),(91251),(91252),(91253),(91254),(91255),(91256),(91257),(91258),(91259),(91260),(91261),(91262),(91263),(91264),(91265),(91266),(91267),(91268),(91269);
UPDATE `quest_template_addon` SET `SourceSpellID`=1241010, `ScriptName`='quest_prey_hunt' WHERE `ID` IN (91095,91096,91097,91098,91099,91100,91101,91102,91103,91104,91105,91106,91107,91108,91109,91110,91111,91112,91113,91114,91115,91116,91117,91118,91119,91120,91121,91122,91123,91124,91210,91211,91212,91213,91214,91215,91216,91217,91218,91219,91220,91221,91222,91223,91224,91225,91226,91227,91228,91229,91230,91231,91232,91233,91234,91235,91236,91237,91238,91239,91240,91241,91242,91243,91244,91245,91246,91247,91248,91249,91250,91251,91252,91253,91254,91255,91256,91257,91258,91259,91260,91261,91262,91263,91264,91265,91266,91267,91268,91269);

-- Hunted Remnant, pursuit summons and the prey are hostile (faction 14 on all three captured entries 246605 / 253903 /
-- 246501); pursuit summons carry NpcFlags 0x1000000 (spellclick, cursor "attack") and 1260302 "Recovering Strength".
UPDATE `creature_template` SET `faction`=14, `ScriptName`='npc_prey_hunted_remnant' WHERE `entry`=246605;
UPDATE `creature_template` SET `faction`=14, `npcflag`=16777216, `ScriptName`='npc_prey_pursuit' WHERE `entry` IN (253562,253894,253895,253896,253897,253898,253899,253900,253901,253902,253903,253904,253905,253906,253907,253908,253909,253910,253911,253912,253913,253914,253915,253916,253917,253918,253919,253920,253921,253922);
UPDATE `creature_template` SET `faction`=14, `ScriptName`='npc_prey_hunt_target' WHERE `entry` IN (246434,246491,246492,246493,246494,246495,246496,246498,246499,246500,246501,246502,246503,246504,246505,246506,246507,246508,246509,246510,246511,246512,246514,246515,246516,246517,246518,246519,246520,246521,246438,246439,246925,246926,246927,246928,246929,246930,246932,246933,246934,246935,246936,246937,246938,246939,246940,246941,246942,246943,246944,246945,246946,246947,246948,246949,246950,246951,246952,246953,246954,246955,246956,246958,246960,246963,246965,246968,246972,246974,246976,246978,246980,246982,246985,246987,246957,246959,246961,246964,246966,246969,246973,246975,246977,246979,246981,246984,246986,246988);
UPDATE `creature_template` SET `ScriptName`='npc_prey_earthgrab_totem' WHERE `entry`=252051;

DELETE FROM `creature_template_addon` WHERE `entry` IN (246501,253562,253894,253895,253896,253897,253898,253899,253900,253901,253902,253903,253904,253905,253906,253907,253908,253909,253910,253911,253912,253913,253914,253915,253916,253917,253918,253919,253920,253921,253922);
INSERT INTO `creature_template_addon` (`entry`,`auras`) VALUES
(246501,'1241480'),
(253562,'1260302'),
(253894,'1260302'),
(253895,'1260302'),
(253896,'1260302'),
(253897,'1260302'),
(253898,'1260302'),
(253899,'1260302'),
(253900,'1260302'),
(253901,'1260302'),
(253902,'1260302'),
(253903,'1260302'),
(253904,'1260302'),
(253905,'1260302'),
(253906,'1260302'),
(253907,'1260302'),
(253908,'1260302'),
(253909,'1260302'),
(253910,'1260302'),
(253911,'1260302'),
(253912,'1260302'),
(253913,'1260302'),
(253914,'1260302'),
(253915,'1260302'),
(253916,'1260302'),
(253917,'1260302'),
(253918,'1260302'),
(253919,'1260302'),
(253920,'1260302'),
(253921,'1260302'),
(253922,'1260302');

DELETE FROM `creature_text` WHERE `CreatureID`=246501;
INSERT INTO `creature_text` (`CreatureID`,`GroupID`,`ID`,`Text`,`Type`,`Language`,`Probability`,`Emote`,`Duration`,`Sound`,`BroadcastTextId`,`TextRange`,`comment`) VALUES
(246501,0,0,'Gonna burn ya alive.',12,0,100,0,0,0,0,0,'The Talon of Jan''alai - Aggro'),
(246501,1,0,'I''ll... be back.',12,0,100,0,0,0,0,0,'The Talon of Jan''alai - Death');

-- Remnant of Anguish (555631, personal chest from 1242380): 7, 7 and 12 Remnant of Anguish (currency 3392) in the three
-- captured loots; opening it is worth +10 hunt progress (1241123, scripted).
UPDATE `gameobject_template` SET `Data1`=555631, `AIName`='', `ScriptName`='go_prey_remnant_of_anguish' WHERE `entry`=555631;
DELETE FROM `gameobject_loot_template` WHERE `Entry`=555631;
INSERT INTO `gameobject_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(555631,1,3392,100,0,1,0,7,12,'Remnant of Anguish - Remnant of Anguish');

-- Hunt progress from world quests and prey activities: 12.1 rewards them with 1241124 (+20, hunt zone only). 32 quests
-- captured with RewardSpell 1241124 (69273 + 69933), no quest out of 902 captured rewards 1241123 (+10, which only
-- ambushes and Remnants of Anguish grant). The world DB still had 1241123 from builds 66102..66384.
UPDATE `quest_template` SET `RewardSpell`=1241124 WHERE `RewardSpell`=1241123;

-- The Talon of Jan'alai (Normal) reward pickers as queried in 12.1.0.69933 (was 4269 / 4541 / 4774).
DELETE FROM `quest_treasure_pickers` WHERE `QuestID`=91105;
INSERT INTO `quest_treasure_pickers` (`QuestID`,`TreasurePickerID`,`OrderIndex`) VALUES
(91105,4887,0),
(91105,4877,1);

-- Spell scripts
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_prey_ui_toast_accept','spell_prey_target_accept_shared','spell_prey_ambush_zone_aura','spell_prey_active_ambush','spell_prey_summon_anguish','spell_prey_on_the_hunt_small','spell_prey_on_the_hunt_large','spell_prey_progress_gained','spell_prey_final_party_share','spell_prey_final_toast','spell_prey_final_strike','spell_prey_credit','spell_prey_first_four_hunts_bonus','spell_prey_toast_complete','spell_prey_earthgrab_totem');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1241010,'spell_prey_ui_toast_accept'),
(1244011,'spell_prey_target_accept_shared'),
(1241553,'spell_prey_ambush_zone_aura'),
(1241561,'spell_prey_active_ambush'),
(1242379,'spell_prey_summon_anguish'),
(1241123,'spell_prey_on_the_hunt_small'),
(1241124,'spell_prey_on_the_hunt_large'),
(1241147,'spell_prey_progress_gained'),
(1241311,'spell_prey_final_party_share'),
(1241154,'spell_prey_final_toast'),
(1259096,'spell_prey_final_strike'),
(1259068,'spell_prey_credit'),
(1279779,'spell_prey_first_four_hunts_bonus'),
(1262032,'spell_prey_toast_complete'),
(1254818,'spell_prey_earthgrab_totem');
