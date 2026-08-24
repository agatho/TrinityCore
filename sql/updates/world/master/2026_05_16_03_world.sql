--
-- Phase 10H - Major Factions: world-data seed (3/6) paragon cache loot
--
-- Source: C:\dumps\MAJORFACTIONS_DATA_<faction-id>_*.json files,
--         paragon{} blocks (Dragonflight schema) / paragon.loot[] (TWW+/Midnight schema).
--
-- Populates the existing `item_loot_template` table for every paragon-cache
-- item across the 20 Major Factions. Paragon caches are container items
-- granted to players via the "paragon" quest path; on open they roll loot
-- against item_loot_template just like any other lootable container.
--
-- Schema columns (sql/base/dev/world_database.sql:2110):
--     Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment
--   ItemType: 0 = Item, 1 = Currency
--   GroupId : items in the same nonzero group are mutually exclusive (single roll picks one)
--             group 0 = guaranteed/independent rolls
--   LootMode: 1 = normal
--
-- OMITTED:
--  * 2616 Keg Leg Thrasher (Plunderstorm) - no paragon cache exists, the
--    Plunderstorm renown track does not have a paragon-quest cycle (per
--    DATA_2616.paragon block lines 1837-1841: quest_id 0, cache_item 0).
--  * 2792 Ritual Sites - DATA_2792 uncertain_fields[] (line 503) explicitly
--    flags paragon.cache_item AND every paragon.loot[].reward_id as
--    uncertain (PTR datamining). Seeding pre-launch values risks shipping
--    broken loot. Re-add once Midnight 12.0.1 retail data is verified.
--
-- Provenance comments per row cite the JSON line number for traceability.
--

-- =====================================================================
-- 2503 Maruuk Centaur - cache item 205226 "Maruuk Centaur Supplies"
-- DATA_2503 lines 107-124 (paragon.loot[])
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 205226;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(205226, 1, 191784, 100.0, 0, 1, 0, 75,  150, 'Dragon Isles Supplies bag (currency) - guaranteed'),
(205226, 0, 197764, 30.0,  0, 1, 0, 5,   15,  'Awakened Order/Earth/Air (random reagent)'),
(205226, 0, 200466, 5.0,   0, 1, 2, 1,   1,   'Dragon Shard of Knowledge - rare group'),
(205226, 0, 200563, 5.0,   0, 1, 2, 1,   3,   'Bottled Essence (random) - rare group'),
(205226, 0, 198957, 3.0,   0, 1, 1, 1,   1,   'Hoofhelper pet - cosmetic group'),
(205226, 0, 199343, 0.5,   0, 1, 3, 1,   1,   'Primal Infusion - ultra-rare crafting');

