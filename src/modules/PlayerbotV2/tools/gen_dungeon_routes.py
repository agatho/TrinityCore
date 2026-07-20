#!/usr/bin/env python3
"""Auto-generate dungeon route_waypoints from the navmesh -- V2 (quality-focused).

Same core idea as gen_dungeon_routes.py: snap the LFG entrance to the mesh, then
chain-pathfind entrance -> boss1 -> boss2 -> ... (bosses in encounter order),
dropping waypoints along the corridor whenever a hop exceeds the ~74-poly (~292y)
path cap. But V2 hardens ROUTE QUALITY:

  1. WIDER ANCHOR SNAP. The raw LFG entrance is often a few yards OFF the mesh
     (instanced-dungeon teleport coord). The v1 snap just reused the first probe's
     START; if that probe returned FARFROMPOLY_START the anchor stayed off-mesh and
     EVERY subsequent boss failed (gnomeregan: all 6 bosses). V2 does a small
     ring/vertical offset search to find an on-mesh anchor near the entrance.

  2. ROBUST ADVANCE. v1 re-probed anchor->boss each hop and picked ONE vertex
     ~STEP along the returned partial path. On WINDING corridors the returned
     partial path oscillates (heads down a dead-end spur, then a different one),
     so the anchor never converges -> "stalled". V2 instead advances the anchor to
     the partial path's FURTHEST reachable on-mesh vertex, drops crumbs every STEP
     along that path, tracks the best (closest-to-boss) point reached, and detects
     revisits/loops. A hop that makes < MIN_PROGRESS yards toward the boss AND
     whose final short leg is a tiny-poly no-connection is flagged a GENUINE MESH
     GAP (unfixable by the generator; needs an off-mesh bridge / mesh regen).

  3. BOSS OFF-MESH HANDLING. If the boss coord itself is off-mesh
     (FARFROMPOLY_END), v1 already stops at the last on-mesh path vertex; V2 keeps
     that behavior and records the residual gap distance in the report.

  4. FOLLOWABILITY VALIDATION. After building the chain, V2 probes EACH consecutive
     crumb pair and requires a clean STATUS OK path within the poly cap. A route the
     runtime tank can't follow crumb-to-crumb is worse than none. --report prints
     the pass/fail counts per dungeon and writes NOTHING to the DB.

Usage:
    gen_dungeon_routes.py --report [--dump] [mapid ...]   # NO DB writes
    gen_dungeon_routes.py [--all|--dump] [mapid ...]       # writes routes (as v1)

Env: GEN_STEP (waypoint spacing, default 200), ROUTE_DB (default wowc_playerbot).
Offline: zero world-thread pathfinding risk.
"""
import subprocess, re, sys, math, os, statistics

MMAP_DIR = "M:/WorldofWarcraft/mmaps"
PROBE    = "M:/PlayerbotServer/mmap_probe.exe"
MYSQL    = r"C:/Program Files/MySQL/MySQL Server 9.4/bin/mysql.exe"
ROUTE_DB = os.environ.get("ROUTE_DB", "wowc_playerbot")
WORLD_DB = "wc_world"
STEP     = float(os.environ.get("GEN_STEP", "25"))    # resample spacing (yds)
MAX_GAP  = float(os.environ.get("GEN_MAX_GAP", "40"))  # hard cap between consecutive crumbs (yds)
MIN_SPACE = float(os.environ.get("GEN_MIN_SPACE","8"))
GUARD       = int(os.environ.get("GEN_GUARD","120"))  # max hops per boss leg
MIN_PROGRESS= 8.0      # a hop must close at least this many yds toward the boss
SNAP_RINGS  = [0.0, 8.0, 16.0, 30.0, 50.0, 80.0]   # xy ring radii for anchor snap
SNAP_ZS     = [0.0, -6.0, 6.0, -20.0, 20.0, -45.0, 45.0]  # vertical offsets
POLY_CAP    = 74       # the ~292y pathfinder poly cap mmap_probe truncates at
DENSIFY_PASSES = 6     # densify_chain iterates until stable or this many passes

REACHED = ("OK", "FARFROMPOLY_END", "NORMAL")
WALKABLE = ("SHORT", "PARTIAL", "INCOMPLETE")

# --- persistent per-map batch probe processes -------------------------------
# mmap_probe --batch loads the mesh ONCE and answers many src->dst queries
# over stdin/stdout. Without this, resampling (which needs many more probes
# than the old vertex-only crumbs_along) would be prohibitively slow -- every
# probe() call would re-load every .mmtile for the map from scratch.
_batch_procs = {}


