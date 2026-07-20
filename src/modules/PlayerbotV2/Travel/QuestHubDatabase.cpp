#include "QuestHubDatabase.h"

#include "Bot/BotSnapshot.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QueryResult.h"
#include "QuestDef.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <set>

namespace Playerbot::V2::Travel {

namespace {

// Race-id -> team bit. Mirrors TC's Player::TeamForRace without pulling in
// Player.h (the per-tick lookup runs on AI worker threads which intentionally
// don't touch Player). Pandaren-neutral (24) treated as Neutral until they
// pick a faction; bots are spawned with race 25/26 so that case is rare.
// Bit0 Alliance, Bit1 Horde, Bit2 Neutral.
constexpr uint32 race_team_bit(uint8 race)
{
    switch (race)
    {
        // Alliance
        case 1:  // Human
        case 3:  // Dwarf
        case 4:  // NightElf
        case 7:  // Gnome
        case 11: // Draenei
        case 22: // Worgen
        case 25: // PandarenA
        case 29: // VoidElf
        case 30: // LightforgedDraenei
        case 32: // KulTiran
        case 34: // DarkIronDwarf
        case 37: // Mechagnome
        case 52: // DracthyrA
        case 84: // EarthenA
            return 0x01;

        // Horde
        case 2:  // Orc
        case 5:  // Undead
        case 6:  // Tauren
        case 8:  // Troll
        case 9:  // Goblin
        case 10: // BloodElf
        case 26: // PandarenH
        case 27: // Nightborne
        case 28: // HighmountainTauren
        case 31: // ZandalariTroll
        case 35: // Vulpera
        case 36: // MagharOrc
        case 70: // DracthyrH
        case 85: // EarthenH
            return 0x02;

        // Neutral / unknown (Pandaren-neutral 24, Haranir 86)
        default:
            return 0x04;
    }
}

// ---- R7 leveling-zone band tables --------------------------------------
// Continent map-ids per leveling expansion (indexes match the Chromie-time
// bucket). Several expansions intentionally share continents (Classic / Cata
// both revamp Eastern Kingdoms + Kalimdor); the level-bracket filter inside
// SelectLevelingHub still picks an age-appropriate hub. Ids cross-checked
// against the QuestHubDatabase forensic dump comment (0=EK, 1=Kalimdor,
// 530=Outland, 571=Northrend, 646=Deepholm, 870=Pandaria, 1116=Draenor,
// 1220=Broken Isles, 1642/1643=BfA Zandalar/Kul Tiras, 2444=Dragon Isles,
// 2552=Khaz Algar).
constexpr uint32 kDragonIslesMap = 2444;   // L60-69 (Dragonflight)
constexpr uint32 kKhazAlgarMap   = 2552;   // L70-80 (newest 70-80 content)

// Band level edges (inclusive) for hysteresis bracket clamping.
void leveling_band_edges(uint8 level, uint8& lo, uint8& hi)
{
    if (level < 10)      { lo = 1;  hi = 9;  }
    else if (level < 60) { lo = 10; hi = 59; }
    else if (level < 70) { lo = 60; hi = 69; }
    else                 { lo = 70; hi = 80; }
}

} // anonymous

uint8 LevelingExpansionBucket(uint64 guid_low)
{
    // splitmix64 finaliser — decorrelates sequential guids so the 8 buckets
    // fill evenly even when characters are created in contiguous id ranges.
    uint64 z = guid_low + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z =  z ^ (z >> 31);
    return static_cast<uint8>(z & 0x7);   // [0,8)
}

std::vector<uint32> LevelingBandContinents(uint8 level, uint64 guid_low)
{
    if (level < 10)  return {};                    // racial starter (same-map)
    if (level < 60)
    {
        switch (LevelingExpansionBucket(guid_low))
        {
            case 0: return {0, 1};                 // Classic (EK + Kalimdor)
            case 1: return {530};                  // TBC / Outland
            case 2: return {571};                  // WotLK / Northrend
            case 3: return {0, 1, 646};            // Cata (revamp + Deepholm)
            case 4: return {870};                  // MoP / Pandaria
            case 5: return {1116};                 // WoD / Draenor
            case 6: return {1220};                 // Legion / Broken Isles
            case 7: return {1642, 1643};           // BfA Zandalar + Kul Tiras
            default: return {0, 1};
        }
    }
    if (level < 70) return {kDragonIslesMap};      // L60-69 Dragon Isles
    return {kKhazAlgarMap};                        // L70-80 newest content
}

bool LevelingBandAllowsContinent(uint8 level, uint64 guid_low, uint32 map_id)
{
    std::vector<uint32> const conts = LevelingBandContinents(level, guid_low);
    return std::find(conts.begin(), conts.end(), map_id) != conts.end();
}

// ============================================================================
// QuestHub
// ============================================================================

bool QuestHub::IsAppropriateFor(BotSnapshot const& snap) const
{
    // Cross-map hubs ARE allowed at the data level — but GetDistanceFrom returns
    // +inf cross-map, so the nearest-hub picker naturally filters them out
    // unless no in-map alternative exists. (V1's intent: long-range navigation
    // via flight/hearth eventually fills in.)

    if (snap.identity.level < minLevel) return false;
    if (maxLevel > 0 && snap.identity.level > maxLevel) return false;
    if (!(factionMask & race_team_bit(snap.identity.race))) return false;
    return true;
}

bool QuestHub::FactionAllows(BotSnapshot const& snap) const
{
    return (factionMask & race_team_bit(snap.identity.race)) != 0;
}

float QuestHub::GetDistanceFrom(BotSnapshot const& snap) const
{
    if (snap.position.map_id != mapId)
        return std::numeric_limits<float>::max();

    const float dx = snap.position.x - location.GetPositionX();
    const float dy = snap.position.y - location.GetPositionY();
    return std::sqrt(dx * dx + dy * dy);
}

bool QuestHub::ContainsPosition(Position const& pos) const
{
    const float dx = pos.GetPositionX() - location.GetPositionX();
    const float dy = pos.GetPositionY() - location.GetPositionY();
    return std::sqrt(dx * dx + dy * dy) <= radius;
}

float QuestHub::CalculateSuitabilityScore(BotSnapshot const& snap) const
{
    if (!IsAppropriateFor(snap)) return 0.0f;

    float score = 100.0f;

    // Level fit: ±2 levels = full score, ±10 levels ≈ 0.
    const uint32 hubMid = (minLevel + maxLevel) / 2;
    const int32  diff   = std::abs(int32(snap.identity.level) - int32(hubMid));
    score *= std::max(0.0f, 1.0f - diff / 10.0f);

    // Distance penalty (closer = better). Same-map only at this point because
    // IsAppropriateFor would still pass for cross-map; cross-map = +inf below.
    const float distance = GetDistanceFrom(snap);
    if (distance == std::numeric_limits<float>::max())
        score *= 0.5f;
    else
        score *= 1.0f / (1.0f + distance / 1000.0f);

    // More quests in the hub = more value per visit.
    const float questBonus = 1.0f + questIds.size() * 0.1f;
    score *= std::min(2.0f, questBonus);

    return score;
}

// ============================================================================
// QuestHubDatabase
// ============================================================================

bool QuestHubDatabase::Initialize()
{
    TC_LOG_INFO("playerbot.v2", "[QuestHubDatabase] Initializing quest hub database...");

    const auto t0 = std::chrono::steady_clock::now();

    try
    {
        const uint32 giverCount = LoadQuestGiversFromDB();
        if (giverCount == 0)
        {
            TC_LOG_ERROR("playerbot.v2", "[QuestHubDatabase] No quest givers found in DB.");
            return false;
        }

        const uint32 hubCount = ClusterQuestGiversIntoHubs();
        if (hubCount == 0)
        {
            TC_LOG_ERROR("playerbot.v2", "[QuestHubDatabase] DBSCAN produced no hubs.");
            return false;
        }

        LoadQuestDataForHubs();
        BuildSpatialIndex();
        ValidateHubData();

        // Approximate memory usage. Vector capacities aren't perfectly known
        // without iterating each, but this gets within a few percent.
        _memoryUsage  = _questHubs.size() * sizeof(QuestHub);
        _memoryUsage += _hubIdToIndex.size() * (sizeof(uint32) + sizeof(size_t));
        for (auto const& [zoneId, indices] : _zoneIndex)
            _memoryUsage += indices.size() * sizeof(size_t);

        // Release-store publishes the immutable _questHubs / _zoneIndex
        // / _hubIdToIndex to lock-free readers (GetQuestHubAtPosition).
        _initialized.store(true, std::memory_order_release);

        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        TC_LOG_INFO("playerbot.v2",
            "[QuestHubDatabase] Ready: {} hubs in {} ms, ~{} KB.",
            hubCount, dt, _memoryUsage / 1024);
        return true;
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("playerbot.v2", "[QuestHubDatabase] Exception during init: {}", ex.what());
        return false;
    }
}

bool QuestHubDatabase::Reload()
{
    TC_LOG_INFO("playerbot.v2", "[QuestHubDatabase] Reloading...");
    {
        std::unique_lock lk(_mutex);
        _questHubs.clear();
        _hubIdToIndex.clear();
        _zoneIndex.clear();
        _initialized.store(false, std::memory_order_release);
        _memoryUsage = 0;
    }
    return Initialize();
}

size_t QuestHubDatabase::GetQuestHubCount() const
{
    std::shared_lock lk(_mutex);
    return _questHubs.size();
}

size_t QuestHubDatabase::GetMemoryUsage() const
{
    std::shared_lock lk(_mutex);
    return _memoryUsage;
}

QuestHub const* QuestHubDatabase::GetNearestQuestHub(BotSnapshot const& snap) const
{
    if (!_initialized.load(std::memory_order_acquire)) return nullptr;
    std::shared_lock lk(_mutex);

    QuestHub const* best = nullptr;
    float           bestDist = std::numeric_limits<float>::max();
    for (auto const& hub : _questHubs)
    {
        if (!hub.IsAppropriateFor(snap)) continue;
        const float d = hub.GetDistanceFrom(snap);
        if (d < bestDist)
        {
            bestDist = d;
            best     = &hub;
        }
    }
    return best;
}

std::vector<QuestHub const*> QuestHubDatabase::GetQuestHubsForBot(
    BotSnapshot const& snap, uint32 maxCount) const
{
    if (!_initialized.load(std::memory_order_acquire)) return {};
    std::shared_lock lk(_mutex);

    std::vector<std::pair<QuestHub const*, float>> scored;
    scored.reserve(_questHubs.size());
    for (auto const& hub : _questHubs)
    {
        const float s = hub.CalculateSuitabilityScore(snap);
        if (s > 0.f) scored.emplace_back(&hub, s);
    }
    std::sort(scored.begin(), scored.end(),
              [](auto const& a, auto const& b) { return a.second > b.second; });

    std::vector<QuestHub const*> out;
    out.reserve(std::min<size_t>(maxCount, scored.size()));
    for (size_t i = 0; i < scored.size() && i < maxCount; ++i)
        out.push_back(scored[i].first);
    return out;
}

LevelingHubChoice QuestHubDatabase::SelectLevelingHub(
    BotSnapshot const& snap,
    std::function<bool(QuestHub const&)> const& is_doable) const
{
    LevelingHubChoice choice{};
    if (!_initialized.load(std::memory_order_acquire)) return choice;

    const uint8  level    = snap.identity.level;
    const uint64 guid_low = snap.guid.GetCounter();
    if (level < 10) return choice;     // L1-9: same-map starter, no relocation

    std::vector<uint32> const conts = LevelingBandContinents(level, guid_low);

    // --- Phase 1: collect & rank candidates UNDER the lock. ---------------
    // We score every level-/faction-appropriate hub (CalculateSuitabilityScore
    // needs only the snapshot — no Player), tag band membership, and stash the
    // (pointer, score, on_band) triples into a local vector. We do NOT call
    // is_doable here: it touches the live Player and MUST NOT run while _mutex is
    // held (the database is otherwise Player-free for AI-worker safety). QuestHub
    // pointers remain valid after the lock drops — _questHubs is immutable
    // post-init (same lock-free guarantee GetQuestHubAtPosition documents).
    struct Cand { QuestHub const* hub; float score; bool on_band; };
    std::vector<Cand> cands;
    {
        std::shared_lock lk(_mutex);
        cands.reserve(_questHubs.size());
        for (auto const& hub : _questHubs)
        {
            const float s = hub.CalculateSuitabilityScore(snap);
            if (s <= 0.f) continue;                       // wrong level / faction
            const bool on_band =
                std::find(conts.begin(), conts.end(), hub.mapId) != conts.end();
            cands.push_back(Cand{&hub, s, on_band});
        }
    }

    // --- Phase 2: pick the best DOABLE candidate (lock released). ----------
    // Pass 1 — best DOABLE hub on the band's continents (the Chromie pick).
    // Pass 2 (fallback) — best DOABLE hub ANYWHERE, used only if the band's
    // continents have no usable+doable hub so a high-level bot is never stranded.
    // When is_doable is empty, every candidate counts as doable (diagnostic /
    // non-availability callers keep the original behavior). The suitability
    // ranking (level fit × distance × faction × quest density) is preserved
    // AMONG the doable candidates; for cross-continent hubs the distance term is
    // constant so level-fit + quest density decide the winner.
    QuestHub const* best_band = nullptr; float best_band_score = 0.f;
    QuestHub const* best_any  = nullptr; float best_any_score  = 0.f;
    for (Cand const& c : cands)
    {
        if (is_doable && !is_doable(*c.hub)) continue;    // exhausted for this bot
        if (c.score > best_any_score) { best_any_score = c.score; best_any = c.hub; }
        if (c.on_band && c.score > best_band_score)
        { best_band_score = c.score; best_band = c.hub; }
    }

    QuestHub const* picked = best_band ? best_band : best_any;
    if (!picked) return choice;

    uint8 band_lo = 0, band_hi = 0;
    leveling_band_edges(level, band_lo, band_hi);
    choice.hub_id     = picked->hubId;
    choice.map_id     = picked->mapId;
    // Clamp the hub's ContentTuning bracket to the band edges so hysteresis
    // (a) keeps the choice while the bot levels inside the hub's range and
    // (b) FORCES a re-pick at a band transition (e.g. 59→60 leaves the L10-59
    // band even if the hub nominally spans higher). minLevel/maxLevel==0 mean
    // "open-ended" in IsAppropriateFor, so fall back to the band edge.
    choice.bracket_lo = std::max<uint8>(picked->minLevel ? uint8(picked->minLevel) : band_lo, band_lo);
    choice.bracket_hi = picked->maxLevel ? std::min<uint8>(uint8(picked->maxLevel), band_hi) : band_hi;
    if (choice.bracket_hi < choice.bracket_lo) choice.bracket_hi = band_hi;
    return choice;
}

QuestHub const* QuestHubDatabase::GetQuestHubById(uint32 hubId) const
{
    if (!_initialized.load(std::memory_order_acquire)) return nullptr;
    std::shared_lock lk(_mutex);
    auto it = _hubIdToIndex.find(hubId);
    return it == _hubIdToIndex.end() ? nullptr : &_questHubs[it->second];
}

std::vector<QuestHub const*> QuestHubDatabase::GetQuestHubsInZone(uint32 zoneId) const
{
    if (!_initialized.load(std::memory_order_acquire)) return {};
    std::shared_lock lk(_mutex);
    auto it = _zoneIndex.find(zoneId);
    if (it == _zoneIndex.end()) return {};

    std::vector<QuestHub const*> out;
    out.reserve(it->second.size());
    for (size_t idx : it->second) out.push_back(&_questHubs[idx]);
    return out;
}

QuestHub const* QuestHubDatabase::GetQuestHubAtPosition(
    Position const& pos, std::optional<uint32> zoneId) const
{
    // Hubs are loaded once at module Initialize() and never mutated
    // afterwards. The acquire on `_initialized` ordering-pairs with the
    // release on the Initialize side, so all subsequent reads of
    // _zoneIndex / _questHubs see the fully-published data without a
    // mutex. At 1000+ bots this drops 1-2 ms / tick worth of
    // shared_lock acq/rel from the hot snapshot Build path.
    if (!_initialized.load(std::memory_order_acquire)) return nullptr;

    if (zoneId.has_value())
    {
        auto it = _zoneIndex.find(*zoneId);
        if (it == _zoneIndex.end()) return nullptr;
        for (size_t idx : it->second)
            if (_questHubs[idx].ContainsPosition(pos))
                return &_questHubs[idx];
        return nullptr;
    }

    for (auto const& hub : _questHubs)
        if (hub.ContainsPosition(pos))
            return &hub;
    return nullptr;
}

// ============================================================================
// Init pipeline
// ============================================================================

uint32 QuestHubDatabase::LoadQuestGiversFromDB()
{
    _tempQuestGivers.clear();

    // Reuses TC's WORLD_SEL_QUEST_GIVER_SPAWNS prepared statement (see
    // src/server/database/Database/Implementation/WorldDatabase.{h,cpp}).
    // Query yields all creature spawns whose template has the questgiver
    // npcflag (bit 1 = 2). Columns:
    //   0:guid 1:creature_entry 2:x 3:y 4:z 5:map 6:faction 7:zoneId
    WorldDatabasePreparedStatement* stmt =
        WorldDatabase.GetPreparedStatement(WORLD_SEL_QUEST_GIVER_SPAWNS);
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    if (!result)
    {
        TC_LOG_WARN("playerbot.v2",
            "[QuestHubDatabase] WORLD_SEL_QUEST_GIVER_SPAWNS returned no rows.");
        return 0;
    }

    uint32 count = 0;
    do
    {
        Field* f = result->Fetch();
        QuestGiverData d;
        d.creatureEntry   = f[1].GetUInt32();
        d.position.Relocate(f[2].GetFloat(), f[3].GetFloat(), f[4].GetFloat());
        d.mapId           = f[5].GetUInt32();
        d.factionTemplate = f[6].GetUInt32();
        d.zoneId          = f[7].GetUInt32();
        _tempQuestGivers.push_back(d);
        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("playerbot.v2", "[QuestHubDatabase] Loaded {} quest givers.", count);
    return count;
}

uint32 QuestHubDatabase::ClusterQuestGiversIntoHubs()
{
    if (_tempQuestGivers.empty()) return 0;

    // DBSCAN parameters: 75y EPSILON, MIN_POINTS=2. Empirically these produce
    // ~hundreds of hubs in TC retail data — large enough to cover all WoW
    // quest hubs, small enough to keep runtime queries cheap.
    constexpr float  EPSILON     = 75.0f;
    constexpr uint32 MIN_POINTS  = 2;

    TC_LOG_INFO("playerbot.v2",
        "[QuestHubDatabase] DBSCAN: epsilon={}y, min_points={}, on {} givers.",
        EPSILON, MIN_POINTS, _tempQuestGivers.size());

    std::vector<bool>  visited(_tempQuestGivers.size(), false);
    std::vector<int32> clusterIds(_tempQuestGivers.size(), -1);
    int32 currentClusterId = 0;

    auto findNeighbors = [&](size_t idx) -> std::vector<size_t>
    {
        std::vector<size_t> n;
        Position const& p = _tempQuestGivers[idx].position;
        const uint32 mapId = _tempQuestGivers[idx].mapId;

        for (size_t i = 0; i < _tempQuestGivers.size(); ++i)
        {
            if (i == idx) continue;
            // Same-map filter — without this, X/Y proximity across continents
            // would falsely cluster (Stormwind ~ Orgrimmar at 0,0,0 etc).
            if (_tempQuestGivers[i].mapId != mapId) continue;
            Position const& q = _tempQuestGivers[i].position;
            const float dx = p.GetPositionX() - q.GetPositionX();
            const float dy = p.GetPositionY() - q.GetPositionY();
            if (std::sqrt(dx * dx + dy * dy) <= EPSILON)
                n.push_back(i);
        }
        return n;
    };

    uint32 noise = 0;
    for (size_t i = 0; i < _tempQuestGivers.size(); ++i)
    {
        if (visited[i]) continue;
        visited[i] = true;
        std::vector<size_t> neighbors = findNeighbors(i);

        if (neighbors.size() < MIN_POINTS)
        {
            clusterIds[i] = -1;
            ++noise;
            continue;
        }

        clusterIds[i] = currentClusterId;
        for (size_t j = 0; j < neighbors.size(); ++j)
        {
            const size_t nIdx = neighbors[j];
            if (!visited[nIdx])
            {
                visited[nIdx] = true;
                std::vector<size_t> nn = findNeighbors(nIdx);
                if (nn.size() >= MIN_POINTS)
                    neighbors.insert(neighbors.end(), nn.begin(), nn.end());
            }
            if (clusterIds[nIdx] == -1)
                clusterIds[nIdx] = currentClusterId;
        }
        ++currentClusterId;
    }

    TC_LOG_INFO("playerbot.v2",
        "[QuestHubDatabase] DBSCAN: {} clusters, {} singleton givers excluded.",
        currentClusterId, noise);

    // Build hubs from clusters of >= MIN_POINTS members.
    std::unordered_map<int32, std::vector<size_t>> clusterMap;
    for (size_t i = 0; i < clusterIds.size(); ++i)
        if (clusterIds[i] >= 0) clusterMap[clusterIds[i]].push_back(i);

    uint32 hubId = 1;
    uint32 ghostsDropped = 0;
    for (auto const& [cid, indices] : clusterMap)
    {
        if (indices.size() < MIN_POINTS) continue;

        QuestHub hub;
        hub.hubId = hubId;   // tentative — only commit on push_back below

        float sumX = 0.f, sumY = 0.f, sumZ = 0.f;
        std::set<uint32> uniqueCreatures;
        uint32 mapId = 0, zoneId = 0;
        for (size_t idx : indices)
        {
            QuestGiverData const& qg = _tempQuestGivers[idx];
            sumX += qg.position.GetPositionX();
            sumY += qg.position.GetPositionY();
            sumZ += qg.position.GetPositionZ();
            uniqueCreatures.insert(qg.creatureEntry);
            if (mapId  == 0) mapId  = qg.mapId;
            if (zoneId == 0) zoneId = qg.zoneId;
        }

        // Mathematical centroid (mean) — used as a reference point for the
        // snap-to-nearest-giver step below. We do NOT publish this as the
        // hub location: a hub whose givers sit on a shoreline (Ratchet,
        // Booty Bay, Echo Isles, etc.) gets a mean that lands in the
        // water between them. Bots routed there hit `NoPath` because the
        // navmesh ends at the shore. Instead we snap hub.location to the
        // closest actual giver, guaranteeing it's on walkable terrain.
        const float meanX = sumX / indices.size();
        const float meanY = sumY / indices.size();
        const float meanZ = sumZ / indices.size();

        // Find the cluster member closest to the mean. If the nearest is
        // farther than EPSILON, the cluster is a numerical fluke — DBSCAN
        // can produce one when transitive-neighbor merging stitches two
        // distant arms together. Drop those: they have no walkable
        // anchor and any move_to(centroid) would always fail.
        size_t bestIdx   = indices.front();
        float  bestDist2 = std::numeric_limits<float>::max();
        for (size_t idx : indices)
        {
            Position const& p = _tempQuestGivers[idx].position;
            const float dx = p.GetPositionX() - meanX;
            const float dy = p.GetPositionY() - meanY;
            const float d2 = dx * dx + dy * dy;
            if (d2 < bestDist2) { bestDist2 = d2; bestIdx = idx; }
        }
        if (std::sqrt(bestDist2) > EPSILON)
        {
            ++ghostsDropped;
            TC_LOG_WARN("playerbot.v2",
                "[QuestHubDatabase] dropped ghost cluster map={} mean=({:.1f},{:.1f},{:.1f}) "
                "members={} nearest_giver={:.1f}y away (>{:.0f}y EPS)",
                mapId, meanX, meanY, meanZ,
                uint32(indices.size()),
                std::sqrt(bestDist2), EPSILON);
            continue;
        }

        // Snap location to the nearest giver. This puts the hub on
        // walkable terrain right next to a quest NPC — bots arriving
        // here will find the giver in nearby_friends immediately.
        Position const& anchor = _tempQuestGivers[bestIdx].position;
        hub.location.Relocate(anchor.GetPositionX(),
                              anchor.GetPositionY(),
                              anchor.GetPositionZ());
        hub.mapId       = mapId;
        hub.zoneId      = zoneId;
        hub.creatureIds.assign(uniqueCreatures.begin(), uniqueCreatures.end());

        // Hub radius = max member distance from snapped anchor + 10y
        // buffer. Used by ContainsPosition to test "is the bot already
        // at this hub". Snapping the anchor onto a giver makes the
        // farthest-member distance slightly larger than the old
        // mean-relative radius — still well under EPSILON × 2.
        float maxDist = 0.f;
        for (size_t idx : indices)
        {
            Position const& p = _tempQuestGivers[idx].position;
            const float dx = p.GetPositionX() - hub.location.GetPositionX();
            const float dy = p.GetPositionY() - hub.location.GetPositionY();
            maxDist = std::max(maxDist, std::sqrt(dx * dx + dy * dy));
        }
        hub.radius = maxDist + 10.0f;

        hub.factionMask = 0x07;   // refined in LoadQuestDataForHubs from quest allowable_races
        hub.name        = "Quest Hub " + std::to_string(hub.hubId);
        _questHubs.push_back(std::move(hub));
        ++hubId;
    }
    if (ghostsDropped > 0)
        TC_LOG_INFO("playerbot.v2",
            "[QuestHubDatabase] {} ghost clusters dropped (no giver within {:.0f}y of mean)",
            ghostsDropped, EPSILON);

    // Free the temp data — only needed during clustering, ~MB-scale on retail.
    _tempQuestGivers.clear();
    _tempQuestGivers.shrink_to_fit();

    return uint32(_questHubs.size());
}

void QuestHubDatabase::LoadQuestDataForHubs()
{
    if (_questHubs.empty()) return;

    // Collect every unique creature entry across all hubs, then issue batched
    // queries (IN clause of 100 entries each) to creature_queststarter joined
    // with quest_template. One round-trip per 100 creatures keeps query plans
    // sane and avoids hitting the server's max_allowed_packet on huge IN lists.
    std::set<uint32> allCreatureIds;
    for (auto const& hub : _questHubs)
        allCreatureIds.insert(hub.creatureIds.begin(), hub.creatureIds.end());

    std::unordered_map<uint32, std::vector<std::pair<uint32, uint64>>> creatureQuests;

    constexpr size_t BATCH = 100;
    std::vector<uint32> creatureIdVec(allCreatureIds.begin(), allCreatureIds.end());
    for (size_t s = 0; s < creatureIdVec.size(); s += BATCH)
    {
        const size_t e = std::min(s + BATCH, creatureIdVec.size());
        std::string list;
        list.reserve((e - s) * 8);
        for (size_t i = s; i < e; ++i)
        {
            if (i != s) list += ',';
            list += std::to_string(creatureIdVec[i]);
        }

        const std::string q =
            "SELECT DISTINCT qr.id, qr.quest, qt.AllowableRaces "
            "FROM creature_queststarter qr "
            "INNER JOIN quest_template qt ON qr.quest = qt.ID "
            "WHERE qr.id IN (" + list + ")";

        QueryResult result = WorldDatabase.Query(q.c_str());
        if (!result) continue;
        do
        {
            Field* f = result->Fetch();
            const uint32 cid     = f[0].GetUInt32();
            const uint32 questId = f[1].GetUInt32();
            const uint64 races   = f[2].GetUInt64();
            creatureQuests[cid].emplace_back(questId, races);
        } while (result->NextRow());
    }

    for (auto& hub : _questHubs)
    {
        std::set<uint32> uniqueQuests;
        uint32 factionMask = 0;
        for (uint32 cid : hub.creatureIds)
        {
            auto it = creatureQuests.find(cid);
            if (it == creatureQuests.end()) continue;
            for (auto const& [qid, races] : it->second)
            {
                uniqueQuests.insert(qid);
                if (races == 0 || races == 0xFFFFFFFFFFFFFFFFULL)
                {
                    factionMask |= 0x07;
                }
                else
                {
                    // Race-bit masks per ChrRaces.dbc / SharedDefines.h. Conservative
                    // on the race coverage; missing newer races default to neutral
                    // because they typically also map to one of the originals via
                    // reskin (e.g., Mechagnome = Gnome lineage).
                    constexpr uint64 ALLIANCE_MASK = 0x44D;     // Human|Dwarf|NE|Gnome|Draenei|Worgen ...
                    constexpr uint64 HORDE_MASK    = 0x2B2;     // Orc|Undead|Tauren|Troll|BloodElf|Goblin ...
                    if (races & ALLIANCE_MASK) factionMask |= 0x01;
                    if (races & HORDE_MASK)    factionMask |= 0x02;
                }
            }
        }
        hub.questIds.assign(uniqueQuests.begin(), uniqueQuests.end());
        if (factionMask != 0) hub.factionMask = factionMask;

        // Hub level range derived from each quest's ContentTuning DB2 row
        // (WoW 12.0+ dynamic level scaling). For a quest with a tuning
        // entry, MinLevel/MaxLevel define the band a bot can pick it up
        // in — these are the player-facing bounds Player::GetQuestLevel
        // and SatisfyQuestMinLevel/MaxLevel ultimately reference.
        //
        // Fallback for pre-ContentTuning (vanilla / TBC / WotLK / Cata
        // pre-squish) legacy quests: use Quest::GetMaxLevel() as the
        // ceiling and infer a 5-level band below it.
        //
        // Hub effective range = (min of all qmin, max of all qmax),
        // clamped to [1, 80]. This is what bot routing actually wants:
        // the hub matches a bot whose level falls inside the union of
        // its quests' bands. A starter zone with quests tuned L1-10
        // resolves to hub L1-10 → only L1-10 bots are routed to it;
        // L1 bots are never routed to a Cataclysm L10-50 hub.
        int32 hubMin = 0;
        int32 hubMax = 0;
        for (uint32 qid : uniqueQuests)
        {
            Quest const* q = sObjectMgr->GetQuestTemplate(qid);
            if (!q) continue;
            int32 qmin = 0;
            int32 qmax = 0;
            if (uint32 ctid = q->GetContentTuningId())
            {
                if (ContentTuningEntry const* ct = sContentTuningStore.LookupEntry(ctid))
                {
                    qmin = ct->MinLevel;
                    qmax = ct->MaxLevel;
                }
            }
            if (qmin <= 0)
            {
                // Legacy quest (no ContentTuning row): derive the bracket
                // from the only static level field this schema exposes,
                // Quest::GetMaxLevel(). WoW 12.0+ QuestDef has NO
                // RequiredLevel / QuestLevel accessor — those fields were
                // folded into ContentTuning. So we treat GetMaxLevel() as
                // the ceiling and assume an 8-level band below it.
                //
                // T-P2a: when MaxLevel is ALSO 0 (very common for legacy
                // quests), we have zero level signal. Do NOT collapse to
                // L1-10 — that mis-brackets the entire hub and pulls L1
                // bots into high-level zones. Skip this quest's level
                // contribution entirely; other quests in the hub (with
                // valid tuning/max) define the band, and if none do the
                // hub falls back to the [1,80] default below.
                const int32 legacyMax = int32(q->GetMaxLevel());
                if (legacyMax > 0)
                {
                    qmax = legacyMax;
                    qmin = std::max<int32>(1, legacyMax - 8);
                }
                else
                {
                    continue;   // no derivable bracket — don't pollute the hub range
                }
            }
            if (qmin <= 0) qmin = 1;
            if (qmax <= 0 || qmax < qmin) qmax = std::min<int32>(80, qmin + 8);
            if (hubMin == 0 || qmin < hubMin) hubMin = qmin;
            if (qmax > hubMax) hubMax = qmax;
        }
        if (hubMin <= 0) hubMin = 1;
        if (hubMax <= 0) hubMax = 80;
        hub.minLevel = uint8(std::clamp<int32>(hubMin, 1, 80));
        hub.maxLevel = uint8(std::clamp<int32>(hubMax, hub.minLevel, 80));
        if (hub.zoneId > 0)
            hub.name = "Quest Hub (Zone " + std::to_string(hub.zoneId) + ")";
    }
}

void QuestHubDatabase::BuildSpatialIndex()
{
    _hubIdToIndex.clear();
    _zoneIndex.clear();
    for (size_t i = 0; i < _questHubs.size(); ++i)
    {
        _hubIdToIndex[_questHubs[i].hubId] = i;
        _zoneIndex[_questHubs[i].zoneId].push_back(i);
    }
}

void QuestHubDatabase::ValidateHubData()
{
    uint32 warnings = 0;
    for (auto const& hub : _questHubs)
    {
        if (hub.questIds.empty()) ++warnings;
        if (hub.location.GetPositionX() == 0.f &&
            hub.location.GetPositionY() == 0.f &&
            hub.location.GetPositionZ() == 0.f) ++warnings;
        if (hub.minLevel > hub.maxLevel && hub.maxLevel > 0) ++warnings;
        if (hub.factionMask == 0) ++warnings;
    }
    if (warnings > 0)
        TC_LOG_WARN("playerbot.v2",
            "[QuestHubDatabase] {} validation warnings across {} hubs.",
            warnings, _questHubs.size());

    // Bracket coverage report. Helps catch dataset gaps where the DB
    // simply doesn't have quest-givers clustered in a leveling band —
    // bots in such a band will fail GetQuestHubsForBot at runtime and
    // collapse to wander. Worth knowing at boot rather than mid-session.
    //
    // Brackets are coarse on purpose (5 buckets) so an L1 vs L5 split
    // doesn't matter; what matters is "L1-9 has at least N hubs per
    // faction" — anything <2 here means questing-from-zero is going
    // to break for one or both factions.
    struct BracketCount { uint32 alliance = 0; uint32 horde = 0; uint32 neutral = 0; };
    BracketCount b_1_9, b_10_19, b_20_39, b_40_59, b_60_plus;
    // Race-bit constants — same encoding QuestHub::IsAppropriateFor uses
    // (raceMask bits, not faction enum). Alliance = Human/Dwarf/NE/Gnome/
    // Draenei/Worgen/Dark Iron/Kul Tiran/Lightforged Draenei/Void Elf/
    // Mechagnome/Pandaren-A/Dracthyr-A. Horde = Orc/Undead/Tauren/Troll/
    // BloodElf/Goblin/Nightborne/Highmountain/Mag'har/Zandalari/Vulpera/
    // Pandaren-H/Dracthyr-H. We just need any-of-each for the gap check.
    constexpr uint32 kAllyMask  = (1u<<0) | (1u<<2) | (1u<<3) | (1u<<6) | (1u<<10);
    constexpr uint32 kHordeMask = (1u<<1) | (1u<<4) | (1u<<5) | (1u<<7) | (1u<<9);
    for (auto const& hub : _questHubs)
    {
        const uint8 lo = hub.minLevel;
        BracketCount* tgt = nullptr;
        if      (lo < 10)  tgt = &b_1_9;
        else if (lo < 20)  tgt = &b_10_19;
        else if (lo < 40)  tgt = &b_20_39;
        else if (lo < 60)  tgt = &b_40_59;
        else               tgt = &b_60_plus;
        const bool allyOk  = (hub.factionMask & kAllyMask)  != 0;
        const bool hordeOk = (hub.factionMask & kHordeMask) != 0;
        if (allyOk)  ++tgt->alliance;
        if (hordeOk) ++tgt->horde;
        if (!allyOk && !hordeOk) ++tgt->neutral;
    }
    auto emit = [](char const* tag, BracketCount const& c)
    {
        const uint32 total = c.alliance + c.horde + c.neutral;
        // Warn if EITHER faction has zero hubs in the bracket — bots
        // of that faction in that level band will have no destination.
        if (c.alliance == 0 || c.horde == 0)
        {
            TC_LOG_WARN("playerbot.v2",
                "[QuestHubDatabase] bracket {}: total={} ally={} horde={} neutral={} "
                "— faction GAP, bots in that level band will not route to a hub",
                tag, total, c.alliance, c.horde, c.neutral);
        }
        else
        {
            TC_LOG_INFO("playerbot.v2",
                "[QuestHubDatabase] bracket {}: total={} ally={} horde={} neutral={}",
                tag, total, c.alliance, c.horde, c.neutral);
        }
    };
    emit("L1-9",   b_1_9);
    emit("L10-19", b_10_19);
    emit("L20-39", b_20_39);
    emit("L40-59", b_40_59);
    emit("L60+",   b_60_plus);

    // Per-hub forensic dump. Each line names the AreaTable row the hub's
    // centroid lives in plus a Z-warning when the centroid sits at or near
    // sea level. Lets the operator cross-check which hubs are legitimate
    // shore clusters (Ratchet / Booty Bay / etc.) vs centroids that ended
    // up in open water because DBSCAN averaged shoreline givers across the
    // coastline. Logs at INFO so they only appear when the playerbot.v2
    // logger is set to level >= 3.
    for (auto const& hub : _questHubs)
    {
        AreaTableEntry const* area =
            sAreaTableStore.LookupEntry(hub.zoneId);
        char const* zone_name = area ? area->AreaName[LOCALE_enUS] : "?";
        // Z < 5 on continent maps (0=EK, 1=Kalimdor, 530=Outland, 571=
        // Northrend, 870=Pandaria, 1116=Draenor, 1220=Broken Isles,
        // 1642/1643=BfA, 2222=Shadowlands, 2444=DF, 2552=TWW) is at
        // or below sea level — likely an underwater centroid. The
        // hub may still be legitimate (Ratchet, Booty Bay, etc.) and
        // just needs its centroid snapped to the nearest land giver;
        // the warning flags it so we know which ones need attention.
        const float z = hub.location.GetPositionZ();
        const bool z_warn = z < 5.0f;
        TC_LOG_INFO("playerbot.v2",
            "[QuestHubDatabase] hub#{} map={} zone={}({}) "
            "center=({:.1f},{:.1f},{:.1f}){} quests={} L{}-{} fmask={:#x}",
            hub.hubId, hub.mapId, hub.zoneId, zone_name,
            hub.location.GetPositionX(),
            hub.location.GetPositionY(),
            hub.location.GetPositionZ(),
            z_warn ? " ZWARN" : "",
            uint32(hub.questIds.size()),
            uint32(hub.minLevel), uint32(hub.maxLevel),
            uint32(hub.factionMask));
    }
}

} // namespace Playerbot::V2::Travel
