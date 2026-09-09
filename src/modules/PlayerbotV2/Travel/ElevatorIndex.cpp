// ElevatorIndex implementation — see header for design.

#include "ElevatorIndex.h"

#include "GameObjectData.h"
#include "ObjectMgr.h"
#include "TransportMgr.h"
#include "DB2Structure.h"
#include "SharedDefines.h"
#include "Log.h"
#include "Player.h"
#include "Position.h"
#include "PlayerbotMovement.h"   // BotMovement::NearestNavPoint (world-thread navmesh)

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace Playerbot::V2::Travel {

ElevatorIndex& ElevatorIndex::Instance()
{
    static ElevatorIndex inst;
    return inst;
}

namespace {

// Find the animation frame whose TimeIndex is closest to `time_ms`.
// Returns nullptr only when the animation is empty.
TransportAnimationEntry const* FrameAtTime(TransportAnimation const* anim,
                                            uint32_t time_ms)
{
    if (!anim || anim->Path.empty()) return nullptr;
    // std::map sorted by time. lower_bound finds first >= time.
    auto it = anim->Path.lower_bound(time_ms);
    if (it == anim->Path.end()) return std::prev(it)->second;
    if (it == anim->Path.begin()) return it->second;
    // Pick closer of (it-1, it).
    auto prev = std::prev(it);
    uint32_t d_prev = time_ms - prev->first;
    uint32_t d_next = it->first - time_ms;
    return (d_prev <= d_next) ? prev->second : it->second;
}

// Transform a local-space animation frame offset (already taken relative to
// the anchor frame) into world coordinates using the spawn orientation
// (rotation about Z) + spawn translation. Mirrors the original Timeto math.
ElevatorStop MakeStop(uint32_t map_id, uint32_t entry, uint64_t spawn_id,
                      float spawn_x, float spawn_y, float spawn_z,
                      float cos_o, float sin_o,
                      float local_dx, float local_dy, float local_dz,
                      uint8_t stop_idx)
{
    const float world_dx = cos_o * local_dx - sin_o * local_dy;
    const float world_dy = sin_o * local_dx + cos_o * local_dy;
    ElevatorStop s;
    s.map_id   = map_id;
    s.entry    = entry;
    s.spawn_id = spawn_id;
    s.x        = spawn_x + world_dx;
    s.y        = spawn_y + world_dy;
    s.z        = spawn_z + local_dz;   // Z unaffected by yaw rotation
    s.stop_idx = stop_idx;
    return s;
}

} // anonymous namespace

ElevatorIndex::StopSource ElevatorIndex::AppendStopsFor(
    uint32_t map_id, uint32_t entry, uint64_t spawn_id,
    float spawn_x, float spawn_y, float spawn_z, float spawn_o)
{
    GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(entry);
    if (!tmpl || tmpl->type != GAMEOBJECT_TYPE_TRANSPORT) return StopSource::None;

    // Animation is keyed by the GO template entry. TransportMgr loads it via
    // AddPathNodeToTransport(anim->TransportID, ...) where the DB2 column named
    // "TransportID" stores the GameObject template entry (NOT the displayId),
    // and GetTransportAnimInfo() looks up by that same entry. So `entry` is the
    // correct key here (matching the original code path).
    TransportAnimation const* anim = sTransportMgr->GetTransportAnimInfo(entry);

    auto const& tdata = tmpl->transport;
    // Stop times in ms — only the ones the template actually sets.
    // Order matters: Timeto2nd ... Timeto9th in increasing index.
    uint32_t const stop_times[] = {
        tdata.Timeto2ndfloor, tdata.Timeto3rdfloor, tdata.Timeto4thfloor,
        tdata.Timeto5thfloor, tdata.Timeto6thfloor, tdata.Timeto7thfloor,
        tdata.Timeto8thfloor, tdata.Timeto9thfloor
    };
    bool any_timeto = false;
    for (uint32_t t : stop_times)
        if (t > 0) { any_timeto = true; break; }

    const float cos_o = std::cos(spawn_o);
    const float sin_o = std::sin(spawn_o);

    // ---- Path 1: world-DB Timeto*floor columns (~30 lifts have these) -----
    // The original behaviour: base stop = spawn position, upper stops = the
    // anim frame at each Timeto offset, expressed as a delta from the anim's
    // earliest (time-0) frame.
    if (any_timeto)
    {
        TransportAnimationEntry const* anchor =
            (anim && !anim->Path.empty()) ? anim->Path.begin()->second : nullptr;

        // Stage stops locally so we can apply the >=2-distinct-Z guard before
        // committing anything to stops_.
        std::vector<ElevatorStop> staged;
        staged.push_back(MakeStop(map_id, entry, spawn_id, spawn_x, spawn_y, spawn_z,
                                  cos_o, sin_o, 0.f, 0.f, 0.f, /*stop_idx*/ 0));

        if (anchor)
        {
            uint8_t idx = 1;
            for (uint32_t t : stop_times)
            {
                if (t == 0) { continue; }
                TransportAnimationEntry const* frame = FrameAtTime(anim, t);
                if (!frame) { ++idx; continue; }
                staged.push_back(MakeStop(
                    map_id, entry, spawn_id, spawn_x, spawn_y, spawn_z, cos_o, sin_o,
                    frame->Pos.X - anchor->Pos.X,
                    frame->Pos.Y - anchor->Pos.Y,
                    frame->Pos.Z - anchor->Pos.Z,
                    idx));
                ++idx;
            }
        }

        // Guard: a real elevator must offer >=2 distinct Z floors. If the
        // Timeto columns reference frames that never change Z (or there was
        // no anim to resolve them), fall through to the animation extrema
        // path below rather than register a degenerate single-floor stop.
        bool multi_floor = false;
        for (size_t i = 1; i < staged.size() && !multi_floor; ++i)
            if (std::fabs(staged[i].z - staged[0].z) > 1.0f) multi_floor = true;

        if (multi_floor)
        {
            for (ElevatorStop const& s : staged) stops_.push_back(s);
            return StopSource::Timeto;
        }
        // else: degenerate Timeto data — try the animation extrema below.
    }

    // ---- Path 2: synthesize from TransportAnimation Z extrema -------------
    // ~126/156 type-11 elevators (incl. the Orgrimmar zeppelin-tower lift,
    // template 206609) have all Timeto*floor == 0; they are driven purely by
    // the client-side TransportAnimation path. Each frame is a local-model
    // (timeOffset -> X,Y,Z). The platform rises from a lowest-Z frame to a
    // highest-Z frame; those two frames are the boarding floor and the
    // platform floor. We anchor on the lowest-Z frame so its world position
    // matches the spawn (the GO is spawned at rest, i.e. at its lowest stop).
    if (!anim || anim->Path.size() < 2) return StopSource::None;

    TransportAnimationEntry const* lo = nullptr;
    TransportAnimationEntry const* hi = nullptr;
    for (auto const& kv : anim->Path)
    {
        TransportAnimationEntry const* f = kv.second;
        if (!f) continue;
        if (!lo || f->Pos.Z < lo->Pos.Z) lo = f;
        if (!hi || f->Pos.Z > hi->Pos.Z) hi = f;
    }
    if (!lo || !hi) return StopSource::None;

    // Guard: degenerate (flat) animation — not a real lift.
    if (std::fabs(hi->Pos.Z - lo->Pos.Z) < 2.0f) return StopSource::None;

    // Anchor = lowest frame. The spawn position is the rest (lowest) pose, so
    // the lowest frame maps exactly to the spawn point; the highest frame is
    // offset by (hi - lo) in local space, rotated by the spawn yaw.
    TransportAnimationEntry const* anchor = lo;

    // Base / boarding floor = spawn position (the rest pose).
    stops_.push_back(MakeStop(map_id, entry, spawn_id, spawn_x, spawn_y, spawn_z,
                              cos_o, sin_o, 0.f, 0.f, 0.f, /*stop_idx*/ 0));

    // Detect any clear intermediate Z plateaus for multi-stop shafts: a frame
    // whose Z sits strictly between lo and hi AND is a local maximum where the
    // platform dwells (>=2 consecutive frames at ~same Z). Collected as
    // distinct floors so multi-stop lifts (rare) get their middle stops too.
    std::vector<float> intermediate_dz;
    {
        std::vector<std::pair<uint32_t, float>> frames;   // (time, localZ-lo)
        frames.reserve(anim->Path.size());
        for (auto const& kv : anim->Path)
            if (kv.second) frames.emplace_back(kv.first, kv.second->Pos.Z - lo->Pos.Z);
        const float span = hi->Pos.Z - lo->Pos.Z;
        for (size_t i = 1; i + 1 < frames.size(); ++i)
        {
            const float z = frames[i].second;
            // strictly interior (5%..95% of span) and a dwell point: the next
            // frame holds the same Z (platform paused before continuing).
            if (z <= span * 0.05f || z >= span * 0.95f) continue;
            if (std::fabs(frames[i + 1].second - z) > 0.5f) continue;
            // de-dup against already-recorded plateaus.
            bool dup = false;
            for (float d : intermediate_dz) if (std::fabs(d - z) < 1.0f) { dup = true; break; }
            if (!dup) intermediate_dz.push_back(z);
        }
    }

    uint8_t idx = 1;
    for (float dz : intermediate_dz)
    {
        // Intermediate frames share the rest pose's X/Y in most lift anims;
        // we only have a Z plateau, so apply the lo-frame X/Y (zero local dx/dy
        // relative to the anchor's plane) plus the Z delta.
        stops_.push_back(MakeStop(map_id, entry, spawn_id, spawn_x, spawn_y, spawn_z,
                                  cos_o, sin_o, 0.f, 0.f, dz, idx));
        ++idx;
    }

    // Top / platform floor.
    stops_.push_back(MakeStop(map_id, entry, spawn_id, spawn_x, spawn_y, spawn_z,
                              cos_o, sin_o,
                              hi->Pos.X - anchor->Pos.X,
                              hi->Pos.Y - anchor->Pos.Y,
                              hi->Pos.Z - anchor->Pos.Z,
                              idx));
    return StopSource::Animation;
}

void ElevatorIndex::LoadFromGameObjects()
{
    std::lock_guard<std::mutex> lk(mtx_);
    stops_.clear();

    // Iterate every spawned GO. The data store keys by spawnId (gameobject.guid).
    auto const& store = sObjectMgr->GetAllGameObjectData();
    size_t scanned = 0;
    size_t elevators = 0;
    size_t via_timeto = 0;     // elevators indexed from world-DB Timeto columns
    size_t via_anim = 0;       // elevators indexed from TransportAnimation DB2
    size_t skipped = 0;        // type-11 GOs with no usable multi-floor data
    for (auto const& kv : store)
    {
        GameObjectData const& d = kv.second;
        ++scanned;
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(d.id);
        if (!tmpl) continue;
        if (tmpl->type != GAMEOBJECT_TYPE_TRANSPORT) continue;
        ++elevators;
        switch (AppendStopsFor(d.mapId, d.id, d.spawnId,
                               d.spawnPoint.GetPositionX(),
                               d.spawnPoint.GetPositionY(),
                               d.spawnPoint.GetPositionZ(),
                               d.spawnPoint.GetOrientation()))
        {
            case StopSource::Timeto:    ++via_timeto; break;
            case StopSource::Animation: ++via_anim;   break;
            case StopSource::None:      ++skipped;    break;
        }
    }
    TC_LOG_INFO("playerbot.v2",
        "[ElevatorIndex] scanned {} GO spawns, found {} type-11 elevators "
        "({} via Timeto, {} via TransportAnimation, {} skipped/degenerate), "
        "registered {} stops total",
        scanned, elevators, via_timeto, via_anim, skipped, stops_.size());
}

ElevatorStop const* ElevatorIndex::NearestStopOnFloor(uint32_t map_id,
                                                       float x, float y, float z,
                                                       float xy_range, float z_range) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    ElevatorStop const* best = nullptr;
    const float r2 = xy_range * xy_range;
    float best_dsq = r2;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        if (std::fabs(s.z - z) > z_range) continue;
        const float dx = s.x - x, dy = s.y - y;
        const float dsq = dx*dx + dy*dy;
        if (dsq < best_dsq) { best_dsq = dsq; best = &s; }
    }
    return best;
}