def _get_batch_proc(mapid):
    proc = _batch_procs.get(mapid)
    if proc is not None and proc.poll() is None:
        return proc
    proc = subprocess.Popen(
        [PROBE, MMAP_DIR, str(mapid), "--batch"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        text=True, bufsize=1)
    # Drain the one-time setup block (dir/mapId, navMeshParams, loaded tiles)
    # that mmap_probe prints once at startup, before the first query.
    while True:
        ln = proc.stdout.readline()
        if not ln or ln.startswith("  loaded "):
            break
    _batch_procs[mapid] = proc
    return proc


def _read_block(proc):
    lines = []
    while True:
        ln = proc.stdout.readline()
        if not ln:
            break  # EOF -- process died mid-block
        if ln.rstrip("\n") == "END":
            break
        lines.append(ln)
    return "".join(lines)


def close_batch_procs():
    """Shut down all persistent probe processes. Call once at the end of a run."""
    for mapid, proc in list(_batch_procs.items()):
        try:
            proc.stdin.close()
        except Exception:
            pass
        try:
            proc.wait(timeout=5)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
    _batch_procs.clear()


def sql(db, q):
    r = subprocess.run([MYSQL, "-uplayerbot", "-pplayerbot", db, "-N", "-e", q],
                       capture_output=True, text=True)
    return [ln.split("\t") for ln in r.stdout.strip().splitlines() if ln.strip()]


def load_dungeons_from_dump(path="M:/PlayerbotServer/dungeondump.txt", lines=None):
    if lines is None:
        try:
            with open(path) as f:
                lines = f.read().splitlines()
        except FileNotFoundError:
            return None
    out = []
    for ln in lines:
        i = ln.find("DDUMP|")
        if i < 0:
            continue
        parts = ln[i:].strip().rstrip("&#xD;").split("|")
        if len(parts) < 5:
            continue
        try:
            mapid = int(parts[1]); lfgid = int(parts[2])
        except ValueError:
            continue
        name = parts[3]
        bosses = [int(b) for b in parts[4].split(",") if b.strip().isdigit()]
        if not bosses:
            continue
        out.append((name, mapid, lfgid, bosses))
    return out


def boss_positions(mapid, bosses):
    ids = ",".join(str(b) for b in bosses)
    rows = sql(WORLD_DB,
        f"SELECT c.id,c.position_x,c.position_y,c.position_z FROM creature c "
        f"WHERE c.id IN ({ids}) AND c.map={mapid}")
    pos = {}
    for r in rows:
        e = int(r[0]); p = (float(r[1]), float(r[2]), float(r[3]))
        pos.setdefault(e, p)
    return pos


def entrance(dungeon_id):
    rows = sql(WORLD_DB,
        f"SELECT position_x,position_y,position_z FROM lfg_dungeon_template WHERE dungeonId={dungeon_id}")
    if not rows:
        return None
    return (float(rows[0][0]), float(rows[0][1]), float(rows[0][2]))


def probe(mapid, a, b):
    """Run mmap_probe a->b via the persistent per-map --batch process (mesh
    loaded once; each call is one stdin/stdout round-trip, not a fresh
    process spawn + full tile reload). Return (status, start_snap, polyCount,
    planar_dist, path) where path is a list of (x, y, z, offmesh) tuples --
    offmesh=True means the straight-path edge FROM this vertex TO the next
    is an off-mesh connection (crossed by one committed move, not walked)."""
    proc = _get_batch_proc(mapid)
    query = f"{a[0]:.3f} {a[1]:.3f} {a[2]:.3f} {b[0]:.3f} {b[1]:.3f} {b[2]:.3f}\n"
    try:
        proc.stdin.write(query)
        proc.stdin.flush()
        out = _read_block(proc)
    except (BrokenPipeError, OSError):
        out = ""
    if not out or proc.poll() is not None:
        # process died -- drop it so the next call respawns, retry once
        _batch_procs.pop(mapid, None)
        proc = _get_batch_proc(mapid)
        try:
            proc.stdin.write(query)
            proc.stdin.flush()
            out = _read_block(proc)
        except (BrokenPipeError, OSError):
            out = ""
    status = "ERR"
    m = re.search(r"STATUS:\s+([A-Z_]+)", out)
    if m:
        status = m.group(1)
    start = None
    m = re.search(r"START:.*-> TC \(([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\)", out)
    if m:
        start = (float(m.group(1)), float(m.group(2)), float(m.group(3)))
    poly = 0
    m = re.search(r"polyCount=(\d+)", out)
    if m:
        poly = int(m.group(1))
    planar = None
    m = re.search(r"planar_dist_to_requested_dst=([\d.]+)", out)
    if m:
        planar = float(m.group(1))
    path = []
    for m in re.finditer(r"\[\s*\d+\](.*?)TC=\(([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\)", out):
        offmesh = "OFFMESH" in m.group(1)
        path.append((float(m.group(2)), float(m.group(3)), float(m.group(4)), offmesh))
    return status, start, poly, planar, path


def dist(a, b):
    return math.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2 + (a[2]-b[2])**2)


