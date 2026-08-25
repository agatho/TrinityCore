#include "RecipeDifficulty.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include <mutex>
#include <unordered_map>

namespace Playerbot {

namespace {

// Spell -> latest SkillLineAbility row. A single recipe spell can appear
// multiple times in SkillLineAbility (legacy expansions / alternate skill
// lines); we keep the last-seen entry on the assumption that newer rows
// supersede older ones. For most cases this is fine — the trivial range
// is the same across duplicates.
std::unordered_map<uint32, RecipeMeta> g_index;
std::once_flag                          g_built;

void BuildIndex()
{
    g_index.reserve(sSkillLineAbilityStore.GetNumRows());
    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        SkillLineAbilityEntry const* row = sSkillLineAbilityStore.LookupEntry(i);
        if (!row) continue;
        if (row->Spell <= 0) continue;
        RecipeMeta meta;
        meta.skill_line_id       = row->SkillLine;
        meta.min_skill_line_rank = row->MinSkillLineRank;
        meta.trivial_low         = row->TrivialSkillLineRankLow;
        meta.trivial_high        = row->TrivialSkillLineRankHigh;
        g_index[uint32(row->Spell)] = meta;
    }
}

} // anonymous

RecipeMeta const* FindRecipeMeta(uint32 spell_id)
{
    std::call_once(g_built, BuildIndex);
    auto it = g_index.find(spell_id);
    return it == g_index.end() ? nullptr : &it->second;
}

RecipeColor ResolveRecipeColor(uint32 spell_id, uint16 current_skill)
{
    RecipeMeta const* m = FindRecipeMeta(spell_id);
    if (!m) return RecipeColor::Unknown;

    // PROF-P3b: degenerate / all-zero skill envelope (typical for "no skill
    // gain" recipes — glyphs, class spells routed through SkillLineAbility,
    // max-level recipes whose trivial bounds were never authored). The old
    // code returned Orange here, which every skill-up consumer reads as
    // "100% skill-up" and crafts forever even though such a recipe can NEVER
    // grant a point — burning reagents in an endless skill-less loop.
    // Return Gray so the skill-up rules skip it (they all branch on
    //   `c == RecipeColor::Gray || c == RecipeColor::Unknown`).
    // Gray (not Unknown) is correct: the recipe IS learnable/known, it simply
    // yields no skill gain. Produce-item rules that craft for the *output*
    // (not for skill points) don't gate on color, so they can still fire it.
    if (m->trivial_high == 0 && m->trivial_low == 0)
        return RecipeColor::Gray;

    const int16 skill = int16(current_skill);
    if (skill < m->min_skill_line_rank)
        return RecipeColor::Unknown;       // not yet learnable
    if (skill < m->trivial_low)
        return RecipeColor::Orange;
    if (skill < m->trivial_high)
        return RecipeColor::Yellow;

    // TC doesn't carry an explicit "green range". The classic convention is
    // ~25 ranks above trivial_high before crossing to gray. We mirror that
    // so the skilling rule still picks up green recipes for a small window.
    constexpr int16 kGreenWindow = 25;
    if (skill < int16(m->trivial_high + kGreenWindow))
        return RecipeColor::Green;
    return RecipeColor::Gray;
}

} // namespace Playerbot
