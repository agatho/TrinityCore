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
    Prey hunts - see PreyHunt.h for the retail flow these scripts reproduce.
*/

#include "CreatureTextMgr.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PreyHunt.h"
#include "PreyMgr.h"
#include "QuestDef.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TaskScheduler.h"

namespace Scripts::QuelThalas::PreyHunts
{
using namespace ::Prey;

namespace Spells
{
    // Hunted Remnant
    static constexpr uint32 RemnantAmbush           = 1241603;
    static constexpr uint32 RemnantAmbushAura       = 1259781;
    static constexpr uint32 FixatedAmbush           = 1241591;
    static constexpr uint32 VindictiveResolve       = 1241600;
    static constexpr uint32 Fallback                = 1241601;
    static constexpr uint32 AmbushCredit            = 1284574;

    // Earthgrab Totem (The Talon of Jan'alai)
    static constexpr uint32 EarthgrabTotemSummon    = 1254819;
    static constexpr uint32 Earthgrab               = 1254834;

    static constexpr uint32 RemnantOfAnguishCurrency = 3392;
}

namespace Texts
{
    static constexpr uint8 PreyAggro = 0;
    static constexpr uint8 PreyDeath = 1;
}

namespace Misc
{
    static constexpr std::string_view HuntProgressText = "+Hunt Progress";

    // the remnant gives up at half health in every captured ambush (47..50 %)
    static constexpr uint32 RemnantFallbackPct = 50;
    static constexpr Seconds RemnantFallbackDelay = 2s;

    // the fleeing prey showed up 43..80 yards from where the remnant fell back
    static constexpr float PursuitMinDistance = 40.0f;
    static constexpr float PursuitMaxDistance = 80.0f;

    static constexpr int32 FirstFourHuntsAnguish = 200;
    static constexpr Seconds PreyApproachInterval = 1s;
    static constexpr Seconds TotemEarthgrabInterval = 3s;
}

void SchedulePreyHuntSpells(TaskScheduler& scheduler, Creature* me, std::vector<PreyHuntSpell> const& spells)
{
    for (PreyHuntSpell const& spell : spells)
    {
        scheduler.Schedule(randtime(spell.InitialMin, spell.InitialMax), [me, spell](TaskContext context)
        {
            if (me->GetVictim())
                me->CastSpell(me->GetVictim(), spell.SpellId, false);

            if (spell.RepeatMax > 0ms)
                context.Repeat(randtime(spell.RepeatMin, spell.RepeatMax));
        });
    }
}

// 246605 - Hunted Remnant
struct npc_prey_hunted_remnant : public ScriptedAI
{
    npc_prey_hunted_remnant(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner->ToPlayer();
        if (!player)
            return;

        _playerGuid = player->GetGUID();
        _hunt = GetActiveHunt(player);
        if (_hunt)
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(_hunt->QuestId))
                if (uint32 displayId = quest->GetQuestGiverPortrait())
                    me->SetDisplayId(displayId);

        DoCastSelf(Spells::RemnantAmbush, true);
        DoCastSelf(Spells::RemnantAmbushAura, true);
        DoCast(player, Spells::FixatedAmbush, true);
        AttackStart(player);

        if (_hunt)
            SchedulePreyHuntSpells(_scheduler, me, _hunt->Target->RemnantSpells);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_fellBack)
        {
            damage = 0;
            return;
        }

        if (!me->HealthBelowPctDamaged(Misc::RemnantFallbackPct, damage))
            return;

        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;

