-- Monk Roll (107427 / 109131): aura 373 / aura 305 amounts, moved out of spell_monk.cpp.
-- Ported from evry/master-track/fel-rush (280a5d99bb).
--
-- MUST be applied together with the spell_monk.cpp change in the same branch: the script used to
-- add a flat +100 to EFFECT_0 (SPELL_AURA_MOD_SPEED_NO_CONTROL) and EFFECT_2
-- (SPELL_AURA_MOD_MINIMUM_SPEED) at aura-amount calculation time. That workaround is gone now that
-- the core handles aura 373, so without these rows Roll loses ~100 of its speed amount.
--
-- UNVERIFIED: the base points (175 -> 275 and 275 -> 375) and the per-level slope come from the
-- source branch's capture, not from a check against our own client data. The source keyed the
-- updates on hotfix record ids (118195, 120480, 157593, 157592); they are keyed on SpellID +
-- EffectAura here so they still hit if our DB2 record ids differ.
UPDATE `spell_effect` SET `EffectBasePoints`=275, `EffectRealPointsPerLevel`=0.08551282051282051 WHERE `SpellID` IN (107427, 109131) AND `EffectAura`=373;
UPDATE `spell_effect` SET `EffectBasePoints`=375, `EffectRealPointsPerLevel`=0.08551282051282051 WHERE `SpellID` IN (107427, 109131) AND `EffectAura`=305;
