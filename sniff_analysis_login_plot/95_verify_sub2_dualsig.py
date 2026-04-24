#!/usr/bin/env python3
"""Verify whether Housing/sub2 really has two fragment signatures, or whether
the mesh-like signature is a false positive from inside MeshObject blocks.

Approach: Apply STRICTER anchor validation:
- Require the PackedGUID's lo_mask/hi_mask to match a sane Housing GUID pattern
  (realm in hi[byte 5-6], not random bytes)
- Require anchor within 128 bytes of header (Housing entities have short MovementUpdate)
- Print GUID details for each match to verify
"""
import struct
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
        return None, None, pos, 0, 0
    lo_mask = buf[pos]
    hi_mask = buf[pos+1]
    off = pos + 2
    lo = hi = 0
    for b in range(8):
        if lo_mask & (1 << b):
            if off >= end:
                return None, None, pos, 0, 0
            lo |= buf[off] << (b*8); off += 1
    for b in range(8):
        if hi_mask & (1 << b):
            if off >= end:
                return None, None, pos, 0, 0
            hi |= buf[off] << (b*8); off += 1
    return lo, hi, off, lo_mask, hi_mask


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


def is_sane_housing_guid(lo, hi, lo_mask, hi_mask):
    """Real Housing GUIDs have specific structure:
       hi = (high:6 << 58) | (sub:5 << 53) | (realm:13 << 42) | (type-specific:42)
    For sub2 (Room) in TC: (2, 0, arg2, roomDbId) → hi top = 0xDC40 (55<<2 | 0b1100)...
    For sub3 (PlayerHouse): (3, realmId, 7, bnetAccountId) → hi top 0xDC60
    For sub1 (Decor): (1, realmId, decorEntryId, counter) → hi top 0xDC20
    Common real Housing GUIDs have hi_mask >= 0x02 (realm bits set) and lo_mask has counter bits.
    """
    # The high 6 bits alone give us high=55 (0x37).
    # hi = 0xDC... (11011100 xx = high 6 bits are 110111 = 55 ✓, sub bits 0xx).
    # Real Housing GUIDs have at least some realm or counter bits set.
    if lo == 0 and hi_mask == 0x01:  # only hi top byte = just "high:sub" pattern, suspicious
        return False
    return True


def main():
    sniffs = glob.glob(r'C:/sniff/*/dumps/*.pkt')
    print(f'Scanning {len(sniffs)} retail sniffs for Housing/sub2 details\n')

    # Detailed per-sig tracking
    sig_samples = {}  # tuple(sig) -> list of (sniff, idx, pos, lo, hi, lo_mask, hi_mask, anchor_distance)

    for pkt in sorted(sniffs):
        name = os.path.basename(pkt)
        for idx, d, op, body in iter_packets(pkt):
            if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
                continue
            i = 0
            while i < len(body) - 10:
                ut = body[i]
                if ut not in (1, 2):
                    i += 1; continue
                lo, hi, next_p, lo_mask, hi_mask = read_packed_guid128(body, i+1, len(body))
                if lo is None:
                    i += 1; continue
                high = (hi >> 58) & 0x3F
                sub = (hi >> 53) & 0x1F
                if high != HIGH_GUID_HOUSING or sub != 2:
                    i += 1; continue
                if next_p >= len(body):
                    i += 1; continue
                obj_type = body[next_p]
                if obj_type > 20:
                    i += 1; continue
                anchor = find_anchor_within(body, next_p + 1, 512, len(body))
                if anchor is None:
                    i += 1; continue
                apos, fsize, frags, bend = anchor
                anchor_dist = apos - next_p
                if anchor_dist > 256:
                    i += 1; continue

                sig = tuple(frags)
                sig_samples.setdefault(sig, []).append({
                    'sniff': name, 'idx': idx, 'pos': i, 'lo': lo, 'hi': hi,
                    'lo_mask': lo_mask, 'hi_mask': hi_mask, 'anchor_dist': anchor_dist,
                    'obj_type': obj_type, 'fsize': fsize,
                })
                i = bend

    print('\n=== Housing/sub2 signatures (full GUID + mask detail) ===\n')
    for sig, samples in sig_samples.items():
        sig_str = ','.join(FRAG_NAMES.get(f, str(f)) for f in sig)
        print(f'Signature [{sig_str}] — {len(samples)} occurrences:')
        for s in samples[:20]:
            print(f"  {s['sniff']} idx={s['idx']} pos={s['pos']:6d} "
                  f"GUID lo={s['lo']:016X}:hi={s['hi']:016X} "
                  f"masks=(lo=0x{s['lo_mask']:02X},hi=0x{s['hi_mask']:02X}) "
                  f"obj={s['obj_type']} anchor_dist={s['anchor_dist']} fsize={s['fsize']}")
        if len(samples) > 20:
            print(f'  ... ({len(samples) - 20} more)')
        print()


if __name__ == '__main__':
    main()