        FallBack();
    }

    void FallBack()
    {
        _fellBack = true;
        _scheduler.CancelAll();
        me->InterruptNonMeleeSpells(true);

        DoCastSelf(Spells::VindictiveResolve, true);
        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
            DoCast(player, SPELL_SUMMON_ANGUISH_DUMMY, true);
        DoCastSelf(Spells::Fallback, true);

        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->SetUninteractible(true);

        _scheduler.Schedule(Misc::RemnantFallbackDelay, [this](TaskContext /*context*/)
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
            {
                DoCast(player, Spells::AmbushCredit, true);

                // the prey flees and recovers a little further away
                if (_hunt && _hunt->Target->PursuitSpellId)
                {
                    Position destination = me->GetPosition();
                    me->MovePositionToFirstCollision(destination, frand(Misc::PursuitMinDistance, Misc::PursuitMaxDistance), frand(0.0f, 2.0f * float(M_PI)));
                    player->CastSpell(destination, _hunt->Target->PursuitSpellId, true);
                }
            }

            me->DespawnOrUnsummon();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);

        if (!_fellBack)
            UpdateVictim();
    }

private:
    ObjectGuid _playerGuid;
    PreyHunt const* _hunt = nullptr;
    bool _fellBack = false;
    TaskScheduler _scheduler;
};

// Pursuit summons (253562, 253894..253922, 268752..268755) - the prey recovering after an ambush
struct npc_prey_pursuit : public ScriptedAI
{
    npc_prey_pursuit(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
    }
};

// The prey at the end of every hunt (`prey_hunt`.`PreyEntry`)
struct npc_prey_hunt_target : public ScriptedAI
{
    npc_prey_hunt_target(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _scheduler.CancelAll();
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_ASTALORS_CHALLENGE)
            return;

        if (Unit* challenger = caster->ToUnit())
            if (!me->IsEngaged())
                AttackStart(challenger);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        TalkIfExists(Texts::PreyAggro);

        if (PreyHunt const* hunt = sPreyMgr->GetHuntByPreyEntry(me->GetEntry()))
            SchedulePreyHuntSpells(_scheduler, me, hunt->Target->PreySpells);
    }

    void JustDied(Unit* /*killer*/) override
    {
        TalkIfExists(Texts::PreyDeath);
        DoCastSelf(SPELL_PREY_CREDIT, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _scheduler.Update(diff);
    }

private:
    void TalkIfExists(uint8 group)
    {
        if (sCreatureTextMgr->TextExist(me->GetEntry(), group))
            Talk(group);
    }

    TaskScheduler _scheduler;
};

// 252051 - Earthgrab Totem
struct npc_prey_earthgrab_totem : public ScriptedAI
{
    npc_prey_earthgrab_totem(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        _scheduler.Schedule(0s, [this](TaskContext context)
        {
            DoCastSelf(Spells::Earthgrab);
            context.Repeat(Misc::TotemEarthgrabInterval);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

// 555631 - Remnant of Anguish
struct go_prey_remnant_of_anguish : public GameObjectAI
{
    go_prey_remnant_of_anguish(GameObject* go) : GameObjectAI(go) { }

    void OnLootStateChanged(uint32 state, Unit* unit) override
    {
        if (state != GO_ACTIVATED || !unit)
            return;

        if (Player* player = unit->ToPlayer())
            player->CastSpell(player, SPELL_PROGRESS_SMALL, true);
    }
};

// 1241010 - UI Toast [DNT] (source spell of every hunt quest)
class spell_prey_ui_toast_accept : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TARGET_ACCEPT_SHARED });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        SendUiEventToast(player, UI_EVENT_TOAST_HUNT_STARTED);
        player->CastSpell(player, SPELL_TARGET_ACCEPT_SHARED, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_ui_toast_accept::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1244011 - Target Accept Shared [DNT]
class spell_prey_target_accept_shared : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        if (PreyHunt const* hunt = GetActiveHunt(player))
            StartHunt(player, *hunt);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_target_accept_shared::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241553 - Ambush (zone aura)
class spell_prey_ambush_zone_aura : public AuraScript
{
    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            StartAmbushTimer(player);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_prey_ambush_zone_aura::AfterApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1241561 - Active Ambush [DNT]
class spell_prey_active_ambush : public AuraScript
{
    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            player->SetDataElementCharacter(ELEMENT_AMBUSH_ACTIVE, int64(1));
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            player->SetDataElementCharacter(ELEMENT_AMBUSH_ACTIVE, int64(0));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_prey_active_ambush::AfterApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_prey_active_ambush::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1242379 - Summon Anguish [DNT] (remnant -> summoner)
class spell_prey_summon_anguish : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SUMMON_ANGUISH, SPELL_AMBUSH_BLOCKER, SPELL_PROGRESS_SMALL });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        player->CastSpell(GetCaster()->GetPosition(), SPELL_SUMMON_ANGUISH, true);
        player->CastSpell(player, SPELL_AMBUSH_BLOCKER, true);
        player->CastSpell(player, SPELL_PROGRESS_SMALL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_summon_anguish::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241123 - On the Hunt (+10)
// 1241124 - On the Hunt (+20, reward spell of the world quests and prey activities, counts only inside the hunt zone)
class spell_prey_on_the_hunt : public SpellScript
{
public:
    explicit spell_prey_on_the_hunt(int32 amount, bool requiresHuntZone) : _amount(amount), _requiresHuntZone(requiresHuntZone) { }

private:
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PROGRESS_GAINED });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player || !GetActiveHunt(player) || GetHuntState(player) != HuntState::Hunting)
            return;

        if (_requiresHuntZone && !IsInHuntZone(player))
            return;

        SendWorldText(player, Misc::HuntProgressText);
        player->CastSpell(player, SPELL_PROGRESS_GAINED, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT0, _amount));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_on_the_hunt::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }

    int32 _amount;
    bool _requiresHuntZone;
};

