#!/usr/bin/env python3
"""For both OUR (22:31) and RETAIL sniffs, extract the UPDATE_OBJECT packet
that contains the large initial Housing CREATE bundle and dump the first
N bytes around each Housing/3 CREATE block so we can compare wire layout.

Heuristic: scan for occurrences of 0x01 or 0x02 followed by a PackedGuid128
whose hi decodes to HighGuid==55 (Housing) and subType==3. For each hit,
dump 80 bytes starting at the updateType byte.
"""
import struct

OUR = r"C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt"
RETAIL = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OP_UPDATE_OBJECT = 0x00580000


def iter_packets(path):
    data = open(path, 'rb').read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    off = min(cands); idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        body = data[ps+4:pe]
        yield idx, tag.decode(), op, body
        off = pe; idx += 1


def read_packed_guid(body, off):
    if off + 2 > len(body): return None, off, 0
    m_lo, m_hi = body[off], body[off+1]
    start = off; off += 2
    lo = 0; hi = 0
    for b in range(8):
        if m_lo & (1<<b):
            if off >= len(body): return None, off, 0
            lo |= body[off] << (b*8); off += 1
    for b in range(8):
        if m_hi & (1<<b):
            if off >= len(body): return None, off, 0
            hi |= body[off] << (b*8); off += 1
    return (lo, hi), off, off - start


def scan(path, label, target_idx):
    print(f"\n{'='*72}\n{label}: {path}\n{'='*72}")
    for idx, dir_, op, body in iter_packets(path):
        if op != OP_UPDATE_OBJECT:
            continue
        if idx != target_idx:
            continue
        print(f"idx={idx} size={len(body)} — scanning for Housing/3 CREATEs")
        hits = []
        for o in range(0, len(body) - 32):
            ut = body[o]
            if ut not in (1, 2):
                continue
            guid, end, consumed = read_packed_guid(body, o+1)
            if guid is None:
                continue
            lo, hi = guid
            if (hi >> 58) != 55:
                continue
            sub = (hi >> 53) & 0x1F
            if sub != 3:
                continue
            # objectType byte is immediately after the PackedGuid ends
            if end >= len(body):
                continue
            obj_type = body[end]
            hits.append((o, ut, lo, hi, end, obj_type, consumed))
        seen = set()
        for o, ut, lo, hi, end, obj_type, consumed in hits:
            key = (lo, hi)
            if key in seen: continue
            seen.add(key)
            print(f"\n  offset=0x{o:X} ({o})  updateType={ut}  "
                  f"PackedGuid consumed={consumed}B  objectType=0x{obj_type:02X} ({obj_type})")
            print(f"    Housing-3 hi=0x{hi:016X} lo=0x{lo:016X}")
            # Dump 100 bytes starting at 'o'
            dump = body[o:o+100]
            hex_str = ' '.join(f'{b:02x}' for b in dump)
            # Split in rows of 16 bytes
            for i in range(0, len(dump), 16):
                row = dump[i:i+16]
                hex_part = ' '.join(f'{b:02x}' for b in row)
                print(f"      +{i:3}: {hex_part}")
            # Annotate start positions
            guid_bytes = consumed
            print(f"    ^ byte 0 = updateType  ({ut})")
            print(f"    ^ bytes 1..{guid_bytes} = PackedGuid (mask 2B + {guid_bytes-2}B data)")
            print(f"    ^ byte {1+guid_bytes} = objectType (0x{obj_type:02X} = {obj_type})")


if __name__ == '__main__':
    # OUR 22:31 — idx 295 is big bundle
    scan(OUR, 'OUR 22:31', 295)
    # RETAIL — idx 9984
    scan(RETAIL, 'RETAIL', 9984)