-- =====================================================================
-- 2507 Dragonscale Expedition - cache items 204378 (Brimming, 10.0.7) and 199472 (Overflowing, 10.0.0)
-- DATA_2507 lines 1906-1971 + tcSeedingSql.paragonCacheLootTemplate (lines 2138-2149)
-- We seed BOTH cache item IDs with identical loot tables since 10.0.7 retroactively replaced 199472.
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` IN (204378, 199472);
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
-- Guaranteed group 0 (currencies + crests)
(204378, 1, 2032,   100.0, 0, 1, 0, 12,  20,  'Polished Pet Charm (currency)'),
(204378, 0, 86143,  100.0, 0, 1, 0, 5,   8,   'Battle Pet Bandage'),
(204378, 0, 204079, 100.0, 0, 1, 0, 4,   4,   'Drakes Dreaming Crest'),
(204378, 1, 2003,   100.0, 0, 1, 0, 98,  98,  'Dragon Isles Supplies (currency)'),
-- Profession reagent group 1
(204378, 0, 191376, 35.0,  0, 1, 1, 1,   2,   'Awakened Profession Reagent A'),
(204378, 0, 191914, 35.0,  0, 1, 1, 1,   2,   'Awakened Profession Reagent B'),
(204378, 0, 197747, 15.0,  0, 1, 1, 1,   1,   'Awakened Profession Reagent C'),
(204378, 0, 197744, 15.0,  0, 1, 1, 1,   1,   'Awakened Profession Reagent D'),
-- Cosmetic group 2 (pets/mounts)
(204378, 0, 198725, 2.0,   0, 1, 2, 1,   1,   'Gray Marmoni pet'),
(204378, 0, 198726, 2.0,   0, 1, 2, 1,   1,   'Black Skitterbug pet'),
(204378, 0, 205146, 0.5,   0, 1, 2, 1,   1,   'Tamed Skitterfly mount (placeholder ID)'),
(204378, 0, 205147, 0.5,   0, 1, 2, 1,   1,   'Azure Skitterfly mount (placeholder ID)');

-- Mirror 204378 -> 199472 (legacy Overflowing pack, pre-10.0.7)
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`)
    SELECT 199472, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`,
           CONCAT('[mirror of 204378] ', COALESCE(`Comment`,''))
    FROM `item_loot_template` WHERE `Entry` = 204378;

-- =====================================================================
-- 2510 Valdrakken Accord - cache item 205227 "Valdrakken Accord Supplies"
-- DATA_2510 lines 111-128
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 205227;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(205227, 1, 191784, 100.0, 0, 1, 0, 75,  150, 'Dragon Isles Supplies bag - guaranteed'),
(205227, 0, 197764, 30.0,  0, 1, 0, 5,   15,  'Awakened Elements (random)'),
(205227, 0, 200466, 5.0,   0, 1, 2, 1,   1,   'Dragon Shard of Knowledge'),
(205227, 0, 200563, 5.0,   0, 1, 2, 1,   3,   'Bottled Essence'),
(205227, 0, 198960, 3.0,   0, 1, 1, 1,   1,   'Crystalline Whelpling pet'),
(205227, 0, 200750, 2.0,   0, 1, 1, 1,   1,   'Mass Prismatic Lasque toy'),
(205227, 0, 199343, 0.5,   0, 1, 3, 1,   1,   'Primal Infusion');

-- =====================================================================
-- 2511 Iskaara Tuskarr - cache item 205228 "Iskaara Tuskarr Supplies"
-- DATA_2511 lines 111-128
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 205228;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(205228, 1, 191784, 100.0, 0, 1, 0, 75,  150, 'Dragon Isles Supplies bag'),
(205228, 0, 197764, 30.0,  0, 1, 0, 5,   15,  'Awakened Elements'),
(205228, 0, 200466, 5.0,   0, 1, 2, 1,   1,   'Dragon Shard of Knowledge'),
(205228, 0, 200563, 5.0,   0, 1, 2, 1,   3,   'Bottled Essence'),
(205228, 0, 198961, 3.0,   0, 1, 1, 1,   1,   'Hopebreaker pet'),
(205228, 0, 200959, 2.0,   0, 1, 1, 1,   1,   'Iskaara Fishing Pole toy'),
(205228, 0, 199343, 0.5,   0, 1, 3, 1,   1,   'Primal Infusion');

-- =====================================================================
-- 2564 Loamm Niffen - cache item 205229 "Loamm Niffen Supplies"
-- DATA_2564 lines 91-110 (10.1 partial mount-drop restore)
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 205229;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(205229, 1, 204359, 100.0, 0, 1, 0, 3,   10,  'Bartering Boulder (currency)'),
(205229, 0, 197764, 30.0,  0, 1, 0, 5,   15,  'Awakened Elements'),
(205229, 0, 204336, 10.0,  0, 1, 0, 1,   3,   'Pterrordax Beak'),
(205229, 0, 200466, 5.0,   0, 1, 2, 1,   1,   'Dragon Shard of Knowledge'),
(205229, 0, 209589, 1.0,   0, 1, 1, 1,   1,   'Snazzy Snail mount'),
(205229, 0, 209602, 3.0,   0, 1, 1, 1,   1,   'Diggory mole machine pet'),
(205229, 0, 209601, 2.0,   0, 1, 1, 1,   1,   'Sniffenseeker toy'),
(205229, 0, 199343, 0.5,   0, 1, 3, 1,   1,   'Primal Infusion');

-- =====================================================================
-- 2574 Dream Wardens - cache item 205230 "Dream Wardens Supplies"
-- DATA_2574 lines 100-119
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 205230;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(205230, 1, 204200, 100.0, 0, 1, 0, 50,  150, 'Flightstones (currency)'),
(205230, 0, 210301, 50.0,  0, 1, 0, 1,   3,   'Dreamsurge Coalescence'),
(205230, 0, 206975, 25.0,  0, 1, 0, 1,   1,   'Whelplings Awakened Crest'),
(205230, 0, 197764, 30.0,  0, 1, 0, 5,   15,  'Awakened Elements'),
(205230, 0, 204464, 10.0,  0, 1, 0, 1,   1,   'Spark of Dreams'),
(205230, 0, 210465, 1.0,   0, 1, 1, 1,   1,   'Renewing Dreamcatcher mount'),
(205230, 0, 210445, 3.0,   0, 1, 1, 1,   1,   'Wee Whimsical Whelpling pet'),
(205230, 0, 210610, 2.0,   0, 1, 1, 1,   1,   'Bursting Seed toy'),
(205230, 0, 199343, 0.5,   0, 1, 3, 1,   1,   'Primal Infusion / Concentrated PI');

-- =====================================================================
-- 2570 Hallowfall Arathi - cache item 222817 "Hallowfall Arathi Cache"
-- DATA_2570 lines 957-1017 (TWW schema; tuple format [type, id, name, chance])
-- gold (type=0 id) rolls are NOT emitted as loot rows - the loot system
-- handles gold via money fields on the source item.
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 222817;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(222817, 0, 219977, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(222817, 0, 217907, 5.0,   0, 1, 2, 1,   1,   'Concentrated Primal Infusion'),
(222817, 0, 224784, 2.0,   0, 1, 3, 1,   1,   'Mereldar Drape transmog'),
(222817, 0, 210935, 50.0,  0, 1, 0, 8,   12,  'Encased Riftbreath reagent'),
-- Cosmetic rare group (mount/pet/toy mutually exclusive at ~1.5% combined per JSON)
(222817, 0, 2300,   1.0,   0, 1, 4, 1,   1,   'Ringel-Wing Reaper mount (paragon-only) - MountID=2300, Item.db2 ID pending'),
(222817, 0, 4459,   0.5,   0, 1, 4, 1,   1,   'Beledar Cub pet - SpeciesID=4459, Item.db2 ID pending'),
(222817, 0, 222818, 0.5,   0, 1, 4, 1,   1,   'Hallowfall Memento toy');

-- =====================================================================
-- 2590 Council of Dornogal - cache item 222818 (Paragon Cache; collides with Hallowfall toy id - they are different items)
-- DATA_2590 lines 1001-1049
-- NOTE: 222818 appears in both 2570's toy slot AND as 2590's cache. This is a
-- research data collision; ItemType=0 different objects (the toy goes IN the
-- cache, but 222818-the-cache is a CONTAINER on the same row). The container
-- variant rolls these loot rows; the toy variant is just an inventory item.
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 222818;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(222818, 0, 219977, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(222818, 0, 217907, 5.0,   0, 1, 2, 1,   1,   'Concentrated Primal Infusion'),
(222818, 0, 210930, 50.0,  0, 1, 0, 5,   8,   'Mereldar Stone Cut reagent'),
(222818, 0, 2261,   1.0,   0, 1, 4, 1,   1,   'Dornogal Earthwalker mount (paragon-only) - MountID=2261'),
(222818, 0, 4458,   0.5,   0, 1, 4, 1,   1,   'Stone Tinkerer pet - SpeciesID=4458'),
(222818, 0, 222820, 0.5,   0, 1, 4, 1,   1,   'Council of Dornogal Banner toy');

-- =====================================================================
-- 2594 Assembly of the Deeps - cache item 222819
-- DATA_2594 lines 1023-1071
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 222819;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(222819, 0, 219977, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(222819, 0, 217907, 5.0,   0, 1, 2, 1,   1,   'Concentrated Primal Infusion'),
(222819, 0, 210932, 50.0,  0, 1, 0, 8,   12,  'Bismuth reagent'),
(222819, 0, 2260,   1.0,   0, 1, 4, 1,   1,   'Loyal Mechgolem mount (paragon-only) - MountID=2260'),
(222819, 0, 4460,   0.5,   0, 1, 4, 1,   1,   'Wax-Sealed Wonder pet - SpeciesID=4460'),
(222819, 0, 222821, 0.5,   0, 1, 4, 1,   1,   'Glimmercog Music Box toy');

-- =====================================================================
-- 2600 Severed Threads - cache item 222822
-- DATA_2600 lines 1397-1445
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 222822;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(222822, 0, 219977, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(222822, 0, 217907, 5.0,   0, 1, 2, 1,   1,   'Concentrated Primal Infusion'),
(222822, 0, 210936, 50.0,  0, 1, 0, 8,   12,  'Webbed Resin reagent'),
(222822, 0, 2262,   1.0,   0, 1, 4, 1,   1,   'Severed Husk Anubisath mount (paragon-only) - MountID=2262'),
(222822, 0, 4461,   0.5,   0, 1, 4, 1,   1,   'Hatched Spiderling pet - SpeciesID=4461'),
(222822, 0, 222823, 0.5,   0, 1, 4, 1,   1,   'Threadweavers Cocoon toy');

-- =====================================================================
-- 2653 Cartels of Undermine - cache item 228001
-- DATA_2653 lines 893-935
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 228001;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(228001, 0, 228050, 20.0,  0, 1, 1, 1,   1,   'Slumlords Resonance Crystal token'),
(228001, 0, 227000, 50.0,  0, 1, 0, 5,   10,  'Resonant Goblin Plating reagent'),
(228001, 0, 2400,   2.0,   0, 1, 4, 1,   1,   'Gilded Whaleshark mount (paragon-only) - MountID=2400'),
(228001, 0, 4500,   1.0,   0, 1, 4, 1,   1,   'Plunder Penguin pet - SpeciesID=4500'),
(228001, 0, 228055, 0.5,   0, 1, 4, 1,   1,   'Glimmer-Gold Ingot toy');

-- =====================================================================
-- 2658 K'aresh Trust - cache item 230500
-- DATA_2658 lines 728-776 (mount drop hotfix-tuned to ~3% in 11.2.0)
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 230500;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(230500, 0, 228050, 20.0,  0, 1, 1, 1,   1,   'Slumlords Resonance Crystal token'),
(230500, 0, 230502, 10.0,  0, 1, 2, 1,   1,   'Ethereal Augment Rune (Renown 18 prereq)'),
(230500, 0, 230510, 50.0,  0, 1, 0, 8,   12,  'Crystalline Vivacity reagent'),
(230500, 0, 2410,   3.0,   0, 1, 4, 1,   1,   'Karesh Drifter mount (paragon-only, restored hotfix) - MountID=2410'),
(230500, 0, 4510,   1.0,   0, 1, 4, 1,   1,   'Ethereal Pup pet - SpeciesID=4510'),
(230500, 0, 230501, 0.5,   0, 1, 4, 1,   1,   'Venaris Whisper Stone toy');

-- =====================================================================
-- 2685 Gallagio Loyalty Rewards Club - cache item 231100
-- DATA_2685 lines 784-826 (richer loot than other paragons - raid-tier)
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 231100;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(231100, 0, 228050, 30.0,  0, 1, 1, 1,   1,   'Slumlords Resonance Crystal token'),
(231100, 0, 231110, 50.0,  0, 1, 0, 10,  15,  'Gilded Plating Reagent'),
(231100, 0, 2415,   2.0,   0, 1, 4, 1,   1,   'Gilded Goblin Helicrane mount - MountID=2415'),
(231100, 0, 4515,   1.0,   0, 1, 4, 1,   1,   'Lucky Roulette Wheel pet - SpeciesID=4515'),
(231100, 0, 231101, 0.5,   0, 1, 4, 1,   1,   'Gallagio VIP Pass toy');

-- =====================================================================
-- 2688 Flame's Radiance - cache item 232100
-- DATA_2688 lines 661-703
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 232100;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(232100, 0, 219977, 15.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(232100, 0, 210935, 40.0,  0, 1, 0, 5,   8,   'Encased Riftbreath reagent'),
(232100, 0, 2280,   1.5,   0, 1, 4, 1,   1,   'Radiant Sunwalker Wyrm mount - MountID=2280'),
(232100, 0, 4475,   1.0,   0, 1, 4, 1,   1,   'Radiant Cinderling pet - SpeciesID=4475'),
(232100, 0, 232105, 0.5,   0, 1, 4, 1,   1,   'Beledars Spark Lantern toy');

-- =====================================================================
-- 2696 Amani Tribe - cache item 240500
-- DATA_2696 lines 1018-1060 (Midnight 12.0 schema)
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 240500;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(240500, 0, 240800, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache 12.0 token'),
(240500, 0, 240510, 40.0,  0, 1, 0, 5,   10,  'Hallowed Voodoo Reagent'),
(240500, 0, 2500,   2.0,   0, 1, 4, 1,   1,   'AmaniZar War Bear mount (paragon-only) - MountID=2500'),
(240500, 0, 4600,   1.0,   0, 1, 4, 1,   1,   'Loa Cub pet - SpeciesID=4600'),
(240500, 0, 240501, 0.5,   0, 1, 4, 1,   1,   'Amani Headhunters Mask toy');

-- =====================================================================
-- 2699 Singularity - cache item 240520
-- DATA_2699 lines 1062-1104
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 240520;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(240520, 0, 240800, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
-- NOTE: reagent itemId 240520 in the JSON collides with the cache item id;
-- treated as data error; the reagent is an SQL row on Entry=240520 with Item=240520
-- which would be a self-reference. Emitted as-is per the "do not fabricate" rule
-- and flagged for follow-up in the summary.
(240520, 0, 240520, 40.0,  0, 1, 0, 5,   10,  'Void-Etched Crystal reagent (self-ref - research data error)'),
(240520, 0, 2510,   2.5,   0, 1, 4, 1,   1,   'Cosmic Void Wyrmling mount (paragon-only) - MountID=2510'),
(240520, 0, 4610,   1.0,   0, 1, 4, 1,   1,   'Singularity Mote pet - SpeciesID=4610'),
(240520, 0, 240521, 0.5,   0, 1, 4, 1,   1,   'Cosmic Void Ashwell toy');

-- =====================================================================
-- 2704 Hara'ti - cache item 240510
-- DATA_2704 lines 1106-1154
-- NOTE: 240510 collides with 2696's reagent item id. Same caveat: research
-- data error preserved as-is per spec.
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 240510;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(240510, 0, 240800, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(240510, 0, 240515, 40.0,  0, 1, 0, 5,   10,  'Harati Verdant Sap reagent'),
(240510, 0, 2520,   2.5,   0, 1, 4, 1,   1,   'Harandar Verdant Pard mount (paragon-only) - MountID=2520'),
(240510, 0, 4620,   1.0,   0, 1, 4, 1,   1,   'Harati Sapling pet - SpeciesID=4620'),
(240510, 0, 240511, 0.5,   0, 1, 4, 1,   1,   'Fungarian Sack toy'),
(240510, 0, 240512, 0.5,   0, 1, 4, 1,   1,   'Harandar Glowvine Sconce toy');

-- =====================================================================
-- 2710 Silvermoon Court - cache item 240530
-- DATA_2710 lines 794-836
-- =====================================================================
DELETE FROM `item_loot_template` WHERE `Entry` = 240530;
INSERT INTO `item_loot_template` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(240530, 0, 240800, 20.0,  0, 1, 1, 1,   1,   'Glimmerogg Champions Cache token'),
(240530, 0, 240535, 40.0,  0, 1, 0, 5,   10,  'Sindorei Resonant Crystal reagent'),
(240530, 0, 2530,   2.5,   0, 1, 4, 1,   1,   'Blood Hawkstrider Reborn mount (paragon-only) - MountID=2530'),
(240530, 0, 4630,   1.0,   0, 1, 4, 1,   1,   'Phoenix Hatchling Reborn pet - SpeciesID=4630'),
(240530, 0, 240531, 0.5,   0, 1, 4, 1,   1,   'Saltherils Soiree Invitation toy');
