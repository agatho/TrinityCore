#!/usr/bin/env python3
"""Inventory all entities the client receives during login on our TC server.
Specifically count Housing/3 (FHousingPlayerHouse_C), Housing/4 (mirror),
Entity (mirrors), and Housing/2 (rooms).

If the client's map-icon picker resolves ownerType via entity-registry
lookup on each plot's HouseGUID, we need 1 Housing/3 per occupied plot.
"""
import struct


PATH = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'
LOGIN_WINDOW_END = 2873  # First manual CMSG_NEIGHBORHOOD_GET_ROSTER


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


def decode_guid_hi(hi):
    """Extract HighGuid and subType from hi part."""
    if hi == 0: return 0, 0
    high = (hi >> 58) & 0x3F
    sub = (hi >> 53) & 0x1F
    return high, sub


def scan_updateobject(body):
    """Scan a SMSG_UPDATE_OBJECT body for CREATE blocks.
    Return list of (updateType, highGuid, subType, guid_lo, guid_hi)."""
    blocks = []
    # Header: uint32 count, then blocks
    if len(body) < 4:
        return blocks

    # Try to parse: blocks start with updateType byte
    i = 0
    MAX_DEPTH = 200000
    scans = 0
    while i < len(body) - 10 and scans < MAX_DEPTH:
        scans += 1
        ut = body[i]
        if ut not in (0, 1, 2, 3):
            i += 1
            continue
        # read packed GUID
        if i + 3 > len(body):
            break
        m_lo, m_hi = body[i+1], body[i+2]
        off = i + 3
        lo = hi = 0
        ok = True
        for b in range(8):
            if m_lo & (1 << b):
                if off >= len(body): ok = False; break
                lo |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        for b in range(8):
            if m_hi & (1 << b):
                if off >= len(body): ok = False; break
                hi |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        if hi == 0 and lo == 0:
            i += 1; continue

        high, sub = decode_guid_hi(hi)
        if high == 0 or high > 60:
            i += 1; continue

        if ut in (1, 2):  # CREATE
            # Must have objectType byte next (0..20)
            if off < len(body) and body[off] <= 20:
                blocks.append((ut, high, sub, lo, hi, i))
                i = off + 1
            else:
                i += 1
        else:
            blocks.append((ut, high, sub, lo, hi, i))
            i += 1
    return blocks


def main():
    pkts = list(iter_packets(PATH))
    total_blocks_by_entity = {}  # (high, sub, ut) -> count
    housing3_guids = set()

    for idx, d, op, body in pkts:
        if idx >= LOGIN_WINDOW_END:
            break
        if d != 'SMSG' or op != 0x580000:
            continue
        # UPDATE_OBJECT
        blocks = scan_updateobject(body)
        for ut, high, sub, lo, hi, _ in blocks:
            key = (high, sub, ut)
            total_blocks_by_entity[key] = total_blocks_by_entity.get(key, 0) + 1
            if high == 55 and sub == 3 and ut == 1:
                housing3_guids.add((lo, hi))

    print(f'Blocks in login-phase UPDATE_OBJECTs (idx < {LOGIN_WINDOW_END}):')
    HG = {2: 'Player', 3: 'Item', 11: 'GameObject', 13: 'AT',
          30: 'BNetAccount', 55: 'Housing', 56: 'MeshObject', 57: 'Entity'}
    UT = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2'}
    for (high, sub, ut), count in sorted(total_blocks_by_entity.items()):
        hn = HG.get(high, f'HG{high}')
        ut_s = UT.get(ut, f'ut{ut}')
        extra = f' sub={sub}' if high == 55 else ''
        print(f'  {hn}{extra} {ut_s}: {count}')

    print(f'\nUnique Housing/3 CREATE GUIDs: {len(housing3_guids)}')
    for lo, hi in housing3_guids:
        print(f'  lo={lo:016X} hi={hi:016X}')


if __name__ == '__main__':
    main()