def resample(path, step=STEP, max_gap=MAX_GAP):
    """Resample a probe corridor (list of (x,y,z[,offmesh]) verts, in path
    order) into points spaced ~step yds apart ALONG ARC LENGTH -- i.e.
    interpolated WITHIN long segments, not just emitted at existing
    vertices. This is the fix for the dominant defect in the old
    crumbs_along(): straight-path vertices on open ground can be hundreds of
    yards apart, and the old vertex-only walker would silently emit a single
    multi-hundred-yard gap. Guarantees no two consecutive OUTPUT points are
    more than `max_gap` apart, EXCEPT across a segment flagged OFFMESH (an
    off-mesh connection) -- those are never subdivided; both endpoints are
    emitted verbatim because the bot crosses the link with one committed
    direct move, not incremental walking."""
    if not path:
        return []
    pts = [(p[0], p[1], p[2]) for p in path]
    flags = [bool(p[3]) if len(p) > 3 else False for p in path]
    if len(pts) == 1:
        return [pts[0]]
    out = []
    acc = 0.0
    for i in range(1, len(pts)):
        p0, p1 = pts[i-1], pts[i]
        if flags[i-1]:
            # off-mesh connection edge: pass through both endpoints untouched
            if not out or dist(out[-1], p0) > 0.01:
                out.append(p0)
            out.append(p1)
            acc = 0.0
            continue
        seg_len = dist(p0, p1)
        if seg_len <= 1e-6:
            continue
        pos = 0.0
        while True:
            need = step - acc
            remain = seg_len - pos
            if remain < need:
                acc += remain
                break
            pos += need
            t = pos / seg_len
            out.append((p0[0] + (p1[0]-p0[0]) * t,
                        p0[1] + (p1[1]-p0[1]) * t,
                        p0[2] + (p1[2]-p0[2]) * t))
            acc = 0.0
    if not out or dist(out[-1], pts[-1]) > 0.01:
        out.append(pts[-1])
    return out


def snap_anchor(mapid, ent, goal):
    """Return an on-mesh anchor near `ent`.

    IMPORTANT: if the RAW entrance already snaps on-mesh (probe gets a START snap,
    i.e. status is anything other than FARFROMPOLY_START), keep the raw snap --
    nudging it sideways can land on a DIFFERENT, disconnected mesh pocket that
    can't reach the boss (WC: raw=SHORT correct corridor, +16y nudge=PARTIAL dead
    pocket). Only when the raw entrance is genuinely OFF the mesh
    (FARFROMPOLY_START) do we search ring/vertical offsets to find the nearest
    on-mesh foothold (gnomeregan: entrance is ~a few yds off-mesh)."""
    st, snap, _poly, _pl, _path = probe(mapid, ent, goal)
    if st != "FARFROMPOLY_START" and snap is not None:
        return snap, st
    # entrance is off-mesh -- search outward for an on-mesh foothold that ROUTES
    # toward the goal. Rank by forward status first (a foothold that reaches / gets
    # SHORT toward the boss beats one that only gets PARTIAL into a dead pocket),
    # then by nearness to the entrance. Continue searching wider rings until a
    # clean (OK/SHORT) foothold is found or the search space is exhausted.
    best = None       # (rank, dist_to_ent, anchor, status)
    def rank(s):
        return {"OK": 0, "NORMAL": 0, "SHORT": 1, "FARFROMPOLY_END": 2,
                "PARTIAL": 3, "INCOMPLETE": 4}.get(s, 9)
    for r in SNAP_RINGS:
        for dz in SNAP_ZS:
            angles = range(0, 360, 30) if r > 0 else [0]
            for ang in angles:
                dx = r * math.cos(math.radians(ang))
                dy = r * math.sin(math.radians(ang))
                a = (ent[0]+dx, ent[1]+dy, ent[2]+dz)
                s2, snap2, _p, _pl, _pa = probe(mapid, a, goal)
                if s2 == "FARFROMPOLY_START" or snap2 is None:
                    continue
                cand = (rank(s2), dist(a, ent), snap2, s2)
                if best is None or cand[:2] < best[:2]:
                    best = cand
        # once we have a clean routing foothold (OK/SHORT), don't search wider
        if best is not None and best[0] <= 1:
            break
    if best is None:
        return ent, "FARFROMPOLY_START"
    return best[2], best[3]


