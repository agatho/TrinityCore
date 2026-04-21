#!/usr/bin/env python3
"""Extract CMSG 0x005A005D packets from the retail sniff and analyze."""
import os
import struct
import sys

def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    if not cands:
        return
    off = min(cands)
    idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx:
                return
            off = min(nx)
            continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1
            continue
        ps = off+29
        pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, dlen, ps, data
        off = pe
        idx += 1


TARGET = 0x005A005D
SNIFF = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

def main():
    all_packets = list(iter_packets(SNIFF))
    print(f"Total packets: {len(all_packets)}")

    # Find all occurrences of target
    target_idx = []
    for rec in all_packets:
        idx, direction, op, dlen, ps, data = rec
        if op == TARGET:
            target_idx.append((idx, direction, op, dlen, ps))

    print(f"Target opcode 0x{TARGET:08X} occurrences: {len(target_idx)}")
    if not target_idx:
        return

    # Analyze payload sizes
    sizes = [t[3] for t in target_idx]
    print(f"Payload size distribution: min={min(sizes)} max={max(sizes)} unique={set(sizes)}")

    # Show first N payloads (first 32 bytes after opcode)
    print("\nFirst 10 payloads (hex after 4-byte opcode):")
    for i, (idx, direction, op, dlen, ps) in enumerate(target_idx[:10]):
        body = all_packets[idx][5][ps+4:ps+dlen]
        hexv = body[:32].hex()
        print(f"  #{i} idx={idx} dir={direction} size={dlen} body[0:32]={hexv}")

    # Show last few
    print("\nLast 5 payloads:")
    for i, (idx, direction, op, dlen, ps) in enumerate(target_idx[-5:]):
        body = all_packets[idx][5][ps+4:ps+dlen]
        hexv = body[:32].hex()
        print(f"  idx={idx} dir={direction} size={dlen} body[0:32]={hexv}")

    # Find distribution between occurrences (gap in packet index)
    gaps = []
    for i in range(1, len(target_idx)):
        gaps.append(target_idx[i][0] - target_idx[i-1][0])
    if gaps:
        print(f"\nPacket-index gaps between 0x5D occurrences: min={min(gaps)} max={max(gaps)} avg={sum(gaps)/len(gaps):.1f}")
        # Show histogram of gaps
        from collections import Counter
        c = Counter(gaps)
        print("  Top gap counts:", c.most_common(10))

    # Look at what SMSG comes right after each CMSG 0x5D
    print("\nPackets within 5 indices after each CMSG 0x5D (first 10 instances):")
    op_names_seen = {}
    for i, (idx, direction, op, dlen, ps) in enumerate(target_idx[:15]):
        nexts = []
        for j in range(1, 6):
            if idx + j < len(all_packets):
                ni, nd, nop, nd_len, nps, _ = all_packets[idx + j]
                nexts.append(f"{nd}:0x{nop:08X}({nd_len})")
        print(f"  idx={idx}: {' -> '.join(nexts)}")

    # Count all opcodes immediately after 0x5D
    from collections import Counter
    next_op = Counter()
    for (idx, direction, op, dlen, ps) in target_idx:
        if idx+1 < len(all_packets):
            ni, nd, nop, _, _, _ = all_packets[idx+1]
            next_op[(nd, nop)] += 1
    print("\nOpcode directly after 0x5D (top 10):")
    for (nd, nop), c in next_op.most_common(10):
        print(f"  {c:4} {nd}:0x{nop:08X}")

    # Count overall opcodes in sniff (direction) for comparison
    from collections import Counter
    all_op = Counter()
    for rec in all_packets:
        idx, d, op, dl, _, _ = rec
        all_op[(d, op)] += 1
    print(f"\nTotal unique opcodes: {len(all_op)}")
    print("Top 20 CMSGs by freq:")
    for (d, op), c in [(k,v) for k,v in all_op.most_common() if k[0]=='CMSG'][:20]:
        group = (op>>16) & 0xFFFF
        sub = op & 0xFFFF
        print(f"  {c:5}  0x{op:08X}  group=0x{group:02X} sub=0x{sub:02X}")

if __name__ == '__main__':
    main()
