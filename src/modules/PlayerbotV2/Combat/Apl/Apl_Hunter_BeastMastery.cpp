// Hunter Beast Mastery - WoW 12.0 enterprise rotation. Pet-driven ranged
// DPS with Frenzy uptime via Barbed Shot, Bestial Wrath / Aspect of the
// Wild burst, and Kill Command focus economy. Multi-target via Multi-Shot
// (Beast Cleave) + Stomp talent.
//
// Leveling-bracket aware (rework 2026-06-10, live-verified on Uraimus L13):
// before this rework a low-level BM bot cast ONLY Steady Shot between Kill
// Commands — no focus spender existed in the rule list for the pre-Cobra
// bracket (Arcane Shot was missing entirely), so focus sat capped while
// Steady kept over-generating. Every rule gates on knows_spell, so one
// rule list serves L10 through max level:
//   * Arcane Shot is the focus dump until Cobra Shot is learned, then
//     Cobra takes over (Arcane rule self-disables once Cobra is known).
//   * Spenders fire only with >=30 focus headroom left AFTER the cast so
//     Kill Command — the highest damage-per-focus BM button — is never
//     starved by a filler.
//   * Multi-Shot is cast to ACTIVATE/refresh Beast Cleave (pet aura
//     118455), not spammed every AoE GCD.
//   * Bestial Wrath is a leveling workhorse (90s CD vs 15s kills): fires
//     on meaty single targets and 2+ pulls, not just raid bosses.
// Ranged white damage (Auto Shot 75) is armed by API::start_attack, not
// by an APL rule — see PlayerbotAPI.cpp ensureAutoShot.
//
// Layered survival: Aspect of the Turtle -> Exhilaration -> Survival of the
// Fittest -> Disengage / Feign Death. Group utility: Misdirection (tank
// threat), Primal Rage (pet-cast Bloodlust, Ferocity pets). Pet upkeep:
// Mend Pet, Revive Pet, Call Pet. CC: Counter Shot, Intimidation (pet
// stun), Binding Shot. Major CDs: Bestial Wrath, Bloodshed, Dire Beast
// (12.0 removed Aspect of the Wild / Call of the Wild / Stampede).
//
// IDs re-validated against wago.tools DB2 exports, build 12.0.5.67823
// (2026-06-10). Every ID below resolves to a live learn path.
//
// Skipped spec spells (not rotation-relevant — intentional omissions):
//   * Track Pets         (1244920) — minimap-utility, never a damage cast.
//   * Exotic Beasts      (   53270) — passive (unlocks tameable families).
//   * Eyes of the Beast  (  321297) — pet-vision toy, breaks bot AI control.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

// ---- Spell IDs (re-validated against wago.tools DB2 exports for retail
// build 12.0.5.67823 on 2026-06-10; 12.0 removed/replaced several legacy
// BM buttons — see each entry) ----
constexpr uint32 KILL_SHOT          = 53351;        // MM-only in 12.0; BM keeps the rule but knows_spell gates it off
constexpr uint32 BARBED_SHOT        = 217200;       // BM spec talent (~3rd spec point); OVERRIDES Steady Shot on the bar
constexpr uint32 KILL_COMMAND       = 34026;        // BM spec talent, 1st starter-build pick -> effectively L10
constexpr uint32 COBRA_SHOT         = 193455;       // BM spec talent (min L14); OVERRIDES Arcane Shot on the bar
constexpr uint32 ARCANE_SHOT        = 185358;       // baseline L2 spender; BM's focus dump until Cobra Shot is talented
constexpr uint32 STEADY_SHOT        = 56641;        // baseline L1 filler/generator until Barbed Shot overrides it
constexpr uint32 BESTIAL_WRATH      = 19574;        // BM spec talent (~L20-26 via starter build)
constexpr uint32 BLOODSHED          = 1272099;      // talent — pet bleed. 12.0 re-ID; the old 321530 is dead data
constexpr uint32 DIRE_BEAST         = 120679;       // BM spec talent (moved from class tree in 12.0)
constexpr uint32 COUNTER_SHOT       = 147362;       // class talent (L18+), BM/MM only
constexpr uint32 WILD_THRASH        = 1264359;      // 12.0 BM AoE button — replaces Multi-Shot (2643 is unobtainable) and triggers Beast Cleave
constexpr uint32 MISDIRECTION       = 34477;
constexpr uint32 ASPECT_TURTLE      = 186265;
constexpr uint32 EXHILARATION       = 109304;
constexpr uint32 SURVIVAL_FITTEST   = 264735;
constexpr uint32 DISENGAGE          = 781;
constexpr uint32 MEND_PET           = 136;
constexpr uint32 REVIVE_PET         = 982;
constexpr uint32 FEIGN_DEATH        = 5384;
constexpr uint32 INTIMIDATION       = 19577;
constexpr uint32 TAR_TRAP           = 187698;
constexpr uint32 FREEZING_TRAP      = 187650;
constexpr uint32 BINDING_SHOT       = 109248;
constexpr uint32 PRIMAL_RAGE        = 264667;
constexpr uint32 FRENZY_AURA        = 272790;
constexpr uint32 BEAST_CLEAVE_PET   = 118455;      // pet aura applied by Multi-Shot; pet melee cleaves while up
constexpr uint32 ASPECT_CHEETAH     = 186257;
constexpr uint32 HUNTERS_MARK       = 257284;

