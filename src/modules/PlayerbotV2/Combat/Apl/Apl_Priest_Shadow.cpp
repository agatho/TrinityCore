// Shadow Priest - WoW 12.1.0.69587 enterprise rotation. Insanity-driven
// caster with two stacked DoTs (SW:P + VT) maintained on every nearby enemy,
// off-CD Mind Blast (Insanity generator), Shadow Word: Madness spending (the
// 12.1 Insanity spender - Devouring Plague is gone), Voidform burst (2 min
// CD, no Insanity cost in 12.1) with Void Bolt inside the window, Void
// Torrent (Voidweaver hero tree, opens an Entropic Rift) and Tentacle Slam
// (AoE damage that spreads Vampiric Touch). Self-survival via Dispersion +
// Desperate Prayer + Flash Heal + Power Word: Shield + Fade. Group utility
// via Vampiric Embrace + Purify Disease + Dispel Magic + Mass Dispel + Power
// Infusion. CC via Psychic Scream, interrupt via Silence. Multi-dot cycling
// piggybacks on the BotSnapshotBuilder enemy outbound scan (spec 258).
//
// Server-side overrides (Unit::GetCastSpellInfo resolves OVERRIDE_ACTIONBAR
// auras) mean the bot casts the BASE id and the game swaps in the talent
// version: Mind Blast -> Void Blast while an Entropic Rift is open (Void
// Blast 450405), Mind Flay -> Mind Flay: Insanity after a spender (Surge of
// Insanity 391399), Mind Flay replaces Smite (spec override). No extra rules.
//
// ---- Validated spell IDs (WoW 12.1.0.69587 SpellName.csv / kit) ----
//   17      Power Word: Shield    (L4 baseline - self absorb, 7.5s cat. CD)
//   528     Dispel Magic          (class talent [R][M] - offensive purge)
//   586     Fade                  (class talent [R][M] - threat dump)
//   589     Shadow Word: Pain     (L2 baseline - DoT)
//   2061    Flash Heal            (L3 baseline - self heal)
//   8092    Mind Blast            (class talent [R][M] - Insanity generator)
//   8122    Psychic Scream        (class talent [R][M] - AoE fear, 40s CD)
//   10060   Power Infusion        (class talent [R][M])
//   15286   Vampiric Embrace      (spec spell L25 - group lifelink)
//   15407   Mind Flay             (spec spell L10 - channel filler)
//   15487   Silence               (spec spell L26 - 40yd interrupt)
//   19236   Desperate Prayer      (class talent [R][M] - self heal)
//   32375   Mass Dispel           (class talent [R][M])
//   32379   Shadow Word: Death    (class talent [R][M] - execute)
//   34914   Vampiric Touch        (spec spell L10 - DoT)
//   47585   Dispersion            (spec spell L13 - 75% DR, 2 min CD)
//   120644  Halo                  (Archon hero talent [R][M] - Shadow ring)
//   194249  Voidform              (AURA while in Voidform - see 228260 text)
//   213634  Purify Disease        (class talent [R][M] - friendly dispel)
//   228260  Voidform              (spec talent [R][M] - enter Voidform)
//   228266  Void Bolt             (taught by 228260 - only in Voidform)
//   232698  Shadowform            (spec spell L10 - stance)
//   263165  Void Torrent          (Voidweaver hero [R] - 3s channel, 30s CD)
//   335467  Shadow Word: Madness  (spec talent [R][M] - 50 Insanity spender)
//   1227280 Tentacle Slam         (spec talent [R][M] - AoE dmg, spreads VT)
//
// ---- Skipped spells (and why) ----
//   - Devouring Plague (369128), Mind Spike (171852), Dark Ascension (391109),
//     Divine Star (122121), Mindgames (375901), Void Eruption (228361),
//     Shadow Crash (205385): not learnable by Shadow in 12.1 (removed or
//     replaced). SW: Madness / Voidform / Tentacle Slam take their slots.
//   - Shadowfiend (34433): a PASSIVE in 12.1 ("SW: Death has a chance to
//     summon a Shadowfiend") - not castable. Mindbender (1230339) is also
//     passive ("Casting Voidform summons a Mindbender"); Voidwraith (451234)
//     is a passive override of the summon.
//   - Shadow Mend (186263 / 186440): 186263 is not learnable; 186440 is a
//     passive placeholder with no cast text. Flash Heal is the self heal.
//   - Weakened Soul (6788): nothing in 12.1 applies it (PW:S uses a 7.5s
//     category cooldown) - is_ready() gates the re-cast instead.
//   - Void Volley (1242173): replaces the Voidform button inside Voidform
//     via override; the base id 228260 sits on its 2 min CD so is_ready()
//     cannot see the Volley charges. Void Bolt covers the in-Voidform slot.
//   - Void Torrent (205065) / Light's Wrath (207946) / Light of T'uure
//     (208065): legacy Artifact rows in the baseline list, no learn level.
//   - Power Word: Fortitude (21562): group buff applied by the class-buff
//     table (Bot/ClassTables.cpp), not by the combat APL.
//   - Leap of Faith (73325), Angelic Feather (121536), Dominate Mind
//     (205364), Mind Control (605), Shackle Horror (9484): positioning /
//     niche CC a DPS bot cannot use well.
//   - Single-Button Assistant (1229376): the APL is the assistant.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (WoW 12.1.0.69587, validated) ----
constexpr uint32 SHADOW_WORD_PAIN     = 589;
constexpr uint32 VAMPIRIC_TOUCH       = 34914;
constexpr uint32 MIND_BLAST           = 8092;
constexpr uint32 MIND_FLAY            = 15407;
constexpr uint32 SHADOW_WORD_MADNESS  = 335467;     // 50 Insanity spender (12.1 replacement for Devouring Plague)
constexpr uint32 VOIDFORM             = 228260;     // enter Voidform (1.5s cast, 2 min CD, no Insanity cost in 12.1)
constexpr uint32 VOID_BOLT            = 228266;     // taught by Voidform - available while in Voidform
constexpr uint32 VOIDFORM_AURA        = 194249;     // buff while in Voidform (referenced by the 228260 tooltip)
constexpr uint32 VOID_TORRENT         = 263165;     // Voidweaver hero talent - 3s channel, opens Entropic Rift
constexpr uint32 TENTACLE_SLAM        = 1227280;    // spec talent - AoE damage + applies VT to up to 3 enemies
constexpr uint32 SHADOW_WORD_DEATH    = 32379;      // execute < 20%
constexpr uint32 HALO                 = 120644;     // Archon hero talent - Shadow variant 30yd ring
constexpr uint32 POWER_INFUSION       = 10060;      // 2min self/ally haste
constexpr uint32 SHADOWFORM           = 232698;
constexpr uint32 DISPERSION           = 47585;      // 75% DR + heal, 2min CD
constexpr uint32 DESPERATE_PRAYER     = 19236;      // self heal + max HP, 90s CD
constexpr uint32 SILENCE              = 15487;      // 40yd interrupt
constexpr uint32 PSYCHIC_SCREAM       = 8122;       // 8yd fear, 40s CD
constexpr uint32 PW_SHIELD            = 17;         // self absorb (7.5s category CD)
constexpr uint32 FLASH_HEAL           = 2061;       // L3 - self heal
constexpr uint32 FADE                 = 586;        // threat dump
constexpr uint32 DISPEL_MAGIC         = 528;        // offensive purge (enemy Magic buff)
constexpr uint32 PURIFY_DISEASE       = 213634;     // friendly disease dispel (class talent)
constexpr uint32 MASS_DISPEL          = 32375;      // group Magic dispel
constexpr uint32 VAMPIRIC_EMBRACE     = 15286;      // 12s group leech CD

