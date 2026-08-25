// Shadow Priest - WoW 12.0 enterprise rotation. Insanity-driven caster with
// two stacked DoTs (SW:P + VT) maintained on every nearby enemy, off-CD
// Mind Blast (Insanity generator), Devouring Plague spending, and
// Voidform / Dark Ascension burst windows. Insanity (397527) is the spec's
// resource passive; Hallucinations (280752) is the passive that grants
// Insanity on enemy interrupt — neither needs an APL rule. Self-survival
// via Power Word: Shield + Shadow Mend + Dispersion + Fade. Group utility
// via Vampiric Embrace + Mass Dispel + Power Infusion. CC via Psychic
// Scream. Multi-dot cycling piggybacks on the BotSnapshotBuilder enemy
// outbound scan that already covers spec 258.
//
// ---- Validated spell IDs (WoW 12.0 SpellName.csv / SpellLevels.csv) ----
//   17     Power Word: Shield   (L4 — self-absorb)
//   586    Fade                 (threat dump)
//   589    Shadow Word: Pain    (L2 — DoT)
//   2944   (skipped — see below)
//   6788   Weakened Soul
//   8092   Mind Blast           (Insanity generator)
//   8122   Psychic Scream
//   10060  Power Infusion       (L58)
//   15286  Vampiric Embrace     (L25)
//   15407  Mind Flay            (L10 — channel filler)
//   15487  Silence              (L26 — 30yd interrupt)
//   21562  Power Word: Fortitude
//   32375  Mass Dispel
//   32379  Shadow Word: Death   (L14 — execute)
//   34433  Shadowfiend
//   34914  Vampiric Touch       (L10 — DoT)
//   47585  Dispersion           (L13 — 75% DR + mana, 2min CD)
//   73325  Leap of Faith        (L49)
//   110744 Divine Star          (Shadow Divine Star is 122121 — see below)
//   120517 Halo                 (talent — Shadow variant 120644)
//   122121 Divine Star          (Shadow variant)
//   171852 Mind Spike           (modern Shadow AoE/ST instant filler — replaced 12.0 Mind Sear)
//   186263 Shadow Mend          (L19 — self heal)
//   194249 Voidform              (passive aura while in Voidform)
//   200174 Mindbender           (talent — replaces Shadowfiend)
//   205385 Shadow Crash         (talent — ground AoE pull, applies SW:P)
//   205448 Void Bolt            (L23 — only while in Voidform)
//   228361 Void Eruption        (Voidform-enter cast; aura is 194249)
//   232698 Shadowform           (L10 stance)
//   280752 Hallucinations       (PASSIVE — Insanity on interrupt; no rule)
//   369128 Devouring Plague     (L12 — 50 Insanity spender; modern player cast)
//   375901 Mindgames            (talent)
//   391109 Dark Ascension       (talent — alternative to Voidform burst)
//   397527 Insanity             (PASSIVE — resource bar; no rule)
//
// ---- Skipped spells (and why) ----
//   - Insanity (397527) / Hallucinations (280752): both passive — no cast.
//   - Mind Bomb (205369): removed from the modern Priest kit; Psychic
//     Scream covers the AoE CC slot.
//   - Surrender to Madness (319952): removed in 12.0 — no SpellLevels row.
//   - Searing Nightmare (341385): removed — Mind Spike: Insanity covers
//     the AoE spread via Shadow Crash.
//   - Mind Sear (32000 / 48045): old IDs are deprecated; modern Shadow uses
//     Mind Spike (171852) as the instant AoE/ST filler.
//   - Void Eruption (228260): legacy "Voidform" alias; the actual cast is
//     228361. The Voidform aura that applies after the cast is 194249.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.0, validated) ----
constexpr uint32 SHADOW_WORD_PAIN   = 589;
constexpr uint32 VAMPIRIC_TOUCH     = 34914;
constexpr uint32 MIND_BLAST         = 8092;
constexpr uint32 MIND_FLAY          = 15407;
constexpr uint32 MIND_SPIKE         = 171852;     // modern Shadow AoE/ST instant filler (replaces deprecated Mind Sear)
constexpr uint32 DEVOURING_PLAGUE   = 369128;     // 50 Insanity spender (12.0 player cast — was wrongly 335467 which is "Shadow Word: Madness")
constexpr uint32 VOID_ERUPTION      = 228361;     // enter Voidform — was wrongly 228260 (which is the Voidform alias)
constexpr uint32 VOID_BOLT          = 205448;     // available in Voidform
constexpr uint32 VOIDFORM_AURA      = 194249;
constexpr uint32 DARK_ASCENSION     = 391109;     // talent — alternative burst to Voidform
constexpr uint32 SHADOW_WORD_DEATH  = 32379;      // execute < 20%
constexpr uint32 SHADOW_CRASH       = 205385;     // talent — ground AoE pull, applies SW:P
constexpr uint32 HALO               = 120517;     // talent — 30yd ring
constexpr uint32 DIVINE_STAR        = 122121;     // talent — line aoe + heal (Shadow variant)
constexpr uint32 MINDGAMES          = 375901;     // talent — reverse heal/dmg
constexpr uint32 POWER_INFUSION     = 10060;      // 2min self/ally haste
constexpr uint32 SHADOWFIEND        = 34433;
constexpr uint32 MINDBENDER         = 200174;     // talent replacement
constexpr uint32 SHADOWFORM         = 232698;
constexpr uint32 DISPERSION         = 47585;      // 75% DR + mana, 2min CD
constexpr uint32 SILENCE            = 15487;      // 30yd interrupt
constexpr uint32 PSYCHIC_SCREAM     = 8122;       // 8yd fear, 60s CD
constexpr uint32 PW_SHIELD          = 17;         // self absorb
constexpr uint32 WEAKENED_SOUL      = 6788;       // PW:S debuff
constexpr uint32 SHADOW_MEND        = 186263;     // L19 — self heal (Shadow spec ID)
constexpr uint32 FADE               = 586;        // threat dump
constexpr uint32 MASS_DISPEL        = 32375;      // group Magic dispel
constexpr uint32 VAMPIRIC_EMBRACE   = 15286;      // 15s group leech CD
constexpr uint32 LEAP_OF_FAITH      = 73325;      // pull ally
constexpr uint32 PW_FORTITUDE       = 21562;      // group buff

