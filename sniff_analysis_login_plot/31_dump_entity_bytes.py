#!/usr/bin/env python3
"""Dump raw bytes of the first few HighGuid::Entity CREATEs from retail
idx 9984 so we can see what a 'map-proxy' Entity looks like on the wire."""
import struct

RETAIL = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"


def iter_packets(path):
    data = open(path, 'rb').read()
    off = min(x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0)
    idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4: off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def read_packed_guid(body, off):
    if off+2 > len(body): return None, off
    m_lo, m_hi = body[off], body[off+1]
    off += 2
    lo = hi = 0
    for b in range(8):
        if m_lo & (1<<b):
            if off >= len(body): return None, off
            lo |= body[off] << (b*8); off += 1
    for b in range(8):
        if m_hi & (1<<b):
            if off >= len(body): return None, off
            hi |= body[off] << (b*8); off += 1
    return (lo, hi), off


for idx, dir_, op, body in iter_packets(RETAIL):
    if op != 0x00580000 or idx != 9984: continue
    print(f"idx={idx} size={len(body)}")
    # Find all HighGuid==57 (Entity) CREATEs, dump 140 bytes each
    seen = set()
    samples = []
    for o in range(len(body) - 32):
        ut = body[o]
        if ut not in (1, 2): continue
        guid, end = read_packed_guid(body, o+1)
        if guid is None: continue
        lo, hi = guid
        if hi < (1<<56): continue
        if (hi >> 58) != 57: continue  # Only HighGuid::Entity
        if end >= len(body): continue
        ot = body[end]
        if (lo, hi) in seen: continue
        seen.add((lo, hi))
        samples.append((o, ut, lo, hi, end, ot))
    # Group by objectType: show first 2 of each unique objectType
    by_ot = {}
    for s in samples:
        by_ot.setdefault(s[5], []).append(s)
    for ot, group in sorted(by_ot.items()):
        print(f"\n{'='*72}\nHighGuid::Entity entities with objectType={ot} (0x{ot:02X}): {len(group)} total, showing 2\n{'='*72}")
        for o, ut, lo, hi, end, _ in group[:2]:
            print(f"\n  offset=0x{o:X}  lo={lo:016x} hi={hi:016x}")
            dump = body[o:o+140]
            for i in range(0, len(dump), 16):
                row = dump[i:i+16]
                hex_part = ' '.join(f'{b:02x}' for b in row)
                print(f"      +{i:3}: {hex_part}")
    break