ElevatorStop const* ElevatorIndex::NearestStopAnyZ(uint32_t map_id,
                                                    float x, float y,
                                                    float xy_range) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    ElevatorStop const* best = nullptr;
    const float r2 = xy_range * xy_range;
    float best_dsq = r2;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        const float dx = s.x - x, dy = s.y - y;
        const float dsq = dx*dx + dy*dy;
        if (dsq < best_dsq) { best_dsq = dsq; best = &s; }
    }
    return best;
}

void ElevatorIndex::EnsureLedges(uint64_t spawn_id, Player* p)
{
    if (!p) return;
    // stops_ is immutable after the one-shot boot LoadFromGameObjects, so we
    // read it lock-free here (every caller is a post-boot world-thread tick).
    // Only ledges_ mutation/lookup is guarded by mtx_.
    for (ElevatorStop const& s : stops_)
    {
        if (s.spawn_id != spawn_id) continue;
        const uint64_t key = LedgeKey(spawn_id, s.stop_idx);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (ledges_.find(key) != ledges_.end()) continue;   // already derived
        }

        // Derive OUTSIDE the lock — the navmesh query (NearestNavPoint) can
        // LoadGrid + findNearestPoly, and we must not stall an AI worker's
        // LedgeFor read for that duration.
        LedgeRec rec;
        Position out;
        if (Playerbot::BotMovement::NearestNavPoint(
                p, s.x, s.y, s.z, /*hxy*/ 16.0f, /*hz*/ 8.0f, out))
        {
            const float dx  = out.GetPositionX() - s.x;
            const float dy  = out.GetPositionY() - s.y;
            const float dz  = out.GetPositionZ() - s.z;
            const float dxy = std::sqrt(dx * dx + dy * dy);
            // Accept only a ledge on THIS floor (|dz| <= 7y) and within reach
            // (<= 18y). A poly farther out / on another level means the query
            // escaped the shaft — fall back to the platform centre rather than
            // route the bot to the wrong floor.
            if (dxy <= 18.0f && std::fabs(dz) <= 7.0f)
            {
                rec.x = out.GetPositionX();
                rec.y = out.GetPositionY();
                rec.z = out.GetPositionZ();
                rec.has_ledge = true;
            }
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            ledges_.emplace(key, rec);   // no-op if a concurrent tick beat us
        }
        TC_LOG_INFO("playerbot.v2",
            "[ElevatorIndex] ledge entry={} spawn={} stop={} centre=({:.1f},{:.1f},{:.1f}) "
            "-> {} ({:.1f},{:.1f},{:.1f}) offset={:.1f}y",
            s.entry, spawn_id, uint32_t(s.stop_idx), s.x, s.y, s.z,
            rec.has_ledge ? "ledge" : "NONE", rec.x, rec.y, rec.z,
            rec.has_ledge ? std::sqrt((rec.x - s.x) * (rec.x - s.x) +
                                      (rec.y - s.y) * (rec.y - s.y)) : 0.f);
    }
}