// Power index for Insanity. Power array layout matches the WoW 12.0
// Powers enum where POWER_INSANITY = 13.
constexpr uint8 POWER_INSANITY_IDX = 13;

bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

bool BossLikeTargetEngaged(ApPredicateContext const& ctx)
{
    constexpr int32 kBossHpThreshold = 5'000'000;
    NearbyUnit const* t = ctx.bot.victim_info();
    if (t && t->max_hp >= kBossHpThreshold) return true;
    for (auto const& a : ctx.bot.raw().combat.attackers)
        if (a.max_hp >= kBossHpThreshold) return true;
    return false;
}

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
}

// ---- Stance / buffs ----
bool ShouldShadowform(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SHADOWFORM)) return false;
    return !ctx.bot.has_aura(SHADOWFORM);
}
void DoShadowform(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOWFORM); }

bool ShouldPowerWordShield(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PW_SHIELD)) return false;
    if (!ctx.bot.is_ready(PW_SHIELD)) return false;
    if (ctx.bot.has_aura(WEAKENED_SOUL)) return false;
    if (ctx.bot.has_aura(PW_SHIELD)) return false;
    return ctx.bot.hp_pct() <= 75 || (ctx.bot.in_combat() && ctx.bot.hp_pct() <= 90);
}
void DoPowerWordShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PW_SHIELD); }

// ---- Survival ----
bool ShouldDispersion(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DISPERSION)) return false;
    if (!ctx.bot.is_ready(DISPERSION)) return false;
    return ctx.bot.hp_pct() <= 30;
}
void DoDispersion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DISPERSION); }

bool ShouldFade(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FADE)) return false;
    if (!ctx.bot.is_ready(FADE)) return false;
    // Fade if multiple attackers chose us — likely a threat problem the
    // tank hasn't recovered from yet.
    return ctx.bot.attackers_count() >= 2;
}
void DoFade(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FADE); }

bool ShouldShadowMend(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SHADOW_MEND)) return false;
    if (!ctx.bot.is_ready(SHADOW_MEND)) return false;
    return ctx.bot.hp_pct() <= 45;
}
void DoShadowMend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_MEND, ctx.bot.raw().guid);
}

