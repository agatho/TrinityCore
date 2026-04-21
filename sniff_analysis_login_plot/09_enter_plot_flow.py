"""Extract and dump the EXTERIOR plot-enter flow.

Focus: from SMSG_NEW_WORLD (map 2735 Founder's Point) onward,
dump everything the SERVER sends until the first decor edit mode / or ~300 packets later.

This is the critical flow for the "grey cursor on cornerstone/door at login" issue
and for validating AT + GameObject CREATE packets that set up the owned plot.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
import importlib.util

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")
opc6   = _load("opc6",   "06_opcodes.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_enter_plot_flow.txt")

# Extended interesting groups
INTERESTING_GROUPS = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                       0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E,
                       0x46, 0x42, 0x3A, 0x3E}

TRANSITION_OPCODES = {
    0x400016, 0x42002F, 0x42002B, 0x42002C, 0x400015, 0x3A001A, 0x3E0016,
    0x580000, 0x580001, 0x460012,
}

def group(op):
    return (op >> 16) & 0xFF

def hex_dump(b, max_bytes=256):
    if len(b) <= max_bytes:
        return b.hex(' ', 4)
    return b[:max_bytes].hex(' ', 4) + f' ... (+{len(b) - max_bytes} bytes)'

def main():
    packets = list(parse5.iter_packets(PATH))
    print(f"Total parsed: {len(packets)}")

    # Find ALL SMSG_NEW_WORLD occurrences — may be multiple (interior→exterior→interior)
    new_worlds = []
    for p in packets:
        if p['opcode'] == 0x42002B:
            # body is: mapId (u32) + (pos?) + ...
            if len(p['body']) >= 4:
                map_id = struct.unpack_from('<I', p['body'], 0)[0]
                new_worlds.append((p['idx'], map_id, p['body'][:32]))

    # Find exterior transition — map 2735 (0xAAF) = Founder's Point
    exterior_idx = None
    for idx, map_id, _ in new_worlds:
        if map_id == 2735:
            exterior_idx = idx
            break

    # Find first CMSG_WORLD_PORT_RESPONSE that follows exterior NEW_WORLD
    ack_idx = None
    teleport_ack_idx = None
    enter_plot_smsg = None
    for p in packets:
        if exterior_idx is None:
            break
        if p['idx'] < exterior_idx:
            continue
        if p['opcode'] == 0x400015 and ack_idx is None:
            ack_idx = p['idx']
        if p['opcode'] == 0x3A001A and teleport_ack_idx is None:
            teleport_ack_idx = p['idx']
        # SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT = 0x5C000E
        if p['opcode'] == 0x5C000E and enter_plot_smsg is None:
            enter_plot_smsg = p['idx']

    # Find first CMSG_HOUSING_DECOR_SET_EDIT_MODE after exterior entry
    first_edit_cmsg = None
    for p in packets:
        if exterior_idx is None:
            break
        if p['idx'] < exterior_idx:
            continue
        if p['opcode'] == 0x300000:
            first_edit_cmsg = p['idx']
            break

    start_idx = max(0, (exterior_idx or 0) - 3)
    end_idx = first_edit_cmsg + 10 if first_edit_cmsg else min(len(packets), (exterior_idx or 0) + 400)

    selected = []
    for p in packets[start_idx:end_idx]:
        g = group(p['opcode'])
        if g in INTERESTING_GROUPS or p['opcode'] in TRANSITION_OPCODES:
            selected.append(p)

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write(f"=== EXTERIOR PLOT-ENTER FLOW ===\n")
        w.write(f"File: {PATH}\n")
        w.write(f"Total packets: {len(packets)}\n\n")

        w.write(f"All SMSG_NEW_WORLD occurrences:\n")
        for idx, map_id, head in new_worlds:
            map_name = {2735: "Founder's Point (exterior)",
                        2736: "Razorwind Shores (exterior)",
                        2783: "Home Interior"}.get(map_id, "?")
            w.write(f"  idx {idx:5d}: mapId=0x{map_id:04X} ({map_id}) {map_name}  head={head.hex(' ')}\n")
        w.write("\n")

        w.write(f"exterior_transition_idx (map 2735): {exterior_idx}\n")
        w.write(f"CMSG_WORLD_PORT_RESPONSE @idx {ack_idx}\n")
        w.write(f"CMSG_MOVE_TELEPORT_ACK @idx {teleport_ack_idx}\n")
        w.write(f"SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT @idx {enter_plot_smsg}\n")
        w.write(f"first CMSG_HOUSING_DECOR_SET_EDIT_MODE @idx {first_edit_cmsg}\n")
        w.write(f"Dumping [{start_idx}..{end_idx}] filtered: {len(selected)} packets\n\n")

        first_ts = packets[0]['ts']
        prev_ts = None
        for p in selected:
            name = opc6.name(p['opcode'])
            dt_us = p['ts'] - first_ts
            delta_us = (p['ts'] - prev_ts) if prev_ts else 0
            prev_ts = p['ts']
            w.write(f"[{p['idx']:5d}] +{dt_us:>14d}us Δ{delta_us:>10d}us {p['dir']} 0x{p['opcode']:08X} size={p['size']:>6d}  {name}\n")
            w.write(f"          body: {hex_dump(p['body'])}\n")

    print(f'Wrote {OUT} ({len(selected)} packets)')

if __name__ == '__main__':
    main()
