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
 * Delve companion - creature 248567 "Valeera Sanguinar" (Midnight delve seasons).
 *
 * Evidence: C:\sniff\tcharvest\out\delve_research\REPORT.md section 3 and companion_exit.txt.
 *   - She is a PLAYER-cast summon: spell 1247560 "Summon" (12.0.7: 1247562), SpellEffect 28 SPELL_EFFECT_SUMMON,
 *     MiscValue 248567, SummonProperties 6425 (gulf 103987, eversong 1848985, deatholme 56090, shadowmoon 114847).
 *     DelveInstanceScript::SummonCompanionFor makes the cast; this file is what she does once she exists.
 *   - CreatureStats: Elite, CreatureType 7, DisplayID 26365, TypeFlags 0x00001820 / 0x03300002, VignetteID 7135,
 *     WidgetSetID 1739.
 *   - At spawn (gulf 103987-103996) she casts 1252003 "Stealth", 413899 "Party Leader", 1266068, 1248903, 1260257,
 *     17683, 1249690 "Npc Join Player Party", 1272120 "Flint and Tinder", 1251111 "Bloodcrypt Toxin"; the player
 *     casts 469315 "[DNT] No Curio Selected"; SMSG_PARTY_UPDATE grows from 138 B to 222 B (she is in the party).
 *   - She follows (2216 SMSG_ON_MONSTER_MOVE frames in gulf) and fights: gulf SPELL_START counts
 *     1272299 Jagged Caltrops 67, 1247770 Rupture 65, 1248196 Cheap Shot 29, 1248011 Killing Spree 24,
 *     1247747 Fan of Knives 24, 1251111 Bloodcrypt Toxin 23, 1272285 Jagged Caltrops 22, 1252003 Stealth 21,
 *     1266682 Afraid of the Dark 18 (deatholme S1: 1247770 57, 1252003 45, 416237 30, 1248196 24).
 *   - 249057 "Valeera Sanguinar" (same display) is created 44 times in gulf: transient combat copies. Not implemented.
 *
 * Not implemented, on purpose:
 *   - 1249690 "Npc Join Player Party" is SpellEffect 190 SPELL_EFFECT_CHANGE_PARTY_MEMBERS, which the core maps to
 *     EffectNULL (SpellEffects.cpp), so casting it would do nothing; Group members are players only. The party
 *     membership seen in SMSG_PARTY_UPDATE therefore has no server-side equivalent yet.
 *   - The exact rotation. Only the observed spell ids are used; the cadence below is derived from the per-run cast
 *     counts over the ~16 minutes of combat in gulf, not from a decoded priority list.
 *
 * Bind with creature_template.ScriptName = 'npc_delve_companion' on 248567 (2026_09_12_02_world.sql).
 */

#include "delves_common.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"

using namespace Delves;

namespace
{

enum CompanionSpells : uint32
{
    SPELL_STEALTH               = 1252003,  // aura 16 MOD_STEALTH, cast at spawn and after every fight
    SPELL_BLOODCRYPT_TOXIN      = 1251111,  // self, at spawn (gulf 103996) and periodically
    SPELL_AFRAID_OF_THE_DARK    = 1266682,  // self; cast by nearly every creature in the delve
    SPELL_CHEAP_SHOT            = 1248196,  // opener from stealth
    SPELL_RUPTURE               = 1247770,
    SPELL_JAGGED_CALTROPS       = 1272299,
    SPELL_JAGGED_CALTROPS_ALT   = 1272285,
    SPELL_FAN_OF_KNIVES         = 1247747,  // AoE
    SPELL_KILLING_SPREE         = 1248011,
};

enum CompanionEvents
{
    EVENT_FOLLOW_CHECK = 1,
    EVENT_RUPTURE,
    EVENT_JAGGED_CALTROPS,
    EVENT_JAGGED_CALTROPS_ALT,
    EVENT_FAN_OF_KNIVES,
    EVENT_KILLING_SPREE,
    EVENT_BLOODCRYPT_TOXIN,
    EVENT_AFRAID_OF_THE_DARK,
};

// REPORT 3.1: "follows" - PET_FOLLOW_DIST-like spacing behind the player. Distances are not on the wire.
constexpr float COMPANION_FOLLOW_DISTANCE = 3.0f;
constexpr float COMPANION_FOLLOW_ANGLE = float(M_PI) / 4.0f;
constexpr float COMPANION_TELEPORT_DISTANCE = 60.0f;
constexpr float FAN_OF_KNIVES_RANGE = 8.0f;

struct npc_delve_companion : public ScriptedAI
{
    npc_delve_companion(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        me->SetReactState(REACT_ASSIST);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* owner = summoner ? summoner->ToPlayer() : nullptr;
        if (!owner)
        {
            // Summoned by something other than a player (e.g. a GM command): nobody to follow, do not linger.
            me->DespawnOrUnsummon(1s);
            return;
        }

        _ownerGuid = owner->GetGUID();
        me->SetFaction(owner->GetFaction());

        // gulf 103987: Stealth is the first thing she casts; 103996: Bloodcrypt Toxin on herself
        CastIfKnown(me, SPELL_STEALTH);
        CastIfKnown(me, SPELL_BLOODCRYPT_TOXIN);

        me->GetMotionMaster()->MoveFollow(owner, COMPANION_FOLLOW_DISTANCE, ChaseAngle(COMPANION_FOLLOW_ANGLE));
        _events.ScheduleEvent(EVENT_FOLLOW_CHECK, 2s);
    }