// 1241147 - On the Hunt (progress gained)
class spell_prey_progress_gained : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitPlayer())
            AddProgress(player, GetEffectValue());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_progress_gained::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241311 - On the Hunt (shares the final with party members nearby)
class spell_prey_final_party_share : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        Player* member = GetHitPlayer();
        if (!caster || !member || member == caster)
            return;

        PreyHunt const* hunt = GetActiveHunt(caster);
        if (hunt && GetActiveHunt(member) == hunt)
            RevealPrey(member, false);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_final_party_share::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1241154 - On the Hunt (prey revealed)
class spell_prey_final_toast : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitPlayer())
            SendUiEventToast(player, UI_EVENT_TOAST_PREY_REVEALED);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_final_toast::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1259096 - Final Strike
class spell_prey_final_strike : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ASTALORS_CHALLENGE, SPELL_CHALLENGE_BLOCKER });
    }

    // Walking up to the prey's POI brings the prey out and challenges it.
    static void CheckPreyApproach(Player* player, time_t auraApplyTime)
    {
        Aura const* aura = player->GetAura(SPELL_FINAL_STRIKE);
        if (!aura || aura->GetApplyTime() != auraApplyTime)
            return;

        if (Creature* prey = SummonPrey(player))
        {
            if (prey->IsAlive() && !prey->IsEngaged())
            {
                player->CastSpell(prey, SPELL_ASTALORS_CHALLENGE, true);
                player->CastSpell(player, SPELL_CHALLENGE_BLOCKER, true);
            }
        }

        player->m_Events.AddEventAtOffset([player, auraApplyTime]() { CheckPreyApproach(player, auraApplyTime); }, Misc::PreyApproachInterval);
    }

    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            time_t applyTime = GetAura()->GetApplyTime();
            player->m_Events.AddEventAtOffset([player, applyTime]() { CheckPreyApproach(player, applyTime); }, Misc::PreyApproachInterval);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_prey_final_strike::AfterApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1259068 - Credit [DNT] (cast by the dying prey on its tap list)
class spell_prey_credit : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        Creature* prey = GetCaster()->ToCreature();
        if (player && prey)
            OnPreySlain(player, prey);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_credit::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1279779 - [DNT] 12.0 Prey First Four Hunts Bonus Anguish