void ElevatorIndex::EnsureLedgesNear(Player* p, float range)
{
    if (!p) return;
    const uint32_t map = p->GetMapId();
    const float px = p->GetPositionX(), py = p->GetPositionY();
    const float r2 = range * range;
    // Distinct shafts within range (stops_ immutable post-boot → lock-free read).
    std::vector<uint64_t> spawns;
    for (ElevatorStop const& s : stops_)
    {
        if (s.map_id != map) continue;
        const float dx = s.x - px, dy = s.y - py;
        if (dx * dx + dy * dy > r2) continue;
        if (std::find(spawns.begin(), spawns.end(), s.spawn_id) == spawns.end())
            spawns.push_back(s.spawn_id);
    }
    for (uint64_t sid : spawns)
        EnsureLedges(sid, p);   // idempotent / cached per stop
}

bool ElevatorIndex::LedgeFor(uint64_t spawn_id, uint8_t stop_idx,
                             float& lx, float& ly, float& lz) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = ledges_.find(LedgeKey(spawn_id, stop_idx));
    if (it == ledges_.end() || !it->second.has_ledge) return false;
    lx = it->second.x;
    ly = it->second.y;
    lz = it->second.z;
    return true;
}

bool ElevatorIndex::BestStopZToward(uint32_t map_id, float x, float y,
                                    float target_z, float xy_range,
                                    float& out_z) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    // Pass 1: nearest shaft to (x,y) by any stop's planar distance.
    ElevatorStop const* nearest = nullptr;
    float best_dsq = xy_range * xy_range;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        const float dx = s.x - x, dy = s.y - y;
        const float dsq = dx * dx + dy * dy;
        if (dsq < best_dsq) { best_dsq = dsq; nearest = &s; }
    }
    if (!nearest) return false;
    // Pass 2: among that shaft's stops, the one whose Z is closest to target_z.
    float best_dz = std::numeric_limits<float>::max();
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        if (s.spawn_id != nearest->spawn_id) continue;
        const float dz = std::fabs(s.z - target_z);
        if (dz < best_dz) { best_dz = dz; out_z = s.z; }
    }
    return best_dz != std::numeric_limits<float>::max();
}

