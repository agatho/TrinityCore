#!/usr/bin/env python3
"""List every CMSG (housing or not) sent by the client after LVW up to N
packets ahead. Tells us what the client asks the server for at login and
what retail's SMSG reactive sequence looks like.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
SCAN_AHEAD = 1500


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


def is_housing_op(op):
    grp = (op >> 16) & 0xFF
    # Verified against memory/HOUSING_SYSTEM_REFERENCE:
    # CMSG housing groups: 0x2E, 0x30-33, 0x35, 0x37-39
    # SMSG housing groups: 0x50-54, 0x56, 0x58-5A
    return grp in (0x2E, 0x30, 0x31, 0x32, 0x33, 0x35, 0x37, 0x38, 0x39,
                   0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x58, 0x59, 0x5A, 0x5C, 0x5F)


def main():
    pkts = list(iter_packets(PATH))
    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No LVW'); return
    print(f'LVW at idx {lvw}. Listing CMSGs (and housing-related SMSG responses) in next {SCAN_AHEAD} pkts:')

    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= lvw + SCAN_AHEAD: break
        if d == 'CMSG':
            offset = idx - lvw
            tag = '[HOUSING]' if is_housing_op(op) else '         '
            print(f'  CMSG idx={idx:5d} LVW+{offset:4d}  op=0x{op:08X}  len={len(body):4d}  {tag}')
        elif d == 'SMSG' and (is_housing_op(op) or op == 0x580000):
            offset = idx - lvw
            tag = '[UPDATE_OBJECT]' if op == 0x580000 else '[HOUSING RSP]'
            print(f'  SMSG idx={idx:5d} LVW+{offset:4d}  op=0x{op:08X}  len={len(body):4d}  {tag}')


if __name__ == '__main__':
    main()
