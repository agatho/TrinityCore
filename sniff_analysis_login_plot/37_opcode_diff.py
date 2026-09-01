#!/usr/bin/env python3
"""With an authoritative opcode→name map, produce a clean side-by-side
histogram diff between OUR and RETAIL sniffs for build 66838. Only compares
meaningful packet counts, filters noise, and flags opcodes that appear in
retail but not in ours.
"""
import json
import os
import struct
import sys

sys.path.insert(0, r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21')
from opcode_map import OPCODE_MAP

OURS = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
RETAILS = [
    ('editor',       r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'),
    ('floorplan',    r'c:/sniff/floorplan_editor_rotation/dumps/dump_12.0.1.66838_2026-04-10_08-45-23.pkt'),
    ('wallcustomize',r'c:/sniff/wall_floor_ceiling_customize/dumps/dump_12.0.1.66838_2026-04-12_10-11-26.pkt'),
]


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    if not cands:
        return
    off = min(cands)
    idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx:
                return
            off = min(nx)
            continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1
            continue
        ps = off+29
        pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, dlen
        off = pe
        idx += 1


def count_opcodes(path):
    counts = {}
    for idx, direction, op, dlen in iter_packets(path):
        counts[op] = counts.get(op, 0) + 1
    return counts


def name(op):
    return OPCODE_MAP.get(op, f'UNKNOWN_0x{op:08X}')


def is_housing_or_neighborhood(op):
    n = OPCODE_MAP.get(op, '')
    return 'HOUSING' in n or 'NEIGHBORHOOD' in n


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    ours = count_opcodes(OURS)
    retails = {label: count_opcodes(path) for label, path in RETAILS}

    # Combined retail view (max of counts across sniffs)
    merged_retail = {}
    for label, counts in retails.items():
        for op, n in counts.items():
            merged_retail[op] = max(merged_retail.get(op, 0), n)

    # Diff: opcodes only in retail (not in ours at all)
    retail_only = {op: n for op, n in merged_retail.items() if op not in ours}
    ours_only = {op: n for op, n in ours.items() if op not in merged_retail}

    with open(os.path.join(outdir, 'OPCODE_DIFF.md'), 'w', encoding='utf-8') as f:
        f.write('# Opcode diff — OUR vs RETAIL build 66838\n\n')
        f.write(f'OUR sniff: {OURS}\n')
        f.write(f'RETAIL sniffs: {len(RETAILS)} (editor, floorplan, wallcustomize) — MAX count per opcode across all three\n\n')

        f.write('## Housing/Neighborhood opcodes — side by side\n\n')
        f.write('| opcode | name | OUR | RETAIL max | status |\n')
        f.write('|--------|------|-----|------------|--------|\n')
        all_housing = set([op for op in ours if is_housing_or_neighborhood(op)]) | \
                      set([op for op in merged_retail if is_housing_or_neighborhood(op)])
        for op in sorted(all_housing):
            o = ours.get(op, 0)
            r = merged_retail.get(op, 0)
            status = '=' if o > 0 and r > 0 else ('MISSING_OURS' if r > 0 and o == 0 else ('MISSING_RETAIL' if o > 0 and r == 0 else '-'))
            f.write(f'| 0x{op:08X} | {name(op)} | {o} | {r} | {status} |\n')

        f.write('\n## Retail-only opcodes (present in any retail sniff, zero in ours) — housing/neighborhood filtered\n\n')
        filtered = [(op, n) for op, n in retail_only.items() if is_housing_or_neighborhood(op)]
        if filtered:
            f.write('| opcode | name | retail max count |\n')
            f.write('|--------|------|------------------|\n')
            for op, n in sorted(filtered, key=lambda kv: -kv[1]):
                f.write(f'| 0x{op:08X} | {name(op)} | {n} |\n')
        else:
            f.write('_(none — housing/neighborhood coverage complete)_\n')

        f.write('\n## Retail-only opcodes (any subsystem, top 40)\n\n')
        f.write('| opcode | name | retail max count |\n')
        f.write('|--------|------|------------------|\n')
        for op, n in sorted(retail_only.items(), key=lambda kv: -kv[1])[:40]:
            f.write(f'| 0x{op:08X} | {name(op)} | {n} |\n')

        f.write('\n## Ours-only opcodes (top 40)\n\n')
        f.write('| opcode | name | our count |\n')
        f.write('|--------|------|-----------|\n')
        for op, n in sorted(ours_only.items(), key=lambda kv: -kv[1])[:40]:
            f.write(f'| 0x{op:08X} | {name(op)} | {n} |\n')

    # Print concise summary
    housing_gap = [(op, ours.get(op, 0), merged_retail.get(op, 0)) for op in all_housing]
    missing = [x for x in housing_gap if x[1] == 0 and x[2] > 0]
    ratio_low = [x for x in housing_gap if x[1] > 0 and x[2] > 0 and x[1] * 10 < x[2]]
    print(f"Housing+Neighborhood opcodes: {len(all_housing)}")
    print(f"  Both emit: {len([x for x in housing_gap if x[1] > 0 and x[2] > 0])}")
    print(f"  Missing in ours (retail-only): {len(missing)}")
    for op, o, r in sorted(missing, key=lambda x: -x[2]):
        print(f"    0x{op:08X}  {name(op):<60}  retail={r}  ours={o}")
    print(f"  Severely under-emitted in ours (< 10% retail): {len(ratio_low)}")
    for op, o, r in sorted(ratio_low, key=lambda x: -x[2])[:10]:
        print(f"    0x{op:08X}  {name(op):<60}  retail={r}  ours={o}")
    print(f"\nOutput: docs/audit_2026_04_21/OPCODE_DIFF.md")
