#include "StateBase.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotRegistry.h"
#include "Bot/IdleRule.h"
#include "Bot/QuestDoable.h"
#include "Bot/ClassTables.h"
#include "Bot/RecipeDifficulty.h"
#include "Bot/BagSizeTable.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/Battleground/BattlegroundScript.h"
#include "Group/GroupSnapshot.h"
#include "../Services.h"
#include "Util/ConfigReader.h"  // full type for Services::Config() (detour pull-gate knobs)
#include "Travel/QuestHubDatabase.h"
#include "Travel/RepairVendorIndex.h"
#include "Travel/UnifiedTravelGraph.h"
#include "Travel/ElevatorIndex.h"  // elevator-base hand-off for off-mesh dock platforms
#include "Travel/PortalPocketIndex.h"  // L5: portal-room entrance-areatrigger gateway
#include "Fleet/BotGuildMgr.h"
#include "Fleet/BotGuildEvent.h"
#include "Combat/ApRegistry.h"
#include "Combat/ApRotation.h"
#include "Diagnostics/PerfCounters.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"      // sObjectMgr->GetGameObjectQuestItemList / GetCreatureQuestItemList
#include "Creature.h"
#include "SharedDefines.h"
#include "DBCEnums.h"       // Difficulty enum (cast from snapshot map_difficulty)
#include "DB2Stores.h"      // sAreaTableStore for idle:pvp_help_callout zone name
#include "CharacterCache.h" // sCharacterCache->GetCharacterNameByGuid for mail-thank whisper
#include "World.h"          // sWorld->GetDefaultDbcLocale for AreaTableEntry localization
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Log.h"            // TC_LOG_INFO for BG-gate diagnostic
#include "Config.h"         // sConfigMgr for guild charter signature threshold
#include "GridNotifiers.h"  // AllCreaturesOfEntryInRange / CreatureListSearcher (tank-advance boss scan)
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "PathGenerator.h"      // dungeon advance: real navmesh reachability test
#include "PlayerbotMovement.h"  // BotMovement::SehSafeCalculatePath (tile-race guard)
#include "PlayerbotAPI.h"       // PathBudget::HasBudget (dungeon scan gating)
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <fmt/format.h>

#include "Bot/States/MaintainHelpers.h"

namespace Playerbot::States {

// The Maintain* family below are declared in MaintainHelpers.h so per-
// subsystem rule files (Bot/States/Rules/MaintainRules.cpp etc.) can
// wrap them as registered IdleRules. Truly private helpers (ScoreItem,
// GetIdleConsumeThresholds, ClassOocRez) get `static` for internal
// linkage instead of an anonymous namespace.

// Score an item against the bot's spec stat weights. Used by the
// equip-upgrade rule (Phase 1 of ITEM_VENDOR_SYSTEM_PLAN.md). Decision
// flow:
//   - item_level alone contributes 1.0× (so a vastly higher ilvl wins
//     even on stat mismatch — see V1's 20-ilvl override).
//   - Each weighted stat contributes (value × spec_weight).
//   - Weapons add weapon_dps_x10 × spec_weapon_dps_weight.
// Returns a single comparable scalar; caller decides "swap if new > old + margin".
static inline float ScoreItem(ItemStatBlock const& stats,
                       uint16 item_level,
                       std::array<float, 12> const& weights,
                       float dps_weight)
{
    float score = static_cast<float>(item_level);
    for (size_t i = 0; i < weights.size(); ++i)
        score += static_cast<float>(stats.stats[i]) * weights[i];
    score += static_cast<float>(stats.weapon_dps_x10) * dps_weight;
    return score;
}

// (ClassSelfBuff lives in Bot/ClassTables.cpp — shared with whisper helpers.)

// Max distance for picking a group-buff target. A shared continent map_id
// is NOT proximity (maps span ~30,000y); without this gate the buff picker
// returns members in other zones and the cast fails OUT_OF_RANGE/LoS every
// tick. 80y comfortably covers any real party spread while killing the
// cross-zone far-cast spam.
static constexpr float kBuffSelectRange = 80.0f;

// A bot that is mounted, on a transport, or in a vehicle is in transit —
// out-of-combat MAINTENANCE casts (class buffs, pet/demon summons, conjured
// items, Soulstone) are rejected server-side with SPELL_FAILED_NOT_MOUNTED in
// those states and then retried every backoff window forever (audit found
// ~34k NOT_MOUNTED rejections, dominated by null-target Soulstone / Healthstone
// / Imp summons cast while riding a zeppelin/boat/mount). Defer maintenance
// until the bot is settled; tracked buffs carry a 60s+ refresh margin, so
// nothing is lost by waiting for dismount/arrival.
static inline bool InTransitNoMaintenance(BotSnapshotView const& s)
{
    return s.is_mounted() || s.on_transport() || s.raw().vehicle.on_vehicle;
}

// Returns true if the bot needs to cast (and did) its class raid buff —
// either on itself or on a group member who is missing it. Maintains a 60s
// safety margin before fall-off. Group buffs apply to the whole party in
// modern WoW, but the cast still requires a friendly target; we pick the
// first member missing the buff to drive the cast (the AoE radius covers
// the rest from there).
bool MaintainSelfBuff(BotSnapshotView const& s, GroupSnapshotView const& g,
                      BotIntentEmitter& emit)
{
    if (InTransitNoMaintenance(s)) return false;
    const uint32 buff = ClassSelfBuff(s.cls());
    if (!buff) return false;
    if (!s.knows_spell(buff)) return false;
    if (!s.is_ready(buff)) return false;
    // 1) Self first — covers solo bots and ungrouped buff maintenance.
    // Same dedup-aware-return pattern as MaintainSoulstone — when cast()'s
    // per-spell emit lockout (1.5 s) blocks the emit, return false so
    // dispatch falls through. Without this, idle:self_buff claimed the
    // dispatch slot every tick whenever a buff was about to expire,
    // tripping the watchdog repeatedly.
    AuraEntry const* mine = s.find_aura(buff);
    if (!mine || mine->remaining.count() <= 60'000)
        return emit.cast(buff);
    // 2) Group members — pick the first one missing the tracked raid buff.
    //    member_missing_buff() filters to map_id AND to a real distance from
    //    the caster: a shared continent map_id spans ~30,000y, so without the
    //    range gate we'd return a member in another zone and the cast would
    //    fail OUT_OF_RANGE / LoS every tick.
    if (g.exists())
    {
        float bx, by, bz; s.position(bx, by, bz);
        if (auto const* miss = g.member_missing_buff(buff, s.map_id(), bx, by, bz, kBuffSelectRange))
            return emit.cast(buff, miss->guid);
    }
    return false;
}

// Warlock Soulstone Resurrection (20707): pre-applied buff that lets the
// target self-rez on death (via spell 3026). Long duration (15 min), 10min
// CD. The healthiest play is to keep a tank or healer stoned through the
// pull. We apply to (1) the lowest-on-mana healer if grouped (healer death
// = wipe), else (2) the tank, else (3) self. Returns true if a Soulstone
// was emitted.
bool MaintainSoulstone(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit)
{
    if (InTransitNoMaintenance(s)) return false;
    if (s.cls() != CLASS_WARLOCK) return false;
    constexpr uint32 SOULSTONE = 20707;
    if (!s.knows_spell(SOULSTONE)) return false;
    if (!s.is_ready(SOULSTONE)) return false;
    // Pick a target: prefer a member who lacks the buff; nullptr means
    // everyone reachable already has it. The aura tracking allowlist must
    // include 20707 for member_missing_buff to work — fall back to self
    // if the group buff cycle isn't picking it up.
    if (g.exists())
    {
        float bx, by, bz; s.position(bx, by, bz);
        if (auto const* miss = g.member_missing_buff(SOULSTONE, s.map_id(), bx, by, bz, kBuffSelectRange))
        {
            // Return whatever cast() returns: false when the per-spell
            // emit dedup (kCastEmitLockoutMs = 1.5 s) blocks. Without
            // this, the rule claims the dispatch slot every snapshot
            // tick even when no intent actually went out — watchdog
            // tripped 1811 times for idle:soulstone over a single
            // session. Falling through to other rules when deduped
            // breaks the wedge.
            return emit.cast(SOULSTONE, miss->guid);
        }
    }
    // Self-stone fallback when solo or all members already stoned.
    AuraEntry const* mine = s.find_aura(SOULSTONE);
    if (!mine || mine->remaining.count() <= 60'000)
        return emit.cast(SOULSTONE);
    return false;
}

// Pet management: hunters and warlocks should always have a pet out
// while not in combat. Warlock destruction has Infernal-on-CD, so we
// only handle the imp/voidwalker baseline. Returns true if a pet
// summon/revive was emitted.
bool MaintainPet(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit)
{
    // Reset backoff as soon as the bot acquires a pet — the rule will
    // resume when the pet later dies / despawns.
    if (s.has_pet()) { ai.clear_pet_summon_backoff(); return false; }
    // Can't summon while mounted / on a transport / in a vehicle — the cast
    // is rejected NOT_MOUNTED and re-tried forever. Wait until settled.
    if (InTransitNoMaintenance(s)) return false;
    const uint32 pet_now_ms = GameTime::GetGameTimeMS();
    if (ai.pet_summon_in_backoff(pet_now_ms))
        return false;
    // Combat-flicker gate, threat-aware (2026-06-22). Revive Pet is a 10s cast,
    // Call Pet 5s, warlock summons 6s — a summon started a single tick before the
    // next pull eats the mob's first hit and is wasted. The OLD gate refused ALL
    // re-summons for 5s after combat exit, but a bot questing through a mob-dense
    // area is ALWAYS within 5s of its last pull — so the window NEVER opened, it
    // NEVER re-summoned, walked into the next pull PET-LESS, and (for a low-level
    // pet-dependent class) died: the Morthan death spiral (L9 warlock, "Summon Imp
    // (no pet)" mid-combat, XP frozen). The 5s timer was only a crude proxy for
    // "another pull is imminent / the cast will be interrupted" — test that
    // directly: within the post-combat window, refuse ONLY when a LIVE hostile is
    // close enough to reach melee and break the cast. When the bot is genuinely
    // safe right now (no hostile within kSummonSafeRadius), summon regardless of
    // how recently combat ended — the cast completes and the bot re-engages WITH
    // its pet. Applies to every pet class (hunter/warlock/DK) — systemic.
    if (s.recently_in_combat(5000))
    {
        constexpr float kSummonSafeRadius = 25.0f;   // a mob within this can close to melee during a 6s cast
        float bx, by, bz; s.position(bx, by, bz);
        for (auto const& e : s.nearby_enemies())
        {
            if (e.hp <= 0) continue;                  // dead/dying — no longer a threat
            const float dx = e.x - bx, dy = e.y - by;
            if (dx * dx + dy * dy <= kSummonSafeRadius * kSummonSafeRadius)
                return false;                         // a pull is imminent — wait for the next safe tick
        }
        // No live hostile in range — this is a genuine safe OOC window; summon.
    }
    if (s.cls() == CLASS_HUNTER)
    {
        constexpr uint32 CALL_PET_1   = 883;
        constexpr uint32 CALL_PET_2   = 83242;
        constexpr uint32 CALL_PET_3   = 83243;
        constexpr uint32 CALL_PET_4   = 83244;
        constexpr uint32 CALL_PET_5   = 83245;
        constexpr uint32 REVIVE_PET   = 982;
        // Pet entry exists but is dead → revive
        if (!s.pet_guid().IsEmpty() && s.knows_spell(REVIVE_PET) && s.is_ready(REVIVE_PET))
        {
            emit.cast(REVIVE_PET);
            ai.note_pet_summon_attempt(pet_now_ms);
            return true;
        }
        // No pet → summon. Try all 5 Call Pet slots — the bot's stable
        // may have a beast in any of them. Each Call Pet is a separate
        // spell ID (TC's 12.0 spell.db2). Backoff prevents the spam-
        // loop on bots whose stable is empty (Call Pet rejects every
        // tick — dispatch never reaches movement rules, bot stuck).
        //
        // Mirror baseline_hunter::ShouldCallPet (Apl_Baseline.cpp) — gate
        // on a real beast in an ACTIVE stable slot. Without this, fresh
        // L1-9 hunters who never tamed waste one Call Pet emit per backoff
        // window indefinitely; server rejects with SPELL_FAILED_NO_PET (32)
        // and the rule keeps re-firing.
        bool has_summonable = false;
        for (auto const& sp : s.raw().pet.stable_pets)
            if (sp.slot_kind == 0 && sp.creature_id != 0)
            { has_summonable = true; break; }
        if (has_summonable)
        {
            for (uint32 sid : { CALL_PET_1, CALL_PET_2, CALL_PET_3,
                                CALL_PET_4, CALL_PET_5 })
            {
                if (s.knows_spell(sid) && s.is_ready(sid))
                {
                    emit.cast(sid);
                    ai.note_pet_summon_attempt(pet_now_ms);
                    return true;
                }
            }
        }
    }
    else if (s.cls() == CLASS_WARLOCK)
    {
        // Spec-aware preference order. Demonology uses the permanent Felguard;
        // the other specs prefer the utility/dps imps. We still walk the full
        // list so an Affliction warlock who somehow knows Felguard can still
        // summon a fallback if its own preferred summon is unavailable.
        // Spell IDs validated against 12.0 spell.db2.
        constexpr uint32 SUMMON_IMP        = 688;
        constexpr uint32 SUMMON_VOIDWALKER = 697;
        constexpr uint32 SUMMON_FELHUNTER  = 691;
        constexpr uint32 SUMMON_SUCCUBUS   = 712;
        constexpr uint32 SUMMON_FELGUARD   = 30146;   // Demonology baseline
        const uint32 spec = s.spec();
        const uint32 SPEC_DEMO = 266, SPEC_AFFL = 265, SPEC_DESTRO = 267;
        uint32 preferred[5];
        int n = 0;
        if (spec == SPEC_DEMO) {
            preferred[n++] = SUMMON_FELGUARD;
        } else if (spec == SPEC_AFFL) {
            preferred[n++] = SUMMON_FELHUNTER;          // utility for the dot spec
            preferred[n++] = SUMMON_IMP;
        } else if (spec == SPEC_DESTRO) {
            preferred[n++] = SUMMON_IMP;                // best singletarget dps for Destro
            preferred[n++] = SUMMON_FELHUNTER;
        } else {
            preferred[n++] = SUMMON_IMP;
        }
        // Always append the rest as fallbacks (skip duplicates).
        for (uint32 sid : { SUMMON_IMP, SUMMON_VOIDWALKER, SUMMON_FELHUNTER, SUMMON_SUCCUBUS, SUMMON_FELGUARD })
        {
            bool dup = false;
            for (int i = 0; i < n; ++i) if (preferred[i] == sid) { dup = true; break; }
            if (!dup && n < 5) preferred[n++] = sid;
        }
        for (int i = 0; i < n; ++i)
        {
            const uint32 sid = preferred[i];
            if (s.knows_spell(sid) && s.is_ready(sid))
            {
                emit.cast(sid);
                ai.note_pet_summon_attempt(pet_now_ms);
                return true;
            }
        }
    }
    else if (s.cls() == CLASS_DEATH_KNIGHT)
    {
        // Unholy DK ghoul — Raise Dead summons a 1-min ghoul (talented to
        // permanent for Unholy via Master of Ghouls). Other DK specs get a
        // temporary 1-min DPS pet which is still useful between pulls when
        // off cooldown. Server-side gates on spec-allowed talent.
        constexpr uint32 RAISE_DEAD = 46584;
        if (s.knows_spell(RAISE_DEAD) && s.is_ready(RAISE_DEAD))
        {
            emit.cast(RAISE_DEAD);
            ai.note_pet_summon_attempt(pet_now_ms);
            return true;
        }
    }
    return false;
}

// Group-wide utility summons. Cheap, long-cooldown spells that benefit
// the whole group: warlock Soulwell (clickable healthstone source) and
// Mage Conjure Refreshment Table (group-wide mana food). Both ground-cast
// at the bot's position. Returns true on emit; the cast cooldown gates
// re-firing (Soulwell 3min cd, Refreshment Table ~3min cd).
bool MaintainGroupUtility(BotSnapshotView const& s, GroupSnapshotView const& g,
                          BotIntentEmitter& emit)
{
    if (!g.exists()) return false;
    if (s.in_combat()) return false;
    if (InTransitNoMaintenance(s)) return false;
    if (s.is_casting()) return false;
    if (s.cls() == CLASS_WARLOCK)
    {
        constexpr uint32 CREATE_SOULWELL = 29893;
        if (s.knows_spell(CREATE_SOULWELL) && s.is_ready(CREATE_SOULWELL))
        {
            emit.cast(CREATE_SOULWELL);
            return true;
        }
        // Soulstone the group's healer (or tank if no healer) as battle-rez
        // insurance. Aura lasts 15min, off the GCD, instant cast. Skipped
        // if the target already has it. Cast is target-friendly so we pick
        // a member specifically — unlike Soulwell which is self-cast.
        constexpr uint32 SOULSTONE = 20707;
        if (s.knows_spell(SOULSTONE) && s.is_ready(SOULSTONE))
        {
            // Prefer the healer; fall back to tank.
            GroupMemberSummary const* tgt = nullptr;
            if (auto const* members = g.members())
            {
                for (auto const& m : *members)
                    if (m.role == Role::Healer && m.online && m.hp > 0)
                    { tgt = &m; break; }
            }
            if (!tgt)
            {
                if (auto const* t = g.tank())
                    tgt = t;
            }
            if (tgt && tgt->online && tgt->map_id == s.map_id() && tgt->hp > 0)
            {
                float bx, by, bz; s.position(bx, by, bz);
                if (auto const* miss = g.member_missing_buff(SOULSTONE, s.map_id(), bx, by, bz, kBuffSelectRange))
                {
                    if (miss->guid == tgt->guid)
                    {
                        emit.cast(SOULSTONE, tgt->guid);
                        return true;
                    }
                }
            }
        }
    }
    if (s.cls() == CLASS_MAGE)
    {
        constexpr uint32 CONJURE_REFRESHMENT_TABLE = 167951;
        if (s.knows_spell(CONJURE_REFRESHMENT_TABLE) &&
            s.is_ready(CONJURE_REFRESHMENT_TABLE))
        {
            emit.cast(CONJURE_REFRESHMENT_TABLE);
            return true;
        }
    }
    return false;
}

// Refill consumable summons that the bot uses in combat:
//   - Warlock Create Healthstone (6201) → Healthstone item 5512
//   - Mage Conjure Mana Cake / Refreshment (190336) → mana food
// Skipped if the bot already has the item in its bag. Returns true on emit.
bool MaintainConjuredItem(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    // Conjure spells are hard-cast — server rejects them with
    // SPELL_FAILED_MOVING (75) while the bot is in motion. Without this
    // gate the rule re-fires every snapshot at a wandering bot, hitting
    // the cast_spell API on every tick: log spam + wasted CPU.
    if (s.is_moving() || s.is_casting()) return false;
    if (InTransitNoMaintenance(s)) return false;
    // Combat-flicker gate. Conjure spells are 3s hard casts — kicking
    // one off in the brief OOC tick between consecutive trash pulls
    // wastes the GCD when damage lands a heartbeat later and aborts
    // the cast. Same rationale as MaintainOocFood.
    if (s.recently_in_combat(5000)) return false;
    if (s.cls() == CLASS_WARLOCK)
    {
        constexpr uint32 CREATE_HEALTHSTONE = 6201;
        constexpr uint32 HEALTHSTONE_ITEM   = 5512;
        if (!s.has_item(HEALTHSTONE_ITEM) &&
            s.knows_spell(CREATE_HEALTHSTONE) && s.is_ready(CREATE_HEALTHSTONE))
        {
            emit.cast(CREATE_HEALTHSTONE);
            return true;
        }
    }
    if (s.cls() == CLASS_MAGE)
    {
        constexpr uint32 CONJURE_REFRESHMENT = 190336;
        // Conjure Refreshment creates one of several conjured-food entries
        // depending on client/server version: Pandaria's Conjured Mana Cake
        // (80610) is what the current item DB ships, while later expansions
        // shipped 113509 / similar IDs. Check for ANY of the known entries
        // so the rule doesn't re-fire forever when the server creates a
        // different item than our hardcoded constant - the symptom we hit
        // with bot Norithyn standing at a reagent vendor casting Conjure
        // Refreshment over and over because his bag had 80610 but the rule
        // only looked for 113509.
        constexpr uint32 CONJURED_MANA_CAKES[] = {
            80610,   // Pandaria-era; what current item DB ships in playerbot_*
            113509,  // Legion+
            187425,  // Dragonflight
        };
        bool already_have = false;
        for (uint32 e : CONJURED_MANA_CAKES)
            if (s.has_item(e)) { already_have = true; break; }
        if (!already_have &&
            s.knows_spell(CONJURE_REFRESHMENT) && s.is_ready(CONJURE_REFRESHMENT))
        {
            emit.cast(CONJURE_REFRESHMENT);
            return true;
        }
    }
    return false;
}

// Risk-tolerance-driven consumable thresholds. Cautious bots top off
// aggressively (90% HP/mana before re-pulling) and reach for bandages /
// potions early; Reckless bots conserve consumables, accepting more
// downtime risk for gear-and-gold ROI between pulls. Centralised here
// so MaintainOocFood, the bandage rule, and the potion rule all read
// the same per-bot tier instead of hard-coding /60/50/30.
struct IdleConsumeThresholds
{
    int32 food_floor;      // Skip food/drink when both HP% and mana% >= this.
    int32 bandage_below;   // Emit bandage when HP% < this.
    int32 potion_below;    // Emit emergency potion when HP% < this.
};

static IdleConsumeThresholds GetIdleConsumeThresholds(BotAI const& ai)
{
    switch (ai.personality().risk_tolerance)
    {
        case RiskTolerance::Cautious: return {90, 70, 50};
        case RiskTolerance::Careful:  return {75, 60, 40};
        case RiskTolerance::Normal:   return {60, 50, 30};
        case RiskTolerance::Reckless: return {40, 30, 15};
    }
    return {60, 50, 30};
}

// OOC consumable use to top off HP / mana between pulls. Walks the bag for
// any item with class==CONSUMABLE && subclass==FOOD_DRINK and uses it —
// works at all character levels (Refreshing Spring Water, Conjured Mana
// Cake, Algari Mana Tea all share the same subclass). Eating/drinking
// triggers a well-fed buff (when applicable) that lasts an hour, so one
// consume per pull cycle is enough; we re-emit per tick if HP/mana drops
// again between pulls. Returns true if a consumable was emitted.
bool MaintainOocFood(BotSnapshotView const& s, BotAI const& ai, BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    // UseItemIntent doesn't go through is_ready(), so it would happily fire
    // mid-conjure and cancel the cast. Guard explicitly.
    if (s.is_casting()) return false;
    // Combat-flicker gate. TC's IsInCombat() can drop for a single tick
    // between trash deaths even when more mobs are mid-pull at us. Without
    // this, MaintainOocFood would emit UseItemByEntryIntent → bot sits
    // (food/drink applies Sit aura) → next mob lands a hit → bot is
    // damage-skipped out of the sit but with the bandage/food cancelled
    // and a wasted GCD. Refuse to eat for 5s after the most recent combat
    // exit; this is below the Well-Fed re-up cadence and below typical
    // pull spacing.
    if (s.recently_in_combat(5000)) return false;
    // Don't eat when there are quest kill targets ready to engage in
    // scan range. Verified 2026-05-20: Halinen (hunter) finishes a fight
    // at 60% HP → MaintainOocFood triggers because food_floor=60 →
    // bot sits to drink with 14 Young Nightsabers (his quest 28713
    // targets) sitting 30-60y away. Player intent is "kill 6 more
    // nightsabers", not "rest to full". HP regen between pulls is
    // fast at low levels; the rule re-fires after the kill objective
    // completes and actionable_objectives empties.
    for (auto const& ao : s.actionable_objectives())
    {
        if (ao.type == /*MONSTER*/ 0 ||
            ao.type == /*KILL_WITH_LABEL*/ 21 ||
            (ao.type == /*ITEM*/ 1 && ao.source_guid.IsCreature()))
            return false;
    }
    const int32 hp_pct  = s.hp_pct();
    const int32 mp_pct  = s.max_power(0) > 0 ? s.power_pct(0) : 100;
    const auto thr = GetIdleConsumeThresholds(ai);
    // Hysteresis: real humans drink to ~full at the inn, not to the
    // start-threshold. Without this, the bot ate down to food_floor
    // (60% for Normal personality), the gate flipped false at 61%, the
    // bot stood up and walked to the next pull at 61% mana — going OOM
    // 2 pulls later. The "already_consuming" check detects an in-flight
    // Food/Drink aura and keeps the rule firing (so the snapshot keeps
    // emitting the use-item, server keeps the channel up) until the
    // bot is topped to 95% on whichever resource was the trigger.
    // SPELL_AURA_FOOD = aura 433 (food regen), DRINK = 430. Both family
    // ids stable across Midnight 12.0.5+.
    constexpr uint32 kFoodAura  = 433;
    constexpr uint32 kDrinkAura = 430;
    const bool already_consuming =
        s.has_aura(kFoodAura) || s.has_aura(kDrinkAura);
    constexpr int32 kTopOffPct = 95;
    const bool start_now = (hp_pct < thr.food_floor) || (mp_pct < thr.food_floor);
    const bool keep_going = already_consuming
        && (hp_pct < kTopOffPct || mp_pct < kTopOffPct);
    if (!start_now && !keep_going) return false;
    // Find any food/drink in the bag. Picks the highest-entry match as a
    // crude "pick the best/newest food I have" — TWW items have higher entry
    // IDs than Vanilla items. Not perfect (heirloom food breaks the
    // ordering) but good enough until we surface restore-amount in
    // ItemTemplate.Effect. Same scan finds both food and drink (subclass 5
    // covers both — modern items are dual-purpose).
    constexpr uint8 ITEM_CLASS_CONSUMABLE_LOCAL = 0;
    constexpr uint8 ITEM_SUBCLASS_FOOD_DRINK_LOCAL = 5;
    uint32 best_entry = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.item_class != ITEM_CLASS_CONSUMABLE_LOCAL) continue;
        if (it.item_subclass != ITEM_SUBCLASS_FOOD_DRINK_LOCAL) continue;
        if (it.entry > best_entry) best_entry = it.entry;
    }
    if (!best_entry) return false;
    emit.emit(UseItemByEntryIntent{best_entry, ObjectGuid::Empty});
    return true;
}

// (FriendlyDispel + ClassDispelSpell live in Bot/ClassTables.cpp — shared
// with whisper helpers.)

// Out-of-combat dispel sweep. APL handles in-combat dispel as a high-priority
// rule for healers; OOC, lingering debuffs (Disease ticks, Curse % stat
// reductions) just sit and waste mana when the next pull starts. Walks the
// class's dispel coverage and fires on the first candidate found — group
// member first, then self. One emit per tick; subsequent ticks pick up
// remaining candidates as the snapshot regenerates.
bool MaintainOocDispel(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    const ClassDispelSpell d = FriendlyDispel(s.cls(), s.spec());
    if (!d.spell_id) return false;
    if (!s.knows_spell(d.spell_id)) return false;
    if (!s.is_ready(d.spell_id)) return false;

    // Encounter / M+ affix priority — Bursting, Raging-enrage, raid one-shot
    // debuffs are listed in advice.dispel_priority_spells. When a member
    // carries one, dispel them BEFORE the normal type-walk regardless of
    // which type the priority spell is — we'd waste mana clearing a
    // benign Curse if a Bursting tick is killing the tank.
    if (g.exists() && Services::Initialized())
    {
        DungeonAdvice const dav = Services::Dungeons().GetAdvice(s);
        if (!dav.dispel_priority_spells.empty())
        {
            if (GroupMemberSummary const* tgt =
                    g.priority_dispel_candidate(dav.dispel_priority_spells))
            {
                emit.cast(d.spell_id, tgt->guid);
                return true;
            }
        }
    }

    // Group candidates first — keeps dispel work distributed across the
    // healer when teammates are debuffed. dispel_candidate already excludes
    // dead members so we won't waste a cast on a corpse.
    if (g.exists())
    {
        auto try_group = [&](DispelType t) -> GroupMemberSummary const*
        {
            return g.dispel_candidate(t);
        };
        GroupMemberSummary const* tgt = nullptr;
        if (d.magic   && (tgt = try_group(DispelType::Magic)))   { emit.cast(d.spell_id, tgt->guid); return true; }
        if (d.curse   && (tgt = try_group(DispelType::Curse)))   { emit.cast(d.spell_id, tgt->guid); return true; }
        if (d.disease && (tgt = try_group(DispelType::Disease))) { emit.cast(d.spell_id, tgt->guid); return true; }
        if (d.poison  && (tgt = try_group(DispelType::Poison)))  { emit.cast(d.spell_id, tgt->guid); return true; }
    }

    // Self-dispel — solo bots and group members where the bot itself is the
    // sufferer. self_dispellable already filters to harmful auras.
    if ((d.magic   && s.self_dispellable(DispelType::Magic))  ||
        (d.curse   && s.self_dispellable(DispelType::Curse))  ||
        (d.disease && s.self_dispellable(DispelType::Disease))||
        (d.poison  && s.self_dispellable(DispelType::Poison)))
    {
        emit.cast(d.spell_id, s.raw().guid);
        return true;
    }
    return false;
}

// Auto-equip rule. Walks the bag for strict-ilvl upgrades and queues the
// equip swaps in a single tick. Edge-triggered on level changes via BotAI's
// last_auto_equip_level: priming on first observation (no spam at login),
// firing once per fresh ding. Skips when no upgrades pending or in combat.
bool MaintainAutoEquipUpgrades(BotSnapshotView const& s, BotAI& ai,
                               BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    const uint8 cur_level = s.level();
    if (cur_level == 0) return false;
    // Prime on first observation: don't auto-equip at login (the owner may
    // have intentionally swapped down for a transmog / leveling bracket etc).
    if (ai.last_auto_equip_level() == 0)
    {
        ai.set_last_auto_equip_level(cur_level);
        return false;
    }
    // Fire on a level-up edge OR periodically when upgrades sit in the bags.
    // The old level-edge-only gate meant a bot that stopped leveling (the
    // exact bots that need gear most) never equipped anything it looted or
    // was quest-rewarded — verified live: an L34 healer fighting at item
    // level 7 with a bag full of upgrades (audit C10). Humans equip drops
    // as they get them, not only on dings.
    const uint32 equip_now_ms = s.published_at_ms();
    constexpr uint32 kPeriodicEquipMs = 3u * 60u * 1000u;
    const bool level_edge = cur_level > ai.last_auto_equip_level();
    const bool periodic   = s.upgrades_pending() > 0 &&
                            (equip_now_ms - ai.last_auto_equip_check_ms()) >= kPeriodicEquipMs;
    if (!level_edge && !periodic) return false;
    ai.set_last_auto_equip_check_ms(equip_now_ms);
    if (s.upgrades_pending() == 0)
    {
        ai.set_last_auto_equip_level(cur_level);
        return false;
    }
    // Walk bag_items applying same predicate as /upgrades apply: strict ilvl
    // gain over the currently equipped item in the same slot. Server enforces
    // class/level eligibility on apply (Locked rebound silently swallowed).
    //
    // Human-pacing 2026-05-21: emit only ONE equip per tick. Pre-fix the
    // bot fired up to 16 equip_item intents on the same level-up tick —
    // a 200ms-wide burst of inventory clicks that no human could match.
    // Now: break after the first emit; last_auto_equip_level is NOT
    // bumped until all slots are clean, so the next idle tick re-walks
    // and emits the next slot. With snapshot cadence ~250ms, a 5-slot
    // upgrade chain spreads over ~1.25s — close to natural click-by-click
    // human equip pacing.
    auto const& raw = s.raw();
    for (auto const& it : raw.inventory.bag_items)
    {
        if (it.equip_slot == 0xFF) continue;
        // Shield-tank weapon + offhand are owned EXCLUSIVELY by
        // EnsureShieldTankWeapon (gear-backfill 1H+shield enforcer). A 2H scores
        // higher than the 1H it equips, so letting this score-based rule manage
        // those slots re-equips the displaced 2H and evicts the shield — an
        // endless 2H<->1H+shield churn (live 2026-06-28). Prot War (73) / Prot
        // Pala (66) only; 2H tanks (Blood DK / Guardian Druid) keep their weapon.
        if ((s.spec() == 73 || s.spec() == 66) &&
            (it.equip_slot == EQUIPMENT_SLOT_MAINHAND ||
             it.equip_slot == EQUIPMENT_SLOT_OFFHAND))
            continue;
        // Defense-in-depth: builder clamps equip_slot to <19, but indexing
        // equipped[19+] is UB (bag slots are 30-33) — never walk past it.
        if (it.equip_slot >= raw.inventory.equipped.size()) continue;
        if (it.item_level == 0)    continue;
        if (it.quality == 0)       continue;
        // Per-entry retry throttle (shared with idle:equip_upgrade). When the
        // executor REFUSES a swap (level req, proficiency, unique constraint,
        // ...) the item never leaves the bag — and because the watermark
        // below is only bumped when the walk comes up empty, the level-edge
        // gate stayed open and this rule re-emitted the same doomed swap
        // every tick, forever (B-11: 5 refused intents/sec + watchdog
        // ping-pong, live on Uraimus). One attempt per lockout window; once
        // every candidate is throttled the walk falls through to the bump
        // and the rule goes quiet until the next ding / periodic re-check.
        if (ai.equip_recently_tried(it.entry, equip_now_ms)) continue;
        EquippedItem const& cur = raw.inventory.equipped[it.equip_slot];
        // Score-based upgrade test (was pure item_level). 12.0 level-scaling
        // floors low-level whites AND quest greens to the same effective ilvl,
        // so `it.item_level <= cur.item_level` tied and refused real upgrades —
        // leaving organic bots stuck at ItemLevel 1. EquipFitScore breaks the
        // tie on the stat allocation (a green's primary/secondary stats outweigh
        // a starter white) and stays armor/weapon-aware via the spec weights.
        if (cur.entry != 0 &&
            EquipFitScore(it.stats, it.item_level, raw.stat_weights) <=
            EquipFitScore(cur.stats, cur.item_level, raw.stat_weights))
            continue;
        emit.equip_item(it.bag, it.slot, it.equip_slot);
        ai.note_equip_try(it.entry, equip_now_ms);
        return true;  // one per tick; next snapshot drains the next slot
    }
    // No upgrades found (or every candidate is inside its retry lockout) —
    // bump the watermark so we don't re-walk on every tick until the next
    // level. Without this final bump, every tick after the bot reached
    // level N+1 would walk the bag looking for an upgrade that doesn't
    // exist. Refused candidates get their next chance from the periodic
    // re-check (kPeriodicEquipMs), not from a hot loop.
    ai.set_last_auto_equip_level(cur_level);
    return false;
}

// Bag-upgrade rule (B-11b). Containers are deliberately NOT regular
// auto-equip candidates (their FindEquipSlot destination is a bag slot
// 30-33, outside equipped[19] — see the builder's equip_slot clamp), so a
// looted 10-slot bag used to rot in the inventory forever. Pick the
// largest normal container in the bags and either fill an empty bag slot
// or replace the smallest equipped normal bag. The world-thread side
// (API::equip_item bag branch) handles the actual content transfer via
// Player::SwapItem's native empty-bag-for-full-bag exchange; emitting an
// EquipItemIntent with a bag-slot destination is all this rule does.
bool MaintainBagUpgrade(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    auto const& caps = s.equipped_bag_capacity();
    auto const& subs = s.equipped_bag_subclass();
    const uint32 bag_now_ms = s.published_at_ms();
    // Largest normal container in the bags that isn't inside its retry
    // lockout. equip_recently_tried covers both "swap just emitted, item
    // hasn't moved yet" and "executor refused it" (e.g. candidate not
    // empty yet) — one attempt per lockout, periodic re-walk retries.
    InventoryItem const* best = nullptr;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.container_slots == 0) continue;
        if (it.item_class != ITEM_CLASS_CONTAINER) continue;
        if (it.item_subclass != ITEM_SUBCLASS_CONTAINER) continue;   // normal bags only
        if (ai.equip_recently_tried(it.entry, bag_now_ms)) continue;
        if (!best || it.container_slots > best->container_slots) best = &it;
    }
    if (!best) return false;
    // Destination: an empty bag slot is a pure gain regardless of size;
    // otherwise replace the smallest equipped NORMAL bag if the candidate
    // is strictly bigger (profession bags are never replaced — wrong
    // capacity comparison domain).
    int dest = -1;
    for (int i = 0; i < 4; ++i)
        if (caps[i] == 0) { dest = i; break; }
    if (dest < 0)
    {
        uint8 smallest_cap = 255;
        for (int i = 0; i < 4; ++i)
        {
            if (subs[i] != ITEM_SUBCLASS_CONTAINER) continue;
            if (caps[i] < smallest_cap) { smallest_cap = caps[i]; dest = i; }
        }
        if (dest < 0 || best->container_slots <= smallest_cap) return false;
    }
    emit.equip_item(best->bag, best->slot,
                    static_cast<uint8>(INVENTORY_SLOT_BAG_START + dest));
    ai.note_equip_try(best->entry, bag_now_ms);
    return true;
}

// Auto-apply curated talent build for the bot's current context.
// Detection:
//   - In raid map → context Raid (1).
//   - In dungeon map (5-man) → MythicPlus (2).
//   - PvP-flagged (BG / arena) → PvP (3).
//   - Below max level → Leveling (4).
//   - Otherwise → Default (0).
// Edge-triggered via BotAI::last_applied_talent_ctx_ so we only emit
// the intent when the detected context CHANGES. Combat-locked.
//
// Curated builds live in `playerbot_v2_talent_build`; missing rows
// fall back to TraitMgr starter build inside the API method.
bool MaintainContextTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    auto const& raw = s.raw();
    // Pick context. Order matters: PvP > Raid > MythicPlus > Leveling > Default.
    uint8 ctx = 0;
    if (raw.vitals.is_pvp || raw.bg.in_battleground)
        ctx = 3;
    else if (raw.instance_ctx.is_in_raid)
        ctx = 1;
    else if (raw.instance_ctx.is_in_dungeon)
        ctx = 2;
    else if (s.level() < 80)  // pragmatic level cap proxy; tune to MAX_LEVEL
        ctx = 4;
    else
        ctx = 0;

    if (ai.last_applied_talent_ctx() == ctx) return false;
    // Leveling (ctx 4) has NO curated build row; apply_talent_build would fall
    // back to the context-0 ENDGAME (max-level) build and over-spend a low-level
    // bot's tiny, level-gated talent budget — observed as L11 Uraimus carrying a
    // full ~70-node L80 loadout + 186-spell book. Use TraitMgr's level-budgeted
    // Starter Build for leveling bots instead (it only spends what the bot can
    // actually afford at its level). Endgame/instance/PvP contexts still apply
    // their curated build.
    if (ctx == 4)
        emit.emit(ApplyStarterTalentsIntent{});
    else
        emit.emit(ApplyTalentBuildIntent{ctx});
    ai.set_last_applied_talent_ctx(ctx);
    return true;
}

// Auto-apply Blizzard's starter trait config when the bot's active talent
// loadout is empty (e.g. fresh bot, or post-respec/spec-swap with no saved
// build). Edge-triggered via BotAI::starter_talents_acked_ so we only emit
// the intent once per empty observation; gets reset when active_talents
// becomes populated, allowing re-fire after a future respec.
bool MaintainStarterTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    // Specs + talents start at L10 (retail): below that there is no trait
    // config to fill and the intent is a guaranteed no-op.
    if (s.level() < 10) return false;
    auto const& raw = s.raw();
    if (!raw.spellbook.active_talents.empty())
    {
        if (ai.starter_talents_acked()) ai.set_starter_talents_acked(false);
        return false;
    }
    // CONVERGENCE, not fire-once (audit B05/B08): the old single-shot latch
    // meant one failed commit (interrupted cast, missing spec) left the bot
    // at 0 talents forever. Re-emit on a slow cadence while the trait-system
    // signal (active_talents, now read from the ACTIVE TraitConfig) stays
    // empty; the ack only suppresses the spam between attempts.
    constexpr uint32 kRetryMs = 2u * 60u * 1000u;
    if (ai.starter_talents_acked() &&
        (s.published_at_ms() - ai.starter_talents_emit_ms()) < kRetryMs)
        return false;
    emit.emit(ApplyStarterTalentsIntent{});
    ai.set_starter_talents_acked(true);
    ai.set_starter_talents_emit_ms(s.published_at_ms());
    return true;
}

// Extend the starter trait config when the bot dings (so newly-granted trait
// points get spent without owner intervention). Only fires when bot is
// flagged as using the StarterBuild — never wipes a custom build the owner
// curated. Edge-triggered via BotAI::last_starter_extend_level so we re-fire
// at most once per ding even if the bot lingers OOC after the level-up.
bool MaintainStarterTalentsExtend(BotSnapshotView const& s, BotAI& ai,
                                  BotIntentEmitter& emit)
{
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    if (!s.is_starter_build()) return false;
    const uint8 cur = s.level();
    if (cur == 0) return false;
    // Prime on first observation: avoid an apply at login (the apply fires a
    // 1.5s "applying talents" cast which would feel surprising on join).
    if (ai.last_starter_extend_level() == 0)
    {
        ai.set_last_starter_extend_level(cur);
        return false;
    }
    if (cur <= ai.last_starter_extend_level()) return false;
    emit.emit(ApplyStarterTalentsIntent{});
    ai.set_last_starter_extend_level(cur);
    return true;
}

// Out-of-combat resurrection spell for the bot's class. Returns 0 if the
// class doesn't have one (DK / Warlock use battle-rezzes wired into APL).
static uint32 ClassOocRez(uint8 cls)
{
    switch (cls)
    {
        case CLASS_PRIEST:  return 2006;       // Resurrection
        case CLASS_DRUID:   return 50769;      // Revive
        case CLASS_SHAMAN:  return 2008;       // Ancestral Spirit
        case CLASS_PALADIN: return 7328;       // Redemption
        case CLASS_MONK:    return 115178;     // Resuscitate
        case CLASS_EVOKER:  return 361227;     // Return
        default:            return 0;
    }
}

// A bot whose ONLY attackers are the untargetable 49521 Vanessa Lightning
// Stalkers — no fightable attacker AND no real victim — is in pure FALSE
// combat: the server holds it InCombat (UNINTERACTIBLE faction-14 triggers that
// deal no damage) yet there is nothing it can fight or flee. Gating dungeon
// maintenance / recovery on the bare in_combat() flag makes the whole harbor
// group go dark under the ~56-stalker flood — rez / heal / wipe-regroup /
// escort all freeze, so the post-death spiral never recovers (observed live
// 2026-06-27: tank+healer died at the harbor crossing, survivors stalker-pinned,
// no rez ever cast → cascade wipe → world-thread crash). Treat stalker-only
// combat as NOT combat for those gates; any fightable attacker or a real victim
// means a genuine fight and is excluded. fightable_attackers_count() is the
// stalker-free count — the same predicate proven in DungeonRecoverStranded-
// Follower's real_combat_strand.
static bool DungeonRealCombat(BotSnapshotView const& s)
{
    return s.in_combat() &&
           (s.fightable_attackers_count() > 0 || !s.victim().IsEmpty());
}

// (ClassOocHeal lives in Bot/ClassTables.cpp — shared with whisper helpers.)

// Out-of-combat group topup. Picks the most-wounded group member on our map
// and casts the spec's basic heal if we're a healer. 80% threshold avoids
// burning mana on near-full members; the next pull will fill them from a
// clean baseline. Single emit per tick — multiple wounded members get healed
// across consecutive ticks as the snapshot regenerates.
bool MaintainOocHeal(BotSnapshotView const& s, GroupSnapshotView const& g,
                     BotIntentEmitter& emit, Role effective_role)
{
    if (DungeonRealCombat(s)) return false;   // stalker-only false-combat still heals
    if (s.is_casting()) return false;
    if (effective_role != Role::Healer) return false;
    if (!g.exists()) return false;
    const uint32 spell = ClassOocHeal(s.cls(), s.spec());
    if (!spell) return false;
    if (!s.knows_spell(spell)) return false;
    if (!s.is_ready(spell)) return false;
    auto const* tgt = g.lowest_hp_on_map(s.map_id());
    if (!tgt) return false;
    if (tgt->hp_pct() >= 80) return false;
    emit.cast(spell, tgt->guid);
    return true;
}

// Returns true if the bot emitted an OOC rez on a fallen group member.
// Only fires when out-of-combat; battle-rezzes are handled by combat APL.
bool MaintainOocRez(BotSnapshotView const& s, GroupSnapshotView const& g,
                    BotIntentEmitter& emit)
{
    if (DungeonRealCombat(s)) return false;   // stalker-only false-combat still rezzes
    if (!g.exists())   return false;
    // Pass map_id: dead_member() filters out corpses on a different map —
    // cast would just fail InvalidTarget (common case: someone hearthed away
    // while dead).
    auto const* dead = g.dead_member(s.map_id());
    if (!dead) return false;

    // Mass Resurrection (Priest 212036) — prefer when 3+ dead on this map and
    // the spell is ready (5min CD). Casts on the bot itself; server applies
    // the AoE rez to all dead group members in range. Saves cast time per
    // member after a wipe.
    if (s.cls() == CLASS_PRIEST)
    {
        constexpr uint32 kMassResurrection = 212036;
        if (s.knows_spell(kMassResurrection) && s.is_ready(kMassResurrection)
            && g.count_dead(s.map_id()) >= 3)
        {
            // Multi-priest dedup: skip if any other priest is already mid-cast
            // on Mass Resurrection. 5-min CD makes redundant casts wasteful.
            bool another = false;
            if (auto const* mems = g.members())
            {
                for (auto const& m : *mems)
                {
                    if (m.guid == s.raw().guid) continue;
                    if (m.is_casting && m.casting_spell == kMassResurrection)
                    { another = true; break; }
                }
            }
            if (!another)
            {
                emit.cast(kMassResurrection, s.raw().guid);
                return true;
            }
        }
    }

    // Distance gate (2026-06-27): only single-target OOC-rez a corpse within rez
    // cast range. dead_member() has NO range filter, so a corpse far back (live:
    // the hunter dead ~150y away at the Glubtok room while the group pushes the
    // harbor) otherwise makes the healer re-emit a doomed out-of-range rez every
    // ready tick (idle:dungeon_ooc_rez), pinning it short-handed instead of
    // healing/advancing the harbor push. The walk-into-range fetch is owned by
    // idle:dungeon_go_rez (kRezCastRange/kFetchMaxY); here we simply refuse a cast
    // that cannot land. (Mass Resurrection above is self-cast AoE — left ungated.)
    {
        float sx, sy, sz; s.position(sx, sy, sz);
        const float dxr = dead->x - sx, dyr = dead->y - sy, dzr = dead->z - sz;
        constexpr float kRezCastRange = 28.0f;
        if (dxr * dxr + dyr * dyr + dzr * dzr > kRezCastRange * kRezCastRange)
            return false;
    }
    const uint32 rez = ClassOocRez(s.cls());
    if (!rez) return false;
    if (!s.knows_spell(rez)) return false;
    if (!s.is_ready(rez))    return false;
    // Cross-caster dedup. Multiple Priests/Druids/Paladins/Shamans/Monks
    // all see the same first-dead member; without coordination they each
    // start their respective rez cast (8-10s OOC rez) and only the first
    // lands. Skip if another raid member is already mid-cast on a known
    // OOC rez spell targeting the same corpse.
    if (auto const* mems = g.members())
    {
        for (auto const& m : *mems)
        {
            if (m.guid == s.raw().guid) continue;
            if (!m.is_casting) continue;
            if (m.casting_target != dead->guid) continue;
            switch (m.casting_spell)
            {
                case 2006: case 50769: case 2008: case 7328:
                case 115178: case 361227:
                    return false;  // someone else is casting OOC rez on this corpse
                default: break;
            }
        }
    }
    emit.cast(rez, dead->guid);
    return true;
}

// ---------- Shared dungeon-combat positioning (BUG G-P0a) ----------
// The avoidance / mechanic positioning rules below were originally inline
// in DungeonDispatch (the idle cascade) and therefore NEVER ran during
// combat — the FSM routes combat ticks to DispatchInCombat. They are now
// factored into ONE implementation called from both the idle dungeon path
// and the top of DispatchInCombat, so a bot standing in boss fire / failing
// to spread/stack/soak/kite/melee-behind during a fight is fixed at the
// structural level instead of being silently dead code.
//
// Priority order (avoidance / mechanics BEFORE the cosmetic melee-behind):
//   dangerous-aura step-out → kite-fixate → soak → spread → stack →
//   melee-behind.
// Returns true when any sub-rule emitted a positioning move (caller consumes
// the tick / suppresses the chase). Cheap early-out when the advice carries
// no positioning data.
//
// Forward declaration: rule (0) below pushes the tank toward a reachable boss
// using the same off-mesh-aware stepping the idle boss-navigator uses; the
// definition lives further down (after the idle dispatch helpers).
static bool DungeonTargetReachableAndStep(Player* self, float tx, float ty,
                                          float tz, float maxStep,
                                          G3D::Vector3& step_out,
                                          bool* step_is_offmesh = nullptr,
                                          bool allow_incomplete_progress = false);
// Forward declaration: tank detour-ratio verdict (2026-07-02 SFK wedge, Task 1).
// Read-only measurement helper — definition lives just after
// DungeonTargetReachableAndStep, whose PathGenerator call shape it copies.
struct DungeonDetourVerdict;
static DungeonDetourVerdict DungeonTankDetour(Player* tank, float tx, float ty, float tz);
static bool DungeonDetourExcessive(DungeonDetourVerdict const& v);
// Forward declaration: scalar-out wrapper around DungeonTankDetour +
// DungeonDetourExcessive (2026-07-02, Task 3) for callers positioned ahead
// of DungeonDetourVerdict's definition (dungeon:untankable_disengage below,
// in DungeonCombatPositioning) — an incomplete struct type cannot be held
// by value this early in the translation unit, so the verdict fields the
// caller needs for its diagnostic log are returned via out-params instead.
// Definition lives right after DungeonDetourExcessive further down.
static bool DungeonTankDetourExcessiveVerbose(Player* tank, float tx, float ty, float tz,
                                              float& out_beeline, float& out_path_len,
                                              float& out_ratio, bool& out_complete);
// Forward declaration: rule (-1) at the top of DungeonCombatPositioning recovers
// an off-mesh-stranded dungeon bot WHILE IN COMBAT (the idle DungeonDispatch nudge
// can't run when the FSM has routed the bot to State_InCombat). Definition further
// down with the other idle dungeon helpers.
static bool DungeonNudgeOntoMesh(Player* self, BotIntentEmitter& emit, BotAI& ai);
// Shared (declared in MaintainHelpers.h): GUID-keyed wrapper around
// DungeonTargetReachableAndStep with FLOAT out-params, so other state files
// (State_InGroup's dungeon-rejoin) can emit the SAME off-mesh far-vertex step
// the idle regroup-cross uses — without this, a follower that alternates the
// Idle (far vertex) and InGroup (tank position) states flip-flops the move_to
// goal-key, defeats the spline dedup, and never completes the Gap-1 hop. Keeps
// the G3D::Vector3 / Player* types out of the shared header.
bool DungeonStepTowardTank(ObjectGuid self_guid, float tx, float ty, float tz,
                           float maxStep, float& ox, float& oy, float& oz,
                           bool& is_offmesh)
{
    Player* self = ObjectAccessor::FindConnectedPlayer(self_guid);
    if (!self) return false;
    G3D::Vector3 step;
    bool off = false;
    if (!DungeonTargetReachableAndStep(self, tx, ty, tz, maxStep, step, &off))
        return false;
    ox = step.x; oy = step.y; oz = step.z; is_offmesh = off;
    return true;
}

// Route-aware step toward an arbitrary target position (2026-07-22). Fallback
// for a FOLLOWER whose DIRECT path to the tank is broken by a navmesh gap: the
// tank route-follows DOWN a descent (RFC Adarogg pit — the DB crumb-route steps
// z-29→-61 in navigable ~24y hops, crumbs 5-13), then the follower's rejoin
// BEELINES the 60y straight to the tank, NoPaths across the gap, and strands —
// halting the whole group on the cohesion gate (live 2026-07-22: Dunghealer
// NoPath (-141,-23,-29)->(-203,-36,-49), group frozen 0/4). Instead, walk the
// follower along the SAME crumb chain the tank used: find the crumb nearest the
// tank (ti) and nearest self (fi), and step to the next crumb from fi toward ti
// that is actually reachable. Each hop is short + on-mesh, so the follower
// descends exactly where the tank did. Returns false when there is no route,
// self and tank share a crumb (direct step should have worked), or the very
// next crumb is itself unreachable (a real gap the route can't bridge — leave
// it to strand-recovery). Fallback-ONLY: callers invoke it after the direct
// step fails, so reachable rejoins and non-routed dungeons are byte-unchanged.
bool DungeonRouteStepTowardPos(BotSnapshotView const& s,
                               float tx, float ty, float tz, float maxStep,
                               float& ox, float& oy, float& oz)
{
    DungeonAdvice const advice = Services::Dungeons().GetAdvice(s);
    auto const& R = advice.route_waypoints;
    if (R.size() < 2) return false;
    Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
    if (!self) return false;
    float sx, sy, sz; s.position(sx, sy, sz);
    auto nearest = [&](float x, float y, float z) -> int {
        int bi = 0; float bd = 1.0e18f;
        for (int i = 0; i < static_cast<int>(R.size()); ++i)
        {
            const float dx = R[i].x - x, dy = R[i].y - y, dz = R[i].z - z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < bd) { bd = d; bi = i; }
        }
        return bi;
    };
    const int ti = nearest(tx, ty, tz);
    const int fi = nearest(sx, sy, sz);
    if (ti == fi) return false;               // co-located on the route
    const int dir = (ti > fi) ? 1 : -1;
    // FORWARD-ONLY guard (2026-07-23): the crumb we step to MUST be closer to the
    // target than we currently are. Without this, when self is FAR from the target
    // (>120y), every crumb near the target is skipped by the range cap and the scan
    // falls back to a crumb near SELF — which is not forward at all and DRAGGED the
    // group backward toward a mis-forward waypoint (live Deadmines: tank at the
    // entrance, target Cookie deep in the ship, helper returned an entrance crumb
    // and yo-yoed the group entrance<->deep). Requiring net progress toward the
    // target makes the escape/rejoin either advance or (no forward crumb reachable)
    // return false so the caller disengages instead of walking the wrong way.
    const float stx = tx - sx, sty = ty - sy, stz = tz - sz;
    const float self_to_target2 = stx * stx + sty * sty + stz * stz;
    // Step to the FARTHEST-along crumb (toward ti) that is reachable from self in
    // one step — scanning from ti BACK toward fi, not fi forward. This makes
    // maximal monotonic forward progress and is immune to route SELF-CROSSINGS:
    // the RFC Slagmaw->Gordoth leg doubles back (crumbs 31 and 33 share a
    // position), so a plain next-crumb step oscillated the escape (-249,167) <->
    // (-226,157) forever (live 2026-07-23, tank held 187s). Skip crumbs already
    // reached (<6y) and ones too far to be this step's anchor (>120y straight
    // line — bounds the reachability probes and never lines the bot up on a
    // cross-cavern crumb whose mesh path would cut back through the lock).
    for (int i = ti; i != fi; i -= dir)
    {
        auto const& rw = R[size_t(i)];
        const float dx = rw.x - sx, dy = rw.y - sy, dz = rw.z - sz;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < 6.0f * 6.0f) continue;             // already at/through this crumb
        if (d2 > 120.0f * 120.0f) continue;         // too far to anchor this step
        // FORWARD only: the crumb must be closer to the target than we are.
        const float ctx = tx - rw.x, cty = ty - rw.y, ctz = tz - rw.z;
        if (ctx * ctx + cty * cty + ctz * ctz >= self_to_target2) continue;
        G3D::Vector3 step;
        if (DungeonTargetReachableAndStep(self, rw.x, rw.y, rw.z, maxStep, step))
        { ox = step.x; oy = step.y; oz = step.z; return true; }
    }
    return false;
}

// OOC dungeon step-hold (2026-07-03 WC/SFK stutter fix). True when the bot's
// LIVE spline (snapshot path_end, populated for POINT motion) is already
// heading within kStepReplanRange (3y) of (tx,ty,tz) AND the bot is moving.
// Callers skip their emit (and usually claim a hold tick) — re-issuing
// MovePoint would RESTART the spline (the OOC stutter engine; the API
// layer's 3s/4y stall-breaker re-issues on a jittering bot,
// PlayerbotAPI.cpp:838-841). Mirrors the melee chase guard
// (State_InCombat.cpp:1754). Config kill switch:
// PlayerbotV2.Move.StepHoldEnabled.
static bool DungeonStepAlreadyInFlight(BotSnapshotView const& s,
                                       float tx, float ty, float tz)
{
    if (!Services::Config().move_step_hold_enabled())
        return false;
    if (!s.is_moving() || !s.has_path_destination())
        return false;
    float px, py, pz;
    s.path_destination(px, py, pz);
    float const dx = tx - px, dy = ty - py, dz = tz - pz;
    constexpr float kStepReplanRange = 3.0f;   // melee-guard tolerance
    return (dx * dx + dy * dy + dz * dz) < kStepReplanRange * kStepReplanRange;
}

// Throttled [step_hold] diag — ONE log site for the whole step-hold family
// (2026-07-03) so forensics can grep a single tag instead of chasing
// per-site duplicates; rule_tag identifies which OOC emit site held.
static void DungeonStepHoldDiag(BotSnapshotView const& s, char const* rule_tag,
                                float tx, float ty, float tz)
{
    static uint32 s_step_hold_dbg_ms = 0;
    const uint32 now = GameTime::GetGameTimeMS();
    if (now - s_step_hold_dbg_ms > 1500u)
    {
        s_step_hold_dbg_ms = now;
        TC_LOG_INFO("playerbot.v2",
            "[step_hold] bot={} rule={} dest=({:.1f},{:.1f},{:.1f})",
            s.bot_id(), rule_tag, tx, ty, tz);
    }
}

// Refusal-aware target selection (2026-07-20). True when API::move_to
// refused (Result::Locked) this EXACT destination recently — see
// BotAI::move_refused_recently's header comment for the poison-loop bug
// this fixes (a rule that keeps re-selecting the same refused destination
// re-arms the API's own path-fail backoff forever, freezing the bot over a
// perfectly valid navmesh). Callers must NOT emit toward a refused
// destination and must NOT claim the tick for it — fall through so the
// next rule/candidate gets tried instead. Gated on the same kill switch as
// the step-hold family (one concept: spline/emit protection, no new key).
static bool DungeonStepRefused(BotSnapshotView const& s, BotAI& ai,
                               float tx, float ty, float tz, uint32 now_ms)
{
    if (!Services::Config().move_step_hold_enabled())
        return false;
    return ai.move_refused_recently(tx, ty, tz, now_ms);
}

// Throttled [step_refused] diag — mirrors DungeonStepHoldDiag's one-log-site
// shape for the refusal-skip family; rule_tag identifies which emit site
// declined a poisoned destination.
static void DungeonStepRefusedDiag(BotSnapshotView const& s, char const* rule_tag,
                                   float tx, float ty, float tz)
{
    static uint32 s_step_refused_dbg_ms = 0;
    const uint32 now = GameTime::GetGameTimeMS();
    if (now - s_step_refused_dbg_ms > 1500u)
    {
        s_step_refused_dbg_ms = now;
        TC_LOG_INFO("playerbot.v2",
            "[step_refused] bot={} rule={} dest=({:.1f},{:.1f},{:.1f})",
            s.bot_id(), rule_tag, tx, ty, tz);
    }
}

// Throttled [move_owned] diag (increment 1h, 2026-07-03; extended increment
// 1k, 2026-07-18 with the caller's rule tag + the objective on both sides —
// the long-standing diag gap: forensics could see the competing XYZs but not
// WHICH rule lost the window or whether the two sides shared an objective)
// — ONE log site for the whole commitment-arbitration family so forensics
// can grep who deferred to whom, instead of chasing per-site duplicates.
// owner_age_ms/prog_age_ms (increment 1m, 2026-07-20): the in-flight
// commitment's total age and its age-since-progress-last-improved — live
// forensics for the PROGRESS-STICKY fix (BotAI::move_commit_active) needs
// to see whether a held window is genuinely still closing distance
// (prog_age small) or stalled (prog_age approaching kMoveCommitStallMs).
static void DungeonMoveOwnedDiag(BotSnapshotView const& s,
                                 char const* rule_tag,
                                 float tx, float ty, float tz, int32_t my_objective,
                                 float ox, float oy, float oz, int32_t owning_objective,
                                 uint32 owner_age_ms, uint32 prog_age_ms)
{
    static uint32 s_move_owned_dbg_ms = 0;
    const uint32 now = GameTime::GetGameTimeMS();
    if (now - s_move_owned_dbg_ms > 1500u)
    {
        s_move_owned_dbg_ms = now;
        TC_LOG_INFO("playerbot.v2",
            "[move_owned] bot={} rule={} my_target=({:.1f},{:.1f},{:.1f}) my_obj={} "
            "owning_target=({:.1f},{:.1f},{:.1f}) owning_obj={} owner_age={}ms prog_age={}ms",
            s.bot_id(), rule_tag, tx, ty, tz, my_objective, ox, oy, oz, owning_objective,
            owner_age_ms, prog_age_ms);
    }
}

// Increment 1h (2026-07-03): true when ANOTHER movement objective currently
// owns the spline — an active commitment (fresh + bot still moving) toward a
// target genuinely far (>3y) from THIS caller's target. See
// BotAI::move_commit_active's header comment for the full failure this
// fixes (live WC route-step-vs-converge-to-fight alternation every ~150ms,
// net crawl ~0.3y/s — DungeonStepAlreadyInFlight above only suppresses
// SAME-target re-emits and cannot arbitrate BETWEEN two different
// objectives). Gated on the SAME kill switch as step-hold
// (PlayerbotV2.Move.StepHoldEnabled — one concept: spline protection, no new
// key). A SAME-target caller (<=3y) is NOT "owned elsewhere": it hits
// DungeonStepAlreadyInFlight at the call site first, which already holds/
// refreshes for it, so this helper never needs to special-case that path.
//
// Increment 1k (2026-07-18): a SECOND ownership path, keyed on the OBJECTIVE
// rather than the XYZ. Two rules can target the exact same route crumb yet,
// through different capped steppers from different start positions, land
// >3y apart (live WC crumb-27 lip: route rule's direct-descent step vs rule
// (0)'s east-switchback point, 21y apart, both aimed at crumb 27) — the pure
// -XYZ check above sees "different target" and lets both fight for the
// window, each spline-restarting the other every 2.5s forever. my_objective
// >= 0 identifies a crumb-based caller; when it equals the in-flight
// commitment's objective, that caller owns the window regardless of XYZ
// distance — the two rules agree on WHERE they are ultimately going, so the
// in-flight path solution should run to completion rather than be re-aimed.
// Objective-less callers (my_objective < 0: rejoin/converge/direct-advance/
// off-mesh-recovery) and callers whose objective differs from the owner's
// are UNCHANGED — they still fall through to the pure-XYZ check.
static bool DungeonMoveOwnedElsewhere(BotSnapshotView const& s, BotAI& ai,
                                      float tx, float ty, float tz, uint32 now_ms,
                                      char const* rule_tag, int32_t my_objective = -1)
{
    if (!Services::Config().move_step_hold_enabled())
        return false;
    if (!s.is_moving())
        return false;
    if (!ai.move_commit_active(s.map_id(), now_ms))
        return false;
    float ox, oy, oz;
    ai.move_commit_target(ox, oy, oz);
    const int32_t owning_objective = ai.move_commit_objective(s.map_id());
    const uint32 owner_age_ms = ai.move_commit_age_ms(now_ms);
    const uint32 prog_age_ms = ai.move_commit_prog_age_ms(now_ms);
    if (my_objective >= 0 && my_objective == owning_objective)
    {
        // Same objective, different capped step point: the in-flight
        // solution owns the window even though it is >3y from mine.
        DungeonMoveOwnedDiag(s, rule_tag, tx, ty, tz, my_objective,
                             ox, oy, oz, owning_objective, owner_age_ms, prog_age_ms);
        return true;
    }
    const float dx = tx - ox, dy = ty - oy, dz = tz - oz;
    constexpr float kOwnedRange = 3.0f;   // matches DungeonStepAlreadyInFlight
    if (dx * dx + dy * dy + dz * dz <= kOwnedRange * kOwnedRange)
        return false;   // same target — not a competing objective
    DungeonMoveOwnedDiag(s, rule_tag, tx, ty, tz, my_objective, ox, oy, oz, owning_objective,
                         owner_age_ms, prog_age_ms);
    return true;
}

// Increment 1b (2026-07-03): ONE step source for the in-combat boss-advance
// family. Rule (0) idle:dungeon_combat_advance_boss and (0c) the ghost-wedge/
// harbor fighting-advance each used to compute their own DIRECT step toward
// the boss's raw position, while idle:dungeon_tank_advance_boss_route (the
// OOC route follower) strides toward the current route crumb — on a map with
// route waypoints those two destinations genuinely differ (that's WHY the
// route exists: the direct line crosses a navmesh pocket), so the two rule
// families alternately emit and re-aim the spline forever (live-evidenced WC
// crumb-14 wedge, 2026-07-03: route rule at (20.7,406.6) vs combat_advance's
// direct (-14.8,395.1), 35y apart, permanent tank combat-flicker). Fix: when
// the map has route waypoints and the cursor is armed, step toward the SAME
// crumb the route rule owns instead of the boss. The cursor is owned by
// idle:dungeon_tank_advance_boss_route (selection, monotonic commit,
// leapfrog) — this is READ-ONLY and never arms/advances it.
// Substitutes ONLY the walk TARGET passed to DungeonTargetReachableAndStep.
// Callers must keep using bossX/Y/Z (unchanged) for engage-range / LoS / pull
// checks — proximity-to-boss logic must never see the crumb, only the step
// does; conflating the two would let a far crumb falsely arm/disarm a pull.
// Falls back to the boss (tx/ty/tz = bossX/Y/Z, no diag) when:
//   * the kill switch PlayerbotV2.Move.RouteAwareCombatAdvance is off;
//   * the dungeon has no route_waypoints;
//   * the cursor is unarmed or stale for THIS map (cur<0 or out of range —
//     dungeon_route_wp() is map-bound and already clamps that);
//   * the REACHED-LATCH is armed for this exact (crumb index, map): the bot
//     was already observed within kRouteArrive of this crumb (see below) —
//     hysteresis so combat micro-movement straddling the 8y boundary cannot
//     flip the walk target crumb<->boss every tick (where the two diverge
//     >3y each flip would defeat DungeonStepAlreadyInFlight and restart the
//     spline — the stateless-threshold oscillation the route follower's
//     committed cursor exists to kill). The latch clears itself when the
//     route rule advances the cursor (recorded index goes stale), the map
//     changes (map-bound sentinel), or the bot is displaced beyond the
//     kLatchReleaseY (12y) band — a latch armed by a TRANSIENT brush past
//     the crumb must not outlive the displacement, or this helper walks
//     boss-ward forever while the route rule still steers to the armed
//     crumb (live WC yo-yo, 2026-07-03);
//   * the bot is within kRouteArrive (8y, same constant as the route
//     follower) of the crumb — the idle route rule just hasn't advanced the
//     cursor yet; stepping toward an already-reached crumb would orbit it.
//     This is also the ONLY place the reached-latch is SET.
// Otherwise: follow the armed cursor's crumb UNCONDITIONALLY when >8y away.
// The monotonic cursor IS the committed navigation state, and route
// segments legitimately move AWAY from the boss on detours — that is the
// entire reason routes exist (WC serpentine east ramp, crumbs 13-15, live
// 2026-07-03: crumb 14 at (33.6,408.8) is ~185y from boss 3669 vs the
// tank's ~167y, so a boss-distance forward-progress guard — Increments
// 1b/1c — vetoed the substitution exactly on the detour and split the
// navigator into two opposite-direction emitters re-aiming the spline at
// tick rate; tank frozen at (15.5,406.1), MovePoint restarts 6-7x/sec,
// zero net motion). Worst-case a displaced bot walks BACK one crumb
// spacing to rejoin the route — and rejoining the route after displacement
// is correct; a guard that second-guesses the cursor is not (Increment 1f,
// which removed the guard and its canonical-boss 500y scan).
static void DungeonAdvanceTarget(BotSnapshotView const& s, BotAI& ai,
                                 DungeonAdvice const& advice,
                                 float bossX, float bossY, float bossZ,
                                 float& tx, float& ty, float& tz,
                                 char const* rule_tag,
                                 int32_t* out_crumb_idx = nullptr,
                                 bool* out_yield = nullptr)
{
    tx = bossX; ty = bossY; tz = bossZ;
    // Increment 1k (2026-07-18): the objective (route-crumb index) this step
    // serves, for the movement-commitment layer's same-objective ownership
    // check (see DungeonMoveOwnedElsewhere). -1 whenever the substitution is
    // NOT active (boss fallback / arrived-latch) — set below the ONE place
    // the substitution actually fires (tx = wp.x), so every early return
    // here keeps this at -1 without needing to touch each return site.
    if (out_crumb_idx) *out_crumb_idx = -1;
    if (!Services::Config().route_aware_combat_advance()) return;
    if (advice.route_waypoints.empty()) return;
    int const cur = ai.dungeon_route_wp(s.map_id());
    if (cur < 0 || cur >= int(advice.route_waypoints.size())) return;
    // Route CONSUMED for this crumb: the follower already declined here at
    // the arrived final crumb and handed the approach to the boss-ward
    // fallback. Never re-select it — re-selection is what dragged the tank
    // back into the 20y patrol loop once the reached-latch released.
    if (ai.route_consumed_idx(s.map_id()) == cur) return;
    auto const& wp = advice.route_waypoints[size_t(cur)];
    float bx2, by2, bz2; s.position(bx2, by2, bz2);
    constexpr float kRouteArrive = 8.0f;   // matches the route follower
    const float dx = wp.x - bx2, dy = wp.y - by2, dz = wp.z - bz2;
    const float crumb_d2 = dx * dx + dy * dy + dz * dz;
    // Reached-latch hysteresis (see header comment) — honored only inside a
    // release band. Within kLatchReleaseY of the latched crumb the boss
    // target holds (no crumb<->boss flap across the 8y arrive line). But a
    // bot displaced FARTHER than the band never truly settled there: live
    // deadlock 2026-07-03 (WC east ramp) — one 2.5s movement window brushed
    // <8y of crumb 15 and armed the latch, but the OOC route rule never got
    // the pass that advances the cursor; from then on this helper sent rule
    // (0) WEST toward the boss while the route rule steered EAST to the
    // still-armed crumb — a permanent two-objective yo-yo the movement-
    // commitment layer faithfully paced at 2.5s per leg. Beyond the band:
    // clear the latch and resume following the cursor (rejoining the route
    // after displacement is correct; a latch that outlives the displacement
    // is not).
    // Latched / arrived near the cursor's crumb: YIELD (no step) instead of
    // walking boss-ward. Pre-1d, the boss fallback was a real navigation
    // option; post route-ownership it is actively harmful here — live WC
    // crumb-27 lip, 2026-07-19 00:32 verdict: latch armed by a transient
    // brush, rule (0) in the 8-12y dead zone walked BOSS-ward whose mesh
    // path wraps through the SAME east switchback (my_obj=-1 NE step) while
    // the route rule steered the crumb (obj=27) — an objective-less pair
    // the 1k arbitration correctly cannot unify. Near-arrived means the
    // route rule owns completion/advance; the in-combat advance simply does
    // not step this tick (callers fall through / hold per their own shape).
    if (ai.adv_route_reached_idx(s.map_id()) == cur)
    {
        constexpr float kLatchReleaseY = 12.0f;
        if (crumb_d2 <= kLatchReleaseY * kLatchReleaseY)
        {
            if (out_yield) *out_yield = true;
            return;
        }
        ai.set_adv_route_reached(-1, s.map_id());   // transient arrival: unlatch
    }
    if (crumb_d2 < kRouteArrive * kRouteArrive)
    {
        // Arrived at the cursor's crumb: arm the latch so post-arrival drift
        // back across 8y cannot re-select this same crumb, then yield.
        ai.set_adv_route_reached(cur, s.map_id());
        if (out_yield) *out_yield = true;
        return;
    }
    // Unconditional crumb-follow past this point (Increment 1f — see the
    // header comment): no forward-progress / boss-distance guard, no
    // canonical-boss resolution. The cursor is the committed navigation
    // state; second-guessing it against ANY boss position vetoes exactly
    // the detour segments routes exist for.
    tx = wp.x; ty = wp.y; tz = wp.z;
    if (out_crumb_idx) *out_crumb_idx = cur;

    // Throttled [adv_route] diag — ONE log site, fires only while the
    // substitution is actually active, so live forensics can grep it to
    // confirm both rule families are now aiming at the same crumb. boss_d
    // is the distance to the CALLER's boss (the coords substituted away).
    static uint32 s_adv_route_dbg_ms = 0;
    const uint32 now = GameTime::GetGameTimeMS();
    if (now - s_adv_route_dbg_ms > 1500u)
    {
        s_adv_route_dbg_ms = now;
        const float bdx = bossX - bx2, bdy = bossY - by2, bdz = bossZ - bz2;
        TC_LOG_INFO("playerbot.v2",
            "[adv_route] bot={} rule={} cur=({:.1f},{:.1f},{:.1f}) "
            "crumb=({:.1f},{:.1f},{:.1f}) idx={} boss_d={:.1f}",
            s.bot_id(), rule_tag, bx2, by2, bz2, wp.x, wp.y, wp.z, cur,
            std::sqrt(bdx * bdx + bdy * bdy + bdz * bdz));
    }
}

// Increment 1d (2026-07-03): true when the OOC route follower
// (idle:dungeon_tank_advance_boss_route below) has an armed cursor for this
// map — same knob/state as DungeonAdvanceTarget above (the in-combat
// sibling of this same split), one concept, no new key. When armed, the
// plain direct-step advance branches (strict stride / leashed same-Z /
// generic trash-chase) must defer to the route rule instead of computing
// their own destination: on WC the direct line to the boss and the route's
// committed crumb genuinely differ (that is WHY the route exists — the
// direct line crosses a navmesh pocket), so letting both fire alternately
// re-aims the spline every tick and the tank oscillates in place (live WC
// 2026-07-03, tank_advance_boss at (-151,414) vs tank_advance_boss_route at
// (24.5,454), 0/8). Deliberately loose (only checks cur>=0, not
// cur<route_waypoints.size()) to match the route rule's own re-acquire
// logic below — a stale/out-of-range cursor still routes through the
// prefix-clamp and reachable-crumb search there rather than falling back to
// the direct step.
static bool DungeonRouteArmed(BotSnapshotView const& s, BotAI& ai,
                               DungeonAdvice const& advice)
{
    if (!Services::Config().route_aware_combat_advance()) return false;
    if (advice.route_waypoints.empty()) return false;
    const int32_t cur = ai.dungeon_route_wp(s.map_id());
    if (cur < 0) return false;
    // A CONSUMED route must stop asserting ownership. Once the follower has
    // declined at the arrived final crumb it no longer navigates — but this
    // flag was still gating OFF the direct stride, the leashed approach and
    // the far-trash advance (and nothing ever clears the cursor:
    // clear_dungeon_route_wp has zero call sites), so at the LAST boss the
    // whole ladder was disabled and idle:dungeon_hold claimed every tick
    // silently. Live: 11+ minutes with zero MoveTo after the second-to-last
    // boss died — Arcatraz 3/4, Botanica 4/5, Shadow Labyrinth 3/4,
    // Stonecore 3/4, Utgarde Keep 3/4, Halls of Stone 2/3 (campaign
    // 2026-07-21). Consume = release ownership, not merely stop substituting.
    return ai.route_consumed_idx(s.map_id()) != cur;
}

// Shared (declared in MaintainHelpers.h). Honor an active off-mesh crossing
// commitment in ANY state and REFRESH its TTL so a combat-contested crossing can
// neither be interrupted nor expire mid-jump. See the header for the full failure
// it fixes (mid-jump stall on the off-mesh poly, NoPath, no recovery).
bool DungeonHonorCross(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit, uint32 now_ms)
{
    if (!s.is_alive() || !ai.dungeon_cross_active(now_ms))
    { ai.cross_episode_reset(); return false; }   // no crossing in flight — next one starts clean
    float cx, cy, cz; s.position(cx, cy, cz);
    const float tgx = ai.dungeon_cross_x();
    const float tgy = ai.dungeon_cross_y();
    const float tgz = ai.dungeon_cross_z();
    const float dx = tgx - cx, dy = tgy - cy, dz = tgz - cz;
    if (dx * dx + dy * dy + dz * dz <= 6.0f * 6.0f)
    {
        ai.clear_dungeon_cross();   // landed on the far ledge — resume normal nav
        ai.cross_hold_reset();      // fresh tracker for the next crossing episode
        ai.cross_window_reset();    // clean window baseline for the next crossing
        return false;
    }
    // Stale-spline strand recovery. The !is_moving relocate below cannot fire when the
    // bot is frozen ON the off-mesh poly while is_moving() still reads TRUE — a spline
    // that was launched, interrupted, and never cleared (live 06-26: Dungtank+Dunghealer
    // held 2+ min at the Gap-1 north endpoint (-213,-520) in dungeon_offmesh_cross_hold,
    // is_moving stuck true, while 3 DPS crossed south and the group fragmented). Detect
    // zero net XY progress while a cross is committed; once it exceeds the timeout, force
    // the jump's completion REGARDLESS of is_moving. A genuine in-flight hop keeps moving
    // >4y so it never trips this; only a frozen bot accumulates the dwell. Same relocate
    // primitive/semantics as the !is_moving path — the off-mesh connection is an
    // instantaneous jump and the exit is solid ground, so this is not a content skip.
    constexpr uint32 kCrossStuckMs = 6000;
    const float dist_to_exit = std::sqrt(dx * dx + dy * dy + dz * dz);
    const uint32 hold_stuck   = ai.cross_hold_stuck_ms(dist_to_exit, now_ms);
    const uint32 cross_frozen = ai.cross_frozen_ms(cx, cy, cz, now_ms);
    const uint32 cross_episode = ai.cross_episode_ms(now_ms);
    // FLIP-FLOP backstop (06-29). The committed exit alternates between the two
    // bridge endpoints (boss-side vs group-side: boss-nav pulls forward, cohesion
    // pulls back to trailing followers), and that flip resets hold_stuck (target-
    // relative best dist) and the bot's physical oscillation across the span resets
    // cross_frozen — so neither of the two clocks below ever reaches 6s though the
    // tank bounces -520<->-548 on the Helix Gap forever (live). cross_episode_ms
    // measures wall-time since the crossing FIRST committed and is immune to the
    // re-commit churn, so it climbs through the oscillation. Force-complete onto the
    // currently-committed exit once it caps — landing on solid navmesh breaks the
    // bounce; normal nav then re-evaluates from a single stable footing.
    constexpr uint32 kCrossEpisodeMs = 10000;
    constexpr uint32 kCrossFrozenMs  = 6000;
    // near_teleport_to is ServerRefused for an in-combat or casting bot
    // (PlayerbotAPI::near_teleport_to) — and every relocate below used to
    // clear the cross REGARDLESS of that refusal, silently tearing the
    // crossing down so the hop just re-armed on its 15s cooldown forever
    // (live WC crumb-27 lip, 2026-07-03: hop → direct spline stalls under
    // combat flicker → frozen clock trips → refused teleport + cross
    // cleared → repeat; the bot never descends). When the teleport WOULD
    // be refused: keep the commitment, reset whichever clock capped (a
    // full fresh window instead of re-tripping every tick), and fall
    // through to the tail re-assert — the rescue fires on the first
    // out-of-combat evaluation.
    const bool can_teleport = !s.in_combat() && !s.is_casting();
    if (!can_teleport)
    {
        if (cross_episode > kCrossEpisodeMs) ai.cross_episode_reset();
        if (hold_stuck   > kCrossStuckMs)    ai.cross_hold_reset();
        if (cross_frozen > kCrossFrozenMs)   ai.cross_frozen_reset();
        ai.cross_window_reset();   // stale baseline must not insta-trip post-combat
    }
    if (can_teleport &&
        cross_episode > kCrossEpisodeMs && Playerbot::PathBudget::HasBudget(now_ms))
    {
        emit.near_teleport_to(tgx, tgy, tgz, s.raw().position.o);
        ai.clear_dungeon_cross();
        ai.cross_hold_reset();
        ai.cross_frozen_reset();
        ai.cross_window_reset();
        ai.set_last_rule_fired("dungeon_offmesh_cross_relocate_episode");
        return true;
    }
    if (can_teleport &&
        hold_stuck > kCrossStuckMs && Playerbot::PathBudget::HasBudget(now_ms))
    {
        emit.near_teleport_to(tgx, tgy, tgz, s.raw().position.o);
        ai.clear_dungeon_cross();
        ai.cross_hold_reset();
        ai.cross_frozen_reset();
        ai.cross_window_reset();
        ai.set_last_rule_fired("dungeon_offmesh_cross_relocate");
        return true;
    }
    // Own-position FROZEN recovery — the reliable path when hold_stuck is defeated
    // by committed-exit churn (live 06-27: Dungtank+Dunghealer wedged at the Gap-1
    // north endpoint the WHOLE run, move_blocked, 0 cross_relocate firings, group
    // never reached the harbor). cross_frozen_ms tracks the bot's OWN lack of motion,
    // so it accumulates regardless of the churning target AND regardless of is_moving
    // (a launched-then-interrupted spline reads in-flight while move_blocked pins the
    // bot in place). Force-complete the committed crossing onto its far exit — a real
    // navmesh poly on solid ground (same primitive/semantics as the stale-spline
    // relocate above; the off-mesh hop is an instantaneous jump, not a content skip).
    if (can_teleport && cross_frozen > kCrossFrozenMs)   // constant hoisted above
    {
        emit.near_teleport_to(tgx, tgy, tgz, s.raw().position.o);
        ai.clear_dungeon_cross();
        ai.cross_hold_reset();
        ai.cross_frozen_reset();
        ai.cross_window_reset();
        ai.set_last_rule_fired("dungeon_offmesh_cross_relocate_frozen");
        return true;
    }
    // Windowed NET-PROGRESS recovery — the reliable path for an OSCILLATING strand
    // that defeats BOTH clocks above (live 06-27: Dungrogue swung y -510<->-526 at
    // the Gap-1 mouth, exit-distance bouncing 21<->37, hold+froz climbing to ~5000
    // then RESET by every jitter, never relocating; the group held harbor_stage on
    // it at 3/6). The committed exit is STABLE here (a follower's regroup-cross), so
    // compare exit-distance now vs one 8s window ago: if the bot has not NET-closed
    // >9y toward the exit across the whole window it is wedged at the mouth however
    // it jitters within it. Force-complete onto the exit (same primitive/semantics
    // as the relocates above; the off-mesh hop is an instantaneous jump). dist>6
    // here always (past the landed check) so the detector ticks every honored frame.
    if (can_teleport && ai.cross_window_noprogress(dist_to_exit, now_ms, 8000u, 9.0f))
    {
        emit.near_teleport_to(tgx, tgy, tgz, s.raw().position.o);
        ai.clear_dungeon_cross();
        ai.cross_hold_reset();
        ai.cross_frozen_reset();
        ai.cross_window_reset();
        ai.set_last_rule_fired("dungeon_offmesh_cross_relocate_window");
        return true;
    }
    // Stranded mid-jump recovery. A bot can come to rest ON the off-mesh connection
    // poly partway across (spline interrupted by combat, a follower stopping mid-span
    // to regroup, or a revive that lands on the bridge). From the off-mesh poly EVERY
    // move_to to the committed exit NoPaths — the off-mesh jump only initiates from the
    // entry endpoint, never from its middle — so the re-emit below spins forever and the
    // bot wedges (live 06-26: Dunghealer parked at (-213,-531,z52) on the Gap-1 span,
    // dozens of "[gap1] REFUSE NoPath" toward the far vertex, run deadlocked; the
    // FARFROMPOLY nudge can't help because the off-mesh connection IS a valid poly).
    // When the bot is STATIONARY (no live spline — a normal jump is still in flight and
    // must not be cut short) AND the committed exit is unreachable by path, COMPLETE the
    // interrupted off-mesh traversal by relocating to the exit (a real navmesh poly on
    // solid ground). The off-mesh connection is semantically an instantaneous jump, so
    // this finishes the intended crossing — it is not a content skip (same primitive the
    // elevator/ledge stepper uses). Gated on PathBudget so it never adds a hot-path cost.
    // SKIPPED for a DIRECT (nav-link) crossing: NOPATH toward the exit is EXPECTED
    // across a real navmesh split — the straight no-pathfind spline below handles it,
    // and relocating here would replace the intended walk with a teleport.
    if (can_teleport && !s.is_moving() && !ai.dungeon_cross_direct() &&
        Playerbot::PathBudget::HasBudget(now_ms))
    {
        if (Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
        {
            PathGenerator pg(self);
            BotMovement::SehSafeCalculatePath(pg, tgx, tgy, tgz);
            PathType const pt = pg.GetPathType();
            if (pt & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY))
            {
                emit.near_teleport_to(tgx, tgy, tgz, s.raw().position.o);
                ai.clear_dungeon_cross();
                ai.cross_hold_reset();
                ai.set_last_rule_fired("dungeon_offmesh_cross_relocate");
                return true;
            }
        }
    }
    // Still mid-crossing: re-assert the fixed off-mesh far vertex (the jump's exit,
    // always a reachable nav point even from the off-mesh poly) and refresh the TTL
    // so the commit outlives a slow combat-contested hop. Re-emitting the SAME goal
    // dedups in API::move_to (the spline is held, not restarted); the FIRST honored
    // tick after a combat interruption emits a goal different from the opener's, so
    // it relaunches the spline toward the exit and un-stalls the bot.
    // A DIRECT crossing (DB traversal link) is driven with a straight no-pathfind
    // MovePoint spline — a pathfound move toward the far side of a real navmesh
    // split would NoPath and refuse (see MoveToIntent::direct).
    ai.set_dungeon_cross(tgx, tgy, tgz, now_ms + 12000);
    emit.move_to(tgx, tgy, tgz, /*run=*/true, ai.dungeon_cross_direct());
    ai.set_last_rule_fired(ai.dungeon_cross_direct() ? "dungeon_navlink_cross_hold"
                                                     : "dungeon_offmesh_cross_hold");
    return true;
}

bool DungeonCombatPositioning(BotSnapshotView const& s, BotAI& ai,
                              GroupSnapshotView const& g,
                              BotIntentEmitter& emit,
                              DungeonAdvice const& advice)
{
    // Cheap guards. Dead bots and bots mid-cast don't reposition (don't
    // break a heal cast for cosmetic movement). All sub-rules also re-check
    // is_alive(); this is the fast common-case exit when nothing applies.
    if (!s.is_alive()) return false;

    const uint32 now_ms = s.published_at_ms();

    // SURVIVAL PREEMPT — SURFACE TO BREATHE. The world idle:surface_to_breathe
    // rule (priority 890) is SUPPRESSED inside a dungeon: the DungeonDispatch
    // hard-stop (idle:dungeon_hold) swallows every world-idle rule. So a submerged
    // bot in a water dungeon would NEVER surface and would DROWN — drowning damage
    // sets no combat flag, so it dies ~10% max-HP/tick with InCombat=false while
    // the advance keeps walking it deeper. DungeonCombatPositioning runs from BOTH
    // the idle dispatch AND the top of DispatchInCombat, so re-admitting the
    // surface move HERE makes it preempt all positioning/advance/combat — drowning
    // kills regardless of combat state, so it must run first. Surfaces in place
    // (same XY, just above the water line); the bot then swims the surface instead
    // of sinking. Mirrors SurvivalRules SurfaceToBreatheGate/Fire. (Latent-bug fix
    // found chasing the Deadmines harbor wall — which turned out NOT to be water,
    // but this is still correct for any future water dungeon.)
    if (s.is_underwater() && s.water_surface_z() != 0.0f)
    {
        float sbx, sby, sbz; s.position(sbx, sby, sbz);
        if (sbz < s.water_surface_z() - 0.5f)
        {
            emit.move_to(sbx, sby, s.water_surface_z() + 0.5f, /*run=*/true);
            ai.set_last_rule_fired("idle:dungeon_surface_to_breathe");
            return true;
        }
    }

    // SURVIVAL PREEMPT — REPAIR BROKEN GEAR (2026-06-29). A self-repair rule
    // already exists, but it sits at the FAR TAIL of the dungeon idle dispatch
    // (after advance / engage / healup-hold / regroup / rez), so on the harbor
    // death-grind it was NEVER reached: a freshly-rezzed tank is always either
    // healing-up, advancing, or dying again before the dispatch falls through to
    // it (measured live 2026-06-29: tank at 0% durability, max HP collapsed
    // 16272->12976, self_repair fired 0x all run). Durability is binary — an item
    // at 0% gives ZERO stats — so a death-grinding tank's gear breaks, its HP/
    // armor halve, and it dies even faster: the spiral that loses the harbor. Hoist
    // repair to a high-priority preempt: OOC with any equipped piece under 60%
    // durability, self-repair BEFORE doing anything else. emit.repair_all(Empty)
    // routes to API's self-repair branch (no vendor; stipend top-up makes it free
    // for AI bots). The retry throttle makes it one tick of repair then the bot
    // proceeds, so it never blocks progression. OOC-gated so it never interrupts a
    // fight. General (all roles/dungeons) — a 0%-durability DPS is dead weight too.
    if (s.is_alive() && !s.in_combat() &&
        s.lowest_equipped_durability_pct() < 60 &&
        !ai.action_recently_tried(BotAI::ActionKind::Repair, 0, now_ms))
    {
        emit.repair_all(ObjectGuid::Empty);
        ai.note_action_retry(BotAI::ActionKind::Repair, 0, now_ms);
        ai.set_last_rule_fired("idle:dungeon_self_repair_preempt");
        return true;
    }

    // (-2) Off-mesh crossing commitment — runs IN COMBAT, before everything (incl.
    // the void-recovery nudge and the boss push). A follower mid-jump over the Gap-1
    // void must COMPLETE the hop before it fights; otherwise the opener/assist below
    // emits a move toward a target across the gap, replaces the off-mesh spline, and
    // strands the bot on the off-mesh poly (NoPath thereafter). Honoring the commit
    // here makes the jump uninterruptible. No-op when no crossing is in flight.
    if (s.is_in_dungeon() && DungeonHonorCross(s, ai, emit, now_ms))
        return true;

    // (-1) Off-mesh void recovery — runs IN COMBAT (this helper is invoked from
    // State_InCombat as well as the idle DungeonDispatch). A dungeon bot stranded
    // off-mesh in a bridge gap (Deadmines Gap-1 void at ~Y-528, between the north
    // platform navmesh edge -522 and the far-ledge edge -543) can path NOWHERE:
    // every move_to NoPaths from an off-mesh source. The idle DungeonDispatch
    // nudge (the non-tank cohesion block) only runs in State_Idle, but a stranded
    // bot is almost always IN COMBAT (healing, auto-attacking a mob that trailed
    // it across, or holding aggro), so the FSM routes it to State_InCombat and the
    // idle nudge never fires. Worse, once the WHOLE group consolidates in the void
    // (tank dies mid-cross, revives off-mesh at -528, the group regroups onto it —
    // observed live 2026-06-26: all 5 frozen at -528, cohesion satisfied, hd=0,
    // nobody moving, no path out), it is a permanent stall. Nudge onto the nearest
    // navmesh poly FIRST, before any combat positioning, so an in-combat stranded
    // bot recovers and the next tick re-paths from real mesh. No-op (returns false)
    // for any on-mesh bot via DungeonNudgeOntoMesh's FARFROMPOLY gate, so this is
    // free for the common case.
    if (s.is_in_dungeon())
        if (Player* self_recover = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
            if (DungeonNudgeOntoMesh(self_recover, emit, ai))
                return true;

    // (-0.4) Dungeon FALSE-COMBAT escape — LAST-RESORT safety net. The whole COHERED
    // group can be held InCombat indefinitely by attackers with NO seedable victim
    // among them: every attacker IGNORED, UNTARGETABLE, or IMMUNE. In Deadmines the
    // SOURCE of these (firewall platters, immune boss cutscene doubles, untargetable
    // lightning stalkers) is now neutralized at spawn (instance_deadmines
    // DecoyNeutralizeEntries), so this rarely fires there anymore — it remains the
    // general backstop for any dungeon where such an entity slips through. The seed
    // loop leaves victim empty (the bot cannot DPS its way out); the strand recovery
    // only relocates an OUTLIER vs the body, so when the WHOLE body is wedged together
    // it never fires; and the in-combat boss-push only triggers within 150y of a boss.
    // So a group dragged BACK by chasing un-killable hostiles after a forward death can
    // sit in false combat forever (observed live 06-28: all 5 pinned at the zone-in,
    // prog frozen 1/6 for 20+ min). Detect the lock — in combat, victim empty,
    // attackers present, but NONE seedable (replicating the State_InCombat seed-loop
    // accept test) — and, once it has held past a brief grace window, near_teleport
    // the bot to the NEXT UN-KILLED BOSS's waypoint (waypoints[bosses_done_count]),
    // but only when that boss is FAR (the lock dragged us off it) — never teleporting
    // off a boss we are already on. The teleport breaks the melee leash instantly and
    // normal navigation resumes. Excludes a live boss encounter (a boss may go briefly
    // untargetable mid-phase; never teleport out of a real fight).
    if (s.is_in_dungeon() && s.in_combat() && s.victim().IsEmpty() &&
        !s.is_encounter_in_progress() &&
        !s.raw().combat.attackers.empty() &&
        !advice.progression_waypoints.empty())
    {
        Player* self_seed = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
        bool any_seedable = false;
        // Classify WHY there is no seedable victim, to pick the right escape:
        //  - immune_present : a reachable, fully-selectable attacker the bot simply
        //    cannot DAMAGE (Unit::IsValidAttackTarget false — an IMMUNE_TO_PC event
        //    creature). These live AT the boss, so teleporting toward the boss just
        //    re-aggros them — an escape-teleport LOOP (observed live 2026-07-22: RFC
        //    healer teleport-looped onto immune Dark Shaman Acolyte 61672, InCombat
        //    285s). The cure is to STOP teleporting so the creature leashes.
        //  - dragger_present : an untargetable / pacified / UNREACHABLE attacker
        //    (chasing firewall platters, lightning stalkers on a nav-island) that
        //    dragged the group BACK off the boss. Here the forward teleport is
        //    exactly right — it breaks the leash and puts us back on the objective.
        // immune_lock (immune, and NOTHING draggable) => disengage in place, never
        // teleport. Otherwise keep the original forward-teleport-when-far behavior.
        bool immune_present = false;
        bool dragger_present = false;
        for (auto const& a : s.raw().combat.attackers)
        {
            if (a.hp <= 0) continue;
            if (a.untargetable || a.is_pacified) { dragger_present = true; continue; }
            bool ign = false;
            for (uint32_t ie : advice.ignore_entries)
                if (ie == a.entry) { ign = true; break; }
            if (ign) continue;
            // An attacker the in-combat seed loop can never actually melee — out
            // of LoS or sitting on a navmesh island with NO path to it — is NOT a
            // way out of false combat: the bot keeps victim empty forever (observed
            // 2026-06-28: a Foe Reaper arena prop "Mining Monkey" 48442, unit_flags=0
            // so targetable yet unreachable behind the hulk, pinned the tank InCombat
            // 826s at the gauntlet tail — any_seedable stayed true so this escape
            // never fired, while the InCombat seed loop rejected it as unreachable).
            // Require the attacker be reachable for it to suppress the escape, so the
            // two tests agree. Cheap here: only runs while in_combat + victim empty.
            if (!a.in_los) { dragger_present = true; continue; }
            // An IMMUNE_TO_PC attacker is fully SELECTABLE and REACHABLE yet
            // UNDAMAGEABLE by the bot — Unit::IsValidAttackTarget rejects it, so
            // every StartAttack returns InvalidTarget and victim() stays empty. It
            // is therefore NOT a way out of false combat and must not count as
            // seedable (distinct from untargetable/is_pacified, already skipped
            // above — an immune event creature carries none of those). This is the
            // wc_world faction-14-on-immune-event-creature corruption class:
            // root-fixed in the data (sql/world/0006_immune_event_creature_faction_
            // restore, 14->16 for 335 audited carriers), but defended here so a
            // single residual/re-introduced mis-authored faction can never again
            // pin the whole group InCombat with nothing killable. Checked before
            // the (expensive) reachability probe so immune attackers short-circuit.
            if (self_seed)
                if (Unit* au = ObjectAccessor::GetUnit(*self_seed, a.guid))
                    if (!self_seed->IsValidAttackTarget(au))
                        { immune_present = true; continue; }
            if (self_seed)
            {
                G3D::Vector3 _seed_step;
                if (!DungeonTargetReachableAndStep(self_seed, a.x, a.y, a.z,
                                                   20.0f, _seed_step))
                    { dragger_present = true; continue; }
            }
            any_seedable = true;
            break;
        }
        // An immune-only lock: teleporting forward re-aggros the same immune adds.
        bool const immune_lock = immune_present && !dragger_present && !any_seedable;
        constexpr uint32 kFalseCombatEscapeMs = 5000;   // grace before relocating
        const uint32 fc_ms = ai.false_combat_ms(!any_seedable, now_ms);
        if (!any_seedable && fc_ms > kFalseCombatEscapeMs)
        {
            if (Player* self_fc = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
            {
                // Relocate toward the NEXT UN-KILLED boss's waypoint. The
                // progression_waypoints are authored 1:1 at the boss locations
                // (waypoints[i] == bosses[i]), so waypoints[bosses_done_count] is
                // exactly the encounter we still owe. Using the proximity-advanced
                // dungeon_waypoint_index here was wrong: it can sit at wp[2]=Foe
                // Reaper while Helix at wp[1] is still alive, teleporting the tank
                // straight PAST the un-killed Helix (observed live 2026-06-28 — the
                // tank yo-yoed foundry<->Helix on an immune Vanessa double and never
                // killed Helix). bosses_done_count tracks actual kills, so it never
                // points past a live boss.
                size_t wi = s.raw().dungeon_exec.bosses_done_count;
                if (wi >= advice.progression_waypoints.size())
                    wi = advice.progression_waypoints.size() - 1;
                auto const& wp = advice.progression_waypoints[wi];
                float fcx, fcy, fcz; s.position(fcx, fcy, fcz);
                const float ddx = wp.x - fcx, ddy = wp.y - fcy, ddz = wp.z - fcz;
                const float wd2 = ddx * ddx + ddy * ddy + ddz * ddz;
                // Only relocate when the lock has displaced us FAR from the boss we
                // owe (dragged back by chasers, the entrance-wedge case). If we are
                // already AT that boss and merely pinned by an immune add right there,
                // a teleport can't help and would risk skipping the boss — leave that
                // to the decoy neutralization + normal combat / boss-push below. 40y
                // ~ the advance/heal leash. (Falls through, so the boss-push still
                // runs when we're already on the boss.)
                constexpr float kEscapeMinDisplaceSq = 40.0f * 40.0f;
                // Route-WALK forward out of the false-combat lock (near_teleport_to
                // is gone: it REFUSES in combat — the escape's own precondition — so
                // it was always a silent no-op that looped forever, RFC Dancing
                // Flames 2026-07-22). The route helper is now FORWARD-ONLY (only
                // returns a crumb closer to the target than we are), so neither of
                // these can drag the group backward the way the Deadmines yo-yo did:
                //  - a FOLLOWER walks toward the TANK (cohesion — the tank leads);
                //  - the TANK walks toward the boss WAYPOINT it owes. The tank MUST
                //    have its own in-combat mover here: RFC's Dancing Flames is a
                //    PERSISTENT hazard the tank stands in, so it never goes OOC and
                //    the OOC route-follower never runs — merely disengaging left the
                //    tank pinned (RFC regressed to 2/4, combat 578s, 2026-07-23).
                // If no forward step resolves for either, fall through to disengage.
                if (!immune_lock && wd2 >= kEscapeMinDisplaceSq)
                {
                    float qx, qy, qz;
                    bool have_step = false;
                    char const* esc_tag = "";
                    if (ai.effective_role(s) != Role::Tank)
                    {
                        if (GroupMemberSummary const* tk = g.tank())
                            if (tk->online && tk->is_alive &&
                                tk->guid != s.guid() && tk->map_id == s.map_id())
                            {
                                have_step = DungeonRouteStepTowardPos(
                                    s, tk->x, tk->y, tk->z, 45.0f, qx, qy, qz);
                                esc_tag = "TANK";
                            }
                    }
                    else
                    {
                        have_step = DungeonRouteStepTowardPos(
                            s, wp.x, wp.y, wp.z, 45.0f, qx, qy, qz);
                        esc_tag = "boss_wp";
                    }
                    if (have_step)
                    {
                        TC_LOG_INFO("playerbot.v2",
                            "[false_combat_esc] {} pos=({:.0f},{:.0f},{:.0f}) atk={} "
                            "held={}ms -> ROUTE-WALK toward {} via ({:.0f},{:.0f},{:.0f})",
                            s.name(), fcx, fcy, fcz,
                            static_cast<unsigned>(s.raw().combat.attackers.size()),
                            fc_ms, esc_tag, qx, qy, qz);
                        emit.move_to(qx, qy, qz, /*run=*/true);
                        // Multi-tick walk: leave the false-combat clock armed so the
                        // escape re-issues the step until combat ends; the target is
                        // stable so move_to dedups the spline between ticks.
                        ai.set_last_rule_fired("dungeon:false_combat_escape_route");
                        return true;
                    }
                    // No forward step → fall through to disengage.
                }
                {
                    // DISENGAGE IN PLACE — the universal fallback: the TANK (which
                    // must not be route-walked), an immune_lock, boss-close, or a
                    // follower with no route step to the tank. Reached when relocating
                    // is either useless or harmful:
                    //  - immune_lock: the only thing holding us is UNDAMAGEABLE
                    //    immune adds that live AT the boss — teleporting toward the
                    //    boss re-aggros them (the escape-teleport loop). We must
                    //    stop relocating so the immune creature leashes and drops us.
                    //  - boss CLOSE (< 40y, the original fall-through): a teleport
                    //    can't help and would risk skipping the boss we owe.
                    // Either way: shield every current attacker so combat target-
                    // selection stops re-acquiring them, and stop attacking. We
                    // DELIBERATELY do NOT return true — control falls through to the
                    // in-combat boss-advance below, so the bot keeps navigating (onto
                    // the boss, or off the immune creature's leash) instead of
                    // thrashing. No teleport-rescue (feedback_no_teleport_rescue).
                    // The 60s shield is refreshed every tick we remain wedged, so it
                    // never lapses mid-escape; it expires on its own once we are clear.
                    for (auto const& atk : s.raw().combat.attackers)
                        if (atk.hp > 0)
                            ai.note_engage(atk.guid, now_ms, 60000u);
                    emit.stop_attack();
                    ai.set_last_rule_fired("dungeon:false_combat_disengage");
                    static uint32 s_fc_disengage_dbg_ms = 0;
                    if (now_ms - s_fc_disengage_dbg_ms > 1500u)
                    {
                        s_fc_disengage_dbg_ms = now_ms;
                        TC_LOG_INFO("playerbot.v2",
                            "[false_combat_esc] {} pos=({:.0f},{:.0f},{:.0f}) atk={} "
                            "held={}ms boss_wp[{}] within {:.0f}y -> DISENGAGE in place",
                            s.name(), fcx, fcy, fcz,
                            static_cast<unsigned>(s.raw().combat.attackers.size()),
                            fc_ms, static_cast<unsigned>(wi), std::sqrt(wd2));
                    }
                }
            }
        }
    }

    // dungeon:untankable_disengage (2026-07-02, stage 3): a NON-TANK in
    // combat with a victim the tank can only reach via an excessive detour,
    // while the tank has no fightable enemy of its own, is the mob-AGGRO
    // variant of the SFK wedge — proximity aggro pulls a bot straight into
    // combat, so neither pre-combat pull gate (idle:dungeon_focus_assist /
    // idle:dungeon_dps_assist, both gated !s.in_combat()) ever evaluates
    // this target. Drop it + shield 60s; combat then times out and the
    // group advances via the normal route instead of false-locking the
    // whole party on an unreachable pull. Sustained-only (>6s), latched on
    // the CURRENT victim guid via combat_victim_since_ms (see BotAI.h) —
    // NOT the opener's OOC latch, which never armed for a mob-initiated
    // pull — so a fresh engage gets a moment before judgment and a target
    // swap restarts the window; a tank mid-corridor toward a legitimate
    // pull (Task 4 commitment) is not undercut the instant combat starts.
    // The `!tk->in_combat` guard is the load-bearing one: it keeps this
    // rule out of every REAL encounter, where the tank is always fighting
    // something.
    if (Services::Config().pull_gate_disengage_enabled() &&
        s.is_in_dungeon() &&
        ai.effective_role(s) != Role::Tank && g.exists())
    {
        // Latch maintenance runs on EVERY pass through here — in combat or
        // not — feeding an EMPTY guid whenever there is no live in-combat
        // victim. The empty-guid pass RE-ARMS the latch (guid-change branch),
        // so ghost-combat / evade / combat-end windows clear it and a later
        // re-aggro of the SAME mob starts a fresh 6s window instead of
        // inheriting a stale since-timestamp and firing on the first tick of
        // the re-engagement (review finding, 2026-07-02).
        uint32 const vsince_ms = ai.combat_victim_since_ms(
            s.in_combat() ? s.victim() : ObjectGuid::Empty, now_ms);
        if (s.in_combat() && !s.victim().IsEmpty() && vsince_ms > 6000u)
        {
            if (GroupMemberSummary const* tk = g.tank())
                if (tk->online && tk->is_alive && tk->guid != s.guid() &&
                    tk->map_id == s.map_id() && !tk->in_combat)
                {
                    // Find the victim's coords in the snapshot sweep.
                    NearbyUnit const* vu = nullptr;
                    for (auto const& u : s.raw().combat.nearby_enemies)
                        if (u.guid == s.victim()) { vu = &u; break; }
                    if (vu)
                    {
                        // Tank-to-victim prefilter (final-review fix, 2026-07-03):
                        // a victim already trivially close to the TANK can never
                        // be judged an excessive detour — skip the full pathfind
                        // probe entirely (no probe, no fire) instead of paying for
                        // a DungeonTankDetourExcessiveVerbose call whose answer is
                        // a foregone conclusion.
                        const float tvx = tk->x - vu->x, tvy = tk->y - vu->y, tvz = tk->z - vu->z;
                        bool const trivially_close =
                            (tvx * tvx + tvy * tvy + tvz * tvz) <= 12.0f * 12.0f;
                        // Probe cadence (final-review fix, 2026-07-03): this rule
                        // had none — once sustain>6s it re-issued a full tank
                        // pathfind EVERY tick per non-tank bot until the verdict
                        // flipped. Cap the probe at once per 3s, matching the
                        // other dungeon detour rules' pacing.
                        bool const probe_due =
                            now_ms - ai.untankable_probe_last_ms() >= 3000u;
                        if (!trivially_close && probe_due)
                        if (Player* tkp = ObjectAccessor::FindConnectedPlayer(tk->guid))
                        {
                            ai.touch_untankable_probe(now_ms);
                            float dv_beeline = 0.f, dv_path_len = 0.f, dv_ratio = 0.f;
                            bool  dv_complete = false;
                            if (DungeonTankDetourExcessiveVerbose(tkp, vu->x, vu->y, vu->z,
                                                                  dv_beeline, dv_path_len,
                                                                  dv_ratio, dv_complete))
                            {
                                emit.stop_attack();
                                // Re-arm the sustain latch NOW: the snapshot's
                                // victim stays set until the stop_attack lands,
                                // and this rule's own disengage must not leave a
                                // frozen since-timestamp that lets a post-shield
                                // re-aggro of the same guid fire instantly
                                // (review finding, 2026-07-02).
                                ai.combat_victim_latch_reset();
                                ai.note_engage(s.victim(), now_ms, 60000u);
                                ai.set_last_rule_fired("dungeon:untankable_disengage");
                                static uint32 s_pullgate_dbg_disengage_ms = 0;
                                if (now_ms - s_pullgate_dbg_disengage_ms > 1500u)
                                {
                                    s_pullgate_dbg_disengage_ms = now_ms;
                                    TC_LOG_INFO("playerbot.v2",
                                        "[pull_gate] DISENGAGE bot={} victim={} path={:.1f} "
                                        "beeline={:.1f} ratio={:.2f} complete={}",
                                        s.bot_id(), s.victim().ToString(), dv_path_len,
                                        dv_beeline, dv_ratio, dv_complete ? 1 : 0);
                                }
                                return true;
                            }
                        }
                    }
                }
        }
    }

    // (0) Tank push-to-boss WHILE in combat (Deadmines Helix foundry, 2026-06-26).
    // The idle boss-navigator only advances the tank toward the next boss while
    // OUT of combat (its gate is !in_combat). But a boss room fronted by trash
    // across a chokepoint puts mobs between the tank and the boss: the instant the
    // tank strides toward a reachable Helix (reach=1, ~90y) a Goblin Overseer /
    // Defias Envoker aggroes, the tank enters combat, the idle advance STOPS, and
    // the melee gap-close below chases that trash — back NORTH across the Gap-1
    // bridge, away from Helix and the forward group (observed live: tank
    // ping-ponged -557<->-404 for minutes, never closing the last 90y while the
    // healer + DPS waited AT the boss). A real tank walks to the boss with the
    // trash trailing and pulls it; the group then collapses on the boss and
    // cleaves the adds. So when the earliest live boss is reachable, close, and
    // the group is already committing to it (a member within 50y of the boss),
    // the TANK keeps stepping toward the boss IN COMBAT (off-mesh-aware, committed)
    // and pulls it on arrival — preempting the trash chase. The member-near-boss
    // gate prevents a premature solo-pull when the group has NOT reached the room.
    // Tank-only; DPS assist the tank's boss target through the normal combat path.
    if (ai.effective_role(s) == Role::Tank &&
        !s.is_casting() &&
        s.is_in_dungeon() &&
        !s.is_encounter_in_progress() &&
        !advice.bosses.empty() &&
        !(s.raw().pve_order.active &&
          !s.raw().pve_order.main_tank.IsEmpty() &&
          s.raw().pve_order.main_tank != s.guid()))
    {
        if (Player* self_pb = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
        {
            struct PushBossCheck
            {
                Player const* origin;
                std::vector<uint32_t> const& entries;
                float range;
                bool operator()(Creature* c) const
                {
                    if (!c || !c->IsAlive()) return false;
                    for (uint32_t e : entries)
                        if (c->GetEntry() == e)
                            return origin->IsWithinDistInMap(c, range);
                    return false;
                }
            };
            // Commit range: only push in-combat when the boss is genuinely close.
            // The long OOC approach (waypoints / idle boss-nav) covers the rest.
            constexpr float kPbScanR = 150.0f;
            std::list<Creature*> pb_cre;
            PushBossCheck pb_chk{self_pb, advice.bosses, kPbScanR};
            Trinity::CreatureListSearcher<PushBossCheck> pb_search(self_pb, pb_cre, pb_chk);
            Cell::VisitAllObjects(self_pb, pb_search, kPbScanR);
            Creature* pb_boss = nullptr;
            for (uint32_t be : advice.bosses)
            {
                for (Creature* bc : pb_cre)
                    if (bc && bc->GetEntry() == be && bc->IsAlive())
                    { pb_boss = bc; break; }
                if (pb_boss) break;
            }
            if (pb_boss)
            {
                const float pbx = pb_boss->GetPositionX();
                const float pby = pb_boss->GetPositionY();
                const float pbz = pb_boss->GetPositionZ();
                float sx0, sy0, sz0; s.position(sx0, sy0, sz0);
                const float ddx = pbx - sx0, ddy = pby - sy0, ddz = pbz - sz0;
                const float bd2 = ddx * ddx + ddy * ddy + ddz * ddz;
                constexpr float kPbEngageR2 = 25.0f * 25.0f;
                // HEALER LEASH. The push to the boss MUST be paced by the healer or
                // the tank outruns its heals and dies alone at the boss while the
                // group is still a chokepoint behind (observed live 2026-06-26: the
                // tank ran 89y ahead to Helix and died at 0% while the healer sat at
                // 100% back on the gap ledge). A real tank advances only while the
                // healer is in heal range; if the healer falls behind it holds the
                // current pack and waits. So gate the ADVANCE on a healer within 40y
                // and the PULL on a healer within 45y (heals available). With no
                // healer in the group the leash is moot (solo / DPS comp).
                float healer_d2 = std::numeric_limits<float>::max();
                bool have_healer = false;
                if (g.exists() && g.members())
                    for (auto const& m : *g.members())
                    {
                        if (!m.online || m.map_id != s.map_id()) continue;
                        if (m.guid == s.raw().guid || !m.is_alive) continue;
                        if (m.role != Role::Healer) continue;
                        have_healer = true;
                        const float mdx = m.x - sx0, mdy = m.y - sy0;
                        const float d2 = mdx * mdx + mdy * mdy;
                        if (d2 < healer_d2) healer_d2 = d2;
                    }
                const bool healer_in_adv  = !have_healer || healer_d2 <= 35.0f * 35.0f;
                const bool healer_in_pull = !have_healer || healer_d2 <= 45.0f * 45.0f;
                // PULL MANAGEMENT (Deadmines Helix foundry gauntlet, 2026-06-26).
                // The navmesh path to Helix runs through a DENSE Defias pit — ~10
                // Defias Miners/Diggers + 3 caster Defias Envokers packed into a
                // ~50y span (DB: map 36, x -265..-315, y -555..-610, z47-51). The
                // continuous 18y forward creep below vacuumed the WHOLE pit in a
                // few ticks: 15+ mobs aggroed before any died and the level-30-
                // scaled tank went 76%->0 in seconds at (-298,-589) with the healer
                // RIGHT THERE healing — a 2/5 wipe (observed live 2026-06-26, PID
                // 67640). The cohesion was perfect (across=5/5, healer on the tank);
                // the kill was lost purely to OVER-PULLING. A real tank pulls a
                // pack, holds while the group cleaves it, then advances. So pace the
                // PULL SIZE (the healer leash already paces DISTANCE): suppress the
                // advance while the tank is already in an AoE pull (>=3 attackers)
                // or is critically low — fall through to the normal in-combat melee
                // so the group clears the current pack first; resume the creep once
                // it thins (<3 attackers). attackers_count() counts melee AND the
                // ranged Envokers, so a caster-heavy pull also holds the line.
                // PULL-SIZE GATE (extends pack_clear). attackers_count() counts
                // only mobs ALREADY hitting the tank, so a tank with 1-2 attackers
                // still reads pack_clear and creeps another 18y — diving into a
                // SPREAD pack before the rest aggro. The Helix->FoeReaper Defias
                // gauntlet is ~15 mobs (11 Miners + 2 Diggers + 2 Overseers + 3
                // ranged Envokers) over a ~50y span: the creep vacuumed the lot and
                // the tank died alone in seconds (2026-06-26). Also count UN-engaged
                // hostiles within ~24y — if the tank is already at the edge of a
                // cluster, HOLD and clear it (chunk the pull) instead of stepping
                // deeper. Props / the boss are excluded (mirrors tank_pull_next).
                int near_hostiles = 0;
                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0 || u.no_xp_kill || u.is_pacified || u.is_dungeon_boss || u.untargetable)
                        continue;
                    bool ign = false;
                    for (uint32_t ie : advice.ignore_entries)
                        if (ie == u.entry) { ign = true; break; }
                    if (ign) continue;
                    const float hx = u.x - sx0, hy = u.y - sy0, hz = u.z - sz0;
                    if (hx * hx + hy * hy + hz * hz <= 24.0f * 24.0f) ++near_hostiles;
                }
                // fightable_attackers_count() (stalker-free) not attackers_count():
                // once this push reaches within 150y of Ripsnarl the raw count is
                // flooded by the untargetable Lightning Stalkers (49521) and would
                // pin pack_clear false forever. Identical to attackers_count() in
                // the gauntlet (no trigger units there), so no pull-pacing change.
                const bool pack_clear = s.fightable_attackers_count() < 3 &&
                                        near_hostiles < 4 && s.hp_pct() > 35;
                if (bd2 <= kPbEngageR2 && self_pb->IsWithinLOSInMap(pb_boss))
                {
                    // On top of the boss: pull it (start the encounter) — but only
                    // with the healer in range so the tank isn't soloing the open.
                    // When the healer lags it falls through and holds near the boss
                    // until heals arrive, then pulls.
                    if (healer_in_pull && s.victim() != pb_boss->GetGUID() &&
                        emit.start_attack(pb_boss->GetGUID()))
                    {
                        ai.note_engage(pb_boss->GetGUID(), now_ms);
                        ai.set_last_rule_fired("idle:dungeon_combat_pull_boss");
                        return true;
                    }
                }
                else if (bd2 > kPbEngageR2 && healer_in_adv && pack_clear)
                {
                    // Healer is in range — step toward the boss through the trash
                    // (committed, off-mesh-aware) instead of chasing a trash mob
                    // away. The capped step + the healer leash keep the tank at most
                    // ~one step ahead of heals, so the group leapfrogs to the boss
                    // together. This is the fix for the Gap-1 ping-pong: an in-combat
                    // tank near a reachable boss heads TO the boss, never away to
                    // chase trash — but never faster than the healer can follow.
                    G3D::Vector3 pstep;
                    bool pb_off = false;
                    // Small step: the tank creeps toward the boss so it never ends
                    // a tick more than ~one step past the healer leash (35y) — i.e.
                    // stays inside heal range as it advances. The move_to spline runs
                    // at run speed toward this waypoint and is re-issued each tick, so
                    // a small waypoint paces the push without slowing the actual walk.
                    //
                    // The cap MUST equal the OOC route follower's (tight_engage ? 10
                    // : kMaxAdvanceStep 20 — see route_step at its pass-1 steer): when
                    // this rule and the route rule alternate ticks toward the SAME
                    // crumb, differing caps put their path-capped step points 3-7y
                    // apart on curving corridors — past every 3y dedup radius — so the
                    // two emitters restarted the spline 6-7x/sec and the tank crawled
                    // at ~0.3y/s (WC east ramp, live 2026-07-03, kPbStep was 18 vs 20).
                    // Same cap + same target + same stepper = identical point, and the
                    // emitter dedup / step-hold guard engage as designed.
                    const bool pb_tight =
                        advice.tight_engage_below_z > 0.0f && sz0 < advice.tight_engage_below_z;
                    const float kPbStep = pb_tight ? 10.0f : 20.0f;
                    // Increment 1b: step toward the route crumb (when armed)
                    // instead of the raw boss position — see DungeonAdvanceTarget.
                    // bd2/kPbEngageR2 above (the pull/advance branch choice) and
                    // the LoS check both already ran against pbx/pby/pbz and are
                    // untouched; only this walk target substitutes.
                    float pbx_t, pby_t, pbz_t;
                    int32_t pb_crumb = -1;
                    bool pb_yield = false;
                    DungeonAdvanceTarget(s, ai, advice, pbx, pby, pbz,
                                        pbx_t, pby_t, pbz_t,
                                        "idle:dungeon_combat_advance_boss",
                                        &pb_crumb, &pb_yield);
                    // Near-arrived at the cursor's crumb (latch/arrive): do not
                    // step at all — the route rule owns completion/advance. FALL
                    // THROUGH like this site's in-flight branch (lower rules keep
                    // their tick access), never boss-ward-step from the dead zone.
                    if (!pb_yield &&
                        DungeonTargetReachableAndStep(self_pb, pbx_t, pby_t, pbz_t,
                                                      kPbStep, pstep, &pb_off))
                    {
                        if (pb_off)
                            ai.set_dungeon_cross(pstep.x, pstep.y, pstep.z,
                                                 now_ms + 12000);
                        if (DungeonStepAlreadyInFlight(s, pstep.x, pstep.y, pstep.z))
                        {
                            // in-flight: skip the re-emit (spline keeps running)
                            // and FALL THROUGH like the emitter-dedup false path
                            // always did — lower rules (e.g. the ghost-combat
                            // break) must keep their tick access mid-walk.
                            DungeonStepHoldDiag(s, "idle:dungeon_combat_advance_boss",
                                                pstep.x, pstep.y, pstep.z);
                        }
                        else if (DungeonMoveOwnedElsewhere(s, ai, pstep.x, pstep.y,
                                                           pstep.z, now_ms,
                                                           "idle:dungeon_combat_advance_boss",
                                                           pb_crumb))
                        {
                            // another objective owns the spline this window —
                            // FALL THROUGH exactly like the in-flight branch above.
                        }
                        else if (DungeonStepRefused(s, ai, pstep.x, pstep.y, pstep.z, now_ms))
                        {
                            // API refused this exact destination recently — FALL
                            // THROUGH so a different candidate is tried next tick
                            // instead of re-poisoning the API's own backoff.
                            DungeonStepRefusedDiag(s, "idle:dungeon_combat_advance_boss",
                                                   pstep.x, pstep.y, pstep.z);
                        }
                        else if (emit.move_to(pstep.x, pstep.y, pstep.z, /*run=*/true))
                        {
                            ai.set_last_rule_fired("idle:dungeon_combat_advance_boss");
                            ai.note_move_commit(s.map_id(), pstep.x, pstep.y, pstep.z, now_ms,
                                               pb_crumb);
                            return true;
                        }
                    }
                    // Boss not strictly reachable from here — let the normal
                    // combat path handle it (don't force a blind move).
                }
                // Healer out of range (or boss unreachable): yield. The tank fights
                // the trash on it roughly in place; the healer's regroup closes the
                // gap, then the advance resumes. No forced forward move = no solo
                // overrun.
            }
        }
    }

    // (0c) In-combat boss-advance — break the harbor GHOST-COMBAT wedge (Admiral
    // Ripsnarl, 2026-06-26). The idle boss-navigator (State_Idle DungeonDispatch)
    // is the ONLY navigator that reaches a FAR / off-mesh-footed boss: 500y scan,
    // incremental-progress path (allow_incomplete_progress), and off-mesh-deck
    // nearest-nav recovery. It is gated !in_combat. On the Deadmines harbor floor
    // the tank aggroes ~8 hostiles across an off-mesh / elevated edge it cannot
    // path to; they EVADE (leashed) yet stay in m_attackers (contact damage), so
    // the tank is held in_combat with victim() empty INDEFINITELY and the idle
    // navigator never runs — a permanent wedge ~263y short of Ripsnarl (observed
    // live 2026-06-26). With the executor change every doomed StartAttack on those
    // mobs now returns InvalidTarget and arms the 30s per-target refusal lockout,
    // and the self-acquire stops re-seeding a refused attacker — so once EVERY
    // attacker is refused (none reachable) the combat is a pure ghost the tank can
    // do nothing about. Drive it toward the boss exactly as the idle navigator
    // would (incremental, off-mesh-aware): it walks off the unreachable mobs (they
    // reset, combat drops), then the idle navigator resumes and engages. Rule (0)
    // already handles a CLOSE boss (<=150y), so this is strictly the far/harbor
    // case. Gated tight — victim empty + combat >=1.5s + EVERY attacker un-fightable
    // (untargetable or start_attack-refused) — so it can never override a legitimate
    // trash pull (a real fight always has a reachable, targetable victim, so victim()
    // is non-empty and this is skipped).
    // Cheap pre-gate: only a tank in active dungeon combat past the 1.5s settle,
    // with attackers and a boss list, and not an off-tank, can wedge/advance here.
    const bool tank_dungeon_combat_0c =
        ai.effective_role(s) == Role::Tank &&
        !s.is_casting() &&
        s.is_in_dungeon() && ai.dungeon_active() &&
        !s.is_encounter_in_progress() &&
        !advice.bosses.empty() &&
        s.in_combat() &&
        s.combat_duration_ms() >= 1500 &&
        !s.raw().combat.attackers.empty() &&
        !(s.raw().pve_order.active &&
          !s.raw().pve_order.main_tank.IsEmpty() &&
          s.raw().pve_order.main_tank != s.guid());
    // Real (targetable) hostiles within 24y of the tank — stalker-free pack
    // density (mirrors rule (0)'s near_hostiles loop): excludes untargetable
    // triggers / props / the boss / ignore_entries. Distinguishes a pure ghost
    // wedge from a thin-real-pack harbor fighting-advance. Computed only for an
    // eligible tank so non-tanks don't pay the scan.
    int near_real_hostiles_0c = 0;
    if (tank_dungeon_combat_0c)
    {
        float p0x, p0y, p0z; s.position(p0x, p0y, p0z);
        for (auto const& u : s.raw().combat.nearby_enemies)
        {
            if (u.hp <= 0 || u.no_xp_kill || u.is_pacified ||
                u.is_dungeon_boss || u.untargetable) continue;
            bool ign0 = false;
            for (uint32_t ie : advice.ignore_entries)
                if (ie == u.entry) { ign0 = true; break; }
            if (ign0) continue;
            const float hx = u.x - p0x, hy = u.y - p0y, hz = u.z - p0z;
            if (hx * hx + hy * hy + hz * hz <= 24.0f * 24.0f) ++near_real_hostiles_0c;
        }
    }
    // (0c) fires for EITHER a pure GHOST wedge (victim empty — every attacker
    // untargetable/refused, the original Ripsnarl ghost-combat case) OR a harbor
    // FIGHTING ADVANCE: a real victim is present but only a THIN real pack (<3)
    // surrounds the tank amid the untargetable stalker flood — an add stream to
    // move THROUGH toward Ripsnarl, not a dense pull to stand and grind. The
    // pull-segmentation in the advance guard below throttles the step so this
    // never blob-rushes successive packs together (the harbor wipe mode).
    const bool ghost_wedge_0c  = s.victim().IsEmpty();
    const bool push_context_0c = !s.victim().IsEmpty() && near_real_hostiles_0c < 3;
    if (tank_dungeon_combat_0c && (ghost_wedge_0c || push_context_0c))
    {
        // Ghost-wedge confirmation: EVERY live attacker un-fightable (untargetable
        // or start_attack-refused). Required ONLY for the ghost_wedge branch — in
        // push_context a real fightable victim exists by construction (all_refused
        // would be false), so the fighting advance proceeds without it.
        bool all_refused = true;
        for (auto const& a : s.raw().combat.attackers)
        {
            if (a.hp <= 0) continue;
            if (a.untargetable) continue;
            if (!ai.start_attack_recently_refused(a.guid.GetCounter(), now_ms))
            { all_refused = false; break; }
        }
        if (push_context_0c || all_refused)
        {
            if (Player* self_w = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
            {
                struct WideBossCheck
                {
                    Player const* origin;
                    std::vector<uint32_t> const& entries;
                    float range;
                    bool operator()(Creature* c) const
                    {
                        if (!c || !c->IsAlive()) return false;
                        for (uint32_t e : entries)
                            if (c->GetEntry() == e)
                                return origin->IsWithinDistInMap(c, range);
                        return false;
                    }
                };
                constexpr float kWideScanR = 500.0f;   // whole-dungeon
                constexpr float kWedgeStep = 20.0f;     // matches kMaxAdvanceStep
                std::list<Creature*> wcre;
                WideBossCheck wchk{self_w, advice.bosses, kWideScanR};
                Trinity::CreatureListSearcher<WideBossCheck> wsearch(self_w, wcre, wchk);
                Cell::VisitAllObjects(self_w, wsearch, kWideScanR);
                // Earliest live boss in progression order (advice.bosses authored
                // in encounter order) — never skip ahead to a later boss.
                Creature* wboss = nullptr;
                for (uint32_t bentry : advice.bosses)
                {
                    for (Creature* bc : wcre)
                        if (bc && bc->GetEntry() == bentry && bc->IsAlive())
                        { wboss = bc; break; }
                    if (wboss) break;
                }
                if (wboss)
                {
                    float wsx, wsy, wsz; s.position(wsx, wsy, wsz);
                    // HARBOR SURVIVABILITY (2026-06-27, workflow-derived). The
                    // Admiral Ripsnarl approach death is a TERMINAL deck pile-up,
                    // NOT ramp over-pull: ~39 PhaseId=0 Defias ring the (inert)
                    // Ripsnarl marker at -62,-823, and 24 of them are Pirates
                    // (48522/657) that LEAP-CLEAVE (90905) on aggro. A single 20y
                    // (0c) step lands the tank inside that ring and ~24 attackers
                    // engage near-simultaneously, far past one L30 healer's ceiling
                    // (~930 HP/s, tank 87%->dead in ~15s). Step size alone barely
                    // matters on the walk (13 vs 14 dealers at 8y vs 20y), so the
                    // real lever is capping SIMULTANEOUS attackers: make the harbor
                    // push PREVENTIVE. (a) An 8y (vs 20y) step rakes a thinner slice
                    // and re-checks the segmentation gate ~2.5x more often; (b)
                    // segmented_push requires fightable==0 at the harbor (below) so
                    // one cluster fully dies before the next is pulled — true
                    // pull/clear/step. Both hard-gated to map 36 + z<25, so no other
                    // dungeon / BG / normal combat changes. fightable_attackers_count
                    // is stalker-free, so the 49521 Lightning Stalker flood can't
                    // pin the gate and ghost_push (atk_fight==0) still carries a
                    // pure stalker pin — the group never freezes.
                    const bool harbor_floor = (s.map_id() == 36 && wsz < 25.0f);
                    const float wedgeStep = harbor_floor ? 8.0f : kWedgeStep;
                    // HARBOR COHESION GATE. (0c) runs IN COMBAT and so BYPASSES the
                    // idle dungeon_harbor_stage cohesion gate. Without this, the
                    // instant the tank descends to the harbor floor and aggroes the
                    // untargetable Lightning Stalker field it SOLO-RUSHES toward
                    // Ripsnarl, outrunning the still-descending healer, which strands
                    // a level up (z54) while the tank dies alone deep in the swarm at
                    // ~d2rip 219 (observed live 2026-06-26). The stalkers are
                    // stationary (MovementType 0) and only shed when the group keeps
                    // moving THROUGH together; a lone tank that stops accumulates 40+
                    // and dies. So at the harbor floor (map 36, z<30) HOLD the rush
                    // until the whole LIVING group has descended and balled up tight,
                    // then the advance below carries them through as one. Holding at
                    // the entrance edge (few stalkers) is survivable (verified live:
                    // the idle members held there at 88-100% HP). Mirrors the z<30
                    // ball-up of idle:dungeon_harbor_stage, generalized to in-combat.
                    // 45y ball-up (looser than harbor_stage's 25y): a group crossing
                    // a swarm trails the tank by up to the follower-rejoin distance
                    // (45y), so a tight gate would stutter-stop the tank mid-field and
                    // pile stalkers. 45y still trivially catches the solo-dive failure
                    // (the stranded healer was ~200y behind) while letting the balled
                    // group flow through without stops.
                    bool harbor_hold = false;
                    if (s.map_id() == 36 && wsz < 30.0f && g.exists() && g.members())
                    {
                        for (auto const& m : *g.members())
                        {
                            if (m.guid == s.guid()) continue;
                            if (!m.online || m.map_id != s.map_id() || !m.is_alive)
                                continue;
                            const float mdx = m.x - wsx, mdy = m.y - wsy,
                                        mdz = m.z - wsz;
                            if (mdx * mdx + mdy * mdy + mdz * mdz > 45.0f * 45.0f)
                            { harbor_hold = true; break; }
                        }
                        if (harbor_hold)
                        {
                            // Hold in place — do NOT advance and do NOT claim the
                            // tick, so the tank keeps its defensive rotation while
                            // regroup_follow_tank / go_rez pulls the group down to it.
                            ai.set_last_rule_fired("idle:dungeon_combat_harbor_hold");
                            if (ai.tank_diag_due(now_ms))
                                TC_LOG_INFO("playerbot.v2",
                                    "[harbor_hold] {} z={:.0f} hold rush until group balls up",
                                    s.name(), wsz);
                        }
                    }
                    // HEALER LEASH (mirror rule 0): don't outrun heals. A capped step
                    // keeps the healer in range and the follower-rejoin rule (0b)
                    // leapfrogs the group forward with the tank. No healer => moot.
                    float healer_d2 = std::numeric_limits<float>::max();
                    bool have_healer = false;
                    if (g.exists() && g.members())
                        for (auto const& m : *g.members())
                        {
                            if (!m.online || m.map_id != s.map_id()) continue;
                            if (m.guid == s.raw().guid || !m.is_alive) continue;
                            if (m.role != Role::Healer) continue;
                            have_healer = true;
                            const float mdx = m.x - wsx, mdy = m.y - wsy;
                            const float d2 = mdx * mdx + mdy * mdy;
                            if (d2 < healer_d2) healer_d2 = d2;
                        }
                    // PULL-SEGMENTATION (mirror rule (0)'s pack_clear): turn the
                    // fighting advance into pull-clear-step so it never blob-rushes
                    // successive Defias packs together (the harbor wipe mode).
                    // fightable_attackers_count() is stalker-free so the 49521 flood
                    // can't pin it at >=3 forever; near_real_hostiles_0c caps the
                    // SPREAD pack the tank sits at the edge of. member_fighting waits
                    // for the WHOLE group (not self) to drop its REAL targets before
                    // creeping — it reads victim / cast-target, NOT the bare in_combat
                    // flag (which stays phantom-true via stalker contact). post-kill
                    // settle is a 2.5s floor (no-op while combat never drops at the
                    // harbor; paces distinct episodes elsewhere).
                    const bool pack_clear_0c =
                        s.fightable_attackers_count() < 3 &&
                        near_real_hostiles_0c < 4 && s.hp_pct() > 35;
                    bool member_fighting_0c = false;
                    if (g.exists() && g.members())
                        for (auto const& m : *g.members())
                            if (m.online && m.is_alive && m.guid != s.guid() &&
                                m.map_id == s.map_id() &&
                                (!m.victim.IsEmpty() ||
                                 (m.is_casting && !m.casting_target.IsEmpty())))
                            { member_fighting_0c = true; break; }
                    const bool post_kill_settle_0c =
                        (now_ms - ai.last_kill_ms()) >= 2500;
                    // GHOST-WEDGE PUSH-THROUGH (2026-06-27): when the tank has NO
                    // targetable victim — a pure untargetable-stalker pin (the 49521
                    // Lightning Stalker flood at the harbor floor: 35+ attackers, all
                    // atk_fight=0) — there is NOTHING for the tank to clear and standing
                    // still is fatal: the no-damage stalkers merely pin the group
                    // InCombat while the REAL Defias adds focus the stationary blob and
                    // wipe it (observed live 2026-06-27: cohered at d2rip 241, push=0 on
                    // member_fight=1, wiped, respawned at entrance, looped). The
                    // member_fighting / pack_clear / post-kill gates SEGMENT real packs —
                    // irrelevant here (no real pack ON the tank), so they only freeze the
                    // group in the kill-zone. PUSH toward Ripsnarl keeping ONLY the
                    // cohesion gates: harbor_hold (ball up <=45y) so the push stays
                    // together, and healer-in-range so it never outruns heals. The
                    // medoid relocate recovers any straggler. For a thin REAL pack
                    // (push_context) keep the FULL pull-clear-step segmentation so the
                    // tank never blob-rushes successive Defias packs together.
                    const bool healer_ok_0c =
                        (!have_healer || healer_d2 <= 40.0f * 40.0f);
                    // Ghost-push ONLY on a true stalker pin: zero FIGHTABLE attackers
                    // on the tank (so it never abandons a real add it should be
                    // tanking). victim-empty + atk_fight==0 == nothing to clear.
                    const bool ghost_push     = ghost_wedge_0c && healer_ok_0c &&
                        s.fightable_attackers_count() == 0;
                    // HARBOR STOP-AND-KILL (rank 1): at the harbor floor advance
                    // ONLY between FULLY-cleared sub-pulls (fightable==0), exactly
                    // like ghost_push, so the tank never blob-rushes the deck
                    // kill-box. Other dungeons keep the existing fightable<3 cadence.
                    const bool segmented_push = !ghost_push && healer_ok_0c &&
                        pack_clear_0c && !member_fighting_0c && post_kill_settle_0c &&
                        (!harbor_floor || s.fightable_attackers_count() == 0);
                    if (!harbor_hold && (ghost_push || segmented_push))
                    {
                        const float wbx = wboss->GetPositionX();
                        const float wby = wboss->GetPositionY();
                        const float wbz = wboss->GetPositionZ();
                        // Incremental off-mesh-aware progress step (idle navigator's
                        // far-boss tier). The net-progress gate inside the helper
                        // keeps this INERT at a true navmesh disconnect.
                        // Increment 1b: step toward the armed route crumb instead
                        // of the raw boss position (see DungeonAdvanceTarget) — the
                        // harbor_hold / healer-leash / segmentation gates above and
                        // the wboss lookup itself are all still boss-position-based
                        // and untouched; only this walk target substitutes. The
                        // off-mesh NearestNavPoint fallback further below keeps
                        // targeting the raw boss deck (wbx/wby/wbz) unchanged — it
                        // is recovering from a boss-side navmesh disconnect, a
                        // different concern from the route crumb.
                        G3D::Vector3 wstep;
                        bool w_off = false;
                        float wbx_t, wby_t, wbz_t;
                        int32_t w_crumb = -1;
                        bool w_yield = false;
                        DungeonAdvanceTarget(s, ai, advice, wbx, wby, wbz,
                                            wbx_t, wby_t, wbz_t,
                                            "idle:dungeon_combat_wedge_advance",
                                            &w_crumb, &w_yield);
                        // Near-arrived at the cursor's crumb: yield the WHOLE emit
                        // region (primary step AND the NearestNavPoint fallback —
                        // both would walk boss-ward) so the route rule owns
                        // completion; hold-claim per this site's 1k shape.
                        if (w_yield)
                        {
                            ai.set_last_rule_fired("idle:dungeon_combat_wedge_advance_hold");
                            return true;
                        }
                        if (DungeonTargetReachableAndStep(self_w, wbx_t, wby_t, wbz_t,
                                wedgeStep, wstep, &w_off,
                                /*allow_incomplete_progress=*/true))
                        {
                            if (w_off)
                                ai.set_dungeon_cross(wstep.x, wstep.y, wstep.z,
                                                     now_ms + 12000);
                            if (DungeonMoveOwnedElsewhere(s, ai, wstep.x, wstep.y, wstep.z,
                                                          now_ms,
                                                          "idle:dungeon_combat_wedge_advance",
                                                          w_crumb))
                            {
                                ai.set_last_rule_fired("idle:dungeon_combat_wedge_advance_hold");
                                return true;
                            }
                            if (emit.move_to(wstep.x, wstep.y, wstep.z, /*run=*/true))
                            {
                                ai.set_last_rule_fired("idle:dungeon_combat_wedge_advance");
                                ai.note_move_commit(s.map_id(), wstep.x, wstep.y, wstep.z, now_ms,
                                                   w_crumb);
                                return true;
                            }
                        }
                        // Off-mesh boss DESTINATION (Ripsnarl ship deck at z42.8
                        // where findNearestPoly resolves no poly at his feet): route
                        // to the nearest navmesh poly beside/below the deck, which IS
                        // reachable when only the elevated deck is off-mesh.
                        Position wnn;
                        if (BotMovement::NearestNavPoint(self_w, wbx, wby, wbz,
                                                         45.0f, 45.0f, wnn))
                        {
                            G3D::Vector3 wnn_step;
                            bool wnn_off = false;
                            if (DungeonTargetReachableAndStep(self_w,
                                    wnn.GetPositionX(), wnn.GetPositionY(),
                                    wnn.GetPositionZ(), wedgeStep, wnn_step,
                                    &wnn_off, /*allow_incomplete_progress=*/true))
                            {
                                if (wnn_off)
                                    ai.set_dungeon_cross(wnn_step.x, wnn_step.y,
                                                         wnn_step.z, now_ms + 12000);
                                if (emit.move_to(wnn_step.x, wnn_step.y, wnn_step.z,
                                                 /*run=*/true))
                                {
                                    ai.set_last_rule_fired("idle:dungeon_combat_wedge_advance");
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // (0d) Tank long-detour CHASE COMMITMENT (config-gated, 2026-07-02, stage
    // 4). Once we choose to walk a genuinely long-but-valid corridor to a
    // locked combat victim (a taunt_peel target the tank got pulled onto, a
    // boss, or a Task-2/3 survivor), KEEP WALKING it instead of re-deciding
    // every tick — the SFK courtyard wedge is oscillation on a long-but-
    // reachable path, not an unreachable one. Re-plan at most every 3s
    // (kCommitReplanMs), never re-pick mid-corridor, and give up loudly after
    // TankCommitMaxMs so the stage-3 dungeon:untankable_disengage / normal
    // escape paths own a genuinely stuck pull instead of this latch holding
    // it forever. Mirrors the route-follower's committed-cursor idiom
    // (:4742-4968): the fix for oscillation is refusing to re-decide.
    //
    // Placement (deliberate): AFTER rule (0) and (0c) above, so the leashed
    // boss-push rules get first right of refusal on the tick. Their healer
    // leashes only guard THEIR boss-directed pushes and fall through
    // silently, so this rule carries its OWN healer leash (mirror of
    // healer_ok_0c, 40y) on both the commitment START and every chase step
    // — a committing tank never outruns its heals; the group still follows
    // via regroup_follow_tank.
    if (Services::Config().tank_commit_enabled() &&
        ai.effective_role(s) == Role::Tank &&
        s.is_in_dungeon() && ai.dungeon_active() &&
        s.in_combat() && !s.victim().IsEmpty())
    {
        constexpr uint32 kCommitReplanMs = 3000;
        NearbyUnit const* vu = nullptr;
        for (auto const& u : s.raw().combat.nearby_enemies)
            if (u.guid == s.victim()) { vu = &u; break; }
        if (vu)
        {
            float ccx, ccy, ccz; s.position(ccx, ccy, ccz);
            // HEALER LEASH (review fix, 2026-07-02). Rules (0)/(0c) above
            // leash only their BOSS-directed pushes and fall through silently
            // when the healer lags — they do not leash this rule's
            // victim-directed walk. A committing tank must never outrun its
            // heals for up to the whole commit window, so mirror (0c)'s
            // healer_ok_0c shape (40y, XY) and require it for BOTH the
            // commitment START and EVERY chase step below.
            float healer_d2 = std::numeric_limits<float>::max();
            bool have_healer = false;
            if (g.exists() && g.members())
                for (auto const& m : *g.members())
                {
                    if (!m.online || m.map_id != s.map_id()) continue;
                    if (m.guid == s.raw().guid || !m.is_alive) continue;
                    if (m.role != Role::Healer) continue;
                    have_healer = true;
                    const float mdx = m.x - ccx, mdy = m.y - ccy;
                    const float d2 = mdx * mdx + mdy * mdy;
                    if (d2 < healer_d2) healer_d2 = d2;
                }
            const bool healer_in_leash = !have_healer || healer_d2 <= 40.0f * 40.0f;
            bool committed = (ai.chase_commit_target(s.map_id()) == s.victim() &&
                              ai.chase_commit_since_ms(s.map_id()) != 0);
            if (!committed)
            {
                // PRE-COMMIT PROBE COST (review fix, 2026-07-02). The detour
                // probe is a full SehSafeCalculatePath and this branch runs
                // for EVERY in-combat dungeon tank with a victim — every
                // ordinary trash pull, module-wide — so it must not run
                // per-tick (world-thread pathfind-storm history). Two cheap
                // pre-filters: (a) a victim within 12y beeline can never be
                // an excessive-detour commit candidate — skip; (b) probe at
                // most every kCommitReplanMs — the latch's plan-timestamp
                // DOUBLES as the pre-commit probe cadence (touched on every
                // probe; set_chase_commit re-stamps it when a commitment
                // actually starts).
                const float pdx = vu->x - ccx, pdy = vu->y - ccy, pdz = vu->z - ccz;
                const float victim_d2 = pdx * pdx + pdy * pdy + pdz * pdz;
                if (healer_in_leash &&
                    victim_d2 > 12.0f * 12.0f &&
                    now_ms - ai.chase_commit_last_plan_ms(s.map_id()) >= kCommitReplanMs)
                {
                    // Commit only when the corridor is genuinely LONG (the
                    // short-path case stays the existing advance's job) and
                    // COMPLETE (NOPATH/INCOMPLETE stays with the escape/
                    // disengage paths, never this latch). DungeonDetourVerdict
                    // is an incomplete type this early in the translation unit
                    // (definition is after DungeonTargetReachableAndStep further
                    // down), so use the scalar-out wrapper — same reason
                    // dungeon:untankable_disengage above uses it.
                    if (Player* self_cc = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                    {
                        ai.touch_chase_commit_plan(now_ms, s.map_id());
                        float dv_beeline = 0.f, dv_path_len = 0.f, dv_ratio = 0.f;
                        bool  dv_complete = false;
                        DungeonTankDetourExcessiveVerbose(self_cc, vu->x, vu->y, vu->z,
                                                          dv_beeline, dv_path_len,
                                                          dv_ratio, dv_complete);
                        // START uses the same "excessive" definition as the pull
                        // gates (DungeonDetourExcessive above): ratio alone lets a
                        // 13y-beeline/40y-path ordinary mid-range chase satisfy the
                        // ratio term and START a commitment; the MinExtraYards floor
                        // is required too (final-review fix, 2026-07-03).
                        if (dv_complete && dv_ratio > Services::Config().pull_gate_max_ratio() &&
                            dv_path_len - dv_beeline > Services::Config().pull_gate_min_extra_yards())
                        {
                            ai.set_chase_commit(s.victim(), now_ms, s.map_id());
                            TC_LOG_INFO("playerbot.v2",
                                "[commit] START bot={} victim={} path={:.1f} beeline={:.1f} ratio={:.2f}",
                                s.bot_id(), s.victim().ToString(), dv_path_len, dv_beeline, dv_ratio);
                            committed = true;
                        }
                    }
                }
            }
            if (committed)
            {
                uint32 const commit_since_ms = ai.chase_commit_since_ms(s.map_id());
                if (now_ms - commit_since_ms > Services::Config().tank_commit_max_ms())
                {
                    ai.clear_chase_commit();
                    emit.stop_attack();
                    ai.note_engage(s.victim(), now_ms, 60000u);
                    TC_LOG_INFO("playerbot.v2",
                        "[commit] GIVE_UP bot={} victim={} after={}ms",
                        s.bot_id(), s.victim().ToString(), now_ms - commit_since_ms);
                    ai.set_last_rule_fired("dungeon:tank_commit_give_up");
                    return true;
                }
                // Arrived? Melee range of the victim -> normal combat owns
                // it from here (do NOT return; fall through).
                const float cdx = vu->x - ccx, cdy = vu->y - ccy, cdz = vu->z - ccz;
                if (cdx * cdx + cdy * cdy + cdz * cdz < 8.0f * 8.0f)
                {
                    ai.clear_chase_commit();
                }
                else
                {
                    // ROTATION GUARD (review fix, 2026-07-02).
                    // DungeonCombatPositioning returns BEFORE the APL
                    // rotation in State_InCombat, so an unconditional
                    // hold/chase claim would starve the tank's own rotation
                    // (defensive CDs / self-heals / threat) for up to the
                    // whole commit window while it takes real hits. Mirror
                    // rule (0)'s pack_clear: commitment walking is for
                    // BETWEEN-fights corridor traversal, same as (0)/(0c) —
                    // while a real pack is on the tank (or its HP is low),
                    // and likewise when the healer leash breaks mid-corridor,
                    // do NOT step and do NOT claim the tick: fall through so
                    // the rotation and the heal/regroup rules run. The latch
                    // stays ARMED and the give-up clock keeps ticking, so the
                    // commitment stays bounded; the walk resumes once the
                    // pack thins / the healer catches up.
                    int cc_near_hostiles = 0;
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.hp <= 0 || u.no_xp_kill || u.is_pacified ||
                            u.is_dungeon_boss || u.untargetable) continue;
                        bool ign_cc = false;
                        for (uint32_t ie : advice.ignore_entries)
                            if (ie == u.entry) { ign_cc = true; break; }
                        if (ign_cc) continue;
                        const float hx = u.x - ccx, hy = u.y - ccy, hz = u.z - ccz;
                        if (hx * hx + hy * hy + hz * hz <= 24.0f * 24.0f)
                            ++cc_near_hostiles;
                    }
                    const bool cc_pack_clear =
                        s.fightable_attackers_count() < 3 &&
                        cc_near_hostiles < 4 && s.hp_pct() > 35;
                    if (healer_in_leash && cc_pack_clear)
                    {
                        if (now_ms - ai.chase_commit_last_plan_ms(s.map_id()) >= kCommitReplanMs)
                        {
                            ai.touch_chase_commit_plan(now_ms, s.map_id());
                            if (Player* self_cs = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                            {
                                G3D::Vector3 cstep;
                                bool c_off = false;
                                // Long stride along the corridor; INCOMPLETE
                                // progress accepted mid-corridor (same flag the
                                // boss-wedge advance above uses) — the give-up
                                // clock bounds the pathology at a dead end.
                                if (DungeonTargetReachableAndStep(self_cs, vu->x, vu->y, vu->z,
                                        /*maxStep*/ 30.0f, cstep, &c_off,
                                        /*allow_incomplete_progress=*/true))
                                {
                                    if (c_off)
                                        ai.set_dungeon_cross(cstep.x, cstep.y, cstep.z,
                                                             now_ms + 12000);
                                    if (emit.move_to(cstep.x, cstep.y, cstep.z, /*run=*/true))
                                    {
                                        ai.set_last_rule_fired("idle:dungeon_tank_commit_chase");
                                        return true;
                                    }
                                }
                            }
                            // Step failed this tick (budget / transient
                            // no-path) — hold instead of falling through to a
                            // re-pick; the give-up clock still bounds a
                            // genuine dead end.
                            ai.set_last_rule_fired("idle:dungeon_tank_commit_hold");
                            return true;
                        }
                        // Inside the re-plan window: let the previous stride
                        // finish; claim the tick so nothing re-decides.
                        ai.set_last_rule_fired("idle:dungeon_tank_commit_hold");
                        return true;
                    }
                    // Leash broken or pack on the tank: fall through (no
                    // step, no tick claim, latch armed — see comment above).
                }
            }
        }
        else if (ai.chase_commit_since_ms(s.map_id()) != 0)
            ai.clear_chase_commit();   // victim left the snapshot
    }

    // (0b) FOLLOWER rejoin IN COMBAT (Deadmines Gap-1, 2026-06-26). The InGroup
    // dungeon-rejoin and the opener yield cover OOC / InGroup stragglers, but a
    // non-tank in the InCombat state (Engage on a stray NE trash mob) reaches
    // neither — so it stays put while the group is across the gap, the tank's
    // guard_group chases to protect it, and the squad oscillates off the boss
    // route. Mirror the rejoin here: a follower >40y from the tank DROPS its
    // target and crosses to the tank (committed, off-mesh-aware). Suppressed once
    // the boss encounter is in progress (then followers fight, not regroup). The
    // tank's heal-leash holds it in place meanwhile, so this converges.
    if (ai.effective_role(s) != Role::Tank &&
        s.is_in_dungeon() && ai.dungeon_active() &&
        !s.is_encounter_in_progress() &&
        g.exists() && g.members())
    {
        if (GroupMemberSummary const* tk = g.tank())
        {
            if (tk->online && tk->is_alive && tk->guid != s.raw().guid &&
                tk->map_id == s.map_id())
            {
                float fx, fy, fz; s.position(fx, fy, fz);
                const float dxt = tk->x - fx, dyt = tk->y - fy, dzt = tk->z - fz;
                // 3D gap: across the Gap-1 z-step a straggler can sit <40y planar
                // yet far above/below the tank — a planar-only gate let it fall
                // through both this rejoin and the 22y cohesion floor into the
                // silent dungeon_hold. Include dz so a z-displaced follower rejoins.
                if (dxt * dxt + dyt * dyt + dzt * dzt > 40.0f * 40.0f)
                {
                    if (!s.victim().IsEmpty())
                        emit.stop_attack();
                    if (Player* self_fr =
                            ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                    {
                        G3D::Vector3 frstep;
                        bool fr_off = false;
                        if (DungeonTargetReachableAndStep(self_fr, tk->x, tk->y, tk->z,
                                                          45.0f, frstep, &fr_off))
                        {
                            if (fr_off)
                                ai.set_dungeon_cross(frstep.x, frstep.y, frstep.z,
                                                     now_ms + 12000);
                            if (DungeonStepAlreadyInFlight(s, frstep.x, frstep.y, frstep.z))
                            {
                                DungeonStepHoldDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                    frstep.x, frstep.y, frstep.z);
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (DungeonMoveOwnedElsewhere(s, ai, frstep.x, frstep.y,
                                                          frstep.z, now_ms,
                                                          "idle:dungeon_combat_rejoin_tank"))
                            {
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (DungeonStepRefused(s, ai, frstep.x, frstep.y, frstep.z, now_ms))
                            {
                                DungeonStepRefusedDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                       frstep.x, frstep.y, frstep.z);
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (emit.move_to(frstep.x, frstep.y, frstep.z, /*run=*/true))
                                ai.note_move_commit(s.map_id(), frstep.x, frstep.y, frstep.z, now_ms);
                        }
                        else
                        {
                            // Direct path to the tank is gap-broken (NoPath).
                            // Rejoin along the crumb-route — same fix + same crumb
                            // as the InGroup rejoin, so the Idle/InGroup states
                            // share ONE goal-key and never flip-flop the spline
                            // (the Gap-1 two-state yo-yo this module already fought).
                            {
                                float qx, qy, qz;
                                if (DungeonRouteStepTowardPos(s, tk->x, tk->y, tk->z,
                                                              45.0f, qx, qy, qz))
                                {
                                    if (DungeonStepAlreadyInFlight(s, qx, qy, qz))
                                    {
                                        DungeonStepHoldDiag(s, "idle:dungeon_combat_rejoin_tank_route",
                                                            qx, qy, qz);
                                        ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                        return true;
                                    }
                                    if (emit.move_to(qx, qy, qz, /*run=*/true))
                                        ai.note_move_commit(s.map_id(), qx, qy, qz, now_ms);
                                    ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_route");
                                    return true;
                                }
                            }
                            if (DungeonStepAlreadyInFlight(s, tk->x, tk->y, tk->z))
                            {
                                DungeonStepHoldDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                    tk->x, tk->y, tk->z);
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (DungeonMoveOwnedElsewhere(s, ai, tk->x, tk->y, tk->z, now_ms,
                                                          "idle:dungeon_combat_rejoin_tank"))
                            {
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (DungeonStepRefused(s, ai, tk->x, tk->y, tk->z, now_ms))
                            {
                                DungeonStepRefusedDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                       tk->x, tk->y, tk->z);
                                ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                                return true;
                            }
                            if (emit.move_to(tk->x, tk->y, tk->z, /*run=*/true))
                                ai.note_move_commit(s.map_id(), tk->x, tk->y, tk->z, now_ms);
                        }
                    }
                    else
                    {
                        if (DungeonStepAlreadyInFlight(s, tk->x, tk->y, tk->z))
                        {
                            DungeonStepHoldDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                tk->x, tk->y, tk->z);
                            ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                            return true;
                        }
                        if (DungeonMoveOwnedElsewhere(s, ai, tk->x, tk->y, tk->z, now_ms,
                                                      "idle:dungeon_combat_rejoin_tank"))
                        {
                            ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                            return true;
                        }
                        if (DungeonStepRefused(s, ai, tk->x, tk->y, tk->z, now_ms))
                        {
                            DungeonStepRefusedDiag(s, "idle:dungeon_combat_rejoin_tank",
                                                   tk->x, tk->y, tk->z);
                            ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank_hold");
                            return true;
                        }
                        if (emit.move_to(tk->x, tk->y, tk->z, /*run=*/true))
                            ai.note_move_commit(s.map_id(), tk->x, tk->y, tk->z, now_ms);
                    }
                    ai.set_last_rule_fired("idle:dungeon_combat_rejoin_tank");
                    return true;
                }
            }
        }
    }

    // (1) Step-out of dangerous aura (boss ground-effects). If the bot has
    // any aura listed in advice.dangerous_auras, walk ~14y away from the
    // aura's caster. Skipped for tanks (must keep boss positioned) and while
    // casting.
    if (!advice.dangerous_auras.empty() &&
        !s.is_casting() &&
        ai.effective_role(s) != Role::Tank)
    {
        for (uint32 spell_id : advice.dangerous_auras)
        {
            AuraEntry const* aura = s.find_aura(spell_id);
            if (!aura) continue;
            // Find the caster in nearby_enemies for its position.
            NearbyUnit const* caster = nullptr;
            for (auto const& e : s.nearby_enemies())
            {
                if (e.guid == aura->caster) { caster = &e; break; }
            }
            if (!caster) continue;

            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            float dx = self_x - caster->x;
            float dy = self_y - caster->y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.5f)  // nearly stacked on caster — pick a default vector
            {
                dx = 1.0f; dy = 0.0f; len = 1.0f;
            }
            // Step to a point 14y from caster along the same axis.
            float scale = 14.0f / len;
            float step_x = caster->x + dx * scale;
            float step_y = caster->y + dy * scale;
            emit.move_to(step_x, step_y, self_z, /*run=*/true);
            ai.set_last_rule_fired("idle:dungeon_step_out_dangerous_aura");
            return true;
        }
    }

    // (2) Kite-on-fixate (M+ Spiteful Shade, raid adds that fixate DPS).
    // Fires when a creature whose entry is in advice.kite_creature_entries
    // has the bot as its current victim. The bot runs ~25y on the
    // away-vector from the kiter.
    if (!advice.kite_creature_entries.empty())
    {
        float bx2 = 0.f, by2 = 0.f, bz2 = 0.f;
        s.position(bx2, by2, bz2);
        NearbyUnit const* kiter = nullptr;
        for (auto const& u : s.raw().combat.nearby_enemies)
        {
            if (u.hp <= 0) continue;
            if (u.victim != s.raw().guid) continue;
            bool flagged = false;
            for (uint32_t e : advice.kite_creature_entries)
                if (e == u.entry) { flagged = true; break; }
            if (flagged) { kiter = &u; break; }
        }
        if (kiter)
        {
            float dx = bx2 - kiter->x;
            float dy = by2 - kiter->y;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len < 0.5f) { dx = 1.f; dy = 0.f; len = 1.f; }
            const float scale = 25.0f / len;
            emit.move_to(kiter->x + dx * scale,
                         kiter->y + dy * scale, bz2, /*run=*/true);
            ai.set_last_rule_fired("idle:dungeon_kite_fixate");
            return true;
        }
    }

    // (3) Soak (raid orb-soaks etc). When an enemy casts a spell listed in
    // advice.soak_spells, move INTO the spell's ground area (inverted
    // polarity vs dangerous_auras). Approximates the soak target as the
    // closest enemy currently casting a soak spell.
    // PvE-coordinator gate: when a group plan is active, only DESIGNATED
    // soakers walk in (soaker == 1); soaker == 2 stays out. Without the
    // gate this rule marched the ENTIRE group into one soak circle —
    // 4 wasted bodies in a 5-man, a raid-wide detonation in a 25-man.
    if (!advice.soak_spells.empty() &&
        !(s.raw().pve_order.active && s.raw().pve_order.soaker == 2))
    {
        NearbyUnit const* soak_src = nullptr;
        float soak_d2 = std::numeric_limits<float>::max();
        float bx5 = 0.f, by5 = 0.f, bz5 = 0.f;
        s.position(bx5, by5, bz5);
        for (auto const& u : s.raw().combat.nearby_enemies)
        {
            if (u.hp <= 0 || !u.is_casting) continue;
            bool flagged = false;
            for (uint32_t ss : advice.soak_spells)
                if (ss == u.casting_spell_id) { flagged = true; break; }
            if (!flagged) continue;
            const float dx5 = u.x - bx5, dy5 = u.y - by5;
            const float d2 = dx5*dx5 + dy5*dy5;
            if (d2 < soak_d2) { soak_d2 = d2; soak_src = &u; }
        }
        if (soak_src && soak_d2 > 3.0f * 3.0f)
        {
            emit.move_to(soak_src->x, soak_src->y, soak_src->z,
                         /*run=*/true);
            ai.set_last_rule_fired("idle:dungeon_soak");
            return true;
        }
    }

    // (4) Spread-on-self-aura (M+ Quaking, raid AoE-explode debuffs). When
    // the bot has any aura listed in advice.spread_on_self_auras, move ~10y
    // from the nearest ally so the detonation doesn't hit the group.
    if (!advice.spread_on_self_auras.empty())
    {
        bool flagged = false;
        for (uint32_t a : advice.spread_on_self_auras)
            if (s.has_aura(a)) { flagged = true; break; }
        if (flagged)
        {
            float bx3 = 0.f, by3 = 0.f, bz3 = 0.f;
            s.position(bx3, by3, bz3);
            NearbyUnit const* closest_ally = nullptr;
            float closest_d2 = 8.0f * 8.0f;
            for (auto const& f : s.raw().combat.nearby_friends)
            {
                if (f.hp <= 0) continue;
                if (f.guid == s.raw().guid) continue;
                const float dx3 = f.x - bx3;
                const float dy3 = f.y - by3;
                const float d2 = dx3*dx3 + dy3*dy3;
                if (d2 < closest_d2) { closest_d2 = d2; closest_ally = &f; }
            }
            if (closest_ally)
            {
                auto const& spread_po = s.raw().pve_order;
                if (spread_po.active && spread_po.spread_slot != 0xFF)
                {
                    // Coordinator bearing slot: members fan out on
                    // distinct 45° spokes. The legacy step-away-from-
                    // nearest cascades when two members are mutually
                    // nearest — both step, remain nearest, step again.
                    const float ang =
                        float(spread_po.spread_slot % 8u) * 0.785398f;
                    emit.move_to(bx3 + std::cos(ang) * 10.0f,
                                 by3 + std::sin(ang) * 10.0f, bz3,
                                 /*run=*/true);
                }
                else
                {
                    float dx3 = bx3 - closest_ally->x;
                    float dy3 = by3 - closest_ally->y;
                    float len = std::sqrt(dx3*dx3 + dy3*dy3);
                    if (len < 0.5f) { dx3 = 1.f; dy3 = 0.f; len = 1.f; }
                    const float scale = 10.0f / len;
                    emit.move_to(closest_ally->x + dx3 * scale,
                                 closest_ally->y + dy3 * scale, bz3,
                                 /*run=*/true);
                }
                ai.set_last_rule_fired("idle:dungeon_spread_on_self_aura");
                return true;
            }
        }
    }

    // (5) Stack-on-cast (raid stack-soaks). When any visible enemy casts a
    // spell listed in advice.stack_on_cast_spells, non-tanks move to stack on
    // the closest tank ally. Only fires while the cast is in flight.
    if (!advice.stack_on_cast_spells.empty() &&
        ai.effective_role(s) != Role::Tank)
    {
        bool cast_in_flight = false;
        for (auto const& u : s.raw().combat.nearby_enemies)
        {
            if (u.hp <= 0 || !u.is_casting) continue;
            for (uint32_t cs : advice.stack_on_cast_spells)
                if (cs == u.casting_spell_id) { cast_in_flight = true; break; }
            if (cast_in_flight) break;
        }
        if (cast_in_flight)
        {
            NearbyUnit const* tank_target = nullptr;
            for (auto const& f : s.raw().combat.nearby_friends)
            {
                if (f.hp <= 0) continue;
                if (f.role != Role::Tank) continue;
                tank_target = &f;   // first tank seen is fine
                break;
            }
            if (tank_target)
            {
                float bx4 = 0.f, by4 = 0.f, bz4 = 0.f;
                s.position(bx4, by4, bz4);
                const float dx4 = tank_target->x - bx4;
                const float dy4 = tank_target->y - by4;
                if (dx4*dx4 + dy4*dy4 > 3.0f * 3.0f)
                {
                    emit.move_to(tank_target->x, tank_target->y,
                                 tank_target->z, /*run=*/true);
                    ai.set_last_rule_fired("idle:dungeon_stack_on_cast");
                    return true;
                }
            }
        }
    }

    // (6) Melee behind-positioning. Non-tank melee specs lose ~25% of frontal
    // hits to parry against boss-tier enemies. Real players sidestep to the
    // rear. The previous inline body carried an `s.in_combat()` gate; that is
    // now redundant because this function is the combat-path positioner, so
    // it has been removed. Spec-aware melee detection is retained (replicated
    // inline rather than cross-including State_InGroup's file-local
    // IsMeleeClass) — only confirmed melee specs reposition.
    if (!s.is_casting() &&
        !s.victim().IsEmpty() &&
        ai.effective_role(s) != Role::Tank)
    {
        const uint8 cls_bp = s.raw().identity.cls;
        const uint32 spec_bp = s.spec();
        bool is_melee_class_bp = false;
        switch (cls_bp)
        {
            case 1:  /*Warrior*/
            case 2:  /*Paladin*/
            case 4:  /*Rogue*/
            case 6:  /*DK*/
            case 10: /*Monk*/
            case 12: /*DH*/
                is_melee_class_bp = true; break;
            case 3:  /*Hunter*/  is_melee_class_bp = (spec_bp == 255); break;        // Survival
            case 11: /*Druid*/   is_melee_class_bp = (spec_bp == 103 || spec_bp == 104); break; // Feral/Guardian
            case 7:  /*Shaman*/  is_melee_class_bp = (spec_bp == 263); break;        // Enhancement
            default: is_melee_class_bp = false; break;
        }
        if (is_melee_class_bp)
        {
            // Look up victim's NearbyUnit entry for x/y/o.
            ObjectGuid const vg_bp = s.victim();
            NearbyUnit const* vt_bp = nullptr;
            for (auto const& u : s.raw().combat.nearby_enemies)
                if (u.guid == vg_bp) { vt_bp = &u; break; }
            if (vt_bp && vt_bp->hp > 0 &&
                (vt_bp->is_dungeon_boss || vt_bp->max_hp >= 5'000'000))
            {
                float bx_bp, by_bp, bz_bp; s.position(bx_bp, by_bp, bz_bp);
                const float dxv = bx_bp - vt_bp->x;
                const float dyv = by_bp - vt_bp->y;
                const float d2v = dxv*dxv + dyv*dyv;
                if (d2v <= 5.0f * 5.0f && d2v > 0.25f)
                {
                    // Target's frontal vector is (cos o, sin o). Bot is in
                    // target's frontal half-plane when the dot product
                    // (front · botRel) > 0.
                    const float front_x = std::cos(vt_bp->o);
                    const float front_y = std::sin(vt_bp->o);
                    const float dot = (dxv * front_x + dyv * front_y);
                    if (dot > 0.0f)   // in target's frontal half-plane
                    {
                        const uint64 reposition_key = uint64(vg_bp.GetCounter());
                        if (!ai.action_recently_tried(BotAI::ActionKind::WanderToNode,
                                                      reposition_key, now_ms))
                        {
                            // Aim 3y behind the target.
                            const float aim_x = vt_bp->x - front_x * 3.0f;
                            const float aim_y = vt_bp->y - front_y * 3.0f;
                            if (emit.move_to(aim_x, aim_y, vt_bp->z, /*run*/ false))
                            {
                                ai.note_action_retry(BotAI::ActionKind::WanderToNode,
                                                     reposition_key, now_ms);
                                ai.set_last_rule_fired("idle:melee_behind");
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

// ---------- Shared BG carrier-homeward resolution (BUG BG-P0a) ----------
// When the bot is carrying the flag / an orb it must keep running toward its
// capture point even while attacked (real players never stop to trade blows
// with the chaser). Resolves the destination the same way the idle BG
// cascade does — static own-flag pedestal first, then closest owned node,
// then Kotmogu home_base — and emits a run move_to. Returns true when a
// homeward move was emitted so the combat path can suppress the melee
// gap-close. Returns false when the bot isn't a carrier or no destination
// resolves.
bool BgCarrierHomeward(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit,
                       BattlegroundAdvice const& bg_advice)
{
    if (!s.is_alive()) return false;

    // Carrier identity: the scalar tracks only the first detected carrier,
    // so consult the multi-carrier vector too (Kotmogu has up to 4).
    bool i_am_carrier = (s.bg_friendly_flag_carrier() == s.guid());
    if (!i_am_carrier)
    {
        for (auto const& fc : s.bg_all_friendly_carriers())
            if (fc == s.guid()) { i_am_carrier = true; break; }
    }
    if (!i_am_carrier) return false;

    float self_x = 0.f, self_y = 0.f, self_z = 0.f;
    s.position(self_x, self_y, self_z);

    // Destination resolution (mirrors the idle idle:bg_fc_return_flag /
    // idle:bg_orb_hold_center logic).
    //   1. static own-flag pedestal (WSG/TP/BfG).
    //   2. closest owned node (EotS — cap at any owned base).
    //   3. Kotmogu home_base (map center hold).
    float dest_x = bg_advice.own_flag_x;
    float dest_y = bg_advice.own_flag_y;
    float dest_z = bg_advice.own_flag_z;
    if (dest_x == 0.f && dest_y == 0.f)
    {
        uint8 my_team_id = s.team();
        BotSnapshot::BgNodeState const* target = nullptr;
        float best_dsq = std::numeric_limits<float>::max();
        for (auto const& n : s.bg_node_states())
        {
            if (n.owner_team != my_team_id) continue;
            float dx = n.x - self_x;
            float dy = n.y - self_y;
            float dsq = dx * dx + dy * dy;
            if (dsq < best_dsq) { best_dsq = dsq; target = &n; }
        }
        if (target)
        {
            dest_x = target->x; dest_y = target->y; dest_z = target->z;
        }
    }
    if (dest_x == 0.f && dest_y == 0.f)
    {
        dest_x = bg_advice.home_base_x;
        dest_y = bg_advice.home_base_y;
        dest_z = bg_advice.home_base_z;
    }
    // EotS zero-towers case (BG audit N30): no owned node, no static
    // pedestal, no home_base — the carrier used to park at mid with the
    // flag forever. Shadow the team instead: head for the script node
    // closest to the friendly cluster (the tower the team is pushing),
    // so the cap happens the instant it flips.
    if (dest_x == 0.f && dest_y == 0.f && !bg_advice.nodes.empty())
    {
        float fx = 0.f, fy = 0.f; uint32 fn = 0;
        for (auto const& fr : s.nearby_friends())
        {
            if (!fr.is_player || fr.hp <= 0) continue;
            fx += fr.x; fy += fr.y; ++fn;
        }
        if (fn >= 2)
        {
            fx /= float(fn); fy /= float(fn);
            float best = std::numeric_limits<float>::max();
            for (auto const& n : bg_advice.nodes)
            {
                const float ddx = n.x - fx, ddy = n.y - fy;
                const float d2 = ddx * ddx + ddy * ddy;
                if (d2 < best) { best = d2; dest_x = n.x; dest_y = n.y; dest_z = n.z; }
            }
        }
    }
    if (dest_x == 0.f && dest_y == 0.f) return false;  // nothing to run toward

    // Arrival gate must be 3D. The capture AreaTrigger is a 5y SPHERE at the
    // pedestal Z (areatrigger_create_properties Id 30/31: Shape=Sphere r=5,
    // SearchUnitInSphere is check3D=true). On split-elevation CTF maps — Twin
    // Peaks: Alliance base z=44, Horde base z=2.4, ~42y apart — a 2D-only gate
    // let the carrier stop directly over/under the pedestal while still tens of
    // yards BELOW/ABOVE it, i.e. OUTSIDE the 3D sphere, so OnUnitEnter never
    // fired and the flag cycled Taken->Dropped without ever capping (WSG's
    // near-flat bases hid this — 2D≈3D there). Drive move_to until the body is
    // genuinely inside the capture volume (3y 3D < the 5y sphere radius).
    float dx = dest_x - self_x;
    float dy = dest_y - self_y;
    float dz = dest_z - self_z;
    if (dx * dx + dy * dy + dz * dz <= 9.0f) return false;  // inside the 5y cap sphere

    emit.move_to(dest_x, dest_y, dest_z, /*run=*/true);
    ai.set_last_rule_fired("combat:bg_carrier_homeward");
    return true;
}

// ---------- Shared BG objective-GO interaction (BG audit S1) ----------
// The body was previously inline in the idle BG cascade and hard-gated on
// !s.in_combat(), which made every contested objective (defended WSG/TP
// flagroom, contested EotS mid / AB-BfG-AV node) un-clickable — the single
// biggest "no bot picks up a flag" / "node never caps" blocker. Extracted so
// State_InCombat can run the exact same proven scan in combat. Callers own
// the is_alive / casting / in_combat gating.
bool BgTryUseObjectiveGo(BotSnapshotView const& s, BotAI& ai,
                         BotIntentEmitter& emit,
                         BattlegroundAdvice const& bg_advice)
{
    if (bg_advice.auto_use_go_types.empty() &&
        bg_advice.auto_use_go_entries.empty())
        return false;

    float self_x = 0.f, self_y = 0.f, self_z = 0.f;
    s.position(self_x, self_y, self_z);
    uint8 my_team_id = s.team();
    // Candidate list: nearest GO per advertised TYPE, plus any GO matching
    // the per-entry allow-list (audit B26: AV/IoC/SotA banners are generic
    // BUTTON/GOOBER types that can't be type-matched safely — they're
    // enumerated by entry instead).
    std::vector<BotSnapshot::NearbyObject const*> use_candidates;
    for (uint32_t go_type : bg_advice.auto_use_go_types)
        if (BotSnapshot::NearbyObject const* tgo =
                s.nearest_object_of_type(uint8(go_type)))
            use_candidates.push_back(tgo);
    if (!bg_advice.auto_use_go_entries.empty())
        for (auto const& ego : s.raw().world_objects.nearby_objects)
            for (uint32_t allow : bg_advice.auto_use_go_entries)
                if (ego.entry == allow)
                { use_candidates.push_back(&ego); break; }
    for (BotSnapshot::NearbyObject const* go : use_candidates)
    {
        if (!go) continue;
        float dx = go->x - self_x;
        float dy = go->y - self_y;
        const float go_dsq = dx * dx + dy * dy;
        // 12y outer threshold closes the dead-zone between the
        // Attacker/Defender arrival radii (10y/15y) and the 5y use radius
        // (BG audit N08/N16).
        if (go_dsq > 144.0f)
            continue;
        // Match the GO to a node by position (3y tolerance). If matched +
        // own-team + not contested → skip (don't re-cap a held node).
        bool skip_held = false;
        for (auto const& n : s.bg_node_states())
        {
            float ndx = n.x - go->x;
            float ndy = n.y - go->y;
            if (ndx * ndx + ndy * ndy < 9.0f)
            {
                if (n.owner_team == my_team_id && !n.is_contested)
                    skip_held = true;
                break;
            }
        }
        if (skip_held) continue;
        // Never target our OWN flag PEDESTAL (NEW_FLAG 36). In CTF you only
        // lift the ENEMY pedestal; your own in-base flag is not a pickup —
        // clicking it is futile and burns the per-GO retry slot. This was the
        // #1 reason bots near their own base emitted `use` with ZERO pickups
        // (2026-06-22 live: USE_EMITTED>0, pickups=0 — the nearest NEW_FLAG was
        // frequently the bot's OWN pedestal). A DROPPED flag (NEW_FLAG_DROP 37)
        // is always allowed: that is how a team RETURNS its own flag after an
        // enemy carrier drops it, or re-grabs the enemy flag a dead carrier
        // dropped. Neutral pedestals (e.g. EotS centre) have no own_flag match
        // and so still pass.
        if (go->go_type == /*GAMEOBJECT_TYPE_NEW_FLAG*/ 36 &&
            (bg_advice.own_flag_x != 0.f || bg_advice.own_flag_y != 0.f))
        {
            const float odx = bg_advice.own_flag_x - go->x;
            const float ody = bg_advice.own_flag_y - go->y;
            if (odx * odx + ody * ody < 100.0f)   // within 10y of OUR pedestal
                continue;
        }
        if (go_dsq > 25.0f) // 5-12y: walk the last meters
        {
            emit.move_to(go->x, go->y, go->z, /*run=*/true);
            ai.set_last_rule_fired("idle:bg_approach_objective_go");
            return true;
        }
        // Capture clicks / flag pickups are rejected while mounted (BG
        // audit N10); no other BG rule dismounts. Dismount first.
        if (s.is_mounted())
        {
            emit.dismount();
            ai.set_last_rule_fired("idle:bg_dismount_for_objective");
            return true;
        }
        // Short per-GO cooldown (3s) — flag mechanics need fast retry on
        // legit failure (carrier dropped flag, re-pickup) but not every
        // snapshot tick. The 5-min default lockout would be too long.
        const uint32 bg_now_ms = GameTime::GetGameTimeMS();
        const uint64 go_low = go->guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::BgUseGo,
                                     go_low, bg_now_ms))
            continue;
        emit.use_game_object(go->guid);
        ai.note_action_retry(BotAI::ActionKind::BgUseGo,
                             go_low, bg_now_ms);
        ai.set_last_rule_fired("idle:bg_use_objective_go");
        return true;
    }
    return false;
}

// Shared BG endgame "push to the boss" mover — see MaintainHelpers.h. Leapfrogs
// the bot through the script's node chain so each leg is a single navmesh-
// routable move_to, instead of one impossible 1400-2300y hop into the AV boss
// room (which silently no-paths and froze the push squad at the 1-tower lead).
bool BgPushThroughNodes(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit,
                        BattlegroundAdvice const& bg_advice,
                        float tx, float ty, float tz, uint32 now_ms)
{
    float bx = 0.f, by = 0.f, bz = 0.f; s.position(bx, by, bz);
    const float gdx = tx - bx, gdy = ty - by;
    const float gd2 = gdx * gdx + gdy * gdy;
    if (gd2 <= 81.0f) return false;   // <9y: on top of it — let caller engage

    // Drive STRAIGHT at the far boss with a plain move_to and let the core mover
    // (PlayerbotAPI::MoveToPosition) do the long-haul: when the goal is past the
    // navmesh partial-path cap it returns an "advancing partial" and walks the
    // REACHABLE navmesh path chunk (around the river / through the bridge), then
    // the next emit re-paths from the advanced pose and extends — proper chunked
    // long-haul over real navmesh. An earlier node-chain/straight-line-chunk
    // version advanced ~940y then PLATEAUED at ~511y from the captain because the
    // central Field-of-Strife has no node within one leg, dropping it into a
    // straight-line walk that wedged on the stream; the core mover follows the
    // mesh continuously and has no such gap. `bg_advice` is unused now but kept
    // in the signature for future per-map routing hints.
    (void)bg_advice;

    emit.move_to(tx, ty, tz, /*run=*/true);
    ai.set_last_rule_fired("idle:bg_push_endgame");
    return true;
}

bool BgBossStagingPoint(uint32 entry, float& sx, float& sy, float& sz)
{
    switch (entry)
    {
        // All four AV bosses sit inside walled garrisons/keeps whose enemy
        // approach dead-ends at the perimeter; each staging point is an interior
        // poly just inside the real entrance — verified by mmap_probe (complete
        // 1-2 poly hop to the boss + reachable from the attackers' approach
        // side). Galvangar's was confirmed LoS-clear by the [avlos] probe;
        // VERIFIED LIVE: Galvangar dies once bots funnel through here.
        case 11947: sx = -535.0f; sy = -172.0f; sz = 58.0f; return true; // Galvangar (Iceblood Garrison)
        case 11946: sx = -1360.0f; sy = -222.0f; sz = 98.0f; return true; // Drek'Thar general (Frostwolf Keep)
        case 11949: sx = -57.0f; sy = -293.0f; sz = 15.0f; return true; // Balinda (Stonehearth Outpost)
        case 11948: sx = 712.0f; sy = -20.0f; sz = 51.0f; return true; // Vanndar general (Dun Baldar)
        default: return false;
    }
}

// Navmesh reachability test for the dungeon tank-advance target selection.
// This is the ROOT constraint the game itself enforces — the tank can only
// advance to (and ultimately pull) a target it can actually WALK to — so the
// advance must consult Detour, not a heuristic. It replaces an earlier
// `|z-delta| > 80y` band-aid that faked reachability with a magic number and
// was wrong: it permanently excluded the Deadmines ship-deck boss, which is
// reachable by walking UP the scaffold/ramp even though it sits ~240y above
// the ground path. With a real path test the behaviour is correct at both
// ends: from the entrance the deck boss is whole-dungeon away so Detour
// returns INCOMPLETE (296y A* cap) -> "not yet reachable", and waypoint/trash
// progression carries the group toward it; from the scaffold base the ramp-up
// path is short and NORMAL -> reachable -> the tank advances/pulls it.
//
// Cost: one Detour query per *candidate* on the OOC advance tick for the TANK
// only (the advance branch is gated on no-close-target + post-kill pacing, and
// we break on the first reachable boss) — nowhere near the per-bot hot path, so
// it can't recreate the fleet pathfinding storm. SEH-guarded like every other
// playerbot Detour call. World-thread only (the advance branch already does
// live-object Cell scans, so it runs there).
static bool DungeonTargetReachable(Player* self, float tx, float ty, float tz)
{
    if (!self) return false;
    PathGenerator pg(self);
    BotMovement::SehSafeCalculatePath(pg, tx, ty, tz);
    PathType const t = pg.GetPathType();
    // Reject anything that is not a complete walkable path: NOPATH, a partial
    // INCOMPLETE (target beyond the A* cap / behind a gap), an air SHORTCUT, or
    // an off-mesh end. Require the NORMAL flag AND that the path actually
    // arrives at the target (guards the rare NORMAL|partial combination).
    if (t & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORTCUT |
             PATHFIND_FARFROMPOLY))
        return false;
    if (!(t & PATHFIND_NORMAL))
        return false;
    G3D::Vector3 const& e = pg.GetActualEndPosition();
    const float dx = e.x - tx, dy = e.y - ty, dz = e.z - tz;
    return (dx * dx + dy * dy + dz * dz) <= 13.0f * 13.0f;
}

// Combined reachability check + navmesh-aware intermediate step extraction.
// Returns true (same criteria as DungeonTargetReachable) AND sets step_out to
// the position at up to maxStep yards along the ACTUAL PATH, not a straight-
// line direction. Using real path waypoints means the step Z follows ramps and
// ledge transitions correctly: e.g. the Deadmines entrance platform (z≈62)
// descends to the mine floor (z≈55) via a ramp the navmesh knows about.
// Straight-line bz_adv ignores that descent and resolves to the wrong floor
// poly → FARFROMPOLY/INCOMPLETE move-blocked every tick.
// ── DB-authored traversal links (playerbot_nav_links) ──────────────────────
// The behavioral alternative to baking off-mesh connections into binary mmap
// tiles: a human-verified "from A you can just MOVE to B" row (jump a real
// geometric split, walk an unmeshed stretch). Consumed ONLY on the stepper's
// reject paths, so on-mesh behavior stays byte-identical when pathing works.
//
// HOP — the bot itself stands at a link mouth while its on-mesh path to the
// target failed: commit the crossing (set_dungeon_cross + direct flag) and
// return the FAR endpoint as an off-mesh step; DungeonHonorCross then drives
// the straight no-pathfind spline ("just move, don't think") until landing.
// Fires only through call sites that handle step_is_offmesh (they own the
// cross-commit contract); a per-(bot,link) cooldown prevents ping-ponging
// across a bidirectional link when the target is unreachable from both sides.
static bool DungeonNavLinkHop(Player* self, G3D::Vector3& step_out, bool* step_is_offmesh,
                               char const* reason = "unspecified")
{
    if (!step_is_offmesh || !Playerbot::Services::Initialized()) return false;
    auto links_tbl = Playerbot::Services::Dungeons().GetNavLinks();
    if (!links_tbl) return false;
    auto it = links_tbl->find(self->GetMapId());
    if (it == links_tbl->end()) return false;
    const float sx = self->GetPositionX(), sy = self->GetPositionY(), sz = self->GetPositionZ();
    const uint32 now_ms = GameTime::GetGameTimeMS();
    for (Playerbot::NavLink const& l : it->second)
    {
        struct End { float x, y, z, ox, oy, oz; };
        End const ends[2] = { { l.ax, l.ay, l.az, l.bx, l.by, l.bz },
                              { l.bx, l.by, l.bz, l.ax, l.ay, l.az } };
        int const n = l.bidirectional ? 2 : 1;
        for (int d = 0; d < n; ++d)
        {
            const float dx = sx - ends[d].x, dy = sy - ends[d].y, dz = sz - ends[d].z;
            if (dx*dx + dy*dy + dz*dz > l.radius * l.radius) continue;
            if (!Playerbot::Services::Dungeons().TryClaimLinkHop(
                    self->GetGUID().GetCounter(), l.id, now_ms))
                continue;
            if (Playerbot::BotAI* ai = Playerbot::Services::Registry().ai(
                    self->GetGUID().GetCounter()))
            {
                ai->set_dungeon_cross(ends[d].ox, ends[d].oy, ends[d].oz, now_ms + 12000);
                ai->set_dungeon_cross_direct(true);
            }
            step_out = G3D::Vector3(ends[d].ox, ends[d].oy, ends[d].oz);
            *step_is_offmesh = true;
            TC_LOG_INFO("playerbot.v2",
                "[nav_link] {} hop link {} reason={} ({:.1f},{:.1f},{:.1f})->({:.1f},{:.1f},{:.1f})",
                self->GetName(), l.id, reason, ends[d].x, ends[d].y, ends[d].z,
                ends[d].ox, ends[d].oy, ends[d].oz);
            return true;
        }
    }
    return false;
}

// WALK — the failed path DEAD-ENDS at a link mouth: the link is the authored
// way onward, so the stepper should NOT reject; walking the path to the mouth
// brings the bot into hop range on a later tick.
static bool DungeonNavLinkMouthNear(uint32 map_id, G3D::Vector3 const& p)
{
    if (!Playerbot::Services::Initialized()) return false;
    auto links_tbl = Playerbot::Services::Dungeons().GetNavLinks();
    if (!links_tbl) return false;
    auto it = links_tbl->find(map_id);
    if (it == links_tbl->end()) return false;
    for (Playerbot::NavLink const& l : it->second)
    {
        const float a2 = (p.x-l.ax)*(p.x-l.ax) + (p.y-l.ay)*(p.y-l.ay) + (p.z-l.az)*(p.z-l.az);
        if (a2 <= l.radius * l.radius) return true;
        if (l.bidirectional)
        {
            const float b2 = (p.x-l.bx)*(p.x-l.bx) + (p.y-l.by)*(p.y-l.by) + (p.z-l.bz)*(p.z-l.bz);
            if (b2 <= l.radius * l.radius) return true;
        }
    }
    return false;
}

// SELF-IN-MOUTH — is the BOT ITSELF (not a path endpoint) inside a link
// mouth right now? Same radius test as DungeonNavLinkMouthNear (in fact just
// that check against the bot's own position), used to disambiguate WHY a
// DungeonNavLinkHop attempt just declined: if self is in a mouth, the only
// remaining decline reason is the per-(bot,link) cooldown (TryClaimLinkHop,
// DungeonScript.cpp) — never "no link here". Callers use that distinction to
// avoid emitting a fighting lip-step while a just-claimed crossing's cooldown
// is still ticking (the pending DungeonHonorCross episode is what actually
// carries the bot across; see DungeonTargetReachableAndStep's NORMAL
// far-end branch).
static bool DungeonNavLinkSelfInMouth(Player* self)
{
    if (!self) return false;
    return DungeonNavLinkMouthNear(self->GetMapId(),
        G3D::Vector3(self->GetPositionX(), self->GetPositionY(), self->GetPositionZ()));
}

static bool DungeonTargetReachableAndStep(Player* self, float tx, float ty, float tz,
                                           float maxStep, G3D::Vector3& step_out,
                                           bool* step_is_offmesh,
                                           bool allow_incomplete_progress)
{
    if (step_is_offmesh) *step_is_offmesh = false;
    if (!self) return false;
    // Respect the per-tick pathfinding budget so dungeon-scan loops (which call
    // this once per trash candidate) can't blow the world-thread time budget.
    // Fails open (HasBudget==true) outside DrainIntents so diagnostics/tools work.
    if (!Playerbot::PathBudget::HasBudget(GameTime::GetGameTimeMS()))
        return false;
    PathGenerator pg(self);
    BotMovement::SehSafeCalculatePath(pg, tx, ty, tz);
    PathType const t = pg.GetPathType();
    // TEMP DIAG (remove after Gap-1 cohesion fixed): for a bot near the Gap-1
    // bridge approach (map 36), log WHICH return path fires + the chosen step, so
    // we can see why a MID-GAP step leaks out and move_to REFUSEs it.
    const bool gapDbg = self->GetMapId() == 36 &&
        self->GetPositionX() > -235.0f && self->GetPositionX() < -195.0f &&
        self->GetPositionY() > -560.0f && self->GetPositionY() < -500.0f;
    auto gapLog = [&](char const* where, float sx, float sy, float sz, bool off)
    {
        if (!gapDbg) return;
        static uint32 s_gms = 0; const uint32 gn = GameTime::GetGameTimeMS();
        if (gn - s_gms < 1000u) return; s_gms = gn;
        TC_LOG_INFO("playerbot.v2",
            "[gap_step] {} ret={} t=0x{:X} step=({:.1f},{:.1f},{:.1f}) off={} "
            "self=({:.1f},{:.1f},{:.1f}) tgt=({:.1f},{:.1f},{:.1f})",
            self->GetName(), where, uint32(t), sx, sy, sz, off,
            self->GetPositionX(), self->GetPositionY(), self->GetPositionZ(), tx, ty, tz);
    };
    // Genuine path failure (no route, off-poly source, raycast shortcut) is ALWAYS
    // rejected. PATHFIND_INCOMPLETE is normally rejected too — but the long-range
    // boss approach (allow_incomplete_progress) ACCEPTS it: a corridor longer than
    // the core 74-poly cap (MAX_PATH_LENGTH) string-pulls to a truncated-but-valid
    // path whose on-mesh end falls short of the boss, yet stepping toward it makes
    // real progress and the next tick re-paths from closer — incremental, human-like
    // approach (exactly how MotionMaster walks long routes), NOT an off-mesh bridge
    // around a gate. Verified live 2026-06-26: the Helix probe was INCOMPLETE at
    // pts=71-73 (right at the cap) yet its end sat 6.5y from the boss.
    if (t & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_FARFROMPOLY))
    {
        // Standing at a DB traversal-link mouth with no on-mesh path to the
        // target: hop (commit the direct crossing). Otherwise reject as before.
        if (DungeonNavLinkHop(self, step_out, step_is_offmesh, "nopath"))
        { gapLog("navlink_hop", step_out.x, step_out.y, step_out.z, true); return true; }
        gapLog("reject_nopath", 0, 0, 0, false);
        return false;
    }
    if (!allow_incomplete_progress)
    {
        if (t & PATHFIND_INCOMPLETE)
        {
            // A steep-drop split legitimately truncates the on-mesh path to
            // PATHFIND_INCOMPLETE (a short/degenerate remainder past the
            // ledge) — exactly the geometry a DB traversal link exists to
            // bridge. Hop before rejecting, same commit contract as the
            // NOPATH branch above; the link facility was previously dead
            // here because INCOMPLETE short-circuited to reject first.
            if (DungeonNavLinkHop(self, step_out, step_is_offmesh, "incomplete"))
            {
                gapLog("navlink_hop_incomplete", step_out.x, step_out.y, step_out.z, true);
                return true;
            }
            gapLog("reject_incomplete", 0, 0, 0, false);
            return false;
        }
        if (!(t & PATHFIND_NORMAL))
        {
            gapLog("reject_notnormal", 0, 0, 0, false);
            return false;
        }
    }
    else if (!(t & (PATHFIND_NORMAL | PATHFIND_INCOMPLETE)))
        return false;
    G3D::Vector3 const& e = pg.GetActualEndPosition();
    {
        const float dex = e.x - tx, dey = e.y - ty, dez = e.z - tz;
        const float end2 = dex*dex + dey*dey + dez*dez;
        if (!allow_incomplete_progress)
        {
            // Strict: the path must reach within 13y of the exact target.
            if (end2 > 13.0f * 13.0f)
            {
                // At a link mouth already -> hop. Path dead-ends AT a link
                // mouth -> the link is the authored way onward: accept the
                // path and walk to the mouth (hop fires when we arrive).
                if (DungeonNavLinkHop(self, step_out, step_is_offmesh, "normal_far"))
                { gapLog("navlink_hop", step_out.x, step_out.y, step_out.z, true); return true; }
                // Hop declined while self stands IN a mouth: the only way
                // DungeonNavLinkHop can decline there is the per-(bot,link)
                // cooldown (TryClaimLinkHop) still pending from a just-
                // claimed crossing — never "no link here" (that already
                // requires being in-radius). Do NOT fall through to the lip
                // step below: DungeonHonorCross (priority -2, runs before
                // any rule reaches this stepper) is already driving that
                // claimed crossing to completion, and a truncated ~2y lip
                // step here would fight its spline and orbit the mouth for
                // the 15s cooldown window (the live WC ledge oscillation).
                // Reject; the caller retries next tick, by which point the
                // cross has either landed (path now reads NORMAL/near) or
                // the cooldown has cleared for a fresh hop attempt.
                if (DungeonNavLinkSelfInMouth(self))
                {
                    gapLog("navlink_cooldown_wait", 0, 0, 0, false);
                    return false;
                }
                if (!DungeonNavLinkMouthNear(self->GetMapId(), e))
                    return false;
            }
        }
        else
        {
            // Progress mode: the truncated path end may be far from the boss, but it
            // MUST be meaningfully closer to the boss than we are now. When a route is
            // genuinely blocked, Detour returns the nearest reachable poly — which sits
            // ~at our own position — so end ≈ self and this test fails, leaving the bot
            // parked instead of grinding into the wall. This net-progress gate is the
            // built-in watchdog that makes the progress step inert at a true disconnect.
            const float sdx = self->GetPositionX() - tx;
            const float sdy = self->GetPositionY() - ty;
            const float sdz = self->GetPositionZ() - tz;
            const float self_d = std::sqrt(sdx*sdx + sdy*sdy + sdz*sdz);
            if (std::sqrt(end2) > self_d - 12.0f)
            {
                // Same link rescue as the strict branch: hop at the mouth, or
                // accept a no-net-progress path that DEAD-ENDS at a link mouth
                // (walking to the mouth IS the progress the gate can't see).
                if (DungeonNavLinkHop(self, step_out, step_is_offmesh, "progress_far"))
                { gapLog("navlink_hop", step_out.x, step_out.y, step_out.z, true); return true; }
                if (!DungeonNavLinkMouthNear(self->GetMapId(), e))
                    return false;
            }
        }
    }
    Movement::PointsArray const& pts = pg.GetPath();
    if (pts.size() < 2) { step_out = e; gapLog("pts<2_end", e.x, e.y, e.z, false); return true; }
    float acc = 0.0f;
    step_out = G3D::Vector3(pts[0].x, pts[0].y, pts[0].z);
    // Authoritative off-mesh landing (a world position from PathGenerator), so a SHORT
    // off-mesh bridge is honored too — the segment-length heuristic below misses spans
    // under 15y (the Deadmines foundry FoeReaper bridge is ~10.5y). Immune to path
    // dedupe/NormalizePath: matched by XY against the path's far vertices in the loop.
    const bool pathOffMesh = pg.PathTraversesOffMesh();
    G3D::Vector3 const offLand = pathOffMesh ? pg.GetFirstOffMeshLanding() : G3D::Vector3();
    for (size_t i = 1; i < pts.size(); ++i)
    {
        const float segx = pts[i].x - pts[i-1].x;
        const float segy = pts[i].y - pts[i-1].y;
        const float segz = pts[i].z - pts[i-1].z;
        const float seg  = std::sqrt(segx*segx + segy*segy + segz*segz);
        if (seg < 0.01f) continue;
        // OFF-MESH JUMP detection, INDEPENDENT of maxStep (2026-06-26). A single
        // smoothed segment much longer than a normal poly-portal hop (~<10y) is an
        // off-mesh connection jump. The old test below only flagged a jump when the
        // CAPPED segment exceeded maxStep (45y) — but the REALIGNED Gap-1 bridge is
        // ~27y, SHORTER than maxStep, so it was traversed within the step budget, the
        // cap landed PAST the jump, step_is_offmesh was never set, the caller never
        // stored the cross commit, and DungeonHonorCross couldn't protect the hop →
        // a follower stalled mid-jump the instant combat emitted a competing move
        // (observed live 2026-06-26, across stuck 1/5). Flag ANY >15y segment as the
        // jump and return its FAR vertex so the caller commits and the honor-check
        // makes the crossing uninterruptible. Benign false positive on a long open
        // corridor: the bot just commits to a valid waypoint, reaches it, and clears.
        // Authoritative off-mesh detection: this segment's FAR vertex IS the path's
        // off-mesh landing (matched on XY; NormalizePath only shifts Z). Catches SHORT
        // bridges (< 15y) the length heuristic misses, so the caller commits the cross
        // and DungeonHonorCross makes the hop uninterruptible (root cause of the
        // ~10.5y FoeReaper-bridge mouth wedge — crossed only by luck before).
        bool segOffMesh = false;
        if (pathOffMesh)
        {
            const float lx = pts[i].x - offLand.x, ly = pts[i].y - offLand.y;
            segOffMesh = (lx*lx + ly*ly) < 4.0f;  // far vertex == off-mesh landing (XY, < 2y)
        }
        // Legacy length heuristic kept so the long (27y) Gap-1 bridge behavior is
        // unchanged: ANY > 15y smoothed segment is an off-mesh jump. Either signal
        // flags the crossing and returns its far vertex.
        constexpr float kOffMeshJumpYards = 15.0f;
        if (seg > kOffMeshJumpYards || segOffMesh)
        {
            if (step_is_offmesh) *step_is_offmesh = true;
            step_out = G3D::Vector3(pts[i].x, pts[i].y, pts[i].z);
            gapLog(segOffMesh ? "offmesh_land" : "jump_farvtx", step_out.x, step_out.y, step_out.z, true);
            return true;
        }
        if (acc + seg >= maxStep)
        {
            // Cap reached mid-segment. INTERPOLATING a point at exactly maxStep
            // can land OFF the navmesh: where the string-pulled path spans two
            // on-mesh waypoints via an OFF-MESH CONNECTION (the Deadmines foundry
            // Gap-1/2/3 bridges), the interpolated mid-point sits in the gap
            // (dstpoly=0), so the subsequent move_to NoPaths and the tank wedges
            // forever — observed 2026-06-25: boss_nav reach=1 stride toward Helix
            // emitted move_to (-259,-514,50.7) dstpoly=0, 273s GoalUnreachable.
            // The smoothed-path WAYPOINTS are always valid nav points (poly edges
            // or off-mesh endpoints), so step to a waypoint, never an interpolated
            // mid-point. TWO-PHASE off-mesh crossing (2026-06-25): a single segment
            // LONGER than the step cap IS an off-mesh jump (normal smoothed segments
            // break at every poly portal, so they're short). Returning the jump's
            // FAR vertex made the MotionMaster MovePoint beeline toward it and, from
            // the SOUTH approach (the mesh edge sits ~25y PAST the entry), walk
            // straight into the void instead of routing BACK to the entry — the
            // Gap-1 crossing dropped bots in the hole ~half the runs. So when the
            // cap lands on an off-mesh segment and we are NOT yet at its NEAR vertex,
            // return the NEAR vertex (the entry): the bot first reaches the entry
            // over plain mesh (reliable), then next tick the off-mesh is its
            // IMMEDIATE segment (i==1) and MovePoint fires the jump WHOLE onto the
            // far vertex (reliable). Already at the entry, or a normal short segment
            // (false positive on a long open corridor is benign — just an extra
            // waypoint stop, never oscillation) → return pts[i] as before.
            // ROOT FIX (2026-06-25, supersedes the two-phase above): ALWAYS return
            // the capped segment's FAR waypoint (pts[i]) as a SINGLE STABLE step.
            // The two-phase scheme returned the NEAR vertex (entry) until within 5y,
            // then the far vertex -- but the bot jitters across that 5y boundary
            // every tick, flip-flopping the emitted move_to goal-key entry<->far.
            // Different goal-keys DEFEAT the off-mesh POINT-spline dedup in
            // API::move_to (which only holds the in-flight spline while the goal-key
            // is STABLE), so MovePoint was re-issued every tick, restarting the
            // PointMovementGenerator spline before the off-mesh hop could fire. Live
            // [gap1] capture 2026-06-25: tank oscillated -494..-497 for minutes,
            // MOVEPOINT re-issued every tick with dst alternating (-490.7)/(-547.5),
            // never crossing. A single stable far-vertex goal lets the dedup hold the
            // spline so the MotionMaster's own PathGenerator routes bot->entry->far
            // and executes the off-mesh hop WHOLE -- the internal route back-steps to
            // the entry on its own (mmap_probe from -495 gives exactly
            // [-495, -490.7(entry), -547.5(far)]). pts[i] is always a valid nav
            // waypoint (poly portal or off-mesh endpoint), so a benign false positive
            // on a long open corridor is just an ordinary waypoint stop.
            if (step_is_offmesh && seg > maxStep)
                *step_is_offmesh = true;
            step_out = G3D::Vector3(pts[i].x, pts[i].y, pts[i].z);
            gapLog("capped_vtx", step_out.x, step_out.y, step_out.z, step_is_offmesh && *step_is_offmesh);
            return true;
        }
        acc += seg;
        step_out = pts[i];
    }
    step_out = e;
    gapLog("loop_end_e", e.x, e.y, e.z, false);
    return true;
}

// ── Tank detour verdict (2026-07-02) ────────────────────────────────────────
// The SFK courtyard wedge: combat locks onto an enemy whose walk path for
// the TANK is valid but enormous (139.7yd for a 20yd beeline, navmesh
// connected — editor-probe verified). PathGenerator length vs beeline gives
// a cheap "will the tank actually get there" verdict BEFORE the group
// commits. Read-only helper; policy lives at the call sites.
struct DungeonDetourVerdict
{
    bool  computed = false;
    bool  complete = false;
    float beeline  = 0.f;
    float path_len = 0.f;
    float ratio    = 0.f;
};

static DungeonDetourVerdict DungeonTankDetour(Player* tank,
                                              float tx, float ty, float tz)
{
    DungeonDetourVerdict v;
    if (!tank || !tank->IsInWorld())
        return v;
    // Same guard the wedge watchdog uses (and this file's own
    // DungeonTargetReachableAndStep just above). Honest scope (final-review
    // fix, 2026-07-03): PathBudget's window is opened/closed only around
    // DrainIntents on the WORLD thread (BotIntentExecutor.cpp); HasBudget
    // FAILS OPEN (always true, no bound) when that window is not active —
    // which is the case for the AI worker threads these dungeon rules
    // actually run on. So this only bounds aggregate probe cost within a
    // DrainIntents world-thread tick; it does NOT cap probe frequency on a
    // worker tick, and callers here still need their own cadence/prefilter.
    if (!Playerbot::PathBudget::HasBudget(GameTime::GetGameTimeMS()))
        return v;

    float sx = tank->GetPositionX(), sy = tank->GetPositionY(), sz = tank->GetPositionZ();
    float const dx = tx - sx, dy = ty - sy, dz = tz - sz;
    v.beeline = std::sqrt(dx * dx + dy * dy + dz * dz);

    PathGenerator pg(tank);
    BotMovement::SehSafeCalculatePath(pg, tx, ty, tz);
    PathType const pt = pg.GetPathType();
    v.computed = true;
    // "complete" mirrors DungeonTargetReachable's strictness: a NORMAL path
    // that actually ends at the target (13y tolerance, same constant).
    if (!(pt & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORTCUT | PATHFIND_FARFROMPOLY)))
    {
        G3D::Vector3 const end = pg.GetActualEndPosition();
        float const ex = end.x - tx, ey = end.y - ty, ez = end.z - tz;
        v.complete = (ex * ex + ey * ey + ez * ez) <= 13.0f * 13.0f;
    }
    v.path_len = pg.GetPathLength();
    v.ratio    = v.path_len / std::max(v.beeline, 1.0f);
    return v;
}

static bool DungeonDetourExcessive(DungeonDetourVerdict const& v)
{
    auto const& cfg = Services::Config();
    if (!v.computed)
        return false;              // unknown => never block (fail-open)
    if (!v.complete)
        return true;               // tank literally cannot arrive
    return v.ratio    > cfg.pull_gate_max_ratio() &&
           v.path_len - v.beeline > cfg.pull_gate_min_extra_yards();
}

// Scalar-out wrapper (2026-07-02, Task 3) — see the forward declaration
// above DungeonTargetReachableAndStep for why this exists (callers ahead of
// DungeonDetourVerdict's definition need the verdict fields without holding
// the struct by value). Thin passthrough to DungeonTankDetour +
// DungeonDetourExcessive; no independent logic.
static bool DungeonTankDetourExcessiveVerbose(Player* tank, float tx, float ty, float tz,
                                              float& out_beeline, float& out_path_len,
                                              float& out_ratio, bool& out_complete)
{
    DungeonDetourVerdict const v = DungeonTankDetour(tank, tx, ty, tz);
    out_beeline  = v.beeline;
    out_path_len = v.path_len;
    out_ratio    = v.ratio;
    out_complete = v.complete;
    return DungeonDetourExcessive(v);
}

// ── Off-mesh recovery for a stranded dungeon bot ──────────────────────────────
// A bot that lands OFF the navmesh in a bridge gap (Deadmines Gap-1: the z51 hole
// at ~(-213,-537) over genuine void, srcpoly=0) can path nowhere — every move_to
// NoPaths, terrain_walk refuses (no surface), and grouped dungeon bots are exempt
// from the stuck-rescue ladder, so it strands forever and stalls the group-ready
// advance gate (observed live 2026-06-25: the healer hung in the Gap-1 hole while
// the other four crossed, holding the tank from reaching Helix). Nudge it onto its
// OWN nearest navmesh poly within a tight box: hxy 16y reaches the far-ledge
// landing (~10y away); hz 5y + an 18y stratum check reject the z19 foundry floor
// 32y below the z51 ledge. The emitted move_to is a MovePoint spline, which
// carries the bot across the few yards of void to the solid poly even though no
// ground spans the gap (a spline does not fall mid-flight) — the next tick then
// re-paths from real mesh. No-op for any on-mesh bot (the FARFROMPOLY gate).
static bool DungeonNudgeOntoMesh(Player* self, BotIntentEmitter& emit, BotAI& ai)
{
    if (!self) return false;
    if (!Playerbot::PathBudget::HasBudget(GameTime::GetGameTimeMS()))
        return false;
    Position on_mesh;
    if (!BotMovement::NearestNavPoint(self, self->GetPositionX(), self->GetPositionY(),
                                      self->GetPositionZ(), /*hxy*/ 16.0f, /*hz*/ 5.0f,
                                      on_mesh))
        return false;   // no poly within reach
    if (std::fabs(on_mesh.GetPositionZ() - self->GetPositionZ()) >= 18.0f)
        return false;   // reject a cross-stratum snap (z51 ledge vs z19 floor)
    // Act only when we are genuinely OFF-mesh — a path from our pose to the snapped
    // poly resolves FARFROMPOLY_START only when no poly underlies us right now.
    {
        PathGenerator pg(self);
        BotMovement::SehSafeCalculatePath(pg, on_mesh.GetPositionX(),
                                          on_mesh.GetPositionY(), on_mesh.GetPositionZ());
        if (!(pg.GetPathType() & PATHFIND_FARFROMPOLY))
            return false;   // on-mesh — nothing to recover
    }
    emit.move_to(on_mesh.GetPositionX(), on_mesh.GetPositionY(),
                 on_mesh.GetPositionZ(), /*run=*/true);
    ai.set_last_rule_fired("idle:dungeon_offmesh_recover");
    return true;
}

// Stranded-follower recovery (map-agnostic). A dungeon follower can come to rest
// where EVERY walkable path back to the tank NoPaths: it chased a mob onto a
// navmesh-disconnected perch (the Deadmines FoeReaper-descent z59 ledge — task
// #13), dropped into a void pocket, or otherwise wedged off the group's corridor.
// From there the cohesion regroup's move_to(tank) is REFUSED every tick, so the
// bot neither walks back nor relocates: it holds in place while the group's
// cohesion / harbor-stage gate waits on it indefinitely (observed live
// 2026-06-27: Dungrogue parked at (-136,-401,z59) — 250y and 38y ABOVE the group
// — while 4/5 killed Glubtok/Helix/FoeReaper, then stalled 127s at the harbor
// descent waiting for it; the run could neither advance nor recover). Whether the
// idle dispatch would HOLD or REGROUP the bot, the outcome is identical — it
// cannot move — so this runs AHEAD of both, early in the dispatcher.
//
// Hard-gated so it is a genuine strand recovery, never a content skip:
//   (a) a real FOLLOWER (non-tank), alive, out of combat, not casting, grouped;
//   (b) genuinely far from a living same-map tank (> kStrandFar, out of cohesion
//       AND beyond heal range);
//   (c) NO net progress toward the tank for the whole kStrandStuckMs window — a
//       follower that is simply WALKING a long corridor keeps closing >=3y and
//       resets the clock, so this never fires on a bot that can self-rejoin;
//   (d) a freshly computed path to the tank is CONFIRMED unreachable
//       (NoPath / FarFromPoly / Incomplete) — the decisive gate: a far-but-
//       reachable follower resolves a real path and is left to walk.
// Only when ALL hold do we relocate onto the tank's navmesh poly — the same
// near_teleport primitive/semantics as the off-mesh cross-relocate, finishing a
// rejoin the bot can't make on foot because the terrain under it is disconnected.
bool DungeonRecoverStrandedFollower(BotSnapshotView const& s, BotAI& ai,
                                    GroupSnapshotView const& g,
                                    BotIntentEmitter& emit, uint32 now_ms)
{
    // A follower whose ONLY "attackers" are the untargetable Vanessa Lightning
    // Stalkers (49521) — no fightable attacker AND no real victim — is in pure
    // FALSE combat: it can neither DPS them off (untargetable) nor walk them off
    // (stationary triggers), and the server holds it InCombat indefinitely. That
    // stalker pin (a) trips a plain s.in_combat() gate and (b) the NoPath Gap-1
    // descent blocks its regroup, so a DPS strands at the Gap-1 vertex in stalker
    // combat while the tank+healer push the harbor floor and die 2-v-many (observed
    // live 2026-06-27: tank+healer released corpses at d2rip 233/z15 while Dungmage
    // sat at the (-213,-520,z53) vertex "fought" by 8 stalkers). Treat stalker-only
    // combat as NOT real combat so the recovery can relocate the pinned follower to
    // the tank; any fightable attacker or a real victim means a genuine fight and is
    // left alone (fightable_attackers_count() is the stalker-free count).
    // The real-combat / casting gate (using the stalker-free fightable count
    // described above) is DEFERRED to after the frozen clock below: a member
    // stranded FAR BEHIND and frozen in a fight with trash the group already
    // advanced past must still be able to abandon it and rejoin, or one ranged
    // DPS that engages an entrance pack holds the whole group hostage on the
    // cohesion gate forever (the Dungmage entrance-strand, every run 2026-06-27).
    if (!s.is_alive() || !g.exists()) { ai.frozen_reset(); ai.regroup_stuck_reset(); return false; }

    // ── Rally = largest-cluster MEDOID (not the bare tank) ──────────────────
    // Anchoring cohesion on the tank's CURRENT position back-drags the whole
    // group when the tank dies at the harbor and respawns alive at the FAR
    // entrance graveyard (this instance instant-rezzes at the GY — no corpse
    // run): the old rally (tank-if-alive) teleported every balled member
    // BACKWARD onto the respawned tank, erasing the harbor push (observed live
    // 2026-06-27: group cohered at d2rip 233, tank died, strand-recovery hauled
    // mage/hunter/healer from z19 back up to z57). Instead rally on the MEDOID of
    // the largest tight cluster of living members — the group's MAIN BODY — so an
    // OUTLIER (a respawned-back tank, or a DPS that bolted ahead) is pulled TO the
    // body, and the cohered body is never dragged to an outlier. Self relocates
    // only when it is NOT in the largest cluster (strictly fewer neighbours than
    // the medoid), which also makes the rule oscillation-free on an even split.
    auto const* mems = g.members();
    if (!mems) { ai.frozen_reset(); ai.regroup_stuck_reset(); return false; }
    constexpr float kClusterR = 45.0f;
    float sx, sy, sz; s.position(sx, sy, sz);
    struct RP { float x, y, z; int neigh; bool is_tank; };
    RP pts[16];
    int n = 0, self_idx = -1;
    for (auto const& m : *mems)
    {
        if (!m.online || !m.is_alive || m.map_id != s.map_id()) continue;
        if (n >= 16) break;
        const bool mt = (m.role == Role::Tank);
        if (m.guid == s.guid()) { self_idx = n; pts[n] = {sx, sy, sz, 0, mt}; }
        else                    { pts[n] = {m.x, m.y, m.z, 0, mt}; }
        ++n;
    }
    if (self_idx < 0 || n < 2) { ai.frozen_reset(); ai.regroup_stuck_reset(); return false; }
    float cx = 0.f, cy = 0.f, cz = 0.f;
    for (int i = 0; i < n; ++i) { cx += pts[i].x; cy += pts[i].y; cz += pts[i].z; }
    cx /= n; cy /= n; cz /= n;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if (j == i) continue;
            const float ax = pts[j].x - pts[i].x, ay = pts[j].y - pts[i].y,
                        az = pts[j].z - pts[i].z;
            if (ax * ax + ay * ay + az * az <= kClusterR * kClusterR) pts[i].neigh++;
        }
    int medoid = 0;
    float medoid_c2 = std::numeric_limits<float>::max();
    for (int i = 0; i < n; ++i)
    {
        const float dcx = pts[i].x - cx, dcy = pts[i].y - cy, dcz = pts[i].z - cz;
        const float c2 = dcx * dcx + dcy * dcy + dcz * dcz;
        if (pts[i].neigh > pts[medoid].neigh ||
            (pts[i].neigh == pts[medoid].neigh && c2 < medoid_c2))
        { medoid = i; medoid_c2 = c2; }
    }
    // Rally = the TANK when it is alive and part of the main body (neigh >=
    // medoid) — followers must rejoin the LEADING tank FORWARD. The entrance
    // deadlock (2026-06-27): the bulk sat on the z62 spawn ledge NoPath to the
    // tank (descended to z54), and because the readiness gate is TANK-relative,
    // rallying on the medoid/bulk (which the bulk is already in) just held the
    // whole group on group_not_ready forever. Fall back to the medoid ONLY when
    // the tank is a lone BACK-outlier (respawned at the far GY: neigh < medoid),
    // where pulling everyone onto it WOULD drag the group backward — the case the
    // medoid was introduced for. When self IS the tank, no other tank matches, so
    // rally stays the medoid and a back-fallen tank is itself pulled forward.
    int rally = medoid;
    for (int i = 0; i < n; ++i)
        if (pts[i].is_tank && i != self_idx && pts[i].neigh >= pts[medoid].neigh)
        { rally = i; break; }

    // ── Tight-engagement zone: rally on the TANK, not the medoid ───────────
    // In the tight-engage zone (dangerous final approach, see DungeonAdvice::
    // tight_engage_below_z) the tank-advance readiness gate is TANK-relative and
    // tight (healer ≤18y of the TANK). The medoid rally below is too loose for it:
    // a central member (a DPS parked between the floor body and a trailing healer)
    // becomes the densest medoid, the tank fails the neigh-promotion above, and a
    // healer 39y from that medoid but 46y from the TANK is judged "cohered" by the
    // 40y kStrandFar — so the clocks reset every tick and the strand never fires,
    // while the tank holds FOREVER because the healer is outside its tight gate
    // (the harbor dead-band, observed live 2026-06-29: rogue medoid at z13.5, healer
    // frozen 46y from tank / 39y from rogue, run wedged at 3/6). When self is in the
    // tight zone, rally on the living tank IN THE MAIN BODY and tighten the cohesion
    // cutoff to match the advance gate, so a healer beyond the tight heal-leash is
    // relocated onto the tank instead of parked in the dead-band. The `neigh >= 1`
    // guard preserves the medoid's back-drag protection: a tank that DIED at the
    // harbor and instant-rezzed alone at the far entrance graveyard (this instance
    // has no corpse run) is a lone outlier (neigh == 0) and must NOT become the rally
    // — otherwise it hauls the whole cohered body BACKWARD up to the GY (observed live
    // 2026-06-29: tank died at z16, respawned z57, an unconditional tank-rally relocated
    // all four survivors z20→z63). A lone respawned tank instead relocates DOWN to the
    // body on its own next tick (rally = medoid = the body); only a tank standing WITH
    // part of the group (neigh ≥ 1) anchors the tight rally.
    bool tight_zone = false;
    {
        DungeonAdvice const dav = Services::Dungeons().GetAdvice(s);
        if (dav.tight_engage_below_z > 0.0f && sz < dav.tight_engage_below_z)
        {
            for (int i = 0; i < n; ++i)
                if (pts[i].is_tank && i != self_idx && pts[i].neigh >= 1)
                { rally = i; tight_zone = true; break; }
        }
    }

    const float mx = pts[rally].x, my = pts[rally].y, mz = pts[rally].z;
    const float dx = mx - sx, dy = my - sy, dz = mz - sz;
    const float d2 = dx * dx + dy * dy + dz * dz;
    // Match the tank-advance readiness gate's heal-range threshold (40y): a
    // member just OUTSIDE it that is frozen / NoPath (can't self-close) would
    // otherwise hold the whole group on group_not_ready forever (the healer stuck
    // at 43y on the spawn ledge). Oscillation is prevented by the NoPath + frozen
    // gates below and the single tank rally point — NOT by a cluster-membership
    // abstain, which dead-locked a whole bulk stranded together on a disconnected
    // ledge (every member "in the cluster" so none relocated). In the tight zone the
    // cutoff drops to 22y (advance gate healer ≤18y + margin) so the leash-far healer
    // is recovered rather than parked in the medoid dead-band.
    const float kStrandFar = tight_zone ? 22.0f : 40.0f; // out of cohesion AND heal range
    if (d2 <= kStrandFar * kStrandFar)
    { ai.frozen_reset(); ai.regroup_stuck_reset(); return false; }    // cohered / recoverable on foot

    const float dist = std::sqrt(d2);
    constexpr uint32 kStrandStuckMs     = 8000;    // confirmed-unreachable dwell
    constexpr uint32 kStrandHardStuckMs = 18000;   // path-able-but-frozen dwell
    // A member is STUCK if it makes no progress by EITHER measure — relocate on
    // whichever trips first (both clocks are updated every tick):
    //  • frozen_ms  — own position has not moved >3y. Catches a member frozen in
    //    place even when the rally is itself moving (a moving medoid masked this:
    //    the mage frozen at the entrance the whole descent, d2rip 450 vs 342).
    //  • regroup_stuck_ms — has not gotten >3y CLOSER to the (now stable, tank)
    //    rally. Catches a member that is MOVING but not net-progressing toward the
    //    group — oscillating in a dead zone (the healer bouncing d2rip 408↔449 at
    //    the entrance while the group held at 310, never relocated because it kept
    //    moving so the frozen clock kept resetting, 2026-06-27).
    // A member genuinely walking IN closes the gap (regroup_stuck resets) and
    // moves (frozen resets), so neither trips — a real corridor walk is never cut.
    const uint32 fz_ms  = ai.frozen_ms(sx, sy, sz, now_ms);
    const uint32 rs_ms  = ai.regroup_stuck_ms(dist, now_ms);
    const uint32 stuck_ms = fz_ms > rs_ms ? fz_ms : rs_ms;
    // Leave a NORMALLY fighting/casting bot alone — UNLESS it is a far-behind
    // straggler frozen in place for a sustained window (clearing trash the group
    // already passed while the group is HELD for it). Then the relocate below
    // abandons that behind-fight and rejoins. Stalker-only false-combat is NOT
    // real combat here (fightable_attackers_count is stalker-free), so a pure
    // stalker pin always recovers; only a GENUINE fight gates — and even that
    // yields once the bot is >80y back and frozen past kBehindFrozenMs.
    const bool real_combat = s.in_combat() &&
        (s.fightable_attackers_count() > 0 || !s.victim().IsEmpty());
    constexpr uint32 kBehindFrozenMs = 20000;
    const bool behind_straggler = stuck_ms > kBehindFrozenMs && d2 > 80.0f * 80.0f;
    // TEMP [strand] diag (2026-06-30) — pin WHY a Z-stranded straggler (WC z-104 vs
    // group z-54; SFK; RFC last-boss split) never relocates. Read-only. Logs the gate
    // chain so we see which return below it hits (real_combat blip / clock not matured /
    // reachable-but-slow). Throttled per-bot; any dungeon, only when far-stranded.
    if (s.is_in_dungeon() && d2 > kStrandFar * kStrandFar && ai.tank_diag_due(now_ms))
        TC_LOG_INFO("playerbot.v2",
            "[strand] {} role={} dist={:.0f} fz={} rs={} stuck={} real_combat={} "
            "behind={} casting={} budget={} rally_dz={:.0f} tight={}",
            s.name(), int(s.my_role()), dist, fz_ms, rs_ms, stuck_ms,
            real_combat ? 1 : 0, behind_straggler ? 1 : 0, s.is_casting() ? 1 : 0,
            Playerbot::PathBudget::HasBudget(now_ms) ? 1 : 0, mz - sz, tight_zone ? 1 : 0);
    if ((s.is_casting() || real_combat) && !behind_straggler) return false;
    if (stuck_ms <= kStrandStuckMs) return false;
    if (!Playerbot::PathBudget::HasBudget(now_ms)) return false;

    Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
    if (!self) return false;

    // Path verdict to the rally point. A CONFIRMED-unreachable verdict (NoPath /
    // FarFromPoly / Incomplete = the disconnected perch / void / over-cap route)
    // relocates at the fast 8s dwell. A NORMAL ("reachable") path normally means
    // leave it to walk — UNLESS the member has shown zero net progress for the much
    // longer hard-stuck dwell, in which case the "reachable" route is one the rejoin
    // logic demonstrably cannot traverse (the entrance-GY freeze: stuck 25 min on a
    // poly every step NoPaths from). The best-distance clock resets on any >=3y
    // close, so a follower actually walking in never trips either tier.
    bool unreachable = false;
    {
        PathGenerator pg(self);
        BotMovement::SehSafeCalculatePath(pg, mx, my, mz);
        PathType const pt = pg.GetPathType();
        unreachable = (pt & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY | PATHFIND_INCOMPLETE)) != 0;
    }
    if (s.is_in_dungeon() && ai.tank_diag_due(now_ms))
        TC_LOG_INFO("playerbot.v2", "[strand] {} PATH unreachable={} stuck={} (relocate={})",
            s.name(), unreachable ? 1 : 0, stuck_ms,
            (unreachable || stuck_ms > kStrandHardStuckMs) ? 1 : 0);
    if (!unreachable && stuck_ms <= kStrandHardStuckMs)
        return false;   // reachable and not yet long-frozen — keep walking

    // Land on the rally point's navmesh poly. Snap to the nearest nav point AT the
    // rally so we never materialize inside geometry if it stands on a thin ledge;
    // fall back to its raw pose (it is itself standing on valid navmesh).
    float tx = mx, ty = my, tz = mz;
    Position dest;
    if (BotMovement::NearestNavPoint(self, mx, my, mz, 8.0f, 6.0f, dest))
    { tx = dest.GetPositionX(); ty = dest.GetPositionY(); tz = dest.GetPositionZ(); }
    emit.near_teleport_to(tx, ty, tz, s.raw().position.o);
    ai.frozen_reset();
    ai.regroup_stuck_reset();
    ai.reset_regroup_tracking();
    ai.set_last_rule_fired("idle:dungeon_strand_relocate");
    return true;
}

// ---------- Dungeon converge-to-fight ----------
// A non-tank dungeon bot that is NOT in combat while its tank IS in combat with
// a real victim must close to the tank and join the fight. Without this, a DPS
// that lags when the tank over-extends and pulls (the tank striding toward the
// next boss while the group trails ~40y back) gets stuck in a DEAD-BAND: too
// close for the >40y cohesion rejoin / strand-relocate to fire, but too far for
// its own opener to establish before the 25s give-up shields the kill target
// for 5 minutes. The tank is then left to SOLO a mob it cannot finish — a
// shielded, flee-at-15% Defias Envoker (entry 48418, casts Envoker's Shield on
// spawn) — combat never drops, and the group never advances (the 30-min
// Helix-approach deadlock with three idle DPS standing ~40y back, observed live
// 2026-06-27). Walk the idle bot onto the tank (off-mesh aware, sharing the
// rejoin's bridge-cross commit) so the existing assist/opener engages the
// tank's victim. A short dwell ignores a ranged DPS merely between casts (which
// stays in_combat, so it is never yanked into melee anyway). Runs in every
// state right after strand-recovery; strand handles the >40y UNREACHABLE
// teleport case, this handles the reachable pull-in while the tank fights.
bool DungeonConvergeToFight(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const& g,
                            BotIntentEmitter& emit, uint32 now_ms)
{
    if (!ai.dungeon_active()) { ai.combat_join_reset(); return false; }
    auto const* tk = g.tank();
    // Tank must exist, be a LIVE same-map other bot (self-as-tank excluded by
    // the guid check — a tank never converges on itself).
    if (!tk || !tk->online || !tk->is_alive || tk->guid == s.guid() ||
        tk->map_id != s.map_id())
    { ai.combat_join_reset(); return false; }

    // Stuck-out-of-fight: the tank is genuinely FIGHTING (in combat with a real
    // victim), this bot is NOT in combat (a DPS already trading blows needs no
    // help; a ranged DPS casting from range reads as in_combat, so it is never
    // pulled to melee), and it sits beyond engage range of the tank's mob.
    const bool tank_fighting = tk->in_combat && !tk->victim.IsEmpty();
    float sx, sy, sz; s.position(sx, sy, sz);
    const float dx = tk->x - sx, dy = tk->y - sy;
    const float td = std::sqrt(dx * dx + dy * dy);
    constexpr float kJoinRange = 12.0f;     // inside this, the opener/assist reach the mob
    // Exclude a CASTING bot — a healer mid-rez / mid-heal on a corpse or a
    // member behind us must finish its cast, not be hauled off it. A failing
    // opener does not hold a sustained cast (cast → OOR → idle), so it still
    // converges in the gaps; only genuine channels/long casts are protected.
    const bool stuck_out = tank_fighting && !s.in_combat() &&
                           !s.is_casting() && td > kJoinRange;
    const uint32 join_ms = ai.combat_join_stuck_ms(stuck_out, now_ms);
    if (!stuck_out) return false;
    constexpr uint32 kJoinDwellMs = 4000;   // ignore a transient between-cast OOC blip
    if (join_ms < kJoinDwellMs) return false;
    if (!Playerbot::PathBudget::HasBudget(now_ms)) return false;

    // Off-mesh-aware step toward the tank — the SAME stepping the InGroup rejoin
    // uses, so a pull-in across a foundry off-mesh bridge commits and completes
    // whole instead of restarting over the void each tick.
    float rx, ry, rz; bool roff = false;
    if (DungeonStepTowardTank(s.raw().guid, tk->x, tk->y, tk->z, 45.0f, rx, ry, rz, roff))
    {
        if (roff) ai.set_dungeon_cross(rx, ry, rz, now_ms + 12000);
        if (DungeonStepAlreadyInFlight(s, rx, ry, rz))
        {
            DungeonStepHoldDiag(s, "dungeon:converge_to_fight", rx, ry, rz);
            ai.set_last_rule_fired("dungeon:converge_to_fight_hold");
            return true;
        }
        if (DungeonMoveOwnedElsewhere(s, ai, rx, ry, rz, now_ms,
                                      "dungeon:converge_to_fight"))
        {
            ai.set_last_rule_fired("dungeon:converge_to_fight_hold");
            return true;
        }
        // Refused destination: unlike the in-flight/owned-elsewhere holds
        // above, genuinely FALL THROUGH (return false) — this is a
        // standalone function with its own early-return exits (see
        // !stuck_out / join_ms above), so declining here lets the caller
        // (DungeonDispatch) try a lower-priority rule THIS SAME TICK
        // instead of re-hammering the same poisoned destination.
        if (DungeonStepRefused(s, ai, rx, ry, rz, now_ms))
        {
            DungeonStepRefusedDiag(s, "dungeon:converge_to_fight", rx, ry, rz);
            return false;
        }
        if (emit.move_to(rx, ry, rz, /*run=*/true))
            ai.note_move_commit(s.map_id(), rx, ry, rz, now_ms);
    }
    else
    {
        if (DungeonStepAlreadyInFlight(s, tk->x, tk->y, tk->z))
        {
            DungeonStepHoldDiag(s, "dungeon:converge_to_fight", tk->x, tk->y, tk->z);
            ai.set_last_rule_fired("dungeon:converge_to_fight_hold");
            return true;
        }
        if (DungeonMoveOwnedElsewhere(s, ai, tk->x, tk->y, tk->z, now_ms,
                                      "dungeon:converge_to_fight"))
        {
            ai.set_last_rule_fired("dungeon:converge_to_fight_hold");
            return true;
        }
        if (DungeonStepRefused(s, ai, tk->x, tk->y, tk->z, now_ms))
        {
            DungeonStepRefusedDiag(s, "dungeon:converge_to_fight", tk->x, tk->y, tk->z);
            return false;
        }
        if (emit.move_to(tk->x, tk->y, tk->z, /*run=*/true))
            ai.note_move_commit(s.map_id(), tk->x, tk->y, tk->z, now_ms);
    }
    // TEMP [converge_fight] diag — remove after verify.
    TC_LOG_INFO("playerbot.v2",
        "[converge_fight] {} td={:.0f} join_ms={} -> tank=({:.0f},{:.0f})",
        s.name(), td, join_ms, tk->x, tk->y);
    ai.set_last_rule_fired("dungeon:converge_to_fight");
    return true;
}

// ---------- Dungeon dispatcher (REFACTOR_3 pass 16) ----------
// Body extracted verbatim from the inline `if (ai.dungeon_active())`
// block formerly at line 1017. Each inner `return;` became
// `return true;`; falls through to `return false;` when no
// sub-rule fired so the caller can let lower-priority rules try.
// Wrapped as a single registered rule `idle:dungeon_dispatch`
// in `Bot/States/Rules/DungeonBgRules.cpp` so /whyidle sees the
// dispatcher; sub-rule fires still surface via set_last_rule_fired.
bool DungeonDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit)
{
    if (!ai.dungeon_active()) return false;

    const uint32 now_ms = s.published_at_ms();

    // Increment 1m (2026-07-20): PROGRESS-STICKY ownership tick. Refresh
    // this bot's move-commitment progress clock BEFORE any sub-dispatcher
    // below runs — see BotAI::move_commit_note_progress()/move_commit_active()
    // for the full rationale. Must run unconditionally on every idle dungeon
    // tick: the OWNING rule's own steady re-evaluation usually takes the
    // DungeonStepAlreadyInFlight hold branch at its call site (same target,
    // live spline already heading there) and never reaches
    // DungeonMoveOwnedElsewhere/note_move_commit — so without this tick the
    // stall clock would never advance for a genuinely-walking owner and
    // move_commit_active() would see it as stale the moment a contender
    // shows up. Same kill switch as step-hold (one concept: spline
    // protection). The in-combat counterpart lives at the top of the
    // dungeon block in DispatchInCombat (State_InCombat.cpp) — this
    // function and that one are the only two places a dungeon bot's
    // movement gets dispatched.
    if (Services::Config().move_step_hold_enabled())
    {
        float mc_x, mc_y, mc_z;
        s.position(mc_x, mc_y, mc_z);
        ai.move_commit_note_progress(s.map_id(), mc_x, mc_y, mc_z, now_ms);
    }

    // ── Off-mesh crossing commitment ──────────────────────────────────────────
    // While a dungeon bot is mid-crossing an off-mesh bridge, HOLD the fixed
    // far-vertex goal until it lands on the far ledge — now via the shared
    // DungeonHonorCross (also run at the top of the InCombat and InGroup paths) so a
    // crossing started here cannot be interrupted once the bot enters combat. The
    // helper refreshes the commit TTL so a slow combat-contested hop can't expire
    // mid-jump and strand the bot on the off-mesh poly (the recurring Gap-1 stall).
    if (DungeonHonorCross(s, ai, emit, now_ms))
        return true;

    // Stranded-follower recovery — a follower wedged where it can't path back to
    // the tank (disconnected perch / void pocket) is relocated onto the tank's
    // navmesh after a sustained no-progress + confirmed-NoPath window. Runs ahead
    // of the cohesion regroup/hold below (both of which leave such a bot frozen),
    // so the group's cohesion gate stops waiting forever on a member it lost.
    if (DungeonRecoverStrandedFollower(s, ai, g, emit, now_ms))
        return true;

    // Converge-to-fight — a DPS lagging in the 37-40y dead-band (below the
    // rejoin/strand thresholds) while the tank fights a mob it cannot solo is
    // walked into the tank's combat so the assist/opener engages. Runs right
    // after strand-recovery (which handles the >40y unreachable teleport).
    if (DungeonConvergeToFight(s, ai, g, emit, now_ms))
        return true;

    // Consult per-dungeon script for this map. nullptr (no script
    // registered) returns empty advice — generic logic stands.
        // Pre-resolved at the top of dungeon_active so individual
        // sub-rules below don't each pay the registry lookup.
        DungeonAdvice const advice = Services::Initialized()
            ? Services::Dungeons().GetAdvice(s)
            : DungeonAdvice{};

        // Dungeon-complete auto-leave (Phase J of GROUP_DUNGEON_PLAN.md).
        // Final boss DONE → ladder of exit options:
        //   1. teleporter GO at the entrance (cross-map dest, 5y)
        //      — most dungeons spawn one after the final encounter,
        //      saves the hearth cooldown.
        //   2. walk to a visible teleporter (>5y, ≤30y) so step (1)
        //      fires next tick.
        //   3. hearthstone (universal fallback). Gate on can_hearth()
        //      so bots without a stone (or mid-CD) just stay parked
        //      until ready — the loop-requeue rule below picks up
        //      after the bot is back outside the instance.
        // Mode flips to Off only on definitive exit (hearth fired or
        // teleporter used). The walk-to-portal step keeps mode Active
        // so the next tick re-evaluates exit options.
        // Human-aware: the auto-leave ladder was built for ALL-BOT groups
        // (leave + re-queue loop). A HUMAN group member still inside the
        // instance means the run is over when THEY say it is — TC marks
        // the dungeon complete on the FINAL encounter even with optional
        // bosses alive (2026-06-12: Hogger died before Lord Overheat,
        // "complete" fired at 2/3 and the bots tried to hearth out from
        // under the user). While the human stays, bots stay: follow,
        // fight, and keep advancing to whatever the human wants cleared.
        bool human_in_instance = false;
        if (g.exists())
            if (auto const* mems = g.members())
                for (auto const& m : *mems)
                    if (!m.is_bot && m.online && m.map_id == s.map_id())
                    { human_in_instance = true; break; }
        if (s.dungeon_complete() && !s.in_combat() && !s.is_casting() &&
            !human_in_instance)
        {
            // Look for a teleporter GO. NearbyObject pre-resolves
            // teleport_dest_map for GAMEOBJECT_TYPE_SPELLCASTER (22)
            // and TRANSPORT (11/15) — non-zero means the spell+target
            // resolution found a real destination map.
            auto const& objs = s.nearby_objects();
            float bx = 0.f, by = 0.f, bz = 0.f;
            s.position(bx, by, bz);
            BotSnapshot::NearbyObject const* close_portal = nullptr;
            BotSnapshot::NearbyObject const* far_portal   = nullptr;
            float close_d2 = 0.f, far_d2 = 0.f;
            const uint32 here = s.map_id();
            for (auto const& o : objs)
            {
                if (o.teleport_dest_map == kInvalidMapId) continue;
                if (o.teleport_dest_map == here) continue;  // not an exit
                const float dx = o.x - bx, dy = o.y - by;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= 25.0f)  // 5y interact range
                {
                    if (!close_portal || d2 < close_d2)
                    { close_portal = &o; close_d2 = d2; }
                }
                else if (d2 <= 900.0f)  // 30y attract band
                {
                    if (!far_portal || d2 < far_d2)
                    { far_portal = &o; far_d2 = d2; }
                }
            }

            if (close_portal)
            {
                emit.use_game_object(close_portal->guid);
                ai.set_dungeon_run_mode(BotAI::DungeonRunMode::Off);
                ai.set_last_rule_fired("idle:dungeon_complete_use_portal");
                return true;
            }
            if (far_portal)
            {
                // Walk to ~3y of the portal so the next tick's 5y check
                // fires use_game_object. Step on the line bot→portal.
                const float dx = far_portal->x - bx;
                const float dy = far_portal->y - by;
                const float dist = std::sqrt(far_d2);
                const float scale = (dist > 3.0f) ? (dist - 3.0f) / dist : 0.0f;
                emit.move_to(bx + dx * scale, by + dy * scale, far_portal->z, /*run=*/true);
                ai.set_last_rule_fired("idle:dungeon_complete_walk_to_portal");
                return true;
            }
            if (s.can_hearth())
            {
                emit.hearth();
                ai.set_dungeon_run_mode(BotAI::DungeonRunMode::Off);
                ai.set_last_rule_fired("idle:dungeon_complete_hearth");
                return true;
            }
            // No portal in range, no hearth ready. Bot is parked at
            // the end of the dungeon. The loop-requeue rule won't
            // fire (still in instance) — owner can /run stop or wait
            // for hearth CD; nothing useful to do this tick.
        }

        // Dungeon-combat positioning (BUG G-P0a). The full avoidance /
        // mechanic cascade (step-out-of-fire → kite-fixate → soak → spread →
        // stack → melee-behind) is now a single shared implementation called
        // from BOTH here and the top of DispatchInCombat — so it runs in
        // combat (where it matters most) instead of being idle-only dead
        // code. Placed early so avoidance preempts CC / tank-advance just as
        // it preempts the melee gap-close in the combat path.
        if (DungeonCombatPositioning(s, ai, g, emit, advice))
            return true;

        // Autonomous CC (Phase D3 + per-dungeon advice).
        // Two trigger paths: explicit moon-mark (leader-driven), or
        // per-dungeon `cc_priority_entries` (script-driven). The
        // CC rule fires before tank pull so the marked add is
        // controlled BEFORE engage. Skip when bot is target's victim.
        if (g.exists() && s.is_alive() && !s.is_casting() && !s.in_combat())
        {
            // Cohesion gate: pre-emptive CC only helps when the GROUP is here to
            // kill the add. A bot lagging far behind the tank that CCs a LOCAL mob
            // just pins itself on a target nobody is burning — it re-CCs every
            // tick and never regroups, while the tank stalls/retreats waiting for
            // cohesion (live 2026-06-27: the hunter crossed Gap-1 ahead, perma-CC'd
            // a foundry add at the south vertex while the group held 90y NW; the
            // tank backed into a western pocket and the run deadlocked at the
            // FoeReaper descent). Out of cohesion, suppress CC so the bot falls
            // through to idle:dungeon_regroup_follow_tank and rejoins the group.
            bool cc_cohered = true;
            if (GroupMemberSummary const* cctk = g.tank())
                if (cctk->online && cctk->guid != s.guid() &&
                    cctk->map_id == s.map_id())
                {
                    float ccx, ccy, ccz; s.position(ccx, ccy, ccz);
                    const float ex = cctk->x - ccx, ey = cctk->y - ccy, ez = cctk->z - ccz;
                    // 22y MUST match the regroup_follow_tank trigger (>22y, line
                    // ~5340). A looser radius (was 45y) let a CC-capable follower in
                    // the 22-45y band read cc_cohered=true, cast CC and RETURN TRUE
                    // here, preempting the downstream regroup_follow_tank that WOULD
                    // pull it in — so it perma-CCs a local add while the tank holds
                    // the harbor-stage / cohesion gate on it FOREVER (chronic 127s
                    // harbor stall; live 06-27: 3 DPS pinned in auto_cc ~27y from a
                    // held tank at the Gap-1 south vertex, run deadlocked at 3/6).
                    // Matching 22y gives a clean handoff (<=22y CC, >22y regroup) with
                    // no band where neither fires; a normal balled pull is <22y so CC
                    // is unaffected.
                    cc_cohered = (ex * ex + ey * ey + ez * ez) <= 22.0f * 22.0f;
                }
            // FOCUS-KILL ZONE OVERRIDE (script-driven; see
            // DungeonAdvice::tight_engage_below_z). In a tight-engagement zone
            // (the final boss approach — e.g. the Deadmines harbor floor) the
            // pull is SMALL (2-3 casters) and kill-it-fast beats CC. With those
            // casters in cc_priority_entries every DPS endlessly re-sheeped a
            // fresh one each tick and NEVER fell through to assist/focus-kill, so
            // the tank fought the whole pack SOLO and death-ground (live
            // 2026-06-28: 3 DPS at 100% HP in auto_cc while the tank died to
            // 48505 Shadowguard burst). Suppress pre-emptive CC in the zone so
            // the DPS instead ASSIST-engage and focus-kill (high_priority_kill
            // sets the order). Outside the zone (e.g. the foundry gauntlet's 3
            // elite Envokers) CC-spread is still essential and stays on.
            bool focus_kill_zone = false;
            if (advice.tight_engage_below_z > 0.0f)
            {
                float hkx, hky, hkz; s.position(hkx, hky, hkz);
                focus_kill_zone = (hkz < advice.tight_engage_below_z);
            }
            const uint32 cc = (cc_cohered && !focus_kill_zone)
                                  ? ClassCC(s.cls(), s.spec()) : 0;
            if (cc != 0 && s.knows_spell(cc) && s.is_ready(cc))
            {
                ObjectGuid pick;
                ObjectGuid const moon = g.raid_mark(/*moon*/ 4);
                // Prefer moon-marked target if present and visible.
                if (!moon.IsEmpty())
                {
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.guid != moon) continue;
                        if (u.hp <= 0) break;
                        if (u.victim == s.guid()) break;
                        pick = moon;
                        break;
                    }
                }
                // Fall back to per-dungeon CC priority list. SPREAD control
                // across distinct adds: skip one a teammate already CC'd
                // (is_cc_locked) and skip the skull (the focus-KILL target —
                // never sheep what the group is burning). Without this every
                // CC-capable bot sheeped the SAME first add and the others'
                // casts were wasted on the GCD, so only ONE add was ever
                // controlled (live 06-26: the Deadmines foundry gauntlet spawns
                // 3 elite Defias Envokers all repeating bolt 91004; with all CC
                // on one Envoker the other two nuked the ilvl-48 healer from
                // 100% to dead in one cast window -> wipe -> 250y revive grind.
                // Spreading drops the simultaneous bolt count from 3 to ~1 and
                // makes the gauntlet survivable at the squad's gear). Converges
                // across ~1-2 ticks: once a bot's CC lands the add reads
                // cc_locked and the next bot rolls onto a fresh Envoker.
                if (pick.IsEmpty() && !advice.cc_priority_entries.empty())
                {
                    const ObjectGuid skull = g.skull_target();
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.hp <= 0) continue;
                        if (u.victim == s.guid()) continue;
                        if (u.is_cc_locked) continue;
                        if (!skull.IsEmpty() && u.guid == skull) continue;
                        // Mechanical deny-guard (2026-06-27). The group's CC spells
                        // — Polymorph (118), Sap (6770), Freezing Trap (187650) — are
                        // creature-type restricted and return BAD_TARGETS on
                        // CREATURE_TYPE_MECHANICAL (9). Skip Mechanical candidates so
                        // the mage never burns GCDs spamming Polymorph on a Defias
                        // Reaper/Cannon (the 495K-Polymorph-BAD_TARGETS class the A6
                        // guard exists to prevent). Deny-list (not allow-list) so a
                        // valid CC is never silently disabled.
                        if (u.creature_type == 9 &&
                            (cc == 118 || cc == 6770 || cc == 187650)) continue;
                        bool match = false;
                        for (uint32_t e : advice.cc_priority_entries)
                            if (e == u.entry) { match = true; break; }
                        if (!match) continue;
                        pick = u.guid;
                        break;
                    }
                }
                if (!pick.IsEmpty())
                {
                    emit.cast(cc, pick);
                    ai.set_last_rule_fired(moon == pick
                        ? "idle:dungeon_auto_cc_marked"
                        : "idle:dungeon_auto_cc_script");
                    return true;
                }
            }
        }

        // Wipe detection (Phase G). The threshold scales with group size:
        // 3 dead in a 5-man = 60% gone (wipe), but 3 dead in a 25-man raid
        // = 12% (normal mid-pull casualties). Use max(3, 50% of members)
        // so dungeon behavior is unchanged while raids only flag a wipe
        // when half the raid is down.
        const size_t group_n = g.member_count();
        const uint32 wipe_threshold =
            group_n > 5 ? uint32(std::max<size_t>(3, group_n / 2)) : 3u;
        if (g.exists() && s.is_encounter_in_progress() &&
            g.count_dead(s.map_id()) >= wipe_threshold && s.is_alive())
        {
            // Stop attacking, clear victim manually via stop_attack
            // intent. Bot remains in InCombat until threat fades or
            // boss resets, but at least it stops feeding the boss.
            emit.stop_attack();
            ai.set_last_rule_fired("idle:dungeon_wipe_disengage");
            return true;
        }

        // Phase G — regroup-at-entrance after wipe. Same scaled threshold
        // as the disengage rule above.
        if (g.exists() && !s.is_encounter_in_progress() &&
            g.count_dead(s.map_id()) >= wipe_threshold && s.is_alive() &&
            !s.is_casting() && !DungeonRealCombat(s))
        {
            // Use the IN-INSTANCE entrance (the bot's first position in this
            // run). The old read used instance_entrance_* — the PARENT-map
            // ghost-port location — whose map id can never equal the
            // instance map, so this rule was unreachable since Phase G
            // (audit B36); wipe "recovery" was disengage-only.
            auto const& de = s.raw().dungeon_exec;
            const float ex = de.inside_entrance_x, ey = de.inside_entrance_y;
            const float ez = de.inside_entrance_z;
            if (de.inside_entrance_map == s.map_id() && (ex != 0.f || ey != 0.f))
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                float dx = ex - self_x;
                float dy = ey - self_y;
                if (dx * dx + dy * dy > 100.0f)  // 10y arrival radius
                {
                    emit.move_to(ex, ey, ez, /*run=*/true);
                    ai.set_last_rule_fired("idle:dungeon_wipe_regroup_at_entrance");
                    return true;
                }
            }
        }

        // Single-death in-place rez (2026-06-26). A surviving rezzer walks to a
        // fallen groupmate and resurrects it where it lies, instead of the WHOLE
        // group deadlocking. ROOT CAUSE of the run-ending hang: the OOC-rez
        // maintenance rule (idle:ooc_rez, prio 688) is OUTRANKED by this dungeon
        // dispatch (prio 720), so a healer inside an active dungeon run NEVER
        // reaches the maintenance rez — DungeonDispatch consumes the tick first
        // with idle:dungeon_hold. The dead member then waits out the full 90s
        // group-rez backstop (group_can_rez keyed off an alive on-map healer)
        // while the tank-advance "group_not_ready" gate holds everyone for the
        // corpse → the run stalls for 90s per death and never reaches the next
        // boss. Compounding it, MaintainOocRez CASTS WITHOUT CLOSING DISTANCE:
        // a rezzer 78y from the corpse (observed: healer at (-193,-485), hunter
        // corpse at (-172,-410)) emits a rez that fails out-of-range every tick.
        // Here we MOVE into the rez spell's range first, then cast. Sub-wipe
        // only (count_dead < wipe_threshold handled above); rez-capable classes
        // only (ClassOocRez != 0 → priest/druid/shaman/paladin/monk/evoker), so
        // non-rezzers fall through to their normal hold/advance and the tank
        // keeps tanking. dead_member() uses hp==0 corpse semantics, so a member
        // who CHOSE to release and corpse-run (ghost, hp==1) is never targeted.
        if (s.is_alive() && !DungeonRealCombat(s) && !s.is_casting())
        {
            const uint32 rez = ClassOocRez(s.cls());
            if (rez != 0 && s.knows_spell(rez) && s.is_ready(rez))
            {
                if (auto const* dead = g.dead_member(s.map_id()))
                {
                    float sx = 0.f, sy = 0.f, sz = 0.f;
                    s.position(sx, sy, sz);
                    const float dx = dead->x - sx, dy = dead->y - sy, dz = dead->z - sz;
                    const float d2 = dx * dx + dy * dy + dz * dz;
                    // < 40y spell range, margin for z/path slop. Beyond it we walk
                    // to the corpse; within it we cast.
                    constexpr float kRezCastRange = 28.0f;
                    // Forward-only recovery cap: never trek to a FAR corpse. A member
                    // that died and respawned alive at the entrance graveyard (this
                    // instance instant-rezzes on release) is ~150-440y back; walking
                    // the rezzer there drags the whole body backward and fragments the
                    // group (observed live 2026-06-27: healer go_rez + tank
                    // escort_fallen hauled the body to a gauntlet corpse while the
                    // rogue held the harbor approach alone). Beyond kFetchMaxY abstain:
                    // the dead member releases and DungeonRecoverStrandedFollower
                    // teleports it FORWARD onto the body. Same threshold as the
                    // escort_fallen cap and the State_Dead group_can_rez gate.
                    constexpr float kFetchMaxY = 80.0f;
                    if (d2 > kFetchMaxY * kFetchMaxY)
                    {
                        // far — leave it to release + forward-relocate; do not drag back
                    }
                    else if (d2 > kRezCastRange * kRezCastRange)
                    {
                        // Guarded: a corpse on disconnected/off-mesh geometry
                        // refuses the spline (move_to == false) — fall through to
                        // the normal hold so we don't wedge re-trying; the dead
                        // bot's group-rez backstop then force-releases it to
                        // release + forward-relocate instead.
                        if (emit.move_to(dead->x, dead->y, dead->z, /*run=*/true))
                        {
                            ai.set_last_rule_fired("idle:dungeon_go_rez");
                            return true;
                        }
                    }
                    else
                    {
                        // In range. Cross-caster dedup so multiple rezzers don't
                        // all start an 8-10s OOC rez on the same corpse (only the
                        // first lands). Mirrors MaintainOocRez's known-rez check.
                        bool another = false;
                        if (auto const* mems = g.members())
                            for (auto const& m : *mems)
                            {
                                if (m.guid == s.guid()) continue;
                                if (!m.is_casting) continue;
                                if (m.casting_target != dead->guid) continue;
                                switch (m.casting_spell)
                                {
                                    case 2006: case 50769: case 2008:
                                    case 7328: case 115178: case 361227:
                                        another = true; break;
                                    default: break;
                                }
                                if (another) break;
                            }
                        if (!another)
                        {
                            emit.cast(rez, dead->guid);
                            ai.set_last_rule_fired("idle:dungeon_go_rez");
                            return true;
                        }
                    }
                }
            }
        }

        // Rescue an ATTACKED, SEPARATED healer (2026-06-28). Realistic group
        // behavior: the healer is the squad's highest-value member, so when it is
        // caught in a fight while separated from the tank, the group collapses BACK
        // onto it to clear its attackers — it does NOT keep advancing and let the
        // healer die behind. ROOT FIX for the Deadmines harbor death-spiral: on the
        // foundry->harbor descent the healer gets caught by foundry/Gap-1-bridge
        // Defias (z51-54) and can't follow the tank DOWN; the tank then pulls the
        // harbor UNHEALED, dies, the group force-releases to the entrance GY 250y
        // back, and the cycle repeats (observed live 2026-06-28: tank 67 deaths,
        // healer stuck InCombat at z54 while the tank died at the harbor z19). This
        // is the LIVE-member, PROACTIVE complement of escort_fallen below (which only
        // fires once a member is already DEAD) and reaches farther than
        // tank_guard_group (which needs the attacker inside the tank's 40y). Tank-led
        // (DPS cohere via regroup_follow_tank / dps_assist); the tank turns back to
        // the healer INSTEAD of advancing, so it never solo-pulls the harbor while the
        // healer is in trouble. Bounded: only a same-area separation (kRescueMinY..
        // kRescueMaxY) — closer is the peel/cohesion envelope, farther is a GY-revive
        // handled by escort/forward-relocate, so this never chases a body to the GY.
        // Converges: once the descent Defias are cleared the healer balls up and the
        // advance resumes. Gated to OOC tank so it intercepts the advance, never
        // abandons the tank's own active fight.
        if (s.is_alive() && !DungeonRealCombat(s) && !s.is_casting() &&
            !s.is_encounter_in_progress() && g.exists() &&
            ai.effective_role(s) == Role::Tank)
        {
            if (auto const* mems = g.members())
            {
                float bx_r, by_r, bz_r; s.position(bx_r, by_r, bz_r);
                GroupMemberSummary const* hurt_healer = nullptr;
                float hh_d2 = 0.f;
                constexpr float kRescueMinY = 30.0f;   // beyond the peel/cohesion envelope
                constexpr float kRescueMaxY = 100.0f;  // same area, not the far GY
                for (auto const& m : *mems)
                {
                    if (m.guid == s.guid()) continue;
                    if (!m.online || m.map_id != s.map_id() || !m.is_alive) continue;
                    if (m.role != Role::Healer) continue;
                    if (!m.in_combat) continue;        // only a healer in a real fight
                    const float dxr = m.x - bx_r, dyr = m.y - by_r, dzr = m.z - bz_r;
                    const float d2r = dxr * dxr + dyr * dyr + dzr * dzr;
                    if (d2r < kRescueMinY * kRescueMinY) continue;  // close — tank_guard/peel handles
                    if (d2r > kRescueMaxY * kRescueMaxY) continue;  // too far — escort/relocate handles
                    if (!hurt_healer || d2r > hh_d2) { hurt_healer = &m; hh_d2 = d2r; }
                }
                if (hurt_healer)
                {
                    // Move to the healer (stop ~25y short via the step helper so we
                    // arrive in peel range and body-block its attackers rather than
                    // piling onto its feet). Off-mesh-aware so a foundry bridge between
                    // us and the healer doesn't strand the tank mid-descent.
                    Player* rself = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
                    G3D::Vector3 rstep;
                    bool r_off = false;
                    if (rself && DungeonTargetReachableAndStep(
                            rself, hurt_healer->x, hurt_healer->y, hurt_healer->z,
                            30.0f, rstep, &r_off))
                    {
                        if (r_off)
                            ai.set_dungeon_cross(rstep.x, rstep.y, rstep.z, now_ms + 12000);
                        emit.move_to(rstep.x, rstep.y, rstep.z, /*run=*/true);
                    }
                    else
                        emit.move_to(hurt_healer->x, hurt_healer->y, hurt_healer->z, /*run=*/true);
                    if (ai.tank_diag_due(now_ms))
                        TC_LOG_INFO("playerbot.v2",
                            "[rescue_healer] {} healer_dist={:.1f} healer_pos=({:.0f},{:.0f},{:.0f})",
                            s.name(), std::sqrt(hh_d2),
                            hurt_healer->x, hurt_healer->y, hurt_healer->z);
                    ai.set_last_rule_fired("idle:dungeon_rescue_healer");
                    return true;
                }
            }
        }

        // Single-death escort regroup (2026-06-26). When a group member is dead
        // or corpse-running (is_alive == false) FAR behind us and we're not in
        // combat, the alive group must go BACK to meet and protect it — a lone
        // member cannot traverse the gauntlet by itself to rejoin. ROOT CAUSE of
        // a run-ending death spiral (observed 2026-06-26): the healer fell behind
        // the tank's advance, was caught alone by gauntlet trash and died, revived
        // at the in-instance graveyard, then corpse-ran back THROUGH that same live
        // trash which killed it again and again — while the rest of the group held
        // forward on the cohesion gate (group_not_ready), waiting for a healer that
        // could never make the trip. With no rez-capable survivor (the dead member
        // IS the only healer), go_rez above can't help; the group must fetch it.
        // The TANK leads the escort by walking toward the fallen member's live
        // position; the followers cohere to the tank (regroup_follow_tank), so the
        // whole group falls back together, clears the trash on the runner, and
        // advances again as a unit once it rejoins. Tank-led so the squad moves as
        // one; only for members beyond kEscortYards (a member rezzed/recovered
        // close by just rejoins normally); never during a boss encounter.
        //
        // DEAD-TANK generalization (2026-06-26): tank-led alone CANNOT recover a
        // dead TANK — there is no leader, so the group deadlocks holding forward
        // (compute_group_ready stays false on the dead member) until the 90s
        // watchdog ejects the tank to the far GY (the harbor-fragmentation cycle).
        // When a TANK is dead on our map, let ANY alive member run the escort, so
        // the whole group (incl. the healer) CONVERGES on the tank's corpse — then
        // the OOC go_rez above raises it IN PLACE during the next combat lull,
        // avoiding the 90s wait + the 250y far-GY revive entirely. Tank-led stays
        // the rule when the tank is alive (a dead DPS is handled by the tank
        // leading + go_rez), so normal advancement is unchanged.
        bool tank_dead_on_map = false;
        if (g.exists())
            if (auto const* tmems = g.members())
                for (auto const& m : *tmems)
                    if (m.role == Role::Tank && m.online && !m.is_alive &&
                        m.map_id == s.map_id())
                    { tank_dead_on_map = true; break; }
        if (s.is_alive() && !DungeonRealCombat(s) && !s.is_casting() &&
            !s.is_encounter_in_progress() && g.exists() &&
            (ai.effective_role(s) == Role::Tank || tank_dead_on_map))
        {
            if (auto const* mems = g.members())
            {
                float bx_e, by_e, bz_e; s.position(bx_e, by_e, bz_e);
                GroupMemberSummary const* far_fallen = nullptr;
                float far_d2 = 0.f;
                constexpr float kEscortYards = 60.0f;
                // Upper cap mirrors go_rez / group_can_rez: a fallen member farther
                // than kFetchMaxY has respawned at the far entrance GY (or died deep
                // behind) — fetching it would drag the body backward and fragment the
                // group. Beyond it, leave the member to release (instant-alive here)
                // and forward-relocate onto the body (2026-06-27 forward-only recovery).
                constexpr float kFetchMaxY = 80.0f;
                for (auto const& m : *mems)
                {
                    if (m.guid == s.guid()) continue;
                    if (!m.online || m.map_id != s.map_id()) continue;
                    if (m.is_alive) continue;          // only dead / corpse-running
                    const float dxe = m.x - bx_e, dye = m.y - by_e, dze = m.z - bz_e;
                    const float d2e = dxe * dxe + dye * dye + dze * dze;
                    if (d2e < kEscortYards * kEscortYards) continue;  // close enough
                    if (d2e > kFetchMaxY * kFetchMaxY) continue;      // too far → relocate
                    if (!far_fallen || d2e > far_d2) { far_fallen = &m; far_d2 = d2e; }
                }
                if (far_fallen)
                {
                    // Walk toward the fallen member (stop ~30y short via the step
                    // helper so we don't pile onto its corpse). Off-mesh-aware so a
                    // foundry bridge between us and the runner doesn't strand us.
                    G3D::Vector3 estep;
                    bool e_off = false;
                    Player* eself = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
                    if (eself && DungeonTargetReachableAndStep(
                            eself, far_fallen->x, far_fallen->y, far_fallen->z,
                            30.0f, estep, &e_off))
                    {
                        if (e_off)
                            ai.set_dungeon_cross(estep.x, estep.y, estep.z,
                                                 now_ms + 12000);
                        emit.move_to(estep.x, estep.y, estep.z, /*run=*/true);
                    }
                    else
                        emit.move_to(far_fallen->x, far_fallen->y, far_fallen->z,
                                     /*run=*/true);
                    ai.set_last_rule_fired("idle:dungeon_escort_fallen");
                    return true;
                }
            }
        }

        // Dungeon interrupt — break boss/trash casts. With an active
        // PvE-coordinator plan, interrupts run as a true ROTATION:
        // rank 0 kicks on sight, rank 1 backs up only when the cast is
        // still in flight near its end (rank 0's kick landing flips
        // is_casting off first), ranks 2+ HOLD their cooldown for the
        // NEXT cast, and off-rotation classes (healers with a kick)
        // fire only as a last resort. Without a plan, the legacy
        // per-bot 500ms self-throttle applies — which let every capable
        // kicker burn its interrupt on the same cast.
        if (s.is_alive() && !s.is_casting() && !s.in_combat())
        {
            // Rotation gate shared by the boss and trash paths below.
            // `rem_ms` is the observed cast's remaining time (-1 when
            // unknown — treated as "not near the end", so backups hold).
            // Ranks form layered backups, NOT a hard mutex: rank 0 kicks
            // on sight, rank 1 steps in when the cast survives to its
            // last 800ms, ranks 2+ at the last 550ms, off-rotation
            // (0xFF, e.g. healers with a kick) at the last 450ms —
            // windows above the ~350ms kick-landing time so every layer
            // stays reachable. Deep ranks must stay live — with two mobs
            // casting at once, rank 0 can only cover one of them, and a
            // rotation that tells everyone else "never" would interrupt
            // LESS than the legacy free-for-all. Same windows as the
            // in-combat APL chokepoint (BotSnapshotView::
            // interruptible_caster) so the schedule is consistent.
            auto interrupt_turn = [&](int64 rem_ms) -> bool
            {
                auto const& po = s.raw().pve_order;
                if (!po.active)
                    return true;            // legacy self-throttle only
                if (po.interrupt_rank == 0)    return true;
                if (po.interrupt_rank == 1)    return rem_ms >= 0 && rem_ms < 800;
                if (po.interrupt_rank == 0xFF) return rem_ms >= 0 && rem_ms < 450;
                return rem_ms >= 0 && rem_ms < 550;   // ranks 2+: deep backup
            };

            // Boss casting check first — most impactful interrupt
            // target. Bot's class interrupt resolved via ClassInterrupt
            // table; gate on knows_spell + is_ready.
            if (s.has_visible_boss() &&
                s.current_boss_casting_spell() != 0 &&
                s.current_boss_casting_interruptible())
            {
                const uint32 ispell = ClassInterrupt(s.cls(), s.spec());
                if (ispell != 0 && s.knows_spell(ispell) && s.is_ready(ispell))
                {
                    // 500ms post-emit lockout — gives our own intent
                    // a tick to land and the snapshot to refresh.
                    if (interrupt_turn(
                            int64(s.current_boss_cast_remaining().count())) &&
                        now_ms - ai.last_interrupt_ms() >= 500)
                    {
                        emit.cast(ispell, s.current_boss_guid());
                        ai.note_interrupt(now_ms);
                        ai.set_last_rule_fired("idle:dungeon_interrupt_boss");
                        return true;
                    }
                }
            }
            // Trash interrupt — same mechanism but checks all visible
            // enemies, prioritising mandatory_interrupt_spells from
            // the per-dungeon script when the matched cast is live.
            // If the script doesn't list the cast, fall back to the
            // generic "any interruptible" path.
            for (auto const& u : s.raw().combat.nearby_enemies)
            {
                if (!u.is_casting || !u.is_interruptible) continue;
                if (u.hp <= 0) continue;
                // Mandatory list takes priority — never let these cast
                // even if the bot has only partial CD remaining.
                bool mandatory = false;
                for (uint32_t ms : advice.mandatory_interrupt_spells)
                    if (ms == u.casting_spell_id) { mandatory = true; break; }
                const uint32 ispell = ClassInterrupt(s.cls(), s.spec());
                if (ispell == 0) break;
                if (!s.knows_spell(ispell) || !s.is_ready(ispell)) break;
                // Rotation gate (coordinator-active groups): mandatory
                // casts get rank 0 immediately, deeper ranks as layered
                // late backups — NOT every kicker at once ("better safe
                // than wipe" burned the whole group's interrupts on one
                // cast and left the next three uncovered). `continue`,
                // not break: a holding backup must keep scanning — a
                // SECOND mob's cast may already be near its end while
                // the first mob's cast just started. Legacy groups keep
                // the old behavior: mandatory skips the self-throttle.
                if (s.raw().pve_order.active)
                {
                    if (!interrupt_turn(int64(u.cast_remaining.count())))
                        continue;
                    if (now_ms - ai.last_interrupt_ms() < 500) break;
                }
                else if (!mandatory && now_ms - ai.last_interrupt_ms() < 500)
                    break;
                emit.cast(ispell, u.guid);
                ai.note_interrupt(now_ms);
                ai.set_last_rule_fired(mandatory
                    ? "idle:dungeon_interrupt_mandatory"
                    : "idle:dungeon_interrupt_trash");
                return true;
            }
        }

        // Enemy-buff dispel (M+ Raging 228318, raid enrage buffs,
        // demonic empower buffs). `dispel_enemy_priority_spells` is
        // populated by per-dungeon / per-affix scripts; consumer was
        // missing until 2026-05-22 — populated by 50+ scripts but
        // never read. Class-aware dispatch:
        //   Hunter:  Tranquilizing Shot (19801) — strips Enrage+Magic
        //   Druid:   Soothe (2908) — strips Enrage from a hostile
        //   Mage:    Spellsteal (30449) — steals magic buff
        // Builder samples mob auras into NearbyUnit.affix_buffs against
        // a whitelist; this rule walks the intersection.
        if (s.is_alive() && !s.is_casting() &&
            !advice.dispel_enemy_priority_spells.empty())
        {
            constexpr uint32 TRANQ_SHOT = 19801;
            constexpr uint32 SOOTHE     = 2908;
            constexpr uint32 SPELLSTEAL = 30449;
            uint32 dispel_spell = 0;
            switch (s.cls())
            {
                case CLASS_HUNTER:  dispel_spell = TRANQ_SHOT; break;
                case CLASS_DRUID:   dispel_spell = SOOTHE;     break;
                case CLASS_MAGE:    dispel_spell = SPELLSTEAL; break;
                default: break;
            }
            if (dispel_spell && s.knows_spell(dispel_spell) && s.is_ready(dispel_spell))
            {
                float bx2, by2, bz2; s.position(bx2, by2, bz2);
                NearbyUnit const* enrage_target = nullptr;
                float best_dsq = 40.0f * 40.0f;
                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0) continue;
                    bool carries = false;
                    for (uint32_t want : advice.dispel_enemy_priority_spells)
                    {
                        for (uint32_t have : u.affix_buffs)
                            if (have == want) { carries = true; break; }
                        if (carries) break;
                    }
                    if (!carries) continue;
                    const float dx2 = u.x - bx2;
                    const float dy2 = u.y - by2;
                    const float dz2 = u.z - bz2;
                    const float dsq = dx2*dx2 + dy2*dy2 + dz2*dz2;
                    if (dsq < best_dsq) { best_dsq = dsq; enrage_target = &u; }
                }
                if (enrage_target)
                {
                    emit.cast(dispel_spell, enrage_target->guid);
                    ai.set_last_rule_fired("idle:enrage_dispel");
                    return true;
                }
            }
        }

        // Tank swap on M+ debuff stacks (Necrotic + similar). Off-tank
        // bot in a 2-tank group taunts the active boss when the current
        // tank carries a debuff in advice.tank_swap_on_spells. Fires
        // BEFORE tank_pull_next so a debuffed pull doesn't get re-pulled
        // by the same wrong tank. Single-tank groups skip naturally
        // (no swap candidate).
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            !advice.tank_swap_on_spells.empty() &&
            g.exists())
        {
            auto const* mems = g.members();
            if (mems)
            {
                GroupMemberSummary const* stacked_tank = nullptr;
                for (auto const& m : *mems)
                {
                    if (!m.online || m.hp <= 0) continue;
                    if (m.role != Role::Tank) continue;
                    if (m.guid == s.raw().guid) continue;       // me
                    if (!m.in_combat) continue;                  // only active tank matters
                    if (m.victim.IsEmpty()) continue;            // need a boss to taunt
                    bool needs_swap = false;
                    for (auto const& d : m.debuffs)
                    {
                        for (uint32_t ts : advice.tank_swap_on_spells)
                            if (d.spell_id == ts) { needs_swap = true; break; }
                        if (needs_swap) break;
                    }
                    if (needs_swap) { stacked_tank = &m; break; }
                }
                if (stacked_tank)
                {
                    const uint32 taunt = ClassTaunt(s.cls());
                    if (taunt && s.knows_spell(taunt) && s.is_ready(taunt))
                    {
                        emit.cast(taunt, stacked_tank->victim);
                        ai.set_last_rule_fired("idle:tank_swap_mplus_stacks");
                        return true;
                    }
                }
            }
        }

        // Phase-aware positioning (Tier 4). When any encounter on this
        // map is in EncounterState::SPECIAL — TC's catch-all for sub-
        // phases like Razuvious MC, Heigan eruption dance, Sindragosa
        // air phase — ranged DPS and healers proactively spread from
        // the nearest ally. This is the canonical "phase change" group
        // response: melee may not yet have a positioning cue from the
        // boss's cast, but the visible state-flip is enough signal for
        // ranged to pre-emptively give the boss positioning room.
        // Tank doesn't move (group reforms around them). Melee stays
        // close to current target so they don't lose uptime.
        //
        // Spread distance is shorter than spread_on_self_aura (5y vs
        // 8y) since this is precautionary, not a hard requirement.
        // 30s lockout via last_rule_fired prevents thrash.
        if (s.is_alive() && !s.is_casting() &&
            s.raw().dungeon_exec.any_boss_in_special &&
            ai.effective_role(s) != Role::Tank)
        {
            // Class-based ranged check — melee classes stay put.
            const uint8 cls = s.raw().identity.cls;
            const bool is_melee_class =
                cls == 1  /*Warrior*/  || cls == 4  /*Rogue*/   ||
                cls == 6  /*DK*/       || cls == 12 /*DH*/;
            if (!is_melee_class)
            {
                float bx4 = 0.f, by4 = 0.f, bz4 = 0.f;
                s.position(bx4, by4, bz4);
                NearbyUnit const* closest_ally = nullptr;
                float closest_d2 = 5.0f * 5.0f;
                for (auto const& f : s.raw().combat.nearby_friends)
                {
                    if (f.hp <= 0) continue;
                    if (f.guid == s.raw().guid) continue;
                    const float dx4 = f.x - bx4;
                    const float dy4 = f.y - by4;
                    const float d2 = dx4*dx4 + dy4*dy4;
                    if (d2 < closest_d2) { closest_d2 = d2; closest_ally = &f; }
                }
                if (closest_ally)
                {
                    // Watchdog suppresses if we re-fire this rule too
                    // fast — limits thrash if multiple SPECIAL events
                    // overlap. Distinct rule name vs spread_on_self_aura
                    // so /diag's RuleHist shows phase-driven spread.
                    char const* last = ai.last_rule_fired();
                    if (!last ||
                        std::string_view(last) != "idle:phase_aware_spread")
                    {
                        float dx4 = bx4 - closest_ally->x;
                        float dy4 = by4 - closest_ally->y;
                        float len = std::sqrt(dx4*dx4 + dy4*dy4);
                        if (len < 0.5f) { dx4 = 1.f; dy4 = 0.f; len = 1.f; }
                        const float scale = 7.0f / len;
                        emit.move_to(closest_ally->x + dx4 * scale,
                                     closest_ally->y + dy4 * scale, bz4,
                                     /*run=*/true);
                        ai.set_last_rule_fired("idle:phase_aware_spread");
                        return true;
                    }
                }
            }
        }

        // [BUG G-P0a] spread / stack / soak / kite-fixate bodies moved into
        // the shared DungeonCombatPositioning() helper, called early in this
        // dispatcher (see the call above that replaced the dangerous-aura
        // body). They now run in combat too instead of being idle-only.

        // Tank advance — proactive lead-the-progress for bot tanks in
        // dungeon-run mode. When tank_pull_next has nothing within its
        // 30y pull range, scan the wider nearby_enemies (snapshot covers
        // ~80y) for the closest live hostile and walk toward it. Once the
        // tank closes the gap, tank_pull_next fires on the next tick.
        // This is what makes `;all run` actually move the squad through
        // the dungeon — `dungeon_active` is the explicit owner signal
        // that the tank should take initiative regardless of who leads
        // the group (human or bot). DPS/healer anchor on tank via the
        // existing State_InGroup logic so the squad moves together.
        // Diagnostic: log which gate kills tank_advance when role==Tank.
        // The rule fires only when ALL conditions pass; without this log
        // a bot tank standing idle in a dungeon under `;all run` is a
        // black box. Edge-triggered via ai.last_rule_fired() so we only
        // emit once per state-change.
        // Phase-change callout — fires once on the false→true edge of
        // `any_boss_in_special`. SPECIAL is TC's catch-all for sub-
        // phases (Razuvious MC, Heigan eruption dance, Sindragosa air
        // phase). Real players callout the transition; without this,
        // bots silently react. Per-bot edge state via BotAI::
        // saw_special_phase_; BgCalloutCoordinator kind 11 dedups
        // across the group so only one bot shouts per phase event.
        // Personality-gated.
        if (s.is_in_instance() && s.raw().dungeon_exec.any_boss_in_special &&
            !ai.saw_special_phase())
        {
            ai.set_saw_special_phase(true);
            if (ai.personality().verbosity != Verbosity::Silent &&
                ai.personality().verbosity != Verbosity::Terse)
            {
                // Keyed on (map_id, current_boss_entry) — each boss's
                // phase event gets one callout per minute.
                const uint64 phase_key =
                    (uint64(11) << 32) |
                    (uint64(s.map_id()) << 16) |
                    uint64(s.raw().dungeon_exec.current_boss_entry & 0xFFFFu);
                if (BgCalloutCoordinator::TryClaim(/*kind=*/11, phase_key,
                                                   now_ms,
                                                   /*lockout=*/60u * 1000u))
                {
                    char const* phrases[] = {
                        "phase change!", "phase!", "watch the boss",
                        "phase change", "new phase",
                    };
                    const uint32 sel = uint32(now_ms ^ uint32(ai.bot_id())) %
                                       (sizeof(phrases) / sizeof(phrases[0]));
                    emit.say(phrases[sel]);
                }
            }
            ai.set_last_rule_fired("idle:phase_change_callout");
            // Fall through so other rules still run this tick.
        }
        else if (!s.raw().dungeon_exec.any_boss_in_special && ai.saw_special_phase())
        {
            // Phase ended — reset edge so the NEXT transition fires.
            ai.set_saw_special_phase(false);
        }

        // Group-ready short-circuit for the post-kill cooldown. The 5s
        // window exists so the group can catch up and top off between
        // pulls; if those conditions are already met (positioned,
        // healed, mana'd), the wait is dead time and the tank should
        // advance immediately. Computed once and reused for both the
        // diagnostic gate below and the actual advance fire-condition
        // at the bottom of this block. Owner directive 2026-05-15.
        //
        // Readiness: every same-map living group member (excluding tank
        // itself) is within 15y XY, HP ≥ 80%, mana ≥ 50% (only checked
        // for mana users; max_mana == 0 classes always pass that gate).
        // Solo bots (no group / empty members) trivially count as ready.
        // Readiness, real-tank style (reworked 2026-06-11, user report:
        // "stockade group gets stalled, I had to move forward manually").
        // The old gate required EVERY member within 15y / HP≥80% / mana≥50%
        // — but the human player is a member too, and bots anchor on the
        // TANK: whenever the human lagged 16y behind, the tank waited for
        // the human while the human waited for the tank. Deadlock until the
        // human walked point. A real tank paces on the HEALER (position +
        // mana) and on hurt members topping off; one DPS poking around a
        // side room never freezes the run — followers catch up while the
        // tank walks. Gates now:
        //   * no dead same-map member,
        //   * every member HP ≥ 70%,
        //   * every Healer within 40y AND mana ≥ 50%,
        //   * at least half the OTHER members within 50y (cohesion floor —
        //     the tank still doesn't sprint the instance alone).
        // Harbor-tight cohesion (Deadmines harbor descent, 2026-06-28). The
        // default readiness gate (healer ≤40y, HP ≥70%, half of others ≤50y) is
        // calibrated for the gauntlet's spread pulls — but on the harbor floor /
        // gangplank the Defias packs BURST. A tank that pulls with its healer 40y
        // back and the group at 70% gets bursted dead before a heal lands from
        // that range (observed 2026-06-28: group reaches the gangplank base at
        // -31,-790 but wipes there, healer out of position, then grinds: revive →
        // re-pull → die). A real tank descending into a hot zone pulls TIGHT:
        // healer on top of it, everyone topped, small steps. The tight zone is
        // script-declared (DungeonAdvice::tight_engage_below_z): the tank is in
        // it when its world-Z is below that threshold (e.g. the Deadmines harbor
        // floor at z<30; the gauntlet above sits at z57-62). When tight, require
        // the healer ≤18y, EVERY member ≥85% HP, and EVERY other member ≤25y (no
        // half-floor), and the route-advance below uses a 10y step instead of 20y.
        float tank_zx, tank_zy, tank_zz;
        s.position(tank_zx, tank_zy, tank_zz);
        const bool tight_engage =
            advice.tight_engage_below_z > 0.0f && tank_zz < advice.tight_engage_below_z;
        const float ready_heal_y = tight_engage ? 18.0f : 40.0f;
        const int   ready_hp_pct  = tight_engage ? 85 : 70;
        const float ready_other_y = tight_engage ? 25.0f : 50.0f;
        auto compute_group_ready = [&]() -> bool {
            if (!g.exists()) return true;
            auto const* mems = g.members();
            if (!mems) return true;
            float bx_ga, by_ga, bz_ga;
            s.position(bx_ga, by_ga, bz_ga);
            // Beyond this, a non-healer is "stranded" (being recovered separately),
            // not merely lagging — excluded from the harbor-tight cohesion hold so
            // it can't deadlock the pull. See the harbor branch below.
            constexpr float kStrandedY = 60.0f;
            uint32 others = 0, others_near = 0, others_stranded = 0;
            for (auto const& m : *mems)
            {
                if (!m.online || m.map_id != s.map_id()) continue;
                if (m.guid == s.raw().guid) continue;
                if (!m.is_alive) return false;
                if (m.max_hp > 0 &&
                    (int64(m.hp) * 100) < (int64(m.max_hp) * ready_hp_pct))
                    return false;
                const float dxg = m.x - bx_ga;
                const float dyg = m.y - by_ga;
                const float d2g = dxg*dxg + dyg*dyg;
                if (m.role == Role::Healer)
                {
                    if (d2g > ready_heal_y * ready_heal_y) return false;
                    if (m.max_mana > 0 &&
                        (int64(m.mana) * 100) < (int64(m.max_mana) * 50))
                        return false;
                }
                else
                {
                    ++others;
                    if (d2g <= ready_other_y * ready_other_y) ++others_near;
                    else if (d2g > kStrandedY * kStrandedY) ++others_stranded;
                }
            }
            // Gauntlet: cohesion floor (half the non-healers near) is enough — a
            // straggler DPS shouldn't freeze the run. Harbor: every non-healer must
            // be TIGHT before a burst pull — EXCEPT one stranded far away (>60y),
            // which is a recovery problem, not a cohesion one, and must NOT deadlock
            // the pull (observed 2026-06-28: a relogged Dungmage respawned at the
            // entrance 240y back, stuck in dps_assist; harbor-tight's "ALL ≤25y"
            // held the tank in the harbor FOREVER waiting for it). So hold only for
            // non-healers "catching up" in the (25y,60y] band; pull with the tight
            // core once they clear, letting the stranded one rejoin. The healer is
            // still required ≤18y above (never pull without the healer). Require ≥1
            // tight so we never pull totally alone.
            if (others == 0) return true;
            if (tight_engage)
            {
                const uint32 catching = others - others_near - others_stranded;
                return catching == 0 && others_near > 0;
            }
            return others_near * 2 >= others;
        };
        bool group_ready_for_advance = false;
        if (ai.effective_role(s) == Role::Tank && s.is_in_dungeon())
            group_ready_for_advance = compute_group_ready();

        // Rich, throttled (10s) gate diagnostic — the previous edge-triggered
        // one-liner couldn't answer "why is the tank standing still" without
        // a debugger (2026-06-11: three Stockades stall reports in one day,
        // each a different gate). Mirrors the EXACT fire-condition order of
        // the advance block below, including off-tank duty and the
        // group-ready breakdown.
        if (ai.effective_role(s) == Role::Tank && s.is_in_dungeon() &&
            ai.dungeon_active())
        {
            char const* gate_blocker = nullptr;
            const bool diag_off_tank =
                s.raw().pve_order.active &&
                !s.raw().pve_order.main_tank.IsEmpty() &&
                s.raw().pve_order.main_tank != s.guid();
            if (!s.is_alive())                       gate_blocker = "dead";
            else if (s.is_casting())                 gate_blocker = "casting";
            else if (s.in_combat())                  gate_blocker = "in_combat";
            else if (diag_off_tank)                  gate_blocker = "off_tank_duty";
            else if (s.is_encounter_in_progress())   gate_blocker = "encounter_in_progress";
            else if (now_ms < ai.chat_pause_until_ms()) gate_blocker = "chat_pause";
            else if (s.raw().dungeon_exec.any_boss_in_special) gate_blocker = "boss_special";
            else if (!group_ready_for_advance)       gate_blocker = "group_not_ready";
            // post_kill_cooldown deliberately NOT logged: the first idle
            // tick after every pack dies trips it by definition (kill_dt=0)
            // and it self-clears at 2.5s — it cannot wedge. Logging it
            // produced a false alarm per pull (observed 3x kill_dt=0ms in
            // the monitor stream, all healthy pacing).
            if (gate_blocker && ai.tank_diag_due(now_ms))
            {
                // group_not_ready detail: name the first failing member +
                // check so the log answers the question directly.
                std::string detail;
                if (std::string_view(gate_blocker) == "group_not_ready" && g.exists())
                {
                    if (auto const* mems = g.members())
                    {
                        float dbx, dby, dbz; s.position(dbx, dby, dbz);
                        for (auto const& m : *mems)
                        {
                            if (!m.online || m.map_id != s.map_id()) continue;
                            if (m.guid == s.raw().guid) continue;
                            const float ddx = m.x - dbx, ddy = m.y - dby;
                            const float dd  = std::sqrt(ddx*ddx + ddy*ddy);
                            if (m.hp <= 0)
                            { detail = m.name + " dead"; break; }
                            if (m.max_hp > 0 && (int64(m.hp) * 100) < (int64(m.max_hp) * ready_hp_pct))
                            { detail = m.name + " low_hp"; break; }
                            if (m.role == Role::Healer)
                            {
                                if (dd > ready_heal_y)
                                { detail = m.name + " healer_far " + std::to_string(int(dd)) + "y"; break; }
                                if (m.max_mana > 0 && (int64(m.mana) * 100) < (int64(m.max_mana) * 50))
                                { detail = m.name + " healer_mana"; break; }
                            }
                        }
                        if (detail.empty()) detail = tight_engage
                            ? "cohesion (harbor-tight: a non-healer catching up 25-60y)"
                            : "cohesion (half of non-healers >50y)";
                    }
                }
                TC_LOG_INFO("playerbot.v2",
                    "[tank_advance] {} blocked gate={}{}{} (kill_dt={}ms run_mode={})",
                    s.name(), gate_blocker,
                    detail.empty() ? "" : " ", detail,
                    now_ms - ai.last_kill_ms(), uint32(ai.dungeon_run_mode()));
            }
        }

        // ── Tank catch-up to a forward group (Deadmines Helix deadlock, 2026-06-25) ──
        // The group-ready advance gate (below) holds the tank until the healer
        // and half the DPS are within range. That is correct when the tank is at
        // the FRONT and the group is catching up — but it INVERTS when the group
        // has run AHEAD of the tank toward the boss and the tank is the laggard.
        // Observed live in the Deadmines foundry: the tank held on the Gap-1 z51
        // ledge for 7+ minutes (gate=group_not_ready, healer_far 103y) while the
        // healer + 2 DPS stood next to Helix ~100y away down on the z19 floor. The
        // healer cannot satisfy "within 40y of the tank" without abandoning the
        // boss, and its own opener-approach keeps dragging it back toward Helix —
        // a hard deadlock the regroup logic cannot break because the RALLY POINT is
        // wrong (it pulls the forward group BACK to a tank that should be moving
        // FORWARD). A real tank whose group face-pulled the boss runs to the boss;
        // it does not stand still. So when the normal advance is blocked ONLY
        // because members are far AHEAD (closer to the boss than the tank), the
        // tank takes a capped step toward the boss to retake point. Movement only —
        // the existing boss-nav/engage fires once the tank arrives and the now-
        // clustered group reads ready. Gated on !group_ready so the ready path owns
        // the normal case; the "behind" test (a member >45y from the tank yet >15y
        // CLOSER to the boss) guarantees this never sprints the tank ahead of a
        // trailing group — it only closes a gap the group itself opened forward.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            s.is_in_dungeon() && ai.dungeon_active() &&
            !s.is_encounter_in_progress() &&
            now_ms >= ai.chat_pause_until_ms() &&
            !s.raw().dungeon_exec.any_boss_in_special &&
            !group_ready_for_advance &&
            !advice.bosses.empty() &&
            !(s.raw().pve_order.active &&
              !s.raw().pve_order.main_tank.IsEmpty() &&
              s.raw().pve_order.main_tank != s.guid()) &&
            g.exists() && g.members())
        {
            if (Player* self_cu = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
            {
                // Earliest live boss in progression order (advice.bosses is
                // authored in encounter order), whole-dungeon scan radius.
                struct CatchupBossCheck
                {
                    Player const* origin;
                    std::vector<uint32_t> const& entries;
                    float range;
                    bool operator()(Creature* c) const
                    {
                        if (!c || !c->IsAlive()) return false;
                        for (uint32_t e : entries)
                            if (c->GetEntry() == e)
                                return origin->IsWithinDistInMap(c, range);
                        return false;
                    }
                };
                constexpr float kCuScanR = 500.0f;
                std::list<Creature*> cu_bcre;
                CatchupBossCheck cu_chk{self_cu, advice.bosses, kCuScanR};
                Trinity::CreatureListSearcher<CatchupBossCheck> cu_search(
                    self_cu, cu_bcre, cu_chk);
                Cell::VisitAllObjects(self_cu, cu_search, kCuScanR);
                Creature* cu_boss = nullptr;
                for (uint32_t be : advice.bosses)
                {
                    for (Creature* bc : cu_bcre)
                        if (bc && bc->GetEntry() == be && bc->IsAlive())
                        { cu_boss = bc; break; }
                    if (cu_boss) break;
                }
                if (cu_boss)
                {
                    float tcx, tcy, tcz; s.position(tcx, tcy, tcz);
                    const float cbx = cu_boss->GetPositionX();
                    const float cby = cu_boss->GetPositionY();
                    const float cbz = cu_boss->GetPositionZ();
                    // 2D progress toward the boss: who has advanced furthest down
                    // the corridor. The boss sits on the z19 floor while the tank
                    // is on a z51 ledge — vertical separation must NOT count as
                    // "distance to boss", so the behind-test is XY only.
                    const float tank_db2 = (cbx - tcx) * (cbx - tcx) +
                                           (cby - tcy) * (cby - tcy);
                    constexpr float kBehindMargin2 = 15.0f * 15.0f;
                    constexpr float kFarFromTank2  = 45.0f * 45.0f;
                    bool tank_behind = false;
                    GroupMemberSummary const* fwd = nullptr;
                    float fwd_db2 = tank_db2;
                    for (auto const& m : *g.members())
                    {
                        if (!m.online || m.map_id != s.map_id()) continue;
                        if (m.guid == s.raw().guid || !m.is_alive) continue;
                        const float mdb2 = (cbx - m.x) * (cbx - m.x) +
                                           (cby - m.y) * (cby - m.y);
                        const float mdt2 = (m.x - tcx) * (m.x - tcx) +
                                           (m.y - tcy) * (m.y - tcy);
                        if (mdb2 + kBehindMargin2 < tank_db2 && mdt2 > kFarFromTank2)
                        {
                            tank_behind = true;
                            if (mdb2 < fwd_db2) { fwd_db2 = mdb2; fwd = &m; }
                        }
                    }
                    if (tank_behind)
                    {
                        constexpr float kCatchupStep = 45.0f;
                        G3D::Vector3 cstep;
                        bool cu_off = false;
                        bool moved = false;
                        char const* via = "none";
                        if (DungeonTargetReachableAndStep(self_cu, cbx, cby, cbz,
                                                          kCatchupStep, cstep, &cu_off))
                        {
                            if (cu_off)
                                ai.set_dungeon_cross(cstep.x, cstep.y, cstep.z,
                                                     now_ms + 12000);
                            moved = emit.move_to(cstep.x, cstep.y, cstep.z, /*run=*/true);
                            via = cu_off ? "boss_offmesh" : "boss";
                        }
                        else if (fwd)
                        {
                            // Boss not strictly reachable from the ledge (the z51→
                            // z19 descent the strict test rejects). Descend toward
                            // the furthest-forward member instead — it stands on a
                            // poly the tank CAN reach and lies on the boss-ward
                            // route, so this closes the split without a boss path.
                            moved = emit.move_to(fwd->x, fwd->y, fwd->z, /*run=*/true);
                            via = "fwd_member";
                        }
                        if (moved)
                        {
                            if (ai.tank_diag_due(now_ms))
                                TC_LOG_INFO("playerbot.v2",
                                    "[tank_catchup] {} boss={} tank_dist={:.1f} "
                                    "fwd_dist={:.1f} via={}",
                                    s.name(), cu_boss->GetEntry(),
                                    std::sqrt(tank_db2), std::sqrt(fwd_db2), via);
                            ai.set_last_rule_fired("idle:dungeon_tank_catchup_boss");
                            return true;
                        }
                    }
                }
            }
        }

        // PvE-coordinator: pull initiative belongs to the designated MAIN
        // tank alone. Any other tank-role bot — the raid off-tank (who
        // additionally shadows the main, consumer below) or a 5-man's
        // redundant second tank-spec member (who otherwise behaves as
        // DPS; classic comp is 1/1/3, off-tanks are a raid concept) —
        // never leads. Without this, both bot tanks independently passed
        // the role test and could march the group toward DIFFERENT packs.
        const bool pve_off_tank =
            s.raw().pve_order.active &&
            !s.raw().pve_order.main_tank.IsEmpty() &&
            s.raw().pve_order.main_tank != s.guid();
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            !pve_off_tank &&
            s.is_in_dungeon() &&
            !s.is_encounter_in_progress() &&
            // G-P1a: mirror tank_pull_next's gates so a paused or phase-locked
            // tank doesn't keep marching the group into the next pack — a real
            // tank stops when the leader says "wait"/"inc" and never advances
            // while a boss is mid-special.
            now_ms >= ai.chat_pause_until_ms() &&
            !s.raw().dungeon_exec.any_boss_in_special &&
            // G-P1b: group readiness is a HARD precondition for advancing, not
            // merely a way to bypass the post-kill cooldown. The tank waits for
            // the group to be alive/near/topped before pulling the next pack
            // (unified with tank_pull_next's readiness), and still paces ~5s
            // after a kill so it doesn't chain-pull on top of a fresh kill.
            group_ready_for_advance &&
            // Post-kill pacing trimmed 5000→2500ms: readiness above already
            // holds the tank while anyone is hurt / the healer drinks; a
            // ready group standing around 5s after every pack read as
            // "too slow, not coordinated" (user, 2026-06-11). 2.5s keeps a
            // breath between packs without the dead air.
            now_ms - ai.last_kill_ms() >= 2500)
        {
            bool any_member_in_combat_adv = false;
            if (g.exists())
            {
                if (auto const* mems = g.members())
                {
                    for (auto const& m : *mems)
                    {
                        // Only a member CURRENTLY FIGHTING blocks the advance.
                        // A bare in_combat flag with no victim is a phantom
                        // tag (TC's out-of-combat timer ~5-10s; evaded/leashed
                        // mob; pet threat remnant). Moving away drops phantom
                        // tags naturally. Also count a member actively casting
                        // at a target (e.g. healer casting Smite = in a real
                        // fight with no melee victim).
                        //
                        // NOTE: the earlier version also checked `m.hp < m.max_hp`
                        // to distinguish "was hit" from phantom combat. That caused
                        // a permanent block: after EVERY pack kill, DPS bots retain
                        // in_combat=true for ~5-10s (TC timer) AND have hp < max_hp
                        // from earlier damage, so the check always fired and the
                        // advance NEVER got a clear window (observed live: 5+ min
                        // stuck at same position, 0 dungeon_tank_advance firings).
                        // "hp < max_hp" is a past event, not a current fight signal;
                        // only victim + casting_target are CURRENT-state indicators.
                        if (m.in_combat &&
                            (!m.victim.IsEmpty() ||
                             (m.is_casting && !m.casting_target.IsEmpty())))
                        { any_member_in_combat_adv = true; break; }
                    }
                }
            }
            if (any_member_in_combat_adv)
            {
                char const* last = ai.last_rule_fired();
                if (!last || std::string_view(last) != "idle:tank_advance_member_in_combat")
                {
                    TC_LOG_INFO("playerbot.v2",
                        "[tank_advance] {} blocked: a group member is in combat",
                        s.name());
                    ai.set_last_rule_fired("idle:tank_advance_member_in_combat");
                }
            }
            if (!any_member_in_combat_adv)
            {
                // First check: is there any enemy WITHIN the 30y pull range?
                // If yes, tank_pull_next will handle it; we don't need to
                // advance. Skip and fall through.
                float bx_adv, by_adv, bz_adv;
                s.position(bx_adv, by_adv, bz_adv);
                // ── Harbor staging gate (2026-06-26) ───────────────────────────
                // ROOT FIX for the Deadmines harbor death-fragmentation cycle.
                // The multi-level off-mesh descent to the harbor floor (Gap-1 z51 ->
                // harbor z19) SERIALIZES, so the tank reaches the harbor first and
                // pulls the Defias pirate packs while the healer/DPS are still strung
                // out on the descent above. The lone tank dies; with the only healer
                // either descending or in-combat the OOC rez never fires, so it burns
                // the 90s backstop, auto-revives 250y away at the entrance GY, and the
                // group fragments across z19/z52/z55 chasing it (force_rel=4 / 35min,
                // mind2rip floored at ~237 -> Ripsnarl never reached). The reach
                // machinery itself is FINE: probe-confirmed the deck is continuous
                // navmesh (74-poly cap, not off-mesh) and tier-4 boss_nav reaches it
                // ([harbor_probe] reach=1) WHEN the tank survives there. So the only
                // blocker is the tank tanking the harbor SOLO. Gate: once the tank has
                // descended to the harbor floor (z<30, well below the foundry z48+ /
                // Gap-1 z51 — map 36 only, so the gauntlet and other dungeons are
                // untouched), HOLD the advance/pull until the WHOLE living group is
                // clustered tight (<=kHarborStageR), so the descent finishes as a ball
                // and the healer is in range when the first pirate pull lands. The
                // followers' regroup_follow_tank (22y target) reliably balls them up,
                // so this self-clears; a dead/stranded member is handled by go_rez /
                // escort_fallen / the 90s watchdog, so it cannot deadlock. Generalizes
                // conceptually to any deep descent into an aggro zone.
                if (s.map_id() == 36 && bz_adv < 30.0f && g.exists())
                {
                    if (auto const* hmems = g.members())
                    {
                        constexpr float kHarborStageR2 = 25.0f * 25.0f;
                        bool harbor_balled = true;
                        for (auto const& m : *hmems)
                        {
                            if (m.guid == s.guid()) continue;
                            if (!m.online || m.map_id != s.map_id()) continue;
                            if (!m.is_alive) continue;  // dead -> go_rez/escort/watchdog
                            const float hdx = m.x - bx_adv;
                            const float hdy = m.y - by_adv;
                            const float hdz = m.z - bz_adv;
                            if (hdx * hdx + hdy * hdy + hdz * hdz > kHarborStageR2)
                            { harbor_balled = false; break; }
                        }
                        if (!harbor_balled)
                        {
                            ai.set_last_rule_fired("idle:dungeon_harbor_stage");
                            return true;  // hold: no solo harbor pull until grouped
                        }
                    }
                }
                // Cohesion leash on the advance STEP. Every advance branch
                // below walks toward a target (trash up to 150y / the next
                // boss up to 500y away) stopping ~25y short — but the move is
                // otherwise bounded only by the pathfinder's 292y cap. A single
                // long leap toward a far boss outruns the group: the readiness
                // gate is satisfied for the instant the tank steps off, then it
                // crosses a terrain transition (the Deadmines harbor z58->z0
                // drop) the followers can't path in one hop, stranding them
                // ~1150y back while the tank reaches the end alone (observed
                // live: 0/6, healer_far 1154y). Capping each step keeps the
                // tank at most ~kMaxAdvanceStep ahead, so the followers always
                // have a SHORT, valid path — they retrace the tank's own recent
                // (therefore traversable) footsteps, drop included. Slower but
                // cohesive: a 292y boss approach becomes a string of short hops,
                // paced by the readiness gate between them.
                // STEP must stay BELOW the group-ready healer gate (40y): the gate
                // is checked at step START, but the spline then carries the tank
                // the FULL step, pulling any trash along it and ENTERING COMBAT
                // mid-step — and once in combat the idle readiness gate no longer
                // runs, so an over-long step strands the tank fighting alone past
                // heal range. A 45y step did exactly that (live 06-26: tank pulled
                // 6 mobs at -288,-593 z49, 38y+ ahead of a group fragmented across
                // z34/z48/z51, no healer in range, bursted 79%->0 in ~4s, revived
                // 250y at the entrance GY -> fragmentation loop). 20y keeps the
                // pulled pack inside the healer's 40y support envelope and makes
                // each hop short enough for followers to track without stranding
                // on a harbor z-transition.
                constexpr float kMaxAdvanceStep = 20.0f;
                // Diagnostic accumulators — filled by each advance branch;
                // reported once in the [tank_adv_diag] NO-TARGET log below.
                int    diag_s150_cand    = 0;
                uint32 diag_s150_first   = 0;
                bool   diag_s150_reach   = false;
                int    diag_boss_cand    = 0;
                bool   diag_boss_reach   = false;
                NearbyUnit const* close_target = nullptr;
                NearbyUnit const* far_target   = nullptr;
                float far_d2 = std::numeric_limits<float>::max();
                // Directional trash-scan anchor: the EARLIEST live boss's XY, filled
                // by the boss scan below. The trash navigators skip any candidate that
                // sits FARTHER from the boss than the tank already is — so a tank on
                // the near side of a chokepoint (boss reach=0 across an offmesh) never
                // wanders BACKWARD to entrance-side trash (Deadmines: 140y NE to the
                // crime-scene Defias) while Helix waits across the gap. Only the
                // wide/far navigators (which can pick distant backward trash) honor it;
                // the close 30y pull is unaffected.
                bool  adv_have_boss = false;
                float adv_boss_x = 0.f, adv_boss_y = 0.f;

                // ── Boss-priority navigation (Deadmines Glubtok fix, 2026-06-25) ──
                // The tank navigates to the EARLIEST un-killed boss in
                // advice.bosses[] order and engages it, BEFORE the trash/waypoint
                // navigators below — so it never marches PAST a live boss chasing
                // forward trash. Observed live: the cohesive squad walked THROUGH
                // Glubtok's room (47162 @ -193,-442) to the foundry (0/6) and
                // wedged on the foundry nav gap, because the forward-trash scan
                // outranked the boss-as-destination scan and so pulled the tank
                // onward past the un-killed boss.
                //
                // Engagement is LoS-based, not strict-navmesh. Two root causes:
                //  (1) idle:dungeon_engage_boss (below) depends on
                //      has_visible_boss()/current_boss_guid, which the snapshot
                //      fills ONLY from creatures flagged
                //      CREATURE_FLAG_EXTRA_DUNGEON_BOSS (Creature::IsDungeonBoss).
                //      Glubtok (flags_extra=1) lacks that bit, so the engage rule
                //      stays dark even point-blank. Sourcing the target from
                //      advice.bosses[] (the authored encounter list) is entry-
                //      driven and flag-independent.
                //  (2) Glubtok stands on encounter terrain (fire-platter footing)
                //      whose exact poly FAILS DungeonTargetReachable's NORMAL-path
                //      test even though melee is physically reachable (live: bots
                //      cast at him from 0.0y). So strict-reachability branches skip
                //      him forever. We COMMIT to the earliest live boss and close
                //      in three ways, in order of preference:
                //        * within ENGAGE range + LoS -> start_attack (pull); the
                //          chase + terrain-walk fallback closes the last yards.
                //        * else strict-reachable -> leashed approach (navmesh step).
                //        * else (same-Z, within APPROACH range) -> leashed move
                //          toward the boss's XY at the tank's own Z. move_to
                //          validates the path/terrain itself (Locked on NoPath), so
                //          this can't walk the tank off-route; it just closes on an
                //          off-mesh-footed boss the strict test rejects. The same-Z
                //          gate (|dz|<25) preserves multi-level behavior — a ship-
                //          deck boss above stays deferred to waypoint progression.
                // All advance-block gates (group ready, !encounter_in_progress,
                // !any_boss_in_special, post-kill pacing) are already satisfied.
                if (Player* self_be = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                {
                    if (!advice.bosses.empty())
                    {
                        constexpr float kBossScanR     = 500.0f;  // whole-dungeon
                        constexpr float kBossEngageR2   = 25.0f * 25.0f;  // LoS pull
                        // SHORT non-strict close-in: only for the last yards to an
                        // off-mesh-footed boss the strict test rejects point-blank
                        // (Glubtok's fire-platter). NOT long-range navigation — a
                        // wide radius here HIJACKS the advance: the straight-line
                        // move toward a far, strict-unreachable boss (Helix across
                        // the foundry door) is move_blocked by the intervening wall
                        // every tick yet still claims the dispatch, starving the
                        // waypoint/trash navigators that DO know the corridor route
                        // (observed 2026-06-25: 160y radius pinned the tank at
                        // Glubtok's spot, Helix reach=0, never reaching the foundry
                        // approach where Helix becomes reach=1). Beyond this, defer.
                        constexpr float kBossApproachR  = 30.0f;
                        struct BossEntryCheck
                        {
                            Player const* origin;
                            std::vector<uint32_t> const& entries;
                            float range;
                            bool operator()(Creature* c) const
                            {
                                if (!c || !c->IsAlive()) return false;
                                for (uint32_t e : entries)
                                    if (c->GetEntry() == e)
                                        return origin->IsWithinDistInMap(c, range);
                                return false;
                            }
                        };
                        std::list<Creature*> bcre;
                        BossEntryCheck bchk{self_be, advice.bosses, kBossScanR};
                        Trinity::CreatureListSearcher<BossEntryCheck> bsearch(self_be, bcre, bchk);
                        Cell::VisitAllObjects(self_be, bsearch, kBossScanR);

                        // Commit to the EARLIEST live boss in progression order
                        // (advice.bosses is authored in encounter order). Once
                        // chosen, never skip ahead to a later boss this tick.
                        Creature* boss_target = nullptr;
                        for (uint32_t bentry : advice.bosses)
                        {
                            for (Creature* bc : bcre)
                                if (bc && bc->GetEntry() == bentry && bc->IsAlive())
                                { boss_target = bc; break; }
                            if (boss_target) break;
                        }
                        if (boss_target)
                        {
                            const float btx = boss_target->GetPositionX();
                            const float bty = boss_target->GetPositionY();
                            const float btz = boss_target->GetPositionZ();
                            adv_have_boss = true;
                            adv_boss_x = btx;
                            adv_boss_y = bty;
                            const float bdx = btx - bx_adv;
                            const float bdy = bty - by_adv;
                            const float bdz = btz - bz_adv;
                            const float bd2 = bdx*bdx + bdy*bdy + bdz*bdz;
                            const bool  in_los = self_be->IsWithinLOSInMap(boss_target);
                            G3D::Vector3 boss_step;
                            bool boss_step_offmesh = false;
                            const bool reachable = DungeonTargetReachableAndStep(
                                self_be, btx, bty, btz, kMaxAdvanceStep, boss_step,
                                &boss_step_offmesh);
                            // Increment 1d (2026-07-03): while the OOC route follower
                            // has an armed cursor for this map, it owns boss
                            // navigation — the direct-step branches below defer to it
                            // (see DungeonRouteArmed's header for the WC oscillation
                            // this fixes). Computed here, ABOVE the [boss_nav] diag,
                            // so the diag's action field reflects the deference
                            // instead of claiming "stride" on a tick the stride is
                            // gated (review fix, 2026-07-03).
                            const bool route_armed_ooc = DungeonRouteArmed(s, ai, advice);
                            // Throttled ground-truth diagnostic so the boss
                            // approach is debuggable from the log alone.
                            if (ai.tank_diag_due(now_ms))
                                TC_LOG_INFO("playerbot.v2",
                                    "[boss_nav] {} boss={} dist={:.1f} los={} reach={} "
                                    "action={} pos=({:.1f},{:.1f}) step=({:.1f},{:.1f},{:.1f})",
                                    s.name(), boss_target->GetEntry(),
                                    std::sqrt(bd2), in_los ? 1 : 0, reachable ? 1 : 0,
                                    (bd2 <= kBossEngageR2 && in_los) ? "engage"
                                        : route_armed_ooc ? "route_own"
                                        : (reachable ? "stride" : "approach"),
                                    bx_adv, by_adv,
                                    boss_step.x, boss_step.y, boss_step.z);
                            // POST-REZ / LOW-HP ADVANCE HOLD (2026-06-28, harbor
                            // death-loop fix). A tank freshly raised by Resurrection
                            // comes back at ~34% HP (Resurrection restores partial
                            // HP); if it then immediately route-advances toward a far
                            // boss it walks into the next Defias pull and is re-bursted
                            // to death before the healer tops it off — the live harbor
                            // 20-death grind (rez->advance@34%->die->rez...). Before
                            // advancing toward a NOT-yet-engaged boss, if we are below
                            // full-ish HP AND a healer is alive in heal range AND
                            // nothing is currently hitting us (safe to pause — the
                            // Defias leash/reset after the last death), HOLD so the
                            // healer (MaintainOocHeal, <80% HP) heals us up first. The
                            // no-attacker guard means an ACTIVE pull still fights
                            // (fleeing mid-pull is worse); the healer-present guard
                            // prevents a no-healer deadlock (a DPS-only group keeps
                            // advancing); the boss-not-in-engage-range guard means a
                            // boss already at point-blank is still pulled. Self-clears
                            // the instant HP recovers past the bar. This makes the tank
                            // ENTER each harbor pull near-full instead of at 34%, the
                            // difference between a clearable fight and the death grind.
                            if (bd2 > kBossEngageR2 && s.hp_pct() < 80 &&
                                s.fightable_attackers_count() == 0)
                            {
                                bool heal_ready = false;
                                if (g.exists() && g.members())
                                    for (auto const& m : *g.members())
                                        if (m.online && m.is_alive && m.guid != s.guid() &&
                                            m.map_id == s.map_id() && m.role == Role::Healer)
                                        {
                                            const float hdx = m.x - bx_adv, hdy = m.y - by_adv,
                                                        hdz = m.z - bz_adv;
                                            if (hdx*hdx + hdy*hdy + hdz*hdz <= 40.0f * 40.0f)
                                            { heal_ready = true; break; }
                                        }
                                if (heal_ready)
                                {
                                    ai.set_last_rule_fired("idle:dungeon_tank_healup_hold");
                                    return true;
                                }
                            }
                            // NOTE (2026-06-29): a "clear high-priority trash before
                            // pulling the boss" gate was tried here (pull isolation) and
                            // REMOVED — it regressed Deadmines. The prior behavior (pull
                            // Ripsnarl and zerg the whole deck, boss + packs together)
                            // cleared 5/6 at ~1 death; making the group clear the 5+ e48522
                            // deck packs (25k each) FIRST was strictly worse — the packs
                            // alone out-burst the healer, so the group wiped on the trash
                            // before ever pulling the boss (12+ deaths, 0 boss progress).
                            // The all-in pull is the right call for this encounter, so the
                            // tank engages the boss directly below.
                            if (bd2 <= kBossEngageR2 && in_los)
                            {
                                // Pull: stationary boss, chasing can't overshoot.
                                if (!emit.start_attack(boss_target->GetGUID()))
                                    return false;
                                ai.note_engage(boss_target->GetGUID(), now_ms);
                                ai.set_last_rule_fired("idle:dungeon_engage_boss");
                                return true;
                            }
                            // Increment 1d (2026-07-03): route armed -> skip straight
                            // to the route rule below instead of computing our own
                            // direct-line step (route_armed_ooc hoisted above the
                            // [boss_nav] diag; see DungeonRouteArmed's header).
                            // Falls through unchanged when unarmed.
                            if (reachable && !route_armed_ooc)
                            {
                                if (boss_step_offmesh)
                                    ai.set_dungeon_cross(boss_step.x, boss_step.y,
                                                         boss_step.z, now_ms + 12000);
                                if (DungeonStepAlreadyInFlight(s, boss_step.x, boss_step.y,
                                                               boss_step.z))
                                {
                                    DungeonStepHoldDiag(s, "idle:dungeon_tank_advance_boss",
                                                        boss_step.x, boss_step.y, boss_step.z);
                                    ai.set_last_rule_fired("idle:dungeon_tank_advance_boss_hold");
                                    return true;
                                }
                                if (DungeonMoveOwnedElsewhere(s, ai, boss_step.x, boss_step.y,
                                                              boss_step.z, now_ms,
                                                              "idle:dungeon_tank_advance_boss"))
                                {
                                    ai.set_last_rule_fired("idle:dungeon_tank_advance_boss_hold");
                                    return true;
                                }
                                // Refused destination: FALL THROUGH past this whole
                                // direct-step branch (skip the unconditional return
                                // below) so the leashed-step / waypoint-chunked
                                // fallbacks further down — genuinely DIFFERENT
                                // candidates — get tried this same tick instead of
                                // re-hammering the poisoned boss_step.
                                if (!DungeonStepRefused(s, ai, boss_step.x, boss_step.y,
                                                        boss_step.z, now_ms))
                                {
                                    // Claim the tick ONLY if an intent was really
                                    // pushed. The emitter now returns false for a
                                    // recently-REFUSED destination (see the
                                    // [emit_refused] guard in BotIntentEmitter), and
                                    // claiming anyway produced "claim, no intent, no
                                    // fallback" — a silent freeze with the ladder
                                    // below never reached (campaign 2026-07-21).
                                    if (emit.move_to(boss_step.x, boss_step.y, boss_step.z,
                                                     /*run=*/true))
                                    {
                                        ai.note_move_commit(s.map_id(), boss_step.x, boss_step.y,
                                                            boss_step.z, now_ms);
                                        ai.set_last_rule_fired("idle:dungeon_tank_advance_boss");
                                        return true;
                                    }
                                }
                                DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance_boss",
                                                       boss_step.x, boss_step.y, boss_step.z);
                            }
                            // Off-mesh-footed boss the strict test rejects: close in
                            // with a leashed step toward its XY at the tank's Z,
                            // same-Z only (so we don't walk under an elevated boss).
                            // route armed: the route follower owns OOC navigation —
                            // the direct step diverges exactly where routes exist
                            // (WC 2026-07-03); fall through.
                            const float blen = std::sqrt(bdx*bdx + bdy*bdy);
                            if (blen > 0.5f && blen <= kBossApproachR &&
                                std::fabs(bdz) < 25.0f && !route_armed_ooc)
                            {
                                const float stp = std::min(blen, kMaxAdvanceStep);
                                const float sc  = stp / blen;
                                const float lshx = bx_adv + bdx * sc;
                                const float lshy = by_adv + bdy * sc;
                                const float lshz = bz_adv;
                                if (DungeonStepAlreadyInFlight(s, lshx, lshy, lshz))
                                {
                                    // in-flight: skip the re-emit (spline keeps
                                    // running) and FALL THROUGH like the
                                    // emitter-dedup false path always did —
                                    // lower rules (e.g. the ghost-combat break)
                                    // must keep their tick access mid-walk.
                                    DungeonStepHoldDiag(s, "idle:dungeon_tank_advance_boss",
                                                        lshx, lshy, lshz);
                                }
                                else if (DungeonMoveOwnedElsewhere(s, ai, lshx, lshy, lshz,
                                                                   now_ms,
                                                                   "idle:dungeon_tank_advance_boss"))
                                {
                                    // another objective owns the spline this
                                    // window — FALL THROUGH like above.
                                }
                                else if (DungeonStepRefused(s, ai, lshx, lshy, lshz, now_ms))
                                {
                                    // refused destination — FALL THROUGH so the
                                    // waypoint-chunked fallback below (a genuinely
                                    // different candidate) gets tried this tick.
                                    DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance_boss",
                                                           lshx, lshy, lshz);
                                }
                                else if (emit.move_to(lshx, lshy, lshz, /*run=*/true))
                                {
                                    ai.set_last_rule_fired("idle:dungeon_tank_advance_boss");
                                    ai.note_move_commit(s.map_id(), lshx, lshy, lshz, now_ms);
                                    return true;
                                }
                            }
                            // Waypoint-chunked approach (preferred over the raw
                            // truncated-path step below). The boss is past the 74-poly
                            // cap, so the strict reach failed. Rather than string-pull
                            // along the RAW truncated path — which on the Deadmines
                            // foundry->harbor descent cuts straight across an off-mesh
                            // ledge (z51->z14) and drops the tank ahead of the group
                            // (fall deaths -> solo pull -> fragmentation, the chronic 3/6
                            // stall) — route through the dungeon's authored on-navmesh
                            // route_waypoints. Pick the FARTHEST-FORWARD point that is
                            // (a) meaningfully closer to the boss than we are and (b)
                            // STRICTLY reachable (a clean <=74-poly NORMAL path), then
                            // step kMaxAdvanceStep toward it. This walks the validated
                            // harbor chain down the navmesh single-file; once the tank is
                            // close enough that the deck itself is strictly reachable the
                            // direct-stride branch above takes over and engages. Generic:
                            // any cap-far boss with a route chain hops through it. Falls
                            // through to the raw incremental step when no route point
                            // helps (no chain, or none forward+reachable yet).
                            if (!advice.route_waypoints.empty())
                            {
                                // Harbor: short, deliberate steps so the tank never
                                // out-ranges the tight-cohesion healer between pulls
                                // (see tight_engage above). Elsewhere the full stride.
                                const float route_step =
                                    tight_engage ? 10.0f : kMaxAdvanceStep;
                                const float self_bd = std::sqrt(bd2);
                                bool        wp_found = false;
                                int         route_cur = -1;  // crumb committed this tick
                                G3D::Vector3 best_wp_step;
                                // Off-mesh flag for the committed-cursor steer below (the
                                // ONLY route-rule call site wired to step_is_offmesh — see
                                // its call site for why): lets the route follower commit a
                                // DB nav-link crossing exactly like rule (0)'s pb_off does,
                                // instead of silently defaulting to nullptr and never
                                // being able to hop (the cursor OWNER could never cross the
                                // WC ledge link before this).
                                bool         rw_off = false;
                                const int   route_n =
                                    static_cast<int>(advice.route_waypoints.size());
                                // Monotonic high-water floor: the chain progress already
                                // committed (by either pass). It only ever RISES, and
                                // BOTH passes are forbidden from selecting a crumb behind
                                // it — so pass 0's "closest-to-boss" pick can never yank
                                // the tank BACKWARD onto an earlier crumb that pass 1 has
                                // advanced past. That backward yank was the pass0<->pass1
                                // tug-of-war which deadlocked the WC tank at (-48,422):
                                // pass 0 pulled SW to seq1 while pass 1 held seq2 NE, the
                                // two targets 30y apart, POINT spline reset every tick
                                // (live 07-01). Sharing one forward-only cursor dissolves
                                // it. Deadmines' authored MONOTONIC chain is unaffected:
                                // its closest-to-boss crumb is already the farthest
                                // reachable, so pass 0's pick is always AT/ABOVE the floor
                                // and the backward-skip guard below never trips.
                                int route_lo = ai.dungeon_route_wp(s.map_id());
                                if (route_lo < 0 || route_lo >= route_n) route_lo = 0;
                                // The crumb nearest the LIVE boss = the boss's position on
                                // the flat all-bosses chain. BOTH passes follow only the
                                // prefix [route_lo, boss_i]; without anchoring pass 0 here it
                                // targeted a post-boss crumb merely straight-line-closest to
                                // the boss and dragged the tank the wrong way (live WC 07-01:
                                // pass 0 picked seq35 near Mutanus while Cobrahn = seq4,
                                // walking the tank into the -53,320 dead-end pocket even
                                // though seq0->seq1->..->seq4 is a fully pathable route).
                                int   route_boss_i = 0;
                                float route_boss_bd = 1.0e18f;
                                for (int i = 0; i < route_n; ++i)
                                {
                                    auto const& rw = advice.route_waypoints[i];
                                    const float dx = btx - rw.x, dy = bty - rw.y, dz = btz - rw.z;
                                    const float d = std::sqrt(dx*dx + dy*dy + dz*dz);
                                    if (d < route_boss_bd) { route_boss_bd = d; route_boss_i = i; }
                                }
                                // PASS 0 (monotonic): within the pre-boss prefix, the
                                // STRICTLY-reachable waypoint CLOSEST to the boss that is
                                // ALSO nearer the boss than we are. For a monotonic authored
                                // chain the closest-to-boss crumb IS boss_i and every pick is
                                // at/below it, so restricting to [route_lo, boss_i] leaves
                                // Deadmines byte-unchanged; on a self-crossing generated
                                // chain it stops pass 0 chasing a post-boss crumb.
                                {
                                    float best_wp_bd = self_bd;
                                    int   pass0_i = -1;
                                    for (int i = route_lo; i <= route_boss_i; ++i)
                                    {
                                        auto const& rw = advice.route_waypoints[i];
                                        const float wbx = btx - rw.x, wby = bty - rw.y, wbz = btz - rw.z;
                                        const float wp_bd = std::sqrt(wbx*wbx + wby*wby + wbz*wbz);
                                        if (wp_bd > self_bd - 8.0f) continue;   // forward only
                                        const float swx = rw.x - bx_adv, swy = rw.y - by_adv,
                                                    swz = rw.z - bz_adv;
                                        if (swx*swx + swy*swy + swz*swz < 6.0f * 6.0f) continue;
                                        if (wp_bd >= best_wp_bd) continue;
                                        G3D::Vector3 wstep;
                                        // scan probe — hop side effects (cross commit + cooldown
                                        // claim) must not fire here; links are consumed at the
                                        // STEER call (the committed-cursor branch below).
                                        if (!DungeonTargetReachableAndStep(
                                                self_be, rw.x, rw.y, rw.z, route_step, wstep))
                                            continue;
                                        best_wp_bd  = wp_bd;
                                        best_wp_step = wstep;
                                        pass0_i     = i;
                                        wp_found    = true;
                                    }
                                    if (wp_found)
                                        route_cur = pass0_i;
                                }
                                // PASS 1 (committed breadcrumb chain-follow): pass 0
                                // found nothing because the reachable stretch of an
                                // AUTO-generated corridor runs AWAY from the boss
                                // (Wailing Caverns winds before doubling back to
                                // Cobrahn), so no reachable waypoint is closer to the
                                // boss than we are. Follow the ORDERED chain one
                                // breadcrumb at a time, COMMITTING to the chosen crumb
                                // until it is reached, then advancing the cursor by one.
                                //
                                // A stateless "best crumb this tick" pick OSCILLATES near
                                // ANY selection threshold. Live WC 2026-07-01: the tank
                                // parked ~15y from route seq1 (Cobrahn = seq4), and its
                                // own micro-motion made the nearest-crumb distance
                                // straddle the old 15y re-converge cutoff — one tick
                                // "re-converge N to seq1", next tick "forward-scan S to a
                                // far crumb", two targets 32y apart. That 32y swing is far
                                // past the move_to dedup radius (3y), so every flip reset
                                // the POINT spline and the tank jittered in place at ~0
                                // net progress. The committed cursor gives hysteresis:
                                // pick nearest, walk all the way to it (no mid-approach
                                // abandon), then step to the next crumb. Monotonic +1
                                // advance, so a self-looping corridor cannot ping-pong us.
                                if (!wp_found && advice.route_waypoints.size() >= 2)
                                {
                                    const int n = static_cast<int>(advice.route_waypoints.size());
                                    auto crumb_d = [&](int i) -> float {
                                        auto const& rw = advice.route_waypoints[i];
                                        const float dx = rw.x - bx_adv, dy = rw.y - by_adv,
                                                    dz = rw.z - bz_adv;
                                        return std::sqrt(dx*dx + dy*dy + dz*dz);
                                    };

                                    // The route table is ONE flat chain threading the
                                    // entrance through ALL bosses in order. Follow only
                                    // the PREFIX up to the CURRENT boss's chain node, so
                                    // the cursor can never over-run the boss onto a
                                    // later-boss crumb and steer AWAY from it — live WC
                                    // 07-01: a free cursor advanced to seq7 (post-Cobrahn)
                                    // while Cobrahn = seq4, so pass 1 dragged the tank
                                    // SOUTH while pass 0 pulled NORTH toward the boss; the
                                    // two opposite targets deadlocked at (-72,348). Anchor
                                    // the walk to boss_i = the crumb nearest the live boss
                                    // (hoisted above pass 0 so both passes share it).
                                    const int boss_i = route_boss_i;
                                    // Nearest chain node to self within [lo, boss_i]:
                                    // never consider post-boss crumbs, and never retreat
                                    // below the committed high-water floor (route_lo) — a
                                    // combat shove behind an already-passed crumb must not
                                    // re-target it (that would reopen the backward tug).
                                    const int lo = route_lo <= boss_i ? route_lo : boss_i;
                                    int   near_i = lo;
                                    float near_d = 1.0e18f;
                                    for (int i = lo; i <= boss_i; ++i)
                                    {
                                        const float d = crumb_d(i);
                                        if (d < near_d) { near_d = d; near_i = i; }
                                    }

                                    constexpr float kRouteArrive    = 8.0f;   // "reached" a crumb
                                    constexpr float kRouteReacquire = 30.0f;  // shoved off the chain

                                    int cur = ai.dungeon_route_wp(s.map_id());
                                    // Invalidate a cursor that is stale for THIS boss
                                    // segment (past the boss node, or below the floor —
                                    // e.g. the boss changed and the prefix moved).
                                    if (cur < 0 || cur > boss_i || cur < lo) cur = -1;
                                    if (cur < 0)
                                        // (Re)establish: commit to the nearest pre-boss
                                        // crumb and close the gap to it, or the next one
                                        // if we are already standing on the nearest.
                                        cur = (near_d <= kRouteArrive && near_i + 1 <= boss_i)
                                                  ? near_i + 1 : near_i;
                                    else if (crumb_d(cur) <= kRouteArrive)
                                    {
                                        // Reached the committed crumb -> advance one,
                                        // never past the boss node.
                                        cur = (cur + 1 <= boss_i) ? cur + 1 : cur;
                                        // PERSIST THE ARRIVAL IMMEDIATELY. Arrival is
                                        // progress STATE, not a steer result — the
                                        // unified commit below only writes the cursor
                                        // `if (wp_found)`, i.e. only when the NEXT
                                        // crumb is strictly reachable in one step.
                                        // 90% of shipped routes contain >50y crumb
                                        // gaps (campaign audit 2026-07-20: 129/144
                                        // maps, worst 1114y), so that steer fails, the
                                        // advance is DISCARDED, the boss-ward fallback
                                        // drags the tank 20y, the bot is then >8y from
                                        // the crumb and this rule steers BACK to it —
                                        // the 20y patrol loop that blocked most of the
                                        // dungeon matrix. Persisting here turns an
                                        // unreachable next crumb into a long walk
                                        // (the incremental fallback closes it) instead
                                        // of an infinite oscillation. Still monotonic
                                        // (floor-clamped) and still bounded by boss_i.
                                        ai.set_dungeon_route_wp(
                                            cur < route_lo ? route_lo : cur, s.map_id());
                                    }
                                    else if (near_d > kRouteReacquire && near_i != cur)
                                        // Combat shoved us far off the chain -> re-acquire
                                        // the nearest pre-boss crumb.
                                        cur = near_i;

                                    // Chain complete for this boss — the cursor sits ON
                                    // the boss-nearest crumb AND we have arrived at it
                                    // (<= kRouteArrive): DECLINE (leave wp_found false)
                                    // and hand navigation back to the direct steppers
                                    // (1d gated the strict stride; without this decline
                                    // the rule would claim forever re-targeting an
                                    // arrived crumb — a <8y destination is trivially
                                    // strict-reachable, so the steer below kept emitting
                                    // + returning true every tick at zero progress,
                                    // parking a tank whose boss is >25y away or LoS-
                                    // blocked). Control falls through to the UNGATED
                                    // long-range incremental step below, which closes
                                    // the last stretch to the boss via strict or
                                    // truncated paths exactly as pre-1d. The normal
                                    // endgame advance (cur+1 <= boss_i, handled above)
                                    // and pass-0 / unreachable-crumb fallback behavior
                                    // are untouched — this fires ONLY on the
                                    // arrived-final-crumb case (review fix, 2026-07-03).
                                    if (cur == boss_i && crumb_d(cur) <= kRouteArrive)
                                    {
                                        // wp_found stays false -> fall through.
                                        // CONSUME the route for this map: the chain is
                                        // walked, the boss-ward fallback owns the rest.
                                        // Without this the campaign's 20y patrol loop
                                        // runs forever (live 2026-07-20, Blackfathom +
                                        // Sunken Temple): decline -> fallback steps 20y
                                        // boss-ward (obj=-1) -> bot now >12y from the
                                        // crumb -> the 1i latch-release band frees the
                                        // reached-latch -> this rule RE-ACQUIRES the
                                        // crumb it already finished and walks the tank
                                        // back -> arrive -> decline -> repeat. 1i's
                                        // release is right for a transient brush at a
                                        // MID-route crumb; at the FINAL crumb the route
                                        // is genuinely done and must stop competing.
                                        ai.set_route_consumed(cur, s.map_id());
                                    }
                                    // Steer to the committed crumb; if it is (now)
                                    // unreachable, walk forward through the prefix to the
                                    // first reachable crumb and re-commit there.
                                    else
                                    {
                                        auto const& rw = advice.route_waypoints[cur];
                                        G3D::Vector3 wstep;
                                        if (DungeonTargetReachableAndStep(
                                                self_be, rw.x, rw.y, rw.z, route_step, wstep,
                                                &rw_off))
                                        {
                                            best_wp_step = wstep;
                                            route_cur    = cur;
                                            wp_found     = true;
                                        }
                                        else
                                        {
                                            for (int i = near_i; i <= boss_i && !wp_found; ++i)
                                            {
                                                // Skip a crumb we have effectively reached
                                                // (same threshold as arrival) so the fallback
                                                // leapfrogs FORWARD to the next reachable
                                                // crumb instead of re-committing the one we
                                                // stand on; if none ahead is reachable,
                                                // wp_found stays false and control falls to
                                                // the incremental boss step below.
                                                if (crumb_d(i) <= kRouteArrive) continue;
                                                auto const& rr = advice.route_waypoints[i];
                                                // Refusal-aware target selection (2026-07-20):
                                                // skip a crumb whose destination the API
                                                // refused recently (path-fail backoff) so the
                                                // leapfrog scan advances to the NEXT crumb
                                                // instead of re-committing the same poisoned
                                                // one every tick — see
                                                // BotAI::move_refused_recently header comment.
                                                // Gated on the same kill switch as the rest of
                                                // the step-hold/refusal family so
                                                // PlayerbotV2.Move.StepHoldEnabled=false keeps
                                                // this scan byte-identical to pre-fix behavior.
                                                if (Services::Config().move_step_hold_enabled() &&
                                                    ai.move_refused_recently(rr.x, rr.y, rr.z, now_ms))
                                                    continue;
                                                // scan probe — hop side effects (cross commit +
                                                // cooldown claim) must not fire here; links are
                                                // consumed at the STEER call above (cur's
                                                // DungeonTargetReachableAndStep(..., &rw_off)).
                                                if (!DungeonTargetReachableAndStep(
                                                        self_be, rr.x, rr.y, rr.z, route_step, wstep))
                                                    continue;
                                                cur          = i;
                                                best_wp_step = wstep;
                                                route_cur    = cur;
                                                wp_found     = true;
                                            }
                                        }
                                    }
                                }
                                // Unified monotonic commit (either pass): advance the
                                // high-water cursor, never letting it retreat below the
                                // floor this tick — this is what makes pass 0 and pass 1
                                // share ONE forward-only progress marker.
                                if (wp_found && route_cur >= 0)
                                    ai.set_dungeon_route_wp(
                                        route_cur < route_lo ? route_lo : route_cur,
                                        s.map_id());
                                if (wp_found)
                                {
                                    // Commit a DB nav-link crossing exactly like rule (0)'s
                                    // pb_off handling (idle:dungeon_combat_advance_boss):
                                    // the values here already equal what DungeonNavLinkHop
                                    // set internally on the far endpoint when the step came
                                    // from a hop, so this is a same-value refresh of the TTL
                                    // (harmless) — it only matters (and is required) when
                                    // rw_off came from the plain off-mesh-jump detection
                                    // further down the stepper, which does not itself touch
                                    // dungeon_cross.
                                    if (rw_off)
                                        ai.set_dungeon_cross(best_wp_step.x, best_wp_step.y,
                                                             best_wp_step.z, now_ms + 12000);
                                    static uint32 s_route_dbg_ms = 0;
                                    if (now_ms - s_route_dbg_ms > 1500u)
                                    {
                                        s_route_dbg_ms = now_ms;
                                        const float sd = std::sqrt(
                                            (best_wp_step.x - bx_adv) * (best_wp_step.x - bx_adv) +
                                            (best_wp_step.y - by_adv) * (best_wp_step.y - by_adv) +
                                            (best_wp_step.z - bz_adv) * (best_wp_step.z - bz_adv));
                                        TC_LOG_INFO("playerbot.v2",
                                            "[route_wp] {} boss={} self=({:.1f},{:.1f},{:.1f}) "
                                            "step=({:.1f},{:.1f},{:.1f}) step_dist={:.1f} cur={} n_wp={}",
                                            s.name(), boss_target->GetEntry(),
                                            bx_adv, by_adv, bz_adv,
                                            best_wp_step.x, best_wp_step.y, best_wp_step.z, sd,
                                            route_cur, advice.route_waypoints.size());
                                    }
                                    if (DungeonStepAlreadyInFlight(s, best_wp_step.x,
                                                                   best_wp_step.y, best_wp_step.z))
                                    {
                                        DungeonStepHoldDiag(s, "idle:dungeon_tank_advance_boss_route",
                                                            best_wp_step.x, best_wp_step.y,
                                                            best_wp_step.z);
                                        ai.set_last_rule_fired(
                                            "idle:dungeon_tank_advance_boss_route_hold");
                                        return true;
                                    }
                                    if (DungeonMoveOwnedElsewhere(s, ai, best_wp_step.x,
                                                                  best_wp_step.y, best_wp_step.z,
                                                                  now_ms,
                                                                  "idle:dungeon_tank_advance_boss_route",
                                                                  route_cur))
                                    {
                                        ai.set_last_rule_fired(
                                            "idle:dungeon_tank_advance_boss_route_hold");
                                        return true;
                                    }
                                    // Refused destination: FALL THROUGH past this
                                    // route-waypoint branch (skip the unconditional
                                    // return below) so the long-range incremental /
                                    // off-mesh-recovery fallbacks further down — real
                                    // alternative candidates — get a chance this tick.
                                    if (!DungeonStepRefused(s, ai, best_wp_step.x,
                                                            best_wp_step.y, best_wp_step.z,
                                                            now_ms))
                                    {
                                        if (emit.move_to(best_wp_step.x, best_wp_step.y,
                                                         best_wp_step.z, /*run=*/true))
                                            ai.note_move_commit(s.map_id(), best_wp_step.x,
                                                                best_wp_step.y, best_wp_step.z, now_ms,
                                                                route_cur);
                                        ai.set_last_rule_fired("idle:dungeon_tank_advance_boss_route");
                                        return true;
                                    }
                                    DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance_boss_route",
                                                           best_wp_step.x, best_wp_step.y,
                                                           best_wp_step.z);
                                }
                            }
                            // Long-range incremental approach: the strict test failed
                            // because the boss is farther than the core 74-poly path cap
                            // (MAX_PATH_LENGTH), so the full corridor string-pulls to a
                            // truncated (INCOMPLETE) path. Step toward that truncated-but-
                            // progressing path and re-path next tick — walking the real
                            // corridor (including any foundry off-mesh bridge) exactly
                            // like a player, instead of stalling at reach=0 under
                            // dungeon_hold. The net-progress gate inside the helper makes
                            // this INERT at a true navmesh disconnect (won't grind into a
                            // wall). Fixes Admiral Ripsnarl reach=0 on the foundry->harbor
                            // corridor after Foe Reaper opens GO_FOUNDRY_DOOR.
                            {
                                G3D::Vector3 prog_step;
                                bool prog_offmesh = false;
                                // Campaign class-B fix (2026-07-20): when a route is
                                // armed this fallback must walk to the SAME crumb the
                                // route rule owns, not the raw boss. Live evidence
                                // (Blackfathom/Sunken Temple/ZF/LBRS/Strat/Maraudon —
                                // 40% of all campaign failures): boss-direct and crumb
                                // targets sit 15-30y apart, each rule COMPLETES its
                                // walk, and the other walks the tank straight back —
                                // a 20y patrol loop forever (sticky ownership 1m made
                                // the walks complete, exposing the arrival-level
                                // ping-pong underneath). Same doctrine as 1b/1f: the
                                // armed cursor is the single source of truth for
                                // "which way to the boss". Objective threaded too, so
                                // the 1k same-objective hold applies between them.
                                float ptx = btx, pty = bty, ptz = btz;
                                int32_t prog_crumb = -1;
                                // NO yield here (deliberate — first attempt at this
                                // fix added one and DEADLOCKED Blackfathom: 20 min
                                // with every rule *_hold and zero MoveTo). THIS rule
                                // is the handoff target of the route rule's own
                                // final-crumb decline (1d) — if it yields too, the
                                // route declines, the fallback yields, and nobody
                                // claims. Substitution only: walk the SAME crumb the
                                // route owns while it is armed; when near-arrived
                                // DungeonAdvanceTarget returns the boss target and
                                // this rule legitimately closes the last stretch.
                                DungeonAdvanceTarget(s, ai, advice, btx, bty, btz,
                                                     ptx, pty, ptz,
                                                     "idle:dungeon_tank_advance_boss",
                                                     &prog_crumb);
                                if (DungeonTargetReachableAndStep(
                                        self_be, ptx, pty, ptz, kMaxAdvanceStep,
                                        prog_step, &prog_offmesh,
                                        /*allow_incomplete_progress=*/true))
                                {
                                    if (prog_offmesh)
                                        ai.set_dungeon_cross(prog_step.x, prog_step.y,
                                                             prog_step.z, now_ms + 12000);
                                    if (DungeonStepAlreadyInFlight(s, prog_step.x, prog_step.y,
                                                                   prog_step.z))
                                    {
                                        DungeonStepHoldDiag(s, "idle:dungeon_tank_advance_boss",
                                                            prog_step.x, prog_step.y, prog_step.z);
                                        ai.set_last_rule_fired("idle:dungeon_tank_advance_boss_hold");
                                        return true;
                                    }
                                    if (DungeonMoveOwnedElsewhere(s, ai, prog_step.x, prog_step.y,
                                                                  prog_step.z, now_ms,
                                                                  "idle:dungeon_tank_advance_boss",
                                                                  prog_crumb))
                                    {
                                        ai.set_last_rule_fired("idle:dungeon_tank_advance_boss_hold");
                                        return true;
                                    }
                                    // Refused destination: FALL THROUGH past this
                                    // incremental-progress branch (skip the
                                    // unconditional return below) so the off-mesh
                                    // boss-destination recovery further down — a
                                    // real alternative candidate — gets tried this
                                    // same tick.
                                    if (!DungeonStepRefused(s, ai, prog_step.x, prog_step.y,
                                                            prog_step.z, now_ms))
                                    {
                                        // Claim only on a real emit (see the same
                                        // note at the strict-stride site above).
                                        if (emit.move_to(prog_step.x, prog_step.y, prog_step.z,
                                                         /*run=*/true))
                                        {
                                            ai.note_move_commit(s.map_id(), prog_step.x, prog_step.y,
                                                                prog_step.z, now_ms, prog_crumb);
                                            ai.set_last_rule_fired("idle:dungeon_tank_advance_boss");
                                            return true;
                                        }
                                    }
                                    DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance_boss",
                                                           prog_step.x, prog_step.y, prog_step.z);
                                }
                            }
                            // Off-mesh boss DESTINATION recovery (Admiral Ripsnarl stands
                            // on the ship deck at z42.8 where findNearestPoly resolves NO
                            // poly at his feet -> strict + progress both NOPATH, pts=2).
                            // Route to the nearest navmesh poly to the boss (the dock
                            // beside/below the deck), which IS reachable when only the
                            // elevated deck is off-mesh; the 25y LoS engage above then
                            // pulls him (a mobile Cata boss closes to the dock) while
                            // ranged DPS/heals work over LoS. Human-like: walk to the dock
                            // and board the ship — NOT an off-mesh bridge around a gate.
                            // The progress variant's net-progress gate keeps this INERT if
                            // the dock itself is unreachable (a true harbor disconnect), so
                            // the throttled log below doubles as the connectivity probe.
                            {
                                Position nn;
                                if (BotMovement::NearestNavPoint(self_be, btx, bty, btz,
                                                                 45.0f, 45.0f, nn))
                                {
                                    G3D::Vector3 nn_step;
                                    bool nn_off = false;
                                    const bool nn_reach = DungeonTargetReachableAndStep(
                                        self_be, nn.GetPositionX(), nn.GetPositionY(),
                                        nn.GetPositionZ(), kMaxAdvanceStep, nn_step,
                                        &nn_off, /*allow_incomplete_progress=*/true);
                                    // TEMP connectivity log (private throttle: the
                                    // [boss_nav] line above already consumed tank_diag_due
                                    // this tick, which starved the prior probe).
                                    {
                                        static uint32 s_hp_ms = 0;
                                        const uint32 gnow = GameTime::GetGameTimeMS();
                                        if (gnow - s_hp_ms > 3000u)
                                        {
                                            s_hp_ms = gnow;
                                            const float hdx = nn.GetPositionX() - btx;
                                            const float hdy = nn.GetPositionY() - bty;
                                            TC_LOG_INFO("playerbot.v2",
                                                "[harbor_probe] nn=({:.1f},{:.1f},{:.1f}) "
                                                "nn2boss_h={:.1f} nn2boss_v={:.1f} reach={}",
                                                nn.GetPositionX(), nn.GetPositionY(),
                                                nn.GetPositionZ(),
                                                std::sqrt(hdx*hdx + hdy*hdy),
                                                nn.GetPositionZ() - btz, nn_reach ? 1 : 0);
                                        }
                                    }
                                    if (nn_reach)
                                    {
                                        if (nn_off)
                                            ai.set_dungeon_cross(nn_step.x, nn_step.y,
                                                                 nn_step.z, now_ms + 12000);
                                        if (DungeonStepAlreadyInFlight(s, nn_step.x, nn_step.y,
                                                                       nn_step.z))
                                        {
                                            DungeonStepHoldDiag(s, "idle:dungeon_tank_advance_boss",
                                                                nn_step.x, nn_step.y, nn_step.z);
                                            ai.set_last_rule_fired(
                                                "idle:dungeon_tank_advance_boss_hold");
                                            return true;
                                        }
                                        if (DungeonMoveOwnedElsewhere(s, ai, nn_step.x, nn_step.y,
                                                                      nn_step.z, now_ms,
                                                                      "idle:dungeon_tank_advance_boss"))
                                        {
                                            ai.set_last_rule_fired(
                                                "idle:dungeon_tank_advance_boss_hold");
                                            return true;
                                        }
                                        // Refused destination: FALL THROUGH (this is
                                        // the last fallback in the boss-advance
                                        // cascade) to the trash navigators below —
                                        // a genuinely different candidate — instead
                                        // of re-hammering the poisoned nn_step.
                                        if (!DungeonStepRefused(s, ai, nn_step.x, nn_step.y,
                                                                nn_step.z, now_ms))
                                        {
                                            if (emit.move_to(nn_step.x, nn_step.y, nn_step.z,
                                                             /*run=*/true))
                                                ai.note_move_commit(s.map_id(), nn_step.x, nn_step.y,
                                                                    nn_step.z, now_ms);
                                            ai.set_last_rule_fired("idle:dungeon_tank_advance_boss");
                                            return true;
                                        }
                                        DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance_boss",
                                                               nn_step.x, nn_step.y, nn_step.z);
                                    }
                                }
                            }
                            // Boss alive but not yet engageable/reachable/approachable
                            // (too far, or behind a gate/level the trash + waypoints
                            // below must open). Fall through to the trash navigators.
                        }
                    }
                }

                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0) continue;
                    if (!u.victim.IsEmpty()) continue;
                    if (u.is_dungeon_boss) continue;
                    // Environmental/unkillable mobs (props, Mining Powder, etc.)
                    // have no_xp_kill or PACIFIED. Skip — mirrored in tank_pull_next.
                    if (u.no_xp_kill || u.is_pacified || u.untargetable) continue;
                    // Per-dungeon environmental object ignore list: encounter props
                    // (fire platters, bunny stalkers, rope anchors) that are alive
                    // but unkillable — spells against them are always interrupted,
                    // causing a stuck loop blocking tank advance (Glubtok Firewall
                    // Platters held the mine group for 90s+, observed 2026-06-25).
                    {
                        bool env_ignored = false;
                        for (uint32_t ie : advice.ignore_entries)
                            if (ie == u.entry) { env_ignored = true; break; }
                        if (env_ignored) continue;
                    }
                    // MUST mirror tank_pull_next's eligibility exactly,
                    // including the LoS gate: a close-but-walled-off mob
                    // (Stockades cell trash) otherwise makes this scan say
                    // "pull will handle it" while the pull's in_los gate
                    // refuses the same mob — advance defers, pull declines,
                    // tank deadlocks (caught live by the NO TARGET diag
                    // 2026-06-12: close=true, nothing fired). Out-of-LoS
                    // mobs are simply invisible here; the tank advances
                    // toward visible trash / the next boss and gains LoS
                    // by walking the corridor.
                    if (!u.in_los) continue;
                    // Mirror tank_pull_next's eligibility filters for note_engage
                    // (opener_give_up 5-min block) and start_attack lockout (30s
                    // ServerRefused block). Without both checks, a target blocked
                    // in tank_pull_next still sets close_target=true here, causing
                    // advance to forever defer ("pull will handle it" — but pull
                    // skips the same target). Both mismatch scenarios observed live
                    // in Deadmines: opener_give_up on an unkillable mob, and
                    // start_attack ServerRefused on an immune target.
                    if (u.guid == ai.last_engage_target() &&
                        now_ms - ai.last_engage_at_ms() < ai.last_engage_shield_ms()) continue;
                    if (ai.start_attack_recently_refused(u.guid.GetCounter(), now_ms)) continue;
                    const float dx = u.x - bx_adv;
                    const float dy = u.y - by_adv;
                    const float dz = u.z - bz_adv;
                    const float d2 = dx*dx + dy*dy + dz*dz;
                    if (d2 <= 30.0f * 30.0f) { close_target = &u; break; }
                    // Directional: skip FAR trash that lies BACKWARD (farther from the
                    // boss than the tank) — chasing it walks the tank away from the
                    // boss. Close 30y trash above is exempt (already on us).
                    if (adv_have_boss)
                    {
                        const float cbx = adv_boss_x - u.x, cby = adv_boss_y - u.y;
                        const float tbx = adv_boss_x - bx_adv, tby = adv_boss_y - by_adv;
                        if (cbx*cbx + cby*cby > tbx*tbx + tby*tby + 15.0f*15.0f) continue;
                    }
                    if (d2 < far_d2) { far_d2 = d2; far_target = &u; }
                }
                // route armed: the route follower owns OOC navigation — the
                // direct step diverges exactly where routes exist (WC
                // 2026-07-03); fall through (toward wide-scan / boss-as-
                // destination below, which stay ungated as the fallbacks).
                if (!close_target && far_target && !DungeonRouteArmed(s, ai, advice))
                {
                    // Walk to a point ~25y from the target so we end up in
                    // pull range without overshooting into the pack.
                    const float dx = far_target->x - bx_adv;
                    const float dy = far_target->y - by_adv;
                    const float len = std::sqrt(dx*dx + dy*dy);
                    if (len > 0.5f)
                    {
                        const float stop_at =
                            std::min(std::max(0.0f, len - 25.0f), kMaxAdvanceStep);
                        const float scale = stop_at / len;
                        // Use the tank's own Z for the intermediate hop, not
                        // the target's Z. The target may be on a different
                        // level (Deadmines ship deck at z=302 vs entrance
                        // z=62); passing target-Z causes the pathfinder to
                        // snap the destination to the wrong floor's navmesh
                        // poly → INCOMPLETE → move blocked. The tank's Z
                        // gives a valid ground-level hop; the pathfinder
                        // naturally routes up/down ramps on later hops.
                        const float advx = bx_adv + dx * scale;
                        const float advy = by_adv + dy * scale;
                        const float advz = bz_adv;
                        if (DungeonStepAlreadyInFlight(s, advx, advy, advz))
                        {
                            DungeonStepHoldDiag(s, "idle:dungeon_tank_advance",
                                                advx, advy, advz);
                            ai.set_last_rule_fired("idle:dungeon_tank_advance_hold");
                            return true;
                        }
                        if (DungeonMoveOwnedElsewhere(s, ai, advx, advy, advz, now_ms,
                                                      "idle:dungeon_tank_advance"))
                        {
                            ai.set_last_rule_fired("idle:dungeon_tank_advance_hold");
                            return true;
                        }
                        // Refused destination: FALL THROUGH past this far-trash
                        // advance (skip the unconditional return below) so the
                        // wide-scan / boss-as-destination / waypoint fallbacks
                        // below — real alternative candidates — get tried this
                        // same tick instead of re-hammering the poisoned spot.
                        if (!DungeonStepRefused(s, ai, advx, advy, advz, now_ms))
                        {
                            if (emit.move_to(advx, advy, advz, /*run=*/true))
                                ai.note_move_commit(s.map_id(), advx, advy, advz, now_ms);
                            ai.set_last_rule_fired("idle:dungeon_tank_advance");
                            return true;
                        }
                        DungeonStepRefusedDiag(s, "idle:dungeon_tank_advance", advx, advy, advz);
                    }
                }
                // A route-owned far_target must not ALSO veto the ladder below.
                // The far-trash advance is gated off while the route is armed
                // (correct — the route owns navigation), but wide-scan,
                // boss-as-destination and waypoint progression all test
                // `!far_target`, so a single un-aggroed mob in the 30-40y band
                // silently disabled every remaining fallback. Combined with the
                // never-cleared armed flag this is what left tanks in
                // idle:dungeon_hold for 11+ minutes after the second-to-last
                // boss (campaign 2026-07-21). Clearing it here restores the
                // fallbacks without re-enabling the trash walk itself.
                // Keyed on CONSUMED, not merely armed. First cut used
                // DungeonRouteArmed() and was wrong twice over: (a) fix 1 makes
                // that false exactly at the consumed hand-off, so the veto it
                // was meant to lift stayed in place, and (b) it therefore fired
                // during NORMAL mid-route travel, letting wide-scan/waypoint
                // fallbacks compete with the follower — Deadmines regressed
                // 6/6 -> 2/6 on the control run (campaign 2026-07-21).
                {
                    const int32_t rc_cur = ai.dungeon_route_wp(s.map_id());
                    if (far_target && rc_cur >= 0 &&
                        ai.route_consumed_idx(s.map_id()) == rc_cur)
                        far_target = nullptr;
                }
                // Wide-scan fallback: snapshot's nearby_enemies is 40y-capped
                // (BotSnapshotBuilder SCAN_RADIUS), so trash 50-150y away is
                // invisible to the rules above. Without this, the tank falls
                // through to coarse progression_waypoints which can leave it
                // walking past clusters that sit off the straight line.
                // Observed 2026-05-15 in Ragefire: 5-point waypoints with
                // 70-315y jumps; tank "wandered in circles" between rooms
                // because vision was empty between waypoint legs and the
                // anchor sanity gate flipped DPS/Healer anchors as the tank
                // crossed 60y from the leader.
                //
                // Scan radius 150y — large enough to find the next room or
                // corridor, small enough that move_to's Detour path-validator
                // can usually reach it. If Detour rejects (NOPATH), nothing
                // is emitted and the waypoint fallback below takes over.
                if (!close_target && !far_target)
                {
                    if (Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                    {
                        constexpr float kTrashScanRadius = 150.0f;
                        std::list<Unit*> hostiles;
                        Trinity::AnyUnfriendlyUnitInObjectRangeCheck chk(
                            self, self, kTrashScanRadius);
                        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
                            searcher(self, hostiles, chk);
                        Cell::VisitAllObjects(self, searcher, kTrashScanRadius);

                        // Pick the NEAREST hostile Detour can actually walk to.
                        // Cell::VisitAllObjects is a 2D GRID scan — it returns
                        // hostiles on other levels (e.g. the Deadmines ship rigging
                        // directly overhead the ground path). Rather than guess with
                        // a z cutoff, sort by distance and take the first target with
                        // a real walkable path; if none is reachable we leave nearest
                        // null and fall through to boss/waypoint progression. The
                        // path test runs only until the first reachable hit.
                        std::vector<std::pair<float, Unit*>> cand;
                        cand.reserve(hostiles.size());
                        for (Unit* u : hostiles)
                        {
                            if (!u || !u->IsAlive()) continue;
                            if (u->IsCritter() || u->IsTotem()) continue;
                            if (u->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
                                continue;
                            if (!self->IsValidAttackTarget(u)) continue;
                            // Skip per-dungeon environmental encounter objects.
                            {
                                bool env_ignored = false;
                                if (Creature const* c = u->ToCreature())
                                    for (uint32_t ie : advice.ignore_entries)
                                        if (ie == c->GetEntry())
                                        { env_ignored = true; break; }
                                if (env_ignored) continue;
                            }
                            // Directional: skip BACKWARD trash (farther from the boss
                            // than the tank). Without this, a tank on the near side of
                            // a chokepoint (boss reach=0 across an offmesh) picks the
                            // nearest reachable hostile — the entrance-side trash 140y
                            // BEHIND it — and wanders away from the boss (Deadmines
                            // Gap-1 NE oscillation to the crime scene, 2026-06-26).
                            if (adv_have_boss)
                            {
                                const float cbx = adv_boss_x - u->GetPositionX();
                                const float cby = adv_boss_y - u->GetPositionY();
                                const float tbx = adv_boss_x - bx_adv;
                                const float tby = adv_boss_y - by_adv;
                                if (cbx*cbx + cby*cby > tbx*tbx + tby*tby + 15.0f*15.0f)
                                    continue;
                            }
                            const float dxu = u->GetPositionX() - bx_adv;
                            const float dyu = u->GetPositionY() - by_adv;
                            const float dzu = u->GetPositionZ() - bz_adv;
                            cand.emplace_back(dxu * dxu + dyu * dyu + dzu * dzu, u);
                        }
                        std::sort(cand.begin(), cand.end(),
                                  [](auto const& a, auto const& b) { return a.first < b.first; });
                        G3D::Vector3 scan_step;
                        Unit* nearest = nullptr;
                        for (auto const& cu : cand)
                        {
                            if (DungeonTargetReachableAndStep(
                                    self,
                                    cu.second->GetPositionX(),
                                    cu.second->GetPositionY(),
                                    cu.second->GetPositionZ(),
                                    kMaxAdvanceStep, scan_step))
                            { nearest = cu.second; break; }
                        }
                        diag_s150_cand  = (int)cand.size();
                        diag_s150_reach = nearest != nullptr;
                        if (!cand.empty())
                            diag_s150_first = cand.front().second->GetEntry();
                        if (nearest)
                        {
                            // Use the navmesh path's own waypoints (scan_step)
                            // rather than a straight-line direction at bz_adv.
                            // The path knows the correct Z at each segment,
                            // so this step naturally handles Z transitions
                            // (entrance ledge → mine floor ramp) without
                            // resolving to the wrong floor's poly.
                            emit.move_to(scan_step.x, scan_step.y, scan_step.z,
                                         /*run=*/true);
                            ai.set_last_rule_fired("idle:dungeon_tank_scan_next");
                            return true;
                        }
                    }
                }
                // Boss-as-destination: if neither close trash, far trash,
                // nor wide-scan trash gave us a target, walk toward the
                // NEXT boss in advice.bosses[] order. The order is
                // authored to match the dungeon's intended progression
                // (TC's instance script encounter order), so walking the
                // list and picking the first LIVE entry preserves the
                // narrative path. Nearest-alive would happily skip
                // Adarogg if the tank's current position were closer to
                // Slagmaw — wrong direction in the Ragefire spiral.
                // Either we meet the boss directly (tank_pull_next /
                // boss-engage fires next tick) or trash aggros along
                // the way — both good outcomes. Owner directive
                // 2026-05-15.
                //
                // Scan radius 500y — covers the diameter of a typical
                // 5-man dungeon. Cost is bounded by Cell::VisitAllObjects
                // walking ~1 grid cell (533y grid) and is only paid when
                // every closer rule above missed. We stop iterating
                // advice.bosses as soon as we find a live one — earlier
                // dead bosses are skipped, later bosses aren't probed.
                //
                // Path is validated by API::move_to (PathGenerator
                // CalculatePath); NOPATH / FARFROMPOLY returns Locked and
                // no move is emitted, so if Detour can't reach the boss
                // the tank stays put rather than walking off-map.
                if (!close_target && !far_target && !advice.bosses.empty())
                {
                    if (Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                    {
                        constexpr float kBossScanRadius = 500.0f;
                        Creature* next_boss = nullptr;
                        // Perf 2026-05-22: was M× Cell::VisitAllObjects
                        // (one per boss entry). 1000 bots × 5Hz × M=5
                        // bosses = 25K 500y grid walks/sec. Replaced
                        // with a single visit + inline entry-set
                        // membership check. Same correctness; M× CPU
                        // win. The match check is hot — pass entries
                        // by reference and break on first hit.
                        struct AliveBossEntryCheck
                        {
                            Player const* origin;
                            std::vector<uint32_t> const& entries;
                            float range;
                            AliveBossEntryCheck(Player const* o, std::vector<uint32_t> const& e, float r)
                                : origin(o), entries(e), range(r) {}
                            bool operator()(Creature* c) const
                            {
                                if (!c || !c->IsAlive()) return false;
                                bool match = false;
                                for (uint32_t e : entries)
                                    if (c->GetEntry() == e) { match = true; break; }
                                if (!match) return false;
                                return origin->IsWithinDistInMap(c, range);
                            }
                        };
                        std::list<Creature*> creatures;
                        AliveBossEntryCheck chk(self, advice.bosses, kBossScanRadius);
                        Trinity::CreatureListSearcher<AliveBossEntryCheck>
                            searcher(self, creatures, chk);
                        Cell::VisitAllObjects(self, searcher, kBossScanRadius);
                        // Pick the EARLIEST boss in progression order (advice.bosses
                        // is authored in the instance's encounter order) that has a
                        // live creature Detour can actually WALK to — NOT
                        // creatures.front(), whose order is spatial/arbitrary and made
                        // the tank path straight to the elevated ship-deck boss. The
                        // reachability test (not a z heuristic) is what makes this
                        // correct on multi-level maps: a deck boss whole-dungeon away
                        // returns INCOMPLETE so we skip it and let waypoint/trash
                        // progression carry the group up the scaffold; once at the
                        // base the ramp-up path is NORMAL and the same boss becomes
                        // the chosen target. Ground bosses spawn-on-approach, so the
                        // earliest reachable live boss is naturally the right one.
                        G3D::Vector3 boss_step;
                        for (uint32_t bentry : advice.bosses)
                        {
                            for (Creature* c : creatures)
                                if (c && c->GetEntry() == bentry &&
                                    DungeonTargetReachableAndStep(
                                        self,
                                        c->GetPositionX(), c->GetPositionY(), c->GetPositionZ(),
                                        kMaxAdvanceStep, boss_step))
                                { next_boss = c; break; }
                            if (next_boss) break;
                        }
                        diag_boss_cand  = (int)creatures.size();
                        diag_boss_reach = next_boss != nullptr;
                        if (next_boss)
                        {
                            emit.move_to(boss_step.x, boss_step.y, boss_step.z,
                                         /*run=*/true);
                            ai.set_last_rule_fired("idle:dungeon_tank_advance_boss");
                            return true;
                        }
                    }
                }

                // Waypoint-driven progression — last-resort fallback for
                // dungeons whose bosses[] is empty / unset (the boss
                // scan above is now the primary cross-room navigator).
                // Walks progression_waypoints[] in order (per-bot index
                // in BotAI::dungeon_waypoint_index). When the tank gets
                // within 10y of the current waypoint, advance the index
                // so the next tick targets the following point.
                if (!close_target && !far_target &&
                    !advice.progression_waypoints.empty() &&
                    ai.dungeon_waypoint_index() < advice.progression_waypoints.size())
                {
                    auto const& wp = advice.progression_waypoints[ai.dungeon_waypoint_index()];
                    const float dxw = wp.x - bx_adv;
                    const float dyw = wp.y - by_adv;
                    const float dzw = wp.z - bz_adv;
                    const float dw  = std::sqrt(dxw*dxw + dyw*dyw + dzw*dzw);
                    if (dw <= 10.0f)
                    {
                        ai.set_dungeon_waypoint_index(uint8(ai.dungeon_waypoint_index() + 1));
                        ai.set_last_rule_fired("idle:dungeon_tank_waypoint_reached");
                        return true;
                    }
                    // Same cohesion leash as the boss/trash advance: step at most
                    // kMaxAdvanceStep toward the waypoint so a far waypoint leg
                    // can't strand the followers. Prefer the off-mesh-aware
                    // navmesh stepping (DungeonTargetReachableAndStep) used by the
                    // boss-navigator and the regroup: a STRAIGHT-LINE scaled step
                    // toward a waypoint on the far side of an off-mesh bridge lands
                    // IN the gap (off-mesh) and wedges, whereas stepping to the
                    // next path WAYPOINT carries the tank ACROSS the bridge (the
                    // Helix waypoint sits past the Deadmines Gap-1 bridge). Falls
                    // back to the straight-line scaled step when the corridor isn't
                    // a clean NORMAL path (open terrain, or a waypoint the strict
                    // step can't yet reach — the unstick ladder then recovers).
                    G3D::Vector3 wstep;
                    Player* wself = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
                    if (wself && DungeonTargetReachableAndStep(
                            wself, wp.x, wp.y, wp.z, kMaxAdvanceStep, wstep))
                        emit.move_to(wstep.x, wstep.y, wstep.z, /*run=*/true);
                    else if (dw > kMaxAdvanceStep)
                    {
                        const float scale = kMaxAdvanceStep / dw;
                        emit.move_to(bx_adv + dxw * scale, by_adv + dyw * scale,
                                     bz_adv + dzw * scale, /*run=*/true);
                    }
                    else
                        emit.move_to(wp.x, wp.y, wp.z, /*run=*/true);
                    ai.set_last_rule_fired("idle:dungeon_tank_waypoint");
                    return true;
                }

                // Every gate passed, every navigator came up empty: 40y
                // snapshot scan, 150y wide scan, 500y boss scan, waypoint
                // list. A tank reaching this line every tick IS the stall —
                // log it (10s throttle) with the source inventory so the
                // missing data layer (bosses[] / waypoints / scan radius)
                // is identifiable from the log alone. close_target present
                // is NOT a stall (deliberate defer to tank_pull_next) —
                // skip the log for that case or it cries wolf every pull.
                if (!close_target && ai.tank_diag_due(now_ms))
                    TC_LOG_INFO("playerbot.v2",
                        "[tank_adv_diag] {} NO TARGET: close={} far={} "
                        "scan150(cand={} first_entry={} reach={}) "
                        "boss_scan(cand={} reach={}) script_bosses={} "
                        "wp={}/{} pos=({:.0f},{:.0f},{:.0f}) map={}",
                        s.name(), close_target != nullptr, far_target != nullptr,
                        diag_s150_cand, diag_s150_first, diag_s150_reach,
                        diag_boss_cand, diag_boss_reach,
                        advice.bosses.size(),
                        uint32(ai.dungeon_waypoint_index()),
                        advice.progression_waypoints.size(),
                        bx_adv, by_adv, bz_adv, s.map_id());
            }
        }

        // Tank guard: when a group member is in an ACTIVE fight (in_combat
        // with a real victim) and the attacking creature is visible in the
        // tank's 40y nearby_enemies, the tank engages it. This is the
        // complement of tank_advance: advance won't march the group FORWARD
        // while someone is fighting, but the tank MUST join the fight —
        // otherwise a trash mob that aggroed the healer runs unopposed.
        // Priority order: healer first (peel threats off the heal-engine),
        // then any DPS. The tank does NOT need group_ready or post-kill pacing
        // to respond to an emergency — the urgency overrides those gates.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank && !pve_off_tank &&
            s.is_in_dungeon() && !s.is_encounter_in_progress() && g.exists())
        {
            auto const* guard_mems = g.members();
            if (guard_mems)
            {
                ObjectGuid guard_target;
                // Returns true and sets guard_target if m is in a real fight
                // and the threat is visible in the tank's nearby_enemies.
                auto try_guard = [&](GroupMemberSummary const& m) -> bool
                {
                    if (!m.in_combat) return false;
                    if (!m.online || m.map_id != s.map_id()) return false;
                    if (m.guid == s.guid()) return false;
                    // Primary: find the mob the member is directly attacking.
                    // Prefer explicit melee victim; fall back to spellcasting
                    // target (healer casting Smite has no melee victim but IS
                    // in a real fight). NOTE: if the member is a healer
                    // self-casting Flash Heal, casting_target == healer.guid,
                    // which won't appear in nearby_enemies — the reverse-lookup
                    // below catches that.
                    ObjectGuid const threat_guid = !m.victim.IsEmpty() ? m.victim :
                        (m.is_casting ? m.casting_target : ObjectGuid::Empty);
                    if (!threat_guid.IsEmpty())
                    {
                        for (auto const& u : s.raw().combat.nearby_enemies)
                        {
                            if (u.guid != threat_guid) continue;
                            if (u.hp <= 0) continue;
                            // No in_los gate: let the combat movement system
                            // route the tank to the target. The old gate caused
                            // the tank to never peel when dungeon geometry
                            // blocked LoS (Deadmines harbor columns). A tank
                            // charging around a corner IS correct behavior.
                            guard_target = u.guid;
                            return true;
                        }
                    }
                    // Reverse lookup: find any nearby enemy targeting this member.
                    // Handles the case where the member is a healer whose
                    // victim/casting_target is themselves (self-Flash-Heal) rather
                    // than the mob that's hitting them. Without this the guard
                    // rule was blind to healer-pulled fights in which the healer
                    // cast-cycles heals on self and never shows a mob victim.
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.victim != m.guid) continue;
                        if (u.hp <= 0) continue;
                        guard_target = u.guid;
                        return true;
                    }
                    return false;
                };
                // First pass: peel threats off the healer
                for (auto const& m : *guard_mems)
                    if (m.role == Role::Healer && try_guard(m)) break;
                // Second pass: assist any DPS
                if (guard_target.IsEmpty())
                    for (auto const& m : *guard_mems)
                        if (m.role != Role::Healer && try_guard(m)) break;
                if (!guard_target.IsEmpty())
                {
                    if (!emit.start_attack(guard_target)) return true;
                    ai.note_engage(guard_target, now_ms);
                    ai.set_last_rule_fired("idle:dungeon_tank_guard_group");
                    return true;
                }
            }
        }

        // IN-COMBAT TAUNT-PEEL (2026-06-29). The OOC tank_guard_group above is
        // gated `!s.in_combat()`, so once the tank is ALREADY tanking one pack it
        // can NEVER rip a second pack off the healer. On the Deadmines deck this is
        // the healer-kill mechanism: multiple e48522 packs (25k-HP melee) spawn at
        // once; the tank holds 2-3, the spillover wanders onto the healer and melees
        // it to death (diag 2026-06-29: `e48522 ... d4 v=ME` on the HEAL line, healer
        // 36% then dead → it is the SOLE rezzer, so its death cascades the group).
        // Healer survival is the gate to the deck. Fix: a tank IN combat TAUNTS a mob
        // whose victim is a non-tank member — healer FIRST (sole rezzer), then any
        // DPS — forcing it onto the tank where the group's AoE/cleave kills it. The
        // taunt's own cooldown (is_ready) throttles this to one mob per peel, so it
        // cannot thrash; a 2-tank group's off-tank is role Tank and so is skipped
        // (it WANTS its mobs). General tank competence — not instance-specific — so
        // it runs in every dungeon, scoped tightly to tank+dungeon+combat so it
        // cannot misfire elsewhere. Single-target taunts only (ClassTaunt); a tank
        // class with no taunt (none in the supported set) is a no-op.
        if (s.is_alive() && !s.is_casting() && s.in_combat() &&
            ai.effective_role(s) == Role::Tank && !pve_off_tank &&
            s.is_in_dungeon() && g.exists())
        {
            const uint32 taunt = ClassTaunt(s.cls());
            if (taunt && s.knows_spell(taunt) && s.is_ready(taunt))
            {
                auto const* peel_mems = g.members();
                // Build the set of non-tank member guids (healer flagged) so we can
                // match a mob's victim against them in one nearby_enemies pass.
                ObjectGuid healer_guid, peel_target;
                if (peel_mems)
                    for (auto const& m : *peel_mems)
                        if (m.role == Role::Healer && m.online &&
                            m.map_id == s.map_id() && m.is_alive)
                        { healer_guid = m.guid; break; }
                float tpx, tpy, tpz; s.position(tpx, tpy, tpz);
                // Pass 0 = a mob meleeing the HEALER, pass 1 = a mob on any non-tank
                // member. Never taunt a mob already attacking the tank (us) — that is
                // wasted GCD + CD. Range-cap to 30y so we don't taunt across the map.
                auto find_peel = [&](bool healer_only) -> ObjectGuid
                {
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.hp <= 0 || u.untargetable || u.is_pacified) continue;
                        if (u.victim.IsEmpty() || u.victim == s.guid()) continue;
                        bool is_healer_v = !healer_guid.IsEmpty() && u.victim == healer_guid;
                        if (healer_only && !is_healer_v) continue;
                        if (!healer_only)
                        {
                            // Victim must be a non-tank GROUP member.
                            bool member_v = false;
                            if (peel_mems)
                                for (auto const& m : *peel_mems)
                                    if (m.guid == u.victim && m.role != Role::Tank)
                                    { member_v = true; break; }
                            if (!member_v) continue;
                        }
                        const float ex = u.x - tpx, ey = u.y - tpy, ez = u.z - tpz;
                        if (ex*ex + ey*ey + ez*ez > 30.0f * 30.0f) continue;
                        return u.guid;
                    }
                    return ObjectGuid();
                };
                peel_target = find_peel(/*healer_only=*/true);
                if (peel_target.IsEmpty())
                    peel_target = find_peel(/*healer_only=*/false);
                if (!peel_target.IsEmpty())
                {
                    emit.cast(taunt, peel_target);
                    ai.set_last_rule_fired("idle:dungeon_taunt_peel");
                    return true;
                }
            }
        }

        // PROACTIVE FOCUS-ASSIST in the tight-engagement zone (script-driven;
        // DungeonAdvice::tight_engage_below_z). In the zone the tank's combat with
        // event-spawned packs is TRANSIENT — they burst the tank then leash/reset
        // in 2-3s — so the standard dungeon_dps_assist below, which requires a
        // group member to be in_combat AT THE SAMPLED TICK, almost never catches a
        // valid window and the tank fights/dies SOLO (live 2026-06-28: dps_assist
        // fired 0x across a whole run while all 3 DPS held at 100% HP and the tank
        // died 37x to the Deadmines harbor Shadowguard burst — diag confirmed it
        // is the combat, NOT drowning). Here a cohered DPS OPENS on the TANK's
        // target REGARDLESS of the tank's in_combat flag, so the casters are
        // focus-killed (high_priority_kill) instead of left on the tank. Gated to
        // the zone + cohesion (don't run a lone DPS into the pack ahead of the
        // tank); pre-emptive CC is already suppressed in the zone (focus_kill_zone)
        // so this is the intended engage.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Dps &&
            s.is_in_dungeon() && !s.is_encounter_in_progress() &&
            advice.tight_engage_below_z > 0.0f && g.exists())
        {
            float dpx, dpy, dpz; s.position(dpx, dpy, dpz);
            GroupMemberSummary const* dtk = g.tank();
            bool cohered = false;
            if (dpz < advice.tight_engage_below_z &&
                dtk && dtk->online && dtk->map_id == s.map_id())
            {
                const float tdx = dtk->x - dpx, tdy = dtk->y - dpy,
                            tdz = dtk->z - dpz;
                cohered = (tdx*tdx + tdy*tdy + tdz*tdz) <= 30.0f * 30.0f;
            }
            if (cohered)
            {
                // Focus-fire the TANK's actual target, not just the nearest mob:
                // (1) the group skull focus-kill mark, (2) the tank's current
                // victim (what it is swinging at), (3) the tank's cast target,
                // then (4) fall back to whatever mob is ATTACKING the tank (its
                // victim == tank) — covers the harbor desync where the tank has
                // attackers but an empty victim. A nearest-mob pick could open a
                // FRESH pack or scatter the DPS onto the wrong caster; assisting
                // the tank's target keeps the group focused on one kill at a time.
                auto resolve = [&](ObjectGuid want) -> ObjectGuid
                {
                    if (want.IsEmpty()) return ObjectGuid();
                    // Shield consult (final-review fix, 2026-07-03): never resolve
                    // to a guid this bot already gate-skipped and shielded via
                    // note_engage() below — otherwise this loop re-picks (and the
                    // gate below re-probes with a full pathfind) the same target
                    // every tick until the shield window lapses.
                    if (ai.engage_shielded(want, now_ms)) return ObjectGuid();
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.guid != want) continue;
                        if (u.hp <= 0 || u.untargetable || u.is_pacified ||
                            u.no_xp_kill) return ObjectGuid();
                        const float ex = u.x - dpx, ey = u.y - dpy, ez = u.z - dpz;
                        if (ex*ex + ey*ey + ez*ez > 40.0f * 40.0f)
                            return ObjectGuid();   // out of engage range
                        return want;
                    }
                    return ObjectGuid();   // not visible to this DPS
                };
                ObjectGuid pick = resolve(g.skull_target());
                if (pick.IsEmpty()) pick = resolve(dtk->victim);
                if (pick.IsEmpty() && dtk->is_casting)
                    pick = resolve(dtk->casting_target);
                if (pick.IsEmpty())
                {
                    // The tank has no resolvable victim (desync) — assist whatever
                    // is hitting the tank.
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.victim != dtk->guid) continue;
                        if (u.hp <= 0 || u.untargetable || u.is_pacified ||
                            u.no_xp_kill) continue;
                        // Shield consult (final-review fix, 2026-07-03) — see resolve().
                        if (ai.engage_shielded(u.guid, now_ms)) continue;
                        bool ign = false;
                        for (uint32_t ie : advice.ignore_entries)
                            if (ie == u.entry) { ign = true; break; }
                        if (ign) continue;
                        const float ex = u.x - dpx, ey = u.y - dpy, ez = u.z - dpz;
                        if (ex*ex + ey*ey + ez*ez > 40.0f * 40.0f) continue;
                        pick = u.guid; break;
                    }
                }
                // Ranged-pull discipline (2026-07-02, SFK courtyard wedge):
                // do NOT open fire on an enemy the TANK can only reach via a
                // huge detour (or not at all) — one DPS shot locks the whole
                // group into false combat the tank cannot resolve. Decline the
                // pick (as if unresolved) so lower-priority rules can run, and
                // shield the target — resolve() above now consults the shield
                // too (final-review fix, 2026-07-03), so a gated candidate is
                // re-probed at most once per 15s shield window instead of every
                // tick. The gate fails OPEN (uncomputed verdict never blocks a
                // pull).
                if (Services::Config().pull_gate_enabled())
                {
                    if (!pick.IsEmpty() && dtk->online && dtk->is_alive && dtk->map_id == s.map_id())
                        if (Player* tkp = ObjectAccessor::FindConnectedPlayer(dtk->guid))
                            for (auto const& u : s.raw().combat.nearby_enemies)
                            {
                                if (u.guid != pick) continue;
                                DungeonDetourVerdict const dv =
                                    DungeonTankDetour(tkp, u.x, u.y, u.z);
                                if (DungeonDetourExcessive(dv))
                                {
                                    ai.note_engage(pick, s.published_at_ms(), 15000u);
                                    static uint32 s_pullgate_dbg_focus_ms = 0;
                                    uint32 const dnow = s.published_at_ms();
                                    if (dnow - s_pullgate_dbg_focus_ms > 1500u)
                                    {
                                        s_pullgate_dbg_focus_ms = dnow;
                                        TC_LOG_INFO("playerbot.v2",
                                            "[pull_gate] SKIP rule=focus_assist bot={} target={} "
                                            "beeline={:.1f} path={:.1f} ratio={:.2f} complete={}",
                                            s.bot_id(), pick.ToString(), dv.beeline,
                                            dv.path_len, dv.ratio, dv.complete ? 1 : 0);
                                    }
                                    pick = ObjectGuid();   // decline: let lower-priority rules run
                                }
                                break;
                            }
                }
                else
                {
                    // [detour] READ-ONLY stage (Task 1): measure, log, never block.
                    // Kept alive so diagnosis survives the kill switch.
                    if (!pick.IsEmpty() && dtk->online && dtk->is_alive && dtk->map_id == s.map_id())
                        if (Player* tkp = ObjectAccessor::FindConnectedPlayer(dtk->guid))
                            for (auto const& u : s.raw().combat.nearby_enemies)
                            {
                                if (u.guid != pick) continue;
                                DungeonDetourVerdict const dv =
                                    DungeonTankDetour(tkp, u.x, u.y, u.z);
                                static uint32 s_detour_dbg_focus_ms = 0;
                                uint32 const dnow = s.published_at_ms();
                                if (dv.computed && dnow - s_detour_dbg_focus_ms > 1500u)
                                {
                                    s_detour_dbg_focus_ms = dnow;
                                    TC_LOG_INFO("playerbot.v2",
                                        "[detour] rule=focus_assist bot={} target={} beeline={:.1f} "
                                        "path={:.1f} ratio={:.2f} complete={} excessive={}",
                                        s.name(), pick.ToString(), dv.beeline, dv.path_len,
                                        dv.ratio, dv.complete ? 1 : 0,
                                        DungeonDetourExcessive(dv) ? 1 : 0);
                                }
                                break;
                            }
                }
                if (!pick.IsEmpty() && emit.start_attack(pick))
                {
                    ai.note_engage(pick, now_ms);
                    ai.set_last_rule_fired("idle:dungeon_focus_assist");
                    return true;
                }
            }
        }

        // DPS/healer dungeon assist — non-tank bots engage mobs that are
        // actively attacking a group member. Without this, DPS bots idle in
        // follow_recall while the healer fights alone (observed live: Dungrogue
        // and Dungmage followed the tank for 10+ minutes while Dunghealer
        // fought a harbor mob at 40% HP). The tank has tank_guard_group (peel)
        // above; this gives DPS the complementary "pile on" role so the mob dies
        // fast rather than grinding the healer down.
        //
        // Condition: out of combat, not casting, not an encounter boss fight,
        // any group member in combat AND a mob targeting that member is visible.
        // No LoS gate — let the combat movement system route to the target.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Dps &&
            s.is_in_dungeon() && !s.is_encounter_in_progress() && g.exists())
        {
            if (auto const* assist_mems = g.members())
            {
                for (auto const& m : *assist_mems)
                {
                    if (!m.in_combat) continue;
                    if (!m.online || m.map_id != s.map_id()) continue;
                    if (m.guid == s.guid()) continue;
                    for (auto const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.victim != m.guid) continue;
                        if (u.hp <= 0) continue;
                        // Skip environmental encounter objects (fire platters etc.)
                        {
                            bool env_ignored = false;
                            for (uint32_t ie : advice.ignore_entries)
                                if (ie == u.entry) { env_ignored = true; break; }
                            if (env_ignored) continue;
                        }
                        // Shield consult (final-review fix, 2026-07-03): skip a
                        // candidate this bot already gate-skipped and shielded via
                        // note_engage() below, BEFORE the DungeonTankDetour probe —
                        // otherwise this loop re-picks (and re-probes with a full
                        // pathfind) the same target every tick until the shield
                        // window lapses.
                        if (ai.engage_shielded(u.guid, now_ms)) continue;
                        // Ranged-pull discipline (2026-07-02, SFK courtyard wedge):
                        // do NOT open fire on an enemy the TANK can only reach via a
                        // huge detour (or not at all) — one DPS shot locks the whole
                        // group into false combat the tank cannot resolve. Skip the
                        // target and shield it — the check above now consults the
                        // shield too, so a gated target is re-probed at most once
                        // per 15s shield window instead of every tick. The gate
                        // fails OPEN (uncomputed verdict never blocks a pull).
                        if (Services::Config().pull_gate_enabled())
                        {
                            bool gated = false;
                            if (GroupMemberSummary const* tk2 = g.tank())
                                if (tk2->online && tk2->is_alive && tk2->map_id == s.map_id())
                                    if (Player* tkp = ObjectAccessor::FindConnectedPlayer(tk2->guid))
                                    {
                                        DungeonDetourVerdict const dv =
                                            DungeonTankDetour(tkp, u.x, u.y, u.z);
                                        if (DungeonDetourExcessive(dv))
                                        {
                                            ai.note_engage(u.guid, s.published_at_ms(), 15000u);
                                            static uint32 s_pullgate_dbg_ms = 0;
                                            uint32 const dnow = s.published_at_ms();
                                            if (dnow - s_pullgate_dbg_ms > 1500u)
                                            {
                                                s_pullgate_dbg_ms = dnow;
                                                TC_LOG_INFO("playerbot.v2",
                                                    "[pull_gate] SKIP rule=dps_assist bot={} target={} "
                                                    "beeline={:.1f} path={:.1f} ratio={:.2f} complete={}",
                                                    s.bot_id(), u.guid.ToString(), dv.beeline,
                                                    dv.path_len, dv.ratio, dv.complete ? 1 : 0);
                                            }
                                            gated = true;
                                        }
                                    }
                            if (gated) continue;   // next candidate enemy
                        }
                        else
                        {
                            // [detour] READ-ONLY stage (Task 1): measure, log, never
                            // block. Kept alive so diagnosis survives the kill switch.
                            if (GroupMemberSummary const* tk2 = g.tank())
                                if (tk2->online && tk2->is_alive && tk2->map_id == s.map_id())
                                    if (Player* tkp = ObjectAccessor::FindConnectedPlayer(tk2->guid))
                                    {
                                        DungeonDetourVerdict const dv =
                                            DungeonTankDetour(tkp, u.x, u.y, u.z);
                                        static uint32 s_detour_dbg_dps_ms = 0;
                                        uint32 const dnow = s.published_at_ms();
                                        if (dv.computed && dnow - s_detour_dbg_dps_ms > 1500u)
                                        {
                                            s_detour_dbg_dps_ms = dnow;
                                            TC_LOG_INFO("playerbot.v2",
                                                "[detour] rule=dps_assist bot={} target={} beeline={:.1f} "
                                                "path={:.1f} ratio={:.2f} complete={} excessive={}",
                                                s.name(), u.guid.ToString(), dv.beeline, dv.path_len,
                                                dv.ratio, dv.complete ? 1 : 0,
                                                DungeonDetourExcessive(dv) ? 1 : 0);
                                        }
                                    }
                        }
                        if (!emit.start_attack(u.guid)) return true;
                        ai.note_engage(u.guid, now_ms);
                        ai.set_last_rule_fired("idle:dungeon_dps_assist");
                        return true;
                    }
                }
            }
        }

        // Tank pull next — fires when:
        //   * Bot is tank-spec.
        //   * Bot has no current victim, not in combat.
        //   * No group member is in combat (don't pull during a fight).
        //   * No encounter currently in progress.
        //   * 5+ seconds since last kill (healer mana regen).
        //   * At least one unengaged enemy in range (not someone's
        //     victim already).
        // Pull preference: closest non-boss, then non-elite. Boss
        // pulls go through `idle:dungeon_engage_boss` below which
        // wraps a full encounter-context check.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            // PvE-coordinator gate: only the designated MAIN tank starts
            // pulls (raid off-tanks follow + taunt-swap; a 5-man's
            // second tank-spec member acts as DPS). See pve_off_tank
            // above for the rationale.
            !(s.raw().pve_order.active &&
              !s.raw().pve_order.main_tank.IsEmpty() &&
              s.raw().pve_order.main_tank != s.guid()) &&
            !s.is_encounter_in_progress() &&
            // 2500 (was 5000) — matches the tank_advance pacing; the
            // flap-resistant note_kill (≥1.5s fights only) makes the
            // shorter window safe.
            now_ms - ai.last_kill_ms() >= 2500 &&
            // Chat pause cue ("wait"/"hold"/"stop" in /p) suppresses
            // voluntary tank pulls. Cleared by "go"/"pull" or by the
            // 12s timeout. Doesn't affect reactive combat — if a mob
            // aggros the tank anyway, State_InCombat takes over.
            now_ms >= ai.chat_pause_until_ms() &&
            // SPECIAL phase gate: when any encounter in the current
            // instance is in EncounterState::SPECIAL (Razuvious MC,
            // Heigan dance, Sindragosa air phase, etc.), suppress NEW
            // pulls — phase-aware idle rules and class APLs handle the
            // movement/dispel/positioning during these sub-phases.
            // Reactive combat is unaffected.
            !s.raw().dungeon_exec.any_boss_in_special)
        {
            bool any_member_in_combat = false;
            // Group-ready check — real tanks wait for the group to
            // assemble + heal up + drink before charging the next pack.
            // Without this, a freshly teleported tank walks ahead of a
            // still-zoning healer, pulls solo, and dies (observed by
            // owner 2026-05-15). Required state:
            //   * ≥2/3 of living same-map members within 25y of tank
            //   * Every living member ≥70% HP
            //   * Every Healer member's mana ≥60%
            // Solo bots (no group) bypass — soloable tanks should still
            // act autonomously.
            bool group_ready = true;
            if (g.exists())
            {
                if (auto const* mems = g.members())
                {
                    float tx_grp, ty_grp, tz_grp;
                    s.position(tx_grp, ty_grp, tz_grp);
                    uint32 alive_mems = 0;
                    uint32 within_range = 0;
                    bool any_low_hp = false;
                    bool any_low_mana = false;
                    // Healer-paced readiness (2026-06-11, aligned with the
                    // tank_advance gate rework): the HEALER must be near
                    // (40y) and mana'd; non-healers only need half within
                    // 50y — one member (often the HUMAN) poking a side room
                    // must not freeze the pull cycle while everyone anchors
                    // on the tank waiting.
                    for (auto const& m : *mems)
                    {
                        if (m.in_combat) any_member_in_combat = true;
                        if (!m.online) continue;
                        if (m.map_id != s.map_id()) continue;
                        if (m.guid == s.raw().guid) continue;
                        if (!m.is_alive) { group_ready = false; continue; }
                        const float dx = m.x - tx_grp;
                        const float dy = m.y - ty_grp;
                        const float d2 = dx * dx + dy * dy;
                        if (m.role == Role::Healer)
                        {
                            if (d2 > 40.0f * 40.0f)
                                group_ready = false;
                            if (m.max_mana > 0 &&
                                int64(m.mana) * 100 < int64(m.max_mana) * 50)
                                any_low_mana = true;
                        }
                        else
                        {
                            ++alive_mems;
                            if (d2 <= 50.0f * 50.0f) ++within_range;
                        }
                        if (m.max_hp > 0 &&
                            int64(m.hp) * 100 < int64(m.max_hp) * 70)
                            any_low_hp = true;
                    }
                    // Need at least half the non-healers within range.
                    if (alive_mems > 0 && within_range * 2 < alive_mems)
                        group_ready = false;
                    if (any_low_hp || any_low_mana)
                        group_ready = false;
                }
            }
            if (!any_member_in_combat && group_ready)
            {
                NearbyUnit const* best = nullptr;
                float bestSq = std::numeric_limits<float>::max();
                bool best_is_priority = false;
                float bx, by, bz; s.position(bx, by, bz);
                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0) continue;
                    if (!u.victim.IsEmpty()) continue;          // already engaged by someone
                    if (u.is_dungeon_boss) continue;            // bosses go through engage_boss
                    // Environmental/unkillable mobs (scene props, Mining Powder
                    // entry 48284, training dummies) have no_xp_kill or PACIFIED.
                    // They appear in the enemy scan but can't be fought — pulling
                    // them causes an opener_give_up loop that pins the tank for
                    // minutes (observed: 11-min loop, Deadmines mine area).
                    if (u.no_xp_kill || u.is_pacified || u.untargetable) continue;
                    // Per-dungeon environmental object ignore list (mirrors the
                    // same check in the advance close/far scan above).
                    {
                        bool env_ignored = false;
                        for (uint32_t ie : advice.ignore_entries)
                            if (ie == u.entry) { env_ignored = true; break; }
                        if (env_ignored) continue;
                    }
                    // Engage-shield: opener_give_up sets a 5-min block on the
                    // victim's GUID. Without this check, tank_pull_next re-selects
                    // the same unreachable mob on the very next tick (stop_attack
                    // clears the combat path but note_engage is single-target and
                    // not consulted here). Checking the shield prevents the loop.
                    if (u.guid == ai.last_engage_target() &&
                        now_ms - ai.last_engage_at_ms() < ai.last_engage_shield_ms()) continue;
                    // Skip targets with a live start_attack ServerRefused lockout
                    // (immune / phased / faction-locked at Player::Attack level).
                    // start_attack() will return false for this target for 30s
                    // anyway; skipping here prevents selecting it as `best` and
                    // hitting the failed-start_attack return-false path every tick.
                    if (ai.start_attack_recently_refused(u.guid.GetCounter(), now_ms)) continue;
                    // Real tanks pull what they can SEE. The 40y enemy scan
                    // returns mobs through dungeon walls; opening on one
                    // wedges the whole party into "not in line of sight"
                    // cast errors while the tank swings at masonry (user
                    // report 2026-06-11, Deadmines). The pack behind the
                    // wall gets pulled normally once the corridor advance
                    // brings it into view.
                    if (!u.in_los) continue;
                    // CC-respect: don't break friendly CC by pulling
                    // a poly'd / sapped / sheeped mob. is_cc_by_ally is
                    // declared later in the dispatch; inline the check
                    // here so tank-pull respects the same constraint
                    // as later engagement rules. Creature-cast CC
                    // (mob CC'ing mob — rare but exists) falls through
                    // because cc_caster won't be in nearby_friends.
                    if (u.is_cc_locked && !u.cc_caster.IsEmpty())
                    {
                        bool cc_by_ally = (u.cc_caster == s.guid());
                        if (!cc_by_ally)
                            for (auto const& f : s.raw().combat.nearby_friends)
                                if (f.guid == u.cc_caster) { cc_by_ally = true; break; }
                        if (cc_by_ally) continue;
                    }
                    const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq > 30.0f * 30.0f) continue;          // 30y pull range
                    // Per-dungeon priority list — promote priority
                    // entries above range-only sort so the script's
                    // "kill the healer first" advice holds even when
                    // a melee add is closer.
                    bool is_priority = false;
                    for (uint32_t e : advice.high_priority_kill_entries)
                        if (e == u.entry) { is_priority = true; break; }
                    if (best_is_priority && !is_priority) continue;
                    if (is_priority && !best_is_priority)
                    { bestSq = dsq; best = &u; best_is_priority = true; continue; }
                    if (dsq < bestSq) { bestSq = dsq; best = &u; }
                }
                if (best)
                {
                    // Pull-separately gate (M+ Bolstering, certain raid
                    // adds that bolster each other on kill): if the chosen
                    // target's entry is flagged, refuse to pull when ANY
                    // other live mob of the same entry sits within ~15y of
                    // the target — tank must single-pull these so the
                    // bolster stack doesn't compound. Skipping the pull
                    // (return early without firing the rule) defers to
                    // healer/quest-kill rules until a peel opens.
                    if (!advice.pull_separately_entries.empty() ||
                        !advice.pull_separately_auras.empty())
                    {
                        bool flagged = false;
                        for (uint32_t e : advice.pull_separately_entries)
                            if (e == best->entry) { flagged = true; break; }
                        // Aura-based flag — Bolstering and similar affixes
                        // apply at runtime to whichever mobs happen to be
                        // near. Match against the candidate's affix_buffs
                        // (captured by the snapshot builder from a small
                        // whitelist).
                        if (!flagged && !advice.pull_separately_auras.empty())
                        {
                            for (uint32_t want : advice.pull_separately_auras)
                            {
                                for (uint32_t have : best->affix_buffs)
                                    if (have == want) { flagged = true; break; }
                                if (flagged) break;
                            }
                        }
                        if (flagged)
                        {
                            int sibling_count = 0;
                            for (auto const& u : s.raw().combat.nearby_enemies)
                            {
                                if (&u == best) continue;
                                if (u.hp <= 0) continue;
                                if (u.entry != best->entry) continue;
                                const float dx2 = u.x - best->x;
                                const float dy2 = u.y - best->y;
                                const float dz2 = u.z - best->z;
                                if (dx2*dx2 + dy2*dy2 + dz2*dz2 <= 15.0f * 15.0f)
                                    ++sibling_count;
                            }
                            if (sibling_count > 0)
                            {
                                ai.set_last_rule_fired("idle:tank_pull_skipped_bolstering");
                                return true;
                            }
                        }
                    }
                    // Lockout-gate: refused StartAttack (immune / phased /
                    // or stale snapshot) must NOT claim the dispatch slot.
                    // Return false — the start_attack_recently_refused skip
                    // in the selection loop above prevents re-selecting this
                    // target next tick, and tank_advance (which also checks
                    // start_attack_recently_refused) can now take over.
                    if (!emit.start_attack(best->guid))
                        return false;
                    ai.note_engage(best->guid, now_ms);
                    // Dungeon pull callout — real tanks announce pulls so
                    // the group times their burst CDs / standby buffs. The
                    // BgCalloutCoordinator dedups so multi-tank groups
                    // (2-tank raids leaking into dungeon mode) don't both
                    // shout. Per-(target-entry) lockout key so a single
                    // tank's chain-pull of the same pack stays quiet, but
                    // a NEW pack/pull does re-announce. Personality-gated:
                    // Silent/Terse stay quiet.
                    if (ai.personality().verbosity != Verbosity::Silent &&
                        ai.personality().verbosity != Verbosity::Terse)
                    {
                        const uint64 pull_key = (uint64(10) << 32) | uint64(best->entry);
                        // 10-second lockout — pull pacing is on similar timing
                        // to that of the BG inc callout (20s) but a bit tighter
                        // because dungeon pulls cluster faster than node fights.
                        if (BgCalloutCoordinator::TryClaim(/*kind=*/10, pull_key,
                                                            now_ms, /*lockout=*/10u * 1000u))
                        {
                            // Phrase pool — short, neutral, "pulling" is the
                            // canonical raid-leader callout. Mix in a few
                            // variants so multi-pull runs don't read robotic.
                            char const* phrases[] = {
                                "pulling", "pull", "incoming pull", "go",
                            };
                            const uint32 sel = uint32(now_ms ^ uint32(best->entry)) %
                                               (sizeof(phrases) / sizeof(phrases[0]));
                            emit.say(phrases[sel]);
                        }
                    }
                    ai.set_last_rule_fired("idle:tank_pull_next");
                    return true;
                }
            }
        }

        // [BUG G-P0a] melee behind-positioning body moved into the shared
        // DungeonCombatPositioning() helper (called early in this dispatcher
        // and from DispatchInCombat). The previous inline `s.in_combat()`
        // gate was dropped there because the combat path is the primary
        // caller; the idle call still only repositions when the bot has a
        // boss-tier victim, so an out-of-combat idle tick naturally no-ops.

        // PvE-coordinator off-tank follow: the second tank shadows the
        // main tank between pulls (8y — close enough to taunt-swap or
        // pick up adds instantly, far enough not to body-block). This is
        // the positive half of the off-tank contract; the gates above
        // are the negative half (never start pulls/advances/boss opens).
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            s.raw().pve_order.active &&
            s.raw().pve_order.tank_duty == 2 &&
            !s.raw().pve_order.main_tank.IsEmpty() &&
            s.raw().pve_order.main_tank != s.guid() &&
            g.exists())
        {
            if (auto const* mems = g.members())
                for (auto const& m : *mems)
                {
                    if (m.guid != s.raw().pve_order.main_tank) continue;
                    if (!m.online || !m.is_alive ||
                        m.map_id != s.map_id())
                        break;
                    float ox, oy, oz;
                    s.position(ox, oy, oz);
                    const float dxo = m.x - ox, dyo = m.y - oy;
                    if (dxo * dxo + dyo * dyo > 15.0f * 15.0f)
                    {
                        emit.follow(s.raw().pve_order.main_tank, 8.0f);
                        ai.set_last_rule_fired("idle:dungeon_offtank_follow");
                        return true;
                    }
                    break;
                }
        }

        // Boss engagement — tank-only; pulls a IsDungeonBoss creature
        // when conditions allow. Encounter is initiated by the
        // start_attack; server's BossAI does the rest. Only the
        // designated main tank opens the boss (same rationale as the
        // tank_pull / advance gates).
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            ai.effective_role(s) == Role::Tank &&
            !(s.raw().pve_order.active &&
              !s.raw().pve_order.main_tank.IsEmpty() &&
              s.raw().pve_order.main_tank != s.guid()) &&
            s.has_visible_boss() &&
            !s.is_encounter_in_progress())
        {
            // Lockout-gate so a refused boss-engage doesn't claim
            // dispatch every tick. note_engage on success aligns this
            // with idle:tank_pull_next and idle:quest_kill.
            if (!emit.start_attack(s.current_boss_guid())) return false;
            ai.note_engage(s.current_boss_guid(), now_ms);
            ai.set_last_rule_fired("idle:dungeon_engage_boss");
            return true;
        }

        // ── Dungeon cohesion floor + hard-stop ──────────────────────────
        // Non-tanks: actively follow the tank when out of cohesion (>22y).
        // All alive bots: hard-stop here to block world-idle navigation from
        // running inside an instance. Quest goals, vendor runs, and flight
        // paths must not run while dungeon_active — observed live (Deadmines
        // entrance): DPS bots navigated toward quest waypoints, failed to
        // path, and path-escape crawl backed them into the dungeon exit
        // areatrigger, teleporting the DPS roster out to Westfall with 0/6
        // bosses done. The deliberate-fall-through for "between-pack equip/
        // loot realism" is removed; add explicit dungeon-safe idle actions
        // above this point if that realism is re-introduced.
        if (s.is_alive())
        {
            if (ai.effective_role(s) != Role::Tank)
            {
                if (GroupMemberSummary const* tk = g.tank())
                {
                    if (tk->online && tk->is_alive && tk->guid != s.guid() &&
                        tk->map_id == s.map_id())
                    {
                        // Off-mesh recovery FIRST: a follower stranded in a bridge
                        // gap (Gap-1 z51 hole) can't follow the tank by any path —
                        // nudge it back onto the navmesh before the cohesion logic,
                        // else it wedges and stalls the group-ready advance gate.
                        if (Player* rself =
                                ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                            if (DungeonNudgeOntoMesh(rself, emit, ai))
                                return true;
                        float sx, sy, sz; s.position(sx, sy, sz);
                        const float ddx = tk->x - sx, ddy = tk->y - sy, ddz = tk->z - sz;
                        // Tighter regroup radius inside the tight-engagement zone
                        // (DungeonAdvice::tight_engage_below_z — e.g. the Deadmines
                        // harbor floor at z<30). The tight advance gate
                        // (compute_group_ready) requires the healer ≤18y, but the
                        // default 22y regroup trigger leaves an 18-22y DEAD BAND: a
                        // follower there neither lets the tank advance (gate fails) nor
                        // closes the gap (regroup holds ≤22y) → the tank holds FOREVER
                        // (live 2026-06-29: healer parked at 18.1y, group_ready=false, 0
                        // deaths but 0 progress to Ripsnarl — the navmesh chain to the
                        // deck was fully reachable, the group just never balled up tight
                        // enough to commit). A 14y regroup in the zone pulls followers
                        // WELL inside the 18y gate so the advance always clears; CC is
                        // already suppressed in the zone (focus_kill_zone) so the 22y
                        // cc_cohered coupling doesn't apply.
                        const bool tight_regroup_zone =
                            advice.tight_engage_below_z > 0.0f &&
                            sz < advice.tight_engage_below_z;
                        const float regroup_trigger = tight_regroup_zone ? 14.0f : 22.0f;
                        if (ddx * ddx + ddy * ddy + ddz * ddz >
                            regroup_trigger * regroup_trigger)
                        {
                            // Use move_to (not follow) to regroup toward the tank.
                            // API::move_to uses the advancing-partial-path logic:
                            // even when the destination is INCOMPLETE (navmesh dead-end,
                            // ledge drop, z-disconnected corridor), it follows the
                            // best-available sub-path, incrementally closing the gap.
                            // emit.follow (MoveFollow) has no such fallback — when the
                            // FollowMovementGenerator's path fails it silently stops,
                            // leaving is_moving=true (MotionMaster active) even though
                            // the bot is motionless.
                            //
                            // Convergence-aware re-issue: the old gate re-issued
                            // ONLY when !is_moving, which a follower being driven
                            // the WRONG way defeats — a stale MotionMaster generator
                            // (e.g. a destination that survived a teleport) keeps
                            // is_moving=true forever, so the corrective move_to never
                            // fired and the bot crawled away indefinitely (observed:
                            // a healer walking 5000y toward a prior-map staging
                            // coordinate on the dungeon map). Re-issue when stationary
                            // OR when moving yet NOT closing the gap to the tank over a
                            // full sample window. A normally-converging path (dist²
                            // shrinking past the epsilon) is left intact so the spline
                            // isn't reset mid-route; the move_to dedup (3y / 1.5s) is a
                            // second thrash guard.
                            const float  rdist2  = ddx * ddx + ddy * ddy + ddz * ddz;
                            const bool   rmoving = s.raw().movement.is_moving;
                            const uint32 rprev   = ai.last_regroup_ms();
                            const bool   rsampled =
                                (rprev != 0) && (now_ms - rprev) >= BotAI::kRegroupSampleMs;
                            bool reissue   = !rmoving;
                            bool diverging = false;
                            if (rmoving && rsampled &&
                                rdist2 >= ai.last_regroup_dist2() - BotAI::kRegroupDivergeEps2)
                            { reissue = true; diverging = true; }
                            // Refresh the convergence baseline on the first fire and
                            // at the end of each judging interval.
                            if (reissue || rprev == 0 || rsampled)
                                ai.note_regroup_sample(rdist2, now_ms);
                            if (reissue)
                            {
                                // A diverging follower is driven by a stale mover that
                                // move_to alone can't unseat when the tank is currently
                                // unreachable from here (move_to would be refused and
                                // the old generator keeps running). Clear the generator
                                // first so it at least STOPS fleeing, then path to the
                                // tank's live position.
                                if (diverging)
                                    emit.stop_movement(/*clear_generators*/ true);
                                // Step toward the tank using the SAME off-mesh-aware
                                // stepping the tank's boss-navigator uses to cross
                                // foundry bridges, instead of a plain move_to(tank).
                                // A plain move_to snaps its destination to the nearest
                                // poly under the tank — when the tank stands on a thin
                                // ELEVATED ledge reached only via an off-mesh bridge
                                // (Deadmines foundry Gap-1: a ~6y z=51 ledge above the
                                // z=19 floor), that snap lands on the FLOOR below and
                                // the live pathfinder walks the follower DOWN a descent
                                // that dead-ends 32y under the tank, or onto the ledge's
                                // off-mesh lip where it strands srcpoly=0 (observed live
                                // 2026-06-25: followers FarFromPolyEnd@z19 / NoPath@lip,
                                // 55y back, while the tank crossed fine). DungeonTarget-
                                // ReachableAndStep steps to the next navmesh WAYPOINT
                                // toward the tank — for an off-mesh connection that is
                                // its SOLID far endpoint — carrying the follower ACROSS
                                // the bridge exactly like the tank. Falls back to a
                                // direct move_to when the corridor isn't a clean NORMAL
                                // path (open terrain — the common case — and any target
                                // the strict step rejects), preserving prior behavior.
                                constexpr float kRegroupStep = 45.0f;
                                G3D::Vector3 rstep;
                                bool rstep_offmesh = false;
                                Player* rself =
                                    ObjectAccessor::FindConnectedPlayer(s.raw().guid);
                                if (rself && DungeonTargetReachableAndStep(
                                        rself, tk->x, tk->y, tk->z, kRegroupStep, rstep,
                                        &rstep_offmesh))
                                {
                                    // Off-mesh step toward the tank: COMMIT to the far
                                    // landing vertex so the follower crosses the bridge
                                    // whole instead of restarting over the void when the
                                    // moving-tank goal shifts mid-span (the strand bug).
                                    if (rstep_offmesh)
                                        ai.set_dungeon_cross(rstep.x, rstep.y, rstep.z,
                                                             now_ms + 12000);
                                    emit.move_to(rstep.x, rstep.y, rstep.z);
                                }
                                else if (std::fabs(tk->z - sz) < 8.0f)
                                    // Genuine same stratum (|dz| < 8): a plain
                                    // move_to is safe. The old gate tested
                                    // (tk->z - sz < 8) which is ALSO true when the
                                    // tank is far BELOW us (negative dz) — that
                                    // wrongly fired this floor-snapping move_to for
                                    // the Gap-1 DESCENT (rogue z49 -> tank z19), and
                                    // that move NoPaths from the off-mesh vertex so
                                    // no spline launches: the follower freezes at the
                                    // vertex while the group's cohesion gate waits on
                                    // it (the 9-min harbor strand, observed 2026-06-27).
                                    emit.move_to(tk->x, tk->y, tk->z);
                                else if (!rself || !DungeonNudgeOntoMesh(rself, emit, ai))
                                    // Tank is >8y ABOVE or BELOW us across an off-mesh
                                    // lip (tank on the z51 ledge while we're snapped to
                                    // the z19 floor, OR the reverse Gap-1 descent): a
                                    // plain move_to snaps our dst to the wrong-stratum
                                    // poly and strands us. Recover onto the tank's
                                    // stratum via the off-mesh nudge; if it can't fire,
                                    // HOLD rather than drop.
                                    ai.set_last_rule_fired("idle:dungeon_hold");
                            }
                            ai.set_last_rule_fired("idle:dungeon_regroup_follow_tank");
                            return true;
                        }
                    }
                }
            }
            // Re-admit stationary maintenance before the hard-stop hold. The
            // hold below deliberately suppresses ALL world-idle rules so a bot
            // can't wander off to quest/vendor/hub mid-run — but that also
            // swallowed idle:ooc_rez and idle:ooc_heal, which BELONG in a
            // dungeon: they cast in place on a group member, never relocate.
            // Without them a healer that reaches this idle fall-through next to
            // an injured member just HOLDS, the tank-advance group-ready gate
            // (every member >=70% HP) never clears, and the run deadlocks
            // (observed live 06-26: rogue pinned 1-13% on the Gap-1 ledge while
            // the healer sat at 100% mana / idle:dungeon_hold, atk=0, healing
            // nothing — the group could neither recover nor advance). Run the
            // two maintenance helpers here, scoped to this dungeon-idle tail so
            // they cannot pull the bot off the run (each is no-op when in combat
            // or when nothing needs rez/heal). Rez first (more urgent than a
            // top-off); go_rez higher up already owns the walk-into-range case,
            // so this only fires when a corpse is already in cast range.
            if (MaintainOocRez(s, g, emit))
            {
                ai.set_last_rule_fired("idle:dungeon_ooc_rez");
                return true;
            }
            // Dungeon healer PRE-ENGAGE (2026-06-28). The reactive MaintainOocHeal
            // below only fires once a member is already <80% HP and casts one heal
            // per idle tick — too slow for a fast harbor-floor Defias burst: the tank
            // dropped 100%->0 between snapshots while the healer sat at 100% mana 16y
            // away in idle:dungeon_hold and NEVER cast (the harbor death, observed
            // live 2026-06-28). Fix: when a group member — the TANK first — is IN
            // COMBAT within heal range, the healer casts its heal on it PROACTIVELY
            // (even near full HP). Healing a unit that is in combat puts the healer in
            // combat, so the continuous combat heal APL takes over BEFORE the tank is
            // bursted, instead of the one-shot reactive idle heal. One-shot: once the
            // cast lands the healer is in combat and DungeonRealCombat() routes it to
            // the combat state next tick, so this can't loop. No-op when nobody is
            // fighting / out of range / the heal is on cooldown. Runs only at the
            // dungeon-idle tail (after regroup + rez), so it never pulls the healer
            // off cohesion or a rez.
            if (ai.effective_role(s) == Role::Healer && !DungeonRealCombat(s) &&
                !s.is_casting() && g.exists())
            {
                const uint32 hspell = ClassOocHeal(s.cls(), s.spec());
                if (hspell && s.knows_spell(hspell) && s.is_ready(hspell))
                {
                    float hx, hy, hz; s.position(hx, hy, hz);
                    GroupMemberSummary const* heal_tgt = nullptr;
                    if (auto const* mems = g.members())
                        // Pass 0 = tank (top priority), pass 1 = any member.
                        for (int pass = 0; pass < 2 && !heal_tgt; ++pass)
                            for (auto const& m : *mems)
                            {
                                if (m.guid == s.guid()) continue;
                                if (!m.online || m.map_id != s.map_id() || !m.is_alive)
                                    continue;
                                if (!m.in_combat) continue;
                                if (pass == 0 && m.role != Role::Tank) continue;
                                const float dxh = m.x - hx, dyh = m.y - hy, dzh = m.z - hz;
                                if (dxh * dxh + dyh * dyh + dzh * dzh > 40.0f * 40.0f)
                                    continue;
                                heal_tgt = &m;
                                break;
                            }
                    if (heal_tgt)
                    {
                        emit.cast(hspell, heal_tgt->guid);
                        if (ai.tank_diag_due(now_ms))
                            TC_LOG_INFO("playerbot.v2",
                                "[healer_engage] {} -> member {} (role={}) in combat",
                                s.name(), heal_tgt->guid.ToString(),
                                heal_tgt->role == Role::Tank ? "tank" : "other");
                        ai.set_last_rule_fired("idle:dungeon_healer_engage");
                        return true;
                    }
                }
            }
            if (MaintainOocHeal(s, g, emit, ai.effective_role(s)))
            {
                ai.set_last_rule_fired("idle:dungeon_ooc_heal");
                return true;
            }
            // Dungeon SELF-repair (task #8, 2026-06-28). Instances have NO repair
            // vendor, so a tank whose durability breaks mid-run (every death
            // strips 10%) degrades into paper — 0% durability ~ halves armor/
            // stats — and DEATH-SPIRALS: the Deadmines harbor wall (tank 38
            // deaths, durability 16264->12976 HP, never reached the boss).
            // Out of combat (this idle tail) with gear meaningfully worn (<60%),
            // self-repair. emit.repair_all(Empty) routes to API::repair_all's
            // self-repair branch (no NPC needed) whose stipend top-up makes it
            // effectively free for AI bots — the "gold self-repair on dungeon
            // staging/running" the owner asked for. After it repairs to full the
            // <60% gate self-clears; the 5s Repair lockout only stops the
            // snapshot-lag double-fire. Fires each OOC window (incl. just after a
            // death+rez, when the 10% loss just landed), so durability is kept
            // topped and the spiral can't start.
            if (!s.in_combat() && s.lowest_equipped_durability_pct() < 60 &&
                !ai.action_recently_tried(BotAI::ActionKind::Repair, 0, now_ms))
            {
                emit.repair_all(ObjectGuid::Empty);
                ai.note_action_retry(BotAI::ActionKind::Repair, 0, now_ms);
                ai.set_last_rule_fired("idle:dungeon_self_repair");
                return true;
            }
            // Equip pending gear upgrades IN-DUNGEON (task #8, 2026-06-28). The
            // dungeon hard-stop below suppresses the world-idle idle:auto_equip
            // rule (same swallow that dropped ooc_rez/ooc_heal above), so a
            // dungeon bot NEVER applied the upgrades it looted / was generated:
            // observed live 2026-06-28 the Deadmines tank sat on 5 pending
            // strict-ilvl swaps (mage 2, hunter 3) it couldn't equip because it
            // is perpetually in combat in the harbor. We do NOT reuse
            // MaintainAutoEquipUpgrades here: its level-edge + 3-min periodic
            // gating (tuned for the open world) is far too slow for a dungeon —
            // the squad pulls before it ever fires. Instead apply the SAME
            // strict-score predicate immediately whenever OOC with upgrades
            // pending, one swap/tick, throttled per-entry (equip_recently_tried)
            // so a server-refused piece (level/proficiency/unique) can't spam.
            // Gears the squad up at the entrance before the first pull and during
            // each post-death OOC window.
            if (!s.in_combat() && s.upgrades_pending() > 0)
            {
                auto const& eq_raw = s.raw();
                for (auto const& bi : eq_raw.inventory.bag_items)
                {
                    if (bi.equip_slot == 0xFF) continue;
                    // Shield-tank weapon + offhand are owned EXCLUSIVELY by
                    // EnsureShieldTankWeapon (the gear-backfill 1H+shield enforcer).
                    // The score-based auto-equip must never touch those two slots
                    // for a Prot Warrior (73) / Prot Paladin (66): the 2H that
                    // EnsureShieldTankWeapon displaces into the bag scores HIGHER
                    // than the 1H it just equipped, so this rule would re-equip the
                    // 2H, evict the shield, and the next backfill swaps back —
                    // an infinite 2H<->1H+shield churn that leaves the tank
                    // shield-less mid-pull (live 2026-06-28, Deadmines harbor). 2H
                    // tanks (Blood DK / Guardian Druid) are NOT spec 73/66 and keep
                    // their weapon.
                    if ((s.spec() == 73 || s.spec() == 66) &&
                        (bi.equip_slot == EQUIPMENT_SLOT_MAINHAND ||
                         bi.equip_slot == EQUIPMENT_SLOT_OFFHAND))
                        continue;
                    if (bi.equip_slot >= eq_raw.inventory.equipped.size()) continue;
                    if (bi.item_level == 0 || bi.quality == 0) continue;
                    if (ai.equip_recently_tried(bi.entry, now_ms)) continue;
                    EquippedItem const& cur_eq = eq_raw.inventory.equipped[bi.equip_slot];
                    if (cur_eq.entry != 0 &&
                        EquipFitScore(bi.stats, bi.item_level, eq_raw.stat_weights) <=
                        EquipFitScore(cur_eq.stats, cur_eq.item_level, eq_raw.stat_weights))
                        continue;
                    emit.equip_item(bi.bag, bi.slot, bi.equip_slot);
                    ai.note_equip_try(bi.entry, now_ms);
                    ai.set_last_rule_fired("idle:dungeon_auto_equip");
                    return true;
                }
            }
            // Hard-stop: no world-idle rule runs inside a dungeon.
            // Name WHY we reached the terminal hold — the whole advance ladder
            // declined. route_cur with consumed!=cur or a non-null far-target
            // that got vetoed points at the route-armed veto; empty victim +
            // no candidates points at a genuine dead end. (Diagnostic pattern:
            // [move_lock]/[emit_refused] named the last two freeze classes.)
            {
                static uint32 s_dhold_dbg_ms = 0;
                const uint32 dh_now = GameTime::GetGameTimeMS();
                if (dh_now - s_dhold_dbg_ms > 5000u)
                {
                    s_dhold_dbg_ms = dh_now;
                    TC_LOG_INFO("playerbot.v2",
                        "[dungeon_hold] bot={} role={} victim={} route_cur={} "
                        "consumed={} reached={}",
                        s.bot_id(), int(ai.effective_role(s)),
                        s.victim().IsEmpty() ? 0u : 1u,
                        ai.dungeon_route_wp(s.map_id()),
                        ai.route_consumed_idx(s.map_id()),
                        ai.adv_route_reached_idx(s.map_id()));
                }
            }
            ai.set_last_rule_fired("idle:dungeon_hold");
            return true;
        }
    return false;
}
// ---------- BG dispatcher (REFACTOR_3 pass 16) ----------
// Body extracted verbatim from the inline `if (ai.bg_active() &&
// s.in_battleground())` block. Each inner `return;` became
// `return true;`; the final hard-stop return at the bottom
// (preventing BG bots from falling through to world idle rules)
// is preserved as `return true;`. Wrapped as a single registered
// rule `idle:bg_dispatch` in `Bot/States/Rules/DungeonBgRules.cpp`.
bool BgDispatch(BotSnapshotView const& s, BotAI& ai,
               GroupSnapshotView const& g,
               BotIntentEmitter& emit)
{
    if (!ai.bg_active() || !s.in_battleground()) return false;
        // BG advice cache. GetAdvice() runs per BG-bot per tick; at 1000+
        // BG bots that's ~20K constructions/sec, each allocating several
        // vectors via the script's push_back chains. The advice is
        // near-static once a bot is in a BG — only a handful of snapshot
        // fields drive changes. Cache keyed on (bg_type_id, faction,
        // sota_attacker_team, packed SoTA gate state, packed IoC gate
        // destroyed bits) + a 2s hard-refresh fallback. The fields below
        // are exactly the snapshot inputs the BG scripts consult inside
        // get_advice; adding a new snapshot consumer means adding it to
        // the cache key.
        uint16 cur_bg_id = uint16(s.raw().bg.current_type_id);
        int8   cur_sota_atk = s.bg_sota_attacker_team();
        uint32 sota_gate_pack = 0u;
        for (uint8 g = 0; g < BotSnapshot::SotaGateCount; ++g)
            sota_gate_pack |= (uint32(s.bg_sota_gate_state(BotSnapshot::SotaGateId(g))) & 0x3u) << (g * 2);
        uint8  ioc_gate_pack = 0u;
        for (uint8 g = 0; g < BotSnapshot::IocGateCount; ++g)
            if (s.bg_ioc_gate_destroyed(BotSnapshot::IocGateId(g)))
                ioc_gate_pack = uint8(ioc_gate_pack | (1u << g));
        uint8  av_captain_pack =
            uint8((s.bg_av_balinda_alive()   ? 0x1u : 0u) |
                  (s.bg_av_galvangar_alive() ? 0x2u : 0u));
        auto& bg_cache = ai.bg_advice_cache();
        uint32 now_ms = s.published_at_ms();
        bool cache_miss =
            bg_cache.bg_type_id_key != cur_bg_id ||
            bg_cache.is_horde_key   != s.is_horde() ||
            bg_cache.sota_atk_key   != cur_sota_atk ||
            bg_cache.sota_gate_key  != sota_gate_pack ||
            bg_cache.ioc_gate_key   != ioc_gate_pack ||
            bg_cache.av_captain_key != av_captain_pack ||
            (now_ms - bg_cache.last_built_ms) > 2000u;
        if (cache_miss)
        {
            bg_cache.cached = Services::Initialized()
                ? Services::Battlegrounds().GetAdvice(s)
                : BattlegroundAdvice{};
            bg_cache.bg_type_id_key = cur_bg_id;
            bg_cache.is_horde_key   = s.is_horde();
            bg_cache.sota_atk_key   = cur_sota_atk;
            bg_cache.sota_gate_key  = sota_gate_pack;
            bg_cache.ioc_gate_key   = ioc_gate_pack;
            bg_cache.av_captain_key = av_captain_pack;
            bg_cache.last_built_ms  = now_ms;
            // Mirror the fresh advice for world-thread readers (snapshot
            // builder bg_role block). They must never touch bg_cache
            // directly — this reassignment frees its previous vectors.
            ai.publish_bg_advice(cur_bg_id, bg_cache.cached);
            if (Services::Initialized())
                Services::Perf().record_bg_advice_miss();
        }
        else if (Services::Initialized())
        {
            Services::Perf().record_bg_advice_hit();
        }
        BattlegroundAdvice const& bg_advice = bg_cache.cached;

        // Resolve a Node's effective position. For moving objectives
        // (Silvershard mine carts — entry 60140) the node's static x/y/z
        // is the spawn point but the cart travels on rails. When
        // follow_creature_entry is set, scan nearby units for the closest
        // matching live creature and use its position instead. Caller
        // gets the static coord back when no live unit is in range.
        auto resolve_node_pos = [&](BattlegroundAdvice::Node const& n,
                                    float& out_x, float& out_y, float& out_z)
        {
            out_x = n.x; out_y = n.y; out_z = n.z;
            if (n.follow_creature_entry == 0) return;
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            float best_dsq = std::numeric_limits<float>::max();
            auto scan = [&](std::vector<NearbyUnit> const& vec)
            {
                for (auto const& u : vec)
                {
                    if (u.entry != n.follow_creature_entry) continue;
                    if (u.hp <= 0) continue;
                    float dx = u.x - self_x;
                    float dy = u.y - self_y;
                    float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq)
                    {
                        best_dsq = dsq;
                        out_x = u.x; out_y = u.y; out_z = u.z;
                    }
                }
            };
            scan(s.nearby_friends());
            scan(s.nearby_enemies());
        };

        // Diagnostic: confirm we entered the BG block and report advice.
        {
            static thread_local uint32 last_inside_diag_ms = 0;
            const uint32 inside_now = s.published_at_ms();
            if ((inside_now - last_inside_diag_ms) > 5000)
            {
                last_inside_diag_ms = inside_now;
                TC_LOG_INFO("playerbot.v2",
                    "[bg_inside] bot={} bg_type={} role_slots={} nodes={} "
                    "formation_slot={} home_base=({:.0f},{:.0f},{:.0f})",
                    s.bot_id(), uint32(s.raw().bg.current_type_id),
                    uint32(bg_advice.role_by_slot.size()),
                    uint32(bg_advice.nodes.size()),
                    uint32(ai.formation_slot()),
                    bg_advice.home_base_x, bg_advice.home_base_y,
                    bg_advice.home_base_z);
            }
        }

        // Post-match auto-leave. bg_status=4 (STATUS_WAIT_LEAVE) means
        // the match has ended and TC is waiting ~2min for everyone to
        // leave voluntarily before auto-kicking. Without this rule, bots
        // kept firing BG patrol rules (idle:bg_roamer_patrol etc.) inside
        // the dead instance, stayed in the BG raid group, and were
        // therefore ineligible to be queued for the NEXT BG — which
        // looked to the user like "no BG is starting". Emitting bg_leave
        // immediately on status=4 dissolves the bot's BG attachment;
        // TC removes them from the raid group + teleports them out, and
        // they're back in the candidate pool for the next QueueFill on
        // the next tick. We don't gate on is_alive — dead bots should
        // also leave so their corpse-run doesn't linger in a defunct BG.
        if (s.raw().bg.status == 4 /*STATUS_WAIT_LEAVE*/)
        {
            // Peer-learning data: record BG outcome before we leave.
            // Compare our team's score vs enemy team's. team==1 alliance,
            // team==2 horde. Tie counts as loss (modern WoW BGs rarely tie).
            if (Services::Initialized() && !ai.recorded_bg_outcome_for(s.published_at_ms()))
            {
                const uint32 my_score   = (s.raw().identity.team == 1)
                    ? s.raw().bg.score_alliance : s.raw().bg.score_horde;
                const uint32 enemy_score = (s.raw().identity.team == 1)
                    ? s.raw().bg.score_horde : s.raw().bg.score_alliance;
                Services::Perf().record_bg_outcome(
                    s.cls(), uint16(s.spec()), uint8(s.level() / 10),
                    /*won*/ my_score > enemy_score);
                ai.note_bg_outcome_recorded(s.published_at_ms());
            }
            emit.bg_leave();
            ai.set_last_rule_fired("idle:bg_post_match_leave");
            return true;
        }

        // Prep-phase gate. While the BG is in STATUS_WAIT_JOIN the start
        // gates are still up — running "objective" rules would have bots
        // hammering invisible walls at the gate. Wrap all aggressive
        // movement / combat-engagement BG rules in an `is_live` check so
        // they only fire after the gates open (STATUS_IN_PROGRESS).
        // EXCEPTION: water-escape. Some BGs (Seething Shore) spawn
        // players on a beach edge that's partially in water. Real
        // players move inland instantly; bots standing still take
        // continuous fatigue damage and drown. If we detect we're in
        // water during prep AND a safe home_base is defined, walk
        // there. Bots on solid ground / on a transport stay still.
        if (s.bg_in_prep() && s.is_alive() && !s.in_combat() && !s.is_casting() &&
            (s.is_swimming() || s.is_underwater()) &&
            (bg_advice.home_base_x != 0.f || bg_advice.home_base_y != 0.f))
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            float dx = bg_advice.home_base_x - self_x;
            float dy = bg_advice.home_base_y - self_y;
            if (dx * dx + dy * dy > 100.f)  // 10y arrival radius
            {
                emit.move_to(bg_advice.home_base_x,
                             bg_advice.home_base_y,
                             bg_advice.home_base_z, /*run=*/true);
                ai.set_last_rule_fired("idle:bg_prep_water_escape");
                return true;
            }
        }

        // ---- OOC recovery band (BG audit N01/N05) ----
        // BgDispatch hard-stops with `return true` at the bottom, so the
        // world maintenance family below priority 718 (idle:ooc_eat_food
        // 666, idle:self_buff 682, idle:pet 678, idle:conjured_item 672)
        // is UNREACHABLE for the entire match — bots sprinted between
        // fights at 30% HP and zero mana, healers went OOM permanently,
        // hunters/warlocks fought 15+ minutes petless after one pet
        // death, and during the 1-2 minute prep phase everyone stood
        // motionless and unbuffed. Reuse the same Maintain* helpers the
        // world rules wrap, gated on safety: alive, not casting, not in
        // combat, and (outside prep) no enemy PLAYER within 30y — never
        // sit down to drink in front of an attacker.
        if (s.is_alive() && !s.in_combat() && !s.is_casting())
        {
            bool recovery_safe = s.bg_in_prep();
            if (!recovery_safe)
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                bool enemy_close = false;
                for (auto const& e : s.nearby_enemies())
                {
                    if (!e.is_player || e.hp <= 0) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    if (dx * dx + dy * dy < 900.0f) { enemy_close = true; break; }
                }
                recovery_safe = !enemy_close;
            }
            if (recovery_safe)
            {
                if (MaintainOocFood(s, ai, emit))
                {
                    ai.set_last_rule_fired("idle:bg_ooc_eat_drink");
                    return true;
                }
                if (MaintainSelfBuff(s, g, emit))
                {
                    ai.set_last_rule_fired("idle:bg_self_buff");
                    return true;
                }
                if (MaintainPet(s, ai, emit))
                {
                    ai.set_last_rule_fired("idle:bg_pet");
                    return true;
                }
                // Mage table food / healthstones only during prep — mid-
                // match the cast time isn't worth it.
                if (s.bg_in_prep() && MaintainConjuredItem(s, emit))
                {
                    ai.set_last_rule_fired("idle:bg_conjured_item");
                    return true;
                }
            }
        }

        if (!s.bg_in_prep())
        {

        // Resolve this bot's BG role from the per-slot role table.
        // Wraps modulo size so 40-slot raids in Ashran reuse the
        // 10-slot pattern. Empty table → BgRole::Free → fall through.
        // Owner-set formation_slot (via /squad) takes precedence.
        //
        // Slot derivation (BG audit N07/N24/N61/N72): the old fallback
        // hashed each bot's own guid modulo table size — sampling WITH
        // replacement. At 10-man scale that rolled duplicate slots and
        // whole missing roles (~35% of WSG teams had no FC slot at all,
        // ~10% no Defender or no Healer). Every BG team is a raid group,
        // so derive a true PERMUTATION instead: my slot = my guid's rank
        // in the sorted roster of my group — every slot 0..N-1 occupied
        // exactly once, computable per-bot with no coordinator. Falls
        // back to the guid hash only when the group snapshot is missing
        // (boot tick before group sync).
        // Team rank: my guid's rank within the sorted raid roster. Used
        // for the slot permutation below AND as the per-bot spread index
        // that de-herds node assignment (audit N14/N19/N50). Falls back
        // to a guid hash when the group snapshot is missing.
        uint32 bg_team_rank = uint32(s.guid().GetCounter() % 16u);
        bool   bg_team_ranked = false;
        if (auto const* mem = g.members(); mem && mem->size() > 1)
        {
            uint32 rank = 0;
            bool self_seen = false;
            const uint64 my_low = s.guid().GetCounter();
            for (auto const& m : *mem)
            {
                const uint64 m_low = m.guid.GetCounter();
                if (m_low == my_low) { self_seen = true; continue; }
                if (m_low < my_low) ++rank;
            }
            if (self_seen)
            {
                bg_team_rank = rank;
                bg_team_ranked = true;
            }
        }

        BgRole my_role = BgRole::Free;
        if (!bg_advice.role_by_slot.empty())
        {
            uint8 slot = ai.formation_slot();
            if (slot == 0)
            {
                // Rank permutation (audit N07/N24/N61/N72): the old
                // guid-hash fallback sampled slots WITH replacement —
                // duplicate slots and whole missing roles (~35% of WSG
                // teams rolled no FC slot). The roster rank fills every
                // slot 0..N-1 exactly once, coordinator-free.
                slot = bg_team_ranked
                    ? uint8(bg_team_rank % bg_advice.role_by_slot.size())
                    : uint8(s.guid().GetCounter() %
                            bg_advice.role_by_slot.size());
            }
            my_role = bg_advice.role_by_slot[slot % bg_advice.role_by_slot.size()];

            // Class-aware role fixup: the slot-index → role mapping is
            // class-blind, so a Hunter landing on a Healer slot (or a
            // Mage on a Tank slot) gets a role its kit can't fulfill.
            // The result is a "Healer" that never heals — wasted slot.
            // Heuristic fix: only honor Healer slot when the bot's
            // spec is actually a healer spec. Tank role (used by AV's
            // GY rush + IoC's siege defense) similarly only goes to
            // tank specs. Mismatched bots fall back to Roamer (mobile
            // contributor) which any spec can fulfill.
            const uint8  my_cls  = s.raw().identity.cls;
            const uint16 my_spec = uint16(s.raw().identity.spec);
            const bool i_can_heal = IsHealerSpec(my_cls, my_spec);
            const bool i_can_tank = IsTankSpec(my_cls, my_spec);
            if (my_role == BgRole::Healer && !i_can_heal)
                my_role = BgRole::Roamer;
            // BgRole has no explicit Tank assignment (AV's tank-role
            // bots use Defender slots); guarding here anyway for any
            // future script that adds a Tank slot type.
            (void)i_can_tank;

            // Class-aware FC override. When the script names preferred
            // FC classes (stealth-FC meta in WSG/TP, mobility-FC in EotS):
            //   * Preferred class → forced FlagCarrier role.
            //   * Non-preferred class hashed into FlagCarrier slot →
            //     demoted to Roamer (frees the role for a preferred bot).
            // Self-consistent without coordination: every bot of every
            // class arrives at the same role regardless of who else is
            // in the BG. Multiple preferred bots all show up as
            // FlagCarrier — the acts_as_fc grab race picks one carrier;
            // the rest peel / harass via the standard role behaviors.
            if (!bg_advice.fc_class_preference.empty())
            {
                bool class_preferred = false;
                for (uint8 c : bg_advice.fc_class_preference)
                    if (c == my_cls) { class_preferred = true; break; }
                // Promote ONLY if current role isn't a class-matched
                // healer/tank slot. Without this gate a Druid (cls 11,
                // preferred for WSG/TP FC) hashed into a Healer slot
                // would stay Healer through fixup then force-promote to
                // FlagCarrier, starving the bg of healers when there
                // are few Druid bots online.
                // Healer specs are tier-1 irreplaceable: a Druid/Priest/Pala
                // hashed into the FlagCarrier slot still skips promotion so
                // its kit stays in healing roles. Without checking the spec
                // (not just the current role), a healer-spec bot landing
                // directly in the FC slot via guid-hash would force-promote
                // and the BG would lose a healer permanently.
                const bool i_have_irreplaceable_role = i_can_heal;
                if (class_preferred && !i_have_irreplaceable_role)
                    my_role = BgRole::FlagCarrier;
                else if (!class_preferred && my_role == BgRole::FlagCarrier)
                    my_role = BgRole::Roamer;
            }
        }

        // Score-aware tactical directive. Real players read the score
        // and adjust: comfortable lead → turtle (defend held nodes,
        // don't overextend). Big deficit → all-in (every body on
        // offense even at the cost of node defense). The threshold is
        // per-script (BattlegroundAdvice::score_bias_threshold): score
        // units differ wildly by BG — AB/EotS count resource points
        // (max 1500+) where 200 is "noticeable", WSG/TP count FLAG CAPS
        // (max 3) where the old hardcoded 200 could never fire (BG
        // audit N55/N65). FlagCarrier / FCEscort / Healer roles are NOT
        // overridden — they have specific jobs that don't bend to score.
        enum TacticalBias { Bias_Normal, Bias_Turtle, Bias_AllIn };
        TacticalBias bias = Bias_Normal;
        {
            uint8 mt = s.team();
            int32 my_sc  = (mt == 1) ? int32(s.bg_score_alliance()) : int32(s.bg_score_horde());
            int32 en_sc  = (mt == 1) ? int32(s.bg_score_horde())    : int32(s.bg_score_alliance());
            int32 delta  = my_sc - en_sc;
            const int32 thr = bg_advice.score_bias_threshold > 0
                ? bg_advice.score_bias_threshold : 200;
            if      (delta >=  thr) bias = Bias_Turtle;
            else if (delta <= -thr) bias = Bias_AllIn;
            // Late-game urgency (BG audit N54): humans read the clock,
            // not just the score. Past 18 minutes (timed normal BGs cap
            // at 20-25), ANY deficit means there is no time for a
            // measured comeback -- go all-in; any lead is worth
            // protecting -- turtle. Ties keep playing normally.
            else if (s.raw().bg.in_progress_ms > 18u * 60u * 1000u)
            {
                if      (delta < 0) bias = Bias_AllIn;
                else if (delta > 0) bias = Bias_Turtle;
            }
        }
        const bool role_override_eligible =
            my_role == BgRole::Attacker ||
            my_role == BgRole::Defender ||
            my_role == BgRole::Roamer ||
            my_role == BgRole::Free;
        if (role_override_eligible)
        {
            if (bias == Bias_Turtle && my_role == BgRole::Attacker)
                my_role = BgRole::Defender;       // hold what we have
            else if (bias == Bias_AllIn && my_role == BgRole::Defender)
                my_role = BgRole::Attacker;       // throw bodies forward
            else if (bias == Bias_AllIn && my_role == BgRole::Roamer)
                my_role = BgRole::Attacker;       // every roamer pushes
        }

        // Per-bot diagnostic so we can attribute "bots idle in BG" to a
        // specific role / state. thread_local to keep volume bounded.
        {
            static thread_local uint32 last_role_diag_ms = 0;
            const uint32 role_now = s.published_at_ms();
            if ((role_now - last_role_diag_ms) > 5000)
            {
                last_role_diag_ms = role_now;
                float px = 0.f, py = 0.f, pz = 0.f;
                s.position(px, py, pz);
                size_t node_states = s.bg_node_states().size();
                TC_LOG_INFO("playerbot.v2",
                    "[bg_role] bot={} role={} bias={} alive={} casting={} "
                    "combat={} pos=({:.0f},{:.0f}) node_states={}",
                    s.bot_id(), uint32(my_role), uint32(bias),
                    s.is_alive() ? 1 : 0, s.is_casting() ? 1 : 0,
                    s.in_combat() ? 1 : 0, px, py, uint32(node_states));
            }
        }

        // CC-by-ally helper closure. Returns true when the enemy is
        // currently CC'd by a teammate (or the bot itself), so target-
        // switching rules can skip them — attacking a CC'd target
        // breaks the CC and burns the ally's spell. The check walks
        // nearby_friends + own guid; any match flags the CC as
        // friendly-applied. NPC casters (creatures) wouldn't appear
        // in nearby_friends so creature-cast CC always falls through.
        auto is_cc_by_ally = [&](NearbyUnit const& e) -> bool
        {
            if (!e.is_cc_locked) return false;
            if (e.cc_caster.IsEmpty()) return false;
            if (e.cc_caster == s.guid()) return true;
            for (auto const& f : s.nearby_friends())
                if (f.guid == e.cc_caster) return true;
            return false;
        };

        // ---- BG mount-up ----
        // Open-field BGs (AB, AV, IoC, EotS, Ashran, SoTA-out-of-vehicle)
        // allow mounts and the distances are long enough that running
        // on foot is a measurable disadvantage. Mounting after the
        // gates open gets bots to objectives in roughly a third of the
        // time. Server-side checks gate which BGs actually accept the
        // cast — the API returns Locked on no-mount maps (WSG indoors,
        // arenas), so the rule's per-attempt 60s lockout absorbs the
        // refusal cheaply.
        //
        // Skip conditions:
        //   * is_mounted    — already done
        //   * on_vehicle    — siege engine seat, not a personal mount
        //   * in_combat     — auto-dismount; re-mount triggers after
        //                     combat ends via this rule on next idle tick
        //   * is_casting    — would interrupt our own cast
        //   * !is_alive     — ghost; can't mount
        //   * best_mount_spell == 0 — no known mount on this bot
        //
        // BG-wide policy decision: we don't gate on a per-BG allow-list
        // because the server's CanUseMount() already encodes that, and
        // the 60s lockout means the bot tries once per minute and gives
        // up gracefully when it's an arena / WSG-tunnel scenario.
        if (s.is_alive() && !s.raw().movement.is_mounted && !s.on_vehicle() &&
            !s.is_casting() && !s.in_combat() &&
            s.best_mount_spell() != 0)
        {
            const uint32 mt_now_ms = s.published_at_ms();
            // Dedicated BgMount kind, 25s (audit N43: the old PetSwap
            // piggyback was a 5-minute lockout armed even on success —
            // every combat dismount cost 5 minutes of walking).
            const uint64 mt_key = uint64(s.raw().bg.current_type_id);
            if (!ai.action_recently_tried(BotAI::ActionKind::BgMount,
                                          mt_key, mt_now_ms))
            {
                emit.emit(MountIntent{s.best_mount_spell()});
                ai.note_action_retry(BotAI::ActionKind::BgMount,
                                     mt_key, mt_now_ms);
                ai.set_last_rule_fired("idle:bg_mount_up");
                return true;
            }
        }

        // Auto-use any nearby BG GO of an opted-in type. AB / EotS /
        // BfG / IoC scripts opt in to FLAGSTAND (24); WSG / TP opt in
        // to FLAGDROP (26) for return mechanics. 5-yard contact range.
        // Skipped while moving in a planned route — the move-to below
        // handles approach. Cross-references bg_node_states by position
        // proximity — if the matching node is already fully owned by
        // my team (and not contested), skip the use: a full-control
        // re-cap is a wasted server cast that the bot would spam every
        // tick on a held node.
        // Out-of-combat objective interaction (flag pickup / node cap).
        // The scan body now lives in the shared BgTryUseObjectiveGo helper
        // so State_InCombat can run the IDENTICAL scan in combat (BG audit
        // S1) — a bot fighting over a defended WSG flag / contested AB-EotS
        // node could never click it before, the #1 "no flag pickup" blocker.
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            BgTryUseObjectiveGo(s, ai, emit, bg_advice))
            return true;

        // SoTA on-foot SEAFORIUM (BG audit §1 — infantry gate-breaking when
        // no demolisher is available). A bot that picked up a charge (the
        // auto-use above clicks the seaforium GO 190753/194086 → grants aura
        // 52415 "Carrying Seaforium Charge") detonates it by casting Place
        // Seaforium Charge (52410, SELF cast, range 0 → must stand AT the
        // gate; consumes 52415). The summoned trap fires 52408 which damages
        // the destructible gate. When carrying but not yet at a gate, walk to
        // the script's breach waypoint (endgame_target aims at the nearest
        // STANDING gate). Gated on has_aura so it only runs for charge
        // carriers — no behavioural change on other BGs.
        if (s.has_aura(52415) && s.is_alive() && !s.is_casting() &&
            !bg_advice.siege_target_go_entries.empty())
        {
            float sf_x = 0.f, sf_y = 0.f, sf_z = 0.f;
            s.position(sf_x, sf_y, sf_z);
            BotSnapshot::NearbyObject const* gate = nullptr;
            float gate_dsq = 36.0f;  // 6y — place is range 0, stand on the gate
            for (auto const& o : s.raw().world_objects.nearby_objects)
            {
                if (o.go_type != /*DESTRUCTIBLE_BUILDING*/ 33 || o.is_destroyed)
                    continue;
                bool match = false;
                for (uint32_t ge : bg_advice.siege_target_go_entries)
                    if (o.entry == ge) { match = true; break; }
                if (!match) continue;
                const float dx = o.x - sf_x, dy = o.y - sf_y;
                const float dsq = dx * dx + dy * dy;
                if (dsq < gate_dsq) { gate_dsq = dsq; gate = &o; }
            }
            if (gate)
            {
                const uint32 sf_now = GameTime::GetGameTimeMS();
                if (!ai.action_recently_tried(BotAI::ActionKind::BgUseGo,
                                              /*key*/ 52410ull, sf_now))
                {
                    emit.cast(52410);  // self-cast; places the charge at our feet
                    ai.note_action_retry(BotAI::ActionKind::BgUseGo, 52410ull, sf_now);
                    ai.set_last_rule_fired("idle:bg_seaforium_place");
                    return true;
                }
            }
            else if ((bg_advice.endgame_target_x != 0.f ||
                      bg_advice.endgame_target_y != 0.f) && !s.in_combat())
            {
                const float dx = bg_advice.endgame_target_x - sf_x;
                const float dy = bg_advice.endgame_target_y - sf_y;
                if (dx * dx + dy * dy > 25.0f)
                {
                    emit.move_to(bg_advice.endgame_target_x,
                                 bg_advice.endgame_target_y,
                                 bg_advice.endgame_target_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_seaforium_carry");
                    return true;
                }
            }
        }

        // ---- Vehicle behaviors (Phase #209) ----
        // Mount-nearby-vehicle: scan nearby_friends for Creatures whose
        // entry matches the script's allow-list. Pick the closest empty
        // one (HP > 0 — destroyed demolishers stay in nearby_friends
        // until despawn). Fires only when bot is alive, on foot, and
        // out of combat.
        if (!s.on_vehicle() && s.is_alive() && !s.is_casting() &&
            !s.in_combat() &&
            !bg_advice.vehicle_creature_entries.empty())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            NearbyUnit const* best = nullptr;
            // Search to 100y (was 64y): IoC siege engines sit ~80y from the
            // Workshop node, so a node-holder must be able to target + walk to
            // them. The builder's wide BG-vehicle scan publishes vehicles out to
            // 110y in nearby_friends; this lets the bot route to one.
            float best_dsq = 100.0f * 100.0f;
            for (auto const& f : s.nearby_friends())
            {
                bool match = false;
                for (uint32 e : bg_advice.vehicle_creature_entries)
                    if (f.entry == e) { match = true; break; }
                if (!match) continue;
                if (f.hp <= 0) continue;
                float dx = f.x - self_x;
                float dy = f.y - self_y;
                float dsq = dx * dx + dy * dy;
                if (dsq < best_dsq) { best_dsq = dsq; best = &f; }
            }
            if (best)
            {
                if (best_dsq <= 64.0f)  // 8y enter range
                {
                    // Per-vehicle 30s cooldown — vehicle may be claimed
                    // between snapshot and emit, or seat is locked. The
                    // vehicle stays in nearby_friends so this would re-
                    // fire every tick.
                    const uint32 veh_now_ms = GameTime::GetGameTimeMS();
                    const uint64 veh_low = best->guid.GetCounter();
                    if (!ai.action_recently_tried(BotAI::ActionKind::BgEnterVehicle,
                                                  veh_low, veh_now_ms))
                    {
                        emit.enter_vehicle(best->guid, /*seat_id=*/-1);
                        ai.note_action_retry(BotAI::ActionKind::BgEnterVehicle,
                                             veh_low, veh_now_ms);
                        ai.set_last_rule_fired("idle:bg_enter_vehicle");
                        return true;
                    }
                }
                else
                {
                    // Walk toward the vehicle.
                    emit.move_to(best->x, best->y, best->z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_move_to_vehicle");
                    return true;
                }
            }
        }

        // On-vehicle seat fire: when seated and an enemy is in range,
        // cast the script's seat ability (boulder hurl etc). 50-yard
        // engagement window — most siege spells have 30-50y range.
        // Server-side spell cooldown on the vehicle Unit gates re-fires.
        // Per-vehicle-entry seat spell wins over the script's default;
        // IoC ships demolisher / siege engine / glaive thrower etc each
        // with a different fire ability. Falls back to the default when
        // the entry isn't in the override map.
        uint32 active_seat_spell = 0;
        if (s.on_vehicle())
        {
            uint32 ve = s.vehicle_entry();
            if (ve != 0)
            {
                auto it = bg_advice.vehicle_seat_spell_by_entry.find(ve);
                if (it != bg_advice.vehicle_seat_spell_by_entry.end())
                    active_seat_spell = it->second;
            }
            if (active_seat_spell == 0)
                active_seat_spell = bg_advice.vehicle_seat_spell;
        }

        // Siege-vehicle GATE fire (BG audit IoC / SoTA blockers). Before
        // hunting unit targets, a seated bot fires its gate-damage seat spell
        // AT the closest STANDING enemy gate (DESTRUCTIBLE_BUILDING 33 whose
        // entry is in the script's siege_target_go_entries). Gates are the
        // win path — IoC keep gates open the General, SoTA defense-line gates
        // unlock the Titan Relic — and the unit-only fire below never saw a
        // gate (it scans nearby_enemies Units), so gates never fell. Uses
        // cast_vehicle_at (ground/position target) since a gate is a GO, not
        // a Unit. Once a gate is breached (is_destroyed) the bot advances to
        // the next standing gate / the General (a Unit, killed by ANY vehicle
        // spell via the boss SpellHit instakill).
        if (s.on_vehicle() && active_seat_spell != 0 && !s.is_casting() &&
            !bg_advice.siege_target_go_entries.empty())
        {
            float gv_x = 0.f, gv_y = 0.f, gv_z = 0.f;
            s.position(gv_x, gv_y, gv_z);
            BotSnapshot::NearbyObject const* gate = nullptr;
            float gate_dsq = 60.0f * 60.0f;  // siege range + approach slack
            for (auto const& o : s.raw().world_objects.nearby_objects)
            {
                if (o.go_type != /*DESTRUCTIBLE_BUILDING*/ 33) continue;
                if (o.is_destroyed) continue;
                bool match = false;
                for (uint32_t ge : bg_advice.siege_target_go_entries)
                    if (o.entry == ge) { match = true; break; }
                if (!match) continue;
                float dx = o.x - gv_x, dy = o.y - gv_y;
                float dsq = dx * dx + dy * dy;
                if (dsq < gate_dsq) { gate_dsq = dsq; gate = &o; }
            }
            if (gate)
            {
                const uint32 gf_now_ms = GameTime::GetGameTimeMS();
                const uint64 gate_low = gate->guid.GetCounter();
                if (!ai.action_recently_tried(BotAI::ActionKind::BgVehicleFire,
                                              gate_low, gf_now_ms))
                {
                    emit.cast_vehicle_at(active_seat_spell,
                                         gate->x, gate->y, gate->z);
                    ai.note_action_retry(BotAI::ActionKind::BgVehicleFire,
                                         gate_low, gf_now_ms);
                    ai.set_last_rule_fired("idle:bg_vehicle_fire_gate");
                    return true;
                }
                // Gate in range but the seat weapon is on its short cooldown:
                // HOLD at the gate (return) instead of falling through to the
                // node-order march, which would drive the siege vehicle back
                // off the gate and break the breach. Keeps it battering.
                ai.set_last_rule_fired("idle:bg_vehicle_fire_gate_hold");
                return true;
            }
            else if (bg_advice.endgame_target_x != 0.f ||
                     bg_advice.endgame_target_y != 0.f)
            {
                // No standing gate in snapshot range (30y scan): drive the
                // siege vehicle toward the script's breach waypoint, which
                // the IoC/SoTA scripts aim at the nearest STANDING enemy
                // gate. This is UNCONDITIONAL (not bias-gated) so siege
                // engines actually reach the wall instead of waiting on an
                // AllIn score state that round-based SoTA never reaches.
                const float dx = bg_advice.endgame_target_x - gv_x;
                const float dy = bg_advice.endgame_target_y - gv_y;
                if (dx * dx + dy * dy > 25.0f * 25.0f)  // within ~25y → in fire range soon
                {
                    emit.move_to(bg_advice.endgame_target_x,
                                 bg_advice.endgame_target_y,
                                 bg_advice.endgame_target_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_vehicle_drive_to_gate");
                    return true;
                }
            }
        }

        if (s.on_vehicle() && active_seat_spell != 0 &&
            !s.is_casting())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            NearbyUnit const* best = nullptr;
            float best_dsq = 50.0f * 50.0f;  // engage to 50y
            for (auto const& e : s.nearby_enemies())
            {
                if (e.hp <= 0) continue;
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                float dsq = dx * dx + dy * dy;
                if (dsq < best_dsq) { best_dsq = dsq; best = &e; }
            }
            if (best)
            {
                // cast_vehicle is fire-and-forget; we don't have a built-in
                // dedup. Use a short per-target cooldown to avoid claiming
                // the dispatch slot every tick when the vehicle's GCD or
                // ammo state silently rejects the cast. Reuse BgUseGo
                // since the namespace is "BG action by GUID".
                const uint32 vf_now_ms = GameTime::GetGameTimeMS();
                const uint64 tgt_low = best->guid.GetCounter();
                if (!ai.action_recently_tried(BotAI::ActionKind::BgUseGo,
                                              tgt_low, vf_now_ms))
                {
                    emit.cast_vehicle(active_seat_spell, best->guid);
                    ai.note_action_retry(BotAI::ActionKind::BgUseGo,
                                         tgt_low, vf_now_ms);
                    ai.set_last_rule_fired("idle:bg_vehicle_fire");
                    return true;
                }
            }
        }

        // Tank-intercept-FC (Phase C generic). Tanks have peel kits
        // that make them ideal interceptors regardless of assigned
        // BG role. Fires whenever an enemy flag carrier exists, even
        // if the script didn't set chase_enemy_carrier.
        // CARRIER GUARD (BG audit N29): a tank-spec bot CARRYING the
        // flag must never divert to chase the EFC across the map — the
        // promoted Guardian-druid FC did exactly that, hauling our flag
        // into the enemy team. Carriers run home; everyone else peels.
        bool i_carry_bg_flag =
            s.bg_friendly_flag_carrier() == s.guid();
        if (!i_carry_bg_flag)
            for (auto const& fcg : s.bg_all_friendly_carriers())
                if (fcg == s.guid()) { i_carry_bg_flag = true; break; }
        if (s.is_alive() && !s.is_casting() && !i_carry_bg_flag &&
            !s.bg_enemy_flag_carrier().IsEmpty() &&
            ai.effective_role(s) == Role::Tank)
        {
            // Gate on start_attack — when the per-target StartAttack
            // lockout suppresses the emit (carrier out-of-range / immune
            // / phased), fall through to other rules instead of claiming
            // the dispatch slot every BG tick.
            if (emit.start_attack(s.bg_enemy_flag_carrier()))
            {
                ai.set_last_rule_fired("idle:bg_tank_intercept_carrier");
                return true;
            }
        }

        // BG callout: "FC down" / "FC up". Edge-trigger on friendly
        // carrier transitions. Real players say "FC down" then "FC up
        // at X" — we drop the HP suffix since the snapshot drops the
        // pct field once the carrier guid clears.
        {
            const bool fc_now = !s.bg_friendly_flag_carrier().IsEmpty();
            const bool fc_prev = ai.prev_friendly_fc_set();
            if (fc_prev != fc_now)
            {
                const uint32 fc_now_ms = GameTime::GetGameTimeMS();
                // Distinct keys for up/down so the 20s lockout per
                // direction can fire one of each within the window.
                const uint64 fc_key = (uint64(2) << 32) |
                                      uint64(fc_now ? 0xFCFC : 0xFCD0);
                // Cross-bot dedup — first bot to detect the transition
                // claims the 30s slot; the other 39 stay quiet.
                const bool claimed = BgCalloutCoordinator::TryClaim(
                    /*kind=*/2, /*key=*/fc_now ? 1u : 0u, fc_now_ms,
                    /*lockout=*/30u * 1000u);
                if (claimed &&
                    !ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                              fc_key, fc_now_ms))
                {
                    if (emit.raid_chat(fc_now ? "FC up" : "FC down"))
                    {
                        ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                             fc_key, fc_now_ms);
                        ai.set_last_rule_fired(fc_now
                            ? "idle:bg_callout_fc_up"
                            : "idle:bg_callout_fc_down");
                        ai.set_prev_friendly_fc_set(fc_now);
                        return true;
                    }
                }
            }
            ai.set_prev_friendly_fc_set(fc_now);
        }

        // BG callout: "EFC at <node>" — when bot sees the enemy flag
        // carrier's resolved position (snapshot's bg_enemy_carrier_*
        // populated by the builder regardless of nearby_enemies range),
        // and that position is within 60y of a known BG node, call out
        // the location so the team can intercept. Uses the same
        // BgCalloutCoordinator slot-claim pattern as other BG callouts.
        // Without this, real players outside FC range never learn where
        // the FC is despite the bot's omniscient snapshot. 30s lockout
        // per (node_entry) so the same camp doesn't spam chat.
        if (!s.bg_enemy_flag_carrier().IsEmpty() &&
            (s.raw().bg.enemy_carrier_x != 0.f ||
             s.raw().bg.enemy_carrier_y != 0.f))
        {
            const float fcx = s.raw().bg.enemy_carrier_x;
            const float fcy = s.raw().bg.enemy_carrier_y;
            BotSnapshot::BgNodeState const* fc_node = nullptr;
            float best_dsq = 60.0f * 60.0f;
            for (auto const& n : s.bg_node_states())
            {
                const float dx = n.x - fcx;
                const float dy = n.y - fcy;
                const float dsq = dx * dx + dy * dy;
                if (dsq < best_dsq)
                { best_dsq = dsq; fc_node = &n; }
            }
            if (fc_node)
            {
                const uint32 efc_now = GameTime::GetGameTimeMS();
                // Key on (node_entry) so we don't re-shout for the same
                // location within the window. If the FC moves to a new
                // node, the new entry re-fires.
                const uint64 efc_key = (uint64(12) << 32) |
                                        uint64(fc_node->entry);
                if (!ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                              efc_key, efc_now) &&
                    BgCalloutCoordinator::TryClaim(/*kind=*/12,
                                                   uint64(fc_node->entry),
                                                   efc_now,
                                                   /*lockout=*/30u * 1000u))
                {
                    std::string node_name = fc_node->name.empty()
                        ? std::to_string(fc_node->entry) : fc_node->name;
                    std::string msg = "EFC at "; msg += node_name;
                    if (emit.raid_chat(msg))
                    {
                        ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                             efc_key, efc_now);
                        ai.set_last_rule_fired("idle:bg_callout_efc_location");
                        return true;
                    }
                }
            }
        }

        // BG callout: "carrying, low" — when bot IS the friendly FC
        // and HP drops below 50%, broadcast a status. Real flag
        // carriers ping their healers explicitly when in trouble.
        if (s.is_alive() && s.bg_friendly_flag_carrier() == s.guid() &&
            s.hp_pct() < 50)
        {
            const uint32 fcs_now = GameTime::GetGameTimeMS();
            // Bucket by 25%-HP so each crossing reannounces:
            //   <50% → bucket 1, <25% → bucket 2.
            const uint32 bucket = s.hp_pct() < 25 ? 2 : 1;
            const uint64 fcs_key = (uint64(5) << 32) | uint64(bucket);
            if (!ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                          fcs_key, fcs_now))
            {
                std::string msg = bucket == 2
                    ? "carrying, dying"
                    : "carrying, low HP";
                if (emit.raid_chat(msg))
                {
                    ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                         fcs_key, fcs_now);
                    ai.set_last_rule_fired("idle:bg_callout_fc_self_status");
                    return true;
                }
            }
        }

        // BG callout: "their healer at <node>" — when bot spots an
        // enemy healer near a node, share the position with the team.
        // 30s lockout per node-entry.
        {
            uint8 my_team_id = s.team();
            for (auto const& e : s.nearby_enemies())
            {
                if (e.role != Role::Healer) continue;
                if (e.hp <= 0) continue;
                // Find the closest node to the enemy healer for context.
                BotSnapshot::BgNodeState const* near_node = nullptr;
                float best_node_dsq = 40.0f * 40.0f;
                for (auto const& n : s.bg_node_states())
                {
                    float dx = n.x - e.x;
                    float dy = n.y - e.y;
                    float dsq = dx * dx + dy * dy;
                    if (dsq < best_node_dsq)
                    { best_node_dsq = dsq; near_node = &n; }
                }
                if (!near_node) break; // healer not near a known node
                const uint64 hkey = (uint64(4) << 32) | uint64(near_node->entry);
                const uint32 hnow = GameTime::GetGameTimeMS();
                if (ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                             hkey, hnow))
                    break;
                // Cross-bot dedup — only one bot per node every 30s.
                if (!BgCalloutCoordinator::TryClaim(/*kind=*/4,
                                                   uint64(near_node->entry),
                                                   hnow,
                                                   /*lockout=*/30u * 1000u))
                    break;
                // Ownership context tells teammates who's at risk.
                char const* ctx = (near_node->owner_team == my_team_id)
                    ? "defending" : "at";
                std::string node_name = near_node->name.empty()
                    ? std::to_string(near_node->entry) : near_node->name;
                std::string msg = "their healer ";
                msg += ctx; msg += " "; msg += node_name;
                if (emit.raid_chat(msg))
                {
                    ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                         hkey, hnow);
                    ai.set_last_rule_fired("idle:bg_callout_enemy_healer");
                    return true;
                }
                break; // one healer-callout per tick
            }
        }

        // BG callout: "heal pls" — when bot drops below 30% HP and a
        // friendly healer is within 30y, request a heal in raid chat.
        // 30s per-bot lockout. Real players type "heal pls" / "heal
        // me" all the time; bots that silently die feel like NPCs.
        if (s.is_alive() && s.hp_pct() < 30)
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            bool healer_nearby = false;
            for (auto const& f : s.nearby_friends())
            {
                if (f.role != Role::Healer) continue;
                if (f.hp <= 0) continue;
                float dx = f.x - self_x;
                float dy = f.y - self_y;
                if (dx * dx + dy * dy < 30.0f * 30.0f)
                { healer_nearby = true; break; }
            }
            if (healer_nearby)
            {
                const uint32 hp_now_ms = GameTime::GetGameTimeMS();
                const uint64 hp_key = (uint64(3) << 32) | uint64(0xBEEFu);
                if (!ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                              hp_key, hp_now_ms))
                {
                    if (emit.raid_chat("heal pls"))
                    {
                        ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                             hp_key, hp_now_ms);
                        ai.set_last_rule_fired("idle:bg_callout_heal");
                        return true;
                    }
                }
            }
        }

        // BG callout: "inc N <node>". Detects enemy proximity to an
        // own-team captured node (the universal "incoming" signal).
        // Fires per bot per node every 20s so the chat stays readable
        // when a node sees sustained pressure. Real players speak;
        // bots without callouts feel like NPCs.
        {
            uint8 my_team_id = s.team();
            for (auto const& n : s.bg_node_states())
            {
                if (n.owner_team != my_team_id) continue;
                if (n.is_contested) continue; // a separate "X contested" callout pending
                int enemy_count = 0;
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.hp <= 0) continue;
                    float dx = e.x - n.x;
                    float dy = e.y - n.y;
                    if (dx * dx + dy * dy < 30.0f * 30.0f) ++enemy_count;
                }
                if (enemy_count < 2) continue;
                // Encode (callout=1, node-entry) into the action key so
                // 20s applies per-node-per-bot.
                const uint64 callout_key = (uint64(1) << 32) | uint64(n.entry);
                const uint32 c_now_ms = GameTime::GetGameTimeMS();
                if (ai.action_recently_tried(BotAI::ActionKind::BgCallout,
                                             callout_key, c_now_ms))
                    continue;
                // Cross-bot dedup — first detector wins.
                if (!BgCalloutCoordinator::TryClaim(/*kind=*/1,
                                                   uint64(n.entry),
                                                   c_now_ms,
                                                   /*lockout=*/20u * 1000u))
                    continue;
                std::string node_name = n.name.empty()
                    ? std::to_string(n.entry) : n.name;
                std::string msg = "inc " + std::to_string(enemy_count) +
                                  " " + node_name;
                if (emit.raid_chat(msg))
                {
                    ai.note_action_retry(BotAI::ActionKind::BgCallout,
                                         callout_key, c_now_ms);
                    ai.set_last_rule_fired("idle:bg_callout_inc");
                    return true;
                }
            }
        }

        // BG CC-on-FC. When the enemy flag carrier is visible nearby
        // and the bot has a class CC (poly / sap / fear / hex / trap)
        // ready, cast it. CC briefly stops the FC's run; teammates
        // catch up. Modern FC carries a damage-reduction aura but is
        // still CC-able for a short window. Bot's own attack target
        // doesn't matter for the CC cast (it's a separate emit).
        if (s.is_alive() && !s.is_casting() &&
            !s.bg_enemy_flag_carrier().IsEmpty())
        {
            const uint32 cc_spell = ClassCC(s.cls(), s.spec());
            if (cc_spell != 0 && s.knows_spell(cc_spell) &&
                s.is_ready(cc_spell))
            {
                ObjectGuid fc = s.bg_enemy_flag_carrier();
                NearbyUnit const* fc_unit = nullptr;
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.guid == fc) { fc_unit = &e; break; }
                }
                if (fc_unit)
                {
                    // Don't reapply CC if the FC is already locked by
                    // a friendly cast — would diminish returns and
                    // shorten our team's effective lockdown.
                    if (is_cc_by_ally(*fc_unit))
                    {
                        // fall through past CC rule
                    }
                    else
                    {
                        float dx = fc_unit->x - self_x;
                        float dy = fc_unit->y - self_y;
                        if (dx * dx + dy * dy < 30.0f * 30.0f)
                        {
                            // 30s lockout per (CC kind, FC guid) so we don't
                            // spam-recast on a target with diminishing
                            // returns active.
                            const uint64 cc_key = (uint64(cc_spell) << 32) |
                                                  uint64(fc.GetCounter() & 0xFFFFFFFFu);
                            const uint32 cc_now = GameTime::GetGameTimeMS();
                            if (!ai.action_recently_tried(BotAI::ActionKind::BgUseGo,
                                                          cc_key, cc_now))
                            {
                                emit.cast(cc_spell, fc);
                                ai.note_action_retry(BotAI::ActionKind::BgUseGo,
                                                     cc_key, cc_now);
                                ai.set_last_rule_fired("idle:bg_cc_enemy_fc");
                                return true;
                            }
                        }
                    }
                }
            }
        }

        // BG healer-heal-FC. When bot is Healer role / spec and the
        // friendly carrier is within 35y at <80% HP, cast the class
        // heal on the carrier. Real-PvP healers prio FC over self
        // and other randos. The class APL handles general heal
        // rotation; this rule is a BG-specific override that pins
        // FC as the heal target.
        if (s.is_alive() && !s.is_casting() &&
            ai.effective_role(s) == Role::Healer &&
            !s.bg_friendly_flag_carrier().IsEmpty() &&
            s.bg_friendly_flag_carrier() != s.guid())
        {
            const uint32 hspell = ClassOocHeal(s.cls(), s.spec());
            if (hspell != 0 && s.knows_spell(hspell) && s.is_ready(hspell))
            {
                ObjectGuid fc = s.bg_friendly_flag_carrier();
                NearbyUnit const* fc_unit = nullptr;
                for (auto const& f : s.nearby_friends())
                {
                    if (f.guid == fc) { fc_unit = &f; break; }
                }
                if (fc_unit && fc_unit->hp_pct() < 80)
                {
                    float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                    s.position(self_x, self_y, self_z);
                    float dx = fc_unit->x - self_x;
                    float dy = fc_unit->y - self_y;
                    if (dx * dx + dy * dy < 35.0f * 35.0f)
                    {
                        emit.cast(hspell, fc);
                        ai.set_last_rule_fired("idle:bg_healer_heal_fc");
                        return true;
                    }
                }
            }
        }

        // BG CC enemies attacking friendly FC. When the bot sees an
        // enemy targeting our flag carrier, lock them out with class
        // CC. Different rule from cc_enemy_fc — that one CCs the
        // carrier itself; this one peels the carrier's chasers.
        if (s.is_alive() && !s.is_casting() &&
            !s.bg_friendly_flag_carrier().IsEmpty() &&
            s.bg_friendly_flag_carrier() != s.guid())
        {
            const uint32 cc_spell = ClassCC(s.cls(), s.spec());
            if (cc_spell != 0 && s.knows_spell(cc_spell) &&
                s.is_ready(cc_spell))
            {
                ObjectGuid friendly_fc = s.bg_friendly_flag_carrier();
                NearbyUnit const* peel_target = nullptr;
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.hp <= 0) continue;
                    if (e.victim != friendly_fc) continue;
                    if (is_cc_by_ally(e)) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    if (dx * dx + dy * dy > 30.0f * 30.0f) continue;
                    peel_target = &e; break;
                }
                if (peel_target)
                {
                    const uint64 peel_key = (uint64(cc_spell) << 32) |
                                            uint64(peel_target->guid.GetCounter() & 0xFFFFFFFFu);
                    const uint32 peel_now = GameTime::GetGameTimeMS();
                    if (!ai.action_recently_tried(BotAI::ActionKind::BgUseGo,
                                                  peel_key, peel_now))
                    {
                        emit.cast(cc_spell, peel_target->guid);
                        ai.note_action_retry(BotAI::ActionKind::BgUseGo,
                                             peel_key, peel_now);
                        ai.set_last_rule_fired("idle:bg_cc_fc_chaser");
                        return true;
                    }
                }
            }
        }

        // BG interrupt-on-heal. When bot has a class interrupt ready
        // and an enemy HEALER is casting an interruptible spell within
        // 30y, kick — even if the bot is mid-combat with a different
        // target. The dungeon interrupt rule above gates on
        // `!in_combat()` which makes sense in PvE but kills BG
        // healer-kicking. This rule complements it in the BG block.
        // Skipped if bot is itself casting (would self-interrupt).
        if (s.is_alive() && !s.is_casting())
        {
            const uint32 ispell = ClassInterrupt(s.cls(), s.spec());
            if (ispell != 0 && s.knows_spell(ispell) && s.is_ready(ispell))
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                NearbyUnit const* casting_healer = nullptr;
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.role != Role::Healer) continue;
                    if (!e.is_casting || !e.is_interruptible) continue;
                    if (e.hp <= 0) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    if (dx * dx + dy * dy > 30.0f * 30.0f) continue;
                    casting_healer = &e; break;
                }
                if (casting_healer)
                {
                    const uint32 ki_now = GameTime::GetGameTimeMS();
                    if (ki_now - ai.last_interrupt_ms() >= 500)
                    {
                        emit.cast(ispell, casting_healer->guid);
                        ai.note_interrupt(ki_now);
                        ai.set_last_rule_fired("idle:bg_interrupt_healer");
                        return true;
                    }
                }
                // No healer cast → kick any enemy CC cast (Polymorph,
                // Fear, Hex etc.) targeting friendly. Real PvP players
                // kick the polly the second it goes out — bots without
                // this rule sit on a CC'd ally whole duration.
                NearbyUnit const* casting_cc = nullptr;
                for (auto const& e : s.nearby_enemies())
                {
                    if (!e.is_casting || !e.is_interruptible) continue;
                    if (e.hp <= 0) continue;
                    // CC mechanics on the SPELL (we don't have spell-info
                    // mechanic in NearbyUnit, but a reasonable proxy:
                    // the cast targets a friendly and is interruptible.
                    // Non-CC interruptible casts on friendlies are rare;
                    // most damage casts on us come back via direct
                    // damage which is fine for the focus_low_hp loop).
                    bool casts_on_friend = false;
                    if (e.victim == s.guid()) casts_on_friend = true;
                    else
                    {
                        for (auto const& f : s.nearby_friends())
                            if (f.guid == e.victim) { casts_on_friend = true; break; }
                    }
                    if (!casts_on_friend) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    if (dx * dx + dy * dy > 30.0f * 30.0f) continue;
                    casting_cc = &e; break;
                }
                if (casting_cc)
                {
                    const uint32 ki_now = GameTime::GetGameTimeMS();
                    if (ki_now - ai.last_interrupt_ms() >= 500)
                    {
                        emit.cast(ispell, casting_cc->guid);
                        ai.note_interrupt(ki_now);
                        ai.set_last_rule_fired("idle:bg_interrupt_cc");
                        return true;
                    }
                }
            }
        }

        // BG offensive burst window. When the bot's victim is sub-30%
        // HP, pop the class burst CD (Recklessness / Combustion /
        // Avenging Wrath / etc) to secure the kill. APL handles
        // generic procs; this is the BG-context "we're killing this
        // target right now, give it everything" trigger. CDs are
        // 1.5-3 min so this fires once per ~2 fights.
        if (s.is_alive() && !s.is_casting() && !s.victim().IsEmpty())
        {
            const uint32 burst = ClassOffensiveBurst(s.cls(), uint16(s.spec()));
            if (burst != 0 && s.knows_spell(burst) && s.is_ready(burst))
            {
                NearbyUnit const* victim_unit = nullptr;
                for (auto const& e : s.nearby_enemies())
                    if (e.guid == s.victim()) { victim_unit = &e; break; }
                if (victim_unit && victim_unit->hp_pct() < 30 &&
                    victim_unit->hp > 0)
                {
                    emit.cast(burst);
                    ai.set_last_rule_fired("idle:bg_offensive_burst");
                    return true;
                }
            }
        }

        // BG Bloodlust burst window. Shaman / Mage / Hunter / Evoker
        // drop their group haste cooldown (Bloodlust / Time Warp /
        // Primal Rage / Fury of the Aspects) when an active fight
        // forms — 2+ friends + 2+ enemies in range. Spell's own 5-
        // min CD prevents spam; the per-spell ready check gates re-
        // application. Real PvP teams call lust mid-fight; bots'
        // APLs sit on it because they don't see the BG-context
        // "we're in a real fight now" signal.
        if (s.is_alive() && !s.is_casting())
        {
            const uint32 lust = ClassLustSpell(s.cls());
            if (lust != 0 && s.knows_spell(lust) && s.is_ready(lust))
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                int n_friends = 0, n_enemies = 0;
                for (auto const& f : s.nearby_friends())
                {
                    if (f.hp <= 0) continue;
                    float dx = f.x - self_x;
                    float dy = f.y - self_y;
                    if (dx * dx + dy * dy < 30.0f * 30.0f) ++n_friends;
                }
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.hp <= 0) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    if (dx * dx + dy * dy < 30.0f * 30.0f) ++n_enemies;
                }
                if (n_friends >= 2 && n_enemies >= 2)
                {
                    emit.cast(lust);
                    ai.set_last_rule_fired("idle:bg_bloodlust");
                    return true;
                }
            }
        }

        // PvP cast-fake. When the bot is mid-cast and an enemy is
        // mid-cast on us with an interrupt spell (Counterspell, Pummel,
        // Kick, Wind Shear, etc.) that will land before our cast
        // finishes, cancel our cast at the last moment so the
        // interrupt eats nothing — kick goes on full CD, bot's
        // school stays unlocked. Real high-end PvP fundamental;
        // separates competent casters from "always get kicked"
        // bots. The window is narrow (200-400ms before kick lands)
        // so this rule fires for at most one tick per kick attempt.
        if (s.is_casting() && s.current_cast_remaining().count() > 200)
        {
            for (auto const& e : s.nearby_enemies())
            {
                if (!e.is_casting_kick) continue;
                if (e.victim != s.guid()) continue;
                const int64 kick_rem = e.cast_remaining.count();
                if (kick_rem <= 0) continue;
                // Cancel only when kick is about to connect and our
                // cast won't finish first naturally.
                if (kick_rem > 400) continue;
                if (kick_rem >= s.current_cast_remaining().count()) continue;
                emit.cancel_cast();
                ai.set_last_rule_fired("idle:bg_cast_fake");
                return true;
            }
        }

        // PvP LoS-aware target switch (ranged). When bot is a ranged
        // spec and current victim is OUT of line of sight, switch to
        // the closest in-LoS enemy. Casts on out-of-LoS targets fail
        // server-side; bots without this just stand there hammering
        // a wall for the whole engagement. Melee specs continue
        // chasing — they'll reposition naturally via melee chase.
        if (s.is_alive() && !s.is_casting() && !s.victim().IsEmpty() &&
            IsRangedSpec(s.cls(), uint16(s.spec())))
        {
            NearbyUnit const* victim_unit = nullptr;
            for (auto const& e : s.nearby_enemies())
                if (e.guid == s.victim()) { victim_unit = &e; break; }
            if (victim_unit && !victim_unit->in_los)
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                NearbyUnit const* alt = nullptr;
                float best_dsq = 40.0f * 40.0f;
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.guid == s.victim()) continue;
                    if (e.hp <= 0) continue;
                    if (!e.in_los) continue;
                    if (is_cc_by_ally(e)) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq) { best_dsq = dsq; alt = &e; }
                }
                if (alt && emit.start_attack(alt->guid))
                {
                    ai.set_last_rule_fired("idle:bg_target_los_switch");
                    return true;
                }
            }
        }

        // PvP target-off-friendly-CC. When the bot's current victim
        // is locked by a friendly CC, switch off — beating on a polly
        // breaks it for free, undoing the CC'er's spell. Pick the
        // closest non-CC'd enemy as replacement. The class APL handles
        // generic combat and won't override this since we emit
        // start_attack which sets a hard target.
        if (s.is_alive() && !s.is_casting() && !s.victim().IsEmpty())
        {
            NearbyUnit const* victim_unit = nullptr;
            for (auto const& e : s.nearby_enemies())
                if (e.guid == s.victim()) { victim_unit = &e; break; }
            if (victim_unit && is_cc_by_ally(*victim_unit))
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                NearbyUnit const* alt = nullptr;
                float best_dsq = std::numeric_limits<float>::max();
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.guid == s.victim()) continue;
                    if (e.hp <= 0) continue;
                    if (is_cc_by_ally(e)) continue;
                    float dx = e.x - self_x;
                    float dy = e.y - self_y;
                    float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq) { best_dsq = dsq; alt = &e; }
                }
                if (alt && emit.start_attack(alt->guid))
                {
                    ai.set_last_rule_fired("idle:bg_avoid_friendly_cc");
                    return true;
                }
            }
        }

        // PvP focus-healer. When alive and an enemy healer is nearby
        // (≤30y), force-target them. Real players burn the healer first;
        // without this, bots beat on whichever DPS happened to wander
        // closer. Skips when the bot already has a healer victim, or
        // is itself FlagCarrier (their job overrides target choice).
        if (s.is_alive() && !s.is_casting() &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier)
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            NearbyUnit const* healer = nullptr;
            float best_dsq = 30.0f * 30.0f;
            const bool i_am_ranged = IsRangedSpec(s.cls(), uint16(s.spec()));
            for (auto const& e : s.nearby_enemies())
            {
                if (e.role != Role::Healer) continue;
                if (e.hp <= 0) continue;
                if (is_cc_by_ally(e)) continue;  // don't break friendly CC
                if (i_am_ranged && !e.in_los) continue; // can't cast through walls
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                float dsq = dx * dx + dy * dy;
                if (dsq < best_dsq) { best_dsq = dsq; healer = &e; }
            }
            if (healer && s.victim() != healer->guid)
            {
                if (emit.start_attack(healer->guid))
                {
                    ai.set_last_rule_fired("idle:bg_focus_healer");
                    return true;
                }
            }
        }

        // PvP retreat-when-outnumbered. When alive at <40% HP and 3+
        // enemies are within 20y while <2 friendlies are within 30y,
        // walk toward the nearest own-team node (live state) — or
        // home_base, or just away from the enemy centroid. Real
        // players don't 1v3; without this, bots feed the enemy team
        // honor by suiciding into a zerg.
        // Skipped for FlagCarrier (FC sometimes has to take the fight
        // to score even outnumbered) and Healers (they retreat via
        // self-heals + LoS, not move-away).
        if (s.is_alive() && !s.is_casting() &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier &&
            my_role != BgRole::Healer &&
            ai.effective_role(s) != Role::Healer &&
            s.hp_pct() < 40)
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            int   close_enemies = 0;
            float ec_x = 0.f, ec_y = 0.f;
            for (auto const& e : s.nearby_enemies())
            {
                if (e.hp <= 0) continue;
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                if (dx * dx + dy * dy < 20.0f * 20.0f)
                { ++close_enemies; ec_x += e.x; ec_y += e.y; }
            }
            int close_friends = 0;
            for (auto const& f : s.nearby_friends())
            {
                if (f.hp <= 0) continue;
                float dx = f.x - self_x;
                float dy = f.y - self_y;
                if (dx * dx + dy * dy < 30.0f * 30.0f) ++close_friends;
            }
            if (close_enemies >= 3 && close_friends < 2)
            {
                // Pick a fall-back point: closest own-team node, then
                // home_base, then "away from enemy centroid".
                float dest_x = 0.f, dest_y = 0.f, dest_z = 0.f;
                bool have_dest = false;
                uint8 my_team_id = s.team();
                for (auto const& n : s.bg_node_states())
                {
                    if (n.owner_team != my_team_id) continue;
                    if (n.is_contested) continue;
                    float dx = n.x - self_x;
                    float dy = n.y - self_y;
                    float dsq = dx * dx + dy * dy;
                    if (!have_dest || dsq < (dest_x - self_x) * (dest_x - self_x) +
                                            (dest_y - self_y) * (dest_y - self_y))
                    {
                        dest_x = n.x; dest_y = n.y; dest_z = n.z;
                        have_dest = true;
                    }
                }
                if (!have_dest &&
                    (bg_advice.home_base_x != 0.f || bg_advice.home_base_y != 0.f))
                {
                    dest_x = bg_advice.home_base_x;
                    dest_y = bg_advice.home_base_y;
                    dest_z = bg_advice.home_base_z;
                    have_dest = true;
                }
                if (!have_dest)
                {
                    // Move away from enemy centroid by 25y.
                    float ec_n = float(close_enemies);
                    float cx = ec_x / ec_n, cy = ec_y / ec_n;
                    float dx = self_x - cx, dy = self_y - cy;
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len < 0.5f) { dx = 1.f; dy = 0.f; len = 1.f; }
                    float scale = 25.0f / len;
                    dest_x = self_x + dx * scale;
                    dest_y = self_y + dy * scale;
                    dest_z = self_z;
                    have_dest = true;
                }
                // Pop class mobility CD (Blink / Disengage / Sprint /
                // Heroic Leap / etc) BEFORE issuing the run-away
                // move_to so the bot actually escapes — without this
                // the slow-walking-while-stunned would just feed.
                const uint32 esc_spell = ClassMobilityEscape(s.cls(), uint16(s.spec()));
                if (esc_spell != 0 && s.knows_spell(esc_spell) &&
                    s.is_ready(esc_spell) && !s.is_stunned())
                {
                    emit.cast(esc_spell);
                    // Don't return — fall through to move_to so the
                    // bot keeps moving the same tick.
                }
                emit.move_to(dest_x, dest_y, dest_z, /*run=*/true);
                ai.set_last_rule_fired("idle:bg_retreat_outnumbered");
                return true;
            }
        }

        // PvP kite-melee. Ranged casters keep distance from melee
        // attackers — real warlocks/mages/hunters disengage and pillar.
        // Fires when bot's spec is ranged AND a melee enemy is within
        // 8y AND bot is alive + not casting + not FC. Steps the bot
        // 25y along the away-vector. Skipped while currently in cast
        // (would interrupt) or while bot itself is melee-spec.
        if (s.is_alive() && !s.is_casting() &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier &&
            IsRangedSpec(s.cls(), uint16(s.spec())))
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            NearbyUnit const* melee_threat = nullptr;
            float best_dsq = 8.0f * 8.0f;
            for (auto const& e : s.nearby_enemies())
            {
                if (e.hp <= 0) continue;
                // Treat any non-Healer enemy in melee range as a kite
                // trigger — healers in melee likely came to body-block,
                // but they're squishy and already covered by focus_healer.
                if (e.role == Role::Healer) continue;
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                float dsq = dx * dx + dy * dy;
                if (dsq < best_dsq)
                { best_dsq = dsq; melee_threat = &e; }
            }
            if (melee_threat)
            {
                // Step ~25y from threat along the bot→threat reverse
                // vector. If stacked on top, pick a default direction.
                float dx = self_x - melee_threat->x;
                float dy = self_y - melee_threat->y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.5f) { dx = 1.f; dy = 0.f; len = 1.f; }
                float scale = 25.0f / len;
                float kite_x = melee_threat->x + dx * scale;
                float kite_y = melee_threat->y + dy * scale;
                emit.move_to(kite_x, kite_y, self_z, /*run=*/true);
                ai.set_last_rule_fired("idle:bg_kite_melee");
                return true;
            }
        }

        // PvP focus low-HP. Real players executable-finish: when 2+
        // enemies in range and one is below 25% HP, switch target.
        // Healer-focus already won above; this catches the "we're
        // killing the warrior, the rogue is at 8% — finish him" case.
        // Skipped for FlagCarrier (their job is mobility, not target
        // shopping) and when current victim is itself <25% (already
        // executing the right target).
        if (s.is_alive() && !s.is_casting() &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier)
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            // Find the lowest-HP enemy within 30y.
            NearbyUnit const* low = nullptr;
            int32 best_pct = 25;
            const bool low_i_ranged = IsRangedSpec(s.cls(), uint16(s.spec()));
            for (auto const& e : s.nearby_enemies())
            {
                if (e.hp <= 0) continue;
                if (is_cc_by_ally(e)) continue;  // don't break friendly CC
                if (low_i_ranged && !e.in_los) continue; // ranged needs LoS
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                if (dx * dx + dy * dy > 30.0f * 30.0f) continue;
                int32 pct = e.hp_pct();
                if (pct < best_pct) { best_pct = pct; low = &e; }
            }
            if (low && s.victim() != low->guid)
            {
                if (emit.start_attack(low->guid))
                {
                    ai.set_last_rule_fired("idle:bg_focus_low_hp");
                    return true;
                }
            }
        }

        // PvP coordinated focus-fire. Real teams converge on a single
        // target via assist macros / "/assist". Without explicit
        // coordination, pick the lowest-GUID alive friend with a
        // valid victim and copy their victim. Deterministic — every
        // bot seeing the same scene picks the same focus, producing
        // emergent team focus without an actual leader designation.
        // Runs AFTER focus_healer / focus_low_hp / focus_fc — those
        // win when the rule's target is more important than whatever
        // the squad is currently hitting. Skipped for FlagCarrier.
        if (s.is_alive() && !s.is_casting() &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier)
        {
            ObjectGuid lowest_friend_guid;
            ObjectGuid focus_target;
            for (auto const& f : s.nearby_friends())
            {
                if (f.hp <= 0) continue;
                if (f.victim.IsEmpty()) continue;
                if (lowest_friend_guid.IsEmpty() ||
                    f.guid.GetCounter() < lowest_friend_guid.GetCounter())
                {
                    lowest_friend_guid = f.guid;
                    focus_target = f.victim;
                }
            }
            if (!focus_target.IsEmpty() && s.victim() != focus_target)
            {
                bool valid = false;
                for (auto const& e : s.nearby_enemies())
                {
                    if (e.guid == focus_target && e.hp > 0)
                    { valid = true; break; }
                }
                if (valid && emit.start_attack(focus_target))
                {
                    ai.set_last_rule_fired("idle:bg_focus_fire");
                    return true;
                }
            }
        }

        // Open-engage: when no friend has flagged a target (focus_fire
        // had nothing to copy) and the bot has no victim, attack the
        // closest enemy in PvP range. Without this, Roamers/Attackers/
        // Defenders walk right past enemy bots because no upstream rule
        // ever set a victim. focus_low_hp only fires below 25% HP, and
        // focus_fire requires another teammate to be engaging first —
        // so combat had to start somewhere. This rule is that "somewhere".
        // Closest within 25y; Healer skipped (they engage via APL assist);
        // FlagCarrier skipped (their job is to RUN the flag, not fight).
        // Coordinator-ordered bots skipped (N60): a bot marching on a
        // DefendNode/EscortFC/PickupFlag order must not be diverted into
        // a roadside fight by this rule — on-station combat is owned by
        // the order block's 30y engage at the ordered post.
        if (s.is_alive() && !s.is_casting() &&
            s.victim().IsEmpty() &&
            s.raw().bg.order.kind == BgState::BgOrder::None &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier &&
            my_role != BgRole::Healer)
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            const bool eng_ranged = IsRangedSpec(s.cls(), uint16(s.spec()));
            NearbyUnit const* closest = nullptr;
            float best_dsq = std::numeric_limits<float>::max();
            for (auto const& e : s.nearby_enemies())
            {
                if (e.hp <= 0) continue;
                if (is_cc_by_ally(e)) continue;
                if (eng_ranged && !e.in_los) continue;
                float dx = e.x - self_x;
                float dy = e.y - self_y;
                float dsq = dx * dx + dy * dy;
                if (dsq > 25.0f * 25.0f) continue;  // PvP engage range
                if (dsq < best_dsq) { best_dsq = dsq; closest = &e; }
            }
            if (closest && emit.start_attack(closest->guid))
            {
                ai.set_last_rule_fired("idle:bg_engage_nearby");
                return true;
            }
        }

        // Flag-carrier chase. Honored only for CTF-style BGs where
        // bg_enemy_flag_carrier is non-empty AND the script asked for
        // chase (chase_enemy_carrier). Defenders/Attackers/Roamers
        // chase; FC and FCEscort (and Healers if there's a friendly
        // carrier they should be hugging) skip.
        // Coordinator-ordered bots skipped (N60): without this gate every
        // hash-role Attacker/Defender/Roamer holding an order still
        // start_attack'd the EFC map-wide here, the whole team dogpiled
        // the carrier exactly as before, and the deliberate 2-3 bot
        // HuntEFC squad (plus every garrison/escort order) was dead code
        // for as long as an enemy carrier existed — most of a CTF match.
        // HuntEFC-ordered bots chase via the order block's own logic.
        if (bg_advice.chase_enemy_carrier && s.is_alive() &&
            !s.is_casting() && !s.bg_enemy_flag_carrier().IsEmpty() &&
            s.raw().bg.order.kind == BgState::BgOrder::None &&
            my_role != BgRole::FlagCarrier &&
            my_role != BgRole::OrbCarrier &&
            my_role != BgRole::FCEscort)
        {
            // Optional gate: chase_melee_only filters out pure-caster
            // classes (Priest 5, Mage 8, Warlock 9, Evoker 13) that
            // can't keep up with a sprinting FC and feed honor when
            // they try. Hybrid classes (Shaman/Druid/Paladin/Monk) are
            // allowed since spec may be melee. Hunter (3) kept in as a
            // ranged class with mobility (Disengage / pet snares).
            bool class_ok = true;
            if (bg_advice.chase_melee_only)
            {
                uint8 c = s.cls();
                class_ok = (c != 5 && c != 8 && c != 9 && c != 13);
            }
            if (class_ok)
            {
                // Carrier selection: when multiple enemy carriers exist
                // (Kotmogu — up to 4 simultaneous orb-bearers), pick one
                // by hashing the bot's GUID against the carrier list so
                // chasers distribute across carriers instead of dogpiling
                // the scalar bg_enemy_flag_carrier (the FIRST one detected
                // by the builder). For single-carrier BGs the vector has
                // 1 entry and this collapses to the existing behavior.
                ObjectGuid target_carrier = s.bg_enemy_flag_carrier();
                auto const& all_carriers = s.bg_all_enemy_carriers();
                if (all_carriers.size() > 1)
                {
                    uint64 guid_low = s.guid().GetCounter();
                    size_t idx = size_t(guid_low % all_carriers.size());
                    target_carrier = all_carriers[idx];
                }
                if (emit.start_attack(target_carrier))
                {
                    ai.set_last_rule_fired("idle:bg_chase_enemy_carrier");
                    return true;
                }
            }
        }

        // ---- Team-coordinator order execution (BG audit N60) ----
        // When the BgTeamCoordinator published an order for this bot, it
        // REPLACES the macro movement below (escort cascade, FC grab,
        // Attacker/Defender/Roamer node logic): march to the ordered
        // target, then fight what's there or hold the post. Everything
        // ABOVE this block (recovery, mount-up, objective-GO auto-use,
        // vehicle play, callouts, CC, focus-healer, retreat) still runs
        // first — orders direct WHERE the bot fights, not HOW it fights.
        // kind == None (coordinator disabled / plan doesn't cover this
        // bot / data cold) falls through to the legacy cascade unchanged.
        if (s.raw().bg.order.kind != BgState::BgOrder::None &&
            s.is_alive() && !s.is_casting() && !s.in_combat())
        {
            using BgOrd = BgState::BgOrder;
            auto const& ord = s.raw().bg.order;
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);

            if (ord.kind == BgOrd::EscortFC)
            {
                // Track the LIVE carrier, not the plan-time coordinates —
                // the carrier has moved since the plan was cut. Stale
                // focus (carrier died) falls through until the next plan.
                ObjectGuid fc = ord.focus.IsEmpty()
                    ? s.bg_friendly_flag_carrier() : ord.focus;
                if (!fc.IsEmpty() && fc != s.guid())
                {
                    float const esc_d =
                        ai.effective_role(s) == Role::Healer ? 12.0f : 5.0f;
                    emit.follow(fc, esc_d);
                    ai.set_last_rule_fired("idle:bg_order_escort");
                    return true;
                }
            }
            else if (ord.kind == BgOrd::HuntEFC)
            {
                ObjectGuid efc = s.bg_enemy_flag_carrier();
                const float ex = s.raw().bg.enemy_carrier_x;
                const float ey = s.raw().bg.enemy_carrier_y;
                const float ez = s.raw().bg.enemy_carrier_z;
                if (!efc.IsEmpty() && (ex != 0.f || ey != 0.f))
                {
                    const float hdx = ex - self_x, hdy = ey - self_y;
                    if (hdx * hdx + hdy * hdy < 40.0f * 40.0f &&
                        emit.start_attack(efc))
                    {
                        ai.set_last_rule_fired("idle:bg_order_hunt_efc");
                        return true;
                    }
                    const uint32 h_now = GameTime::GetGameTimeMS();
                    const uint64 h_key =
                        (uint64(ord.kind) << 48) |
                        (uint64(uint16(int16(ex / 16.f))) << 16) |
                         uint64(uint16(int16(ey / 16.f)));
                    if (!ai.action_recently_tried(
                            BotAI::ActionKind::BgOrderMove, h_key, h_now))
                    {
                        emit.move_to(ex, ey, ez, /*run=*/true);
                        ai.note_action_retry(BotAI::ActionKind::BgOrderMove,
                                             h_key, h_now);
                    }
                    ai.set_last_rule_fired("idle:bg_order_hunt_efc");
                    return true;
                }
                // EFC gone (capped / killed) — stale order; legacy runs
                // until the coordinator's next 750ms plan clears it.
            }
            else if (ord.x != 0.f || ord.y != 0.f)
            {
                // Positional orders: AttackNode / DefendNode / PickupFlag /
                // CarryHome / Regroup / PushEndgame.
                // AV endgame: a PushEndgame order carrying a creature entry
                // (enemy captain Galvangar/Balinda or general Vanndar/Drek)
                // means "march to the boss room AND kill THAT creature".
                // Engage the named creature the moment it's visible — even
                // mid-march — mirroring the live-entry scan in the legacy
                // attacker-endgame rule (the boss can wander out of spawn).
                // If it isn't in range yet, fall through to the march below
                // so bots keep advancing into the boss room.
                if (ord.kind == BgOrd::PushEndgame && ord.target_entry != 0)
                {
                    NearbyUnit const* boss = nullptr;
                    float boss_dsq = std::numeric_limits<float>::max();
                    for (auto const& e : s.nearby_enemies())
                    {
                        if (e.entry != ord.target_entry) continue;
                        if (e.hp <= 0) continue;
                        if (is_cc_by_ally(e)) continue;
                        const float bdx = e.x - self_x, bdy = e.y - self_y;
                        const float bd = bdx * bdx + bdy * bdy;
                        if (bd < boss_dsq) { boss_dsq = bd; boss = &e; }
                    }
                    if (boss)
                    {
                        // ENGAGE-ON-LINE-OF-SIGHT (matches State_InCombat 2d). The
                        // captain/general sits in an OPEN-TOP walled courtyard (live
                        // [avlos] probe: roofed=0, interior within 15y has clear LoS).
                        // The `in_los` raycast is clear ONLY from a position with a
                        // real shot, so engaging the instant LoS exists at combat
                        // range (<=40y, max cast range) lets every bot with a clear
                        // angle damage him — casters from range, melee closing — and
                        // the class AI handles positioning. The old <=6y gate forced
                        // bots to chase the moving captain to point-blank before
                        // attacking, so only a couple ever landed damage.
                        if (boss->in_los && boss_dsq <= 40.0f * 40.0f &&
                            emit.start_attack(boss->guid))
                        {
                            ai.set_last_rule_fired("idle:bg_order_push_boss");
                            return true;
                        }
                        // ENTRANCE STAGING (matches State_InCombat 2d): no LoS yet
                        // and within 80y of a walled boss with a known interior
                        // staging point -> route THROUGH the entrance (a direct
                        // move_to dead-ends in the perimeter wall pockets). Reaching
                        // the staging point grants LoS -> the engage branch fires.
                        {
                            float sx = 0.f, sy = 0.f, sz = 0.f;
                            if (boss_dsq <= 80.0f * 80.0f &&
                                BgBossStagingPoint(ord.target_entry, sx, sy, sz))
                            {
                                const float sdx = sx - self_x, sdy = sy - self_y;
                                if (sdx * sdx + sdy * sdy > 8.0f * 8.0f)
                                {
                                    emit.move_to(sx, sy, sz, /*run=*/true);
                                    ai.set_last_rule_fired("idle:bg_push_endgame_stage");
                                    return true;
                                }
                            }
                        }
                        const uint32 pb_now = GameTime::GetGameTimeMS();
                        const uint64 pb_key =
                            (uint64(ord.kind) << 48) |
                            (uint64(uint16(int16(boss->x / 16.f))) << 16) |
                             uint64(uint16(int16(boss->y / 16.f)));
                        if (!ai.action_recently_tried(
                                BotAI::ActionKind::BgOrderMove, pb_key, pb_now))
                        {
                            emit.move_to(boss->x, boss->y, boss->z, /*run=*/true);
                            ai.note_action_retry(BotAI::ActionKind::BgOrderMove,
                                                 pb_key, pb_now);
                        }
                        ai.set_last_rule_fired("idle:bg_order_push_boss");
                        return true;
                    }
                    // Not visible yet — fall through to the coord march so the
                    // squad keeps pushing toward the boss room.
                }
                float arrive = 8.0f;
                if (ord.kind == BgOrd::DefendNode)  arrive = 12.0f;
                if (ord.kind == BgOrd::PickupFlag)  arrive = 5.0f;
                const float odx = ord.x - self_x, ody = ord.y - self_y;
                if (odx * odx + ody * ody > arrive * arrive)
                {
                    // PushEndgame targets (AV enemy captain / general) sit
                    // 1400-2300y across the map from the home base. A single
                    // raw move_to that far exceeds the pathfinder's partial-path
                    // cap (74 polys / ~292y) and returns no usable path, so the
                    // bot never advances — the squad froze at the 1-tower lead
                    // with PUSH=1 but zero forward motion (live: advance=0,
                    // idle_push=0, score frozen at 75). Cross the distance in
                    // bounded ~35y chunks that each path completely; over many
                    // ticks the squad walks the whole way into the boss room.
                    if (ord.kind == BgOrd::PushEndgame)
                    {
                        if (BgPushThroughNodes(s, ai, emit, bg_advice,
                                               ord.x, ord.y, ord.z,
                                               GameTime::GetGameTimeMS()))
                            return true;
                        // <9y from the boss — fall through to the on-station
                        // objective/engage block so the bot opens on it.
                    }
                    else
                    {
                        // March. Re-emit every 3s (BgOrderMove lockout) toward
                        // the SAME quantized target; a re-plan that moves the
                        // target produces a new key and re-arms instantly.
                        const uint32 mv_now = GameTime::GetGameTimeMS();
                        const uint64 mv_key =
                            (uint64(ord.kind) << 48) |
                            (uint64(uint16(int16(ord.x / 16.f))) << 16) |
                             uint64(uint16(int16(ord.y / 16.f)));
                        if (!ai.action_recently_tried(
                                BotAI::ActionKind::BgOrderMove, mv_key, mv_now))
                        {
                            emit.move_to(ord.x, ord.y, ord.z, /*run=*/true);
                            ai.note_action_retry(BotAI::ActionKind::BgOrderMove,
                                                 mv_key, mv_now);
                        }
                        ai.set_last_rule_fired("idle:bg_order_march");
                        return true;
                    }
                }
                // On station. The objective GO must be actively USED — modern
                // NEW_FLAG pickup and capture-point assault are active Use()
                // calls, NOT server-side proximity (the old comment claiming
                // "standing on the spawn completes the PickupFlag order" was
                // wrong, and was part of why no bot ever grabbed the flag).
                // The top-of-dispatch auto-use pass normally fires first within
                // 12y, but attempt the grab here too so an order that parked
                // the bot right on the pedestal still triggers it. Carriers
                // never self-engage (they run / hold the scoring zone).
                if (ord.kind != BgOrd::CarryHome && !i_carry_bg_flag)
                {
                    if (s.is_alive() && !s.is_casting() &&
                        BgTryUseObjectiveGo(s, ai, emit, bg_advice))
                        return true;
                    NearbyUnit const* foe = nullptr;
                    float foe_dsq = 30.0f * 30.0f;
                    for (auto const& e : s.nearby_enemies())
                    {
                        if (e.hp <= 0) continue;
                        if (is_cc_by_ally(e)) continue;
                        const float fdx = e.x - self_x, fdy = e.y - self_y;
                        const float fd = fdx * fdx + fdy * fdy;
                        if (fd < foe_dsq) { foe_dsq = fd; foe = &e; }
                    }
                    if (foe && emit.start_attack(foe->guid))
                    {
                        ai.set_last_rule_fired("idle:bg_order_engage");
                        return true;
                    }
                }
                ai.set_last_rule_fired("idle:bg_order_hold");
                return true;
            }
        }

        // FC escort. The friendly carrier wants peelers around them.
        // Only FCEscort and Healer roles escort; other roles (Attacker,
        // Defender, Roamer) stay on their objective. The script's
        // `escort_friendly_carrier` flag is a CAPABILITY gate — true for
        // BGs with a carryable flag — but the role still selects WHO
        // escorts. Without the role conjunction, every bot in EotS/WSG
        // would chase the FC and abandon their post.
        bool wants_escort = bg_advice.escort_friendly_carrier &&
                            (my_role == BgRole::FCEscort ||
                             my_role == BgRole::Healer ||
                             // Pure-CTF maps (no nodes to attack): once a
                             // friendly carrier exists, Attackers move WITH
                             // the flag — the previous behavior was loitering
                             // motionless wherever the grab-rush left them,
                             // often inside the enemy base (audit N21).
                             (my_role == BgRole::Attacker &&
                              bg_advice.nodes.empty() &&
                              s.bg_node_states().empty()));
        if (wants_escort && s.is_alive() &&
            !s.is_casting() && !s.in_combat() &&
            !s.bg_friendly_flag_carrier().IsEmpty() &&
            s.bg_friendly_flag_carrier() != s.guid())
        {
            float const escort_dist =
                ai.effective_role(s) == Role::Healer ? 12.0f : 5.0f;
            emit.follow(s.bg_friendly_flag_carrier(), escort_dist);
            ai.set_last_rule_fired("idle:bg_escort_friendly_carrier");
            return true;
        }

        // Healer anchor (BG audit N06/N15/N49/N62/N71): in node-race BGs
        // (AB/BfG/EotS/Deepwind/Silvershard/Seething Shore) the Healer
        // role previously had NO movement rule at all without a flag
        // carrier — healer-spec bots stood at the spawn the whole match.
        // Anchor on the nearest living non-healer teammate: follow keeps
        // the healer inside heal range of the fight wherever the team
        // goes, with zero coordination required.
        if (my_role == BgRole::Healer && s.is_alive() &&
            !s.is_casting() && !s.in_combat() &&
            s.bg_friendly_flag_carrier().IsEmpty())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            ObjectGuid anchor;
            float anchor_dsq = std::numeric_limits<float>::max();
            if (auto const* mem = g.members())
            {
                const uint32 my_map = s.map_id();
                const uint64 my_low = s.guid().GetCounter();
                for (auto const& m : *mem)
                {
                    if (m.guid.GetCounter() == my_low) continue;
                    if (!m.online || m.hp <= 0 || m.map_id != my_map) continue;
                    if (m.role == Role::Healer) continue;  // don't stack healers
                    float dx = m.x - self_x;
                    float dy = m.y - self_y;
                    float dsq = dx * dx + dy * dy;
                    if (dsq < anchor_dsq) { anchor_dsq = dsq; anchor = m.guid; }
                }
            }
            // Only re-anchor when drifting beyond heal range (>30y);
            // inside it, stand free so the heal rotation isn't fighting
            // a follow loop.
            if (!anchor.IsEmpty() && anchor_dsq > 900.0f)
            {
                emit.follow(anchor, 15.0f);
                ai.set_last_rule_fired("idle:bg_healer_anchor");
                return true;
            }
        }

        // FlagCarrier role: head to the enemy flag pickup if we don't
        // have a flag yet (no friendly carrier set). Once carrying,
        // back to own base for the cap.
        // Dynamic handoff: when slot-0 FC dies, allow Roamer / FCEscort
        // / Attacker to step in as backup FC if no friendly carrier
        // exists and the bot is alive and the script defines an enemy
        // flag pickup. Without this, BGs deadlock after the primary FC
        // dies — no slot has the role and the flag stays uncovered.
        // Backup-FC handoff is BOUNDED (audit N20/N64): the old version
        // promoted EVERY Roamer, FCEscort and Attacker simultaneously
        // whenever no friendly carrier existed — the whole team minus
        // healers blob-rushed the enemy flag stand. FCEscorts and
        // Roamers always step up (they live in the FC corridor anyway);
        // Attackers only 1-in-3 by team rank, the rest stay on their
        // objective pressure.
        bool acts_as_fc =
            my_role == BgRole::FlagCarrier ||
            my_role == BgRole::OrbCarrier ||
            (s.bg_friendly_flag_carrier().IsEmpty() &&
             (bg_advice.enemy_flag_x != 0.f || bg_advice.enemy_flag_y != 0.f) &&
             (my_role == BgRole::Roamer ||
              my_role == BgRole::FCEscort ||
              (my_role == BgRole::Attacker && (bg_team_rank % 3u) == 0u)));
        if (acts_as_fc && s.is_alive() &&
            !s.is_casting() && !s.in_combat())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            // No friendly carrier → I'm the designated grabber. Walk
            // to enemy flag. For OrbCarrier (Kotmogu), multiple bots
            // can hold "no friendly carrier" simultaneously because the
            // BG has 4 orbs — the snapshot's scalar bg_friendly_flag_carrier
            // tracks one carrier at a time, so OrbCarrier slots after the
            // first will see the scalar as set but still need to grab
            // their own orb. We forward to the grab path regardless of
            // scalar state for OrbCarrier.
            bool need_grab = s.bg_friendly_flag_carrier().IsEmpty() ||
                             my_role == BgRole::OrbCarrier;
            if (need_grab &&
                (bg_advice.enemy_flag_x != 0.f || bg_advice.enemy_flag_y != 0.f))
            {
                float dx = bg_advice.enemy_flag_x - self_x;
                float dy = bg_advice.enemy_flag_y - self_y;
                if (dx * dx + dy * dy > 25.0f)  // 5y arrival radius
                {
                    emit.move_to(bg_advice.enemy_flag_x,
                                 bg_advice.enemy_flag_y,
                                 bg_advice.enemy_flag_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_fc_grab_flag");
                    return true;
                }
            }
            // OrbCarrier (Kotmogu): after pickup, hold at home_base
            // (map center) for the SmallAura distance-from-center score
            // multiplier. No "return home to cap" phase.
            //
            // Important: the SCALAR `bg_friendly_flag_carrier()` only
            // tracks the FIRST detected carrier. Kotmogu has up to 4
            // simultaneous orb-bearers, so carriers 2-4 would fail a
            // scalar `== s.guid()` check and re-enter the grab path,
            // perpetually trying to grab instead of holding. Use the
            // multi-carrier vector for membership.
            bool i_am_orb_carrier = false;
            for (auto const& g : s.bg_all_friendly_carriers())
                if (g == s.guid()) { i_am_orb_carrier = true; break; }
            if (my_role == BgRole::OrbCarrier && i_am_orb_carrier &&
                (bg_advice.home_base_x != 0.f || bg_advice.home_base_y != 0.f))
            {
                float dx = bg_advice.home_base_x - self_x;
                float dy = bg_advice.home_base_y - self_y;
                if (dx * dx + dy * dy > 100.0f)  // 10y hold radius
                {
                    emit.move_to(bg_advice.home_base_x,
                                 bg_advice.home_base_y,
                                 bg_advice.home_base_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_orb_hold_center");
                    return true;
                }
            }
            // FlagCarrier role: head back for the cap. Routed through the
            // SHARED carrier-homeward resolver so the idle path gets the SAME
            // full destination cascade the combat path already had:
            //   1. static own_flag pedestal (WSG / TP / BfG)
            //   2. closest owned node (EotS — cap at any owned tower)
            //   3. home_base (DHR — deliver the crystal to the faction base
            //      capture AreaTrigger; the inline version had no home_base
            //      fallback so DHR carriers stalled at {0,0} forever)
            //   4. friendly-cluster shadow (EotS zero-tower case, BG audit N30)
            // OrbCarriers are intentionally excluded — they hold at center
            // (handled by the orb-hold branch above), they don't run a flag
            // home. BgCarrierHomeward does its own vector-membership carrier
            // check, so non-carriers fall through.
            if (my_role != BgRole::OrbCarrier &&
                BgCarrierHomeward(s, ai, emit, bg_advice))
                return true;
        }

        // [REMOVED] Arena formation (leader follow when nodes empty).
        // Per user directive: in BGs, bots must NOT chain-follow a
        // specific leader — behavior is strategy- and task-bound, with
        // dynamic logical subgroups (FC + escorts, Healer + FC).
        // The old `nodes.empty()` arena gate also misfired for unmapped
        // BG variants (DOM_AB id 1018, Brawl variants, etc.) where the
        // script lookup returned no advice — every bot clumped on the
        // lowest-GUID friend and never moved to objectives.
        // For true arenas (2v2/3v3): assist / focus_fire rules above
        // and the InCombat APL already keep the team coordinated
        // through target acquisition; no formation follow is needed
        // for that small a roster.

        // Arena positional intelligence — pillars / hazards / opening
        // rally. Gated on the script populating at least one of those
        // fields, so non-arena BGs see zero cost (vector empty checks).
        if (s.is_alive() && !s.is_casting() && !s.in_combat() &&
            (!bg_advice.arena_pillars.empty() ||
             !bg_advice.arena_hazards.empty() ||
             bg_advice.opening_rally_x != 0.f ||
             bg_advice.opening_rally_y != 0.f))
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            // Opening rally: while gates still up OR match elapsed
            // is brief (start_delay_ms running), advance to the rally
            // coord so bots don't dawdle in the starting cage AABB.
            if ((bg_advice.opening_rally_x != 0.f ||
                 bg_advice.opening_rally_y != 0.f) &&
                s.bg_start_delay_ms() > 0)
            {
                float dx = bg_advice.opening_rally_x - self_x;
                float dy = bg_advice.opening_rally_y - self_y;
                if (dx * dx + dy * dy > 100.0f)  // 10y arrival
                {
                    emit.move_to(bg_advice.opening_rally_x,
                                 bg_advice.opening_rally_y,
                                 bg_advice.opening_rally_z, /*run=*/false);
                    ai.set_last_rule_fired("idle:arena_opening_rally");
                    return true;
                }
            }
            // Hazard avoidance — if standing inside an active hazard
            // radius, walk to the nearest pillar (or just step out if
            // no pillars defined). Time-gated activation (RoV elevator
            // pillars rise ~60s in).
            //
            // active_after/until checks: wall-clock ms since gates dropped.
            // The previous formula `(900 - time_remaining_sec) * 1000` was
            // broken — TC's `GetRemainingTime()` returns 0 during active
            // IN_PROGRESS play (it's the END-of-match countdown, not the
            // total-match clock), so elapsed_ms saturated at 900000 ms and
            // RoV pillars were treated as permanently active. Snapshot's
            // bg_in_progress_ms reads TC's GetInProgressDuration directly.
            uint32 elapsed_ms = s.bg_in_progress_ms();
            for (auto const& h : bg_advice.arena_hazards)
            {
                if (h.active_after_ms > 0 && elapsed_ms < h.active_after_ms) continue;
                if (h.active_until_ms > 0 && elapsed_ms > h.active_until_ms) continue;
                float dx = h.x - self_x;
                float dy = h.y - self_y;
                if (dx * dx + dy * dy >= h.radius * h.radius) continue;
                // Inside hazard. Find nearest pillar (fallback: step
                // along bot→hazard reverse vector by hazard radius+5y).
                float dest_x = 0.f, dest_y = 0.f, dest_z = h.z;
                float best_dsq = std::numeric_limits<float>::max();
                for (auto const& p : bg_advice.arena_pillars)
                {
                    float pdx = p.x - self_x;
                    float pdy = p.y - self_y;
                    float pdsq = pdx * pdx + pdy * pdy;
                    if (pdsq < best_dsq)
                    {
                        best_dsq = pdsq;
                        dest_x = p.x; dest_y = p.y; dest_z = p.z;
                    }
                }
                if (best_dsq == std::numeric_limits<float>::max())
                {
                    // No pillar — step the radius+5y back along the
                    // reverse-vector. Hazard center to bot:
                    float rx = self_x - h.x;
                    float ry = self_y - h.y;
                    float len = std::sqrt(rx * rx + ry * ry);
                    if (len < 0.5f) { rx = 1.f; ry = 0.f; len = 1.f; }
                    float scale = (h.radius + 5.0f) / len;
                    dest_x = h.x + rx * scale;
                    dest_y = h.y + ry * scale;
                }
                emit.move_to(dest_x, dest_y, dest_z, /*run=*/true);
                ai.set_last_rule_fired("idle:arena_avoid_hazard");
                return true;
            }
        }

        // Roamer role: fire-fighter. Priority:
        //   1. Closest contested node (live state) — flip in progress.
        //   2. Closest neutral node (live state) — easiest cap.
        //   3. Script `nodes` round-robin via guid+30s-bucket hash —
        //      keeps roamers moving on BGs without CAPTURE_POINT state
        //      (Silvershard, ToK orb corners, WSG). Different roamers
        //      pick different nodes; the bucket flips every 30s so each
        //      roamer also rotates targets over time.
        if (my_role == BgRole::Roamer && s.is_alive() &&
            !s.is_casting() && !s.in_combat())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            BotSnapshot::BgNodeState const* target = nullptr;
            float best_dsq = std::numeric_limits<float>::max();
            int   best_priority = 99;
            // Owner-aware (BG audit N22): contested owner_team = the
            // team DOING the flip. Enemy flips are true defense -- worth
            // any distance. OUR OWN flips only get reinforced within 80y
            // (the attackers there don't need the whole roamer corps);
            // beyond that the roamer keeps patrolling.
            uint8 my_team_id = s.team();
            for (auto const& n : s.bg_node_states())
            {
                float dx = n.x - self_x;
                float dy = n.y - self_y;
                float dsq = dx * dx + dy * dy;
                int prio;
                if (n.is_contested && n.owner_team != my_team_id)
                    prio = 0;                       // enemy mid-flip: fight it
                else if (n.owner_team == 0)
                    prio = 1;                       // neutral: free cap
                else if (n.is_contested && dsq <= 80.0f * 80.0f)
                    prio = 2;                       // our nearby flip: back it up
                else if (n.owner_team != my_team_id)
                    prio = 3;                       // fully enemy-held: go flip
                                                    // it. LOWEST priority so a
                                                    // contested / neutral node
                                                    // always wins, but without
                                                    // this a Roamer ignored an
                                                    // enemy-controlled SM cart
                                                    // entirely (BG audit §2 —
                                                    // SM is all-Roamer, so a
                                                    // held enemy cart was never
                                                    // contested). owner==mine
                                                    // (held / far-flip) still
                                                    // falls through.
                else
                    continue;
                if (prio < best_priority ||
                   (prio == best_priority && dsq < best_dsq))
                {
                    best_priority = prio;
                    best_dsq = dsq;
                    target = &n;
                }
            }
            if (target && best_dsq > 100.0f)  // 10y arrival
            {
                emit.move_to(target->x, target->y, target->z, /*run=*/true);
                ai.set_last_rule_fired("idle:bg_roamer_contested");
                return true;
            }
            if (!target && !bg_advice.nodes.empty())
            {
                uint64 guid_low = s.guid().GetCounter();
                // 120s bucket (BG audit N51: at 30s the destination
                // re-rolled MID-ROUTE on most cross-map runs -- bots
                // zig-zagged between far objectives and never arrived;
                // 120s exceeds any normal-BG corner-to-corner run).
                uint32 bucket   = s.published_at_ms() / 120000u;
                uint32 hash     = uint32(guid_low ^ bucket);
                auto const& n = bg_advice.nodes[hash % bg_advice.nodes.size()];
                float nx, ny, nz;
                resolve_node_pos(n, nx, ny, nz);
                if (nx != 0.f || ny != 0.f)
                {
                    float dx = nx - self_x;
                    float dy = ny - self_y;
                    if (dx * dx + dy * dy > 100.0f)
                    {
                        emit.move_to(nx, ny, nz, /*run=*/true);
                        ai.set_last_rule_fired("idle:bg_roamer_patrol");
                        return true;
                    }
                }
            }
            // CTF Roamer fallback: BGs like Twin Peaks / Warsong Gulch
            // have flags, not capturable nodes, so `bg_advice.nodes` is
            // empty and the round-robin above does nothing. A Roamer in
            // CTF should hold mid-map to intercept enemy carriers and
            // back up FC retrieval. We compute a patrol point on the
            // line between own_flag and enemy_flag with a guid-based
            // offset so Roamers spread along the corridor instead of
            // stacking on the midpoint.
            if (!target && bg_advice.nodes.empty() &&
                (bg_advice.own_flag_x != 0.f || bg_advice.own_flag_y != 0.f) &&
                (bg_advice.enemy_flag_x != 0.f || bg_advice.enemy_flag_y != 0.f))
            {
                // Range 0.35 .. 0.65 along the corridor (mid-third) so the
                // Roamer stays between flags without crashing the enemy base.
                const uint64 gh = s.guid().GetCounter();
                const float frac = 0.35f + 0.30f * float(gh % 100) / 99.0f;
                const float px = bg_advice.own_flag_x +
                                 frac * (bg_advice.enemy_flag_x - bg_advice.own_flag_x);
                const float py = bg_advice.own_flag_y +
                                 frac * (bg_advice.enemy_flag_y - bg_advice.own_flag_y);
                const float pz = bg_advice.own_flag_z +
                                 frac * (bg_advice.enemy_flag_z - bg_advice.own_flag_z);
                const float dx = px - self_x;
                const float dy = py - self_y;
                if (dx * dx + dy * dy > 225.0f)  // 15y settle zone
                {
                    emit.move_to(px, py, pz, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_roamer_ctf_patrol");
                    return true;
                }
            }
        }

        // Defender role: defend a specific position. Priority:
        //   1. Live state — defend the closest OWN-team node (from
        //      bg_node_states). Distributes defenders organically
        //      across captured nodes by proximity.
        //   2. home_base — single faction-aware coordinate. Used by
        //      AV/IoC where the "home" is the starting fortress, NOT
        //      one of the capturable nodes.
        //   3. Script nodes round-robin — fallback when no live state
        //      and no home_base (CTF-only / boot-before-snapshot).
        if (my_role == BgRole::Defender && s.is_alive() &&
            !s.is_casting() && !s.in_combat())
        {
            float dest_x = 0.f, dest_y = 0.f, dest_z = 0.f;
            bool have_dest = false;
            // Live-state assignment with priority buckets. SEMANTICS (BG
            // audit N02/N13/N18/N48): a contested node's owner_team is
            // the team DOING the flip (builder maps ContestedAlliance→1,
            // ContestedHorde→2 — the capture-point worldstates name the
            // assaulting side; there is no "previous owner" in the state).
            //   * owner_team == ENEMY && is_contested → enemy mid-capture
            //     (on our node or denying a re-take) — RUSH to interrupt.
            //     The previous `owner_team != mine → continue` filter
            //     discarded exactly these, so defenders structurally
            //     could not react to an assault: the "bots don't stop
            //     caps" complaint was baked into the rule.
            //   * owner_team == MINE && is_contested → OUR offensive flip
            //     in progress. The old code ranked this "RUSH" — the
            //     whole defense squad deserted held bases to dogpile our
            //     own attack. Skip it; attackers finish their own flips.
            //   * owner_team == MINE && !is_contested → held — anchor.
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                uint8 my_team_id = s.team();
                BotSnapshot::BgNodeState const* target = nullptr;
                // Stop-the-cap tier: closest enemy mid-flip — everyone
                // converges (interrupting a cap beats spread). Hold tier:
                // spread defenders across ALL held nodes by team rank
                // (audit N19: closest-own-node from a common spawn glued
                // the whole defense squad to one base forever).
                float best_dsq = std::numeric_limits<float>::max();
                std::vector<BotSnapshot::BgNodeState const*> held;
                BotSnapshot::BgNodeState const* threatened = nullptr;
                float threat_dsq = std::numeric_limits<float>::max();
                for (auto const& n : s.bg_node_states())
                {
                    if (n.is_contested)
                    {
                        if (n.owner_team == my_team_id) continue;  // our flip — attackers' job
                        float dx = n.x - self_x;
                        float dy = n.y - self_y;
                        float dsq = dx * dx + dy * dy;
                        if (dsq < best_dsq)                        // enemy mid-cap: interrupt!
                        {
                            best_dsq = dsq;
                            target = &n;
                        }
                    }
                    else if (n.owner_team == my_team_id)
                    {
                        // Pre-emptive reinforce (BG audit N66): an own
                        // node with enemy players closing in outranks an
                        // idle hold — defense arrives BEFORE the flip
                        // starts instead of reacting to is_contested.
                        const uint8 foes = (my_team_id == 1)
                            ? n.horde_players_near : n.alliance_players_near;
                        const uint8 own  = (my_team_id == 1)
                            ? n.alliance_players_near : n.horde_players_near;
                        if (foes > 0 && foes + 1 > own)
                        {
                            float dx = n.x - self_x;
                            float dy = n.y - self_y;
                            float dsq = dx * dx + dy * dy;
                            if (dsq < threat_dsq)
                            {
                                threat_dsq = dsq;
                                threatened = &n;
                            }
                        }
                        held.push_back(&n);                        // ours: hold candidate
                    }
                }
                if (!target) target = threatened;
                if (!target && !held.empty())
                    target = held[bg_team_rank % held.size()];
                if (target)
                {
                    dest_x = target->x; dest_y = target->y; dest_z = target->z;
                    have_dest = true;
                }
            }
            // Fallback 2: faction home base.
            if (!have_dest &&
                (bg_advice.home_base_x != 0.f || bg_advice.home_base_y != 0.f))
            {
                dest_x = bg_advice.home_base_x;
                dest_y = bg_advice.home_base_y;
                dest_z = bg_advice.home_base_z;
                have_dest = true;
            }
            // Fallback 3: script nodes round-robin by GUID hash. The
            // previous version used formation_slot only, which for JIT
            // bots is always 0 — every Defender stacked on nodes[0].
            // Hashing on the bot's GUID spreads Defenders across the
            // full node list deterministically. Hashing on guid alone
            // (no time bucket) keeps the SAME bot at the SAME node so
            // they don't ping-pong between nodes every 30s.
            if (!have_dest && !bg_advice.nodes.empty())
            {
                const uint64 gh = s.guid().GetCounter();
                auto const& n = bg_advice.nodes[gh % bg_advice.nodes.size()];
                resolve_node_pos(n, dest_x, dest_y, dest_z);
                have_dest = (dest_x != 0.f || dest_y != 0.f);
            }
            if (have_dest)
            {
                float self_x = 0.f, self_y = 0.f, self_z = 0.f;
                s.position(self_x, self_y, self_z);
                float dx = dest_x - self_x;
                float dy = dest_y - self_y;
                if (dx * dx + dy * dy > 225.0f)  // 15y hold zone
                {
                    emit.move_to(dest_x, dest_y, dest_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_defender_hold_home");
                    return true;
                }
            }
        }

        // Attacker role: prefer under-defended enemy/neutral nodes
        // using live ownership state from the snapshot's
        // bg_node_states. Priority order:
        //   1. A neutral node (owner_team == 0) — easiest cap.
        //   2. An enemy-held node we haven't claimed yet.
        //   3. A contested node (mid-flip) — finish the cap.
        // Within each category, pick the closest one to spread bots
        // organically. Falls back to script's `nodes` round-robin
        // hash when bg_node_states is empty (CTF-only BGs / boot-
        // before-first-snapshot).
        if (my_role == BgRole::Attacker && s.is_alive() &&
            !s.is_casting() && !s.in_combat())
        {
            float self_x = 0.f, self_y = 0.f, self_z = 0.f;
            s.position(self_x, self_y, self_z);
            // Endgame redirect — when the script defines an endgame
            // target AND the match is in AllIn bias OR own-team score
            // is critically low, push to the endgame coord (enemy
            // boss / fortress) instead of nodes. This is how AV is
            // actually won in the late game — node ownership doesn't
            // matter once one side's reinforcements drop, the boss
            // kill ends the match. endgame_unconditional overrides the
            // bias gate for round-based BGs whose score stays 0/0 (SoTA:
            // attackers must drive to the breach / relic regardless of a
            // bias that can never leave Normal — BG audit SoTA blocker).
            if ((bg_advice.endgame_target_x != 0.f ||
                 bg_advice.endgame_target_y != 0.f) &&
                (bias == Bias_AllIn || bg_advice.endgame_unconditional))
            {
                // Live-unit chase: if the script named a boss entry AND
                // it's visible in nearby_enemies, prefer the unit's live
                // position over the static coord. Handles bosses that
                // walk / are pulled away from spawn (IoC General routinely
                // pathing into adjacent rooms; AV Drek/Vandar wandering
                // inside the keep). When no live unit visible, falls back
                // to the static coord (typical case until bot is in range).
                float dest_x = bg_advice.endgame_target_x;
                float dest_y = bg_advice.endgame_target_y;
                float dest_z = bg_advice.endgame_target_z;
                if (bg_advice.endgame_creature_entry != 0)
                {
                    // Pick the CLOSEST matching creature, not the first.
                    // nearby_enemies is not guaranteed sorted by distance for
                    // BG iteration, and "first" can be 100+ y away while a
                    // closer instance sits next to the bot. Snapping to the
                    // closest match keeps the move_to short and predictable.
                    float best_dsq = std::numeric_limits<float>::max();
                    for (auto const& u : s.nearby_enemies())
                    {
                        if (u.entry != bg_advice.endgame_creature_entry) continue;
                        if (u.hp <= 0) continue;
                        const float dx = u.x - self_x;
                        const float dy = u.y - self_y;
                        const float dsq = dx * dx + dy * dy;
                        if (dsq < best_dsq)
                        {
                            best_dsq = dsq;
                            dest_x = u.x; dest_y = u.y; dest_z = u.z;
                        }
                    }
                }
                float dx = dest_x - self_x;
                float dy = dest_y - self_y;
                if (dx * dx + dy * dy > 400.0f)  // 20y arrival
                {
                    emit.move_to(dest_x, dest_y, dest_z, /*run=*/true);
                    ai.set_last_rule_fired("idle:bg_attacker_endgame");
                    return true;
                }
            }
            uint8 my_team_id = s.team();  // 1 = alliance, 2 = horde
            BotSnapshot::BgNodeState const* target = nullptr;
            float best_dsq = std::numeric_limits<float>::max();
            int   best_priority = 99;  // lower = better
            uint8 best_weight = 0;     // higher = better (tie-breaker)
            // Helper: look up the script-side priority weight by matching
            // the live node's (x, y) to the script's nodes[] entries
            // within a 5y tolerance. Returns 0 when no match (default).
            // O(N²) over node lists, both ≤16 → ~256 fp ops per Attacker
            // tick — negligible compared to the move-to cost.
            auto node_weight_for = [&](float nx, float ny) -> uint8 {
                for (auto const& sn : bg_advice.nodes)
                {
                    float ddx = sn.x - nx;
                    float ddy = sn.y - ny;
                    if (ddx * ddx + ddy * ddy < 25.0f) return sn.priority;
                }
                return 0;
            };
            // Contested semantics (BG audit N48): owner_team of a contested
            // node = the team DOING the flip, not the prior holder.
            // Collect every viable target, then spread attackers across
            // the best-priority tier by team rank (audit N14/N50): the
            // old global argmax sent EVERY attacker to the same node —
            // a 5-cap AB opening became one blob at the closest base
            // while four nodes sat untouched.
            struct AtkCand
            {
                BotSnapshot::BgNodeState const* node;
                int prio; uint8 weight; float dsq;
            };
            std::vector<AtkCand> atk_cands;
            for (auto const& n : s.bg_node_states())
            {
                int prio = 99;
                if (n.owner_team == 0)
                    prio = 0;  // neutral, nobody flipping — easiest cap
                else if (n.owner_team != my_team_id && !n.is_contested)
                    prio = 1;  // enemy held — start a flip
                else if (n.owner_team != my_team_id && n.is_contested)
                    prio = 2;  // ENEMY mid-flip — deny (defenders rush these
                               // at their prio 0; attackers only as last pick)
                else
                    continue;  // ours (held, or our own flip in progress) —
                               // skip: don't dogpile our own assault (N14)
                float dx = n.x - self_x;
                float dy = n.y - self_y;
                atk_cands.push_back({ &n, prio, node_weight_for(n.x, n.y),
                                      dx * dx + dy * dy });
            }
            if (!atk_cands.empty())
            {
                // Priority bucket wins first. Within a bucket, high weight
                // (e.g. AV towers @ priority=2) wins. Within same weight,
                // closest wins. The weight bias is what turns "attack the
                // closest enemy node" into "attack the closest TOWER" when
                // a tower is available — drain reinforcements faster.
                std::sort(atk_cands.begin(), atk_cands.end(),
                          [](AtkCand const& a, AtkCand const& b)
                          {
                              if (a.prio   != b.prio)   return a.prio   < b.prio;
                              if (a.weight != b.weight) return a.weight > b.weight;
                              return a.dsq < b.dsq;
                          });
                // Spread across the best-priority tier: rank k of the
                // team takes the (k mod tier-size)-th candidate. Single-
                // candidate tiers still converge everyone (correct when
                // only one node matters).
                size_t tier_end = 1;
                while (tier_end < atk_cands.size() &&
                       atk_cands[tier_end].prio == atk_cands[0].prio)
                    ++tier_end;
                AtkCand const& pick = atk_cands[bg_team_rank % tier_end];
                best_priority = pick.prio;
                best_weight   = pick.weight;
                best_dsq      = pick.dsq;
                target        = pick.node;
            }
            if (target && best_dsq > 100.0f)  // 10y arrival
            {
                emit.move_to(target->x, target->y, target->z, /*run=*/true);
                ai.set_last_rule_fired("idle:bg_attacker_undefended_node");
                return true;
            }
            // Fallback: round-robin through script's per-script `nodes`
            // when no live state available (no CAPTURE_POINT GOs on
            // this BG map, e.g. WSG or before snapshot population).
            if (!target && !bg_advice.nodes.empty())
            {
                uint64 guid_low = s.guid().GetCounter();
                // 120s bucket (BG audit N51: at 30s the destination
                // re-rolled MID-ROUTE on most cross-map runs -- bots
                // zig-zagged between far objectives and never arrived;
                // 120s exceeds any normal-BG corner-to-corner run).
                uint32 bucket   = s.published_at_ms() / 120000u;
                uint32 hash     = uint32(guid_low ^ bucket);
                auto const& n = bg_advice.nodes[hash % bg_advice.nodes.size()];
                float nx, ny, nz;
                resolve_node_pos(n, nx, ny, nz);
                if (nx != 0.f || ny != 0.f)
                {
                    float dx = nx - self_x;
                    float dy = ny - self_y;
                    if (dx * dx + dy * dy > 100.0f)
                    {
                        emit.move_to(nx, ny, nz, /*run=*/true);
                        ai.set_last_rule_fired("idle:bg_attacker_push_node");
                        return true;
                    }
                }
            }
        }

        }  // end if (!bg_in_prep)

        // Hard stop: a bot inside a battleground must never fall through
        // to world idle rules below (wander, travel_to_hub, quest path,
        // engage_nearby_mob outside the BG, etc.). Those rules emit
        // move_to with destinations OUTSIDE the BG playable area —
        // pathfinding fails, the spline falls back to a straight line,
        // and the bot walks through the BG start gates / out of bounds
        // and disappears off the map. Two TP bots ("Ulethius" et al.)
        // were observed doing exactly this during prep phase. The BG
        // block above is the complete behavior set for bots in BG; if
        // nothing matched on this tick, the right answer is "do
        // nothing" (stay put), not "go do world chores".
        return true;
}
// ---------- Shared UnifiedTravelGraph route executor (free function) ----------
// Extracted from the driveTravelPlanTo lambda formerly local to AutoactDispatch
// so the registered idle rules (idle:far_same_map_travel @697) can reuse the
// SAME travel-plan executor instead of duplicating the leg-walk logic. All
// captured state became explicit params (s/ai/emit/bx/by/bz + to_map/tx/ty/tz);
// the body is verbatim. The nested walkTo lambda is preserved as-is.
bool DriveTravelPlanTo(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float bx, float by, float bz,
                       uint32 to_map, float tx, float ty, float tz)
{
            using namespace ::Playerbot::V2::Travel;
            // MUST mirror the builder's BridgeGoalKey exactly (incl. the
            // 16-yard quantization) — the snapshot's bridge_route_goal_key is
            // computed with that formula, and a mismatch here would silently
            // disable the validated-legs handoff.
            const uint64 goalKey =
                (uint64(to_map) << 42) ^
                (uint64((uint32(int32(tx)) >> 4) & 0x1FFFFF) << 21) ^
                (uint64((uint32(int32(ty)) >> 4) & 0x1FFFFF));
            if (ai.travel_plan_goal_key() != goalKey)
            {
                std::vector<BotAI::PlanLeg> legs;
                // Prefer the builder's VALIDATED route from the snapshot when one
                // is published for this exact goal: its source attaches were
                // checked against the live navmesh on the world thread, which
                // this worker-side FindRoute cannot do. Recomputing here would
                // re-derive the euclidean walk-only route a pocket-wedged bot
                // (Undercity interior) provably can't follow.
                if (s.bridge_route_goal_key() == goalKey && !s.bridge_route().empty())
                {
                    for (auto const& bl : s.bridge_route())
                    {
                        BotAI::PlanLeg pl;
                        pl.kind   = bl.kind;
                        pl.to_map = bl.to_map;
                        pl.to_x = bl.to_x; pl.to_y = bl.to_y; pl.to_z = bl.to_z;
                        pl.from_x = bl.from_x; pl.from_y = bl.from_y; pl.from_z = bl.from_z;
                        pl.payload = bl.payload; pl.to_taxi_node = bl.to_taxi_node;
                        legs.push_back(pl);
                    }
                }
                else if (Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid))
                {
                    RouteRequest req{};
                    req.bot = self; req.from_map = s.map_id();
                    req.from_x = bx; req.from_y = by; req.from_z = bz;
                    req.to_map = to_map; req.to_x = tx; req.to_y = ty; req.to_z = tz;
                    req.allow_hearth = false;   // hearth would lose travel progress
                    req.skip_trivial = ai.travel_force_graph();
                    Route route = Services::TravelGraph().FindRoute(req);
                    if (route.ok && !route.legs.empty())
                    {
                        auto& g = Services::TravelGraph();
                        for (RouteLeg const& leg : route.legs)
                        {
                            BotAI::PlanLeg pl;
                            pl.kind    = uint8(leg.kind);
                            pl.to_map  = leg.to_map;
                            pl.to_x    = leg.to_x; pl.to_y = leg.to_y; pl.to_z = leg.to_z;
                            pl.payload = leg.payload_id;
                            if (GraphNode const* fn = g.GetNode(leg.from_node))
                            { pl.from_x = fn->x; pl.from_y = fn->y; pl.from_z = fn->z; }
                            if (leg.kind == EdgeKind::Taxi)
                                if (GraphNode const* tn = g.GetNode(leg.to_node))
                                    pl.to_taxi_node = tn->payload_id;
                            legs.push_back(pl);
                        }
                    }
                }
                {
                    // Plan-plant diagnostic: one line per planted plan so a
                    // misrouted journey can be attributed to its exact legs
                    // (emitter ambiguity cost hours on the Somi UC case).
                    std::string plan_str;
                    for (auto const& l : legs)
                        plan_str += fmt::format(" k{}->({:.0f},{:.0f},{:.0f})m{}",
                                                uint32(l.kind), l.to_x, l.to_y,
                                                l.to_z, l.to_map);
                    TC_LOG_INFO("playerbot.v2",
                        "[travel_plan] bot={} goal=({},{:.0f},{:.0f}) legs={}:{}",
                        s.bot_id(), to_map, tx, ty, legs.size(), plan_str);
                }
                // No route to a CROSS-MAP quest objective (a complete breadcrumb
                // whose ender sits on an unreachable map): blacklist the objective
                // (5-min) so the picker ROTATES to a reachable one instead of
                // parking the bot in ambient_emote forever. Observed 2026-06-19:
                // Tindle's Q56185 ender is on map 1929 (FindRoute legs=0), and it
                // kept hijacking the objective from the reachable Q270 (Dun Morogh,
                // 4-leg route to the SW flight master) — so the bot never sustained
                // travel out of the Stormwind harbor. Cross-map only: an empty
                // SAME-map plan is the trivial direct-walk branch, not a routing
                // failure, and must not blacklist a reachable local goal.
                if (legs.empty() && s.current_quest_id() != 0 && to_map != s.map_id())
                {
                    ai.blacklist_objective_now(s.current_quest_id(),
                        s.current_objective().id, s.published_at_ms());
                }
                ai.set_travel_plan(std::move(legs), goalKey);
                ai.set_travel_force_graph(false);
            }
            while (BotAI::PlanLeg const* lg = ai.current_travel_leg())
            {
                const bool  onMap  = (s.map_id() == lg->to_map);
                const float rx = lg->to_x - bx, ry = lg->to_y - by;
                const float reachR = (lg->kind == 1 /*Walk*/) ? 22.0f : 70.0f;
                if (onMap && rx*rx + ry*ry <= reachR * reachR) ai.advance_travel_leg();
                else break;
            }
            BotAI::PlanLeg const* lg = ai.current_travel_leg();
            if (!lg) return false;
            const float kStep =
                ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f : 35.0f;
            auto walkTo = [&](float wx, float wy, float wz, char const* tag) -> bool
            {
                const float dx = wx - bx, dy = wy - by, dsq = dx*dx + dy*dy;
                if (dsq <= 4.0f) return false;
                if (ai.check_anchor_wedge(tag, s.path_blocked_count(), s.published_at_ms()))
                {
                    // LEG-SKIP (harbor fix): when anchor-wedged on a pure WALK
                    // leg that has a successor, SKIP to the next leg before
                    // discarding the whole plan. A start-attach node can sit
                    // across a water gap on the wrong side — Stormwind harbor:
                    // the route's leg1 (-8610,1289) points NW into the water,
                    // unreachable from the bot's water-edge spot, while the
                    // SOUTHERN leg2 (-8495,1079) IS reachable (mmap_probe: ledge
                    // ->leg2 OK 24 polys). Replanning yields the identical bad
                    // route, so the bot oscillates forever; skipping to the
                    // reachable leg lets it resume. Rate-limited (6s) because the
                    // anchor-wedge cooldown reports "wedged" every tick for 30s —
                    // without the gate it would cascade-skip the whole plan before
                    // the bot starts moving. WALK legs only: a taxi/portal/ship
                    // from-anchor is REQUIRED (skipping strands the boarding).
                    const uint32 sk_now = s.published_at_ms();
                    if (lg->kind == 1 /*Walk*/ &&
                        ai.travel_plan_leg_index() + 1 < ai.travel_plan().size() &&
                        (sk_now - ai.last_leg_skip_ms()) > 6000)
                    {
                        ai.advance_travel_leg();
                        ai.set_last_leg_skip_ms(sk_now);
                        ai.set_last_rule_fired("idle:travel_plan_skip_leg");
                        return false;
                    }
                    // No skippable leg (non-walk, or last leg): discard the plan
                    // and force the next plant through graph A* so the
                    // elevator/teleport route composes (trivial-branch trap).
                    if (ai.current_travel_leg() && !ai.travel_force_graph())
                    {
                        ai.set_travel_force_graph(true);
                        ai.clear_travel_plan();
                    }
                    return false;
                }
                const float dist = std::sqrt(dsq);
                // Emit the FULL leg waypoint when it's within a comfortable move_to
                // range, so move_to PATHFINDS the navmesh route to it (curving
                // around water / obstacles) instead of a straight-line chunk that
                // can land OFF the mesh. In the Stormwind harbor the 35y chunk
                // toward a dock leg landed IN the water (off-navmesh): the bot
                // waded in, idle:water_escape yanked it out, and it never advanced
                // — endless oscillation despite a clean 25-poly path to the leg
                // (mmap_probe, 2026-06-19). Chunking only remains for FAR legs,
                // where it guards move_to's far-goal MovePath handling (FIX #2/#14).
                // 250y: covers a full graph hop between adjacent harbor/dock nodes
                // (Stormwind leg1->leg2 is ~180y over water — chunking it cut back
                // into the water; the navmesh path is a clean 24-poly curve) while
                // staying under the ~292y FindSmoothPath length where a single
                // move_to path starts getting budget-truncated.
                constexpr float kFullWaypointDist = 250.0f;
                if (dist <= kFullWaypointDist)
                {
                    emit.move_to(wx, wy, wz, /*run*/ true);
                }
                else
                {
                    const float scale = kStep / dist;
                    emit.move_to(bx + dx * scale, by + dy * scale, bz, /*run*/ true);
                }
                ai.set_last_rule_fired(tag);
                return true;
            };
            switch (lg->kind)
            {
                case 1: // Walk
                    if (lg->to_map == s.map_id() &&
                        walkTo(lg->to_x, lg->to_y, lg->to_z, "idle:travel_plan_walk"))
                        return true;
                    break;
                case 2: // Taxi
                {
                    ObjectGuid fm;
                    for (auto const& u : s.raw().combat.nearby_friends)
                    {
                        if ((u.npc_flags & UNIT_NPC_FLAG_FLIGHTMASTER) == 0) continue;
                        const float fdx = u.x - bx, fdy = u.y - by;
                        if (fdx*fdx + fdy*fdy <= 12.0f * 12.0f) { fm = u.guid; break; }
                    }
                    if (!fm.IsEmpty() && lg->to_taxi_node != 0)
                    {
                        emit.fly_to_node(fm, lg->to_taxi_node);
                        ai.set_last_rule_fired("idle:travel_plan_fly");
                        return true;
                    }
                    if (walkTo(lg->from_x, lg->from_y, lg->from_z, "idle:travel_plan_to_fm"))
                        return true;
                    break;
                }
                case 6: // Teleport (areatrigger)
                {
                    const float fdx = lg->from_x - bx, fdy = lg->from_y - by;
                    if (fdx*fdx + fdy*fdy <= 14.0f * 14.0f)
                    {
                        if (lg->to_map == s.map_id())
                            emit.near_teleport_to(lg->to_x, lg->to_y, lg->to_z, s.raw().position.o);
                        else
                            emit.teleport_to(lg->to_map, lg->to_x, lg->to_y, lg->to_z, s.raw().position.o);
                        ai.set_last_rule_fired("idle:use_areatrigger_teleport");
                        return true;
                    }
                    if (walkTo(lg->from_x, lg->from_y, lg->from_z, "idle:travel_plan_to_areatrigger"))
                        return true;
                    break;
                }
                case 3: // Portal
                case 4: // Ship
                {
                    const float fdx = lg->from_x - bx, fdy = lg->from_y - by;
                    if ((lg->from_x != 0.f || lg->from_y != 0.f) &&
                        fdx*fdx + fdy*fdy > 30.0f * 30.0f)
                    {
                        if (walkTo(lg->from_x, lg->from_y, lg->from_z,
                                   lg->kind == 4 ? "idle:travel_plan_to_dock"
                                                 : "idle:travel_plan_to_portal"))
                            return true;
                    }
                    break;  // at the anchor → legacy in-range board/use takes over
                }
                case 7: // Elevator — walk to the bottom stop; idle:elevator_step_on rides up
                    if (walkTo(lg->from_x, lg->from_y, lg->from_z, "idle:travel_plan_to_elevator"))
                        return true;
                    break;
                default: break;  // Hearth → legacy fallback
            }
            return false;
}

// ---------- Recommended-taxi cascade (free function) ----------
// Extracted verbatim from the inline `if (s.has_recommended_taxi_route())`
// block in AutoactDispatch so idle:far_same_map_travel @697 can reuse the same
// proactive flight-master walk + fly_to_taxi logic. Returns true if it drove
// (emitted a move/fly/board) this tick; returns false when wedged/yield OR when
// it fell off the end (no taxi route, or no proactive start position). The
// caller distinguishes those two false cases via *out_fell_through: when set,
// the cascade should CONTINUE to lower rules; when clear, the tick is consumed.
bool DriveRecommendedTaxi(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                          float bx, float by, float bz, bool* out_fell_through)
{
    if (!s.has_recommended_taxi_route()) { if (out_fell_through) *out_fell_through = true; return false; }
        {
            const ObjectGuid start_fm = s.recommended_taxi_start_fm();
            // Find the start FM in nearby_friends to read its position.
            float fm_x = 0.f, fm_y = 0.f, fm_z = 0.f;
            bool  fm_visible = false;
            for (auto const& u : s.raw().combat.nearby_friends)
            {
                if (u.guid != start_fm) continue;
                fm_x = u.x; fm_y = u.y; fm_z = u.z; fm_visible = true;
                break;
            }
            if (fm_visible)
            {
                const float dx = fm_x - bx, dy = fm_y - by, dz = fm_z - bz;
                const float dsq = dx*dx + dy*dy + dz*dz;
                // 8y (was 5y): standing within 5y *3D* of an FM NPC is hard given
                // its collision radius + any Z offset, so bots stabilized at ~6-7y
                // and never fired the flight. fly_to_node does its own server-side
                // interact-range check, so a slightly looser pre-gate is safe.
                constexpr float kInteract = 8.0f;
                if (dsq <= kInteract * kInteract)
                {
                    // Within interact range — emit the flight. The API
                    // routes via TaxiPathGraph internally and validates
                    // both endpoints against the bot's mask (we already
                    // confirmed the dest is known in the builder, but
                    // the server-side check also covers race conditions
                    // where the mask just changed).
                    if (!s.in_combat() && !s.is_casting())
                    {
                        // Server rejects mounted FM interact. After the
                        // idle:mount_for_travel rule mounts the bot
                        // before the long walk, it arrives at the FM
                        // mounted and fly_to_node silently fails. Emit
                        // dismount this tick; the actual fly fires on
                        // the next tick when is_mounted has flipped.
                        if (s.raw().movement.is_mounted)
                        {
                            emit.dismount();
                            ai.set_last_rule_fired("idle:dismount_for_taxi");
                            return true;
                        }
                        // Flight hysteresis (Uraimus ping-pong): one
                        // proactive ride per 5-min window. If the last
                        // flight didn't bring the goal in reach (NoPath
                        // POI on a mesh island), flying AGAIN won't
                        // either — walk/bridge resolution gets the window
                        // instead of an endless node-to-node shuttle.
                        const uint32 fly_now_ms = s.published_at_ms();
                        if (ai.action_recently_tried(BotAI::ActionKind::Flight,
                                                     0u, fly_now_ms))
                            return false;
                        emit.fly_to_node(start_fm, s.recommended_taxi_dest_node());
                        ai.note_action_retry(BotAI::ActionKind::Flight,
                                             0u, fly_now_ms);
                        ai.set_last_rule_fired("idle:fly_to_taxi");
                        return true;
                    }
                }
                else
                {
                    if (ai.check_anchor_wedge("idle:walk_to_taxi",
                                              s.path_blocked_count(),
                                              s.published_at_ms()))
                        return false;
                    // Walk toward the FM. Personality-scaled step matches
                    // the POI / quest_path rules below.
                    const float kStep =
                        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                        35.0f;
                    const float dist = std::sqrt(dsq);
                    const float scale = std::min(kStep, dist) / dist;
                    const float tx = bx + dx * scale;
                    const float ty = by + dy * scale;
                    // Threat look-ahead — flight masters in low-level
                    // zones often sit just outside town with mob
                    // patrols between bot and FM.
                    if (NearbyUnit const* threat = s.path_threat(
                            tx, ty,
                            /*max_forward*/ std::min(kStep, 35.0f),
                            /*half_width*/  10.0f))
                    {
                        if (emit.start_attack(threat->guid))
                        {
                            ai.set_last_rule_fired("idle:walk_taxi_pull_threat");
                            return true;
                        }
                    }
                    // Flight masters are routinely ELEVATED and reached by an
                    // ELEVATOR, not a ramp (Doras, the Orgrimmar FM, is at z103 on
                    // the Valley rim above the z42 ground, served by the Valley of
                    // Strength lift). A bot walking straight at the FM xy piles up
                    // 60y below it and never reaches the 8y flight gate (754
                    // walk_to_taxi / 0 fly). When the FM sits materially above the
                    // bot AND a lift is near, route to the lift's BASE so the
                    // higher-priority idle:elevator_step_on rule boards it, rides
                    // up, and idle:elevator_step_off drops the bot on the upper
                    // floor next to the FM — then walk_to_taxi/fly_to_taxi resume.
                    float tgt_x = fm_x, tgt_y = fm_y, tgt_z = fm_z;
                    constexpr float kFmAboveBot = 15.0f;
                    if (fm_z - bz >= kFmAboveBot)
                    {
                        using ::Playerbot::V2::Travel::ElevatorIndex;
                        if (auto const* lift = ElevatorIndex::Instance().LowestStopNear(
                                s.map_id(), fm_x, fm_y, /*xy_range*/ 90.0f))
                        { tgt_x = lift->x; tgt_y = lift->y; tgt_z = lift->z; }
                    }
                    // Full navmesh move_to (no fixed-Z stepping — that strands the
                    // bot across elevation changes); PathGenerator routes the climb.
                    emit.move_to(tgt_x, tgt_y, tgt_z, /*run*/ true);
                    ai.set_last_rule_fired("idle:walk_to_taxi");
                    return true;
                }
            }
            // FM not in scan range yet — walk to the start node's POSITION (the
            // flight master). This is the PROACTIVE "go to the airport" leg: a
            // bot with a far same-map goal heads to the nearest known flight
            // master and flies, exactly like a real player, instead of wedging
            // on a doomed walk or grabbing a wrong-destination boat. Once the bot
            // reaches the node the FM enters its scan, the snapshot fills in
            // recommended_taxi_start_fm, and idle:fly_to_taxi (above) fires.
            const float sx = s.recommended_taxi_start_x();
            const float sy = s.recommended_taxi_start_y();
            const float sz = s.recommended_taxi_start_z();
            if (sx != 0.f || sy != 0.f)
            {
                const float dx = sx - bx, dy = sy - by;
                const float dsq = dx*dx + dy*dy;
                if (dsq > 10.0f * 10.0f)
                {
                    // Wedged this window — yield so the higher-priority elevator
                    // rules (idle:elevator_step_on, 710) can take over and ride the
                    // bot UP to an elevated flight master. We do NOT blacklist the
                    // FM: players reach every FM, so a route always exists (usually
                    // via a city lift) — blacklisting would mask the real elevator/
                    // navmesh routing bug. A genuinely unreachable FM surfaces via
                    // the [move_blocked] milestone log for a data fix.
                    if (ai.check_anchor_wedge("idle:walk_to_flightmaster",
                                              s.path_blocked_count(),
                                              s.published_at_ms()))
                        return false;
                    // Single-owner routing: an ACTIVE travel plan owns the
                    // journey. Without this the FM walk fought the plan
                    // every tick (observed: Somi underground in UC — plan
                    // says "walk to Undervator bottom", this rule says
                    // "walk to the surface FM", both fire alternately and
                    // the surface walk NoPaths forever).
                    if (ai.current_travel_leg())
                        return false;
                    // Composed route first whenever the FM is far OR
                    // elevated: the UnifiedTravelGraph stitches walk →
                    // elevator / areatrigger-teleport / bridge legs that a
                    // raw PathGenerator call can't see. This covers city
                    // lifts (Orgrimmar Doras z103), tower FMs, AND
                    // island-pocket FMs like Rut'theran below Darnassus —
                    // where the old elevation-only gate (sz-bz >= 15) never
                    // fired because the FM sits BELOW the bot, leaving it
                    // wedged on a NoPath straight line forever (observed:
                    // Uraimus L12, Darnassus).
                    const float fm_dist = std::sqrt(dsq);
                    if ((fm_dist > 150.0f || sz - bz >= 15.0f) &&
                        DriveTravelPlanTo(s, ai, emit, bx, by, bz, s.map_id(), sx, sy, sz))
                        return true;
                    // Far FM on the same mesh: STEP toward it instead of
                    // emitting one 400y+ move_to. A far destination often
                    // sits in an unloaded grid/mmap tile, so the full-route
                    // PathGenerator call returns NoPath and the rule loops
                    // without moving an inch (observed: Somi/Tibeo, Razor
                    // Hill FM 420y away, [path_fail] NoPath every tick).
                    // Bounded steps keep both endpoints in loaded space;
                    // tiles stream in as the bot advances.
                    if (fm_dist > 150.0f)
                    {
                        const float kStep =
                            ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                            ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                            35.0f;
                        const float scale = kStep / fm_dist;
                        // Blocked-bearing deflection (see walk_to_known_portal).
                        float fdx = dx, fdy = dy;
                        if (s.path_blocked_count() > 0)
                        {
                            const float defl =
                                (s.path_blocked_count() % 2 == 1 ? 0.7f : -0.7f);
                            const float cs = std::cos(defl), sn = std::sin(defl);
                            fdx = dx * cs - dy * sn;
                            fdy = dx * sn + dy * cs;
                        }
                        emit.move_to(bx + fdx * scale, by + fdy * scale, bz, /*run*/ true);
                        ai.set_last_rule_fired("idle:walk_to_flightmaster");
                        return true;
                    }
                    // Near FM — direct navmesh move_to (PathGenerator
                    // routes any walkable climb).
                    emit.move_to(sx, sy, sz, /*run*/ true);
                    ai.set_last_rule_fired("idle:walk_to_flightmaster");
                    return true;
                }
                // At the node but the FM isn't in the scan yet — hold for a tick;
                // the next snapshot captures the FM guid and fly_to_taxi fires.
                ai.set_last_rule_fired("idle:wait_for_flightmaster");
                return true;
            }
            // No proactive start position — fall through to mount/quest_path.
        }
    // reached the end of the taxi block without returning (the old fall-through):
    if (out_fell_through) *out_fell_through = true;
    return false;
}

// ---------- Auto-act dispatcher (REFACTOR_3 pass 17) ----------
// Body extracted verbatim from the inline `if (can_autoact)` block.
// Holds the quest-execution / travel / wander / crafting / engage
// cascade — ~50 sub-rules. Each fire-path return; in the outer
// function scope became `return true;`; lambda-internal returns
// (depth >= 1 inside nested `[&](...)` bodies) are preserved.
// Falls through to `return false;` when no sub-rule fired.
bool AutoactDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit)
{
    // can_autoact gate, replicating the original inline guard — with one
    // refinement: a SELECTION only blocks autonomy when it is a live
    // hostile (current_target_hostile). The raw `current_target == Empty`
    // test froze the whole quest/travel/wander cascade whenever a selfbot
    // owner click-selected a friendly NPC / party member / self on their
    // client (UNIT_FIELD_TARGET persists until they clear it — observed
    // 2026-06-13: Uraimus idle for hours, only maintenance/ambient rules
    // firing). A hostile selection still gates: it signals imminent
    // engagement (headless bots set it via start_attack's SetSelection).
    const bool can_autoact = !s.in_combat() && !s.is_casting() && !s.is_moving() &&
                             !s.is_stunned() && !s.is_rooted() &&
                             s.level() >= 1 &&
                             s.victim() == ObjectGuid::Empty &&
                             (s.current_target() == ObjectGuid::Empty ||
                              !s.current_target_hostile()) &&
                             !s.is_in_instance() && !s.is_in_dungeon();
    if (!can_autoact) return false;

    // hp_gate_for_engage was computed in OnTick before the inline block;
    // replicate the same Aggression->threshold mapping locally so the
    // sub-rules reading hp_above_engage_gate still resolve.
    const Aggression agg = ai.personality().aggression;
    const int32 hp_gate_for_engage =
        agg == Aggression::Defensive ? 80 :
        agg == Aggression::Aggressive ? 40 :
        60;

    // Profession-mode flag — extracted with the body. Sub-rules
    // (idle:wander_to_quest_hub etc.) consult it to widen attract radius.
    const bool _prof_mode = ai.in_profession_mode(s.published_at_ms());
    (void)_prof_mode;
        // Engage / random-wander gate: above the HP threshold for the bot's
        // Aggression personality. Below this, the bot will still pursue
        // recovery wanders (vendor / quest hub / node) but won't pull new
        // mobs or wander aimlessly.
        const bool hp_above_engage_gate = s.hp_pct() >= hp_gate_for_engage;
        float bx, by, bz;
        s.position(bx, by, bz);

        // ---- Shared UnifiedTravelGraph route executor ----
        // Drives the bot one leg along a cached A* route to an arbitrary target,
        // composing walk + flight + portal + ship + areatrigger + ELEVATOR legs.
        // Used by THREE callers so they all reach elevated/disconnected targets
        // through the ONE travel manager instead of ad-hoc shortcuts: the cross-
        // map / relocation quest goal, an elevated flight master, and a platform
        // vendor. Returns true if it emitted a move/board this tick. FindRoute
        // runs once per goal change (cached in BotAI by goal key), not per tick;
        // only one caller fires per tick (dispatch order), so the single plan
        // cache never thrashes. Elevator legs are walked-to-bottom-stop here; the
        // idle:elevator_step_on rule (prio 710, runs first) rides the lift up.
        // Thin forwarding lambda — the executor body is now the free function
        // ::Playerbot::States::DriveTravelPlanTo (extracted so idle rules can
        // reuse it). All existing call sites below keep working unchanged.
        auto driveTravelPlanTo = [&](uint32 to_map, float tx, float ty, float tz) -> bool
        {
            return ::Playerbot::States::DriveTravelPlanTo(s, ai, emit, bx, by, bz, to_map, tx, ty, tz);
        };

        // Centralized stuck-detection observation: each tick the current
        // objective is pinned, BotAI tracks how long progress has stalled.
        // After ~5 min without progress, the objective is auto-blacklisted
        // for 5 min so the Builder can pick a different quest. Resets
        // automatically when progress increments.
        //
        // Fast-abandon signal: the objective is UNREACHABLE when the bot's
        // last action was the objective POI walk (idle:quest_path) AND it is
        // currently in a consecutive path-block streak (move_to returned
        // Result::Locked — NoPath / FarFromPolyEnd — and path_blocked_count
        // has NOT been reset by a successful move). note_move_succeeded() zeroes
        // path_blocked_count on any successful move, so a non-zero count here
        // means the most recent move emits are all failing. Feeding this into
        // note_obj_observed lets a literally-unpathable objective (e.g. Uraimus
        // q483 den across a NoPath gap) blacklist in ~8s instead of ~5 min,
        // so the Builder swaps to a reachable quest before any teleport-rescue
        // fires. A merely-slow (reachable) objective never trips this because
        // its move_to succeeds and resets the streak.
        // Gate on a REAL quest (current_quest_id != 0), not has_current_objective():
        // an R7 relocation goal has current_quest_id==0 + a default objective, so
        // feeding it to the progress/blacklist observer would record a phantom
        // "quest 0 / objective 0" and could mis-blacklist. The relocation goal has
        // its own wedge handling in the travel-plan executor.
        if (s.current_quest_id() != 0)
        {
            char const* lr = ai.last_rule_fired();
            const bool walk_nopath =
                ai.path_blocked_count() > 0 &&
                lr != nullptr &&
                std::string_view(lr) == "idle:quest_path";

            // CombatLoop FIX D: scan-MISS signal. A valid in-range target for
            // the current objective resets the fast scan-miss strike track; its
            // ABSENCE accrues strikes toward a ~30s blacklist. This recovers
            // phased / scripted (D-class) objectives whose target never
            // materializes for the bot even though the POI walk succeeds (so the
            // walk_nopath track never fires and only the slow ~5min stuck_ticks
            // would). "In range" mirrors the snapshot's neutral-admit radius
            // (~80y) so a target the bot can see/approach this tick counts as a
            // hit. Checks the consumer-relevant container per objective type:
            // nearby_enemies for kill MONSTER / KILL_WITH_LABEL, nearby_friends
            // for friendly (talk_credit / TALKTO) targets, nearby_objects for
            // GAMEOBJECT. Only meaningful for objectives that HAVE a scannable
            // target type; other types report no-miss (don't accrue) so they
            // rely on the existing nopath / stuck tracks.
            bool scan_miss = false;
            {
                auto const& cobj = s.current_objective();
                constexpr float kScanRadiusSq = 80.0f * 80.0f;
                bool scannable = false;
                bool target_in_range = false;
                auto in_range = [&](float ux, float uy, float uz) {
                    const float dx = ux - bx, dy = uy - by, dz = uz - bz;
                    return (dx*dx + dy*dy + dz*dz) <= kScanRadiusSq;
                };
                auto entry_matches = [&](uint32 e) {
                    if (cobj.type == /*KILL_WITH_LABEL*/ 21)
                    {
                        for (uint32 le : cobj.labeled_target_entries)
                            if (le == e) return true;
                        return false;
                    }
                    return cobj.object_id > 0 && uint32(cobj.object_id) == e;
                };
                if (cobj.type == /*MONSTER*/ 0 || cobj.type == /*KILL_WITH_LABEL*/ 21 ||
                    cobj.type == /*TALKTO*/ 3 || cobj.type == /*GAMEOBJECT*/ 2)
                {
                    scannable = true;
                    // Friendly targets (talk_credit MONSTER + TALKTO) live in
                    // nearby_friends; everything attackable in nearby_enemies.
                    const bool friendly_target =
                        cobj.type == /*TALKTO*/ 3 ||
                        (cobj.type == /*MONSTER*/ 0 && cobj.talk_credit);
                    if (cobj.type == /*GAMEOBJECT*/ 2)
                    {
                        for (auto const& go : s.raw().world_objects.nearby_objects)
                            if (cobj.object_id > 0 && go.entry == uint32(cobj.object_id) &&
                                in_range(go.x, go.y, go.z)) { target_in_range = true; break; }
                    }
                    else if (friendly_target)
                    {
                        for (auto const& u : s.raw().combat.nearby_friends)
                            if (u.hp > 0 && entry_matches(u.entry) &&
                                in_range(u.x, u.y, u.z)) { target_in_range = true; break; }
                    }
                    else
                    {
                        for (auto const& u : s.raw().combat.nearby_enemies)
                            if (u.hp > 0 && entry_matches(u.entry) &&
                                in_range(u.x, u.y, u.z)) { target_in_range = true; break; }
                    }
                }
                scan_miss = scannable && !target_in_range;
            }

            ai.note_obj_observed(s.current_quest_id(), s.current_objective().id,
                                  s.current_objective().progress, s.published_at_ms(),
                                  walk_nopath, scan_miss);
        }

        // ---- Quest-kill rule: prioritize current quest's monster objective ----
        // Fires BEFORE the generic engage so a bot working a kill quest pulls
        // its quest target instead of whatever happens to be closest. Same HP
        // gate as engage (don't pull while wounded). Attempts the closest
        // matching mob in `nearby_enemies` whose `entry == current_objective.object_id`.
        // Skips creatures the bot already tried recently via the standard
        // last_engage_target_ cooldown. Skips the rule entirely when the
        // current objective isn't a MONSTER, when there's no current
        // objective, or when the objective is blacklisted (stuck-detection).
        // ---- Quest spatial-batching dispatcher ----
        // Iterate `actionable_objectives` (sorted by distance ascending) and
        // act on the closest in-range entry, regardless of which quest owns
        // it. This is the "kills in a shared zone progress 3 quests" path.
        // The per-type rules below still run as a fallback for the current
        // objective when batching doesn't find anything visible (e.g., POI
        // pathing while targets are out of nearby range).
        //
        // Range gates per type:
        //   MONSTER / KILL_WITH_LABEL : 40y engage range (matches quest_kill)
        //   ITEM (creature/GO drop)   : 40y / 5y respectively
        //   GAMEOBJECT                : 5y interact
        //   TALKTO                    : 5y interact (uses 2-phase via current_objective rule)
        //   AREATRIGGER               : ignored here (the AT rule handles)
        // Bot's HP / aggression gates apply to combat dispatches.
        if (!s.actionable_objectives().empty())
        {
            const uint32 now_ms = s.published_at_ms();
            const ObjectGuid recent = ai.last_engage_target();
            const bool recent_active = !recent.IsEmpty() &&
                                       (now_ms - ai.last_engage_at_ms() < 8000u);
            constexpr float kEngageRangeSq = 40.0f * 40.0f;
            constexpr float kInteractRangeSq = 5.0f * 5.0f;

            // Skip the head objective if we've already chosen current_objective
            // and a per-type rule below will fire — actually no, we want the
            // dispatcher to be authoritative. Walk the list, dispatch the
            // first match we can fulfill on this tick.
            for (auto const& ao : s.actionable_objectives())
            {
                // For combat dispatches: only when the bot can actually engage.
                // Same gates as the original rules.
                // QUEST kills are NOT personality-gated: Passive means "doesn't
                // pick optional fights" (no open-world grinding, no assists),
                // never "cannot do its quests". The old `agg != Passive` gate
                // here (and on every quest-kill rule) made the ~5% of bots that
                // roll Passive permanently unlevelable — they could never tag a
                // single quest target (verified live: L2 bot, 150 played-hours,
                // 5 XP, killable quest targets in scan range the whole time).
                const bool combat_capable = hp_above_engage_gate;

                // Compute tick-time distance from the bot to the source. Note:
                // ao.distance_sq is the *snapshot-time* planar distance. Per
                // the existing rules we re-validate using current bot pos
                // and the source's nearby_* coords.
                if (ao.type == /*MONSTER*/ 0 || ao.type == /*KILL_WITH_LABEL*/ 21)
                {
                    if (!combat_capable) continue;
                    if (recent_active && ao.source_guid == recent) continue;
                    NearbyUnit const* match = nullptr;
                    for (auto const& u : s.raw().combat.nearby_enemies)
                        if (u.guid == ao.source_guid)
                        { match = &u; break; }
                    if (!match || match->hp <= 0 || match->victim != ObjectGuid::Empty) continue;
                    const float dx = match->x - bx, dy = match->y - by, dz = match->z - bz;
                    if (dx*dx + dy*dy + dz*dz > kEngageRangeSq) continue;
                    // Gate the rule on emit.start_attack returning true.
                    // The emit drops the intent silently when the per-target
                    // StartAttack lockout is active (server keeps refusing
                    // the same target). Without gating, the rule claims the
                    // dispatch slot every tick despite no intent going out
                    // — watchdog tripped 325 times in one session.
                    if (!emit.start_attack(match->guid)) continue;
                    ai.note_engage(match->guid, now_ms);
                    ai.set_last_rule_fired("idle:quest_batch:kill");
                    return true;
                }
                if (ao.type == /*ITEM*/ 1)
                {
                    // ITEM objective: source is either a creature (kill+loot)
                    // or a GO (use). Distinguish by source_guid kind.
                    if (ao.source_guid.IsCreature())
                    {
                        if (!combat_capable) continue;
                        if (recent_active && ao.source_guid == recent) continue;
                        NearbyUnit const* match = nullptr;
                        for (auto const& u : s.raw().combat.nearby_enemies)
                            if (u.guid == ao.source_guid) { match = &u; break; }
                        if (!match || match->hp <= 0 || match->victim != ObjectGuid::Empty) continue;
                        const float dx = match->x - bx, dy = match->y - by, dz = match->z - bz;
                        if (dx*dx + dy*dy + dz*dz > kEngageRangeSq) continue;
                        // Same start_attack-return gate as quest_batch:kill above.
                        if (!emit.start_attack(match->guid)) continue;
                        ai.note_engage(match->guid, now_ms);
                        ai.set_last_rule_fired("idle:quest_batch:collect_kill");
                        return true;
                    }
                    if (ao.source_guid.IsAnyTypeGameObject())
                    {
                        for (auto const& go : s.raw().world_objects.nearby_objects)
                        {
                            if (go.guid != ao.source_guid) continue;
                            const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                            if (dx*dx + dy*dy + dz*dz > kInteractRangeSq) break;
                            emit.use_game_object(go.guid);
                            ai.set_last_rule_fired("idle:quest_batch:collect_use");
                            return true;
                        }
                    }
                    continue;
                }
                if (ao.type == /*GAMEOBJECT*/ 2)
                {
                    for (auto const& go : s.raw().world_objects.nearby_objects)
                    {
                        if (go.guid != ao.source_guid) continue;
                        const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                        if (dx*dx + dy*dy + dz*dz > kInteractRangeSq) break;
                        emit.use_game_object(go.guid);
                        ai.set_last_rule_fired("idle:quest_batch:use_go");
                        return true;
                    }
                    continue;
                }
                // TALKTO + AREATRIGGER intentionally fall through to per-type
                // rules — TALKTO needs 2-phase gossip handling, AT has its
                // own dedicated rule.
            }

            // ---- Walk-to-target fallback for combat dispatches ----
            // The combat dispatches above (MONSTER / KILL_WITH_LABEL /
            // ITEM-from-creature) gate engagement on `kEngageRangeSq = 40y`.
            // The snapshot scan however admits neutrals up to 80y (see
            // BotSnapshotBuilder's quest-target neutral expansion).
            // Without a walk step the 40-80y zone is a dead band — every
            // candidate is rejected as "too far", no engagement fires,
            // bot sits forever in plain sight of its quest targets.
            // Verified 2026-05-20 with [neutral_scan_ok] bot=Uraimus
            // appended=14 scanned=44 followed by 0 engagements: he had
            // 14 Young Nightsabers in nearby_enemies but the closest was
            // ~59y away, outside engage but inside scan.
            //
            // Fix: find closest combat-capable actionable that's >40y but
            // present in nearby_enemies (so within scan radius) and walk
            // toward it. Next tick, the engagement gate above passes.
            // Per-target retry dedup keeps a single unreachable mob from
            // claiming the dispatch slot every tick.
            // (No Passive gate: quest kills are mandatory work, not optional
            // aggression — see combat_capable above.)
            if (hp_above_engage_gate)
            {
                NearbyUnit const* walk_target = nullptr;
                float walk_distSq = 1e9f;
                for (auto const& ao : s.actionable_objectives())
                {
                    if (ao.type != /*MONSTER*/ 0 &&
                        ao.type != /*KILL_WITH_LABEL*/ 21 &&
                        !(ao.type == /*ITEM*/ 1 && ao.source_guid.IsCreature()))
                        continue;
                    if (recent_active && ao.source_guid == recent) continue;
                    NearbyUnit const* match = nullptr;
                    for (auto const& u : s.raw().combat.nearby_enemies)
                        if (u.guid == ao.source_guid) { match = &u; break; }
                    if (!match || match->hp <= 0 || match->victim != ObjectGuid::Empty) continue;
                    const float dx = match->x - bx, dy = match->y - by, dz = match->z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq <= kEngageRangeSq) continue;     // engagement loop already handled this
                    if (dsq < walk_distSq) { walk_distSq = dsq; walk_target = match; }
                }
                if (walk_target)
                {
                    // No per-target dedup here. The bot needs multiple
                    // ticks of move_to to refresh the destination as it
                    // closes the 40-80y gap (running 7y/s × 250ms tick
                    // = 1.75y per tick → 20y close needs ~12 ticks).
                    // MotionMaster naturally collapses identical
                    // destinations, so spam-emitting is safe. Earlier
                    // attempt used a 5-min ActionKind::WanderToNode
                    // lockout which fired once then starved subsequent
                    // ticks — bot moved 2y then stopped. Verified
                    // 2026-05-20: 108 walk_to_target fires but only 1
                    // quest completion in 20 min.
                    //
                    // Class-aware stop distance: ranged classes stop at
                    // ~30y so they're inside Steady Shot / Frostbolt /
                    // Shadow Bolt range but outside melee aggro. Melee
                    // classes close to 4y for swing range. Without
                    // this, hunters/casters walked to 4y, tried to
                    // melee with no weapon stance, and the APL never
                    // got a chance to fire ranged openers. Verified
                    // 2026-05-20 (Halinen, hunter): bot was at 4y,
                    // auto-attacking, but the APL's ShouldSteadyShot
                    // never advanced past walk because main engage
                    // loop already fired start_attack in melee.
                    float stop_y = 4.0f;
                    switch (s.cls())
                    {
                        case CLASS_HUNTER:
                        case CLASS_MAGE:
                        case CLASS_PRIEST:
                        case CLASS_WARLOCK:
                            // Hunter/Mage/Priest/Warlock baseline ranged
                            // openers are 40y range (Steady Shot, Frostbolt,
                            // Smite/SW:Pain, Shadow Bolt). 30y stop keeps
                            // the bot inside cast range with a 10y safety
                            // margin for movement variance.
                            stop_y = 30.0f;
                            break;
                        case CLASS_EVOKER:
                            // Evoker baseline ranged is 25y (Living Flame,
                            // Disintegrate, Azure Strike). 30y would leave
                            // the bot OUT of cast range; 20y stop keeps
                            // it inside the 25y range with 5y safety.
                            stop_y = 20.0f;
                            break;
                        default:
                            break;
                    }
                    const float dist = std::sqrt(walk_distSq);
                    // walk_target is only selected when dist > 40y (the
                    // main engage gate), so dist > stop_y is guaranteed
                    // even for the 30y ranged stop. No skip-branch needed.
                    const float scale = (dist - stop_y) / dist;
                    const float tx = bx + (walk_target->x - bx) * scale;
                    const float ty = by + (walk_target->y - by) * scale;
                    // Threat look-ahead: scan a corridor from current
                    // position to the planned waypoint and bail out into
                    // a tactical pull if any non-target hostile sits in
                    // it. Without this the bot runs through aggro radii
                    // and accumulates 2-3 attackers before its quest
                    // mob is even targeted — verified Uraimus death
                    // 2026-05-21 (3 nightsabers en route to a single
                    // POI item). Excluding walk_target->guid keeps the
                    // intended quest mob from masking the scan.
                    //
                    // Cluster check: if 3+ hostiles sit on the path,
                    // pulling one chain-aggros the pack. Sidestep
                    // perpendicular ~20y to skirt instead. Pick the
                    // side with fewer threats (rotate corridor 90°
                    // left vs right and count each). If both sides
                    // are equally bad, prefer left (no behavioural
                    // reason; consistent tiebreak avoids tick-flip).
                    {
                        const float corridor_fwd = std::min(dist, 35.0f);
                        const size_t threats = s.path_threat_count(
                            tx, ty, corridor_fwd, 10.0f, walk_target->guid);
                        // Cluster cleared — drop any committed side so a future
                        // cluster picks fresh.
                        if (threats < 3)
                        {
                            ai.clear_lateral_side();
                        }
                        else
                        {
                            // Hold + hysteresis to stop the per-tick side flip.
                            // The naive "recompute the pick each tick, take fewer
                            // (tie→left)" caused L→R→L oscillation: each tick the
                            // bot's position shifts, the fresh count ties or near-
                            // ties, the side flips, and the >3y new destination
                            // resets the spline so the bot never actually clears
                            // the cluster (Irothoth flip-flop). Instead: commit a
                            // side and keep it for kHoldMs, only switching when the
                            // OTHER side is MEANINGFULLY better (>=2 fewer threats),
                            // never on a tie. After kGiveUpMs still blocked, stop
                            // sidestepping entirely and fall through to the pull /
                            // check_anchor_wedge path so we can't flip-flop forever.
                            constexpr uint32 kHoldMs    = 3500u;   // commit window
                            constexpr uint32 kGiveUpMs  = 7000u;   // abandon sidestep
                            constexpr size_t kSwitchMargin = 2;    // threats fewer to flip

                            const float ux = (tx - bx) / dist;
                            const float uy = (ty - by) / dist;
                            constexpr float kSidestep = 20.0f;
                            // perp_left = rotate(u, +90deg) = (-uy, +ux)
                            const float lx = bx - uy * kSidestep;
                            const float ly = by + ux * kSidestep;
                            const float rx = bx + uy * kSidestep;
                            const float ry = by - ux * kSidestep;
                            const size_t left_threats = s.path_threat_count(
                                lx, ly, 25.0f, 8.0f, walk_target->guid);
                            const size_t right_threats = s.path_threat_count(
                                rx, ry, 25.0f, 8.0f, walk_target->guid);

                            const int8   committed = ai.lateral_side();
                            const uint32 committed_ms = ai.lateral_side_ms();
                            const bool   have_commit = committed != 0 && committed_ms != 0;
                            const bool   commit_aged =
                                have_commit && (now_ms - committed_ms) >= kGiveUpMs;

                            // Give-up: committed too long and STILL blocked. Stop
                            // sidestepping; clear the side and fall through so the
                            // pull / check_anchor_wedge path takes over. (Don't
                            // re-commit — that's what would loop forever.)
                            if (commit_aged)
                            {
                                ai.clear_lateral_side();
                            }
                            else
                            {
                                // Decide the side. With an active commit inside the
                                // hold window, KEEP it unless the other side is
                                // >=kSwitchMargin better. Outside the hold window (or
                                // no commit yet) pick fresh: fewer threats, tie→left.
                                int8 side;
                                if (have_commit && (now_ms - committed_ms) < kHoldMs)
                                {
                                    side = committed;   // honour the commitment
                                    if (committed < 0)  // currently left
                                    {
                                        if (right_threats + kSwitchMargin <= left_threats)
                                            side = +1;
                                    }
                                    else                // currently right
                                    {
                                        if (left_threats + kSwitchMargin <= right_threats)
                                            side = -1;
                                    }
                                }
                                else
                                {
                                    side = (left_threats <= right_threats) ? -1 : +1;
                                }

                                const float pick_x = (side < 0) ? lx : rx;
                                const float pick_y = (side < 0) ? ly : ry;
                                if (emit.move_to(pick_x, pick_y, bz, /*run*/ true))
                                {
                                    // Re-stamp the commit time only when the side
                                    // actually changes (or there was none), so the
                                    // hold window measures "time on THIS side", not
                                    // "time since last emit". A stable side keeps its
                                    // original timestamp and ages toward kGiveUpMs.
                                    if (side != committed)
                                        ai.commit_lateral_side(side, now_ms);
                                    ai.set_last_rule_fired("idle:quest_walk_lateral_reroute");
                                    TC_LOG_INFO("playerbot.v2",
                                        "[quest_walk_lateral_reroute] {} cluster={} side={} (L={}, R={})",
                                        s.name(), threats,
                                        side < 0 ? "L" : "R",
                                        left_threats, right_threats);
                                    return true;
                                }
                            }
                        }
                    }
                    if (NearbyUnit const* threat = s.path_threat(
                            tx, ty,
                            /*max_forward*/ std::min(dist, 35.0f),
                            /*half_width*/  10.0f,
                            walk_target->guid))
                    {
                        if (emit.start_attack(threat->guid))
                        {
                            ai.set_last_rule_fired("idle:quest_walk_pull_threat");
                            TC_LOG_INFO("playerbot.v2",
                                "[quest_walk_pull_threat] {} threat_entry={} ahead",
                                s.name(), threat->entry);
                            return true;
                        }
                    }
                    if (emit.move_to(tx, ty, walk_target->z, /*run*/ true))
                    {
                        ai.set_last_rule_fired("idle:quest_walk_to_target");
                        // Diagnostic throttled by the per-bot wander_diag
                        // window (60s) so we don't drown the log even if
                        // the rule itself fires every tick.
                        const uint32 wd_last = ai.last_wander_diag_ms();
                        if (wd_last == 0 || (now_ms - wd_last) >= 60u * 1000u)
                        {
                            ai.set_last_wander_diag_ms(now_ms);
                            TC_LOG_INFO("playerbot.v2",
                                "[quest_walk_to_target] {} target_entry={} dist={:.1f}",
                                s.name(), walk_target->entry, dist);
                        }
                        return true;
                    }
                }
            }
        }

        // ---- Quest "tool" rule (use-item-on-target / self-use) ----
        // Higher priority than quest_kill: when the bag holds an item whose
        // ON_USE spell carries SPELL_EFFECT_KILL_CREDIT for the active
        // objective, using the item is the credit channel — there is no
        // "kill" to perform. Bell on the cultist; Disk-of-Mounting; Eel
        // Trap. Self-cast tools (drink-the-elixir) skip the target search.
        // Range to target is 5y (UseItemByEntryIntent's interact gate).
        if (s.has_current_objective() && s.current_objective_tool().valid)
        {
            const auto& tool = s.current_objective_tool();
            const auto& obj  = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                if (tool.target_entry == 0)
                {
                    // Self-use; no target search. UseItemByEntry resolves
                    // the bag-slot internally so we don't need the precise
                    // (bag, slot) pair to match the snapshot.
                    //
                    // POI gate (2026-05-21): self-use quest items that need
                    // a specific WORLD LOCATION (e.g. Teldrassil "Refusal of
                    // the Aspects" — use the censer at an AreaTrigger POI)
                    // would otherwise fire UseItemByEntryIntent the moment
                    // the bot has the item, regardless of where it stands.
                    // Server rejects the use because the AreaTrigger script
                    // sees the bot out of range; the rule re-fires next
                    // tick and the bot wedges (/history shows
                    // quest_use_tool spam while quest_path tries to walk
                    // toward the POI underneath). Gate: if the objective
                    // has a valid POI on THIS map and we're outside the
                    // POI radius, DON'T fire — fall through so the
                    // quest_path fallback below walks the bot to the POI
                    // first. Subsequent ticks (now in range) will fire
                    // the use-item.
                    bool poi_ok_or_absent = true;
                    {
                        const auto& self_poi = s.current_objective_poi();
                        if (self_poi.valid && self_poi.map_id == s.map_id())
                        {
                            const float dx = self_poi.x - bx;
                            const float dy = self_poi.y - by;
                            const float dz = self_poi.z - bz;
                            // Use the POI's declared radius; fall back to
                            // a conservative 8y if POI carries radius=0
                            // (some QuestPOIBlobPoint rows do).
                            const float r = self_poi.radius > 0.0f
                                ? self_poi.radius : 8.0f;
                            if (dx*dx + dy*dy + dz*dz > r * r)
                                poi_ok_or_absent = false;
                        }
                    }
                    if (poi_ok_or_absent)
                    {
                        emit.emit(UseItemByEntryIntent{tool.item_entry, ObjectGuid::Empty});
                        ai.set_last_rule_fired("idle:quest_use_tool");
                        return true;
                    }
                    // Out of POI range — fall through. The target-search
                    // code below is a harmless no-op for self-use because
                    // `tool.target_entry == 0` matches no NearbyUnit (all
                    // units have entry > 0), so it exits without emitting.
                    // The quest_path fallback walks the bot to the POI.
                }
                // Find a nearby unit whose entry matches the credit creature.
                // Most are friendly (NPCs to charm/cleanse) so we look in
                // BOTH nearby_friends and nearby_enemies — the API accepts
                // any GUID in interact range. Prefer the closest match.
                constexpr float kInteract = 5.0f;
                constexpr float kInteractSq = kInteract * kInteract;
                ObjectGuid best_target;
                float       bestDist = std::numeric_limits<float>::infinity();
                auto consider = [&](NearbyUnit const& u) {
                    if (u.entry != tool.target_entry) return;
                    if (u.hp <= 0) return;     // dead doesn't credit
                    const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq > kInteractSq) return;
                    if (dsq < bestDist) { bestDist = dsq; best_target = u.guid; }
                };
                for (auto const& u : s.raw().combat.nearby_friends) consider(u);
                for (auto const& u : s.raw().combat.nearby_enemies) consider(u);
                if (!best_target.IsEmpty())
                {
                    emit.emit(UseItemByEntryIntent{tool.item_entry, best_target});
                    ai.set_last_rule_fired("idle:quest_use_tool");
                    return true;
                }
                // No target in range — fall through; quest_path takes over
                // and walks toward the POI where the target is expected.
            }
        }

        // ---- Quest "use item on friendly NPC" rule ----
        // For Northshire-style quests where the objective is type=MONSTER
        // but the credit comes from using a quest item on a FRIENDLY NPC
        // (heal-the-wounded patterns). Detected by:
        //   * Current objective type==MONSTER + has unfinished progress.
        //   * Current quest has a source_item_id (granted on accept).
        //   * Target entry appears in nearby_friends (would be in
        //     nearby_enemies for a real kill quest).
        //   * Bot still has the source item in bag_items.
        // The item's on-use spell handles the credit server-side; bot
        // just walks to interact range and emits use_item_by_entry.
        if (s.has_current_objective() &&
            s.current_objective().type == /*MONSTER*/ 0 &&
            s.current_objective().progress < s.current_objective().amount)
        {
            // Resolve the source item for the current quest.
            uint32 source_item = 0;
            for (auto const& q : s.raw().quest_log.quests)
                if (q.quest_id == s.current_quest_id())
                { source_item = q.source_item_id; break; }
            if (source_item != 0)
            {
                // Confirm bot still has the item.
                bool has_item = false;
                for (auto const& it : s.raw().inventory.bag_items)
                    if (it.entry == source_item) { has_item = true; break; }
                if (has_item)
                {
                    const int32 target_id = s.current_objective().object_id;
                    NearbyUnit const* friendly = nullptr;
                    float bestSq = std::numeric_limits<float>::max();
                    for (auto const& u : s.raw().combat.nearby_friends)
                    {
                        if (uint32(target_id) != u.entry) continue;
                        if (u.hp <= 0) continue;
                        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                        const float dsq = dx*dx + dy*dy + dz*dz;
                        if (dsq < bestSq) { bestSq = dsq; friendly = &u; }
                    }
                    if (friendly)
                    {
                        constexpr float kInteract = 5.0f;
                        if (bestSq <= kInteract * kInteract)
                        {
                            // In range — use the quest item targeting
                            // the wounded NPC. Server fires the item's
                            // spell; the spell credits the objective.
                            emit.emit(UseItemByEntryIntent{source_item, friendly->guid});
                            ai.set_last_rule_fired("idle:quest_use_item_on_friend");
                            return true;
                        }
                        // Walk closer — small step toward the friendly.
                        const float dist = std::sqrt(bestSq);
                        const float kStep = 25.f;
                        const float scale = std::min(kStep, dist) / dist;
                        const float dx = friendly->x - bx, dy = friendly->y - by;
                        const float tx = bx + dx * scale;
                        const float ty = by + dy * scale;
                        // Threat look-ahead: pull hostile in the path
                        // first instead of running through it.
                        if (NearbyUnit const* threat = s.path_threat(
                                tx, ty,
                                /*max_forward*/ std::min(kStep, 35.0f),
                                /*half_width*/  10.0f))
                        {
                            if (emit.start_attack(threat->guid))
                            {
                                ai.set_last_rule_fired("idle:walk_friend_pull_threat");
                                return true;
                            }
                        }
                        emit.move_to(tx, ty, friendly->z, /*run*/ true);
                        ai.set_last_rule_fired("idle:walk_to_quest_friend");
                        return true;
                    }
                    // CombatLoop FIX A2: the objective friendly is NOT in
                    // nearby_friends (scan-MISS — wrong location / not yet
                    // spawned in range). Previously the rule fell through to the
                    // kill scan, which is gated !talk_credit and so skips this
                    // friendly-faction target, leaving the bot looping in place.
                    // Instead WALK to the objective POI (the builder resolves it
                    // to the NEAREST same-map spawn of the target — FIX C) so the
                    // bot heads to where the friendly actually lives, then this
                    // rule re-fires on arrival. Gated on a real interact
                    // mechanism: we are inside `source_item != 0 && has_item`, so
                    // the quest DOES provide a usable item — the bot WILL have an
                    // action on arrival (use_item_by_entry), not loop at the POI.
                    // (Friendly objectives with NEITHER a source item NOR a talk
                    // option never reach here — no source_item → this whole block
                    // is skipped → FIX D's scan_miss blacklist recovers them.)
                    else if (s.current_objective_poi().valid &&
                             s.current_objective_poi().map_id == s.map_id() &&
                             !s.in_combat() && !s.is_casting() &&
                             !s.raw().movement.is_mounted)
                    {
                        const auto& poi = s.current_objective_poi();
                        const float pdx = poi.x - bx, pdy = poi.y - by;
                        const float pdsq = pdx * pdx + pdy * pdy;
                        const float kArrive = poi.radius > 0.f ? poi.radius : 40.0f;
                        if (pdsq > kArrive * kArrive)
                        {
                            const float dist = std::sqrt(pdsq);
                            const float kStep = 35.f;
                            const float scale = std::min(kStep, dist) / dist;
                            emit.move_to(bx + pdx * scale, by + pdy * scale, poi.z, /*run*/ true);
                            ai.set_last_rule_fired("idle:walk_to_quest_friend_poi");
                            return true;
                        }
                        // Already at the POI but the friendly still isn't in
                        // range — fall through; FIX D's scan_miss strike track
                        // blacklists this objective in ~30s so the Builder swaps.
                    }
                }
            }
        }

        // No Passive gate: quest kills are mandatory work (see combat_capable).
        // Dungeon gate: a bot ON an active dungeon run must NOT target open-world
        // quest mobs that happen to share the instance map (Deadmines: Q27781/Q27756
        // both KILL Lieutenant Horatio Laine 46612 at the entrance crime-scene POI
        // -71,-406). quest_kill set the victim, the opener walked the bot 140y NE to
        // it, and the tank's guard_group chased to protect the straggler — the whole
        // squad oscillated off the Helix route (observed live 2026-06-26). Boss
        // progression owns target selection during the run (mirrors pursue_quest_goal).
        if (hp_above_engage_gate &&
            !ai.dungeon_active() &&
            s.has_current_objective() &&
            (s.current_objective().type == /*MONSTER*/ 0 ||
             s.current_objective().type == /*KILL_WITH_LABEL*/ 21) &&
            // talk_credit MONSTER objectives ("Speak with X", friendly target)
            // are executed by the quest_talk rule later in this dispatch — the
            // target never appears in nearby_enemies, so scanning here is dead
            // work that would shadow nothing; skipping keeps the fall-through
            // to the talk rule unconditional.
            !s.current_objective().talk_credit)
        {
            const auto& obj = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            // (note_obj_observed is called by the centralized observer at the
            // top of can_autoact — don't double-tick stuck counters here.)
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                const float kQuestEngageRange = 40.0f;
                const float kQuestEngageRangeSq = kQuestEngageRange * kQuestEngageRange;
                NearbyUnit const* best     = nullptr;
                float             bestDist = kQuestEngageRangeSq + 1.f;
                const ObjectGuid recent = ai.last_engage_target();
                const bool recent_active = !recent.IsEmpty() &&
                                           (now_ms - ai.last_engage_at_ms() < 8000u);
                // For MONSTER objectives the entry-match list is `object_id`
                // plus any KillCredit aliases. For KILL_WITH_LABEL it's the
                // pre-resolved labeled_target_entries list (object_id is a
                // label id, not a creature entry).
                auto entry_matches = [&](uint32 entry) -> bool {
                    if (obj.type == /*MONSTER*/ 0)
                    {
                        if (entry == uint32(obj.object_id)) return true;
                        for (uint32 alias : obj.credit_alias_entries)
                            if (alias == entry) return true;
                        return false;
                    }
                    // KILL_WITH_LABEL
                    for (uint32 t : obj.labeled_target_entries)
                        if (t == entry) return true;
                    return false;
                };
                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (!u.guid.IsCreature()) continue;
                    if (!entry_matches(u.entry)) continue;
                    if (u.hp <= 0) continue;
                    if (u.victim != ObjectGuid::Empty) continue;
                    if (recent_active && u.guid == recent) continue;
                    const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                    const float distSq = dx*dx + dy*dy + dz*dz;
                    if (distSq > kQuestEngageRangeSq) continue;
                    if (distSq < bestDist) { bestDist = distSq; best = &u; }
                }
                if (best)
                {
                    // Gate on the StartAttack lockout so a refused attack
                    // (immune / phased / faction-locked / vehicle / already
                    // attacking) doesn't claim the dispatch slot every tick
                    // — let lower-priority rules run when the lockout
                    // absorbs the emit. note_engage stays inside the success
                    // branch so the "recent target" filter still skips this
                    // creature on subsequent ticks.
                    if (!emit.start_attack(best->guid)) return true;
                    ai.note_engage(best->guid, now_ms);
                    ai.set_last_rule_fired("idle:quest_kill");
                    return true;
                }
            }
        }

        // ---- Quest-collect-kill rule: prioritize creatures that drop the
        //      target quest item ----
        // Mirrors quest_kill but for QUEST_OBJECTIVE_ITEM (type=1). Walks
        // nearby_enemies looking for creatures whose quest item drop list
        // (sObjectMgr->GetCreatureQuestItemList) contains the target item id
        // (current_objective.object_id is the item entry for ITEM objectives).
        // Uses the bot's active map difficulty so heroic-only quest drops
        // resolve correctly. Same range / engage gates as quest_kill.
        // No Passive gate: quest collect-kills are mandatory work.
        if (hp_above_engage_gate &&
            s.has_current_objective() && s.current_objective().type == /*ITEM*/ 1)
        {
            const auto& obj = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                const float kQuestEngageRange = 40.0f;
                const float kQuestEngageRangeSq = kQuestEngageRange * kQuestEngageRange;
                NearbyUnit const* best     = nullptr;
                float             bestDist = kQuestEngageRangeSq + 1.f;
                const ObjectGuid recent = ai.last_engage_target();
                const bool recent_active = !recent.IsEmpty() &&
                                           (now_ms - ai.last_engage_at_ms() < 8000u);
                const Difficulty diff = static_cast<Difficulty>(s.raw().instance_ctx.map_difficulty);
                for (auto const& u : s.raw().combat.nearby_enemies)
                {
                    if (!u.guid.IsCreature()) continue;
                    if (u.hp <= 0) continue;
                    if (u.victim != ObjectGuid::Empty) continue;
                    if (recent_active && u.guid == recent) continue;
                    std::vector<uint32> const* drops =
                        sObjectMgr->GetCreatureQuestItemList(u.entry, diff);
                    if (!drops) continue;
                    bool match = false;
                    for (uint32 item_id : *drops)
                        if (item_id == uint32(obj.object_id)) { match = true; break; }
                    if (!match) continue;
                    const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                    const float distSq = dx*dx + dy*dy + dz*dz;
                    if (distSq > kQuestEngageRangeSq) continue;
                    if (distSq < bestDist) { bestDist = distSq; best = &u; }
                }
                if (best)
                {
                    // See idle:quest_kill above — gate on lockout to avoid
                    // claiming dispatch every tick when StartAttack is
                    // refused.
                    if (!emit.start_attack(best->guid)) return true;
                    ai.note_engage(best->guid, now_ms);
                    ai.set_last_rule_fired("idle:quest_collect_kill");
                    return true;
                }
            }
        }

        // ---- Engage rule (skipped for Passive personality + when wounded) ----
        // Also skip when the bot has active QUEST targets — opportunistic
        // pulls on random hostile critters (Tall Strider, Fawn, Goretusk)
        // waste time and threaten the bot's HP for zero quest progress.
        // Verified 2026-05-20: Uraimus (L1 NE) attacking deer next to a
        // Young Nightsaber quest target because engage_nearby_mob picked
        // the closer hostile. The quest dispatcher only engages from
        // actionable_objectives, so if any are present we trust the
        // quest pipeline and skip generic engage.
        bool _has_quest_engage_target = false;
        for (auto const& ao : s.actionable_objectives())
        {
            if (ao.type == /*MONSTER*/ 0 ||
                ao.type == /*KILL_WITH_LABEL*/ 21 ||
                (ao.type == /*ITEM*/ 1 && ao.source_guid.IsCreature()))
            {
                _has_quest_engage_target = true;
                break;
            }
        }
        // Don't grind random mobs IN PLACE when there's a FAR same-map objective
        // to travel to — head there instead (the POI-pathing / travel rules below
        // move the bot toward the goal). Without this, a bot whose quest target is
        // across the zone loops engage→kill→engage forever and never arrives
        // (2026-06-15: Somi, quest-845 target ~1200y away, stuck auto-attacking
        // Durotar mobs at (272,-3309) for 180s+ = the dominant CombatLoop wedge,
        // 87 bots/run). A NEAR objective (bot already in the quest area, ≤50y)
        // still allows the opportunistic engage so the bot can clear its way in.
        // Lowered 80→50y (2026-06-16 CombatLoop FIX B): at 50-80y from the quest
        // POI the bot was re-pulling a nearby cluster and re-looping instead of
        // walking the last leg to the goal; 50y still lets it clear adds inside
        // the quest area while forcing the final approach to fall through to
        // idle:quest_path.
        // Defensive combat is unaffected — a mob that ATTACKS the bot is handled
        // by the InCombat APL; this only suppresses PROACTIVE grinding. Cross-map
        // objectives keep grind-as-fallback (their travel is fragile; the
        // relocation pipeline owns them) — same-map only here.
        bool _far_travel_objective = false;
        if (s.has_current_objective() && s.current_objective_poi().valid &&
            s.current_objective_poi().map_id == s.map_id())
        {
            const float odx = s.current_objective_poi().x - bx;
            const float ody = s.current_objective_poi().y - by;
            _far_travel_objective = (odx * odx + ody * ody) > (50.0f * 50.0f);
        }
        if (agg != Aggression::Passive && hp_above_engage_gate && !_has_quest_engage_target &&
            !_far_travel_objective)
        {
            // Engage range / level band scale with Aggression. Aggressive bots
            // pull from further out and tolerate higher-level mobs (more risk,
            // more XP); defensive bots stay close and pick same-level or lower.
            const float kEngageRange =
                agg == Aggression::Defensive ? 20.0f :
                agg == Aggression::Aggressive ? 35.0f :
                30.0f;
            const float kEngageRangeSq = kEngageRange * kEngageRange;
            constexpr int32 kMinHpPctToTag = 50;
            const uint8 levelSpan =
                agg == Aggression::Defensive ? 2u :
                agg == Aggression::Aggressive ? 5u :
                4u;
            const uint8 botLevel = s.level();
            // Retail level scaling (Chromie Time / zone scaling) scales a zone's
            // mobs UP to the bot's level, so a LOWER-base-level mob in the bot's
            // current zone still awards FULL XP. The old `u.level < minLevel`
            // lower bound assumed pre-scaling "gray = no XP" and excluded exactly
            // those mobs — so an out-of-base-bracket bot (e.g. L14 in Teldrassil,
            // mobs base L5-10) found NO target and idle:engage_nearby_mob fired
            // 0x fleet-wide, leaving quest-starved bots at 0 XP. Drop the lower
            // bound; keep an upper cap so the bot doesn't open on a genuinely
            // higher, un-scaled mob from an above-range zone.
            const uint8 maxLevel = uint8(botLevel + levelSpan);

            NearbyUnit const* best     = nullptr;
            float             bestDist = kEngageRangeSq + 1.f;
            // Conservative single-pull (2026-06-21): a squishy pet class WITHOUT
            // its tank pet — a Warlock before Summon Voidwalker @L10 (only the Imp)
            // — plays like a human and pulls ONE ISOLATED mob at a time so adds
            // don't gang up and kill the soft caster. Track the nearest isolated
            // candidate (no other live hostile within the cluster radius); prefer
            // it over a closer-but-packed one. Falls back to `best` when nothing is
            // isolated, so the bot still grinds in a dense camp (never frozen).
            const bool conservative_pull =
                (s.cls() == CLASS_WARLOCK && !s.knows_spell(697 /*Summon Voidwalker*/));
            NearbyUnit const* bestIso     = nullptr;
            float             bestIsoDist = kEngageRangeSq + 1.f;
            constexpr float   kAddClusterRadiusSq = 12.0f * 12.0f;

            // Skip the most recently attempted target for ~8s after the attempt
            // — covers the LOS/terrain/pathing-failure case where start_attack
            // would otherwise loop infinitely on the same unreachable mob. After
            // the cooldown, either the bot has wandered out of range (mob no
            // longer in nearby_enemies, no skip needed) or it can re-try with
            // fresh state. A successful engage removes the mob from the snapshot
            // (combat → kill) before the cooldown expires, so the cooldown is
            // moot in the success case.
            // Shield duration is variable now: 8s for ordinary engage
            // attempts, minutes when the opener proved the target
            // unreachable (casts OutOfRange + approach steps both failed).
            const uint32 now_ms = s.published_at_ms();
            const ObjectGuid recent = ai.last_engage_target();
            const bool recent_active = !recent.IsEmpty() &&
                                       (now_ms - ai.last_engage_at_ms() < ai.last_engage_shield_ms());

            for (auto const& u : s.raw().combat.nearby_enemies)
            {
                // Creatures only — nearby_enemies includes opposing-faction
                // Players on PvP-flagged servers, and bots shouldn't open
                // unprovoked PvP on real players. Players, pets, and vehicles
                // are filtered here. Combat APL still handles them when the
                // player attacks the bot first (snapshot.attackers path).
                if (!u.guid.IsCreature()) continue;
                // Never grind-engage a no-XP target: Training Dummies are
                // attackable, never die, and award nothing — opening on one
                // wedges the bot InCombat forever with zero yield (verified
                // live: L22 + L34 bots stuck on Razor Hill dummies for hours).
                if (u.no_xp_kill || u.is_pacified || u.untargetable) continue;   // dummies/props/stalkers: immortal, can't fight back, or untargetable
                if (u.level > maxLevel) continue;   // no lower bound: level scaling gives XP on lower-base mobs
                if (u.hp_pct() < kMinHpPctToTag) continue;
                if (u.victim != ObjectGuid::Empty) continue;
                if (recent_active && u.guid == recent) continue;
                const float dx = u.x - bx;
                const float dy = u.y - by;
                const float dz = u.z - bz;
                const float distSq = dx*dx + dy*dy + dz*dz;
                if (distSq > kEngageRangeSq) continue;
                if (distSq < bestDist) { bestDist = distSq; best = &u; }
                // Isolated-target tracking for conservative single-pull: would
                // pulling `u` drag adds? Skip it as an isolated candidate if any
                // other live, attackable hostile creature sits within the cluster
                // radius of it.
                if (conservative_pull && distSq < bestIsoDist)
                {
                    bool has_add = false;
                    for (auto const& other : s.raw().combat.nearby_enemies)
                    {
                        if (&other == &u || !other.guid.IsCreature()) continue;
                        if (other.no_xp_kill || other.is_pacified) continue;
                        const float ox = other.x - u.x, oy = other.y - u.y, oz = other.z - u.z;
                        if (ox*ox + oy*oy + oz*oz <= kAddClusterRadiusSq) { has_add = true; break; }
                    }
                    if (!has_add) { bestIsoDist = distSq; bestIso = &u; }
                }
            }

            // Prefer an isolated target for a no-tank-pet caster; else nearest.
            NearbyUnit const* chosen = (conservative_pull && bestIso) ? bestIso : best;
            if (chosen)
            {
                emit.start_attack(chosen->guid);
                ai.note_engage(chosen->guid, now_ms);
                ai.set_last_rule_fired("idle:engage_nearby_mob");
                return true;
            }
        }

        // ---- Quest ender pathing (walk-to-quest-ender) ----
        // PRIORITY (travel-engagement fix 2026-06-15): turning in COMPLETE
        // quests outranks the filler vendor/repair/wander rules below. Was at
        // the bottom of the cascade, so a bot with completed quests in hand
        // wandered to a service NPC (or aimlessly) instead of turning them in —
        // Uraimus looped idle:wander_to_service + path_fail at the Dolanaar inn
        // with 6 completable quests. Placed AFTER active quest-pursuit
        // (quest_walk_to_target / walk_to_quest_friend, above) and BEFORE the
        // filler rules so productive turn-in always wins over wandering.
        //
        // Bridges the gap between "quest is complete in log" and "ender NPC is
        // too far to appear in snapshot's quest_turnins list" (40y on the world,
        // 500y in hub clusters): BotSnapshotBuilder populated QuestEntry.
        // ender_x/y/z; we walk the closest in-map COMPLETE quest with
        // ender_resolved=true. The 5y quest_turnin rule fires on arrival.
        //
        // Gates: same map only (cross-map ender uses idle:travel_plan); quest
        // state == 1 (complete); not in combat/casting/mounted; skip when
        // quest_turnins[] is already non-empty (close-approach handled elsewhere).
        if (!s.in_combat() && !s.is_casting() && !s.raw().movement.is_mounted &&
            s.raw().quest_discovery.quest_turnins.empty())
        {
            const uint32 my_map = s.map_id();
            float best_ender_dsq = std::numeric_limits<float>::infinity();
            float best_ex = 0.f, best_ey = 0.f, best_ez = 0.f;
            bool  found_ender = false;
            for (auto const& q : s.raw().quest_log.quests)
            {
                if (q.state != 1) continue;          // not complete yet
                if (!q.ender_resolved) continue;     // no spawn position
                if (q.ender_map_id != my_map) continue;
                const float dx = q.ender_x - bx;
                const float dy = q.ender_y - by;
                const float dsq = dx*dx + dy*dy;
                if (dsq < best_ender_dsq)
                {
                    best_ender_dsq = dsq;
                    best_ex = q.ender_x;
                    best_ey = q.ender_y;
                    best_ez = q.ender_z;
                    found_ender = true;
                }
            }
            // Stop at 35y so it OVERLAPS the snapshot's 40y turn-in scan (which
            // then surfaces the ender via quest_turnins[]) rather than leaving a
            // 40–60y dead band where nothing walks the last stretch.
            constexpr float kArriveSq = 35.0f * 35.0f;
            // FULL-PATH ender routing (2026-06-17). Cap the rule at a walkable max:
            // beyond ~2500y the ender is cross-continent-scale and belongs to the flight
            // cascade (a single CalculatePath that far is wasteful); fall through there.
            constexpr float kMaxEnderWalkSq = 2500.0f * 2500.0f;
            if (found_ender && best_ender_dsq > kArriveSq && best_ender_dsq <= kMaxEnderWalkSq)
            {
                // Wedge-guard (added with the priority move 2026-06-15): now that
                // turn-in runs HIGH, an unreachable ender (mesh-walled, wrong
                // pocket) must not create a NEW path-fail loop in its place. If we
                // can't make progress toward it, YIELD so the cascade falls
                // through to the travel rules / wedge-watchdog recovery below.
                if (ai.check_anchor_wedge("idle:walk_to_quest_ender",
                                          s.path_blocked_count(), s.published_at_ms()))
                    return false;
                const float dist = std::sqrt(best_ender_dsq);
                // Per-personality step — used ONLY for the near-corridor threat sweep,
                // not as the move target. The previous version emitted move_to this 35y
                // greedy hop, which always heads straight at the ender and gets trapped in
                // a local minimum on any route that needs a detour (around a river/cliff/
                // building) — the dominant walk_to_quest_ender wedge cluster (~62 bots,
                // 2026-06-17). We now emit move_to the ender's ACTUAL position so
                // PathGenerator computes the whole route around geometry. Cost is bounded:
                // the PathBudget caps aggregate world-thread pathfinding per tick (a far-
                // ender surge cannot hang the world thread), the 1.5s move_to dedup makes
                // it ~one CalculatePath per 1.5s per bot (not per tick), and the wedge-guard
                // above still bails a genuinely-unreachable ender to the travel cascade.
                const float kStep =
                    ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                    ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                    35.0f;
                const float scale = std::min(kStep, dist) / dist;
                const float tx = bx + (best_ex - bx) * scale;   // near waypoint, threat sweep only
                const float ty = by + (best_ey - by) * scale;
                // Threat look-ahead: the ender NPC is friendly, so every hostile
                // in the corridor is a genuine threat (no exclude_guid).
                if (NearbyUnit const* threat = s.path_threat(
                        tx, ty,
                        /*max_forward*/ std::min(kStep, 35.0f),
                        /*half_width*/  10.0f))
                {
                    if (emit.start_attack(threat->guid))
                    {
                        ai.set_last_rule_fired("idle:walk_ender_pull_threat");
                        return true;
                    }
                }
                // Full route to the ender — PathGenerator detours around obstacles.
                emit.move_to(best_ex, best_ey, best_ez, /*run*/ true);
                ai.set_last_rule_fired("idle:walk_to_quest_ender");
                return true;
            }
        }

        // ---- Wander-to-service redirect (vendor / repair when urgent) ----
        // Bigger attract radius (25y) than node/quest because these are
        // recovery actions: bag is full and the bot is dropping loot, or
        // gear is about to break. Vendor flag covers both sell-trash and
        // repair (most repair NPCs are also vendors). Picks the nearest
        // qualifying NPC. Skipped if neither condition is critical, so a
        // bot with empty bags + healthy gear ignores vendors.
        {
            // HUMAN MODEL (2026-06-15): this is the OPPORTUNISTIC 25y redirect —
            // it only fires when a vendor/repair NPC is already within 25y (i.e.
            // the bot is passing one, typically at a quest hub). Even so, keep the
            // triggers tight so it doesn't yank a bot off a nearby quest: genuinely
            // full bags (≤2, loot stops) or critically-low durability (<30%, about
            // to fail) — NOT the old <50% which had bots ducking to repair while
            // still perfectly able to fight and quest.
            const bool need_vendor = s.bag_free_slots() <= 2;
            const bool need_repair = s.lowest_equipped_durability_pct() < 30;
            if (need_vendor || need_repair)
            {
                constexpr float kSvcAttract   = 25.0f;
                constexpr float kSvcAttractSq = kSvcAttract * kSvcAttract;
                constexpr float kSvcNoAct     = 25.0f;     // 5² — already in interact range
                NearbyUnit const* best_svc = nullptr;
                float best_svc_distSq = kSvcAttractSq + 1.f;
                // Accept either flag — repair NPCs are usually also vendors,
                // and even a vendor-only NPC clears bag pressure (the most
                // urgent of the two recovery actions when both are critical).
                // Repair will happen later when the bot drifts past a repair
                // NPC, by which point the bot can keep fighting with cleared
                // bags from this visit.
                const uint32 want_flag =
                    (need_repair ? uint32(UNIT_NPC_FLAG_REPAIR) : 0u) |
                    (need_vendor ? uint32(UNIT_NPC_FLAG_VENDOR) : 0u);
                for (auto const& u : s.raw().combat.nearby_friends)
                {
                    if (!(u.npc_flags & want_flag)) continue;
                    const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq > kSvcAttractSq || dsq <= kSvcNoAct) continue;
                    if (dsq < best_svc_distSq) { best_svc_distSq = dsq; best_svc = &u; }
                }
                if (best_svc)
                {
                    // Wedge-guard this previously-unguarded 25y opportunistic
                    // redirect: if the bot can't make progress to a near-but-
                    // walled-off vendor (186K blocks on one bot in the 4-day run),
                    // YIELD so the proper-routing idle:travel_to_vendor takes over.
                    // No blacklist — the vendor is reachable via the door, just not
                    // by this short straight-line redirect.
                    if (ai.check_anchor_wedge("idle:wander_to_service",
                                              s.path_blocked_count(), s.published_at_ms()))
                        return false;
                    const float dist = std::sqrt(best_svc_distSq);
                    const float scale = (dist - 4.0f) / dist;
                    const float tx = bx + (best_svc->x - bx) * scale;
                    const float ty = by + (best_svc->y - by) * scale;
                    emit.move_to(tx, ty, best_svc->z, /*run*/ true);
                    ai.set_last_rule_fired("idle:wander_to_service");
                    return true;
                }
            }
        }

        // ---- Walk-toward-trainer (profession recipe acquisition) ----
        // The pipeline grants profession SKILLS but not recipes. The
        // existing idle:trainer rule fires trainer_buy_all only at 5 y
        // interact range — opportunistic, requires the bot to already
        // be standing next to a trainer. Without this redirect, bots
        // with empty known_recipes never visit trainers organically:
        // population telemetry showed 0 / 1237 online bots had any
        // crafting skill > rank 1 after hours of runtime.
        //
        // Fires when:
        //   (a) bot has at least one trained profession (skill > 0),
        //   (b) total known_recipes is sparse (< 12 entries — the
        //       threshold a fresh apprentice rank-1 + first-tier
        //       trainer visit would push past),
        //   (c) a trainer NPC is in nearby_friends and outside the 5 y
        //       interact range covered by idle:trainer (otherwise that
        //       rule already handles it).
        // Activity-mode aware: 40 y attract during Questing, 200 y
        // during Professioning so a bot in profession mode crosses the
        // city to reach a trainer.
        // ---- idle:travel_to_vendor ----
        // Symmetric to idle:travel_to_trainer. The existing idle:vendor_visit
        // rule fires only when a vendor NPC is already within 5y. Bots
        // questing far from the nearest town who accumulate full bags
        // (bag_free_slots <= 4) or low durability (lowest_dura_pct < 70)
        // would loop on quest activities forever without disposition.
        // This walk-rule attracts toward the closest vendor when:
        //   - vendor_visit_phases_pending has the bag-full (bit 1) OR
        //     critical-dura (bit 0) flag set; AND
        //   - a vendor / repair NPC is in nearby_friends within 80y; AND
        //   - bot isn't already in the 5y interact ring (vendor_visit
        //     handles that).
        {
            const uint8 vv_phases = s.vendor_visit_phases_pending();
            constexpr uint8 kVvBagFull = 1u << 1;
            // HUMAN MODEL (2026-06-15): a DEDICATED vendor trip (this rule walks up
            // to 80y, or routes to a whole town when out in the wilderness) only
            // makes sense when bags are genuinely full — that's the only
            // maintenance need that actually blocks progress (can't loot). Low
            // durability no longer triggers a special trip (kVvDuraLow dropped);
            // repair happens opportunistically at hub vendors / via the 25y
            // wander_to_service redirect when passing one. This stops bots
            // abandoning quests to cross the zone for a minor repair/grey-sell.
            if ((vv_phases & kVvBagFull) != 0)
            {
                NearbyUnit const* vendor =
                    s.nearest_npc_with_flag(UNIT_NPC_FLAG_REPAIR);
                if (!vendor)
                    vendor = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR);
                // Wedge guard (R5, 2026-06-03): the "nearest" vendor can be
                // unreachable (no vendor in a starter zone, or one walled off
                // on the same map). Without this the rule re-emits move_to every
                // tick forever and never escalates — observed live: Ralel (L5)
                // wedged 438 consecutive ticks, 100% on idle:travel_to_vendor.
                // Suppress the rule after it wedges so quest/hub/wander rules
                // below get a turn (mirrors idle:travel_to_hub / walk_to_taxi).
                if (vendor && ai.check_anchor_wedge("idle:travel_to_vendor",
                        s.path_blocked_count(), s.published_at_ms()))
                    vendor = nullptr;
                if (vendor)
                {
                    constexpr float kVvAttract = 80.0f;
                    constexpr float kVvAttractSq = kVvAttract * kVvAttract;
                    constexpr float kVvNoActSq = 25.0f;       // 5y² interact
                    const float vdx = vendor->x - bx, vdy = vendor->y - by, vdz = vendor->z - bz;
                    const float vdsq = vdx*vdx + vdy*vdy + vdz*vdz;
                    if (vdsq <= kVvAttractSq && vdsq > kVvNoActSq)
                    {
                        // Vendor on an elevated platform (Orgrimmar/Undercity rim,
                        // a zeppelin deck): route through the UnifiedTravelGraph so
                        // the bot rides the city lift UP — the SAME travel-manager
                        // path the flight masters use (walk → Elevator edge → walk to
                        // the vendor floor). Once at the vendor's floor this rule
                        // walks the last few yards into interact range. Without this
                        // the bot walks straight at the platform vendor and NoPaths
                        // up the shaft.
                        if (vendor->z - bz >= 15.0f &&
                            driveTravelPlanTo(s.map_id(), vendor->x, vendor->y, vendor->z))
                            return true;
                        const float vdist = std::sqrt(vdsq);
                        const float vscale = (vdist - 4.0f) / vdist;
                        emit.move_to(bx + (vendor->x - bx) * vscale,
                                     by + (vendor->y - by) * vscale,
                                     vendor->z, /*run*/ true);
                        ai.set_last_rule_fired("idle:travel_to_vendor");
                        return true;
                    }
                }
                // Bags genuinely full but NO vendor in scan range (out questing in
                // the wilderness, or the only nearby vendor was unreachable/wedged):
                // head to the nearest town — the nearest level-appropriate quest hub,
                // where vendors live — via the travel graph so the bot can actually
                // SELL. Without this a full-bag bot can't loot and never disposes
                // (the [loot_give_up] bag_free=0 case in the forensics); on arrival
                // the 80y/5y vendor rules above + idle:vendor_visit handle the sale.
                // Gated on a HARD-full bag (<=2 free) so a merely-low bot keeps
                // questing. The graph route composes walk/flight/elevator to the town.
                if (!vendor && (vv_phases & kVvBagFull) && s.bag_free_slots() <= 2)
                {
                    if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
                        if (hub->mapId == s.map_id() &&
                            driveTravelPlanTo(s.map_id(),
                                              hub->location.GetPositionX(),
                                              hub->location.GetPositionY(),
                                              hub->location.GetPositionZ()))
                        {
                            ai.set_last_rule_fired("idle:travel_to_vendor");
                            return true;
                        }
                }
            }
        }

        // ---- idle:proactive_repair_route ----
        // OPPORTUNISTIC repair top-up. The builder sets vendor_visit bit5
        // (0x20) when gear is heading toward failure (<35% durability) and the
        // bot can afford the repair, but it is NOT yet critical (<30%, bit0,
        // handled by the vendor FSM / travel_to_vendor's bag-full path). This
        // rule routes the bot to a real repair vendor BEFORE it breaks — but
        // only when it has nothing better to do.
        //
        // RECONCILE (2026-06-15): low durability was deliberately REMOVED as a
        // dedicated-trip trigger because forcing a cross-zone repair made bots
        // abandon quests. So this stays strictly OPPORTUNISTIC: it fires only
        // when the bot is OOC (AutoactDispatch's can_autoact gate) AND has NO
        // reachable actionable quest (has_actionable_quest, same check the
        // vendor gates use). The FORCED / quest-overriding repair is reserved
        // for the CRITICAL case (0% / death-spiral) in State_Dead /
        // State_InCombat. It routes through the BOUNDED UnifiedTravelGraph
        // composer (driveTravelPlanTo), never a raw per-tick CalculatePath.
        {
            constexpr uint8 kVvRepairSoon = 1u << 5;   // builder bit5 (0x20)
            const uint8 vv_phases = s.vendor_visit_phases_pending();
            if ((vv_phases & kVvRepairSoon) != 0 && !s.has_actionable_quest())
            {
                // Same-map real repair spawn first; else nearest same-map quest
                // hub (hubs have vendors). Cross-map hubs are left to the normal
                // relocation pipeline — a proactive top-up never crosses a
                // continent. Pure lock-free index reads (off the Build path).
                uint32 rmap = 0; float rx = 0.f, ry = 0.f, rz = 0.f; bool have = false;
                if (auto hit = Services::RepairVendors().GetNearestRepairVendor(s.raw()))
                { rmap = hit->map_id; rx = hit->x; ry = hit->y; rz = hit->z; have = true; }
                else if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
                {
                    if (hub->mapId == s.map_id())
                    {
                        rmap = hub->mapId;
                        rx = hub->location.GetPositionX();
                        ry = hub->location.GetPositionY();
                        rz = hub->location.GetPositionZ();
                        have = true;
                    }
                }
                if (have)
                {
                    // Drive the full approach through the bounded composer. The
                    // composer's final walk leg brings the bot to the vendor
                    // floor; once inside the 5y interact ring the higher-priority
                    // vendor_visit rule (registry prio 500, runs before this
                    // autoact block) fires and repairs (its phase-1 now accepts
                    // bit5). Only stop driving once we're essentially at the NPC,
                    // so a bit5-only bot doesn't stall in the 5-80y gap (no other
                    // rule attracts <35% gear toward a vendor by design).
                    const float ddx = rx - bx, ddy = ry - by;
                    if (ddx*ddx + ddy*ddy > 8.0f * 8.0f &&
                        driveTravelPlanTo(rmap, rx, ry, rz))
                    {
                        ai.set_last_rule_fired("idle:proactive_repair_route");
                        return true;
                    }
                }
            }
        }

        {
            constexpr uint16 kProfSkills[] = {
                164,   // Blacksmithing
                165,   // Leatherworking
                171,   // Alchemy
                182,   // Herbalism
                185,   // Cooking
                186,   // Mining
                197,   // Tailoring
                202,   // Engineering
                333,   // Enchanting
                356,   // Fishing
                393,   // Skinning
                755,   // Jewelcrafting
                773,   // Inscription
            };
            bool has_any_prof = false;
            for (uint16 sk : kProfSkills)
                if (s.has_skill(sk)) { has_any_prof = true; break; }
            const bool sparse_recipes = s.raw().spellbook.known_recipes.size() < 12;
            if (has_any_prof && sparse_recipes)
            {
                if (auto const* tr = s.nearest_npc_with_flag(UNIT_NPC_FLAG_TRAINER))
                {
                    // Skip the walk-attract if the nearest trainer is
                    // already failed-locked by idle:trainer (15min). The
                    // nearest_npc_with_flag returns a SINGLE NPC; without
                    // this guard the bot would walk back to a trainer that
                    // can teach it nothing, idle there until something
                    // else moves it, then walk back again. Lockout expires
                    // naturally — by then the bot has typically leveled or
                    // travelled and re-evaluates against a different
                    // nearest NPC.
                    const uint32 now_ms_tt = GameTime::GetGameTimeMS();
                    if (ai.action_recently_tried(BotAI::ActionKind::TrainerLearn,
                                                 tr->guid.GetCounter(), now_ms_tt))
                    {
                        // fall through — other rules below may still fire
                    }
                    else
                    {
                        const float kAttract = _prof_mode ? 200.0f : 40.0f;
                        const float kAttractSq = kAttract * kAttract;
                        constexpr float kNoActSq = 25.0f;       // 5y² — already in interact range
                        const float dx = tr->x - bx, dy = tr->y - by, dz = tr->z - bz;
                        const float dsq = dx*dx + dy*dy + dz*dz;
                        if (dsq <= kAttractSq && dsq > kNoActSq)
                        {
                            const float dist = std::sqrt(dsq);
                            const float scale = (dist - 4.0f) / dist;
                            const float tx = bx + (tr->x - bx) * scale;
                            const float ty = by + (tr->y - by) * scale;
                            emit.move_to(tx, ty, tr->z, /*run*/ true);
                            ai.set_last_rule_fired("idle:travel_to_trainer");
                            return true;
                        }
                    }
                }
            }
        }

        // ---- Wander-to-quest-hub redirect ----
        // The idle:quest_accept / idle:quest_turnin rules above fire only at
        // 5y interact range. The snapshot pre-resolves quest_offers and
        // quest_turnins (giver + quest_id) up to 40y. Walks to the CLOSEST
        // eligible giver.
        //
        // Two-tier radius based on whether the bot already has work to do:
        //  - 15y "opportunistic" — bot has quests in log; only detour to a
        //    giver that's already close, so existing objectives aren't
        //    abandoned for every passing quest hub.
        //  - 40y "aggressive seek" — bot has 0 quests in log; pull toward
        //    any visible giver in the full snapshot scan range. Without
        //    this, a quest-less bot whose nearest giver is >15y away just
        //    randomly wanders forever and may never drift into pickup
        //    range — leaving the bot effectively stuck (no quests to
        //    pursue → no quest:* rules → only engage_nearby_mob/wander).
        {
            // "No actionable quest" — either an empty log, OR a log that
            // holds only non-actionable entries that yield no current
            // objective (the classic case: the objective-less, ender-less
            // system quest 55660 "Time Trials" auto-granted to every
            // character — observed freezing L3 "Balast" in Durotar, whose
            // only quest was 55660 so quests.empty() was false and the
            // attract radius collapsed to 15y, leaving it unable to walk to
            // the chain-starter giver). has_current_objective() is true
            // whenever the bot has a pursuable objective OR a resolvable
            // (incl. same-map turn-in) breadcrumb, so this never abandons or
            // skips a real quest — a bot with genuine work keeps the 15y
            // opportunistic radius; only a bot with nothing to act on this
            // tick gets the 500y aggressive seek so it can reach a giver.
            const bool no_quests = !s.has_current_objective();
            // Three-tier attract radius:
            //  - no quests / has offer or turnin → 500y aggressive seek.
            //    Snapshot's nearby_friends carries 40y entries everywhere
            //    plus hub-extended quest-giver entries up to 500y when the
            //    bot stands inside a capital cluster (see the hub-aware
            //    scan in BotSnapshotBuilder). In wilderness, nearby_friends
            //    only ever has ≤ 40y entries — the 500y attract collapses
            //    to whatever is actually present. In a city it lets the
            //    rule walk the full cluster radius to reach an actionable
            //    giver instead of capping out at 40y and oscillating
            //    around the centroid forever.
            //  - quests in pursuit → 15y opportunistic (don't abandon
            //                        current objective for every passing
            //                        giver)
            const bool any_resolved_giver =
                !s.raw().quest_discovery.quest_offers.empty() || !s.raw().quest_discovery.quest_turnins.empty();
            // Q-P0a: when the bot has a valid SAME-MAP objective POI it should
            // be PURSUING that objective (via quest_path / quest_walk_to_target
            // below), not being yanked 500y across a capital toward an
            // unrelated quest giver. The 500y "aggressive seek" gravity is for
            // bots that are genuinely out of work (no actionable objective) or
            // that have a concretely resolved nearby offer/turn-in. A bot with
            // a real local objective keeps only the 15y opportunistic pull so a
            // giver it literally walks past is still grabbed, without abandoning
            // the objective walk. This was the capital-livelock root cause:
            // marooned bots with a far objective got dragged hub-to-hub forever.
            const bool has_local_obj =
                s.has_current_objective() &&
                s.current_objective_poi().valid &&
                s.current_objective_poi().map_id == s.map_id();
            const float kQuestAttract = ((no_quests || any_resolved_giver) && !has_local_obj)
                ? 500.0f : 15.0f;
            const float kQuestAttractSq = kQuestAttract * kQuestAttract;
            constexpr float kQuestNoAct = 9.0f;      // 3² — redirect until ~3y, then go silent (must stay BELOW kAcceptEmitSq 3.5² so accept fires while redirect is quiet — no 5y triple-tie out_of_range loop)
            float best_qdistSq = kQuestAttractSq + 1.f;
            float best_qx = 0.f, best_qy = 0.f, best_qz = 0.f;
            bool  qfound = false;

            auto consider = [&](ObjectGuid giver)
            {
                for (auto const& u : s.raw().combat.nearby_friends)
                    if (u.guid == giver)
                    {
                        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                        const float dsq = dx*dx + dy*dy + dz*dz;
                        if (dsq > kQuestAttractSq || dsq <= kQuestNoAct) return;
                        if (dsq < best_qdistSq)
                        { best_qdistSq = dsq; best_qx = u.x; best_qy = u.y; best_qz = u.z; qfound = true; }
                        return;
                    }
                for (auto const& o : s.raw().world_objects.nearby_objects)
                    if (o.guid == giver)
                    {
                        const float dx = o.x - bx, dy = o.y - by, dz = o.z - bz;
                        const float dsq = dx*dx + dy*dy + dz*dz;
                        if (dsq > kQuestAttractSq || dsq <= kQuestNoAct) return;
                        if (dsq < best_qdistSq)
                        { best_qdistSq = dsq; best_qx = o.x; best_qy = o.y; best_qz = o.z; qfound = true; }
                        return;
                    }
            };
            // In Professioning mode, do not get pulled toward quest givers
            // — bot lingers near gathering opportunities instead.
            if (!_prof_mode)
            {
                for (auto const& off : s.raw().quest_discovery.quest_offers)   consider(off.giver);
                for (auto const& tin : s.raw().quest_discovery.quest_turnins) consider(tin.giver);
            }
            if (qfound)
            {
                const float dist = std::sqrt(best_qdistSq);
                const float scale = (dist - 2.5f) / dist;     // stop 2.5y — inside the 3.5y accept-emit margin (drift headroom for the live interact gate)
                const float tx = bx + (best_qx - bx) * scale;
                const float ty = by + (best_qy - by) * scale;
                // Threat look-ahead: hub walks cross open terrain at the
                // start of every accepted quest; same death pattern as
                // the quest-walk-to-target rule.
                if (NearbyUnit const* threat = s.path_threat(
                        tx, ty,
                        /*max_forward*/ std::min(dist, 35.0f),
                        /*half_width*/  10.0f))
                {
                    if (emit.start_attack(threat->guid))
                    {
                        ai.set_last_rule_fired("idle:wander_hub_pull_threat");
                        return true;
                    }
                }
                emit.move_to(tx, ty, best_qz, /*run*/ true);
                ai.set_last_rule_fired("idle:wander_to_quest_hub");
                return true;
            }
        }

        // ---- Long-range travel to a quest hub (NEW) ----
        // Fires when the bot has nothing locally actionable and the
        // wander_to_quest_hub redirect above didn't fire (i.e. no eligible
        // giver was inside the 40y snapshot scan). The QuestHubDatabase holds
        // ~hundreds of DBSCAN-clustered quest hubs across the world; pick the
        // nearest level-/faction-appropriate one and emit a long move_to to
        // its center. The bot's MotionMaster handles the actual pathfinding;
        // each subsequent tick re-evaluates and may re-target if a closer
        // hub becomes apparent (e.g. on map change, although cross-map
        // travel still requires hearth/flight to actually arrive).
        //
        // Two trigger conditions:
        //   1. quests.empty() — classic "stuck forever in town with no quest"
        //      where idle:wander was gated off by rest_bonus_xp.
        //   2. quests in log but nothing locally actionable — observed with
        //      75 horde bots clustered at the Orgrimmar entrance teleport
        //      coords (1633,-4439) after PlaceInCapital. They had 2 quests
        //      each, but their objectives were back in Durotar starter zone
        //      and the snapshot scan from the AH spot showed no offers /
        //      turn-ins / current-objective POI. Without this clause they
        //      fell through to idle:wander, which without an anchor POI
        //      randomly stepped 25y in 5s buckets — netting zero progress.
        // A current objective with a valid POI is "actionable" — for both
        // same-map and CROSS-MAP goals. The old code (pre FIX C) treated a
        // cross-map objective as a dead end for LOW-LEVEL bots (<=9) on the
        // theory that they had no working autonomous cross-continent travel,
        // so it forced `nothing_local` true and sent them to a same-map hub
        // (DB-verified: L1 Undead "Somi" stuck 83h in Orgrimmar/map1 with its
        // only objective — quest 28608 — in Tirisfal on map 0).
        //
        // That bar is now removed: with the elevator-base hand-off (FIX B) +
        // ElevatorIndex auto-detection (sibling fix) the Orgrimmar lift +
        // Org→Undercity zeppelin chain works for low-level bots, so a
        // questless-locally L1 whose only actionable objective is cross-map
        // CAN now route there via the portal/dock anchor + travel-graph rules
        // (idle:walk_to_known_dock → elevator → zeppelin). Treating the
        // cross-map objective as actionable keeps `nothing_local` false, so
        // the long-range hub router yields and the cross-map travel rules run.
        //
        // Same-map preference is preserved structurally: a same-map objective
        // POI is equally `obj_actionable_here`, and the per-rule dispatch below
        // walks the bot to a same-map objective before any cross-map anchor is
        // consulted. We only stop FORCING low-level bots away from a legitimate
        // cross-map goal. This is now CONSISTENT with the portal-anchor
        // snapshot (BotSnapshotBuilder.cpp ~5860), which was never level-gated
        // and already emitted the cross-map dock anchor for these bots.
        const bool obj_actionable_here =
            s.has_current_objective() &&
            s.current_objective_poi().valid;
        const bool nothing_local =
            s.raw().quest_discovery.quest_offers.empty() &&
            s.raw().quest_discovery.quest_turnins.empty() &&
            !obj_actionable_here;
        // A complete quest whose turn-in NPC is spawned on the bot's CURRENT
        // map is a reachable, high-value action: idle:walk_to_quest_ender
        // (further down this dispatch) walks the bot to it and hands it in.
        // Suppress the long-range hub router whenever such a turn-in exists
        // so it cannot steal the tick and send the bot hub-hopping past a
        // quest it could complete right now. This is the root cause of L1
        // "Somi" sitting on 4 complete quests in Orgrimmar for ~96h: every
        // tick travel_to_hub fired (nothing_local was true because the ender
        // was >40y outside the snapshot scan) BEFORE walk_to_quest_ender
        // could run, so the 152y walk to Yelmak never happened. The check is
        // distance-agnostic — within ~60y the dedicated turn-in path
        // (quest_turnins + idle:quest_turnin) takes over, so the bot stays
        // covered across the whole approach and won't oscillate.
        bool has_same_map_turnin = false;
        for (auto const& q : s.raw().quest_log.quests)
        {
            if (q.state == 1 && q.ender_resolved && q.ender_map_id == s.map_id())
            {
                has_same_map_turnin = true;
                break;
            }
        }
        // In Professioning mode, suppress the long-range hub gravity. The
        // bot stays put / wanders locally so its idle:gather rule has a
        // chance to fire on visible nodes.
        // Defer to the relocation pipeline (R7) when the builder has marked this
        // bot a leveling-zone relocator: it owns the goal (cross-map, OR a
        // same-map navmesh-disconnected island bridged by an areatrigger
        // teleport / ship). The travel-plan executor below routes it; this
        // greedy same-map hub move_to would otherwise preempt that with a doomed
        // walk straight into the gap.
        // FIX 6 (group/owner exclusion): a grouped bot (owner-following or
        // all-bot group) must NOT be pulled to a hub by this rule — it would
        // scatter the group. Solo autonomous bots (ungrouped) still relocate.
        if (!_prof_mode && !has_same_map_turnin && !s.objective_is_relocation() &&
            !s.in_group() &&
            (s.raw().quest_log.quests.empty() || nothing_local))
        {
            // Pick the nearest hub the bot hasn't already tried unproductively
            // that STILL has a doable quest for this bot.
            // GetQuestHubsForBot returns top-N by suitability score (level
            // fit + distance + faction + quest count); we walk the list and
            // skip ones in the per-bot recently-tried cache OR with no doable
            // quest left. If a hub is selected and the bot is already within 30y
            // of its center but nothing_local is STILL true, mark that hub tried
            // (5 min) so the next picker pass returns the next-nearest. Without
            // this, bots in Stormwind / Orgrimmar oscillate around the city hub
            // centroid forever — the city cluster has no takeable quests
            // (Brewfest / Darkmoon / repeatable trainers all filtered) but
            // it's the closest level-appropriate hub so the picker keeps
            // returning it.
            //
            // MUST-FIX (top-N truncation): GetQuestHubsForBot ranks by SUITABILITY
            // and returns only the top N BEFORE any doable filter, so a bot whose
            // top-8 are all exhausted would find nothing here while the builder's
            // doable-aware scan picks a 9th hub — the two disagree and the bot
            // oscillates. Raise the cap to 32 so the doable filter has a deep
            // enough candidate pool to agree with the builder. (note_hub_tried is
            // kept as a secondary guard against re-walking an at-arrival hub.)
            const uint32 hub_now_ms = GameTime::GetGameTimeMS();
            auto candidates = Services::Hubs().GetQuestHubsForBot(s.raw(), 32);
            // Live Player for the shared doable predicate (world thread — same
            // context the other rules use FindConnectedPlayer in).
            Player* hub_self = ObjectAccessor::FindConnectedPlayer(s.raw().guid);

            // Iterate candidates in suitability order. For each:
            //   - skip if recently tried (5-min cooldown for hubs already
            //     visited with no eligible quests)
            //   - if within 30y of hub centroid but `nothing_local` still
            //     true → this hub has no quests for us; mark tried + try
            //     the next candidate (don't surrender the dispatch — a
            //     L1 stuck in Stormwind plaza should walk to Goldshire,
            //     not collapse to wander for 5 min)
            //   - if wedged on the long-range walk → mark THIS hub tried
            //     so next tick the picker advances; surrender for this
            //     tick (lower-priority rules may free us)
            //   - otherwise emit move toward this hub and return.
            //
            // The previous version surrendered the entire AutoactDispatch
            // on the very first wedge OR first at-arrival hub, leaving
            // wander as the only firing rule (87% of fleet decisions per
            // the 2026-05-19 routing audit). Looping the candidates
            // recovers the routing pipeline for the 92k+ "see givers but
            // can't take any" cases AND the 146k "see nothing" cases.
            ::Playerbot::V2::Travel::QuestHub const* chosen = nullptr;
            for (auto const* h : candidates)
            {
                if (!h) continue;
                if (ai.hub_recently_tried(h->hubId, hub_now_ms)) continue;
                // T-P0a: emit.move_to is map-implicit — it pathfinds on the
                // bot's CURRENT map. Feeding it a cross-map hub's coordinates
                // produces a doomed same-map pathfind toward a point that
                // doesn't exist here, which is exactly how wrong-continent
                // marooned bots (e.g. an Undead stranded in a Kalimdor capital
                // whose content is on the Eastern Kingdoms) burn forever. Skip
                // cross-map hubs here; genuine cross-map travel falls through to
                // the hearth / flight / TravelGraph rules below which know how
                // to actually change maps. (Mirrors the swim_stuck same-map guard.)
                if (h->mapId != s.map_id()) continue;

                // FIX 5a: skip any hub with no doable quest left for this bot —
                // the SAME shared predicate the builder uses, so the rule and the
                // builder agree about which hubs are exhausted (no oscillation).
                // Mark it tried so subsequent passes advance past it. (If the live
                // Player can't be resolved we conservatively keep the legacy
                // behavior rather than dropping the candidate.)
                if (hub_self && !::Playerbot::HubHasDoableQuest(hub_self, *h))
                {
                    ai.note_hub_tried(h->hubId, hub_now_ms);
                    continue;
                }

                const float dx = s.raw().position.x - h->location.GetPositionX();
                const float dy = s.raw().position.y - h->location.GetPositionY();
                const float dist_sq = dx * dx + dy * dy;

                // Already at this hub but nothing_local is still true —
                // hub is exhausted for this bot. Mark tried, advance.
                if (dist_sq <= (30.0f * 30.0f))
                {
                    ai.note_hub_tried(h->hubId, hub_now_ms);
                    continue;
                }

                // Charter grace: bot was teleported onto a capital
                // petitioner plaza for guild founding/signing. Global
                // block (same for every candidate), so surrender now.
                if (ai.in_charter_grace(hub_now_ms))
                    return false;

                // Wedge is a per-rule "pathing keeps failing" signal.
                // Mark THIS hub tried so the next tick picks a different
                // candidate; surrender for this tick so lower-priority
                // rules (vendor/gather/wander) can fire and untangle.
                if (ai.check_anchor_wedge("idle:travel_to_hub",
                                          s.path_blocked_count(), hub_now_ms))
                {
                    ai.note_hub_tried(h->hubId, hub_now_ms);
                    return false;
                }

                chosen = h;
                break;
            }
            if (chosen != nullptr)
            {
                emit.move_to(chosen->location.GetPositionX(),
                             chosen->location.GetPositionY(),
                             chosen->location.GetPositionZ(),
                             /*run*/ true);
                ai.set_last_rule_fired("idle:travel_to_hub");
                return true;
            }
            // No candidates viable — fall through to lower-priority
            // rules below (vendor/gather/wander). Hub picker will refresh
            // next tick as cooldowns expire.
        }

        // ---- Wander-to-node redirect (only for bots with a gathering skill) ----
        // The idle:gather rule (above) only fires within 5y of a node, but the
        // snapshot scan reaches 40y. If a node is in the 5–15y band, redirect
        // wander to land near it so the gather rule can pick it up next tick.
        // Without this, gather is limited to nodes that happen to coincide
        // with a random wander arrival — vanishingly rare. Same skill IDs
        // as idle:gather (Herbalism 182, Mining 186, Skinning 393, Fishing 356).
        {
            constexpr uint16 kHerbalism = 182;
            constexpr uint16 kMining    = 186;
            constexpr uint16 kSkinning  = 393;
            constexpr uint16 kFishing   = 356;
            constexpr uint8  GO_GATHERING_NODE_LOCAL = 50;
            // Skill-aware: only consider gathering for skills that AREN'T
            // capped. A capped Herbalism gives no gain; with 3 other gather
            // skills uncapped, prefer to spend time walking to those nodes.
            // If ALL gathering skills are capped, skip the wander-to-node
            // entirely so the bot doesn't burn cycles on no-gain nodes.
            const bool can_gather =
                (s.has_skill(kHerbalism) && !s.is_skill_capped(kHerbalism)) ||
                (s.has_skill(kMining)    && !s.is_skill_capped(kMining))    ||
                (s.has_skill(kSkinning)  && !s.is_skill_capped(kSkinning))  ||
                (s.has_skill(kFishing)   && !s.is_skill_capped(kFishing));
            // No node detours during a relocation / manual-travel journey
            // (same rationale as GatherGate: the herb-to-herb chain stalls
            // the trip indefinitely).
            if (can_gather && !s.raw().quest_log.objective_is_relocation)
            {
                // Mode-aware attract radius:
                //   Questing mode → 40y (opportunistic, won't detour far)
                //   Professioning mode → 200y (active seek; bot crosses
                //                              the zone to reach nodes)
                // Snapshot's nearby_objects carries gathering nodes up to
                // 200y when the bot has any gathering skill (extended scan
                // in BotSnapshotBuilder). 200y is the snapshot ceiling, so
                // 200y attract just means "every node we can see".
                const float kAttract = _prof_mode ? 200.0f : 40.0f;
                const float kAttractSq = kAttract * kAttract;
                BotSnapshot::NearbyObject const* best_node = nullptr;
                float best_node_distSq = kAttractSq + 1.f;
                // Per-node walk dedup (30 s). When the bot can't actually
                // reach a node — terrain, water, phasing, or another
                // player ninja-tagging it — wander_to_node would re-fire
                // every tick on the same node with slightly drifting
                // dest coords (so move_to dedup doesn't block, but the
                // bot never gets close enough for idle:gather). Tracking
                // per-node attempt and skipping for 30 s lets the rule
                // walk past unreachable nodes and try the next-best one,
                // or fall through to other rules.
                const uint32 wtn_now_ms = s.published_at_ms();
                for (auto const& obj : s.raw().world_objects.nearby_objects)
                {
                    if (obj.go_type != GO_GATHERING_NODE_LOCAL) continue;
                    if (ai.action_recently_tried(BotAI::ActionKind::WanderToNode,
                                                 obj.guid.GetCounter(), wtn_now_ms))
                        continue;
                    const float dx = obj.x - bx;
                    const float dy = obj.y - by;
                    const float dz = obj.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq > kAttractSq) continue;
                    if (dsq < best_node_distSq) { best_node_distSq = dsq; best_node = &obj; }
                }
                if (best_node)
                {
                    // Walk to ~3.5y from the node so the gather rule's 5y
                    // interact range comfortably fires next tick. Trinity's
                    // path system handles last-meter approach.
                    const float dist = std::sqrt(best_node_distSq);
                    const float scale = (dist > 3.5f) ? (dist - 3.5f) / dist : 0.f;
                    const float tx = bx + (best_node->x - bx) * scale;
                    const float ty = by + (best_node->y - by) * scale;
                    if (emit.move_to(tx, ty, best_node->z, /*run*/ true))
                    {
                        ai.note_action_retry(BotAI::ActionKind::WanderToNode,
                                             best_node->guid.GetCounter(), wtn_now_ms);
                        ai.set_last_rule_fired("idle:wander_to_node");
                        // Diagnostic: rate-bounded by the 30s per-node dedup
                        // above, so one line per distinct node walk. Lets the
                        // owner verify mode-aware attract radius (40y questing
                        // vs 200y professioning) is producing real detours.
                        TC_LOG_INFO("playerbot.v2",
                            "[wander_to_node] {} mode={} dist={:.1f}y entry={}",
                            s.name(),
                            _prof_mode ? "Professioning" : "Questing",
                            std::sqrt(best_node_distSq), best_node->entry);
                        return true;
                    }
                }
            }
        }

        // ---- Quest GAMEOBJECT objective execution rule ----
        // For QUEST_OBJECTIVE_GAMEOBJECT (type=2): walk nearby_objects looking
        // for a GO with matching entry, and if in 5y interact range, fire
        // UseObjectIntent. The chest/herb/quest-pickup loop will progress
        // naturally as the bot drifts past these objects.
        if (s.has_current_objective() && s.current_objective().type == /*GAMEOBJECT*/ 2)
        {
            const auto& obj = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                constexpr float kInteractSq = 25.f;        // 5²
                for (auto const& go : s.raw().world_objects.nearby_objects)
                {
                    if (uint32(obj.object_id) != go.entry) continue;
                    const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq > kInteractSq) continue;
                    // Server rejects mounted GO use (chests / clickables /
                    // quest pedestals all require dismount). Same pattern
                    // as idle:dismount_for_talk above.
                    if (s.raw().movement.is_mounted)
                    {
                        emit.dismount();
                        ai.set_last_rule_fired("idle:dismount_for_go");
                        return true;
                    }
                    emit.use_game_object(go.guid);
                    ai.set_last_rule_fired("idle:quest_use_go");
                    return true;
                }
            }
        }

        // ---- Quest COLLECT objective execution rule (GO source) ----
        // For QUEST_OBJECTIVE_ITEM (type=1) where the item drops from a
        // gameobject (e.g. a chest, herb cluster, or quest-specific lootable
        // GO with a quest item drop list). Walks nearby_objects for any GO
        // whose quest item drop list (sObjectMgr->GetGameObjectQuestItemList)
        // contains the target item id; uses it when in 5y interact range.
        // The drop-from-creature path is handled earlier in the quest_collect_kill
        // rule above; both can coexist on the same objective if a quest item
        // drops from both source types.
        if (s.has_current_objective() && s.current_objective().type == /*ITEM*/ 1)
        {
            const auto& obj = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                constexpr float kInteractSq = 25.f;        // 5²
                BotSnapshot::NearbyObject const* best_far_go = nullptr;
                float best_far_distSq = 1e9f;
                for (auto const& go : s.raw().world_objects.nearby_objects)
                {
                    std::vector<uint32> const* drops =
                        sObjectMgr->GetGameObjectQuestItemList(go.entry);
                    if (!drops) continue;
                    bool match = false;
                    for (uint32 item_id : *drops)
                        if (item_id == uint32(obj.object_id)) { match = true; break; }
                    if (!match) continue;
                    const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq <= kInteractSq)
                    {
                        // Per-GO retry cooldown (5 min) — most-frequent
                        // wedge in the watchdog log (1983 fires/session).
                        // GO use can fail silently (already looted by
                        // another player, LOS, condition mismatch); the
                        // GO stays in nearby_objects with the matching
                        // drop list, so the rule re-fires every tick.
                        const uint64 go_low = go.guid.GetCounter();
                        if (ai.action_recently_tried(BotAI::ActionKind::QuestUseGo,
                                                     go_low, now_ms))
                            continue;
                        emit.use_game_object(go.guid);
                        ai.note_action_retry(BotAI::ActionKind::QuestUseGo,
                                             go_low, now_ms);
                        ai.set_last_rule_fired("idle:quest_collect_use");
                        return true;
                    }
                    // Track the closest out-of-interact-range candidate so
                    // we can walk toward it if no in-range match exists.
                    // This is the "balloons in Razor Hill" path: snapshot's
                    // POI-aware GO scan (BotSnapshotBuilder, 160y around
                    // POI) populates nearby_objects with the lootable GOs,
                    // but they're 30-150y away. Without an explicit walk
                    // step the bot wanders. With it, the bot homes in on
                    // the nearest balloon, uses it, repeats.
                    if (dsq < best_far_distSq)
                    {
                        best_far_distSq = dsq;
                        best_far_go = &go;
                    }
                }
                if (best_far_go)
                {
                    // Walk to ~3.5y from the GO so the use-step fires next
                    // tick. Same scaling as wander_to_node. Dedup walks per
                    // GO so repeated re-paths (blocked / LOS) don't spam.
                    const uint64 go_low = best_far_go->guid.GetCounter();
                    if (!ai.action_recently_tried(BotAI::ActionKind::WanderToNode,
                                                  go_low, now_ms))
                    {
                        const float dist = std::sqrt(best_far_distSq);
                        const float scale = (dist > 3.5f) ? (dist - 3.5f) / dist : 0.f;
                        const float tx = bx + (best_far_go->x - bx) * scale;
                        const float ty = by + (best_far_go->y - by) * scale;
                        // Threat look-ahead before walking to a quest
                        // loot GO — pull blockers before they collapse
                        // onto the bot at the interact point.
                        if (NearbyUnit const* threat = s.path_threat(
                                tx, ty,
                                /*max_forward*/ std::min(dist, 35.0f),
                                /*half_width*/  10.0f))
                        {
                            if (emit.start_attack(threat->guid))
                            {
                                ai.set_last_rule_fired("idle:quest_loot_go_pull_threat");
                                return true;
                            }
                        }
                        if (emit.move_to(tx, ty, best_far_go->z, /*run*/ true))
                        {
                            ai.note_action_retry(BotAI::ActionKind::WanderToNode,
                                                 go_low, now_ms);
                            ai.set_last_rule_fired("idle:quest_loot_go");
                            TC_LOG_INFO("playerbot.v2",
                                "[quest_loot_go] {} quest={} item={} go_entry={} dist={:.1f}",
                                s.name(), s.current_quest_id(), obj.object_id,
                                best_far_go->entry, dist);
                            return true;
                        }
                    }
                }
            }
        }

        // ---- Quest TALKTO objective execution rule (2-phase) ----
        // For QUEST_OBJECTIVE_TALKTO (type=3): emit InteractWithNpc to open
        // the gossip menu. Most NPCs credit on Hello (the OnGossipHello
        // path calls TalkedToCreature server-side). Some require selection
        // of a specific menu option whose script then fires the credit.
        //
        // Phase 0 — gossip not open with this NPC: walk nearby_friends for
        //           a matching entry, emit interact_with_npc.
        // Phase 1 — gossip open with this NPC's GUID (snapshot reflects):
        //           the next snapshot still shows the objective uncredited
        //           AND the menu has options. Pick the first option whose
        //           tag is None (default talk) — that's the most common
        //           quest-credit route. Skip vendor / trainer / banker etc.
        //           tagged options. If no plain-talk option exists, close
        //           gossip (server-side: SendCloseGossip via empty select).
        // Also handles MONSTER objectives flagged talk_credit: "Speak with X"
        // quests whose target is FRIENDLY (un-attackable) and credits via a
        // gossip script — the kill rule can never act on those, so they
        // execute here exactly like a TALKTO (interact -> gossip -> script
        // fires the KillCredit spell). E.g. Forsaken starter 24960.
        if (s.has_current_objective() &&
            (s.current_objective().type == /*TALKTO*/ 3 ||
             (s.current_objective().type == /*MONSTER*/ 0 &&
              s.current_objective().talk_credit)))
        {
            const auto& obj = s.current_objective();
            const uint32 now_ms = s.published_at_ms();
            if (!ai.objective_blacklisted(s.current_quest_id(), obj.id, now_ms))
            {
                // Phase 1 detection: gossip is open with the objective's NPC.
                bool phase1_active = false;
                ObjectGuid target_npc;
                if (!s.gossip_npc().IsEmpty())
                {
                    // Match the open-gossip NPC against the objective entry.
                    // Walk nearby_friends + nearby_enemies for entry==target.
                    for (auto const& u : s.raw().combat.nearby_friends)
                        if (u.guid == s.gossip_npc() && u.entry == uint32(obj.object_id))
                        { phase1_active = true; target_npc = u.guid; break; }
                    if (!phase1_active)
                    for (auto const& u : s.raw().combat.nearby_enemies)
                        if (u.guid == s.gossip_npc() && u.entry == uint32(obj.object_id))
                        { phase1_active = true; target_npc = u.guid; break; }
                }

                if (phase1_active)
                {
                    // GossipOptionNpc::None = 0 in GossipDef.h. Plain "talk"
                    // entries route through the standard quest-credit path.
                    int picked = -1;
                    for (size_t i = 0; i < s.gossip_options().size(); ++i)
                    {
                        if (s.gossip_options()[i].option_npc == 0)
                        { picked = static_cast<int>(i); break; }
                    }
                    if (picked >= 0)
                    {
                        emit.gossip_select(target_npc,
                                           s.gossip_options()[picked].order_index);
                        ai.set_last_rule_fired("idle:quest_talk:select");
                        return true;
                    }
                    // No plain-talk option exists. Close gossip and let the
                    // bot try a different objective. Doing nothing here
                    // would loop because phase1 stays true while the menu
                    // is open. CancelInteraction cleans the InteractionData.
                    // We also reset the gossip_npc by emitting an empty
                    // select; lacking a dedicated close-intent we drop into
                    // the NPC-not-found path next tick (server clears the
                    // menu when the bot moves out of range).
                    ai.set_last_rule_fired("idle:quest_talk:no_credit_option");
                    // Fall through; quest_path/wander rule will pull the
                    // bot away and snapshot will reset gossip_npc.
                }
                else
                {
                    constexpr float kInteractSq = 25.f;
                    NearbyUnit const* best_far = nullptr;
                    float bestSq = std::numeric_limits<float>::max();
                    for (auto const& u : s.raw().combat.nearby_friends)
                    {
                        if (u.entry != uint32(obj.object_id)) continue;
                        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
                        const float dsq = dx*dx + dy*dy + dz*dz;
                        if (dsq > kInteractSq)
                        {
                            if (dsq < bestSq) { bestSq = dsq; best_far = &u; }
                            continue;
                        }
                        // Server rejects mounted NPC gossip interact.
                        // After mount_for_travel + quest_walk_to_target,
                        // bot can arrive mounted and the gossip dialog
                        // never opens. Dismount this tick; next tick's
                        // re-fire of idle:quest_talk lands the actual
                        // interact_with_npc once is_mounted has flipped.
                        if (s.raw().movement.is_mounted)
                        {
                            emit.dismount();
                            ai.set_last_rule_fired("idle:dismount_for_talk");
                            return true;
                        }
                        emit.interact_with_npc(u.guid);
                        ai.set_last_rule_fired("idle:quest_talk");
                        return true;
                    }
                    // Walk fallback: TALKTO NPC is in nearby_friends but
                    // outside the 5y interact range. Without this, the rule
                    // silently falls through when the bot has no QuestPOI
                    // (common for server-side TalkTo objectives), forcing
                    // wander recovery and starving the objective. Mirror the
                    // pattern from `idle:quest_use_item_on_friend`.
                    if (best_far)
                    {
                        const float dist = std::sqrt(bestSq);
                        if (dist > 0.1f)
                        {
                            const float scale = (dist - 3.0f) / dist;
                            emit.move_to(bx + (best_far->x - bx) * scale,
                                         by + (best_far->y - by) * scale,
                                         best_far->z, /*run*/ true);
                            ai.set_last_rule_fired("idle:quest_talk_walk");
                            return true;
                        }
                    }
                }
            }
        }

        // ---- Quest AreaTrigger rule ----
        // For QUEST_OBJECTIVE_AREATRIGGER (10) and AREA_TRIGGER_ENTER (19),
        // server credit fires the moment the bot is *inside* the trigger
        // volume — there's nothing to "do" except be there. Walk to the
        // centre when outside; once inside, the next snapshot will reflect
        // progress and the bot moves on. The arrival radius is `radius - 2`
        // so we always step a yard or two inside the boundary instead of
        // grazing the edge (where rounding can leave us juuust outside).
        // Same map gate as the other quest rules: an AT on a different
        // continent forces the travel pipeline first.
        if (s.has_current_objective() && s.current_objective_areatrigger().valid &&
            s.current_objective_areatrigger().map_id == s.map_id() &&
            (s.current_objective().type == /*AREATRIGGER*/ 10 ||
             s.current_objective().type == /*AREA_TRIGGER_ENTER*/ 19))
        {
            const auto& at = s.current_objective_areatrigger();
            const float dx = at.x - bx, dy = at.y - by;
            const float dsq = dx*dx + dy*dy;
            // Inside? — server fires credit on its own per-tick world update;
            // we just need to wait one snapshot cycle. Mark fired with an
            // ":in" suffix so /history shows the bot is on-trigger.
            const float arrived = at.radius > 2.f ? (at.radius - 2.f) : at.radius;
            if (dsq <= arrived * arrived)
            {
                ai.set_last_rule_fired("idle:quest_areatrigger:in");
                return true;
            }
            // Outside — walk to centre. Step size matches the POI rule.
            const float kStep =
                ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                35.0f;
            const float dist = std::sqrt(dsq);
            const float scale = std::min(kStep, dist) / dist;
            const float tx = bx + dx * scale;
            const float ty = by + dy * scale;
            // Threat look-ahead: same survival fix as the other walks.
            // Area-trigger objectives are often deep in hostile terrain
            // (cave entrances, enemy camps) — without this the bot
            // runs straight through and aggros 2-3 mobs.
            if (NearbyUnit const* threat = s.path_threat(
                    tx, ty,
                    /*max_forward*/ std::min(kStep, 35.0f),
                    /*half_width*/  10.0f))
            {
                if (emit.start_attack(threat->guid))
                {
                    ai.set_last_rule_fired("idle:quest_at_pull_threat");
                    return true;
                }
            }
            // Use bot Z so move_to snaps to navmesh ground; AT Z is often
            // the trigger volume centre and may be off the floor.
            emit.move_to(tx, ty, bz, /*run*/ true);
            ai.set_last_rule_fired("idle:quest_areatrigger");
            return true;
        }

        // ---- Riding a transport (ship / zeppelin) ----
        // We boarded server-side (idle:board_transport → AddPassenger). While
        // aboard the bot MUST NOT run its objective-walk rules: a passenger's
        // move_to is relative to the moving deck and would walk it off into the
        // sea. Hold position until the transport is STOPPED at a dock AND we've
        // arrived on the objective's map; then step off (toggle use_game_object
        // on the ridden transport → RemovePassenger) so the same-map rules can
        // finish the trip. The ridden transport sits at dist ~0 in
        // nearby_objects (we move with it), so we can recover its guid there.
        if (s.on_transport() && !s.in_combat() && !s.is_casting())
        {
            // This inline guard's map-based disembark is for type-15 cross-map
            // ships/zeppelins ONLY. A type-11 elevator ride is ALWAYS same-map, so
            // arrived_on_goal_map is trivially true and disembarking here would eject
            // the bot the instant it parks at the boarding floor — before the lift
            // ever carries it up — fighting idle:elevator_step_on into a tight
            // board/disembark loop (observed: one bot boarded transport 20655 2,066x
            // in a single run, never progressing). Type-11 elevators are owned end-
            // to-end by idle:on_transport_wait (freeze during ascent) + idle:
            // elevator_step_off (Z-delta-gated exit at the destination floor).
            //
            // CRITICAL: gate on the bot's ACTUAL ridden-transport type (snapshot
            // transport_is_ship), NOT "is there a type-15 in nearby_objects". A bot
            // riding an Orgrimmar tower ELEVATOR sits next to docked zeppelins, so a
            // nearby-scan found a ship guid and RemovePassenger'd a transport the bot
            // wasn't even on — 733 spurious DISEMBARKs in one run (Gorois, lift
            // 206610 beside zeppelin 175080) while the bot stayed stuck on the lift.
            // When riding an elevator we fall straight through to the quiet-ride
            // freeze below and let the elevator rules handle the exit.
            ObjectGuid ship_guid;
            if (s.transport_is_ship())
                for (auto const& go : s.raw().world_objects.nearby_objects)
                    if (go.go_type == /*MAP_OBJ_TRANSPORT*/ 15)
                    { ship_guid = go.guid; break; }
            const bool arrived_on_goal_map =
                s.has_current_objective() && s.current_objective_poi().valid &&
                s.current_objective_poi().map_id == s.map_id();
            if (!ship_guid.IsEmpty() && s.transport_stopped() && arrived_on_goal_map)
            {
                TC_LOG_INFO("playerbot.v2",
                    "[xport_board] {} arrived map={} -> disembarking",
                    s.name(), s.map_id());
                emit.use_game_object(ship_guid);   // toggle → RemovePassenger
                ai.set_last_rule_fired("idle:disembark_transport");
                return true;
            }
            // Mid-voyage ship (or stopped at the SOURCE dock before departure, or the
            // ridden GO momentarily out of the snapshot), OR riding a type-11 elevator
            // (exit owned by idle:elevator_step_off) — ride quietly. The bot MUST NOT
            // run objective-walk rules aboard a moving deck/platform.
            ai.set_last_rule_fired("idle:riding_transport");
            return true;
        }

        // ---- UnifiedTravelGraph: A*-routed cross-map travel ----
        // Before the legacy portal/dock cascade fires, ask the unified
        // graph for a full route to the current objective POI. The
        // graph composes walk + flight + portal + ship hops across the
        // entire world (vs. the legacy cascade which is greedy and
        // can't see beyond one anchor hop). When the planner returns
        // a route, we execute the bot's CURRENT leg (move toward the
        // leg's anchor, or "use" it when adjacent). When the planner
        // returns no route, we fall through to the legacy cascade —
        // which still handles same-map walks fine and is the right
        // fallback for goal positions the graph hasn't anchored.
        if (!s.in_combat() && !s.is_casting() && !s.raw().movement.is_mounted &&
            s.has_current_objective() && s.current_objective_poi().valid)
        {
            const auto& poi = s.current_objective_poi();
            // map 0 (Eastern Kingdoms) is a valid goal; .valid is the unset
            // sentinel (gated by has_current_objective + .valid above). The
            // old `poi.map_id != 0` excluded all EK destinations.
            // R7 island-escape: also run for a SAME-MAP relocation goal — the
            // builder only sets objective_is_relocation on a same-map hub when
            // the cached route needs a bridge leg (areatrigger teleport / ship),
            // so this executes that bridge instead of a doomed walk into the gap.
            // objective_needs_bridge generalises that to a STUCK same-map QUEST
            // objective the navmesh can't reach but the graph can via a non-walk
            // edge (the Org tower deck->ground via the lift): the builder sets it
            // only after the bot has repeatedly path-failed toward the POI.
            if (poi.map_id != s.map_id() || s.objective_is_relocation() ||
                s.objective_needs_bridge())
            {
                // Execute the cached graph route to the quest goal via the shared
                // travel-plan executor (defined near the top of this dispatch).
                if (driveTravelPlanTo(poi.map_id, poi.x, poi.y, poi.z))
                    return true;
            }
        }

        // ---- Cross-continent portal (Phase C) ----
        // When the bot has a goal on a different map, scan nearby_objects
        // for a portal (GAMEOBJECT_TYPE_SPELLCASTER, type 22) whose
        // pre-resolved teleport destination matches the goal's map. The
        // builder pre-resolves portal destinations (spell ->
        // spell_target_position) so this rule is a cheap snapshot scan
        // with no DB lookups on the AI worker. Two sub-rules:
        //   * idle:walk_to_portal : portal > 5y → walk to it
        //   * idle:use_portal     : portal ≤ 5y → emit use_game_object
        // The portal GO is invisible to other-faction players (it's a
        // physical object owned by the city it's in), and the snapshot's
        // nearby_objects already filters by visibility, so we never
        // try to use a hostile-city portal.
        //
        // Server-side: GO of type 22 fires its `spellCaster.spell` on
        // anyone within `InteractRadiusOverride` (default 5 y) who passes
        // the conditionID1 gate. Using the GO triggers the spell, which
        // teleports the user. The bot just needs to be in interact range
        // and emit use_game_object — no spell cast on our side.
        if (!s.in_combat() && !s.is_casting() && !s.raw().movement.is_mounted &&
            s.has_current_objective() && s.current_objective_poi().valid)
        {
            const auto& poi = s.current_objective_poi();
            // map 0 (Eastern Kingdoms) is a valid goal; .valid is the unset
            // sentinel. The old `poi.map_id != 0` blocked the entire
            // walk_to_known_portal / walk_to_known_dock cascade for any
            // Kalimdor→EK trip (e.g. Org→Undercity zeppelin).
            if (poi.map_id != s.map_id())
            {
                BotSnapshot::NearbyObject const* best_portal = nullptr;
                float best_distSq = std::numeric_limits<float>::max();
                // Match the ship/portal destination to the NEXT HOP of the route,
                // not the FINAL objective map. The walk-to-dock cascade below uses
                // next_hop_dest_map() — a multi-hop route (e.g. Stormwind ->
                // Teldrassil -> Bloodmyst, poi.map_id=530) walks the bot to the dock
                // for its FIRST leg (map 1). But this in-range board check used
                // poi.map_id, so the docked ship (dest = next hop = map 1, != 530)
                // never matched and the bot stood at the dock without boarding —
                // walk_to_transport stayed 0 fleet-wide despite 500+ dock-walks.
                // Use the same expected-dest as the walk so the bot boards the leg
                // it actually walked to.
                const uint32 _board_dest =
                    (s.next_hop_dest_map() != kInvalidMapId) ? s.next_hop_dest_map() : poi.map_id;
                // SPECIFIC-zeppelin gate. The Org tower docks TWO zeppelins ~20y
                // apart that BOTH go to map 0 — Undercity (164871 -> Tirisfal) and
                // Grom'gol (175080 -> Stranglethorn). Matching by dest_map alone
                // (both ==0) would board whichever is docked, sending a Tirisfal-
                // bound bot to Stranglethorn. When the bot's anchor resolved to a
                // SPECIFIC transport, board ONLY that exact entry; a wrong-entry
                // zeppelin docked alongside is ignored (the bot waits for its own).
                const uint32 _want_entry =
                    (s.nearest_portal_anchor_kind() == /*Transport*/ 2)
                        ? s.nearest_portal_anchor_entry() : 0;
                int _xport_seen = 0;   // DIAG: # transports physically in scan range
                uint32 _miss_entry = 0, _miss_dest = kInvalidMapId;  // DIAG: the wrong transport Somi keeps seeing
                for (auto const& go : s.raw().world_objects.nearby_objects)
                {
                    // Accept BOTH static portals (SPELLCASTER 22) AND moving
                    // continental transports (TRANSPORT 11 = zeppelins; certain
                    // elevators are also 11 but carry no cross-map dest and are
                    // dropped by the teleport_dest_map==0 gate below; and
                    // MAP_OBJ_TRANSPORT 15 = boats). The old `!= 22` filter
                    // excluded every boat/zeppelin, so the is_transport boarding
                    // branch below was DEAD CODE — 0 bots ever boarded a ship
                    // fleet-wide (idle:walk_to_transport / wait_on_transport never
                    // fired, despite walk_to_known_dock firing 98K times),
                    // stranding ~42% of leveling bots on the wrong continent. The
                    // snapshot already populates teleport_dest_map for transports
                    // (BotSnapshotBuilder ~5057, their first non-current MapId), so
                    // the dest match works identically for portals and ships. A
                    // transport only appears in this 30y nearby_objects scan when
                    // it is physically near the bot (i.e. docked), so walking onto
                    // its current position is correct — when it's mid-route it
                    // simply isn't in the list and the bot waits at the dock anchor.
                    // type-11 TRANSPORT (elevators) are EXCLUDED here — they are
                    // same-map lifts handled by idle:elevator_step_on/off, not
                    // cross-map vehicles. Only static portals (22) and type-15
                    // ships/zeppelins (15) are cross-map board targets. (Even so
                    // the kInvalidMapId gate below would drop them now that the
                    // builder never stamps a dest on a type-11 — this is the
                    // explicit belt-and-suspenders.)
                    if (go.go_type != /*SPELLCASTER*/ 22 &&
                        go.go_type != /*MAP_OBJ_TRANSPORT*/ 15) continue;
                    if (go.teleport_dest_map == kInvalidMapId) continue;
                    if (go.go_type != /*SPELLCASTER*/ 22) ++_xport_seen;  // a real ship/zeppelin physically in range
                    // Specific-zeppelin gate (shared Org tower): only the bot's own
                    // anchored zeppelin, never the sibling that shares the dest map.
                    if (go.go_type == /*MAP_OBJ_TRANSPORT*/ 15 && _want_entry != 0 &&
                        go.entry != _want_entry)
                        continue;
                    if (go.teleport_dest_map != _board_dest)
                    {
                        if (go.go_type != /*SPELLCASTER*/ 22)
                        { _miss_entry = go.entry; _miss_dest = go.teleport_dest_map; }  // DIAG: capture the mismatched ship
                        continue;
                    }
                    const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    if (dsq < best_distSq) { best_distSq = dsq; best_portal = &go; }
                }
                // DIAG: a transport was physically in scan range but nothing was
                // picked — the dest didn't match. Separates "ship never showed up"
                // (no line) from "ship there, wrong dest / multi-hop mismatch".
                // Only surface this when the bot is genuinely STUCK (not making
                // movement progress). A bot in TRANSIT — walking to its lift/dock
                // while a wrong-dest ship merely passes within 30y — is not
                // "missing a board"; it's en route, and logging/escalating every
                // tick is pure noise (2660 spam lines from Somi mid-walk to the Org
                // lift; the user rightly asked "why is it checking transport when
                // it's not at a boarding point?"). The boarding itself (best_portal
                // above) still runs every tick regardless. stuck_tracker.active
                // only latches once the bot has stopped progressing, so a genuinely
                // parked-at-the-dock bot still logs + gets the Tier-2 escalation.
                if (_xport_seen > 0 && !best_portal && ai.stuck_tracker().active)
                {
                    TC_LOG_INFO("playerbot.v2",
                        "[xport_miss] {} poi_map={} next_hop={} board_dest={} want_entry={} xports_in_range={} "
                        "miss_entry={} miss_dest={} | anchor: has={} entry={} dest={} z={:.1f} pos=({:.0f},{:.0f},{:.0f})",
                        s.name(), poi.map_id, s.next_hop_dest_map(), _board_dest, _want_entry, _xport_seen,
                        _miss_entry, _miss_dest,
                        s.has_nearest_portal_anchor() ? 1 : 0,
                        s.nearest_portal_anchor_entry(),
                        s.has_nearest_portal_anchor() ? s.nearest_portal_anchor_dest_map() : kInvalidMapId,
                        s.has_nearest_portal_anchor() ? s.nearest_portal_anchor_z() : 0.0f,
                        bx, by, bz);
                    // Nudge the unstick ladder to Tier-2 so it walk-escapes the
                    // dock instead of waiting indefinitely for a never-matching ship.
                    if (ai.stuck_tracker().no_progress_ticks < 50)
                        ai.mutable_stuck_tracker().no_progress_ticks = 50;
                }
                if (best_portal)
                {
                    const bool is_static_portal =
                        best_portal->go_type == /*SPELLCASTER*/ 22;
                    const bool is_transport =
                        best_portal->go_type == /*MAP_OBJ_TRANSPORT*/ 15;
                    // Static portals: use within 5y interact range —
                    // server fires the triggered teleport spell.
                    // Transports (ships/zeppelins): the bot waits on the PIER,
                    // but the ship's pivot/origin docks OFFSHORE (the STOP
                    // waypoint, ~15-20y out). So use a generous board-emit range
                    // so a pier-waiting bot still triggers the board when the
                    // ship is docked; the API then gates on the ship being
                    // stationary and walks the bot the rest of the way onto the
                    // deck. (Too-tight 10y left pier bots forever just short of
                    // the offshore origin → 0 boards.)
                    // Static portal: 10y (was 5y). A tower-deck portal (Org→Undercity
                    // replacement) is reached by climbing, so the bot settles a few
                    // yards out at the deck height; 10y lets it trigger the portal
                    // there. A SPELLCASTER portal also auto-fires on proximity server-
                    // side, so a slightly looser use-emit is harmless for ground
                    // portals (the bot is standing on them anyway).
                    const float kArrive = is_static_portal ? 10.0f : 30.0f;
                    if (best_distSq <= kArrive * kArrive)
                    {
                        if (is_static_portal)
                        {
                            emit.use_game_object(best_portal->guid);
                            ai.set_last_rule_fired("idle:use_portal");
                            return true;
                        }
                        if (is_transport)
                        {
                            // BOARD server-side. A bot has no game client to send
                            // the transport-movement packet that auto-boards real
                            // players, so standing on the deck does nothing
                            // (observed: dist 0.0, on_transport=0 forever). Emit
                            // use_game_object on the transport guid — the API
                            // detects a transport target and calls AddPassenger.
                            // Once aboard, the early ride/disembark guard (above
                            // the travel blocks) takes over: it keeps the bot put
                            // while the ship moves and steps it off at the
                            // destination dock. Re-emitting while already aboard
                            // is a harmless no-op (API returns Ok immediately).
                            if (!s.on_transport())
                            {
                                TC_LOG_INFO("playerbot.v2",
                                    "[xport_board] {} reached transport entry={} go_type={} "
                                    "dest_map={} dist={:.1f} -> boarding",
                                    s.name(), best_portal->entry, uint32(best_portal->go_type),
                                    best_portal->teleport_dest_map, std::sqrt(best_distSq));
                                emit.use_game_object(best_portal->guid);
                                ai.set_last_rule_fired("idle:board_transport");
                                return true;
                            }
                            ai.set_last_rule_fired("idle:wait_on_transport");
                            return true;
                        }
                    }
                    // A TRANSPORT beyond board range must NOT be chased: a ship/
                    // zeppelin moves faster than the bot, so walking toward its
                    // LIVE position just trails it (observed: Somi dist 3.6 -> 9.6
                    // as the zeppelin departed) and the bot is never standing there
                    // while it's docked+stationary, so it never boards. Instead
                    // fall through to the dock-anchor wait below: walk to the FIXED
                    // dock STOP point (where the transport halts) and hold there;
                    // when the transport docks AT that point the in-range board
                    // above re-engages with the bot already in range. Only STATIC
                    // portals (stationary) are walked toward here.
                    if (!is_transport)
                    {
                        // Walk toward the portal. Same step-and-stop as the
                        // walk_to_taxi / quest_path rules.
                        const float kStep =
                            ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                            ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                            35.0f;
                        const float dx = best_portal->x - bx, dy = best_portal->y - by;
                        const float dist = std::sqrt(best_distSq);
                        const float scale = std::min(kStep, dist) / dist;
                        const float tx = bx + dx * scale;
                        const float ty = by + dy * scale;
                        emit.move_to(tx, ty, best_portal->z, /*run*/ true);
                        ai.set_last_rule_fired("idle:walk_to_portal");
                        return true;
                    }
                    // is_transport beyond board range: fall through to dock-anchor wait.
                }
                // No portal/transport visible nearby. Second option:
                // walk toward a KNOWN cross-map anchor (static portal or
                // dock spawn point) for this source→dest map pair. The
                // PortalIndex pre-resolved every portal/transport in
                // the world at module init, so the bot doesn't need to
                // see the GO to know where it is. Once the bot gets
                // within 30 y, the SPELLCASTER/TRANSPORT GO appears in
                // nearby_objects and the in-range rules above take over.
                // Match anchor against EITHER the goal map (direct route)
                // or the planner's next-hop intermediate (multi-hop route).
                // The snapshot builder stores whichever resolves.
                const uint32 _expected_anchor_dest =
                    (s.next_hop_dest_map() != kInvalidMapId)
                        ? s.next_hop_dest_map()
                        : poi.map_id;
                if (s.has_nearest_portal_anchor() &&
                    s.nearest_portal_anchor_dest_map() == _expected_anchor_dest)
                {
                    // ---- L5: portal-room pocket gateway ----
                    // A static portal anchor may sit in a navmesh-DISCONNECTED
                    // "portal room" (e.g. the Stormwind Mage Tower portal room):
                    // the GO dais (z~68) has no walkable path from the city, and
                    // players enter by stepping onto a teleporter areatrigger up
                    // in the Mage Tower (z~148). PortalPocketIndex detected that
                    // structure at boot. If the chosen portal lies in such a
                    // pocket and the bot is NOT yet inside it, walk the bot ONTO
                    // the entrance areatrigger instead of at the unreachable dais:
                    // the server-side AT casts its teleport on the bot (a bot is a
                    // real Unit, so the enter-handler fires for it too) and drops
                    // it in the room. Next tick the bot is inside the pocket, this
                    // redirect no longer applies, and the normal walk-to-portal /
                    // use_portal below reaches the GO (now a short internal walk).
                    //
                    // Walk to the gateway's REAL Z (the tower floor is on the
                    // navmesh, unlike the off-mesh portal dais) and do not step —
                    // the navmesh path climbs the tower ramps in one move_to.
                    if (s.nearest_portal_anchor_kind() == /*Portal*/ 1)
                    {
                        using ::Playerbot::V2::Travel::PortalPocketIndex;
                        auto const& ppi = PortalPocketIndex::Instance();
                        if (auto const* pocket = ppi.PocketContaining(
                                s.map_id(),
                                s.nearest_portal_anchor_x(),
                                s.nearest_portal_anchor_y(),
                                s.nearest_portal_anchor_z()))
                        {
                            if (!ppi.PocketContaining(s.map_id(), bx, by, bz))
                            {
                                // Bot outside the room: head for the entrance
                                // teleporter. Wedge-guard so we yield instead of
                                // hammering move_to if the tower itself is blocked.
                                if (ai.check_anchor_wedge(
                                        "idle:walk_to_portal_gateway",
                                        s.path_blocked_count(), s.published_at_ms()))
                                    return false;
                                emit.move_to(pocket->gw_x, pocket->gw_y, pocket->gw_z,
                                             /*run*/ true);
                                ai.set_last_rule_fired("idle:walk_to_portal_gateway");
                                return true;
                            }
                            // else: bot is inside the room — fall through to the
                            // normal walk-to-portal/use_portal path below.
                        }
                    }

                    float ax = s.nearest_portal_anchor_x();
                    float ay = s.nearest_portal_anchor_y();
                    // Anchor Z (s.nearest_portal_anchor_z()) is intentionally
                    // unused: portal/dock spawns sit on raised dais geometry
                    // that is often off-mesh; we feed bot Z (bz) into move_to
                    // so the navmesh path snaps to walkable ground instead.
                    //
                    // ---- FIX B: elevator-base hand-off ----
                    // Some docks (e.g. the Orgrimmar zeppelin towers) anchor at
                    // the PLATFORM deck (anchor_z ~79.8), which is off-mesh from
                    // the ground floor where the bot stands (bz ~17-44). Pathing
                    // straight to the deck coords NoPaths/FarFromPolyEnd and the
                    // bot gets teleport-rescued. When the anchor sits materially
                    // ABOVE the bot AND the auto-detected ElevatorIndex knows of
                    // an elevator stop near the anchor at a floor the bot can
                    // actually reach (its own Z band), retarget the walk to that
                    // LOWER stop. Once the bot arrives, the higher-priority
                    // idle:elevator_step_on rule (priority 710, run at the top of
                    // DispatchIdle before this legacy dispatch is reached) boards
                    // the lift; on_transport_wait rides it; elevator_step_off
                    // releases the bot onto the platform near the dock; then the
                    // existing idle:walk_to_transport / wait_on_transport boards
                    // the zeppelin. We only need to hand the bot to the lift base.
                    //
                    // Loop safety: this redirect just changes the walk target. As
                    // soon as the bot is within boarding range of the lower stop,
                    // idle:elevator_step_on fires at top-of-tick and returns, so
                    // this dock-walk rule never runs that tick — no oscillation.
                    bool retargeted_to_lift = false;
                    float lift_floor_z = bz;   // Z to walk at once retargeted to a lift base
                    {
                        const float anchor_z = s.nearest_portal_anchor_z();
                        constexpr float kAnchorAboveBot = 15.0f;   // deck vs ground
                        if (anchor_z - bz >= kAnchorAboveBot)
                        {
                            using ::Playerbot::V2::Travel::ElevatorIndex;
                            // Route to the BASE of the nearest elevator shaft so
                            // the bot can ride up to the deck. LowestStopNear
                            // resolves the shaft by X/Y proximity to the dock
                            // anchor and returns its lowest (boarding) floor —
                            // unlike the old NearestStopOnFloor, it does NOT
                            // require the stop's Z to sit within 15y of the bot's
                            // ground Z (the boarding floor can be above the bot's
                            // current footing). 90y planar tolerance: at the
                            // Orgrimmar zeppelin towers the deck-level dock anchor
                            // is ~50-60y from the lift shaft, so the previous 40y
                            // radius found nothing and the bot pathed straight at
                            // the off-mesh deck (NoPath/FarFromPolyEnd, 0 boardings
                            // fleet-wide). Retarget Z to the boarding floor too so
                            // move_to walks to the lift base, not the deck.
                            // CRITICAL guard (2026-06-09): only divert to a lift
                            // that makes vertical PROGRESS toward the dock floor.
                            // The Orgrimmar zeppelin route (owner-confirmed) IS:
                            // ground (z42) -> ride lift 206609/206610 UP to the RIM
                            // (z109, where the zeppelin tower stands) -> SHORT WALK
                            // to the tower -> up to the deck (z135) -> board. So the
                            // lift IS needed for the FIRST leg (ground->rim) but must
                            // NOT be used once the bot is already on the rim — there
                            // the lift only goes back DOWN, away from the deck, so
                            // the bot must walk to the tower instead. The lifts top
                            // at ~z109 (DB: 206610 @z42 / 206609 @z44, anim top z109)
                            // and the deck anchor is z135, so a "lift reaches the
                            // anchor floor" test would wrongly REFUSE the ground->rim
                            // ride. The right test is strict progress: the shaft has
                            // a stop materially closer to the anchor than the bot's
                            // current Z. At ground that's the rim (ride up); at the
                            // rim the closest stop IS the bot's Z (no progress -> walk
                            // to the tower). This also broke the old rim<->lift loop.
                            float reach_z = 0.f;
                            const bool lift_makes_progress =
                                ElevatorIndex::Instance().BestStopZToward(
                                    s.map_id(), ax, ay, anchor_z,
                                    /*xy_range*/ 90.0f, reach_z) &&
                                (std::fabs(bz - anchor_z) - std::fabs(reach_z - anchor_z) > 5.0f);
                            if (lift_makes_progress)
                            {
                                // Prefer a lift whose boarding floor is reachable
                                // (has a derived ledge) over merely the nearest one:
                                // a tower served by two lifts (the Org zeppelin
                                // towers) must route the bot to the one it can board,
                                // not the nearest that drops it in an unboardable pit
                                // (206609 @z29.5). Falls back to nearest when neither
                                // is known-boardable yet.
                                if (auto const* lower = ElevatorIndex::Instance().LowestStopNearBoardable(
                                        s.map_id(), ax, ay, /*xy_range*/ 90.0f))
                                {
                                    // Boarding floor = the navmesh-derived LEDGE beside the shaft
                                    // (the walkable z44 ground), not the stop CENTRE (which for a
                                    // pit lift can sit in disconnected sub-ground).
                                    float lx = 0.f, ly = 0.f, lz = 0.f;
                                    const bool have_ledge =
                                        ElevatorIndex::Instance().LedgeFor(lower->spawn_id, lower->stop_idx, lx, ly, lz);
                                    const float floor_z = have_ledge ? lz : lower->z;
                                    // Only divert DOWN to a lift base when the bot is AT/NEAR that
                                    // boarding floor (the normal ground->rim ride). If the bot is
                                    // already well ABOVE it — Somi wedged at z89 vs the z44 lift
                                    // base, sitting on an upper ledge by the tower — going back
                                    // down to the lift is pointless (the lift only tops out at the
                                    // rim z106, barely above z89) AND usually unreachable: the path
                                    // to the base snaps into the disconnected sub-ground pit
                                    // (verified live: 350+ frozen ticks, dst z24.7, Incomplete).
                                    // Leave retargeted_to_lift false so climb_to_deck walks the bot
                                    // UP toward the deck/portal from where it already stands.
                                    if (bz <= floor_z + 15.0f)
                                    {
                                        ax = have_ledge ? lx : lower->x;
                                        ay = have_ledge ? ly : lower->y;
                                        lift_floor_z = floor_z;
                                        retargeted_to_lift = true;
                                    }
                                }
                            }
                        }
                    }
                    const float dx = ax - bx, dy = ay - by;
                    const float dsq = dx * dx + dy * dy;
                    // A TRANSPORT dock anchor is the ship's OFFSHORE float point
                    // (the path STOP waypoint, sitting in the water) which a bot
                    // can NEVER reach on foot — it stops at the adjacent pier/coast,
                    // tens of yards short. So "arrived at a transport dock" uses a
                    // GENEROUS radius (the pier vicinity); the docked ship's 200y
                    // board-scan still catches the bot there and the in-range board
                    // walks it onto the deck. This is why wait_for_transport stayed
                    // 0 at 25y despite ~1000 dock walks. Portals keep the tight 25y
                    // (the GO is a precise interactable the bot stands on).
                    //
                    // BUT when FIX-B retargeted the walk to an ELEVATOR LIFT BASE
                    // (the dock is up a zeppelin tower the bot must ride a lift to
                    // reach), the target is a PRECISE stand-on spot, NOT an offshore
                    // pier. The 100y radius made the bot "arrive" up to ~90y short of
                    // the lift and switch to wait_for_transport — but the zeppelin
                    // docks at the z135 deck overhead and never comes to the waiting
                    // bot, AND idle:elevator_step_on only engages within 8y of the
                    // shaft, so the bot waited forever 86y from the lift it needed
                    // (verified: Somi at the Org→Undercity tower, lift 206609 86y
                    // away, 0 boardings). Use a TIGHT radius so the bot walks all the
                    // way onto the lift base; elevator_step_on then fires and rides
                    // it up to the deck where the zeppelin board-scan takes over.
                    const bool is_transport_dock =
                        s.nearest_portal_anchor_kind() == /*Transport*/ 2;
                    // A PORTAL can also sit on a high tower deck. Post-Shadowlands
                    // the Org→Undercity zeppelin was replaced by a PORTAL at the
                    // tower top (z135) — same geometry as the old zeppelin deck, so
                    // the bot must ride the lift then CLIMB to it exactly as it did
                    // for the deck. Treat an elevated portal like an elevated deck.
                    const bool is_portal_anchor =
                        s.nearest_portal_anchor_kind() == /*Portal*/ 1;
                    const bool is_elevated_target = is_transport_dock || is_portal_anchor;
                    // ELEVATED DECK climb: a zeppelin docks on a TOWER deck (Org→UC
                    // The Thundercaller's curated deck is z135) reached by riding the
                    // lift to the tower base (z106) then CLIMBING the tower. The bot
                    // must walk UP to the deck before holding for the zeppelin —
                    // otherwise the generous 100y dock radius makes it "arrive" at
                    // z106, 29y BELOW the deck, and wait forever for a zeppelin it's
                    // too low to board (verified: Somi idle on the rim at z106). When
                    // the dock anchor sits materially ABOVE the bot, walk AT the deck
                    // Z (so the navmesh climbs the tower ramp) and tighten the arrival
                    // radius so the bot reaches the deck rather than stopping short.
                    const float dock_anchor_z = s.nearest_portal_anchor_z();
                    const bool climb_to_deck = is_elevated_target && !retargeted_to_lift &&
                                               (dock_anchor_z - bz >= 15.0f);
                    // An ELEVATED transport deck (zeppelin tower deck z135, vs a
                    // sea-level ship pier z~5) is a PRECISE walkable target shared
                    // with a sibling deck only ~20-37y away (Org tower: Undercity +
                    // Grom'gol). The 100y ship-pier radius spans both, so a bot
                    // standing on the WRONG deck "arrives" and waits there forever
                    // (Somi parked on the Grom'gol deck 37y from its Undercity
                    // anchor). Give elevated decks a TIGHT radius so the bot walks
                    // to ITS specific deck. Low (sea-level) anchors keep the 100y:
                    // their STOP is an offshore float the bot can't reach, so it
                    // waits on the pier vicinity (the wedge fallback below holds it).
                    const bool elevated_deck = is_elevated_target && dock_anchor_z > 50.0f;
                    const float kArrivedSq = retargeted_to_lift
                        ? (6.0f * 6.0f)
                        : (climb_to_deck || elevated_deck)
                            // A PORTAL must be reached within USE range (the bot
                            // teleports by using the GO), so walk tight to it; a
                            // transport DECK only needs board-scan range (~30y), so
                            // 15y is plenty and avoids over-walking onto the platform.
                            ? (is_portal_anchor ? (8.0f * 8.0f) : (15.0f * 15.0f))
                            : (is_transport_dock ? (100.0f * 100.0f) : (25.0f * 25.0f));
                    // For a deck climb the "arrived" test must be 3D: the planar dsq
                    // alone let the bot stop at the deck's X/Y while still 30y BELOW
                    // it (Somi parked at z104 under the z135 deck and never boarded).
                    // Keep walking (at the deck Z, so it climbs the tower ramp) until
                    // it is actually NEAR the deck height. If the ramp has no navmesh
                    // the move_to will NoPath here, surfacing the gap instead of a
                    // silent low-altitude wait.
                    const bool below_deck =
                        climb_to_deck && std::fabs(dock_anchor_z - bz) > 8.0f;
                    if (dsq > kArrivedSq || below_deck)
                    {
                        // Wedge gate: if this rule has been firing repeatedly
                        // and the bot keeps getting path-block failures, the
                        // anchor is unreachable (off-mesh raised dais, behind
                        // closed dungeon gate, on a wrong-faction map). Fall
                        // through so other rules get a turn instead of
                        // emitting move_to 60 times in 30s and tripping
                        // the watchdog (audit 2026-05-17: this rule was 55%
                        // of all watchdog hits).
                        char const* const rule_tag =
                            s.nearest_portal_anchor_kind() == /*Transport*/ 2
                                ? "idle:walk_to_known_dock"
                                : "idle:walk_to_known_portal";
                        // Single-owner routing (see walk_to_flightmaster).
                        if (ai.current_travel_leg())
                            return false;
                        if (ai.check_anchor_wedge(rule_tag, s.path_blocked_count(),
                                                  s.published_at_ms()))
                        {
                            // L4: a TRANSPORT dock anchor is the ship's OFFSHORE
                            // float point (the stop waypoint sits in the water),
                            // so the bot can only reach the adjacent PIER — tens
                            // of yards short — and wedges here instead of arriving
                            // within 25y. That's why wait_for_transport never
                            // fired (0) despite thousands of dock walks. If we're
                            // within the ship's board-scan range, treat the pier
                            // as "at the dock": HOLD and wait for the ship rather
                            // than giving up and wandering. When the ship docks at
                            // the float point (≤150y away) it enters the bot's
                            // nearby scan and the in-range board (above) walks the
                            // bot onto the deck. The ~5-min objective stuck-detector
                            // still bounds a genuinely-wrong dock.
                            if (s.nearest_portal_anchor_kind() == /*Transport*/ 2 &&
                                dsq < 150.0f * 150.0f)
                            {
                                ai.set_last_rule_fired("idle:wait_for_transport");
                                return true;
                            }
                            return false;
                        }
                        const float kStep =
                            ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                            ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                            35.0f;
                        const float dist = std::sqrt(dsq);
                        // climb_to_deck aims at the FULL anchor (the real deck/portal
                        // poly), NOT a 35y stepped intermediate. Stepping toward a z135
                        // deck lands the waypoint over open ground that has no deck-height
                        // navmesh, so Detour snaps it DOWN into the sub-ground pit and the
                        // path dead-ends (verified: Somi step (1907,-4409,z135) snapped to
                        // z24.8, Incomplete, forward 1y). Pointing at the real deck coords
                        // makes Detour findNearestPoly hit the deck itself and route the
                        // actual tower ramp up to it (no pit snap).
                        // Blocked-bearing deflection: a straight-line step
                        // is deterministic, so a step target on unmeshed
                        // micro-terrain (tower base, boulder, riverbed)
                        // NoPaths IDENTICALLY forever (observed: Somi wedged
                        // at (637,-4595) stepping at the Razor Hill water
                        // tower every tick of her Brill journey). Rotate the
                        // bearing ±40 deg while path-blocked; the counter
                        // resets on the first successful move.
                        float step_dx = dx, step_dy = dy;
                        float step_len = kStep;
                        bool aim_full_anchor = climb_to_deck;
                        if (!climb_to_deck && s.path_blocked_count() > 0)
                        {
                            // Escalation: deflect ±40° first; if the local
                            // terrain blocks EVERY bearing (bot on a bluff
                            // over a river — all 35y targets land in the
                            // gorge), LONG-STEP 220y toward the anchor.
                            // 220y stays inside the bot's loaded grid
                            // neighborhood (so the dest poly exists — the
                            // FULL anchor 1,000y+ away NoPaths on unloaded
                            // tiles) while overshooting local obstructions
                            // to the far bank/road, where the complete A*
                            // (+ road-preference cost) routes the actual
                            // bridge. Budget-truncated partial paths still
                            // advance the bot.
                            const uint32 pb = s.path_blocked_count();
                            float defl = 0.f;
                            if (pb % 3 == 0)
                            {
                                step_len = 220.0f;
                                // Long-steps rotate bearing across CYCLES
                                // too (straight, +35°, -35°): a long
                                // straight shot can point into a mountain
                                // wall just as a short one can.
                                // Bearing fan out to ±70°: the walkable
                                // corridor (road/bridge) can sit far off
                                // the direct bearing — Somi's bridge was
                                // +70° off the straight-to-Org line, with
                                // unmeshed bluffs everywhere nearer.
                                static constexpr float kFan[10] =
                                    { 0.f, 0.6f, -0.6f, 1.2f, -1.2f,
                                      1.8f, -1.8f, 2.4f, -2.4f, 3.1f };
                                defl = kFan[(pb / 3) % 10];
                            }
                            else
                                defl = (pb % 3 == 1 ? 0.7f : -0.7f);
                            if (defl != 0.f)
                            {
                                const float cs = std::cos(defl), sn = std::sin(defl);
                                step_dx = dx * cs - dy * sn;
                                step_dy = dx * sn + dy * cs;
                            }
                        }
                        const float step_scale = std::min(step_len, dist) / dist;
                        const float tx = aim_full_anchor ? ax : (bx + step_dx * step_scale);
                        const float ty = aim_full_anchor ? ay : (by + step_dy * step_scale);
                        // Walk Z: normally the bot's own Z (so move_to navmesh-snaps
                        // to ground; the anchor Z is often an off-mesh raised dais).
                        // BUT when retargeted to a lift base, hold the boarding-floor
                        // Z (lift_floor_z, e.g. z44) instead of the bot's CURRENT Z:
                        // feeding bz let the pathfinder drift the bot DOWN into a
                        // sub-ground pit beside lift 206609 (each tick bz dropped, so
                        // the target sank with it — Somi funnelled to z23-29 instead
                        // of the z44 boarding ground where it can board). Aiming at
                        // the boarding floor keeps the approach on the right level.
                        const float walk_z = retargeted_to_lift ? lift_floor_z
                                           : aim_full_anchor    ? dock_anchor_z
                                           : bz;
                        emit.move_to(tx, ty, walk_z, /*run*/ true);
                        ai.set_last_rule_fired(rule_tag);
                        return true;
                    }
                    // Within 25 y of the anchor but the GO isn't yet in
                    // nearby_objects.
                    // ---- L4: WAIT at the dock for a TRANSPORT ----
                    // For a ship/zeppelin dock the transport is simply mid-route
                    // right now. The OLD behaviour fell through here, so the bot
                    // walked to the dock, found no ship that instant, and wandered
                    // away before the ship returned — which is exactly why 0 bots
                    // ever boarded despite 100K+ dock walks. HOLD at the dock and
                    // wait: emit no movement and claim the tick so wander/idle
                    // rules don't pull the bot off the pier. Ships cycle ~3-5 min;
                    // when the ship arrives within the 30y scan, the in-range
                    // boarding block above (runs earlier each tick) walks the bot
                    // onto its deck and boards. Bound: the objective stuck-detector
                    // (note_obj_observed, ~5 min of no progress) blacklists the
                    // objective if no serving ship ever shows (wrong dock), so the
                    // Builder re-routes — no infinite wait. Portals fall through as
                    // before (a static portal in range would already have fired
                    // use_portal above; the portal-room reach issue is L5).
                    if (s.nearest_portal_anchor_kind() == /*Transport*/ 2)
                    {
                        ai.set_last_rule_fired("idle:wait_for_transport");
                        return true;
                    }
                    // Portal anchor within 25y but GO not in scan — fall through;
                    // next snapshot's in-range rule fires, else stuck-detector
                    // escalates.
                }

                // Third option: a self-cast teleport spell the bot
                // knows whose destination matches the goal map. Covers Mage city teleports, DK Death Gate, Druid
                // Teleport: Moonglade, and any future class teleport.
                // Walks the pre-resolved snapshot list (typical mage:
                // 5-10 entries; non-caster: 0-1 entries) so this is a
                // tiny linear scan. is_ready() gates on cooldown; the
                // cast() optimistic emit cooldown absorbs the next-tick
                // race after the cast is queued.
                for (auto const& tele : s.self_teleport_spells())
                {
                    if (tele.dest_map != poi.map_id) continue;
                    if (!s.is_ready(tele.spell_id)) continue;
                    emit.cast(tele.spell_id);
                    ai.set_last_rule_fired("idle:cast_self_teleport");
                    return true;
                }
                // Third option: hearth, IF homebind happens to be on the
                // goal map. Bots auto-bind at any innkeeper they pass
                // (idle:homebind), so as the bot adventures, homebind
                // tracks recently-visited cities — when the goal returns
                // to a previously-visited map, hearth is a free cross-
                // continent path. 30 min server CD prevents spamming.
                // Skipped in BG / instance (server rejects there).
                if (!s.in_battleground() &&
                    s.can_hearth() &&
                    s.homebind_map_id() != 0 &&
                    s.homebind_map_id() == poi.map_id)
                {
                    emit.hearth();
                    ai.set_last_rule_fired("idle:hearth_to_distant_goal");
                    return true;
                }
                // Fourth option: alternate-hearth items (Garrison
                // Hearthstone, Dalaran Hearthstone, etc.) when the goal
                // is on the item's fixed destination map. These items
                // have their own cooldowns (15-30 min) independent of
                // the regular hearthstone, so they're a "second hearth
                // slot" the player would naturally use. Per-(item entry)
                // cooldown via ActionKind::AltHearth absorbs server-side
                // CD rejects (each item has its own cd that we don't
                // track snapshot-side; the action lockout is the gate).
                // Items below are character-bind-on-pickup so just
                // having them in bags means the bot can use them.
                struct AltHearth { uint32 item_entry; uint32 dest_map; };
                constexpr AltHearth kAltHearths[] = {
                    // Garrison Hearthstone (Draenor garrisons; map 1116
                    // Draenor outdoor). Quest reward from intro chain.
                    {110560, 1116},
                    // Dalaran Hearthstone (Legion Dalaran-Broken Isles).
                    // Quest reward from Legion intro.
                    {140192, 1220},
                    // Garrison Hearthstone fallback faction-shared dest
                    // map; the GO at the garrison forwards Alliance to
                    // the Lunarfall map and Horde to Frostwall, but both
                    // are children of Draenor — keep the simple
                    // fixed-dest table for V0.
                };
                if (!s.in_battleground())
                {
                    const uint32 ah_now_ms = GameTime::GetGameTimeMS();
                    for (auto const& ah : kAltHearths)
                    {
                        if (ah.dest_map != poi.map_id) continue;
                        // Verify the item is in the bot's bags before
                        // emitting — use_item_by_entry would Locked-out
                        // if missing, but we save the cooldown slot.
                        bool have_item = false;
                        for (auto const& it : s.raw().inventory.bag_items)
                        {
                            if (it.guid.IsEmpty()) continue;
                            if (it.entry == ah.item_entry) { have_item = true; break; }
                        }
                        if (!have_item) continue;
                        const uint64 ah_key = uint64(ah.item_entry);
                        if (ai.action_recently_tried(BotAI::ActionKind::AltHearth,
                                                      ah_key, ah_now_ms))
                            continue;
                        emit.emit(UseItemByEntryIntent{ah.item_entry, ObjectGuid::Empty});
                        ai.note_action_retry(BotAI::ActionKind::AltHearth,
                                             ah_key, ah_now_ms);
                        ai.set_last_rule_fired("idle:alt_hearth_to_distant_goal");
                        return true;
                    }
                }
            }
        }

        // ---- Travel via taxi (Phase B) ----
        // The snapshot builder has resolved a (start FM creature, dest
        // node) pair when (a) the bot has a same-map long-distance goal,
        // (b) a known FM is in nearby_friends, (c) a known FM near the
        // destination exists, (d) TaxiPathGraph reports a routable path.
        // Two sub-rules:
        //   * idle:walk_to_taxi : start FM > 5y away → walk to it. Only
        //     fires when no other movement-priority rule already picked
        //     a destination, so it slots ahead of the auto-mount rule
        //     (mounting to walk to the FM is fine — saves time on the
        //     approach).
        //   * idle:fly_to_taxi  : start FM ≤ 5y away → emit fly_to_node.
        //     The API verifies interact range + computes the actual
        //     multi-hop route internally; we just hand it the dest.
        // No combat / casting / mounted gate on the walk variant — the
        // movement rules below handle that. The cast variant *does*
        // require dismount (server rejects mounted FM interact) but
        // that's also enforced server-side and surfaces as Locked, which
        // the optimistic emit cooldown absorbs.
        // Recommended-taxi cascade — body extracted to the free function
        // ::Playerbot::States::DriveRecommendedTaxi (reused by idle:far_same_map_travel
        // @697). Preserve the exact original control flow: drove → return true;
        // wedged/yield → return false; fell through (no taxi route, or no
        // proactive start) → continue the cascade below.
        {
            bool taxi_fell_through = false;
            if (DriveRecommendedTaxi(s, ai, emit, bx, by, bz, &taxi_fell_through))
                return true;
            if (!taxi_fell_through)
                return false;   // wedged/yield — consume tick exactly as the inline block did
            // fell through (no taxi route, or no proactive start) → continue cascade
        }

        // ---- Stuck-by-terrain recovery (idle:unstick) ----
        // The snapshot builder maintains a per-bot StuckTracker that counts
        // consecutive ticks during which the bot's distance to its active
        // movement goal has not dropped by ≥5 y. Snapshots fire at ~5 Hz.
        // Prefer proper path calculation over teleport band-aids:
        //   50 ticks  ≈  10 s  → Tier 0: path to nearest travel-graph anchor
        //                        (quest hub, flight master, capital). Uses
        //                        Detour pathfinding in a DIFFERENT direction
        //                        from the stuck goal — often escapes pockets.
        //  100 ticks  ≈  20 s  → Tier 1: small forward hop (JumpIntent).
        //  200 ticks  ≈  40 s  → Tier 2: NearTeleportTo with ~5y random
        //                        offset to break out of geometry pockets.
        //  500 ticks  ≈ 100 s  → Tier 3: blacklist the active quest so the
        //                        bot drops the goal and picks something new.
        //  750 ticks  ≈ 150 s  → Tier 4 (terminal): hearth back to inn,
        //                        let the next idle cycle start clean.
        // Each tier emits at most once per ~5 s (tracker.last_recovery_ms),
        // so a wedged bot escalates progressively rather than spamming the
        // intent queue. Skipped while in combat / casting / mounted (those
        // states either preempt movement or the bot is mid-recovery).
        // Unstick recovery is suppressed inside BGs / instances: the
        // Tier 4 hearth would teleport the bot out of the match, and
        // even Tier 2 near_teleport_to / Tier 1 jump are aggressive
        // movement intents that interfere with role-bound BG behavior
        // (the BG rules above already have their own pathing logic).
        // A bot truly stuck inside a BG dies in combat or sits out the
        // round — both better than abandoning the match.
        if (!s.in_combat() && !s.is_casting() && !s.raw().movement.is_mounted &&
            !s.in_battleground() && !s.is_in_dungeon())
        {
            // ---- Oscillation escape (goal-thrash wedge) ----
            // The StuckTracker's no_progress_ticks resets on every goal change,
            // so a bot bouncing between two goals it can't reach (a quest
            // objective + a turn-in giver, or a cave objective it can't descend
            // to) keeps "running back and forth" without ever tripping the
            // ladder below. The goal-agnostic leash (note_position_leash) counts
            // ticks the bot fails to escape a ~45y radius while still wanting to
            // travel — independent of goal flips. When it trips, walk to the
            // nearest known-good travel node to break out of the dead zone, then
            // clear the leash so the bot re-plans from a fresh location.
            if (ai.osc_stuck_ticks() >= 120)   // ~24s of can't-escape-the-area
            {
                auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                    s.map_id(), s.raw().position.x, s.raw().position.y, 800.f);
                ai.reset_position_leash();
                if (node)
                {
                    const float ndx = node->x - s.raw().position.x;
                    const float ndy = node->y - s.raw().position.y;
                    if (ndx * ndx + ndy * ndy > 25.f)
                    {
                        emit.move_to(node->x, node->y, node->z, /*run*/ true);
                        ai.set_last_rule_fired("idle:unstick:oscillation_escape");
                        return true;
                    }
                }
            }
            auto const& t = ai.stuck_tracker();
            if (t.active && t.no_progress_ticks >= 50)
            {
                const uint32 now_ms = s.published_at_ms();
                // A casting bot is NOT wedged — it's busy. Firing another
                // recovery action mid-cast cancels the cast; the terminal
                // hearth (10s cast) could never complete because the generic
                // 5s retry below re-fired and interrupted it, looping
                // "idle:unstick:hearth" ~2x/min forever with zero effect
                // (audit C19). Let any in-progress cast finish first.
                if (s.is_casting()) { /* wait out the cast */ }
                else {
                // Tier-aware retry: after emitting the tier-5 hearth, give the
                // 10s cast room to land (+ loading screen) before ANY further
                // recovery emit; other tiers keep the snappy 5s cadence.
                const uint32 kMinRetryMs =
                    (t.recovery_tier == 5) ? 20000u : 5000u;
                if (now_ms - t.last_recovery_ms >= kMinRetryMs)
                {
                    auto& tw = ai.mutable_stuck_tracker();
                    // ---- LAST RESORT: teleport (walk-first policy) ----
                    // Only after ~5+ minutes of failed WALK-based recovery
                    // (Tiers 0-3 below) do we allow a teleport at all. Per the
                    // no-teleport-rescue principle, walking out is always tried
                    // first and for a long time; this is the backstop for a bot
                    // genuinely sealed off from the mesh so it isn't frozen
                    // forever.
                    if (t.no_progress_ticks >= 2000)   // ~6.7 min
                    {
                        // Terminal escape — hearth home; the post-hearth idle
                        // loop picks a fresh hub next snapshot. Gated on
                        // can_hearth() (no item / mid-CD would just spam
                        // rejections); falls through to the near-teleport hop
                        // below when unavailable.
                        if (s.can_hearth())
                        {
                            emit.hearth();
                            tw.last_recovery_ms = now_ms;
                            tw.recovery_tier = 5;
                            ai.set_last_rule_fired("idle:unstick:hearth");
                            return true;
                        }
                    }
                    if (t.no_progress_ticks >= 1500)   // ~5 min
                    {
                        // Short goal-biased near-teleport hop (~5y) to break a
                        // wedge that no amount of walking cleared. Goal-biased
                        // so the hop stays along the intended path / same nav
                        // island rather than into a wall or onto another
                        // stratum (the SafeNearTeleport Z-delta guard still
                        // rejects bad strata).
                        float angle;
                        bool goal_biased = false;
                        if (s.has_current_objective() &&
                            s.current_objective_poi().valid &&
                            s.current_objective_poi().map_id == s.map_id())
                        {
                            const float dx = s.current_objective_poi().x - s.raw().position.x;
                            const float dy = s.current_objective_poi().y - s.raw().position.y;
                            if (dx * dx + dy * dy > 1.0f)
                            {
                                angle = std::atan2(dy, dx);
                                goal_biased = true;
                            }
                            else
                            {
                                angle = static_cast<float>(now_ms % 6283) * 0.001f;
                            }
                        }
                        else
                        {
                            angle = static_cast<float>(now_ms % 6283) * 0.001f;
                        }
                        const float dx = std::cos(angle) * 5.0f;
                        const float dy = std::sin(angle) * 5.0f;
                        emit.near_teleport_to(s.raw().position.x + dx,
                                              s.raw().position.y + dy,
                                              s.raw().position.z,
                                              s.raw().position.o);
                        tw.last_recovery_ms = now_ms;
                        tw.recovery_tier = 4;
                        ai.set_last_rule_fired(goal_biased
                            ? "idle:unstick:teleport_goal"
                            : "idle:unstick:teleport");
                        return true;
                    }
                    // ---- Tier 3: drop / reroute the goal (walk-based) ----
                    if (t.no_progress_ticks >= 500)
                    {
                        // Quest goal → blacklist the active quest (60min) so
                        // goal selection moves to another objective; the
                        // tracker resets via goal_changed next builder pass.
                        if (t.goal_is_quest && s.current_quest_id() != 0)
                        {
                            ai.blacklist_quest(s.current_quest_id(), now_ms);
                            tw.last_recovery_ms = now_ms;
                            tw.recovery_tier = 3;
                            ai.set_last_rule_fired("idle:unstick:drop_goal");
                            return true;
                        }
                        // Movement goal (or quest with no droppable id — a
                        // turn-in / offer chase): there's no quest to blacklist,
                        // so fall through to the wider walk-escape below. The
                        // walk-escape's own move_to re-stamps last_move_to, which
                        // makes the next builder pass see a new goal and reset
                        // the tracker via goal_changed — a clean restart without
                        // freezing this tick's recovery.
                    }
                    // ---- Tier 2: wide walk-escape (replaces near-teleport) ----
                    if (t.no_progress_ticks >= 200)
                    {
                        // Actively WALK toward the nearest known-good navmesh
                        // node at a wider range than Tier 0 (≤800y). A bot
                        // wedged in a geometry pocket for ~40s walks out toward
                        // a road / hub / flight master instead of teleporting.
                        auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                            s.map_id(), s.raw().position.x, s.raw().position.y, 800.f);
                        if (node)
                        {
                            const float ndx = node->x - s.raw().position.x;
                            const float ndy = node->y - s.raw().position.y;
                            if (ndx * ndx + ndy * ndy > 25.f)
                            {
                                emit.move_to(node->x, node->y, node->z, /*run*/ true);
                                tw.last_recovery_ms = now_ms;
                                tw.recovery_tier = 2;
                                ai.set_last_rule_fired("idle:unstick:walk_escape");
                                return true;
                            }
                        }
                        // No node within 800y — fall through to jump.
                    }
                    if (t.no_progress_ticks >= 100)
                    {
                        // Tier 1 — small forward hop. Cheap, often clears
                        // 1-y ledge wedges where pathing refuses to climb.
                        emit.jump(/*forward*/ 5.0f);
                        tw.last_recovery_ms = now_ms;
                        tw.recovery_tier = 1;
                        ai.set_last_rule_fired("idle:unstick:jump");
                        return true;
                    }
                    // Tier 0 — path to nearest known-good location.
                    // Use the travel graph to find a nearby anchor
                    // (quest hub, flight master, capital, etc.) that
                    // sits on validated navmesh. Walking TOWARD a road
                    // or hub in a different direction often escapes
                    // geometry pockets without any teleport band-aid.
                    {
                        auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                            s.map_id(), s.raw().position.x, s.raw().position.y, 300.f);
                        if (node)
                        {
                            const float ndx = node->x - s.raw().position.x;
                            const float ndy = node->y - s.raw().position.y;
                            if (ndx * ndx + ndy * ndy > 25.f)
                            {
                                emit.move_to(node->x, node->y, node->z, /*run*/ true);
                                tw.last_recovery_ms = now_ms;
                                tw.recovery_tier = 0;
                                ai.set_last_rule_fired("idle:unstick:move_to_anchor");
                                return true;
                            }
                        }
                    }
                    // No anchor within range — fall through; next tick
                    // will hit Tier 1 (jump) once ticks cross 100.
                    tw.last_recovery_ms = now_ms;
                    return true;
                }
                }   // !s.is_casting() — recovery deferred while a cast is in flight
            }
        }

        // ---- Auto-mount for long travel ----
        // Mount up before walking long distances. Gates:
        //  - bot has a usable mount spell in spellbook (riding > 0 +
        //    SPELL_AURA_MOUNTED-bearing spell)
        //  - bot has a long-distance travel goal (POI > 80y or quest hub
        //    > 80y or turn-in giver > 80y)
        //  - bot is currently out of combat, not casting, not mounted
        // Server enforces zone/indoor/flight restrictions at cast time;
        // failures are absorbed by the optimistic emit-cooldown so the
        // rule doesn't spam re-emits when the server says "not here."
        if (!s.in_combat() && !s.is_casting() && !s.raw().movement.is_mounted &&
            s.best_mount_spell() != 0)
        {
            float far_target_x = 0.f, far_target_y = 0.f;
            bool  has_far_target = false;
            constexpr float kMountThreshold   = 80.0f;
            constexpr float kMountThresholdSq = kMountThreshold * kMountThreshold;
            if (s.has_current_objective() && s.current_objective_poi().valid &&
                s.current_objective_poi().map_id == s.map_id())
            {
                const auto& poi = s.current_objective_poi();
                const float dx = poi.x - bx, dy = poi.y - by;
                if (dx*dx + dy*dy > kMountThresholdSq)
                { far_target_x = poi.x; far_target_y = poi.y; has_far_target = true; }
            }
            if (!has_far_target)
            {
                for (auto const& tin : s.raw().quest_discovery.quest_turnins)
                {
                    for (auto const& u : s.raw().combat.nearby_friends)
                    {
                        if (u.guid != tin.giver) continue;
                        const float dx = u.x - bx, dy = u.y - by;
                        if (dx*dx + dy*dy > kMountThresholdSq)
                        {
                            far_target_x = u.x; far_target_y = u.y;
                            has_far_target = true;
                        }
                        break;
                    }
                    if (has_far_target) break;
                }
            }
            if (!has_far_target)
            {
                for (auto const& off : s.raw().quest_discovery.quest_offers)
                {
                    for (auto const& u : s.raw().combat.nearby_friends)
                    {
                        if (u.guid != off.giver) continue;
                        const float dx = u.x - bx, dy = u.y - by;
                        if (dx*dx + dy*dy > kMountThresholdSq)
                        {
                            far_target_x = u.x; far_target_y = u.y;
                            has_far_target = true;
                        }
                        break;
                    }
                    if (has_far_target) break;
                }
            }
            if (has_far_target)
            {
                // Per-zone mount-attempt lockout. Without this, in
                // no-mount zones (Dalaran Hall of Shadows, dungeon
                // corridors, certain quest phases, Wintergrasp when
                // inactive) the cast emit re-fires every snapshot
                // tick — server rejects each, but the SpellCast
                // pipeline still pays. At 2000 bots this is
                // measurable. Bucket by (map, zone) so a zone
                // transition gives the bot a fresh attempt.
                const uint32 mt_now_ms = GameTime::GetGameTimeMS();
                const uint64 mt_key =
                    (uint64(0xFAu) << 48)
                    | (uint64(s.raw().position.map_id) << 32)
                    | uint64(s.raw().area.zone_id);
                if (!ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                               mt_key, mt_now_ms))
                {
                    emit.cast(s.best_mount_spell());
                    ai.note_action_retry(BotAI::ActionKind::BgPort,
                                         mt_key, mt_now_ms);
                    ai.set_last_rule_fired("idle:mount_for_travel");
                    return true;
                }
            }
        }

        // ---- Quest ender pathing (walk-to-quest-ender) ----
        // MOVED 2026-06-15 (travel-engagement fix): this block was here, at the
        // BOTTOM of the idle cascade, BELOW the filler wander/service rules. That
        // meant a bot holding COMPLETE quests would fire idle:wander_to_service
        // (urgent vendor/repair) or wander_to_quest_hub FIRST and never reach the
        // turn-in walk — Uraimus looped wander_to_service + path_fail at the
        // Dolanaar inn-island with 6 completable quests in his log, "doing
        // nothing productive". Turning in completed quests is productive and must
        // outrank filler, so the block now runs earlier — right after the active
        // quest-pursuit rules and before idle:wander_to_service. See it there.

        // ---- Quest POI pathing fallback ----
        // No type-specific rule fired — walk toward the pre-resolved
        // QuestPOI waypoint so subsequent ticks bring the target into
        // nearby range. Stops when within arrival radius so the type-
        // specific rule takes over for the close approach.
        //
        // Same-map quests: try direct PathGenerator walk first (cheap,
        // works for in-zone POIs). If the walk wedges (e.g., POI behind
        // a navmesh seam at zone boundary), fall back to the unified
        // travel graph so the bot can take a flight or portal to a
        // closer anchor and walk from there. Real players do the same:
        // try to run there, then if it's too far, take the flight.
        //
        // Different-map quests: skip direct walk (PathGenerator is
        // single-mesh) and go straight to the travel graph. The
        // earlier `idle:travel_plan` rule at the top of DispatchIdle
        // also handles different-map cases — kept here as a defense-
        // in-depth fallback in case priority ordering misses it.
        if (s.has_current_objective() && s.current_objective_poi().valid)
        {
            const bool poi_same_map =
                s.current_objective_poi().map_id == s.map_id();
            const auto& poi = s.current_objective_poi();

            // Local helper: ask UnifiedTravelGraph for an A*-routed
            // path (walk/flight/portal/ship/dungeon-entry hops) to the
            // POI and emit the move toward the next leg's anchor on the
            // bot's current map. Returns true when a move was emitted
            // (caller should `return true`). False when graph has no
            // route OR all on-map legs already reached. Used both for
            // cross-map quests (always) and same-map quests where the
            // direct PathGenerator walk has wedged (POI behind a zone
            // seam, etc.).
            auto try_travel_graph_to_poi =
              [&](char const* rule_tag) -> bool
            {
                if (s.in_combat() || s.is_casting() ||
                    s.raw().movement.is_mounted) return false;
                Player* self = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
                if (!self) return false;
                ::Playerbot::V2::Travel::RouteRequest req{};
                req.bot          = self;
                req.from_map     = s.map_id();
                req.from_x       = bx;
                req.from_y       = by;
                req.from_z       = bz;
                req.to_map       = poi.map_id;
                req.to_x         = poi.x;
                req.to_y         = poi.y;
                req.to_z         = poi.z;
                req.allow_hearth = false;
                auto route = Services::TravelGraph().FindRoute(req);
                if (!route.ok || route.legs.empty()) return false;
                for (auto const& leg : route.legs)
                {
                    if (leg.to_map != s.map_id()) continue;
                    // Skip non-walk legs — dedicated boarding rules drive
                    // those (see idle:travel_plan comment above).
                    if (leg.kind != ::Playerbot::V2::Travel::EdgeKind::Walk) continue;
                    const float ldx = leg.to_x - bx, ldy = leg.to_y - by;
                    const float ldsq = ldx * ldx + ldy * ldy;
                    constexpr float kArrived = 25.0f;
                    if (ldsq <= kArrived * kArrived) continue;
                    const float kStep =
                        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                        35.0f;
                    const float dist = std::sqrt(ldsq);
                    const float scale = std::min(kStep, dist) / dist;
                    const float etx = bx + ldx * scale;
                    const float ety = by + ldy * scale;
                    emit.move_to(etx, ety, bz, /*run*/ true);
                    ai.set_last_rule_fired(rule_tag);
                    return true;
                }
                return false;
            };

            // Cross-map quest: direct walk would just NoPath against a
            // different-map target, so go straight to the travel graph.
            // (The earlier `idle:travel_plan` block at line ~4697 also
            // handles this; this branch fires when it didn't — defense
            // in depth.)
            if (!poi_same_map)
            {
                if (ai.in_charter_grace(s.published_at_ms()))
                {
                    // Defer travel during charter participation.
                }
                else if (try_travel_graph_to_poi("idle:quest_path:travel_plan"))
                {
                    return true;
                }
                // No graph route → fall through; quest stuck-detector
                // will eventually blacklist the quest.
                return false;
            }
            // Planar distance: Quest POI Z is a map-display hint; the DB
            // often stores 0 there. The type-specific rules (kill / collect
            // / talk / use_go) handle close-approach Z via nearby_* lists.
            //
            // Arrival radius: scales with the POI polygon size. A pinpoint
            // POI uses a 25y arrival circle (one screen). A zone POI (e.g.
            // "kill 10 wolves in Northshire Valley", which is a polygon
            // covering a chunk of the zone) uses radius + 10y slop — so
            // the bot stops walking and starts hunting once it crosses
            // into the polygon, instead of marching to the centroid.
            const float dx = poi.x - bx, dy = poi.y - by;
            const float dsq = dx*dx + dy*dy;
            // Default arrival radius scales with polygon size — for a
            // pinpoint POI use 25 y, for a zone POI use radius+10 y so
            // bot stops at the polygon edge instead of marching to the
            // centroid (matches how players naturally hunt scattered
            // mobs across a zone).
            //
            // BUT: if the bot has an unfulfilled MONSTER objective and
            // there are zero matching enemies in nearby_enemies, the
            // type-specific quest_kill rule didn't fire and standing in
            // the polygon edge does nothing — the bot just wanders
            // until the stuck-detector escalates. Fall back to a tight
            // 25 y arrival so the bot patrols toward the densest spawn
            // area (the centroid). Same logic for ITEM (collect-from-
            // creature) and GAMEOBJECT objectives.
            float arrived = (poi.radius > 1.f ? poi.radius + 10.f : 25.f);
            if (s.current_objective().type == /*MONSTER*/ 0 ||
                s.current_objective().type == /*ITEM*/ 1 ||
                s.current_objective().type == /*GAMEOBJECT*/ 2 ||
                s.current_objective().type == /*TALKTO*/ 3)
            {
                const int32 target_id = s.current_objective().object_id;
                bool any_match = false;
                if (s.current_objective().type == /*MONSTER*/ 0 ||
                    s.current_objective().type == /*ITEM*/ 1)
                {
                    for (auto const& u : s.raw().combat.nearby_enemies)
                        if (uint32(target_id) == u.entry) { any_match = true; break; }
                }
                else if (s.current_objective().type == /*TALKTO*/ 3)
                {
                    for (auto const& u : s.raw().combat.nearby_friends)
                        if (uint32(target_id) == u.entry) { any_match = true; break; }
                }
                else /* GAMEOBJECT */
                {
                    for (auto const& o : s.raw().world_objects.nearby_objects)
                        if (uint32(target_id) == o.entry) { any_match = true; break; }
                }
                if (!any_match)
                    arrived = 25.f;   // patrol toward centroid
            }
            const float kPoiArrivedSq = arrived * arrived;
            if (dsq > kPoiArrivedSq)
            {
                // Personality-scaled step toward POI; cap at the actual POI
                // distance to avoid overshoot.
                const float kStep =
                    ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
                    ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                    35.0f;
                const float dist = std::sqrt(dsq);
                const float scale = std::min(kStep, dist) / dist;
                const float tx = bx + dx * scale;
                const float ty = by + dy * scale;
                // move_to runs UpdateAllowedPositionZ on the destination,
                // so passing the bot's current Z just lets it snap to
                // navmesh ground at (tx, ty). poi.z is unreliable (often 0).
                // Charter grace: bot was just teleported to a petitioner
                // plaza for guild founding / signing. Quest POIs are in
                // their original level zone — walking back kills the
                // charter FSM (observed 2026-05-18: founders looping
                // idle:quest_path move_to + path_fail, never reaching
                // phase-1 NPC interact).
                if (ai.in_charter_grace(s.published_at_ms()))
                {
                    // Fall through, give the charter rule a turn.
                }
                else if (ai.check_anchor_wedge("idle:quest_path",
                                          s.path_blocked_count(),
                                          s.published_at_ms()))
                {
                    // Direct walk wedged — POI is behind a navmesh seam
                    // (zone boundary, indoors, water). Try the travel
                    // graph: maybe a flight or portal puts us closer.
                    // Same as a real player who can't run there directly
                    // and takes a flight instead.
                    if (try_travel_graph_to_poi("idle:quest_path:travel_plan"))
                        return true;
                    // Direct walk AND the travel graph both failed → this
                    // objective is unreachable from here. The slow stuck-
                    // detector does NOT reliably catch this: the wedge branch
                    // doesn't re-emit the idle:quest_path move_to, so the
                    // NoPath-strike streak (keyed on last_rule_fired) never
                    // advances and the objective loops forever (observed: L4
                    // "Somi" 400× NoPath to an Orgrimmar POI whose SnapToGround
                    // grabbed an upper WMO platform Z=59 the ground-floor poly
                    // can't reach). Blacklist the objective NOW so the Builder
                    // picks a reachable quest; once same-map work is exhausted
                    // the cross-map breadcrumb surfaces and flight/zeppelin
                    // travel engages.
                    ai.blacklist_objective_now(s.current_quest_id(),
                                               s.current_objective().id,
                                               s.published_at_ms());
                }
                else
                {
                    // Q-P1b: for FAR same-map POIs hand the full POI coords to
                    // move_to and let Detour build the entire navmesh corridor
                    // (exactly as idle:travel_to_hub does). The old 35y
                    // straight-line step wedged against walls / zone seams
                    // between steps (the POI could sit behind geometry the
                    // straight line cut through) and re-ran Detour every tick;
                    // full coords route around obstacles and the move_to dedup
                    // suppresses per-tick re-emission. Keep the short stepped
                    // approach for nearby POIs where finer threat-aware control
                    // matters during the final approach.
                    // UNDERGROUND objective Z preservation (Ban'ethil Barrow Den,
                    // Uraimus q483). Normally we pass the bot's Z and let move_to's
                    // UpdateAllowedPositionZ snap the destination to navmesh ground
                    // at (tx,ty) — poi.z is usually a surface display hint (often 0).
                    // BUT for a cave/den objective the snapshot's surface-over-cave
                    // correction (BotSnapshotBuilder adopt) writes the granting GO's
                    // REAL den-floor Z into poi.z. At a den's x/y there are TWO
                    // navmesh layers — the hilltop surface AND the den floor below —
                    // and UpdateAllowedPositionZ snaps to whichever poly is closest
                    // to the Z we pass. Passing bz (the bot's surface Z) snaps the
                    // destination to the HILLTOP, so the bot climbs the hill and then
                    // can't path straight down into the den (NoPath). Passing the
                    // adopted den Z snaps to the den-floor poly, so Detour routes IN
                    // via the tunnel / off-mesh entrance bridge. Only override when
                    // poi.z is set and clearly BELOW the bot (mirrors the builder's
                    // own "clearly below" guard); flat/surface/dock POIs keep bz.
                    const float mz = (poi.z != 0.0f && poi.z < bz - 12.0f) ? poi.z : bz;
                    if (dist > 60.0f)
                        emit.move_to(poi.x, poi.y, mz, /*run*/ true);
                    else
                        emit.move_to(tx, ty, mz, /*run*/ true);
                    ai.set_last_rule_fired("idle:quest_path");
                    return true;
                }
            }
        }

        // ---- First Aid: bandage crafting from cloth ----
        // Skill 129. Bandages craft from cloth (Linen, Wool, Silk, Mageweave,
        // Runecloth, etc — all subclass 5 of class 0 Trade Goods → Cloth).
        // Only fires when bot has a known First Aid recipe with reagents
        // and is below skill cap. Self-targeted cast. Captured by the
        // generic auto-craft below as well, but kept as a distinct rule for
        // /activity attribution and to skip the rest_bonus_xp gate (you can
        // safely craft a bandage in the wild between fights, unlike most
        // other recipes).
        if (s.has_skill(/*FirstAid*/ 129) && !s.is_skill_capped(129) &&
            !s.raw().spellbook.known_recipes.empty() && !s.raw().movement.is_mounted &&
            s.bag_free_slots() >= 2)
        {
            uint32 fa_spell = 0;
            for (uint32 spell_id : s.raw().spellbook.known_recipes)
            {
                RecipeMeta const* meta = FindRecipeMeta(spell_id);
                if (!meta || meta->skill_line_id != 129) continue;
                const RecipeColor c = ResolveRecipeColor(spell_id, s.skill_value(129));
                if (c == RecipeColor::Gray || c == RecipeColor::Unknown) continue;
                if (s.is_ready(spell_id) && s.has_reagents(spell_id))
                { fa_spell = spell_id; break; }
            }
            if (fa_spell != 0)
            {
                emit.cast(fa_spell);
                ai.set_last_rule_fired("idle:craft_bandage");
                return true;
            }
        }

        // ---- Auto-learn Recipe items in bag ----
        // Recipe items (ItemTemplate.Class == 9 / ITEM_CLASS_RECIPE) teach a
        // profession recipe when used. Without this rule a bot's bag fills
        // up with looted recipes that go unused. Per-entry 5min cooldown
        // via ActionKind::LearnRecipe absorbs server-side rejections (level
        // / skill / class requirement, already known) — once accepted the
        // item is consumed and the entry never reappears.
        if (!s.raw().movement.is_mounted && !s.is_casting() && !s.in_combat())
        {
            const uint32 lr_now_ms = GameTime::GetGameTimeMS();
            for (auto const& it : s.raw().inventory.bag_items)
            {
                if (it.guid.IsEmpty()) continue;
                if (it.is_quest_item) continue;
                if (it.item_class != /*ITEM_CLASS_RECIPE*/ 9) continue;
                const uint64 lr_key = uint64(it.entry);
                if (ai.action_recently_tried(BotAI::ActionKind::LearnRecipe,
                                              lr_key, lr_now_ms))
                    continue;
                emit.emit(UseItemByEntryIntent{it.entry, ObjectGuid::Empty});
                ai.note_action_retry(BotAI::ActionKind::LearnRecipe,
                                     lr_key, lr_now_ms);
                ai.set_last_rule_fired("idle:learn_recipe");
                return true;
            }
        }

        // ---- Disenchant green+ items ----
        // Spell 13262 (Disenchanting). Enchanters turn unwanted uncommon+
        // gear into dust/essence/shards. Two cases qualify:
        //   1) equip_slot == 0xFF — the bot CANNOT equip the item at all
        //      (wrong armor proficiency, wrong class restriction, wrong
        //      race lock). Useless dead weight; DE unconditionally for
        //      free mats. This is the case the user asked us to add.
        //   2) bot CAN equip the item, but item_level + 10 < bot's avg
        //      equipped ilvl — significantly worse than current gear and
        //      not worth a swap. The 10-ilvl margin keeps potential
        //      sidegrades safe.
        // Quest-locked items are always skipped (server refuses anyway).
        if (s.knows_spell(13262) && s.is_ready(13262) && !s.raw().movement.is_mounted)
        {
            uint16 const my_ilvl = s.average_item_level();
            for (auto const& it : s.raw().inventory.bag_items)
            {
                if (it.guid.IsEmpty()) continue;
                if (it.is_quest_item) continue;
                if (it.quality < 2) continue;        // green+ only
                if (it.item_level == 0) continue;
                if (it.equip_slot == 0xFF)
                {
                    // Case 1: not equippable for THIS bot. DE unconditionally.
                }
                else
                {
                    // Case 2: equippable but only DE if clearly worse than
                    // current gear (10-ilvl margin against avg equipped).
                    if (my_ilvl == 0) continue;
                    if (it.item_level + 10 > my_ilvl) continue;
                }
                emit.cast_on_item(13262, it.guid);
                ai.set_last_rule_fired("idle:disenchant");
                return true;
            }
        }

        // ---- Prospect ore stacks (Jewelcrafting) ----
        // Spell 31252. Consumes 5 ore, produces gems.
        // ITEM_CLASS_TRADE_GOODS=7 / ITEM_SUBCLASS_TRADE_GOODS_METAL_AND_STONE=7.
        if (s.knows_spell(31252) && s.is_ready(31252) && !s.raw().movement.is_mounted)
        {
            for (auto const& it : s.raw().inventory.bag_items)
            {
                if (it.guid.IsEmpty()) continue;
                if (it.count < 5) continue;
                if (it.item_class != 7 || it.item_subclass != 7) continue;
                emit.cast_on_item(31252, it.guid);
                ai.set_last_rule_fired("idle:prospect");
                return true;
            }
        }

        // ---- Mill herb stacks (Inscription) ----
        // Spell 51005. Consumes 5 herbs, produces pigments.
        // ITEM_CLASS_TRADE_GOODS=7 / ITEM_SUBCLASS_TRADE_GOODS_HERB=9.
        if (s.knows_spell(51005) && s.is_ready(51005) && !s.raw().movement.is_mounted)
        {
            for (auto const& it : s.raw().inventory.bag_items)
            {
                if (it.guid.IsEmpty()) continue;
                if (it.count < 5) continue;
                if (it.item_class != 7 || it.item_subclass != 9) continue;
                emit.cast_on_item(51005, it.guid);
                ai.set_last_rule_fired("idle:mill");
                return true;
            }
        }

        // ---- Auto-craft (skill-up driven) ----
        // Fires when nothing else has — bot is idle and has at least one
        // recipe with reagents available + non-Gray color. Picks the
        // highest-color (Orange > Yellow > Green) eligible recipe for
        // any non-capped profession skill. CastSpellIntent on the
        // recipe self-targets — server resolves to inventory item
        // creation. Skipped while moving (we set is_moving) or in combat
        // (can_autoact gates that).
        //
        // Used to be rested-only; relaxed so all profession chains can
        // fire end-to-end (mine → smelt → smith, herb → alch, cloth →
        // tailor, etc.) without the bot needing to detour to an inn.
        // Combat / mounted / duel / bag-pressure gates still apply, so
        // a bot under threat won't waste reagents mid-fight.
        if (!s.raw().spellbook.known_recipes.empty() &&
            !s.raw().movement.is_mounted && !s.raw().social_events.has_duel_request &&
            s.bag_free_slots() >= 4)
        {
            // Tiebreaker: among same-color recipes, prefer the one whose
            // skill is LOWEST. For multi-profession bots this evens out
            // skill progression — a bot with Tailoring 100 + Engineering 50
            // will pick an Engineering recipe over a Tailoring recipe of the
            // same color, catching up the laggard. Within the same skill,
            // recipe order is whatever the snapshot delivered (no further
            // tiebreak — server cooldowns make sequential casts deterministic
            // anyway).
            uint32      best_spell = 0;
            RecipeColor best_color = RecipeColor::Unknown;
            uint16      best_skill = 0xFFFFu;
            for (uint32 spell_id : s.raw().spellbook.known_recipes)
            {
                RecipeMeta const* meta = FindRecipeMeta(spell_id);
                if (!meta) continue;
                if (s.is_skill_capped(meta->skill_line_id)) continue;
                const uint16 cur = s.skill_value(meta->skill_line_id);
                const RecipeColor c = ResolveRecipeColor(spell_id, cur);
                if (c == RecipeColor::Gray || c == RecipeColor::Unknown) continue;
                // Color rank: Orange=1, Yellow=2, Green=3 — lower is better.
                // Replace if: better color, OR same color but lower skill.
                const bool better_color = (best_spell == 0) || (uint8(c) < uint8(best_color));
                const bool same_color_lower_skill =
                    (best_spell != 0) && (c == best_color) && (cur < best_skill);
                if (better_color || same_color_lower_skill)
                {
                    if (s.is_ready(spell_id) && s.has_reagents(spell_id))
                    {
                        best_spell = spell_id;
                        best_color = c;
                        best_skill = cur;
                    }
                }
            }
            if (best_spell != 0)
            {
                emit.cast(best_spell);
                ai.set_last_rule_fired("idle:craft_skillup");
                return true;
            }
        }

        // ---- Wander rule (fires for ALL aggressions including Passive) ----
        // "No mob in pull range, drift to find one." Without this, a bot that
        // clears its 30y bubble would idle forever in the empty spot. Picks
        // an angle from a stable hash of (bot_id, 5s-bucket of published_at_ms)
        // so each bot in the same patch fans out in a different direction,
        // and the angle drifts every 5s — if a wander step hits unreachable
        // terrain the next attempt picks a new angle and recovers. Skipped
        // in rested areas (cities/inns — let the bot sit and emote there).
        // Quest givers / gathering nodes / vendors in range are handled by
        // higher-priority rules above this block, so the wander only ever
        // fires when there is genuinely nothing else of interest to do at
        // the current spot. Step distance scales with RiskTolerance — a
        // cautious bot makes shorter, safer steps; a reckless bot covers
        // more ground per step. Gated on hp_above_engage_gate so wounded bots
        // sit and regen instead of wandering into the next mob pack — the
        // wander_to_service / quest_hub / node rules above are intentional
        // recovery moves and intentionally NOT gated on HP.
        // Skip wander only when the bot is rested AND has nothing to do
        // (no quests in log, no resolved offers/turnins, no current objective).
        // Rested in a starter zone where the bot still has questing work was
        // gating wander off, leaving the bot frozen at homebind: starter_talents
        // fires once → all maintenance rules pass → no quest action visible at
        // 5y → wander gated by rest → bot literally never moves. Wander when
        // there's work; sit only when truly idle.
        // A bot being routed to a far leveling hub (R7 relocation) has no real
        // quest work, so it counts as truly idle here: if we reached the wander
        // rule its cross-map travel rules already declined this tick (e.g. no
        // route yet), and the right fallback is to wander/grind locally —
        // exactly the relocate-first, grind-as-fallback behaviour.
        const bool truly_idle =
            s.raw().quest_log.quests.empty() &&
            s.raw().quest_discovery.quest_offers.empty() &&
            s.raw().quest_discovery.quest_turnins.empty() &&
            (!s.has_current_objective() || s.objective_is_relocation());
        const bool wander_allowed_by_rest =
            s.raw().identity.rest_bonus_xp == 0 || !truly_idle;
        // Chat pause cue suppresses wander — when the leader said
        // "wait" or "inc", real players stop walking around. The
        // pause clears automatically after 12s (or 8s for inc) so
        // wander resumes when the moment passes.
        const bool chat_pause_active =
            s.published_at_ms() < ai.chat_pause_until_ms();
        if (wander_allowed_by_rest && hp_above_engage_gate && !chat_pause_active)
        {
            // Diagnostic: every wander fire snapshots WHY the higher-
            // priority rules cascaded down here. Throttled per-bot to
            // once per 60s via last_wander_diag_ms; without the throttle
            // the wander rule (87% of fleet decisions) would drown the
            // log. Output is grep-friendly: a single line, key=val pairs,
            // one tag per cascade gate so post-run analysis can tally
            // distributions cheaply.
            {
                const uint32 wd_now = s.published_at_ms();
                const uint32 wd_last = ai.last_wander_diag_ms();
                if (wd_last == 0 || (wd_now - wd_last) >= 60000u)
                {
                    ai.set_last_wander_diag_ms(wd_now);
                    auto const& ql = s.raw().quest_log;
                    auto const& qd = s.raw().quest_discovery;
                    auto const& wo = s.raw().world_objects;
                    const bool   has_obj  = s.has_current_objective();
                    const auto&  obj      = s.current_objective();
                    const auto&  poi      = s.current_objective_poi();
                    const uint32 obj_type = has_obj ? uint32(obj.type) : 999u;
                    // Skill/profession indicators — gather rule's first
                    // gate. 182=Herbalism 186=Mining 393=Skinning 356=Fishing.
                    const bool has_gather_skill =
                        s.has_skill(182) || s.has_skill(186) ||
                        s.has_skill(393) || s.has_skill(356);
                    // Gathering node visible? GO_GATHERING_NODE_LOCAL=50.
                    uint32 nodes_in_sight = 0;
                    for (auto const& gobj : wo.nearby_objects)
                        if (gobj.go_type == 50) ++nodes_in_sight;
                    // Vendor visible? Either flag bit.
                    uint32 vendors_in_sight = 0;
                    for (auto const& nf : s.raw().combat.nearby_friends)
                        if (nf.npc_flags & (uint32(UNIT_NPC_FLAG_VENDOR) |
                                            uint32(UNIT_NPC_FLAG_REPAIR)))
                            ++vendors_in_sight;
                    // Capture quest_id + obj_id so we can wowhead the exact
                    // quest driving the wander. For obj_type=999 (no
                    // current_objective) emit the first 3 quest IDs from
                    // the log instead — that's our best lead for "qlog
                    // populated but picker rejected everything".
                    const uint32 quest_id = has_obj ? s.current_quest_id() : 0u;
                    const uint32 obj_id   = has_obj ? uint32(obj.id) : 0u;
                    std::string qlog_head;
                    if (!has_obj && !ql.quests.empty())
                    {
                        const size_t n = std::min<size_t>(3, ql.quests.size());
                        for (size_t i = 0; i < n; ++i)
                        {
                            if (i) qlog_head.push_back(',');
                            qlog_head += std::to_string(ql.quests[i].quest_id);
                        }
                    }
                    TC_LOG_INFO("playerbot.v2",
                        "[wander_reason] bot={} lvl={} mode={} "
                        "qlog={} offers={} turnins={} "
                        "cur_obj={} obj_type={} quest_id={} obj_id={} "
                        "qlog_head={} "
                        "poi_valid={} poi_map={} "
                        "gather_skill={} nodes={} vendors={} "
                        "vv_mask={:#x} moving={} path_blocks={}",
                        s.name(), uint32(s.level()),
                        uint32(ai.activity_mode()),
                        uint32(ql.quests.size()),
                        uint32(qd.quest_offers.size()),
                        uint32(qd.quest_turnins.size()),
                        has_obj ? 1 : 0, obj_type,
                        quest_id, obj_id,
                        qlog_head.empty() ? "-" : qlog_head.c_str(),
                        poi.valid ? 1 : 0, poi.map_id,
                        has_gather_skill ? 1 : 0,
                        nodes_in_sight, vendors_in_sight,
                        uint32(s.vendor_visit_phases_pending()),
                        s.is_moving() ? 1 : 0,
                        s.path_blocked_count());
                }
            }
            // Post-rescue grace: GlobalStuckRescue just teleported this
            // bot to a safe plaza (Stormwind Keep / Orgrimmar Grommash
            // Hold). Suppress wander for kPostRescueGraceMs so the bot
            // doesn't immediately drift back into the navmesh-edge area
            // it was rescued from. Cross-map / long-distance rules
            // (travel_plan, walk_to_known_*) ran their planning from
            // the prior position; letting them re-emit immediately
            // would replay the same bad destinations.
            if (ai.in_post_rescue_grace(s.published_at_ms()))
            {
                ai.set_last_rule_fired("idle:post_rescue_settle");
                return true;
            }
            // Charter grace: same idea — bot was teleported onto a
            // petitioner plaza as founder or signer. Don't wander
            // away from the founder; the signer rule needs the
            // founder within 30y to fire.
            if (ai.in_charter_grace(s.published_at_ms()))
            {
                ai.set_last_rule_fired("idle:charter_settle");
                return true;
            }
            // Wedge escalation: if all wander directions are unreachable
            // (bot is off-mesh — observed 2026-05-18 as 70+ same-src
            // NoPath fan-out on map 1 src=(394.9,-2083.7,9.6)), the wedge
            // tracker suppresses the rule for 30s. After the second
            // suppression the bot can't escape on its own; escalate to
            // a homebind teleport, which always lands on a navmesh-valid
            // poly (innkeepers don't sit off-mesh). Without this, an
            // off-mesh bot saturates the path system forever.
            if (ai.check_anchor_wedge("idle:wander",
                                       s.path_blocked_count(),
                                       s.published_at_ms(),
                                       /*threshold=*/ 5,
                                       /*max_window_ms=*/ 8000,
                                       /*cooldown_ms=*/ 30000))
            {
                // Wedged: wander has cycled through bearings fast (mmap gap /
                // off-mesh) and gotten nowhere. WALK-FIRST recovery — head for
                // the nearest known-good navmesh node instead of teleporting.
                // The move_to re-stamps last_move_to, so if even the node is
                // unreachable the unstick ladder (now fed by the movement goal)
                // escalates and its own ~5-min last-resort teleport is the
                // eventual backstop. We only teleport directly here when there
                // is NO known-good node within range — a genuinely isolated
                // off-mesh bot that would otherwise saturate the path system
                // forever.
                auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                    s.map_id(), s.raw().position.x, s.raw().position.y, 800.f);
                if (node)
                {
                    const float ndx = node->x - s.raw().position.x;
                    const float ndy = node->y - s.raw().position.y;
                    if (ndx * ndx + ndy * ndy > 25.f)
                    {
                        emit.move_to(node->x, node->y, node->z, /*run*/ true);
                        ai.set_last_rule_fired("idle:wander_wedge_escape");
                        return true;
                    }
                }
                // No known-good node within 800y — last resort: homebind
                // teleport so the bot isn't frozen off-mesh forever.
                const auto& tr = s.raw().travel;
                if (tr.homebind_map_id != 0)
                {
                    emit.emit(TeleportToIntent{
                        tr.homebind_map_id,
                        tr.homebind_x, tr.homebind_y, tr.homebind_z, 0.f});
                    ai.set_last_rule_fired("idle:wander_wedge_rescue");
                    return true;
                }
                // No homebind row — let the next rule try.
                return false;
            }
            const float kWanderStep =
                ai.personality().risk_tolerance == RiskTolerance::Cautious ? 15.0f :
                ai.personality().risk_tolerance == RiskTolerance::Careful  ? 20.0f :
                ai.personality().risk_tolerance == RiskTolerance::Reckless ? 35.0f :
                25.0f;     // Normal default
            const uint32 epoch    = s.published_at_ms() / 5000u;
            // Path-blocked salt: every Locked move_to from API::move_to
            // bumps ai.path_blocked_count(); mixing it in here means the
            // very next wander emit picks a different bearing instead of
            // waiting up to 5s for the epoch bucket to advance. Sustained
            // blocks (mmap gap) cycle through angles fast — the
            // check_anchor_wedge guard above escalates after the threshold.
            const uint64 mixed    = (uint64_t(s.bot_id()) * 0x9E3779B97F4A7C15ULL)
                                    ^ (uint64_t(epoch) * 0xBF58476D1CE4E5B9ULL)
                                    ^ (uint64_t(ai.path_blocked_count()) * 0x94D049BB133111EBULL);
            // Map [0, 2^16) → [0, 2π) for an angle without floating-point modulo.
            const float angle = (float(mixed & 0xFFFFu) / 65536.0f) * 6.2831853f;
            float tx = bx + std::cos(angle) * kWanderStep;
            float ty = by + std::sin(angle) * kWanderStep;
            // Stay anchored to the current quest POI when one exists.
            // Without this, the 5s-bucketed angle drifts the bot far from
            // its objective over time — and once outside the starter
            // zone, idle:engage_nearby_mob pulls it deeper into the next
            // zone (Northshire L2 bot bleeding south into Goldshire mobs).
            // Clamp to a 60y radius around the POI: still gives the bot
            // room to find mobs / nodes nearby, but the zone-leave drift
            // can't accumulate.
            if (s.has_current_objective() && s.current_objective_poi().valid &&
                s.current_objective_poi().map_id == s.map_id())
            {
                // Anchor radius scales with the POI polygon. Point POI →
                // 60y orbit. Zone POI → poi.radius + 30y, so the bot can
                // wander anywhere inside the zone plus a small slop.
                const auto& poi = s.current_objective_poi();
                const float kAnchorRadius   = (poi.radius > 1.f ? poi.radius + 30.f : 60.f);
                const float kAnchorRadiusSq = kAnchorRadius * kAnchorRadius;
                const float adx = tx - poi.x, ady = ty - poi.y;
                if (adx*adx + ady*ady > kAnchorRadiusSq)
                {
                    // Walk back toward POI instead. Keeps bot orbiting
                    // its objective rather than drifting away from it.
                    const float bdx = poi.x - bx, bdy = poi.y - by;
                    const float bdist = std::sqrt(bdx*bdx + bdy*bdy);
                    if (bdist > 1.f)
                    {
                        const float scale = std::min(kWanderStep, bdist) / bdist;
                        tx = bx + bdx * scale;
                        ty = by + bdy * scale;
                    }
                }
            }
            emit.move_to(tx, ty, bz, /*run*/ true);
            ai.set_last_rule_fired("idle:wander");
            return true;
        }
    return false;
}
// ---------- Legacy single-need vendor dispatcher (pass 18) ----------
// Body extracted verbatim from the inline legacy vendor cascade in
// DispatchIdle (originally line ~5961). These are the pre-FSM
// single-need rules (idle:repair, idle:buy_bag, idle:vendor_sell_trash,
// idle:cook_self_food, idle:buy_food, idle:buy_bandage, idle:buy_potion,
// idle:buy_reagent) kept as a fallback for vendor_visit FSM. Fires from
// the bottom-of-tick dispatch at priority 60 (above autoact at 50).
bool LegacyVendorDispatch(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit)
{
    (void)g;
    // Local kEffectiveRole — bandage / potion rules consult it. Same
    // computation as the inline DispatchIdle.
    const Role kEffectiveRole = ai.effective_role(s);
    (void)kEffectiveRole;

    // Local quest-action gate shared by the still-inline single-need vendor
    // rules below (legacy fast paths kept until they're migrated to the
    // registry in a later pass). Mirrors the gate used by VendorVisitGate.
    const bool has_quest_action_for_repair =
        !s.raw().quest_discovery.quest_turnins.empty() || !s.raw().quest_discovery.quest_offers.empty();

    // Auto-repair when at a repair NPC and gear durability dipped low.
    // Bit 0 of vendor_visit_phases_pending is set by the builder when:
    //   - lowest equipped durability < 70%, AND
    //   - bot.gold ≥ estimated_repair_cost × 1.2 (20% safety margin
    //     covers reputation-discount variance + any rounding).
    // Without the cost gate, a near-broke bot would re-emit RepairAll
    // every tick to a vendor that returns "not enough money" — wasting
    // ticks and (since the gear stays broken) the trigger condition
    // never resolves.
    // bit0 (0x01, critical <30%) OR bit5 (0x20, proactive <35%): once a repair
    // NPC is in interact range, repair regardless of which band triggered —
    // mirrors VendorRules phase-1. The <35%-doesn't-abandon-quests gating is in
    // the routing rules, not at the interact ring.
    if (!has_quest_action_for_repair && (s.vendor_visit_phases_pending() & (0x01 | 0x20)))
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_REPAIR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                // Per-NPC retry cooldown: the snapshot bit recomputes from
                // durability/gold every tick, so a partial repair (gold ran
                // out) keeps the bit set and the rule re-fires every tick
                // until the bot moves out of range or buys gear. The 5-min
                // ActionKind::Repair lockout absorbs partial / refused
                // repairs and lets lower-priority rules run.
                const uint32 rep_now_ms = GameTime::GetGameTimeMS();
                const uint64 rep_key = npc->guid.GetCounter();
                if (!ai.action_recently_tried(BotAI::ActionKind::Repair,
                                              rep_key, rep_now_ms))
                {
                    emit.emit(VendorIntent{RepairAllIntent{npc->guid, /*from_guild_bank*/ false}});
                    ai.note_action_retry(BotAI::ActionKind::Repair,
                                         rep_key, rep_now_ms);
                    ai.set_last_rule_fired("idle:repair");
                    return true;
                }
            }
        }
    }

    // Bag automation: when bot has an empty bag slot or a smallest bag
    // below tier 14 (Runecloth) AND a vendor is in interact range, buy
    // the largest bag whose vendor price ≤ gold/2. Phase bit 2 of
    // vendor_visit_phases_pending is set by the snapshot builder.
    //
    // Retry cooldown: many starter vendors don't stock bags; a 60s wait
    // per (npc) prevents this rule from dominating tick after tick when
    // the vendor in front of the bot doesn't carry the chosen entry.
    // The next vendor the bot reaches gets a fresh attempt.
    // Gate: every vendor / shopping rule below defers when the bot has any
    // quest-turnin or offer resolved nearby. Quest interactions are higher
    // priority than shopping; without this gate a bot parked next to a
    // quest giver who is also a general-goods vendor (e.g., Marshal McBride
    // / innkeeper combos in starter zones) loops on buy_food/buy_bag and
    // never closes the quest. The gate fires every tick and is cheap —
    // both lists are pre-resolved by the snapshot builder.
    const bool has_pending_quest_action =
        !s.raw().quest_discovery.quest_turnins.empty() || !s.raw().quest_discovery.quest_offers.empty();
    if (!has_pending_quest_action && (s.vendor_visit_phases_pending() & 0x04))
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            const uint32 now_ms = s.published_at_ms();
            constexpr uint32 kBagBuyCooldownMs = 60u * 1000u;
            const bool same_npc_recent =
                ai.last_bag_buy_npc() == npc->guid &&
                (now_ms - ai.last_bag_buy_ms()) < kBagBuyCooldownMs;
            if (!same_npc_recent)
            {
                float bx, by, bz; s.position(bx, by, bz);
                const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
                constexpr float kInteract = 5.0f;
                if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
                {
                    uint32 best_entry = 0;
                    const int32 gold = s.gold();
                    const uint8 cur_smallest = s.smallest_bag_capacity();
                    const bool has_empty = s.has_empty_bag_slot();
                    for (auto it = kBagSizeTable.rbegin(); it != kBagSizeTable.rend(); ++it)
                    {
                        if (uint64(gold) < uint64(it->approx_price) * 6 / 5) continue;
                        if (!has_empty && it->capacity <= cur_smallest) continue;
                        best_entry = it->item_entry;
                        break;
                    }
                    if (best_entry != 0)
                    {
                        emit.emit(VendorIntent{VendorBuyByEntryIntent{npc->guid, best_entry, /*count*/ 1}});
                        ai.note_bag_buy_try(npc->guid, now_ms);
                        ai.set_last_rule_fired("idle:buy_bag");
                        return true;
                    }
                }
            }
            // Recent failure or out of range — fall through to other rules
            // so the bot can quest, wander, etc.
        }
    }

    // Auto-vendor when bags are full and a vendor is in interact range.
    // Drains grey-quality trash via the existing sell_trash API. Threshold:
    // <= 2 free slots. Below this we stop accepting loot, so an immediate
    // sell unblocks future drops without owner intervention.
    if (!has_pending_quest_action && s.bag_free_slots() <= 2)
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                // 5-min retry cooldown — if the bot's bag is full of non-
                // trash items (quest items, soulbound greens, profession
                // mats), VendorSellTrash is a no-op, free_slots stays <=2,
                // and the rule re-fires every tick.
                const uint32 now_ms = GameTime::GetGameTimeMS();
                const uint64 npc_low = npc->guid.GetCounter();
                if (!ai.action_recently_tried(BotAI::ActionKind::SellTrash, npc_low, now_ms))
                {
                    emit.emit(VendorIntent{VendorSellTrashIntent{npc->guid}});
                    ai.note_action_retry(BotAI::ActionKind::SellTrash, npc_low, now_ms);
                    ai.set_last_rule_fired("idle:vendor_sell_trash");
                    return true;
                }
            }
        }
    }

    // (Auction rules — idle:ah_post_surplus / idle:ah_cancel_undercut —
    //  migrated to AuctionRules.cpp + IdleRuleRegistry as REFACTOR_3 pass 2.)

    // Auto-restock food/drink when bag count drops below the comfort
    // threshold and a vendor is in interact range. Buy a 20-stack bracket so
    // the bot has plenty before next departure. Gated on having gold to spend
    // (food costs are trivial but the rule shouldn't bankrupt a fresh bot).
    // Vendor category filter resolves the right item server-side; AI doesn't
    // need to know vendor inventory layout.
    constexpr uint16 kFoodComfortMin = 5;
    constexpr int32  kFoodMinGold    = 5000;   // 50 silver
    // Self-cook FIRST (no gold required — uses bag mats). Bot with Cooking
    // skill + non-Gray cooking recipe + reagents bypasses the vendor entirely.
    // Runs even when bot has 0 gold so a fresh L1 hunter with raw meat from
    // a wolf kill can still produce food.
    if (!has_pending_quest_action &&
        s.food_drink_count() < kFoodComfortMin &&
        s.has_skill(/*Cooking*/ 185) && !s.is_skill_capped(185) &&
        !s.raw().spellbook.known_recipes.empty() && !s.raw().movement.is_mounted &&
        s.bag_free_slots() >= 2)
    {
        for (uint32 spell_id : s.raw().spellbook.known_recipes)
        {
            RecipeMeta const* meta = FindRecipeMeta(spell_id);
            if (!meta || meta->skill_line_id != 185) continue;
            const RecipeColor c = ResolveRecipeColor(spell_id, s.skill_value(185));
            if (c == RecipeColor::Gray || c == RecipeColor::Unknown) continue;
            if (s.is_ready(spell_id) && s.has_reagents(spell_id))
            {
                if (emit.cast(spell_id))
                {
                    ai.set_last_rule_fired("idle:cook_self_food");
                    return true;
                }
            }
        }
    }
    if (!has_pending_quest_action &&
        s.food_drink_count() < kFoodComfortMin && s.gold() >= kFoodMinGold &&
        ai.food_buy_off_cooldown(GameTime::GetGameTimeMS()))
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                const uint32 buy_now_ms = GameTime::GetGameTimeMS();
                const uint64 npc_low = npc->guid.GetCounter();
                // ITEM_CLASS_CONSUMABLE=0, ITEM_SUBCLASS_FOOD_DRINK=5. One small
                // top-up; the 30-min food_buy cooldown (note_food_buy) stops the
                // pile-up that filled bags and drove the vendor oscillation.
                if (!ai.vendor_buy_recently_tried(npc_low, 5, buy_now_ms))
                {
                    emit.vendor_buy_category(npc->guid, 0, 5, 20);
                    ai.note_vendor_buy_try(npc_low, 5, buy_now_ms);
                    ai.note_food_buy(buy_now_ms);
                    ai.set_last_rule_fired("idle:buy_food");
                    return true;
                }
            }
        }
    }

    // Auto-restock bandages — useful for non-healers between pulls when
    // mana-classes have already drunk and HP-classes need to top off.
    // Threshold lower than potions; bandages stack large and we burn one per
    // pull at most. Only buy if the bot's class can benefit (no healer; they
    // have spells).
    constexpr uint16 kBandageComfortMin = 5;
    constexpr int32  kBandageMinGold    = 5000;     // 50 silver
    if (!has_pending_quest_action &&
        s.bandage_count() < kBandageComfortMin && s.gold() >= kBandageMinGold &&
        kEffectiveRole != Role::Healer)
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                const uint32 buy_now_ms = GameTime::GetGameTimeMS();
                const uint64 npc_low = npc->guid.GetCounter();
                // ITEM_CLASS_CONSUMABLE=0, ITEM_SUBCLASS_BANDAGE=7.
                // First Aid was removed in BfA 8.0; most modern vendors no
                // longer stock bandages — the cooldown prevents the rule
                // from re-emitting forever against vendors that will never
                // succeed.
                if (!ai.vendor_buy_recently_tried(npc_low, 7, buy_now_ms))
                {
                    emit.vendor_buy_category(npc->guid, 0, 7, 10);
                    ai.note_vendor_buy_try(npc_low, 7, buy_now_ms);
                    ai.set_last_rule_fired("idle:buy_bandage");
                    return true;
                }
            }
        }
    }

    // Auto-restock potions when below comfort threshold. Same pattern as
    // food but lower target stack — potions are pricier and the APL only
    // pops them on cooldown (~5min), so 5 is a healthy reserve. Same gold
    // gate so a bankrupt bot doesn't dig deeper.
    constexpr uint16 kPotionComfortMin = 3;
    constexpr int32  kPotionMinGold    = 50000;   // 5 silver — healing pots are cheap, mana pots scale
    if (!has_pending_quest_action &&
        s.potion_count() < kPotionComfortMin && s.gold() >= kPotionMinGold)
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                const uint32 buy_now_ms = GameTime::GetGameTimeMS();
                const uint64 npc_low = npc->guid.GetCounter();
                // ITEM_CLASS_CONSUMABLE=0, ITEM_SUBCLASS_POTION=1.
                if (!ai.vendor_buy_recently_tried(npc_low, 1, buy_now_ms))
                {
                    emit.vendor_buy_category(npc->guid, 0, 1, 5);
                    ai.note_vendor_buy_try(npc_low, 1, buy_now_ms);
                    ai.set_last_rule_fired("idle:buy_potion");
                    return true;
                }
            }
        }
    }

    // Auto-restock profession reagents when at vendor. Walks known recipes
    // for any non-Gray (skill-up potential), checks each Reagent[] entry
    // against bag stock, and buys the first missing reagent. Server rejects
    // gracefully (Result::ServerRefused) when the vendor doesn't carry the
    // item — wastes one intent slot per tick on no-op but no further harm.
    // Buys 5 stacks at a time so a few crafts can happen before re-restock.
    // Skipped when bot has < 1 gold (basic reagents are cheap but we don't
    // want to bankrupt fresh L1 bots).
    constexpr int32 kReagentBuyMinGold = 10000;   // 1 gold
    if (!has_pending_quest_action &&
        s.gold() >= kReagentBuyMinGold && !s.raw().spellbook.known_recipes.empty())
    {
        if (auto const* npc = s.nearest_npc_with_flag(UNIT_NPC_FLAG_VENDOR))
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = npc->x - bx, dy = npc->y - by, dz = npc->z - bz;
            constexpr float kInteract = 5.0f;
            if (dx*dx + dy*dy + dz*dz <= kInteract * kInteract)
            {
                // Per-reagent retry cooldown. Earlier this used a single-
                // slot cache (last_reagent_try_entry) which was leaky: with
                // 3 missing reagents X/Y/Z the bot rotated X→Y→Z→X every
                // tick because each new try overwrote the slot, so the
                // older reagents looked "not recent" again. Watchdog log
                // showed 365 fires of idle:buy_reagent in one session.
                // Switched to a proper per-entry map via the generic
                // ActionKind::ReagentBuy slot (5-min lockout). After one
                // failed attempt per reagent, the rule walks past it for
                // 5 min and tries the next reagent — or falls through to
                // other rules if all are on cooldown.
                const uint32 now_ms = s.published_at_ms();
                for (uint32 spell_id : s.raw().spellbook.known_recipes)
                {
                    RecipeMeta const* meta = FindRecipeMeta(spell_id);
                    if (!meta) continue;
                    if (s.is_skill_capped(meta->skill_line_id)) continue;
                    const RecipeColor c = ResolveRecipeColor(spell_id, s.skill_value(meta->skill_line_id));
                    if (c == RecipeColor::Gray || c == RecipeColor::Unknown) continue;
                    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
                    if (!si) continue;
                    bool fired = false;
                    for (size_t i = 0; i < si->Reagent.size(); ++i)
                    {
                        const int32 entry = si->Reagent[i];
                        const int16 need  = si->ReagentCount[i];
                        if (entry <= 0 || need <= 0) continue;
                        if (s.item_count(uint32(entry)) >= uint32(need)) continue;
                        if (ai.action_recently_tried(BotAI::ActionKind::ReagentBuy,
                                                     uint64(entry), now_ms))
                            continue;
                        emit.vendor_buy_by_entry(npc->guid, uint32(entry), 5);
                        ai.note_action_retry(BotAI::ActionKind::ReagentBuy,
                                             uint64(entry), now_ms);
                        ai.set_last_rule_fired("idle:buy_reagent");
                        fired = true;
                        break;
                    }
                    if (fired) return true;
                }
            }
        }
    }
    return false;
}





// (Anonymous namespace removed — Maintain* family is now externally
//  callable; private helpers above are `static` for internal linkage.)

// Idle - the bot is not in combat, not on a quest path, not in a group
// directing it. The job here is small: occasional emote, occasional path
// to an inn, otherwise do nothing. The cross-cutting layers (InGroup) and
// the activity tier system handle most "what next" decisions.

void DispatchIdle(BotAI& ai,
                  BotSnapshotView s,
                  GroupSnapshotView g,
                  BotIntentEmitter& emit)
{
    // Bot is dead → state machine has already transitioned (see BotAI::tick),
    // so we should not be in Idle at the same time. Defensive guard:
    if (!s.is_alive()) return;

    // ---- Taxi flight: stand down completely ----
    // While the core flies the bot on a taxi path (UNIT_STATE_IN_FLIGHT) the
    // FlightPathMovementGenerator OWNS movement. Running ANY idle rule now is
    // wrong and actively dangerous: watchdog_escape / unstick / wander would
    // emit a move_to, and the hearth/homebind rescue a teleport, either of
    // which aborts the flight mid-air and drops the bot in the wild (the
    // "fly_to_taxi → watchdog_escape" symptom). The bot needs no decisions in
    // transit — it simply arrives and re-plans from the new location next tick.
    if (s.raw().movement.is_in_taxi)
    {
        ai.set_last_rule_fired("idle:taxi_flight");
        return;
    }

    // Roll a fresh activity mode if the prior decision has expired (or the
    // bot just logged in — initial activity_mode_until_ms_=0 makes the
    // expiry check fire on the first tick). Mode persists for 30-60 min;
    // see BotAI::roll_activity_mode for probabilities and rationale.
    {
        const uint32 mode_now_ms = s.published_at_ms();
        if (ai.activity_mode_expired(mode_now_ms))
            ai.roll_activity_mode(mode_now_ms, ai.rng(), s.level());
    }

    // ---- BG / dungeon-run auto-activation — MUST run BEFORE the
    // priority>=700 dispatch (moved here, BG audit S2). These blocks only
    // mutate run-mode STATE and emit nothing, so running them first is safe;
    // doing so guarantees bg_active()/dungeon_active() is armed on the very
    // first in-instance tick and can NEVER be starved by an unthrottled
    // >=700 rule. Previously the >=700 tick ran first and return'd on any
    // fire, so a bot holding an un-abandonable quest (idle:quest_abandon_
    // unachievable, prio 720, no BG exclusion / throttle) re-fired every tick
    // and bg_run_mode stayed permanently Off → the whole tactical layer was
    // dead for the entire match. The transitions stay EDGE-TRIGGERED so a
    // /bgrun stop mid-match still sticks (it is not re-armed level-style).
    // ---- Dungeon-run mode auto-activation (Phase B5/I3) ----
    // When the bot is inside a dungeon AND is the group leader, the
    // dungeon-run mode auto-activates so the squad runs the dungeon
    // without an explicit /squadrun. Auto-deactivates when the bot
    // leaves the instance map (typical via portal or instance reset).
    //
    // Auto-activate rule: bot in dungeon/raid + leader == bot + mode==Off.
    // Auto-deactivate rule: bot NOT in dungeon/raid + mode!=Off.
    // Raids share the same DungeonScript registry by map_id, so the same
    // advice/rules system drives them.
    const bool in_instance = s.is_in_dungeon() || s.raw().instance_ctx.is_in_raid;
    if (in_instance)
    {
        if (ai.dungeon_run_mode() == BotAI::DungeonRunMode::Off)
        {
            // Leader check via the GroupSnapshotView. When the bot is
            // the group leader inside an instance, treat that as an
            // implicit "you're driving this run" handover.
            if (g.exists() && g.leader() == s.guid())
                ai.set_dungeon_run_mode(BotAI::DungeonRunMode::Active);
        }
        // LFG-group auto-run (2026-06-11): everyone who queued the dungeon
        // finder signed up for an auto-run — including NON-LEADER bots when
        // a human player holds lead (live: human + 4 bots in Deadmines,
        // bots idle at the entrance because only the leader path armed).
        // EDGE-TRIGGERED on the false→true transition of
        // (in_instance && group is LFG) so `/run stop` sticks for the rest
        // of the visit instead of re-arming every tick.
        //
        // Trigger on EITHER the group's LFG flag OR this bot's own LFG-queue
        // record. The group-flag path alone is INSUFFICIENT: a group the
        // finder forms from solo-queued bots is tracked as an active LFG
        // dungeon by LFGMgr (GroupsStore Dungeon=N, State=In dungeon) yet does
        // NOT reliably carry Group::GROUP_FLAG_LFG — observed live 2026-06-25
        // (dungeontest squad inside Deadmines: group kind=party, is_lfg()=
        // false). With only g.is_lfg(), just the LEADER armed (via the leader
        // check above); the non-leader DPS/healer never entered dungeon-run
        // mode, so DungeonDispatch (prio 720) was skipped for them, they fell
        // through to generic ingroup:follow_recall, and — crucially — the
        // DungeonDispatch hard-stop that suppresses world-idle questing inside
        // an instance never ran, so the quest picker walked them onto an
        // off-mesh entrance perch and the whole run wedged on the cohesion
        // gate. last_lfg_dungeon_id is set the instant a bot queues the
        // finder, so it arms run-mode reliably for non-leaders too, and it is
        // edge-triggered + in_instance-gated like the group-flag path. (It
        // persists post-run for the loop-requeue rule, but run-mode is forced
        // Off on leaving the instance below, so a stale id can only re-arm the
        // bot inside SOME dungeon — which is the desired auto-run anyway.)
        {
            // Third signal s.lfg_in_dungeon(): the LFGMgr per-player DUNGEON
            // state, surfaced into the snapshot on the world thread. It is the
            // ONLY one of the three that survives a worldserver restart — a
            // squad reloaded mid-run after a crash has g.is_lfg()==false (no
            // GROUP_FLAG_LFG on a finder-formed bot group) AND
            // last_lfg_dungeon_id reset to 0, so without this the non-leader
            // followers never re-armed dungeon-run mode and pinned on generic
            // follow_recall across the foundry off-mesh bridge (squad split,
            // 57 min frozen, 2026-06-25). See BotSnapshotBuilder lfg.in_dungeon.
            // Robust auto-run signal (2026-06-27): the three LFG signals each
            // desync per-bot — a finder-formed group lacks GROUP_FLAG_LFG
            // (g.is_lfg()==false), last_lfg_dungeon_id can reset to 0, and a bot's
            // LFGMgr per-player state can read != LFG_STATE_DUNGEON after a deferred
            // proposal accept. Live 2026-06-27: Dungmage hit all-three-false, never
            // armed dungeon-run mode, ran the open-world QUEST PICKER at the entrance
            // the entire run, and stranded the whole group on the cohesion gate (the
            // others held at d2rip 304 waiting for a member that was busy questing).
            // Any bot GROUPED inside a 5-man/raid instance is, by definition, on this
            // run: arm it (|| in_instance — true throughout this block). Still
            // edge-triggered (prev_lfg_dungeon_auto), so a transient is_in_dungeon
            // flicker self-heals (re-arms) yet a deliberate /run stop sticks (the edge
            // was already consumed while mode is forced Off).
            const bool lfg_auto = g.exists() &&
                                  (g.is_lfg() || ai.last_lfg_dungeon_id() != 0 ||
                                   s.lfg_in_dungeon() || in_instance);
            if (lfg_auto && !ai.prev_lfg_dungeon_auto() &&
                ai.dungeon_run_mode() == BotAI::DungeonRunMode::Off)
                ai.set_dungeon_run_mode(BotAI::DungeonRunMode::Active);
            ai.set_prev_lfg_dungeon_auto(lfg_auto);
        }
    }
    else
    {
        if (ai.dungeon_run_mode() != BotAI::DungeonRunMode::Off)
            ai.set_dungeon_run_mode(BotAI::DungeonRunMode::Off);
        if (ai.prev_lfg_dungeon_auto())
            ai.set_prev_lfg_dungeon_auto(false);
    }

    // ---- BG auto-activate / auto-deactivate ----
    // Mirrors the dungeon-run gate above. BGs have no group-leader
    // semantics (raid auto-formed by queue) so the auto-activate is
    // unconditional on entry — the bot starts driving objective play
    // as soon as the BG match begins. /bgrun stop overrides; the
    // override sticks until the bot leaves and re-enters.
    //
    // Auto-activate is EDGE-TRIGGERED on the false→true transition of
    // in_battleground() so that /bgrun stop flipping the mode to Off
    // mid-match doesn't get overwritten back to Active by the very
    // next tick inside the same BG. The previous "level-triggered"
    // version made /bgrun stop a no-op in practice (audit
    // a99fa148d689c9a5b finding #2).
    {
        const bool in_bg_now  = s.in_battleground();
        const bool in_bg_prev = ai.prev_in_battleground();
        if (in_bg_now && !in_bg_prev)
        {
            // Just entered a BG — auto-arm objective play. Owner can
            // still override with /bgrun stop after this tick; the
            // override holds until the bot exits and re-enters.
            ai.set_bg_run_mode(BotAI::BgRunMode::Active);
        }
        else if (!in_bg_now && in_bg_prev)
        {
            // Just left a BG — clear the run mode so a future entry
            // re-arms cleanly (and so /bgrun status reads Off outside).
            ai.set_bg_run_mode(BotAI::BgRunMode::Off);
        }
        ai.set_prev_in_battleground(in_bg_now);
    }

    // ---- Top-of-tick IdleRuleRegistry dispatch (REFACTOR_3 two-stage) ----
    // Runs rules registered at priority >= 700. This band holds the
    // preemption family: on_transport_wait, watchdog_escape, combat:opener,
    // owner_* manual overrides, survival hazards (flee_damaging_liquid /
    // surface_to_breathe / flee_hazard), swim_stuck, group_convert_to_raid,
    // acceptance family (group/guild invite / lfg proposal / bg port),
    // rejoin_group_instance, dungeon_loop_requeue, invite_nearby_player,
    // dungeon_dispatch, bg_dispatch. When any rule fires (non-empty return),
    // we consume the tick and return. (Runs AFTER the BG/dungeon auto-
    // activation above so the dispatch sees an armed run-mode on entry tick.)
    if (Services::Initialized())
    {
        const uint32 hi_now_ms = GameTime::GetGameTimeMS();
        if (!Services::IdleRules().tick(s, ai, g, emit, hi_now_ms, /*min_priority=*/700).empty())
            return;
    }

    // ---- Bottom-of-tick IdleRuleRegistry dispatch (REFACTOR_3) ----
    // Runs every rule registered at priority < 700 (the high-pri band
    // already had its chance via the top dispatch above). When any rule
    // fires (non-empty return), we consume the tick and return.
    //
    // All rules that used to live inline below this point — vendor FSM,
    // auction, mail, calendar, quest, bank, gathering, guild family,
    // trainer, hearth, hunter-pet, loot-drain, ambient, dungeon dispatcher,
    // bg dispatcher, autoact dispatcher, legacy single-need vendor cascade
    // — were migrated to per-subsystem rule files in REFACTOR_3 passes
    // 1-20. See `Bot/States/Rules/*.cpp` and `project_v2_idle_rules_refactor.md`
    // for the priority table and per-pass changelog.
    if (Services::Initialized())
    {
        const uint32 reg_now_ms = GameTime::GetGameTimeMS();
        if (!Services::IdleRules().tick(s, ai, g, emit, reg_now_ms).empty())
            return;
    }

    // ---- Periodic housekeeping (not a fire-rule) ----
    // Cap memory growth for long-lived bots that cycle through hundreds
    // of stuck quests. ~5min cadence; the prune walks a tiny map. Always
    // runs once the registry has had its chance to fire a rule.
    {
        constexpr uint32 kPruneIntervalMs = 5u * 60u * 1000u;
        const uint32 prune_now_ms = s.raw().published_at_ms;
        if (prune_now_ms - ai.last_blacklist_prune_ms() >= kPruneIntervalMs)
        {
            ai.prune_quest_blacklist(prune_now_ms);
            ai.prune_cast_emit_cache(prune_now_ms);
        }
    }

    // All idle behavior was registered with IdleRuleRegistry across
    // REFACTOR_3 passes 1-20. The remaining inline code in this function
    // is limited to the state-machine prologue: dispatch calls, dungeon/BG
    // mode auto-activate state transitions, the activity-mode roll, and
    // the periodic blacklist prune above. See
    // `project_v2_idle_rules_refactor.md` and
    // `src/modules/PlayerbotV2/docs/REFACTOR_3_IDLE_RULES_HANDOVER.md`
    // for the priority table and per-pass changelog.
}

} // namespace Playerbot::States
