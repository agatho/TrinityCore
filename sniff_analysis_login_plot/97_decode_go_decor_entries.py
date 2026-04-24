#!/usr/bin/env python3
"""Decode GameObject entry IDs from the 10 GO-with-FHousingDecor_C CREATEs in
retail sniffs so we can cross-reference them with gameobject_template.

GameObject GUID format (ObjectGuidFactory::CreateWorldObject):
  hi = (type:6 << 58) | (realmId:13 << 42) | (mapId:13 << 29) | (entry:23 << 6) | subType:6
  lo = (serverId:24 << 40) | counter:40
"""
import struct
import os
import glob

SMSG_UPDATE_OBJECT = 0x580000
HIGH_GUID_GAMEOBJECT = 11

VALID_FRAGMENTS = {
    1, 2, 5, 13, 15, 17, 18, 19, 20, 21, 22, 23, 27, 28, 29, 30, 31, 32, 33, 34, 37,
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,
    216, 217, 218, 219, 220, 221, 224, 225,
}


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


def read_packed_guid128(buf, pos, end):
    if pos + 2 > end:
        return None, None, pos
    lo_mask = buf[pos]
    hi_mask = buf[pos+1]
    off = pos + 2
    lo = hi = 0
    for b in range(8):
        if lo_mask & (1 << b):
            if off >= end:
                return None, None, pos
            lo |= buf[off] << (b*8); off += 1
    for b in range(8):
        if hi_mask & (1 << b):
            if off >= end:
                return None, None, pos
            hi |= buf[off] << (b*8); off += 1
    return lo, hi, off


def find_anchor_within(buf, start, max_scan, end):
    p = start
    limit = min(end - 8, start + max_scan)
    while p < limit:
        fsize = struct.unpack_from('<I', buf, p)[0]
        if 2 <= fsize <= (end - p - 4):
            ff = buf[p+4]
            if ff <= 3:
                q = p + 5
                frags = []
                ok = True
                max_q = min(end, p + 5 + 48)
                while q < max_q:
                    fv = buf[q]
                    if fv == 255:
                        break
                    if fv not in VALID_FRAGMENTS:
                        ok = False; break
                    frags.append(fv)
                    q += 1
                if ok and q < max_q and buf[q] == 255 and frags:
                    block_end = p + 4 + fsize
                    if block_end <= end:
                        return (p, fsize, frags, block_end)
        p += 1
    return None


def decode_go_guid(lo, hi):
    """Decode WorldObject GUID into (type, realm, mapId, entry, subType, serverId, counter)."""
    gtype = (hi >> 58) & 0x3F
    realm = (hi >> 42) & 0x1FFF
    mapId = (hi >> 29) & 0x1FFF
    entry = (hi >> 6) & 0x7FFFFF
    subType = hi & 0x3F
    serverId = (lo >> 40) & 0xFFFFFF
    counter = lo & 0xFFFFFFFFFF
    return gtype, realm, mapId, entry, subType, serverId, counter


def main():
    sniffs = glob.glob(r'C:/sniff/*/dumps/*.pkt')

    # Track entries found
    entry_counts = {}  # entry -> { count, sniffs: set() }

    for pkt in sorted(sniffs):
        name = os.path.basename(pkt)
        sniff_entries = []
        for idx, d, op, body in iter_packets(pkt):
            if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
                continue
            i = 0
            while i < len(body) - 10:
                ut = body[i]
                if ut not in (1, 2):
                    i += 1; continue
                lo, hi, next_p = read_packed_guid128(body, i+1, len(body))
                if lo is None:
                    i += 1; continue
                gtype = (hi >> 58) & 0x3F
                if gtype != HIGH_GUID_GAMEOBJECT:
                    i += 1; continue
                if next_p >= len(body):
                    i += 1; continue
                obj_type = body[next_p]
                if obj_type != 8:
                    i += 1; continue
                anchor = find_anchor_within(body, next_p + 1, 512, len(body))
                if anchor is None:
                    i += 1; continue
                apos, fsize, frags, bend = anchor
                if apos - next_p > 256:
                    i += 1; continue
                if 20 not in frags:
                    i = bend; continue
                _, realm, mapId, entry, subType, serverId, counter = decode_go_guid(lo, hi)
                sniff_entries.append((idx, i, lo, hi, realm, mapId, entry, subType, serverId, counter))
                entry_counts.setdefault(entry, {'count': 0, 'sniffs': set(), 'samples': []})
                entry_counts[entry]['count'] += 1
                entry_counts[entry]['sniffs'].add(name)
                if len(entry_counts[entry]['samples']) < 3:
                    entry_counts[entry]['samples'].append((name, idx, mapId))
                i = bend
        if sniff_entries:
            print(f'\n{name}:')
            for idx, pos, lo, hi, realm, mapId, entry, subType, serverId, counter in sniff_entries:
                print(f'  idx={idx} pos={pos:6d}  GUID {lo:016X}:{hi:016X}')
                print(f'    realm={realm} mapId={mapId} entry={entry} subType={subType} '
                      f'serverId={serverId} counter={counter}')

    print('\n\n=== Unique GO entries across all sniffs ===')
    for entry in sorted(entry_counts.keys()):
        info = entry_counts[entry]
        print(f'  entry={entry}: {info["count"]} CREATEs across {len(info["sniffs"])} sniffs')
        for name, idx, mapId in info['samples']:
            print(f'    sample: {name} idx={idx} mapId={mapId}')


if __name__ == '__main__':
    main()