// ---- Group utility ----
bool ShouldVampiricEmbrace(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VAMPIRIC_EMBRACE)) return false;
    if (!ctx.bot.is_ready(VAMPIRIC_EMBRACE)) return false;
    if (auto const* low = ctx.group.lowest_hp_on_map(ctx.bot.map_id(), Role::Unknown, ctx.bot.raw().position.x, ctx.bot.raw().position.y, ctx.bot.raw().position.z, 45.0f))
        return low->online && low->max_hp > 0 && (low->hp * 100) / low->max_hp <= 70;
    return ctx.bot.hp_pct() <= 70;
}
void DoVampiricEmbrace(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(VAMPIRIC_EMBRACE); }

bool ShouldPowerInfusion(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(POWER_INFUSION)) return false;
    if (!ctx.bot.is_ready(POWER_INFUSION)) return false;
    // Pop on boss-tier target — the haste burst window is wasted on trash.
    return BossLikeTargetEngaged(ctx);
}
void DoPowerInfusion(ApPredicateContext const&, BotIntentEmitter& e)
{
    // Self-cast: with no specific assignment the bot will benefit itself.
    e.cast(POWER_INFUSION);
}

bool ShouldMassDispel(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MASS_DISPEL)) return false;
    if (!ctx.bot.is_ready(MASS_DISPEL)) return false;
    // Trigger on nearby enemies casting interruptible Magic — Mass Dispel
    // also strips immunities. We approximate "useful" by the presence of
    // a dispellable buff on the current victim.
    if (ctx.bot.target_dispellable(Playerbot::DispelType::Magic)) return true;
    return false;
}
void DoMassDispel(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(MASS_DISPEL, v->x, v->y, v->z);
    else
        e.cast(MASS_DISPEL, ctx.bot.victim());
}

// ---- Interrupt / CC ----
bool ShouldSilence(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(SILENCE)) return false;
    if (!ctx.bot.is_ready(SILENCE)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 30.0f) != nullptr;
}
void DoSilence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 30.0f))
        e.cast(SILENCE, c->guid);
}

bool ShouldPsychicScream(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PSYCHIC_SCREAM)) return false;
    if (!ctx.bot.is_ready(PSYCHIC_SCREAM)) return false;
    // Personal panic button — at least 2 in melee and we're hurting.
    return ctx.bot.enemies_within(8.0f) >= 2 && ctx.bot.hp_pct() <= 50;
}
void DoPsychicScream(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PSYCHIC_SCREAM); }

// ---- Major offensive cooldowns ----
bool ShouldVoidEruption(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_ERUPTION)) return false;
    if (!ctx.bot.is_ready(VOID_ERUPTION)) return false;
    if (ctx.bot.has_aura(VOIDFORM_AURA)) return false;
    // Need 60 Insanity for entry; gate on resource so we don't wait-cast.
    return ctx.bot.power(POWER_INSANITY_IDX) >= 60;
}
void DoVoidEruption(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOID_ERUPTION, ctx.bot.victim());
}

bool ShouldVoidBolt(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_BOLT)) return false;
    if (!ctx.bot.is_ready(VOID_BOLT)) return false;
    return ctx.bot.has_aura(VOIDFORM_AURA);
}
void DoVoidBolt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOID_BOLT, ctx.bot.victim());
}

// Dark Ascension — talent burst alternative to Voidform. Same priority slot
// as Void Eruption: gate on a live target and not already in Voidform.
bool ShouldDarkAscension(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DARK_ASCENSION)) return false;
    if (!ctx.bot.is_ready(DARK_ASCENSION)) return false;
    if (ctx.bot.has_aura(VOIDFORM_AURA)) return false;
    return true;
}
void DoDarkAscension(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DARK_ASCENSION, ctx.bot.victim());
}

bool ShouldShadowfiend(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOWFIEND)) return false;
    return ctx.bot.is_ready(SHADOWFIEND);
}
void DoShadowfiend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOWFIEND, ctx.bot.victim());
}

bool ShouldMindbender(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MINDBENDER)) return false;
    return ctx.bot.is_ready(MINDBENDER);
}
void DoMindbender(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MINDBENDER, ctx.bot.victim());
}

// ---- Talent damage ----
bool ShouldShadowCrash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_CRASH)) return false;
    if (!ctx.bot.is_ready(SHADOW_CRASH)) return false;
    // Apply SW:P AoE — value scales with enemy count near the impact zone.
    return ctx.aoe_preference || ctx.bot.enemies_within(15.0f) >= 2;
}
void DoShadowCrash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(SHADOW_CRASH, v->x, v->y, v->z);
    else
        e.cast(SHADOW_CRASH, ctx.bot.victim());
}

