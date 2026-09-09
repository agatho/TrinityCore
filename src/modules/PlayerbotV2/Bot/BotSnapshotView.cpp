#include "BotSnapshotView.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellDefines.h"
#include "World/WorldMetadata.h"
#include <algorithm>
#include <cfloat>

namespace Playerbot {

namespace {

// Legacy linear scan, kept as a fallback for callers that don't have a
// CooldownsState in scope. Hot-path callers (is_ready/cd_remaining/
// charges in this file) take the FindCooldownIndexed path below — O(1)
// lookup via the spell_cooldowns_index map.
CooldownEntry const* FindCooldown(std::vector<CooldownEntry> const& list, uint32 spell_id)
{
    for (auto const& e : list)
        if (e.spell_id == spell_id)
            return &e;
    return nullptr;
}

CooldownEntry const* FindCooldownIndexed(CooldownsState const& cds, uint32 spell_id)
{
    // Tier 3.3: spell_cooldowns_index is a sorted flat vector; find() returns
    // a pointer to the index value (or nullptr when absent).
    uint32 const* pi = cds.spell_cooldowns_index.find(spell_id);
    if (!pi) return nullptr;
    const uint32 i = *pi;
    if (i >= cds.spell_cooldowns.size()) return nullptr;  // defensive
    return &cds.spell_cooldowns[i];
}

AuraEntry const* FindAura(std::vector<AuraEntry> const& list, uint32 spell_id)
{
    for (auto const& a : list)
        if (a.spell_id == spell_id)
            return &a;
    return nullptr;
}

// As FindAura but only returns the entry if the bot is the caster. Damage-DoT
// refresh checks need this: in a raid with multiple priests, the bot's own
// SWP may have fallen off while another priest's SWP is up. The plain
// FindAura would short-circuit and skip the recast, costing the bot's own
// damage. Filtering by caster makes "is my X up?" queries correct per-bot.
AuraEntry const* FindMyAura(std::vector<AuraEntry> const& list, uint32 spell_id, ObjectGuid me)
{
    for (auto const& a : list)
        if (a.spell_id == spell_id && a.caster == me)
            return &a;
    return nullptr;
}

} // anonymous

bool BotSnapshotView::has_reagents(uint32 spell_id) const
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si) return false;
    // Walk the per-spell reagent table. Reagent==0 marks an unused slot;
    // ReagentCount of 0 also means no requirement. Both cases skip silently.
    // For each reagent that IS required, sum bag stacks of that entry and
    // bail out if any is short.
    for (size_t i = 0; i < si->Reagent.size(); ++i)
    {
        const int32  entry = si->Reagent[i];
        const int16  need  = si->ReagentCount[i];
        if (entry <= 0 || need <= 0) continue;
        if (item_count(uint32(entry)) < uint32(need))
            return false;
    }
    return true;
}

bool BotSnapshotView::is_ready(uint32 spell_id) const
{
    // Don't even try to cast spells the bot hasn't learned. Distribution-
    // leveled bots (especially DK/DH/Evoker, who skip the class starter
    // quests that grant abilities like Death Coil 47541 / Death Grip 49576)
    // don't have the spells the combat APL assumes. Without this gate, every
    // tick produces SpellCastResult=91 (SPELL_FAILED_NOT_KNOWN) log spam.
    // Hunter Call Pet (883) when the bot has no pet is the same pattern.
    // Cheap (binary_search on the sorted known_spells vector) — runs many
    // times per tick across all spec APLs but never measurable.
    if (!knows_spell(spell_id)) return false;
    // Block while we have an active cast/channel — re-emitting CastSpellIntent
    // mid-cast cancels the in-progress one and restarts, costing latency and
    // breaking healers/casters that should let their spell finish. Treats
    // re-firing the same spell as also blocked: server-side replay produces
    // wasted resource use even if it "works".
    if (s_->cast.is_casting && s_->cast.current_cast_remaining.count() > 0) return false;
    // Stuns block all casts. Silence only blocks magic-school spells —
    // physical-ability classes (Warrior/Rogue/Hunter/DK/DH/Monk) cast through
    // it. Without per-class branching, gating on is_silenced would block
    // Mortal Strike / Backstab / Death Strike etc. when silenced. Let the
    // server-side Spell::CheckCast reject magic-school casts the proper way
    // (mapped to ServerRefused). Stuns stay gated since nothing escapes them.
    if (s_->vitals.is_stunned) return false;
    if (gcd_active()) return false;
    auto cd = FindCooldownIndexed(s_->cooldowns, spell_id);
    if (!cd) return true;
    if (cd->charges > 0) return true;
    // A charge-based spell with 0 charges is NOT ready, full stop — never
    // fall through to the remaining-time check. The builder also populates
    // `remaining` with the recharge timer for depleted charge spells now,
    // but this guard makes the contract explicit (audit B01: 0-charge
    // spells reported ready and generated 53% of all NOT_READY cast-reject
    // spam, ~8 rejects per recharge cycle per bot, fleet-wide).
    if (cd->max_charges > 0) return false;
    return cd->remaining.count() == 0;
}

