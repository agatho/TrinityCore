#!/usr/bin/env python3
"""UPDATE_OBJECT parser with 1-step lookahead validation.

Finds the anchor (uint32 fieldsSize + uint8 fieldFlags + EntityFragment[] + 0xFF)
but only accepts it if the resulting blockEnd leads to another parseable block
(or exactly data_end). This rejects false anchors that hit inside MovementUpdate
or fragment data.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
SMSG_UPDATE_OBJECT = 0x580000

HG_NAMES = {0: 'Null', 2: 'Player', 3: 'Item', 4: 'GameObject', 5: 'Unit',
            6: 'Corpse', 8: 'AreaTrigger', 11: 'Scenario', 30: 'BNet',
            55: 'Housing', 56: 'MeshObject', 57: 'Entity'}

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


def guid_info(lo, hi):
    high = (hi >> 58) & 0x3F
    sub = (hi >> 53) & 0x1F
    return high, sub


# HighGuid values that actually appear on the wire. From ObjectGuid.h.
# 0=Null, 2=Player, 3=Item, 4=GameObject, 5=Unit, 6=Corpse, 7=LootObject,
# 8=AreaTrigger, 9=DynamicObject, 11=Scenario, 12=Conversation, 13=Cast,
# 14=ClientActor, 15=Vignette, 17=Transport(?), 30=BNet, 55=Housing,
# 56=MeshObject, 57=Entity, 58=LMM, 59=WorldLocation, 60=ActorVendorItem
VALID_HIGH_GUIDS = {0, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 17, 18, 30,
                    55, 56, 57, 58, 59, 60}
VALID_OBJECT_TYPES = set(range(0, 21))  # TYPEID_* from ObjectGuid.h (max ~20)


def looks_like_block_start(buf, pos, end):
    """Quick sanity check: does `pos` look like a valid block start?"""
    if pos >= end:
        return False
    if pos == end:
        return True  # clean end
    ut = buf[pos]
    if ut > 3:
        return False
    lo, hi, new_pos = read_packed_guid128(buf, pos+1, end)
    if lo is None:
        return False
    high, sub = guid_info(lo, hi)
    if high not in VALID_HIGH_GUIDS:
        return False
    if ut == 3:  # OUT_OF_RANGE
        return True
    if ut in (1, 2):  # CREATE
        if new_pos >= end:
            return False
        obj_type = buf[new_pos]
        if obj_type not in VALID_OBJECT_TYPES:
            return False
    return True


def find_valid_anchor(buf, hdr_pos, end):
    """Scan forward from hdr_pos looking for an anchor that:
    - Has a sane fieldsSize
    - fieldFlags ∈ {0..3}
    - Valid fragment list terminated by 0xFF
    - Resulting blockEnd is either == end OR a valid next block start
    Returns (anchor_pos, fields_size, fragments) or None.
    """
    p = hdr_pos
    while p + 8 < end:
        fsize = struct.unpack_from('<I', buf, p)[0]
        if fsize < 2 or fsize > (end - p - 4):
            p += 1; continue
        ff = buf[p+4]
        if ff > 3:
            p += 1; continue
        # Parse fragment list
        q = p + 5
        frags = []
        max_frags_end = min(end, p + 5 + 48)
        while q < max_frags_end:
            fv = buf[q]
            if fv == 255:
                break
            if fv not in VALID_FRAGMENTS:
                break
            frags.append(fv)
            q += 1
        if q >= max_frags_end or buf[q] != 255 or not frags:
            p += 1; continue
        # Candidate anchor — validate with lookahead
        block_end = p + 4 + fsize
        if block_end > end:
            p += 1; continue
        if looks_like_block_start(buf, block_end, end):
            return (p, fsize, frags)
        p += 1
    return None


def parse_block(buf, pos, end):
    """Parse one block. Returns (info, block_end) or (None, pos)."""
    start = pos
    if pos >= end:
        return None, pos

    ut = buf[pos]
    pos += 1
    if ut > 3:
        return None, start

    lo, hi, pos = read_packed_guid128(buf, pos, end)
    if lo is None:
        return None, start

    high, sub = guid_info(lo, hi)
    info = {
        'start': start, 'ut': ut, 'lo': lo, 'hi': hi,
        'high': high, 'sub': sub, 'obj_type': None,
        'fields_size': 0, 'fragments': [], 'block_end': pos,
    }

    if ut == 3:
        info['block_end'] = pos
        return info, pos

    if ut in (1, 2):
        if pos >= end:
            return None, start
        info['obj_type'] = buf[pos]
        pos += 1

    anchor = find_valid_anchor(buf, pos, end)
    if anchor is None:
        return None, start
    apos, fsize, frags = anchor

    info['anchor_pos'] = apos
    info['fields_size'] = fsize
    info['fragments'] = frags
    info['block_end'] = apos + 4 + fsize
    return info, info['block_end']


def scan_update_object(body):
    blocks = []
    if len(body) < 7:
        return blocks, 'too short'

    try:
        map_id = struct.unpack_from('<H', body, 0)[0]
        block_count = struct.unpack_from('<I', body, 2)[0]
    except Exception:
        return blocks, 'header'
    pos = 6

    flag_byte = body[pos]; pos += 1
    has_destroy = (flag_byte >> 6) & 1

    if has_destroy:
        if pos + 6 > len(body):
            return blocks, 'destroy truncated'
        destroy_count = struct.unpack_from('<H', body, pos)[0]; pos += 2
        total_destroy = struct.unpack_from('<I', body, pos)[0]; pos += 4
        if pos + total_destroy * 16 > len(body):
            return blocks, 'destroy data truncated'
        pos += total_destroy * 16

    if pos + 4 > len(body):
        return blocks, 'no dataSize'
    data_size = struct.unpack_from('<I', body, pos)[0]; pos += 4

    data_start = pos
    data_end = min(pos + data_size, len(body))

    hdr_info = {
        'map_id': map_id, 'block_count': block_count, 'flag_byte': flag_byte,
        'has_destroy': has_destroy, 'data_size': data_size,
        'data_start': data_start, 'data_end': data_end,
    }

    while pos < data_end and len(blocks) < block_count:
        info, new_pos = parse_block(body, pos, data_end)
        if info is None:
            # Cannot parse further
            break
        blocks.append(info)
        pos = new_pos

    return blocks, hdr_info


def format_block(info):
    ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT_OF_RANGE'}[info['ut']]
    hname = HG_NAMES.get(info['high'], f'HG{info["high"]}')
    sub_str = f'/sub{info["sub"]}' if info['high'] == 55 else ''
    obj_s = f'type={info["obj_type"]:2d}' if info['obj_type'] is not None else 'type=--'
    frags = ','.join(FRAG_NAMES.get(f, f'F{f}') for f in info['fragments'])
    datalen = info['block_end'] - info['start']
    return (f"[pos={info['start']:6d}] {hname:12s}{sub_str:7s}{ut_name:13s}"
            f"GUID {info['lo']:016X}:{info['hi']:016X} {obj_s} len={datalen:5d} "
            f"frags=[{frags}]")


def main():
    pkts = list(iter_packets(PATH))
    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No LVW'); return

    first_h_cmsg = None
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if d == 'CMSG':
            grp = (op >> 16) & 0xFF
            if grp in (0x2E, 0x30, 0x31, 0x32, 0x33, 0x35, 0x37, 0x38, 0x39):
                first_h_cmsg = idx
                break
    upper = first_h_cmsg if first_h_cmsg else len(pkts)

    print(f'File: {PATH}')
    print(f'LVW={lvw}, first-housing-CMSG={first_h_cmsg}')
    print()

    housing_blocks = []
    total_parsed = 0
    total_expected = 0

    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        print(f'=== UPDATE_OBJECT idx={idx} (LVW+{idx-lvw}) len={len(body)} ===')
        blocks, hdr = scan_update_object(body)
        if isinstance(hdr, str):
            print(f'  ERROR: {hdr}')
            continue
        print(f"  Header: mapId={hdr['map_id']} blockCount={hdr['block_count']} "
              f"flags=0x{hdr['flag_byte']:02X} destroy={hdr['has_destroy']} "
              f"dataSize={hdr['data_size']}")
        total_parsed += len(blocks)
        total_expected += hdr['block_count']

        for b in blocks:
            line = format_block(b)
            if b['high'] == 55:
                housing_blocks.append(b)
                print('  >> ' + line)
            else:
                print('     ' + line)

        consumed = (blocks[-1]['block_end'] - hdr['data_start']) if blocks else 0
        unparsed = hdr['data_size'] - consumed
        status = 'OK' if len(blocks) == hdr['block_count'] else 'INCOMPLETE'
        print(f'  Parsed {len(blocks)}/{hdr["block_count"]} blocks ({status}, {unparsed} unparsed bytes)')
        print()

    print(f'\n=== HOUSING SUBTYPE SUMMARY ===')
    by_sub = {}
    for b in housing_blocks:
        key = b['sub']
        if key not in by_sub:
            by_sub[key] = {'CREATE': 0, 'CREATE2': 0, 'VALUES': 0, 'OUT': 0, 'samples': []}
        ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}[b['ut']]
        by_sub[key][ut_name] += 1
        if len(by_sub[key]['samples']) < 5 and b['ut'] in (1, 2):
            by_sub[key]['samples'].append(b)
    for sub in sorted(by_sub.keys()):
        s = by_sub[sub]
        print(f'  Housing/sub{sub:2d}  CREATE={s["CREATE"]:3d}  CREATE2={s["CREATE2"]:3d}  '
              f'VALUES={s["VALUES"]:3d}  OUT={s["OUT"]:3d}')
        for sample in s['samples']:
            frags = ','.join(FRAG_NAMES.get(f, f'F{f}') for f in sample['fragments'])
            print(f'      sample: type={sample["obj_type"]} len={sample["block_end"]-sample["start"]} frags=[{frags}]')

    print(f'\n=== TOTAL: parsed {total_parsed}/{total_expected} blocks ===')


if __name__ == '__main__':
    main()
