#!/usr/bin/env python3
"""Auto-generate dungeon route_waypoints from the navmesh.

For each dungeon: snap the LFG entrance to the mesh, then chain-pathfind
entrance -> boss1 -> boss2 -> ... (bosses in encounter order). Whenever a hop
exceeds the ~74-poly (~292y) path cap, drop a waypoint ~STEP yards along the
corridor (conservative, well under the cap) and re-path from there. Writes the
ordered chain to wc_world.playerbot_dungeon_routes for the runtime to follow.

Reuses the built mmap_probe (full straight-path dump). Offline: zero
world-thread pathfinding risk.
"""
import subprocess, re, sys, math

MMAP_DIR = "M:/WorldofWarcraft/mmaps"
PROBE    = "M:/PlayerbotServer/mmap_probe.exe"
MYSQL    = r"C:/Program Files/MySQL/MySQL Server 9.4/bin/mysql.exe"
STEP     = 200.0     # conservative waypoint spacing (< 292y cap)
MIN_SPACE= 25.0      # drop waypoints closer than this to the previous
GUARD    = 30        # max hops per boss leg (anti-infinite-loop)

# (name, map, dungeonId, [bosses in encounter order])
DUNGEONS = [
    ("Deadmines",       36,  6, [47162,47296,43778,47626,47739]),
    ("WailingCaverns",  43,  1, [3669,3671,3670,3674,3673,3654]),
    ("ShadowfangKeep",  33,  8, [46962,3887,4278,46963,46964]),
    ("BlackfathomDeeps",48, 10, [74446,74476,74565,74505,74518,74728,4829]),
    ("Stockade",        34, 12, [46383,46264,46254]),
    ("Gnomeregan",      90, 14, [7361,7079,6235,6000,6229,7800,6228]),
]

def sql(db, q):
    r = subprocess.run([MYSQL,"-uplayerbot","-pplayerbot",db,"-N","-e",q],
                       capture_output=True,text=True)
    return [ln.split("\t") for ln in r.stdout.strip().splitlines() if ln.strip()]

def boss_positions(mapid, bosses):
    """Return {entry:(x,y,z)} for spawned bosses on this map (nearest-to-origin spawn)."""
    ids = ",".join(str(b) for b in bosses)
    rows = sql("wc_world",
        f"SELECT c.id,c.position_x,c.position_y,c.position_z FROM creature c "
        f"WHERE c.id IN ({ids}) AND c.map={mapid}")
    pos = {}
    for r in rows:
        e = int(r[0]); p = (float(r[1]),float(r[2]),float(r[3]))
        pos.setdefault(e, p)   # first spawn wins
    return pos

def entrance(dungeon_id):
    rows = sql("wc_world",
        f"SELECT position_x,position_y,position_z FROM lfg_dungeon_template WHERE dungeonId={dungeon_id}")
    if not rows: return None
    return (float(rows[0][0]),float(rows[0][1]),float(rows[0][2]))

def probe(mapid, a, b):
    """Run mmap_probe a->b. Return (status, start_snap, full_path[list of (x,y,z)])."""
    r = subprocess.run([PROBE, MMAP_DIR, str(mapid),
                        f"{a[0]:.3f}",f"{a[1]:.3f}",f"{a[2]:.3f}",
                        f"{b[0]:.3f}",f"{b[1]:.3f}",f"{b[2]:.3f}"],
                       capture_output=True,text=True)
    out = r.stdout
    status = "ERR"
    m = re.search(r"STATUS:\s+([A-Z_]+)", out)
    if m: status = m.group(1)
    start = None
    m = re.search(r"START:.*-> TC \(([-\d.]+),([-\d.]+),([-\d.]+)\)", out)
    if m: start = (float(m.group(1)),float(m.group(2)),float(m.group(3)))
    path = []
    for m in re.finditer(r"\[\s*\d+\]\s+\S*\s*TC=\(([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\)", out):
        path.append((float(m.group(1)),float(m.group(2)),float(m.group(3))))
    return status, start, path