Ms BotSnapshotView::cd_remaining(uint32 spell_id) const
{
    auto cd = FindCooldownIndexed(s_->cooldowns, spell_id);
    return cd ? cd->remaining : Ms{0};
}

uint8 BotSnapshotView::charges(uint32 spell_id) const
{
    // Charge-bearing spells live in the unified spell_cooldowns vector
    // with charges/max_charges populated on the CooldownEntry by
    // BotSnapshotBuilder::CopyCooldowns. (REFACTOR_2 removed the
    // legacy item_cooldowns / charge_cooldowns side-tables since both
    // were declared-but-never-populated, returning 0 for every lookup.)
    auto cd = FindCooldownIndexed(s_->cooldowns, spell_id);
    if (!cd) return uint8(0);
    return cd->charges;
}

bool BotSnapshotView::can_cast_while_moving(uint32 spell_id) const
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si) return true;   // unknown — let the cast attempt decide
    // Spells without the Movement interrupt flag (instants, channels with
    // movement allowed) pass through immediately.
    if ((si->InterruptFlags & SpellInterruptFlags::Movement) == SpellInterruptFlags::None)
        return true;
    // Aura-based "next cast can move" / "all hard-casts can move while up"
    // talents. These don't strip the Movement flag from SpellInfo; instead
    // they apply a self-aura that lets the cast through. The exhaustive set
    // we model in 12.0:
    //   - Ice Floes (108839) — Mage talent: 3 charges, each consumed by a
    //     hard-cast spell, removing the Movement interrupt for that one cast
    //   - Spiritwalker's Grace (79206) — Resto Shaman: all hard-casts free
    //     to move during the 15s window
    //   - Hover (358267) — Evoker: 6s window where everything can move-cast
    //   - Free Movement (366155, Bulwark of Order placeholder — not exhaustive)
    constexpr uint32 ICE_FLOES_AURA          = 108839;
    constexpr uint32 SPIRITWALKERS_GRACE_AURA = 79206;
    constexpr uint32 HOVER_AURA              = 358267;
    if (FindAura(s_->auras.own_auras, ICE_FLOES_AURA))           return true;
    if (FindAura(s_->auras.own_auras, SPIRITWALKERS_GRACE_AURA)) return true;
    if (FindAura(s_->auras.own_auras, HOVER_AURA))               return true;
    return false;
}

bool BotSnapshotView::has_aura(uint32 spell_id, ObjectGuid on) const
{
    return find_aura(spell_id, on) != nullptr;
}

AuraEntry const* BotSnapshotView::find_aura(uint32 spell_id, ObjectGuid on) const
{
    if (on == ObjectGuid::Empty || on == s_->guid)
    {
        // O(1) hit via own_auras_index. Falls back to nullptr (not the
        // linear scan) — the index is built every Build, so a missing
        // entry means the aura really isn't on the bot.
        uint32 const* pi = s_->auras.own_auras_index.find(spell_id);
        if (!pi) return nullptr;
        const uint32 i = *pi;
        if (i >= s_->auras.own_auras.size()) return nullptr;
        return &s_->auras.own_auras[i];
    }
    // For damage debuff refresh on enemies, filter by caster. See FindMyAura
    // comment: a teammate's SWP being up doesn't mean OUR SWP is up.
    if (on == s_->combat.current_target)
        return FindMyAura(s_->auras.target_auras, spell_id, s_->guid);
    if (on == s_->combat.victim)
        return FindMyAura(s_->auras.victim_auras, spell_id, s_->guid);
    // Group members / pet: O(1) composite-key lookup. Key shape matches
    // BotSnapshotBuilder where the index is populated.
    {
        const uint64 key = (uint64(on.GetCounter()) << 32) | uint64(spell_id);
        uint32 const* pi = s_->auras.my_auras_on_others_index.find(key);
        if (pi)
        {
            const uint32 i = *pi;
            if (i < s_->auras.my_auras_on_others.size())
            {
                auto const& o = s_->auras.my_auras_on_others[i];
                // Materialise the matched outbound row into per-view storage.
                // Previously this used a single thread_local stash that the
                // NEXT find_aura call overwrote — a caller holding two results
                // (or calling again before using the first) got aliased data.
                // outbound_aura_rows_ is a std::deque, so this push_back never
                // invalidates pointers handed out by earlier calls; every
                // returned pointer stays valid for this view's lifetime.
                AuraEntry& row = outbound_aura_rows_.emplace_back();
                row.spell_id  = o.spell_id;
                row.stacks    = o.stacks;
                row.remaining = o.remaining;
                row.caster    = s_->guid;
                return &row;
            }
        }
    }
    return nullptr;
}

