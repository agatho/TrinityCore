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
    SPELL_DRAGONRIDING_SURGE_FORWARD    = 372608,
    SPELL_DRAGONRIDING_SKYWARD_ASCENT   = 372610,
    SPELL_DRAGONRIDING_WHIRLING_SURGE   = 361584,
    SPELL_DRAGONRIDING_LAUNCH_BOOST     = 392752,
    SPELL_DRAGONRIDING_LIFT_OFF         = 374763,
};

// Blizzlike impulse values from sniff data:
// Launch Boost:    (0, 0, 45.0)           — pure upward, magnitude 45.0, sent on spell hit (before periodic aura)
// Whirling Surge:  magnitude 5.0 per tick  — facing+pitch oriented, 5-6 ticks, periodic aura (3s duration)
// Skyward Ascent:  horizontal 12.25 + Z 49.0 — magnitude ~50.51, single impulse
// Surge Forward:   magnitude 30.0          — facing+pitch oriented, single burst (estimated; not directly observable in sniff)

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
    {
        script->SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_REQUIRES_SKYRIDING);
        return SPELL_FAILED_CUSTOM_ERROR;
    }
    return SPELL_CAST_OK;
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
            SendFacingImpulse(caster, 30.0f);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_surge_forward::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_surge_forward::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
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

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_skyward_ascent::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_skyward_ascent::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 361584 - Whirling Surge (SpellScript for cast validation)
class spell_dragonriding_whirling_surge : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return CheckSkyriding(this);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_whirling_surge::CheckCast);
    }
};

// 361584 - Whirling Surge (AuraScript for periodic impulse ticks)
// Wowhead: Apply Aura: Dummy, 3s duration. Sniff: 5-6 impulse ticks at magnitude 5.0, facing+pitch oriented.
class spell_dragonriding_whirling_surge_aura : public AuraScript
{
    void HandlePeriodicDummy(AuraEffect const* /*aurEff*/)
    {
        if (Unit* target = GetTarget())
            SendFacingImpulse(target, 5.0f);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dragonriding_whirling_surge_aura::HandlePeriodicDummy, EFFECT_0, SPELL_AURA_DUMMY);
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

void AddSC_dragonriding_spell_scripts()
{
    RegisterSpellAndAuraScriptPair(spell_dragonriding_whirling_surge, spell_dragonriding_whirling_surge_aura);
    RegisterSpellAndAuraScriptPair(spell_dragonriding_launch_boost, spell_dragonriding_launch_boost_aura);
    RegisterSpellScript(spell_dragonriding_surge_forward);
    RegisterSpellScript(spell_dragonriding_skyward_ascent);
}
