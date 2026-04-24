#!/usr/bin/env python3
"""Canonical retail post-LVW SMSG sequence dump.

For each SMSG between SMSG_LOGIN_VERIFY_WORLD and the first housing CMSG,
emit a record line: `LVW+N  opcode=0xNNNNNN  name  bodylen  first16bytes`.

Use as input to the divergence table build.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-56-30.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F


OPCODES = {
    # SMSGs we care about for housing login
    0x42002F: 'SMSG_LOGIN_VERIFY_WORLD',
    0x580000: 'SMSG_UPDATE_OBJECT',
    0x580001: 'SMSG_OUT_OF_RANGE',
    0x420004: 'SMSG_INIT_WORLD_STATES',
    0x4201F0: 'SMSG_UPDATE_WORLD_STATE',
    0x460000: 'SMSG_DB_REPLY',
    0x460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x460013: 'SMSG_INVALIDATE_NEIGHBORHOOD_NAME',
    0x5C0000: 'SMSG_NEIGHBORHOOD_CREATE_RESPONSE',
    0x5C0001: 'SMSG_NEIGHBORHOOD_INFO_UPDATE',
    0x5C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
    0x5C0013: 'SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE',
    0x5C0016: 'SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE',
    0x5C0018: 'SMSG_NEIGHBORHOOD_MAP_DATA_UPDATE',
    0x5C000A: 'SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE',
    0x5F0008: 'SMSG_INVALIDATE_NEIGHBORHOOD',
    0x5F0011: 'SMSG_VIGNETTE_UPDATE',
    0x5F0027: 'SMSG_QUERY_PLAYER_NAMES_RESPONSE',
    0x420318: 'SMSG_NPC_INTERACTION_OPEN_RESULT',
    0x550000: 'SMSG_HOUSING_HOUSE_STATUS_RESPONSE',
    0x550001: 'SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE',
    0x550006: 'SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE',
    0x540011: 'SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR',
    0x540015: 'SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR_ALT',
    0x540017: 'SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE',
    0x54000B: 'SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE',
    0x54001E: 'SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOODS_RESPONSE',
    0x560010: 'SMSG_HOUSING_DECOR_STORAGE_RSP',
    0x56000E: 'SMSG_HOUSING_CATALOG_STATE_SYNC',
    0x560020: 'SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_ALT',
    0x4A0004: 'SMSG_SUSPEND_TOKEN',
    0x420005: 'SMSG_WORLD_SERVER_INFO',
    0x420030: 'SMSG_TUTORIAL_FLAGS',
    0x5A0000: 'SMSG_TIME_SYNC_REQUEST',
    0x5A0003: 'SMSG_MOVE_SET_ACTIVE_MOVER',
    0x590000: 'SMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS',
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


def op_name(op):
    return OPCODES.get(op, f'0x{op:06X}')


def is_housing_op(op):
    grp = (op >> 16) & 0xFF
    return grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5F)


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
        if d == 'CMSG' and is_housing_op(op):
            first_h_cmsg = idx
            break
    upper = first_h_cmsg if first_h_cmsg else len(pkts)

    print(f'File: {PATH}')
    print(f'LVW idx={lvw}  first-housing-CMSG idx={first_h_cmsg}  window packets={upper-lvw-1}')
    print()
    print(f'{"Dir":4s}  {"idx":5s}  {"LVW+":5s}  {"opcode":10s}  {"name":55s}  {"len":6s}  body[0..16]')
    print('-' * 140)

    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        hex16 = body[:16].hex(' ') if body else ''
        print(f'{d:4s}  {idx:5d}  {idx-lvw:5d}  0x{op:08X}  {op_name(op):55s}  {len(body):6d}  {hex16}')


if __name__ == '__main__':
    main()
