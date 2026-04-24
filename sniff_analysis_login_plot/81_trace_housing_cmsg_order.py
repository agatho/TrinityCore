#!/usr/bin/env python3
"""List every housing CMSG and its neighbouring SMSG activity to disambiguate
the 'retail sends mirror CREATE at +500ms' question from the
'CMSG response dispatches mirror CREATE' case.

Output: for each housing CMSG in the first N packets, print the closest
preceding and following UPDATE_OBJECT SMSGs. If a Housing/4 CREATE shows up
in the close-following SMSG, it's response-driven, not unprompted.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'

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


OPCODE_NAMES = {
    # a subset
    0x350007: 'CMSG_HOUSING_GET_PLAYER_PERMISSIONS',
    0x350026: 'CMSG_HOUSING_DECOR_REQUEST_STORAGE',
    0x39000E: 'CMSG_NEIGHBORHOOD_GET_ROSTER',
    0x390009: 'CMSG_NEIGHBORHOOD_BUY_HOUSE',
    0x380000: 'CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK',
    0x360001: 'CMSG_HOUSING_GET_CURRENT_HOUSE_INFO',
    0x54001B: 'CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO',
    0x54000B: 'CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOODS',
}


def scan_blocks(body):
    """Yield (updateType, high, sub) for blocks in one UPDATE_OBJECT body."""
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
    print(f'Trace: {PATH}')
    pkts = list(iter_packets(PATH))

    # Find all housing CMSGs in first 2500 packets
    housing_cmsgs = []
    for idx, d, op, body in pkts:
        if idx > 2500:
            break
        if d != 'CMSG':
            continue
        grp = (op >> 16) & 0xFF
        if grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A):
            housing_cmsgs.append((idx, op))

    # For each housing CMSG, find closest SMSG UPDATE_OBJECT that contains Housing/4 or Housing/3
    print(f'\nAll housing CMSGs in first 2500 pkts (order matters):')
    for cmsg_idx, op in housing_cmsgs:
        name = OPCODE_NAMES.get(op, f'0x{op:06X}')
        print(f'  idx={cmsg_idx:4d}  {name}')

    # Now iterate through all UPDATE_OBJECT SMSGs and annotate with "nearest preceding CMSG"
    print(f'\nUPDATE_OBJECT SMSGs containing Housing/4 (mirror) or Housing/3 (PlayerHouse) in first 2500 pkts:')
    print(f'(relative to nearest preceding housing CMSG — if none, it was server-initiated)')
    for idx, d, op, body in pkts:
        if idx > 2500:
            break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        # Scan blocks
        housing_blocks = [(ut, sub) for (ut, high, sub) in scan_blocks(body) if high == HIGH_GUID_HOUSING and sub in (3, 4)]
        if not housing_blocks:
            continue

        # Find nearest preceding housing CMSG
        preceding = None
        for cmsg_idx, cop in housing_cmsgs:
            if cmsg_idx < idx:
                preceding = (cmsg_idx, cop)
            else:
                break

        ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}
        descr = []
        for ut, sub in housing_blocks:
            descr.append(f'sub{sub}/{ut_name.get(ut, ut)}')

        if preceding:
            gap = idx - preceding[0]
            pname = OPCODE_NAMES.get(preceding[1], f'0x{preceding[1]:06X}')
            print(f'  SMSG idx={idx:4d}  {", ".join(descr)}  <- after CMSG {pname} (gap={gap} pkts)')
        else:
            print(f'  SMSG idx={idx:4d}  {", ".join(descr)}  <- NO preceding housing CMSG (server-initiated)')


if __name__ == '__main__':
    main()