def classify_block(mapid, start, best_pt, bp, best_d, bentry, status, log):
    """Diagnose why the walk to a boss stalled and return (outcome). Distinguishes:
      - MESH GAP: best point is on-mesh but there's no navmesh connection to the
        (on-mesh) boss within a short (<120y) hop -> needs an off-mesh bridge.
      - ENTRANCE DISCONNECT: the walk never got past the start chamber (best point
        is still near the starting anchor) yet the boss is far -> the entrance
        chamber is meshed but severed from the interior (needs an entrance bridge).
      - CAP STALL: a genuinely long winding corridor exceeding the poly cap."""
    st2, _s2, poly2, planar2, _p2 = probe(mapid, best_pt, bp)
    gapd = dist(best_pt, bp)
    if poly2 <= 2 and gapd < 120.0:
        log.append(f"    boss {bentry}: MESH GAP -- {gapd:.0f}y from boss, no "
                   f"navmesh connection (poly={poly2}); needs off-mesh bridge")
        return "mesh_gap"
    if dist(best_pt, start) < 45.0 and gapd > 120.0:
        log.append(f"    boss {bentry}: ENTRANCE DISCONNECT -- walk never left the "
                   f"start chamber; interior severed from entrance ({gapd:.0f}y to "
                   f"boss); needs entrance off-mesh bridge")
        return "mesh_gap"
    log.append(f"    boss {bentry}: cap stall -- best {best_d:.0f}y from boss "
               f"(poly={poly2}, {status})")
    return "cap_stall"


def load_nav_links(mapid):
    """Verified DB traversal links for this map (playerbot_nav_links):
    [(id, A, B, radius, bidir)]. The runtime crosses these with a committed
    direct move; the generator threads its chain through them so the route
    follower delivers bots to the mouth."""
    rows = sql(ROUTE_DB,
        f"SELECT id, from_x, from_y, from_z, to_x, to_y, to_z, radius, bidirectional "
        f"FROM playerbot_nav_links WHERE verified=1 AND map_id={mapid}")
    out = []
    for r in rows:
        v = [float(x) for x in r[1:8]]
        out.append((int(r[0]), tuple(v[0:3]), tuple(v[3:6]), v[6], r[8] != "0"))
    return out

def link_hop_from(links, pt, used):
    """If `pt` is within a link mouth (radius + 4y slack for the stall point
    sitting a little short of the mouth), return (link_id, mouth, far)."""
    for lid, A, B, radius, bidir in links:
        ends = [(A, B)] + ([(B, A)] if bidir else [])
        for mouth, far in ends:
            if (lid, mouth) in used: continue
            if dist(pt, mouth) <= radius + 4.0:
                return lid, mouth, far
    return None

def link_detour(mapid, anchor, bp, links, used):
    """A link need not be anywhere near where the direct walk stalls — the
    mesh path toward the boss may dead-end in a different corridor entirely
    (WC: the floor route ends UNDER Serpentis' platform while the jump-split
    onto the ledge route is 165y away). Treat links as GRAPH EDGES instead:
    find one whose mouth is reachable on-mesh from `anchor` and whose far side
    leads onward toward the boss. Returns (lid, mouth, far, path_to_mouth) or
    None."""
    for lid, A, B, radius, bidir in links:
        ends = [(A, B)] + ([(B, A)] if bidir else [])
        for mouth, far in ends:
            if (lid, mouth) in used: continue
            st1, _s1, poly1, _pl1, path1 = probe(mapid, anchor, mouth)
            if st1 not in REACHED:
                continue                      # can't get to this mouth on-mesh
            st2, _s2, _poly2, _pl2, _p2 = probe(mapid, far, bp)
            if st2 not in REACHED and st2 not in WALKABLE:
                continue                      # far side leads nowhere
            return lid, mouth, far, path1
    return None

