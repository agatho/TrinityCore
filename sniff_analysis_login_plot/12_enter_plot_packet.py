"""Extract the full body of the one SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT (0x5C0000)
plus several packets around it for timing context, and fully decode the PackedGUID128.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_enter_plot_packet.txt")

def decode_packed_guid128(b, off):
    if off + 2 > len(b):
        return None
    mask = struct.unpack_from('<H', b, off)[0]
    off += 2
    guid = bytearray(16)
    for i in range(16):
        if mask & (1 << i):
            if off >= len(b):
                return None
            guid[i] = b[off]
            off += 1
    lo = struct.unpack_from('<Q', guid, 0)[0]
    hi = struct.unpack_from('<Q', guid, 8)[0]
    return lo, hi, off, mask

def main():
    packets = list(parse5.iter_packets(PATH))
    by_idx = {p['idx']: p for p in packets}

    ep_packets = [p for p in packets if p['opcode'] == 0x5C0000]

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write(f"SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT: {len(ep_packets)} occurrence(s)\n\n")
        for p in ep_packets:
            w.write(f"=== idx {p['idx']} size={p['size']} ===\n")
            b = p['body']
            w.write(f"  raw ({len(b)}): {b.hex(' ')}\n")
            g = decode_packed_guid128(b, 0)
            if g:
                lo, hi, off, mask = g
                w.write(f"  PackedGUID128: hi=0x{hi:016X} lo=0x{lo:016X} mask=0x{mask:04X} guidBytes={off}\n")
                # Decode as Housing/Neighborhood GUID
                # Per CLAUDE.md: subType=4 = Neighborhood
                # WoW 12.x GUID encoding: hi[63:58]=type, hi[57:42]=realm, etc.
                type_bits = (hi >> 58) & 0x3F
                realm_id = (hi >> 42) & 0x1FFF
                w.write(f"    decoded: highType={type_bits:#x} realmId={realm_id}\n")
                rest = b[off:]
                w.write(f"  remaining ({len(rest)}): {rest.hex(' ')}\n")
                if len(rest) >= 4:
                    u32 = struct.unpack_from('<I', rest, 0)[0]
                    w.write(f"    remaining[0..4] as uint32 = {u32} (0x{u32:08X})\n")
            w.write("\n")

        # Dump ~10 packets before and after the first ENTER_PLOT for context
        if ep_packets:
            center = ep_packets[0]['idx']
            w.write(f"\n=== Context around idx {center} (-20 .. +20) ===\n")
            first_ts = packets[0]['ts']
            for p in packets:
                if center - 20 <= p['idx'] <= center + 20:
                    dt = (p['ts'] - first_ts)
                    w.write(f"[{p['idx']:5d}] +{dt:>14d}us {p['dir']} 0x{p['opcode']:08X} size={p['size']:>6d} body[0:24]={p['body'][:24].hex(' ')}\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
