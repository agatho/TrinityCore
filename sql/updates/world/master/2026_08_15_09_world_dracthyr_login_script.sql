-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr intro: login script in update chain; create casts = Altered Form only.
-- First-login room teleport is driven by player_dracthyr_intro_login (OnLogin); 369744 abandon uses spell_dracthyr_login.
-- Do not cast 365560 or 369728 at create — stasis comes from scene_dracthyr_evoker_intro after room movie.
-- Consolidated from the source branch (defer stasis + remove 369728 at create).

DELETE FROM `spell_script_names` WHERE `spell_id` = 369728 AND `ScriptName` = 'spell_dracthyr_login';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(369728, 'spell_dracthyr_login');

DELETE FROM `playercreateinfo_cast_spell` WHERE `raceMask` = 98304 AND `classMask` = 4096 AND `spell` IN (97709, 365560, 369728);
INSERT INTO `playercreateinfo_cast_spell` (`raceMask`, `classMask`, `createMode`, `spell`, `note`) VALUES
(98304, 4096, 0, 97709, 'Dracthyr - Evoker - Altered Form');
