-- Demon Hunter baseline triage (EVR-47 slice #8): Chaos Brand bind.
-- Build evidence: 12.0.7.67808 EvryDb2Export temp/db2/12.0.7.67808/SpellEffect-pack-8-spells.csv
-- + SpellAuraOptions-baseline.csv.
-- Prune-with-cite (no binds): 212611 Demon Hunter informational (Evoker 353167);
-- 196055 Double Jump core-covered (Unit.h SPELL_DH_DOUBLE_JUMP + Player.cpp ENABLE +
-- SpellAuraEffects HandleModAdvFlying); 320364 IA passive gates existing
-- spell_dh_immolation_aura_initial_burst on 258920 (no new ScriptName).

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dh_chaos_brand';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(255260, 'spell_dh_chaos_brand');
