#!/usr/bin/env python3
"""Extract the full chronological opcode sequence from a sniff, annotated with
direction, size, and a friendly name for the most common TC opcodes.

Used as the baseline for sniff-vs-sniff flow comparison. Writes:
  - <out>/<label>_opcodes.txt : one line per packet with idx, dir, opcode hex,
    name, size
  - <out>/<label>_opcode_counts.txt : histogram by opcode name
"""
import os
import struct
import sys

# Known TC opcodes (subset — housing-relevant + common setup).
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
    # Group by high 16 bits
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
        yield idx, tag.decode(), op, dlen, ps
        off = pe
        idx += 1


def process(path, label, outdir):
    out_seq = os.path.join(outdir, f'{label}_opcodes.txt')
    out_hist = os.path.join(outdir, f'{label}_opcode_counts.txt')
    counts = {}
    group_counts = {}
    with open(out_seq, 'w', encoding='utf-8') as f:
        f.write(f"# {path}\n")
        f.write(f"# idx\tdir\topcode\tname\tsize\n")
        for idx, direction, op, dlen, pos in iter_packets(path):
            name = OP_NAMES.get(op) or classify(op) or f'Unknown'
            f.write(f"{idx}\t{direction}\t0x{op:08X}\t{name}\t{dlen}\n")
            counts[name] = counts.get(name, 0) + 1
            grp = (op >> 16) & 0xFFFF
            group_counts[grp] = group_counts.get(grp, 0) + 1
    with open(out_hist, 'w', encoding='utf-8') as f:
        f.write(f"# histogram: {path}\n")
        f.write(f"# Total packets: {sum(counts.values())}\n\n")
        f.write("## by name\n")
        for name, n in sorted(counts.items(), key=lambda kv: -kv[1]):
            f.write(f"{n:5}  {name}\n")
        f.write("\n## by opcode group (high 16 bits)\n")
        for grp, n in sorted(group_counts.items()):
            f.write(f"{n:5}  0x{grp:04X}\n")
    print(f"[{label}] wrote {out_seq} and {out_hist} — {sum(counts.values())} packets")


if __name__ == '__main__':
    OUTDIR = r'c:\TrinityBots\wt\housing-system\docs\audit_2026_04_21'
    os.makedirs(OUTDIR, exist_ok=True)
    targets = [
        (r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt', 'OUR_22-31'),
        (r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_18-11-22.pkt', 'OUR_18-11'),
        (r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt', 'RETAIL_editor'),
        (r'c:/sniff/floorplan_editor_rotation/dumps/dump_12.0.1.66838_2026-04-10_08-45-23.pkt', 'RETAIL_floorplan'),
        (r'c:/sniff/wall_floor_ceiling_customize/dumps/dump_12.0.1.66838_2026-04-12_10-11-26.pkt', 'RETAIL_wallcustomize'),
    ]
    for path, label in targets:
        if os.path.exists(path):
            process(path, label, OUTDIR)
        else:
            print(f"MISSING: {path}")