def dist(a,b):
    return math.sqrt((a[0]-b[0])**2+(a[1]-b[1])**2+(a[2]-b[2])**2)

def point_along(path, target_len):
    """Return the actual ON-MESH straight-path VERTEX at ~target_len yards.
    findStraightPath vertices sit on the navmesh surface; we must NOT interpolate
    between them — a linear interp across a ramp segment (big dz) lands OFF the
    surface, so the runtime move_to to it stalls (WC seq2 landed at z-70 on a
    z-85..-104 corridor). Pick the first real vertex at/after target_len instead;
    spacing stays comfortably under the 292y cap because a single straight run
    rarely exceeds it, and if it does the vertex is still strictly reachable."""
    if len(path) < 2: return path[-1] if path else None
    acc = 0.0
    for i in range(1, len(path)):
        acc += dist(path[i-1], path[i])
        if acc >= target_len:
            return path[i]   # on-mesh vertex
    return path[-1]

def gen_chain(name, mapid, dungeon_id, bosses):
    ent = entrance(dungeon_id)
    if not ent:
        print(f"  [{name}] NO ENTRANCE (dungeonId {dungeon_id})"); return []
    bpos = boss_positions(mapid, bosses)
    ordered = [(b,bpos[b]) for b in bosses if b in bpos]
    print(f"  [{name}] map={mapid} entrance={tuple(round(v,1) for v in ent)} "
          f"bosses_found={len(ordered)}/{len(bosses)}")
    if not ordered: return []
    # snap the entrance onto the mesh via the first probe's START snap
    st,snap,_ = probe(mapid, ent, ordered[0][1])
    anchor = snap if snap else ent
    chain = []
    for bentry, bp in ordered:
        leg = []
        reached = False
        for _ in range(GUARD):
            status, _s, path = probe(mapid, anchor, bp)
            if status in ("OK","FARFROMPOLY_END","NORMAL"):   # boss reachable from anchor
                # include the ON-MESH point at/near the boss so the chain stays
                # continuous across boss boundaries (no gap for the next leg).
                leg.append(path[-1] if path else bp)
                reached = True
                break
            if status not in ("SHORT","PARTIAL") or len(path) < 2:
                print(f"    boss {bentry}: unroutable from anchor ({status})")
                break
            wp = point_along(path, STEP)
            if wp is None or dist(wp, anchor) < MIN_SPACE:
                wp = path[-1]   # fall back to the truncation point
                if dist(wp, anchor) < 10.0:
                    print(f"    boss {bentry}: stalled (no forward progress)"); break
            leg.append(wp); anchor = wp
        chain.extend(leg)
        # continue from the last on-mesh chain point (NOT the raw, maybe off-mesh boss)
        if leg:
            anchor = leg[-1]
    # dedup / min-spacing
    out = []
    for p in chain:
        if not out or dist(out[-1], p) >= MIN_SPACE:
            out.append(p)
    print(f"    -> {len(out)} waypoints")
    return out

def write_db(mapid, chain):
    sql("wc_world", f"DELETE FROM playerbot_dungeon_routes WHERE map_id={mapid} AND difficulty=0")
    if not chain: return
    vals = ",".join(f"({mapid},0,{i},{p[0]:.3f},{p[1]:.3f},{p[2]:.3f})" for i,p in enumerate(chain))
    sql("wc_world", f"INSERT INTO playerbot_dungeon_routes (map_id,difficulty,seq,position_x,position_y,position_z) VALUES {vals}")

def main():
    only = set(int(x) for x in sys.argv[1:]) if len(sys.argv)>1 else None
    print("=== generating dungeon route waypoints ===")
    for name, mapid, did, bosses in DUNGEONS:
        if only and mapid not in only: continue
        chain = gen_chain(name, mapid, did, bosses)
        write_db(mapid, chain)
    print("=== done ===")
    for r in sql("wc_world","SELECT map_id,COUNT(*) FROM playerbot_dungeon_routes GROUP BY map_id ORDER BY map_id"):
        print(f"  map {r[0]}: {r[1]} waypoints")

if __name__ == "__main__":
    main()
