// Apl_Baseline_Shaman.cpp — baseline rotation for class CLASS_SHAMAN (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

constexpr uint32 LIGHTNING_BOLT    = 188196;       // L1 — core caster filler (also wago 318044 rank, share knows_spell)
constexpr uint32 LIGHTNING_BOLT_R2 = 318044;       // L6 alt rank — kept as candidate (knows_spell gates)
constexpr uint32 PRIMAL_STRIKE     = 73899;        // L2 — single-target melee opener
constexpr uint32 FLAME_SHOCK_NEW   = 470411;       // L3 — Midnight ID (preferred)
constexpr uint32 FLAME_SHOCK_OLD   = 188389;       // legacy — fallback if 470411 isn't known
constexpr uint32 EARTH_SHOCK       = 8042;         // L10 — elemental damage / single-target spender
constexpr uint32 WIND_SHEAR        = 57994;        // L12 — interrupt
constexpr uint32 EARTHBIND_TOTEM   = 2484;         // L5 — AoE slow (avoid in dungeons — totems pull adds)
constexpr uint32 LIGHTNING_SHIELD  = 192106;       // L9 — self-reactive damage buff
constexpr uint32 HEALING_SURGE     = 8004;         // self-heal
constexpr uint32 ASTRAL_SHIFT      = 108271;       // 40% dmg reduction, 8s, 90s CD

// Flame Shock candidate list — prefer the new Midnight ID (470411). Fall
// back to legacy (188389) when only the old spell is known. Some
// installs / talents may grant one but not the other; checking both
// keeps the baseline rotation correct across DB2 revisions.
constexpr uint32 FLAME_SHOCK_IDS[] = { FLAME_SHOCK_NEW, FLAME_SHOCK_OLD };

uint32 PickFlameShockId(ApPredicateContext const& ctx)
{
    for (uint32 sid : FLAME_SHOCK_IDS)
        if (ctx.bot.knows_spell(sid)) return sid;
    return 0;
}

// ---- Survival: Astral Shift (defensive CD, soak heavy hits) ----
// 40% damage reduction for 8s on a 90s CD. Healing Surge is a heal —
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

// ---- Debuff: Flame Shock (candidate list — prefer 470411, fall back 188389) ----
bool ShouldFlameShock(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    uint32 sid = PickFlameShockId(ctx);
    if (!sid) return false;
    if (!ctx.bot.is_ready(sid)) return false;
    // Skip if ANY of the candidate auras is already up on the victim
    // (some servers carry both IDs simultaneously; treat them as one).
    for (uint32 cid : FLAME_SHOCK_IDS)
        if (ctx.bot.find_aura(cid, ctx.bot.victim()) != nullptr) return false;
    return true;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 sid = PickFlameShockId(ctx);
    if (sid) e.cast(sid, ctx.bot.victim());
}

// ---- AoE control: Earthbind Totem (slow) ----
// Only in open world — totems aggro adds in dungeons, which is a net
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

// ---- Damage: Earth Shock (Elemental spender — level 10) ----
BASELINE_SPELL_RULE(EarthShock, EARTH_SHOCK)

// ---- Damage: Lightning Bolt (caster filler) ----
// Two ranks exist on wago.tools (L1 188196, L6 318044). Prefer the L1
// ID and fall back to the rank-2 ID only if the bot doesn't know the
// L1 one. In practice both share the same spec rotations and only one
// is taught, but checking both is cheap and correct.
bool ShouldLightningBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(LIGHTNING_BOLT) || ctx.bot.knows_spell(LIGHTNING_BOLT_R2);
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    uint32 sid = ctx.bot.knows_spell(LIGHTNING_BOLT)    ? LIGHTNING_BOLT
               : ctx.bot.knows_spell(LIGHTNING_BOLT_R2) ? LIGHTNING_BOLT_R2
               : 0;
    if (sid) e.cast(sid, ctx.bot.victim());
}

ApRule const baseline_shaman_kRules[] = {
    { ShouldAstralShift,      DoAstralShift,      "Astral Shift (<40% dmg red)"   },
    { ShouldHealingSurgeSelf, DoHealingSurgeSelf, "Healing Surge (<=50% self)"    },
    { ShouldLightningShield,  DoLightningShield,  "Lightning Shield (self buff)"  },
    { ShouldWindShear,        DoWindShear,        "Wind Shear (interrupt)"        },
    { ShouldFlameShock,       DoFlameShock,       "Flame Shock (DoT, 470411>188389)"},
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
