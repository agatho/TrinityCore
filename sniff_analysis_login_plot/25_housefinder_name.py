#!/usr/bin/env python3
"""
Dump every SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO_RESPONSE / DETAIL_RESPONSE
packet in the retail sniff and extract the Name strings the client sees, so
we can tell whether retail sends resolved names ("Free Flame Hill") or the
raw ID triplet ("91-6-4") inside JamCliHouseFinderNeighborhood.
"""
import struct, os, re

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

# SMSG opcodes (group 0x55 = HousingSystem? 0x53 = HousingServices? Let me try both)
TARGET_OPCODES = {
    0x00530001: "HOUSE_FINDER_INFO_RESPONSE",
    0x00530002: "HOUSE_FINDER_NBRHD_RESPONSE",
    0x00530003: "HOUSE_FINDER_NBRHD_RESPONSE?",
    0x00550000: "HOUSE_STATUS_RESPONSE",
    0x00550008: "QUERY_NEIGHBORHOOD_NAME_RESPONSE",
    0x00460012: "QUERY_NEIGHBORHOOD_NAME_RESPONSE(alt)",
    0x005C0012: "NEIGHBORHOOD_GET_ROSTER_RESPONSE",
}


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    first_tag = min(x for x in (data.find(b'SMSG', 0, 256), data.find(b'CMSG', 0, 256)) if x > 0)
    off = first_tag
    idx = 0
    while off + 4 + 25 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            next_s = data.find(b'SMSG', off + 1)
            next_c = data.find(b'CMSG', off + 1)
            cands = [x for x in (next_s, next_c) if x > 0]
            if not cands: return
            off = min(cands); continue
        header = data[off+4:off+4+25]
        conn, f2, f3, dlen = struct.unpack_from('<IIII', header, 0)
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        payload_start = off + 4 + 25
        payload_end = payload_start + dlen
        if payload_end > len(data): return
        opcode = struct.unpack_from('<I', data, payload_start)[0]
        body = data[payload_start+4:payload_end]
        yield idx, tag.decode(), opcode, body
        off = payload_end
        idx += 1


# Extract printable substrings 3+ chars long
def extract_strings(body, min_len=3):
    out = []
    i = 0
    while i < len(body):
        if 32 <= body[i] < 127:
            j = i
            while j < len(body) and 32 <= body[j] < 127:
                j += 1
            if j - i >= min_len:
                out.append((i, body[i:j].decode('ascii', errors='replace')))
            i = j
        else:
            i += 1
    return out


def main():
    # First: find every unique opcode in groups 0x53, 0x55, 0x5C, 0x46 to see what's actually used
    by_group = {}
    for idx, dir_, op, body in iter_packets(PKT):
        if dir_ != 'SMSG': continue
        grp = (op >> 16) & 0xFF
        if grp not in (0x53, 0x55, 0x5C, 0x46): continue
        by_group.setdefault(op, []).append((idx, body))

    print("SMSG opcodes in groups 0x46, 0x53, 0x55, 0x5C (counts):")
    for op in sorted(by_group):
        samples = by_group[op]
        print(f"  0x{op:08X}  count={len(samples):4}  first_idx={samples[0][0]} first_size={len(samples[0][1])}")

    # Dump extracted strings for a few sample packets of each opcode
    print()
    print("Sample bodies + extracted printable strings per opcode:")
    for op in sorted(by_group):
        samples = by_group[op]
        print(f"\n=== 0x{op:08X} ({len(samples)} pkts) ===")
        for idx, body in samples[:2]:
            strs = extract_strings(body, 3)
            print(f"  idx={idx} size={len(body)} strings={[s for _, s in strs[:6]]}")
            if len(body) < 200:
                print(f"    body hex: {body.hex(' ')}")


if __name__ == "__main__":
    main()
