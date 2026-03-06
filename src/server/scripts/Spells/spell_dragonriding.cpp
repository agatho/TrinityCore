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
// Launch Boost:    (0, 0, 45.0)           — pure upward, magnitude 45.0
// Whirling Surge:  magnitude 5.0 per tick  — facing+pitch oriented, 6 ticks over ~626ms
// Skyward Ascent:  horizontal 12.25 + Z 49.0 — magnitude ~50.51
// Surge Forward:   magnitude 30.0          — facing+pitch oriented, single burst (estimated; not directly observable in sniff)

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
        {
            // Facing+pitch oriented single impulse (estimated; not directly observable from bystander sniff)
            float orientation = caster->GetOrientation();
            float pitch = caster->m_movementInfo.pitch;
            float speed = 30.0f;
            float cosPitch = std::cos(pitch);
            Position direction(
                std::cos(orientation) * cosPitch * speed,
                std::sin(orientation) * cosPitch * speed,
                std::sin(pitch) * speed
            );
            caster->SendAddImpulse(direction);
        }
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
            // Sniff: Z=49.0 fixed + horizontal=12.25 in facing direction, total magnitude ~50.51
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

// 361584 - Whirling Surge
class spell_dragonriding_whirling_surge : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return CheckSkyriding(this);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            // Sniff: magnitude 5.0 per tick, oriented to facing+pitch, ~6 ticks over ~1s
            float orientation = caster->GetOrientation();
            float pitch = caster->m_movementInfo.pitch;
            float speed = 5.0f;
            float cosPitch = std::cos(pitch);
            Position direction(
                std::cos(orientation) * cosPitch * speed,
                std::sin(orientation) * cosPitch * speed,
                std::sin(pitch) * speed
            );
            caster->SendAddImpulse(direction);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dragonriding_whirling_surge::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_whirling_surge::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 392752 - Launch Boost (triggered by 374763 Lift Off on takeoff)
class spell_dragonriding_launch_boost : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            // Sniff: pure upward impulse, magnitude 45.0
            Position direction(0.0f, 0.0f, 45.0f);
            caster->SendAddImpulse(direction);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dragonriding_launch_boost::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_dragonriding_spell_scripts()
{
    RegisterSpellScript(spell_dragonriding_surge_forward);
    RegisterSpellScript(spell_dragonriding_skyward_ascent);
    RegisterSpellScript(spell_dragonriding_whirling_surge);
    RegisterSpellScript(spell_dragonriding_launch_boost);
}
