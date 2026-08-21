-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: class-adaptive quest rewards (QuestPackage)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Target table: HOTFIX DB `quest_package_item` (QuestPackageItem.db2 mirror).
--   Schema (this fork, sql/base/dev/hotfixes_database.sql:7800 + HotfixDatabase.cpp:1455):
--     quest_package_item (ID, PackageID, ItemID, ItemQuantity, DisplayType, VerifiedBuild)
--     PRIMARY KEY (ID, VerifiedBuild); PackageID = smallint unsigned (MAX 65535).
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent: DELETE-by-PackageID before the INSERTs (no natural unique key beyond ID).
-- ============================================================================
--
-- ---- WHY THIS FILE EXISTS (the bug fix) ----
-- The RPE reward quests award CLASS-APPROPRIATE gear: each single quest's choice set spans
-- all 13 classes (e.g. quest 90882 = 29 distinct weapon items). That cannot fit the 6 static
-- `quest_template.RewardChoiceItemID1-6` columns, and TrinityCore's BuildQuestRewards() sends
-- those 6 columns to EVERY player unfiltered (no class filtering on RewardChoiceItem). Our
-- prior authoring (35_quest_offer_reward.sql Section 2) put ONE class's items (the Alliance
-- Shaman capture set) into those columns -> every player, every class, was offered Shaman mail.
--
-- Retail serves these via the QuestPackage mechanism, which TrinityCore implements fully with
-- NO core change: quest_template.QuestPackageID -> QuestPackageItem.db2 rows, filtered per
-- player. Server: Player::CanSelectQuestPackageItem (DisplayType=1 CLASS -> ItemSpecClassMask
-- & GetClassMask()); client: renders only rows whose DisplayType filter matches the player.
-- This file populates that package table; 30_quest_template.sql sets QuestPackageID; and
-- 35_quest_offer_reward.sql drops the static RewardChoiceItemID rows (they must NOT coexist).
-- Full analysis: .superpowers/sdd/CATCHUP_BLIZZLIKE_IMPLEMENTATION_PLAN/phk-class-rewards-report.md
--
-- ---- MINTED PackageID SCHEME (custom, fully self-contained -- no client DB2 extraction) ----
-- Real retail QuestPackageIDs are unknown (not captured; not in our db2_cache dump; TCHarvest
-- dropped the column), so we MINT custom package IDs and push them to the client via
-- VerifiedBuild=0 (a 0-build hotfix row is client-applied for display). Scheme:
--     PackageID = 64000 + (questID mod 1000)   -> 64882/64885/64886/64887/64888/64893/64895/64896
-- All <= 64896 < 65535 (PackageID is smallint unsigned). The 64xxx range sits far above any
-- real retail QuestPackageID (retail package space is well under ~10000), so mint collision is
-- implausible; and this fork's base hotfix DB ships ZERO quest_package_item rows (verified:
-- no INSERT INTO quest_package_item in hotfixes_database.sql), so there is nothing to collide
-- with locally. Row ID scheme (must be unique per VerifiedBuild):
--     ID = PackageID*100 + seq   (seq = 1..N within the package; N max 29 << 100)
--   -> 6488201.. / 6488501.. / ... (int unsigned; globally unique).
--
-- ---- DisplayType = 1 (CLASS) on every row ----
-- TC's CanSelectQuestPackageItem then filters each row by the ITEM's own class-spec mask
-- (ItemSpecClassMask & player class mask). So we author EVERY class's item for the quest into
-- one package and let the core show each player only their class's family. ItemQuantity=1.
--
-- ---- DATA SOURCING & VERIFICATION ----
-- Per-quest per-class item ids read from each quest's Wowhead reward-choice list
-- (wowhead.com/quest=<id>), which tags every item by its set-family name. Family->class map
-- (phk report Section 3.1): Oathsworn=Warrior, Sunsoul=Paladin, Heart-Lesion=DeathKnight,
-- Trailseeker=Hunter, Streamtalker=Shaman, Blue/Cobalt Winglord's=Evoker, Lightdrinker=Rogue,
-- Mistdancer=Monk, Springrain=Druid, Illidari=DemonHunter, Communal=Priest,
-- Mountainsage=Mage, Felsoul=Warlock. Shaman(Streamtalker) + Druid(Springrain) ids are
-- CAPTURE-CONFIRMED (Alliance + Horde play-session captures) and match Wowhead exactly.
-- ALL 211 item ids below were verified present in ItemSparse.12.0.7.68275.csv (0 missing).
-- Family->class spot-verified against ItemSparse.AllowableClass: 154025 Oathsworn=1(Warrior),
-- 153889 Sunsoul=2(Paladin), 153726 Heart-Lesion=32(DK), 194522 Blue Winglord's=4096(Evoker),
-- 153934 Communal=16(Priest), 153830 Mountainsage=128(Mage), 154024 Felsoul=256(Warlock).
--
-- ---- KNOWN CAVEATS / TODO-VARIANT ----
-- * 90882 Hunter: Wowhead's live page lists Trailseeker Spear 153814 + Longbow 231839; we ALSO
--   include Trailseeker Shotgun 153813 (present in ItemSparse and cited in phk report Section 3.3)
--   as a plausible Trailseeker weapon variant. TODO variant: confirm 153813 belongs on 90882.
-- * 90887 Wowhead lists BACK cloaks (capture: 153793/153998) AND chest pieces per plate/leather
--   family. Capture only surfaced the cloak for the captured toon; the package legitimately
--   spans back + chest, so all are included (each class still sees only its own family). TODO
--   variant: a loot-spec (DisplayType=0) split may narrow which piece a given spec is offered --
--   not modelled here (task specifies DisplayType=1 CLASS).
-- * 90888 DemonHunter trinkets = Demon Trophy 154743 / Charm of Demonic Fire 154744 (Illidari
--   trinket equivalents); Evoker = Claw-Carved Figurine 194531 / Blue Winglord's Insignia 194532.
-- * Illidari (DH) items carry ItemSparse.AllowableClass=-1 (all classes) but their ItemSpec
--   class mask restricts to DH, which is what CanSelectQuestPackageItem's CLASS filter uses --
--   so they still surface only to Demon Hunters. Flagged for Phase-K in-game confirmation.
-- ============================================================================

