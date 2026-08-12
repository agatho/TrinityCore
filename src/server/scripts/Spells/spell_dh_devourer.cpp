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
// Effects [DB2 12.0.7]: EFFECT_0 TRIGGER 1217607 (transform buff, data-driven), EFFECT_1 TRIGGER
// 201453 (shared Metamorphosis movement helper, data-driven), EFFECT_2 DUMMY (bp 2) - the seam a
// script would hook for an on-activation Soul-Fragment gate.
// TODO(RESEARCH): the EFFECT_2 dummy carries bp = 2, but the client tooltip / a combat capture has
// NOT confirmed whether that is a Soul-Fragment cost (and if so, whether 2 is the required count) or
// an unrelated value. Per the blueprint's evidence-vs-invention rule the fragment count is NOT
// guessed here; the existing DH Soul-Fragment consume path (spell_dh.cpp) would be wired in once the
// requirement is source-confirmed. Realm-safe no-op until then.
class spell_dh_void_metamorphosis : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_METAMORPHOSIS, SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM });
    }

    void HandleActivate() const
    {
        // TODO(RESEARCH): consume the required Soul Fragments once the EFFECT_2 (bp 2) requirement is
        // confirmed. No-op today; the transform buff 1217607 is applied by the data-driven TRIGGER.
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_void_metamorphosis::HandleActivate);
    }
};

// 1217607 - Void Metamorphosis (transform buff): drives the per-tick Fury drain that ends the form
// when Fury is exhausted. Enhanced Consume/Reap (SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS -> 1217610 /
// 1245453) and the empower package on this same aura are all data-driven; the ONLY custom piece is
// this sustain drain.
//
// Effect layout on 1217607 [DB2 12.0.7.68275, blueprint-confirmed]:
//   EFFECT_6  = SPELL_AURA_PERIODIC_DUMMY (aura 226), period 1000 ms  -> the drain tick cadence
//   EFFECT_10 = SPELL_AURA_DUMMY (aura 4), base points 25             -> the Fury drained per tick
//   EFFECT_13 = SPELL_AURA_DUMMY (aura 4), base points 1000           -> unresolved secondary dummy
// The per-tick amount is read LIVE from the EFFECT_10 dummy (GetAmount) so it tracks DB2/hotfix data
// rather than a hardcoded constant. NOTE(RESEARCH): the exact per-tick value and the 1-second cadence
// are provisional pending a Devourer combat capture (no Devourer sniff exists yet); if a capture shows
// a different rate, only the DB2 rows change - this script needs no edit. The EFFECT_13 (bp 1000)
// dummy's role is not source-confirmed and is deliberately NOT used here.
class spell_dh_void_metamorphosis_drain : public AuraScript
{
    // Index of the SPELL_AURA_DUMMY effect on 1217607 that carries the per-tick Fury drain amount.
    static constexpr SpellEffIndex EFFECT_FURY_DRAIN_AMOUNT = EFFECT_10;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM, EFFECT_6 },
                                     { SPELL_DH_VOID_METAMORPHOSIS_TRANSFORM, EFFECT_FURY_DRAIN_AMOUNT } });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();

        // Drain amount is data-driven: read it from the aura's own SPELL_AURA_DUMMY effect.
        AuraEffect const* drainEffect = GetEffect(EFFECT_FURY_DRAIN_AMOUNT);
        int32 const drain = drainEffect ? drainEffect->GetAmount() : 0;
        if (drain <= 0)
            return;

        // Already out of Fury -> cannot sustain the void form; end it (mirrors how other sustained
        // transforms are cancelled - remove the transform buff this aura lives on).
        if (target->GetPower(POWER_FURY) <= 0)
        {
            Remove();
            return;
        }

        // ModifyPower clamps at 0, so a partial-affordability tick still drains to empty.
        target->ModifyPower(POWER_FURY, -drain);

        if (target->GetPower(POWER_FURY) <= 0)
            Remove();
    }

    void Register() override
    {
        // Periodic dummy tick is EFFECT_6 (SPELL_AURA_PERIODIC_DUMMY) on 1217607 [DB2 68887].
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_metamorphosis_drain::HandlePeriodic, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1234195 - Void Nova
// Effects [DB2 12.0.7]: EFFECT_0 APPLY_AURA (aura 12 SPELL_AURA_MOD_STUN, data-driven), EFFECT_1
// SCHOOL_DAMAGE (the AoE cosmic damage, data-driven), EFFECT_2 DUMMY (bp 30) - the secondary seam.
// Both the damage and the stun already resolve from data, so no script is required for the confirmed
// behaviour. Only the EFFECT_2 dummy needs custom code, and its mechanic (knockback vs. slow vs.
// soul-scatter) is RESEARCH-flagged in the blueprint and NOT decodable from bp = 30 alone.
// TODO(RESEARCH): confirm the EFFECT_2 secondary effect from an enhanced tooltip or a combat capture
// before implementing. Registered no-op keeps the binding realm-safe; the mechanic is not guessed.
class spell_dh_void_nova : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_NOVA });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        // TODO(RESEARCH): apply the Void Nova secondary effect (EFFECT_2 DUMMY bp 30) once the exact
        // mechanic is source-confirmed. Realm-safe no-op for now.
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
