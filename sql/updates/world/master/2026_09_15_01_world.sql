--
-- Fix corrupt spell_proc for Vampiric Embrace (15286).
--
-- The WCDB base shipped a broken proc definition for Vampiric Embrace:
--   SchoolMask 0, SpellFamilyName 0, ProcFlags 2424832, SpellTypeMask 0,
--   SpellPhaseMask 1 (PROC_SPELL_PHASE_CAST).
-- With SpellPhaseMask = CAST the aura procs on a spell cast, where the proc
-- event carries no DamageInfo. spell_pri_vampiric_embrace::CheckProc then
-- dereferences eventInfo.GetDamageInfo() (a null) and segfaults the whole
-- worldserver. It was hit live 2026-09-15 by a Priest bot casting a non-damage
-- spell (528, Cure Disease); gdb pinned the crash to spell_priest.cpp CheckProc.
--
-- Restore the canonical TrinityCore values (TDB 1210.26091, the 12.1.0.69814
-- release this branch is merged to): the aura procs on Shadow (SchoolMask 32),
-- Priest family (6), spell DAMAGE (SpellTypeMask 1) on HIT (SpellPhaseMask 2),
-- which is the event shape the script's damage-leech logic requires.
--
-- The script is also hardened to null-check GetDamageInfo() regardless, but this
-- row is the actual data bug and is corrected from the authoritative TDB, not
-- hand-authored.

DELETE FROM `spell_proc` WHERE `SpellId` = 15286;
INSERT INTO `spell_proc`
  (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
  (15286, 32, 6, 0, 0, 0, 0, 0, 0, 1, 2, 0, 2, 0, 0, 0, 0, 0);