ElevatorStop const* ElevatorIndex::LowestStopNear(uint32_t map_id,
                                                   float x, float y,
                                                   float xy_range) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    // Two-pass: (1) find the nearest shaft to (x,y) by any stop's planar
    // distance; (2) among that shaft's stops (same spawn_id), return the
    // lowest-Z one. This routes a ground-level bot to the boarding floor
    // even when the deck-level dock anchor that seeded (x,y) is dozens of
    // yards from the shaft (Org zeppelin towers: dock↔lift ≈ 50-60y).
    ElevatorStop const* nearest = nullptr;
    const float r2 = xy_range * xy_range;
    float best_dsq = r2;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        const float dx = s.x - x, dy = s.y - y;
        const float dsq = dx*dx + dy*dy;
        if (dsq < best_dsq) { best_dsq = dsq; nearest = &s; }
    }
    if (!nearest) return nullptr;
    ElevatorStop const* lowest = nearest;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        if (s.spawn_id != nearest->spawn_id) continue;
        if (s.z < lowest->z) lowest = &s;
    }
    return lowest;
}

ElevatorStop const* ElevatorIndex::LowestStopNearBoardable(uint32_t map_id,
                                                           float x, float y,
                                                           float xy_range) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    const float r2 = xy_range * xy_range;
    // Per-shaft lowest stop (the boarding floor).
    std::unordered_map<uint64_t, ElevatorStop const*> lowest;
    for (auto const& s : stops_)
    {
        if (s.map_id != map_id) continue;
        auto it = lowest.find(s.spawn_id);
        if (it == lowest.end() || s.z < it->second->z)
            lowest[s.spawn_id] = &s;
    }
    // Among shafts within range, track the nearest overall AND the nearest whose
    // boarding stop has a derived ledge (a lift we know the bot can board from).
    ElevatorStop const* nearest = nullptr;        float nearest_d = r2;
    ElevatorStop const* nearest_ledged = nullptr; float nearest_ledged_d = r2;
    for (auto const& [spawn_id, st] : lowest)
    {
        const float dx = st->x - x, dy = st->y - y;
        const float d  = dx * dx + dy * dy;
        if (d > r2) continue;
        if (d < nearest_d) { nearest_d = d; nearest = st; }
        auto lit = ledges_.find(LedgeKey(spawn_id, st->stop_idx));
        if (lit != ledges_.end() && lit->second.has_ledge && d < nearest_ledged_d)
        { nearest_ledged_d = d; nearest_ledged = st; }
    }
    // Prefer a boardable shaft; fall back to nearest when none is known-boardable
    // yet (ledges derive lazily — until a bot has visited a lift its boarding
    // floor is unknown, so we don't want to refuse all travel meanwhile).
    return nearest_ledged ? nearest_ledged : nearest;
}

size_t ElevatorIndex::StopsForMapCount(uint32_t map_id) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    size_t n = 0;
    for (auto const& s : stops_)
        if (s.map_id == map_id) ++n;
    return n;
}

} // namespace Playerbot::V2::Travel
