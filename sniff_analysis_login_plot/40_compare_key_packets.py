#!/usr/bin/env python3
"""Dump and side-by-side compare byte content of key housing/login packets
between OUR server and RETAIL. Packets chosen to trigger initial UI state:

  - SMSG_MIRROR_VARS                                  (0x0042036A)
  - SMSG_INIT_WORLD_STATES                            (0x004201EE)
  - SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE      (0x00560006)
  - SMSG_HOUSING_HOUSE_STATUS_RESPONSE                (0x00560005)
  - SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE             (0x005C0012)
  - SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE             (0x00460012)
  - SMSG_WORLD_SERVER_INFO                            (0x0047000A)
  - SMSG_HOUSING_DECOR_STORAGE_RSP                    (0x00510010)
  - SMSG_TUTORIAL_FLAGS                               (0x004200A6)

For each target opcode, dump first N bytes of each occurrence in each sniff
and emit a parallel table.
"""
import os
import struct

OURS = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
RETAIL = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

TARGETS = {
    0x0042036A: 'SMSG_MIRROR_VARS',
    0x004201EE: 'SMSG_INIT_WORLD_STATES',
    0x00560006: 'SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE',
    0x00560005: 'SMSG_HOUSING_HOUSE_STATUS_RESPONSE',
    0x005C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
    0x00460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x0047000A: 'SMSG_WORLD_SERVER_INFO',
    0x00510010: 'SMSG_HOUSING_DECOR_STORAGE_RSP',
    0x004200A6: 'SMSG_TUTORIAL_FLAGS',
}
MAX_DUMP = 256


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    off = min(cands); idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4: off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def collect(path, target_ops):
    by_op = {op: [] for op in target_ops}
    for idx, direction, op, body in iter_packets(path):
        if op in by_op:
            by_op[op].append((idx, direction, body))
    return by_op


def dump_hex(body, limit=MAX_DUMP):
    b = body[:limit]
    rows = []
    for i in range(0, len(b), 32):
        rows.append('  ' + ' '.join(f'{x:02x}' for x in b[i:i+32]))
    if len(body) > limit:
        rows.append(f'  ... ({len(body) - limit} more bytes truncated)')
    return '\n'.join(rows)


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    os.makedirs(outdir, exist_ok=True)
    ours_packets = collect(OURS, list(TARGETS))
    retail_packets = collect(RETAIL, list(TARGETS))
    md = ['# Key packet wire-format comparison: OURS vs RETAIL\n\n']
    for op, name in TARGETS.items():
        md.append(f'## {name} (0x{op:08X})\n\n')
        md.append(f'- OURS count: {len(ours_packets[op])}\n')
        md.append(f'- RETAIL count: {len(retail_packets[op])}\n\n')
        # Show up to 2 samples from each side
        md.append('### OURS samples\n\n')
        for i, (idx, direction, body) in enumerate(ours_packets[op][:2]):
            md.append(f'**idx {idx} {direction} size {len(body)}**\n\n')
            md.append('```\n')
            md.append(dump_hex(body))
            md.append('\n```\n\n')
        if not ours_packets[op]:
            md.append('_(none sent by ours)_\n\n')
        md.append('### RETAIL samples\n\n')
        for i, (idx, direction, body) in enumerate(retail_packets[op][:2]):
            md.append(f'**idx {idx} {direction} size {len(body)}**\n\n')
            md.append('```\n')
            md.append(dump_hex(body))
            md.append('\n```\n\n')
        if not retail_packets[op]:
            md.append('_(none in this retail sniff)_\n\n')
        md.append('---\n\n')
    with open(os.path.join(outdir, 'WIRE_KEY_PACKETS.md'), 'w', encoding='utf-8') as f:
        f.writelines(md)
    print(f'Wrote {outdir}/WIRE_KEY_PACKETS.md')
    # Print summary
    print('\nPacket count side-by-side:')
    print(f'  {"opcode":<12} {"name":<55} {"OURS":>5} {"RETAIL":>7}')
    for op, name in TARGETS.items():
        print(f'  0x{op:08X} {name:<55} {len(ours_packets[op]):>5} {len(retail_packets[op]):>7}')
