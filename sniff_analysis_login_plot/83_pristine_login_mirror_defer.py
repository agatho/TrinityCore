#!/usr/bin/env python3
"""Pristine login mirror-emission analysis.

Uses a PKT that contains SMSG_LOGIN_VERIFY_WORLD as the login anchor. Walks
forward from LVW until the first housing CMSG from the client — any housing
SMSG in that window is server-initiated.

Answers:
  1. Does retail send Housing/4 mirror CREATE(s) in the unprompted login phase?
  2. If so, how many, and at which packet-offsets from LVW?
  3. Are there VALUES_UPDATEs on the mirror before the first housing CMSG?
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-56-30.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
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
    """Yield (updateType, high, sub) for update-object blocks."""
    if len(body) < 4:
        return
    i = 0
    scans = 0
    while i < len(body) - 10 and scans < 200000:
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
        if ut in (1, 2):
            if off < len(body) and body[off] <= 20:
                yield (ut, high, sub)
                i = off + 1
                continue
        else:
            yield (ut, high, sub)
        i += 1


def main():
    print(f'Pristine login analysis: {PATH}')
    pkts = list(iter_packets(PATH))
    print(f'Total packets: {len(pkts)}')

    # 1. Find SMSG_LOGIN_VERIFY_WORLD
    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No SMSG_LOGIN_VERIFY_WORLD found — aborting.')
        return
    print(f'LOGIN_VERIFY_WORLD at idx {lvw}')

    # 2. Find first housing CMSG after LVW
    first_housing_cmsg = None
    for idx, d, op, body in pkts:
        if idx <= lvw:
            continue
        if d != 'CMSG':
            continue
        grp = (op >> 16) & 0xFF
        if grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A):
            first_housing_cmsg = (idx, op)
            break
    if first_housing_cmsg:
        print(f'First housing CMSG after LVW: idx={first_housing_cmsg[0]} opcode=0x{first_housing_cmsg[1]:06X} '
              f'(gap={first_housing_cmsg[0] - lvw} pkts)')
    else:
        print(f'No housing CMSG found after LVW — scanning full post-LVW range')

    upper_bound = first_housing_cmsg[0] if first_housing_cmsg else len(pkts)

    # 3. In the (LVW, first_housing_cmsg) window, find all Housing entity emissions
    housing_emissions = []  # (idx, [(ut, sub)])
    for idx, d, op, body in pkts:
        if idx <= lvw:
            continue
        if idx >= upper_bound:
            break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        blocks = [(ut, sub) for (ut, high, sub) in scan_blocks(body) if high == HIGH_GUID_HOUSING]
        if blocks:
            housing_emissions.append((idx, blocks))

    print(f'\nHousing UPDATE_OBJECT SMSGs in unprompted post-LVW window (idx {lvw+1}..{upper_bound-1}):')
    print(f'  Count: {len(housing_emissions)}')
    ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}

    sub4_create_count = 0
    sub4_values_count = 0
    sub3_create_count = 0
    sub3_values_count = 0
    for idx, blocks in housing_emissions:
        counts = {}
        for ut, sub in blocks:
            counts[(ut, sub)] = counts.get((ut, sub), 0) + 1
        parts = [f'sub{sub}/{ut_name.get(ut, ut)}={n}' for (ut, sub), n in sorted(counts.items())]
        offset = idx - lvw
        print(f'  idx={idx:5d}  (LVW+{offset:4d})  {", ".join(parts)}')

        for (ut, sub), n in counts.items():
            if sub == 4 and ut in (1, 2):
                sub4_create_count += n
            elif sub == 4 and ut == 0:
                sub4_values_count += n
            elif sub == 3 and ut in (1, 2):
                sub3_create_count += n
            elif sub == 3 and ut == 0:
                sub3_values_count += n

    print(f'\nSummary for unprompted post-LVW window:')
    print(f'  Housing/4 mirror CREATEs: {sub4_create_count}')
    print(f'  Housing/4 mirror VALUES:  {sub4_values_count}')
    print(f'  Housing/3 proxy CREATEs:  {sub3_create_count}')
    print(f'  Housing/3 proxy VALUES:   {sub3_values_count}')


if __name__ == '__main__':
    main()