// Power index for Insanity. Power array layout matches the WoW 12.1
// Powers enum where POWER_INSANITY = 13 (snapshot stores display units).
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
    if (!ctx.bot.is_ready(PW_SHIELD)) return false;   // 7.5s category CD replaces Weakened Soul
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

bool ShouldDesperatePrayer(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(DESPERATE_PRAYER)) return false;
    if (!ctx.bot.is_ready(DESPERATE_PRAYER)) return false;
    return ctx.bot.hp_pct() <= 40;
}
void DoDesperatePrayer(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DESPERATE_PRAYER); }

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

// Flash Heal is the only direct self heal Shadow owns in 12.1 (Shadow Mend
// is a Discipline passive upgrade now). 1.5s cast - worth it below 45%.
bool ShouldFlashHealSelf(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(FLASH_HEAL)) return false;
    if (!ctx.bot.is_ready(FLASH_HEAL)) return false;
    return ctx.bot.hp_pct() <= 45;
}
void DoFlashHealSelf(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FLASH_HEAL, ctx.bot.raw().guid);
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

// Dispel Magic (class talent [R][M]) - cheap single-target purge of an
// enemy Magic buff. Preferred over Mass Dispel for the same job.
bool ShouldDispelMagic(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DISPEL_MAGIC)) return false;
    if (!ctx.bot.is_ready(DISPEL_MAGIC)) return false;
    return ctx.bot.target_dispellable(Playerbot::DispelType::Magic);
}
void DoDispelMagic(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DISPEL_MAGIC, ctx.bot.victim());
}