def walk_to_boss(mapid, anchor, bentry, bp, step, log, links=None):
    """Walk from `anchor` toward boss `bp`, dropping on-mesh crumbs. Returns
    (crumbs, new_anchor, outcome) where outcome is one of:
      'reached'      boss reachable within cap from the last crumb
      'boss_offmesh' boss coord is off-mesh; stopped at nearest on-mesh vertex
      'mesh_gap'     progress stalled at a tiny-poly disconnect (needs mesh fix)
      'cap_stall'    stalled hitting the poly cap with no clear disconnect
      'dead'         unroutable (ERR / no path)
    A stall at a verified traversal-link mouth hops the link (both endpoints
    become crumbs so the runtime route passes exactly through it) and the walk
    continues from the far side.
    """
    crumbs = []
    links = links or []
    used_links = set()
    start_anchor = anchor
    best_d = dist(anchor, bp)
    best_pt = anchor
    seen = set()

    def hop_link(at_pt):
        # Near-mouth fast path (stall right at a mouth), then the graph-edge
        # detour (mouth reachable on-mesh from the current anchor, far side
        # leads onward) — the general case: the direct walk usually dead-ends
        # in a different corridor than the one the link bridges.
        h = link_hop_from(links, at_pt, used_links)
        path_to_mouth = None
        if not h:
            d = link_detour(mapid, at_pt, bp, links, used_links)
            if not d: return False
            lid, mouth, far, path_to_mouth = d
        else:
            lid, mouth, far = h
        used_links.add((lid, mouth))
        if path_to_mouth:
            for c in resample(path_to_mouth, step):
                if not crumbs or dist(crumbs[-1], c) >= MIN_SPACE:
                    crumbs.append(c)
        for p in (mouth, far):
            if not crumbs or dist(crumbs[-1], p) >= 2.0:
                crumbs.append(p)
        log.append(f"    boss {bentry}: crossed nav-link {lid} at {tuple(round(v,1) for v in mouth)}")
        return far

    def unroutable(label):
        """No usable path from the current anchor. A verified traversal link
        at the anchor (or the best point reached so far) is the authored way
        onward -- hop and keep walking. Otherwise this is either a topology
        dead-end (diagnosed against the best point reached) or a total
        routing failure straight from the start."""
        far = hop_link(anchor) or hop_link(best_pt)
        if far:
            return "continue", far
        if crumbs or best_pt != anchor:
            outcome = classify_block(mapid, start_anchor, best_pt, bp, best_d,
                                     bentry, label, log)
            return "stop", (crumbs, best_pt, outcome)
        log.append(f"    boss {bentry}: unroutable from anchor ({label})")
        return "stop", (crumbs, anchor, "dead")

    for hop in range(GUARD):
        status, _s, poly, planar, path = probe(mapid, anchor, bp)
        if status in REACHED and path:
            # Emit the WHOLE resampled corridor, not just the final vertex --
            # discarding it here is what starved 129/144 maps of crumbs
            # (>200y consecutive-crumb gaps).
            crumbs.extend(resample(path, step))
            if status == "FARFROMPOLY_END" and planar and planar > 12.0:
                log.append(f"    boss {bentry}: reached nearest on-mesh pt, boss "
                           f"off-mesh by ~{planar:.0f}y")
                return crumbs, (crumbs[-1] if crumbs else anchor), "boss_offmesh"
            return crumbs, (crumbs[-1] if crumbs else anchor), "reached"
        if status in REACHED and not path:
            # SILENT-FAILURE GUARD: a REACHED status (most commonly
            # FARFROMPOLY_END) with an EMPTY parsed path is NOT actually a
            # reached boss -- findStraightPath produced nothing usable. The
            # old code treated this as success, which is exactly what
            # produced the 1-crumb Freehold/Atal'Dazar routes (every boss
            # "reached" instantly with zero progress). Treat it as unroutable
            # from this anchor instead.
            log.append(f"    boss {bentry}: status={status} but EMPTY path -- "
                       f"NOT actually reached (silent-failure guard)")
            kind, payload = unroutable(f"{status}(empty-path)")
            if kind == "continue":
                anchor = payload
                continue
            return payload
        if status not in WALKABLE or len(path) < 2:
            kind, payload = unroutable(status)
            if kind == "continue":
                anchor = payload
                continue
            return payload
        # Advance to the END of the returned (possibly winding) partial path,
        # emitting the FULL resampled corridor as crumbs along the way (not
        # just one step point) so no distance is silently discarded. Re-probe
        # from there; the pathfinder re-plans the next cap-limited segment
        # toward the boss.
        end = path[-1][:3]
        d_end = dist(end, bp)
        if d_end < best_d:
            best_d = d_end; best_pt = end
        # loop / revisit detection on a coarse grid: if we re-arrive at
        # essentially the same spot, the walk is oscillating -> stop and diagnose.
        key = (round(end[0]/8), round(end[1]/8), round(end[2]/8))
        stalled = (dist(end, anchor) < MIN_SPACE) or (key in seen)
        seen.add(key)
        if not stalled:
            for c in resample(path, step):
                if not crumbs or dist(crumbs[-1], c) >= MIN_SPACE:
                    crumbs.append(c)
            anchor = end
            continue
        # Stalled. A verified traversal link at the stall point (or at the
        # closest-to-boss point the walk reached) is the authored way onward.
        far = hop_link(anchor) or hop_link(best_pt) or hop_link(end)
        if far:
            anchor = far
            continue
        # Otherwise classify the blocker.
        outcome = classify_block(mapid, start_anchor, best_pt, bp, best_d,
                                 bentry, status, log)
        return crumbs, best_pt, outcome
    log.append(f"    boss {bentry}: hop guard exhausted (best {best_d:.0f}y from boss)")
    return crumbs, best_pt, "cap_stall"