constexpr uint8 POWER_FOCUS_IDX = 2;

// Focus costs (display units, stable since Legion). Spenders only fire
// when the cast leaves >= KILL_COMMAND_COST in the tank so KC — BM's
// best damage-per-focus button — is never delayed by a filler.
constexpr int32 KILL_COMMAND_COST = 30;
constexpr int32 COBRA_SHOT_COST   = 35;
constexpr int32 ARCANE_SHOT_COST  = 40;

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

bool BotHasSatedDebuff(ApPredicateContext const& ctx)
{
    constexpr uint32 SATED_DEBUFF           = 57724;
    constexpr uint32 TEMPORAL_DISPL_DEBUFF  = 80354;
    constexpr uint32 INSANITY_HUNTER_DEBUFF = 95809;
    constexpr uint32 FATIGUED_DEBUFF        = 264689;
    return ctx.bot.has_aura(SATED_DEBUFF)
        || ctx.bot.has_aura(TEMPORAL_DISPL_DEBUFF)
        || ctx.bot.has_aura(INSANITY_HUNTER_DEBUFF)
        || ctx.bot.has_aura(FATIGUED_DEBUFF);
}

int32 FocusVal(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_FOCUS_IDX); }

bool TargetExecuteRange(ApPredicateContext const& ctx)
{
    NearbyUnit const* t = ctx.bot.victim_info();
    if (!t || t->max_hp <= 0 || t->hp <= 0) return false;
    return (t->hp * 100) / t->max_hp <= 20;
}

// ---- Pet maintenance ----
bool ShouldRevivePet(ApPredicateContext const& ctx)
{
    // Revive Pet brings a *dead* pet back. If pet_guid is empty the bot
    // never had a pet (or it was dismissed) — Revive Pet would silently
    // fail and starve the rest of the rotation by re-firing every tick.
    // Use Call Pet via the BM-specific summon flow for that case instead.
    if (ctx.bot.pet_guid().IsEmpty()) return false;
    if (ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(REVIVE_PET)) return false;
    return ctx.bot.is_ready(REVIVE_PET);
}
void DoRevivePet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REVIVE_PET); }

bool ShouldMendPet(ApPredicateContext const& ctx)
{
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(MEND_PET)) return false;
    if (!ctx.bot.is_ready(MEND_PET)) return false;
    // Mend Pet is a ~10s HoT, not a direct heal. Without this gate the rule
    // (which sits ABOVE the damage tier) re-cast every GCD while the pet stayed
    // <50%, starving the whole shot rotation — the bot "kept healing the pet and
    // never attacked" (observed L11 Uraimus). Once the HoT is ticking, yield to
    // damage; only re-apply after it falls off and the pet is still hurt.
    if (ctx.bot.find_pet_aura(MEND_PET) != nullptr) return false;
    return ctx.bot.pet_hp_pct() <= 50;
}
void DoMendPet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MEND_PET); }

