// Apl_Baseline_Paladin.cpp — baseline rotation for class CLASS_PALADIN (spec=0). Extracted from the monolithic Apl_Baseline.cpp on the split refactor; future edits go
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

constexpr uint32 CRUSADER_STRIKE     = 35395;
// Judgment baseline ID 20271 retained — matches Retribution + Holy
// spec rotations. Wago's L8 327977 is a Mastery-rank visual / spec
// override (Prot uses 275779 instead); both inherit the cooldown from
// 20271. Baseline keeps 20271 so unspecced + L1-9 bots fire the
// canonical instance.
constexpr uint32 JUDGMENT            = 20271;
// Word of Glory has two retail IDs in active use depending on
// patch/spec (85673 ret-flavoured, 115675 prot-flavoured). Multi-ID
// candidate list — first known wins.
constexpr uint32 WORD_OF_GLORY_IDS[] = { 115675, 85673 };
constexpr uint32 FLASH_OF_LIGHT      = 19750;
constexpr uint32 HAMMER_OF_JUSTICE   = 853;     // 60s CD, 6s stun
constexpr uint32 SHIELD_OF_RIGHTEOUS = 53600;
constexpr uint32 HAND_OF_RECKONING   = 62124;   // L9 — single-target taunt
constexpr uint32 CONSECRATION        = 26573;   // L6 — AoE ground tick (signature)
constexpr uint32 DIVINE_SHIELD       = 642;     // L1 — 10s full immunity, halves dmg done
constexpr uint32 LAY_ON_HANDS        = 633;     // L34 — full HP heal

BASELINE_SPELL_RULE(CrusaderStrike,    CRUSADER_STRIKE)
BASELINE_SPELL_RULE(Judgment,           JUDGMENT)
BASELINE_SPELL_RULE(ShieldOfRighteous,  SHIELD_OF_RIGHTEOUS)

// Hand of Reckoning (L9): single-target taunt. Fire when there is an
// untaunted enemy threatening the bot or an ally — tanks pull aggro,
// DPS/healers no-op when no taunt target exists. Knows_spell gate keeps
// L1-8 bots silent.
bool ShouldHandOfReckoning(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(HAND_OF_RECKONING)) return false;
    if (!ctx.bot.is_ready(HAND_OF_RECKONING)) return false;
    return ctx.bot.untaunted_enemy() != nullptr;
}
void DoHandOfReckoning(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* c = ctx.bot.untaunted_enemy())
        e.cast(HAND_OF_RECKONING, c->guid);
}

// Hammer of Justice (L1): 60s CD stun. Spend it on actual casters
// (interrupt via stun) or when the bot is taking a beating (≤50% HP)
// to peel pressure. Never burn it as a generic filler.
bool ShouldHammerOfJustice(ApPredicateContext const& ctx)
{
    if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HAMMER_OF_JUSTICE)) return false;
    if (!ctx.bot.is_ready(HAMMER_OF_JUSTICE)) return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    // Victim is mid-cast on an interruptible spell — top-priority use.
    if (v->is_casting && v->is_interruptible) return true;
    // Defensive peel — bot is under pressure.
    if (ctx.bot.hp_pct() <= 50 && ctx.bot.in_combat()) return true;
    return false;
}
void DoHammerOfJustice(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HAMMER_OF_JUSTICE, ctx.bot.victim());
}

// Consecration (L6): AoE ground tick, paladin signature. Fire on any
// in-combat tick when 2+ enemies are in range — lower threshold than
// the typical 3-target AoE rule because it's also a single-target
// threat tool for Prot and a passive damage source for Ret. Knows_spell
// gate keeps L1-5 bots silent.
bool ShouldConsecration(ApPredicateContext const& ctx)
{
    if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(CONSECRATION)) return false;
    if (!ctx.bot.is_ready(CONSECRATION)) return false;
    // 2+ enemies in melee/Consecration radius (8y).
    return ctx.bot.enemies_within(8.0f) >= 2;
}
void DoConsecration(ApPredicateContext const&, BotIntentEmitter& e)
{
    // Ground-targeted at the bot's feet — empty guid lets the cast
    // resolver place the AoE under the caster.
    e.cast(CONSECRATION, ObjectGuid::Empty);
}

bool ShouldWordOfGlory(ApPredicateContext const& ctx)
{
    if (ctx.bot.hp_pct() >= 60) return false;
    for (uint32 sid : WORD_OF_GLORY_IDS)
        if (ctx.bot.is_ready(sid)) return true;
    return false;
}
void DoWordOfGlory(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    for (uint32 sid : WORD_OF_GLORY_IDS)
        if (ctx.bot.is_ready(sid)) { e.cast(sid, ObjectGuid::Empty); return; }
}

