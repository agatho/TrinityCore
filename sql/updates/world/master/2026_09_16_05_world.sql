-- Lorewalking stories
DROP TABLE IF EXISTS `lorewalking_story`;
CREATE TABLE `lorewalking_story` (
  `StoryID` int unsigned NOT NULL,
  `ResponseBegin` int unsigned NOT NULL DEFAULT '0',
  `ResponseContinue` int unsigned NOT NULL DEFAULT '0',
  `ResponseStartOver` int unsigned NOT NULL DEFAULT '0',
  `LaunchSpellID` int unsigned NOT NULL DEFAULT '0' COMMENT 'cast by the PlayerChoice 845 response',
  `EnterSpellID` int unsigned NOT NULL DEFAULT '0' COMMENT 'starts the story (applies 463943)',
  `IntroSpellID` int unsigned NOT NULL DEFAULT '0' COMMENT 'intro timeline scenes, not played when continuing',
  `AskQuestionSpellID` int unsigned NOT NULL DEFAULT '0' COMMENT 'cast by 468124 Ask a Question',
  `FirstQuestID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`StoryID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `lorewalking_story_map`;
CREATE TABLE `lorewalking_story_map` (
  `StoryID` int unsigned NOT NULL,
  `MapID` int NOT NULL COMMENT 'instance map the story takes the player into; any other instance suspends Lorewalking',
  PRIMARY KEY (`StoryID`,`MapID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `lorewalking_story` (`StoryID`,`ResponseBegin`,`ResponseContinue`,`ResponseStartOver`,`LaunchSpellID`,`EnterSpellID`,`IntroSpellID`,`AskQuestionSpellID`,`FirstQuestID`) VALUES
(1,4321,0,0,467482,463926,463932,460935,84371),   -- Xal'atath
(2,4322,0,0,468532,468534,468536,466138,85027),   -- Ethereals
(3,4379,0,0,471657,471655,1285800,1215622,85884), -- The Lich King
(4,5046,5047,5048,467592,467591,1285850,474136,85252), -- Elves
(5,5419,0,0,1258364,1258361,1261188,1259855,92826); -- Loa

INSERT INTO `lorewalking_story_map` (`StoryID`,`MapID`) VALUES
(1,1539), -- Tirisfal Glades (Blade in Twilight scenario)
(3,595),  -- The Culling of Stratholme
(3,668),  -- Halls of Reflection
(3,2819), -- Icecrown Citadel
(5,2291); -- De Other Side

-- PlayerChoice 845 "Lorewalking Campaigns"
UPDATE `playerchoice` SET `ScriptName`='playerchoice_lorewalking' WHERE `ChoiceId`=845;

DELETE FROM `playerchoice_response` WHERE `ChoiceId`=845 AND `ResponseId` IN (4321,4322,4379,5047,5048,5419);
INSERT INTO `playerchoice_response` (`ChoiceId`,`ResponseId`,`Index`,`ChoiceArtFileId`,`Flags`,`WidgetSetID`,`UiTextureAtlasElementID`,`SoundKitID`,`GroupID`,`UiTextureKitID`,`Answer`,`Header`,`SubHeader`,`ButtonTooltip`,`Description`,`Confirmation`,`RewardQuestID`,`VerifiedBuild`) VALUES
(845,4321,0,6403389,0,1571,0,0,0,0,'Begin','Xal\'atath','','Xal\'atath awaits...','Xal\'atath, the Harbinger, was once known as the Blade of the Black Empire. Her history is a well-kept secret. But her words and deeds have affected so many that she is impossible to ignore. Her story must be told.','',NULL,69497),
(845,4322,1,6403387,0,1572,0,0,0,0,'Begin','Ethereals','','Who are the ethereals?','Mysterious and elusive, the ethereals that have traveled to our world speak little of their history. Yet there is much to be learned from the few things they do choose to share...','',NULL,69497),
(845,4379,2,6403388,0,1573,0,0,0,0,'Begin','The Lich King','','','Arthas Menethil. It is said that when he was born, the very forests of Lordaeron whispered his name. But did the forests of his youth know what he would eventually grow to become?','',NULL,69497),
(845,5047,4,6403386,0,1574,0,0,1,0,'Continue','Elves','In Progress','','The elves of Azeroth each have their own stories that weave together into an intricate tapestry of history. For the blood elves and the void elves, it is a story shaped by strength in the face of utmost tragedy.','',NULL,69497),
(845,5048,5,6403386,0,1574,0,0,1,0,'Start Over','Elves','Start Over','','The elves of Azeroth each have their own stories that weave together into an intricate tapestry of history. For the blood elves and the void elves, it is a story shaped by strength in the face of utmost tragedy.','11.1.7 Lorewalking - Restart (LAS)',NULL,69497),
(845,5419,6,7525957,0,1860,0,0,0,0,'Begin','Loa','','','The trolls of Azeroth have long found power in their relationship with the loa. But what are the loa? How have they influenced history?','',NULL,69497);

-- Elves: Begin while "Lorewalking: Children of the Blood" is not in progress, Continue / Start Over while it is (parked)
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=36 AND `SourceGroup`=845 AND `SourceEntry` IN (5046,5047,5048);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`,`ConditionStringValue1`,`NegativeCondition`,`ErrorType`,`ErrorTextId`,`ScriptName`,`Comment`) VALUES
(36,845,5046,0,0,47,0,85252,10,0,'',1,0,0,'','Lorewalking Elves - Begin: Children of the Blood not in progress'),
(36,845,5047,0,0,47,0,85252,10,0,'',0,0,0,'','Lorewalking Elves - Continue: Children of the Blood in progress'),
(36,845,5048,0,0,47,0,85252,10,0,'',0,0,0,'','Lorewalking Elves - Start Over: Children of the Blood in progress');

-- Li Li Stormstout: "(Lorewalking) What stories can you tell me?" casts Lorewalking Choice
UPDATE `creature_template` SET `AIName`='SmartAI' WHERE `entry`=230246;
DELETE FROM `smart_scripts` WHERE `entryorguid`=230246 AND `source_type`=0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_param1`,`event_param2`,`action_type`,`action_param1`,`action_param2`,`target_type`,`comment`) VALUES
(230246,0,0,1,62,36405,0,72,0,0,7,'Li Li Stormstout - On Gossip Option 0 Selected - Close Gossip'),
(230246,0,1,0,61,0,0,134,463923,0,7,'Li Li Stormstout - Linked - Invoker Cast \'Lorewalking Choice\'');

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_lorewalking_cast_next','spell_lorewalking_enter_story','spell_lorewalking_enter_story_cast_intro','spell_lorewalking_teleport_to_tirisfal','spell_lorewalking_exit','spell_lorewalking_teleport_to_base_arrival','spell_lorewalking_ask_a_question');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(467482,'spell_lorewalking_cast_next'),
(468532,'spell_lorewalking_cast_next'),
(471657,'spell_lorewalking_cast_next'),
(467592,'spell_lorewalking_cast_next'),
(1258364,'spell_lorewalking_cast_next'),
(1239389,'spell_lorewalking_cast_next'),
(467524,'spell_lorewalking_cast_next'),
(1275636,'spell_lorewalking_cast_next'),
(473059,'spell_lorewalking_cast_next'),
(463926,'spell_lorewalking_enter_story'),
(468534,'spell_lorewalking_enter_story'),
(471655,'spell_lorewalking_enter_story'),
(467591,'spell_lorewalking_enter_story'),
(1258361,'spell_lorewalking_enter_story_cast_intro'),
(463941,'spell_lorewalking_teleport_to_tirisfal'),
(460937,'spell_lorewalking_exit'),
(1239378,'spell_lorewalking_teleport_to_base_arrival'),
(468124,'spell_lorewalking_ask_a_question');

-- Chapter teleports: destinations as captured (SMSG_NEW_WORLD after the transfer, SMSG_MOVE_TELEPORT for same-map moves)
DELETE FROM `spell_target_position` WHERE `ID` IN (463980,465993,466098,466122,466129,466134,466140,466161,466289,466301,466485,474632,1215651,1215654,1215663,1215666,1215669,1215671,1219292,1238159,1238191,1239362,1239784,1259916,1259920,1259924,1260858,1260921,1271889,1275602);
INSERT INTO `spell_target_position` (`ID`,`EffectIndex`,`OrderIndex`,`MapID`,`PositionX`,`PositionY`,`PositionZ`,`Orientation`,`VerifiedBuild`) VALUES
(463980,0,0,2801,-830.08,-41.48,-234.27,6.2106,69497), -- [DNT] Teleport to Nyalotha
(465993,1,0,530,4254.42,2169.38,137.68,4.2295,69497), -- [DNT] Teleport to Netherstorm
(466098,0,0,530,4272.1,2132.02,138.54,6.2116,69497), -- [DNT] Teleport
(466122,0,0,2222,-1833.91,1161.94,5271.09,4.6426,69497), -- [DNT] Teleport
(466129,0,0,2222,3287.94,5710.85,4940.47,0.4477,69497), -- [DNT] Teleport
(466134,0,0,2222,-3338.84,6544.63,3992.8,3.1936,69497), -- [DNT] Teleport
(466140,0,0,1669,5328.11,10391.6,-76.95,1.0551,69497), -- [DNT] Teleport
(466161,0,0,1865,2055.13,3398.11,55.53,4.7171,69497), -- [DNT] Teleport
(466289,0,0,1642,3741.7,3163.7,0.07,3.588,69497), -- [DNT] Teleport to Vol'dun
(466301,0,0,2123,3485.1,-1106.88,-628.19,1.5942,69497), -- [DNT] Teleport to Crucible of Storms
(466485,0,0,1643,3958.06,1401.91,70.53,0.8514,69497), -- [DNT] Teleport to BfA
(474632,0,0,530,7582.13,-6812.86,86.59,3.1213,69497), -- Blood Elves Teleport: Accept [DNT]
(1215651,0,0,2818,826.18,608.3,13.48,6.2636,69497), -- Teleport (DNT)
(1215654,0,0,595,1431.47,555.038,36.2723,5.0615,69497), -- Teleport (DNT)
(1215663,0,0,571,6104.04,2220.15,518.01,3.5891,69497), -- Teleport (DNT)
(1215666,0,0,668,5239.46,1932.99,707.695,0.7854,69497), -- Teleport (DNT)
(1215669,0,0,2819,428.75,-2122.74,864.88,0.0063,69497), -- Teleport (DNT)
(1215671,0,0,571,3434.63,-1261.32,125.4,3.3278,69497), -- Teleport (DNT)
(1219292,0,0,595,2369.57,1408.82,128.63,3.2337,69497), -- CoT Stratholme Skip [DNT]
(1238159,0,0,1643,-173.8,4131.22,123.41,0.2253,69497), -- [DNT] Teleport to Drustvar
(1238191,0,0,1643,-1521.85,-577.98,68.71,3.1091,69497), -- [DNT] Teleport to Freehold
(1239362,0,0,0,-8181.26,609.63,73.4,3.9827,69497), -- [DNT] Lorewalking Teleport to Base
(1239784,0,0,2222,-1908.73,1466.38,5273.59,3.1758,69497), -- [DNT] Teleport to Oribos
(1259916,1,0,1642,-730.57,1110.07,320.8,2.8957,69497), -- [DNT] Teleport to Zuldazar
(1259920,0,0,571,5749.96,-3571.1,386.59,3.1159,69497), -- [DNT] Teleport to Zim'Torga
(1259924,0,0,571,5754.0,-3585.05,386.5,4.4022,69497), -- [DNT] Teleport to Har'koa
(1260858,0,0,2291,2740.83,-1756.73,54.8716,1.5947,69497), -- Teleport Hakkar (DNT)
(1260921,0,0,1,-1268.96,-5550.56,20.89,3.8633,69497), -- [DNT] Teleport to Echo Isles
(1271889,0,0,0,-10455.3,-3822.3,17.87,6.0662,69497), -- [DNT] Teleport to Sunken Temple
(1275602,0,0,530,12579.6,-6774.14,15.09,3.1604,69497); -- [DNT] Lorewalking Elves: Catch Up Teleport

-- [DNT] Blade in Twilight: retail 12.1 puts the Lorewalking player here (SMSG_NEW_WORLD of the LFG transfer)
UPDATE `lfg_dungeon_template` SET `position_x`=2054.11, `position_y`=2454.43, `position_z`=132.489, `orientation`=0.8455, `VerifiedBuild`=69497 WHERE `dungeonId`=1381;