// Purify Disease (class talent [R][M]) - friendly disease cleanse; the
// only friendly dispel Shadow owns (Purify is Disc/Holy-only).
bool ShouldPurifyDisease(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(PURIFY_DISEASE)) return false;
    if (!ctx.bot.is_ready(PURIFY_DISEASE)) return false;
    if (ctx.group.dispel_candidate(Playerbot::DispelType::Disease)) return true;
    return ctx.bot.self_dispellable(Playerbot::DispelType::Disease);
}
void DoPurifyDisease(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* ds = ctx.group.dispel_candidate(Playerbot::DispelType::Disease)) { e.cast(PURIFY_DISEASE, ds->guid); return; }
    e.cast(PURIFY_DISEASE, ctx.bot.raw().guid);
}

bool ShouldMassDispel(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MASS_DISPEL)) return false;
    if (!ctx.bot.is_ready(MASS_DISPEL)) return false;
    // Only when the cheap Dispel Magic cannot do the job (unknown or on CD):
    // Mass Dispel is 2 min / 20% mana and also strips immunities. We
    // approximate "useful" by a dispellable buff on the current victim.
    if (ctx.bot.knows_spell(DISPEL_MAGIC) && ctx.bot.is_ready(DISPEL_MAGIC)) return false;
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
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoSilence(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
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
// Voidform (228260) is the 12.1 entry cast: 1.5s, 2 min CD, no Insanity
// cost. Enter once Vampiric Touch is rolling on the victim so the Void Bolt
// DoT extensions inside the window have something to extend. The Mindbender
// (1230339) and Voidwraith (451234) passives summon their pets off this cast.
bool ShouldVoidform(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(VOIDFORM)) return false;
    if (!ctx.bot.is_ready(VOIDFORM)) return false;
    if (ctx.bot.has_aura(VOIDFORM_AURA)) return false;
    return ctx.bot.find_aura(VAMPIRIC_TOUCH, ctx.bot.victim()) != nullptr;
}
void DoVoidform(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOIDFORM, ctx.bot.victim());
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

// Void Torrent (Voidweaver hero talent) - 3s channel, 30s CD, generates
// Insanity and tears open an Entropic Rift (which turns Mind Blast into
// Void Blast via server-side override). Channel needs a standing bot and
// pays best with the DoTs already ticking.
bool ShouldVoidTorrent(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(VOID_TORRENT)) return false;
    if (!ctx.bot.is_ready(VOID_TORRENT)) return false;
    if (ctx.bot.is_moving()) return false;
    return ctx.bot.find_aura(VAMPIRIC_TOUCH, ctx.bot.victim()) != nullptr;
}
void DoVoidTorrent(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(VOID_TORRENT, ctx.bot.victim());
}

// ---- Talent damage ----
// Tentacle Slam (spec talent [R][M]) - instant AoE around the victim that
// applies Vampiric Touch to up to 3 enemies (VT-less ones first) and
// generates Insanity. It is the 12.1 DoT-spreader (Shadow Crash is gone):
// fire on 2+ enemies / owner AoE pin, or whenever an enemy in range still
// lacks our VT.
bool ShouldTentacleSlam(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(TENTACLE_SLAM)) return false;
    if (!ctx.bot.is_ready(TENTACLE_SLAM)) return false;
    if (ctx.aoe_preference || ctx.bot.enemies_within(15.0f) >= 2) return true;
    return ctx.bot.enemy_without_my_aura(VAMPIRIC_TOUCH, 40.0f) != nullptr;
}
void DoTentacleSlam(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(TENTACLE_SLAM, ctx.bot.victim());
}