// ---- Survival ----
bool ShouldAspectTurtle(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(ASPECT_TURTLE)) return false;
    if (!ctx.bot.is_ready(ASPECT_TURTLE)) return false;
    // PvP: player-spike windows kill faster than PvE; pop earlier so the
    // immunity catches the burst rather than chasing it.
    const int32 threshold = ctx.pvp.under_player_attack ? 40 : 20;
    return ctx.bot.hp_pct() <= threshold;
}
void DoAspectTurtle(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_TURTLE); }

bool ShouldExhilaration(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(EXHILARATION)) return false;
    if (!ctx.bot.is_ready(EXHILARATION)) return false;
    return ctx.bot.hp_pct() <= 50;
}
void DoExhilaration(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EXHILARATION); }

bool ShouldSurvivalFittest(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(SURVIVAL_FITTEST)) return false;
    if (!ctx.bot.is_ready(SURVIVAL_FITTEST)) return false;
    return ctx.bot.hp_pct() <= 60;
}
void DoSurvivalFittest(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SURVIVAL_FITTEST); }

bool ShouldDisengage(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(DISENGAGE)) return false;
    if (!ctx.bot.is_ready(DISENGAGE)) return false;
    // "Kite 2+ melee": leap back only when 2+ enemies are actually ATTACKING
    // the bot in melee range — not merely near it. The old enemies_within(8)
    // gate also counted the PET's targets, so a full-HP BM hunter whose boar
    // pulled a camp leapt away from the pet's fight every cooldown, never
    // engaging — a 60-minute in-combat / 0-XP stall (Zekani, L10, Barrens).
    return ctx.bot.melee_attackers_within(8.0f) >= 2;
}
void DoDisengage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DISENGAGE); }

bool ShouldFeignDeath(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.knows_spell(FEIGN_DEATH)) return false;
    if (!ctx.bot.is_ready(FEIGN_DEATH)) return false;
    return ctx.bot.hp_pct() <= 30 && ctx.bot.attackers_count() >= 1;
}
void DoFeignDeath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEIGN_DEATH); }

// ---- Group utility ----
bool ShouldMisdirection(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(MISDIRECTION)) return false;
    if (!ctx.bot.is_ready(MISDIRECTION)) return false;
    auto const* tank = ctx.group.tank();
    return tank && tank->online && tank->guid != ctx.bot.raw().guid;
}
void DoMisdirection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* tank = ctx.group.tank())
        e.cast(MISDIRECTION, tank->guid);
}

// Primal Rage is a PET ability (Ferocity family), not a hunter spell —
// knows_spell()/is_ready() query the BOT's spellbook and always failed, and
// even if they passed, e.cast() would cast from the player and bounce off
// NOT_KNOWN. Route through pet_cast; API::pet_cast validates the pet knows
// it (Ferocity) and checks the pet's own SpellHistory, so no bot-side
// spellbook gates apply.
bool ShouldPrimalRage(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.pet_can_bloodlust()) return false;   // Ferocity pets only
    if (BotHasSatedDebuff(ctx)) return false;
    return BossLikeTargetEngaged(ctx);
}
void DoPrimalRage(ApPredicateContext const&, BotIntentEmitter& e) { e.pet_cast(PRIMAL_RAGE); }

// ---- Interrupt / CC ----
bool ShouldCounterShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(COUNTER_SHOT)) return false;
    if (!ctx.bot.is_ready(COUNTER_SHOT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoCounterShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(COUNTER_SHOT, c->guid);
}

bool ShouldIntimidation(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(INTIMIDATION)) return false;
    if (!ctx.bot.is_ready(INTIMIDATION)) return false;
    if (ctx.bot.is_ready(COUNTER_SHOT)) return false;
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    return ctx.bot.kick_target(pvp, 40.0f) != nullptr;
}
void DoIntimidation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    const bool pvp = ctx.pvp.in_battleground || ctx.pvp.in_arena;
    if (auto const* c = ctx.bot.kick_target(pvp, 40.0f))
        e.cast(INTIMIDATION, c->guid);
}

