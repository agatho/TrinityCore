--
-- world_quest_template: 147 activation rows recovered from the 12.1.0.69273 capture
--   (C:/dumps/sniff_69273/12.1.0.69273_preyandwqpart1.pkt, 20 snapshots, union 307 QuestIDs).
--
-- PROVENANCE / SAFETY
--   The packet was identified by STRUCTURE + CONTENT, never by opcode number (the 12.1.0
--   opcode space is renumbered and MUST NOT be mapped onto our build):
--     body = uint32 count + count * 24, entry = {int64 LastUpdate, uint32 QuestID,
--     uint32 Timer, int32 VariableID, int32 Value} -- byte-exact, zero slack, against
--     WorldQuestUpdateResponse::Write (src/server/game/Server/Packets/QuestPackets.cpp).
--   Two independent decoders produced identical values for all 307 rows.
--   Build-stability measured, not assumed: of the 305 rows that overlap this branch's
--   effective world_quest_template (389 rows after 2026_07_09_00 + 2026_08_08_00/01/02),
--   158 are byte-identical and only 2 differ (handled in a separate file - see below).
--   Rows referencing a QuestID with no quest_template row are skipped at load with an
--   error log (WorldQuestMgr::LoadFromDB), so unknown Midnight-range ids are harmless.
--
--
-- ORDERING NOTE (read before re-running anything by hand)
--   2026_08_08_00_world.sql line 8 is an UNSCOPED `DELETE FROM world_quest_template;`.
--   A clean database build is safe, because filenames sort and 2026_08_13 runs after
--   2026_08_08. But re-running 2026_08_08_00 by hand AFTER this file will silently wipe
--   these 147 rows along with everything else. If you need to re-seed, re-run the whole
--   world_quest_template chain in filename order: 2026_07_09_00 -> 2026_08_08_00 ->
--   2026_08_08_01 -> 2026_08_08_02 -> this file.
--
-- FLAGGED, DO NOT APPLY BLIND. Two rows where the 12.1.0.69273 capture disagrees with the
-- branch's current value. Both are single observations from a DIFFERENT patch; the branch's
-- values came from 12.0.7.68974 captures. Activation worldstates are known to change between
-- rotation instances (see 2026_08_08_02_world.sql, which adopted exactly such a change for
-- quest 49091 - and the 12.1.0 capture independently agrees with that corrected value, 14062).
-- Decide per row; do not run this file as part of a batch.
--
-- 49099: branch VariableID 14063  ->  capture 14244   (Duration/Value unchanged)
-- UPDATE `world_quest_template` SET `VariableID` = 14244 WHERE `QuestID` = 49099;
--
-- 79173: branch Value 1  ->  capture 2                (Duration/VariableID unchanged)
-- UPDATE `world_quest_template` SET `Value` = 2 WHERE `QuestID` = 79173;
--
-- NON-DESTRUCTIVE: INSERT IGNORE only. No DELETE, no UPDATE, no TRUNCATE in this file.
--
INSERT IGNORE INTO `world_quest_template` (`QuestID`, `Duration`, `VariableID`, `Value`) VALUES
 (43473,86400,12220,1), -- Experimental Potion: Test Subjects Needed
 (47728,86400,13676,1), -- Talestra the Vile
 (47858,86400,13931,2), -- Security: Engaged
 (47953,86400,13824,1), -- Tereck the Selector
 (48374,86400,13987,1), -- Supplies Needed: Lightweave Cloth
 (48466,86400,14010,1), -- Ven'orn
 (48512,86400,14048,1), -- Sister Subversia
 (48614,86400,14130,2), -- Woeful Implications
 (48701,86400,14169,1), -- Baruut the Bloodthirsty
 (48733,86400,14183,1), -- Jed'hin Champion Vorusk
 (48738,86400,14188,1), -- Zul'tan the Numerous
 (48739,86400,14189,1), -- Commander Xethgar
 (48783,43200,14201,1), -- Nobody Expects Them
 (48830,86400,14212,1), -- Inquisitor Vethroz
 (48866,86400,14232,1), -- Void Warden Valsuran
 (48951,604800,14288,1), -- Seat of the Triumvirate: Voidmaw
 (48952,43200,14294,1), -- Throw Them a Bone
 (48958,43200,14299,1), -- Ritual Interruption
 (49054,86400,14320,1), -- Bloat
 (49196,604800,14249,1), -- Greater Invasion Point: Pit Lord Vilemus
 (52948,604800,16006,2), -- Call to Arms: Tiragarde Sound
 (53236,86400,16737,1), -- Arathi Donations: War-Scroll of Fortitude
 (53334,86400,16717,2), -- Arathi Donations: War Resources
 (53363,86400,16742,1), -- Arathi Donations: Drums of the Maelstrom
 (56141,86400,17639,3), -- Security First
 (56172,86400,17734,2), -- Other Interests
 (59018,604800,17934,2), -- Call to Arms: Vale of Eternal Blossoms
 (60215,604800,27341,2), -- Timely Gate Crashers
 (65784,270000,22309,1), -- The Otter Side
 (65792,270000,22311,1), -- Teeth for a Tooth
 (65796,270000,21623,0), -- The Best Defense...
 (66070,302400,22478,1), -- Brightblade's Bones
 (66551,86400,22579,1), -- The Terrible Three
 (66698,270000,22312,1), -- Counting Argali
 (69916,302400,22178,1), -- Famous Frogs
 (69928,604800,22192,4), -- Liskanoth
 (69949,86400,22242,1), -- Extermination
 (70068,604800,22218,2), -- Cobalt Catastrophe
 (70074,302400,22257,1), -- Plunder the Sundered
 (70110,302400,22409,1), -- Cataloging Thaldraszus
 (70112,302400,22248,1), -- Furbolg Threat
 (70176,302400,22349,1), -- Web Victims
 (70224,302400,22347,1), -- Fetid Threat
 (70410,302400,22378,1), -- Dragonrider Racing - Flashfrost Flyover
 (70423,302400,22384,1), -- Dragonrider Racing - Maruukai Dash
 (70426,302400,22387,1), -- Dragonrider Racing - Azure Span Slalom
 (70549,302400,22441,1), -- Low Hanging Fruit
 (70612,302400,22449,1), -- Feed Three-Falls
 (70652,302400,22467,1), -- Take One Down, Pass It Around
 (70658,302400,22471,1), -- Artifact or Fiction
 (71140,86400,22582,1), -- Two and Two Together
 (71180,86400,22600,1), -- You Have to Start Somewhere
 (72028,604800,22963,1), -- Fishing Frenzy!
 (72058,86400,22665,1), -- What Hoof We Here: Tarolekk
 (72090,302400,22685,1), -- Disrupting the Primalist Plan
 (73084,302400,23020,1), -- Dragonrider Racing - Forbidden Reach Rush
 (73146,86400,23023,1), -- Cutting Wind
 (74378,7200,22894,1), -- The Storm's Fury
 (74835,86400,23250,1), -- Enok the Stinky
 (74879,259200,23571,1), -- Corrosive Counterbalance
 (74990,604800,23307,1), -- Roiling Shadow
 (75061,259200,23033,1), -- No Mushroom For Ever
 (75071,259200,23334,1), -- Sniffing Mice are Nice
 (75151,259200,23572,1), -- Redistributing the Remnants
 (75343,259200,23575,1), -- All That Glitter
 (75661,259200,23716,1), -- Curative Crystalline Collection
 (75680,302400,23731,1), -- To a Land Down Under
 (76244,86400,24022,1), -- Prince in Peril
 (76518,259200,24682,1), -- Root Security
 (76519,259200,24687,1), -- All The Children
 (76990,259200,24734,1), -- Portal Panic
 (76991,259200,24736,1), -- Carpe Diem
 (77754,259200,24726,1), -- Pyromania Problems
 (77757,259200,24725,1), -- Terror in Haven
 (78015,259200,24526,1), -- Firebrand Fystia
 (78915,604800,23587,2), -- Squashing the Threat
 (78933,604800,20119,2), -- The Sweet Eclipse
 (79158,604800,22006,1), -- Seeds of Salvation
 (79346,604800,23587,2), -- Chew On That
 (80395,86400,25932,1), -- Elemental Excavation
 (81465,345600,26029,1), -- Artifacts Galore
 (81574,604800,26056,2), -- Sporadic Growth
 (81632,604800,20115,2), -- Lizard Looters
 (81675,345600,26231,1), -- Water the Sheep
 (81750,345600,26097,1), -- Cloud Farming
 (81767,345600,26254,1), -- Scrounge that Scrap!
 (81804,302400,26107,1), -- Skyrider Racing - The Wold Ways
 (81807,302400,26110,1), -- Skyrider Racing - Earthenworks Weave
 (81818,302400,26118,1), -- Skyrider Racing - Light's Redoubt Descent
 (82197,345600,26214,1), -- Reserve Rumpus
 (82256,345600,26249,1), -- Capturing the Cataract's Creatures
 (82288,345600,26264,1), -- Work Hard, Play Hard
 (82291,86400,26281,1), -- Robot Rumble
 (82451,345600,26358,1), -- Preserving Plush Pals
 (82455,345600,26361,1), -- No More Bread
 (82582,345600,26481,1), -- Mired in Shadow
 (82585,345600,26480,1), -- With Great Pyre
 (82653,604800,26045,2), -- Aggregation of Horrors
 (82962,604800,26464,3), -- A Handful of Luredrops
 (83457,604800,26587,4), -- The Stonevault
 (83753,604800,26672,0), -- Cannon Karma
 (83827,604800,26672,0), -- Silence the Song
 (83930,345600,26721,1), -- Deworming Solution
 (84001,604800,26672,0), -- Cart Blanche
 (84299,604800,26672,0), -- Pirate Plunder
 (84619,604800,26672,0), -- Ooker Dooker Literature Club
 (84851,604800,26672,0), -- Tides of Greed
 (85822,302400,27361,1), -- Making a Market
 (85855,302400,27385,1), -- Anything to Declare?
 (85864,302400,27678,1), -- Phase Diving: Fractured Laacunite
 (85926,302400,27424,1), -- Breakneck Racing - Junkyard Jaunt
 (86372,302400,27570,1), -- Wasting the Wastelanders
 (86391,302400,29179,1), -- Taking Back our Power
 (86395,302400,27580,1), -- Stealing the Stolen
 (86584,302400,28882,3), -- Overwhelm Them with Mandatory Time Off
 (86810,604800,26056,2), -- Harvesting the Void
 (86869,302400,27769,1), -- Phase Diving: Shan'dorah Saboteurs
 (89377,43200,29912,1), -- Undercover Hunt
 (91193,604800,28300,1), -- Special Assignment: Capstone 1 - Unlock
 (91555,302400,29155,1), -- Defenders of the Vale
 (91802,302400,29312,1), -- The Best Bites are Bog Bugs
 (91810,302400,29319,1), -- Blistereel Boar Buffet
 (92013,86400,29482,1), -- WANTED: Dionaea's Thorntusks
 (92120,302400,29959,1), -- To Understand Magic
 (92123,604800,29468,2), -- Cragpine
 (92140,302400,29515,1), -- Uprooting Efforts
 (92141,302400,29516,1), -- Classic Threats
 (92153,302400,29521,1), -- The Moon at Twilight
 (92582,302400,29163,1), -- Apply to Roots
 (92746,302400,29859,1), -- The Twist of the Stormfields
 (93053,302400,29164,1), -- Cleaning the Den
 (93397,302400,29849,1), -- Gnawing Hunger
 (93426,604800,30117,4), -- Sparks of War: Voidstorm
 (93499,604800,30131,1), -- Enshrouded in Arenas
 (93507,302400,29852,1), -- Disrupting the Void
 (93524,302400,29862,1), -- Trench Run
 (93701,604800,30194,2), -- Brittle and Brilliant
 (93752,604800,30402,2), -- Murder Row
 (94386,604800,29528,2), -- Void Assaults: Zul'Aman
 (94866,604800,29306,1), -- Special Assignment: Ours Once More!
 (96591,86400,30562,4), -- Prey: Venom Ambush
 (96726,604800,30609,1), -- Sparks of War: Naigtal
 (97084,604800,30609,1), -- More Disruption: Naigtal
 (97085,604800,30609,1), -- Dangerous Enemies: Naigtal
 (97086,604800,30609,1), -- Dangerous Enemies: Naigtal (Heroic)
 (97087,604800,30609,1), -- More Disruption: Naigtal (Heroic)
 (98172,1209600,32231,1); -- 
