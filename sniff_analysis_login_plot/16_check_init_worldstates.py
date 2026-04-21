"""Verify whether retail sends SMSG_INIT_WORLD_STATES (0x4201EE) separately
from the 0x0056000E bulk blob, or the 0x0056000E IS the new init packet.

Also scan for 0x5D group (HouseExterior or other) to identify missing Housing sub-opcodes.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

def main():
    packets = list(parse5.iter_packets(PATH))
    from collections import Counter

    # Check for SMSG_INIT_WORLD_STATES 0x4201EE
    init_ws = [p for p in packets if p['opcode'] == 0x4201EE]
    print(f"SMSG_INIT_WORLD_STATES (0x4201EE): {len(init_ws)} occurrences")
    for p in init_ws[:5]:
        print(f"  idx {p['idx']:5d} size={p['size']} body[:32]={p['body'][:32].hex(' ')}")

    # Check if 0x0056000E actually looks like worldstate init format
    # Body: uint32 count + count*(uint32 id, uint32 value)
    # For worldstate init, values are seen to be small ints.
    # Let's also check if the IDs are in valid TrinityCore worldstate ID range.
    print("\n0x0056000E body content patterns:")
    for opcode in (0x56000E, 0x56001B, 0x500000):
        matches = [p for p in packets if p['opcode'] == opcode]
        print(f"\n  Opcode 0x{opcode:06X}: {len(matches)} matches")
        for p in matches[:3]:
            b = p['body']
            print(f"    idx {p['idx']} size={p['size']} body[:16]={b[:16].hex(' ')}")
            if len(b) >= 8:
                count = struct.unpack_from('<I', b, 0)[0]
                print(f"      first u32 (count?) = {count}")
                if opcode == 0x56000E and len(b) == 4 + count*8:
                    # Print all unique u32a values as sorted list
                    ids = sorted(set(struct.unpack_from('<I', b, 4 + i*8)[0] for i in range(count)))
                    print(f"      {len(ids)} unique ids, range {ids[0]}..{ids[-1]}")
                    print(f"      sample ids: {ids[:20]}")
                    # Count unique values
                    vals = Counter(struct.unpack_from('<I', b, 4 + i*8 + 4)[0] for i in range(count))
                    print(f"      value distribution: {vals.most_common(10)}")

    # Also check opcode 0x500000 (HouseExterior start) which appears 2x
    # Look for relation to housing maps
    print("\n0x500000 (HouseExterior group 0) contents:")
    matches = [p for p in packets if p['opcode'] == 0x500000]
    for p in matches:
        b = p['body']
        print(f"  idx {p['idx']} size={p['size']} body={b[:64].hex(' ')}{'...' if len(b)>64 else ''}")

if __name__ == '__main__':
    main()
