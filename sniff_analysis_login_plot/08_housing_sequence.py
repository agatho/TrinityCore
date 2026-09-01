"""Extract and dump the HOUSING-related packet sequence from the sniff.

Focus: what the SERVER sends to the client from world-entry through plot-enter.
Inspect exact ordering, sizes, and embedded GUID/plot values where reachable.
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
OUT  = os.path.join(os.path.dirname(__file__), "out_housing_sequence.txt")

# Groups we care about for housing-related traffic
HOUSING_GROUPS = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E}

def group(op):
    return (op >> 16) & 0xFF

def hex_dump(b, max_bytes=128):
    if len(b) <= max_bytes:
        return b.hex(' ', 4)
    return b[:max_bytes].hex(' ', 4) + f' ... (+{len(b) - max_bytes} bytes)'

def main():
    packets = list(parse5.iter_packets(PATH))
    # Find transition markers
    marks = {}
    for p in packets:
        if p['opcode'] == 0x400016 and 'login' not in marks: marks['login'] = p['idx']
        if p['opcode'] == 0x42002F and 'verify_world' not in marks: marks['verify_world'] = p['idx']
        if p['opcode'] == 0x550000 and 'house_status' not in marks: marks['house_status'] = p['idx']

    # Range: from slightly before LOGIN to just after first HOUSE_STATUS_RESPONSE + follow-ups
    start_idx = marks.get('login', 0)
    # Stop idx: end = first ENTER_PLOT-related SMSG + 80 packets or first edit-mode CMSG
    first_edit_cmsg = None
    for p in packets:
        if p['opcode'] == 0x300000:  # CMSG_HOUSING_DECOR_SET_EDIT_MODE
            first_edit_cmsg = p['idx']
            break
    if first_edit_cmsg is None:
        first_edit_cmsg = marks.get('house_status', len(packets)) + 120
    end_idx = min(len(packets), first_edit_cmsg + 5)

    housing_packets = [p for p in packets[start_idx:end_idx]
                       if group(p['opcode']) in HOUSING_GROUPS
                       or p['opcode'] in {0x400016, 0x42002F, 0x42002B, 0x42002C, 0x3A001A,
                                          0x580000,  # UPDATE_OBJECT
                                          0x460012,  # QUERY_NEIGHBORHOOD_NAME_RESPONSE
                                          }]

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write(f"=== HOUSING-RELATED PACKETS (login → first edit mode) ===\n")
        w.write(f"File: {PATH}\n")
        w.write(f"Range in raw stream: idx [{start_idx}..{end_idx}]\n")
        w.write(f"Filter: opcode groups {sorted(HOUSING_GROUPS)} + a few transition opcodes\n")
        w.write(f"Total housing-ish packets: {len(housing_packets)}\n\n")
        w.write(f"Markers:\n")
        for k, v in marks.items():
            w.write(f"  {k:>16s}: packet idx {v}\n")
        w.write("\n")

        first_ts = packets[0]['ts']
        for p in housing_packets:
            name = opc6.name(p['opcode'])
            dt_us = p['ts'] - first_ts
            w.write(f"[{p['idx']:5d}] +{dt_us:>14d}us {p['dir']} 0x{p['opcode']:08X} size={p['size']:>6d}  {name}\n")
            w.write(f"          body: {hex_dump(p['body'])}\n")

    print(f'Wrote {OUT} ({len(housing_packets)} packets)')

if __name__ == '__main__':
    main()
