#!/usr/bin/env python3
"""Dump every SMSG sent from SMSG_LOGIN_VERIFY_WORLD up to the first housing
CMSG (unprompted server emissions only). Highlights housing-relevant opcodes.

This is the "what retail actually ships at login" reference for diffing
against our TC server's equivalent window.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-56-30.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F


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


# Minimal opcode mapping — just the housing + common login packets
OPCODES = {
    0x42002F: 'SMSG_LOGIN_VERIFY_WORLD',
    0x580000: 'SMSG_UPDATE_OBJECT',
    0x4201F0: 'SMSG_UPDATE_WORLD_STATE',
    0x460000: 'SMSG_DB_REPLY',
    0x460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x460013: 'SMSG_INVALIDATE_NEIGHBORHOOD_NAME',
    0x5C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
    0x5C0013: 'SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE',
    0x5C0016: 'SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE',
    0x5F0008: 'SMSG_INVALIDATE_NEIGHBORHOOD',
    0x5F0027: 'SMSG_QUERY_PLAYER_NAMES_RESPONSE',
    0x420318: 'SMSG_NPC_INTERACTION_OPEN_RESULT',
    0x420004: 'SMSG_INIT_WORLD_STATES',
    0x550018: 'SMSG_HOUSING_HOUSE_STATUS_RESPONSE',
    0x550008: 'SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE',
    0x560020: 'SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE',
    0x560010: 'SMSG_HOUSING_CATALOG_STATE_SYNC',
    0x5C0001: 'SMSG_NEIGHBORHOOD_INFO_UPDATE',
    0x5C0018: 'SMSG_NEIGHBORHOOD_MAP_DATA_UPDATE',
    0x5C0000: 'SMSG_NEIGHBORHOOD_CREATE_RESPONSE',
    0x540013: 'SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE',
    0x540015: 'SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR',
    0x540017: 'SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE',
    0x54001E: 'SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOODS_RESPONSE',
    0x580001: 'SMSG_OUT_OF_RANGE',
    0x4A0004: 'SMSG_SUSPEND_TOKEN',
    0x420005: 'SMSG_WORLD_SERVER_INFO',
    0x420030: 'SMSG_TUTORIAL_FLAGS',
    0x5C000A: 'SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE',
}


def op_name(op):
    return OPCODES.get(op, f'0x{op:06X}')


def is_housing_op(op):
    grp = (op >> 16) & 0xFF
    return grp in (0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5F)


def main():
    print(f'Post-LVW unprompted SMSG sequence from: {PATH}')
    pkts = list(iter_packets(PATH))

    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No LVW found'); return
    print(f'LOGIN_VERIFY_WORLD at idx {lvw}\n')

    first_housing_cmsg = None
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if d != 'CMSG': continue
        if is_housing_op(op):
            first_housing_cmsg = (idx, op)
            break

    upper = first_housing_cmsg[0] if first_housing_cmsg else len(pkts)
    print(f'Unprompted window: idx {lvw+1}..{upper-1} ({upper-lvw-1} packets)')
    if first_housing_cmsg:
        print(f'First housing CMSG: idx={first_housing_cmsg[0]} {op_name(first_housing_cmsg[1])}')
    print()

    # Count every SMSG opcode in window, flag housing-relevant
    seen = {}
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        if d != 'SMSG': continue
        key = op
        if key not in seen:
            seen[key] = []
        seen[key].append(idx)

    housing_ops = {op: ixs for op, ixs in seen.items() if is_housing_op(op) or op == 0x580000}
    misc_ops = {op: ixs for op, ixs in seen.items() if not is_housing_op(op) and op != 0x580000}

    print('Housing / UPDATE_OBJECT SMSGs in window:')
    for op in sorted(housing_ops.keys()):
        ixs = housing_ops[op]
        offsets = [f'LVW+{i-lvw}' for i in ixs[:10]]
        extra = f' ...({len(ixs)-10} more)' if len(ixs) > 10 else ''
        print(f'  {op_name(op):55s}  count={len(ixs):3d}  at {", ".join(offsets)}{extra}')

    print()
    print('Other SMSGs in window (non-housing, for context):')
    for op in sorted(misc_ops.keys()):
        ixs = misc_ops[op]
        print(f'  {op_name(op):55s}  count={len(ixs):3d}')


if __name__ == '__main__':
    main()
