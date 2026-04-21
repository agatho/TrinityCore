#!/usr/bin/env python3
"""Dump and diff SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE wire bytes.
This packet is the trigger for the client's initial housing UI setup:
  - PlotId, HouseGuid, OwnerGuid, NeighborhoodGuid, AccessFlags

Opcode: 0x00560006 (SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE).
Also checks SMSG_HOUSING_HOUSE_STATUS_RESPONSE (0x00560005).

Output: docs/audit_2026_04_21/WIRE_HOUSE_INFO.md
"""
import os
import struct

OURS = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
RETAIL = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

TARGET_OPCODES = {
    0x00560006: 'SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE',
    0x00560005: 'SMSG_HOUSING_HOUSE_STATUS_RESPONSE',
    0x00560007: 'SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE',
    0x00560000: 'SMSG_HOUSING_CURRENT_HOUSE_INFO',
    0x0056001B: 'SMSG_HOUSING_SYSTEM_UNK_1B',
    0x0056000E: 'SMSG_HOUSING_CATALOG_STATE_SYNC',
}


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    off = min(cands)
    idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def dump(path, label, md):
    md.append(f'## {label}\n')
    md.append(f'file: `{path}`\n\n')
    for idx, direction, op, body in iter_packets(path):
        if op not in TARGET_OPCODES:
            continue
        name = TARGET_OPCODES[op]
        md.append(f'### idx={idx} {direction} {name} (0x{op:08X}) size={len(body)}\n')
        md.append('```\n')
        for i in range(0, len(body), 32):
            chunk = body[i:i+32]
            md.append('  ' + ' '.join(f'{b:02x}' for b in chunk) + '\n')
        md.append('```\n\n')


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    os.makedirs(outdir, exist_ok=True)
    md = ['# Wire-format dumps: key Housing status/response packets\n\n']
    dump(OURS, 'OUR (2026-04-21 22:31)', md)
    dump(RETAIL, 'RETAIL (editor 2026-04-15)', md)
    with open(os.path.join(outdir, 'WIRE_HOUSE_INFO.md'), 'w', encoding='utf-8') as f:
        f.writelines(md)
    print(f'Wrote {outdir}/WIRE_HOUSE_INFO.md')