def gen_chain(name, mapid, dungeon_id, bosses, log):
    """Build the ordered crumb chain. Returns (chain, stats)."""
    stats = {"bosses": 0, "reached": 0, "boss_offmesh": 0, "mesh_gap": 0,
             "cap_stall": 0, "dead": 0}
    ent = entrance(dungeon_id)
    if not ent:
        log.append(f"  [{name}] NO ENTRANCE (dungeonId {dungeon_id})")
        return [], stats
    bpos = boss_positions(mapid, bosses)
    ordered = [(b, bpos[b]) for b in bosses if b in bpos]
    stats["bosses"] = len(ordered)
    log.append(f"  [{name}] map={mapid} entrance={tuple(round(v,1) for v in ent)} "
               f"bosses_found={len(ordered)}/{len(bosses)}")
    if not ordered:
        return [], stats
    anchor, snap_st = snap_anchor(mapid, ent, ordered[0][1])
    if dist(anchor, ent) > 1.0:
        log.append(f"    anchor snapped {dist(anchor,ent):.0f}y off entrance "
                   f"-> {tuple(round(v,1) for v in anchor)} ({snap_st})")
    links = load_nav_links(mapid)
    link_pts = [p for _lid, A, B, _r, _b in links for p in (A, B)]
    def is_link_pt(p):
        return any(dist(p, lp) < 0.5 for lp in link_pts)
    chain = [anchor]
    good_anchor = anchor   # last anchor from which a boss was actually REACHED
    for bentry, bp in ordered:
        crumbs, new_anchor, outcome = walk_to_boss(mapid, good_anchor, bentry, bp,
                                                    STEP, log, links)
        for c in crumbs:
            # Link endpoints are exact traversal-link mouths -- they must
            # survive spacing dedup verbatim (the runtime hops mouth->far).
            if is_link_pt(c) or not chain or dist(chain[-1], c) >= MIN_SPACE:
                chain.append(c)
        stats[outcome] = stats.get(outcome, 0) + 1
        # Only carry the anchor forward when we truly reached the boss. On a
        # gap/stall/dead outcome, new_anchor is a dead-end pocket -- starting the
        # NEXT boss's walk from there strands every subsequent boss too. Keep the
        # last good anchor instead so later bosses still get a fair attempt.
        if outcome in ("reached", "boss_offmesh"):
            good_anchor = new_anchor
    # dedup / min-spacing (link endpoints exempt, same as above)
    out = []
    for p in chain:
        if is_link_pt(p) or not out or dist(out[-1], p) >= MIN_SPACE:
            out.append(p)
    out = densify_chain(mapid, out, links)
    return out, stats


def is_link_pair(links, a, b):
    """True when (a,b) is exactly a traversal-link crossing (either direction) —
    the runtime hops it with a committed direct move, so no on-mesh path exists
    or is needed between the two."""
    for _lid, A, B, _r, _bidir in (links or []):
        if (dist(a, A) < 0.5 and dist(b, B) < 0.5) or \
           (dist(a, B) < 0.5 and dist(b, A) < 0.5):
            return True
    return False

def _pair_followable(mapid, a, b, links=None):
    """The single followability predicate used everywhere a crumb pair is
    judged: a verified traversal-link crossing is always followable (no
    on-mesh path exists between its endpoints by design -- the runtime hops
    it with a committed direct move); otherwise the pair must probe a clean
    OK/NORMAL path within the poly cap AND within MAX_GAP yards straight-line.
    A pair with NO path at all (len(path) < 2) is NOT accepted -- that used to
    be silently treated as "fine" and is exactly how un-followable gaps
    reached the DB. Returns (good, status, poly, path)."""
    if is_link_pair(links, a, b):
        return True, "LINK", 0, []
    st, _s, poly, _pl, path = probe(mapid, a, b)
    good = st in ("OK", "NORMAL") and poly <= POLY_CAP and dist(a, b) <= MAX_GAP
    return good, st, poly, path


