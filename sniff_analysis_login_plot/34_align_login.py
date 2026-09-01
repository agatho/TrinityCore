#!/usr/bin/env python3
"""Side-by-side alignment of login/map-entry packet streams between retail and
our server. Anchored on the first big SMSG_UPDATE_OBJECT (>40KB), shows 50
packets before and 200 after on both sides, with a greedy matcher that advances
one pointer at a time when the two streams diverge.

Output: docs/audit_2026_04_21/FLOW_COMPARISON.md
"""
import os
import struct
import sys

# ---------------------------------------------------------------------------
# Opcode name table (copied/extended from 33_opcode_sequence.py)
# ---------------------------------------------------------------------------
OP_NAMES = {
    # Connection / auth
    0x00620003: 'SMSG_CONNECT_TO',
    0x00620011: 'SMSG_AURA_UPDATE',
    0x0062002C: 'SMSG_SPELL_GO',
    0x0062002D: 'SMSG_SPELL_START',
    0x00620049: 'SMSG_CAST_FAILED',
    # Player state
    0x00580000: 'SMSG_UPDATE_OBJECT',
    0x00580001: 'SMSG_DESTROY_OBJECT',
    # Movement
    0x004B0000: 'SMSG_MOVE_UPDATE',
    # World state
    0x004201EE: 'SMSG_INIT_WORLD_STATES',
    0x004201EF: 'SMSG_UPDATE_WORLD_STATE',
    0x0042036A: 'SMSG_MIRROR_VARS',
    # Housing: HouseExteriorSystem (0x2E)
    0x002E0000: 'SMSG_HOUSE_EXTERIOR_0',
    # HousingDecorSystem (0x30)
    0x00300000: 'CMSG_HOUSING_DECOR_SET_EDIT_MODE',
    0x00300001: 'SMSG_HOUSING_DECOR_PLACE_RESPONSE',
    0x00300002: 'CMSG_HOUSING_DECOR_MOVE',
    0x00300004: 'CMSG_HOUSING_DECOR_LOCK',
    0x0030000E: 'CMSG_HOUSING_DECOR_REQUEST_STORAGE',
    0x00300010: 'SMSG_HOUSING_DECOR_STORAGE_RSP',
    # HousingFixtureSystem (0x31)
    0x00310000: 'CMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE',
    0x00310001: 'SMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE_RSP',
    0x00310009: 'SMSG_HOUSING_FIXTURE_DEFAULT_COMPONENT_TYPES',
    # HousingRoomSystem (0x32)
    0x00320000: 'SMSG_HOUSING_ROOM_0',
    # HousingServicesSystem (0x33)
    0x00330000: 'SMSG_HOUSING_SERVICES_0',
    # HousingSystem (0x35)
    0x00350005: 'CMSG_HOUSING_HOUSE_STATUS',
    0x00350006: 'CMSG_HOUSING_GET_CURRENT_HOUSE_INFO',
    0x00350007: 'CMSG_HOUSING_GET_PLAYER_PERMISSIONS',
    # NeighborhoodSystem (0x39)
    0x00390012: 'SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT',
    # NeighborhoodInitiativeSystem (0x38)
    0x00380000: 'CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK',
    # Account entity
    0x00560009: 'SMSG_ACCOUNT_DATA_UPDATE',
    # Common setup
    0x003D0001: 'SMSG_SEND_KNOWN_SPELLS',
    0x0047000A: 'SMSG_WORLD_SERVER_INFO',
    # Query
    0x00480006: 'SMSG_QUERY_PLAYER_NAME_RESPONSE',
    0x00460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x005C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
}


def classify(opcode):
    group = (opcode >> 16) & 0xFFFF
    sub = opcode & 0xFFFF
    groups = {
        0x002E: 'HouseExteriorSystem', 0x002F: 'HouseInteriorSystem',
        0x0030: 'HousingDecorSystem', 0x0031: 'HousingFixtureSystem',
        0x0032: 'HousingRoomSystem', 0x0033: 'HousingServicesSystem',
        0x0035: 'HousingSystem', 0x0037: 'NeighborhoodCharterSystem',
        0x0038: 'NeighborhoodInitiativeSystem', 0x0039: 'NeighborhoodSystem',
        0x0050: 'CMSG_HouseExteriorSystem', 0x0051: 'CMSG_HousingDecorSystem',
        0x0052: 'CMSG_HousingFixtureSystem', 0x0053: 'CMSG_HousingRoomSystem',
        0x0054: 'CMSG_HousingServicesSystem', 0x0056: 'CMSG_HousingSystem',
        0x0058: 'CMSG_NeighborhoodCharterSystem', 0x0059: 'CMSG_NeighborhoodInitiativeSystem',
        0x005A: 'CMSG_NeighborhoodSystem',
    }
    if group in groups:
        return f"{groups[group]}[{sub:04X}]"
    return None


