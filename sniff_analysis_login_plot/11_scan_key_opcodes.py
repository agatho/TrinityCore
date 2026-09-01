"""Count occurrences of key housing-related opcodes over the whole sniff.

We want to know:
- When SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT (0x5C0000) is sent, if at all
- All SMSG_HOUSING_* opcodes the server sends, and when
- All CMSG_HOUSING_* opcodes the client sends, and when
"""
import sys, os, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_key_opcodes_scan.txt")

# Real opcode names from TrinityCore Opcodes.h
NAMES = {
    0x400016: "CMSG_PLAYER_LOGIN",
    0x400015: "CMSG_WORLD_PORT_RESPONSE",
    0x42002F: "SMSG_LOGIN_VERIFY_WORLD",
    0x42002B: "SMSG_NEW_WORLD",
    0x42002C: "SMSG_TRANSFER_PENDING",
    0x420040: "SMSG_SUSPEND_TOKEN",
    0x420041: "SMSG_RESUME_TOKEN",
    0x420045: "SMSG_WORLD_SERVER_INFO",

    # Housing SMSG (group 0x55 / 0x56)
    0x550000: "SMSG_HOUSING_HOUSE_STATUS_RESPONSE",
    0x550001: "SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE",
    0x550002: "SMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT_RESPONSE",
    0x550003: "SMSG_HOUSING_EXPORT_HOUSE_RESPONSE",
    0x550004: "SMSG_HOUSING_UPDATE_HOUSE_INFO",
    0x550005: "SMSG_HOUSING_SET_HOUSE_NAME_RESPONSE",
    0x550006: "SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE",
    0x550007: "SMSG_HOUSING_RESET_KIOSK_MODE_RESPONSE",
    0x550008: "SMSG_HOUSING_EDITOR_AVAILABILITY_RESPONSE",

    # Neighborhood enter plot and related (group 0x5C)
    0x5C0000: "SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT",
    # Housing CMSG (group 0x35)
    0x350005: "CMSG_HOUSING_HOUSE_STATUS",
    0x350006: "CMSG_HOUSING_GET_CURRENT_HOUSE_INFO",
    0x350007: "CMSG_HOUSING_GET_PLAYER_PERMISSIONS",

    # Housing Decor / Fixture / Room CMSG + SMSG groups
    0x300000: "CMSG_HOUSING_DECOR_SET_EDIT_MODE",
    0x300001: "CMSG_HOUSING_DECOR_PLACE",
    0x300002: "CMSG_HOUSING_DECOR_MOVE",
    0x300003: "CMSG_HOUSING_DECOR_REMOVE",
    0x510000: "SMSG_HOUSING_DECOR_PLACE_RESPONSE",
    0x510007: "SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE",
    0x51000C: "SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE",

    0x310000: "CMSG_HOUSING_FIXTURE_SET_EDIT_MODE",
    0x520000: "SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE",
    0x320000: "CMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE",

    # Query responses
    0x460012: "SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE",
    0x460006: "SMSG_QUERY_CREATURE_RESPONSE",
    0x460007: "SMSG_QUERY_GAME_OBJECT_RESPONSE",

    # Moves
    0x5A0075: "SMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE",
    0x5A0000: "SMSG_TIME_SYNC_REQUEST",
    0x3A001A: "CMSG_MOVE_TELEPORT_ACK",
    0x3E0016: "SMSG_MOVE_TELEPORT",
}

TARGET_OPCODES = set(NAMES.keys()) | {
    0x560000, 0x560001, 0x560002, 0x560003, 0x560004, 0x560005, 0x560006, 0x560007, 0x560008,
    0x560009, 0x56000A, 0x56000B, 0x56000C, 0x56000D, 0x56000E, 0x56000F, 0x560010,
    0x56001B,
    # Common 0x5C group
    0x5C0001, 0x5C0002, 0x5C0003, 0x5C0004, 0x5C0005, 0x5C0006, 0x5C0007, 0x5C0008,
    0x5C0009, 0x5C000A, 0x5C000B, 0x5C000C, 0x5C000D, 0x5C000E, 0x5C000F,
}
# Also scan all housing CMSG/SMSG groups for unknown opcodes
HOUSING_GROUPS = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                   0x51, 0x52, 0x53, 0x54, 0x55, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D}
# 0x50 is HouseExterior SMSG group; 0x56 is HousingSystem but overlaps with LFG list opcodes

def group(op):
    return (op >> 16) & 0xFF

def main():
    packets = list(parse5.iter_packets(PATH))

    # Find map transitions
    transitions = []
    for p in packets:
        if p['opcode'] in (0x42002B, 0x42002F):
            mapid = 0
            if len(p['body']) >= 4:
                import struct
                mapid = struct.unpack_from('<I', p['body'], 0)[0]
            transitions.append((p['idx'], p['opcode'], mapid))

    # Count opcode occurrences in the whole sniff
    from collections import Counter
    counts = Counter(p['opcode'] for p in packets)

    # Find first/last idx of every housing opcode
    first_last = {}
    for p in packets:
        op = p['opcode']
        g = group(op)
        if g in HOUSING_GROUPS or op in TARGET_OPCODES:
            if op not in first_last:
                first_last[op] = [p['idx'], p['idx'], 1]
            else:
                first_last[op][1] = p['idx']
                first_last[op][2] += 1

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== KEY OPCODE SCAN (whole sniff) ===\n\n")
        w.write("Map transitions (SMSG_NEW_WORLD / SMSG_LOGIN_VERIFY_WORLD):\n")
        for idx, op, mid in transitions:
            name = NAMES.get(op, f"0x{op:08X}")
            w.write(f"  idx {idx:5d} {name} mapId={mid} (0x{mid:04X})\n")
        w.write("\n")

        # Specifically SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT
        ep = [p for p in packets if p['opcode'] == 0x5C0000]
        w.write(f"SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT (0x5C0000): {len(ep)} occurrences\n")
        for p in ep[:20]:
            w.write(f"  idx {p['idx']:5d} size={p['size']:>4d} body[0:32]={p['body'][:32].hex(' ', 4)}\n")
        w.write("\n")

        # SMSG_NEIGHBORHOOD_* (group 0x5C)
        w.write("SMSG NeighborhoodSystem group 0x5C opcodes:\n")
        for op in sorted(op for op in counts if group(op) == 0x5C):
            w.write(f"  0x{op:08X} ({counts[op]} times)\n")
        w.write("\n")

        # Housing group counts
        w.write("Housing-related opcodes (groups 0x30-0x39, 0x50-0x5D) — first..last idx, count:\n")
        for op, (first, last, cnt) in sorted(first_last.items()):
            name = NAMES.get(op, f"0x{op:08X}")
            w.write(f"  0x{op:08X} [{first:5d}..{last:5d}]  x{cnt:<4d}  {name}\n")

        w.write("\n")

        # Looking for a possible ENTER_PLOT analog: check group 0x51-0x55 first byte being PackedGUID128
        # Also check 0x500000 (HouseExterior)
        w.write("All opcodes in groups 0x50 (HouseExterior), 0x51 (Decor), 0x52 (Fixture), 0x53 (Room), 0x54 (Svcs), 0x55 (Housing), 0x58..0x5D:\n")
        for op in sorted(counts):
            g = group(op)
            if g in {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D}:
                name = NAMES.get(op, "?")
                w.write(f"  0x{op:08X} ({counts[op]:>4d}x)  {name}\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