bool ShouldHalo(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HALO)) return false;
    if (!ctx.bot.is_ready(HALO)) return false;
    return true;
}
void DoHalo(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HALO); }

bool ShouldDivineStar(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIVINE_STAR)) return false;
    if (!ctx.bot.is_ready(DIVINE_STAR)) return false;
    return true;
}
void DoDivineStar(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(DIVINE_STAR, v->x, v->y, v->z);
    else
        e.cast(DIVINE_STAR);
}

bool ShouldMindgames(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MINDGAMES)) return false;
    if (!ctx.bot.is_ready(MINDGAMES)) return false;
    return true;
}
void DoMindgames(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MINDGAMES, ctx.bot.victim());
}

// ---- Execute ----
bool ShouldShadowWordDeath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_DEATH)) return false;
    if (!ctx.bot.is_ready(SHADOW_WORD_DEATH)) return false;
    return TargetExecuteRange(ctx);
}
void DoShadowWordDeath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_DEATH, ctx.bot.victim());
}

// ---- Insanity spender ----
bool ShouldDevouringPlague(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DEVOURING_PLAGUE)) return false;
    if (!ctx.bot.is_ready(DEVOURING_PLAGUE)) return false;
    if (ctx.bot.power(POWER_INSANITY_IDX) < 50) return false;
    // Refresh DP on victim only when missing or near pandemic window.
    AuraEntry const* a = ctx.bot.find_aura(DEVOURING_PLAGUE, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoDevouringPlague(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEVOURING_PLAGUE, ctx.bot.victim());
}

// ---- DoT primary + multi-target expand ----
bool ShouldShadowWordPainPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN)) return false;
    AuraEntry const* a = ctx.bot.find_aura(SHADOW_WORD_PAIN, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoShadowWordPainPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_PAIN, ctx.bot.victim());
}

bool ShouldVampiricTouchPrimary(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VAMPIRIC_TOUCH)) return false;
    AuraEntry const* a = ctx.bot.find_aura(VAMPIRIC_TOUCH, ctx.bot.victim());
    return !a || a->remaining.count() <= 3000;
}
void DoVampiricTouchPrimary(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VAMPIRIC_TOUCH, ctx.bot.victim());
}

bool ShouldShadowWordPainExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_PAIN)) return false;
    return ctx.bot.enemy_without_my_aura(SHADOW_WORD_PAIN, 40.0f) != nullptr;
}
void DoShadowWordPainExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(SHADOW_WORD_PAIN, 40.0f))
        e.cast(SHADOW_WORD_PAIN, off->guid);
}

bool ShouldVampiricTouchExpand(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VAMPIRIC_TOUCH)) return false;
    return ctx.bot.enemy_without_my_aura(VAMPIRIC_TOUCH, 40.0f) != nullptr;
}
void DoVampiricTouchExpand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* off = ctx.bot.enemy_without_my_aura(VAMPIRIC_TOUCH, 40.0f))
        e.cast(VAMPIRIC_TOUCH, off->guid);
}

// ---- Filler / generators ----
bool ShouldMindBlast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MIND_BLAST)) return false;
    return ctx.bot.is_ready(MIND_BLAST);
}
void DoMindBlast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MIND_BLAST, ctx.bot.victim());
}

// Mind Spike — modern instant-cast generator that replaces the deprecated
// Mind Sear channel. Prefer it over Mind Flay when moving or when an AoE
// preference is signalled (the spell also functions as an instant ST tool
// when MIND_BLAST is on CD).
bool ShouldMindSpike(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(MIND_SPIKE)) return false;
    if (!ctx.bot.is_ready(MIND_SPIKE)) return false;
    // Prefer Mind Spike when we have to move (Mind Flay is a channel that
    // breaks on movement) or when the user/encounter signals AoE.
    if (ctx.bot.is_moving()) return true;
    if (ctx.aoe_preference) return true;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoMindSpike(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MIND_SPIKE, ctx.bot.victim());
}

bool ShouldMindFlay(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    return ctx.bot.knows_spell(MIND_FLAY);
}
void DoMindFlay(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(MIND_FLAY, ctx.bot.victim());
}

