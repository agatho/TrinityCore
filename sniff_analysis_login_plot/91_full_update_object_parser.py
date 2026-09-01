#!/usr/bin/env python3
"""PROPER UPDATE_OBJECT parser that parses ALL blocks by locking onto the
unambiguous `uint32 fieldsSize + uint8 fieldFlags + EntityFragment[] + 0xFF`
anchor that ends every block.

Block structure per TC BaseEntity.cpp:
  uint8 updateType                       # 0=VALUES, 1=CREATE, 2=CREATE2, 3=OUT_OF_RANGE
  PackedGUID128 guid                     # 2 mask bytes + 0..16 data bytes
  # For CREATE/CREATE2 only:
  uint8 objectType                       # TYPEID_*
  <21 bits CreateObjectBits flags>       # padded to 3 bytes via FlushBits
  <variable MovementUpdate conditional fields per flag>
  # Then for CREATE/CREATE2 + VALUES:
  uint32 fieldsSize                      # bytes that follow
  uint8 fieldFlags                       # UF::UpdateFieldFlag (0..3)
  EntityFragment[] fragments             # from set of valid values
  uint8 255 (End)
  <per-fragment data ...>
  # VALUES-only: possibly IdsChanged flag + fragments list + contentsChangedMask

Strategy: after reading header fields, scan forward for the anchor:
  uint32 fieldsSize (sane value, 1..totalSize) AT offset P
  uint8 fieldFlags (0..3)                    AT P+4
  uint8[] only valid EntityFragment values   AT P+5..N
  uint8 0xFF                                 AT N
Then blockEnd = P + 4 + fieldsSize.

For UT=3 (OUT_OF_RANGE), no data follows — just the GUID.
For UT=0 (VALUES), the structure is: uint32 fieldsSize + rest; anchor still holds.
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

# Valid EntityFragment IDs from WowCSEntityDefinitions.h
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


def read_packed_guid128(buf, pos):
    """Returns (lo, hi, new_pos) or (None, None, pos) on failure."""
    if pos + 2 > len(buf):
        return None, None, pos
    lo_mask = buf[pos]
    hi_mask = buf[pos+1]
    off = pos + 2
    lo = hi = 0
    for b in range(8):
        if lo_mask & (1 << b):
            if off >= len(buf):
                return None, None, pos
            lo |= buf[off] << (b*8); off += 1
    for b in range(8):
        if hi_mask & (1 << b):
            if off >= len(buf):
                return None, None, pos
            hi |= buf[off] << (b*8); off += 1
    return lo, hi, off


def guid_info(lo, hi):
    high = (hi >> 58) & 0x3F
    sub = (hi >> 53) & 0x1F
    return high, sub


def find_anchor(body, start, end):
    """Scan for the unambiguous `uint32 fieldsSize + uint8 fieldFlags + fragList[] + 0xFF`
    anchor that marks the end of the movement-update section.

    Returns (anchor_pos, fields_size, fragments) or None.

    anchor_pos is position of the uint32 fieldsSize.
    Block ends at anchor_pos + 4 + fields_size.
    """
    p = start
    while p + 8 < end:
        # Try reading fieldsSize
        fsize = struct.unpack_from('<I', body, p)[0]
        if fsize < 2 or fsize > (end - p - 4):
            p += 1; continue
        # uint8 fieldFlags at p+4
        ff = body[p+4]
        if ff > 3:
            p += 1; continue
        # Scan fragment list
        q = p + 5
        frags = []
        ok = True
        while q < end and q < p + 5 + 64:  # fragments list limited to ~64 bytes max
            fv = body[q]
            if fv == 255:
                # end marker
                break
            if fv not in VALID_FRAGMENTS:
                ok = False; break
            frags.append(fv)
            q += 1
        else:
            ok = False
        if not ok:
            p += 1; continue
        if body[q] != 255:
            p += 1; continue
        # We also need at least one fragment for CREATE
        if not frags:
            p += 1; continue
        # Anchor confirmed
        return (p, fsize, frags)
    return None


def parse_block(body, pos, end):
    """Parse one block starting at `pos`. Returns (info_dict, new_pos) or (None, pos)."""
    start = pos
    if pos >= end:
        return None, pos

    ut = body[pos]
    pos += 1
    if ut > 3:
        return None, start

    lo, hi, pos = read_packed_guid128(body, pos)
    if lo is None:
        return None, start

    high, sub = guid_info(lo, hi)
    info = {
        'start': start, 'ut': ut, 'lo': lo, 'hi': hi,
        'high': high, 'sub': sub, 'obj_type': None,
        'fields_size': 0, 'fragments': [], 'block_end': pos,
    }

    if ut == 3:  # OUT_OF_RANGE — just the GUID
        info['block_end'] = pos
        return info, pos

    if ut in (1, 2):  # CREATE / CREATE2
        if pos >= end:
            return None, start
        info['obj_type'] = body[pos]
        pos += 1

    # For VALUES (ut=0) and CREATE/CREATE2, find the fieldsSize+fragList anchor
    # For CREATE we need to scan past the bit-packed MovementUpdate; for VALUES
    # we're already at the anchor.
    anchor = find_anchor(body, pos, end)
    if anchor is None:
        return None, start
    apos, fsize, frags = anchor

    info['anchor_pos'] = apos
    info['fields_size'] = fsize
    info['fragments'] = frags
    info['block_end'] = apos + 4 + fsize
    if info['block_end'] > end:
        return None, start

    return info, info['block_end']


def scan_update_object(body, verbose=False):
    """Parse the UPDATE_OBJECT packet body, returning list of block infos."""
    blocks = []
    if len(body) < 7:
        return blocks, 'too short'

    try:
        map_id = struct.unpack_from('<H', body, 0)[0]
        block_count = struct.unpack_from('<I', body, 2)[0]
    except Exception:
        return blocks, 'header parse error'
    pos = 6

    if pos >= len(body):
        return blocks, 'no flag byte'
    flag_byte = body[pos]; pos += 1
    unk_bit = (flag_byte >> 7) & 1
    has_destroy = (flag_byte >> 6) & 1

    if has_destroy:
        if pos + 6 > len(body):
            return blocks, 'destroy header truncated'
        destroy_count = struct.unpack_from('<H', body, pos)[0]; pos += 2
        total_destroy = struct.unpack_from('<I', body, pos)[0]; pos += 4
        if pos + total_destroy * 16 > len(body):
            return blocks, 'destroy block truncated'
        pos += total_destroy * 16

    if pos + 4 > len(body):
        return blocks, 'no dataSize'
    data_size = struct.unpack_from('<I', body, pos)[0]; pos += 4

    data_end = pos + data_size
    if data_end > len(body):
        data_end = len(body)

    hdr_info = {
        'map_id': map_id, 'block_count': block_count, 'flag_byte': flag_byte,
        'unk_bit': unk_bit, 'has_destroy': has_destroy,
        'data_size': data_size, 'data_start': pos, 'data_end': data_end,
    }

    while pos < data_end and len(blocks) < block_count + 10:
        info, new_pos = parse_block(body, pos, data_end)
        if info is None:
            # Stuck — try advancing one byte
            pos += 1
            continue
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
    return (f"[pos={info['start']:6d}] {hname}{sub_str:6s}  {ut_name:12s}  "
            f"GUID lo={info['lo']:016X} hi={info['hi']:016X}  {obj_s}  "
            f"len={datalen:5d}  frags=[{frags}]")


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

        unparsed = hdr['data_end'] - (blocks[-1]['block_end'] if blocks else hdr['data_start'])
        status = 'COMPLETE' if len(blocks) == hdr['block_count'] else 'INCOMPLETE'
        print(f'  Parsed {len(blocks)}/{hdr["block_count"]} blocks ({status}, {unparsed} unparsed bytes)')
        print()

    print(f'\n=== HOUSING SUBTYPE SUMMARY (login to first housing CMSG) ===')
    by_sub = {}
    for b in housing_blocks:
        key = b['sub']
        if key not in by_sub:
            by_sub[key] = {'CREATE': 0, 'CREATE2': 0, 'VALUES': 0, 'OUT': 0, 'samples': []}
        ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}[b['ut']]
        by_sub[key][ut_name] += 1
        if len(by_sub[key]['samples']) < 3 and b['ut'] in (1, 2):
            by_sub[key]['samples'].append(b)
    for sub in sorted(by_sub.keys()):
        s = by_sub[sub]
        print(f'  Housing/sub{sub:2d}  CREATE={s["CREATE"]:3d}  CREATE2={s["CREATE2"]:3d}  '
              f'VALUES={s["VALUES"]:3d}  OUT={s["OUT"]:3d}')
        for sample in s['samples']:
            frags = ','.join(FRAG_NAMES.get(f, f'F{f}') for f in sample['fragments'])
            print(f'      sample: type={sample["obj_type"]} frags=[{frags}]')

    print(f'\n=== TOTAL: parsed {total_parsed}/{total_expected} blocks ===')


if __name__ == '__main__':
    main()