bool ShouldBindingShot(ApPredicateContext const& ctx)
{
    if (!ctx.bot.knows_spell(BINDING_SHOT)) return false;
    if (!ctx.bot.is_ready(BINDING_SHOT)) return false;
    return ctx.bot.enemies_within(15.0f) >= 3;
}
void DoBindingShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* v = ctx.bot.victim_info())
        e.cast_at(BINDING_SHOT, v->x, v->y, v->z);
    else
        e.cast(BINDING_SHOT);
}

// ---- Major offensive cooldowns ----
// 12.0 removed Aspect of the Wild (193530), Call of the Wild (359844) and
// Stampede (201430) — no SkillLineAbility / SpecializationSpells / live
// trait node grants them (wago.tools 12.0.5.67823). Their rules were
// deleted with them; BM burst is Bestial Wrath + Bloodshed + Dire Beast.
bool ShouldBestialWrath(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BESTIAL_WRATH)) return false;
    if (!ctx.bot.is_ready(BESTIAL_WRATH)) return false;
    // Leveling workhorse, not a boss-only button: at a 90s CD vs ~15s
    // overworld kills, holding it for "boss-like" (5M HP) targets meant
    // leveling bots NEVER pressed it. Fire on any fight that can absorb
    // the CD: a meaty single target (more HP than the bot), a multi-pull,
    // or the classic boss/burst windows.
    if (BossLikeTargetEngaged(ctx)) return true;
    if (ctx.bot.attackers_count() >= 2 || ctx.bot.enemies_within(10.0f) >= 2) return true;
    NearbyUnit const* t = ctx.bot.victim_info();
    return t && t->max_hp >= ctx.bot.max_hp();
}
void DoBestialWrath(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BESTIAL_WRATH); }

bool ShouldBloodshed(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(BLOODSHED)) return false;
    if (!ctx.bot.is_ready(BLOODSHED)) return false;
    return BossLikeTargetEngaged(ctx) || ctx.bot.enemies_within(10.0f) >= 3;
}
void DoBloodshed(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLOODSHED, ctx.bot.victim());
}

// Hunter's Mark — baseline ranged-damage-taken debuff (5%). Auto-granted
// around L7 and persists across all three specs. Apply once per target
// (the aura is permanent on the victim until they die or the bot dies).
bool ShouldHuntersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(HUNTERS_MARK)) return false;
    if (!ctx.bot.is_ready(HUNTERS_MARK)) return false;
    return ctx.bot.find_aura(HUNTERS_MARK, ctx.bot.victim()) == nullptr;
}
void DoHuntersMark(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HUNTERS_MARK, ctx.bot.victim());
}

bool ShouldDireBeast(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(DIRE_BEAST)) return false;
    return ctx.bot.is_ready(DIRE_BEAST);
}
void DoDireBeast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DIRE_BEAST, ctx.bot.victim());
}

// ---- Execute ----
bool ShouldKillShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(KILL_SHOT)) return false;
    if (!ctx.bot.is_ready(KILL_SHOT)) return false;
    return TargetExecuteRange(ctx);
}
void DoKillShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_SHOT, ctx.bot.victim());
}

// ---- Frenzy + AoE + filler ----
bool ShouldBarbedShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(BARBED_SHOT)) return false;
    if (!ctx.bot.is_ready(BARBED_SHOT)) return false;
    if (!ctx.bot.has_pet()) return true;
    AuraEntry const* frenzy = ctx.bot.find_pet_aura(FRENZY_AURA);
    if (!frenzy) return true;
    // 2.5s refresh window: snapshot cadence (~250ms) + per-bot reaction
    // delay (200-450ms) + the GCD ate too far into the old 2.0s margin —
    // Frenzy (8s) dropped between ticks. 2.5s still avoids early clipping.
    if (frenzy->remaining.count() <= 2500) return true;
    // Anti-cap: with Frenzy healthy, still spend a charge when BOTH are
    // banked — sitting at 2/2 wastes recharge time (12s per charge).
    return ctx.bot.charges(BARBED_SHOT) >= 2;
}
void DoBarbedShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BARBED_SHOT, ctx.bot.victim());
}

