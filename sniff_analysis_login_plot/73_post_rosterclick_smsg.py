#!/usr/bin/env python3
"""List every SMSG within 2 seconds after the manual CMSG_NEIGHBORHOOD_GET_ROSTER
click. Gives the full picture of what server emits during the user's
icon-fixing interaction.
"""
import struct

PATH = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'


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
    0x460000: 'SMSG_DB_REPLY',
    0x460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x4201F0: 'SMSG_UPDATE_WORLD_STATE',
    0x5C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
    0x580000: 'SMSG_UPDATE_OBJECT',
    0x5F0027: 'SMSG_QUERY_PLAYER_NAMES_RESPONSE',
    0x5F0008: 'SMSG_INVALIDATE_NEIGHBORHOOD',
    0x460013: 'SMSG_INVALIDATE_NEIGHBORHOOD_NAME',
    0x5C0013: 'SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE',
    0x5C0016: 'SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE',
    0x420318: 'SMSG_NPC_INTERACTION_OPEN_RESULT',
    0x39000E: 'CMSG_NEIGHBORHOOD_GET_ROSTER',
    0x3B0086: 'CMSG_GAME_OBJ_USE',
    0x3B0087: 'CMSG_GAME_OBJ_REPORT_USE',
    0x400010: 'CMSG_DB_QUERY_BULK',
}

ROSTER_CMSG_IDX = 2873


def main():
    pkts = list(iter_packets(PATH))
    print(f'Total packets: {len(pkts)}')

    # Window: 50 packets after roster CMSG
    print(f'\n=== Packets idx {ROSTER_CMSG_IDX} to {ROSTER_CMSG_IDX + 60} ===')
    for idx, d, op, body in pkts:
        if idx < ROSTER_CMSG_IDX or idx > ROSTER_CMSG_IDX + 60:
            continue
        name = OPCODE_NAMES.get(op, f'0x{op:08X}')
        print(f'  idx={idx} {d} {name} len={len(body)}')


if __name__ == '__main__':
    main()
