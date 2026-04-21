#!/usr/bin/env python3
"""
Scan retail sniff for CREATE-type update blocks inside SMSG_UPDATE_OBJECT
and extract the GUID written as the block header. A CREATE block begins
with `uint8 updateType (1=CREATE_OBJECT, 2=CREATE_OBJECT2)` followed by a
PackedGuid128.

Count how many distinct GUIDs of each HighGuid::Housing subtype appear —
especially subtype 3 (HousingPlayerHouse). If retail sends one CREATE per
occupied plot in the neighborhood, that proves the per-plot proxy entity
theory. If not, it's a workaround.
"""
import struct, os

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OP_UPDATE_OBJECT = 0x00580000


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


def read_packed_guid(body, off):
    """TC packed GUID: 2 mask bytes, then data bytes for each set mask bit."""
    if off + 2 > len(body):
        return None, off
    mask_lo = body[off]
    mask_hi = body[off + 1]
    off += 2
    lo = 0
    hi = 0
    for b in range(8):
        if mask_lo & (1 << b):
            if off >= len(body): return None, off
            lo |= body[off] << (b * 8)
            off += 1
    for b in range(8):
        if mask_hi & (1 << b):
            if off >= len(body): return None, off
            hi |= body[off] << (b * 8)
            off += 1
    return (lo, hi), off


def housing_fmt(lo, hi):
    subType = (hi >> 53) & 0x1F
    if subType in (1, 4, 5):
        a1 = (hi >> 32) & 0xFFFF
        a2 = hi & 0xFFFFFFFF
        return f"Housing-{subType}-{a1}-{a2}-{lo}"
    elif subType == 2:
        a2 = hi & 0xFFFFFFFF
        return f"Housing-{subType}-{a2}-{lo:x}"
    elif subType == 3:
        arg2 = hi & 0x7FFF
        arg1 = (hi >> 15) & 0x3F
        return f"Housing-{subType}-{arg2}-{lo}-{arg1}"
    else:
        return f"Housing-?-{hi:x}-{lo:x}"


def main():
    # Map opcode name -> tag mapping for UPDATE_OBJECT: sniff uses 0x00580000.
    # UPDATE_OBJECT body wire layout (TC compatible):
    #   uint16 mapId
    #   uint32 blockCount  (OR: varint-packed?)
    #   bit flag
    #   ...
    # We don't fully parse the bit-packed header. Instead, scan the body for
    # every byte position where an updateType 1 or 2 appears followed by a
    # plausible PackedGuid — if the decoded high qword has bits 58-63 == 55
    # (HighGuid::Housing), count it.

    per_subtype = {1: set(), 2: set(), 3: set(), 4: set(), 5: set()}
    per_packet_hits = []

    upd_count = 0
    for idx, direction, opcode, body in iter_packets(PKT):
        if opcode != OP_UPDATE_OBJECT:
            continue
        upd_count += 1
        packet_subtype_guids = {1: set(), 2: set(), 3: set(), 4: set(), 5: set()}
        # Scan every byte position. For each, try: updateType (1 or 2), then
        # optionally objectType byte, then PackedGuid128. Only accept if the
        # decoded GUID's HighGuid type bits == 55 (Housing).
        for o in range(1, len(body) - 32):
            ut = body[o - 1]
            if ut not in (1, 2):
                continue
            # Try reading PackedGuid starting at o
            guid, _end = read_packed_guid(body, o)
            if guid is None:
                continue
            lo, hi = guid
            if (hi >> 58) != 55:
                continue
            sub = (hi >> 53) & 0x1F
            if sub in per_subtype:
                per_subtype[sub].add((lo, hi))
                packet_subtype_guids[sub].add((lo, hi))
        if any(packet_subtype_guids.values()):
            per_packet_hits.append((idx, {s: len(v) for s, v in packet_subtype_guids.items() if v}))

    print(f"Total SMSG_UPDATE_OBJECT packets: {upd_count}")
    print()
    for sub in sorted(per_subtype):
        guids = per_subtype[sub]
        print(f"HighGuid::Housing subtype {sub}: {len(guids)} distinct GUIDs")
        for (lo, hi) in sorted(guids)[:20]:
            print(f"  {housing_fmt(lo, hi)}  raw_lo={lo:016x} raw_hi={hi:016x}")
        print()

    print(f"UPDATE_OBJECT packets containing any Housing-typed GUID (CREATE-framed): {len(per_packet_hits)}")
    print("(idx, per-subtype unique count):")
    for idx, counts in per_packet_hits[:40]:
        print(f"  idx={idx:5}  {counts}")


if __name__ == "__main__":
    main()
