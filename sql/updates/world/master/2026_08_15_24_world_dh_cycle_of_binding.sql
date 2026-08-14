-- Cycle of Binding (389718): Midnight redesign replaced the old DUMMY OnEffectProc
-- (flat ModifyCooldown on each sigil spell id) with five core
-- SPELL_AURA_CHARGE_RECOVERY_MULTIPLIER (-15) effects keyed to sigil ChargeCategories
-- 1605/1607/1608/1606/1887 (Flame/Misery/Silence/Chains/Spite). Unbind the obsolete script.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dh_cycle_of_binding';
