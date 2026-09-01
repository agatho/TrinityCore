#!/usr/bin/env python3
"""Heuristic but comprehensive inventory of entities in the map-entry
UPDATE_OBJECT packet. Scans for all byte offsets where a plausible CREATE
block starts (updateType 1/2 followed by a valid PackedGuid whose HighGuid
matches a known housing/entity type). Deduplicates by GUID.

Compared to script 35 (strict parser), this heuristic accepts some false
positives but gives near-complete coverage of the ~1200 entity CREATEs
retail packs into idx 9984. Script 35 only found 1.
"""
import os
import struct

OURS = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
RETAIL = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

HIGH_GUID_NAMES = {
    0: 'Null', 1: 'Uniq', 2: 'Player', 3: 'Item',
    6: 'GameObject', 7: 'Creature', 9: 'Pet',
    10: 'Vehicle', 11: 'AreaTrigger', 12: 'Conversation',
    13: 'LootObject', 14: 'MeshObject-old?', 15: 'WorldTransaction',
    16: 'SceneObject?', 17: 'StaticDoor', 20: 'WorldTransaction',
    21: 'StaticDoor', 22: 'Transport', 24: 'DynamicObject',
    25: 'DynamicDoor', 26: 'AIGroup', 27: 'AIGroup', 28: 'ClientActor',
    29: 'Uniq', 30: 'BNetAccount(renumbered?)', 32: 'Corpse?',
    40: 'BNetAccount', 50: 'ToolsClient', 51: 'WorldLayer',
    52: 'ArenaTeam', 53: 'LMMParty', 54: 'LMMLobby',
    55: 'Housing', 56: 'MeshObject', 57: 'Entity',
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


def read_packed_guid(body, off):
    if off+2 > len(body): return None, off
    m_lo, m_hi = body[off], body[off+1]
    off += 2
    lo = hi = 0
    for b in range(8):
        if m_lo & (1<<b):
            if off >= len(body): return None, off
            lo |= body[off] << (b*8); off += 1
    for b in range(8):
        if m_hi & (1<<b):
            if off >= len(body): return None, off
            hi |= body[off] << (b*8); off += 1
    return (lo, hi), off


def scan_target_packet(path, target_idx):
    """Returns dict: (HighGuid, objectType) -> set of (lo, hi)."""
    for idx, direction, op, body in iter_packets(path):
        if op != 0x00580000 or idx != target_idx:
            continue
        by_tuple = {}
        seen = set()
        for o in range(len(body) - 32):
            ut = body[o]
            if ut not in (1, 2): continue
            guid, end = read_packed_guid(body, o+1)
            if guid is None: continue
            lo, hi = guid
            if hi < (1 << 56): continue
            if lo == 0 and hi == 0: continue
            high = hi >> 58
            if high > 63: continue
            if end >= len(body): continue
            ot = body[end]
            key = (lo, hi)
            if key in seen: continue
            seen.add(key)
            by_tuple.setdefault((high, ot), []).append((o, lo, hi))
        return by_tuple
    return {}


def classify_role(high, ot):
    """Return a human-readable role name for (HighGuid, objectType) tuple."""
    if high == 55:  # Housing
        # subType lives in bits 53-57; but we can't recover it here without the subType byte on wire
        return f'Housing/ot={ot}'
    if high == 56:
        return f'MeshObject/ot={ot}'
    if high == 57:
        return f'Entity/ot={ot}'
    if high == 11 and ot == 8:
        return 'AreaTrigger (with housing fragment)'
    if high == 11:
        return f'AreaTrigger/ot={ot}'
    if high == 6:
        return f'GameObject/ot={ot}'
    if high == 7:
        return 'Creature'
    if high == 2:
        return f'Player/ot={ot}'
    if high == 3:
        return 'Item'
    if high == 40:
        return 'BNetAccount'
    return f'{HIGH_GUID_NAMES.get(high, f"H{high}")}/ot={ot}'


def emit(md, label, path, target_idx):
    md.append(f'## {label}\n\n')
    md.append(f'file: `{path}` packet idx={target_idx}\n\n')
    by_tuple = scan_target_packet(path, target_idx)
    total = sum(len(v) for v in by_tuple.values())
    md.append(f'**Total distinct entity CREATEs detected: {total}**\n\n')
    md.append('| role | count | GUIDs (up to 3 sample) |\n')
    md.append('|------|-------|------------------------|\n')
    by_role = {}
    for (high, ot), v in by_tuple.items():
        role = classify_role(high, ot)
        by_role.setdefault(role, [])
        by_role[role].extend(v)
    for role, v in sorted(by_role.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        samples = ', '.join(f'lo={l:x} hi={h:x}' for _, l, h in sorted(v)[:3])
        md.append(f'| {role} | {len(v)} | {samples} |\n')
    md.append('\n')
    return by_role


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    os.makedirs(outdir, exist_ok=True)
    md = ['# Map-entry UPDATE_OBJECT entity role inventory\n\n',
          '_Heuristic scan: counts distinct (lo, hi) GUIDs that look like valid CREATE headers._\n\n']
    r = emit(md, 'RETAIL (idx 9984, editor-session sniff)', RETAIL, 9984)
    o = emit(md, 'OURS (idx 295, 2026-04-21 22:31)', OURS, 295)
    md.append('## Role deficit (retail count - ours count)\n\n')
    md.append('| role | retail | ours | gap |\n')
    md.append('|------|-------:|-----:|----:|\n')
    all_roles = sorted(set(r.keys()) | set(o.keys()))
    deficits = []
    for role in all_roles:
        rc = len(r.get(role, []))
        oc = len(o.get(role, []))
        gap = rc - oc
        deficits.append((role, rc, oc, gap))
    deficits.sort(key=lambda x: (-x[3], x[0]))
    for role, rc, oc, gap in deficits:
        md.append(f'| {role} | {rc} | {oc} | {gap} |\n')
    with open(os.path.join(outdir, 'MAPENTRY_ROLE_INVENTORY.md'), 'w', encoding='utf-8') as f:
        f.writelines(md)
    print(f'Wrote {outdir}/MAPENTRY_ROLE_INVENTORY.md')
    print()
    print('Top role deficits:')
    for role, rc, oc, gap in deficits[:15]:
        if gap > 0:
            print(f'  {role:<40} retail={rc:<4} ours={oc:<4} gap=+{gap}')
