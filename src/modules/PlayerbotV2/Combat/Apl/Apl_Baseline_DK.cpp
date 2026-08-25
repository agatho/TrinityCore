// Apl_Baseline_Dk.cpp — baseline rotation for class CLASS_DEATH_KNIGHT (spec=0).
// DKs start at L8 in Midnight, so this baseline only really covers L8-9
// starter + corrupt-spec fallback. Most defensives/utility (AMS L14,
// Icebound Fortitude L38, Lichborne) are knows_spell-gated so the same
// rule list works for any DK that ends up on spec=0.
//
// See Apl_Baseline_Common.h for the shared rule macros.
//
// To audit coverage:
//   python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py
//
// Festering Wound (197147) is intentionally OMITTED — it's a passive
// proc-driven debuff layered by Unholy spec abilities, not a baseline
// cast. Runeforging (53428) and the Rune of Razorice / Fallen Crusader /
// Stoneskin Gargoyle weapon enchant spells are also OMITTED — they're
// out-of-combat weapon-enchant casts, not combat rotation choices.

#include "Apl_Baseline_Common.h"

#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::HasLiveTarget;
using ::Playerbot::Combat::baseline_common::AlwaysInCombat;
using ::Playerbot::Combat::baseline_common::DoAutoAttack;

constexpr uint32 DEATH_COIL          = 47541;    // L2  — Unholy spender, ranged ST damage
constexpr uint32 DEATH_STRIKE        = 49998;    // L4  — DK signature heal+strike
constexpr uint32 DEATH_GRIP          = 49576;    // L5  — taunt/pull
constexpr uint32 MIND_FREEZE         = 47528;    // L7  — interrupt
constexpr uint32 ANTI_MAGIC_SHELL    = 48707;    // L14 — magic damage soak
constexpr uint32 RUNE_STRIKE         = 316239;   // L1 Frost — basic ST attack
constexpr uint32 DARK_COMMAND        = 56222;    // L? — single-target taunt
constexpr uint32 DEATH_AND_DECAY     = 43265;    // L3  — AoE ground effect
constexpr uint32 LICHBORNE           = 49039;    // L9  — panic CD (charm/fear/sleep immune + heals on Death Coil self-cast)
constexpr uint32 DEATHS_ADVANCE      = 48265;    // L9  — sprint + snare removal
constexpr uint32 ICEBOUND_FORTITUDE  = 48792;    // L38 — 30% dmg reduction, stun break

BASELINE_SPELL_RULE(DeathCoil,    DEATH_COIL)
BASELINE_SPELL_RULE(DeathStrike,  DEATH_STRIKE)
BASELINE_INTERRUPT_RULE(MindFreeze, MIND_FREEZE)
BASELINE_SPELL_RULE(RuneStrike,   RUNE_STRIKE)

BASELINE_DEFENSIVE_RULE(AntiMagicShell, ANTI_MAGIC_SHELL, 60)

// Death and Decay — AoE ground effect. ≥2 nearby enemies within 8y
// (DnD's own radius). cast_at the victim's footprint when we have a
// victim_info, otherwise self-cast (places the patch under the bot).
bool ShouldDeathAndDecay(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_AND_DECAY)) return false;
    if (!ctx.bot.is_ready(DEATH_AND_DECAY)) return false;
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoDeathAndDecay(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(DEATH_AND_DECAY, v->x, v->y, v->z);
    else
        e.cast(DEATH_AND_DECAY, ObjectGuid::Empty);
}

// Death Grip — pull a target out of melee. Fires when the bot has a
// victim that's beyond melee reach (≥10y) so we close the gap by
// pulling rather than running. Also acts as a baseline soft-taunt for
// Blood-spec'd-but-fallen-through-to-baseline tanks.
bool ShouldDeathGrip(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEATH_GRIP)) return false;
    if (!ctx.bot.is_ready(DEATH_GRIP)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    // Only pull when the target is out of melee range (>10y).
    // NearbyUnit doesn't carry a precomputed distance, so derive it
    // from the bot/victim positions.
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    const float dx = v->x - bx, dy = v->y - by;
    return (dx*dx + dy*dy) > 100.0f;
}
void DoDeathGrip(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_GRIP, ctx.bot.victim());
}

// Dark Command — single-target taunt. Mostly relevant for blood-spec
// tanks but harmless on DPS specs (it only forces aggro, has no GCD-
// stealing damage effect). Fire on the first untaunted enemy.
bool ShouldDarkCommand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DARK_COMMAND)) return false;
    if (!ctx.bot.is_ready(DARK_COMMAND)) return false;
    return ctx.bot.untaunted_enemy(30.f) != nullptr;
}
void DoDarkCommand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.untaunted_enemy(30.f))
        e.cast(DARK_COMMAND, t->guid);
}

