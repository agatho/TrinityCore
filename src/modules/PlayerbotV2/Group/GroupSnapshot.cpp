#include "GroupSnapshot.h"
#include <algorithm>

namespace Playerbot {

namespace {

ObjectGuid GuidFromBotId(BotId id)
{
    return ObjectGuid::Create<HighGuid::Player>(id);
}

} // anonymous

GroupMemberSummary const* GroupSnapshotView::me(BotId id) const
{
    if (!g_) return nullptr;
    const ObjectGuid g = GuidFromBotId(id);
    for (auto const& m : g_->members)
        if (m.guid == g) return &m;
    return nullptr;
}

GroupMemberSummary const* GroupSnapshotView::lowest_hp(Role only) const
{
    if (!g_) return nullptr;
    GroupMemberSummary const* best = nullptr;
    int32 best_pct = 101;
    for (auto const& m : g_->members)
    {
        if (only != Role::Unknown && m.role != only) continue;
        if (!m.online || m.max_hp <= 0) continue;
        // Skip dead members — heals would just InvalidTarget on cast. dead_member()
        // is the right query for those (used by rez rules separately).
        if (m.hp <= 0) continue;
        const int32 pct = (m.hp * 100) / m.max_hp;
        if (pct < best_pct) { best_pct = pct; best = &m; }
    }
    return best;
}

GroupMemberSummary const* GroupSnapshotView::lowest_hp_on_map(uint32 map_id, Role only,
    float cx, float cy, float cz, float max_range) const
{
    if (!g_) return nullptr;
    GroupMemberSummary const* best = nullptr;
    int32 best_pct = 101;
    const float max_range_sq = max_range * max_range;
    for (auto const& m : g_->members)
    {
        if (only != Role::Unknown && m.role != only) continue;
        if (!m.online || m.max_hp <= 0) continue;
        if (m.map_id != map_id) continue;
        if (m.hp <= 0) continue;          // skip dead — see lowest_hp() for rationale
        // Distance gate (audit B23) — see header. max_range==0 = legacy.
        if (max_range > 0.f)
        {
            const float dx = m.x - cx, dy = m.y - cy, dz = m.z - cz;
            if (dx*dx + dy*dy + dz*dz > max_range_sq) continue;
        }
        const int32 pct = (m.hp * 100) / m.max_hp;
        if (pct < best_pct) { best_pct = pct; best = &m; }
    }
    return best;
}

GroupMemberSummary const* GroupSnapshotView::tank() const
{
    if (!g_) return nullptr;
    // Skip dead tanks — every caller (heal, beacon, lifebloom, augment buff,
    // pet send, threat dump) needs a living target. Battle-rez logic uses
    // dead_member() instead.
    for (auto const& m : g_->members)
        if (m.role == Role::Tank && m.online && m.hp > 0) return &m;
    return nullptr;
}

GroupMemberSummary const* GroupSnapshotView::lowest_mana_caster() const
{
    if (!g_) return nullptr;
    GroupMemberSummary const* best = nullptr;
    int32 best_pct = 101;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.max_mana <= 0) continue;
        if (m.hp <= 0) continue;          // dead casters don't burn mana
        const int32 pct = (m.mana * 100) / m.max_mana;
        if (pct < best_pct) { best_pct = pct; best = &m; }
    }
    return best;
}

GroupMemberSummary const* GroupSnapshotView::dead_member(uint32 map_id) const
{
    if (!g_) return nullptr;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.max_hp <= 0) continue;
        if (map_id != 0 && m.map_id != map_id) continue;
        if (m.hp <= 0) return &m;
    }
    return nullptr;
}

GroupMemberSummary const* GroupSnapshotView::dead_member_priority(uint32 map_id) const
{
    if (!g_) return nullptr;
    GroupMemberSummary const* tank   = nullptr;
    GroupMemberSummary const* healer = nullptr;
    GroupMemberSummary const* dps    = nullptr;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.max_hp <= 0) continue;
        if (map_id != 0 && m.map_id != map_id) continue;
        if (m.hp > 0) continue;
        switch (m.role)
        {
            case Role::Tank:   if (!tank)   tank   = &m; break;
            case Role::Healer: if (!healer) healer = &m; break;
            default:           if (!dps)    dps    = &m; break;
        }
    }
    if (tank)   return tank;
    if (healer) return healer;
    return dps;
}

uint32 GroupSnapshotView::count_dead(uint32 map_id) const
{
    if (!g_) return 0;
    uint32 n = 0;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.max_hp <= 0) continue;
        if (map_id != 0 && m.map_id != map_id) continue;
        if (m.hp <= 0) ++n;
    }
    return n;
}

GroupMemberSummary const* GroupSnapshotView::dispel_candidate(DispelType type) const
{
    if (!g_ || type == DispelType::None) return nullptr;
    for (auto const& m : g_->members)
    {
        if (!m.online) continue;
        if (m.hp <= 0) continue;          // can't dispel a corpse
        for (auto const& d : m.debuffs)
            if (d.is_harmful && d.dispel_type == type)
                return &m;
    }
    return nullptr;
}