// Wild Thrash (12.0) — BM's AoE button, replacing the unobtainable
// Multi-Shot (2643): it triggers Beast Cleave so the pet's melee + Kill
// Command strike everything nearby for ~8s. Cast it to TURN CLEAVE ON
// and keep it up through a pack, not every AoE GCD: re-cast only when
// the pet's cleave aura is missing or about to fall off. AoE detection
// uses attackers_count (mobs actually fighting us) — a bot leveling
// through a populated camp should not cleave on proximity alone.
bool ShouldWildThrash(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(WILD_THRASH)) return false;
    if (!ctx.bot.is_ready(WILD_THRASH)) return false;
    const bool aoe = ctx.aoe_preference || ctx.bot.attackers_count() >= 2;
    if (!aoe) return false;
    if (!ctx.bot.has_pet()) return false;    // cleave is pet damage; pointless petless
    AuraEntry const* cleave = ctx.bot.find_pet_aura(BEAST_CLEAVE_PET);
    return !cleave || cleave->remaining.count() <= 1500;
}
void DoWildThrash(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(WILD_THRASH, ctx.bot.victim());
}

bool ShouldKillCommand(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.has_pet()) return false;
    if (!ctx.bot.knows_spell(KILL_COMMAND)) return false;
    if (!ctx.bot.is_ready(KILL_COMMAND)) return false;
    return FocusVal(ctx) >= KILL_COMMAND_COST;
}
void DoKillCommand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(KILL_COMMAND, ctx.bot.victim());
}

// ---- Focus spenders ----
// Shared gate: spend only when the cast leaves >= KILL_COMMAND_COST
// banked, so Kill Command comes off cooldown fully funded. Without the
// reserve the old Cobra rule (bare 35-focus check) drained the pool and
// delayed KC by 1-2 GCDs every cycle.
bool CanAffordSpender(ApPredicateContext const& ctx, int32 cost)
{
    return FocusVal(ctx) >= cost + KILL_COMMAND_COST;
}

bool ShouldCobraShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (!ctx.bot.knows_spell(COBRA_SHOT)) return false;
    if (!ctx.bot.is_ready(COBRA_SHOT)) return false;
    return CanAffordSpender(ctx, COBRA_SHOT_COST);
}
void DoCobraShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(COBRA_SHOT, ctx.bot.victim());
}

// Arcane Shot: the focus dump for the pre-Cobra leveling bracket. This
// was MISSING from the BM list entirely — live-verified 2026-06-10
// (Uraimus, L13): rule history showed Hunter's Mark -> Kill Command ->
// Steady, Steady, Steady... with focus pinned at cap, because the only
// listed spender (Cobra Shot) isn't learned yet at that level. Once
// Cobra Shot is known this rule self-disables (Cobra is strictly better
// for BM and sits one slot above).
bool ShouldArcaneShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(COBRA_SHOT)) return false;
    if (!ctx.bot.knows_spell(ARCANE_SHOT)) return false;
    if (!ctx.bot.is_ready(ARCANE_SHOT)) return false;
    return CanAffordSpender(ctx, ARCANE_SHOT_COST);
}
void DoArcaneShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_SHOT, ctx.bot.victim());
}

// Steady Shot: L1 baseline filler for unspecced and Beast Mastery hunters.
// Verified 2026-05-20 — fleet hunters at L1-9 default-route to the BM
// rotation (ApRegistry::DefaultSpecForClass returns 253), but BM's only
// filler is Cobra Shot which is learned at higher level + requires
// 35 focus. Without Steady Shot, L1 hunters fall through to AutoAttack
// which is melee — and L1 hunters typically aren't carrying a melee
// weapon, so auto-attack does nothing. Steady Shot has no minimum
// range, no focus cost, and is the baseline shot all hunters keep
// across levels.
//
// is_ready gate (2026-06-10): without it the rule re-emitted mid-cast
// once the 1.5s emit lockout lapsed inside Steady's ~2s cast — the
// executor then burned a world-thread cast that bounced off
// SPELL_FAILED_SPELL_IN_PROGRESS (observed as CastSpell|NotReady pairs
// in Uraimus's intent history). is_ready folds in is_casting + GCD.
// Barbed-Shot gate (12.0): the Barbed Shot talent OVERRIDES Steady Shot on
// the bar (TraitDefinition OverridesSpellID) — TC redirects casts of the
// base spell to the override, so a Steady emit from a Barbed-talented bot
// would silently burn a Barbed CHARGE as an unintended filler. Once Barbed
// is known, BM's filler is auto shot + focus regen (matches retail play).
bool ShouldSteadyShot(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx)) return false;
    if (ctx.bot.knows_spell(BARBED_SHOT)) return false;
    if (!ctx.bot.knows_spell(STEADY_SHOT)) return false;
    return ctx.bot.is_ready(STEADY_SHOT);
}
void DoSteadyShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(STEADY_SHOT, ctx.bot.victim());
}