uint8 BotSnapshotView::aura_stacks(uint32 spell_id, ObjectGuid on) const
{
    if (AuraEntry const* a = find_aura(spell_id, on))
        return a->stacks;
    return 0;
}

NearbyUnit const* BotSnapshotView::enemy_without_my_aura(uint32 spell_id, float range) const
{
    const float r2 = range * range;
    const ObjectGuid me = s_->guid;
    for (auto const& e : s_->combat.nearby_enemies)
    {
        if (e.hp <= 0) continue;
        const float dx = e.x - s_->position.x;
        const float dy = e.y - s_->position.y;
        const float dz = e.z - s_->position.z;
        if (dx*dx + dy*dy + dz*dz > r2) continue;
        // Skip the current victim — multi-DoT logic is "OFF-target dot
        // expansion"; the rotation handles the primary target separately.
        if (e.guid == s_->combat.victim) continue;
        // O(1) outbound check via my_auras_on_others_index. Pre-fix this
        // was the hottest O(N²) loop in the view: 16 enemies × ~20
        // outbound auras = 320 comparisons per call, called 2–3× per
        // tick by every DoT-spec rotation. Now: one map lookup per
        // enemy.
        const uint64 key = (uint64(e.guid.GetCounter()) << 32) | uint64(spell_id);
        if (s_->auras.my_auras_on_others_index.find(key) != nullptr)
            continue;
        return &e;
    }
    return nullptr;
}

bool BotSnapshotView::target_dispellable(DispelType type) const
{
    for (auto const& a : s_->auras.target_auras)
        if (a.is_harmful && a.dispel_type == type)
            return true;
    return false;
}

bool BotSnapshotView::self_dispellable(DispelType type) const
{
    for (auto const& a : s_->auras.own_auras)
        if (a.is_harmful && a.dispel_type == type)
            return true;
    return false;
}

bool BotSnapshotView::has_mechanic(uint32 mechanic) const
{
    if (mechanic == 0) return false;
    for (auto const& a : s_->auras.own_auras)
        if (a.is_harmful && a.mechanic == mechanic)
            return true;
    return false;
}

NearbyUnit const* BotSnapshotView::target_info() const
{
    if (s_->combat.current_target == ObjectGuid::Empty) return nullptr;
    for (auto const& u : s_->combat.nearby_enemies)
        if (u.guid == s_->combat.current_target) return &u;
    for (auto const& u : s_->combat.nearby_friends)
        if (u.guid == s_->combat.current_target) return &u;
    return nullptr;
}

NearbyUnit const* BotSnapshotView::victim_info() const
{
    if (s_->combat.victim == ObjectGuid::Empty) return nullptr;
    // Attackers list is the freshest source — the bot's victim is virtually
    // always also targeting the bot, so attackers is checked first to avoid
    // an O(N) walk over nearby_enemies.
    for (auto const& u : s_->combat.attackers)
        if (u.guid == s_->combat.victim) return &u;
    for (auto const& u : s_->combat.nearby_enemies)
        if (u.guid == s_->combat.victim) return &u;
    return nullptr;
}

NearbyUnit const* BotSnapshotView::lowest_hp_friend() const
{
    NearbyUnit const* best = nullptr;
    int32 best_pct = 101;
    for (auto const& u : s_->combat.nearby_friends)
    {
        if (u.max_hp <= 0) continue;
        if (u.hp <= 0) continue;          // dead — heal would InvalidTarget
        const int32 pct = (u.hp * 100) / u.max_hp;
        if (pct < best_pct) { best_pct = pct; best = &u; }
    }
    return best;
}