def _densify_pass(mapid, chain, links=None):
    """One splice pass: walk consecutive pairs, and for any pair that fails
    _pair_followable, probe it and splice in the resampled (~STEP-spaced,
    interpolated, MAX_GAP-bounded) corridor vertices between them. Returns
    (new_chain, changed)."""
    if len(chain) < 2:
        return chain, False
    out = [chain[0]]
    changed = False
    for i in range(1, len(chain)):
        a, b = chain[i-1], chain[i]
        good, st, poly, path = _pair_followable(mapid, a, b, links)
        if good:
            out.append(b)
            continue
        changed = True
        if len(path) >= 2:
            for c in resample(path, STEP):
                if dist(out[-1], c) >= MIN_SPACE and dist(c, b) >= MIN_SPACE:
                    out.append(c)
        # b still gets appended even when nothing could be spliced (e.g. no
        # path at all) -- densify can't manufacture a route where none
        # exists; validate_followability/the write-gate catch that below.
        if dist(out[-1], b) >= MIN_SPACE:
            out.append(b)
    return out, changed


def densify_chain(mapid, chain, links=None, max_passes=DENSIFY_PASSES):
    """Ensure every consecutive crumb pair is followable within the poly cap
    AND within MAX_GAP yards. Iterates _densify_pass until the chain stops
    changing or max_passes is hit -- a single splice pass can leave the newly
    inserted points still too far apart from their neighbours on very long
    boss-boundary jumps, so this must converge over multiple passes rather
    than assuming one pass is always enough."""
    cur = chain
    for _ in range(max_passes):
        nxt, changed = _densify_pass(mapid, cur, links)
        cur = nxt
        if not changed:
            break
    return cur


def validate_followability(mapid, chain, links=None):
    """Probe each consecutive crumb pair with the same distance-aware
    predicate densify_chain uses; require a clean OK path within the cap AND
    within MAX_GAP yards. A pair that IS a verified traversal link counts as
    followable (the runtime crosses it with a committed direct move -- no
    on-mesh path exists by design). Returns (ok_pairs, fail_pairs,
    [descriptions of the failing pairs])."""
    ok = fail = 0
    fails = []
    for i in range(1, len(chain)):
        a, b = chain[i-1], chain[i]
        good, st, poly, _path = _pair_followable(mapid, a, b, links)
        if good:
            ok += 1
        else:
            fail += 1
            if len(fails) < 6:
                fails.append(f"seq {i-1}->{i} d={dist(a,b):.0f}y {st} poly={poly}")
    return ok, fail, fails


def write_db(mapid, chain):
    """Overwrite this map's route with `chain`. Only called by main() AFTER
    validate_route_gate() has passed -- never delete existing rows for a
    chain that hasn't been validated (a failed/empty chain must leave prior
    rows in place, not wipe them)."""
    if not chain:
        return
    sql(ROUTE_DB, f"DELETE FROM playerbot_dungeon_routes WHERE map_id={mapid} AND difficulty=0")
    vals = ",".join(f"({mapid},0,{i},{p[0]:.3f},{p[1]:.3f},{p[2]:.3f})"
                    for i, p in enumerate(chain))
    sql(ROUTE_DB, "INSERT INTO playerbot_dungeon_routes "
        "(map_id,difficulty,seq,position_x,position_y,position_z) VALUES " + vals)


def gap_stats(chain):
    """Consecutive-crumb gap distances for a chain. Returns (gaps, worst)."""
    if len(chain) < 2:
        return [], 0.0
    gaps = [dist(chain[i-1], chain[i]) for i in range(1, len(chain))]
    return gaps, max(gaps)