// SPELL_EFFECT_LOOT (NYI in the core) - retail granted 200 Remnant of Anguish with a loot toast.
class spell_prey_first_four_hunts_bonus : public SpellScript
{
    void HandleLoot(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        player->ModifyCurrency(Spells::RemnantOfAnguishCurrency, Misc::FirstFourHuntsAnguish, CurrencyGainSource::Spell);
        player->SendDisplayToast(Spells::RemnantOfAnguishCurrency, DisplayToastType::NewCurrency, false, Misc::FirstFourHuntsAnguish, DisplayToastMethod::Loot);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_first_four_hunts_bonus::HandleLoot, EFFECT_0, SPELL_EFFECT_LOOT);
    }
};

// 1262032 - Toast [DNT] (hunt completed)
class spell_prey_toast_complete : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitPlayer())
            SendUiEventToast(player, UI_EVENT_TOAST_HUNT_COMPLETED);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_prey_toast_complete::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1254818 - Earthgrab Totem (the cast bar; the totem itself is 1254819 at the victim)
class spell_prey_earthgrab_totem : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ Spells::EarthgrabTotemSummon });
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (Unit* victim = caster->GetVictim())
            caster->CastSpell(victim->GetPosition(), Spells::EarthgrabTotemSummon, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_prey_earthgrab_totem::HandleAfterCast);
    }
};

// Every hunt quest: publishes the end of the hunt
class quest_prey_hunt : public QuestScript
{
public:
    quest_prey_hunt() : QuestScript("quest_prey_hunt") { }

    void OnQuestStatusChange(Player* player, Quest const* quest, QuestStatus oldStatus, QuestStatus newStatus) override
    {
        if (newStatus == QUEST_STATUS_REWARDED)
            EndHunt(player, quest, true);
        else if (newStatus == QUEST_STATUS_NONE && oldStatus != QUEST_STATUS_NONE)
            EndHunt(player, quest, false);
    }
};

// Zone auras follow the player in and out of the hunt zone
class player_prey_hunt : public PlayerScript
{
public:
    player_prey_hunt() : PlayerScript("player_prey_hunt") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        UpdateZoneAuras(player);
    }

    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override
    {
        UpdateZoneAuras(player);
    }
};
}

void AddSC_prey_hunt()
{
    using namespace Scripts::QuelThalas::PreyHunts;

    // Creatures
    RegisterCreatureAI(npc_prey_hunted_remnant);
    RegisterCreatureAI(npc_prey_pursuit);
    RegisterCreatureAI(npc_prey_hunt_target);
    RegisterCreatureAI(npc_prey_earthgrab_totem);

    // GameObjects
    RegisterGameObjectAI(go_prey_remnant_of_anguish);

    // Spells
    RegisterSpellScript(spell_prey_ui_toast_accept);
    RegisterSpellScript(spell_prey_target_accept_shared);
    RegisterSpellScript(spell_prey_ambush_zone_aura);
    RegisterSpellScript(spell_prey_active_ambush);
    RegisterSpellScript(spell_prey_summon_anguish);
    RegisterSpellScriptWithArgs(spell_prey_on_the_hunt, "spell_prey_on_the_hunt_small", PROGRESS_SMALL, false);
    RegisterSpellScriptWithArgs(spell_prey_on_the_hunt, "spell_prey_on_the_hunt_large", PROGRESS_LARGE, true);
    RegisterSpellScript(spell_prey_progress_gained);
    RegisterSpellScript(spell_prey_final_party_share);
    RegisterSpellScript(spell_prey_final_toast);
    RegisterSpellScript(spell_prey_final_strike);
    RegisterSpellScript(spell_prey_credit);
    RegisterSpellScript(spell_prey_first_four_hunts_bonus);
    RegisterSpellScript(spell_prey_toast_complete);
    RegisterSpellScript(spell_prey_earthgrab_totem);

    // Quests
    new quest_prey_hunt();

    // Players
    new player_prey_hunt();
}