NearbyUnit const* BotSnapshotView::highest_threat_attacker() const
{
    // Snapshot stores attackers ordered by threat-to-me by the builder.
    // Return the highest-threat FIGHTABLE attacker: skip untargetable trigger
    // units (UNINTERACTIBLE stalkers) and pacified/dead entries so peel/taunt/
    // assist consumers never get handed a unit they can't attack (the harbor
    // 49521 flood would otherwise sit at attackers.front() and stall every
    // threat consumer with Victim=0).
    for (auto const& a : s_->combat.attackers)
    {
        if (a.hp <= 0) continue;
        if (a.untargetable || a.is_pacified) continue;
        return &a;
    }
    return nullptr;
}

NearbyUnit const* BotSnapshotView::interruptible_caster() const
{
    // PvP-aware front: in a BG/arena, ALWAYS check enemy healers
    // first via the nearby_enemies scan. Healers typically don't
    // appear in `attackers` (they cast heals on their team, not on
    // us), so the legacy attackers-only walk missed them entirely.
    // Audit 2026-05-22 confirmed all spec APLs called
    // interruptible_caster() directly without going through
    // kick_target(in_pvp); routing here means every spec gets the
    // healer-first behavior for free in BG/arena.
    if (s_->bg.in_battleground)
        if (auto const* h = enemy_healer_to_interrupt(30.0f))
            return h;
    // Skip casts about to finish — by the time the kick travels and lands, the
    // cast would already have completed. 350ms is roughly one server tick + a
    // generous net buffer, balanced against not pre-emptively skipping casts
    // we could still catch on later targets.
    constexpr int64_t kMinRemainingMs = 350;
    // Human-pacing: also skip casts that have barely started. A human sees
    // the cast bar appear, reads the spell name, then kicks at 30–80% of
    // the bar — not at 50ms. We don't have total-cast-duration in the
    // snapshot, but we have remaining-ms; an upper cap on remaining-ms
    // approximates "wait for the cast to progress". Per-bot jitter
    // (1200–2000ms upper cap) means in a group of bots, the interrupts
    // stagger across the cast rather than all firing on observation.
    // For short casts (≤1200ms total) the cap never blocks (cast_remaining
    // is already below it on first observation), so quick casts stay
    // interruptible without a hesitation hit.
    const uint32 cap_jitter = 1200u + (uint32(s_->guid.GetCounter()) * 2654435761u) % 800u;
    int64_t kMaxRemainingMs = int64_t(cap_jitter);
    // PvE-coordinator interrupt rotation (dungeon/raid groups with an
    // active plan): the guid-jitter only DECORRELATES bots — several can
    // still land in overlapping windows and double-kick one cast while
    // the next goes free. With an assigned rank, the window is exact:
    // rank 0 keeps the human-pacing jitter cap (kick on sight), rank 1
    // covers the cast's last 800ms (fires only if rank 0's kick didn't
    // land — a landed kick clears is_casting first), ranks 2+ the last
    // 550ms, off-rotation (0xFF, healer kicks) the last 450ms. Windows
    // sit above the 350ms landing floor so every layer stays REACHABLE
    // — a backstop window below the floor would silently never fire.
    // This is the chokepoint every spec APL routes through, so all 39
    // rotations inherit the schedule without per-spec edits.
    if (s_->pve_order.active && !s_->bg.in_battleground)
    {
        const uint8 irank = s_->pve_order.interrupt_rank;
        if      (irank == 1)    kMaxRemainingMs = 800;
        else if (irank == 0xFF) kMaxRemainingMs = 450;
        else if (irank >= 2)    kMaxRemainingMs = 550;
        // rank 0 keeps the jitter cap.
    }
    for (auto const& u : s_->combat.attackers)
        if (u.is_casting && u.is_interruptible
            && u.cast_remaining.count() >= kMinRemainingMs
            && u.cast_remaining.count() <= kMaxRemainingMs)
            return &u;
    return nullptr;
}