def validate_route_gate(mapid, chain, stats, gaps, fok, ffail):
    """The write gate: a chain is publishable only if it covers the dungeon
    (spans the snapped entrance to at least the first reached boss), has at
    least 2 crumbs, has no consecutive gap over MAX_GAP, and every pair is
    followable. Returns (ok, [reasons])."""
    reasons = []
    if stats.get("bosses", 0) == 0:
        reasons.append("no bosses found in world DB")
    if len(chain) < 2:
        reasons.append(f"only {len(chain)} crumb(s)")
    reached_any = stats.get("reached", 0) + stats.get("boss_offmesh", 0)
    if stats.get("bosses", 0) > 0 and reached_any == 0:
        reasons.append("no boss reached -- coverage failure "
                       "(entrance does not span to any boss)")
    bad_gaps = [g for g in gaps if g > MAX_GAP]
    if bad_gaps:
        reasons.append(f"{len(bad_gaps)} gap(s) > {MAX_GAP:.0f}y (worst {max(bad_gaps):.0f}y)")
    if ffail > 0:
        reasons.append(f"{ffail} unfollowable pair(s)")
    return (len(reasons) == 0), reasons


def main():
    args = sys.argv[1:]
    report = "--report" in args
    use_dump = ("--dump" in args) or report
    only = set(int(x) for x in args if x.isdigit()) or None

    dungeons = load_dungeons_from_dump()
    if not dungeons:
        print("no dungeondump.txt -- run gen_dungeon_routes.py --all first")
        return
    if not report:
        print(f"  loaded {len(dungeons)} dungeons")

    print(f"=== ROUTE GENERATION (GEN_STEP={STEP:.0f}y GEN_MAX_GAP={MAX_GAP:.0f}y"
          f"{', REPORT ONLY -- NO DB WRITES' if report else ''}) ===")
    hdr = (f"{'dungeon':22} {'map':>5} {'boss':>7} {'crumbs':>6} "
           f"{'gap min/med/worst':>20} {'follow':>9}  outcome")
    print(hdr)
    print("-" * len(hdr))

    tot = {"reached": 0, "boss_offmesh": 0, "mesh_gap": 0, "cap_stall": 0,
           "dead": 0, "foll_ok": 0, "foll_fail": 0, "bosses": 0,
           "maps_ok": 0, "maps_fail": 0}
    any_fail = False
    try:
        for name, mapid, did, bosses in dungeons:
            if only and mapid not in only:
                continue
            log = []
            chain, st = gen_chain(name, mapid, did, bosses, log)
            links = load_nav_links(mapid)
            gaps, worst = gap_stats(chain)
            fok, ffail, fdesc = validate_followability(mapid, chain, links) if len(chain) >= 2 else (0, 0, [])
            ok, reasons = validate_route_gate(mapid, chain, st, gaps, fok, ffail)

            gmin = min(gaps) if gaps else 0.0
            gmed = statistics.median(gaps) if gaps else 0.0
            gaptxt = f"{gmin:5.1f}/{gmed:5.1f}/{worst:6.1f}"
            outcome = "OK" if ok else "FAIL: " + "; ".join(reasons)
            print(f"{name[:22]:22} {mapid:>5} {st['reached']+st['boss_offmesh']:>3}/{st['bosses']:<3} "
                  f"{len(chain):>6} {gaptxt:>20} {fok:>4}/{ffail:<4}  {outcome}")

            if not report:
                for line in log:
                    print(line)
            else:
                for line in log:
                    if "boss" in line and ("GAP" in line or "unroutable" in line
                                           or "stall" in line or "off-mesh" in line
                                           or "EMPTY path" in line):
                        print("      " + line.strip())
            for fd in fdesc:
                print(f"      FOLLOW-FAIL {fd}")

            for k in ("reached", "boss_offmesh", "mesh_gap", "cap_stall", "dead"):
                tot[k] += st.get(k, 0)
            tot["bosses"] += st["bosses"]
            tot["foll_ok"] += fok; tot["foll_fail"] += ffail

            if not ok:
                any_fail = True
                tot["maps_fail"] += 1
                print(f"  !!! FAIL [{name}] map={mapid}: {'; '.join(reasons)} "
                      f"-- leaving existing DB rows untouched")
            else:
                tot["maps_ok"] += 1
                if not report:
                    write_db(mapid, chain)
                    print(f"    -> wrote {len(chain)} waypoints")
    finally:
        close_batch_procs()

    print("-" * 60)
    print(f"TOTALS  bosses={tot['bosses']} reached={tot['reached']} "
          f"offmesh={tot['boss_offmesh']} mesh_gap={tot['mesh_gap']} "
          f"cap_stall={tot['cap_stall']} dead={tot['dead']}  "
          f"follow_ok={tot['foll_ok']} follow_fail={tot['foll_fail']}  "
          f"maps_ok={tot['maps_ok']} maps_fail={tot['maps_fail']}")
    if any_fail:
        print(f"!!! {tot['maps_fail']} map(s) FAILED validation -- DB left untouched for those maps")
        sys.exit(1)


if __name__ == "__main__":
    main()
