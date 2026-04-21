#!/usr/bin/env python3
"""Scan retail idx 9984 and our idx 295 for ALL HighGuid types present in
CREATE blocks. Focus on whether HighGuid::Entity (57) shows up."""
import struct

OUR = r"C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt"
RETAIL = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

HIGH_GUID_NAMES = {
    0: 'Null', 1: 'Uniq', 2: 'Player', 3: 'Item', 6: 'GameObject',
    7: 'Creature', 11: 'AreaTrigger', 14: 'MeshObject',
    40: 'BNetAccount', 55: 'Housing', 56: 'MeshObject', 57: 'Entity',
}


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


def scan_all_creates(path, label, target_idx):
    print(f"\n{'='*72}\n{label}: target idx {target_idx}\n{'='*72}")
    for idx, dir_, op, body in iter_packets(path):
        if op != 0x00580000 or idx != target_idx:
            continue
        by_high = {}
        seen = set()
        for o in range(0, len(body) - 16):
            ut = body[o]
            if ut not in (1, 2):
                continue
            guid, end = read_packed_guid(body, o+1)
            if guid is None:
                continue
            lo, hi = guid
            # Ensure something meaningful — reject all-zero
            if lo == 0 and hi == 0:
                continue
            # HighGuid is bits 58-63 of hi
            if hi < (1 << 56):
                continue
            high = hi >> 58
            if high > 63:
                continue
            # objectType byte after guid
            if end >= len(body):
                continue
            obj_type = body[end]
            key = (lo, hi)
            if key in seen:
                continue
            seen.add(key)
            by_high.setdefault(high, []).append((o, obj_type, lo, hi))
        print(f"CREATE blocks decoded: {len(seen)}")
        for high in sorted(by_high):
            entries = by_high[high]
            name = HIGH_GUID_NAMES.get(high, f'Unknown-{high}')
            print(f"\nHighGuid={high} ({name}): {len(entries)} entities")
            ot_hist = {}
            for o, ot, lo, hi in entries:
                ot_hist[ot] = ot_hist.get(ot, 0) + 1
            print(f"  objectType histogram: {sorted(ot_hist.items())}")
            # show first few
            for o, ot, lo, hi in entries[:4]:
                print(f"    offset=0x{o:X} objType=0x{ot:02X} lo={lo:016x} hi={hi:016x}")
        break


if __name__ == '__main__':
    scan_all_creates(OUR, 'OUR 22:31', 295)
    scan_all_creates(RETAIL, 'RETAIL', 9984)