NearbyUnit const* BotSnapshotView::enemy_healer_to_interrupt(float range) const
{
    // Same 350ms tail-skip rationale as interruptible_caster().
    constexpr int64_t kMinRemainingMs = 350;
    // Same human-pacing upper cap as interruptible_caster — wait for the
    // cast to actually progress before kicking. Healer interrupts in
    // particular were the loudest robot tell: a 2.5s Flash Heal kicked
    // at 50ms is unmistakable. Per-bot jitter here is independent from
    // the trash-interrupt slot so two specs in the same bot don't sync.
    const uint32 cap_jitter = 1300u + (uint32(s_->guid.GetCounter() ^ 0xA5A5u) * 2246822519u) % 900u;
    const int64_t kMaxRemainingMs = int64_t(cap_jitter);
    const float r2 = range * range;
    NearbyUnit const* best = nullptr;
    float best_dsq = r2;
    for (auto const& u : s_->combat.nearby_enemies)
    {
        if (u.role != Role::Healer) continue;
        if (!u.is_casting || !u.is_interruptible) continue;
        if (u.cast_remaining.count() < kMinRemainingMs) continue;
        if (u.cast_remaining.count() > kMaxRemainingMs) continue;
        if (u.hp <= 0) continue;
        const float dx = u.x - s_->position.x;
        const float dy = u.y - s_->position.y;
        const float dz = u.z - s_->position.z;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq < best_dsq) { best_dsq = dsq; best = &u; }
    }
    return best;
}

NearbyUnit const* BotSnapshotView::kick_target(bool in_pvp, float range) const
{
    if (in_pvp)
        if (auto const* h = enemy_healer_to_interrupt(range))
            return h;
    return interruptible_caster();
}

NearbyUnit const* BotSnapshotView::enemy_near_friendly_carrier(float range) const
{
    ObjectGuid const& fc_guid = s_->bg.friendly_flag_carrier;
    if (fc_guid.IsEmpty()) return nullptr;

    // Locate carrier's position from nearby_friends. If it's not in our
    // snapshot the carrier is out of awareness range; nothing to peel for.
    float cx = 0.f, cy = 0.f, cz = 0.f;
    bool found = false;
    for (auto const& f : s_->combat.nearby_friends)
    {
        if (f.guid == fc_guid)
        {
            cx = f.x; cy = f.y; cz = f.z;
            found = true;
            break;
        }
    }
    if (!found) return nullptr;

    const float r2 = range * range;
    NearbyUnit const* best = nullptr;
    float best_dsq = r2;
    for (auto const& u : s_->combat.nearby_enemies)
    {
        if (u.hp <= 0) continue;
        const float dx = u.x - cx;
        const float dy = u.y - cy;
        const float dz = u.z - cz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq < best_dsq) { best_dsq = dsq; best = &u; }
    }
    return best;
}

size_t BotSnapshotView::enemies_within(float range) const
{
    const float r2 = range * range;
    size_t n = 0;
    for (auto const& u : s_->combat.nearby_enemies)
    {
        // Never count un-fightable trigger units (UNINTERACTIBLE stalkers /
        // pacified) as "nearby enemies" — they exert no pressure and would
        // mis-inflate AoE/defensive/kite density gates (harbor 49521 flood).
        if (u.untargetable || u.is_pacified) continue;
        const float dx = u.x - s_->position.x;
        const float dy = u.y - s_->position.y;
        const float dz = u.z - s_->position.z;
        if (dx*dx + dy*dy + dz*dz <= r2) ++n;
    }
    return n;
}

