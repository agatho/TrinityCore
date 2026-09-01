#!/usr/bin/env python3
"""
Deep scan: find all CREATE blocks inside SMSG_UPDATE_OBJECT (0x00580000)
that carry either FHousingPlayerHouse_C (23) or FNeighborhoodMirrorData_C (30)
fragment. Count and list the distinct GUIDs.

This tells us:
  - How many Housing/3 entities retail actually creates on the client
  - Whether retail uses proxy Housing/3 entities for other players' houses
  - How many Housing/4 mirror entities exist
"""
import struct, sys, os, collections

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OP_UPDATE_OBJECT = 0x00580000

FRAG_FHOUSINGPLAYERHOUSE = 23
FRAG_FNEIGHBORHOODMIRROR = 30


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
            off = off + 1; continue
        payload_start = off + 4 + 25
        payload_end = payload_start + dlen
        if payload_end > len(data): return
        opcode = struct.unpack_from('<I', data, payload_start)[0]
        body = data[payload_start+4:payload_end]
        yield idx, tag.decode(), opcode, body
        off = payload_end
        idx += 1


def main():
    pkts = list(iter_packets(PKT))
    upds = [p for p in pkts if p[2] == OP_UPDATE_OBJECT]
    print(f"SMSG_UPDATE_OBJECT packets: {len(upds)}")
    print()

    # Scan every UPDATE_OBJECT body for the byte pattern:
    #   any byte sequence where we find the fragment id byte 23 or 30 preceded by
    #   plausible CREATE context. Since the fragment ID list is a list of bytes in
    #   ascending order, we look for any occurrence of the byte 23 or 30.
    # For a lower-noise approach: find every 16-byte window whose HIGH qword bits
    # 58-63 are exactly 55 (HighGuid::Housing) and subType is in {3,4}. Then for
    # each such packet, report how many distinct such GUIDs appear.

    mirror_guids = collections.Counter()
    house_guids = collections.Counter()

    for idx, direction, opcode, body in upds:
        packet_mirror = set()
        packet_house = set()
        # Scan byte-aligned 16-byte windows
        step = 1
        for off in range(0, len(body) - 16, step):
            hi = struct.unpack_from('<Q', body, off+8)[0]
            lo = struct.unpack_from('<Q', body, off)[0]
            if (hi >> 58) == 55:
                sub = (hi >> 53) & 0x1F
                if sub == 3:
                    packet_house.add((lo, hi))
                elif sub == 4:
                    packet_mirror.add((lo, hi))
        for g in packet_house:
            house_guids[g] += 1
        for g in packet_mirror:
            mirror_guids[g] += 1

    # Filter noise: keep only GUIDs that APPEAR VALIDATED. Real Housing/3
    # GUIDs (subType=3 format): (55<<58)|(3<<53)|(arg1&0x3F)<<15|(arg2&0x7FFF)
    # So mask: high & ~((0x3F<<15)|0x7FFF) must equal 0xDC60000000000000
    MASK_SUB3_CLEAR = ~((0x3F << 15) | 0x7FFF) & ((1<<64)-1)
    EXPECT_SUB3     = (55 << 58) | (3 << 53)
    # For subType=3 Housing, Low is arg3 (counter/bnet id) which can be any
    # plausible small integer. We add the extra filter: 0 <= Low < 2^32 to
    # reduce random 16-byte windows that look like GUIDs.

    real_house = {(lo, hi): n for (lo, hi), n in house_guids.items()
                  if (hi & MASK_SUB3_CLEAR) == EXPECT_SUB3 and lo < (1 << 40)}
    real_mirror = mirror_guids  # subType=4 check already accurate

    print(f"Filtered real Housing/3 GUIDs: {len(real_house)}")
    for (lo, hi), n in sorted(real_house.items(), key=lambda x: -x[1])[:30]:
        arg1 = (hi >> 15) & 0x3F
        arg2 = hi & 0x7FFF
        print(f"  Housing-3-{arg1}-{arg2}-{lo}  occurs_in_{n}_packets")

    print()
    print(f"Filtered real Housing/4 (mirror) GUIDs: {len(real_mirror)}")
    for (lo, hi), n in sorted(real_mirror.items(), key=lambda x: -x[1])[:30]:
        sub = (hi >> 53) & 0x1F
        arg1 = (hi >> 15) & 0x3F
        arg2 = hi & 0x7FFF
        print(f"  Housing-{sub}-{arg1}-{arg2}-{lo}  occurs_in_{n}_packets")


if __name__ == "__main__":
    main()
