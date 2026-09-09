// PlayerbotAPI.cpp
// First real-bodies pass: cast_spell / move_to / hearth wired to TrinityCore.
// Remaining commands listed in v2/API.md fill in incrementally.

#include "PlayerbotAPI.h"
#include "BotItemScorer.h"
#include "Player.h"
#include "Corpse.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "ObjectAccessor.h"
#include "Bag.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "DB2Structure.h"
#include "Creature.h"
#include "UnitDefines.h"
#include "Loot.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"  // sGuildMgr for accept_guild_invite()
#include "Map.h"  // InstanceResetMethod for reset_instances()
#include "Transport.h"  // server-side ship/zeppelin boarding (AddPassenger)
#include "PathGenerator.h"  // path validation in move_to
#include "HandcraftedRoadGraph.h"  // authored road centerline routing in move_to
#include "WaypointDefines.h"  // WaypointPath/WaypointNode for road MovePath
#include "MMapManager.h"   // endpoint-resolution diag in path_fail
#include "PhasingHandler.h"
#include "DetourNavMeshQuery.h"
#include "PlayerbotHooks.h"  // path-outcome telemetry forwarding
#include "PlayerbotMovement.h"  // unified bot-movement helpers
#include "SocialMgr.h"  // SOCIAL_FLAG_FRIEND/IGNORED for friend ops
#include "CalendarMgr.h"  // sCalendarMgr for calendar_rsvp_all_pending
#include "SpellAuraDefines.h"
#include "Pet.h"
#include "PetAI.h"
#include "CreatureAI.h"
#include "CharmInfo.h"
#include "GroupMgr.h"
#include "ChatPackets.h"
#include "Language.h"
#include "PartyPackets.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "DB2Stores.h"
#include "ReputationMgr.h"
#include "LFGMgr.h"
#include "LFG.h"
#include "Mail.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "DuelPackets.h"
#include "WorldSession.h"
#include "CharacterCache.h"
#include "WorldSession.h"
#include "Trainer.h"
#include "TaxiPathGraph.h"
#include "DB2Stores.h"
#include "AuctionHouseMgr.h"
#include "World.h"
#include "Util.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "Battleground.h"
#include "DisableMgr.h"
#include "TraitMgr.h"
#include "TraitPackets.h"
#include "UpdateFields.h"

#include <limits>
#include <algorithm>
#include <vector>
#include <utility>

namespace Playerbot {

// SEH-protected PathGenerator::CalculatePath lives in PlayerbotMovement.cpp
// (helper-namespace BotMovement). Bring it into local scope so existing
// callers below read naturally.
using BotMovement::SehSafeCalculatePath;

// Per-world-tick pathfinding budget (declared PlayerbotAPI.h). World-thread only,
// no synchronization. g_active gates the window: outside a Begin/End pair (e.g. a
// non-DrainIntents caller of move_to) HasBudget fails OPEN so nothing is throttled
// unexpectedly. See the header for the 2026-06-17 87s-hang rationale.
namespace PathBudget
{
    static bool   g_active      = false;
    static uint32 g_deadline_ms = 0;
    void BeginWorldTick(uint32 now_ms, uint32 budget_ms) { g_active = true; g_deadline_ms = now_ms + budget_ms; }
    void EndWorldTick() { g_active = false; }
    bool HasBudget(uint32 now_ms) { return !g_active || now_ms < g_deadline_ms; }
}

// Per-bot path-failure LRU. Quantize destinations to 5y buckets so wander
// angle jitter still re-attempts, but persistent dead-target spam doesn't
// burn Detour CalculatePath every tick. Stamp on NoPath /
// FarFromPolyEnd; early-return Result::Locked on the same bucket for
// `kPathFailTtlMs` after the stamp. Keyed (player guid low → ring of 4
// recent failures). Single-threaded access from the world tick so no lock.
namespace {
    constexpr size_t  kPathFailCap     = 16;     // was 4 — a bot thrashing between
                                                 // several dead targets evicted a
                                                 // persistent off-mesh dest before it
                                                 // could be suppressed (Falin/Varothel
                                                 // Dolanaar oscillation).
    constexpr uint32  kPathFailTtlMs     = 3000;  // transient blocks (wander jitter,
                                                  // momentary dynamic obstacle)
    constexpr uint32  kPathFailTtlLongMs = 60000; // PERSISTENT geometry failures:
                                                  // NoPath / FarFromPolyEnd. The
                                                  // destination is off-mesh (above
                                                  // terrain, inside a wall, a phantom
                                                  // POI Z) and will NOT become reachable
                                                  // in 3s — re-trying every 3s forever
                                                  // is what made bots run back and forth.
                                                  // Suppress long so goal-selection
                                                  // commits to a reachable objective.
    struct PathFailEntry { uint64 key; uint32 ms; uint32 ttl; };
    struct PathFailRing
    {
        std::array<PathFailEntry, kPathFailCap> entries{};
        size_t head = 0;
    };
    std::unordered_map<uint64, PathFailRing> g_path_fail{};

    bool PathFailedRecently(Player const* p, uint64 dest_key, uint32 now_ms)
    {
        if (!p) return false;
        auto it = g_path_fail.find(p->GetGUID().GetCounter());
        if (it == g_path_fail.end()) return false;
        for (auto const& e : it->second.entries)
            if (e.key == dest_key && e.ms != 0
                && (now_ms - e.ms) < (e.ttl ? e.ttl : kPathFailTtlMs))
                return true;
        return false;
    }

    void NotePathFail(Player const* p, uint64 dest_key, uint32 now_ms,
                      uint32 ttl_ms = kPathFailTtlMs)
    {
        if (!p) return;
        auto& ring = g_path_fail[p->GetGUID().GetCounter()];
        ring.entries[ring.head] = {dest_key, now_ms, ttl_ms};
        ring.head = (ring.head + 1) % kPathFailCap;
    }

    inline uint64 PathDestKey(uint32 map_id, float x, float y)
    {
        // 5y buckets — angle jitter within ~5y still re-tries; persistent
        // same-spot pushes hit the same bucket. map_id mixed into the high
        // bits so cross-map (x,y) collisions (e.g. (0,0) on every map)
        // don't false-block. 16 bits each for qx/qy is plenty given world
        // bounds (±17000y / 5 = ±3400, fits in i16).
        constexpr float kBucket = 5.0f;
        int32 qx = static_cast<int32>(std::floor(x / kBucket));
        int32 qy = static_cast<int32>(std::floor(y / kBucket));
        return (uint64(map_id & 0xFFFFu) << 32)
             | (uint64(uint16(int16(qx))) << 16)
             | uint64(uint16(int16(qy)));
    }

    // Active travel-MovePath goal per bot (guid_low -> PathDestKey). When move_to
    // drives a chunked route as a WAYPOINT spline, the goal is recorded here; a
    // later move_to to the SAME goal while the bot is still walking that spline is
    // skipped (no rebuild/restart) so winding routes don't oscillate. A move_to to
    // a DIFFERENT goal (repair / flee / gather redirect) does NOT match, so it
    // re-paths immediately. World-thread only (mirrors g_path_fail).
    std::unordered_map<uint64, uint64> g_movePathGoal{};

    // Progress probe for the same-goal skip's STALL-BREAKER. The same-goal skip
    // (above) intentionally lets an active spline run without re-issuing, so a
    // winding/chunked route isn't restart-thrashed. But a spline can STALL with
    // its generator still "live" — e.g. a WaypointMovementGenerator that can't
    // navigate a long water segment or a water↔land shoreline seam (live: AV
    // pushers frozen in the river z~10 mid-valley; IoC Workshop attackers frozen
    // in the moat 151y short, both on COMPLETE mmap_probe paths). The plain skip
    // then freezes the bot forever AND keeps returning Ok, which resets the
    // wedge detector so no recovery ever fires. This records the last position
    // where the bot was seen MAKING PROGRESS toward the current goal (+ when):
    // if it sits within ~4y of that mark for >3s the spline is genuinely stuck
    // and the skip is broken (re-issue / re-path). 3s is well past the ~1s
    // WaypointMovementGenerator init delay, so it never trips the start gap the
    // skip was built to tolerate. World-thread only.
    struct MoveProbe { uint64 key = 0; float x = 0.f; float y = 0.f; uint32 ms = 0; };
    std::unordered_map<uint64, MoveProbe> g_movePathProbe{};

    // Local-minimum ESCAPE tracker (2026-06-20). A bot in a confined/winding
    // spot (building interior, terrain pocket) can have its only route OUT wind
    // past the 74-poly/~292y path cap before the route turns toward a far goal,
    // so the reachable partial ends a touch FARTHER in straight-line
    // (forward_progress<0) — yet walking it UNLOCKS an advancing path from the
    // new pose (live: Durnan, indoors z505, partial nets 115y to (-4986,-954)
    // [−47y to goal], but from THERE the next partial advances +372y). The
    // non-advancing branch otherwise refuses → bot frozen forever (154 blocks).
    // We FOLLOW such a partial to escape, bounded per (bot,goal) so a true tiny
    // pocket can't ping-pong forever: `crawls` counts consecutive non-advancing
    // escape steps; it resets when the goal changes OR the bot has genuinely
    // closed in (best_cur_to_dst improved), so real multi-pocket progress keeps
    // refreshing the budget while a dead-end exhausts it and falls through to the
    // normal refuse (wedge-watchdog / wander / dst-LRU then engage). World-thread
    // only (mirrors g_path_fail).
    struct EscapeCrawl { uint64 goal_key = 0; float best_cur_to_dst = 1e9f; uint32 crawls = 0; };
    std::unordered_map<uint64, EscapeCrawl> g_escape_crawl{};
    constexpr uint32 kMaxEscapeCrawls = 8u;       // bounds ping-pong in a true pocket
    constexpr float  kMaxEscapeClimbYards = 8.0f; // no elevator/ramp-UP stranding (the
                                                  // rolled-back crawl's failure mode)
    constexpr float  kMinEscapeLegYards   = 15.0f;// a real escape leg, not jitter

    // Per-bot equip-refusal backoff. EquipUpgradeFire (V2 MaintainRules) re-fires
    // equip_item every eligible tick while the candidate is still in the bag.
    // When the server REFUSES the swap (CanEquipItem != OK — e.g. the loadout
    // would break a 2H/dual-wield rule, item is level-gated, slot can't be
    // unequipped), the item stays in the bag and the rule retries forever,
    // burning a CanEquipItem probe every tick AND making /diag believe an equip
    // is perpetually "pending". Record refused (item-entry → dest-slot) pairs
    // with a long TTL so the same hopeless swap is skipped until the situation
    // can plausibly change (level-up, gear swap, respec all far longer than the
    // backoff). Keyed by player-guid-low like the path-fail ring.
    constexpr uint32  kEquipFailTtlMs = 60000;   // 60s — far longer than a tick
    // Per-bot map of refused (item-entry,dest-slot) key -> failure timestamp.
    // Was a fixed 6-slot FIFO ring (kEquipFailCap) whose eviction re-enabled the
    // OLDEST hopeless swap once a bot carried >6 distinct un-equippable items —
    // the round-robin then out-paced the rule-side lockout, producing the
    // "EquipItem|ServerRefused every few seconds forever" livelock (freeze-dump
    // signature). An unbounded-per-key map keeps EVERY distinct refused key
    // suppressed the full TTL regardless of bag count; expired keys are GC'd
    // opportunistically on insert so the inner map stays bounded by "distinct
    // items refused in the last 60s" (small). World-thread only (equip executes
    // via the intent drain on the world thread), exactly like the prior ring —
    // no lock needed.
    std::unordered_map<uint64, std::unordered_map<uint32, uint32>> g_equip_fail{};

    // Key folds the source item-entry (24 low bits — entries fit) with the
    // destination equipment slot (8 high bits) so "this item into this slot"
    // is the unit of backoff. The same item refused into a different slot, or a
    // different item into the same slot, still gets a fresh attempt.
    inline uint32 EquipFailKey(uint32 item_entry, uint8 to_slot)
    {
        return (uint32(to_slot) << 24) | (item_entry & 0x00FFFFFFu);
    }

    bool EquipFailedRecently(Player const* p, uint32 key, uint32 now_ms)
    {
        if (!p) return false;
        auto it = g_equip_fail.find(p->GetGUID().GetCounter());
        if (it == g_equip_fail.end()) return false;
        auto kit = it->second.find(key);
        return kit != it->second.end() && (now_ms - kit->second) < kEquipFailTtlMs;
    }

    void NoteEquipFail(Player const* p, uint32 key, uint32 now_ms)
    {
        if (!p) return;
        auto& m = g_equip_fail[p->GetGUID().GetCounter()];
        // Opportunistic GC so the per-bot map stays bounded by distinct items
        // refused within the TTL window.
        for (auto it = m.begin(); it != m.end(); )
            it = ((now_ms - it->second) >= kEquipFailTtlMs) ? m.erase(it) : std::next(it);
        m[key] = now_ms;
    }
} // anonymous

API::API(Player* p) : p_(p) {}

// ---- Identity (snapshot-time) --------------------------------------------

ObjectGuid API::guid() const
{
    return p_ ? p_->GetGUID() : ObjectGuid::Empty;
}

std::string API::name() const
{
    return p_ ? p_->GetName() : std::string{};
}

uint8 API::level() const
{
    return p_ ? p_->GetLevel() : uint8(0);
}

uint8 API::cls() const
{
    return p_ ? p_->GetClass() : uint8(0);
}

uint8 API::race() const
{
    return p_ ? p_->GetRace() : uint8(0);
}

uint8 API::spec() const
{
    // Per v2/API.md, this returns the active spec index. Real implementation
    // reads the active talent loadout; bootstrap returns 0.
    return 0;
}

uint32 API::faction() const
{
    return p_ ? p_->GetFaction() : uint32(0);
}

// ---- Vitals --------------------------------------------------------------

int32 API::hp() const
{
    return p_ ? int32(p_->GetHealth()) : 0;
}

int32 API::max_hp() const
{
    return p_ ? int32(p_->GetMaxHealth()) : 0;
}

bool API::is_in_combat() const
{
    return p_ && p_->IsInCombat();
}

bool API::is_alive() const
{
    return p_ && p_->IsAlive();
}

// ---- Commands (real bodies) ----------------------------------------------

Result API::cast_spell(uint32 spell_id, ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (!spell_id) return Result::NotKnown;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, p_->GetMap()->GetDifficultyID());
    if (!info)
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} target {} rejected: SpellInfo missing for difficulty {}",
            spell_id, target.ToString(), uint32(p_->GetMap()->GetDifficultyID()));
        return Result::NotKnown;
    }

    if (!p_->HasSpell(spell_id) && !info->HasAttribute(SPELL_ATTR0_PASSIVE))
    {
        // The bot's spellbook doesn't contain this spell. Most common cause for
        // a freshly logged-in bot: class-default spells didn't get added during
        // login (BotFactory / character creation skipped LearnDefaultSkills).
        // The APL keeps re-firing the rule because it has no way to know the
        // spell isn't actually learned, so this surfaces as "Casting=0" with
        // LastRule pointing at a real ability.
        char const* sname = info->SpellName ? (*info->SpellName)[sWorld->GetDefaultDbcLocale()] : "?";
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} ({}) target {} rejected: not in spellbook",
            spell_id, sname && *sname ? sname : "?", target.ToString());
        return Result::NotKnown;
    }

    Unit* tu = nullptr;
    if (!target.IsEmpty() && target != p_->GetGUID())
    {
        tu = ObjectAccessor::GetUnit(*p_, target);
        if (!tu) return Result::InvalidTarget;
        // Pre-cast target validation (R3, 2026-06-03). APL rules pick a
        // target from the snapshot, which can be one cadence stale: the
        // member may have changed map, phased out, or just disconnected.
        // Casting at it then fails server-side with SPELL_FAILED_BAD_TARGETS
        // (error 91) — the dominant cause of a 72%-cast-failure / 32K-reject
        // storm observed live (heals/totems firing at a no-longer-valid
        // unit). Reject here so we never emit the doomed cast.
        if (tu->GetMapId() != p_->GetMapId() || !p_->InSamePhase(tu))
        {
            TC_LOG_DEBUG("playerbot.api",
                "[API::cast_spell] spell {} target {} rejected: different map/phase",
                spell_id, target.ToString());
            return Result::InvalidTarget;
        }
    }
    else
    {
        tu = p_;
    }

    // Reject offensive casts on a dead (or about-to-despawn) target.
    // Audit 2026-05-22: all 14 caster specs gate only on
    // HasLiveTarget() (target guid present), not on the target's HP
    // — so when a mob dies mid-cast-emit, the rotation queues a fresh
    // 2.5s Frostbolt that resolves with SPELL_FAILED_TARGETS_DEAD,
    // wasting the GCD and mana. Centralize the dead-target gate here
    // so every spec inherits it, except rez-style spells that
    // legitimately target dead units (Resurrection / Revive Pet /
    // Soulstone Rez). IsAllowingDeadTarget() captures the rez allowlist
    // via SPELL_ATTR2_ALLOW_DEAD_TARGET on the spell itself.
    if (tu != p_ && !tu->IsAlive() && !info->IsAllowingDeadTarget())
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} target {} rejected: target dead",
            spell_id, target.ToString());
        return Result::InvalidTarget;
    }

    // Reject offensive casts on a target carrying a Spell Reflect
    // aura. Audit 2026-05-22: no caster spec checks this — bots
    // happily chain Frostbolts into a Warrior with Spell Reflection
    // up and absorb their own damage. Targeted spells (info->IsTargetingArea
    // == false) on a HOSTILE target with a reflect aura should be
    // skipped — let the rotation pick a different spell (instant /
    // melee) or the spec's normal "no target" fallback.
    // Spell-reflect aura ids to detect:
    //   23920 Spell Reflection (Warrior; 5s reflect-all-magic buff)
    //   213915 Mass Spell Reflection (Warrior PvP talent variant)
    //   48707 Anti-Magic Shell (DK; absorb + reflect on full pool)
    //   216890 Improved Spell Reflection (Warrior)
    // Mob versions (Reflective Shield variants) trigger the same
    // SPELL_AURA_REFLECT_SPELLS aura type, so the type check below
    // catches everything without needing an ID allowlist.
    if (tu != p_ && p_->IsValidAttackTarget(tu, info) &&
        !info->IsPositive() &&
        tu->HasAuraType(SPELL_AURA_REFLECT_SPELLS))
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} target {} rejected: target has Spell Reflect aura",
            spell_id, target.ToString());
        return Result::InvalidTarget;
    }

    // Stun gate. Stuns block EVERY cast (physical + spell). The
    // snapshot exposes is_stunned() but APL predicates don't gate
    // consistently. Reject early so a stunned bot doesn't spam
    // SPELL_FAILED_STUNNED until the stun fades.
    if (p_->HasAuraType(SPELL_AURA_MOD_STUN))
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} rejected: stunned",
            spell_id);
        return Result::Locked;
    }

    // Silence gate. The snapshot already exposes is_silenced() and
    // can_cast(), but APL predicates across all 39 specs don't
    // consistently gate. Centralize at the API level so a silenced
    // bot doesn't spam SPELL_FAILED_SILENCED. Exception: melee abilities
    // and self-buff spells aren't silenced — only spells in a school
    // the silence covers. Use the same check TC's CheckCast performs:
    // `HasAuraType(SPELL_AURA_MOD_SILENCE)` blocks all schools, while
    // `HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE)` blocks both melee
    // and casts. Physical-only spells (`info->GetSchoolMask() ==
    // SPELL_SCHOOL_MASK_NORMAL`) skip the silence gate.
    if (info->GetSchoolMask() != SPELL_SCHOOL_MASK_NORMAL &&
        (p_->HasAuraType(SPELL_AURA_MOD_SILENCE) ||
         p_->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE)))
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::cast_spell] spell {} rejected: silenced",
            spell_id);
        return Result::Locked;
    }

    // Defensive range gate. Belt-and-suspenders for any rule whose target
    // picker selects a unit far out of range — observed: group-buff rules
    // casting Fortitude/Intellect/Soulstone at members 200-4800y away on a
    // shared continent map_id (~287k rejected casts in a 300MB log window).
    // The core rejects these in Spell::CheckRange anyway, but only after
    // building the Spell object and emitting a SpellCastResult we then log;
    // at fleet scale that's pure waste. Reject early when the target is
    // clearly beyond reach. The +8y slack (combat reach + target size)
    // keeps us from ever pre-empting a cast the core would have accepted at
    // the margin, so combat behaviour can't regress. GetMaxRange(positive,
    // caster) folds in range-modifying auras for an accurate ceiling.
    if (tu != p_)
    {
        float const max_range = info->GetMaxRange(info->IsPositive(), p_);
        float const tdist = p_->GetDistance(tu);
        if (max_range > 0.f && tdist > max_range + 8.0f)
        {
            TC_LOG_DEBUG("playerbot.api",
                "[API::cast_spell] spell {} target {} ({:.1f}y) skipped: beyond range {:.1f}y",
                spell_id, target.ToString(), tdist, max_range);
            return Result::OutOfRange;
        }
    }

    // Face the target if needed — melee/cone abilities fail with
    // SPELL_FAILED_UNIT_NOT_INFRONT otherwise, since AI never sets facing
    // outside of in-progress melee swings.
    if (tu != p_ && !p_->HasInArc(static_cast<float>(M_PI), tu))
        p_->SetFacingToObject(tu, true);

    // Movement spells (Charge/Leap/Jump) pathfind SYNCHRONOUSLY in core
    // Spell::CheckCast / effect handlers — the 2026-06-17 87s world-thread hang
    // was exactly a Warrior Charge's PathGenerator here, unguarded by the module.
    // Gate ONLY these on the per-tick pathfinding budget (normal rotation casts
    // never pathfind, so they are never throttled). Return Result::Locked
    // EXPLICITLY rather than letting the cast run / fall through to ServerRefused
    // (which would 10s-backoff the spell); the rule re-attempts next tick.
    if (info && !PathBudget::HasBudget(GameTime::GetGameTimeMS()) &&
        (info->HasEffect(SPELL_EFFECT_CHARGE)      || info->HasEffect(SPELL_EFFECT_CHARGE_DEST) ||
         info->HasEffect(SPELL_EFFECT_JUMP)        || info->HasEffect(SPELL_EFFECT_JUMP_DEST)   ||
         info->HasEffect(SPELL_EFFECT_JUMP_DEST_2) || info->HasEffect(SPELL_EFFECT_JUMP_CHARGE)  ||
         info->HasEffect(SPELL_EFFECT_LEAP)        || info->HasEffect(SPELL_EFFECT_LEAP_BACK)))
        return Result::Locked;

    SpellCastResult sr = p_->CastSpell(tu, spell_id, false);
    if (sr != SPELL_CAST_OK)
    {
        // Throttled diagnostic: log the first failure per (spell, reason) combo
        // so the operator can see WHY a rule is firing but the cast never lands
        // (out of range, LOS, moving, etc). Without this the bot looks "stuck"
        // in inspect: LastRule says e.g. "Frostbolt (filler)" because the AI
        // picked it, but Casting=0 because the world thread rejected it.
        // For BAD_TARGETS we additionally dump the relationship breakdown so
        // we can tell apart visibility failures, faction friendliness, At-War
        // flag misses, and immune-to-PC flags — they all surface as the same
        // SpellCastResult but have different fixes.
        const float dist = tu != p_ ? p_->GetDistance(tu) : 0.f;
        if (sr == SPELL_FAILED_BAD_TARGETS && tu != p_)
        {
            // Faction / friendliness / hostility / immunity locals were
            // historically captured here but the log line below only consumes
            // canSee + validAttack + visibility/phase/stealth diagnostics.
            // The MSVC C4189 spam (8 unused locals × every TU compile) hid
            // real diagnostics — keep only what the log uses.
            bool canSee = p_->CanSeeOrDetect(tu);
            bool validAttack = p_->IsValidAttackTarget(tu, info);
            // Decompose CanSeeOrDetect: phase + map already verified equal in
            // the previous iteration. Now drill down into the remaining branches
            // — never-visible, private-object ownership, ghost/GM bitmask checks,
            // detect-stealth/invis — to identify which one rejects the worg.
            bool sameMap = p_->GetMap() == tu->GetMap();
            bool samePhase = WorldObject::InSamePhase(p_, tu);
            bool tgtInWorld = tu->IsInWorld();
            bool tgtPrivate = tu->IsPrivateObject();
            bool privateOwnerOk = tu->CheckPrivateObjectOwnerVisibility(p_);
            uint32 tgtVisGM = tu->m_serverSideVisibility.GetValue(SERVERSIDE_VISIBILITY_GM);
            uint32 selfDetGM = p_->m_serverSideVisibilityDetect.GetValue(SERVERSIDE_VISIBILITY_GM);
            uint32 tgtVisGhost = tu->m_serverSideVisibility.GetValue(SERVERSIDE_VISIBILITY_GHOST);
            uint32 selfDetGhost = p_->m_serverSideVisibilityDetect.GetValue(SERVERSIDE_VISIBILITY_GHOST);
            uint32 selfStealthDet = p_->m_stealthDetect.GetFlags();
            uint32 tgtStealthFlags = tu->m_stealth.GetFlags();
            uint32 tgtInvisFlags = uint32(tu->m_invisibility.GetFlags() & 0xFFFFFFFFu);
            TC_LOG_DEBUG("playerbot.api",
                "[API::cast_spell] spell {} target {} ({:.1f}y) BAD_TARGETS: "
                "canSee={} validAttack={} | sameMap={} samePhase={} tgtInWorld={} "
                "tgtPrivate={} privateOwnerOk={} | tgtVisGM={} selfDetGM={} "
                "tgtVisGhost={} selfDetGhost={} | tgtStealthFlags=0x{:x} "
                "selfStealthDet=0x{:x} tgtInvisFlags=0x{:x}",
                spell_id, target.ToString(), dist,
                canSee, validAttack, sameMap, samePhase, tgtInWorld,
                tgtPrivate, privateOwnerOk, tgtVisGM, selfDetGM,
                tgtVisGhost, selfDetGhost, tgtStealthFlags, selfStealthDet,
                tgtInvisFlags);
        }
        else
        {
            TC_LOG_DEBUG("playerbot.api",
                "[API::cast_spell] spell {} target {} ({:.1f}y, alive={}) rejected: SpellCastResult={}",
                spell_id, target.ToString(), dist, tu->IsAlive(), uint32(sr));
        }
    }
    switch (sr)
    {
        case SPELL_CAST_OK:                  return Result::Ok;
        case SPELL_FAILED_OUT_OF_RANGE:      return Result::OutOfRange;
        case SPELL_FAILED_LINE_OF_SIGHT:     return Result::OutOfRange;
        case SPELL_FAILED_BAD_TARGETS:       return Result::InvalidTarget;
        case SPELL_FAILED_BAD_IMPLICIT_TARGETS: return Result::InvalidTarget;
        case SPELL_FAILED_TARGETS_DEAD:      return Result::InvalidTarget;
        case SPELL_FAILED_NO_POWER:          return Result::NotEnoughResource;
        case SPELL_FAILED_NOT_READY:         return Result::NotReady;
        case SPELL_FAILED_TRY_AGAIN:         return Result::NotReady;
        // SPELL_FAILED_CASTER_AURASTATE: caster fails the spell's aurastate
        // prereq (typical bot cases: warlock Create Healthstone refused
        // because one already exists, Soulstone already up on a target, a
        // proc-gated spell with no proc active). These persist far longer
        // than a GCD — minutes, or until the underlying state cycles.
        // Treat as persistent so the 10s back-off in BotIntentExecutor
        // kicks in, instead of hammering the rule every 1.5s GCD tick.
        case SPELL_FAILED_CASTER_AURASTATE:  return Result::ServerRefused;
        case SPELL_FAILED_MOVING:            return Result::Locked;
        case SPELL_FAILED_INTERRUPTED:       return Result::Locked;
        case SPELL_FAILED_SPELL_IN_PROGRESS: return Result::Locked;
        case SPELL_FAILED_DONT_REPORT:       return Result::Other;
        default:                             return Result::ServerRefused;
    }
}

Result API::cast_spell_on_item(uint32 spell_id, ObjectGuid item_guid)
{
    if (!p_) return Result::Other;
    if (!spell_id) return Result::NotKnown;
    if (item_guid.IsEmpty()) return Result::InvalidTarget;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, p_->GetMap()->GetDifficultyID());
    if (!info) return Result::NotKnown;
    if (!p_->HasSpell(spell_id) && !info->HasAttribute(SPELL_ATTR0_PASSIVE))
        return Result::NotKnown;

    Item* item = p_->GetItemByGuid(item_guid);
    if (!item) return Result::InvalidTarget;

    SpellCastResult sr = p_->CastSpell(item, spell_id, false);
    switch (sr)
    {
        case SPELL_CAST_OK:                  return Result::Ok;
        case SPELL_FAILED_BAD_TARGETS:       return Result::InvalidTarget;
        case SPELL_FAILED_BAD_IMPLICIT_TARGETS: return Result::InvalidTarget;
        case SPELL_FAILED_NO_POWER:          return Result::NotEnoughResource;
        case SPELL_FAILED_NOT_READY:         return Result::NotReady;
        case SPELL_FAILED_TRY_AGAIN:         return Result::NotReady;
        case SPELL_FAILED_MOVING:            return Result::Locked;
        case SPELL_FAILED_INTERRUPTED:       return Result::Locked;
        case SPELL_FAILED_SPELL_IN_PROGRESS: return Result::Locked;
        default:                             return Result::ServerRefused;
    }
}

Result API::cast_spell_at_position(uint32 spell_id, float x, float y, float z)
{
    if (!p_) return Result::Other;
    if (!spell_id) return Result::NotKnown;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, p_->GetMap()->GetDifficultyID());
    if (!info) return Result::NotKnown;
    if (!p_->HasSpell(spell_id) && !info->HasAttribute(SPELL_ATTR0_PASSIVE))
        return Result::NotKnown;

    // Gate movement ground-casts (Heroic Leap etc.) on the per-tick pathfinding
    // budget — they pathfind synchronously in core like targeted Charge (see
    // cast_spell). AoE ground-casts have no movement effect and are never gated.
    if (!PathBudget::HasBudget(GameTime::GetGameTimeMS()) &&
        (info->HasEffect(SPELL_EFFECT_CHARGE)      || info->HasEffect(SPELL_EFFECT_CHARGE_DEST) ||
         info->HasEffect(SPELL_EFFECT_JUMP)        || info->HasEffect(SPELL_EFFECT_JUMP_DEST)   ||
         info->HasEffect(SPELL_EFFECT_JUMP_DEST_2) || info->HasEffect(SPELL_EFFECT_JUMP_CHARGE)  ||
         info->HasEffect(SPELL_EFFECT_LEAP)        || info->HasEffect(SPELL_EFFECT_LEAP_BACK)))
        return Result::Locked;

    SpellCastResult sr = p_->CastSpell(Position{x, y, z}, spell_id, false);
    switch (sr)
    {
        case SPELL_CAST_OK:                  return Result::Ok;
        case SPELL_FAILED_OUT_OF_RANGE:      return Result::OutOfRange;
        case SPELL_FAILED_LINE_OF_SIGHT:     return Result::OutOfRange;
        case SPELL_FAILED_BAD_TARGETS:       return Result::InvalidTarget;
        case SPELL_FAILED_BAD_IMPLICIT_TARGETS: return Result::InvalidTarget;
        case SPELL_FAILED_TARGETS_DEAD:      return Result::InvalidTarget;
        case SPELL_FAILED_NO_POWER:          return Result::NotEnoughResource;
        case SPELL_FAILED_NOT_READY:         return Result::NotReady;
        case SPELL_FAILED_TRY_AGAIN:         return Result::NotReady;
        // SPELL_FAILED_CASTER_AURASTATE: caster fails the spell's aurastate
        // prereq (typical bot cases: warlock Create Healthstone refused
        // because one already exists, Soulstone already up on a target, a
        // proc-gated spell with no proc active). These persist far longer
        // than a GCD — minutes, or until the underlying state cycles.
        // Treat as persistent so the 10s back-off in BotIntentExecutor
        // kicks in, instead of hammering the rule every 1.5s GCD tick.
        case SPELL_FAILED_CASTER_AURASTATE:  return Result::ServerRefused;
        case SPELL_FAILED_MOVING:            return Result::Locked;
        case SPELL_FAILED_INTERRUPTED:       return Result::Locked;
        case SPELL_FAILED_SPELL_IN_PROGRESS: return Result::Locked;
        case SPELL_FAILED_DONT_REPORT:       return Result::Other;
        default:                             return Result::ServerRefused;
    }
}

Result API::move_to(float x, float y, float z, bool run, bool direct)
{
    if (!p_) return Result::Other;

    // ── VEHICLE DRIVING ──
    // When the bot CONTROLS a vehicle (IoC siege engine / demolisher / glaive
    // thrower; SoTA/WG engines), it is a SEATED passenger of the vehicle base.
    // The player's own MotionMaster can't move it — and worse, a seated bot
    // trips UNIT_STATE_NOT_MOVE / IsMovementPreventedByCasting below and bails
    // Result::Locked, so the vehicle never drives (live: IoC demolishers mounted
    // but frozen at the Workshop pad, [move_blocked] idle:bg_vehicle_drive_to_gate,
    // fire_gate=0). Drive the VEHICLE BASE's MotionMaster instead so the vehicle
    // (a Creature) pathfinds to the destination. Gated to the seat we actually
    // CONTROL (charmer == us) so a non-driver passenger never hijacks the base.
    if (Unit* vbase = p_->GetVehicleBase())
    {
        if (vbase != p_ && vbase->GetCharmerGUID() == p_->GetGUID())
        {
            if (!vbase->IsAlive())
                return Result::Locked;
            // Snap the destination to walkable ground (the dst is often a gate /
            // boss anchor a few yards off the mesh) and let the vehicle's own
            // PathGenerator spline the route. generatePath=true so it routes the
            // navmesh rather than a straight line into geometry. Snap uses p_ for
            // the map/phase (identical to the vehicle base's).
            BotMovement::SnapToGround(p_, x, y, z);
            vbase->GetMotionMaster()->MovePoint(0, x, y, z, /*generatePath*/ true);
            return Result::Ok;
        }
    }

    // Allow ghosts: corpse-run requires the dead bot to walk from the
    // graveyard back to the death point. Players walk freely while ghosted;
    // the only unalive state we should reject here is the loading/in-transit
    // window where Player::IsBeingTeleported is true. PLAYER_FLAGS_GHOST
    // marks the corpse-run state; allow movement in it.
    if (!p_->IsAlive() && !p_->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return Result::Locked;

    // Movement-prevented precheck. PointMovementGenerator::Initialize sets
    // MOVEMENTGENERATOR_FLAG_INTERRUPTED + StopMoving when the unit has
    // UNIT_STATE_NOT_MOVE (root|stunned|died|distracted) or is mid-cast on
    // a non-movable spell — the spline never starts and Result::Ok would
    // mislead the rule into thinking the bot is walking. Refuse early so
    // the rule's per-target retry / fallback rules can take over.
    if (p_->HasUnitState(UNIT_STATE_NOT_MOVE) ||
        p_->IsMovementPreventedByCasting())
    {
        // Name the refusal. A repeating MoveTo|Locked is indistinguishable in
        // the intent ring from a pathfinding refusal, and the campaign burned
        // two Phase-2 candidates (Shadow Labyrinth, SFK) chasing "missing
        // geometry" that headless probes then proved COMPLETE — the real
        // cause is one of ~7 unnamed Locked branches. Throttled, one line.
        static uint32 s_lock_state_ms = 0;
        const uint32 lk_now = GameTime::GetGameTimeMS();
        if (lk_now - s_lock_state_ms > 2000u)
        {
            s_lock_state_ms = lk_now;
            TC_LOG_INFO("playerbot.v2",
                "[move_lock] bot={} reason={} dst=({:.1f},{:.1f},{:.1f})",
                p_->GetGUID().GetCounter(),
                p_->IsMovementPreventedByCasting() ? "casting" : "unit_state_not_move",
                x, y, z);
        }
        return Result::Locked;
    }

    // Active FlightPath guard. Bot is on a taxi spline (FLIGHT_MOTION_TYPE)
    // — issuing MovePoint while in flight queues under, not over, the
    // flight generator and the bot looks "stuck" until landing. Refuse so
    // the AI re-evaluates next snapshot when the taxi arrives.
    if (p_->GetMotionMaster()->GetCurrentMovementGeneratorType() ==
            FLIGHT_MOTION_TYPE)
        return Result::Locked;

    // ── DIRECT move: committed traversal-link crossing ("just move, don't
    // think"). No pathfinding, no ground snap — the endpoints are human-
    // verified rows from {SharedDb()}.playerbot_nav_links, and pathfinding
    // toward the far side of a real navmesh split would NoPath and refuse.
    // MovePoint with generatePath=false launches a straight walk/jump spline
    // exactly like a player crossing the split. Distance-capped so a corrupt
    // row cannot yeet a bot across the map; the same-goal skip below is
    // intentionally bypassed (the caller-side emit dedup already holds the
    // running spline — see BotIntentEmitter::move_to).
    if (direct)
    {
        const float ddx = x - p_->GetPositionX();
        const float ddy = y - p_->GetPositionY();
        const float ddz = z - p_->GetPositionZ();
        if (ddx*ddx + ddy*ddy + ddz*ddz > 60.0f * 60.0f)
            return Result::Locked;
        p_->GetMotionMaster()->MovePoint(0, x, y, z, /*generatePath*/ false);
        return Result::Ok;
    }

    // Snap target Z to a real walkable surface. Callers (idle:wander,
    // idle:travel_to_hub, idle:quest_path …) pass either the bot's
    // current Z or a creature/object anchor Z, neither guaranteed to
    // sit on the navmesh. SnapToGround applies the same vmap/mmap
    // composite TC uses in Player::SetPosition, with a fall-through to
    // GetWaterOrGroundLevel and a top-down GetHeight probe if the first
    // lookup returns INVALID_HEIGHT. It also LoadGrid()s the destination
    // tile so Detour doesn't fall back to a straight-line shortcut on a
    // freshly-loading mmap tile.
    BotMovement::SnapToGround(p_, x, y, z);

    // Don't RESTART an actively-running travel MovePath toward the SAME goal.
    // The chunked long-haul branches (advancing-partial / complete-path) drive
    // the route as a WAYPOINT spline that takes many seconds; re-issuing move_to
    // to the same goal after the emitter's ~1.5s dedup rebuilds the path and
    // RESETS the generator to point 0. On a WINDING route that restart-thrash
    // oscillates the bot at the bend instead of letting it round the curve (live:
    // Grimfang oscillating ~22y at the Valley of Trials exit on a 243y advancing
    // chunk to Q25136, while Gorthak's straight southward chunk traversed fine).
    // While a WAYPOINT route toward the same goal is still ACTIVE, let it run;
    // re-path only when it genuinely FINISHES (the generator finalizes ->
    // motion type reverts to IDLE, so the WAYPOINT_MOTION_TYPE check below stops
    // matching and a fresh path computes from the advanced pose) or a rule
    // redirects to a DIFFERENT goal (the goal-key won't match, so repair/flee/
    // gather redirects take effect immediately).
    //
    // IMPORTANT: the skip MUST NOT be gated on p_->isMoving(). A freshly added
    // WaypointMovementGenerator runs DoInitialize -> StopMoving() +
    // _nextMoveTime.Reset(1000ms) before its first StartMove, and there is a
    // brief non-moving gap as it advances between nodes. During those windows
    // isMoving() is FALSE even though the route is alive and about to move. The
    // emitter re-emits move_to every ~1.5s; if the skip required isMoving()==true
    // it fell through during the init delay, re-ran MovePath, and
    // MotionMaster::DirectAdd REPLACED the generator (same ACTIVE/NORMAL slot) —
    // restarting the 1000ms delay and _currentNode=0 every cycle. Net effect: a
    // bot with a complete far-goal path froze in place forever, each tick
    // returning Result::Ok (so note_move_succeeded reset the wedge detector and
    // no rescue ever engaged). Live: Velruun frozen at a 338y goal whose navmesh
    // path mmap_probe proved COMPLETE (65 polys). Keying the skip off the live
    // generator type + goal-key (not the movement flag) lets the spline run to
    // completion. This also strictly improves the original winding-route fix
    // (Grimfang Valley-exit oscillation) — same intent, without the isMoving hole.
    uint64 const move_goal_key = PathDestKey(p_->GetMapId(), x, y);
    // ── TEMP Gap-1 crossing diagnostic (Deadmines map 36 foundry bridge) ──
    // Logs every branch move_to takes while a bot sits in the Gap-1 box, so the
    // off-mesh crossing is debuggable from the log alone. Remove after the fix.
    const bool dbg_gap1 = p_->GetMapId() == 36 &&
        p_->GetPositionY() > -560.0f && p_->GetPositionY() < -480.0f &&
        p_->GetPositionX() > -235.0f && p_->GetPositionX() < -200.0f;
    // Same-goal skip covers BOTH spline kinds:
    //   * WAYPOINT (long-haul MovePath, below) — original winding-route fix.
    //   * POINT    (MovePoint(generatePath), incl. the BG short-approach +
    //     off-mesh-bridge case) — re-issuing MovePoint REPLACES the
    //     PointMovementGenerator and RESTARTS its spline from the bot's current
    //     pose every tick. For a path that crosses an OFF-MESH connection (AV
    //     garrison bridge, cave/den bridges) the off-mesh "hop" is a few points
    //     in; restarting every ~1s resets the bot to the approach BEFORE the hop,
    //     so it perpetually re-walks up to the connection and NEVER executes the
    //     traversal (live: AV pushers crept a few yards at the bridge mouth for
    //     minutes, never crossing, capalive never flipped). Letting the active
    //     POINT spline run to completion lets the off-mesh hop fire. The 5y
    //     PathDestKey bucket means a captain wandering <5y holds the skip (no
    //     restart) while a real >5y move / a rule redirect changes the key and
    //     re-issues immediately. Reverts to IDLE on arrival → re-evaluated next
    //     tick. (Keyed off generator type + goal, NOT isMoving — see the init-gap
    //     note below.)
    // POINT dedup scoped to battlegrounds AND dungeon/raid instances — both are
    // verified off-mesh-approach cases (BG captain-garrison / cave-den bridges, and
    // the Deadmines foundry Gap-1 bridge). In a dungeon a per-tick re-emit was
    // restarting the in-flight off-mesh hop from the bot's mid-gap pose, dropping it
    // into the void at (-213.6,-536.7) instead of letting it land on the far vertex
    // (-213.3,-547.5). Safe to broaden: the 4y/3s stall-breaker below still fires,
    // so this cannot mask a genuine POINT stall on open terrain inside an instance.
    {
        uint64 const guid_low = p_->GetGUID().GetCounter();
        auto const mtype = p_->GetMotionMaster()->GetCurrentMovementGeneratorType();
        const bool in_offmesh_instance =
            p_->InBattleground() || (p_->GetMap() && p_->GetMap()->IsDungeon());
        const bool dedup_type = mtype == WAYPOINT_MOTION_TYPE ||
                                (mtype == POINT_MOTION_TYPE && in_offmesh_instance);
        auto const git = g_movePathGoal.find(guid_low);
        if (dedup_type && git != g_movePathGoal.end() && git->second == move_goal_key)
        {
            // Same goal + active spline. Normally skip (don't restart). But run
            // the STALL-BREAKER: a spline frozen on a water/seam segment must NOT
            // be skipped forever (see g_movePathProbe). Skip only while genuinely
            // progressing or within the 3s grace window; otherwise fall through
            // to recompute + re-issue (a fresh attempt + lets wedge logic fire).
            uint32 const now_probe = GameTime::GetGameTimeMS();
            auto& pr = g_movePathProbe[guid_low];
            float const pdx = p_->GetPositionX() - pr.x;
            float const pdy = p_->GetPositionY() - pr.y;
            if (dbg_gap1)
                TC_LOG_INFO("playerbot.v2",
                    "[gap1] {} DEDUP pos=({:.1f},{:.1f}) goalkey_same={} prog={:.1f}y "
                    "elapsed={}ms mtype={}", p_->GetName(), p_->GetPositionX(),
                    p_->GetPositionY(), (pr.key == move_goal_key) ? 1 : 0,
                    std::sqrt(pdx*pdx + pdy*pdy),
                    (uint32)(now_probe - pr.ms), (int)mtype);
            if (pr.key != move_goal_key)
            {
                // New goal for the probe — start the progress clock here.
                pr = { move_goal_key, p_->GetPositionX(), p_->GetPositionY(), now_probe };
                return Result::Ok;
            }
            if (pdx * pdx + pdy * pdy >= 4.0f * 4.0f)
            {
                // Advanced >=4y since the mark — progressing, refresh + keep running.
                pr.x = p_->GetPositionX(); pr.y = p_->GetPositionY(); pr.ms = now_probe;
                return Result::Ok;
            }
            if (now_probe - pr.ms < 3000u)
                return Result::Ok;   // within grace — let the spline keep trying
            // Stalled >3s within 4y: reset the clock and FALL THROUGH to re-issue.
            pr.x = p_->GetPositionX(); pr.y = p_->GetPositionY(); pr.ms = now_probe;
        }
    }
    // Record the goal we're (re)computing toward, so the next same-goal emit while
    // the resulting WAYPOINT spline is still running is skipped above. Harmless
    // for MovePoint/Locked outcomes — the WAYPOINT-type check gates the skip.
    g_movePathGoal[p_->GetGUID().GetCounter()] = move_goal_key;

    // Per-target failure backoff. If the SAME (5y-quantized) destination
    // failed within the last 3s, short-circuit before re-running Detour.
    // Saves CPU on persistent "wall behind quest target" cases at fleet
    // scale (project_v2_perf_1052bots.md flagged Detour as a hot path).
    // Angle-jitter wander still hits a different bucket after one tick,
    // so legitimate retries proceed.
    {
        uint32 const move_now_ms = GameTime::GetGameTimeMS();
        uint64 const dest_key = PathDestKey(p_->GetMapId(), x, y);
        if (PathFailedRecently(p_, dest_key, move_now_ms))
        {
            // Named refusal (see [move_lock] above): a poisoned destination
            // backoff looks identical to "no path" in the intent ring.
            static uint32 s_lock_backoff_ms = 0;
            if (move_now_ms - s_lock_backoff_ms > 2000u)
            {
                s_lock_backoff_ms = move_now_ms;
                TC_LOG_INFO("playerbot.v2",
                    "[move_lock] bot={} reason=path_fail_backoff dst=({:.1f},{:.1f},{:.1f})",
                    p_->GetGUID().GetCounter(), x, y, z);
            }
            return Result::Locked;
        }
    }

    // Per-world-tick pathfinding budget. Once this tick's bot-pathfinding window
    // is spent (set by DrainIntents), DEFER this move — return Result::Locked so
    // the rule retries next tick. Bounds the aggregate Detour load per world tick
    // so a quest-funnel surge cannot flood the world thread and hang it
    // (2026-06-17). Placed after the cheap prechecks + per-dest backoff, before
    // the first (and any retry) SehSafeCalculatePath below.
    if (!PathBudget::HasBudget(GameTime::GetGameTimeMS()))
        return Result::Locked;

    // Path validation gate. TC's PointMovementGenerator silently falls back
    // to a straight-line spline when Detour returns PATHFIND_NOPATH /
    // PATHFIND_FARFROMPOLY_START — that's the root cause of "bot walks
    // into walls / stairs / off cliffs" reports. We build the same path
    // PointMovementGenerator would build and inspect the result before
    // committing to the move:
    //   * NOPATH                → refuse; rule retries another target
    //   * FARFROMPOLY_START     → NearTeleport onto nearest mesh poly
    //                              first, then proceed (the next emit
    //                              after the snap will succeed)
    //   * FARFROMPOLY_END       → refuse; the destination is off-mesh
    //                              (cliff edge / inside geometry / etc)
    //   * SHORT / INCOMPLETE    → walk the partial path; rule re-fires
    //                              next tick to extend
    //   * NORMAL / SHORTCUT     → proceed (SHORTCUT is intentional for
    //                              swimming / falling cases)
    PathGenerator path(p_);
    // Road-aware per-pathfind opt-out heuristic. Disable road bias for
    // short-range moves (combat gap-close, gather/loot walk, hazard flee,
    // short stuck-rescue) where a road detour would be wrong. A tank
    // pulling toward an 8y mob shouldn't take a 50y road detour. A
    // gatherer 11y from a node shouldn't either.
    //
    // Threshold: 25y. Anything shorter is treated as tactical; longer
    // is treated as travel where road preference helps.
    //
    // Also disable for bots already in combat — even long retreats /
    // kiting / reposition steps shouldn't detour for roads.
    {
        float dx = p_->GetPositionX() - x;
        float dy = p_->GetPositionY() - y;
        float dz = p_->GetPositionZ() - z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < (25.0f * 25.0f) || p_->IsInCombat())
            path.SetDisableRoadBonus(true);
    }
    // Defensive SEH wrap around CalculatePath. Detour's dtNavMeshQuery
    // can ACCESS_VIOLATION when a navmesh tile gets unloaded mid-query
    // (e.g., grid stream-out during the path build). Crash 2026-05-13
    // 06:44 hit getPathToNode+80 with RAX=0 — null poly deref. We can't
    // catch AV via try/catch under /EHsc; SEH is the only option. The
    // helper lives outside this scope so no objects with destructors
    // straddle the __try (PathGenerator's destructor lands after this).
    bool path_ok = SehSafeCalculatePath(path, x, y, z);
    PathType pt = path.GetPathType();
    // Outcome enum mirrors PerfCounters::PathOutcome — order:
    // 0=Ok, 1=NoPath, 2=FarFromPolyStart, 3=FarFromPolyEnd, 4=Incomplete, 5=Short.
    // Per-failure diagnostic. We already counter-aggregate via
    // OnPathOutcome, but the aggregate doesn't tell us WHERE the bot
    // was when the path failed — needed to diagnose water/swim
    // pathing without manually positioning a bot. Logs bot name,
    // current position, target position, water flag at both ends,
    // and the outcome. Only fires on FAILURE outcomes so the success
    // hot path stays silent. Even on a busy fleet that's <100 lines
    // per minute.
    auto log_path_fail = [&](char const* outcome) {
        Map* mm = p_->GetMap();
        bool src_water = false;
        // Classify the destination through the shared water gate so the
        // diagnostic shows WADE / SWIM / HAZARD, not just a water bool —
        // this is what distinguishes "pier path failed over shallow water
        // (data/navmesh problem)" from "destination is genuinely in deep
        // water (correctly refused)". Same per-point + depth model the gate
        // below uses.
        char const* dst_liquid = "";
        if (mm)
        {
            src_water = mm->GetLiquidStatus(p_->GetPhaseShift(),
                            p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
                            map_liquidHeaderTypeFlags::AllLiquids, nullptr)
                        != LIQUID_MAP_NO_WATER;
            switch (BotMovement::ClassifyDestinationLiquid(p_, x, y, z))
            {
                case BotMovement::LiquidVerdict::Dry:      dst_liquid = "";        break;
                case BotMovement::LiquidVerdict::Wadeable: dst_liquid = " WADE";   break;
                case BotMovement::LiquidVerdict::Swim:     dst_liquid = " SWIM";   break;
                case BotMovement::LiquidVerdict::Hazard:   dst_liquid = " HAZARD"; break;
            }
        }
        // Endpoint-resolution introspection: query the SAME navmesh the
        // server uses to see whether src/dst polys resolve and whether
        // their tiles are loaded — distinguishes "tile not loaded" from
        // "poly out of vertical extents" from "filter excluded".
        char ep_diag[128] = "";
        if (Map* mdiag = p_->GetMap())
        {
            uint32 const terrain_map = PhasingHandler::GetTerrainMapId(
                p_->GetPhaseShift(), p_->GetMapId(), mdiag->GetTerrain(),
                p_->GetPositionX(), p_->GetPositionY());
            if (dtNavMeshQuery const* q = MMAP::MMapManager::instance()
                    ->GetNavMeshQuery(terrain_map, p_->GetMapId(), p_->GetInstanceId()))
            {
                dtQueryFilter f;  // permissive: includeAll
                f.setIncludeFlags(0xFFFF);
                float ext[3] = {3.0f, 5.0f, 3.0f};
                // SEH-guarded probes (SehSafeNearestPolyProbe): the raw
                // findNearestPoly here crashed the world thread twice in
                // minutes on 2026-06-12 (boot-time mass logins → max
                // path-fail rate → probe hit a tile mid-load; full WER
                // dumps show dtVlerp/closestPointOnDetailEdges reading
                // bad detail data). Same hazard CalculatePath learned on
                // 2026-05-13 — the diag block shipped without the guard.
                auto test = [&](float tx, float ty, float tz) -> int
                {
                    float pt[3] = {ty, tz, tx};  // TC->Detour (y,z,x)
                    int r = BotMovement::SehSafeNearestPolyProbe(q, pt, ext, &f);
                    if (r != 0) return r > 0 ? 2 : -1;  // 2 = poly found, -1 = SEH fault
                    float ext2[3] = {3.0f, 200.0f, 3.0f};
                    return BotMovement::SehSafeNearestPolyProbe(q, pt, ext2, &f);
                    // 1 = found with huge vertical (extents miss), 0 = no tile/mesh
                };
                std::snprintf(ep_diag, sizeof(ep_diag), " srcpoly=%d dstpoly=%d",
                              test(p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ()),
                              test(x, y, z));
            }
        }
        G3D::Vector3 const& ae = path.GetActualEndPosition();
        TC_LOG_INFO("playerbot.v2",
            "[path_fail] {} outcome={} map={} src=({:.1f},{:.1f},{:.1f}){} dst=({:.1f},{:.1f},{:.1f}){} "
            "ptbits=0x{:x} pts={} actual_end=({:.1f},{:.1f},{:.1f})",
            p_->GetName(), outcome, p_->GetMapId(),
            p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
            src_water ? " WATER" : "",
            x, y, z,
            dst_liquid,
            uint32(path.GetPathType()), uint32(path.GetPath().size()),
            ae.x, ae.y, ae.z);
        if (ep_diag[0])
            TC_LOG_INFO("playerbot.v2", "[path_fail_ep] {}{}", p_->GetName(), ep_diag);
    };

    if (!path_ok || (pt & PATHFIND_NOPATH))
    {
        // Dest-Z mesh-snap RETRY. Stepped/projected waypoints carry the
        // BOT's z; when the true surface lies far below or above (a step
        // bearing crossing a river gorge under a bluff, an elevated
        // anchor over ground), findNearestPoly's ±5y vertical extents
        // miss every poly and CalculatePath returns NOPATH even though
        // the mesh connects (proven with mmap_probe on Somi's Razor Hill
        // wedge: the exact failing pairs return PARTIAL once the dest z
        // is resolved). RETRY-not-preemptive: snapping BEFORE the first
        // attempt mutated perfectly good multi-level interior targets
        // onto the surface/rooftops above (observed: UC entrance-hall
        // ramp targets snapped to surface z95 and ALL failed; same for
        // Orgrimmar street targets snapped to roof height).
        if (Map* mm0 = p_->GetMap())
        {
            const float gz = mm0->GetHeight(p_->GetPhaseShift(), x, y,
                                            z + 50.0f, true, 120.0f);
            if (gz > INVALID_HEIGHT && std::fabs(gz - z) > 8.0f)
            {
                // Re-run on the SAME PathGenerator (repeated CalculatePath
                // calls overwrite internal state by design).
                if (SehSafeCalculatePath(path, x, y, gz) &&
                    !(path.GetPathType() & PATHFIND_NOPATH))
                {
                    pt      = path.GetPathType();
                    z       = gz;
                    path_ok = true;
                }
            }
            // Midpoint-bisection rescue. NOPATH|SHORTCUT with BOTH
            // endpoint polys resolvable (Somi's UC lobby walk: a clean
            // 71-poly corridor that the standalone probe routes, but the
            // live BuildPointPath collapses on — core findStraightPath
            // edge case). Halving the request converges past whatever
            // segment poisons the long call; the rule re-fires next tick
            // from the midpoint and bisects again if needed.
            if (!path_ok || (path.GetPathType() & PATHFIND_NOPATH))
            {
                if (path.GetPathType() & PATHFIND_SHORTCUT)
                {
                    const float mx = (p_->GetPositionX() + x) * 0.5f;
                    const float my = (p_->GetPositionY() + y) * 0.5f;
                    float mz = mm0->GetHeight(p_->GetPhaseShift(), mx, my,
                                              std::max(p_->GetPositionZ(), z) + 50.0f,
                                              true, 160.0f);
                    if (mz <= INVALID_HEIGHT)
                        mz = (p_->GetPositionZ() + z) * 0.5f;
                    if (SehSafeCalculatePath(path, mx, my, mz) &&
                        !(path.GetPathType() & PATHFIND_NOPATH))
                    {
                        pt = path.GetPathType();
                        // The synthetic midpoint's GetHeight can land on an
                        // upper ledge (UC walkway midpoint resolved 45y
                        // above the path's true level), making the result
                        // FARFROMPOLY_END even though the corridor built
                        // perfectly (observed: 71-point path to z-42.5
                        // refused because the request said z2.5). Walk to
                        // the path's ACTUAL reachable end — that's the
                        // progress we wanted — and clear the far-end bit.
                        G3D::Vector3 const& bend = path.GetActualEndPosition();
                        x = bend.x; y = bend.y; z = bend.z;
                        pt = PathType(pt & ~PATHFIND_FARFROMPOLY_END);
                        path_ok = true;
                    }
                }
            }
        }
    }
    if (!path_ok || (pt & PATHFIND_NOPATH))
    {
        // Water-stuck EXIT recovery. A bot that swam / fell into a canal, lake or
        // harbour can sit on a liquid poly cluster the navmesh won't route OUT of
        // toward a dry goal (Detour NoPath) even though a DESIGNED exit (bank
        // steps / ramp) is only yards away — observed: Tindle wedged in the
        // Stormwind canal for HOURS while a valid 11-leg travel route to its quest
        // existed, unreachable solely because it could not leave the water.
        // Crawling straight at the far goal is WRONG (it scales the canal WALL);
        // the canal HAS exits, so navigate to the nearest one. NearestNavPoint's
        // filter is GROUND|GROUND_STEEP|ROAD and EXCLUDES water, so it returns the
        // nearest DRY footing — the real bank/stairs exit — within the search box.
        // Swim STRAIGHT there (generatePath=false: the span to the nearest bank is
        // open water, no obstacle); once on dry mesh the next emit pathfinds to the
        // goal normally. Only fires while actually in water (a legitimate swim has
        // path_ok=true, not NoPath), and the 60y extent bounds it to a local exit,
        // never long-haul routing.
        // Liquid at the bot's CURRENT spot. Use Map::GetLiquidStatus (ANY liquid
        // present, any depth) — the SAME check that sets the [path_fail] "WATER"
        // flag — NOT p_->IsInWater() (false unless submerged) and NOT
        // ClassifyDestinationLiquid at the bot's feet (returns Dry when the bot
        // stands on the canal BOTTOM with no liquid above its own z). The bot is
        // plainly in the canal but those two report false, which is why the exit
        // recovery never engaged on the first attempts.
        Map* const wmap = p_->GetMap();
        const bool src_in_liquid = wmap && wmap->GetLiquidStatus(
            p_->GetPhaseShift(), p_->GetPositionX(), p_->GetPositionY(),
            p_->GetPositionZ(), map_liquidHeaderTypeFlags::AllLiquids, nullptr)
            != LIQUID_MAP_NO_WATER;
        {
            Position exit_pt;
            const bool navpt = src_in_liquid && Playerbot::BotMovement::NearestNavPoint(
                    p_, p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
                    /*hxy*/ 60.0f, /*hz*/ 15.0f, exit_pt);
            const float exdx = navpt ? exit_pt.GetPositionX() - p_->GetPositionX() : 0.f;
            const float exdy = navpt ? exit_pt.GetPositionY() - p_->GetPositionY() : 0.f;
            const float exdz = navpt ? exit_pt.GetPositionZ() - p_->GetPositionZ() : 0.f;
            const float exd2 = exdx * exdx + exdy * exdy + exdz * exdz;
            // DIAG (throttled 5s/bot): the [water_exit] recovery never logged across
            // builds — show which gate fails (liquid? navpoint? distance?).
            {
                static std::unordered_map<uint64, uint32> s_weAt;
                uint32& weat = s_weAt[p_->GetGUID().GetCounter()];
                uint32 const wenow = GameTime::GetGameTimeMS();
                if (weat == 0 || wenow - weat >= 5000)
                {
                    weat = wenow ? wenow : 1u;
                    TC_LOG_INFO("playerbot.v2",
                        "[water_exit_eval] {} liquid={} navpt={} dist3d={:.1f} "
                        "pos=({:.1f},{:.1f},{:.1f}) exit=({:.1f},{:.1f},{:.1f})",
                        p_->GetName(), src_in_liquid ? 1 : 0, navpt ? 1 : 0,
                        navpt ? std::sqrt(exd2) : -1.f,
                        p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
                        navpt ? exit_pt.GetPositionX() : 0.f,
                        navpt ? exit_pt.GetPositionY() : 0.f,
                        navpt ? exit_pt.GetPositionZ() : 0.f);
                }
            }
            // 3D distance, NOT horizontal-only: a harbour dock / pier exit can sit
            // almost directly ABOVE the bot (small XY, large Z).
            if (navpt && exd2 > 3.0f * 3.0f)
            {
                p_->GetMotionMaster()->MovePoint(
                    0, exit_pt.GetPositionX(), exit_pt.GetPositionY(),
                    exit_pt.GetPositionZ(), /*generatePath*/ false);
                TC_LOG_INFO("playerbot.v2",
                    "[water_exit] {} swimming to nearest dry exit "
                    "({:.1f},{:.1f},{:.1f}) from water ({:.1f},{:.1f},{:.1f})",
                    p_->GetName(), exit_pt.GetPositionX(), exit_pt.GetPositionY(),
                    exit_pt.GetPositionZ(), p_->GetPositionX(), p_->GetPositionY(),
                    p_->GetPositionZ());
                return Result::Ok;
            }
        }
        // Navmesh has no route — but the .map heightfield may still have
        // walkable ground here (a gen-time hole under tree canopy / on a
        // walkway). Try a guarded terrain-skim across a short, near-level gap
        // before giving up. Fixes e.g. the Teldrassil holes that trap Night
        // Elf bots without any navmesh regen.
        if ((p_->InBattleground() && z < p_->GetPositionZ() - 3.0f &&
             TryBgDescentCrawl(x, y, z, run)) ||
            TryTerrainWalkFallback(x, y, z, run))
            return Result::Ok;
        if (dbg_gap1)
            TC_LOG_INFO("playerbot.v2", "[gap1] {} REFUSE NoPath pos=({:.1f},{:.1f}) "
                "dst=({:.1f},{:.1f},{:.1f})", p_->GetName(), p_->GetPositionX(),
                p_->GetPositionY(), x, y, z);
        Playerbot::Hooks::OnPathOutcome(1 /*NoPath*/);
        log_path_fail("NoPath");
        NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y), GameTime::GetGameTimeMS(),
                     kPathFailTtlLongMs);
        return Result::Locked;
    }
    if (pt & PATHFIND_FARFROMPOLY_END)
    {
        if (dbg_gap1)
            TC_LOG_INFO("playerbot.v2", "[gap1] {} REFUSE FarFromPolyEnd pos=({:.1f},{:.1f}) "
                "dst=({:.1f},{:.1f},{:.1f})", p_->GetName(), p_->GetPositionX(),
                p_->GetPositionY(), x, y, z);
        if ((p_->InBattleground() && z < p_->GetPositionZ() - 3.0f &&
             TryBgDescentCrawl(x, y, z, run)) ||
            TryTerrainWalkFallback(x, y, z, run))
            return Result::Ok;
        Playerbot::Hooks::OnPathOutcome(3 /*FarFromPolyEnd*/);
        log_path_fail("FarFromPolyEnd");
        // Off-mesh destination (above terrain / inside geometry / a phantom POI
        // Z). Permanently unreachable until the data changes — suppress long so
        // the bot stops oscillating toward it and goal-selection moves on.
        NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y), GameTime::GetGameTimeMS(),
                     kPathFailTtlLongMs);
        return Result::Locked;
    }
    if (pt & PATHFIND_FARFROMPOLY_START)
    {
        if (dbg_gap1)
            TC_LOG_INFO("playerbot.v2", "[gap1] {} FarFromPolyStart pos=({:.1f},{:.1f}) "
                "dst=({:.1f},{:.1f},{:.1f})", p_->GetName(), p_->GetPositionX(),
                p_->GetPositionY(), x, y, z);
        // Bot is off-mesh at the source (standing on an unmeshed WMO platform,
        // just teleported into geometry, fell off a ledge, login spawn into a
        // wall). Prefer WALKING off it: if there's continuous walkable VMAP
        // ground toward the target, crawl that way (no start poly needed — the
        // skim uses GetHeight, not Detour) and let a later emit re-pathfind once
        // back on mesh. Only when no walkable line exists do we resort to the
        // teleport-snap below. This honours no-teleport-rescue: move under the
        // bot's own feet first, teleport strictly as the last resort.
        if (TryTerrainWalkFallback(x, y, z, run))
            return Result::Ok;
        // Snap to the nearest valid poly so the next move_to emit can pathfind
        // from a real mesh point. Use the path's actual end position as a heuristic
        // for "where Detour wishes the source had been" — this is the
        // closest poly Detour found from the bot's current XY.
        G3D::Vector3 const& near_poly = path.GetActualEndPosition();
        // XY bound on the snap: it exists to NUDGE the bot onto an adjacent
        // poly (unmeshed platform lip, login-into-wall), not to RELOCATE it.
        // When the whole local area is unmeshed, Detour's "closest poly" can
        // be enormously far — observed 2026-06-12 Deephaul Ravine: bot
        // spawned in the (unmeshed) elevated start area got snapped 1.2K
        // yards outside the arena and wedged there for the whole match
        // (133 NoPath re-paths from one spot). Beyond the bound, refuse:
        // the surface gap is a data problem that must surface in logs, not
        // a teleport destination.
        {
            const float sdx = near_poly.x - p_->GetPositionX();
            const float sdy = near_poly.y - p_->GetPositionY();
            if (sdx * sdx + sdy * sdy > 25.0f * 25.0f)
            {
                TC_LOG_INFO("playerbot.v2",
                    "[offmesh_snap] {} REFUSED far snap {:.0f}y map={} src=({:.0f},{:.0f},{:.0f}) poly=({:.0f},{:.0f},{:.0f})",
                    p_->GetName(), std::sqrt(sdx * sdx + sdy * sdy), p_->GetMapId(),
                    p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
                    near_poly.x, near_poly.y, near_poly.z);
                Playerbot::Hooks::OnPathOutcome(2 /*FarFromPolyStart*/);
                NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y),
                             GameTime::GetGameTimeMS());
                return Result::Locked;
            }
        }
        // Go through BotMovement::SafeNearTeleport which applies the
        // 20y Z-delta canary (rejects cave-roof / stratum-skip teleports
        // that the raw Player::NearTeleportTo doesn't guard against).
        // If the snap-target Z is on a different stratum than the bot's
        // current Z, SafeNearTeleport refuses — return Locked so the
        // rule can try a different target, otherwise we'd loop forever
        // on the same off-mesh source with the same refused snap.
        const bool snap_ok = BotMovement::SafeNearTeleport(
            p_, near_poly.x, near_poly.y, near_poly.z, p_->GetOrientation());
        Playerbot::Hooks::OnPathOutcome(2 /*FarFromPolyStart*/);
        if (!snap_ok)
        {
            // Stamp the destination as failed too — the SOURCE is off-mesh
            // for THIS target's pathfind. Different XY destinations may
            // still resolve from the same source, so the LRU's per-bucket
            // expiry lets the rule explore other directions.
            NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y),
                         GameTime::GetGameTimeMS());
            return Result::Locked;
        }
        // Don't issue MovePoint this tick; let next snapshot re-emit
        // from the on-mesh position.
        return Result::Ok;
    }

    // Partial / truncated paths: Detour could not reach the requested
    // destination and returned the closest reachable endpoint instead.
    // Two very different situations share this flag:
    //   (a) ADVANCING partial — a legitimate long-haul / chunked route
    //       whose reachable endpoint is meaningfully closer to the goal.
    //       Walk it; the next emit picks up from the new pose and extends.
    //   (b) NON-ADVANCING partial — the reachable endpoint sits ~at the
    //       bot (target on a disconnected nav island, a kill mob just past
    //       a closable gap, a poly the A* budget couldn't bridge). Issuing
    //       the MovePoint and returning Ok makes BotIntentExecutor call
    //       note_move_succeeded() every tick → path_blocked_count_ resets
    //       → check_anchor_wedge and the wander angle-salt never engage,
    //       and the LRU never suppresses the doomed dst. The bot walks the
    //       short stub, stops, re-emits the SAME dst, and oscillates
    //       forever (project_movement_recovery / wedge reports).
    //
    // Discriminate by FORWARD PROGRESS toward the requested destination:
    // how much closer to (x,y,z) the partial's reachable endpoint gets us
    // than the bot's CURRENT position. _actualEndPosition is exactly that
    // closest-reachable point (PathGenerator computes it for every path).
    // Distances are 3D to match the road-bonus / swim-span gates above.
    if (pt & PATHFIND_INCOMPLETE)
    {
        Playerbot::Hooks::OnPathOutcome(4 /*Incomplete*/);

        // BG start-platform / graveyard STEP-DOWN (2026-06-22, owner-directed).
        // A bot stranded on an elevated, OVERHANGING battleground platform (EotS)
        // is navmesh-disconnected from the arena floor: Detour returns an
        // Incomplete partial (sometimes "advancing" 200y, sometimes non-advancing)
        // but the bot can't actually leave the platform — the drop to the field
        // isn't walkable. Handle it FIRST, before the advancing/non-advancing
        // split, so neither the MovePath-the-partial path (which stalls on the
        // rim) nor the local-minimum escape starves it. Relocate the bot onto the
        // field navmesh just in front of its platform (toward the objective, at
        // field height — the elevator-disembark pattern); normal routing then
        // resumes from solid ground. Tight gate (objective >=18y BELOW the bot)
        // so it fires ONLY for a genuine platform descent, never for normal
        // near-level field movement, and BG-only / downward-only so it can never
        // strand a bot riding geometry up.
        if (p_->InBattleground() && z < p_->GetPositionZ() - 18.0f &&
            TryBgDescentCrawl(x, y, z, run))
            return Result::Ok;

        // Minimum meaningful gain. Below this the partial is effectively a
        // no-op (endpoint at, or no closer to — possibly farther from — the
        // goal). 5y is one SnapToGround bucket of slop; a genuine long route
        // chunk advances far more than this per emit.
        constexpr float kMinForwardProgressYards = 5.0f;

        G3D::Vector3 const& path_end = path.GetActualEndPosition();
        auto dist3d = [](float ax, float ay, float az,
                         float bx, float by, float bz) -> float
        {
            float const ddx = ax - bx, ddy = ay - by, ddz = az - bz;
            return std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);
        };
        float const cur_to_dst = dist3d(p_->GetPositionX(), p_->GetPositionY(),
                                        p_->GetPositionZ(), x, y, z);
        float const end_to_dst = dist3d(path_end.x, path_end.y, path_end.z,
                                        x, y, z);
        float const forward_progress = cur_to_dst - end_to_dst;

        log_path_fail("Incomplete");
        TC_LOG_INFO("playerbot.v2",
            "[path_fail] {} Incomplete forward_progress={:.1f}y (cur_to_dst={:.1f} end_to_dst={:.1f})",
            p_->GetName(), forward_progress, cur_to_dst, end_to_dst);

        if (forward_progress < kMinForwardProgressYards)
        {
            // FINAL-APPROACH exception (2026-06-18): when the REACHABLE endpoint
            // already lands within NPC interaction range of the requested dst, the
            // dst is simply a few yards OFF the navmesh — a quest giver / object /
            // POI whose exact point doesn't snap to a walkable poly. Parking here
            // freezes the bot ~3-12y short of its goal FOREVER, which the live logs
            // show is the single biggest fleet wedge: Norenen frozen cur_to_dst=5.0
            // with the endpoint 3.3y from dst; Lorashel 12.3/11.7; Bemardel
            // 10.0/9.6 — all stuck just outside an interactable they could reach.
            // Take the short final hop to the endpoint: it puts the bot within
            // interaction range so the quest/vendor/loot/gossip logic completes,
            // then the bot stops re-pathing. Bounded to end_to_dst <= 6y (the
            // endpoint is right at the GOAL), so this can NOT resurrect the
            // rolled-back long crawl-on-Incomplete below: an elevator/ramp gap has
            // a LARGE end_to_dst (endpoint on the near side, dst far across), so it
            // never enters this branch.
            constexpr float kGoalCloseEnoughYards = 6.0f;
            if (end_to_dst <= kGoalCloseEnoughYards)
            {
                G3D::Vector3 const& fin_end = path.GetActualEndPosition();
                if (p_->IsWalking() == run)
                    p_->SetWalk(!run);
                p_->GetMotionMaster()->MovePoint(0, fin_end.x, fin_end.y, fin_end.z,
                                                 /*generatePath*/ true);
                return Result::Ok;
            }
            // Water-stuck EXIT recovery (same as the NoPath branch). A bot in a
            // canal / harbour whose objective sits on a navmesh-DISCONNECTED dry
            // island (a dock / pier a few yards UP — e.g. the Stormwind harbour
            // Hired Courier at z=6.2 over Tindle at the water surface z=0) reports
            // a non-advancing partial here (forward_progress~0, end_to_dst just
            // over the 6y final-approach cap), NOT NoPath, so the recovery above
            // never sees it. Route to the nearest DRY footing (NearestNavPoint
            // excludes water -> the dock / bank) and MovePoint straight onto it.
            // Gated on the bot being in liquid, so it can NOT re-introduce the
            // rolled-back crawl-on-Incomplete that stranded bots on elevator
            // platforms (elevators are never in water).
            {
                Map* const nwmap = p_->GetMap();
                const bool ns_in_liquid = nwmap && nwmap->GetLiquidStatus(
                    p_->GetPhaseShift(), p_->GetPositionX(), p_->GetPositionY(),
                    p_->GetPositionZ(), map_liquidHeaderTypeFlags::AllLiquids, nullptr)
                    != LIQUID_MAP_NO_WATER;
                if (ns_in_liquid)
                {
                    Position nexit;
                    if (Playerbot::BotMovement::NearestNavPoint(
                            p_, p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ(),
                            /*hxy*/ 60.0f, /*hz*/ 15.0f, nexit))
                    {
                        const float nexdx = nexit.GetPositionX() - p_->GetPositionX();
                        const float nexdy = nexit.GetPositionY() - p_->GetPositionY();
                        const float nexdz = nexit.GetPositionZ() - p_->GetPositionZ();
                        // 3D (incl. vertical) — the dock/pier exit can be almost
                        // straight up (small XY, large Z); XY-only rejected it.
                        if (nexdx * nexdx + nexdy * nexdy + nexdz * nexdz > 3.0f * 3.0f)
                        {
                            p_->GetMotionMaster()->MovePoint(
                                0, nexit.GetPositionX(), nexit.GetPositionY(),
                                nexit.GetPositionZ(), /*generatePath*/ false);
                            TC_LOG_INFO("playerbot.v2",
                                "[water_exit] {} (non-advancing) to nearest dry exit "
                                "({:.1f},{:.1f},{:.1f}) from water ({:.1f},{:.1f},{:.1f})",
                                p_->GetName(), nexit.GetPositionX(), nexit.GetPositionY(),
                                nexit.GetPositionZ(), p_->GetPositionX(),
                                p_->GetPositionY(), p_->GetPositionZ());
                            return Result::Ok;
                        }
                    }
                }
            }
            // NOTE (2026-06-09): a crawl-on-Incomplete fallback was tried here but
            // ROLLED BACK — combined with the ramp-climb gate it shoved bots up onto
            // elevator platforms they couldn't then step off (Pyrethel/Gorois stranded
            // riding Org lift 206610). The proper elevator traversal is being designed
            // separately (travel-graph-defined post-platform step). See
            // docs/ELEVATOR_TRAVEL_HANDOVER_20260609.md.
            //
            // Non-advancing partial — treat exactly like NoPath: stamp the
            // LRU, do NOT issue the doomed MovePoint, and return Locked so
            // BotIntentExecutor increments path_blocked_count_, logs
            // [move_blocked], and does NOT call note_move_succeeded(). That lets
            // the wedge detector, wander angle-salt, and dst-suppression engage.
            //
            // TTL is distance-aware. The 60s long TTL is right for a NEARBY
            // disconnected target (an island / unbridgeable poly the bot is
            // standing next to — geometry-stable, don't re-probe). But for a FAR
            // goal the bot is mid CHUNKED long-haul: it momentarily reads
            // non-advancing at THIS pose (budget truncation, a tile not yet
            // loaded, a poly seam) yet the route genuinely extends further (e.g.
            // Gorthak reached -4732 from -4499 but stalls at -4722 -> Sen'jin
            // -4921). A 60s freeze on the destination key then blocks ALL
            // re-approaches via the early per-target backoff above, even from a
            // better pose. Use the SHORT transient TTL for far goals so the haul
            // retries within a tick or two and continues; keep the long TTL only
            // for a genuinely local disconnected target.
            // LOCAL-MINIMUM ESCAPE (2026-06-20). Before refusing, try to FOLLOW
            // the partial to crawl OUT of a confined/winding spot — the only fix
            // for a bot whose route to a far goal must wind away-then-back past
            // the 74-poly cap (live: Durnan frozen 154 blocks indoors; walking the
            // −47y partial to (-4986,-954) unlocks a +372y advancing path from
            // there). Strictly gated so it can NOT re-introduce the rolled-back
            // crawl's elevator/ramp stranding or thrash a true dead-end:
            //   * far goal mid-haul (cur_to_dst > 100y) — not a local disconnected
            //     target (those keep the long-TTL refuse below);
            //   * a real escape leg (endpoint >= 15y away) — not sub-step jitter;
            //   * NOT climbing (endpoint z <= pose z + 8y) — a downward/level
            //     escape can't strand the bot riding a lift UP (the documented
            //     2026-06-09 rollback failure mode);
            //   * pts >= 2 so MovePath feeds real waypoints (no MotionMaster
            //     re-pathfind that degenerates to a same-poly zero-move);
            //   * anti-oscillation budget per (bot,goal): a true tiny pocket
            //     ping-pongs without ever closing in → after kMaxEscapeCrawls
            //     it falls through to the refuse and the wedge machinery engages.
            // No extra pathfind — MovePath drives the ALREADY-COMPUTED partial,
            // so this adds zero world-thread CalculatePath cost.
            {
                auto const& esc_pts = path.GetPath();
                const float esc_leg = dist3d(path_end.x, path_end.y, path_end.z,
                                             p_->GetPositionX(), p_->GetPositionY(),
                                             p_->GetPositionZ());
                if (cur_to_dst > 100.0f && esc_pts.size() >= 2 &&
                    esc_leg >= kMinEscapeLegYards &&
                    path_end.z <= p_->GetPositionZ() + kMaxEscapeClimbYards)
                {
                    EscapeCrawl& ec = g_escape_crawl[p_->GetGUID().GetCounter()];
                    const uint64 esc_goal = PathDestKey(p_->GetMapId(), x, y);
                    // Reset the budget on a new goal OR when the bot has genuinely
                    // closed in since the last crawl (a pocket was escaped — real
                    // progress unlocked), so a long multi-pocket haul never starves.
                    if (ec.goal_key != esc_goal ||
                        cur_to_dst + kMinForwardProgressYards < ec.best_cur_to_dst)
                    {
                        ec.goal_key = esc_goal;
                        ec.best_cur_to_dst = cur_to_dst;
                        ec.crawls = 0;
                    }
                    if (cur_to_dst < ec.best_cur_to_dst)
                        ec.best_cur_to_dst = cur_to_dst;
                    if (ec.crawls < kMaxEscapeCrawls)
                    {
                        ++ec.crawls;
                        std::vector<WaypointNode> esc_nodes;
                        esc_nodes.reserve(esc_pts.size());
                        uint32 enid = 0;
                        for (G3D::Vector3 const& pp : esc_pts)
                            esc_nodes.emplace_back(enid++, pp.x, pp.y, pp.z);
                        WaypointPath escPath(0, std::move(esc_nodes),
                            run ? WaypointMoveType::Run : WaypointMoveType::Walk);
                        escPath.BuildSegments();
                        if (p_->IsWalking() == run)
                            p_->SetWalk(!run);
                        p_->GetMotionMaster()->MovePath(escPath, /*repeatable*/ false, {}, {},
                            run ? MovementWalkRunSpeedSelectionMode::ForceRun
                                : MovementWalkRunSpeedSelectionMode::ForceWalk);
                        TC_LOG_INFO("playerbot.v2",
                            "[path_escape] {} crawl {}/{} out of local minimum: leg={:.0f}y "
                            "endpoint=({:.0f},{:.0f},{:.0f}) cur_to_dst={:.0f}",
                            p_->GetName(), ec.crawls, kMaxEscapeCrawls, esc_leg,
                            path_end.x, path_end.y, path_end.z, cur_to_dst);
                        return Result::Ok;
                    }
                    // Budget exhausted — a genuine pocket / oscillation; fall through.
                }
            }
            // BG boss-approach CRAWL (2026-06-23). A CLOSE goal (cur_to_dst <
            // 80y) reading non-advancing is typically an objective just OFF/below
            // the navmesh: the AV enemy captain (Galvangar/Balinda) and general
            // (Drek'Thar/Vanndar) stand inside a walled garrison/keep whose floor
            // is not meshed — the nearest poly sits ~35y ABOVE the spawn, so the
            // navmesh path dead-ends ~47y out and the whole push squad freezes
            // there and never lands a hit (live: min_gd pinned at 47y for 10min,
            // 0 casts landed, captain untouched). Crawl the remaining gap across
            // the collision-backed structure floor toward the goal instead of
            // long-locking the destination. Gated BG-only + close-only (< 80y) so
            // it can never reintroduce open-world crawl stranding or thrash a far
            // haul (which the advancing-partial path above already drives).
            if (p_->InBattleground() && cur_to_dst < 80.0f &&
                ((z < p_->GetPositionZ() - 3.0f && TryBgDescentCrawl(x, y, z, run)) ||
                 TryTerrainWalkFallback(x, y, z, run)))
                return Result::Ok;
            const uint32 nonadv_ttl =
                (cur_to_dst > 100.0f) ? kPathFailTtlMs : kPathFailTtlLongMs;
            NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y),
                         GameTime::GetGameTimeMS(), nonadv_ttl);
            return Result::Locked;
        }
        // Advancing partial — drive to the REACHABLE endpoint Detour found, NOT
        // the far requested dst. Re-issuing MovePoint(far dst, generatePath) can
        // leave the bot FROZEN: when the dst's navmesh tile isn't loaded (e.g. a
        // 676y cross-zone quest goal), the MotionMaster's OWN path resolves the
        // unreachable dst to a degenerate same-poly route and emits zero movement
        // (live: Gorthak wedged 9min at the Valley of Trials exit on quest 25133
        // "Report to Sen'jin Village", srcpoly==dstpoly, forward_progress=254y but
        // never a step taken). The API already computed the reachable endpoint;
        // MovePoint to THAT — it sits on a loaded poly so the MotionMaster paths
        // cleanly and the bot actually moves. The next emit re-paths from the
        // advanced pose and extends toward the goal, pulling in the next navmesh
        // tile as the bot crosses the boundary — proper chunked long-haul. This
        // is the documented "advancing partial — walk it" behavior, made real.
        {
            // Drive the API's ALREADY-COMPUTED path points via MovePath rather than
            // MovePoint(actual_end, generatePath=true). MovePoint makes the
            // MotionMaster RE-pathfind to the endpoint, and on a truncated long-haul
            // (pts hit the cap) that endpoint sits on a poly/tile seam where the MM's
            // own pathfind degenerates to a same-poly route and emits ZERO movement —
            // the bot freezes mid-route despite a 230y advancing partial (live:
            // Gorthak/Grimfang frozen at the Valley of Trials exit on Q25133
            // "Report to Sen'jin", Incomplete forward_progress=230y, never a step).
            // MovePath feeds the validated smooth points straight to the waypoint
            // generator (which still navmesh-paths the short hops BETWEEN adjacent
            // points), so the bot actually walks the chunk; the next emit re-paths
            // from the advanced pose and extends — proper chunked long-haul. Same
            // mechanism the road-centerline branch above already uses successfully.
            auto const& pts = path.GetPath();
            if (pts.size() >= 2)
            {
                std::vector<WaypointNode> nodes;
                nodes.reserve(pts.size());
                uint32 nid = 0;
                for (G3D::Vector3 const& pp : pts)
                    nodes.emplace_back(nid++, pp.x, pp.y, pp.z);

                WaypointPath advPath(0, std::move(nodes),
                    run ? WaypointMoveType::Run : WaypointMoveType::Walk);
                advPath.BuildSegments();

                if (p_->IsWalking() == run)
                    p_->SetWalk(!run);
                p_->GetMotionMaster()->MovePath(advPath, /*repeatable*/ false, {}, {},
                    run ? MovementWalkRunSpeedSelectionMode::ForceRun
                        : MovementWalkRunSpeedSelectionMode::ForceWalk);
                return Result::Ok;
            }
            // Degenerate path (single point) — fall back to the endpoint MovePoint.
            G3D::Vector3 const& adv_end = path.GetActualEndPosition();
            if (p_->IsWalking() == run)
                p_->SetWalk(!run);
            p_->GetMotionMaster()->MovePoint(0, adv_end.x, adv_end.y, adv_end.z,
                                             /*generatePath*/ true);
            return Result::Ok;
        }
    }
    else if (pt & PATHFIND_SHORT)
    {
        Playerbot::Hooks::OnPathOutcome(5 /*Short*/);
        log_path_fail("Short");
        // A SHORT result discards the real corridor and returns a blind straight
        // 2-point shortcut (the route exceeded the 74-poly/point cap), which walks
        // straight THROUGH terrain and wedges on any hill/wall for a FAR goal
        // (live: Durnan SHORT to Gremlock 544y, frozen at -5113,-800). Chunk it:
        // re-path to the MIDPOINT (mirrors the NoPath bisection above) so the
        // shorter corridor fits the cap and yields a REAL navmesh route; we then
        // MovePath to that reachable end and re-path from the advanced pose next
        // emit — proper chunked long-haul. Only for a far goal (a near SHORT span
        // is open ground and fine to cross straight). One extra CalculatePath, and
        // only on SHORT (rare) — within the already-checked per-tick path budget.
        if (Map* mmS = p_->GetMap())
        {
            const float sdx = x - p_->GetPositionX();
            const float sdy = y - p_->GetPositionY();
            // Chunk ANY non-melee SHORT span, not just far (>250y) ones. A "near"
            // SHORT span is NOT always open ground: a short but WINDING corridor
            // (e.g. a switchback descent off a ledge) also exceeds the 74-poly cap,
            // and the blind 2-point shortcut then walks straight through terrain and
            // wedges (observed: L10 Tindle on Q432, ~88y down a Dun Morogh slope,
            // SHORT-shortcut'd into the hillside, 0 progress). Threshold 40y keeps a
            // true melee-range hop cheap (blind cross is fine there) but chunks the
            // rest into a real navmesh corridor the bot can actually follow.
            if (sdx * sdx + sdy * sdy > 40.0f * 40.0f)
            {
                const float mx = (p_->GetPositionX() + x) * 0.5f;
                const float my = (p_->GetPositionY() + y) * 0.5f;
                float mz = mmS->GetHeight(p_->GetPhaseShift(), mx, my,
                                          std::max(p_->GetPositionZ(), z) + 50.0f,
                                          true, 160.0f);
                if (mz <= INVALID_HEIGHT)
                    mz = (p_->GetPositionZ() + z) * 0.5f;
                if (SehSafeCalculatePath(path, mx, my, mz) &&
                    !(path.GetPathType() & PATHFIND_NOPATH))
                {
                    // Re-target to the reachable end of the chunk corridor and
                    // clear the SHORT/far-end bits so the normal MovePath below
                    // drives the REAL points (not the discarded straight shortcut).
                    G3D::Vector3 const& bend = path.GetActualEndPosition();
                    x = bend.x; y = bend.y; z = bend.z;
                    pt = PathType(path.GetPathType()
                                  & ~PATHFIND_FARFROMPOLY_END & ~PATHFIND_SHORT);
                    TC_LOG_INFO("playerbot.v2",
                        "[path_chunk] {} SHORT far goal -> midpoint chunk to "
                        "({:.0f},{:.0f},{:.0f})", p_->GetName(), x, y, z);
                }
            }
        }
    }
    else
        Playerbot::Hooks::OnPathOutcome(0 /*Ok*/);

    // Liquid safety gate. Classifies the destination POINT (not the tile/chunk
    // — a chunk is routinely half-water / half-dry) through the shared
    // BotMovement water gate, which probes Map::GetLiquidStatus at (x,y,z) with
    // the bot's REAL collision height (floored at DEFAULT_COLLISION_HEIGHT).
    // Two refuse cases, both conservative:
    //   * Hazard (magma / slime) — bot burns to death on contact.
    //   * Swim (fully submerged for THIS bot) — a non-aquatic bot drowns and
    //     the navmesh ends at the surface, so no walkable path exists below.
    //
    // Wadeable depth is ALLOWED for all bots. Shoreline hubs (Ratchet, Booty
    // Bay, Echo Isles' Den, etc.) sit half-in-water — a bot approaching along a
    // curved shore momentarily clips the surface; blocking that caused wander
    // to oscillate and starter-zone bots to make zero progress. The collision-
    // height floor guarantees this gate is never more restrictive than the
    // historically-tuned default, so that tuning is preserved.
    //
    // PathGenerator::CreateFilter for TYPEID_PLAYER unconditionally adds
    // NAV_MAGMA_SLIME / NAV_WATER to the include flags (TC: "perfect support
    // not possible, just stay safe"), so a path can route through hazards and
    // return PATHFIND_NORMAL. This point gate is the only thing that stops it;
    // the mmap read is cheap and only fires on otherwise-valid paths.
    {
        BotMovement::LiquidVerdict const lv =
            BotMovement::ClassifyDestinationLiquid(p_, x, y, z);

        // Hazard (magma / slime) is always refused — the bot burns to death
        // on contact regardless of distance or breath.
        if (lv == BotMovement::LiquidVerdict::Hazard)
        {
            // Treat as unreachable — wander angle hash picks a different
            // direction next emit. Stamp the LRU so a wander loop that hashes
            // the same 5y bucket doesn't re-pay the full Detour + liquid probe
            // every tick; hazard polygons are terrain-stable so the 3s TTL is
            // plenty for wander to rotate through.
            Playerbot::Hooks::OnPathOutcome(3 /*FarFromPolyEnd*/);
            log_path_fail("Magma");
            NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y),
                         GameTime::GetGameTimeMS());
            return Result::Locked;
        }

        // Swim (destination submerges this bot) used to be refused outright,
        // which meant bots could never cross ANY water — a pond, a stream, a
        // shallow lagoon between two quest hubs all read as Swim at the far
        // bank and the move was rejected. Real players swim short stretches;
        // they just don't path across oceans. So: PERMIT a short swim that
        // comfortably fits inside the bot's breath budget; REFUSE only a long
        // deep-water span (open water / lake / ocean the bot would drown
        // partway through, and where the navmesh has no surface route anyway).
        //
        // Breath model: Player::getMaxTimer(BREATH_TIMER) returns the bot's
        // FULL underwater time in ms, or DISABLED_MIRROR_TIMER (-1) when the
        // bot can breathe underwater indefinitely (water-breathing aura, GM
        // breath-disable, or dead/ghost). We assume full breath here because
        // the bot is at the START of the crossing (move_to is the entry to a
        // swim, not a mid-swim re-emit) — a conservative assumption that only
        // becomes MORE permissive when the bot actually still has full air.
        //
        // Budget: require the estimated swim duration to stay under a fraction
        // of the breath timer (kBreathSafety) so the bot surfaces on the far
        // bank with air to spare, never mid-lake gasping. Distance is the
        // straight-line span to the destination; swim speed is the bot's real
        // MOVE_SWIM speed (mounts / aquatic forms / speed auras all folded in).
        if (lv == BotMovement::LiquidVerdict::Swim)
        {
            // Fraction of the breath timer a single crossing may consume. 0.5
            // leaves half the air bar as margin for current drift, the dive at
            // the near bank, and the climb-out at the far bank.
            constexpr float kBreathSafety = 0.5f;
            // Hard span cap independent of breath, so an indefinite-breather
            // (water-walking / aquatic mount) still can't be routed straight
            // across an ocean by this gate — long-haul water travel belongs to
            // the higher routing layer, not a single MovePoint emit.
            constexpr float kMaxSwimSpanYards = 60.0f;

            float const dxw = p_->GetPositionX() - x;
            float const dyw = p_->GetPositionY() - y;
            float const dzw = p_->GetPositionZ() - z;
            float const span = std::sqrt(dxw * dxw + dyw * dyw + dzw * dzw);

            // The breath timer (getMaxTimer) is a protected Player member we
            // can't read from here. We don't need it: the hard span cap is the
            // real guard. A <=60y crossing at normal swim speed takes only a
            // few seconds — far inside the shortest breath bar (~1 min) — so
            // any crossing that passes the span cap is breath-safe by
            // construction. kBreathSafety is retained only as documentation of
            // the margin the cap implies.
            (void)kBreathSafety;
            const bool allow_swim = (span <= kMaxSwimSpanYards);

            if (!allow_swim)
            {
                // Genuinely long / unbreathable deep-water span — refuse as
                // before. Stamp the LRU so wander doesn't re-probe every tick.
                Playerbot::Hooks::OnPathOutcome(3 /*FarFromPolyEnd*/);
                log_path_fail("Submerged");
                NotePathFail(p_, PathDestKey(p_->GetMapId(), x, y),
                             GameTime::GetGameTimeMS());
                return Result::Locked;
            }
            // Short crossing within breath budget — fall through and let the
            // MovePoint below issue the swim spline. PathGenerator already
            // returned a NORMAL/SHORTCUT path (we're past the NOPATH gate), so
            // Detour found a route across; we are only relaxing the *policy*
            // refusal here, not bypassing pathfinding.
        }
    }

    // Road-aware centerline routing. If the operator-authored handcrafted road
    // network offers a beneficial route to the destination, follow its centerline
    // as a waypoint spline instead of beelining. WaypointMovementGenerator still
    // navmesh-paths BETWEEN nodes (generatePath defaults true), so the authored
    // nodes act as guide points that pull the route onto the road while Detour
    // handles fine pathing — and those sub-paths keep the NAV_AREA_ROAD bias.
    // Skipped in combat and while flying; the area-cost bias remains the fallback.
    if (!p_->IsInCombat() && !p_->IsFlying())
    {
        std::vector<Position> roadWps;
        bool roadOk = sHandcraftedRoadGraph->ComputeRoute(p_->GetMap(), p_->GetPhaseShift(),
                p_->GetPosition(), Position(x, y, z, 0.0f), roadWps) && roadWps.size() >= 2;
        // Reject a road route whose FIRST waypoint heads AWAY from the goal — the
        // nearest road entry node can sit BEHIND the bot (e.g. back inside the Orc
        // Valley of Trials), so MovePathing it makes the bot backtrack onto the
        // road and oscillate at the exit chokepoint instead of leaving (live:
        // Grimfang frozen at the Valley exit on Q25133 "Report to Sen'jin" until
        // the road graph was bypassed; navmesh routing walked him straight out).
        // Fall through to the (MovePath-driven) navmesh branch in that case; a
        // forward-heading road route is still used for its smoother centerline.
        if (roadOk)
        {
            float const bgx = p_->GetPositionX(), bgy = p_->GetPositionY();
            float const b2g = std::sqrt((x - bgx) * (x - bgx) + (y - bgy) * (y - bgy));
            float const w2g = std::sqrt(
                (x - roadWps[0].GetPositionX()) * (x - roadWps[0].GetPositionX()) +
                (y - roadWps[0].GetPositionY()) * (y - roadWps[0].GetPositionY()));
            if (w2g > b2g + 15.0f)   // first road node is a >15y backtrack — skip the road
                roadOk = false;
        }
        if (roadOk)
        {
            std::vector<WaypointNode> nodes;
            nodes.reserve(roadWps.size());
            uint32 nid = 0;
            for (Position const& wp : roadWps)
                nodes.emplace_back(nid++, wp.GetPositionX(), wp.GetPositionY(), wp.GetPositionZ());

            WaypointPath roadPath(0, std::move(nodes),
                run ? WaypointMoveType::Run : WaypointMoveType::Walk);
            roadPath.BuildSegments();

            if (p_->IsWalking() == run)
                p_->SetWalk(!run);

            p_->GetMotionMaster()->MovePath(roadPath, /*repeatable*/ false, {}, {},
                run ? MovementWalkRunSpeedSelectionMode::ForceRun
                    : MovementWalkRunSpeedSelectionMode::ForceWalk);
            return Result::Ok;
        }
    }

    // Run/walk selection. Without this the bot inherits whatever walk
    // state a previous immersive-RP toggle / /walk command left behind,
    // and a "run to quest hub" emit becomes a leisurely 2.5y/s stroll.
    // Mirrors the SetWalk pattern used by FollowMovementGenerator (target's
    // IsWalking → bot follows with same speed mode).
    if (p_->IsWalking() == run)
        p_->SetWalk(!run);

    // Complete path: out of combat, drive the API's computed points via MovePath
    // for the SAME reason as the advancing-partial branch — MovePoint(x,y,z,
    // generatePath=true) lets the MotionMaster RE-pathfind the dst and can resolve
    // it to a degenerate same-poly route (zero movement) on certain seams, even
    // when the API found a full path (live: Grimfang frozen at the Valley exit
    // pursuing a 160y COMPLETE-path goal, no path_fail, never a step — while
    // Gorthak on the Incomplete branch was already unstuck by the MovePath fix).
    // GATED to !combat && !flying (mirrors the road-centerline branch) so chase /
    // flight movement keeps the established MovePoint behavior; this only changes
    // out-of-combat travel, where the degenerate-freeze wedge actually happens.
    //
    // EXCEPTION — short approach over an OFF-MESH bridge in an instance (in-BG
    // 2026-06-23; broadened to dungeon/raid 2026-06-25). On a complete path that
    // crosses an off-mesh connection (AV captain garrison / cave-den bridges, and
    // the Deadmines foundry Gap-1 bridge), MovePath re-pathfinds BETWEEN its
    // precomputed points and cannot traverse the off-mesh's unwalkable span, so the
    // bot freezes / drops into the gap (live: AV pushers pinned ~40y from Galvangar
    // on a COMPLETE bridged path, 0 casts; Deadmines bots stranded mid-gap at
    // (-213.6,-536.7) instead of the (-213.3,-547.5) far landing). MovePoint hands
    // the dst to the MotionMaster's own PathGenerator, which splines off-mesh
    // connections natively (Movement/PathGenerator.cpp getOffMeshConnectionPoly-
    // EndPoints). Scoped to a SHORT (<80y) in-instance approach so it can't reach
    // the FAR cross-tile MovePoint degenerate-freeze (Grimfang, ~160y) the MovePath
    // branch exists to dodge — at <80y inside a loaded instance the dst poly is
    // local, so MovePoint resolves a real spline.
    const float bg_cdx = x - p_->GetPositionX();
    const float bg_cdy = y - p_->GetPositionY();
    const bool short_offmesh_approach =
        (p_->InBattleground() || (p_->GetMap() && p_->GetMap()->IsDungeon())) &&
        (bg_cdx * bg_cdx + bg_cdy * bg_cdy) < 80.0f * 80.0f;
    if (!short_offmesh_approach && !p_->IsInCombat() && !p_->IsFlying())
    {
        auto const& pts = path.GetPath();
        if (pts.size() >= 2)
        {
            std::vector<WaypointNode> nodes;
            nodes.reserve(pts.size());
            uint32 nid = 0;
            for (G3D::Vector3 const& pp : pts)
                nodes.emplace_back(nid++, pp.x, pp.y, pp.z);

            WaypointPath cmpPath(0, std::move(nodes),
                run ? WaypointMoveType::Run : WaypointMoveType::Walk);
            cmpPath.BuildSegments();

            p_->GetMotionMaster()->MovePath(cmpPath, /*repeatable*/ false, {}, {},
                run ? MovementWalkRunSpeedSelectionMode::ForceRun
                    : MovementWalkRunSpeedSelectionMode::ForceWalk);
            return Result::Ok;
        }
    }

    if (dbg_gap1)
    {
        auto const& dpts = path.GetPath();
        float maxseg = 0.0f;
        for (size_t i = 1; i < dpts.size(); ++i)
        {
            const float sx = dpts[i].x - dpts[i-1].x, sy = dpts[i].y - dpts[i-1].y,
                        sz = dpts[i].z - dpts[i-1].z;
            maxseg = std::max(maxseg, std::sqrt(sx*sx + sy*sy + sz*sz));
        }
        auto const mt_now = p_->GetMotionMaster()->GetCurrentMovementGeneratorType();
        TC_LOG_INFO("playerbot.v2",
            "[gap1] {} MOVEPOINT pos=({:.1f},{:.1f}) dst=({:.1f},{:.1f},{:.1f}) "
            "ptype=0x{:x} npts={} maxseg={:.1f}y short_offmesh={} mtype_before={}",
            p_->GetName(), p_->GetPositionX(), p_->GetPositionY(), x, y, z,
            (uint32)pt, (uint32)dpts.size(), maxseg, short_offmesh_approach ? 1 : 0,
            (int)mt_now);
    }
    p_->GetMotionMaster()->MovePoint(0, x, y, z, /*generatePath*/ true);
    return Result::Ok;
}

bool API::TryTerrainWalkFallback(float x, float y, float z, bool run)
{
    Map* mm = p_->GetMap();
    if (!mm) return false;

    // Per-emit skim cap. This bridges navmesh HOLES (canopy/walkway occlusion
    // during gen, elevated WMO platforms Recast skipped, bridges) where VMAP
    // collision EXISTS but no navmesh poly does. We no longer REFUSE a target
    // beyond this range — instead we crawl toward it one capped segment per
    // emit and let the next tick (re-issued from the advanced pose) continue,
    // so the bot "walks forward bit by bit" across an arbitrarily large
    // continuous walkable surface (e.g. the 73y Orgrimmar zeppelin-tower
    // platform that has collision but no mesh). Each segment is still validated
    // step-by-step below, so the crawl STOPS the instant the line hits a wall,
    // ledge, void, or hazard — it never walks the bot off an edge or through
    // geometry. Long-range ROUTING still belongs to the pathfinder/travel graph;
    // this only takes over once Detour has already returned NoPath/FarFromPoly.
    constexpr float kFallbackRange = 40.0f;
    // Max ground-height delta between consecutive samples. Above this we're at
    // a cliff edge / wall face / steep ramp, NOT a flat hole — refuse so we
    // never walk off a ledge, through a wall, or down a fall.
    // NOTE (2026-06-09): a 5y asymmetric (up) variant was tried to climb the Org
    // lift RAMP but ROLLED BACK — it let bots climb onto elevator platforms they
    // then couldn't step off. Elevator ramp/platform traversal is being designed
    // properly (travel-graph-defined). See docs/ELEVATOR_TRAVEL_HANDOVER_20260609.md.
    constexpr float kStepClimb = 3.0f;
    // Relaxed DOWNWARD per-sample drop allowed only for a BG descent (see
    // bg_descent below). The EotS floating start platform has a steep rim:
    // live capture showed a 13.4y step down in the first 4y sample
    // (z1232.4 -> g1219) onto the descent slope, then a gentler run to the
    // arena floor (~z1190). 25y clears the rim with margin while still
    // refusing a genuinely sheer >25y plunge. Safe because in a BG the bot
    // only ever DESCENDS (start / graveyard), so a steep step can't strand it
    // riding geometry up, and the void/hazard gates still apply per sample.
    constexpr float kStepClimbBgDown = 25.0f;
    constexpr float kSampleStep = 4.0f;

    // Upper sanity bound on the TARGET distance. The crawl bridges LOCAL holes
    // (platforms, bridges, mesh gaps) — it must never become long-haul routing
    // across the open world toward a far target Detour wrongly NoPath'd over flat
    // terrain. Beyond this the travel graph / taxi own the trip; refuse so the
    // bot gives up (NotePathFail) and re-routes instead of wandering. 150y
    // comfortably covers the largest WMO platforms (the Org tower deck is ~73y).
    constexpr float kMaxCrawlTarget = 150.0f;
    // BG start-platform / graveyard DESCENT (2026-06-22, owner-directed). In a
    // battleground a bot only ever moves DOWN off an elevated start or respawn
    // platform — never UP — so the elevator-stranding failure that retired the
    // general crawl-on-Incomplete (see the move_to non-advancing branch) cannot
    // occur here. Two BG-only relaxations let the crawl carry a bot down the
    // unmeshed descent ramp onto the arena floor (where navmesh routing resumes):
    //   * lift the local-hole TARGET cap — the descent's true goal is a far
    //     objective (an EotS tower ~250y across the arena). Terrain-walk only
    //     engages where Detour already FAILED (the disconnected platform edge);
    //     the field below is meshed, so a far target can't degenerate this into
    //     open-world long-haul — the crawl ends the instant the bot lands on a
    //     navmesh poly and the next emit re-paths normally.
    //   * allow a larger DOWNWARD step (applied at the per-sample gate below);
    //     UP steps keep the tight 3y limit so nothing climbs onto geometry.
    const bool bg_descent = p_->InBattleground();
    const float maxCrawlTarget = bg_descent ? 600.0f : kMaxCrawlTarget;
    const float sx = p_->GetPositionX(), sy = p_->GetPositionY(), sz = p_->GetPositionZ();
    const float dx = x - sx, dy = y - sy;
    const float dist = std::sqrt(dx * dx + dy * dy);

    // Throttled INFO diagnostic (one line per bot per ~5s) so we can SEE whether
    // the crawl engages or why it refuses, WITHOUT the per-tick [terrain_walk]
    // DEBUG flood. Reveals exactly what GetHeight returns on problem surfaces
    // (e.g. the Org tower deck: void? cliff-drop to ground? hazard?).
    static std::unordered_map<uint64, uint32> s_twLogAt;
    uint64 const tw_gl  = p_->GetGUID().GetCounter();
    uint32 const tw_now = GameTime::GetGameTimeMS();
    auto tw_log = [&](char const* verdict, float atx, float aty, float g)
    {
        uint32& at = s_twLogAt[tw_gl];
        if (at != 0 && tw_now - at < 5000) return;
        at = tw_now ? tw_now : 1u;
        TC_LOG_INFO("playerbot.v2",
            "[terrain_walk] {} {} src=({:.1f},{:.1f},{:.1f}) dst=({:.1f},{:.1f},{:.1f}) "
            "dist={:.1f} at=({:.1f},{:.1f}) g={:.1f}",
            p_->GetName(), verdict, sx, sy, sz, x, y, z, dist, atx, aty, g);
    };

    if (dist < 1.0f || dist > maxCrawlTarget)
    {
        if (dist >= 1.0f) tw_log("refuse:too_far", x, y, 0.f);
        return false;
    }

    // Aim at the destination directly when it's within one segment, otherwise at
    // an intermediate point kFallbackRange along the line — the next emit picks
    // up the remainder. The per-step validation is identical either way.
    const float reach = std::min(dist, kFallbackRange);
    const float tgtX  = sx + (dx / dist) * reach;
    const float tgtY  = sy + (dy / dist) * reach;

    int const steps = std::max(2, int(reach / kSampleStep));
    float prevZ = sz;
    float endZ  = sz;
    for (int i = 1; i <= steps; ++i)
    {
        float const t  = float(i) / float(steps);
        float const px = sx + (tgtX - sx) * t;
        float const py = sy + (tgtY - sy) * t;
        // Continuous heightfield (terrain + vmap). Search downward from just
        // above the last sample so we follow the ground, not a high vmap roof.
        float const g = mm->GetHeight(p_->GetPhaseShift(), px, py, prevZ + 5.0f, /*vmap*/ true);
        if (g <= INVALID_HEIGHT)
        {
            tw_log("refuse:void", px, py, g);              // genuine void — nothing to walk on
            return false;
        }
        // Cliff/wall/drop guard. UP steps always use the tight kStepClimb (3y)
        // so nothing climbs onto a ledge/elevator. DOWN steps are relaxed in a
        // BG (bg_descent): a bot leaving its start/respawn platform may need to
        // walk a steep ramp — or take a short controlled drop — onto the arena
        // floor, and it can only ever go DOWN there, so a steeper descent can't
        // strand it. The void + hazard checks below still apply, so it never
        // crawls into nothing or into magma/deep water.
        const float dzStep = g - prevZ;                    // <0 = descending
        const float climbLimit =
            (bg_descent && dzStep < 0.f) ? kStepClimbBgDown : kStepClimb;
        if (std::fabs(dzStep) > climbLimit)
        {
            tw_log("refuse:cliff", px, py, g);             // cliff / wall / steep ramp / drop
            return false;
        }
        // Same per-point + depth water policy as the main move_to gate, via the
        // shared helper. A skim sample that lands in magma/slime or in water
        // deep enough to submerge the bot is refused (hazard / drown); wading
        // depth is fine.
        if (BotMovement::IsLiquidImpassableForWalk(
                BotMovement::ClassifyDestinationLiquid(p_, px, py, g)))
        {
            tw_log("refuse:hazard", px, py, g);            // hazard / drown
            return false;
        }
        prevZ = g;
        if (i == steps) endZ = g;                          // validated ground at the segment end
    }

    // The segment is walkable, near-level ground. Straight-skim it
    // (generatePath=false => no Detour). The near-level gate keeps the linear-Z
    // spline hugging the real ground.
    if (p_->IsWalking() == run)
        p_->SetWalk(!run);
    p_->GetMotionMaster()->MovePoint(0, tgtX, tgtY, endZ, /*generatePath*/ false);
    tw_log("skim", tgtX, tgtY, endZ);
    return true;
}

// BG start-platform / graveyard STEP-DOWN (2026-06-22, owner-directed).
// The EotS start platforms OVERHANG the arena: the platform edge toward the
// objective has only void below it (GetHeight returns nothing within range), and
// no continuous walkable ramp exists for a crawl to follow — players reach the
// field by dropping off a designed exit, not by walking. Trying to crawl the
// descent is the wrong model. Instead, do exactly what the elevator-disembark
// does: find the FIELD navmesh poly in front of the platform (toward the
// objective, at the objective's field height) and relocate the bot onto it, then
// let normal navmesh routing take over. This is a SHORT, designed platform→field
// step (the BG's intended start), not a long-distance stuck-rescue, and it only
// ever moves the bot DOWN onto the arena floor it is trying to reach.
bool API::TryBgDescentCrawl(float x, float y, float z, bool run)
{
    (void)run;
    if (!p_ || !p_->InBattleground()) return false;
    Map* mm = p_->GetMap();
    if (!mm) return false;

    const float sx = p_->GetPositionX(), sy = p_->GetPositionY(), sz = p_->GetPositionZ();
    // Only engage for a genuine DESCENT (the objective is well below the bot, i.e.
    // it is stranded up on a start/respawn platform).
    if (z > sz - 8.0f) return false;

    float dx = x - sx, dy = y - sy;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < 1.0f) return false;
    dx /= d; dy /= d;

    static std::unordered_map<uint64, uint32> s_bgdLogAt;
    uint64 const gl  = p_->GetGUID().GetCounter();
    uint32 const now = GameTime::GetGameTimeMS();
    auto bgd_log = [&](char const* verdict, float ex, float ey, float ez)
    {
        uint32& at = s_bgdLogAt[gl];
        if (at != 0 && now - at < 5000) return;
        at = now ? now : 1u;
        TC_LOG_INFO("playerbot.v2",
            "[bg_descent] {} {} src=({:.1f},{:.1f},{:.1f}) goal=({:.1f},{:.1f},{:.1f}) "
            "land=({:.1f},{:.1f},{:.1f})",
            p_->GetName(), verdict, sx, sy, sz, x, y, z, ex, ey, ez);
    };

    // Step toward the objective and look for the field poly just in front of the
    // platform. Several forward distances so we clear the platform footprint /
    // overhang regardless of how far the lip extends. Search AT the objective's
    // field height with a modest vertical extent so NearestNavPoint grabs the
    // arena floor, not the platform poly the bot is standing on.
    for (float fwd : { 25.0f, 40.0f, 55.0f, 70.0f, 90.0f })
    {
        const float lx = sx + dx * fwd;
        const float ly = sy + dy * fwd;
        Position land;
        if (!Playerbot::BotMovement::NearestNavPoint(
                p_, lx, ly, z, /*hxy*/ 25.0f, /*hz*/ 12.0f, land))
            continue;
        // Must be a real step DOWN onto the field (not the platform we are on).
        if (land.GetPositionZ() > sz - 8.0f)
            continue;
        mm->PlayerRelocation(p_, land.GetPositionX(), land.GetPositionY(),
                             land.GetPositionZ(), p_->GetOrientation());
        p_->SetFallInformation(0, land.GetPositionZ());
        bgd_log("stepdown", land.GetPositionX(), land.GetPositionY(),
                land.GetPositionZ());
        return true;
    }
    bgd_log("no_landing", sx, sy, sz);   // no field poly found in front of the platform
    return false;
}

Result API::stop_movement(bool clear_generators)
{
    if (!p_) return Result::Other;
    if (!clear_generators)
    {
        p_->StopMoving();
        return Result::Ok;
    }
    // A taxi flight owns the FLIGHT generator and IS the bot's intended
    // movement — never tear it down here (the caller wants stale movers gone,
    // not a legitimate flight cancelled mid-air).
    if (p_->IsInFlight())
        return Result::Ok;
    // Pop EVERY generator back to Idle. StopMoving() alone only halts the
    // current spline; a POINT/CHASE/FOLLOW generator survives and re-splines
    // toward its stored (possibly cross-map, now-meaningless) target on the
    // next Update. Clear() + MoveIdle() is the mechanism that actually drops it
    // (same sequence the pet stay-command path uses).
    p_->StopMoving();
    p_->GetMotionMaster()->Clear();
    p_->GetMotionMaster()->MoveIdle();
    return Result::Ok;
}

Result API::start_attack(ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    if (target.IsEmpty()) return Result::InvalidTarget;

    Unit* tu = ObjectAccessor::GetUnit(*p_, target);
    if (!tu) return Result::InvalidTarget;
    if (tu == p_) return Result::InvalidTarget;
    if (!tu->IsAlive()) return Result::InvalidTarget;

    // Friendly-fire gate. Unit::Attack() itself never validates hostility —
    // it happily arms the swing timer against a groupmate — and the spell
    // engine only rejects at cast time (BAD_TARGETS), so a friendly victim
    // WEDGES: the bot keeps swinging air, member.victim publishes the
    // friendly guid, and group assist copies it fleet-wide (2026-06-11
    // Deadmines: healer wedged onto a hunter pet, combat-state contagion
    // via the pet's owner had the whole party "fighting" each other while
    // the dungeon run stalled). Refuse here, and if we are ALREADY wedged
    // on this now-invalid target, drop it so assist stops re-seeding.
    if (!p_->IsValidAttackTarget(tu))
    {
        if (p_->GetVictim() == tu)
        {
            p_->AttackStop();
            p_->SetSelection(ObjectGuid::Empty);
        }
        return Result::InvalidTarget;
    }

    // Ranged white damage. A real hunter's client casts Auto Shot (75, an
    // auto-repeat ranged spell) the moment they engage; the server then
    // re-fires it from Unit::_UpdateAutoRepeatSpell entirely server-side.
    // Bots have no client, and Player::Attack(melee=true) only arms the
    // MELEE swing timer — so until 2026-06-10 every hunter bot did ZERO
    // ranged auto-attack damage (verified live: Uraimus L13 BM, white
    // damage absent between Steady Shots). Start the auto-repeat here,
    // both on fresh engages and on the idempotent re-engage path (the
    // core cancels CURRENT_AUTOREPEAT_SPELL when a CheckCast fails, e.g.
    // a brief LOS break — re-arming on the next engage tick restores it).
    // Spell 75 is special-cased in _UpdateAutoRepeatSpell to coexist with
    // hard casts (Steady Shot) and movement, matching retail behavior.
    auto ensureAutoShot = [&](Unit* victim)
    {
        constexpr uint32 AUTO_SHOT = 75;
        if (p_->GetClass() != CLASS_HUNTER) return;
        if (p_->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)) return;
        if (!p_->HasSpell(AUTO_SHOT)) return;
        if (!p_->GetWeaponForAttack(RANGED_ATTACK, true)) return;
        p_->CastSpell(victim, AUTO_SHOT, false);
    };

    // Idempotency fast-path: if we are already attacking this exact target,
    // do not re-call Attack or re-init MoveChase. Both have visible side
    // effects every tick — MoveChase resets the chase generator, clearing
    // the spline and forcing a path recalc. Calling it every tick (which
    // the AI does as long as the engage rule keeps firing on the same
    // victim) produces the visible "stutter": bot stops, plans, takes one
    // step, stops again. Detect the no-op case and return Ok.
    // Mirror what a player click does: keep the selection in sync with the
    // victim. NOTE: Player::SetTarget is an EMPTY override ("does not apply
    // to players") — selection is normally client-driven via
    // CMSG_SET_SELECTION → SetSelection. Headless bots never send that
    // packet, so until 2026-06-11 UNIT_FIELD_TARGET stayed permanently
    // empty on every bot: other players selecting a bot saw no
    // target-of-target frame, and anything else reading the field
    // (assist macros, UnitIsUnit comparisons client-side) misbehaved.
    // MUST run BEFORE the idempotency fast-path: a victim acquired through
    // any other route (Charge-style attack spells, core melee engage)
    // means every subsequent engage tick takes the early return — with the
    // selection-set below it, UNIT_FIELD_TARGET never caught up (user
    // re-report 2026-06-11 Stockades).
    if (p_->GetTarget() != tu->GetGUID())
        p_->SetSelection(tu->GetGUID());

    // Idempotency fast-path: if we are already attacking this exact target,
    // do not re-call Attack or re-init MoveChase. Both have visible side
    // effects every tick — MoveChase resets the chase generator, clearing
    // the spline and forcing a path recalc. Calling it every tick (which
    // the AI does as long as the engage rule keeps firing on the same
    // victim) produces the visible "stutter": bot stops, plans, takes one
    // step, stops again. Detect the no-op case and return Ok.
    if (p_->GetVictim() == tu)
    {
        ensureAutoShot(tu);
        return Result::Ok;
    }

    // Face the target. WoW's spell engine rejects casts that aren't in
    // the caster's front arc (HasInArc(M_PI, target) ~= ±90° cone). Without
    // an explicit orientation update the bot may have last faced its
    // previous travel waypoint or last npc dialog, causing every cast
    // to fail with SpellCastResult=126 (NOT_INFRONT) until MotionMaster
    // happens to rotate the bot. Verified 2026-05-20 (Halinen, hunter):
    // Steady Shot at 23.6y rejected with code 126 because the bot was
    // sideways to the Nightsaber. SetFacingToObject snaps orientation
    // and sends SMSG_MOVE_SET_FACING so clients see the turn too.
    p_->SetFacingToObject(tu);

    // Ranged-spec hunters with a usable ranged weapon engage RANGED-only:
    // Attack(melee=true) arms the melee swing timer, and TC happily swings
    // it whenever the victim stands in reach — so in tight corridors
    // (Stockades, user report 2026-06-11) the hunter PUNCHED mobs between
    // shots. Modern hunters have no minimum range and never melee outside
    // Survival; meleeAttack=false still sets the victim/combat state while
    // Auto Shot (ensureAutoShot below) carries the white damage.
    const bool ranged_hunter_engage =
        p_->GetClass() == CLASS_HUNTER &&
        p_->GetPrimarySpecialization() != ChrSpecialization::HunterSurvival &&
        p_->GetWeaponForAttack(RANGED_ATTACK, true) != nullptr;

    if (!p_->Attack(tu, /*meleeAttack*/ !ranged_hunter_engage))
        return Result::ServerRefused;

    ensureAutoShot(tu);

    // Real Players chase the target via client input; an AI-driven bot Player
    // has no client and Player::Attack alone doesn't move them. Without an
    // explicit MoveChase the bot just stands at whatever range the engage
    // rule spotted them — typically 30–40y — which breaks both melee
    // (no swings, no combat ignites) and casters (cast fails OOR / LOS).
    //
    // Class-aware chase distance: melee classes close to weapon range; pure
    // ranged classes stop at ~25y so they have a clean window for hard-cast
    // openers (Frostbolt / Shadow Bolt / Steady Shot all 30y range).
    // Hybrids (Druid/Shaman/Evoker) default to melee here — their caster
    // specs will switch to a ranged stance via the APL once it picks up,
    // and melee specs stay engaged.
    bool is_pure_caster = false;
    bool is_short_range_caster = false;   // 25y-range classes (Evoker)
    switch (p_->GetClass())
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            is_pure_caster = true;
            break;
        case CLASS_HUNTER:
            // BM / MM are ranged-only since the BfA artifact removal, but
            // SURVIVAL has been a melee spec since Legion — parking an SV
            // bot at 28y makes every Raptor Strike / Mongoose Bite fail
            // OOR and leaves only Auto Shot-less wand-range idling. Spec
            // check instead of class check; pre-L10 (spec None) hunters
            // default to ranged, which matches their Steady Shot kit.
            is_pure_caster =
                p_->GetPrimarySpecialization() != ChrSpecialization::HunterSurvival;
            break;
        case CLASS_EVOKER:
            // Evoker is pure-caster like Mage/Hunter (no melee spec —
            // Devastation/Augmentation/Preservation are all caster) BUT
            // baseline range is only 25y (Living Flame, Disintegrate,
            // Azure Strike). The 28y max used for the other casters
            // would leave Evoker OUT of cast range. Separate flag → 23y
            // max keeps the bot inside Living Flame range with 2y safety.
            is_pure_caster = true;
            is_short_range_caster = true;
            break;
        default:
            break;
    }
    if (is_pure_caster)
    {
        // MinRange=0 — never back-pedal. ChaseRange(min=5, max=28) had the
        // bot moving backwards whenever a melee mob walked inside 5y, which
        // kept is_moving=true and starved every hard cast (cast attempts
        // surfaced as SPELL_FAILED_MOVING in the log). Real Frost Mages
        // accept being meleed during a cast and rely on Frost Nova / Ice
        // Block / Slow rather than chase-distance kiting; matching that
        // behavior here also gets cast-while-melee'd consistently working.
        // MaxRange=28 still keeps the bot inside Frostbolt / Shadow Bolt /
        // Steady Shot's 30y range with a small safety margin; Evoker
        // (25y range baseline) gets a tighter 23y to stay in range.
        const float max_chase = is_short_range_caster ? 23.0f : 28.0f;
        p_->GetMotionMaster()->MoveChase(tu, ChaseRange(0.0f, max_chase));
    }
    else
        p_->GetMotionMaster()->MoveChase(tu);
    return Result::Ok;
}

Result API::stop_attack(bool clear_ghost_combat)
{
    if (!p_) return Result::Other;
    p_->AttackStop();
    // Mirror a real client deselecting on disengage — keeps the published
    // UNIT_FIELD_TARGET truthful for other players' target-of-target frames.
    p_->SetSelection(ObjectGuid::Empty);
    // Ghost-combat self-heal: a stuck combat flag (combat refs to an
    // unreachable/evaded mob) with NO attackers means nothing is actually
    // fighting the bot — drop the PvE refs so in_combat consumers (the
    // tank's member-in-combat advance gate, drink/food rules, vendor
    // gating) stop reading a fight that doesn't exist. Guarded on the
    // attacker list so a bot in a REAL fight can never combat-drop: with
    // live attackers their refs re-establish combat instantly anyway.
    if (clear_ghost_combat && p_->getAttackers().empty() && p_->IsInCombat())
    {
        TC_LOG_INFO("playerbot.v2",
            "[ghost_combat] {} clearing wedged combat state (no victim, no attackers)",
            p_->GetName());
        p_->CombatStop(/*includingCast*/ false);
        // Also drop the bot's PET/minion combat. CombatStop(owner) purges the
        // owner's PvE refs, but a controlled unit (hunter/warlock pet, guardian)
        // still fighting an EVADED / out-of-scan mob keeps re-flagging the OWNER
        // InCombat every tick — so clearing only the owner never sticks (observed
        // 2026-06-16: 105 bots cycling [ghost_combat] ~19x each, never escaping).
        // The ghost-combat gate above already excluded the case where a nearby
        // enemy is genuinely fighting the bot or its pet (group_engaged), so the
        // pet here is fighting nothing reachable — stopping its combat is correct
        // and lets the owner's clear hold.
        for (Unit* minion : p_->m_Controlled)
            if (minion && minion->IsInCombat() && minion->getAttackers().empty())
                minion->CombatStop(/*includingCast*/ false);
    }
    return Result::Ok;
}

Result API::cancel_cast()
{
    if (!p_) return Result::Other;
    p_->InterruptNonMeleeSpells(/*withDelayed*/ true);
    return Result::Ok;
}

Result API::follow(ObjectGuid leader, float distance, float angle_radians)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    if (leader.IsEmpty() || leader == p_->GetGUID()) return Result::InvalidTarget;

    Unit* target = ObjectAccessor::GetUnit(*p_, leader);
    if (!target) return Result::InvalidTarget;

    // ChaseAngle is the relative angle in the leader's local frame.
    // 0 = directly behind, pi/2 = right flank etc. ChaseAngle has a
    // (relative_angle, tolerance) ctor; the tolerance is how much the
    // bot may deviate before re-aligning. ~0.5 rad keeps the slot
    // visually stable without re-pathing every yard.
    p_->GetMotionMaster()->MoveFollow(target, distance,
                                      ChaseAngle(angle_radians, 0.5f));
    return Result::Ok;
}

Result API::dismount()
{
    if (!p_) return Result::Other;
    if (!p_->IsMounted()) return Result::Ok;
    p_->Dismount();
    return Result::Ok;
}

Result API::mount(uint32 mount_id)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;
    if (p_->IsMounted())  return Result::Ok;
    if (p_->IsInWater() || p_->IsFlying()) return Result::Locked;
    // Indoors / instances generally block mounts; let the spell engine
    // produce the proper Result if so.
    if (mount_id == 0)
    {
        // Pick the first known spell that applies SPELL_AURA_MOUNTED. We
        // scan the player's spell map looking for an effect that applies
        // the mount aura. Linear pass; runs only on explicit /mount intent
        // so the per-cast cost is irrelevant.
        for (auto const& [sid, ps] : p_->GetSpellMap())
        {
            if (!ps.active || ps.disabled) continue;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, p_->GetMap()->GetDifficultyID());
            if (!si) continue;
            bool is_mount = false;
            for (SpellEffectInfo const& eff : si->GetEffects())
                if (eff.ApplyAuraName == SPELL_AURA_MOUNTED) { is_mount = true; break; }
            if (is_mount) { mount_id = sid; break; }
        }
        if (mount_id == 0) return Result::NotKnown;
    }
    return cast_spell(mount_id, p_->GetGUID());
}

// ---- Vehicle ----
Result API::enter_vehicle(ObjectGuid vehicle, int8 seat_id)
{
    if (!p_) return Result::Other;
    if (vehicle.IsEmpty()) return Result::InvalidTarget;
    Unit* base = ObjectAccessor::GetUnit(*p_, vehicle);
    if (!base) return Result::InvalidTarget;
    Vehicle* veh = base->GetVehicleKit();
    if (!veh) return Result::InvalidTarget;
    if (p_->IsInCombat() && !base->IsFriendlyTo(p_)) return Result::Locked;
    // Interact-range gate: vehicles in WoW require ~5y to mount via spell.
    // The triggered VEHICLE_SPELL_RIDE_HARDCODED has its own range check;
    // we mirror it here so the move-to step gates correctly upstream.
    if (p_->GetExactDist(base) > 8.0f) return Result::OutOfRange;
    // Already on this vehicle, in this seat?
    if (p_->GetVehicle() == veh && (seat_id < 0 || p_->GetTransSeat() == seat_id))
        return Result::Ok;
    p_->EnterVehicle(base, seat_id);
    return Result::Ok;
}

Result API::exit_vehicle()
{
    if (!p_) return Result::Other;
    if (!p_->GetVehicle()) return Result::Ok;  // already off
    p_->ExitVehicle();
    return Result::Ok;
}

Result API::cast_vehicle_spell(uint32 spell_id, ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (!p_->GetVehicle()) return Result::Locked;
    // The seat ability is cast by the Vehicle Unit (the base), not the
    // passenger. cast_spell is keyed to the player; we route via the
    // vehicle base unit so VEHICLE_AURA_TYPE_FORCED_SUMMON_BY_SEAT and
    // similar seat-bound spell flags work.
    Unit* base = p_->GetVehicleBase();
    if (!base) return Result::Locked;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, base->GetMap()->GetDifficultyID());
    if (!si) return Result::NotKnown;
    Unit* tgt = nullptr;
    if (!target.IsEmpty())
        tgt = ObjectAccessor::GetUnit(*base, target);
    base->CastSpell(tgt, spell_id, false);
    return Result::Ok;
}

Result API::cast_vehicle_spell_at(uint32 spell_id, float x, float y, float z)
{
    if (!p_) return Result::Other;
    if (!p_->GetVehicle()) return Result::Locked;
    Unit* base = p_->GetVehicleBase();
    if (!base) return Result::Locked;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, base->GetMap()->GetDifficultyID());
    if (!si) return Result::NotKnown;
    // IoC/SoTA siege weapons (demolisher Hurl Boulder 67440, siege-engine Ram /
    // cannon, catapult) are CATAPULT/GROUND-targeted AoE — they land at a
    // destination and apply siege damage to buildings in the blast. So cast at
    // the gate's POSITION (NOT a GO target — that fails for ground spells; the
    // 600k-HP keep gates breach under sustained siege fire).
    SpellCastResult sr = base->CastSpell(Position{x, y, z}, spell_id, false);
    switch (sr)
    {
        case SPELL_FAILED_OUT_OF_RANGE: return Result::OutOfRange;
        case SPELL_FAILED_NOT_READY:    return Result::NotReady;
        default:                        return Result::Ok;
    }
}

Result API::party_chat(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    Group* group = p_->GetGroup();
    if (!group) return Result::Locked;

    // Mirror ChatHandler::CHAT_MSG_PARTY: leader-tagged variant when sender is
    // the group leader, party-broadcast otherwise. Original-group takes
    // precedence so battleground party messages don't leak to the BG group.
    Group* og = p_->GetOriginalGroup();
    if (og) group = og;
    ChatMsg type = group->IsLeader(p_->GetGUID()) ? CHAT_MSG_PARTY_LEADER : CHAT_MSG_PARTY;

    WorldPackets::Chat::Chat packet;
    packet.Initialize(type, LANG_UNIVERSAL, p_, nullptr, text);
    group->BroadcastPacket(packet.Write(), false, group->GetMemberGroup(p_->GetGUID()));
    return Result::Ok;
}

Result API::unstuck(float distance)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::ServerRefused;
    if (p_->IsNonMeleeSpellCast(false, true, true)) return Result::ServerRefused;
    // SafeNearTeleport below runs a synchronous PathGenerator rescue — gate it on
    // the per-tick pathfinding budget so unstuck storms can't pile onto the world
    // thread; already returns Locked on rescue failure so deferral is contract-safe.
    if (!PathBudget::HasBudget(GameTime::GetGameTimeMS())) return Result::Locked;
    const float orient = p_->GetOrientation();
    const float nx = p_->GetPositionX() + std::cos(orient) * distance;
    const float ny = p_->GetPositionY() + std::sin(orient) * distance;
    // SafeNearTeleport: Z-snaps via the vmap/mmap composite + falls back
    // to GetHeight, then runs a PathGenerator FARFROMPOLY rescue so the
    // bot doesn't unstuck into an off-mesh wall.
    if (!BotMovement::SafeNearTeleport(p_, nx, ny, p_->GetPositionZ(), orient))
        return Result::Locked;
    return Result::Ok;
}

Result API::near_teleport_to(float x, float y, float z, float o)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::ServerRefused;
    if (p_->IsNonMeleeSpellCast(false, true, true)) return Result::ServerRefused;
    // SafeNearTeleport runs a synchronous PathGenerator FARFROMPOLY rescue — gate
    // on the per-tick pathfinding budget (low-volume rescue, rarely trips). Already
    // returns Locked on failure so deferral is contract-safe.
    if (!PathBudget::HasBudget(GameTime::GetGameTimeMS())) return Result::Locked;
    // SafeNearTeleport handles Z snap (vmap/mmap composite, then
    // GetWaterOrGroundLevel, then top-down GetHeight), the BIH overlap
    // exception race (crash 2026-05-13), AND a FARFROMPOLY rescue so a
    // caller-supplied off-mesh waypoint gets re-snapped onto the navmesh
    // before NearTeleportTo lands the bot.
    if (!BotMovement::SafeNearTeleport(p_, x, y, z, o))
        return Result::Locked;
    return Result::Ok;
}

Result API::cancel_aura(uint32 spell_id)
{
    if (!p_) return Result::Other;
    if (!p_->HasAura(spell_id)) return Result::NotKnown;
    p_->RemoveAurasDueToSpell(spell_id);
    return Result::Ok;
}

Result API::guild_chat(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::Locked;
    guild->BroadcastToGuild(p_->GetSession(), /*officerOnly*/ false, text, LANG_UNIVERSAL);
    return Result::Ok;
}

Result API::officer_chat(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::Locked;
    guild->BroadcastToGuild(p_->GetSession(), /*officerOnly*/ true, text, LANG_UNIVERSAL);
    return Result::Ok;
}

Result API::toggle_pvp()
{
    // Mirrors HandleTogglePvP: flip the explicit PvP flag, then UpdatePvP
    // re-evaluates the actual flag state (zone overrides, sanctuary, etc).
    if (!p_) return Result::Other;
    p_->UpdatePvP(!p_->IsPvP());
    return Result::Ok;
}

Result API::add_friend(std::string const& name, std::string const& note)
{
    // Mirrors HandleAddFriendOpcode but synchronous (no async DB callback for
    // GM-permission lookup — bots use the cached AccountId directly).
    if (!p_) return Result::Other;
    std::string normalized = name;
    if (!normalizePlayerName(normalized)) return Result::InvalidTarget;
    CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(normalized);
    if (!info) return Result::InvalidTarget;
    if (info->Guid == p_->GetGUID()) return Result::InvalidTarget;
    if (Player::TeamForRace(info->Race) != p_->GetTeam()) return Result::Locked;
    if (p_->GetSocial()->HasFriend(info->Guid)) return Result::Ok;  // already
    ObjectGuid friendAccountGuid = ObjectGuid::Create<HighGuid::WowAccount>(info->AccountId);
    if (!p_->GetSocial()->AddToSocialList(info->Guid, friendAccountGuid, SOCIAL_FLAG_FRIEND))
        return Result::ServerRefused;  // friend list full
    if (!note.empty())
        p_->GetSocial()->SetFriendNote(info->Guid, note);
    return Result::Ok;
}

Result API::remove_friend(ObjectGuid friend_guid)
{
    // Mirrors HandleDelFriendOpcode. RemoveFromSocialList is permissive — if
    // the guid isn't on the list it's a no-op. Returns Ok for both branches.
    if (!p_) return Result::Other;
    if (friend_guid.IsEmpty()) return Result::InvalidTarget;
    p_->GetSocial()->RemoveFromSocialList(friend_guid, SOCIAL_FLAG_FRIEND);
    return Result::Ok;
}

Result API::add_ignore(std::string const& name)
{
    // Mirrors HandleAddIgnoreOpcode synchronously. Self-ignore rejected.
    if (!p_) return Result::Other;
    std::string normalized = name;
    if (!normalizePlayerName(normalized)) return Result::InvalidTarget;
    CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(normalized);
    if (!info) return Result::InvalidTarget;
    if (info->Guid == p_->GetGUID()) return Result::InvalidTarget;
    ObjectGuid ignoreAccountGuid = ObjectGuid::Create<HighGuid::WowAccount>(info->AccountId);
    if (p_->GetSocial()->HasIgnore(info->Guid, ignoreAccountGuid)) return Result::Ok;
    if (!p_->GetSocial()->AddToSocialList(info->Guid, ignoreAccountGuid, SOCIAL_FLAG_IGNORED))
        return Result::ServerRefused;  // ignore list full
    return Result::Ok;
}

Result API::remove_ignore(ObjectGuid ignore_guid)
{
    if (!p_) return Result::Other;
    if (ignore_guid.IsEmpty()) return Result::InvalidTarget;
    p_->GetSocial()->RemoveFromSocialList(ignore_guid, SOCIAL_FLAG_IGNORED);
    return Result::Ok;
}

Result API::mail_send_money(std::string const& recipient_name, uint64 copper,
                            std::string const& subject, std::string const& body)
{
    // Mirrors HandleSendMail's money-only path. Sender bears the 30c flat
    // postage fee; total = postage + copper. Bot must have enough money.
    // Recipient is resolved via CharacterCache (cache miss → InvalidTarget).
    if (!p_) return Result::Other;
    std::string normalized = recipient_name;
    if (!normalizePlayerName(normalized)) return Result::InvalidTarget;
    CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(normalized);
    if (!info) return Result::InvalidTarget;
    if (info->Guid == p_->GetGUID()) return Result::InvalidTarget;  // can't mail self
    constexpr uint64 kPostage = 30;  // flat fee per HandleSendMail
    uint64 cost = kPostage + copper;
    if (cost < copper) return Result::InvalidTarget;  // overflow
    if (!p_->HasEnoughMoney(cost)) return Result::NotEnoughResource;
    p_->ModifyMoney(-static_cast<int64>(cost));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    MailDraft(subject, body)
        .AddMoney(copper)
        .SendMailTo(trans, MailReceiver(info->Guid.GetCounter()),
                    MailSender(p_), MAIL_CHECK_MASK_NONE);
    p_->SaveInventoryAndGoldToDB(trans);  // persist money debit alongside the mail row
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::mail_send_item(std::string const& recipient_name, ObjectGuid item_guid, uint32 count,
                           uint64 copper, uint64 cod, std::string const& subject,
                           std::string const& body)
{
    // Mirrors HandleSendMail's item-attachment path. Resolves recipient via
    // CharacterCache (no async DB lookup), validates the item: must exist on
    // the bot, not be a non-empty bag, must CanBeTraded(true), not be a
    // wrapped item when COD is set, no expiration, not conjured. Splits the
    // stack server-side when count is below the full stack count.
    //
    // Cost: 30c per attachment + copper to send. Returns NotEnoughResource on
    // overflow or insufficient gold; InvalidTarget on bad name / missing item /
    // ineligible item / self-mail; ServerRefused on cross-faction without
    // RBAC mail permission.
    if (!p_) return Result::Other;
    if (item_guid.IsEmpty()) return Result::InvalidTarget;

    std::string normalized = recipient_name;
    if (!normalizePlayerName(normalized)) return Result::InvalidTarget;
    CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByName(normalized);
    if (!info) return Result::InvalidTarget;
    if (info->Guid == p_->GetGUID()) return Result::InvalidTarget;

    Item* item = p_->GetItemByGuid(item_guid);
    if (!item) return Result::InvalidTarget;
    if (item->IsNotEmptyBag()) return Result::Locked;
    if (!item->CanBeTraded(true)) return Result::Locked;
    if (item->GetTemplate()->HasFlag(ITEM_FLAG_CONJURED) || *item->m_itemData->Expiration)
        return Result::Locked;
    if (cod && item->IsWrapped()) return Result::Locked;

    // Faction same-team check. Account-bound items skip this; per-toon checks
    // on AccountId/BattlenetAccountId are skipped (bots are server-side; we
    // trust the caller).
    bool accountBound = item->GetTemplate()->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT);
    if (!accountBound && p_->GetTeam() != Player::TeamForRace(info->Race))
        return Result::ServerRefused;

    // 30c flat per attachment + copper to forward. Single attachment for now.
    constexpr uint64 kPostage = 30;
    uint64 cost = kPostage + copper;
    if (cost < copper) return Result::InvalidTarget;  // overflow
    if (!p_->HasEnoughMoney(cost)) return Result::NotEnoughResource;

    p_->ModifyMoney(-static_cast<int64>(cost));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Two paths. Full stack (count == 0 OR count >= existing): detach the
    // original item from inventory + DB, then re-save under the recipient's
    // ownership. Partial stack: clone the requested count, decrement the
    // surviving stack, save both. The clone path mirrors how vendor-buy split
    // and stack-split handlers work elsewhere in the core.
    Item* attachment = nullptr;
    if (count == 0 || count >= item->GetCount())
    {
        // full-stack mail
        attachment = item;
        attachment->SetNotRefundable(p_);
        p_->MoveItemFromInventory(attachment->GetBagSlot(), attachment->GetSlot(), true);
        attachment->DeleteFromInventoryDB(trans);
    }
    else
    {
        // partial-stack mail
        attachment = item->CloneItem(count, p_);
        if (!attachment)
        {
            CharacterDatabase.CommitTransaction(trans);  // releases the empty txn
            p_->ModifyMoney(static_cast<int64>(cost));   // refund — clone failed
            return Result::Other;
        }
        item->SetCount(item->GetCount() - count);
        item->SetState(ITEM_CHANGED, p_);
    }

    attachment->SetOwnerGUID(info->Guid);
    attachment->SetState(ITEM_CHANGED);
    attachment->SaveToDB(trans);

    MailDraft draft(subject, body);
    draft.AddItem(attachment);
    if (copper)
        draft.AddMoney(copper);
    if (cod)
        draft.AddCOD(cod);

    // Cross-account delivery delay only matters when sender/receiver are on
    // different game accounts; bots typically mail to one human owner so we
    // skip the delay for responsiveness. Guild members get instant mail too.
    uint32 deliver_delay = 0;
    if (Guild* guild = sGuildMgr->GetGuildById(p_->GetGuildId()))
        if (guild->IsMember(info->Guid))
            deliver_delay = 0;

    draft.SendMailTo(trans, MailReceiver(ObjectAccessor::FindConnectedPlayer(info->Guid),
                                         info->Guid.GetCounter()),
                     MailSender(p_), body.empty() ? MAIL_CHECK_MASK_COPIED : MAIL_CHECK_MASK_HAS_BODY,
                     deliver_delay);
    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::calendar_rsvp_all_pending(bool accept, uint32* out_responded)
{
    // Walks the bot's pending calendar invites (CALENDAR_STATUS_INVITED) and
    // flips each to ACCEPTED or DECLINED. Mirrors HandleCalendarEventRsvp's
    // SetStatus + SetResponseTime + UpdateInvite cycle. Persists each via
    // CalendarMgr (UpdateInvite issues a DB write per row).
    if (!p_) return Result::Other;
    CalendarInviteStore invites = sCalendarMgr->GetPlayerInvites(p_->GetGUID());
    uint32 responded = 0;
    for (CalendarInvite* invite : invites)
    {
        if (!invite) continue;
        if (invite->GetStatus() != CALENDAR_STATUS_INVITED) continue;
        invite->SetStatus(accept ? CALENDAR_STATUS_ACCEPTED : CALENDAR_STATUS_DECLINED);
        invite->SetResponseTime(GameTime::GetGameTime());
        sCalendarMgr->UpdateInvite(invite);
        ++responded;
    }
    if (out_responded) *out_responded = responded;
    return responded > 0 ? Result::Ok : Result::OutOfRange;
}

Result API::face_target(ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (target.IsEmpty()) return Result::InvalidTarget;
    Unit* u = ObjectAccessor::GetUnit(*p_, target);
    if (!u) return Result::InvalidTarget;
    p_->SetFacingToObject(u);
    return Result::Ok;
}

Result API::perform_emote(uint32 emote_id, ObjectGuid target)
{
    // Mirror Unit::HandleEmoteCommand. The target arg is forwarded for emotes
    // that point at someone (wave, salute). Pass nullptr (Empty guid) for
    // unaimed emotes (dance, sit, applaud).
    if (!p_) return Result::Other;
    Player* tgt = nullptr;
    if (!target.IsEmpty())
    {
        tgt = ObjectAccessor::GetPlayer(*p_, target);
        if (!tgt) return Result::InvalidTarget;
    }
    p_->HandleEmoteCommand(Emote(emote_id), tgt);
    return Result::Ok;
}

Result API::raid_warning(std::string const& text)
{
    // Mirrors HandleMessagechatOpcode CHAT_MSG_RAID_WARNING branch: in raid
    // groups requires leader/assistant; in plain parties gated by
    // CONFIG_CHAT_PARTY_RAID_WARNINGS. Sends to all group members via
    // BroadcastPacket. Empty text rejected so the server never sees a stub.
    if (!p_ || text.empty()) return Result::Other;
    Group* group = p_->GetGroup();
    if (!group) return Result::Locked;
    if (group->isRaidGroup())
    {
        if (!group->IsLeader(p_->GetGUID()) && !group->IsAssistant(p_->GetGUID()))
            return Result::Locked;
    }
    else if (!sWorld->getBoolConfig(CONFIG_CHAT_PARTY_RAID_WARNINGS))
    {
        return Result::Locked;
    }
    WorldPackets::Chat::Chat packet;
    packet.Initialize(CHAT_MSG_RAID_WARNING, LANG_UNIVERSAL, p_, nullptr, text);
    group->BroadcastPacket(packet.Write(), false);
    return Result::Ok;
}

Result API::raid_chat(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    Group* group = p_->GetGroup();
    if (!group || !group->isRaidGroup()) return Result::Locked;
    Group* og = p_->GetOriginalGroup();
    if (og) group = og;
    ChatMsg type = group->IsLeader(p_->GetGUID()) ? CHAT_MSG_RAID_LEADER : CHAT_MSG_RAID;
    WorldPackets::Chat::Chat packet;
    packet.Initialize(type, LANG_UNIVERSAL, p_, nullptr, text);
    group->BroadcastPacket(packet.Write(), false);
    return Result::Ok;
}

namespace {
// SC-P3a: resolve the racial language for spoken SAY/YELL so bot chatter
// renders in faction script (Common/Orcish, garbled cross-faction) exactly
// like a real player's — instead of LANG_UNIVERSAL, which every faction can
// read and which the client tags as GM/system speech. A team-based default
// is used (every member of a faction knows that faction's base language and
// passes the core HasSkill check): Alliance -> Common, Horde -> Orcish.
// Neutral-team edge cases (rare, e.g. unfactioned Pandaren before choice)
// fall back to Orcish, matching how those characters speak by default.
// LANG_UNIVERSAL is intentionally retained only for addon traffic and the
// group/guild channels (handled by their own faction-aware translation
// downstream).
inline Language RacialSpokenLanguage(Player const* p)
{
    return (p->GetTeam() == ALLIANCE) ? LANG_COMMON : LANG_ORCISH;
}
} // namespace

Result API::say(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    p_->Say(text, RacialSpokenLanguage(p_));
    return Result::Ok;
}

Result API::yell(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    p_->Yell(text, RacialSpokenLanguage(p_));
    return Result::Ok;
}

Result API::emote_text(std::string const& text)
{
    if (!p_ || text.empty()) return Result::Other;
    p_->TextEmote(text);
    return Result::Ok;
}

Result API::whisper(std::string const& target_name, std::string const& text)
{
    if (!p_ || target_name.empty() || text.empty()) return Result::Other;
    Player* recipient = ObjectAccessor::FindConnectedPlayerByName(target_name);
    if (!recipient) return Result::InvalidTarget;
    p_->Whisper(text, LANG_UNIVERSAL, recipient);
    return Result::Ok;
}

Result API::release_corpse()
{
    if (!p_) return Result::Other;
    if (p_->IsAlive())
    {
        TC_LOG_DEBUG("playerbot.api", "[API::release_corpse] {} already alive — noop",
            p_->GetName());
        return Result::Ok;
    }
    if (p_->HasCorpse())
    {
        // A corpse already exists. That is the CORRECT post-release state only
        // when the bot is also a ghost. Dead + corpse + NOT ghost means a
        // STALE corpse survived a previous revival (or a restart-while-dead):
        // BuildPlayerRepop refuses while a same-map corpse exists, so a plain
        // noop here wedges the bot in dead:waiting_release forever (verified
        // live: L2 Kold). Self-heal: retire the stale corpse to bones, then
        // fall through to the normal repop below.
        if (p_->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        {
            TC_LOG_DEBUG("playerbot.api", "[API::release_corpse] {} already released (ghost + corpse) — noop",
                p_->GetName());
            return Result::Ok;
        }
        TC_LOG_INFO("playerbot.api",
            "[API::release_corpse] {} dead with STALE corpse at ({:.1f},{:.1f},{:.1f}) map {} and no ghost state — "
            "retiring bones and repopping",
            p_->GetName(),
            p_->GetCorpseLocation().GetPositionX(),
            p_->GetCorpseLocation().GetPositionY(),
            p_->GetCorpseLocation().GetPositionZ(),
            uint32(p_->GetCorpseLocation().GetMapId()));
        p_->SpawnCorpseBones();
    }
    TC_LOG_DEBUG("playerbot.api", "[API::release_corpse] {} releasing — BuildPlayerRepop + RepopAtGraveyard from ({:.1f},{:.1f},{:.1f})",
        p_->GetName(), p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
    p_->BuildPlayerRepop();
    p_->RepopAtGraveyard();
    TC_LOG_DEBUG("playerbot.api", "[API::release_corpse] {} after release — alive={} hasCorpse={} pos=({:.1f},{:.1f},{:.1f})",
        p_->GetName(), p_->IsAlive(), p_->HasCorpse(),
        p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
    return Result::Ok;
}

Result API::revive_at_corpse()
{
    if (!p_) return Result::Other;
    if (p_->IsAlive())
    {
        TC_LOG_DEBUG("playerbot.api", "[API::revive_at_corpse] {} already alive — noop", p_->GetName());
        return Result::Ok;
    }
    if (!p_->HasCorpse())
    {
        TC_LOG_DEBUG("playerbot.api", "[API::revive_at_corpse] {} no corpse — Locked. pos=({:.1f},{:.1f},{:.1f})",
            p_->GetName(), p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
        return Result::Locked;
    }
    TC_LOG_DEBUG("playerbot.api", "[API::revive_at_corpse] {} reviving at ({:.1f},{:.1f},{:.1f})",
        p_->GetName(), p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
    // Mirror WorldSession::HandleReclaimCorpse: 50% restore (retail corpse-
    // run penalty) and SpawnCorpseBones to retire the corpse object. The old
    // code skipped SpawnCorpseBones, so the corpse SURVIVED the revival — and
    // on the bot's NEXT death release_corpse saw HasCorpse() and no-op'd
    // forever (BuildPlayerRepop also refuses while a same-map corpse exists),
    // leaving the bot wedged in dead:waiting_release for hours (verified
    // live: L2 Kold dead at Deathknell, persisting across a server restart).
    p_->ResurrectPlayer(p_->InBattleground() ? 1.0f : 0.5f, /*applySickness*/ false);
    p_->SpawnCorpseBones();
    return Result::Ok;
}

// Mirrors WorldSession::HandleReclaimCorpse (MiscHandler.cpp:417). Called
// from State_Dead's corpse-run path when the bot has walked back to its
// corpse and is within reclaim radius. ResurrectPlayer is invoked with
// applySickness=false (the default) — the corpse-run is the canonical
// no-sickness path. SpawnCorpseBones cleans up the corpse marker.
Result API::reclaim_corpse()
{
    if (!p_) return Result::Other;
    if (p_->IsAlive())
    {
        TC_LOG_DEBUG("playerbot.api", "[API::reclaim_corpse] {} already alive — noop", p_->GetName());
        return Result::Ok;
    }
    if (p_->InArena()) return Result::Locked;
    if (!p_->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        TC_LOG_DEBUG("playerbot.api", "[API::reclaim_corpse] {} not ghost (release_corpse first) — Locked",
            p_->GetName());
        return Result::Locked;
    }
    Corpse* corpse = p_->GetCorpse();
    if (!corpse)
    {
        TC_LOG_DEBUG("playerbot.api", "[API::reclaim_corpse] {} no corpse — Locked", p_->GetName());
        return Result::Locked;
    }
    const bool pvp = corpse->GetType() == CORPSE_RESURRECTABLE_PVP;
    if (time_t(corpse->GetGhostTime() + p_->GetCorpseReclaimDelay(pvp)) > time_t(GameTime::GetGameTime()))
    {
        // Reclaim window not open yet (default 30s after release).
        return Result::Locked;
    }
    // Distinguish cross-Map corpse from same-map-too-far. The previous
    // "0.0y > 39y" log line was the symptom of bot+corpse on the same
    // map_id but different Map* (continent ↔ instance with same parent
    // map_id, e.g., Stormwind sewers / Naxx / etc), which IsWithinDistInMap
    // rejects because Map* pointers differ. The State_Dead cross-map
    // gate only compares map_id so this case slips through. Returning
    // the same OutOfRange so the rule's back-off behavior stays unchanged.
    if (corpse->GetMap() != p_->GetMap())
    {
        TC_LOG_DEBUG("playerbot.api",
            "[API::reclaim_corpse] {} corpse on different Map* (mapId={}↔{}) — OutOfRange",
            p_->GetName(), p_->GetMapId(), corpse->GetMapId());
        return Result::OutOfRange;
    }
    if (!corpse->IsWithinDistInMap(p_, CORPSE_RECLAIM_RADIUS, true))
    {
        const float d = p_->GetDistance2d(corpse);
        TC_LOG_DEBUG("playerbot.api", "[API::reclaim_corpse] {} corpse too far ({:.1f}y > {}y) — OutOfRange",
            p_->GetName(), d, CORPSE_RECLAIM_RADIUS);
        return Result::OutOfRange;
    }
    TC_LOG_DEBUG("playerbot.api", "[API::reclaim_corpse] {} reclaiming at ({:.1f},{:.1f},{:.1f})",
        p_->GetName(), p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
    p_->ResurrectPlayer(p_->InBattleground() ? 1.0f : 0.5f);
    p_->SpawnCorpseBones();
    return Result::Ok;
}

// Mirrors WorldSession::SendSpiritResurrect (NPCHandler.cpp:290). Used by
// State_Dead's spirit-healer path when corpse-run distance/level make a
// real corpse-run impractical. applySickness=true applies the standard
// resurrection sickness (level-scaled; none below 11), and DurabilityLossAll
// matches the 25% gear hit a real spirit-healer rez incurs. We skip the
// SpiritHealerActivate NPC validation — State_Dead has already chosen this
// path; there's no NPC to interact with for a headless bot.
Result API::spirit_resurrect()
{
    if (!p_) return Result::Other;
    if (p_->IsAlive())
    {
        TC_LOG_DEBUG("playerbot.api", "[API::spirit_resurrect] {} already alive — noop", p_->GetName());
        return Result::Ok;
    }
    // Ensure the bot has released first; the spirit-healer flow assumes the
    // ghost has already reached a graveyard. release_corpse is idempotent.
    if (!p_->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        p_->BuildPlayerRepop();
        p_->RepopAtGraveyard();
    }
    p_->ResurrectPlayer(0.5f, /*applySickness*/ true);
    p_->DurabilityLossAll(0.25f, true);
    // Clean up the corpse marker. SendSpiritResurrect calls SpawnCorpseBones
    // even if HasCorpse() is false (no-op in that case).
    if (p_->HasCorpse())
    {
        // Optionally teleport closer to corpse-graveyard if it differs from
        // ghost-graveyard (matches SendSpiritResurrect's last step). Cheap;
        // just compares closest grave by team for the corpse vs. ghost loc.
        WorldLocation const corpseLoc = p_->GetCorpseLocation();
        if (WorldSafeLocsEntry const* corpseGrave =
                sObjectMgr->GetClosestGraveyard(corpseLoc, p_->GetTeam(), p_))
        {
            if (WorldSafeLocsEntry const* ghostGrave =
                    sObjectMgr->GetClosestGraveyard(*p_, p_->GetTeam(), p_))
            {
                if (corpseGrave != ghostGrave)
                    BotMovement::SafeTeleport(p_, corpseGrave->Loc, /*options*/ 0);
            }
        }
    }
    p_->SpawnCorpseBones();
    return Result::Ok;
}

Result API::teleport_to(uint32 map_id, float x, float y, float z, float orientation)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    if (p_->IsInCombat())  return Result::Locked;
    if (p_->IsNonMeleeSpellCast(false, false, true)) return Result::Locked;
    // SafeTeleport probes the destination map (if loaded) for ground
    // height and snaps to ground+2 when the requested Z is >50y above
    // the queried floor (typical "stale POI / hand-curated coord" bug
    // that used to drop bots onto cliff edges with no navmesh below
    // them and spam ~60k path-fail telemetry lines).
    if (!BotMovement::SafeTeleport(p_, map_id, x, y, z, orientation, TELE_TO_GM_MODE))
        return Result::ServerRefused;
    return Result::Ok;
}

Result API::accept_summon()
{
    if (!p_) return Result::Other;
    if (!p_->HasSummonPending()) return Result::Locked;
    // SummonIfPossible re-checks the expire window and broadcasts the response
    // packet to the rest of the group. It's idempotent: a second call with
    // m_summon_expire == 0 will broadcast a "false" decline, so we only fire
    // once per pending request.
    p_->SummonIfPossible(true);
    // Post-summon snap. SummonIfPossible teleports to the summoner's
    // last-reported position — which may sit slightly off-mesh (summoner
    // moved between the summon cast and the bot accept) or inside an
    // instance portal that hasn't fully geometry-resolved. The probe is
    // cheap when the landing is fine.
    BotMovement::PostTeleportSnap(p_);
    return Result::Ok;
}

Result API::decline_summon()
{
    if (!p_) return Result::Other;
    if (!p_->HasSummonPending()) return Result::Locked;
    p_->SummonIfPossible(false);
    return Result::Ok;
}

Result API::decline_duel()
{
    if (!p_) return Result::Other;
    if (!p_->duel) return Result::Locked;
    if (p_->duel->State != DUEL_STATE_CHALLENGED) return Result::Locked;
    // Mirror HandleDuelCancelled's challenge-state branch: tear down the
    // duel structures cleanly. DuelComplete handles initiator notification
    // and arbiter cleanup.
    p_->DuelComplete(DUEL_INTERRUPTED);
    return Result::Ok;
}

Result API::reset_all_cooldowns()
{
    if (!p_) return Result::Other;
    if (SpellHistory* h = p_->GetSpellHistory())
        h->ResetAllCooldowns();
    return Result::Ok;
}

Result API::activate_spec(uint32 spec_id)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;
    ChrSpecializationEntry const* entry = sChrSpecializationStore.LookupEntry(spec_id);
    if (!entry) return Result::NotKnown;
    if (entry->ClassID != p_->GetClass()) return Result::NotKnown;
    p_->ActivateTalentGroup(entry);
    return Result::Ok;
}

Result API::set_raid_target_icon(uint8 symbol, ObjectGuid target)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (symbol >= TARGET_ICONS_COUNT) return Result::Locked;
    if (g->isRaidGroup() && !g->IsLeader(p_->GetGUID()) && !g->IsAssistant(p_->GetGUID()))
        return Result::Locked;
    if (target.IsPlayer())
    {
        Player* tp = ObjectAccessor::FindConnectedPlayer(target);
        if (!tp || tp->IsHostileTo(p_)) return Result::Locked;
    }
    g->SetTargetIcon(symbol, target, p_->GetGUID());
    return Result::Ok;
}

Result API::accept_duel()
{
    if (!p_) return Result::Other;
    if (!p_->duel) return Result::Locked;
    // Only the challengee may accept; the initiator already started the
    // arbiter cast and can't accept its own challenge.
    if (p_ == p_->duel->Initiator) return Result::Locked;
    if (p_->duel->State != DUEL_STATE_CHALLENGED) return Result::Locked;
    Player* opponent = p_->duel->Opponent;
    if (!opponent || !opponent->duel) return Result::Locked;
    // Mirrors HandleDuelAccepted: kick both sides into a 3-second countdown,
    // dispatch the countdown packet, and enable PvP rules so combat works.
    const time_t now = GameTime::GetGameTime();
    p_->duel->StartTime       = now + 3;
    opponent->duel->StartTime = now + 3;
    p_->duel->State           = DUEL_STATE_COUNTDOWN;
    opponent->duel->State     = DUEL_STATE_COUNTDOWN;
    WorldPackets::Duel::DuelCountdown packet(3000);
    WorldPacket const* pkt = packet.Write();
    if (p_->GetSession())       p_->GetSession()->SendPacket(pkt);
    if (opponent->GetSession()) opponent->GetSession()->SendPacket(pkt);
    p_->EnablePvpRules();
    opponent->EnablePvpRules();
    return Result::Ok;
}

Result API::decline_trade()
{
    if (!p_) return Result::Other;
    if (!p_->GetTradeData()) return Result::Locked;
    p_->TradeCancel(true);
    return Result::Ok;
}

Result API::accept_group_invite()
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroupInvite();
    if (!g) return Result::Locked;
    // If the invite is for a group still being formed, the leader has to be
    // present to actually create it. Mirror HandleGroupAcceptOpcode's path.
    if (g->GetLeaderGUID() == p_->GetGUID()) return Result::Other;
    if (g->IsFull()) return Result::ServerRefused;

    if (!g->IsCreated())
    {
        Player* leader = ObjectAccessor::FindPlayer(g->GetLeaderGUID());
        if (!leader) { g->RemoveAllInvites(); return Result::ServerRefused; }
        g->RemoveInvite(leader);
        g->Create(leader);
        sGroupMgr->AddGroup(g);
    }
    g->RemoveInvite(p_);
    if (!g->AddMember(p_)) return Result::ServerRefused;
    g->BroadcastGroupUpdate();
    return Result::Ok;
}

Result API::invite_to_group(ObjectGuid target_player)
{
    if (!p_) return Result::Other;
    if (target_player == p_->GetGUID()) return Result::InvalidTarget;
    Player* invitee = ObjectAccessor::FindConnectedPlayer(target_player);
    if (!invitee) return Result::InvalidTarget;
    // Faction gate matches HandlePartyInviteOpcode unless cross-faction
    // grouping is enabled server-wide. We're conservative and refuse the
    // cross-faction case rather than relying on the world config — bot
    // packs that party with opposing faction would be a PvP-server bug.
    if (p_->GetTeam() != invitee->GetTeam()) return Result::InvalidTarget;
    // Already-grouped target → server would reject; surface up-front.
    if (invitee->GetGroup() || invitee->GetGroupInvite())
        return Result::Locked;
    if (invitee->GetSocial() &&
        invitee->GetSocial()->HasIgnore(p_->GetGUID(),
                                        p_->GetSession()->GetAccountGUID()))
        return Result::Locked;

    Group* group = p_->GetGroup();
    if (group)
    {
        // Bot must lead or be assistant to invite into an existing group.
        if (!group->IsLeader(p_->GetGUID()) &&
            !group->IsAssistant(p_->GetGUID()))
            return Result::Locked;
        if (group->IsFull()) return Result::Locked;
        if (!group->AddInvite(invitee)) return Result::ServerRefused;
    }
    else
    {
        // No group yet — create one with the bot as leader-invitee. Same
        // shape as the player path: not committed to DB until the invitee
        // accepts. Memory leaked back to the heap if the invitee declines
        // is reclaimed via Group::RemoveAllInvites + delete in that path.
        group = new Group();
        if (!group->AddLeaderInvite(p_)) { delete group; return Result::ServerRefused; }
        if (!group->AddInvite(invitee))
        { group->RemoveAllInvites(); delete group; return Result::ServerRefused; }
    }

    WorldPackets::Party::PartyInvite invitePacket;
    invitePacket.Initialize(p_, /*proposedRoles=*/0, /*canAccept=*/true);
    invitee->SendDirectMessage(invitePacket.Write());
    return Result::Ok;
}

Result API::decline_group_invite()
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroupInvite();
    if (!g) return Result::Locked;
    // Mirror HandleGroupDeclineOpcode: drop the invite and notify the leader.
    Player* leader = ObjectAccessor::FindConnectedPlayer(g->GetLeaderGUID());
    g->RemoveInvite(p_);
    if (leader && leader->GetSession())
    {
        WorldPackets::Party::GroupDecline decline(p_->GetName());
        leader->SendDirectMessage(decline.Write());
    }
    return Result::Ok;
}

Result API::accept_rez()
{
    if (!p_) return Result::Other;
    if (p_->IsAlive()) return Result::Ok;            // already alive
    if (!p_->IsResurrectRequested()) return Result::Locked;
    p_->ResurrectUsingRequestData();
    return Result::Ok;
}

Result API::group_ready_response(bool ready)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    g->SetMemberReadyCheck(p_->GetGUID(), ready);
    return Result::Ok;
}

Result API::leave_group()
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    g->RemoveMember(p_->GetGUID());
    return Result::Ok;
}

Result API::promote_to_leader(ObjectGuid new_leader_guid)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (!g->IsLeader(p_->GetGUID())) return Result::Locked;
    if (!g->IsMember(new_leader_guid)) return Result::InvalidTarget;
    g->ChangeLeader(new_leader_guid);
    return Result::Ok;
}

Result API::kick_group_member(ObjectGuid member_guid)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (!g->IsLeader(p_->GetGUID())) return Result::Locked;
    if (member_guid == p_->GetGUID()) return Result::InvalidTarget;
    if (!g->IsMember(member_guid)) return Result::InvalidTarget;
    g->RemoveMember(member_guid, GROUP_REMOVEMETHOD_KICK, p_->GetGUID());
    return Result::Ok;
}

Result API::convert_to_raid()
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (!g->IsLeader(p_->GetGUID())) return Result::Locked;
    if (g->isRaidGroup()) return Result::Ok;
    g->ConvertToRaid();
    return Result::Ok;
}

Result API::start_ready_check()
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    bool is_leader = g->IsLeader(p_->GetGUID());
    bool is_assistant = g->isRaidGroup() && g->IsAssistant(p_->GetGUID());
    if (!is_leader && !is_assistant) return Result::Locked;
    g->StartReadyCheck(p_->GetGUID());
    return Result::Ok;
}

Result API::set_assistant(ObjectGuid member_guid, bool assistant)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (!g->isRaidGroup()) return Result::Locked;
    if (!g->IsLeader(p_->GetGUID())) return Result::Locked;
    if (!g->IsMember(member_guid)) return Result::InvalidTarget;
    g->SetGroupMemberFlag(member_guid, assistant, MEMBER_FLAG_ASSISTANT);
    return Result::Ok;
}

Result API::reset_instances()
{
    // Mirrors HandleResetInstancesOpcode: when grouped, only the leader can reset
    // (Group::ResetInstances handles per-member binds + notify); ungrouped uses
    // Player::ResetInstances directly. Method::Manual to differentiate from the
    // difficulty-change path that calls the same routine.
    if (!p_) return Result::Other;
    if (Group* g = p_->GetGroup())
    {
        if (!g->IsLeader(p_->GetGUID())) return Result::Locked;
        g->ResetInstances(InstanceResetMethod::Manual, p_);
        return Result::Ok;
    }
    p_->ResetInstances(InstanceResetMethod::Manual);
    return Result::Ok;
}

Result API::accept_guild_invite()
{
    // Mirrors HandleGuildAcceptInvite: short-circuits if the bot is already
    // in a guild (the server enforces this anyway, but Locked tells the caller
    // why nothing happened). The invite is stored on the player as
    // GuildIdInvited; sGuildMgr::GetGuildById resolves the inviter.
    if (!p_) return Result::Other;
    if (p_->GetGuildId()) return Result::Locked;
    Guild* guild = sGuildMgr->GetGuildById(p_->GetGuildIdInvited());
    if (!guild) return Result::Locked;
    guild->HandleAcceptMember(p_->GetSession());
    return Result::Ok;
}

Result API::decline_guild_invite()
{
    // Mirrors HandleGuildDeclineInvitation: clearing GuildIdInvited drops the
    // pending invite. Idempotent — already-cleared state still returns Ok.
    if (!p_) return Result::Other;
    if (p_->GetGuildId()) return Result::Ok;  // Already in a guild — invite is moot.
    p_->SetGuildIdInvited(UI64LIT(0));
    return Result::Ok;
}

Result API::leave_guild()
{
    if (!p_) return Result::Other;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::Locked;
    guild->HandleLeaveMember(p_->GetSession());
    return Result::Ok;
}

Result API::toggle_afk()
{
    if (!p_) return Result::Other;
    p_->ToggleAFK();
    return Result::Ok;
}

Result API::toggle_dnd()
{
    if (!p_) return Result::Other;
    p_->ToggleDND();
    return Result::Ok;
}

Result API::set_dungeon_difficulty(uint32 difficulty_id)
{
    if (!p_) return Result::Other;
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(difficulty_id);
    if (!difficultyEntry) return Result::InvalidTarget;
    if (difficultyEntry->InstanceType != MAP_INSTANCE) return Result::InvalidTarget;
    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT)) return Result::InvalidTarget;

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);
    if (difficultyID == p_->GetDungeonDifficultyID()) return Result::Ok;

    Map* map = p_->FindMap();
    if (map && map->Instanceable()) return Result::Locked;

    if (Group* group = p_->GetGroup())
    {
        if (!group->IsLeader(p_->GetGUID())) return Result::Locked;
        if (group->isLFGGroup()) return Result::Locked;
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, p_);
        group->SetDungeonDifficultyID(difficultyID);
    }
    else
    {
        p_->ResetInstances(InstanceResetMethod::OnChangeDifficulty);
        p_->SetDungeonDifficultyID(difficultyID);
        p_->SendDungeonDifficulty();
    }
    return Result::Ok;
}

Result API::set_raid_difficulty(uint32 difficulty_id, bool legacy)
{
    if (!p_) return Result::Other;
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(difficulty_id);
    if (!difficultyEntry) return Result::InvalidTarget;
    if (difficultyEntry->InstanceType != MAP_RAID) return Result::InvalidTarget;
    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT)) return Result::InvalidTarget;
    if (((difficultyEntry->Flags & DIFFICULTY_FLAG_LEGACY) != 0) != legacy)
        return Result::InvalidTarget;

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);
    Difficulty current = legacy ? p_->GetLegacyRaidDifficultyID() : p_->GetRaidDifficultyID();
    if (difficultyID == current) return Result::Ok;

    Map* map = p_->FindMap();
    if (map && map->Instanceable()) return Result::Locked;

    if (Group* group = p_->GetGroup())
    {
        if (!group->IsLeader(p_->GetGUID())) return Result::Locked;
        if (group->isLFGGroup()) return Result::Locked;
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, p_);
        if (legacy) group->SetLegacyRaidDifficultyID(difficultyID);
        else        group->SetRaidDifficultyID(difficultyID);
    }
    else
    {
        p_->ResetInstances(InstanceResetMethod::OnChangeDifficulty);
        if (legacy) p_->SetLegacyRaidDifficultyID(difficultyID);
        else        p_->SetRaidDifficultyID(difficultyID);
        p_->SendRaidDifficulty(legacy);
    }
    return Result::Ok;
}

Result API::pet_attack(ObjectGuid target)
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet || !pet->IsAlive()) return Result::Locked;
    Unit* victim = ObjectAccessor::GetUnit(*p_, target);
    if (!victim || !victim->IsAlive()) return Result::InvalidTarget;
    // PET_ACTION_ATTACK semantics: enable attack mode + start attacking the
    // selected target. Mirrors what the client emits for the pet bar attack
    // button.
    pet->SetReactState(REACT_AGGRESSIVE);
    pet->GetCharmInfo()->SetIsCommandAttack(true);
    pet->GetCharmInfo()->SetIsAtStay(false);
    pet->GetCharmInfo()->SetIsFollowing(false);
    pet->GetCharmInfo()->SetIsReturning(false);
    pet->ToCreature()->AI()->AttackStart(victim);
    return Result::Ok;
}

Result API::pet_cast(uint32 spell_id, ObjectGuid target)
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet || !pet->IsAlive()) return Result::Locked;
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!spell) return Result::Other;
    // Pet must actually know the spell — otherwise the cast just fails
    // silently server-side. HasSpell on the pet checks both the inherited
    // creature template spells and any taught (Hunter/DK) abilities.
    if (!pet->HasSpell(spell_id)) return Result::Locked;
    // CD check via the pet's own SpellHistory.
    if (!pet->GetSpellHistory()->IsReady(spell)) return Result::Locked;
    Unit* victim = nullptr;
    if (!target.IsEmpty())
    {
        victim = ObjectAccessor::GetUnit(*p_, target);
        if (!victim) return Result::InvalidTarget;
    }
    // Cast as the pet's caster — interrupts (Spell Lock) require a target;
    // self-buffs / pet abilities don't.
    SpellCastTargets targets;
    if (victim) targets.SetUnitTarget(victim);
    pet->CastSpell(victim, spell_id, false);
    return Result::Ok;
}

Result API::set_stand_state(uint8 state)
{
    if (!p_) return Result::Other;
    // Mid-cast sitting cancels the cast. Refuse to spare callers from a
    // surprise cast-cancel; cast then sit if that's what's wanted.
    if (p_->IsNonMeleeSpellCast(false, true, true)) return Result::ServerRefused;
    p_->SetStandState(static_cast<UnitStandStateType>(state));
    return Result::Ok;
}

Result API::dismiss_pet()
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    if (p_->IsInCombat()) return Result::ServerRefused;
    p_->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
    return Result::Ok;
}

// ---- Hunter pet stable -------------------------------------------------
//
// Background: in retail Cataclysm-onwards, the stable owns three flavors of
// pet — active (in slots 0..MAX_ACTIVE_PETS-1), stabled (slots
// PET_SAVE_FIRST_STABLE_SLOT..PET_SAVE_LAST_STABLE_SLOT-1), and unslotted
// (PET_SAVE_NOT_IN_SLOT, kept around for re-summon by spell). The gossip /
// stablemaster flow validates an interact range with the stable NPC. The
// bot path here intentionally bypasses the NPC range check — when running
// autonomously we want the AI to swap pets based on encounter (e.g. swap
// to a stabled tank pet for a tough pull) without first dragging the bot
// to a stablemaster. All other invariants (HUNTER_PET-only, exotic-tame
// gating, dead-active refusal, current-pet-not-deletable) match Player::
// SetPetSlot / DeletePetFromDB exactly.
Result API::swap_pet_to_slot(uint32 pet_number, uint8 dst_slot)
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    if (dst_slot >= PET_SAVE_LAST_STABLE_SLOT) return Result::OutOfRange;

    PetStable* stable = p_->GetPetStable();
    if (!stable) return Result::NotKnown;

    // Verify the source pet exists and is a HUNTER_PET. We replicate the
    // first half of Player::SetPetSlot validation here so the API surface
    // returns a precise Result rather than swallowing it into the silent
    // SendPetStableResult packet that the gossip handler emits.
    auto [srcPet, srcPetSlot] = Pet::GetLoadPetInfo(*stable, 0, pet_number, {});
    if (!srcPet || srcPet->Type != HUNTER_PET) return Result::NotKnown;

    PetSaveMode dstSlotMode = PetSaveMode(dst_slot);
    PetStable::PetInfo const* dstPet = Pet::GetLoadPetInfo(*stable, 0, 0, dstSlotMode).first;
    if (dstPet && dstPet->Type != HUNTER_PET) return Result::Locked;

    // Active<->stable swap with a dead summoned pet would lose the corpse
    // info (Player::RemovePet refuses dead-pet stable saves and instead
    // sends StableResult::NoPet). Replicate that gate.
    const bool srcActive = IsActivePetSlot(srcPetSlot);
    const bool dstStabled = IsStabledPetSlot(dstSlotMode);
    if (srcActive != IsActivePetSlot(dstSlotMode))
    {
        // Cross-region swap will despawn the active pet — must be alive.
        if (Pet* live = p_->GetPet())
            if (!live->IsAlive() && (srcActive || dstStabled))
                return Result::ServerRefused;
    }

    // Exotic taming check for inbound pet on cross-region swap. The
    // stable previously held it; if the bot has since dropped Beast
    // Mastery (Mastery talent / spec change) the dst pet would be
    // unsummonable. SetPetSlot checks this for active<->stable; for
    // active<->active and stable<->stable it's irrelevant since neither
    // side spawns. Mirror the SetPetSlot behavior.
    if (srcActive && dstStabled && dstPet)
    {
        CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(dstPet->CreatureId);
        if (!tmpl || !tmpl->IsTameable(p_->CanTameExoticPets(),
                                       tmpl->GetDifficulty(DIFFICULTY_NONE)))
            return Result::Locked;
    }

    p_->SetPetSlot(pet_number, dstSlotMode);
    return Result::Ok;
}

Result API::delete_stabled_pet(uint32 pet_number)
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    PetStable* stable = p_->GetPetStable();
    if (!stable) return Result::NotKnown;

    // Refuse to delete the currently summoned pet — caller must dismiss
    // first. Without this guard, DeletePetFromDB would yank the row from
    // under a live Pet*, leaving a dangling summon that crashes on the
    // next pet-tick (CharmInfo lookup against a deleted row).
    if (Pet* live = p_->GetPet())
        if (CharmInfo* ci = live->GetCharmInfo())
            if (ci->GetPetNumber() == pet_number)
                return Result::Locked;

    // Also refuse if the pet is the current ActivePet entry (even if not
    // physically summoned right now — e.g. unsummoned for flying mount).
    // In that case the summon path would re-resurrect a deleted DB row.
    if (PetStable::PetInfo const* current = stable->GetCurrentPet())
        if (current->PetNumber == pet_number)
            return Result::Locked;

    // Verify the pet exists in the stable before issuing the delete to
    // distinguish NotKnown from a successful no-op.
    auto [srcPet, srcSlot] = Pet::GetLoadPetInfo(*stable, 0, pet_number, {});
    if (!srcPet) return Result::NotKnown;
    (void)srcSlot;

    p_->DeletePetFromDB(pet_number);
    return Result::Ok;
}

Result API::summon_pet_by_number(uint32 pet_number)
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    if (p_->IsInCombat()) return Result::ServerRefused;
    if (p_->HasUnitState(UNIT_STATE_CASTING)) return Result::ServerRefused;
    PetStable* stable = p_->GetPetStable();
    if (!stable) return Result::NotKnown;
    // A pet is already up — caller must dismiss first.
    if (p_->GetPet()) return Result::Locked;

    // Locate pet across active slots only — stabled pets must be moved
    // to an active slot first via swap_pet_to_slot. This matches the
    // CALL_PET_SPELL_ID semantics: it loads from active-slot bank.
    auto [srcPet, srcSlot] = Pet::GetLoadPetInfo(*stable, 0, pet_number, {});
    if (!srcPet || !IsActivePetSlot(srcSlot)) return Result::NotKnown;

    // Check IsPetNeedBeTemporaryUnsummoned to refuse mid-flight / dead.
    if (p_->IsPetNeedBeTemporaryUnsummoned()) return Result::ServerRefused;

    Pet* fresh = new Pet(p_);
    if (!fresh->LoadPetFromDB(p_, 0, pet_number, true))
    {
        delete fresh;
        return Result::ServerRefused;
    }
    return Result::Ok;
}

Result API::feed_pet(uint32 food_item_entry)
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    if (!pet->IsAlive()) return Result::Locked;

    constexpr uint32 FEED_PET_SPELL_ID = 6991;
    if (!p_->HasSpell(FEED_PET_SPELL_ID)) return Result::NotKnown;

    Item* food = p_->GetItemByEntry(food_item_entry);
    if (!food) return Result::NotEnoughResource;

    ItemTemplate const* tmpl = food->GetTemplate();
    if (!tmpl) return Result::InvalidTarget;

    // The pet's Diet (CreatureFamily.PetFoodMask) plus the food's
    // FoodType decide whether this feed is legal. Mirror Pet::HaveInDiet.
    if (!pet->HaveInDiet(tmpl)) return Result::InvalidTarget;

    // Note: pet level vs food level happiness math lives in
    // SPELL_EFFECT_FEED_PET; we don't pre-gate it here. Casting Feed Pet
    // on too-low food still consumes the item but yields zero happiness,
    // matching live behavior — caller is responsible for filtering food
    // by ItemTemplate::ItemLevel vs Pet::GetLevel before issuing.

    // Cast Feed Pet on the pet itself; the spell handler consumes the
    // food item via SPELL_EFFECT_FEED_PET / EFFECT_INDEX_0.
    CastSpellExtraArgs args;
    args.SetTriggerFlags(TRIGGERED_NONE);
    args.AddSpellMod(SPELLVALUE_BASE_POINT0, int32(food_item_entry));
    p_->CastSpell(pet, FEED_PET_SPELL_ID, args);
    return Result::Ok;
}

Result API::abandon_pet()
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    if (pet->getPetType() != HUNTER_PET) return Result::InvalidTarget;
    if (p_->IsInCombat()) return Result::ServerRefused;

    // PET_SAVE_AS_DELETED is the canonical untame path — RemovePet handles
    // updatefield purge, character_pet row delete, and tame-skill bookkeeping.
    p_->RemovePet(pet, PET_SAVE_AS_DELETED);
    return Result::Ok;
}

Result API::pet_set_react_state(uint8 state)
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    if (state > REACT_ASSIST) return Result::InvalidTarget;
    // Pet inherits Creature::SetReactState (Unit-level state), and PetAI
    // re-reads it on the next tick. CharmInfo doesn't store react state in
    // 12.0 — the master copy is on Unit (UnitAI::ReactState).
    pet->SetReactState(static_cast<ReactStates>(state));
    return Result::Ok;
}

Result API::pet_toggle_autocast(uint32 spell_id, bool enabled)
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    if (!pet->HasSpell(spell_id)) return Result::Locked;
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!info || !info->IsAutocastable()) return Result::InvalidTarget;
    CharmInfo* ci = pet->GetCharmInfo();
    if (!ci) return Result::Other;

    pet->ToggleAutocast(info, enabled);
    ci->SetSpellAutocast(info, enabled);
    return Result::Ok;
}

Result API::rename_pet(std::string const& new_name)
{
    if (!p_) return Result::Other;
    if (p_->GetClass() != CLASS_HUNTER) return Result::InvalidTarget;
    Pet* pet = p_->GetPet();
    if (!pet || pet->getPetType() != HUNTER_PET) return Result::NotKnown;
    if (!pet->HasPetFlag(UNIT_PET_FLAG_CAN_BE_RENAMED)) return Result::Locked;
    if (!pet->GetCharmInfo()) return Result::Other;

    PetStable* stable = p_->GetPetStable();
    if (!stable || !stable->GetCurrentPet()) return Result::Other;
    if (stable->GetCurrentPet()->PetNumber != pet->GetCharmInfo()->GetPetNumber())
        return Result::Other;

    PetNameInvalidReason res = ObjectMgr::CheckPetName(new_name);
    if (res != PET_NAME_SUCCESS) return Result::InvalidTarget;
    if (sObjectMgr->IsReservedName(new_name)) return Result::InvalidTarget;

    pet->SetName(new_name);
    pet->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
    pet->RemovePetFlag(UNIT_PET_FLAG_CAN_BE_RENAMED);
    stable->GetCurrentPet()->Name = new_name;
    stable->GetCurrentPet()->WasRenamed = true;
    return Result::Ok;
}

Result API::pet_set_command_state(uint8 command)
{
    if (!p_) return Result::Other;
    Pet* pet = p_->GetPet();
    if (!pet) return Result::NotKnown;
    CharmInfo* ci = pet->GetCharmInfo();
    if (!ci) return Result::Other;

    // Mirror PetHandler's command-state semantics. Stay = halt + bind to
    // current spot; Follow = chase the owner; Attack(no target) is a no-op
    // here because pet_attack handles target acquisition.
    switch (command)
    {
        case COMMAND_STAY:
        {
            ci->SetCommandState(COMMAND_STAY);
            ci->SetIsCommandAttack(false);
            ci->SetIsAtStay(true);
            ci->SetIsFollowing(false);
            ci->SetIsReturning(false);
            ci->SaveStayPosition();
            // Stop in place. The motion master clearing forces an Idle
            // movement generator until the next command pulls the pet out.
            pet->StopMoving();
            pet->GetMotionMaster()->Clear();
            pet->GetMotionMaster()->MoveIdle();
            return Result::Ok;
        }
        case COMMAND_FOLLOW:
        {
            ci->SetCommandState(COMMAND_FOLLOW);
            ci->SetIsCommandAttack(false);
            ci->SetIsAtStay(false);
            ci->SetIsReturning(true);
            ci->SetIsFollowing(false);
            // AttackStop pulls the pet out of any current target lock so
            // the follow can proceed cleanly.
            pet->AttackStop();
            pet->InterruptNonMeleeSpells(false);
            pet->GetMotionMaster()->MoveFollow(p_, PET_FOLLOW_DIST, pet->GetFollowAngle());
            return Result::Ok;
        }
        case COMMAND_ATTACK:
            // Use pet_attack(target) for the actual attack-with-target
            // semantics. Setting the command without a target is a no-op
            // mirror of the pet bar's attack button when nothing is
            // selected — surface InvalidTarget so the caller routes
            // through pet_attack instead.
            return Result::InvalidTarget;
        default:
            return Result::InvalidTarget;
    }
}

Result API::sell_item_by_entry(ObjectGuid npc, uint32 item_entry)
{
    if (!p_) return Result::Other;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;
    Item* item = p_->GetItemByEntry(item_entry);
    if (!item) return Result::InvalidTarget;
    auto sell = p_->SellItemToVendor(item, item->GetCount());
    if (!sell) return Result::ServerRefused;
    return Result::Ok;
}

Result API::sell_item_by_slot(ObjectGuid npc, uint8 bag, uint8 slot, uint8 count)
{
    if (!p_) return Result::Other;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;
    Item* item = p_->GetItemByPos(bag, slot);
    if (!item) return Result::InvalidTarget;
    const uint32 amount = (count == 0) ? item->GetCount() : std::min<uint32>(count, item->GetCount());
    auto sell = p_->SellItemToVendor(item, amount);
    if (!sell) return Result::ServerRefused;
    return Result::Ok;
}

// Returns true if the trade-goods subclass is a profession material the bot
// would actually USE given its current skills. Used by sell_trash to decide
// whether a white-quality mat is worth keeping (matches a profession the
// bot has) or worth converting to gold (no relevant profession).
//
// Skill IDs:
//   164 Blacksmithing, 165 Leatherworking, 171 Alchemy, 182 Herbalism,
//   185 Cooking, 186 Mining, 197 Tailoring, 202 Engineering, 333 Enchanting,
//   356 Fishing, 393 Skinning, 755 Jewelcrafting, 773 Inscription.
static bool BotWantsTradeGoodsSubclass(Player const* p, uint32 subclass)
{
    auto has = [p](uint32 skill) { return p->HasSkill(uint16(skill)); };
    switch (subclass)
    {
        // PARTS / EXPLOSIVES / DEVICES / EXPLOSIVES_DEVICES — Engineering
        case 1: case 2: case 3: case 17:
            return has(202);
        // JEWELCRAFTING (gems) — Jewelcrafting
        case 4:
            return has(755);
        // CLOTH — Tailoring (First Aid was removed in BfA)
        case 5:
            return has(197);
        // LEATHER — Leatherworking
        case 6:
            return has(165);
        // METAL_AND_STONE — Mining/Blacksmithing/Engineering/Jewelcrafting
        case 7:
            return has(186) || has(164) || has(202) || has(755);
        // MEAT — Cooking. Includes raw fish (TC classifies raw fish meat
        // as TRADE_GOODS subclass 8; cooked fish meals are CONSUMABLE
        // class entirely and never reach this filter). Cooking is granted
        // to every bot at L10+ in DoLearnProfessions, so this branch
        // mostly returns true.
        case 8:
            return has(185);
        // HERB — Herbalism/Alchemy/Inscription
        case 9:
            return has(182) || has(171) || has(773);
        // ELEMENTAL — multiple crafting professions; conservative-keep when
        // any is trained.
        case 10:
            return has(164) || has(165) || has(171) || has(197) || has(202) ||
                   has(333) || has(755) || has(773);
        // ENCHANTING (dust / essence / shard) — Enchanting. Subclass 12
        // (ITEM_SUBCLASS_ENCHANTING). Subclass 15 is
        // WEAPON_ENCHANTMENT (oils, sharpening stones) — those are
        // enchanting outputs / general consumables, not enchanter mats,
        // so we don't auto-keep them on enchanter classes.
        case 12:
            return has(333);
        // INSCRIPTION (pigment / ink) — Inscription
        case 16:
            return has(773);
        // 0 GENERIC, 11 TRADE_GOODS_OTHER, 13 MATERIAL, 14 ENCHANTMENT,
        // 15 WEAPON_ENCHANTMENT, 17+ EXPLOSIVES_DEVICES / OPTIONAL_REAGENT
        // — no specific profession ownership; default keep.
        default:
            return true;
    }
}

// Count empty inventory slots across the backpack (INVENTORY_SLOT_ITEM_*) and
// the equipped bags (INVENTORY_SLOT_BAG_*). Profession bags only count free
// slots that the candidate item could actually occupy is NOT modelled here —
// this is a coarse "how tight are bags" gauge used to gate the BoE-clearance
// branch below, not a precise can-store test (SellItemToVendor / the rule do
// that). A non-bag backpack slot that is empty counts; a bag's empty cells
// count. Returns total free general-purpose slots.
static uint32 CountFreeBagSlots(Player const* p)
{
    uint32 free = 0;
    for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
        if (!p->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
            ++free;
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* b = p->GetBagByPos(i))
            free += b->GetFreeSlots();
    return free;
}

// True when a Bind-on-Equip uncommon armor/weapon in the bag is "clearly
// vendor trash" for THIS bot and may be sold under bag pressure when no
// auctioneer is reachable. Conservative by construction — every protection
// the backlog calls out is enforced here:
//   * quest items            (class QUEST / BIND_QUEST / StartQuest) — kept
//   * heirlooms / account-bound (any BIND_*ACCOUNT*, ITEM_QUALITY_HEIRLOOM) — kept
//   * current upgrades       (would beat or fill the bot's equipped slot) — kept
//   * anything bound that's an upgrade — kept (the upgrade test runs regardless
//     of bind state; BoP-but-better never reaches this branch because it's an
//     upgrade, and we only consider BoE here anyway)
// Only genuine low-ilvl greens the bot has clearly outgrown clear out.
static bool BotShouldVendorBoeGreen(Player* p, Item* it, ItemTemplate const* tmpl)
{
    if (!p || !it || !tmpl) return false;

    // Only Uncommon (green) — Rare/Epic BoE are real AH/bank candidates, never
    // auto-vendored by this branch.
    if (tmpl->GetQuality() != ITEM_QUALITY_UNCOMMON)
        return false;

    // Only equippable gear (weapon / armor). Uncommon trade-goods, recipes,
    // gems, consumables etc. are out of scope for the "BoE clutter" rule.
    uint32 const cls = tmpl->GetClass();
    if (cls != ITEM_CLASS_WEAPON && cls != ITEM_CLASS_ARMOR)
        return false;

    // Must be Bind-on-Equip and not currently bound to this character. A BoP
    // or already-soulbound item is handled by the soulbound branch above; here
    // we only touch tradeable BoE clutter so we never destroy account value
    // beyond what a player would themselves vendor.
    if (tmpl->GetBonding() != BIND_ON_EQUIP || it->IsSoulBound())
        return false;

    // Quest-item protection (SN-P0a canonical test): item class QUEST, quest-
    // bound, or starts a quest.
    if (cls == ITEM_CLASS_QUEST ||
        tmpl->GetBonding() == BIND_QUEST ||
        tmpl->GetStartQuest() != 0)
        return false;

    // Heirloom / account-bound protection — mail to alts, never vendor.
    if (tmpl->GetQuality() == ITEM_QUALITY_HEIRLOOM ||
        tmpl->GetBonding() == BIND_BNET_ACCOUNT ||
        tmpl->GetBonding() == BIND_BNET_ACCOUNT_UNTIL_EQUIPPED ||
        tmpl->GetBonding() == BIND_WOW_ACCOUNT)
        return false;

    // Conservative value/ilvl threshold: only clear greens the bot has clearly
    // outgrown. An item within ~10 ilvls of what the bot could wear might still
    // be a sidegrade for an offspec, so we leave a wide margin below the bot's
    // level-appropriate gear. Compare against the bot's level rather than a
    // fixed ilvl so the threshold scales across the level range.
    //
    // // L-P2a: needs a per-bot "expected ilvl for level" signal for a tighter
    // // threshold. Until then, gate on a generous absolute floor: a green more
    // // than kIlvlSlack below the bot's HIGHEST equipped ilvl in the same slot
    // // family is outgrown clutter. Falls back to the upgrade test below for
    // // empty slots.
    int32 const new_ilvl = static_cast<int32>(tmpl->GetBaseItemLevel());

    // Upgrade protection — the decisive gate. Resolve which equip slot this
    // item would land in for THIS bot; if the slot is empty (would be a strict
    // gain) or the candidate scores at least as well as / out-ilvls the current
    // item, it is NOT trash. Mirrors ScoreQuestReward's slot-resolution path.
    uint16 dest = 0;
    InventoryResult const equip_err =
        p->CanEquipNewItem(NULL_SLOT, dest, it->GetEntry(), false);
    if (equip_err == EQUIP_ERR_OK)
    {
        uint8 const equip_slot = dest & 0xFF;
        if (equip_slot < EQUIPMENT_SLOT_END)
        {
            Item const* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, equip_slot);
            if (!cur)
                return false;                      // empty usable slot — keep, it's a gain
            ItemTemplate const* cur_tmpl = cur->GetTemplate();
            int32 const cur_ilvl =
                cur_tmpl ? static_cast<int32>(cur_tmpl->GetBaseItemLevel()) : 0;
            // Keep unless the equipped item clearly dominates by a wide margin
            // (candidate is >10 ilvls worse). 10 ilvls of slack keeps offspec /
            // marginal sidegrades safe.
            constexpr int32 kIlvlSlack = 10;
            if (new_ilvl >= cur_ilvl - kIlvlSlack)
                return false;                      // potential upgrade / sidegrade — keep
        }
        // equip_slot out of range — falls through to "vendor" (can't be worn).
    }
    // CanEquipNewItem != OK: the bot physically can't wear it (wrong class /
    // armor type / level). A green it can never equip and can't AH (no
    // auctioneer) is bag rot — clear it.

    return true;
}

Result API::sell_trash(ObjectGuid npc)
{
    if (!p_) return Result::Other;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;

    // Iterate inventory and main bags. Sell:
    //   1) Every Poor (grey) item — vendor trash baseline.
    //   2) Trade-goods (item class 7) of any quality where the subclass
    //      doesn't match a profession the bot trained. A Mining bot in
    //      possession of Light Leather has no use for it; freeing that
    //      bag slot for ore is the right call. Quality bound at COMMON
    //      (white) so we don't accidentally vendor an Uncommon+ mat that
    //      a teammate might want — those go to the auction house path
    //      when that lands.
    //   4) Under BAG PRESSURE only: Bind-on-Equip Uncommon (green) gear the
    //      bot has clearly outgrown and can't equip, when bags are tight and
    //      there's no auctioneer to list it. Heavily protected (see
    //      BotShouldVendorBoeGreen): never quest items, heirlooms, account-
    //      bound, current upgrades, or anything that could fill an empty slot.
    // Quest-flagged items are skipped (vendor refuses them anyway).
    //
    // Bag-pressure gate for branch (4). Greens are real AH money, so we only
    // vendor them when the bot is genuinely cramped — otherwise it keeps them
    // for the auction-house disposition. Threshold: 3 or fewer free slots.
    //
    // // L-P2a: needs an "auctioneer reachable / AH-flagged" signal to fully
    // // honour the "no auctioneer accessible" condition — that lives in the V2
    // // rule layer (AuctionRules), not on the live Player. sell_trash only
    // // receives a vendor NPC, so we approximate with bag pressure: the rule
    // // layer only invokes the BoE-clearance path when it has already decided
    // // the AH is out of reach (it routes to an auctioneer first when one is
    // // known). The bag-pressure gate here is the conservative backstop.
    constexpr uint32 kTightBagThreshold = 3;
    bool const bags_tight = CountFreeBagSlots(p_) <= kTightBagThreshold;

    std::vector<Item*> to_sell;
    p_->ForEachItem(ItemSearchLocation::Inventory, [this, &to_sell, bags_tight](Item* it)
    {
        ItemTemplate const* tmpl = it ? it->GetTemplate() : nullptr;
        if (!tmpl) return ItemSearchCallbackResult::Continue;
        if (it->IsNotEmptyBag()) return ItemSearchCallbackResult::Continue;
        const uint32 quality = tmpl->GetQuality();
        if (quality == ITEM_QUALITY_POOR)
        {
            to_sell.push_back(it);
        }
        else if (quality == ITEM_QUALITY_NORMAL &&
                 tmpl->GetClass() == ITEM_CLASS_TRADE_GOODS &&
                 !BotWantsTradeGoodsSubclass(p_, tmpl->GetSubClass()))
        {
            to_sell.push_back(it);
        }
        // Soulbound BoP weapons/armor the bot can't equip — bag rot.
        // Quest rewards offer 2-3 slots, bot picks one, but loot rolls
        // on BoP items the bot is ineligible for still accumulate (per
        // ITEM_VENDOR_SYSTEM_PLAN.md). These can never be auctioned
        // (soulbound) and can't be equipped (class/race/spec mismatch),
        // so vendor is the only disposition. Capped at Rare so we don't
        // accidentally sell an Epic legendary world-drop the bot might
        // re-spec into. Player::CanUseItem returns non-EQUIP_ERR_OK for
        // class/race/profession restrictions.
        //
        // Heirloom protection: items with ITEM_FLAG_HEIRLOOM (or quality
        // ITEM_QUALITY_HEIRLOOM, equivalent in modern client) are
        // account-bound and should be preserved for alts. If the bot
        // can't use it now, mailing it to alts via the JIT mail system
        // is the right disposition — never vendor. Audit 2026-05-22:
        // current code would happily vendor a class-wrong heirloom
        // because it passes the `IsSoulBound() && CanUseItem != OK`
        // check.
        else if ((quality == ITEM_QUALITY_UNCOMMON || quality == ITEM_QUALITY_RARE)
                 && it->IsSoulBound()
                 && (tmpl->GetClass() == ITEM_CLASS_WEAPON
                     || tmpl->GetClass() == ITEM_CLASS_ARMOR)
                 && p_->CanUseItem(tmpl) != EQUIP_ERR_OK
                 && quality != ITEM_QUALITY_HEIRLOOM
                 && tmpl->GetBonding() != BIND_BNET_ACCOUNT
                 && tmpl->GetBonding() != BIND_BNET_ACCOUNT_UNTIL_EQUIPPED
                 && tmpl->GetBonding() != BIND_WOW_ACCOUNT
                 && tmpl->GetBonding() != BIND_QUEST)
        {
            // Only sell ITEMS IN BAGS, never equipped slots — same gate
            // the inventory iterator already implies (Inventory location)
            // but explicit for clarity.
            to_sell.push_back(it);
        }
        // (4) Bag-pressure BoE-green clearance. Only under pressure, and only
        // for items BotShouldVendorBoeGreen clears (all upgrade / quest /
        // heirloom / account-bound protections enforced inside).
        else if (bags_tight && BotShouldVendorBoeGreen(p_, it, tmpl))
        {
            to_sell.push_back(it);
        }
        return ItemSearchCallbackResult::Continue;
    });

    for (Item* it : to_sell)
        p_->SellItemToVendor(it, it->GetCount());

    return to_sell.empty() ? Result::InvalidTarget : Result::Ok;
}

Result API::repair_all(ObjectGuid npc, bool from_guild_bank)
{
    if (!p_) return Result::Other;
    // Self-repair (empty npc): no repair vendor required. Used by the dungeon
    // gear-readiness rule — instances have NO repair NPC, yet a tank whose
    // durability breaks mid-run (every death strips 10%) degrades into paper
    // (0% durability ~ halved armor/stats) and death-spirals (observed
    // 2026-06-28: Deadmines tank 38 deaths, durability 16264->12976 HP, never
    // reached the boss). Bots are AI and the player-money branch below already
    // grants the shortfall as a stipend, so a self-repair is effectively free —
    // exactly the intended "gold self-repair on dungeon staging/running".
    if (!npc.IsEmpty())
    {
        Creature* repairer = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_REPAIR, UNIT_NPC_FLAG_2_NONE);
        if (!repairer) return Result::InvalidTarget;
    }

    // Guild-bank repairs keep the core path: the guild money pool is large and
    // Player::DurabilityRepairAll already repairs cheapest-first until the pool
    // is exhausted (it is only the player-money branch that is all-or-nothing).
    if (from_guild_bank)
    {
        p_->DurabilityRepairAll(/*takeCost*/ true, /*discountMod*/ 1.0f, /*guildBank*/ true);
        return Result::Ok;
    }

    // Player-money repairs: AFFORDABLE-PARTIAL, cheapest item first.
    //
    // Player::DurabilityRepairAll(takeCost=true) sums the cost of EVERY broken
    // item and, if the bot can't afford the whole bill, silently repairs NOTHING
    // (Player.cpp ~4827 `if (!HasEnoughMoney(totalCost)) return;`). A fresh, poor
    // low-level bot almost never affords a full repair, so that all-or-nothing
    // gate stranded bots at 0% gear forever — a death spiral (broken gear -> can't
    // fight -> can't earn -> still can't afford the full repair). Observed:
    // Bramwell (L5, 224c) parked on top of Godric Rothgar, firing
    // idle:critical_repair every tick with durability never moving off 0%.
    //
    // Instead repair as many items as the purse allows, cheapest first, so every
    // visit restores as much functionality as the bot can pay for and makes
    // forward progress. DurabilityRepair(pos, takeCost=true) charges + repairs a
    // single item and no-ops any it can't afford, so the loop is self-bounding.
    std::vector<std::pair<uint16, uint64>> repairables;   // (packed pos, cost)
    auto collect = [&](uint16 pos)
    {
        if (Item* item = p_->GetItemByPos(pos))
            if (uint64 cost = item->CalculateDurabilityRepairCost(/*discountMod*/ 1.0f))
                repairables.emplace_back(pos, cost);
    };
    // equipped + backpack (mirrors Player::DurabilityRepairAll's slot walk)
    uint8 const inventoryEnd = INVENTORY_SLOT_ITEM_START + p_->GetInventorySlotCount();
    for (uint8 i = EQUIPMENT_SLOT_START; i < inventoryEnd; ++i)
        collect(static_cast<uint16>((INVENTORY_SLOT_BAG_0 << 8) | i));
    // items inside equipped bags
    for (uint8 j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; ++j)
        for (uint8 i = 0; i < MAX_BAG_SIZE; ++i)
            collect(static_cast<uint16>((j << 8) | i));

    std::sort(repairables.begin(), repairables.end(),
              [](auto const& a, auto const& b) { return a.second < b.second; });

    // Repair-reserve top-up — SAME stipend pattern as the flight reserve at
    // fly_to_node (owner directive 2026-06-21). Bots are perpetually broke
    // (Morthan: 0% gear, ~20 silver); a poor low-level bot can't afford a full
    // repair, so the affordable-partial loop below leaves its costliest slots at
    // 0% forever — the poverty death-spiral (broken gear -> can't fight -> can't
    // earn -> still can't repair). Grant the shortfall so a broke bot can always
    // fully repair (bots are AI; a free repair is fine and far cheaper than the
    // leveling time lost to a bot stranded at 0% gear). Solvent bots are
    // unaffected — they already cover the bill, so no money is added.
    {
        uint64 total_cost = 0;
        for (auto const& pr : repairables) total_cost += pr.second;
        if (total_cost > 0 && p_->GetMoney() < total_cost)
            p_->ModifyMoney(int64(total_cost - p_->GetMoney()));
    }

    bool any = false;
    for (auto const& [pos, cost] : repairables)
    {
        // Cheapest-first: once the bot can't afford the next item, every costlier
        // one is also unaffordable (money only ever decreases here), so stop.
        if (!p_->HasEnoughMoney(cost))
            break;
        p_->DurabilityRepair(pos, /*takeCost*/ true, /*discountMod*/ 1.0f);
        any = true;
    }
    // Locked == too poor to repair even the cheapest item; the rule treats this
    // as "tried" (5s lockout) and the bot keeps questing/earning until it can.
    return any ? Result::Ok : Result::Locked;
}

// Cast an item's ON_USE effect via Player::CastItemUseSpell — the same path
// HandleUseItemOpcode uses. Going through cast_spell here would fail because
// item-trigger spells (e.g. Food = 433 on Conjured Bread) aren't in the
// player's spellbook, and cast_spell rejects them with NotKnown. The item
// path runs the spell with m_CastItem set, which is what the spell engine
// expects for consumable effects.
static Result UseItemImpl(Player* p, Item* item, ObjectGuid target)
{
    if (!item) return Result::InvalidTarget;
    InventoryResult canuse = p->CanUseItem(item);
    if (canuse != EQUIP_ERR_OK)
        return Result::Locked;

    // Build a target struct mirroring what HandleUseItemOpcode receives.
    SpellCastTargets targets;
    if (!target.IsEmpty() && target != p->GetGUID())
    {
        if (Unit* tu = ObjectAccessor::GetUnit(*p, target))
        {
            // Spellclick NPCs: when the quest target is a UNIT_NPC_FLAG_SPELLCLICK
            // creature, "using the item on it" is, on a real client, a CMSG_SPELL_CLICK —
            // Unit::HandleSpellClick both casts the click spell AND fires the creature's
            // OnSpellClick AI. That AI is what credits this whole class of quests
            // (e.g. Q26118: spellclick Ambassador Slaghammer 42146 -> SmartAI
            // SMART_EVENT_ON_SPELLCLICK -> SMART_ACTION_COMPLETE_QUEST). A raw
            // CastItemUseSpell never triggers the AI, so the quest never completes.
            if (tu->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK))
            {
                p->SetTarget(tu->GetGUID());
                p->SetFacingToObject(tu);
                tu->HandleSpellClick(p);
                return Result::Ok;
            }
            targets.SetUnitTarget(tu);
            // Actively SELECT + face the target, exactly as a real client does before
            // using an item on a creature (quest "use item on NPC" spells expect it).
            p->SetTarget(target);
            p->SetFacingToObject(tu);
        }
    }
    else
    {
        targets.SetUnitTarget(p);
    }

    // Upstream now requires the explicit on-use spell id (it was formerly
    // resolved inside CastItemUseSpell). Pick the item's first ON_USE effect.
    uint32 useSpellId = 0;
    for (ItemEffectEntry const* eff : item->GetEffects())
    {
        if (eff->TriggerType == ITEM_SPELLTRIGGER_ON_USE && eff->SpellID > 0)
        {
            useSpellId = uint32(eff->SpellID);
            break;
        }
    }
    if (!useSpellId)
        return Result::Other;

    p->CastItemUseSpell(item, useSpellId, targets, ObjectGuid::Empty, /*misc*/ {});
    return Result::Ok;
}

Result API::use_item_by_entry(uint32 item_entry, ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    return UseItemImpl(p_, p_->GetItemByEntry(item_entry), target);
}

Result API::use_item_by_slot(uint8 bag, uint8 slot, ObjectGuid target)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    return UseItemImpl(p_, p_->GetItemByPos(bag, slot), target);
}

Result API::loot_corpse(ObjectGuid corpse)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    Creature* c = ObjectAccessor::GetCreature(*p_, corpse);
    if (!c || !c->IsInWorld() || c->IsAlive()) return Result::InvalidTarget;
    constexpr float kLootRange = 11.0f;   // matches AELootCreatureCheck::LootDistance
    if (p_->GetDistance(c) > kLootRange) return Result::OutOfRange;

    Loot* loot = c->GetLootForPlayer(p_);
    if (!loot) return Result::InvalidTarget;
    if (loot->isLooted() && loot->items.empty() && loot->gold == 0)
        return Result::InvalidTarget;

    // Group with non-FFA loot rules: defer to SendLoot so the rolling /
    // master-loot pipeline owns the items. We don't auto-grab in that case.
    Group* group = p_->GetGroup();
    if (group && group->GetLootMethod() != FREE_FOR_ALL)
    {
        p_->SendLoot(*loot, false);
        return Result::Ok;
    }

    // Solo / FFA — directly transfer items the bot is allowed to take.
    ObjectGuid lootOwnerGuid = loot->GetOwnerGUID();
    uint32 dbg_items = uint32(loot->items.size());
    uint32 dbg_stored = 0, dbg_blocked = 0, dbg_looted = 0, dbg_nofit = 0, dbg_nullitem = 0;
    for (uint8 slot = 0; slot < loot->items.size(); ++slot)
    {
        // ROOT CAUSE of the loot/skin hot-loop (4-day forensics): free-for-all
        // (FFA) loot items track their looted state PER PLAYER in `ffaItem`, NOT
        // in the shared `it->is_looted` (Player::StoreLootItem only sets the
        // shared flag for non-FFA items — `if (!item->freeforall)`). The old call
        // omitted the ffaItem out-param, so the bot never saw that it had already
        // taken its own FFA copy and re-looted the SAME FFA items every tick: the
        // shared is_looted never flipped, the corpse never fully cleared, never
        // became skinnable, and idle:skin_corpse fired 0x. One bot+corpse pair
        // re-looted 20,706x in 4 days (the signature: stored>=1 with prelooted=0
        // forever). Pass + honour ffaItem so a looted FFA item is skipped.
        NotNormalLootItem* ffaItem = nullptr;
        LootItem* it = loot->LootItemInSlot(slot, p_, &ffaItem);
        if (!it)                           { ++dbg_nullitem; continue; }
        if (it->is_looted)                 { ++dbg_looted;  continue; }
        if (ffaItem && ffaItem->is_looted) { ++dbg_looted;  continue; }
        if (it->is_blocked) { ++dbg_blocked; continue; }
        // Pre-check space so an un-storable item is reported (and skipped
        // cleanly) instead of silently leaving the corpse un-looted.
        ItemPosCountVec dest;
        InventoryResult ir = p_->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, it->itemid, it->count);
        if (ir != EQUIP_ERR_OK) { ++dbg_nofit;
            TC_LOG_INFO("playerbot.v2", "[loot_skip] bot={} corpse={} item={} count={} reason=cant_store ir={}",
                p_->GetGUID().GetCounter(), corpse.GetCounter(), it->itemid, uint32(it->count), uint32(ir));
            continue;
        }
        p_->StoreLootItem(lootOwnerGuid, slot, loot);
        ++dbg_stored;
    }
    if (loot->gold > 0)
    {
        p_->ModifyMoney(loot->gold);
        loot->gold = 0;
    }
    // Diagnostic: if the corpse is NOT fully looted after this pass, this line
    // names exactly why (blocked/looted/no-fit/null) so the skinning-blocked
    // "corpse still has loot" reports can be root-caused from a single kill.
    TC_LOG_INFO("playerbot.v2",
        "[loot_drain] bot={} corpse={} items={} stored={} blocked={} prelooted={} nofit={} nullitem={} after_isLooted={}",
        p_->GetGUID().GetCounter(), corpse.GetCounter(), dbg_items, dbg_stored, dbg_blocked,
        dbg_looted, dbg_nofit, dbg_nullitem, loot->isLooted() ? 1 : 0);
    // CRITICAL: SendLootRelease() only sends the client a packet — it does NOT
    // run the server-side release. The real release (WorldSession::DoLootRelease)
    // is what clears UNIT_DYNFLAG_LOOTABLE (removes the corpse "glitter") and,
    // for a fully-looted creature corpse, calls AllLootRemovedFromCorpse() which
    // transitions the corpse to SKINNABLE. Without it the bot pulled the items
    // into its bags but the corpse stayed glittering and never became skinnable
    // (root cause of "glittering dead cats, no loot/skin"). Route through the
    // bot's session so the corpse is properly freed.
    if (WorldSession* sess = p_->GetSession())
        sess->DoLootRelease(loot);
    else
        p_->SendLootRelease(corpse);
    return Result::Ok;
}

Result API::hearth()
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;
    // Hearthstone is an ITEM (entry 6948) — players obtain it from any
    // innkeeper after binding their home location, then USE the item to
    // cast its hearth spell. Spell 8690 is the *triggered* spell inside the
    // item; calling cast_spell(8690) rejects with "not in spellbook" since
    // 8690 isn't a learned player spell. The correct path is to invoke the
    // item-use code (CastItemUseSpell), which the existing use_item_by_entry
    // already does.
    constexpr uint32 HEARTHSTONE_ITEM = 6948;
    if (!p_->HasItemCount(HEARTHSTONE_ITEM, 1, /*inBankAlso*/ false))
        return Result::NotKnown;
    return use_item_by_entry(HEARTHSTONE_ITEM, p_->GetGUID());
}

// ---- NPC / world interaction ----------------------------------------------

Result API::interact_with_npc(ObjectGuid npc)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;
    // Mirrors WorldSession::HandleGossipHelloOpcode (NPCHandler.cpp). Same
    // interact-range / NPC-flag check; same cancel-of-Interacting auras;
    // same pause-on-talk; same OnGossipHello → PrepareGossipMenu →
    // SendPreparedGossip pipeline. Only the packet read/write is omitted
    // because we're invoking from server side.
    Creature* unit = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_GOSSIP, UNIT_NPC_FLAG_2_NONE);
    if (!unit) return Result::InvalidTarget;

    if (FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(unit->GetFaction()))
        p_->GetReputationMgr().SetVisible(ft);

    p_->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Interacting);

    if (uint32 pause = unit->GetMovementTemplate().GetInteractionPauseTimer())
        unit->PauseMovement(pause);
    unit->SetHomePosition(unit->GetPosition());

    if (unit->IsAreaSpiritHealer())
    {
        p_->SetAreaSpiritHealer(unit);
        p_->SendAreaSpiritHealerTime(unit);
    }

    p_->PlayerTalkClass->ClearMenus();
    if (!unit->AI()->OnGossipHello(p_))
    {
        p_->PrepareGossipMenu(unit, p_->GetGossipMenuForSource(unit), true);
        p_->SendPreparedGossip(unit);
    }
    return Result::Ok;
}

Result API::gossip_select_by_index(ObjectGuid npc, uint32 order_index)
{
    if (!p_) return Result::Other;
    GossipMenu& menu = p_->PlayerTalkClass->GetGossipMenu();
    if (menu.Empty()) return Result::Locked;
    GossipMenuItem const* item = menu.GetItemByIndex(order_index);
    if (!item) return Result::Locked;
    // Cheating-protection check from HandleGossipSelectOptionOpcode: the
    // player's interaction state must point at this exact NPC, otherwise
    // the menu is stale (we'd dispatch on the wrong target's actions).
    if (!p_->PlayerTalkClass->GetInteractionData().IsInteractingWith(npc, PlayerInteractionType::Gossip))
        return Result::Locked;

    Creature* unit = nullptr;
    GameObject* go = nullptr;
    if (npc.IsCreatureOrVehicle())
    {
        unit = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_GOSSIP, UNIT_NPC_FLAG_2_NONE);
        if (!unit) return Result::InvalidTarget;
    }
    else if (npc.IsGameObject())
    {
        go = p_->GetGameObjectIfCanInteractWith(npc);
        if (!go) return Result::InvalidTarget;
    }
    else return Result::InvalidTarget;

    if (p_->HasUnitState(UNIT_STATE_DIED))
        p_->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    const int32  optionId = item->GossipOptionID;
    const uint32 menuId   = menu.GetMenuId();
    if (unit)
    {
        if (!unit->AI()->OnGossipSelect(p_, menuId, item->OrderIndex))
            p_->OnGossipSelect(unit, optionId, menuId);
    }
    else
    {
        if (!go->AI()->OnGossipSelect(p_, menuId, item->OrderIndex))
            p_->OnGossipSelect(go, optionId, menuId);
    }
    return Result::Ok;
}

Result API::use_game_object(ObjectGuid go_guid)
{
    if (!p_) return Result::Other;
    // ---- Transport boarding / disembarking (ships, zeppelins, elevators) ----
    // A REAL player boards because the game client detects the transport
    // collision under it and sends a movement packet with the transport GUID;
    // the server then attaches it. A bot has NO client, so standing on the deck
    // does nothing (observed live: bots at dist 0.0 with on_transport=0 forever).
    // We attach/detach server-side instead. Treated as a toggle so we can reuse
    // the existing UseObjectIntent (no new intent type — the IntentBody variant
    // is at its MSVC heap limit): not aboard + in range => board; already aboard
    // => disembark (drops the bot at its current world position, i.e. on the dock
    // deck at the destination).
    //
    // Resolve as a TransportBase, which covers BOTH transport flavours:
    //   * GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT (15) — cross-map ships/zeppelins, and
    //   * GAMEOBJECT_TYPE_TRANSPORT (11) — local animated lifts (the Orgrimmar
    //     zeppelin-tower elevators, Undercity/city lifts, etc).
    // The previous code resolved via Map::GetTransport(), which returns ONLY
    // type-15 (guid.IsMOTransport()). Every type-11 elevator therefore fell
    // through the whole branch and AddPassenger NEVER fired for one (verified
    // live: ADDPASSENGER 0 fleet-wide while a bot looped boarding an elevator,
    // dist growing 11→18y as the platform cycled away un-attached). Map::
    // GetGameObject() finds both flavours and GameObject::ToTransportBase()
    // yields the passenger interface for each; the type-11 GO is relocated live
    // by its animation frames, so GetPosition() is the current platform height.
    if (Map* tmap = p_->GetMap())
    {
        GameObject* go = tmap->GetGameObject(go_guid);
        if (TransportBase* tr = go ? go->ToTransportBase() : nullptr)
        {
            // DISEMBARK: already aboard → step off. The caller (idle ride-guard)
            // only emits this when the ship is STOPPED at the destination dock,
            // so RemovePassenger leaves the bot standing on the deck at the dock
            // and its normal rules walk it onto land next tick.
            if (p_->GetTransport())
            {
                tr->RemovePassenger(p_);
                p_->SetFallInformation(0, p_->GetPositionZ());

                // ELEVATOR step-off (type-11). A ship (type-15) drops the bot on
                // its stationary dock deck, which is real navmesh — nothing more to
                // do. A type-11 lift, however, leaves the bot standing on the
                // PLATFORM CENTRE at the arrival floor, which for an upper stop is
                // open air over the shaft (the moving platform is carved OUT of the
                // static navmesh). The boarding/disembark footing is a walkable
                // LEDGE ~9-11y to the side of the centre (owner-verified for the
                // Orgrimmar lift 206610: top centre (1755.5,-4396.3,109.65) -> top
                // ledge (1761.7,-4405.0,109.6)). Snap the bot onto that ledge so it
                // steps OFF onto solid ground instead of riding the platform forever
                // or falling down the shaft when the platform cycles away. The query
                // runs here on the world thread with the live navmesh (the bot is
                // physically at the elevator, so the tile is resident). This is the
                // physical "step off the platform onto the rim" action, NOT a
                // stuck-rescue teleport: it only nudges the bot the few yards from
                // the platform centre to the adjacent ledge at the SAME floor.
                if (go->GetGoType() == GAMEOBJECT_TYPE_TRANSPORT)
                {
                    Position ledge;
                    if (Playerbot::BotMovement::NearestNavPoint(
                            p_, p_->GetPositionX(), p_->GetPositionY(),
                            p_->GetPositionZ(), /*hxy*/ 16.0f, /*hz*/ 8.0f, ledge))
                    {
                        if (Map* dmap = p_->GetMap())
                            dmap->PlayerRelocation(p_, ledge.GetPositionX(),
                                ledge.GetPositionY(), ledge.GetPositionZ(),
                                p_->GetOrientation());
                        p_->SetFallInformation(0, ledge.GetPositionZ());
                        TC_LOG_INFO("playerbot.v2",
                            "[xport_board] {} DISEMBARK(elevator) transport={} -> ledge "
                            "({:.1f},{:.1f},{:.1f})",
                            p_->GetName(), go->GetEntry(),
                            ledge.GetPositionX(), ledge.GetPositionY(), ledge.GetPositionZ());
                        return Result::Ok;
                    }
                    // No walkable ledge within reach — fall through to the plain
                    // log. The bot detaches in place (pre-existing behaviour); the
                    // WARN flags a lift whose disembark floor has no navmesh nearby
                    // (data gap worth investigating).
                    TC_LOG_WARN("playerbot.v2",
                        "[xport_board] {} DISEMBARK(elevator) transport={} found NO ledge "
                        "near ({:.1f},{:.1f},{:.1f}); detaching in place",
                        p_->GetName(), go->GetEntry(),
                        p_->GetPositionX(), p_->GetPositionY(), p_->GetPositionZ());
                }

                TC_LOG_INFO("playerbot.v2",
                    "[xport_board] {} DISEMBARK transport={} map={}",
                    p_->GetName(), go->GetEntry(), p_->GetMapId());
                return Result::Ok;
            }
            // BOARD — V1-proven recipe. A bot has no client to send the board
            // movement packet, so we attach server-side — but it WALKS onto the
            // deck like a player, we never snap/teleport it on. Two requirements
            // V1 honoured and the first V2 cut missed:
            //   1) Only board a transport that is STOPPED at its dock. Boarding a
            //      MOVING transport made the bot chase a departing ship forever
            //      (observed: dist 4.4 -> 9.5, ADDPASSENGER 0 fleet-wide) and is
            //      not how a player boards.
            //   2) WALK the bot onto the deck (the deck is stationary while docked,
            //      so MovePoint reaches it) and only AddPassenger once it is
            //      actually standing on the deck.
            Position const trPos = go->GetPosition();

            // ELEVATOR / already-on-deck fast path. If the bot is standing
            // essentially ON the transport (within ~7y 3D), attach IMMEDIATELY at
            // its current spot. This covers (a) a vertical elevator platform that
            // has cycled down to the bot's floor — elevators move CONTINUOUSLY so
            // the stationary gate below never passes for them, but the bot is
            // physically on the platform so boarding now is correct; and (b) a ship
            // deck the bot already walked onto (the V1 path below leads here). The
            // idle:elevator_step_on rule plants the bot on the boarding spot and
            // emits use_game_object every tick; this fires the instant the
            // platform overlaps it. on_transport then flips and
            // idle:on_transport_wait freezes the bot for the ride.
            {
                const float odx = p_->GetPositionX() - trPos.GetPositionX();
                const float ody = p_->GetPositionY() - trPos.GetPositionY();
                const float odz = p_->GetPositionZ() - trPos.GetPositionZ();
                // Board only when the platform is FULLY at the bot's floor: the bot
                // standing over the shaft (xy <= 6y) AND the platform's Z within ~3y
                // of the bot's feet. A loose 3D sphere attached the bot mid-descent
                // (platform still above/below its feet) so it ended up off-centre
                // and wasn't carried up (observed in-game). With the tight Z gate
                // the bot simply waits on the boarding spot and is attached the
                // instant the platform settles at this floor.
                if (odx*odx + ody*ody <= 6.0f * 6.0f && std::fabs(odz) <= 3.0f)
                {
                    Position onOff;
                    onOff.Relocate(0.0f, 0.0f, 1.0f, p_->GetOrientation());
                    Position onWorld = tr->GetPositionWithOffset(onOff);
                    tr->AddPassenger(p_, onOff);
                    tmap->PlayerRelocation(p_, onWorld.GetPositionX(),
                        onWorld.GetPositionY(), onWorld.GetPositionZ(),
                        onWorld.GetOrientation());
                    p_->SetFallInformation(0, onWorld.GetPositionZ());
                    TC_LOG_INFO("playerbot.v2",
                        "[xport_board] {} ADDPASSENGER(on-platform) transport={} on_transport={} z={:.1f}",
                        p_->GetName(), go->GetEntry(), p_->GetTransport() ? 1 : 0,
                        onWorld.GetPositionZ());
                    return Result::Ok;
                }
            }

            // Per-transport stationary tracking. Ships/zeppelins do NOT report
            // Transport::IsStopped() for their natural dock pauses, so detect "at
            // dock" by the pivot holding still for >= 2s. Keyed by transport guid,
            // shared across every bot waiting for the same ship.
            static std::unordered_map<ObjectGuid, std::pair<Position, uint32>>
                s_transportStationary;
            uint32 const now_ms = GameTime::GetGameTimeMS();
            auto& track = s_transportStationary[go->GetGUID()];
            if (track.second == 0 || track.first.GetExactDist(&trPos) > 1.0f)
            {
                track.first  = trPos;
                track.second = now_ms;
            }
            if (now_ms - track.second < 2000)
            {
                // Still sailing / just pulling in — wait at the dock, don't chase a
                // moving deck. Locked holds the bot; the idle wait_for_transport
                // rule keeps it on the pier until the ship settles, then this
                // re-fires and boards.
                return Result::Locked;
            }

            // Docked and stationary. Walk onto the deck and attach once standing
            // on it (the bot visibly walks aboard, as V1 did). The deck height
            // above the transport pivot varies per ship — most boats deck ~2y
            // above the waterline pivot, but some have a raised deck reached by
            // entrance stairs (e.g. Stormwind's Pride / Valiance-Keep icebreaker,
            // deck ~z9.4 over a z0 pivot). Use an operator-provided per-entry deck
            // height so the bot lands on the actual deck, not in the hull/water.
            float deckZ = 2.0f;
            switch (go->GetEntry())
            {
                case 190536: deckZ = 9.44f; break;  // Stormwind's Pride (SW<->Valiance Keep)
                case 181688: deckZ = 9.44f; break;  // Northspear icebreaker (same class)
                default: break;
            }
            Position deckOffset;
            deckOffset.Relocate(0.0f, 0.0f, deckZ, p_->GetOrientation());
            Position deckWorld = tr->GetPositionWithOffset(deckOffset);
            if (p_->GetExactDist(&deckWorld) >= 5.0f)
            {
                p_->GetMotionMaster()->MovePoint(0, deckWorld);
                return Result::Locked;
            }
            tr->AddPassenger(p_, deckOffset);
            tmap->PlayerRelocation(p_, deckWorld.GetPositionX(),
                deckWorld.GetPositionY(), deckWorld.GetPositionZ(),
                deckWorld.GetOrientation());
            p_->SetFallInformation(0, deckWorld.GetPositionZ());
            TC_LOG_INFO("playerbot.v2",
                "[xport_board] {} ADDPASSENGER transport={} on_transport={} deckZ={:.1f}",
                p_->GetName(), go->GetEntry(), p_->GetTransport() ? 1 : 0,
                deckWorld.GetPositionZ());
            return Result::Ok;
        }
    }
    GameObject* obj = p_->GetGameObjectIfCanInteractWith(go_guid);
    // [bg_use_diag] TEMP — verify BG flag/node use (pickup vs cap).
    if (Map* dmap2 = p_->GetMap())
        if (GameObject* raw = dmap2->GetGameObject(go_guid))
        {
            uint8 const rt = raw->GetGoType();
            if (rt == 36 || rt == 37 || rt == 24 || rt == 42)
                TC_LOG_INFO("playerbot.v2",
                    "[bg_use_diag] {} map={} type={} entry={} interact_ok={} d3={:.1f} "
                    "alive={} can_use_bg={} flagstate={}",
                    p_->GetName(), p_->GetMapId(), uint32(rt), raw->GetEntry(),
                    obj ? 1 : 0, p_->GetDistance(raw), p_->IsAlive() ? 1 : 0,
                    p_->CanUseBattlegroundObject(raw) ? 1 : 0,
                    rt == 36 ? int32(raw->GetFlagState()) : -1);
        }
    if (!obj) return Result::InvalidTarget;
    // Mounted-restriction check from HandleGameObjectUseOpcode: most GOs
    // refuse interaction while mounted unless flagged UsableMounted.
    if (p_->IsMounted() && !obj->GetGOInfo()->IsUsableMounted())
        return Result::Locked;
    obj->Use(p_);

    // Auto-loot for lootable GO types (CHEST, GOOBER with loot, etc.).
    // GameObject::Use opens the loot window server-side (m_loot is
    // populated, GO state transitions to ACTIVATED) but for headless
    // bots — and for self-AI bots without auto-loot enabled — there's
    // no client to send CMSG_LOOT_ITEM. The chest stays open with the
    // items inside untaken, the quest objective never progresses, and
    // the bot moves on or hearths after stuck-detection.
    // Verified 2026-05-20 on "Demonic Thieves" (quest 28715): Uraimus
    // opened the orc bonfire GO but never took the demonic ID stub
    // off it, got stuck on the objective.
    //
    // Mirrors API::loot_corpse: walk loot.items, store each. Skip the
    // group / master-loot path (chests are FFA by default — TC's
    // ROLL_NEED_BEFORE_GREED and master-loot apply only to creature
    // corpses).
    if (Loot* loot = obj->GetLootForPlayer(p_))
    {
        if (!loot->isLooted() || !loot->items.empty() || loot->gold != 0)
        {
            ObjectGuid lootOwnerGuid = loot->GetOwnerGUID();
            for (uint8 slot = 0; slot < loot->items.size(); ++slot)
            {
                // Same FFA fix as API::loot_corpse: chests are FFA, so a looted
                // item's state lives in the per-player ffaItem, not the shared
                // it->is_looted — without checking it the bot re-loots the chest
                // every tick and the GO never reaches GO_JUST_DEACTIVATED.
                NotNormalLootItem* ffaItem = nullptr;
                LootItem* it = loot->LootItemInSlot(slot, p_, &ffaItem);
                if (!it || it->is_looted || it->is_blocked) continue;
                if (ffaItem && ffaItem->is_looted) continue;
                p_->StoreLootItem(lootOwnerGuid, slot, loot);
            }
            if (loot->gold > 0)
            {
                p_->ModifyMoney(loot->gold);
                loot->gold = 0;
            }
            p_->SendLootRelease(go_guid);
        }
    }
    return Result::Ok;
}

// ---- Quest ---------------------------------------------------------------

Result API::accept_quest(ObjectGuid quest_giver, uint32 quest_id)
{
    if (!p_) return Result::Other;
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest)
    {
        TC_LOG_INFO("playerbot.v2",
            "[accept_quest] {} reject quest={} reason=template_missing",
            "?", quest_id);
        return Result::NotKnown;
    }

    auto log_reject = [&](char const* why)
    {
        TC_LOG_INFO("playerbot.v2",
            "[accept_quest] {} reject quest={} lvl={} reason={}",
            p_->GetName(), quest_id, uint32(p_->GetLevel()), why);
    };

    Object* object = nullptr;
    if (quest_giver.IsPlayer())
    {
        object = ObjectAccessor::FindPlayer(quest_giver);
    }
    else
    {
        object = ObjectAccessor::GetObjectByTypeMask(*p_, quest_giver,
                    TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM);
    }
    if (!object)                                        { log_reject("giver_not_found");      return Result::InvalidTarget; }
    if (quest_giver.IsPlayer())                         { log_reject("player_giver_unsupp");  return Result::Other; }
    if (!object->hasQuest(quest_id))                    { log_reject("giver_lost_relation");  return Result::InvalidTarget; }
    if (!p_->CanInteractWithQuestGiver(object))         { log_reject("out_of_range");         return Result::OutOfRange; }
    // Junk-quest accept-gate (R2, defense-in-depth). A quest with ZERO
    // objectives AND no creature/GO turn-in NPC is structurally unfinishable
    // and unturnable — accepting it parks the bot forever (current_quest_id=0
    // → [picker_none]). The snapshot offer-scan already filters these, but a
    // shared-quest / gossip path could still reach here. (q55660 "Time Trials"
    // was held by 13,641 bots fleet-wide.) Mirrors IsBotJunkQuest in the V2
    // module; checked inline to avoid a cross-library dependency.
    if (quest->Objectives.empty())
    {
        auto ce = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(quest_id);
        auto ge = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(quest_id);
        if (ce.begin() == ce.end() && ge.begin() == ge.end())
        { log_reject("no_ender_or_objectives");  return Result::Locked; }
    }
    if (!p_->CanTakeQuest(quest, true))                 { log_reject("cant_take_quest");      return Result::Locked; }
    if (!p_->SatisfyQuestLog(false))                    { log_reject("quest_log_full");       return Result::InventoryFull; }
    if (!p_->CanAddQuest(quest, true))                  { log_reject("src_item_no_bag_space"); return Result::InventoryFull; }

    p_->AddQuestAndCheckCompletion(quest, object);
    p_->PlayerTalkClass->SendCloseGossip();
    // Source-item check: if the quest grants an item on accept (the
    // bot's deliverable, e.g. "Cookbook" for Teldrassil's Reminders of
    // Home), verify it actually landed in bag. Quest::SourceItemId is
    // the granted entry; SourceItemIdCount is the quantity. Helps
    // diagnose "bot can't progress this delivery quest" by confirming
    // the cookbook (or equiv) is actually in inventory.
    const uint32 src_item = quest->GetSrcItemId();
    uint32 src_in_bag = 0;
    if (src_item != 0)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Bag* b = p_->GetBagByPos(i))
                for (uint32 s = 0; s < b->GetBagSize(); ++s)
                    if (Item* it = p_->GetItemByPos(i, s))
                        if (it->GetEntry() == src_item) src_in_bag += it->GetCount();
        for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
            if (Item* it = p_->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
                if (it->GetEntry() == src_item) src_in_bag += it->GetCount();
    }
    TC_LOG_INFO("playerbot.v2",
        "[accept_quest] {} OK quest={} lvl={} giver={} src_item={}/{}",
        p_->GetName(), quest_id, uint32(p_->GetLevel()),
        quest_giver.GetCounter(), src_item, src_in_bag);
    return Result::Ok;
}

// Score a single choice-reward item from a completable quest. Higher score wins.
// Decision tree (matches QUEST_SYSTEM_PLAN.md L1.A):
//   1. Item not usable by bot at all (CanUseItem != OK) → vendor value only.
//   2. Item is equippable for an empty equipment slot → ilvl * 100.
//   3. Item is equippable, slot already occupied → (delta_ilvl) * 100 + (delta_quality) * 10.
//      Negative deltas allowed; the comparison still produces a score we can rank.
//   4. Tabards / shirts (cosmetic): cap at vendor value (no equip-bonus weight).
// Vendor value is item->GetSellPrice() * count; clamped to int32 to keep score
// arithmetic straightforward (huge gold values can overflow int otherwise).
static int32 ScoreQuestReward(Player* bot, Quest const* quest, uint32 choice_index)
{
    if (!bot || !quest) return 0;
    if (choice_index >= quest->GetRewChoiceItemsCount()) return 0;

    uint32 const item_id = quest->RewardChoiceItemId[choice_index];
    uint32 const count   = quest->RewardChoiceItemCount[choice_index];
    if (!item_id) return 0;

    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(item_id);
    if (!tmpl) return 0;

    // Vendor base — used as a tiebreaker plus the floor for un-equippable items.
    // SellPrice is in copper; cast to int32 first so a 0-stack is just 0 not UB.
    int32 const vendor_value = static_cast<int32>(tmpl->GetSellPrice()) *
                               static_cast<int32>(count > 0 ? count : 1);

    // Cosmetic slots: tabard/shirt have InventoryType but no useful stats; cap
    // at vendor value so a bot doesn't pick a tabard over a usable upgrade.
    InventoryType const inv = tmpl->GetInventoryType();
    if (inv == INVTYPE_TABARD || inv == INVTYPE_BODY)
        return vendor_value;

    // Mock-equippable test: CanUseItem rejects wrong-class / wrong-armor /
    // level-too-low. We still want vendor value as a fallback.
    if (bot->CanUseItem(tmpl) != EQUIP_ERR_OK)
        return vendor_value;

    // Resolve which equip slot this item would land in for *this* bot. If no
    // slot fits (off-hand on a class without dual-wield, ranged on a non-hunter
    // when item is a bow, etc.), fall back to vendor.
    uint16 dest = 0;
    InventoryResult const equip_err = bot->CanEquipNewItem(NULL_SLOT, dest, item_id, false);
    if (equip_err != EQUIP_ERR_OK)
        return vendor_value;

    uint8 const equip_slot = dest & 0xFF;
    if (equip_slot >= EQUIPMENT_SLOT_END) return vendor_value;

    // Item level for ranking. Use the LEVEL-SCALED effective ilvl the bot will
    // actually wear the item at — TC 12.0 scales many items via a curve, so the
    // static base ilvl can be wildly off (a base-120 reward collapsing to
    // effective ilvl 5 at L16). The old code used GetBaseItemLevel() despite the
    // comment promising scaled — fixed 2026-06-17 (under-gearing campaign) so
    // quest-reward picks match what the bot wears.
    int32 const new_ilvl = ::Playerbot::Gear::EffectiveItemLevelForLevel(tmpl, bot->GetLevel());
    int32 const new_qual = static_cast<int32>(tmpl->GetQuality());

    // Stat-block fit: primary stat alignment + armor/weapon proficiency.
    // Modern items use either fixed primary (STR/AGI/INT) or "smart" stats
    // (AGI_STR_INT etc.) - the shared Gear scorer handles both. Same scorer
    // used by gear distribution and starter-quest auto-completion so all
    // three pickers agree on what's a good fit.
    uint16 const spec = uint16(AsUnderlyingType(bot->GetPrimarySpecialization()));
    int32 const fit = ::Playerbot::Gear::ScoreItemForClass(
        tmpl, bot->GetClass(), spec, bot->GetLevel());

    Item const* cur = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, equip_slot);
    if (!cur)
    {
        // Empty slot - guaranteed upgrade. Score off ilvl with a flat bonus
        // so even a low-ilvl-but-usable item beats vendor value of common gear.
        return new_ilvl * 100 + new_qual * 10 + 5000 + fit;
    }

    ItemTemplate const* cur_tmpl = cur->GetTemplate();
    // Effective ilvl of the equipped item (live item → real scaled value).
    int32 const cur_ilvl = static_cast<int32>(cur->GetItemLevel(bot));
    int32 const cur_qual = cur_tmpl ? static_cast<int32>(cur_tmpl->GetQuality())       : 0;

    int32 const delta = (new_ilvl - cur_ilvl) * 100 + (new_qual - cur_qual) * 10;

    // Even when the slot's current item dominates, surface vendor value as a
    // secondary signal so two non-upgrades can be compared meaningfully.
    // Primary-stat fit is layered in so a wrong-primary higher-ilvl reward
    // loses to a right-primary slightly-lower-ilvl alternative.
    return delta + (vendor_value / 100) + fit;
}

Result API::complete_quest(ObjectGuid quest_giver, uint32 quest_id, uint32 reward_choice)
{
    if (!p_) return Result::Other;
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest)
    {
        TC_LOG_INFO("playerbot.v2",
            "[complete_quest] {} reject quest={} reason=template_missing",
            "?", quest_id);
        return Result::NotKnown;
    }
    auto log_reject = [&](char const* why)
    {
        TC_LOG_INFO("playerbot.v2",
            "[complete_quest] {} reject quest={} lvl={} reason={}",
            p_->GetName(), quest_id, uint32(p_->GetLevel()), why);
    };
    if (!p_->CanRewardQuest(quest, false))           { log_reject("cant_reward_pre");      return Result::Locked; }

    Object* object = ObjectAccessor::GetObjectByTypeMask(*p_, quest_giver,
                        TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM);
    if (!object)                                     { log_reject("turnin_giver_lost");    return Result::InvalidTarget; }
    if (!p_->CanInteractWithQuestGiver(object))      { log_reject("turnin_out_of_range");  return Result::OutOfRange; }

    // Auto-reward selection: when caller passes the kRewardChoiceAuto sentinel,
    // walk every choice item and pick the highest-scoring one. Score combines
    // equip-upgrade weight (ilvl + quality vs current slot occupant) with
    // vendor value (for un-equippable / un-usable items). Quests with no
    // choices fall through to choice_item = 0 unchanged.
    uint32 choice_item = 0;
    uint32 const num_choices = quest->GetRewChoiceItemsCount();
    if (num_choices > 0)
    {
        if (reward_choice == kRewardChoiceAuto)
        {
            int32 best_score = std::numeric_limits<int32>::min();
            for (uint32 i = 0; i < num_choices; ++i)
            {
                int32 const s = ScoreQuestReward(p_, quest, i);
                if (s > best_score)
                {
                    best_score = s;
                    choice_item = quest->RewardChoiceItemId[i];
                }
            }
        }
        else if (reward_choice < num_choices)
        {
            choice_item = quest->RewardChoiceItemId[reward_choice];
        }
        // reward_choice >= num_choices (and not the sentinel) → choice_item
        // stays 0; CanRewardQuest below will reject and return Locked.
    }

    if (!p_->CanRewardQuest(quest, LootItemType::Item, choice_item, false))
    {
        log_reject("cant_reward_post");
        return Result::Locked;
    }

    p_->RewardQuest(quest, LootItemType::Item, choice_item, object);
    // Verify the reward actually landed in bag. RewardQuest can fail to
    // place items silently when bag space is tight enough that AutoStore
    // can't find a slot — those items go to mail. Log both presence in
    // bag and mail-deferred outcome so the operator can distinguish
    // "reward never arrived" (genuine bug) from "reward arrived but in
    // mail" (bag full, recoverable via mailbox visit).
    auto bag_has = [this](uint32 item_entry) -> uint32 {
        if (item_entry == 0) return 0;
        uint32 total = 0;
        // Walk inventory + bag positions. Use ITERATE_ALL_ITEMS-style range.
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Bag* b = p_->GetBagByPos(i))
                for (uint32 s = 0; s < b->GetBagSize(); ++s)
                    if (Item* it = p_->GetItemByPos(i, s))
                        if (it->GetEntry() == item_entry) total += it->GetCount();
        for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
            if (Item* it = p_->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
                if (it->GetEntry() == item_entry) total += it->GetCount();
        return total;
    };
    uint32 const choice_in_bag = bag_has(choice_item);
    // Build a comma-list of guaranteed-reward items that landed (or not)
    // in bag. Quest::RewardItemId[0..N] holds the auto-grants.
    std::string fixed_summary;
    for (size_t i = 0; i < quest->RewardItemId.size(); ++i)
    {
        const uint32 item_id = quest->RewardItemId[i];
        if (item_id == 0) continue;
        const uint32 want = quest->RewardItemCount[i];
        const uint32 got = bag_has(item_id);
        if (!fixed_summary.empty()) fixed_summary += ',';
        fixed_summary += fmt::format("{}:{}/{}", item_id, got, want);
    }
    TC_LOG_INFO("playerbot.v2",
        "[complete_quest] {} OK quest={} lvl={} xp={} reward_item={} "
        "in_bag={} fixed_rewards=[{}]",
        p_->GetName(), quest_id, uint32(p_->GetLevel()),
        uint32(quest->XPValue(p_)), choice_item,
        choice_in_bag,
        fixed_summary);

    // Same-NPC follow-up auto-accept. Scripted escort NPCs (Tarindrella for
    // quest chain 28725→28726→28727→28728 in Shadowthread Cave) often despawn
    // a few seconds after a turn-in via their SmartAI OOC timer. The
    // post-RewardQuest world tick gives the AI ~1 snapshot to re-probe
    // CanTakeQuest and emit accept_quest before the giver vanishes — that
    // window can close before the bot's accept intent dispatches, producing
    // the "giver_not_found" / "giver_lost_relation" reject pair observed for
    // Uraimus 2026-05-21 (quest 28728 "Signs of Things to Come").
    //
    // Fix: while the giver is still here AND the gossip session is still
    // open (no SendCloseGossip yet), walk every quest the same NPC offers
    // and AddQuest the first one the bot CanTakeQuest accepts. Just-rewarded
    // 28727 has already moved to REWARDED status so SatisfyQuestPreviousQuest
    // on 28728 passes inside this same tick.
    if (Creature* creature_giver = object->ToCreature())
    {
        auto rel = sObjectMgr->GetCreatureQuestRelations(creature_giver->GetEntry());
        for (uint32 follow_qid : rel)
        {
            if (follow_qid == quest_id) continue;
            if (p_->GetQuestStatus(follow_qid) != QUEST_STATUS_NONE) continue;
            Quest const* follow_q = sObjectMgr->GetQuestTemplate(follow_qid);
            if (!follow_q) continue;
            if (!p_->CanTakeQuest(follow_q, false)) continue;
            if (!p_->CanAddQuest(follow_q, false)) continue;
            p_->AddQuestAndCheckCompletion(follow_q, object);
            TC_LOG_INFO("playerbot.v2",
                "[complete_quest] {} chained accept quest={} (after turnin {})",
                p_->GetName(), follow_qid, quest_id);
            break;   // one follow-up per turn-in; the next snapshot can pick
                     // up any others via the normal idle:quest_accept path
        }
    }

    p_->PlayerTalkClass->SendCloseGossip();
    return Result::Ok;
}

Result API::lfg_queue(uint32 dungeon_id, uint8 roles)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive())
    {
        TC_LOG_INFO("playerbot.v2",
            "[lfg_queue] {} reject: dead (dungeon={})", p_->GetName(), dungeon_id);
        return Result::Locked;
    }
    if (p_->InBattleground())
    {
        TC_LOG_INFO("playerbot.v2",
            "[lfg_queue] {} reject: in_bg (dungeon={})", p_->GetName(), dungeon_id);
        return Result::Locked;
    }
    // The LFG manager rejects re-queuing in random states without explicit
    // leave-first; mirror that by short-circuiting when the bot is already
    // active in the queue. Lfg_state == LFG_STATE_QUEUED|PROPOSAL means a
    // queue is in flight already.
    lfg::LfgState const state = sLFGMgr->GetState(p_->GetGUID());
    if (state == lfg::LFG_STATE_QUEUED || state == lfg::LFG_STATE_PROPOSAL ||
        state == lfg::LFG_STATE_DUNGEON || state == lfg::LFG_STATE_FINISHED_DUNGEON)
    {
        TC_LOG_INFO("playerbot.v2",
            "[lfg_queue] {} reject: already_in_state={} (dungeon={})",
            p_->GetName(), uint32(state), dungeon_id);
        return Result::Locked;
    }

    lfg::LfgDungeonSet dungeons;
    dungeons.insert(dungeon_id);
    sLFGMgr->JoinLfg(p_, roles, dungeons);
    // Verify JoinLfg actually queued us — if state didn't transition to
    // QUEUED, an LFG-side gate fired silently (RBAC, deserter aura, gear
    // ilvl, level mismatch, freeze debuff, group membership, etc.). This
    // is the diagnostic that lets us answer "filler queued 4, only 2 in
    // queue — why?" without grepping LFGMgr internals.
    lfg::LfgState const post_state = sLFGMgr->GetState(p_->GetGUID());
    if (post_state != lfg::LFG_STATE_QUEUED &&
        post_state != lfg::LFG_STATE_ROLECHECK &&
        post_state != lfg::LFG_STATE_PROPOSAL)
    {
        TC_LOG_WARN("playerbot.v2",
            "[lfg_queue] {} JoinLfg silently rejected: dungeon={} roles=0x{:02X} "
            "post_state={} (was {}); check LFG eligibility gates "
            "(level/gear/deserter/freeze/rbac)",
            p_->GetName(), dungeon_id, uint32(roles),
            uint32(post_state), uint32(state));
        return Result::ServerRefused;
    }
    return Result::Ok;
}

Result API::lfg_leave_queue()
{
    if (!p_) return Result::Other;
    sLFGMgr->LeaveLfg(p_->GetGUID());
    return Result::Ok;
}

Result API::equip_item(uint8 from_bag, uint8 from_slot, uint8 to_slot)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    if (p_->IsInCombat()) return Result::Locked;
    Item* src_item = p_->GetItemByPos(from_bag, from_slot);
    if (!src_item) return Result::InvalidTarget;

    // Server-refusal backoff. The caller (EquipUpgradeFire) re-issues this
    // every eligible tick while the item is still in the bag. If a prior
    // attempt was refused (and the item therefore never left the bag), skip
    // the swap until the backoff expires — otherwise we re-run CanEquipItem
    // and emit a phantom "Ok" every tick, which makes /diag report an equip
    // that will never land. Keyed on (item-entry, dest-slot).
    uint32 const efail_now = GameTime::GetGameTimeMS();
    uint32 const efail_key = EquipFailKey(src_item->GetEntry(), to_slot);
    if (EquipFailedRecently(p_, efail_key, efail_now))
        return Result::ServerRefused;

    // ---- Bag destination (slots 30-33) — B-11b ----
    // Containers don't go through CanEquipItem (that path refuses them);
    // Player::SwapItem natively supports "swap an EMPTY bag from inventory
    // with an equipped bag, transferring the equipped bag's contents into
    // the incoming one" (the same operation a player's drag-onto-bag-slot
    // performs). We add the bot-shaped guards SwapItem expects the client
    // to have applied, then let SwapItem do the real work and read the
    // truthful outcome back from the slot.
    if (to_slot >= INVENTORY_SLOT_BAG_START && to_slot < INVENTORY_SLOT_BAG_END)
    {
        Bag* src_bag = src_item->ToBag();
        if (!src_bag)
        {
            NoteEquipFail(p_, efail_key, efail_now);
            return Result::ServerRefused;          // not a container — rule bug
        }
        // SwapItem's content-exchange branch only runs when the INCOMING bag
        // is empty (a freshly looted / quest-rewarded bag always is). A bag
        // the bot has stored items into is a transient state — skip, the
        // periodic re-check retries after the bag empties.
        if (!src_bag->IsEmpty())
            return Result::Locked;
        // SwapItem refuses equipping a bag from INSIDE the bag being
        // replaced (EQUIP_ERR_CANT_SWAP, srcbag == dstslot). Relocate the
        // candidate to any other storage first; if nothing has room, retry
        // later rather than recording a permanent refusal.
        if (from_bag == to_slot)
        {
            ItemPosCountVec relocate;
            if (p_->CanStoreItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, relocate,
                                 src_item, false) != EQUIP_ERR_OK)
                return Result::Locked;
            p_->RemoveItem(from_bag, from_slot, true);
            p_->StoreItem(relocate, src_item, true);
        }
        uint16 const bag_src = src_item->GetPos();
        uint16 const bag_dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | to_slot;
        p_->SwapItem(bag_src, bag_dst);
        // SwapItem is void and reports failures only via SendEquipError to
        // the (non-existent) client. The truthful check: did our bag land
        // in the slot?
        Item* now_there = p_->GetItemByPos(INVENTORY_SLOT_BAG_0, to_slot);
        if (now_there == src_item)
            return Result::Ok;
        NoteEquipFail(p_, efail_key, efail_now);
        TC_LOG_DEBUG("playerbot.v2",
            "[equip_item] {} bag swap refused item={} -> bag slot {} (backing off {}ms)",
            p_->GetName(), src_item->GetEntry(), uint32(to_slot), kEquipFailTtlMs);
        return Result::ServerRefused;
    }

    // Pre-validate the equip the same way Player::SwapItem will internally,
    // so we can return a TRUTHFUL result instead of an unconditional Ok.
    // SwapItem returns void and only SendEquipError's the client on failure,
    // leaving the item in the bag — invisible to the API caller. CanEquipItem
    // with swap=true mirrors the swap case (destination slot may be occupied;
    // the current item gets swapped back into the source slot). This is the
    // exact gate SwapItem applies, so a non-OK result here means SwapItem
    // would also refuse.
    uint16 equip_dest = 0;
    InventoryResult const equip_err =
        p_->CanEquipItem(to_slot, equip_dest, src_item, /*swap*/ true);
    if (equip_err != EQUIP_ERR_OK)
    {
        // Record the refusal with a long TTL and surface it. The rule's next
        // tick sees ServerRefused (not Ok) so /diag no longer reports a
        // phantom pending equip, and EquipFailedRecently short-circuits the
        // re-attempt for kEquipFailTtlMs.
        NoteEquipFail(p_, efail_key, efail_now);
        TC_LOG_DEBUG("playerbot.v2",
            "[equip_item] {} refused item={} -> slot={} err={} (backing off {}ms)",
            p_->GetName(), src_item->GetEntry(), uint32(to_slot),
            uint32(equip_err), kEquipFailTtlMs);
        return Result::ServerRefused;
    }

    // Validated equip. SwapItem performs the unequip+equip; the item already
    // at the destination is swapped back into the source slot. Equipped slot
    // lives in INVENTORY_SLOT_BAG_0 (=255).
    const uint16 src = (uint16(from_bag) << 8) | from_slot;
    const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | to_slot;
    p_->SwapItem(src, dst);
    return Result::Ok;
}

Result API::accept_shared_quest()
{
    if (!p_) return Result::Other;
    const ObjectGuid sender_guid = p_->GetPlayerSharingQuest();
    const uint32 quest_id = p_->GetSharedQuestID();
    if (sender_guid.IsEmpty() || quest_id == 0) return Result::Locked;
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest) { p_->ClearQuestSharingInfo(); return Result::NotKnown; }
    Player* sender = ObjectAccessor::FindConnectedPlayer(sender_guid);
    if (!p_->CanTakeQuest(quest, false) || !p_->CanAddQuest(quest, false))
    {
        p_->ClearQuestSharingInfo();
        return Result::ServerRefused;
    }
    p_->AddQuestAndCheckCompletion(quest, sender);
    p_->ClearQuestSharingInfo();
    return Result::Ok;
}

Result API::vendor_buy_by_category(ObjectGuid npc, uint8 item_class, uint8 item_subclass,
                                    uint8 total_count)
{
    if (!p_) return Result::Other;
    if (total_count == 0) return Result::Ok;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;
    VendorItemData const* items = vendor->GetVendorItems();
    if (!items || items->Empty()) return Result::OutOfRange;
    const uint32 bot_level = p_->GetLevel();

    // Pick the best matching slot first: highest required-level item the bot
    // can actually use. For most vendors there's a single food entry per
    // level bracket so this resolves cleanly; when multiple match (Algari
    // Travel Ration vs Loose-Leaf Tea) we'd happily buy whichever shows up
    // highest — both restore the same well-fed buff.
    uint32 best_slot = items->GetItemCount();
    int32 best_required_level = -1;
    uint32 best_buy_count = 1;     // fallback if BuyCount is 0 (older data)
    for (uint32 slot_i = 0; slot_i < items->GetItemCount(); ++slot_i)
    {
        VendorItem const* vi = items->GetItem(slot_i);
        if (!vi || !vi->item) continue;
        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(vi->item);
        if (!tmpl) continue;
        if (tmpl->GetClass() != item_class) continue;
        if (tmpl->GetSubClass() != item_subclass) continue;
        const int32 req_lvl = tmpl->GetBaseRequiredLevel();
        if (req_lvl > static_cast<int32>(bot_level)) continue;
        if (req_lvl < best_required_level) continue;
        best_slot = slot_i;
        best_required_level = req_lvl;
        best_buy_count = vi->maxcount > 0 ? std::min<uint32>(vi->maxcount, tmpl->GetBuyCount()) : tmpl->GetBuyCount();
        if (best_buy_count == 0) best_buy_count = 1;
    }
    if (best_slot >= items->GetItemCount()) return Result::OutOfRange;

    // Issue BuyItemFromVendorSlot in chunks of best_buy_count until total_count
    // units are bought. Each call deducts gold + verifies bag space; we stop
    // on the first failure (out of money / bag full / vendor restock).
    VendorItem const* slot = items->GetItem(best_slot);
    uint32 bought = 0;
    while (bought < total_count)
    {
        const uint32 chunk = std::min<uint32>(best_buy_count, total_count - bought);
        if (!p_->BuyItemFromVendorSlot(npc, best_slot, slot->item, chunk, NULL_BAG, NULL_SLOT))
        {
            if (bought > 0) return Result::Ok;            // partial buy is fine
            return Result::NotEnoughResource;
        }
        bought += chunk;
    }
    return Result::Ok;
}

Result API::vendor_buy_by_entry(ObjectGuid npc, uint32 item_entry, uint32 count)
{
    // Convenience wrapper around vendor_buy_by_slot — walks the vendor's
    // inventory looking for the matching item entry, then issues the buy.
    // Useful when callers know the item ID (script integrations, recipe
    // reagent lookup) but not the slot index.
    if (!p_) return Result::Other;
    if (count == 0) return Result::Ok;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;
    VendorItemData const* items = vendor->GetVendorItems();
    if (!items || items->Empty()) return Result::InvalidTarget;
    for (uint32 slot_i = 0; slot_i < items->GetItemCount(); ++slot_i)
    {
        VendorItem const* vi = items->GetItem(slot_i);
        if (!vi || vi->item != item_entry) continue;
        if (!p_->BuyItemFromVendorSlot(npc, slot_i, item_entry, count, NULL_BAG, NULL_SLOT))
            return Result::ServerRefused;
        return Result::Ok;
    }
    return Result::OutOfRange;
}

Result API::vendor_buy_by_slot(ObjectGuid npc, uint32 vendor_slot, uint32 count)
{
    if (!p_) return Result::Other;
    Creature* vendor = p_->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_VENDOR, UNIT_NPC_FLAG_2_NONE);
    if (!vendor) return Result::InvalidTarget;
    VendorItemData const* items = vendor->GetVendorItems();
    if (!items || items->Empty()) return Result::InvalidTarget;
    if (vendor_slot >= items->GetItemCount()) return Result::OutOfRange;
    VendorItem const* slot = items->GetItem(vendor_slot);
    if (!slot || !slot->item) return Result::InvalidTarget;
    // BuyItemFromVendorSlot does the full checks (gold, conditions, faction,
    // inventory space, vendor restock); auto-store via NULL_BAG/NULL_SLOT.
    if (!p_->BuyItemFromVendorSlot(npc, vendor_slot, slot->item, count, NULL_BAG, NULL_SLOT))
        return Result::ServerRefused;
    return Result::Ok;
}

Result API::jump(float forward)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;
    if (p_->IsFalling())  return Result::Locked;
    // M-P2c: obstacle-aware bearing selection. A hop at the bot's current
    // facing nearly always launches INTO whatever wedged it (the bot was
    // walking toward its goal when it jammed, so it's still pointing at the
    // wall / ledge / closed door). TryUnstickJump tries the current facing,
    // then ±90°, then 180° (reverse out of a dead-end pocket), and launches
    // the first bearing whose landing has real ground and a clear arc chord.
    // It faces the chosen bearing before MoveJump so the arc travels that
    // way. Returns false only when every bearing is blocked / groundless —
    // refuse rather than launch the bot into geometry / the void.
    if (!BotMovement::TryUnstickJump(p_, forward))
        return Result::Locked;
    return Result::Ok;
}

// Validates that `mailbox` is a Mailbox-flagged GO or NPC the bot can
// currently interact with. Mirrors WorldSession::CanOpenMailBox without
// the cheat-chat self-guid path (bots have no GM permissions).
static bool CanInteractWithMailbox(Player* p, ObjectGuid mailbox)
{
    if (!p) return false;
    if (mailbox.IsGameObject())
        return p->GetGameObjectIfCanInteractWith(mailbox, GAMEOBJECT_TYPE_MAILBOX) != nullptr;
    if (mailbox.IsAnyTypeCreature())
        return p->GetNPCIfCanInteractWith(mailbox, UNIT_NPC_FLAG_MAILBOX, UNIT_NPC_FLAG_2_NONE) != nullptr;
    return false;
}

Result API::mail_take_money(ObjectGuid mailbox, uint64 mail_id)
{
    if (!p_) return Result::Other;
    if (!CanInteractWithMailbox(p_, mailbox)) return Result::InvalidTarget;
    Mail* m = p_->GetMail(mail_id);
    if (!m || m->state == MAIL_STATE_DELETED) return Result::NotKnown;
    if (m->deliver_time > GameTime::GetGameTime()) return Result::Locked;
    if (m->money == 0) return Result::Locked;

    // ModifyMoney with the strict overflow guard. Returns false when the
    // bot is over the gold cap, in which case we surface NotEnoughResource
    // rather than mutating mail state — mirrors EQUIP_ERR_TOO_MUCH_GOLD.
    if (!p_->ModifyMoney(m->money, false))
        return Result::NotEnoughResource;

    m->money = 0;
    m->state = MAIL_STATE_CHANGED;
    p_->m_mailsUpdated = true;

    // Persist inventory immediately so the gold pickup is durable across a
    // crash. The mail row mutation (state=CHANGED, m_mailsUpdated=true) is
    // picked up by the next player save (Player::_SaveMail walks the mail
    // vector unconditionally when m_mailsUpdated is set). _SaveMail is
    // protected on Player and not friend-accessible from this module, so we
    // can't co-commit it inside our own transaction — relying on the
    // periodic save tick for mail durability is the same pattern Trinity
    // uses for the in-game UI mail-money take button.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::mail_take_item(ObjectGuid mailbox, uint64 mail_id, uint64 item_guid_low)
{
    if (!p_) return Result::Other;
    if (!CanInteractWithMailbox(p_, mailbox)) return Result::InvalidTarget;
    Mail* m = p_->GetMail(mail_id);
    if (!m || m->state == MAIL_STATE_DELETED) return Result::NotKnown;
    if (m->deliver_time > GameTime::GetGameTime()) return Result::Locked;

    // The handler's cheat check: the requested attachment must actually be
    // on this mail. Bots could only ever drive this from snapshot data, but
    // mail state mutates between snapshot and execution (delete in flight),
    // so we re-validate here.
    auto it = std::find_if(m->items.begin(), m->items.end(),
                           [item_guid_low](MailItemInfo const& info)
                           { return info.item_guid == item_guid_low; });
    if (it == m->items.end()) return Result::NotKnown;
    if (!p_->HasEnoughMoney(m->COD)) return Result::NotEnoughResource;

    Item* item = p_->GetMItem(item_guid_low);
    if (!item) return Result::NotKnown;

    ItemPosCountVec dest;
    InventoryResult inv = p_->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
    if (inv != EQUIP_ERR_OK) return Result::InventoryFull;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    m->RemoveItem(item_guid_low);
    m->removedItems.push_back(item_guid_low);

    // COD bookkeeping: bot pays sender via a generated mail; we mirror the
    // live handler's logic (account-id resolution, sender online or offline).
    if (m->COD > 0)
    {
        ObjectGuid sender_guid = ObjectGuid::Create<HighGuid::Player>(m->sender);
        Player* receiver = ObjectAccessor::FindConnectedPlayer(sender_guid);
        uint32 sender_acc = receiver ? receiver->GetSession()->GetAccountId()
                                     : sCharacterCache->GetCharacterAccountIdByGuid(sender_guid);
        if (receiver || sender_acc)
        {
            MailDraft(m->subject, "")
                .AddMoney(m->COD)
                .SendMailTo(trans,
                            MailReceiver(receiver, m->sender),
                            MailSender(MAIL_NORMAL, m->receiver),
                            MAIL_CHECK_MASK_COD_PAYMENT);
        }
        p_->ModifyMoney(-int64(m->COD));
    }
    m->COD = 0;
    m->state = MAIL_STATE_CHANGED;
    p_->m_mailsUpdated = true;
    p_->RemoveMItem(item_guid_low);

    item->SetState(ITEM_UNCHANGED);   // pre-store, so subsequent moves stay valid
    p_->MoveItemToInventory(dest, item, true);
    p_->SaveInventoryAndGoldToDB(trans);
    // _SaveMail is protected; rely on m_mailsUpdated for the next periodic
    // save (same rationale as mail_take_money — see comment there).
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::mail_delete(ObjectGuid mailbox, uint64 mail_id)
{
    if (!p_) return Result::Other;
    if (!CanInteractWithMailbox(p_, mailbox)) return Result::InvalidTarget;
    Mail* m = p_->GetMail(mail_id);
    if (!m || m->state == MAIL_STATE_DELETED) return Result::NotKnown;
    // The live handler refuses delete on COD-bearing mail (the COD has to
    // be either paid or the mail returned to sender). Mirror that.
    if (m->COD > 0) return Result::Locked;
    m->state = MAIL_STATE_DELETED;
    p_->m_mailsUpdated = true;
    return Result::Ok;
}

Result API::bind_homebind(ObjectGuid innkeeper)
{
    if (!p_) return Result::Other;
    if (!p_->IsAlive()) return Result::Locked;
    if (p_->GetMap() && p_->GetMap()->Instanceable()) return Result::Locked;
    Creature* npc = p_->GetNPCIfCanInteractWith(innkeeper, UNIT_NPC_FLAG_INNKEEPER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;
    // Spell 3286 = Bind. Cast from the NPC at the bot — the spell handler
    // updates the homebind row.
    npc->CastSpell(p_, /*Bind*/ 3286, true);
    return Result::Ok;
}

Result API::loot_roll(ObjectGuid loot_object, uint8 loot_list_id, uint8 vote_type)
{
    if (!p_) return Result::Other;
    LootRoll* roll = p_->GetLootRoll(loot_object, loot_list_id);
    if (!roll) return Result::Locked;
    // RollVote enum values match the wire protocol: Pass=0, Need=1,
    // Greed=2, Disenchant=3. Anything outside that range is forced to
    // Pass (safe default — never accidentally take loot).
    RollVote vote = RollVote::Pass;
    switch (vote_type)
    {
        case 1: vote = RollVote::Need; break;
        case 2: vote = RollVote::Greed; break;
        case 3: vote = RollVote::Disenchant; break;
        default: break;   // Pass
    }
    return roll->PlayerVote(p_, vote) ? Result::Ok : Result::ServerRefused;
}

// Helper: parse a CSV-of-colon-separated-triples build string into a list of
// (nodeID, entryID, ranks). Format: "12345:67890:1,12346:67891:2". Ignores
// whitespace. On parse error the partial result is returned (caller falls
// back to starter build). Defined here to keep the trait code in one TU.
namespace
{
struct CuratedTraitEntry { int32 nodeId; int32 entryId; int32 ranks; };

std::vector<CuratedTraitEntry> ParseCuratedBuild(std::string const& text)
{
    std::vector<CuratedTraitEntry> out;
    out.reserve(64);
    size_t i = 0, n = text.size();
    auto eat_ws = [&]() { while (i < n && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n')) ++i; };
    auto read_int = [&](int32& v) -> bool
    {
        eat_ws();
        bool neg = (i < n && text[i] == '-'); if (neg) ++i;
        if (i >= n || text[i] < '0' || text[i] > '9') return false;
        int64 acc = 0;
        while (i < n && text[i] >= '0' && text[i] <= '9') { acc = acc * 10 + (text[i] - '0'); ++i; }
        v = neg ? -int32(acc) : int32(acc);
        return true;
    };
    while (i < n)
    {
        CuratedTraitEntry e{};
        if (!read_int(e.nodeId))  break;
        eat_ws(); if (i >= n || text[i] != ':') break; ++i;
        if (!read_int(e.entryId)) break;
        eat_ws(); if (i >= n || text[i] != ':') break; ++i;
        if (!read_int(e.ranks))   break;
        out.push_back(e);
        eat_ws();
        if (i >= n) break;
        if (text[i] != ',') break;
        ++i;
    }
    return out;
}

// Look up a curated build row in playerbot_v2_talent_build, with fallback
// from (class, spec, context) → (class, spec, Default). Returns the
// `entries_json` text (despite the column name, we use a CSV format).
// Empty string means no curated row found.
std::string LookupCuratedBuildRow(uint32 classId, uint32 specId, uint8 context)
{
    auto try_row = [&](uint8 ctx) -> std::string
    {
        auto r = CharacterDatabase.PQuery(
            "SELECT entries_json FROM playerbot_v2_talent_build "
            "WHERE class_id={} AND spec_id={} AND context={}",
            classId, specId, ctx);
        if (!r) return {};
        return r->Fetch()[0].GetString();
    };
    if (auto s = try_row(context); !s.empty()) return s;
    if (context != 0)
        if (auto s = try_row(0); !s.empty()) return s;
    return {};
}

// Leveling spec per class (the DPS spec the baseline APLs are tuned for —
// mirrors ApRegistry::DefaultSpecForClass). Used to promote a bot off the
// class's INITIAL placeholder specialization, which 92% of the L10+ fleet
// was still on: the Initial spec has no Combat TraitConfig, so every talent
// path returned NotKnown silently and 13K+ bots fought with zero talents,
// zero spec spells and zero mastery (audit B06).
uint32 LevelingSpecForClass(uint8 cls)
{
    switch (cls)
    {
        case 1:  return 71;   // Warrior   -> Arms
        case 2:  return 70;   // Paladin   -> Retribution
        case 3:  return 253;  // Hunter    -> Beast Mastery
        case 4:  return 259;  // Rogue     -> Assassination
        case 5:  return 258;  // Priest    -> Shadow
        case 6:  return 252;  // DK        -> Unholy
        case 7:  return 263;  // Shaman    -> Enhancement
        case 8:  return 64;   // Mage      -> Frost
        case 9:  return 265;  // Warlock   -> Affliction
        case 10: return 269;  // Monk      -> Windwalker
        case 11: return 102;  // Druid     -> Balance
        case 12: return 577;  // DH        -> Havoc
        case 13: return 1467; // Evoker    -> Devastation
        default: return 0;
    }
}

// Promote the bot to a REAL specialization if it still sits on the class's
// Initial placeholder. ActivateTalentGroup learns spec spells + mastery and
// creates/activates the matching Combat TraitConfig — the precondition for
// any talent application. Returns false when no spec could be resolved.
bool EnsureRealSpec(Player* p)
{
    if (p->GetLevel() < 10)
        return false;  // retail unlocks specialization at L10 — nothing to do yet
    ChrSpecializationEntry const* initial =
        sDB2Manager.GetDefaultChrSpecializationForClass(p->GetClass());
    const uint32 cur = uint32(AsUnderlyingType(p->GetPrimarySpecialization()));
    if (initial == nullptr || cur != initial->ID)
        return true;   // already on a real spec
    ChrSpecializationEntry const* target =
        sChrSpecializationStore.LookupEntry(LevelingSpecForClass(uint8(p->GetClass())));
    if (!target)
        return false;
    p->ActivateTalentGroup(target);
    TC_LOG_INFO("playerbot.api",
        "[API::talents] {} promoted from Initial spec {} to {} (class {})",
        p->GetName(), initial->ID, target->ID, uint32(p->GetClass()));
    return true;
}

// Count purchased ranks in the bot's ACTIVE trait config — the ground-truth
// "talents actually applied" signal (the legacy GetTalentMap is never
// written by the 12.0 trait system).
uint32 CountActiveTraitRanks(Player* p)
{
    int32 const active_id = p->m_activePlayerData->ActiveCombatTraitConfigID;
    UF::TraitConfig const* cfg = p->GetTraitConfig(active_id);
    if (!cfg) return 0;
    uint32 ranks = 0;
    for (auto const& e : cfg->Entries)
        ranks += uint32(std::max<int32>(0, e.Rank));
    return ranks;
}
} // anonymous

Result API::apply_talent_build(uint8 context)
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;

    // Promote off the Initial placeholder spec first — without a real spec
    // there is no Combat TraitConfig and this path silently no-op'd for 92%
    // of the L10+ fleet (audit B05/B06).
    if (!EnsureRealSpec(p_)) return Result::NotKnown;

    int32 const active_id = p_->m_activePlayerData->ActiveCombatTraitConfigID;
    UF::TraitConfig const* active = p_->GetTraitConfig(active_id);
    if (!active) return Result::NotKnown;

    const uint32 classId = p_->GetClass();
    const uint32 specId  = uint32(AsUnderlyingType(p_->GetPrimarySpecialization()));

    // Look up curated build. If none, fall back to TraitMgr's starter
    // build — that's still a valid spec-aware spend (just not optimized
    // for raid / M+ / PvP).
    std::string curatedCsv = LookupCuratedBuildRow(classId, specId, context);

    WorldPackets::Traits::TraitConfig new_cfg(*active);
    if (!curatedCsv.empty())
    {
        auto entries = ParseCuratedBuild(curatedCsv);
        if (!entries.empty())
        {
            new_cfg.Entries.clear();
            new_cfg.Entries.reserve(entries.size());
            for (auto const& e : entries)
            {
                WorldPackets::Traits::TraitEntry te{};
                te.TraitNodeID      = e.nodeId;
                te.TraitNodeEntryID = e.entryId;
                te.Rank             = e.ranks;
                new_cfg.Entries.push_back(te);
            }
            // Validate + auto-fix any invalid entries (common when curated
            // data is stale relative to a patch). removeInvalidEntries=true
            // strips drops; the resulting config is committed best-effort.
            TraitMgr::LearnResult vr = TraitMgr::ValidateConfig(new_cfg, p_,
                /*requireSpendingAllCurrencies=*/ false,
                /*removeInvalidEntries=*/ true);
            // A curated build is a MAX-LEVEL spend. removeInvalidEntries only
            // strips STRUCTURALLY-invalid nodes — it does NOT trim for talent-
            // currency overflow, so an under-level bot's config still holds the
            // full endgame node set and ValidateConfig returns an error (e.g.
            // NotEnoughTalentsInPrimaryTree) WITHOUT pruning. The old code
            // (void)-cast that result and committed anyway, giving an L11 bot the
            // entire endgame kit. Only commit a curated build that actually
            // validates; otherwise reset to a clean config and fall through to
            // the level-budgeted starter build below.
            if (vr == TraitMgr::LearnResult::Ok)
            {
                // withCastTime=false: bots have no client to animate the 1.5s
                // "Applying Talents" commit cast for, and leveling bots move
                // constantly — the interruptible cast was the reason commits
                // never landed fleet-wide (audit B05). Verify the result.
                p_->UpdateTraitConfig(std::move(new_cfg), 0, /*withCastTime*/ false);
                const uint32 ranks = CountActiveTraitRanks(p_);
                TC_LOG_INFO("playerbot.api",
                    "[API::talents] {} curated build (ctx {}) committed: {} ranks active",
                    p_->GetName(), uint32(context), ranks);
                return ranks > 0 ? Result::Ok : Result::Locked;
            }
            new_cfg = WorldPackets::Traits::TraitConfig(*active);   // reset before starter fallback
        }
    }

    // No curated build → starter build. Same as apply_starter_talents.
    TraitMgr::InitializeStarterBuildTraitConfig(new_cfg, p_);
    new_cfg.CombatConfigFlags |= TraitCombatConfigFlags::StarterBuild;
    p_->UpdateTraitConfig(std::move(new_cfg), 0, /*withCastTime*/ false);
    {
        const uint32 ranks = CountActiveTraitRanks(p_);
        TC_LOG_INFO("playerbot.api",
            "[API::talents] {} starter build (ctx {} fallback) committed: {} ranks active",
            p_->GetName(), uint32(context), ranks);
        return ranks > 0 ? Result::Ok : Result::Locked;
    }
}

Result API::apply_starter_talents()
{
    if (!p_) return Result::Other;
    if (p_->IsInCombat()) return Result::Locked;

    // Promote off the Initial placeholder spec first (audit B06): without a
    // real spec there is no Combat TraitConfig and this path returned
    // NotKnown silently for 92% of the L10+ fleet — 13K+ bots at 0 talents.
    if (!EnsureRealSpec(p_)) return Result::NotKnown;

    int32 const active_id = p_->m_activePlayerData->ActiveCombatTraitConfigID;
    UF::TraitConfig const* active = p_->GetTraitConfig(active_id);
    if (!active) return Result::NotKnown;

    // Build a writable copy of the active config, then ask TraitMgr to fill
    // it with the curated starter selections. This is the same code path
    // the client triggers when the player toggles "Starter Build" — the
    // resulting config is a complete, valid trait spend for the spec.
    WorldPackets::Traits::TraitConfig new_cfg(*active);
    TraitMgr::InitializeStarterBuildTraitConfig(new_cfg, p_);
    new_cfg.CombatConfigFlags |= TraitCombatConfigFlags::StarterBuild;

    // withCastTime=false (audit B05): the 1.5s "Applying Talents" commit
    // cast exists for client presentation; leveling bots move constantly,
    // the cast was interrupted, the result was never checked, and the
    // fire-once idle rule never retried — so commits never landed anywhere.
    // Commit immediately and VERIFY by re-reading the active config.
    p_->UpdateTraitConfig(std::move(new_cfg), 0, /*withCastTime*/ false);
    const uint32 ranks = CountActiveTraitRanks(p_);
    TC_LOG_INFO("playerbot.api",
        "[API::talents] {} starter build committed: {} ranks active (spec {})",
        p_->GetName(), ranks,
        uint32(AsUnderlyingType(p_->GetPrimarySpecialization())));
    return ranks > 0 ? Result::Ok : Result::Locked;
}

Result API::bg_queue(ObjectGuid battlemaster, uint16 bg_type_id, uint8 arena_type)
{
    if (!p_) return Result::Other;
    // Empty battlemaster GUID signals the UI-equivalent path (PVP browser /
    // queue-from-anywhere) — no NPC interaction required. Bot-driven queues
    // (BotQueueFiller, /bg whisper, JIT spawn) use this path because the bot
    // isn't always standing at a battlemaster. Non-empty GUID still validates
    // NPC range as before for /bg via NPC click.
    if (!battlemaster.IsEmpty())
    {
        Creature* npc = p_->GetNPCIfCanInteractWith(battlemaster, UNIT_NPC_FLAG_BATTLEMASTER, UNIT_NPC_FLAG_2_NONE);
        if (!npc)
        {
            TC_LOG_INFO("playerbot.v2",
                "[bg_queue] {} reject: battlemaster_not_found (bg_type_id={})",
                p_->GetName(), bg_type_id);
            return Result::InvalidTarget;
        }
    }

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(bg_type_id);
    BattlegroundTemplate const* bg_tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bg_tmpl)
    {
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} reject: no_template (bg_type_id={})",
            p_->GetName(), bg_type_id);
        return Result::NotKnown;
    }
    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bg_type_id, nullptr))
    {
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} reject: bg_disabled (bg_type_id={})",
            p_->GetName(), bg_type_id);
        return Result::Locked;
    }

    PVPDifficultyEntry const* bracket =
        DB2Manager::GetBattlegroundBracketByLevel(bg_tmpl->MapIDs.front(), p_->GetLevel());
    if (!bracket)
    {
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} reject: no_bracket (lvl={} bg_type_id={} map={})",
            p_->GetName(), uint32(p_->GetLevel()), bg_type_id,
            bg_tmpl->MapIDs.front());
        return Result::Locked;
    }

    // Arena vs battleground routing. Arenas live under a separate queue
    // category (BattlegroundQueueIdType::Arena) keyed by team size; the
    // BG category is HARDCODED-teamSize-0. `arena_type` (2/3/5) selects
    // the skirmish bracket; we also treat any arena-flagged template as
    // an arena even if the caller passed 0 (defensive — a bot should
    // never end up BG-queued onto an arena map). rated=false: bots run
    // SKIRMISH arenas (no arena-team / MMR dependency), mirroring
    // HandleBattlemasterJoinArena with arena teams disabled.
    const bool is_arena = arena_type > 0 || bg_tmpl->IsArena();
    // Bots queue UNRATED SKIRMISH only (they have no persistent ArenaTeam).
    // Per BattlegroundMgr::IsValidQueueId, the unrated path is the dedicated
    // ArenaSkirmish queue type which (in this core) is 3v3-ONLY and still
    // carries the Rated bit set. The plain Arena queue type REQUIRES rated +
    // an ArenaTeam, so it can never be a bot path. Hence arena == 3v3 skirmish.
    if (is_arena && arena_type != 3)
    {
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} reject: arena_skirmish_is_3v3_only (arena_type={} bg_type_id={})",
            p_->GetName(), uint32(arena_type), bg_type_id);
        return Result::NotKnown;
    }
    BattlegroundQueueTypeId qid = is_arena
        ? BattlegroundMgr::BGQueueTypeId(bg_type_id,
                                         BattlegroundQueueIdType::ArenaSkirmish,
                                         /*rated*/ true,   // IsValidQueueId requires the bit set for skirmish
                                         /*teamSize*/ arena_type)
        : BattlegroundMgr::BGQueueTypeId(bg_type_id,
                                         BattlegroundQueueIdType::Battleground,
                                         /*rated*/ false,
                                         /*teamSize*/ 0);
    if (!BattlegroundMgr::IsValidQueueId(qid))
    {
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} reject: invalid_qid (bg_type_id={} is_arena={} arena_type={})",
            p_->GetName(), bg_type_id, is_arena ? 1 : 0, uint32(arena_type));
        return Result::NotKnown;
    }

    // GROUP path (audit B25): premade bot groups (guild BG-night,
    // BgTeamForming) queue AS A GROUP, mirroring the live handler's group
    // branch in HandleBattlemasterJoinOpcode. The old categorical rejection
    // made every premade BG event dead code: the guild scheduler announced
    // BG night, the builder formed the team, the leader's queue intent was
    // dropped here, and nothing ever happened.
    if (Group* grp = p_->GetGroup())
    {
        if (grp->GetLeaderGUID() != p_->GetGUID())
        { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: grouped_not_leader", p_->GetName()); return Result::Locked; }
        if (p_->InBattleground())
        { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: in_bg", p_->GetName()); return Result::Locked; }
        ObjectGuid errorGuid;
        // Arena path mirrors HandleBattlemasterJoinArena: Min/MaxPlayerCount
        // == arena_type (the exact team size), isRated=false (skirmish),
        // arenaSlot left 0 (slot is only meaningful for rated arena-team
        // play, which bots don't do). BG path keeps the original
        // (0, MaxPlayersPerTeam, false, 0) shape.
        GroupJoinBattlegroundResult err = is_arena
            ? grp->CanJoinBattlegroundQueue(bg_tmpl, qid, arena_type, arena_type,
                                            /*isRated*/ false, /*arenaSlot*/ 0, errorGuid)
            : grp->CanJoinBattlegroundQueue(bg_tmpl, qid, 0,
                                            bg_tmpl->GetMaxPlayersPerTeam(),
                                            false, 0, errorGuid);
        if (err)
        {
            TC_LOG_INFO("playerbot.v2",
                "[bg_queue] {} reject: group CanJoinBattlegroundQueue err={} (bg_type_id={} arena_type={})",
                p_->GetName(), int32(err), bg_type_id, uint32(arena_type));
            return Result::Locked;
        }
        const bool isPremade = !is_arena &&
            grp->GetMembersCount() >= bg_tmpl->GetMinPlayersPerTeam();
        BattlegroundQueue& gqueue = sBattlegroundMgr->GetBattlegroundQueue(qid);
        // AddGroup(leader, group, team, bracket, isPremade, ArenaRating,
        // MatchmakerRating). For skirmish arenas the live handler passes a
        // sentinel rating of 1/1 (no real arena-team rating); reuse that so
        // the matchmaker's rating window logic behaves identically.
        GroupQueueInfo* gginfo = is_arena
            ? gqueue.AddGroup(p_, grp, Team(p_->GetTeam()), bracket,
                              /*isPremade*/ false, /*ArenaRating*/ 1, /*MatchmakerRating*/ 1)
            : gqueue.AddGroup(p_, grp, Team(p_->GetTeam()), bracket,
                              false, isPremade, 0);
        if (!gginfo)
        { TC_LOG_WARN("playerbot.v2", "[bg_queue] {} reject: group AddGroup null (bg_type_id={})", p_->GetName(), bg_type_id); return Result::ServerRefused; }
        for (GroupReference const& itr : grp->GetMembers())
            if (Player* member = itr.GetSource())
                member->AddBattlegroundQueueId(qid);
        sBattlegroundMgr->ScheduleQueueUpdate(0, qid, bracket->GetBracketId());
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} queued GROUP of {} for bg_type_id={} (arena_type={} premade={})",
            p_->GetName(), grp->GetMembersCount(), bg_type_id, uint32(arena_type),
            isPremade ? 1 : 0);
        // Fill the rest of the match (both factions — most importantly the
        // OPPOSING team, which nothing else provides for a bot premade).
        // The module's hook handler lets bot GROUP LEADERS through for
        // exactly this purpose (audit B25). For arenas the opposing team is
        // an independently-seeded bot group (SeedArenaMatches forms one per
        // faction every cycle), so this BG-fill hook is BG-only.
        if (!is_arena)
            Playerbot::Hooks::OnPlayerJoinedBgQueue(p_, bg_type_id, bracket->GetBracketId());
        return Result::Ok;
    }
    // SOLO path below. Arenas have no solo queue — they require a full
    // arena_type-sized group whose leader queues (the GROUP branch above).
    // A solo arena request can only come from a mis-emitted intent (the
    // group dissolved before the deferred queue fired); reject it cleanly
    // rather than silently BG-queuing onto an arena map.
    if (is_arena)
    { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: arena_requires_group (arena_type={} bg_type_id={})", p_->GetName(), uint32(arena_type), bg_type_id); return Result::Locked; }
    if (p_->InBattleground())
    { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: in_bg", p_->GetName()); return Result::Locked; }
    if (!p_->CanJoinToBattleground(bg_tmpl))
    { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: CanJoinToBattleground=false (bg_type_id={})", p_->GetName(), bg_type_id); return Result::Locked; }
    if (p_->IsDeserter())
    { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: deserter", p_->GetName()); return Result::Locked; }
    if (p_->GetBattlegroundQueueIndex(qid) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return Result::Ok;   // already queued — idempotent
    if (!p_->HasFreeBattlegroundQueueId())
    { TC_LOG_INFO("playerbot.v2", "[bg_queue] {} reject: no_free_queue_slot", p_->GetName()); return Result::Locked; }

    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(qid);
    GroupQueueInfo* ginfo = queue.AddGroup(p_, nullptr, Team(p_->GetTeam()), bracket,
                                           /*isPremade*/ false, false, 0);
    if (!ginfo)
    { TC_LOG_WARN("playerbot.v2", "[bg_queue] {} reject: AddGroup returned null (bg_type_id={})", p_->GetName(), bg_type_id); return Result::ServerRefused; }
    p_->AddBattlegroundQueueId(qid);
    sBattlegroundMgr->ScheduleQueueUpdate(0, qid, bracket->GetBracketId());
    return Result::Ok;
}

Result API::lfg_role_check(uint8 roles)
{
    if (!p_) return Result::Other;
    Group* g = p_->GetGroup();
    if (!g) return Result::Locked;
    if (sLFGMgr->IsRoleCheckPending(g->GetGUID(), p_->GetGUID()))
    {
        sLFGMgr->UpdateRoleCheck(g->GetGUID(), p_->GetGUID(), roles);
        return Result::Ok;
    }
    // No pending LFG role-check (manually formed group via `;all run` or
    // similar). Set the role directly on the group so the UI shows the
    // role icon for the member. Without this, dungeon groups formed
    // outside the LFG queue look like a flat party (no role icons),
    // and Group::SetLfgRoles is the same call LFG itself uses on a
    // successful role-check round.
    g->SetLfgRoles(p_->GetGUID(), roles);
    return Result::Ok;
}

Result API::lfg_proposal_respond(uint32 proposal_id, bool accept)
{
    if (!p_) return Result::Other;
    if (proposal_id == 0) return Result::Locked;
    // sLFGMgr->UpdateProposal silently no-ops on missing proposal — re-check
    // via the read-only accessor so we surface the state to the caller.
    if (sLFGMgr->GetActiveProposalIdForPlayer(p_->GetGUID()) != proposal_id)
        return Result::Locked;
    sLFGMgr->UpdateProposal(proposal_id, p_->GetGUID(), accept);
    return Result::Ok;
}

Result API::bg_port(uint16 bg_type_id, bool accept)
{
    if (!p_) return Result::Other;

    // CRASH FIX 2026-05-13: short-circuit if the bot is already inside a BG
    // whose type matches bg_type_id. The deferred port pipeline + reminder
    // hooks can race a second intent through after a successful port; the
    // RemovePlayerAtLeave + SendToBattleground sequence below then re-enters
    // Map::RemovePlayerFromMap while the bot is mid-teleport, hitting the
    // ASSERT at Map.cpp:935 ("remove" — player not in grid).
    if (accept && p_->InBattleground())
    {
        if (Battleground const* cur = p_->GetBattleground())
        {
            if (uint32(cur->GetTypeID()) == uint32(bg_type_id))
            {
                TC_LOG_INFO("playerbot.v2",
                    "[bg_port] {} already in target BG (type={}, inst={}); skipping re-entry",
                    p_->GetName(), bg_type_id, cur->GetInstanceID());
                return Result::Ok;
            }
        }
    }
    // Also reject when the bot's not in a grid — mid-teleport or pending
    // logout. Map::RemovePlayerFromMap would fire the same ASSERT in this
    // window. The caller will retry on a later tick once the teleport
    // settles.
    if (!p_->IsInWorld() || !p_->FindMap())
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} not in world / no map; deferring", p_->GetName());
        return Result::NotReady;
    }
    if (!p_->InBattlegroundQueue())
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} not in any BG queue", p_->GetName());
        return Result::Locked;
    }

    // Find the queue slot matching bg_type_id.
    BattlegroundQueueTypeId qid = BATTLEGROUND_QUEUE_NONE;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattlegroundQueueTypeId q = p_->GetBattlegroundQueueTypeId(i);
        if (q != BATTLEGROUND_QUEUE_NONE && q.BattlemasterListId == bg_type_id)
        {
            qid = q;
            break;
        }
    }
    if (qid == BATTLEGROUND_QUEUE_NONE)
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} no queue slot for bg_type_id={}", p_->GetName(), bg_type_id);
        return Result::NotKnown;
    }
    if (!p_->IsInvitedForBattlegroundQueueType(qid))
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} not invited for bg_type_id={}", p_->GetName(), bg_type_id);
        return Result::Locked;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(qid);
    GroupQueueInfo ginfo;
    if (!bgQueue.GetPlayerGroupInfoData(p_->GetGUID(), &ginfo))
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} no GroupQueueInfo bg_type_id={}", p_->GetName(), bg_type_id);
        return Result::ServerRefused;
    }
    if (!ginfo.IsInvitedToBGInstanceGUID && accept)
    {
        TC_LOG_INFO("playerbot.v2", "[bg_port] {} ginfo not invited bg_type_id={}", p_->GetName(), bg_type_id);
        return Result::Locked;
    }
    TC_LOG_INFO("playerbot.v2", "[bg_port] {} accepting port bg_type_id={} instance={}",
                p_->GetName(), bg_type_id, ginfo.IsInvitedToBGInstanceGUID);

    BattlegroundTypeId bgTypeId = BattlegroundTypeId(qid.BattlemasterListId);
    BattlegroundTemplate const* bg_tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(bgTypeId);
    if (!bg_tmpl) return Result::NotKnown;
    PVPDifficultyEntry const* bracket = DB2Manager::GetBattlegroundBracketByLevel(
        bg_tmpl->MapIDs.front(), p_->GetLevel());

    Battleground* bg = sBattlegroundMgr->GetBattleground(ginfo.IsInvitedToBGInstanceGUID,
        bgTypeId == BATTLEGROUND_AA ? BATTLEGROUND_TYPE_NONE : bgTypeId);

    if (accept)
    {
        if (!bg) return Result::ServerRefused;
        // Mirror the handler's deserter / level / freeze gates. Bots get
        // the deserter debuff like any other player; bouncing them out keeps
        // the queue functional.
        if (p_->IsDeserter()) return Result::Locked;
        if (p_->GetLevel() > bg->GetMaxLevel()) return Result::Locked;
        if (p_->HasAura(/*Freeze*/ 9454)) return Result::Locked;
        if (!p_->IsInvitedForBattlegroundQueueType(qid)) return Result::Locked;

        if (!p_->InBattleground())
            p_->SetBattlegroundEntryPoint();
        if (!p_->IsAlive())
        {
            p_->ResurrectPlayer(1.0f);
            p_->SpawnCorpseBones();
        }
        p_->FinishTaxiFlight();
        if (Battleground* currentBg = p_->GetBattleground())
            currentBg->RemovePlayerAtLeave(p_->GetGUID(), false, true);

        p_->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID(), qid);
        p_->SetBGTeam(ginfo.Team);
        BattlegroundMgr::SendToBattleground(p_, bg);

        // Seething Shore (BG_SS = 894) post-port fix. SS uses a transport
        // (airship) for initial player placement. Bots porting via this
        // API don't always attach to the transport correctly — they land
        // in mid-air at the airship's spawn coord, fall into water and
        // drown before the snapshot can react. Manually teleport them to
        // a known-safe inland coord on solid ground (Air Supply landing
        // zones from TC's authoritative AIR_SUPPLY_DATA).
        if (bgTypeId == BATTLEGROUND_SS)
        {
            // Team-specific safe coords (mirror SeethingShoreScript home_base).
            // Horde:    Air Supply 3 area  — (1226.99, 2825.09, 39.26)
            // Alliance: Air Supply 1 area  — (1398.72, 2728.05, 28.02)
            WorldLocation safe(/*mapId*/ 1803,
                Position(1398.72f, 2728.05f, 28.02f, 0.f));
            if (ginfo.Team == HORDE)
                safe = WorldLocation(1803, Position(1226.99f, 2825.09f, 39.26f, 0.f));
            BotMovement::SafeTeleport(p_, safe, /*options*/ 0);
        }
        // Post-port snap. SendToBattleground hands the bot to the BG map
        // and the landing spot is BG-template-defined. Hand-curated team
        // spawn coords occasionally sit off-mesh (observed for ported BG
        // variants and modern reworks). PostTeleportSnap is cheap when
        // the landing is fine and rescues off-mesh landings before the
        // first idle tick wastes a path-fail.
        BotMovement::PostTeleportSnap(p_);
        return Result::Ok;
    }
    // Decline: drop from the queue.
    p_->RemoveBattlegroundQueueId(qid);
    bgQueue.RemovePlayer(p_->GetGUID(), true);
    if (bracket && !bgQueue.GetQueueId().TeamSize)
        sBattlegroundMgr->ScheduleQueueUpdate(ginfo.ArenaMatchmakerRating, qid, bracket->GetBracketId());
    return Result::Ok;
}

Result API::bg_leave()
{
    if (!p_) return Result::Other;
    if (Battleground* bg = p_->GetBattleground())
    {
        if (p_->IsInCombat() && bg->GetStatus() != STATUS_WAIT_LEAVE) return Result::Locked;
        p_->LeaveBattleground();
        // After leaving the active BG, also drop ALL other queue slots.
        // Without this the bot can still be invited into a phantom queue
        // it stacked up earlier (different bg_type_id), keeping it out
        // of the next QueueFill. Observed during DOM_AB imbalance: bots
        // were stuck queued for stale WSG/SoTA instances and never
        // re-entered the candidate pool. Match-end leave should fully
        // disengage so the bot is free for the next match.
        for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattlegroundQueueTypeId qid = p_->GetBattlegroundQueueTypeId(i);
            if (qid == BATTLEGROUND_QUEUE_NONE) continue;
            sBattlegroundMgr->GetBattlegroundQueue(qid).RemovePlayer(p_->GetGUID(), true);
            p_->RemoveBattlegroundQueueId(qid);
        }
        return Result::Ok;
    }
    // Not in BG — drop from any active queues. Walk the per-player queue
    // slot table and remove each registered queue.
    bool any = false;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattlegroundQueueTypeId qid = p_->GetBattlegroundQueueTypeId(i);
        if (qid == BATTLEGROUND_QUEUE_NONE) continue;
        sBattlegroundMgr->GetBattlegroundQueue(qid).RemovePlayer(p_->GetGUID(), true);
        p_->RemoveBattlegroundQueueId(qid);
        any = true;
    }
    return any ? Result::Ok : Result::Locked;
}

Result API::auction_sell_item(ObjectGuid auctioneer, ObjectGuid item_guid,
                              uint64 min_bid, uint64 buyout, uint32 run_time_minutes)
{
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    // Mirror the live handler's price gating: silver-aligned, > 0, ≤ cap.
    if (min_bid == 0 && buyout == 0) return Result::Locked;
    if (min_bid > MAX_MONEY_AMOUNT || buyout > MAX_MONEY_AMOUNT) return Result::Locked;
    if ((min_bid % SILVER) != 0 || (buyout % SILVER) != 0) return Result::Locked;

    // Run-time must be one of {12h, 24h, 48h} (sent by the client as
    // minutes — 720 / 1440 / 2880).
    constexpr uint32 kHourMinutes = 60;
    constexpr uint32 kRun12 = 12 * kHourMinutes;
    constexpr uint32 kRun24 = 24 * kHourMinutes;
    constexpr uint32 kRun48 = 48 * kHourMinutes;
    if (run_time_minutes != kRun12 && run_time_minutes != kRun24 && run_time_minutes != kRun48)
        return Result::Locked;

    uint32 houseId = 0;
    AuctionHouseEntry const* house_entry = AuctionHouseMgr::GetAuctionHouseEntry(npc->GetFaction(), &houseId);
    if (!house_entry) return Result::Locked;

    Item* item = p_->GetItemByGuid(item_guid);
    if (!item) return Result::NotKnown;
    if (item->GetTemplate()->GetMaxStackSize() > 1) return Result::Locked;   // commodity
    if (sAuctionMgr->GetAItem(item->GetGUID())) return Result::Locked;        // already in AH
    if (!item->CanBeTraded()) return Result::Locked;
    if (item->IsNotEmptyBag()) return Result::Locked;
    if (item->GetTemplate()->HasFlag(ITEM_FLAG_CONJURED)) return Result::Locked;
    if (*item->m_itemData->Expiration) return Result::Locked;
    if (item->GetCount() != 1) return Result::Locked;

    Seconds auctionTime = Seconds(int64(std::chrono::duration_cast<Seconds>(
                                       Minutes(run_time_minutes)).count() * double(sWorld->getRate(RATE_AUCTION_TIME))));
    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    uint64 deposit = AuctionHouseMgr::GetItemAuctionDeposit(p_, item, Minutes(run_time_minutes));
    if (!p_->HasEnoughMoney(deposit)) return Result::NotEnoughResource;

    uint32 auction_id = sObjectMgr->GenerateAuctionID();
    AuctionPosting auction;
    auction.Id           = auction_id;
    auction.Owner        = p_->GetGUID();
    // OwnerAccount: bots don't carry an account-guid value the same way
    // sessions do; the live handler reads GetAccountGUID() (the bnet acc
    // guid). For bot-owned auctions we leave it Empty — outbid/sold mail
    // resolution falls back to Owner.
    auction.OwnerAccount = ObjectGuid::Empty;
    auction.MinBid       = min_bid;
    auction.BuyoutOrUnitPrice = buyout;
    auction.Deposit      = deposit;
    auction.BidAmount    = min_bid;
    auction.StartTime    = GameTime::GetSystemTime();
    auction.EndTime      = auction.StartTime + auctionTime;
    auction.Items.push_back(item);

    if (!sAuctionMgr->PendingAuctionAdd(p_, auction_house->GetAuctionHouseId(), auction_id, auction.Deposit))
        return Result::NotEnoughResource;

    p_->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->DeleteFromInventoryDB(trans);
    item->SaveToDB(trans);
    auction_house->AddAuction(trans, std::move(auction));
    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::auction_cancel(ObjectGuid auctioneer, uint32 auction_id)
{
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    AuctionPosting* auction = auction_house->GetAuction(auction_id);
    if (!auction || auction->Owner != p_->GetGUID()) return Result::NotKnown;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (!auction->Bidder.IsEmpty())
    {
        // Live handler debits 5% of the active bid as a cancel fee, refunds
        // bid via mail. Reject up-front when we can't afford the fee — the
        // auction stays alive untouched.
        uint64 cancel_cost = CalculatePct(auction->BidAmount, 5u);
        if (!p_->HasEnoughMoney(cancel_cost)) return Result::NotEnoughResource;
        auction_house->SendAuctionCancelledToBidder(auction, trans);
        p_->ModifyMoney(-int64(cancel_cost));
    }
    auction_house->SendAuctionRemoved(auction, p_, trans);
    p_->SaveInventoryAndGoldToDB(trans);
    auction_house->RemoveAuction(trans, auction);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::auction_cancel_all(ObjectGuid auctioneer)
{
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    std::vector<uint32> owned = auction_house->GetOwnedAuctionIds(p_->GetGUID());
    if (owned.empty()) return Result::Locked;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    size_t cancelled = 0;
    for (uint32 id : owned)
    {
        AuctionPosting* auction = auction_house->GetAuction(id);
        if (!auction || auction->Owner != p_->GetGUID()) continue;
        if (!auction->Bidder.IsEmpty())
        {
            uint64 cancel_cost = CalculatePct(auction->BidAmount, 5u);
            if (!p_->HasEnoughMoney(cancel_cost)) continue;     // skip un-affordable
            auction_house->SendAuctionCancelledToBidder(auction, trans);
            p_->ModifyMoney(-int64(cancel_cost));
        }
        auction_house->SendAuctionRemoved(auction, p_, trans);
        auction_house->RemoveAuction(trans, auction);
        ++cancelled;
    }
    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return cancelled > 0 ? Result::Ok : Result::NotEnoughResource;
}

Result API::auction_buyout(ObjectGuid auctioneer, uint32 auction_id, uint64 max_price)
{
    // Buyout == bid the full BuyoutOrUnitPrice. Mirrors the buyout branch
    // of WorldSession::HandleAuctionPlaceBid; the session-side throttle,
    // achievement criteria, and result packet are intentionally omitted
    // (no client to notify). All money/auction mutation runs in one
    // synchronous character-DB transaction, matching auction_sell_item /
    // auction_cancel above.
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    AuctionPosting* auction = auction_house->GetAuction(auction_id);
    if (!auction || auction->IsCommodity()) return Result::NotKnown;

    // Cannot buy your own auction.
    if (auction->Owner == p_->GetGUID()) return Result::Locked;

    const uint64 buyout = auction->BuyoutOrUnitPrice;
    if (buyout == 0) return Result::NotKnown;   // bid-only listing — use auction_bid

    // Caller-supplied price guard: refuse if the LIVE buyout exceeds what the
    // economy rule agreed to (the snapshot-observed price). Protects against a
    // listing re-priced UP between the on-demand AH scan and execution draining
    // more gold than intended. 0 disables the guard.
    if (max_price != 0 && buyout > max_price) return Result::Locked;

    // priceToPay accounts for a prior bid by THIS bot (only the delta is
    // owed), matching the live handler.
    uint64 price_to_pay = buyout;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (!auction->Bidder.IsEmpty())
    {
        if (auction->Bidder != p_->GetGUID())
            auction_house->SendAuctionOutbid(auction, p_->GetGUID(), buyout, trans);
        else
            price_to_pay = buyout - auction->BidAmount;
    }

    if (!p_->HasEnoughMoney(price_to_pay))
        return Result::NotEnoughResource;   // trans rolled back (never committed)

    p_->ModifyMoney(-int64(price_to_pay));
    auction->Bidder    = p_->GetGUID();
    auction->BidAmount = buyout;

    std::map<uint32, AuctionPosting>::node_type removed = auction_house->RemoveAuction(trans, auction);
    auction_house->SendAuctionSold(&removed.mapped(), nullptr, trans);
    auction_house->SendAuctionWon(&removed.mapped(), p_, trans);

    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::auction_bid(ObjectGuid auctioneer, uint32 auction_id, uint64 bid)
{
    // Mirrors the bid branch of WorldSession::HandleAuctionPlaceBid. A bid
    // equal to the buyout short-circuits into the buyout path (same as the
    // live handler). Session-side throttle / criteria / packets omitted.
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    // Auction house does not deal in copper-level increments.
    if (bid % SILVER) return Result::Locked;

    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    AuctionPosting* auction = auction_house->GetAuction(auction_id);
    if (!auction || auction->IsCommodity()) return Result::NotKnown;

    // Cannot bid on your own auction.
    if (auction->Owner == p_->GetGUID()) return Result::Locked;

    const bool can_bid    = auction->MinBid != 0;
    const bool can_buyout = auction->BuyoutOrUnitPrice != 0;

    // A non-biddable (buyout-only) listing requires the exact buyout amount.
    if (!can_bid && bid != auction->BuyoutOrUnitPrice) return Result::Locked;

    const uint64 min_bid = auction->BidAmount
        ? auction->BidAmount + auction->CalculateMinIncrement()
        : auction->MinBid;
    if (can_bid && bid < min_bid) return Result::Locked;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    uint64 price_to_pay = bid;
    if (!auction->Bidder.IsEmpty())
    {
        if (auction->Bidder != p_->GetGUID())
            auction_house->SendAuctionOutbid(auction, p_->GetGUID(), bid, trans);
        else
            price_to_pay = bid - auction->BidAmount;
    }

    if (!p_->HasEnoughMoney(price_to_pay))
        return Result::NotEnoughResource;   // trans rolled back (never committed)

    p_->ModifyMoney(-int64(price_to_pay));
    auction->Bidder    = p_->GetGUID();
    auction->BidAmount = bid;

    if (can_buyout && bid == auction->BuyoutOrUnitPrice)
    {
        // Bid hit the buyout — complete the sale.
        std::map<uint32, AuctionPosting>::node_type removed = auction_house->RemoveAuction(trans, auction);
        auction_house->SendAuctionSold(&removed.mapped(), nullptr, trans);
        auction_house->SendAuctionWon(&removed.mapped(), p_, trans);
    }
    else
    {
        // Persist the new high bid + record this bot in the bidder history.
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_AUCTION_BID);
        stmt->setUInt64(0, auction->Bidder.GetCounter());
        stmt->setUInt64(1, auction->BidAmount);
        stmt->setUInt8(2, auction->ServerFlags.AsUnderlyingType());
        stmt->setUInt32(3, auction->Id);
        trans->Append(stmt);

        if (auction->BidderHistory.insert(p_->GetGUID()).second)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_AUCTION_BIDDER);
            stmt->setUInt32(0, auction->Id);
            stmt->setUInt64(1, p_->GetGUID().GetCounter());
            trans->Append(stmt);
        }

        if (Player* owner = ObjectAccessor::FindConnectedPlayer(auction->Owner))
            owner->GetSession()->SendAuctionOwnerBidNotification(auction);
    }

    p_->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::auction_buy_commodity(ObjectGuid auctioneer, uint32 item_entry,
                                  uint32 quantity, uint64 max_total_price)
{
    // Commodity buy. Modern stackable trade goods (craft reagents) are bought
    // through the bucket-aggregated commodity path, NOT the single-item
    // buyout/bid path (auction_buyout rejects commodities). Mirrors
    // WorldSession::HandleAuctionGetCommodityQuote + HandleAuctionBuyCommodity:
    // we create a quote over the cheapest non-self listings, validate the
    // total against the caller's slippage guard + the bot's gold, then commit
    // BuyCommodity in one synchronous character-DB transaction (no client
    // confirm round-trip — quote+buy are atomic for a server-driven bot).
    if (!p_) return Result::Other;
    if (quantity == 0) return Result::NotKnown;
    Creature* npc = p_->GetNPCIfCanInteractWith(auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    // The item must actually be a commodity (stackable). A single-item entry
    // routed here is a caller bug; reject so the buy path stays consistent
    // with the live handler's stack-size gating (sell side, line ~669/887).
    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(item_entry);
    if (!tmpl) return Result::NotKnown;
    if (tmpl->GetMaxStackSize() <= 1) return Result::NotKnown;   // not a commodity

    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return Result::Locked;

    // Create the quote: walks the commodity bucket's cheapest non-self
    // listings, sums unit prices for `quantity` units, and stashes the quote
    // keyed by the bot's GUID (consumed by BuyCommodity::extract below). Null
    // means: no template, no bucket, not enough quantity listed, or the bot
    // can't afford the total — collapse all to NotKnown (no buyable supply).
    CommodityQuote const* quote = auction_house->CreateCommodityQuote(p_, item_entry, quantity);
    if (!quote)
        return Result::NotKnown;

    // Slippage guard: refuse if the quote total exceeds what the caller agreed
    // to (unit_price * qty + margin from the rule). Cancel the pending quote
    // so it doesn't linger server-side (the live cancel path does the same via
    // HandleAuctionCancelCommoditiesPurchase -> CancelCommodityQuote). 0
    // disables the guard.
    if (max_total_price != 0 && quote->TotalPrice > max_total_price)
    {
        auction_house->CancelCommodityQuote(p_->GetGUID());
        return Result::Locked;
    }

    // CreateCommodityQuote already verified HasEnoughMoney at quote time, but
    // re-check defensively (gold could change between calls in principle).
    if (!p_->HasEnoughMoney(quote->TotalPrice))
    {
        auction_house->CancelCommodityQuote(p_->GetGUID());
        return Result::NotEnoughResource;
    }

    // Commit. BuyCommodity re-walks the bucket, re-validates total <= quoted
    // (allows a LOWER price if cheaper listings appeared), debits the bot,
    // mails the items to the bot, and removes the consumed listings — all
    // appended onto `trans`. delayForNextAction is the throttle delay echoed
    // back in the result packet; immaterial for a bot, pass 0ms.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (!auction_house->BuyCommodity(trans, p_, item_entry, quantity, Milliseconds(0)))
    {
        // BuyCommodity sent its own failure result packet + extracted/discarded
        // the quote. Nothing was committed; let the transaction roll back.
        return Result::Other;
    }
    CharacterDatabase.CommitTransaction(trans);
    return Result::Ok;
}

Result API::craft_fulfill_order(uint64 order_id, uint32 spell_id, uint32 item_entry,
                                uint32 qty, uint64 requester_low)
{
    // #4B-2(a): craft mechanics + delivery ONLY. The module-side caller releases
    // the escrow (CraftOrderBoard::MarkDelivered) when this returns Ok. We never
    // touch escrow gold here, so a non-Ok return leaves the order Claimed with
    // its payment still held.
    if (!p_) return Result::Other;
    if (qty == 0) qty = 1;
    if (spell_id == 0 || item_entry == 0 || requester_low == 0)
        return Result::InvalidTarget;

    // Recipe must be a known create-item spell whose product is item_entry.
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, p_->GetMap()->GetDifficultyID());
    if (!info) return Result::NotKnown;
    if (!p_->HasSpell(spell_id)) return Result::NotKnown;

    // Find the create-item effect that produces item_entry and how many it makes
    // per cast (CalcValue, clamped to >=1). This validates the order's
    // item_entry actually matches what the recipe makes (no swapping the
    // product for something cheaper).
    // Match SPELL_EFFECT_CREATE_ITEM (24) whose ItemType is the direct product.
    // (CREATE_ITEM_2 / CREATE_LOOT use a loot-template indirection rather than a
    // fixed item entry, so they can't be order-validated by entry; the board
    // only posts orders against direct create-item recipes.)
    uint32 per_cast = 0;
    for (SpellEffectInfo const& eff : info->GetEffects())
    {
        if (eff.Effect == SPELL_EFFECT_CREATE_ITEM && eff.ItemType == item_entry)
        {
            int32 v = eff.CalcValueAsInt(p_);
            per_cast = (v > 0) ? static_cast<uint32>(v) : 1u;
            break;
        }
    }
    if (per_cast == 0) return Result::NotKnown;   // recipe doesn't make item_entry

    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(item_entry);
    if (!tmpl) return Result::NotKnown;

    // Number of casts needed to reach qty (round up). Each cast consumes the
    // recipe's reagents once.
    const uint32 casts = (qty + per_cast - 1) / per_cast;

    // Pre-check ALL reagents for the full run so we never half-consume on a
    // shortfall (escrow stays held, order retried/timed out).
    for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
    {
        const int32 reagent = info->Reagent[r];
        const int32 rcount  = info->ReagentCount[r];
        if (reagent <= 0 || rcount <= 0) continue;
        const uint64 need = static_cast<uint64>(rcount) * casts;
        if (p_->GetItemCount(static_cast<uint32>(reagent), false) < need)
            return Result::NotEnoughResource;
    }

    // Resolve the recipient. Must be a real character (the module-side claim
    // path already verified it's a fleet bot; this is the core-side sanity
    // check that the guid maps to a character we can mail).
    ObjectGuid req_guid = ObjectGuid::Create<HighGuid::Player>(requester_low);
    CharacterCacheEntry const* info_rcp = sCharacterCache->GetCharacterCacheByGuid(req_guid);
    if (!info_rcp) return Result::InvalidTarget;
    if (req_guid == p_->GetGUID()) return Result::InvalidTarget;   // never self-deliver

    // Consume reagents for the whole run.
    for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
    {
        const int32 reagent = info->Reagent[r];
        const int32 rcount  = info->ReagentCount[r];
        if (reagent <= 0 || rcount <= 0) continue;
        p_->DestroyItemCount(static_cast<uint32>(reagent),
                             static_cast<uint32>(rcount) * casts, true);
    }

    // Stage the product as a standalone item owned by the requester and mail
    // it. We create it directly (not into the crafter's bags) so the delivery
    // doesn't depend on the crafter having free bag space — mirroring the
    // clone-and-save path in mail_send_item. Respect the item's max stack by
    // splitting into multiple attachments / mails as needed.
    const uint32 max_stack = std::max<uint32>(1u, tmpl->GetMaxStackSize());
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    uint32 remaining = qty;
    bool any_mailed = false;
    // Bound the number of attachments per mail (MAX_MAIL_ITEMS) — start a new
    // mail when full. For typical small craft orders this is a single mail.
    while (remaining > 0)
    {
        MailDraft draft("Craft Order",
                        "Your crafted goods, as ordered. Thank you for your business.");
        uint8 attached = 0;
        while (remaining > 0 && attached < MAX_MAIL_ITEMS)
        {
            const uint32 stack = std::min(remaining, max_stack);
            Item* product = Item::CreateItem(item_entry, stack, ItemContext::Trade_Skill, p_);
            if (!product)
            {
                // Couldn't materialize the product. Roll back the whole delivery;
                // reagents were destroyed on `trans` and are rolled back with it.
                CharacterDatabase.CommitTransaction(trans);   // releases the txn
                TC_LOG_ERROR("playerbot.api",
                    "[API::craft_fulfill_order] order={} CreateItem({}) failed", order_id, item_entry);
                return Result::Other;
            }
            product->SetOwnerGUID(req_guid);
            product->SetState(ITEM_CHANGED);
            product->SaveToDB(trans);
            draft.AddItem(product);
            remaining -= stack;
            ++attached;
        }
        if (attached == 0) break;
        draft.SendMailTo(trans,
                         MailReceiver(ObjectAccessor::FindConnectedPlayer(req_guid),
                                      req_guid.GetCounter()),
                         MailSender(p_), MAIL_CHECK_MASK_COPIED);
        any_mailed = true;
    }

    if (!any_mailed)
    {
        CharacterDatabase.CommitTransaction(trans);
        return Result::Other;
    }

    p_->SaveInventoryAndGoldToDB(trans);   // persist reagent debit alongside the mail
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_INFO("playerbot.api",
        "[API::craft_fulfill_order] order={} crafter={} crafted {}x{} (spell {}) -> requester_low={} (mailed)",
        order_id, p_->GetGUID().GetCounter(), qty, item_entry, spell_id, requester_low);
    return Result::Ok;
}

uint64 API::gold_cost_estimate_repair_all() const
{
    if (!p_) return 0;
    Player* p = const_cast<Player*>(p_);   // read-only walk; CalculateDurabilityRepairCost is non-mutating
    uint64 total = 0;
    const uint8 inventory_end = INVENTORY_SLOT_ITEM_START + p->GetInventorySlotCount();
    for (uint8 i = EQUIPMENT_SLOT_START; i < inventory_end; ++i)
        if (Item* item = p->GetItemByPos(((INVENTORY_SLOT_BAG_0 << 8) | i)))
            total += item->CalculateDurabilityRepairCost(1.0f);
    for (uint8 j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; ++j)
        for (uint8 i = 0; i < MAX_BAG_SIZE; ++i)
            if (Item* item = p->GetItemByPos(((j << 8) | i)))
                total += item->CalculateDurabilityRepairCost(1.0f);
    return total;
}

uint64 API::gold_cost_estimate_reagent_buy(uint32 reagent_entry, uint32 desired_total) const
{
    if (!p_) return 0;
    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(reagent_entry);
    if (!tmpl) return 0;
    const uint32 have = const_cast<Player*>(p_)->GetItemCount(reagent_entry, false);
    if (have >= desired_total) return 0;
    const uint32 need = desired_total - have;

    // Vendor price is per BuyCount-sized stack; bots pay BuyPrice per stack,
    // so per-unit = BuyPrice / BuyCount (round up to be conservative — the
    // affordability gate should never under-estimate).
    const uint32 buy_count = std::max<uint32>(tmpl->GetBuyCount(), 1u);
    const uint64 stack_price = tmpl->GetBuyPrice();
    if (stack_price == 0) return 0;   // not vendor-buyable (e.g. gathered-only mat)
    const uint64 per_unit = (stack_price + buy_count - 1) / buy_count;
    return per_unit * need;
}

uint64 API::gold_cost_estimate_ah_buyout(ObjectGuid auctioneer, uint32 auction_id) const
{
    if (!p_) return 0;
    Creature* npc = const_cast<Player*>(p_)->GetNPCIfCanInteractWith(
        auctioneer, UNIT_NPC_FLAG_AUCTIONEER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return 0;
    AuctionHouseObject* auction_house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    if (!auction_house) return 0;
    AuctionPosting* auction = auction_house->GetAuction(auction_id);
    if (!auction || auction->IsCommodity()) return 0;
    if (auction->Owner == p_->GetGUID()) return 0;   // can't buy own
    return auction->BuyoutOrUnitPrice;
}

Result API::discover_taxi_node(ObjectGuid flight_master)
{
    if (!p_) return Result::Other;
    Creature* unit = p_->GetNPCIfCanInteractWith(flight_master, UNIT_NPC_FLAG_FLIGHTMASTER, UNIT_NPC_FLAG_2_NONE);
    if (!unit) return Result::InvalidTarget;
    uint32 const node = sObjectMgr->GetNearestTaxiNode(
                            unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(),
                            unit->GetMapId(), p_->GetTeam());
    if (!node) return Result::NotKnown;
    // SetTaximaskNode returns false when the node was already known —
    // surface that as Ok (idempotent), false-from-an-unknown-node would
    // be Other but isn't reachable since the node was just resolved.
    p_->m_taxi.SetTaximaskNode(node);
    return Result::Ok;
}

Result API::fly_to_node(ObjectGuid flight_master, uint32 to_node)
{
    if (!p_) return Result::Other;
    Creature* unit = p_->GetNPCIfCanInteractWith(flight_master, UNIT_NPC_FLAG_FLIGHTMASTER, UNIT_NPC_FLAG_2_NONE);
    if (!unit)
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL interact_range (FM not interactable from here)",
            p_->GetName());
        return Result::InvalidTarget;
    }
    uint32 const from = sObjectMgr->GetNearestTaxiNode(
                            unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(),
                            unit->GetMapId(), p_->GetTeam());
    if (!from)
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL no_from_node", p_->GetName());
        return Result::OutOfRange;
    }

    TaxiNodesEntry const* from_e = sTaxiNodesStore.LookupEntry(from);
    TaxiNodesEntry const* to_e   = sTaxiNodesStore.LookupEntry(to_node);
    if (!from_e || !to_e)
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL bad_node from={} to={}",
            p_->GetName(), from, to_node);
        return Result::NotKnown;
    }

    // Bots aren't taxi-cheaters; the live handler skips this check for GMs
    // but for normal play both endpoints must already be on the bot's mask.
    if (!p_->isTaxiCheater() &&
        (!p_->m_taxi.IsTaximaskNodeKnown(from) || !p_->m_taxi.IsTaximaskNodeKnown(to_node)))
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL node_not_known from={}({}) to={}({})",
            p_->GetName(), from, p_->m_taxi.IsTaximaskNodeKnown(from) ? 1 : 0,
            to_node, p_->m_taxi.IsTaximaskNodeKnown(to_node) ? 1 : 0);
        return Result::Locked;
    }

    std::vector<uint32> nodes;
    TaxiPathGraph::GetCompleteNodeRoute(from_e, to_e, p_, nodes);
    if (nodes.empty())
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL empty_route from={} to={}",
            p_->GetName(), from, to_node);
        return Result::Locked;
    }
    // Bots are perpetually broke (a fresh L3 has ~67 copper), and
    // ActivateTaxiPathTo charges the taxi fare — so flights silently failed and
    // the bot ran back and forth at the FM forever. Cover the fare: top the bot
    // up to a flight reserve so it can always afford the hop (bots are AI; a free
    // ride is fine, and this is far cheaper than the leveling time lost to a bot
    // stranded on the wrong continent). 5g covers any single multi-hop route.
    constexpr uint64 kFlightReserve = 50000;   // 5 gold in copper
    if (p_->GetMoney() < kFlightReserve)
        p_->ModifyMoney(int64(kFlightReserve - p_->GetMoney()));
    if (!p_->ActivateTaxiPathTo(nodes, unit, /*spellid*/ 0, /*preferredMountDisplay*/ 0))
    {
        TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} FAIL activate_taxi (cost/state) money={} hops={}",
            p_->GetName(), p_->GetMoney(), uint32(nodes.size()));
        return Result::NotEnoughResource;
    }
    TC_LOG_INFO("playerbot.v2", "[fly_dbg] {} OK takeoff hops={}", p_->GetName(), uint32(nodes.size()));
    return Result::Ok;
}

// Mirrors WorldSession::CanUseBank without the session-only "command source"
// path. Bots always interact via a real banker NPC; we just verify range
// + the BANKER flag (account bank also accepted — newer clients allow both).
static Creature* ResolveBanker(Player* p, ObjectGuid banker)
{
    if (!p) return nullptr;
    return p->GetNPCIfCanInteractWith(banker,
                                      UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_ACCOUNT_BANKER,
                                      UNIT_NPC_FLAG_2_NONE);
}

Result API::bank_deposit_item(ObjectGuid banker, uint8 from_bag, uint8 from_slot)
{
    if (!p_) return Result::Other;
    if (!ResolveBanker(p_, banker)) return Result::InvalidTarget;
    Item* item = p_->GetItemByPos(from_bag, from_slot);
    if (!item) return Result::NotKnown;
    // Refuse no-op: source already in a bank slot. Surfaces as Locked so a
    // looped deposit-everything caller stops re-emitting on the same row.
    if (Player::IsBankPos(from_bag, from_slot)) return Result::Locked;

    ItemPosCountVec dest;
    InventoryResult msg = p_->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
    if (msg != EQUIP_ERR_OK) return Result::InventoryFull;
    // CanBankItem can resolve to "stays where it is" (dest == src). The
    // live handler treats that as CANT_SWAP — surface Locked so the caller
    // knows further attempts will fail too.
    if (dest.size() == 1 && dest[0].pos == item->GetPos()) return Result::Locked;

    p_->RemoveItem(from_bag, from_slot, true);
    p_->ItemRemovedQuestCheck(item->GetEntry(), item->GetCount());
    p_->BankItem(dest, item, true);
    return Result::Ok;
}

Result API::bank_withdraw_item(ObjectGuid banker, uint8 from_bag, uint8 from_slot)
{
    if (!p_) return Result::Other;
    if (!ResolveBanker(p_, banker)) return Result::InvalidTarget;
    if (!Player::IsBankPos(from_bag, from_slot)) return Result::InvalidTarget;
    Item* item = p_->GetItemByPos(from_bag, from_slot);
    if (!item) return Result::NotKnown;

    ItemPosCountVec dest;
    InventoryResult msg = p_->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
    if (msg != EQUIP_ERR_OK) return Result::InventoryFull;

    p_->RemoveItem(from_bag, from_slot, true);
    if (Item const* stored = p_->StoreItem(dest, item, true))
        p_->ItemAddedQuestCheck(stored->GetEntry(), stored->GetCount());
    return Result::Ok;
}

// ---- Guild bank --------------------------------------------------------
//
// All four operations require:
//   1. The bot is in a guild (Player::GetGuild()).
//   2. The banker GameObject (a Guild Vault) is in interact range.
// They then dispatch to Guild::HandleMember*Money / SwapItemsWithInventory
// which performs the rank/permission/withdraw-cap chain identical to the
// gossip path. We map the guild's silent failure into a Result envelope
// by sampling the bot's money / guild-bank money before-and-after.
namespace
{
    GameObject* ResolveGuildVault(Player* p, ObjectGuid banker)
    {
        if (!p) return nullptr;
        return p->GetGameObjectIfCanInteractWith(banker, GAMEOBJECT_TYPE_GUILD_BANK);
    }
}

Result API::guild_bank_deposit_money(ObjectGuid banker, uint64 amount)
{
    if (!p_) return Result::Other;
    if (amount == 0) return Result::Ok;
    if (!ResolveGuildVault(p_, banker)) return Result::InvalidTarget;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::InvalidTarget;
    if (!p_->HasEnoughMoney(amount)) return Result::NotEnoughResource;

    // HandleMemberDepositMoney is void — it logs and broadcasts but doesn't
    // signal failure. Sample bot gold before/after to detect rank-permission
    // refusal (rank 0 always permitted; lower ranks may be blocked). The
    // Guild API will not deduct money on failure, so a no-change after the
    // call means the deposit was rejected.
    uint64 before = p_->GetMoney();
    guild->HandleMemberDepositMoney(p_->GetSession(), amount);
    if (p_->GetMoney() == before) return Result::Locked;
    return Result::Ok;
}

Result API::guild_bank_withdraw_money(ObjectGuid banker, uint64 amount)
{
    if (!p_) return Result::Other;
    if (amount == 0) return Result::Ok;
    if (!ResolveGuildVault(p_, banker)) return Result::InvalidTarget;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::InvalidTarget;

    // HandleMemberWithdrawMoney returns true on success, false on insufficient
    // bank money / rank cap exceeded / withdraw-disabled. Map false to Locked
    // so the caller can distinguish a permission/cap miss from a transport
    // error (which would surface earlier as InvalidTarget).
    if (!guild->HandleMemberWithdrawMoney(p_->GetSession(), amount, /*repair=*/false))
        return Result::Locked;
    return Result::Ok;
}

Result API::guild_bank_deposit_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                    uint8 player_bag, uint8 player_slot, uint32 count)
{
    if (!p_) return Result::Other;
    if (!ResolveGuildVault(p_, banker)) return Result::InvalidTarget;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::InvalidTarget;
    if (!Player::IsInventoryPos(player_bag, player_slot)) return Result::InvalidTarget;
    Item* item = p_->GetItemByPos(player_bag, player_slot);
    if (!item) return Result::NotKnown;

    // SwapItemsWithInventory(toChar=false) deposits inventory→bank. The
    // function logs internally and is silent on failure. Compare item-
    // presence at the source slot before/after to confirm the swap landed.
    // For mergeable stacks the source might still hold a partial — accept
    // any change as success; only an unchanged source is failure.
    uint32 before_count = item->GetCount();
    guild->SwapItemsWithInventory(p_, /*toChar=*/false, tab, bank_slot, player_bag, player_slot, count);
    Item* after = p_->GetItemByPos(player_bag, player_slot);
    if (after && after->GetCount() == before_count)
        return Result::Locked;
    return Result::Ok;
}

// ---- Quest sharing -------------------------------------------------
//
// Mirrors WorldSession::HandlePushQuestToParty end-to-end. The bot must
// own the quest (CanShareQuest checks status + class/race + shareable
// flag) and be in a group. Each receiver is gated by the same Satisfy
// chain the live handler uses; ineligible receivers get the appropriate
// QuestPushReason response. Eligible receivers' SendQuestGiverQuestDetails
// pops their accept/decline UI; their HandleQuestgiverAccept/Decline
// routes back through SetQuestSharingInfo so the live cancellation chain
// (sender died / quest abandoned) still works.
Result API::share_quest_with_party(uint32 quest_id)
{
    if (!p_) return Result::Other;
    Quest const* quest = sObjectMgr->GetQuestTemplate(quest_id);
    if (!quest) return Result::NotKnown;
    if (!p_->CanShareQuest(quest_id)) return Result::ServerRefused;
    Group* group = p_->GetGroup();
    if (!group) return Result::OutOfRange;

    for (GroupReference const& itr : group->GetMembers())
    {
        Player* receiver = itr.GetSource();
        if (!receiver || receiver == p_) continue;
        // Already mid-share or dead — skip silently. The live handler does
        // SendPushToPartyResponse for diagnostics but our caller doesn't
        // need that level of detail.
        if (!receiver->GetPlayerSharingQuest().IsEmpty()) continue;
        if (!receiver->IsAlive()) continue;
        // Status gates — already on / done.
        QuestStatus status = receiver->GetQuestStatus(quest_id);
        if (status == QUEST_STATUS_REWARDED ||
            status == QUEST_STATUS_INCOMPLETE ||
            status == QUEST_STATUS_COMPLETE) continue;
        // Eligibility gates — match HandlePushQuestToParty's Satisfy ladder.
        if (!receiver->SatisfyQuestLog(false))         continue;
        if (!receiver->SatisfyQuestDay(quest, false))  continue;
        if (!receiver->SatisfyQuestMinLevel(quest, false)) continue;
        if (!receiver->SatisfyQuestMaxLevel(quest, false)) continue;
        if (!receiver->SatisfyQuestClass(quest, false))    continue;
        if (!receiver->SatisfyQuestRace(quest, false))     continue;
        if (!receiver->SatisfyQuestMinReputation(quest, false)) continue;
        if (!receiver->SatisfyQuestMaxReputation(quest, false)) continue;
        // Pop the quest-share dialog on the receiver. SetQuestSharingInfo
        // pins the sender so a subsequent receiver-side accept knows where
        // to credit the share, and so the receiver-cancel cleanup path
        // works if the sender disconnects mid-decision.
        receiver->SetQuestSharingInfo(p_->GetGUID(), quest_id);
        receiver->PlayerTalkClass->SendQuestGiverQuestDetails(quest, p_->GetGUID(), true, false);
    }
    return Result::Ok;
}

Result API::guild_bank_withdraw_item(ObjectGuid banker, uint8 tab, uint8 bank_slot,
                                     uint8 player_bag, uint8 player_slot, uint32 count)
{
    if (!p_) return Result::Other;
    if (!ResolveGuildVault(p_, banker)) return Result::InvalidTarget;
    Guild* guild = p_->GetGuild();
    if (!guild) return Result::InvalidTarget;
    if (!Player::IsInventoryPos(player_bag, player_slot) &&
        player_slot != NULL_SLOT) return Result::InvalidTarget;

    // SwapItemsWithInventory(toChar=true) withdraws bank→inventory. Compare
    // the destination slot before/after; the destination should now hold an
    // item it didn't before (or a strictly-larger stack).
    Item* dst_before = (player_slot == NULL_SLOT) ? nullptr : p_->GetItemByPos(player_bag, player_slot);
    uint32 before_count = dst_before ? dst_before->GetCount() : 0;
    guild->SwapItemsWithInventory(p_, /*toChar=*/true, tab, bank_slot, player_bag, player_slot, count);
    if (player_slot != NULL_SLOT)
    {
        Item* dst_after = p_->GetItemByPos(player_bag, player_slot);
        if (!dst_after || (dst_before && dst_after->GetCount() == before_count))
            return Result::Locked;
    }
    return Result::Ok;
}

Result API::trainer_buy_all_available(ObjectGuid trainer_npc,
                                      uint32* out_learned,
                                      uint32* out_already,
                                      uint32* out_skipped)
{
    if (out_learned) *out_learned = 0;
    if (out_already) *out_already = 0;
    if (out_skipped) *out_skipped = 0;
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(trainer_npc, UNIT_NPC_FLAG_TRAINER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;
    uint32 const trainer_id = sObjectMgr->GetCreatureDefaultTrainer(npc->GetEntry());
    if (!trainer_id) return Result::NotKnown;
    Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(trainer_id);
    if (!trainer) return Result::NotKnown;

    // TeachSpell internally re-checks CanTeachSpell — we don't need to
    // pre-filter. Bots may carry insufficient gold partway through the
    // batch; that surfaces as a NotEnoughMoney teach-failure for that
    // spell only and the loop continues with the next.
    auto const& spells = trainer->GetSpells();
    bool any_learned = false;
    for (auto const& s : spells)
    {
        if (p_->HasSpell(s.SpellId))
        {
            if (out_already) ++*out_already;
            continue;
        }
        trainer->TeachSpell(npc, p_, s.SpellId);
        if (p_->HasSpell(s.SpellId))
        {
            any_learned = true;
            if (out_learned) ++*out_learned;
        }
        else if (out_skipped) ++*out_skipped;
    }
    // "Nothing learned + nothing already known + nothing offered" is
    // unusual — surface as Ok (trainer simply has no spells for the bot).
    if (!any_learned && spells.empty()) return Result::Ok;
    if (!any_learned && out_already && *out_already == 0) return Result::ServerRefused;
    return Result::Ok;
}

Result API::trainer_buy_spell(ObjectGuid trainer_npc, uint32 spell_id)
{
    if (!p_) return Result::Other;
    Creature* npc = p_->GetNPCIfCanInteractWith(trainer_npc, UNIT_NPC_FLAG_TRAINER, UNIT_NPC_FLAG_2_NONE);
    if (!npc) return Result::InvalidTarget;

    // Idempotent — caller may re-issue across snapshot ticks if the
    // first buy raced with a snapshot rebuild. Already-known is success.
    if (p_->HasSpell(spell_id)) return Result::Ok;

    // Resolve trainer template id from the creature entry. NPCs without
    // a registered trainer (rare — usually means data isn't loaded) get
    // NotKnown so callers can fall back to another trainer.
    uint32 const trainer_id = sObjectMgr->GetCreatureDefaultTrainer(npc->GetEntry());
    if (!trainer_id) return Result::NotKnown;
    Trainer::Trainer const* trainer = sObjectMgr->GetTrainer(trainer_id);
    if (!trainer) return Result::NotKnown;

    // TeachSpell handles the full chain: spell-on-this-trainer check,
    // CanTeachSpell (level/skill/prereq/profession-slot), gold cost
    // (with reputation discount), and either CastSpell or LearnSpell
    // depending on the trainer entry's IsCastable flag. It only sends
    // a packet on failure; we re-check HasSpell to surface success.
    trainer->TeachSpell(npc, p_, spell_id);
    return p_->HasSpell(spell_id) ? Result::Ok : Result::ServerRefused;
}

Result API::abandon_quest(uint32 quest_id)
{
    if (!p_) return Result::Other;
    QuestStatus status = p_->GetQuestStatus(quest_id);
    if (status == QUEST_STATUS_NONE) return Result::NotKnown;
    // RemoveActiveQuest handles all cleanup (objective tracking, currency,
    // PvP state if applicable). Mirrors HandleQuestlogRemoveQuestOpcode.
    p_->RemoveActiveQuest(quest_id);
    return Result::Ok;
}

} // namespace Playerbot
