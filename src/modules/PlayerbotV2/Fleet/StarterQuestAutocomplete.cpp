#include "StarterQuestAutocomplete.h"
#include "BotItemScorer.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "ItemTemplate.h"
#include "Loot.h"
#include "LootItemType.h"
#include "Log.h"
#include "Creature.h"
#include "GameTime.h"
#include <chrono>
#include <limits>
#include <unordered_set>

namespace Playerbot::V2::Fleet {

namespace {

constexpr int kMaxRounds = 6;       // give up if quest tree won't terminate
constexpr uint8 kMaxStarterLevel = 20;  // skip quests above starter range

// Returns true if this quest belongs to a starter line we want to auto-
// complete:
//   1. Class-specific quest restricted to DK / DH / Evoker (their starter
//      questlines grant baseline class abilities the bot would otherwise
//      never learn - Death Coil, Eye Beam, Living Flame, etc.).
//   2. Race-specific quest the bot's race can take, within starter-tier
//      level. This catches racial intro lines for both core races (Tauren
//      War Stomp grant, Dwarf Stoneform grant historical chains, Forsaken
//      racial-ability quests) and the modern races (Worgen / Pandaren /
//      Goblin / Dracthyr / Earthen / all 14 allied races) whose recruitment
//      scenarios grant heritage abilities & racials. The level cap keeps us
//      out of generic mid-zone race-mask content.
bool IsStarterQuestForBot(Quest const* q, Player* bot)
{
    if (!q) return false;
    if (q->IsRepeatable() || q->IsDaily() || q->IsWeekly() || q->IsMonthly()) return false;
    if (q->IsUnavailable()) return false;
    if (q->GetLimitTime() > 0) return false;

    uint32 cls_mask = (1u << (bot->GetClass() - 1));
    uint32 quest_classes = q->GetAllowableClasses();

    // Class-specific = exactly one class bit set AND it's the bot's class.
    bool class_specific_one = (quest_classes != 0)
        && (quest_classes & cls_mask) != 0
        && ((quest_classes & (quest_classes - 1)) == 0);

    if (class_specific_one)
    {
        uint8 cls = bot->GetClass();
        return cls == CLASS_DEATH_KNIGHT
            || cls == CLASS_DEMON_HUNTER
            || cls == CLASS_EVOKER;
    }

    // Race-specific quest the bot's race can take. AllowableRaces == -1
    // (all bits set) is the DB convention for "any race may take this" -
    // that's NOT a race-specific quest, just an unrestricted one. We must
    // distinguish: a real racial intro / heritage quest has a STRICT subset
    // of races set (1-N out of all). 27k+ quests in the DB carry the
    // "any race" sentinel; if we treated all of those as race-specific,
    // the auto-completer would sweep up endgame weeklies (e.g. quest 87551
    // "Week 3: Spread the Word" with reward spell 1227073, a self-targeting
    // EffectForceCast chain that infinite-recurses to stack overflow).
    auto race_mask = q->GetAllowableRaces();
    if (race_mask.IsEmpty()) return false;            // no races at all
    if ((~race_mask).IsEmpty()) return false;         // all bits set = "any race", not race-specific
    if (!race_mask.HasRace(bot->GetRace())) return false;
    if (bot->GetLevel() > kMaxStarterLevel) return false;

    // Heritage armor questlines are race-restricted (so they pass the race
    // filter above) but their ContentTuning targets level 50+. A freshly
    // distributed L8/L14 bot would burn cycles attempting them via
    // CanTakeQuest only to be rejected at every iteration. Worse, even when
    // CanTakeQuest somehow lets a quest through (no ContentTuningEntry set,
    // GetQuestMinLevel returns 0), the heritage display spell granting a
    // transmog set could trigger the AddItemAppearance recursion (see the
    // CollectionMgr idempotency guard) and would have run if we hadn't
    // patched it. Belt-and-suspenders: reject quests whose effective min
    // level exceeds our starter cap.
    int32 quest_min_level = bot->GetQuestMinLevel(q);
    if (quest_min_level > int32(kMaxStarterLevel)) return false;

    return true;
}

// Walk the quest's choice-reward array and pick the highest-scoring index.
// Defers to Playerbot::Gear::ScoreItemForBot for the per-item scoring -
// same scorer used by the runtime quest turn-in path (PlayerbotAPI's
// ScoreQuestReward) and the gear distributor.
uint32 PickBestRewardChoice(Quest const* q, Player* bot)
{
    uint32 const n = q->GetRewChoiceItemsCount();
    if (n <= 1) return 0;

    int32 best_score = std::numeric_limits<int32>::min();
    uint32 best_idx  = 0;
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

// Locate a real spawned quest-ender Creature in the bot's neighborhood. We
// pass this as questGiver to AddQuest/RewardQuest so:
//   1. Player::RewardQuest's mail-template path gets a TYPEID_UNIT sender
//      (mail shows "from: <NPC name>" instead of "from: <self>").
//   2. The reward-spell caster is the NPC, not the bot - so a self-cast
//      EffectForceCast loop can't form (caster != target).
//
// Returns nullptr if no spawned ender exists within range. Caller falls
// back to passing the bot itself (still safe across all RewardQuest paths
// thanks to TYPEID_PLAYER handling in MailSender).
//
// 500yd range covers a typical starter zone. Heritage / cross-zone enders
// won't be found from the bot's starter spawn, but heritage quests are
// already filtered out by the level cap above, so this is fine.
Creature* FindQuestEnder(Quest const* q, Player* bot)
{
    auto bounds = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(q->GetQuestId());
    for (auto it = bounds.begin(); it != bounds.end(); ++it)
    {
        uint32 ender_entry = it->second;
        if (Creature* c = bot->FindNearestCreature(ender_entry, 500.0f, /*alive=*/true))
            return c;
    }
    return nullptr;
}

} // anonymous

RunForResult StarterQuestAutocomplete::RunFor(Player* bot)
{
    if (!bot) return {0, true};

    // Wall-clock budget. RunFor walks ~50K quest templates, picks the small
    // subset that match the bot's race/class, then iterates AddQuest +
    // CompleteQuest + RewardQuest for each. Under heavy server load (many
    // bots in setup, async DB worker saturated) individual Player::AddQuest
    // calls have been observed taking seconds - if RunFor as a whole runs
    // away, the world thread is blocked here, the freeze detector eventually
    // trips. Cap at 5s; partial completion is fine, the next setup-pipeline
    // tick re-enters RunFor and continues from where we left off (already-
    // rewarded quests are skipped).
    auto const t_start = GameTime::Now();
    constexpr auto kBudget = std::chrono::milliseconds(5000);

    auto const& templates = sObjectMgr->GetQuestTemplates();
    std::unordered_set<uint32> completed;
    uint32 total_completed = 0;
    bool changed = true;
    int rounds = 0;

    std::vector<Quest const*> candidates;
    candidates.reserve(64);
    for (auto const& [id, quest_ptr] : templates)
    {
        Quest const* q = quest_ptr.get();
        if (!IsStarterQuestForBot(q, bot)) continue;
        candidates.push_back(q);
    }

    if (candidates.empty()) return {0, true};

    while (changed && rounds < kMaxRounds)
    {
        changed = false;
        ++rounds;
        for (Quest const* q : candidates)
        {
            // Time-budget check. Bail out cleanly if we've burned 5s; the
            // remaining quests will be picked up next pipeline tick. This
            // is the safety valve against world-thread freezes when
            // AddQuest's per-objective HasCompletedObjective hash lookup
            // somehow blows up (heap pressure, criteria mgr corruption,
            // or other under-load issue). all_processed=false signals to
            // DoSetLevel that this run was incomplete, so the SetLevel
            // pipeline bit stays unset and we re-enter on the next tick.
            if (GameTime::Now() - t_start > kBudget)
            {
                TC_LOG_WARN("playerbot.v2",
                    "[StarterQuestAutocomplete] {} L{}: time budget exhausted, "
                    "stopping at {} quests completed (round {}). Will resume next tick.",
                    bot->GetName(), uint32(bot->GetLevel()),
                    total_completed, rounds);
                return {total_completed, false};
            }
            uint32 qid = q->GetQuestId();
            if (completed.count(qid)) continue;

            if (bot->GetQuestStatus(qid) == QUEST_STATUS_REWARDED)
            {
                completed.insert(qid);
                continue;
            }

            if (int32 prev = q->GetPrevQuestId(); prev > 0)
            {
                uint32 prev_id = uint32(prev);
                if (!completed.count(prev_id) &&
                    bot->GetQuestStatus(prev_id) != QUEST_STATUS_REWARDED)
                    continue;
            }

            if (!bot->CanTakeQuest(q, /*msg=*/false)) continue;

            // Prefer a real spawned quest-ender Creature - mail-reward items
            // get the proper sender NPC, and reward-spell caster != target so
            // self-cast EffectForceCast chains can't infinite-loop. Fall back
            // to the bot itself if no ender is in range; that's still safe
            // across every dereference path in Player::RewardQuest because
            // MailSender accepts TYPEID_PLAYER (mail becomes MAIL_NORMAL with
            // the bot's own GUID as sender, items still land in the mailbox).
            Object* quest_giver = FindQuestEnder(q, bot);
            if (!quest_giver) quest_giver = bot;

            bot->AddQuest(q, quest_giver);
            bot->CompleteQuest(qid);
            uint32 reward_idx = PickBestRewardChoice(q, bot);
            bot->RewardQuest(q, LootItemType::Item, reward_idx,
                             quest_giver, /*announce=*/false);

            completed.insert(qid);
            ++total_completed;
            changed = true;

            // Stop after the first completed quest. Player::RewardQuest
            // unconditionally calls SaveToDB(false) (Player.cpp:15325).
            // Without this break, RunFor would reward all candidate quests
            // in one call -> N full character saves stacking up on the same
            // row, all 12 DB workers serializing on the row lock, blocking
            // every other character DB query including a real player's
            // LoadFromDB. With one-quest-per-call the save commits cleanly
            // before the next pipeline tick re-enters; total fill takes
            // more ticks but the load is predictable. all_processed=false
            // ensures DoSetLevel keeps re-entering until the candidate
            // list is exhausted.
            return {total_completed, false};
        }
    }

    if (total_completed > 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[StarterQuestAutocomplete] {} L{} race={} cls={}: completed {} starter quests over {} rounds",
            bot->GetName(), uint32(bot->GetLevel()),
            uint32(bot->GetRace()), uint32(bot->GetClass()),
            total_completed, rounds);
    }
    // Reached the natural end of the loop (no progress this round, or
    // ran kMaxRounds). All candidate quests were processed.
    return {total_completed, true};
}

} // namespace Playerbot::V2::Fleet