bool AlwaysInCombat(ApPredicateContext const& ctx) { return ctx.bot.in_combat(); }
void DoAutoAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid t = ctx.bot.victim();
    if (t.IsEmpty()) t = ctx.bot.current_target();
    // Retaliate fallback (2026-06-17): in combat with no victim/target the bot
    // looped "Engage auto attack" as a no-op while taking damage (the CombatLoop
    // wedge cluster). Engage whatever is meleeing us (attackers are adjacent and
    // reachable), else the nearest visible enemy, so combat actually resolves.
    // Strictly additive: only runs when no target was already selected.
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.attackers())
            if (u.hp > 0) { t = u.guid; break; }
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.nearby_enemies())
            if (u.hp > 0 && u.in_los) { t = u.guid; break; }
    if (!t.IsEmpty()) e.start_attack(t);
}

ApRule const kRules[] = {
    { ShouldRevivePet,         DoRevivePet,         "Revive Pet"                  },
    { ShouldMendPet,           DoMendPet,           "Mend Pet (<=50%)"            },
    { ShouldAspectTurtle,      DoAspectTurtle,      "Aspect of the Turtle (<=20%)"},
    { ShouldSurvivalFittest,   DoSurvivalFittest,   "Survival of the Fittest"     },
    { ShouldExhilaration,      DoExhilaration,      "Exhilaration (<=50%)"        },
    { ShouldDisengage,         DoDisengage,         "Disengage (kite 2+ melee)"   },
    { ShouldFeignDeath,        DoFeignDeath,        "Feign Death (drop aggro)"    },
    { ShouldMisdirection,      DoMisdirection,      "Misdirection (tank threat)"  },
    { ShouldCounterShot,       DoCounterShot,       "Counter Shot (interrupt)"    },
    { ShouldIntimidation,      DoIntimidation,      "Intimidation (interrupt fb)" },
    { ShouldBindingShot,       DoBindingShot,       "Binding Shot (3+ AoE)"       },
    { ShouldPrimalRage,        DoPrimalRage,        "Primal Rage (Bloodlust)"     },
    { ShouldBestialWrath,      DoBestialWrath,      "Bestial Wrath"               },
    { ShouldBloodshed,         DoBloodshed,         "Bloodshed"                   },
    { ShouldDireBeast,         DoDireBeast,         "Dire Beast"                  },
    { ShouldHuntersMark,       DoHuntersMark,       "Hunter's Mark (debuff)"      },
    { ShouldKillShot,          DoKillShot,          "Kill Shot (<=20%)"           },
    { ShouldBarbedShot,        DoBarbedShot,        "Barbed Shot (Frenzy)"        },
    { ShouldWildThrash,        DoWildThrash,        "Wild Thrash (Beast Cleave)"  },
    { ShouldKillCommand,       DoKillCommand,       "Kill Command"                },
    { ShouldCobraShot,         DoCobraShot,         "Cobra Shot (spender)"        },
    { ShouldArcaneShot,        DoArcaneShot,        "Arcane Shot (pre-Cobra)"     },
    { ShouldSteadyShot,        DoSteadyShot,        "Steady Shot (filler)"        },
    { AlwaysInCombat,          DoAutoAttack,        "Engage auto attack"          },
};

} // anonymous

void RegisterApl_Hunter_BeastMastery()
{
    constexpr uint8 SPEC_HUNTER_BEAST_MASTERY = 253;
    RegisterRotation(CLASS_HUNTER, SPEC_HUNTER_BEAST_MASTERY, ApRotation{kRules});
}

} // namespace Playerbot::Combat
