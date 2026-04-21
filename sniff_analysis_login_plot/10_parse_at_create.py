"""Parse AT CREATE (SMSG 0x005A0002) and GO CREATE bodies from the exterior plot flow.

Also decode the large UPDATE_OBJECT at idx 9984 to find the player/player-related create,
and the BIG UPDATE_OBJECT at idx 10367/10378 which carries AreaTriggers + GameObjects.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
import importlib.util

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")
opc6   = _load("opc6",   "06_opcodes.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_at_and_go_create.txt")

# The 005A0002 packets at idx 10369..10376 (size ~76-84) look like per-AT data.
# Body layout guess (one struct per packet): bitmask byte + GUID128 + position + other fields.

INTERESTING_IDXS = [10369, 10370, 10371, 10372, 10373, 10374, 10375, 10376]
GO_RESPONSE_IDXS = [10398, 10399, 10400, 10401, 10402]

def decode_packed_guid128(b, off):
    """WoW PackedGUID128: 2-byte LE mask covering 16 bytes. Each set bit = 1 stored byte (little-endian).
    Returns (lo64, hi64, new_off).
    """
    if off + 2 > len(b):
        return None
    mask = struct.unpack_from('<H', b, off)[0]
    off += 2
    guid_bytes = bytearray(16)
    for i in range(16):
        if mask & (1 << i):
            if off >= len(b):
                return None
            guid_bytes[i] = b[off]
            off += 1
    lo = struct.unpack_from('<Q', guid_bytes, 0)[0]
    hi = struct.unpack_from('<Q', guid_bytes, 8)[0]
    return lo, hi, off

def fmt_guid(lo, hi):
    return f"0x{hi:016X}:{lo:016X}"

def parse_high_guid(hi):
    """Extract high part details from 12.x GUID encoding."""
    # 12.x format: top bits = type, then realm, then subtype/entry
    return f"hi=0x{hi:016X}"

def analyze_at(idx, body):
    out = [f"--- idx {idx} SMSG_0x005A0002 (AT data?) size={len(body)} ---"]
    out.append(f"  raw ({len(body)}): {body.hex(' ', 4)}")
    # First byte(s) = packed guid mask
    g = decode_packed_guid128(body, 0)
    if g:
        lo, hi, off = g
        out.append(f"  PackedGUID128: {fmt_guid(lo, hi)}  bytesUsed={off}")
        rest = body[off:]
        out.append(f"  remaining({len(rest)}): {rest.hex(' ', 4)}")
    return "\n".join(out)

def analyze_go_query_response(idx, body):
    """SMSG 0x00460007 = QUERY_GAMEOBJECT_RESPONSE (guess).
    Layout: 4-byte entry + 1 byte hasData + data_len + { entry, type, displayId, name*4, iconName,
    castBarCaption, unk, data[34], size, questItems, unkUInt32s }
    """
    out = [f"--- idx {idx} SMSG_0x00460007 (QUERY_GAMEOBJECT_RESPONSE) size={len(body)} ---"]
    if len(body) < 8:
        out.append(f"  body too short: {body.hex(' ')}")
        return "\n".join(out)
    entry = struct.unpack_from('<I', body, 0)[0]
    out.append(f"  entry = {entry}")
    # Find name strings — usually 4 names followed by additional strings
    # Try to find printable ASCII runs
    strings = []
    i = 4
    while i < len(body):
        if 32 <= body[i] < 127:
            start = i
            while i < len(body) and 32 <= body[i] < 127:
                i += 1
            if i - start >= 3:
                strings.append((start, body[start:i].decode('latin-1', errors='replace')))
        i += 1
    out.append(f"  strings: {strings[:10]}")
    return "\n".join(out)

def main():
    packets = list(parse5.iter_packets(PATH))
    by_idx = {p['idx']: p for p in packets}

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== AT & GO CREATE ANALYSIS (exterior plot flow) ===\n\n")

        w.write("## 1. Candidate AT 'data' packets (SMSG 0x005A0002)\n\n")
        for idx in INTERESTING_IDXS:
            p = by_idx.get(idx)
            if not p:
                continue
            w.write(analyze_at(idx, p['body']) + "\n\n")

        w.write("## 2. GameObject query responses (SMSG 0x00460007)\n\n")
        for idx in GO_RESPONSE_IDXS:
            p = by_idx.get(idx)
            if not p:
                continue
            w.write(analyze_go_query_response(idx, p['body']) + "\n\n")

        # Also dump the idx 10367 / 10378 UPDATE_OBJECTs head — these are the biggies
        w.write("## 3. Large UPDATE_OBJECTs (exterior map objects)\n\n")
        for idx in [9984, 10260, 10301, 10367, 10378, 10403]:
            p = by_idx.get(idx)
            if not p:
                continue
            b = p['body']
            w.write(f"--- idx {idx} SMSG_0x{p['opcode']:08X} size={p['size']} ---\n")
            w.write(f"  first 128 bytes: {b[:128].hex(' ', 4)}\n")
            # Count entries: objectsCount at offset 6 (after mapId+packCount?)
            # parsed.txt in other sniffs shows layout: uint32 numObjects_maybe
            if len(b) >= 8:
                # bytes 0..1 mapId, bytes 2..3 zeros, bytes 4..5 numObjs_LE (from existing parse patterns)
                map_id = struct.unpack_from('<H', b, 0)[0]
                num_objs = struct.unpack_from('<H', b, 4)[0]
                hi = struct.unpack_from('<I', b, 6)[0]
                w.write(f"  mapId=0x{map_id:04X}  numObjs=0x{num_objs:04X} ({num_objs}) hi={hi}\n")
            w.write("\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
