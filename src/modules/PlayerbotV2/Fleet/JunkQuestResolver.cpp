#include "JunkQuestResolver.h"
#include "BotItemScorer.h"
#include "Bot/QuestReverseIndex.h"   // QuestHasObjectiveBotCannotComplete
#include "Player.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "ItemTemplate.h"
#include "LootItemType.h"
#include "Creature.h"
#include "Log.h"
#include <array>
#include <limits>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot::V2::Fleet {

namespace {

// Mutually-exclusive profession-specialization choice quests bots accept in
// profession mode but never resolve, keyed by the profession skill that gates
// them. Confirmed held by the live fleet (2026-06-15: ~589 holders each for
// the alchemy trio, ~307 for engineering). Extensible — add a group per
// profession whose spec choice quests show up in the picker_none / quest-log
// pollution census.
struct ProfSpecGroup
{
    uint16 skill;
    std::array<uint32, 4> quests;   // 0-terminated
};
constexpr ProfSpecGroup kProfSpecGroups[] = {
    { 171, {{ 29067, 29481, 29482, 0 }} },   // Alchemy: Potion / Elixir / Transmutation Master
    { 202, {{ 29475, 29476, 29477, 0 }} },   // Engineering: Goblin / Gnomish (two gnomish rows)
};

bool IsProfSpecQuest(uint32 quest_id, uint16* out_skill = nullptr)
{
    for (auto const& g : kProfSpecGroups)
        for (uint32 q : g.quests)
        {
            if (q == 0) break;
            if (q == quest_id) { if (out_skill) *out_skill = g.skill; return true; }
        }
    return false;
}

// Resolvable-junk cache (mirrors BotSnapshotBuilder's g_quest_junk_cache
// pattern): auto-push feature quests + the profession-spec choice quests.
std::unordered_map<uint32, bool> g_resolvable_cache;
std::shared_mutex                g_resolvable_mtx;

// Walk the quest's choice-reward array and pick the highest-scoring index for
// this bot (same scorer the runtime turn-in path uses). Returns 0 when there
// is at most one choice.
uint32 PickBestRewardChoice(Quest const* q, Player* bot)
{
    uint32 const n = q->GetRewChoiceItemsCount();
    if (n <= 1) return 0;
    int32 best_score = std::numeric_limits<int32>::min();
    uint32 best_idx = 0;
    for (uint32 i = 0; i < n && i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        if (q->RewardChoiceItemType[i] != LootItemType::Item) continue;
        uint32 entry = q->RewardChoiceItemId[i];
        if (!entry) continue;
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(entry);
        int32 s = ::Playerbot::Gear::ScoreItemForBot(tpl, bot);
        if (s > best_score) { best_score = s; best_idx = i; }
    }
    return best_idx;
}

// Prefer a real spawned quest-ender Creature as the reward giver so the
// reward-spell caster is the NPC, not the bot — a self-cast EffectForceCast
// reward chain can't infinite-loop when caster != target. Falls back to the
// bot (safe across RewardQuest's MailSender paths). 500yd covers a zone.
Object* ResolveRewardGiver(Quest const* q, Player* bot)
{
    auto bounds = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(q->GetQuestId());
    for (auto it = bounds.begin(); it != bounds.end(); ++it)
        if (Creature* c = bot->FindNearestCreature(it->second, 500.0f, /*alive=*/true))
            return c;
    return bot;
}

// Force a held quest to REWARDED. For auto-push feature quests this is what
// makes the fix stick (SatisfyQuestStatus rejects REWARDED → PushQuests stops
// re-granting). Mirrors StarterQuestAutocomplete's complete+reward path.
void ForceComplete(Player* bot, Quest const* q)
{
    uint32 qid = q->GetQuestId();
    Object* giver = ResolveRewardGiver(q, bot);
    if (bot->GetQuestStatus(qid) != QUEST_STATUS_COMPLETE)
        bot->CompleteQuest(qid);
    uint32 idx = PickBestRewardChoice(q, bot);
    bot->RewardQuest(q, LootItemType::Item, idx, giver, /*announce=*/false);
}

} // anonymous

