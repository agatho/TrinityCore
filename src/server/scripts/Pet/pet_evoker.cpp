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
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "npc_pet_evo_".
 *
 * Temporary autonomous companion shape (Evoker Duplicate / Future Self): recurring
 * EventMap casts for the summon duration — see spell-implementation-guide fundamentals
 * § Temporary autonomous companion. Sibling cadence: npc_pet_shaman_fire_elemental;
 * owner-target gate: npc_pet_dk_lesser_ghoul.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellDefines.h"

enum EvokerPetSpells
{
    SPELL_EVOKER_PET_ERUPTION       = 395160,
    SPELL_EVOKER_PET_FIRE_BREATH    = 357208,
    SPELL_EVOKER_PET_UPHEAVAL       = 396286
};

enum EvokerPetEvents
{
    EVENT_EVO_FUTURE_SELF_ERUPTION      = 1,
    EVENT_EVO_FUTURE_SELF_FIRE_BREATH   = 2,
    EVENT_EVO_FUTURE_SELF_UPHEAVAL      = 3
};

// 253466 - Future Self (Duplicate 1259171)
struct npc_pet_evo_future_self : public ScriptedAI
{
    npc_pet_evo_future_self(Creature* creature) : ScriptedAI(creature) { }

    bool CanAIAttack(Unit const* target) const override
    {
        Unit* owner = me->GetOwner();
        if (owner && !target->IsInCombatWith(owner) && owner->GetVictim() != target)
            return false;
        return ScriptedAI::CanAIAttack(target);
    }

    void JustAppeared() override
    {
        _events.Reset();
        // Stagger the three kit casts across the ~20s summon window (cadence hypothesis;
        // retail sniff can refine later — not a one-shot JustAppeared fake).
        _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_ERUPTION, 1s);
        _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_FIRE_BREATH, 3s);
        _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_UPHEAVAL, 6s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (Unit* owner = me->GetOwner())
                if (Unit* ownerVictim = owner->GetVictim())
                    AttackStart(ownerVictim);
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Unit* target = me->GetVictim();
            if (!target)
                if (Unit* owner = me->GetOwner())
                    target = owner->GetVictim();

            if (!target)
            {
                _events.ScheduleEvent(eventId, 1s);
                break;
            }

            // Ignore power/reagent so the copy can cast without Essence; leave empower
            // channel intact (no TRIGGERED_FULL_MASK) so Fire Breath / Upheaval complete.
            CastSpellExtraArgs args(TriggerCastFlags(TRIGGERED_IGNORE_POWER_COST | TRIGGERED_DONT_REPORT_CAST_ERROR));

            switch (eventId)
            {
                case EVENT_EVO_FUTURE_SELF_ERUPTION:
                    me->CastSpell(target, SPELL_EVOKER_PET_ERUPTION, args);
                    _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_ERUPTION, 5s);
                    break;
                case EVENT_EVO_FUTURE_SELF_FIRE_BREATH:
                    me->CastSpell(target, SPELL_EVOKER_PET_FIRE_BREATH, args);
                    _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_FIRE_BREATH, 8s);
                    break;
                case EVENT_EVO_FUTURE_SELF_UPHEAVAL:
                    me->CastSpell(target, SPELL_EVOKER_PET_UPHEAVAL, args);
                    _events.ScheduleEvent(EVENT_EVO_FUTURE_SELF_UPHEAVAL, 8s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

void AddSC_evoker_pet_scripts()
{
    RegisterCreatureAI(npc_pet_evo_future_self);
}
