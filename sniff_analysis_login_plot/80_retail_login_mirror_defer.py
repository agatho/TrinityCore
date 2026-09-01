#!/usr/bin/env python3
"""Answer the analysis-agent question: does retail send any unprompted
Housing/4 mirror CREATE or VALUES_UPDATE at ~500ms after login?

Strategy:
  1. Walk the PKT file
  2. Identify the login window — from start to the first CMSG from the client
  3. Within that window, scan every SMSG_UPDATE_OBJECT for blocks referencing
     Housing/4 (mirror) or Housing/3 (PlayerHouseEntity proxies)
  4. List them ordered by packet index so we can see if a "second CREATE"
     appears after the bulk initial bundle

HighGuid::Housing = 22 in MoP+. Verified via dump_12.0.1.66838 analysis earlier.
The subType field tells us which housing entity kind:
  1 = Housing decor GUID (FHousingDecor_C)
  2 = Room GUID (Housing/2)
  3 = HousingPlayerHouse (FHousingPlayerHouse_C entity)
  4 = NeighborhoodMirror (FNeighborhoodMirrorData_C entity)
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'

HIGH_GUID_HOUSING = 55   # Verified in 72_entity_inventory_at_login.py
SMSG_UPDATE_OBJECT = 0x580000


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
        # 29-byte header: [tag:4][dir?:4][conn?:4][ts?:8][dlen:4 at +12]
        # ts might be at +4 or +16; try both
        dlen = struct.unpack_from('<I', h, 12)[0]
        ts_guess_a = struct.unpack_from('<Q', h, 4)[0] if len(h) >= 12 else 0
        ts_guess_b = struct.unpack_from('<Q', h, 16)[0] if len(h) >= 24 else 0
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, ts_guess_a, ts_guess_b, data[ps+4:pe]
        off = pe; idx += 1


def decode_guid(hi):
    if hi == 0:
        return 0, 0
    high = (hi >> 58) & 0x3F
    sub = (hi >> 53) & 0x1F
    return high, sub


def scan_update_object(body):
    """Return list of (updateType, subType, lo, hi) for Housing high guids."""
    out = []
    if len(body) < 4:
        return out
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

        high, sub = decode_guid(hi)
        if high == HIGH_GUID_HOUSING:
            # For CREATE types, only accept if followed by objectTypeId 0..20
            if ut in (1, 2):
                if off < len(body) and body[off] <= 20:
                    out.append((ut, sub, lo, hi))
                    i = off + 1
                    continue
            else:
                out.append((ut, sub, lo, hi))
        i += 1
    return out


def main():
    print(f'Analyzing: {PATH}')
    pkts = list(iter_packets(PATH))
    print(f'Total packets: {len(pkts)}')

    # Window: first housing CMSG from client = "user did something housing-related".
    # Before that, any housing-related SMSG is unprompted (server-initiated).
    first_cmsg_idx = None
    for idx, d, op, ta, tb, body in pkts:
        if d != 'CMSG':
            continue
        # Housing CMSG opcode groups: 0x35 0x36 0x37 0x38 0x39 0x3A 0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57 0x58 0x59 0x5A
        grp = (op >> 16) & 0xFF
        if grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A):
            first_cmsg_idx = idx
            break

    if first_cmsg_idx is None:
        print(f'No housing CMSG found — scanning full capture ({len(pkts)} pkts)')
        first_cmsg_idx = len(pkts)

    print(f'First housing CMSG at idx {first_cmsg_idx}')
    # Scan the full login phase window regardless of CMSG — retail may ship the
    # initial CREATE bundle across many SMSGs before any player-initiated
    # housing CMSG. Clamp at 2500 to include multi-thousand-packet inits.
    SCAN_UNTIL = min(max(first_cmsg_idx + 100, 2500), len(pkts))
    print(f'Scanning up to idx {SCAN_UNTIL}')

    # Walk all SMSGs in login window, extract Housing-type blocks
    ut_name = {0: 'VALUES_UPDATE', 1: 'CREATE_OBJECT', 2: 'CREATE_OBJECT2', 3: 'OUT_OF_RANGE'}
    housing_events = []  # (smsg_idx, ut, subType, lo, hi)

    for idx, d, op, ta, tb, body in pkts:
        if idx >= SCAN_UNTIL:
            break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        blocks = scan_update_object(body)
        for ut, sub, lo, hi in blocks:
            housing_events.append((idx, ut, sub, lo, hi))

    # Report
    per_smsg = {}
    for idx, ut, sub, lo, hi in housing_events:
        per_smsg.setdefault(idx, []).append((ut, sub, lo, hi))

    print(f'\nHousing entity blocks in {len(per_smsg)} distinct UPDATE_OBJECT SMSGs, '
          f'across login window (idx 0..{first_cmsg_idx-1}):')
    print()

    for idx in sorted(per_smsg.keys()):
        events = per_smsg[idx]
        subcounts = {}
        for ut, sub, lo, hi in events:
            key = (ut, sub)
            subcounts[key] = subcounts.get(key, 0) + 1
        parts = [f'sub{sub}/{ut_name.get(ut, ut)}={n}' for (ut, sub), n in sorted(subcounts.items())]
        print(f'  idx={idx:4d}  {", ".join(parts)}  (total={len(events)})')

    # Specific question: are there mirror (sub=4) blocks beyond the FIRST such SMSG?
    mirror_idxs = sorted({idx for (idx, ut, sub, lo, hi) in housing_events if sub == 4})
    print()
    print(f'Housing/4 mirror SMSG-indexes in login window: {mirror_idxs}')
    if len(mirror_idxs) > 1:
        print(f'  -> Retail DOES emit mirror in {len(mirror_idxs)} separate UPDATE_OBJECT SMSGs pre-CMSG.')
        print(f'  -> Gap between first and last = {mirror_idxs[-1] - mirror_idxs[0]} packets.')
    elif len(mirror_idxs) == 1:
        print(f'  -> Retail emits mirror ONCE at idx {mirror_idxs[0]} — NO deferred second emission.')
    else:
        print(f'  -> No mirror found in login window. (Unexpected — investigate.)')

    print()
    housing3_idxs = sorted({idx for (idx, ut, sub, lo, hi) in housing_events if sub == 3})
    print(f'Housing/3 PlayerHouseEntity SMSG-indexes in login window: {housing3_idxs[:10]}'
          f'{"..." if len(housing3_idxs) > 10 else ""}  (count={len(housing3_idxs)})')


if __name__ == '__main__':
    main()