def op_name(op):
    return OP_NAMES.get(op) or classify(op) or 'Unknown'


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


def load_all(path):
    """Return list of (idx, dir, op, name, size)."""
    out = []
    for idx, direction, op, dlen in iter_packets(path):
        out.append((idx, direction, op, op_name(op), dlen))
    return out


def find_anchor(packets):
    """First SMSG_UPDATE_OBJECT (0x00580000) with size > 40KB."""
    for p in packets:
        if p[1] == 'SMSG' and p[2] == 0x00580000 and p[4] > 40 * 1024:
            return p[0]
    return None


# ---------------------------------------------------------------------------
# Greedy aligner
# ---------------------------------------------------------------------------
# Matching key: (direction, opcode). We do NOT require size match because
# fragmentation varies, but the pair identifies the packet type.

def key(p):
    return (p[1], p[2])


def align(retail, ours, window=20):
    """Walk both lists, produce rows of (retail|None, ours|None, diff).

    Greedy rule: if keys match -> emit matched row, advance both.
    Otherwise look for retail[i] key in ours[j:j+window] and ours[j] key in
    retail[i:i+window]. Whichever search finds a closer match, emit the
    *_ONLY rows for the packets we skip over, then the matched row.
    If neither finds a match, emit both as *_ONLY (so streams stay aligned
    on count).
    """
    i, j = 0, 0
    rows = []
    while i < len(retail) and j < len(ours):
        if key(retail[i]) == key(ours[j]):
            rows.append((retail[i], ours[j], '='))
            i += 1
            j += 1
            continue

        # Look ahead in each side for the other's current key.
        rk = key(retail[i])
        ok = key(ours[j])
        r_forward = None  # position in ours where retail[i].key appears
        o_forward = None  # position in retail where ours[j].key appears
        for k in range(1, window + 1):
            if r_forward is None and j + k < len(ours) and key(ours[j + k]) == rk:
                r_forward = k
            if o_forward is None and i + k < len(retail) and key(retail[i + k]) == ok:
                o_forward = k
            if r_forward is not None and o_forward is not None:
                break

        if r_forward is None and o_forward is None:
            # Streams have wholly diverged at this position — emit both sides.
            rows.append((retail[i], None, 'RETAIL_ONLY'))
            rows.append((None, ours[j], 'OURS_ONLY'))
            i += 1
            j += 1
        elif r_forward is not None and (o_forward is None or r_forward <= o_forward):
            # retail[i] matches ours[j+r_forward] -> emit OURS_ONLY for ours[j..j+r_forward-1]
            for k in range(r_forward):
                rows.append((None, ours[j + k], 'OURS_ONLY'))
            j += r_forward
            rows.append((retail[i], ours[j], '='))
            i += 1
            j += 1
        else:
            # ours[j] matches retail[i+o_forward] -> emit RETAIL_ONLY for retail[i..i+o_forward-1]
            for k in range(o_forward):
                rows.append((retail[i + k], None, 'RETAIL_ONLY'))
            i += o_forward
            rows.append((retail[i], ours[j], '='))
            i += 1
            j += 1

    # Drain any remaining.
    while i < len(retail):
        rows.append((retail[i], None, 'RETAIL_ONLY'))
        i += 1
    while j < len(ours):
        rows.append((None, ours[j], 'OURS_ONLY'))
        j += 1
    return rows


def fmt_cell(p):
    if p is None:
        return ('', '', '', '', '')
    idx, direction, op, name, size = p
    return (str(idx), direction, f"0x{op:08X}", name, str(size))


