--
-- Wild battle pets: set UNIT_NPC_FLAG_WILD_BATTLE_PET, without which none of them can be engaged.
--
-- 2026_08_08_05_world.sql retyped 97 Midnight battle-pet creatures to type 14 (Wild Pet), which is
-- correct but on its own inert: the runtime gate is the NPC FLAG, not the creature type.
--
--   Unit.h:            bool IsWildBattlePet() const { return HasNpcFlag(UNIT_NPC_FLAG_WILD_BATTLE_PET); }
--   BattlePetHandler:  if (!creature || !creature->IsWildBattlePet()) -> request rejected
--   Creature.cpp:      if (IsWildBattlePet()) SelectWildBattlePetLevel();
--
-- So without the flag a wild pet is never assigned a battle level and CMSG_BATTLE_PET_REQUEST_WILD is
-- refused. Measured on the integrated realm before this file: 1217 templates are type 14, 551 of them
-- have world spawns, and *zero* templates in the whole DB carry the 0x40000000 bit - so wild pet
-- battles could never have worked, and this is not specific to the 97 just retyped.
--
-- Criterion is the branch's own, from 2026_08_08_05_world.sql: BattlePetSpecies.CreatureID membership,
-- which is exactly the type-14 set after that retype. Using `type` = 14 rather than a hardcoded entry
-- list therefore also inherits that file's deliberate exclusions - notably 255832 "Aud'rei III", left
-- at type 7 because it shares an entry with a real NPC.
--
-- Bitwise OR, so any other npcflag already set is preserved (one row carries QUESTGIVER = 2), and the
-- statement is idempotent - re-running it changes nothing.
--
UPDATE `creature_template`
   SET `npcflag` = `npcflag` | 0x40000000
 WHERE `type` = 14
   AND (`npcflag` & 0x40000000) = 0;