// Flash of Light: 1.5s cast self-heal; baseline panic when WoG is
// unavailable / on CD. Threshold pulled below WoG so the instant
// heal is preferred when both are ready.
bool ShouldFlashOfLight(ApPredicateContext const& ctx)
{
    if (ctx.bot.hp_pct() >= 50) return false;
    if (ctx.bot.is_moving()) return false;          // cast-time spell
    return ctx.bot.knows_spell(FLASH_OF_LIGHT) && ctx.bot.is_ready(FLASH_OF_LIGHT);
}
void DoFlashOfLight(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(FLASH_OF_LIGHT, ObjectGuid::Empty);
}

// Divine Shield: 8-10s full immunity, drops aggro, halves damage done.
// L1 baseline so available immediately. Reserved for true emergencies
// (<25% HP) — burns Forbearance debuff which locks Lay on Hands. To
// avoid blowing two CDs simultaneously, skip when WoG is ready AND
// HP > 40% (let the cheap heal handle moderate dips).
bool ShouldDivineShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (ctx.bot.hp_pct() >= 25) return false;
    if (ctx.bot.enemies_within(40.0f) == 0) return false;
    // Alternation: if WoG is still up and HP isn't critical, defer to it.
    if (ctx.bot.hp_pct() > 15)
        for (uint32 sid : WORD_OF_GLORY_IDS)
            if (ctx.bot.is_ready(sid)) return false;
    return ctx.bot.knows_spell(DIVINE_SHIELD) && ctx.bot.is_ready(DIVINE_SHIELD);
}
void DoDivineShield(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(DIVINE_SHIELD, ObjectGuid::Empty);
}

// Lay on Hands: full-HP heal, 8-12 min CD. Last-resort below 15% HP
// when Divine Shield is also unavailable. L34 — knows_spell skips for
// lower levels.
bool ShouldLayOnHands(ApPredicateContext const& ctx)
{
    if (ctx.bot.hp_pct() >= 15) return false;
    return ctx.bot.knows_spell(LAY_ON_HANDS) && ctx.bot.is_ready(LAY_ON_HANDS);
}
void DoLayOnHands(ApPredicateContext const&, BotIntentEmitter& e)
{
    e.cast(LAY_ON_HANDS, ObjectGuid::Empty);
}

// Rule order (defensive ladder first, then utility, then damage):
//   1. Lay on Hands     — full HP heal, ≤15% emergency
//   2. Divine Shield    — 8s immunity, ≤25% bail
//   3. Flash of Light   — instant-when-stationary panic heal, ≤50% self
//   4. Hammer of Justice— interrupt-via-stun OR peel when bot ≤50% HP
//   5. Hand of Reckoning— single-target taunt (tanks; DPS no-op when no untaunted enemy)
//   6. Judgment         — ranged opener / debuff
//   7. Crusader Strike  — melee filler
//   8. Consecration     — AoE ground tick (2+ enemies)
//   9. Shield of the Righteous — Prot active mitigation (knows_spell gate)
//  10. Word of Glory    — Holy Power heal (≤60% self)
//  11. Auto attack      — always-on melee swing
ApRule const baseline_paladin_kRules[] = {
    { ShouldLayOnHands,       DoLayOnHands,       "Lay on Hands (<15% emergency)"   },
    { ShouldDivineShield,     DoDivineShield,     "Divine Shield (<25% immunity)"   },
    { ShouldFlashOfLight,     DoFlashOfLight,     "Flash of Light (<50% self)"      },
    { ShouldHammerOfJustice,  DoHammerOfJustice,  "Hammer of Justice (stun/peel)"   },
    { ShouldHandOfReckoning,  DoHandOfReckoning,  "Hand of Reckoning (taunt)"       },
    { ShouldJudgment,         DoJudgment,         "Judgment"                        },
    { ShouldCrusaderStrike,   DoCrusaderStrike,   "Crusader Strike"                 },
    { ShouldConsecration,     DoConsecration,     "Consecration (AoE 2+)"           },
    { ShouldShieldOfRighteous,DoShieldOfRighteous,"Shield of the Righteous (Prot)"  },
    { ShouldWordOfGlory,      DoWordOfGlory,      "Word of Glory (<60% self heal)"  },
    { AlwaysInCombat,         DoAutoAttack,       "Auto attack"                     },
};

} // anonymous

void RegisterApl_Baseline_Paladin()
{
    RegisterRotation(CLASS_PALADIN, 0, ApRotation{baseline_paladin_kRules});
}

} // namespace Playerbot::Combat
