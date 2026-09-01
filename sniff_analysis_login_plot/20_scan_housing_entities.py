#!/usr/bin/env python3
"""
Scan build 66838 sniff for UPDATE_OBJECT packets and look for packed Housing
high-guid markers to see whether retail creates per-plot Housing/3 proxy entities
for neighbour houses when the player enters the regular neighborhood map.
"""
import struct, sys, os, collections

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OPCODE_UPDATE_OBJECT = 0x00580000


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
            if not cands:
                return
            off = min(cands)
            continue
        header = data[off+4:off+4+25]
        conn, f2, f3, dlen = struct.unpack_from('<IIII', header, 0)
        ts = struct.unpack_from('<Q', header, 16)[0]
        if dlen > 50*1024*1024:
            off += 1
            continue
        payload_start = off + 4 + 25
        payload_end = payload_start + dlen
        if payload_end > len(data):
            return
        if dlen < 4:
            off = payload_end; idx += 1; continue
        opcode = struct.unpack_from('<I', data, payload_start)[0]
        body = data[payload_start+4:payload_end]
        yield idx, tag.decode(), opcode, body, ts, payload_start
        off = payload_end
        idx += 1


def main():
    print(f"Parsing {PKT}")
    pkts = list(iter_packets(PKT))
    print(f"Parsed {len(pkts)} packets\n")

    # Count by opcode for UPDATE_OBJECT
    upd_packets = [p for p in pkts if p[2] == OPCODE_UPDATE_OBJECT]
    print(f"SMSG_UPDATE_OBJECT count: {len(upd_packets)}")

    # Brute-force scan every UPDATE_OBJECT body at byte offsets looking for
    # a 16-byte sequence whose interpretation as (low, high) qwords has
    # high>>58 == 55 (HighGuid::Housing).
    targets = {
        'Housing/3 (House)':         3,
        'Housing/4 (Neighborhood)':  4,
        'Housing/2 (Room)':          2,
        'Housing/1 (Neighborhood?)': 1,
        'Housing/5 (Fixture)':       5,
    }

    seen = {k: collections.Counter() for k in targets}
    packet_count_with = {k: 0 for k in targets}

    for idx, direction, opcode, body, ts, payoff in upd_packets:
        found_in_this_packet = set()
        # scan aligned 8-byte windows - the packed form includes mask bytes so this
        # won't catch every encoding. As a weaker heuristic, scan for raw 16-byte
        # GUIDs that would appear if anything was written as full 16-byte blob.
        for off in range(0, len(body) - 16, 1):
            hi = struct.unpack_from('<Q', body, off+8)[0]
            lo = struct.unpack_from('<Q', body, off)[0]
            if (hi >> 58) == 55:
                subType = (hi >> 53) & 0x1F
                if subType in (1, 2, 3, 4, 5):
                    for k, v in targets.items():
                        if v == subType:
                            seen[k][(lo, hi)] += 1
                            found_in_this_packet.add(k)
                            break
        for k in found_in_this_packet:
            packet_count_with[k] += 1

    print()
    for k, cnt in seen.items():
        unique = len(cnt)
        print(f"  {k}: {unique} unique GUID pairs across {packet_count_with[k]} UPDATE_OBJECT packets")
        for i, ((lo, hi), n) in enumerate(sorted(cnt.items())[:15]):
            subType = (hi >> 53) & 0x1F
            print(f"    lo={lo:016x} hi={hi:016x}  subType={subType}  seen={n}x")


if __name__ == "__main__":
    main()
