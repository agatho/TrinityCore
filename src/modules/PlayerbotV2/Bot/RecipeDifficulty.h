// RecipeDifficulty - Per-recipe skill-color resolver. One-time index built at
// module init from sSkillLineAbilityStore so AI rules can ask "is this recipe
// orange/yellow/green/gray for my current skill?" in O(1) without re-scanning
// the dbc on every tick.

#pragma once

#include "BotTypes.h"
#include <cstdint>

namespace Playerbot {

enum class RecipeColor : uint8_t
{
    Unknown = 0,   // recipe not present in SkillLineAbility — treat as unknown skill gain
    Orange  = 1,   // 100% chance of skill-up
    Yellow  = 2,   // diminishing chance — sweet spot for skilling
    Green   = 3,   // low chance, last useful tier
    Gray    = 4    // no skill-up possible (current_skill >= TrivialSkillLineRankHigh)
};

struct RecipeMeta
{
    uint16 skill_line_id        = 0;
    int16  min_skill_line_rank  = 0;   // recipe locked below this skill
    int16  trivial_low          = 0;   // boundary between Yellow and Green
    int16  trivial_high         = 0;   // boundary between Green and Gray
};

// Lazily builds the spell_id → RecipeMeta index on first call. Safe to call
// repeatedly. Returns nullptr when the spell is not a profession recipe.
RecipeMeta const* FindRecipeMeta(uint32 spell_id);

// Returns the recipe's color relative to the bot's current skill level. The
// thresholds in SkillLineAbility.dbc work like:
//   skill < min_skill_line_rank      → Unknown (recipe not yet learnable)
//   skill < trivial_low              → Orange  (between min and trivial_low)
//   skill < trivial_high             → Yellow  (low cap of trivial range)
//   skill < trivial_high + diff      → Green   (rough — TC uses ~25-rank window)
//   skill >= trivial_high            → Gray    (no skill-up)
// When trivial_low == trivial_high == 0 (degenerate envelope — typical for
// non-skill-up recipes like Glyph patterns or unauthored max-level recipes),
// returns Gray so skill-up rules skip it (a recipe with no envelope can never
// grant a point). Produce-item rules don't gate on color and can still fire it.
RecipeColor ResolveRecipeColor(uint32 spell_id, uint16 current_skill);

} // namespace Playerbot
