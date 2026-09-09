// Apl_Baseline_Shaman.cpp - baseline rotation for class CLASS_SHAMAN (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
// here exclusively. See Apl_Baseline_Common.h for the
// shared helpers + rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

#include "Apl_Baseline_Common.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

// Spell IDs - WoW 12.1.0.69587 class baseline (SkillLineAbility) unless
// noted. Everything is knows_spell-gated so the fallback (spec 0 at any
// level) stays correct for a shaman that has the class-tree talents.
constexpr uint32 LIGHTNING_BOLT    = 188196;       // L1 - core caster filler
constexpr uint32 PRIMAL_STRIKE     = 73899;        // L2 - single-target melee opener
constexpr uint32 FLAME_SHOCK       = 470411;       // Midnight id (12.1); 188389 is the legacy DoT row
constexpr uint32 FLAME_SHOCK_DOT_LEGACY = 188389;  // aura-only - debuff may still land under it
constexpr uint32 EARTH_SHOCK       = 8042;         // Elemental spec choice node - fallback only
constexpr uint32 WIND_SHEAR        = 57994;        // class talent L12 - interrupt (fallback only)
constexpr uint32 EARTHBIND_TOTEM   = 2484;         // L5 - AoE slow (avoid in dungeons - totems pull adds)
constexpr uint32 LIGHTNING_SHIELD  = 192106;       // L9 - self-reactive damage buff
constexpr uint32 HEALING_SURGE     = 8004;         // L4 - self-heal
constexpr uint32 ASTRAL_SHIFT      = 108271;       // class talent L42 - 12s DR, 120s CD (fallback only)

// ---- Survival: Astral Shift (defensive CD, soak heavy hits) ----
// 40% damage reduction for 8s on a 90s CD. Healing Surge is a heal -
// this is a true mitigation. Hold Astral Shift if a Healing Surge cast
// would still top the bot off (mana available, HP not catastrophic).
bool ShouldAstralShift(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 40) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    if (ctx.bot.is_ready(HEALING_SURGE) && ctx.bot.knows_spell(HEALING_SURGE)
        && ctx.bot.power_pct(0) > 30 && ctx.bot.hp_pct() > 20)
        return false;
    return ctx.bot.knows_spell(ASTRAL_SHIFT) && ctx.bot.is_ready(ASTRAL_SHIFT);
}
void DoAstralShift(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(ASTRAL_SHIFT, ObjectGuid::Empty);
}

// ---- Self-heal: Healing Surge on self when <=50% HP ----
bool ShouldHealingSurgeSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(HEALING_SURGE)) return false;
    if (!ctx.bot.is_ready(HEALING_SURGE)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoHealingSurgeSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_SURGE, ctx.bot.raw().guid);
}

// ---- Self-buff: Lightning Shield (reactive damage buff) ----
BASELINE_SELFBUFF_RULE(LightningShield, LIGHTNING_SHIELD)

// ---- Interrupt: Wind Shear ----
BASELINE_INTERRUPT_RULE(WindShear, WIND_SHEAR)

// ---- Debuff: Flame Shock (470411; DoT may be tracked under legacy 188389) ----
bool ShouldFlameShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(FLAME_SHOCK)) return false;
    if (!ctx.bot.is_ready(FLAME_SHOCK)) return false;
    if (ctx.bot.find_aura(FLAME_SHOCK, ctx.bot.victim()) != nullptr) return false;
    return ctx.bot.find_aura(FLAME_SHOCK_DOT_LEGACY, ctx.bot.victim()) == nullptr;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLAME_SHOCK, ctx.bot.victim());
}

// ---- AoE control: Earthbind Totem (slow) ----
// Only in open world - totems aggro adds in dungeons, which is a net
// loss for the group. Gate on >=2 enemies within the totem's 10y pulse
// radius.
bool ShouldEarthbindTotem(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(EARTHBIND_TOTEM)) return false;
    if (!ctx.bot.is_ready(EARTHBIND_TOTEM)) return false;
    if (ctx.bot.is_in_dungeon()) return false;
    return ctx.bot.enemies_within(10.0f) >= 2;
}
void DoEarthbindTotem(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(EARTHBIND_TOTEM, ObjectGuid::Empty);
}

// ---- Damage: Primal Strike (Enhance melee opener) ----
BASELINE_SPELL_RULE(PrimalStrike, PRIMAL_STRIKE)

// ---- Damage: Earth Shock (Elemental spender - level 10) ----
BASELINE_SPELL_RULE(EarthShock, EARTH_SHOCK)

// ---- Damage: Lightning Bolt (caster filler) ----
// 12.1 has a single learnable Lightning Bolt (188196, L1); the old 318044
// rank-2 row is a passive and no longer a cast candidate.
bool ShouldLightningBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(LIGHTNING_BOLT);
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LIGHTNING_BOLT, ctx.bot.victim());
}

ApRule const baseline_shaman_kRules[] = {
    { ShouldAstralShift,      DoAstralShift,      "Astral Shift (<40% dmg red)"   },
    { ShouldHealingSurgeSelf, DoHealingSurgeSelf, "Healing Surge (<=50% self)"    },
    { ShouldLightningShield,  DoLightningShield,  "Lightning Shield (self buff)"  },
    { ShouldWindShear,        DoWindShear,        "Wind Shear (interrupt)"        },
    { ShouldFlameShock,       DoFlameShock,       "Flame Shock (DoT)"             },
    { ShouldEarthbindTotem,   DoEarthbindTotem,   "Earthbind Totem (2+ AoE, open) "},
    { ShouldPrimalStrike,     DoPrimalStrike,     "Primal Strike (melee opener)"  },
    { ShouldEarthShock,       DoEarthShock,       "Earth Shock"                   },
    { ShouldLightningBolt,    DoLightningBolt,    "Lightning Bolt (filler)"       },
    { AlwaysInCombat,         DoAutoAttack,       "Auto attack"                   },
};

} // anonymous

void RegisterApl_Baseline_Shaman()
{
    RegisterRotation(CLASS_SHAMAN, 0, ApRotation{baseline_shaman_kRules});
}

} // namespace Playerbot::Combat