// Halo (Archon hero talent, Shadow variant 120644) - 1.5s cast ring, 60s
// category CD, damages enemies / heals allies / generates Insanity.
bool ShouldHalo(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HALO)) return false;
    if (!ctx.bot.is_ready(HALO)) return false;
    return true;
}
void DoHalo(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HALO); }

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
// Shadow Word: Madness (335467) - the 12.1 spender (50 Insanity, 6s DoT +
// self heal). Re-applying rolls the remaining damage into the new effect,
// so there is no clipping loss: spend freely near the 100 cap, otherwise
// wait for the current DoT to run down.
bool ShouldShadowWordMadness(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(SHADOW_WORD_MADNESS)) return false;
    if (!ctx.bot.is_ready(SHADOW_WORD_MADNESS)) return false;
    const int32 insanity = ctx.bot.power(POWER_INSANITY_IDX);
    if (insanity < 50) return false;
    if (insanity >= 85) return true;
    AuraEntry const* a = ctx.bot.find_aura(SHADOW_WORD_MADNESS, ctx.bot.victim());
    return !a || a->remaining.count() <= 2000;
}
void DoShadowWordMadness(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHADOW_WORD_MADNESS, ctx.bot.victim());
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

// Mind Flay - the only filler left in 12.1 (Mind Spike / Mind Sear are
// gone). After a spender the Surge of Insanity override turns this cast
// into Mind Flay: Insanity server-side.
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
// Order follows the Shadow decision tree: Dispersion / Desperate Prayer
// (panic) -> Fade / Psychic Scream / Flash Heal / PW: Shield (survival) ->
// Vampiric Embrace + dispels (group utility) -> Silence (interrupt) ->
// Power Infusion / Voidform / Void Torrent / Halo (burst CDs) -> Tentacle
// Slam (AoE VT spread) -> SW: Death (execute) -> SW: Madness (50+ Insanity
// spender) -> Void Bolt (in Voidform) -> Vampiric Touch -> Shadow Word:
// Pain -> Mind Blast -> Mind Flay (channel filler). Survival CDs sit above
// the damage rotor so we can break combat to heal mid-fight.
ApRule const kRules[] = {
    // ---- Panic / survival ----
    { ShouldDispersion,             DoDispersion,             "Dispersion (<=30%)"            },
    { ShouldDesperatePrayer,        DoDesperatePrayer,        "Desperate Prayer (<=40%)"      },
    { ShouldFade,                   DoFade,                   "Fade (threat dump)"            },
    { ShouldPsychicScream,          DoPsychicScream,          "Psychic Scream (panic)"        },
    { ShouldFlashHealSelf,          DoFlashHealSelf,          "Flash Heal self (<=45%)"       },
    { ShouldPowerWordShield,        DoPowerWordShield,        "Power Word: Shield"            },
    { ShouldShadowform,             DoShadowform,             "Shadowform stance"             },
    // ---- Group utility ----
    { ShouldVampiricEmbrace,        DoVampiricEmbrace,        "Vampiric Embrace (group heal)" },
    { ShouldPurifyDisease,          DoPurifyDisease,          "Purify Disease (ally)"         },
    { ShouldDispelMagic,            DoDispelMagic,            "Dispel Magic (purge)"          },
    { ShouldMassDispel,             DoMassDispel,             "Mass Dispel"                   },
    // ---- Interrupt ----
    { ShouldSilence,                DoSilence,                "Silence (interrupt)"           },
    // ---- Burst CDs ----
    { ShouldPowerInfusion,          DoPowerInfusion,          "Power Infusion (boss)"         },
    { ShouldVoidform,               DoVoidform,               "Voidform (enter)"              },
    { ShouldVoidTorrent,            DoVoidTorrent,            "Void Torrent (rift)"           },
    { ShouldHalo,                   DoHalo,                   "Halo (Archon)"                 },
    // ---- AoE DoT spread ----
    { ShouldTentacleSlam,           DoTentacleSlam,           "Tentacle Slam (VT spread)"     },
    // ---- Execute / spender ----
    { ShouldShadowWordDeath,        DoShadowWordDeath,        "Shadow Word: Death (execute)"  },
    { ShouldShadowWordMadness,      DoShadowWordMadness,      "SW: Madness (50 ins)"          },
    { ShouldVoidBolt,               DoVoidBolt,               "Void Bolt (in Voidform)"       },
    // ---- DoT maintenance + expansion ----
    { ShouldVampiricTouchPrimary,   DoVampiricTouchPrimary,   "Vampiric Touch (primary)"      },
    { ShouldShadowWordPainPrimary,  DoShadowWordPainPrimary,  "SW: Pain (primary refresh)"    },
    { ShouldVampiricTouchExpand,    DoVampiricTouchExpand,    "Vampiric Touch (expand)"       },
    { ShouldShadowWordPainExpand,   DoShadowWordPainExpand,   "SW: Pain (expand off-target)"  },
    // ---- Insanity generator (Mind Blast) ----
    { ShouldMindBlast,              DoMindBlast,              "Mind Blast"                    },
    // ---- Filler ----
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
