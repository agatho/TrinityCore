-- DK hooks whose DB2 effect moved or disappeared in 12.0.x (logged at boot as
-- "did not match dbc effect data").

-- Obliteration: the proc aura is on the talent 281238 (EFFECT_0 SPELL_AURA_PROC_TRIGGER_SPELL ->
-- 207256, EFFECT_1 dummy 20 = the rune chance the script already reads), not on the triggered
-- 207256 whose three effects are all SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL cost modifiers.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dk_obliteration';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(281238, 'spell_dk_obliteration'); -- Obliteration (talent)

-- DELIBERATELY NOT DONE HERE: dropping the Soul Reaper / Reaper of Souls bindings.
--
-- The upstream branch this was ported from also ran
--   DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_dk_soul_reaper', 'spell_dk_reaper_of_souls');
-- and deleted both scripts, on the grounds that Soul Reaper (343294) lost its delayed execute -
-- it is now damage plus the 1241521 damage-amp debuff, with no SPELL_AURA_PERIODIC_DUMMY left to
-- tick on - and that Reaper of Souls (440002) no longer has the EFFECT_3 its launch hook needs.
-- No replacement was written for either.
--
-- A local SpellEffect.db2 read (12.0.7.67808) agrees with that description: 343294 is
-- {SCHOOL_DAMAGE, TRIGGER_SPELL->1241521, DUMMY} and 440002 has a single ADD_PCT_MODIFIER effect.
-- But 67808 is the same build their analysis used, not the 68275/68887 this realm runs, so it is
-- not independent confirmation. A script whose effect shape no longer matches merely logs
-- "did not match dbc effect data" at boot and goes inert; deleting the binding loses the behaviour
-- outright if the shape is in fact still present. The bindings therefore stay, and the two boot
-- warnings are accepted as the cheaper failure mode.
--
-- STILL OPEN: Reaper of Souls' Midnight behaviour (reset cooldown, free runes, ignore the health
-- gate) has no working implementation on either branch and needs one written against 68275/68887
-- data. The 469180 binding (spell_dk_soul_reaper_reaper_of_souls) is unaffected and works.
