"""Dump the login sequence — from the first packets up to (and including)
the first SMSG_LOGIN_VERIFY_WORLD, plus the following ~50 packets so we
see everything the client expects between CMSG_PLAYER_LOGIN and the
player actually entering the neighborhood map.

Output: saved to ./out_login_flow.txt
"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))

# Import helpers
import importlib.util
spec = importlib.util.spec_from_file_location("parse5", os.path.join(os.path.dirname(__file__), "05_final_parse.py"))
parse5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(parse5)
spec = importlib.util.spec_from_file_location("opc6", os.path.join(os.path.dirname(__file__), "06_opcodes.py"))
opc6 = importlib.util.module_from_spec(spec); spec.loader.exec_module(opc6)

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT  = os.path.join(os.path.dirname(__file__), "out_login_flow.txt")

def main():
    packets = list(parse5.iter_packets(PATH))
    print(f'Total parsed: {len(packets)}')

    # Find first CMSG_PLAYER_LOGIN
    idx_login = None
    for p in packets:
        if p['opcode'] == 0x400016:
            idx_login = p['idx']
            break
    if idx_login is None:
        # Fallback: start from beginning
        idx_login = 0

    # Find first SMSG_LOGIN_VERIFY_WORLD
    idx_verify = None
    for p in packets:
        if p['opcode'] == 0x42002F:
            idx_verify = p['idx']
            break

    # Find first SMSG_HOUSING_HOUSE_STATUS_RESPONSE (= entered neighborhood)
    idx_house_status = None
    for p in packets:
        if p['opcode'] == 0x550000:
            idx_house_status = p['idx']
            break

    # Dump the first 250 packets, or from login up to ~house_status + 50
    end_idx = min(len(packets), (idx_house_status or len(packets)) + 50)

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write(f"=== LOGIN + ENTER PLOT FLOW ===\n")
        w.write(f"File: {PATH}\n")
        w.write(f"Total packets: {len(packets)}\n")
        w.write(f"CMSG_PLAYER_LOGIN   @idx {idx_login}\n")
        w.write(f"SMSG_LOGIN_VERIFY_WORLD @idx {idx_verify}\n")
        w.write(f"SMSG_HOUSING_HOUSE_STATUS_RESPONSE @idx {idx_house_status}\n")
        w.write(f"Dumping packets [0..{end_idx}]\n\n")

        first_ts = packets[0]['ts']
        for p in packets[:end_idx]:
            name = opc6.name(p['opcode'])
            dt_ms = (p['ts'] - first_ts) // 1_000_000  # WoW timestamps are microseconds? tune if needed
            w.write(f"[{p['idx']:5d}] t={p['ts']:>16d} {p['dir']} 0x{p['opcode']:08X} size={p['size']:>6d} {name}\n")

    print(f'Wrote {OUT}')

if __name__ == '__main__':
    main()
