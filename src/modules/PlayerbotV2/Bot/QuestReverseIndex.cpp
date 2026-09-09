#include "QuestReverseIndex.h"

#include "ObjectMgr.h"
#include "DB2Stores.h"
#include "CreatureData.h"

#include <algorithm>
#include <mutex>
#include <span>
#include <unordered_map>

namespace Playerbot {

namespace {

// Build artifacts. Both maps live behind the once_flag — once built, reads
// from any thread are safe (the maps are never mutated post-init).
std::unordered_map<uint32, std::vector<uint32>> g_kill_credit_aliases;
std::unordered_map<int32,  std::vector<uint32>> g_creatures_with_label;
std::vector<uint32>                              g_empty_uint32;
std::once_flag                                   g_built;

void BuildIndicesOnce()
{
    auto const& templates = sObjectMgr->GetCreatureTemplates();
    g_kill_credit_aliases.reserve(templates.size() / 8);   // ~12% of templates carry an alias

    for (auto const& [entry, ct] : templates)
    {
        // KillCredit aliasing — multi-map credit_entry -> list of source
        // entries. MAX_KILL_CREDIT slots, only non-zero ones contribute.
        for (uint32 i = 0; i < MAX_KILL_CREDIT; ++i)
        {
            uint32 const credit = ct.KillCredit[i];
            if (credit == 0 || credit == entry) continue;  // self-alias is the trivial case
            g_kill_credit_aliases[credit].push_back(entry);
        }

        // Creature labels — read via DB2Manager, keyed by CreatureDifficultyID.
        // Templates without explicit difficulty entries fall back to default
        // difficulty; sDB2Manager.GetCreatureLabels handles the lookup.
        // Walk every difficulty this template surfaces because labels are
        // per-difficulty in modern WoW (a creature can carry different
        // labels on Heroic vs Mythic).
        for (auto const& [diff_id, cd] : ct.difficultyStore)
        {
            std::span<int32 const> const labels =
                sDB2Manager.GetCreatureLabels(cd.CreatureDifficultyID);
            for (int32 lbl : labels)
                g_creatures_with_label[lbl].push_back(entry);
        }
    }

    // Sort + dedup each bucket so repeated callers get a deterministic
    // order and we don't re-list the same entry twice (a creature with
    // KillCredit[0]==KillCredit[1], which exists for some boss aliases).
    for (auto& [_, vec] : g_kill_credit_aliases)
    {
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }
    for (auto& [_, vec] : g_creatures_with_label)
    {
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }
}

} // namespace

void EnsureQuestReverseIndicesBuilt()
{
    std::call_once(g_built, BuildIndicesOnce);
}

std::vector<uint32> const& KillCreditAliasesFor(uint32 credit_entry)
{
    EnsureQuestReverseIndicesBuilt();
    auto const it = g_kill_credit_aliases.find(credit_entry);
    return it != g_kill_credit_aliases.end() ? it->second : g_empty_uint32;
}

std::vector<uint32> const& CreaturesWithLabel(int32 label)
{
    EnsureQuestReverseIndicesBuilt();
    auto const it = g_creatures_with_label.find(label);
    return it != g_creatures_with_label.end() ? it->second : g_empty_uint32;
}

} // namespace Playerbot
