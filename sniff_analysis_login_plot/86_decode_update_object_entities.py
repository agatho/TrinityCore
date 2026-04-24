#!/usr/bin/env python3
"""Enumerate entities inside each post-LVW UPDATE_OBJECT in retail.

For each SMSG_UPDATE_OBJECT in the post-LVW window, list the update blocks
with their (updateType, highGuid, subType, counter). Tells us which bundle
contains which entity kind.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-56-30.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
SMSG_UPDATE_OBJECT = 0x580000


HG_NAMES = {
    0: 'Null', 1: 'Uniq', 2: 'Player', 3: 'Item', 4: 'GameObject', 5: 'Unit',
    6: 'Corpse', 7: 'DynamicObject', 8: 'AreaTrigger', 9: 'BattlePet',
    10: 'ActivePlayer', 11: 'Scenario', 12: 'Conversation', 13: 'AI_Group',
    14: 'Cast', 15: 'Vignette', 16: 'Guild', 17: 'Client',
    # Housing family via ObjectGuidFactory
    30: 'BNetAccount', 54: 'HG54', 55: 'Housing', 56: 'MeshObject', 57: 'Entity',
    58: 'HG58', 59: 'HG59', 60: 'HG60',
}

UT_NAMES = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT_OF_RANGE'}


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
    """Yield (ut, high, sub, lo, hi, offset) for every block found."""
    if len(body) < 4:
        return
    # UPDATE_OBJECT starts with: uint16 mapId, uint32 blockCount, then blocks
    # But block delimiters let us scan loosely.
    i = 0
    scans = 0
    while i < len(body) - 10 and scans < 1_000_000:
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
        # Sanity: high in plausible range
        if high == 0 or high > 60:
            i += 1; continue
        if ut in (1, 2):
            # CREATE must have an objectType byte next (0..20)
            if off < len(body) and body[off] <= 20:
                yield (ut, high, sub, lo, hi, i)
                i = off + 1
                continue
        else:
            yield (ut, high, sub, lo, hi, i)
        i += 1


def main():
    pkts = list(iter_packets(PATH))
    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No LVW')
        return

    first_h_cmsg = None
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if d == 'CMSG':
            grp = (op >> 16) & 0xFF
            if grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A):
                first_h_cmsg = idx
                break
    upper = first_h_cmsg if first_h_cmsg else len(pkts)

    print(f'Decoding UPDATE_OBJECT entities post-LVW in: {PATH}')
    print(f'LVW={lvw}, first-housing-CMSG={first_h_cmsg}, window={upper-lvw-1} pkts')
    print()

    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        print(f'=== UPDATE_OBJECT idx={idx} (LVW+{idx-lvw}) len={len(body)} ===')
        blocks = list(scan_blocks(body))
        # aggregate by (ut, high, sub)
        agg = {}
        samples = {}
        for ut, high, sub, lo, hi, off in blocks:
            key = (ut, high, sub)
            agg[key] = agg.get(key, 0) + 1
            if key not in samples:
                samples[key] = (lo, hi)

        if not blocks:
            print('  (no blocks decoded — possibly binary header-only or fragment)')
            print()
            continue

        for (ut, high, sub), count in sorted(agg.items()):
            hname = HG_NAMES.get(high, f'HG{high}')
            utname = UT_NAMES.get(ut, f'ut{ut}')
            extra = f'/sub{sub}' if high == 55 else ''
            lo, hi = samples[(ut, high, sub)]
            print(f'  {hname}{extra:6s}  {utname:10s}  count={count:3d}  sample GUID lo=0x{lo:016X} hi=0x{hi:016X}')
        print()


if __name__ == '__main__':
    main()