bool AlwaysAlive(ApPredicateContext const& ctx) { return ctx.bot.is_alive(); }
void DoNothing(ApPredicateContext const&, BotIntentEmitter&) {}

// ---- Rule table (priority order top-down) ----
// Order follows the Shadow decision tree: Dispersion (panic <=30%) →
// Vampiric Embrace (group heal CD) → Silence (interrupt) → Voidform /
// Dark Ascension (CD burst) → Devouring Plague (Insanity 50+ spender) →
// Vampiric Touch → Shadow Word: Pain → Mind Blast → Mind Flay (channel
// filler). Survival CDs (Shadow Mend / PW: Shield / Fade) sit above the
// damage rotor so we can break combat to heal mid-fight.
ApRule const kRules[] = {
    // ---- Panic / survival ----
    { ShouldDispersion,             DoDispersion,             "Dispersion (<=30%)"            },
    { ShouldFade,                   DoFade,                   "Fade (threat dump)"            },
    { ShouldPsychicScream,          DoPsychicScream,          "Psychic Scream (panic)"        },
    { ShouldShadowMend,             DoShadowMend,             "Shadow Mend (<=45%)"           },
    { ShouldPowerWordShield,        DoPowerWordShield,        "Power Word: Shield"            },
    { ShouldShadowform,             DoShadowform,             "Shadowform stance"             },
    // ---- Group utility ----
    { ShouldVampiricEmbrace,        DoVampiricEmbrace,        "Vampiric Embrace (group heal)" },
    { ShouldMassDispel,             DoMassDispel,             "Mass Dispel"                   },
    // ---- Interrupt ----
    { ShouldSilence,                DoSilence,                "Silence (interrupt)"           },
    // ---- Burst CDs ----
    { ShouldPowerInfusion,          DoPowerInfusion,          "Power Infusion (boss)"         },
    { ShouldVoidEruption,           DoVoidEruption,           "Void Eruption (228361)"        },
    { ShouldDarkAscension,          DoDarkAscension,          "Dark Ascension (talent burst)" },
    { ShouldMindbender,             DoMindbender,             "Mindbender"                    },
    { ShouldShadowfiend,            DoShadowfiend,            "Shadowfiend"                   },
    { ShouldShadowCrash,            DoShadowCrash,            "Shadow Crash (2+ AoE)"         },
    // ---- Execute / spender ----
    { ShouldShadowWordDeath,        DoShadowWordDeath,        "Shadow Word: Death (execute)"  },
    { ShouldDevouringPlague,        DoDevouringPlague,        "Devouring Plague (50 ins)"     },
    { ShouldVoidBolt,               DoVoidBolt,               "Void Bolt (in Voidform)"       },
    // ---- DoT maintenance + expansion ----
    { ShouldVampiricTouchPrimary,   DoVampiricTouchPrimary,   "Vampiric Touch (primary)"      },
    { ShouldShadowWordPainPrimary,  DoShadowWordPainPrimary,  "SW: Pain (primary refresh)"    },
    { ShouldVampiricTouchExpand,    DoVampiricTouchExpand,    "Vampiric Touch (expand)"       },
    { ShouldShadowWordPainExpand,   DoShadowWordPainExpand,   "SW: Pain (expand off-target)"  },
    // ---- Insanity generator (Mind Blast) ----
    { ShouldMindBlast,              DoMindBlast,              "Mind Blast"                    },
    // ---- Talent damage ----
    { ShouldHalo,                   DoHalo,                   "Halo"                          },
    { ShouldDivineStar,             DoDivineStar,             "Divine Star"                   },
    { ShouldMindgames,              DoMindgames,              "Mindgames"                     },
    // ---- Filler (Mind Spike > Mind Flay when moving / AoE) ----
    { ShouldMindSpike,              DoMindSpike,              "Mind Spike (instant filler)"   },
    { ShouldMindFlay,               DoMindFlay,               "Mind Flay (channel filler)"    },
    { AlwaysAlive,                  DoNothing,                "Idle"                          },
};

} // anonymous

void RegisterApl_Priest_Shadow()
{
    constexpr uint32 SPEC_PRIEST_SHADOW = 258;
    RegisterRotation(CLASS_PRIEST, SPEC_PRIEST_SHADOW, ApRotation{kRules});
}

} // namespace Playerbot::Combat
