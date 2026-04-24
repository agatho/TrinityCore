#!/usr/bin/env python3
"""Strict cross-sniff Housing entity scanner.

For each retail sniff PKT, find all Housing/subN CREATE blocks by requiring:
  1. ut ∈ (1, 2)
  2. PackedGUID with high=55 (Housing)
  3. objtype ≤ 20
  4. A valid (uint32 fieldsSize + uint8 fieldFlags + fragments + 0xFF) anchor
     within 256 bytes of the header (CREATE MovementUpdate is typically short
     for Housing entities that have HasEntityPosition only).

This rules out false positives from random byte matches inside fragment data.
"""
import struct
import sys
import os
import glob

SMSG_UPDATE_OBJECT = 0x580000
HIGH_GUID_HOUSING = 55

VALID_FRAGMENTS = {
    1, 2, 5, 13, 15, 17, 18, 19, 20, 21, 22, 23, 27, 28, 29, 30, 31, 32, 33, 34, 37,
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,
    216, 217, 218, 219, 220, 221, 224, 225,
}
FRAG_NAMES = {
    1: 'FEntityPosition', 2: 'CGObject', 5: 'FTransportLink',
    13: 'FPlayerOwnershipLink', 15: 'CActor', 17: 'FVendor_C',
    18: 'FMirroredObject_C', 19: 'FMeshObjectData_C', 20: 'FHousingDecor_C',
    21: 'FHousingRoom_C', 22: 'FHousingRoomComponentMesh_C',
    23: 'FHousingPlayerHouse_C', 27: 'FJamHousingCornerstone_C',
    28: 'FHousingDecorActor_C', 29: 'FHousingPlotAreaTrigger_C',
    30: 'FNeighborhoodMirrorData_C', 31: 'FMirroredPositionData_C',
    32: 'PlayerHouseInfoComponent_C', 33: 'FHousingStorage_C',
    34: 'FHousingFixture_C', 37: 'PlayerInitiativeComponent_C',
    200: 'Tag_Item', 201: 'Tag_Container', 202: 'Tag_AzeriteEmpoweredItem',
    203: 'Tag_AzeriteItem', 204: 'Tag_Unit', 205: 'Tag_Player',
    206: 'Tag_GameObject', 207: 'Tag_DynamicObject', 208: 'Tag_Corpse',
    209: 'Tag_AreaTrigger', 210: 'Tag_SceneObject', 211: 'Tag_Conversation',
    212: 'Tag_AIGroup', 213: 'Tag_Scenario', 214: 'Tag_LootObject',
    215: 'Tag_ActivePlayer', 216: 'Tag_ActiveClient_S',
    217: 'Tag_ActiveObject_C', 218: 'Tag_VisibleObject_C',
    219: 'Tag_UnitVehicle', 220: 'Tag_HousingRoom', 221: 'Tag_MeshObject',
    224: 'Tag_HouseExteriorPiece', 225: 'Tag_HouseExteriorRoot',
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
    """Scan start..min(start+max_scan, end-8) for a valid anchor."""
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


def scan_housing_creates(body):
    """Scan packet body for Housing/subN CREATE blocks with anchor validation.
    Returns list of dicts.
    """
    creates = []
    if len(body) < 10:
        return creates
    i = 0
    while i < len(body) - 10:
        ut = body[i]
        if ut not in (1, 2):
            i += 1; continue
        # Try to read PackedGUID
        lo, hi, next_p = read_packed_guid128(body, i+1, len(body))
        if lo is None:
            i += 1; continue
        high = (hi >> 58) & 0x3F
        sub = (hi >> 53) & 0x1F
        if high != HIGH_GUID_HOUSING:
            i += 1; continue
        if next_p >= len(body):
            i += 1; continue
        obj_type = body[next_p]
        if obj_type > 20:
            i += 1; continue
        # Validate with anchor scan — Housing entities have short MovementUpdates
        # (typically HasEntityPosition + Room/Decor/MeshObject GUID), so 256 bytes
        # is plenty. Exclude fake matches where no anchor exists nearby.
        anchor = find_anchor_within(body, next_p + 1, max_scan=512, end=len(body))
        if anchor is None:
            i += 1; continue
        apos, fsize, frags, bend = anchor
        # Reject anchors that are absurdly far from the header (> 256 bytes of
        # movement-update data would be unusual for a Housing entity)
        if apos - next_p > 256:
            i += 1; continue

        creates.append({
            'pos': i, 'ut': ut, 'sub': sub, 'lo': lo, 'hi': hi,
            'obj_type': obj_type, 'anchor_pos': apos, 'fsize': fsize,
            'fragments': frags, 'block_end': bend,
        })
        i = bend
    return creates


def main():
    sniffs = glob.glob(r'C:/sniff/*/dumps/*.pkt')
    print(f'Scanning {len(sniffs)} retail sniffs for VERIFIED Housing/subN CREATE blocks\n')

    # Aggregate: sub -> { create_count, fragment_signatures: set(tuples) }
    total_by_sub = {}
    per_sniff = []

    for pkt in sorted(sniffs):
        name = os.path.basename(pkt)
        sub_counts = {}  # sub -> count
        sub_sigs = {}    # sub -> set of fragment tuples

        for idx, d, op, body in iter_packets(pkt):
            if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
                continue
            for c in scan_housing_creates(body):
                sub = c['sub']
                sub_counts[sub] = sub_counts.get(sub, 0) + 1
                sig = tuple(c['fragments'])
                sub_sigs.setdefault(sub, set()).add(sig)

                total_by_sub.setdefault(sub, {'count': 0, 'sigs': set(), 'sniffs': set()})
                total_by_sub[sub]['count'] += 1
                total_by_sub[sub]['sigs'].add(sig)
                total_by_sub[sub]['sniffs'].add(name)

        if sub_counts:
            per_sniff.append((name, sub_counts, sub_sigs))

    # Per-sniff summary
    for name, sub_counts, sub_sigs in per_sniff:
        print(f'\n{name}:')
        for sub in sorted(sub_counts.keys()):
            sigs = sub_sigs[sub]
            sig_strs = [','.join(FRAG_NAMES.get(f, str(f)) for f in sig) for sig in sigs]
            print(f'  sub{sub:2d}  CREATE={sub_counts[sub]:4d}')
            for sig in sig_strs[:2]:
                print(f'         frags=[{sig}]')

    # Cross-sniff summary
    print(f'\n\n=== CROSS-SNIFF HOUSING/subN VERIFIED CREATE SUMMARY ===')
    for sub in sorted(total_by_sub.keys()):
        info = total_by_sub[sub]
        print(f'\nHousing/sub{sub:2d}: {info["count"]} CREATEs across {len(info["sniffs"])} sniffs')
        print(f'  Fragment signatures ({len(info["sigs"])}):')
        for sig in info['sigs']:
            frags = ','.join(FRAG_NAMES.get(f, str(f)) for f in sig)
            print(f'    [{frags}]')


if __name__ == '__main__':
    main()
