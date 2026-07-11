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

#include "DelvesCompanion.h"
#include "DelvesDefines.h"
#include "DelvesPackets.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "WorldSession.h"

using namespace Delves;

namespace
{

// Follow distance and angle
static constexpr float COMPANION_FOLLOW_DISTANCE = 3.0f;
static constexpr float COMPANION_FOLLOW_ANGLE = float(M_PI) / 4.0f; // 45 degrees behind-left

// Combat ranges
[[maybe_unused]] static constexpr float COMPANION_MELEE_RANGE = 5.0f;
[[maybe_unused]] static constexpr float COMPANION_RANGED_RANGE = 30.0f;
[[maybe_unused]] static constexpr float COMPANION_HEAL_RANGE = 40.0f;

// Health thresholds
static constexpr float HEAL_THRESHOLD_PCT = 70.0f;
static constexpr float EMERGENCY_HEAL_THRESHOLD_PCT = 35.0f;

// Timers (ms)
static constexpr uint32 ABILITY_COOLDOWN_DPS     = 8000;
static constexpr uint32 ABILITY_COOLDOWN_HEAL    = 6000;
static constexpr uint32 ABILITY_COOLDOWN_TANK    = 10000;
static constexpr uint32 FOLLOW_CHECK_INTERVAL    = 2000;
static constexpr uint32 COMBAT_CHECK_INTERVAL    = 1500;

enum CompanionEvents
{
    EVENT_FOLLOW_CHECK      = 1,
    EVENT_ABILITY_USE       = 2,
    EVENT_HEAL_CHECK        = 3,
    EVENT_COMBAT_CHECK      = 4,
};

struct npc_brann_bronzebeard_delvesAI : public ScriptedAI
{
    npc_brann_bronzebeard_delvesAI(Creature* creature) : ScriptedAI(creature)
    {
        _ownerGuid = ObjectGuid::Empty;
        _role = CompanionRole::Dps;
    }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_ASSIST);
    }

    bool OnGossipHello(Player* player) override
    {
        // Open the client's companion configuration frame (role/curio picker).
        // 68275: the client reads an EMPTY body for this SMSG (the 12.0.1.66709
        // sniff's 4-byte payload is ignored as trailing bytes), so no fields.
        WorldPackets::Delves::ShowDelvesCompanionConfigurationUI packet;
        player->SendDirectMessage(packet.Write());
        return true;
    }

    void SetData(uint32 type, uint32 data) override
    {
        // Used to configure role from DelveInstance
        if (type == 0) // Role
            _role = static_cast<CompanionRole>(data);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
        {
            _ownerGuid = player->GetGUID();
            me->GetMotionMaster()->MoveFollow(player, COMPANION_FOLLOW_DISTANCE, COMPANION_FOLLOW_ANGLE);
        }

        _events.ScheduleEvent(EVENT_FOLLOW_CHECK, 3s);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        switch (_role)
        {
            case CompanionRole::Dps:
                _events.ScheduleEvent(EVENT_ABILITY_USE, 2s);
                break;
            case CompanionRole::Healer:
                _events.ScheduleEvent(EVENT_HEAL_CHECK, 1s);
                _events.ScheduleEvent(EVENT_ABILITY_USE, 3s);
                break;
            case CompanionRole::Tank:
                _events.ScheduleEvent(EVENT_ABILITY_USE, 1s);
                _events.ScheduleEvent(EVENT_COMBAT_CHECK, 2s);
                break;
            default:
                break;
        }
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        _events.Reset();
        me->CombatStop(true);

        // Return to following owner
        if (Player* owner = GetOwner())
            me->GetMotionMaster()->MoveFollow(owner, COMPANION_FOLLOW_DISTANCE, COMPANION_FOLLOW_ANGLE);

        _events.ScheduleEvent(EVENT_FOLLOW_CHECK, 3s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FOLLOW_CHECK:
                    HandleFollowCheck();
                    _events.ScheduleEvent(EVENT_FOLLOW_CHECK, Milliseconds(FOLLOW_CHECK_INTERVAL));
                    break;
                case EVENT_ABILITY_USE:
                    HandleAbilityUse();
                    break;
                case EVENT_HEAL_CHECK:
                    HandleHealCheck();
                    _events.ScheduleEvent(EVENT_HEAL_CHECK, Milliseconds(ABILITY_COOLDOWN_HEAL));
                    break;
                case EVENT_COMBAT_CHECK:
                    HandleCombatCheck();
                    _events.ScheduleEvent(EVENT_COMBAT_CHECK, Milliseconds(COMBAT_CHECK_INTERVAL));
                    break;
                default:
                    break;
            }
        }

        // Standard melee attack for DPS and Tank roles
        if (_role != CompanionRole::Healer && UpdateVictim())
            me->DoMeleeAttackIfReady();
    }