GroupMemberSummary const* GroupSnapshotView::priority_dispel_candidate(
    std::vector<uint32> const& priority_spells) const
{
    if (!g_ || priority_spells.empty()) return nullptr;
    // Linear scan — priority lists are small (typically 1-3 spell ids per
    // active affix/encounter) and group is bounded to 5 / 25.
    for (auto const& m : g_->members)
    {
        if (!m.online) continue;
        if (m.hp <= 0) continue;
        for (auto const& d : m.debuffs)
        {
            if (!d.is_harmful) continue;
            for (uint32 ps : priority_spells)
                if (d.spell_id == ps) return &m;
        }
    }
    return nullptr;
}

GroupMemberSummary const* GroupSnapshotView::heal_assignment(ObjectGuid me, uint32 map_id,
    float cx, float cy, float cz, float max_range) const
{
    if (!g_) return nullptr;
    const float max_range_sq = max_range * max_range;
    // Step 1: collect all living healers on this map sorted by GUID for a
    // stable cross-tick index. Sorting by guid avoids surprises when the
    // members vector reorders (slot swaps, late joiners). Small N (1-5
    // healers in a raid), so an in-place sort of stack-allocated guids is
    // cheaper than maintaining a side cache.
    ObjectGuid healers[40];      // max raid size
    int healer_count = 0;
    int my_index = -1;
    for (auto const& m : g_->members)
    {
        if (m.role != Role::Healer) continue;
        if (!m.online || m.hp <= 0) continue;
        if (m.map_id != map_id) continue;
        if (healer_count >= 40) break;
        healers[healer_count++] = m.guid;
    }
    std::sort(healers, healers + healer_count);
    for (int i = 0; i < healer_count; ++i)
        if (healers[i] == me) { my_index = i; break; }

    // Step 2: collect wounded friendlies (anyone <100% HP, alive, on map),
    // sorted ascending by HP percent. Cap at 40 like above.
    struct WoundedRef { GroupMemberSummary const* m; int32 pct; };
    WoundedRef wounded[40];
    int wounded_count = 0;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.max_hp <= 0 || m.hp <= 0) continue;
        if (m.map_id != map_id) continue;
        const int32 pct = static_cast<int32>((int64_t(m.hp) * 100) / m.max_hp);
        if (pct >= 100) continue;
        // Distance gate (audit B23) — see header. max_range==0 = legacy.
        if (max_range > 0.f)
        {
            const float dx = m.x - cx, dy = m.y - cy, dz = m.z - cz;
            if (dx*dx + dy*dy + dz*dz > max_range_sq) continue;
        }
        if (wounded_count >= 40) break;
        wounded[wounded_count++] = {&m, pct};
    }
    std::sort(wounded, wounded + wounded_count,
              [](WoundedRef const& a, WoundedRef const& b) { return a.pct < b.pct; });

    if (wounded_count == 0) return nullptr;
    // Solo healer (or unindexed caller — me not found) → plain lowest.
    if (my_index < 0 || healer_count <= 1) return wounded[0].m;
    // Multiple healers: each takes the Nth wounded; if fewer wounded than
    // healers, late-indexed healers fall back to the lowest (a single very
    // wounded target gets multiple heals — accepted, better than the lowest
    // dying because higher-indexed healers had no assignment).
    const int slot = my_index < wounded_count ? my_index : 0;
    return wounded[slot].m;
}

GroupMemberSummary const* GroupSnapshotView::member_missing_buff(uint32 spell_id, uint32 map_id,
    float cx, float cy, float cz, float max_range) const
{
    if (!g_ || spell_id == 0) return nullptr;
    const float max_range_sq = max_range * max_range;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.hp <= 0) continue;            // dead/offline can't be buffed
        if (map_id != 0 && m.map_id != map_id) continue; // different map — cast would fail
        if (max_range > 0.f)
        {
            const float dx = m.x - cx, dy = m.y - cy, dz = m.z - cz;
            if (dx*dx + dy*dy + dz*dz > max_range_sq) continue; // too far — cast would fail
        }
        // Treat very-short remaining duration (<=60s) the same as missing so
        // we proactively refresh before drop-off.
        bool present = false;
        for (auto const& a : m.buffs)
        {
            if (a.spell_id != spell_id) continue;
            if (a.remaining.count() > 60'000) { present = true; break; }
        }
        if (!present) return &m;
    }
    return nullptr;
}

bool GroupSnapshotView::group_has_mechanic(uint32 mechanic, uint32 map_id) const
{
    if (!g_ || mechanic == 0) return false;
    for (auto const& m : g_->members)
    {
        if (!m.online || m.hp <= 0) continue;
        if (map_id != 0 && m.map_id != map_id) continue;
        for (auto const& d : m.debuffs)
            if (d.is_harmful && d.mechanic == mechanic)
                return true;
    }
    return false;
}

} // namespace Playerbot