    void JustEngagedWith(Unit* who) override
    {
        // Opener from stealth (Cheap Shot: 29 casts in gulf, always the first SPELL_START of a fight)
        if (who && me->HasAura(SPELL_STEALTH))
            CastIfKnown(who, SPELL_CHEAP_SHOT);

        _events.ScheduleEvent(EVENT_RUPTURE, 1s);
        _events.ScheduleEvent(EVENT_JAGGED_CALTROPS, 3s);
        _events.ScheduleEvent(EVENT_FAN_OF_KNIVES, 4s);
        _events.ScheduleEvent(EVENT_KILLING_SPREE, 10s);
        _events.ScheduleEvent(EVENT_JAGGED_CALTROPS_ALT, 12s);
        _events.ScheduleEvent(EVENT_BLOODCRYPT_TOXIN, 20s);
        _events.ScheduleEvent(EVENT_AFRAID_OF_THE_DARK, 30s);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        _events.Reset();
        me->CombatStop(true);

        if (Player* owner = GetOwner())
            me->GetMotionMaster()->MoveFollow(owner, COMPANION_FOLLOW_DISTANCE, ChaseAngle(COMPANION_FOLLOW_ANGLE));

        // Back into stealth between fights (21 Stealth casts over the gulf run)
        CastIfKnown(me, SPELL_STEALTH);
        _events.ScheduleEvent(EVENT_FOLLOW_CHECK, 2s);
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
                    _events.ScheduleEvent(EVENT_FOLLOW_CHECK, 2s);
                    break;
                case EVENT_RUPTURE:
                    if (Unit* victim = me->GetVictim())
                        CastIfKnown(victim, SPELL_RUPTURE);
                    _events.ScheduleEvent(EVENT_RUPTURE, 8s);
                    break;
                case EVENT_JAGGED_CALTROPS:
                    if (Unit* victim = me->GetVictim())
                        CastIfKnown(victim, SPELL_JAGGED_CALTROPS);
                    _events.ScheduleEvent(EVENT_JAGGED_CALTROPS, 12s);
                    break;
                case EVENT_JAGGED_CALTROPS_ALT:
                    if (Unit* victim = me->GetVictim())
                        CastIfKnown(victim, SPELL_JAGGED_CALTROPS_ALT);
                    _events.ScheduleEvent(EVENT_JAGGED_CALTROPS_ALT, 25s);
                    break;
                case EVENT_FAN_OF_KNIVES:
                    if (me->GetVictim() && CountUnfriendlyInRange(FAN_OF_KNIVES_RANGE) >= 2)
                        CastIfKnown(me, SPELL_FAN_OF_KNIVES);
                    _events.ScheduleEvent(EVENT_FAN_OF_KNIVES, 8s);
                    break;
                case EVENT_KILLING_SPREE:
                    if (Unit* victim = me->GetVictim())
                        CastIfKnown(victim, SPELL_KILLING_SPREE);
                    _events.ScheduleEvent(EVENT_KILLING_SPREE, 30s);
                    break;
                case EVENT_BLOODCRYPT_TOXIN:
                    CastIfKnown(me, SPELL_BLOODCRYPT_TOXIN);
                    _events.ScheduleEvent(EVENT_BLOODCRYPT_TOXIN, 30s);
                    break;
                case EVENT_AFRAID_OF_THE_DARK:
                    CastIfKnown(me, SPELL_AFRAID_OF_THE_DARK);
                    _events.ScheduleEvent(EVENT_AFRAID_OF_THE_DARK, 45s);
                    break;
                default:
                    break;
            }
        }

        UpdateVictim(); // melee auto-attack is driven by the core
    }

private:
    Player* GetOwner() const
    {
        return ObjectAccessor::GetPlayer(*me, _ownerGuid);
    }

    // Every id in the kit is season content: cast only what the spell store actually has.
    void CastIfKnown(Unit* target, uint32 spellId)
    {
        if (!target || !sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
            return;
        me->CastSpell(target, spellId, false);
    }

    uint32 CountUnfriendlyInRange(float range) const
    {
        std::list<Unit*> targets;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(me, me, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, targets, check);
        Cell::VisitAllObjects(me, searcher, range);
        return uint32(targets.size());
    }

    void HandleFollowCheck()
    {
        Player* owner = GetOwner();
        // The companion never outlives her player's stay in the delve (REPORT 3.1: one create per entry, never
        // seen without her player). DelveInstanceScript::OnPlayerLeave despawns her too; this covers the rest.
        if (!owner || !owner->IsInWorld() || owner->GetMap() != me->GetMap())
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (me->IsInCombat())
            return;

        if (me->GetDistance(owner) > COMPANION_TELEPORT_DISTANCE)
        {
            me->NearTeleportTo(owner->GetPosition());
            me->GetMotionMaster()->MoveFollow(owner, COMPANION_FOLLOW_DISTANCE, ChaseAngle(COMPANION_FOLLOW_ANGLE));
        }

        if (!me->HasAura(SPELL_STEALTH))
            CastIfKnown(me, SPELL_STEALTH);

        // Assist: fight what the player fights
        if (owner->IsInCombat())
            if (Unit* target = owner->GetVictim())
                if (me->IsValidAttackTarget(target))
                    AttackStart(target);
    }

    ObjectGuid _ownerGuid;
    EventMap _events;
};

} // anonymous namespace

void AddSC_npc_delve_companion()
{
    RegisterCreatureAI(npc_delve_companion);
}