private:
    Player* GetOwner() const
    {
        return ObjectAccessor::GetPlayer(*me, _ownerGuid);
    }

    void HandleFollowCheck()
    {
        if (me->IsInCombat())
            return;

        Player* owner = GetOwner();
        if (!owner)
            return;

        // If too far from owner, teleport to them
        if (me->GetDistance(owner) > 50.0f)
        {
            me->NearTeleportTo(owner->GetPosition());
            me->GetMotionMaster()->MoveFollow(owner, COMPANION_FOLLOW_DISTANCE, COMPANION_FOLLOW_ANGLE);
        }

        // If owner is in combat and we're not, assist
        if (owner->IsInCombat() && !me->IsInCombat())
        {
            if (Unit* target = owner->GetVictim())
                AttackStart(target);
            else if (_role == CompanionRole::Healer)
            {
                // Healer enters combat to heal but doesn't need a target
                _events.ScheduleEvent(EVENT_HEAL_CHECK, 1s);
            }
        }
    }

    void HandleAbilityUse()
    {
        if (!me->IsInCombat())
            return;

        switch (_role)
        {
            case CompanionRole::Dps:
                HandleDpsAbility();
                _events.ScheduleEvent(EVENT_ABILITY_USE, Milliseconds(ABILITY_COOLDOWN_DPS));
                break;
            case CompanionRole::Healer:
                // Healer's offensive ability - basic melee when no spells configured
                if (me->GetVictim())
                    me->DoMeleeAttackIfReady();
                _events.ScheduleEvent(EVENT_ABILITY_USE, Milliseconds(ABILITY_COOLDOWN_HEAL));
                break;
            case CompanionRole::Tank:
                HandleTankAbility();
                _events.ScheduleEvent(EVENT_ABILITY_USE, Milliseconds(ABILITY_COOLDOWN_TANK));
                break;
            default:
                break;
        }
    }

    void HandleDpsAbility()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        // Kill Shot on low HP targets (below 30%, 5% for bosses)
        float executeThreshold = victim->IsCreature() && victim->ToCreature()->IsDungeonBoss() ? 5.0f : 30.0f;
        if (victim->GetHealthPct() < executeThreshold)
        {
            // TODO: Cast Kill Shot spell when spell IDs are known
            TC_LOG_TRACE("scripts.delves", "Brann (DPS): Execute phase on {} ({}% HP)",
                victim->GetName(), static_cast<int>(victim->GetHealthPct()));
        }

        // Standard damage rotation
        // TODO: Cast abilities when spell IDs are determined from trait tree
    }

    void HandleHealCheck()
    {
        if (!me->IsInCombat())
            return;

        Player* owner = GetOwner();
        if (!owner || !owner->IsAlive())
            return;

        // Emergency heal
        if (owner->GetHealthPct() < EMERGENCY_HEAL_THRESHOLD_PCT)
        {
            // TODO: Cast emergency heal spell
            TC_LOG_TRACE("scripts.delves", "Brann (Healer): Emergency heal on {} ({}% HP)",
                owner->GetName(), static_cast<int>(owner->GetHealthPct()));
            return;
        }

        // Regular heal
        if (owner->GetHealthPct() < HEAL_THRESHOLD_PCT)
        {
            // TODO: Cast regular heal spell
            TC_LOG_TRACE("scripts.delves", "Brann (Healer): Healing {} ({}% HP)",
                owner->GetName(), static_cast<int>(owner->GetHealthPct()));
        }

        // Also check other group members
        // TODO: Iterate group members and heal lowest HP
    }

    void HandleTankAbility()
    {
        // Taunt the primary target if it's attacking the owner
        Player* owner = GetOwner();
        if (!owner)
            return;

        if (Unit* ownerTarget = owner->GetVictim())
        {
            if (ownerTarget->GetVictim() == owner && ownerTarget != me->GetVictim())
            {
                AttackStart(ownerTarget);
                // TODO: Cast taunt spell
                TC_LOG_TRACE("scripts.delves", "Brann (Tank): Taunting {} off {}",
                    ownerTarget->GetName(), owner->GetName());
            }
        }

        // Defensive cooldowns
        // TODO: Cast shield/defensive abilities when spell IDs known
    }

    void HandleCombatCheck()
    {
        // Tank: ensure we're tanking the most dangerous target
        if (_role != CompanionRole::Tank)
            return;

        // TODO: Threat management, multi-target taunting
    }

    ObjectGuid _ownerGuid;
    CompanionRole _role;
    EventMap _events;
};

} // anonymous namespace

void AddSC_npc_brann_bronzebeard_delves()
{
    RegisterCreatureAI(npc_brann_bronzebeard_delvesAI);
}
