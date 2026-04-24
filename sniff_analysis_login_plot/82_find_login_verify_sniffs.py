#!/usr/bin/env python3
"""Scan every sniff PKT file and report which ones contain SMSG_LOGIN_VERIFY_WORLD
(opcode 0x42002F = 4325423) — those are pristine login captures.
"""
import struct
import sys
import os
import glob

SMSG_LOGIN_VERIFY_WORLD = 0x42002F


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
        yield idx, tag.decode(), op
        off = pe; idx += 1


def scan(path):
    lvw_idx = None
    pkt_count = 0
    for idx, d, op in iter_packets(path):
        pkt_count = idx + 1
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            if lvw_idx is None:
                lvw_idx = idx
        if idx > 50 and lvw_idx is None:
            # login verify comes very early; if we haven't seen it in first 50 pkts
            # it's definitely mid-session
            pass
    return lvw_idx, pkt_count


def main():
    roots = [
        '/c/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps',
        '/c/sniff',
    ]
    # Windows-style paths from WSL git-bash
    roots = [r.replace('/c/', 'C:/') for r in roots]

    all_pkts = []
    for root in roots:
        if not os.path.isdir(root):
            continue
        for pkt in glob.glob(os.path.join(root, '**', '*.pkt'), recursive=True):
            all_pkts.append(pkt)

    print(f'Scanning {len(all_pkts)} PKT files for SMSG_LOGIN_VERIFY_WORLD (0x42002F)...')
    print()

    hits = []
    misses = []
    for pkt in all_pkts:
        try:
            lvw_idx, total = scan(pkt)
        except Exception as e:
            print(f'  error: {pkt}: {e}')
            continue
        if lvw_idx is not None:
            hits.append((pkt, lvw_idx, total))
        else:
            misses.append((pkt, total))

    print(f'HITS — contain SMSG_LOGIN_VERIFY_WORLD ({len(hits)}):')
    for pkt, lvw_idx, total in sorted(hits, key=lambda t: t[1]):
        mark = '  ⭐ PRISTINE' if lvw_idx <= 3 else ''
        print(f'  idx={lvw_idx:5d}  total={total:6d}  {pkt}{mark}')

    print()
    print(f'MISSES — no login-verify found ({len(misses)}):')
    for pkt, total in sorted(misses):
        print(f'  total={total:6d}  {pkt}')


if __name__ == '__main__':
    main()
