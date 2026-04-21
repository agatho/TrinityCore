#!/usr/bin/env python3
"""
Scan the build 66838 sniff for ALL SMSG_VIGNETTE_UPDATE packets and dump their
size + first bytes. The goal: decide whether retail renders housing plot icons
through the vignette system (VIGNETTES_UPDATED fires every 4s per user's event
log) or through some other channel.
"""
import struct, os

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
# SMSG_VIGNETTE_UPDATE lives in opcode group 0x5F. Try a few candidates.
VIGNETTE_CANDIDATES = {
    0x005F0011: "SMSG_VIGNETTE_UPDATE(old-guess)",
    0x005F0000: "SMSG_VIGNETTE_0x00",
    0x005F0001: "SMSG_VIGNETTE_0x01",
    0x005F0005: "SMSG_VIGNETTE_0x05",
    0x005F0010: "SMSG_VIGNETTE_0x10",
    0x005F0013: "SMSG_VIGNETTE_0x13",
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


def main():
    all_group_5f = {}
    per_opcode_count = {}
    for idx, dir_, op, body in iter_packets(PKT):
        if (op & 0xFF0000) == 0x5F0000:
            per_opcode_count[op] = per_opcode_count.get(op, 0) + 1
            if op not in all_group_5f:
                all_group_5f[op] = (idx, dir_, len(body), body[:64].hex(' '))

    print(f"Group 0x5F opcodes observed in sniff:")
    for op in sorted(per_opcode_count):
        idx, dir_, blen, first = all_group_5f[op]
        print(f"  0x{op:08X}  count={per_opcode_count[op]:4}  first_idx={idx:5} first_size={blen:6}  first={first}")

    # Also print every 0x5F packet with its idx, size
    print()
    print("All 0x5F packets (idx, opcode, size):")
    for idx, dir_, op, body in iter_packets(PKT):
        if (op & 0xFF0000) == 0x5F0000:
            print(f"  idx={idx:5}  op=0x{op:08X}  size={len(body):6}")


if __name__ == "__main__":
    main()
