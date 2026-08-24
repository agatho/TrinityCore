-- Phase 5a: bind Lesser Ghoul (237409) to npc_pet_dk_lesser_ghoul so IsSummonedBy/OnDespawn
-- can maintain SPELL_DK_LESSER_GHOUL_COUNT (1242998) stacks feeding Outnumber (1241705 -> 429).
-- See docs/midnight-assessment/class-abilities/class-abilities-phase5a-mod-summon-damage-handoff.md Task 2.
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_pet_dk_lesser_ghoul' WHERE `entry` = 237409;
