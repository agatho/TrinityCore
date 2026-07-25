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
 * Scripts for dragonriding / skyriding abilities.
 * Scriptnames prefixed with "spell_dragonriding_"
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "Unit.h"

enum DragonridingSpells
{
    SPELL_DRAGONRIDING_SURGE_FORWARD        = 372608,
    SPELL_DRAGONRIDING_SKYWARD_ASCENT       = 372610,
    SPELL_DRAGONRIDING_WHIRLING_SURGE       = 361584,
    SPELL_DRAGONRIDING_LAUNCH_BOOST         = 392752,
    SPELL_DRAGONRIDING_LIFT_OFF             = 374763,
    SPELL_DRAGONRIDING_FLIGHT_STYLE_STEADY  = 404468,
};

// Blizzlike impulse values from sniff data (12.0.1.66709 dragonriding_midnight, 2026-03-31):
// Launch Boost:    (0, 0, 45.0) on hit + 5.0 facing+pitch ticks @ 100ms over 2s aura — counter 29 base, 30..34 ticks
// Whirling Surge:  magnitude 60.0 facing+pitch, SINGLE impulse on hit (NOT periodic) — counter 35
// Skyward Ascent:  horizontal 12.25 + Z 49.0 — magnitude ~50.51 — counter 37 (12.185, 1.264, 49.000) ✓
// Surge Forward:   magnitude 18.0 facing+pitch — counter 36 (-7.642, 16.243, 1.335) and counter 44 (14.779, 10.077, -2.010)
//                  counter 43 observed at 36.0 magnitude — likely talent-buffed; 18.0 is the untalented base.

static void SendFacingImpulse(Unit* caster, float speed)
{
    float orientation = caster->GetOrientation();
    float pitch = caster->m_movementInfo.pitch;
    float cosPitch = std::cos(pitch);
    Position direction(
        std::cos(orientation) * cosPitch * speed,
        std::sin(orientation) * cosPitch * speed,
        std::sin(pitch) * speed
    );
    caster->SendAddImpulse(direction);
}

static SpellCastResult CheckSkyriding(SpellScript* script)
{
    Unit* caster = script->GetCaster();
    if (!caster->HasExtraUnitMovementFlag2(MOVEMENTFLAG3_CAN_ADV_FLY))
        return SPELL_FAILED_DRAGONRIDING_RIDING_REQUIREMENT;
    return SPELL_CAST_OK;
}

// Refresh the vigor bar right after a charge was consumed instead of waiting for the next regen tick.
static void UpdateVigorAfterCast(SpellScript* script)
{
    if (Unit* caster = script->GetCaster())
        if (Player* player = caster->ToPlayer())
            player->UpdateVigor();
}

// 372608 - Surge Forward
class spell_dragonriding_surge_forward : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return CheckSkyriding(this);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
            SendFacingImpulse(caster, 18.0f);
    }

    void RefreshVigor()
    {
        UpdateVigorAfterCast(this);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_surge_forward::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_surge_forward::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_dragonriding_surge_forward::RefreshVigor);
    }
};

// 372610 - Skyward Ascent
class spell_dragonriding_skyward_ascent : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return CheckSkyriding(this);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            float orientation = caster->GetOrientation();
            float horizontalSpeed = 12.25f;
            Position direction(
                std::cos(orientation) * horizontalSpeed,
                std::sin(orientation) * horizontalSpeed,
                49.0f
            );
            caster->SendAddImpulse(direction);
        }
    }

    void RefreshVigor()
    {
        UpdateVigorAfterCast(this);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_skyward_ascent::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_skyward_ascent::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_dragonriding_skyward_ascent::RefreshVigor);
    }
};

// 361584 - Whirling Surge
// Sniff (counter 35, t=66762ms): single impulse vec=(-17.083, 57.364, -4.187) mag=60.0 — facing+pitch, NOT periodic.
// The 3s SPELL_AURA_DUMMY remains client-side (visual/animation); the magnitude-60 push is delivered once on hit.
class spell_dragonriding_whirling_surge : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return CheckSkyriding(this);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
            SendFacingImpulse(caster, 60.0f);
    }

    void RefreshVigor()
    {
        UpdateVigorAfterCast(this);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_whirling_surge::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_whirling_surge::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_dragonriding_whirling_surge::RefreshVigor);
    }
};