def render(retail, ours, retail_window, ours_window, rows):
    """Return markdown string."""
    # Delta summary
    retail_only = {}
    ours_only = {}
    first_divergence = None
    matched = 0
    for r, o, diff in rows:
        if diff == '=':
            matched += 1
        elif diff == 'RETAIL_ONLY':
            retail_only[r[3]] = retail_only.get(r[3], 0) + 1
            if first_divergence is None:
                first_divergence = (r[0], None, r[3], None, 'RETAIL_ONLY')
        elif diff == 'OURS_ONLY':
            ours_only[o[3]] = ours_only.get(o[3], 0) + 1
            if first_divergence is None:
                first_divergence = (None, o[0], None, o[3], 'OURS_ONLY')

    out = []
    out.append('# Login/Map-Entry Flow Comparison (retail vs ours)')
    out.append('')
    out.append(f"- Retail sniff: `c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt`")
    out.append(f"- Ours sniff:   `C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt`")
    out.append('')
    out.append(f"Retail window: {len(retail_window)} packets (idx {retail_window[0][0]}..{retail_window[-1][0]})")
    out.append(f"Ours   window: {len(ours_window)} packets (idx {ours_window[0][0]}..{ours_window[-1][0]})")
    out.append(f"Matched rows: {matched}  |  RETAIL_ONLY rows: {sum(retail_only.values())}  |  OURS_ONLY rows: {sum(ours_only.values())}")
    out.append('')

    if first_divergence is not None:
        out.append(f"**First divergence:** {first_divergence}")
    else:
        out.append("**First divergence:** (none — streams aligned within window)")
    out.append('')

    out.append('## Only in retail (grouped)')
    out.append('')
    out.append('| count | opcode name |')
    out.append('|------:|:------------|')
    for n, c in sorted(retail_only.items(), key=lambda kv: -kv[1]):
        out.append(f"| {c} | {n} |")
    if not retail_only:
        out.append('| - | (none) |')
    out.append('')

    out.append('## Only in ours (grouped)')
    out.append('')
    out.append('| count | opcode name |')
    out.append('|------:|:------------|')
    for n, c in sorted(ours_only.items(), key=lambda kv: -kv[1]):
        out.append(f"| {c} | {n} |")
    if not ours_only:
        out.append('| - | (none) |')
    out.append('')

    out.append('## Side-by-side alignment')
    out.append('')
    out.append('| retail_idx | r_dir | r_op | r_name | r_size | ours_idx | o_dir | o_op | o_name | o_size | DIFF |')
    out.append('|-----------:|:-----:|:-----|:-------|------:|---------:|:-----:|:-----|:-------|------:|:-----|')
    for r, o, diff in rows:
        ri, rd, ro, rn, rs = fmt_cell(r)
        oi, od, oo, on, os = fmt_cell(o)
        out.append(f"| {ri} | {rd} | {ro} | {rn} | {rs} | {oi} | {od} | {oo} | {on} | {os} | {diff} |")

    return '\n'.join(out), matched, retail_only, ours_only, first_divergence


def main():
    retail_path = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'
    ours_path = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
    out_path = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21/FLOW_COMPARISON.md'
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    print(f"Loading retail: {retail_path}")
    retail = load_all(retail_path)
    print(f"  -> {len(retail)} packets")
    print(f"Loading ours:   {ours_path}")
    ours = load_all(ours_path)
    print(f"  -> {len(ours)} packets")

    a_r = find_anchor(retail)
    a_o = find_anchor(ours)
    print(f"Anchor retail idx: {a_r}   anchor ours idx: {a_o}")
    if a_r is None or a_o is None:
        print("ERROR: could not locate anchor (SMSG_UPDATE_OBJECT >40KB) in one of the sniffs")
        sys.exit(1)

    pre, post = 50, 200
    r_lo = max(0, a_r - pre)
    r_hi = min(len(retail), a_r + post + 1)
    o_lo = max(0, a_o - pre)
    o_hi = min(len(ours), a_o + post + 1)
    retail_window = retail[r_lo:r_hi]
    ours_window = ours[o_lo:o_hi]

    # Two-segment alignment: PRE (aligned backward from anchor) + POST (aligned
    # forward from anchor). This seeds both streams at the known anchor so we
    # don't get random mismatches from the wildly different pre-anchor phases.
    retail_pre = retail[r_lo:a_r][::-1]
    ours_pre = ours[o_lo:a_o][::-1]
    retail_post = retail[a_r:r_hi]
    ours_post = ours[a_o:o_hi]

    pre_rows = align(retail_pre, ours_pre, window=20)
    pre_rows = list(reversed(pre_rows))  # restore chronological order
    post_rows = align(retail_post, ours_post, window=20)
    rows = pre_rows + post_rows
    md, matched, retail_only, ours_only, first_div = render(retail, ours, retail_window, ours_window, rows)

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(md)
    print(f"\nWrote {out_path} ({len(md)} bytes, {len(rows)} rows)")

    # stdout summary (<40 lines)
    print()
    print('=== SUMMARY ===')
    print(f"Retail anchor idx: {a_r} ({retail[a_r][4]} bytes)")
    print(f"Ours   anchor idx: {a_o} ({ours[a_o][4]} bytes)")
    print(f"Window sizes: retail={len(retail_window)}, ours={len(ours_window)}")
    print(f"Rows: {len(rows)}  matched={matched}  retail_only={sum(retail_only.values())}  ours_only={sum(ours_only.values())}")
    print()
    print('Top RETAIL_ONLY (packets retail sends that we do not):')
    for n, c in sorted(retail_only.items(), key=lambda kv: -kv[1])[:10]:
        print(f"  {c:5}  {n}")
    if not retail_only:
        print('  (none)')
    print()
    print('Top OURS_ONLY (packets we send that retail does not):')
    for n, c in sorted(ours_only.items(), key=lambda kv: -kv[1])[:10]:
        print(f"  {c:5}  {n}")
    if not ours_only:
        print('  (none)')
    print()
    if first_div:
        print(f"First divergence: {first_div}")


if __name__ == '__main__':
    main()
