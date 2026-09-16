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
 * Lorewalking (12.1). Flow reproduced from retail captures of build 69497:
 * Li Li Stormstout's gossip casts Lorewalking Choice (PlayerChoice 845) -> the response casts the story's launch spell ->
 * the launch spell's enter spell starts the story (quest log swap, first wrapper quest or the parked progress) ->
 * Exit Lorewalking (confirmation prompt or the last wrapper's reward spell) ends it and returns the player to Li Li.
 */

#include "ScriptMgr.h"
#include "LFGMgr.h"
#include "LorewalkingMgr.h"
#include "MapUtils.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include <unordered_map>

namespace Lorewalking::Scripts
{
enum Spells
{
    SPELL_EXIT_LOREWALKING_CONVERSATION = 1271818,
    SPELL_TELEPORT_TO_BASE_DUMMY        = 1239389,
    SPELL_SUMMON_AND_SIT_IN_BENCH       = 466322,
    SPELL_NO_CHO                        = 467455,
};

// 845 - Lorewalking Campaigns
class playerchoice_lorewalking : public PlayerChoiceScript
{
public:
    playerchoice_lorewalking() : PlayerChoiceScript("playerchoice_lorewalking") { }

    void OnResponse(WorldObject* /*object*/, Player* player, PlayerChoice const* /*choice*/, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        Story const* story = GetStoryByResponse(response->ResponseId);
        if (!story)
            return;

        // Start Over drops the parked progress, the launch then begins the story fresh
        if (uint32(response->ResponseId) == story->ResponseStartOver)
            player->RemoveParkedLorewalkingQuests(story->ID);

        player->CastSpell(player, story->LaunchSpellID, TRIGGERED_FULL_MASK);
    }
};

// Spells whose only job is a DUMMY effect casting the next spell of a captured chain (OriginalCast on the wire)
// 467482 - Launch Xal'atath
// 468532 - Launch Ethereals
// 471657 - Launch Lich King
// 467592 - Launch Elves
// 1258364 - Launch Loa
// 1239389 - [DNT] Lorewalking Teleport to Base
// 467524 - [DNT] Complete
// 1275636 - Teleport
// 473059 - Teleport
class spell_lorewalking_cast_next : public SpellScript
{
    static inline std::unordered_map<uint32, uint32> const NextSpell =
    {
        { 467482, 463926 },
        { 468532, 468534 },
        { 471657, 471655 },
        { 467592, 467591 },
        { 1258364, 1258363 },
        { 1239389, 1239362 },
        { 467524, 460937 },
        { 1275636, 1275602 }, // captured while the Sunwell objective was open; the other catch-up teleports 1275600-1275605 are not mapped
        { 473059, 1239389 },
    };

    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 const* next = Trinity::Containers::MapGetValuePtr(NextSpell, spellInfo->Id);
        return next && ValidateSpellInfo({ *next });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        target->CastSpell(target, NextSpell.at(GetSpellInfo()->Id), GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_cast_next::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Starts the story: parks the normal quest log, brings back the story's parked quests or grants its first quest.
// Returns true when the story is continued rather than begun.
bool EnterStory(Player* player, Story const& story)
{
    bool resuming = player->HasParkedLorewalkingQuests(story.ID);

    player->StartLorewalking(story.ID);
    player->CastSpell(player, SPELL_SUMMON_AND_SIT_IN_BENCH, TRIGGERED_FULL_MASK);

    if (!resuming)
        if (Quest const* quest = sObjectMgr->GetQuestTemplate(story.FirstQuestID))
            if (player->CanAddQuest(quest, true) && player->CanTakeQuest(quest, true))
                player->AddQuestAndCheckCompletion(quest, nullptr);

    return resuming;
}

// 463926 - Launch Xal'atath
// 468534 - Launch Ethereals
// 471655 - Launch Lich King
// 467591 - Launch Elves
class spell_lorewalking_enter_story : public SpellScript
{
    bool Load() override
    {
        return GetCaster()->IsPlayer() && GetStoryByEnterSpell(GetSpellInfo()->Id) != nullptr;
    }

    // retail: continuing a story does not replay its intro timeline scenes (the trigger effect runs at launch, before the dummy)
    void PreventIntroOnResume(SpellEffIndex effIndex)
    {
        Story const* story = GetStoryByEnterSpell(GetSpellInfo()->Id);
        Player* player = GetCaster()->ToPlayer();
        if (GetEffectInfo().TriggerSpell == story->IntroSpellID && player->HasParkedLorewalkingQuests(story->ID))
            PreventHitDefaultEffect(effIndex);
    }

    void HandleEnter(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitUnit()->ToPlayer())
            EnterStory(player, *GetStoryByEnterSpell(GetSpellInfo()->Id));
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_lorewalking_enter_story::PreventIntroOnResume, EFFECT_ALL, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectLaunchTarget += SpellEffectFn(spell_lorewalking_enter_story::PreventIntroOnResume, EFFECT_ALL, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_enter_story::HandleEnter, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1258361 - Launch Loa
class spell_lorewalking_enter_story_cast_intro : public SpellScript
{
    bool Load() override
    {
        return GetCaster()->IsPlayer() && GetStoryByEnterSpell(GetSpellInfo()->Id) != nullptr;
    }

    void HandleEnter(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        Story const& story = *GetStoryByEnterSpell(GetSpellInfo()->Id);
        if (!EnterStory(player, story) && story.IntroSpellID)
            player->CastSpell(player, story.IntroSpellID, GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_enter_story_cast_intro::HandleEnter, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 463941 - [DNT] Teleport to Tirisfal
// Retail 12.1 (capture 69497): the only chapter entry without a teleport effect; the server queues the player for the one-player
// scenario dungeon 1381 as damage dealer, the LFG proposal completes itself and moves the player into Blade in Twilight.
class spell_lorewalking_teleport_to_tirisfal : public SpellScript
{
    static constexpr uint32 LFG_DUNGEON_BLADE_IN_TWILIGHT = 1381;

    void HandleJoin(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        lfg::LfgDungeonSet dungeons = { LFG_DUNGEON_BLADE_IN_TWILIGHT };
        sLFGMgr->JoinLfg(player, lfg::PLAYER_ROLE_DAMAGE, dungeons);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_teleport_to_tirisfal::HandleJoin, EFFECT_0, SPELL_EFFECT_KILL_CREDIT);
    }
};

// 460937 - Exit Lorewalking
class spell_lorewalking_exit : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EXIT_LOREWALKING_CONVERSATION, SPELL_TELEPORT_TO_BASE_DUMMY });
    }

    void HandleExit()
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player || !player->GetLorewalkingStoryId())
            return;

        // the effects already removed the Lorewalking auras; park the story's quests and bring the normal quest log back
        player->StopLorewalking();

        player->CastSpell(player, SPELL_EXIT_LOREWALKING_CONVERSATION, GetSpell());
        player->CastSpell(player, SPELL_TELEPORT_TO_BASE_DUMMY, GetSpell());
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_lorewalking_exit::HandleExit);
    }
};

// 1239378 - [DNT] Lorewalking Teleport to Base (arrival at Li Li's bench)
class spell_lorewalking_teleport_to_base_arrival : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_NO_CHO, SPELL_SUMMON_AND_SIT_IN_BENCH });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        player->CastSpell(player, SPELL_NO_CHO, GetSpell());

        // still in a story: back on the bench
        if (player->IsLorewalking())
            player->CastSpell(player, SPELL_SUMMON_AND_SIT_IN_BENCH, GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_teleport_to_base_arrival::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 468124 - Ask a Question
class spell_lorewalking_ask_a_question : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        if (Story const* story = Lorewalking::GetStory(player->GetLorewalkingStoryId()))
            if (story->AskQuestionSpellID)
                player->CastSpell(player, story->AskQuestionSpellID, GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lorewalking_ask_a_question::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};
}

void AddSC_lorewalking()
{
    using namespace Lorewalking::Scripts;

    new playerchoice_lorewalking();

    RegisterSpellScript(spell_lorewalking_cast_next);
    RegisterSpellScript(spell_lorewalking_enter_story);
    RegisterSpellScript(spell_lorewalking_enter_story_cast_intro);
    RegisterSpellScript(spell_lorewalking_teleport_to_tirisfal);
    RegisterSpellScript(spell_lorewalking_exit);
    RegisterSpellScript(spell_lorewalking_teleport_to_base_arrival);
    RegisterSpellScript(spell_lorewalking_ask_a_question);
}
