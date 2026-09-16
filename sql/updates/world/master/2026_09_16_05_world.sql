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

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_lorewalking_cast_next','spell_lorewalking_enter_story','spell_lorewalking_enter_story_cast_intro','spell_lorewalking_exit','spell_lorewalking_ask_a_question');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(467482,'spell_lorewalking_cast_next'),
(468532,'spell_lorewalking_cast_next'),
(471657,'spell_lorewalking_cast_next'),
(467592,'spell_lorewalking_cast_next'),
(1258364,'spell_lorewalking_cast_next'),
(1239389,'spell_lorewalking_cast_next'),
(467524,'spell_lorewalking_cast_next'),
(1275636,'spell_lorewalking_cast_next'),
(463926,'spell_lorewalking_enter_story'),
(468534,'spell_lorewalking_enter_story'),
(471655,'spell_lorewalking_enter_story'),
(467591,'spell_lorewalking_enter_story'),
(1258361,'spell_lorewalking_enter_story_cast_intro'),
(460937,'spell_lorewalking_exit'),
(468124,'spell_lorewalking_ask_a_question');