// 374763 - Lift Off (the double-jump takeoff, cast by the server via SpellKeyboundOverride 218 "JUMP"
// when the client sends CMSG_KEYBOUND_OVERRIDE while the Skyriding aura arms the override).
// The spell itself only carries a dummy + a force-cast of the 404191 marker; the actual launch
// impulse is delivered by Launch Boost (392752), which the retail server casts alongside
// (sniff 66709: takeoff = SPELL_GO 392752 + 374763 + 404191, then SMSG_MOVE_ADD_IMPULSE (0,0,45)).
class spell_dragonriding_lift_off : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRAGONRIDING_LAUNCH_BOOST });
    }

    SpellCastResult CheckCast()
    {
        // launch only transitions INTO advanced flight - while already adv-flying the client uses
        // Skyward Ascent instead, and since the keybound-override cast is triggered (no cooldown)
        // this also stops a client from stacking launch impulses midair
        if (GetCaster()->m_movementInfo.HasExtraMovementFlag2(MOVEMENTFLAG3_ADV_FLYING))
            return SPELL_FAILED_DONT_REPORT;

        return CheckSkyriding(this);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster, SPELL_DRAGONRIDING_LAUNCH_BOOST, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_lift_off::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_lift_off::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 392752 - Launch Boost (SpellScript for initial upward impulse on spell hit)
// Wowhead: Periodic Dummy, period 100ms, duration 2s. Sniff: Z=45 impulse on first hit.
class spell_dragonriding_launch_boost : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            Position direction(0.0f, 0.0f, 45.0f);
            caster->SendAddImpulse(direction);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_launch_boost::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 392752 - Launch Boost (AuraScript for periodic forward impulse ticks after initial launch)
class spell_dragonriding_launch_boost_aura : public AuraScript
{
    void HandlePeriodicDummy(AuraEffect const* /*aurEff*/)
    {
        if (Unit* target = GetTarget())
        {
            if (!target->HasExtraUnitMovementFlag2(MOVEMENTFLAG3_CAN_ADV_FLY))
                return;

            SendFacingImpulse(target, 5.0f);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dragonriding_launch_boost_aura::HandlePeriodicDummy, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 436854 - Switch Flight Style (the spellbook "Skyriding Flight Style" toggle).
// Steady flight is the marker aura 404468 "Flight Style: Steady" (CANNOT_BE_SAVED, so characters
// default back to Skyriding on login); the skyriding mount capabilities' PlayerCondition (96927)
// requires that aura to be ABSENT, so after toggling it a mount-capability re-evaluation flips the
// current mount's flight mode live - exactly what the 67314 flight-style sniff shows.
class spell_dragonriding_switch_flight_style : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRAGONRIDING_FLIGHT_STYLE_STEADY });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_DRAGONRIDING_FLIGHT_STYLE_STEADY))
            caster->RemoveAurasDueToSpell(SPELL_DRAGONRIDING_FLIGHT_STYLE_STEADY);
        else
            caster->CastSpell(caster, SPELL_DRAGONRIDING_FLIGHT_STYLE_STEADY, true);

        caster->UpdateMountCapability();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_switch_flight_style::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_dragonriding_spell_scripts()
{
    RegisterSpellAndAuraScriptPair(spell_dragonriding_launch_boost, spell_dragonriding_launch_boost_aura);
    RegisterSpellScript(spell_dragonriding_lift_off);
    RegisterSpellScript(spell_dragonriding_switch_flight_style);
    RegisterSpellScript(spell_dragonriding_whirling_surge);
    RegisterSpellScript(spell_dragonriding_surge_forward);
    RegisterSpellScript(spell_dragonriding_skyward_ascent);
}