// Lichborne — panic CD. Charm/fear/sleep immunity + lets Death Coil
// self-heal (~25% over 10s). Fire at ≤40% HP when in combat and not
// already up. Knows_spell gate keeps it dormant for L8-9 DKs that
// haven't learned it yet.
bool ShouldLichborne(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() > 40) return false;
    if (!ctx.bot.knows_spell(LICHBORNE)) return false;
    if (!ctx.bot.is_ready(LICHBORNE)) return false;
    return !ctx.bot.has_aura(LICHBORNE);
}
void DoLichborne(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(LICHBORNE, ObjectGuid::Empty);
}

// Icebound Fortitude — 30% damage reduction + stun immunity, 2 min CD.
// AMS only mitigates magic; IBF handles the physical case. Skip when
// AMS is up AND HP > 30% (let AMS take the magic damage first; IBF for
// physical-heavy threat). Knows_spell auto-skips for low-level DKs
// (L38 in retail).
bool ShouldIceboundFortitude(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 40) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    if (ctx.bot.hp_pct() > 20 && ctx.bot.is_ready(ANTI_MAGIC_SHELL)
        && ctx.bot.knows_spell(ANTI_MAGIC_SHELL))
        return false;
    return ctx.bot.knows_spell(ICEBOUND_FORTITUDE)
        && ctx.bot.is_ready(ICEBOUND_FORTITUDE);
}
void DoIceboundFortitude(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(ICEBOUND_FORTITUDE, ObjectGuid::Empty);
}

// Death Strike at ≤70% HP — DK signature self-heal. Higher threshold
// than the usual <60% panic gate because Death Strike heals scale on
// damage taken in the last 5s; firing it proactively while still
// taking hits banks more healing than waiting until critical.
bool ShouldDeathStrikeLow(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() > 70) return false;
    return ctx.bot.knows_spell(DEATH_STRIKE);
}

// Death's Advance — sprint + snare removal. Fire when in combat and
// either snared (MECHANIC_SNARE) or the victim is well out of melee
// range (>15y) so we close the gap. Off the GCD in retail; cheap to
// fire opportunistically. Knows_spell gate keeps it dormant pre-L9.
bool ShouldDeathsAdvance(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DEATHS_ADVANCE)) return false;
    if (!ctx.bot.is_ready(DEATHS_ADVANCE)) return false;
    if (ctx.bot.has_mechanic(MECHANIC_SNARE)) return true;
    if (auto const* v = ctx.bot.victim_info())
    {
        float bx, by, bz; ctx.bot.position(bx, by, bz);
        const float dx = v->x - bx, dy = v->y - by;
        return (dx*dx + dy*dy) > 225.0f;   // ≥15y from victim
    }
    return false;
}
void DoDeathsAdvance(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(DEATHS_ADVANCE, ObjectGuid::Empty);
}

ApRule const baseline_dk_kRules[] = {
    { ShouldLichborne,         DoLichborne,         "Lichborne (<40% panic)"       },
    { ShouldAntiMagicShell,    DoAntiMagicShell,    "AMS (<60% self)"              },
    { ShouldIceboundFortitude, DoIceboundFortitude, "Icebound Fort. (<40%)"        },
    { ShouldDeathStrikeLow,    DoDeathStrike,       "Death Strike (<70% heal)"     },
    { ShouldMindFreeze,        DoMindFreeze,        "Mind Freeze (interrupt)"      },
    { ShouldDeathGrip,         DoDeathGrip,         "Death Grip (pull)"            },
    { ShouldDarkCommand,       DoDarkCommand,       "Dark Command (taunt)"         },
    { ShouldDeathAndDecay,     DoDeathAndDecay,     "Death and Decay (AoE >=2)"    },
    { ShouldRuneStrike,        DoRuneStrike,        "Rune Strike (Frost ST)"       },
    { ShouldDeathCoil,         DoDeathCoil,         "Death Coil"                   },
    { ShouldDeathsAdvance,     DoDeathsAdvance,     "Death's Advance (mobility)"   },
    { AlwaysInCombat,          DoAutoAttack,        "Auto attack"                  },
};

} // anonymous

void RegisterApl_Baseline_DK()
{
    RegisterRotation(CLASS_DEATH_KNIGHT, 0, ApRotation{baseline_dk_kRules});
}

} // namespace Playerbot::Combat