bool JunkQuestResolver::IsResolvableJunk(uint32 quest_id)
{
    if (quest_id == 0) return false;
    {
        std::shared_lock<std::shared_mutex> rlock(g_resolvable_mtx);
        auto it = g_resolvable_cache.find(quest_id);
        if (it != g_resolvable_cache.end()) return it->second;
    }
    const bool result = [quest_id]() -> bool
    {
        if (IsProfSpecQuest(quest_id)) return true;
        // Impossible-for-bot quests (DK vehicle credit-proxies, pet-battle
        // objectives, NOT_SPECIFIED/unspawned credit markers) are resolved by
        // force-complete+reward, NOT abandon: most are LINEAR chain quests
        // (e.g. DK 12779 → 12800 "The Lich King's Command" near the Ebon Hold
        // exit), and abandoning one strands the bot in its starting zone. A
        // real player completes these; force-complete advances the chain + grants
        // the reward, the player-faithful resolution. See RunFor pass 1.
        if (::Playerbot::QuestHasObjectiveBotCannotComplete(quest_id)) return true;
        Quest const* q = sObjectMgr->GetQuestTemplate(quest_id);
        return q && q->IsAutoPush();
    }();
    {
        std::unique_lock<std::shared_mutex> wlock(g_resolvable_mtx);
        g_resolvable_cache[quest_id] = result;
    }
    return result;
}

JunkResolveResult JunkQuestResolver::RunFor(Player* bot)
{
    if (!bot) return {0, 0, true};

    // ---- Pass 1: force-complete (one per call) quests the bot can never finish
    // through play — auto-granted FEATURE quests AND impossible-for-bot quests
    // (DK vehicle credit-proxies, pet-battle, NOT_SPECIFIED/unspawned credit
    // markers). Both resolve the same way: CompleteQuest + RewardQuest, which
    // marks them REWARDED (stops feature re-push) and, crucially for chain
    // quests, fires RewardNextQuest so the bot ADVANCES instead of being
    // stranded. Done first because it is the dominant pollutant and the most
    // valuable fix. Abandon (idle:quest_abandon_unachievable @720) stays as a
    // fallback but is preempted by this rule (@730) for anything resolvable.
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 qid = bot->GetQuestSlotQuestId(slot);
        if (!qid) continue;
        if (bot->GetQuestStatus(qid) == QUEST_STATUS_REWARDED) continue;
        Quest const* q = sObjectMgr->GetQuestTemplate(qid);
        if (!q) continue;
        const bool autopush   = q->IsAutoPush();
        const bool impossible = ::Playerbot::QuestHasObjectiveBotCannotComplete(qid);
        if (!autopush && !impossible) continue;
        ForceComplete(bot, q);
        TC_LOG_INFO("playerbot.v2",
            "[JunkQuestResolver] {} force-completed {} quest {} ({})",
            bot->GetName(), autopush ? "auto-push feature" : "impossible-for-bot",
            qid, q->GetLogTitle());
        return {1, 0, false};   // one reward per call; caller re-fires
    }

    // ---- Pass 2: profession-specialization choice quests. Per group, if the
    // bot HAS the profession keep one (complete it) and abandon the rest; if it
    // lacks the profession, abandon all held ones. Abandons are cheap (no
    // forced save) so we can batch them, but cap reward-causing completes at
    // one per call.
    uint32 abandoned = 0;
    for (auto const& g : kProfSpecGroups)
    {
        // Collect held (non-rewarded) members of this group, in table order.
        std::vector<uint32> held;
        for (uint32 q : g.quests)
        {
            if (q == 0) break;
            QuestStatus st = bot->GetQuestStatus(q);
            if (st != QUEST_STATUS_NONE && st != QUEST_STATUS_REWARDED)
                held.push_back(q);
        }
        if (held.empty()) continue;

        const bool has_prof = bot->GetSkillValue(g.skill) > 0;
        size_t start = 0;
        if (has_prof)
        {
            // Keep the first held spec — force-complete it (grants the spec).
            if (Quest const* q = sObjectMgr->GetQuestTemplate(held[0]))
            {
                ForceComplete(bot, q);
                TC_LOG_INFO("playerbot.v2",
                    "[JunkQuestResolver] {} chose profession spec {} ({}) for skill {}, "
                    "abandoning {} alternative(s)",
                    bot->GetName(), held[0], q->GetLogTitle(), g.skill,
                    uint32(held.size() - 1));
                start = 1;   // abandon the remaining alternatives below
                // Abandon the rest before returning (cheap, no extra save).
                for (size_t i = start; i < held.size(); ++i)
                    if (bot->GetQuestStatus(held[i]) != QUEST_STATUS_NONE)
                        bot->RemoveActiveQuest(held[i]);
                return {1, uint32(held.size() - 1), false};
            }
        }
        // No profession (or template missing) — abandon every held member.
        for (uint32 q : held)
            if (bot->GetQuestStatus(q) != QUEST_STATUS_NONE)
            {
                bot->RemoveActiveQuest(q);
                ++abandoned;
            }
        if (abandoned)
        {
            TC_LOG_INFO("playerbot.v2",
                "[JunkQuestResolver] {} abandoned {} profession-spec quest(s) for "
                "unlearned skill {}", bot->GetName(), abandoned, g.skill);
            return {0, abandoned, false};
        }
    }

    return {0, 0, true};   // log is clean of resolvable junk
}

} // namespace Playerbot::V2::Fleet
