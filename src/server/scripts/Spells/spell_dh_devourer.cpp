/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Scripts for the Demon Hunter "Devourer" specialization (ChrSpecialization 1480, Midnight 12.0.7).
 * Scriptnames of files in this file should be prefixed with "spell_dh_".
 *
 * NOTE: The bulk of the Devourer kit (Consume, Reap, Eradicate, Shift, Shattered Souls,
 * Voidblade, Voidglare Boon, Void Ray, soul-fragment plumbing) is ALREADY implemented in
 * spell_dh.cpp on the baseline. This file collects only the Devourer NEEDS-SCRIPT abilities
 * that are NOT yet handled there. All ids below are verified against DB2 build 12.0.7.68887
 * (Spell / SpellName / SpellMisc / SpellEffect). These are WIP stubs (registered, compiling,
 * realm-safe no-ops) pending full implementation — see C:\dumps\DEVOURER_SPEC_BLUEPRINT.md.
 */

#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"

enum DevourerSpells
{
    // Void Metamorphosis activation (overrides 191427 Metamorphosis). Effects: TRIGGER 1217607
    // (transform buff, applied via data), TRIGGER 201453, DUMMY. The Fury-drain sustain and the
    // "Consume/Reap enhanced while active" behaviour are NOT data-driven -> NEEDS-SCRIPT. [DB2 68887]
    SPELL_DH_VOID_METAMORPHOSIS                = 1217605,
    SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM      = 1217607,

    // Void Nova (trait, tree 854). Effects: AURA + SCHOOL_DAMAGE + DUMMY -> NEEDS-SCRIPT. [DB2 68887]
    SPELL_DH_VOID_NOVA                         = 1234195,
};

// 1217605 - Void Metamorphosis (activation)
// TODO(devourer): on cast, gate on required Soul Fragments; the 1217607 transform buff is applied
// by the data-driven TRIGGER effect. This script owns the enhanced-ability state + the sustain seam.
class spell_dh_void_metamorphosis : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS, SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM });
    }

    void HandleActivate() const
    {
        // TODO(devourer): consume required Soul Fragments; kick off the Fury-drain sustain aura.
        // Realm-safe no-op until implemented.
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_void_metamorphosis::HandleActivate);
    }
};

// 1217607 - Void Metamorphosis (transform buff): drives the per-tick Fury drain that ends the form
// when Fury is exhausted (Grim Focus 1260008 slows the rate out of combat / while CC'd).
// TODO(devourer): implement the periodic Fury drain + auto-cancel. Stubbed no-op for now.
class spell_dh_void_metamorphosis_drain : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        // TODO(devourer): drain Fury each tick (rate from the SPELL_AURA_DUMMY effects on this aura);
        // if Fury <= 0 remove the transform. The "Consume/Reap enhanced while active" behaviour is
        // already data-driven via SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS on this same aura (1217607).
    }

    void Register() override
    {
        // Periodic dummy tick is EFFECT_6 (SPELL_AURA_PERIODIC_DUMMY) on 1217607 [DB2 68887].
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_metamorphosis_drain::HandlePeriodic, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1234195 - Void Nova
// TODO(devourer): AoE cosmic damage + the DUMMY effect (knockback / debuff) at the caster.
class spell_dh_void_nova : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_NOVA });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        // TODO(devourer): apply the Void Nova secondary effect. Realm-safe no-op for now.
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_void_nova::HandleDummy, EFFECT_2, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_demon_hunter_devourer_spell_scripts()
{
    RegisterSpellScript(spell_dh_void_metamorphosis);
    RegisterSpellScript(spell_dh_void_metamorphosis_drain);
    RegisterSpellScript(spell_dh_void_nova);
}
