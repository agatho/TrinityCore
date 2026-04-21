"""Find all GameObjects and MeshObjects named 'door' / 'entrance' on the EXTERIOR map
and inspect their position / parent / attachment relationships.

Focus: the user-reported mismatch — indoor door position aligns with its mesh,
but exterior door GO lands slightly off vs the visible door mesh.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_door_go_vs_mesh.txt")

def decode_packed_guid128(b, off):
    if off + 2 > len(b): return None
    mask = struct.unpack_from('<H', b, off)[0]
    off += 2
    guid = bytearray(16)
    for i in range(16):
        if mask & (1 << i):
            if off >= len(b): return None
            guid[i] = b[off]; off += 1
    lo = struct.unpack_from('<Q', guid, 0)[0]
    hi = struct.unpack_from('<Q', guid, 8)[0]
    return lo, hi, off

def strings_in(body, min_len=3):
    out = []
    i = 0
    while i < len(body):
        if 32 <= body[i] < 127:
            s = i
            while i < len(body) and 32 <= body[i] < 127:
                i += 1
            if i - s >= min_len:
                out.append((s, body[s:i].decode('latin-1', errors='replace')))
        i += 1
    return out

def main():
    packets = list(parse5.iter_packets(PATH))
    out_lines = []

    # 1) Collect all GameObject query responses that mention 'door' or 'entrance' or 'gate' or 'arch'.
    out_lines.append("## 1. Door-like GameObjects (SMSG_QUERY_GAME_OBJECT_RESPONSE 0x00460007)\n")
    door_entries = {}
    for p in packets:
        if p['opcode'] != 0x00460007: continue
        body = p['body']
        if len(body) < 8: continue
        entry = struct.unpack_from('<I', body, 0)[0]
        strs = strings_in(body, 3)
        joined = " | ".join(s for _, s in strs[:5]).lower()
        if any(k in joined for k in ('door', 'entrance', 'gate', 'arch', 'portal', 'frame')):
            door_entries[entry] = [s for _, s in strs[:5]]
            out_lines.append(f"  idx {p['idx']:5d} entry={entry:6d} strings={strs[:5]}")
    out_lines.append("")

    # 2) Collect all CREATE blocks where the GameObject entry matches a door-like entry.
    # Search within big UPDATE_OBJECT bodies for each door entry u32.
    out_lines.append("## 2. Any UPDATE_OBJECT bodies that contain a door-entry u32\n")
    for p in packets:
        if p['opcode'] != 0x00580000: continue
        body = p['body']
        for entry in door_entries:
            needle = struct.pack('<I', entry)
            off = body.find(needle)
            if off > 0 and off < 4096:
                # Context around the hit
                snippet = body[max(0, off-32):off+48]
                out_lines.append(f"  idx {p['idx']:5d} size={p['size']:>6d} entry={entry} found at offset {off}")
                out_lines.append(f"    context: {snippet.hex(' ', 4)}")
    out_lines.append("")

    # 3) Look at the fixture data inside big UPDATE_OBJECT — search for the house fixture type markers.
    # From HOUSING_SYSTEMS_ARCHITECTURE.md: Door fixture type = 11 inside HousingFixtureType.
    # In sniffs, cornerstone entry 457142 has been verified; let's look for the per-plot house
    # mesh tree and identify 'FileDataID' that corresponds to the door mesh.
    # The door model is commonly FileDataID 7118912 (horde) or similar (alliance).
    out_lines.append("## 3. Search for known door FileDataIDs in large UPDATE_OBJECTs\n")
    door_fdids = [7118912,  # Horde door (from CLAUDE.md)
                   7118906, 6648685, 7460531, 7118901, 7462686, 7118918,  # other fixtures
                   # common alliance door fdids (unknown exact — check any matches)
                   ]
    for fdid in door_fdids:
        needle = struct.pack('<I', fdid)
        total = 0
        hits_lines = []
        for p in packets:
            if p['opcode'] != 0x00580000: continue
            body = p['body']
            start = 0
            while True:
                pos = body.find(needle, start)
                if pos < 0: break
                total += 1
                if len(hits_lines) < 10:
                    ctx = body[max(0, pos-24):pos+36]
                    hits_lines.append(f"    idx {p['idx']:5d} pos={pos} ctx={ctx.hex(' ', 4)}")
                start = pos + 4
        if total:
            out_lines.append(f"  FileDataID {fdid}: {total} hits")
            out_lines.extend(hits_lines[:10])
    out_lines.append("")

    # 4) Find all SMSG_QUERY_GAME_OBJECT_RESPONSE on the exterior map and tabulate entries
    out_lines.append("## 4. All GO entries queried on exterior (idx >= 9964)\n")
    for p in packets:
        if p['opcode'] != 0x00460007: continue
        if p['idx'] < 9964: continue
        body = p['body']
        if len(body) < 8: continue
        entry = struct.unpack_from('<I', body, 0)[0]
        strs = strings_in(body, 3)
        names = [s for _, s in strs[:3]]
        out_lines.append(f"  idx {p['idx']:5d} entry={entry:6d} names={names}")
    out_lines.append("")

    # 5) Look for CMSG_QUERY_GAME_OBJECT (0x3A014A) — client asking about entries.
    out_lines.append("## 5. CMSG_QUERY_GAME_OBJECT (0x3A014A) on exterior\n")
    for p in packets:
        if p['opcode'] != 0x003A014A: continue
        if p['idx'] < 9964: continue
        body = p['body']
        if len(body) < 4: continue
        entry = struct.unpack_from('<I', body, 0)[0]
        out_lines.append(f"  idx {p['idx']:5d} size={p['size']} entry={entry} body={body.hex(' ')}")
    out_lines.append("")

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== Door GO vs Entrance Mesh analysis ===\n\n")
        for line in out_lines:
            w.write(line + "\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
