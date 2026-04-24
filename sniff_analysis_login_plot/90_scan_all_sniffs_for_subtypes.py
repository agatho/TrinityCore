#!/usr/bin/env python3
"""Scan ALL retail sniffs for Housing subTypes across the entire PKT (not
just login window). Identifies where and when each subtype appears.
"""
import struct
import sys
import os
import glob

SMSG_UPDATE_OBJECT = 0x580000
HIGH_GUID_HOUSING = 55


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    if not cands:
        return
    off = min(cands); idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx:
                return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def scan_blocks(body):
    """Loose scanner — may have false positives. Returns (ut, sub) tuples."""
    if len(body) < 4:
        return
    i = 0
    scans = 0
    while i < len(body) - 10 and scans < 500_000:
        scans += 1
        ut = body[i]
        if ut not in (0, 1, 2, 3):
            i += 1; continue
        if i + 3 > len(body):
            break
        m_lo, m_hi = body[i+1], body[i+2]
        off = i + 3
        lo = hi = 0
        ok = True
        for b in range(8):
            if m_lo & (1 << b):
                if off >= len(body):
                    ok = False; break
                lo |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        for b in range(8):
            if m_hi & (1 << b):
                if off >= len(body):
                    ok = False; break
                hi |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        if hi == 0 and lo == 0:
            i += 1; continue
        high = (hi >> 58) & 0x3F
        sub = (hi >> 53) & 0x1F
        if high != HIGH_GUID_HOUSING:
            i += 1; continue
        # Be strict: only CREATE with valid typeByte
        if ut in (1, 2):
            if off < len(body) and body[off] <= 20:
                yield (ut, sub, lo, hi, i)
                i = off + 1
                continue
        else:
            # For VALUES, require the GUID to be a 'clean' Housing GUID
            # (e.g. counter not insanely large)
            if lo < 0x00FFFFFFFFFFFFFF:  # reject GUIDs with too-big counter
                yield (ut, sub, lo, hi, i)
        i += 1


def main():
    sniffs = glob.glob(r'C:/sniff/*/dumps/*.pkt')
    print(f'Scanning {len(sniffs)} retail sniffs for Housing/sub5 appearances\n')

    for pkt in sorted(sniffs):
        sub_counts = {}  # sub -> (create, values, ut3)
        for idx, d, op, body in iter_packets(pkt):
            if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
                continue
            for ut, sub, lo, hi, pos in scan_blocks(body):
                if sub not in sub_counts:
                    sub_counts[sub] = [0, 0, 0]
                if ut in (1, 2):
                    sub_counts[sub][0] += 1
                elif ut == 0:
                    sub_counts[sub][1] += 1
                else:
                    sub_counts[sub][2] += 1
        # Show only if sub5 present anywhere
        if 5 in sub_counts:
            name = os.path.basename(pkt)
            print(f'{name}:')
            for s in sorted(sub_counts.keys()):
                c, v, o = sub_counts[s]
                if c + v + o > 0:
                    print(f'  sub{s:2d}  CREATE={c:4d}  VALUES={v:4d}  OUT={o:4d}')
            print()


if __name__ == '__main__':
    main()
