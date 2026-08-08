--
-- Pet battle unlock path: repair the two battle pet trainers whose trainer linkage is incomplete.
--
-- The unlock itself is already fully expressed in the world DB and needs no new rows:
--
--   trainer               Id 580                                            (VerifiedBuild 57689)
--   trainer_spell         580 -> 125610 "Battle Pet Training", 950 copper    (VerifiedBuild 60037)
--   gossip_menu_option    menu 14400/14991 OptionID 0, OptionNpc = 3         (VerifiedBuild 47936/60568)
--                         (3 = GossipOptionNpc::Trainer, GossipDef.h:42)
--   creature_trainer      (<creature>, 580, 14991, 0) for 22 creatures
--   spell_learn_spell     125610 -> 119467 / 125439 / 122026
--
-- Spell 125610 carries SPELL_EFFECT_ENABLE_BATTLE_PETS (effect 201), which is what
-- Spell::EffectEnableBattlePets consumes to set PLAYER_FLAGS_PET_BATTLES_UNLOCKED (0x01000000) and
-- unlock BattlePetSlot::Slot0. Client-derived, read out of the 12.0.7 client data at
-- M:/WorldofWarcraft/dbc/enUS/SpellEffect.db2 (WOWSTATIC_12_0_7_67808, layout 0x5362E3D4). Exactly
-- four spells in that table carry effect 201 - 119467, 125610, 122026 and 414142 "[DNT] TRAIN IF
-- NEEDED" - and 119467/125610 are both named "Battle Pet Training" and share icon FDID 643856:
--
--   SpellEffect row 159381: SpellID 125610, EffectIndex 0, Effect 201, ImplicitTarget[0] 1 (caster)
--   SpellEffect row 137594: SpellID 119467, EffectIndex 0, Effect 201   (the passive it teaches)
--   SpellMisc  125610: Attributes[1] 0x80020020 -> SPELL_ATTR1_CAST_WHEN_LEARNED (0x80000000),
--                      so Player::AddSpell casts it on learn and effect 201 actually fires.
--
-- What was actually broken is fixed in C++ (src/server/scripts/Pet/pet_battle_trainer.cpp): the
-- script replaced the whole gossip menu, so the Trainer option above was never sent to the client.
--
-- The two rows below are the remaining *data* gaps found while auditing every creature carrying a
-- battle pet trainer subname. Both are derived strictly from rows that already exist in this DB, no
-- invented ids.
--

--
-- 1) Creature 86056 "Zarg Bonecrunch" (Pet Battle Trainer, 1 world spawn, npcflag 1 = GOSSIP, no
--    ScriptName so it uses the plain core gossip path).
--
--    It already has creature_trainer (86056, 580, 14991, 0), i.e. the DB itself asserts that its
--    trainer option lives on gossip menu 14991 - but it has no creature_template_gossip row, so
--    Player::GetGossipMenuForSource() returns 0, PrepareGossipMenu builds an empty menu and the
--    creature_trainer lookup (86056, 0, 0) never matches. Result today: empty gossip window.
--
--    Menu id is taken from that creature's own creature_trainer row, not guessed. Compare
--    creature 87427 "Misty Webtangle", the one battle pet trainer that already works: identical
--    shape (npcflag 1, no ScriptName, trainer 580) but *with* creature_template_gossip = 14991.
--
DELETE FROM `creature_template_gossip` WHERE `CreatureID` = 86056 AND `MenuID` = 14991;
INSERT INTO `creature_template_gossip` (`CreatureID`, `MenuID`, `VerifiedBuild`) VALUES
(86056, 14991, 0);

--
-- 2) Creature 185960 "Ansel Fincap" (Battle Pet Trainer, npcflag 145 = GOSSIP|TRAINER|VENDOR,
--    creature_template_gossip 14991, ScriptName npc_pet_battle_trainer) has no creature_trainer row,
--    so its Trainer gossip option would resolve to trainer 0 and silently do nothing.
--
--    Currently unspawned, so this changes nothing live; it is added so the row is correct if/when it
--    is spawned. Values copied verbatim from the 22 sibling rows that all read (580, 14991, 0),
--    including its identically named counterpart 63073 "Ansel Fincap".
--
DELETE FROM `creature_trainer` WHERE `CreatureID` = 185960;
INSERT INTO `creature_trainer` (`CreatureID`, `TrainerID`, `MenuID`, `OptionID`) VALUES
(185960, 580, 14991, 0);

--
-- Deliberately NOT changed
-- -----------------------
-- * trainer_spell.MoneyCost for 125610 is left at the sniffed 950 copper (VerifiedBuild 60037).
--   The "10 gold" figure in circulation is web/MoP-era lore, and the client data contradicts it:
--   every pet spell on trainer 580 sniffed at build >= 56513 reads 475 where the older build-41079
--   sniffs read 500 - exactly a 0.95 factor, i.e. Player::GetReputationPriceDiscount at Friendly.
--   950 is therefore 1000 (10 silver) already discounted, and 10 gold is off by two orders of
--   magnitude. Correcting the discount artifact DB-wide is a separate concern, not this fix.
-- * quest_template "Learning the Ropes" rows (27000, 31308, 31548, ...) keep RewardSpell = 0. All 13
--   are VerifiedBuild 66384 sniffed rows; retail does not unlock pet battles from that quest, the
--   trainer purchase does. The questline is the tutorial that follows the purchase.
-- * npcflag on 64572 / 64582 is left at the sniffed 3 (no TRAINER bit). Player::PrepareGossipMenu
--   applies "no checks" to GossipOptionNpc::Trainer, so the bit is not required for the option to
--   appear, and the sniffed value is authoritative.
-- * Creatures 185959 and 251427 (npcflag 128 = VENDOR only, no GOSSIP bit, no gossip menu, no
--   spawns) are left alone - making them work would require inventing both an npcflag and a gossip
--   menu id that no sniff provides.
--
