// Apl_Baseline_Evoker.cpp - baseline rotation for class CLASS_EVOKER (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py
//
// Validated IDs (WoW 12.1.0.69587, class baseline SkillLineAbility only -
// every id below is learnable by every Evoker regardless of spec):
//   361469 Living Flame       - 2s cast filler
//   362969 Azure Strike       - instant filler
//   356995 Disintegrate       - 3 Essence channel
//   357208 Fire Breath        - Empower cone (382266 is the Font of Magic
//                               variant, not castable)
//   357210 Deep Breath        - flyover AoE
//   368970 Tail Swipe         - knockback (180s)
//   355913 Emerald Blossom    - delayed AoE heal, used as a self-heal
//
// Skipped spells (and why):
//   359073 Eternity Surge / 357211 Pyre / 351338 Quell - Devastation spec
//                               talents in 12.1, not class baseline
//   363916 Obsidian Scales    - class talent, not baseline
//   374348 Renewing Blaze     - passive in 12.1 (rides on Obsidian Scales)
//   358267 Hover / 364342 Blessing of the Bronze - movement utility, no
//                               combat value for the baseline ladder
//   390386 Fury of the Aspects - L48 lust, spec rotations own it
//   361227 Return             - OOC rez, spec rotations own it

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 LIVING_FLAME    = 361469;
constexpr uint32 AZURE_STRIKE    = 362969;
constexpr uint32 DISINTEGRATE    = 356995;
constexpr uint32 FIRE_BREATH     = 357208;
constexpr uint32 DEEP_BREATH     = 357210;
constexpr uint32 TAIL_SWIPE      = 368970;
constexpr uint32 EMERALD_BLOSSOM = 355913;

BASELINE_SPELL_RULE(LivingFlame,   LIVING_FLAME)
BASELINE_SPELL_RULE(AzureStrike,   AZURE_STRIKE)
BASELINE_SPELL_RULE(Disintegrate,  DISINTEGRATE)
BASELINE_SPELL_RULE(FireBreath,    FIRE_BREATH)

// Emerald Blossom - delayed AoE heal at the target's location; the
// baseline has no heal-target logic, so it is a self-rescue only.
bool ShouldEmeraldBlossom(ApPredicateContext const& ctx)
{
    if (ctx.bot.hp_pct() >= 50) return false;
    return ctx.bot.knows_spell(EMERALD_BLOSSOM) && ctx.bot.is_ready(EMERALD_BLOSSOM);
}
void DoEmeraldBlossom(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(EMERALD_BLOSSOM, ctx.bot.raw().guid);
}

// Tail Swipe - panic knockback when swarmed in melee.
bool ShouldTailSwipe(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(TAIL_SWIPE)) return false;
    if (!ctx.bot.is_ready(TAIL_SWIPE)) return false;
    return ctx.bot.attackers_count() >= 3 && ctx.bot.hp_pct() <= 50;
}
void DoTailSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TAIL_SWIPE); }

// Deep Breath - fly-over AoE at the victim's position on 3+ packs.
bool ShouldDeepBreath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEEP_BREATH)) return false;
    if (!ctx.bot.is_ready(DEEP_BREATH)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoDeepBreath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.victim_info())
        e.cast_at(DEEP_BREATH, t->x, t->y, t->z);
    else
        e.cast(DEEP_BREATH, ctx.bot.victim());
}

ApRule const baseline_evoker_kRules[] = {
    { ShouldEmeraldBlossom,DoEmeraldBlossom,"Emerald Blossom (<50%)"  },
    { ShouldTailSwipe,     DoTailSwipe,     "Tail Swipe (3+ swarm)"   },
    { ShouldFireBreath,    DoFireBreath,    "Fire Breath (empower)"   },
    { ShouldDeepBreath,    DoDeepBreath,    "Deep Breath (3+ AoE)"    },
    { ShouldDisintegrate,  DoDisintegrate,  "Disintegrate (channel)"  },
    { ShouldAzureStrike,   DoAzureStrike,   "Azure Strike"            },
    { ShouldLivingFlame,   DoLivingFlame,   "Living Flame (filler)"   },
    { AlwaysInCombat,      DoAutoAttack,    "Auto attack"             },
};

} // anonymous

void RegisterApl_Baseline_Evoker()
{
    RegisterRotation(CLASS_EVOKER, 0, ApRotation{baseline_evoker_kRules});
}

} // namespace Playerbot::Combat