NearbyUnit const* BotSnapshotView::path_threat(float tx, float ty,
                                               float max_forward,
                                               float half_width,
                                               ObjectGuid exclude_guid) const
{
    const float bx = s_->position.x;
    const float by = s_->position.y;
    const float dx = tx - bx;
    const float dy = ty - by;
    const float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 1.0f) return nullptr;   // already on target — corridor undefined
    // Unit direction vector toward (tx, ty).
    const float ux = dx / dist;
    const float uy = dy / dist;
    const float forward_cap = std::min(max_forward, dist);
    const float lateral_sq  = half_width * half_width;
    const uint8 me_level    = s_->identity.level;
    NearbyUnit const* best = nullptr;
    float best_forward = forward_cap;
    for (auto const& u : s_->combat.nearby_enemies)
    {
        if (u.hp <= 0)               continue;       // dead / corpse
        if (u.is_player)             continue;       // PvP path differs
        // Never pull a no-XP / pacified creature as a path threat: training
        // dummies and event props are immortal or grant nothing, so attacking
        // one wedges the bot InCombat forever (the pull rule re-aggros it every
        // tick faster than combat:disengage_no_progress can shake it). Pacified
        // is the robust catch for dummies that lack the NO_XP flag in data.
        if (u.no_xp_kill || u.is_pacified)           continue;
        if (!exclude_guid.IsEmpty() && u.guid == exclude_guid) continue;
        // Grey-con mobs don't aggro and don't threaten — ignore.
        // TC's aggro line: a creature with level + 5 < attacker.level
        // won't initiate aggro at any range (see Creature::CanAggro).
        // Mirror that exactly so we don't flag mobs the bot could walk
        // past unharmed.
        if (me_level >= 6 && u.level + 5 < me_level) continue;
        // Symmetric UPPER cap: never flag a mob so far above the bot that pulling
        // it is suicide — the caller start_attacks path threats, so without this a
        // low bot whose quest corridor crosses high-level content pulls a mob it
        // cannot scratch and gets one-shot (live: Morthan L9 start_attacking L50
        // Tirisfal war-campaign Blighted Soldiers en route to a classic quest).
        // +10 still allows pulling genuinely tough-but-fightable mobs to clear a
        // path; beyond that it isn't a "pull" decision, it's a death. (NOT applied
        // to path_threat_count — a lethal mob still COUNTS as corridor danger.)
        if (u.level > me_level + 10) continue;
        const float vx = u.x - bx;
        const float vy = u.y - by;
        const float fwd = vx * ux + vy * uy;
        if (fwd <= 0.0f)             continue;       // behind us
        if (fwd >= best_forward)     continue;       // farther than current best
        // Lateral distance from corridor axis.
        const float lat_x = vx - fwd * ux;
        const float lat_y = vy - fwd * uy;
        if (lat_x * lat_x + lat_y * lat_y > lateral_sq) continue;
        best         = &u;
        best_forward = fwd;
    }
    return best;
}

size_t BotSnapshotView::path_threat_count(float tx, float ty,
                                          float max_forward,
                                          float half_width,
                                          ObjectGuid exclude_guid) const
{
    const float bx = s_->position.x;
    const float by = s_->position.y;
    const float dx = tx - bx;
    const float dy = ty - by;
    const float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 1.0f) return 0;
    const float ux = dx / dist;
    const float uy = dy / dist;
    const float forward_cap = std::min(max_forward, dist);
    const float lateral_sq  = half_width * half_width;
    const uint8 me_level    = s_->identity.level;
    size_t count = 0;
    for (auto const& u : s_->combat.nearby_enemies)
    {
        if (u.hp <= 0) continue;
        if (u.is_player) continue;
        if (!exclude_guid.IsEmpty() && u.guid == exclude_guid) continue;
        if (me_level >= 6 && u.level + 5 < me_level) continue;
        const float vx = u.x - bx;
        const float vy = u.y - by;
        const float fwd = vx * ux + vy * uy;
        if (fwd <= 0.0f) continue;
        if (fwd >= forward_cap) continue;
        const float lat_x = vx - fwd * ux;
        const float lat_y = vy - fwd * uy;
        if (lat_x * lat_x + lat_y * lat_y > lateral_sq) continue;
        ++count;
    }
    return count;
}

NearbyUnit const* BotSnapshotView::untaunted_enemy(float range) const
{
    const float r2 = range * range;
    const ObjectGuid me = s_->guid;
    for (auto const& e : s_->combat.nearby_enemies)
    {
        if (e.victim.IsEmpty()) continue;       // not engaged
        if (e.victim == me)     continue;       // already on us
        if (e.hp <= 0)          continue;
        const float dx = e.x - s_->position.x;
        const float dy = e.y - s_->position.y;
        const float dz = e.z - s_->position.z;
        if (dx*dx + dy*dy + dz*dz > r2) continue;
        return &e;
    }
    return nullptr;
}

bool BotSnapshotView::has_item(uint32 entry) const
{
    for (auto const& it : s_->inventory.bag_items)
        if (it.entry == entry)
            return true;
    return false;
}

bool BotSnapshotView::knows_spell(uint32 spell_id) const
{
    return std::binary_search(s_->spellbook.known_spells.begin(), s_->spellbook.known_spells.end(), spell_id);
}

