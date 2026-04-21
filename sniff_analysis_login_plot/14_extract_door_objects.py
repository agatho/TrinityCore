"""Extract and compare the Interior 'Front Door' (entry 575017) vs
Exterior 'Front Door' (entry 586576) object CREATE blocks from UPDATE_OBJECT.

Goal: identify position, orientation, FileDataID, parent/attach relations,
and compare with our server's spawn logic.

Based on WoW 12.x object CREATE layout inside SMSG_UPDATE_OBJECT:
  { uint16 mapId, uint16 unused, uint16 numObjects, uint16 hi_count_or_flag, ... }
  per object:
    uint8  updateType (0=VALUES, 1=CREATE_OBJECT, 2=CREATE_OBJECT2, 3=OUT_OF_RANGE, ...)
    PackedGUID128 guid
    uint8  objectTypeId (3=Unit, 5=GameObject, etc.)
    movement/create data (variable)
    UpdateMask + UpdateFields
We don't fully decode — we extract the hex around each door GUID.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_door_objects.txt")

def find_entries_in_body(body, entry_u32):
    needle = struct.pack('<I', entry_u32)
    hits = []
    start = 0
    while True:
        pos = body.find(needle, start)
        if pos < 0: break
        hits.append(pos)
        start = pos + 4
    return hits

def scan_floats(body, start, count):
    res = []
    for i in range(count):
        o = start + i*4
        if o + 4 <= len(body):
            f = struct.unpack_from('<f', body, o)[0]
            res.append(f)
        else:
            res.append(None)
    return res

def dump_area_around(body, pos, before=80, after=200):
    s = max(0, pos - before)
    e = min(len(body), pos + after)
    return s, body[s:e]

def parse_packed_guid_back(body, guid_end_limit):
    """Scan backward from a known entry position to find the most-recent PackedGUID128 mask word.
    In the CREATE block the layout is roughly:
        ...  <packed_guid128_mask>  <packed_guid128_bytes>  <obj_type=3>  <flags/movement> ... entry(u32) ...
    Heuristically we look for 0xFFEF / 0xFFEE / 0xEFFF / 0x7FEF / 0x3F... masks close before the entry.
    """
    best = None
    for off in range(guid_end_limit - 32, guid_end_limit - 2):
        if off < 0 or off + 2 > len(body): continue
        m = struct.unpack_from('<H', body, off)[0]
        # count bits
        bits = bin(m).count('1')
        # plausible masks for 128-bit packed guids in housing: often 0xFFEF (15 bytes), 0xFFFF
        if bits in (14, 15, 16) and 0xF000 <= m <= 0xFFFF:
            # try parse
            pos = off + 2
            guid = bytearray(16)
            ok = True
            for i in range(16):
                if m & (1 << i):
                    if pos >= len(body):
                        ok = False; break
                    guid[i] = body[pos]; pos += 1
            if ok:
                lo = struct.unpack_from('<Q', guid, 0)[0]
                hi = struct.unpack_from('<Q', guid, 8)[0]
                best = (off, m, lo, hi, pos)
    return best

def main():
    packets = list(parse5.iter_packets(PATH))
    out = []

    # Interior door entry 575017, exterior 586576
    # The sample context from the previous run ended with 24 07 00 00 — looks like standard
    # GameObject CREATE VALUES block.
    # Let's isolate the block in the largest interior UPDATE (idx 6063) and exterior (idx 10367).

    for idx_search, entry, label in [
        (6063, 575017, "INTERIOR Front Door"),
        (6382, 575017, "INTERIOR Front Door (VALUES_UPDATE?)"),
        (10367, 586576, "EXTERIOR Front Door"),
        (10741, 586576, "EXTERIOR Front Door (re-update)"),
    ]:
        p = next((p for p in packets if p['idx'] == idx_search), None)
        if not p: continue
        body = p['body']
        hits = find_entries_in_body(body, entry)
        out.append(f"\n=== {label} @ idx {idx_search} SMSG_UPDATE_OBJECT size={p['size']} ===")
        out.append(f"  entry {entry} found at offsets: {hits}")

        for pos in hits[:1]:
            s, blob = dump_area_around(body, pos, before=96, after=256)
            out.append(f"  bytes [{s}..{s+len(blob)}] (entry u32 at rel offset {pos-s}):")
            # Print in 16-byte chunks
            for k in range(0, len(blob), 16):
                row = blob[k:k+16]
                out.append(f"    {s+k:04x}: {row.hex(' ')} | {''.join(chr(c) if 32<=c<127 else '.' for c in row)}")

            # Try backward scan for the enclosing PackedGUID128 (the GO's GUID)
            guid_scan = parse_packed_guid_back(body, pos)
            if guid_scan:
                off, mask, lo, hi, guid_end = guid_scan
                out.append(f"  PackedGUID128 found at offset {off}: mask=0x{mask:04X} lo=0x{lo:016X} hi=0x{hi:016X}")

            # Scan for plausible 3D positions (floats in typical WoW exterior range: X~[−5000..5000], Z~[0..300])
            out.append(f"  Plausible floats (x,y,z) triples near entry:")
            for k in range(max(0, pos-96), min(len(body)-12, pos+200), 4):
                f1 = struct.unpack_from('<f', body, k)[0]
                f2 = struct.unpack_from('<f', body, k+4)[0]
                f3 = struct.unpack_from('<f', body, k+8)[0]
                def isnum(f):
                    return f == f and abs(f) < 1e6 and (abs(f) > 0.01 or f == 0)
                if isnum(f1) and isnum(f2) and isnum(f3) and (-6000 < f1 < 6000) and (-6000 < f2 < 6000) and (-200 < f3 < 400):
                    if (abs(f1) > 0.1 or abs(f2) > 0.1) and (abs(f1) + abs(f2) > 10):
                        out.append(f"    off={k:5d}: ({f1:>10.3f}, {f2:>10.3f}, {f3:>10.3f})")

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== Door Object Extraction ===\n")
        for l in out:
            w.write(l + "\n")
    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