DELETE FROM `quest_package_item` WHERE `PackageID` IN (64882, 64885, 64886, 64887, 64888, 64893, 64895, 64896);

-- ---- Quest 90882 -> PackageID 64882 :: Weapons (1H/2H/shield) (29 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488201, 64882, 154025, 1, 1, 0),  -- Warrior / Oathsworn
 (6488202, 64882, 154035, 1, 1, 0),  -- Warrior / Oathsworn
 (6488203, 64882, 154036, 1, 1, 0),  -- Warrior / Oathsworn
 (6488204, 64882, 153889, 1, 1, 0),  -- Paladin / Sunsoul
 (6488205, 64882, 153891, 1, 1, 0),  -- Paladin / Sunsoul
 (6488206, 64882, 153892, 1, 1, 0),  -- Paladin / Sunsoul
 (6488207, 64882, 153893, 1, 1, 0),  -- Paladin / Sunsoul
 (6488208, 64882, 153726, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488209, 64882, 153747, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488210, 64882, 153814, 1, 1, 0),  -- Hunter / Trailseeker
 (6488211, 64882, 153813, 1, 1, 0),  -- Hunter / Trailseeker (TODO variant: Shotgun, not on live 90882 page)
 (6488212, 64882, 231839, 1, 1, 0),  -- Hunter / Trailseeker
 (6488213, 64882, 153959, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488214, 64882, 153960, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488215, 64882, 153961, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488216, 64882, 153973, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488217, 64882, 153983, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488218, 64882, 154005, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488219, 64882, 153830, 1, 1, 0),  -- Mage / Mountainsage
 (6488220, 64882, 154024, 1, 1, 0),  -- Warlock / Felsoul
 (6488221, 64882, 153934, 1, 1, 0),  -- Priest / Communal
 (6488222, 64882, 153944, 1, 1, 0),  -- Priest / Communal
 (6488223, 64882, 153835, 1, 1, 0),  -- Monk / Mistdancer
 (6488224, 64882, 153856, 1, 1, 0),  -- Monk / Mistdancer
 (6488225, 64882, 153859, 1, 1, 0),  -- Monk / Mistdancer
 (6488226, 64882, 153773, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488227, 64882, 153792, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488228, 64882, 160513, 1, 1, 0),  -- DemonHunter / Illidari
 (6488229, 64882, 194522, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90885 -> PackageID 64885 :: Rings x2 (Finger) (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488501, 64885, 153741, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488502, 64885, 153742, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488503, 64885, 153796, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488504, 64885, 153797, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488505, 64885, 153802, 1, 1, 0),  -- Hunter / Trailseeker
 (6488506, 64885, 153803, 1, 1, 0),  -- Hunter / Trailseeker
 (6488507, 64885, 153817, 1, 1, 0),  -- Mage / Mountainsage
 (6488508, 64885, 153818, 1, 1, 0),  -- Mage / Mountainsage
 (6488509, 64885, 153862, 1, 1, 0),  -- Monk / Mistdancer
 (6488510, 64885, 153863, 1, 1, 0),  -- Monk / Mistdancer
 (6488511, 64885, 153908, 1, 1, 0),  -- Paladin / Sunsoul
 (6488512, 64885, 153909, 1, 1, 0),  -- Paladin / Sunsoul
 (6488513, 64885, 153927, 1, 1, 0),  -- Priest / Communal
 (6488514, 64885, 153928, 1, 1, 0),  -- Priest / Communal
 (6488515, 64885, 153948, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488516, 64885, 153949, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488517, 64885, 153995, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488518, 64885, 153996, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488519, 64885, 154011, 1, 1, 0),  -- Warlock / Felsoul
 (6488520, 64885, 154012, 1, 1, 0),  -- Warlock / Felsoul
 (6488521, 64885, 154114, 1, 1, 0),  -- Warrior / Oathsworn
 (6488522, 64885, 154115, 1, 1, 0),  -- Warrior / Oathsworn
 (6488523, 64885, 154745, 1, 1, 0),  -- DemonHunter / Illidari
 (6488524, 64885, 154746, 1, 1, 0),  -- DemonHunter / Illidari
 (6488525, 64885, 194533, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6488526, 64885, 194534, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90886 -> PackageID 64886 :: Feet + Hands (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488601, 64886, 153735, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488602, 64886, 153736, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488603, 64886, 153785, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488604, 64886, 153786, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488605, 64886, 153806, 1, 1, 0),  -- Hunter / Trailseeker
 (6488606, 64886, 153807, 1, 1, 0),  -- Hunter / Trailseeker
 (6488607, 64886, 153820, 1, 1, 0),  -- Mage / Mountainsage
 (6488608, 64886, 153821, 1, 1, 0),  -- Mage / Mountainsage
 (6488609, 64886, 153845, 1, 1, 0),  -- Monk / Mistdancer
 (6488610, 64886, 153846, 1, 1, 0),  -- Monk / Mistdancer
 (6488611, 64886, 153902, 1, 1, 0),  -- Paladin / Sunsoul
 (6488612, 64886, 153903, 1, 1, 0),  -- Paladin / Sunsoul
 (6488613, 64886, 153936, 1, 1, 0),  -- Priest / Communal
 (6488614, 64886, 153937, 1, 1, 0),  -- Priest / Communal
 (6488615, 64886, 153952, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488616, 64886, 153953, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488617, 64886, 154001, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488618, 64886, 154002, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488619, 64886, 154014, 1, 1, 0),  -- Warlock / Felsoul
 (6488620, 64886, 154015, 1, 1, 0),  -- Warlock / Felsoul
 (6488621, 64886, 154039, 1, 1, 0),  -- Warrior / Oathsworn
 (6488622, 64886, 154040, 1, 1, 0),  -- Warrior / Oathsworn
 (6488623, 64886, 154738, 1, 1, 0),  -- DemonHunter / Illidari
 (6488624, 64886, 154741, 1, 1, 0),  -- DemonHunter / Illidari
 (6488625, 64886, 194524, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6488626, 64886, 194527, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90887 -> PackageID 64887 :: Back (+ chest variants per package) (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488701, 64887, 153718, 1, 1, 0),  -- DeathKnight / Heart-Lesion (chest)
 (6488702, 64887, 153733, 1, 1, 0),  -- DeathKnight / Heart-Lesion (chest)
 (6488703, 64887, 153734, 1, 1, 0),  -- DeathKnight / Heart-Lesion (cloak)
 (6488704, 64887, 153793, 1, 1, 0),  -- Druid / Springrain (cloak, capture-confirmed)
 (6488705, 64887, 153799, 1, 1, 0),  -- Hunter / Trailseeker (cloak)
 (6488706, 64887, 153805, 1, 1, 0),  -- Hunter / Trailseeker (chest)
 (6488707, 64887, 153829, 1, 1, 0),  -- Mage / Mountainsage (cloak)
 (6488708, 64887, 153837, 1, 1, 0),  -- Monk / Mistdancer (chest)
 (6488709, 64887, 153865, 1, 1, 0),  -- Monk / Mistdancer (cloak)
 (6488710, 64887, 153866, 1, 1, 0),  -- Monk / Mistdancer (chest)
 (6488711, 64887, 153867, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488712, 64887, 153875, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488713, 64887, 153900, 1, 1, 0),  -- Paladin / Sunsoul (chest)
 (6488714, 64887, 153901, 1, 1, 0),  -- Paladin / Sunsoul (cloak)
 (6488715, 64887, 153935, 1, 1, 0),  -- Priest / Communal (cloak)
 (6488716, 64887, 153945, 1, 1, 0),  -- Rogue / Lightdrinker (cloak)
 (6488717, 64887, 153951, 1, 1, 0),  -- Rogue / Lightdrinker (chest)
 (6488718, 64887, 153998, 1, 1, 0),  -- Shaman / Streamtalker (cloak, capture-confirmed)
 (6488719, 64887, 154023, 1, 1, 0),  -- Warlock / Felsoul (cloak)
 (6488720, 64887, 154026, 1, 1, 0),  -- Warrior / Oathsworn (chest)
 (6488721, 64887, 154037, 1, 1, 0),  -- Warrior / Oathsworn (chest)
 (6488722, 64887, 154119, 1, 1, 0),  -- Warrior / Oathsworn (cloak)
 (6488723, 64887, 154739, 1, 1, 0),  -- DemonHunter / Illidari (robe)
 (6488724, 64887, 154748, 1, 1, 0),  -- DemonHunter / Illidari (drape)
 (6488725, 64887, 194526, 1, 1, 0),  -- Evoker / Blue Winglord's (chest)
 (6488726, 64887, 194535, 1, 1, 0);  -- Evoker / Cobalt Winglord's (cloak)

-- ---- Quest 90888 -> PackageID 64888 :: Trinkets x2 (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6488801, 64888, 153740, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488802, 64888, 153743, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6488803, 64888, 153795, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488804, 64888, 153798, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6488805, 64888, 153801, 1, 1, 0),  -- Hunter / Trailseeker
 (6488806, 64888, 153804, 1, 1, 0),  -- Hunter / Trailseeker
 (6488807, 64888, 153816, 1, 1, 0),  -- Mage / Mountainsage
 (6488808, 64888, 153819, 1, 1, 0),  -- Mage / Mountainsage
 (6488809, 64888, 153860, 1, 1, 0),  -- Monk / Mistdancer
 (6488810, 64888, 153864, 1, 1, 0),  -- Monk / Mistdancer
 (6488811, 64888, 153907, 1, 1, 0),  -- Paladin / Sunsoul
 (6488812, 64888, 153910, 1, 1, 0),  -- Paladin / Sunsoul
 (6488813, 64888, 153926, 1, 1, 0),  -- Priest / Communal
 (6488814, 64888, 153930, 1, 1, 0),  -- Priest / Communal
 (6488815, 64888, 153947, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488816, 64888, 153950, 1, 1, 0),  -- Rogue / Lightdrinker
 (6488817, 64888, 153994, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488818, 64888, 153997, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6488819, 64888, 154010, 1, 1, 0),  -- Warlock / Felsoul
 (6488820, 64888, 154013, 1, 1, 0),  -- Warlock / Felsoul
 (6488821, 64888, 154116, 1, 1, 0),  -- Warrior / Oathsworn
 (6488822, 64888, 154117, 1, 1, 0),  -- Warrior / Oathsworn
 (6488823, 64888, 154743, 1, 1, 0),  -- DemonHunter / Illidari (Demon Trophy)
 (6488824, 64888, 154744, 1, 1, 0),  -- DemonHunter / Illidari (Charm of Demonic Fire)
 (6488825, 64888, 194531, 1, 1, 0),  -- Evoker / Claw-Carved Figurine
 (6488826, 64888, 194532, 1, 1, 0);  -- Evoker / Blue Winglord's Insignia

-- ---- Quest 90893 -> PackageID 64893 :: Waist + Wrist (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489301, 64893, 153745, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489302, 64893, 153746, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489303, 64893, 153790, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489304, 64893, 153791, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489305, 64893, 153811, 1, 1, 0),  -- Hunter / Trailseeker
 (6489306, 64893, 153812, 1, 1, 0),  -- Hunter / Trailseeker
 (6489307, 64893, 153826, 1, 1, 0),  -- Mage / Mountainsage
 (6489308, 64893, 153827, 1, 1, 0),  -- Mage / Mountainsage
 (6489309, 64893, 153857, 1, 1, 0),  -- Monk / Mistdancer
 (6489310, 64893, 153858, 1, 1, 0),  -- Monk / Mistdancer
 (6489311, 64893, 153912, 1, 1, 0),  -- Paladin / Sunsoul
 (6489312, 64893, 153913, 1, 1, 0),  -- Paladin / Sunsoul
 (6489313, 64893, 153942, 1, 1, 0),  -- Priest / Communal
 (6489314, 64893, 153943, 1, 1, 0),  -- Priest / Communal
 (6489315, 64893, 153957, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489316, 64893, 153958, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489317, 64893, 154007, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489318, 64893, 154008, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489319, 64893, 154020, 1, 1, 0),  -- Warlock / Felsoul
 (6489320, 64893, 154021, 1, 1, 0),  -- Warlock / Felsoul
 (6489321, 64893, 154049, 1, 1, 0),  -- Warrior / Oathsworn
 (6489322, 64893, 154050, 1, 1, 0),  -- Warrior / Oathsworn
 (6489323, 64893, 154740, 1, 1, 0),  -- DemonHunter / Illidari
 (6489324, 64893, 154742, 1, 1, 0),  -- DemonHunter / Illidari
 (6489325, 64893, 194523, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489326, 64893, 194525, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90895 -> PackageID 64895 :: Legs + Neck (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489501, 64895, 153738, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489502, 64895, 153739, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489503, 64895, 153788, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489504, 64895, 153794, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489505, 64895, 153800, 1, 1, 0),  -- Hunter / Trailseeker
 (6489506, 64895, 153809, 1, 1, 0),  -- Hunter / Trailseeker
 (6489507, 64895, 153815, 1, 1, 0),  -- Mage / Mountainsage
 (6489508, 64895, 153823, 1, 1, 0),  -- Mage / Mountainsage
 (6489509, 64895, 153850, 1, 1, 0),  -- Monk / Mistdancer
 (6489510, 64895, 153861, 1, 1, 0),  -- Monk / Mistdancer
 (6489511, 64895, 153905, 1, 1, 0),  -- Paladin / Sunsoul
 (6489512, 64895, 153906, 1, 1, 0),  -- Paladin / Sunsoul
 (6489513, 64895, 153925, 1, 1, 0),  -- Priest / Communal
 (6489514, 64895, 153939, 1, 1, 0),  -- Priest / Communal
 (6489515, 64895, 153946, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489516, 64895, 153955, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489517, 64895, 153993, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489518, 64895, 154004, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489519, 64895, 154009, 1, 1, 0),  -- Warlock / Felsoul
 (6489520, 64895, 154017, 1, 1, 0),  -- Warlock / Felsoul
 (6489521, 64895, 154042, 1, 1, 0),  -- Warrior / Oathsworn
 (6489522, 64895, 154118, 1, 1, 0),  -- Warrior / Oathsworn
 (6489523, 64895, 154736, 1, 1, 0),  -- DemonHunter / Illidari
 (6489524, 64895, 154747, 1, 1, 0),  -- DemonHunter / Illidari
 (6489525, 64895, 194529, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489526, 64895, 194536, 1, 1, 0);  -- Evoker / Blue Winglord's

-- ---- Quest 90896 -> PackageID 64896 :: Head + Shoulder (26 rows) ----
INSERT INTO `quest_package_item` (`ID`, `PackageID`, `ItemID`, `ItemQuantity`, `DisplayType`, `VerifiedBuild`) VALUES
 (6489601, 64896, 153737, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489602, 64896, 153744, 1, 1, 0),  -- DeathKnight / Heart-Lesion
 (6489603, 64896, 153787, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489604, 64896, 153789, 1, 1, 0),  -- Druid / Springrain (capture-confirmed)
 (6489605, 64896, 153808, 1, 1, 0),  -- Hunter / Trailseeker
 (6489606, 64896, 153810, 1, 1, 0),  -- Hunter / Trailseeker
 (6489607, 64896, 153822, 1, 1, 0),  -- Mage / Mountainsage
 (6489608, 64896, 153825, 1, 1, 0),  -- Mage / Mountainsage
 (6489609, 64896, 153842, 1, 1, 0),  -- Monk / Mistdancer
 (6489610, 64896, 153847, 1, 1, 0),  -- Monk / Mistdancer
 (6489611, 64896, 153855, 1, 1, 0),  -- Monk / Mistdancer
 (6489612, 64896, 153904, 1, 1, 0),  -- Paladin / Sunsoul
 (6489613, 64896, 153911, 1, 1, 0),  -- Paladin / Sunsoul
 (6489614, 64896, 153938, 1, 1, 0),  -- Priest / Communal
 (6489615, 64896, 153941, 1, 1, 0),  -- Priest / Communal
 (6489616, 64896, 153954, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489617, 64896, 153956, 1, 1, 0),  -- Rogue / Lightdrinker
 (6489618, 64896, 154003, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489619, 64896, 154006, 1, 1, 0),  -- Shaman / Streamtalker (capture-confirmed)
 (6489620, 64896, 154016, 1, 1, 0),  -- Warlock / Felsoul
 (6489621, 64896, 154041, 1, 1, 0),  -- Warrior / Oathsworn
 (6489622, 64896, 154048, 1, 1, 0),  -- Warrior / Oathsworn
 (6489623, 64896, 154735, 1, 1, 0),  -- DemonHunter / Illidari
 (6489624, 64896, 154737, 1, 1, 0),  -- DemonHunter / Illidari
 (6489625, 64896, 194528, 1, 1, 0),  -- Evoker / Blue Winglord's
 (6489626, 64896, 194530, 1, 1, 0);  -- Evoker / Blue Winglord's