QuestEntry const* BotSnapshotView::find_quest(uint32 quest_id) const
{
    uint32 const* pi = s_->quest_log.quests_index.find(quest_id);
    if (!pi) return nullptr;
    const uint32 i = *pi;
    if (i >= s_->quest_log.quests.size()) return nullptr;
    return &s_->quest_log.quests[i];
}

bool BotSnapshotView::has_drainable_mail() const
{
    return next_drainable_mail() != nullptr;
}

MailEntry const* BotSnapshotView::next_drainable_mail() const
{
    for (auto const& m : s_->mailbox.mail)
    {
        // Pending-deliver: server hasn't released the attachments yet.
        if (m.deliver_in_sec > 0) continue;
        // L-P1a: skip Cash-On-Delivery mail entirely. Auto-paying COD in
        // autonomous drain is a gold-drain exploit (a hostile sender mails
        // a COD item; the bot would pay to take it). Leave COD mail for the
        // owner to handle manually.
        if (m.cod > 0) continue;
        // Returned mails sit in the bot's box but can't be re-returned;
        // include them — the AI may still want to delete or take items
        // (returns from auctions land here too).
        if (m.money > 0 || m.item_count > 0) return &m;
    }
    return nullptr;
}

AuraEntry const* BotSnapshotView::find_pet_aura(uint32 spell_id) const
{
    for (auto const& a : s_->pet.pet_auras)
        if (a.spell_id == spell_id) return &a;
    return nullptr;
}

// ---------- World metadata accessors -----------------------------------
//
// All four methods do a linear scan of the metadata store filtered by
// (map_id, kind). The store is bounded at thousands of points total
// across all maps; per-bot-tick this is ~hundreds of float-compares,
// comparable to a single nearby_enemies scan. No per-snapshot caching
// needed (cache would need invalidation on `meta add/delete` and the
// linear scan is cheap enough not to be worth it).
//
// Mapping: BotSnapshotView::*_metadata APIs take a `uint32 kind`
// argument so the public view header doesn't depend on WorldMetadata.h
// (avoids dragging that include into every TU using snapshots). Callers
// cast the WorldMetadataKind enum value to uint32 at the call site.

float BotSnapshotView::metadata_dist_sq(uint32 kind) const
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;
    auto const& store = WorldMetadataStore::Instance();
    if (store.Size() == 0) return FLT_MAX;
    auto rows = store.RecordsForMapAndKind(
        s_->position.map_id, WorldMetadataKind(kind));
    if (rows.empty()) return FLT_MAX;
    const float bx = s_->position.x;
    const float by = s_->position.y;
    float best = FLT_MAX;
    for (auto const& r : rows)
    {
        const float dx = r.x - bx;
        const float dy = r.y - by;
        const float d2 = dx*dx + dy*dy;
        if (d2 < best) best = d2;
    }
    return best;
}

bool BotSnapshotView::inside_metadata(uint32 kind) const
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;
    auto const& store = WorldMetadataStore::Instance();
    if (store.Size() == 0) return false;
    auto rows = store.RecordsForMapAndKind(
        s_->position.map_id, WorldMetadataKind(kind));
    if (rows.empty()) return false;
    const float bx = s_->position.x;
    const float by = s_->position.y;
    for (auto const& r : rows)
    {
        const float dx = r.x - bx;
        const float dy = r.y - by;
        if (dx*dx + dy*dy <= r.radius * r.radius)
            return true;
    }
    return false;
}

bool BotSnapshotView::in_city() const
{
    return inside_metadata(uint32(::Playerbot::V2::World::WorldMetadataKind::City));
}

bool BotSnapshotView::in_village() const
{
    return inside_metadata(uint32(::Playerbot::V2::World::WorldMetadataKind::Village));
}

bool BotSnapshotView::in_danger_zone() const
{
    return inside_metadata(uint32(::Playerbot::V2::World::WorldMetadataKind::Danger));
}

bool BotSnapshotView::any_metadata_within(uint32 kind, float range) const
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;
    auto const& store = WorldMetadataStore::Instance();
    if (store.Size() == 0) return false;
    auto rows = store.RecordsForMapAndKind(
        s_->position.map_id, WorldMetadataKind(kind));
    if (rows.empty()) return false;
    const float bx = s_->position.x;
    const float by = s_->position.y;
    const float r2 = range * range;
    for (auto const& r : rows)
    {
        const float dx = r.x - bx;
        const float dy = r.y - by;
        if (dx*dx + dy*dy <= r2)
            return true;
    }
    return false;
}

} // namespace Playerbot
