"""Dump SMSG_INIT_WORLD_STATES and all individual SMSG_UPDATE_WORLD_STATE packets
during the exterior neighborhood map entry.

Map 2735 (Founder's Point). Looking for:
 - Per-plot ownership worldstates (plotID -> some value)
 - Per-plot house level / favor / name encoding
 - Anything that the client uses for the house-icon tooltip on the minimap
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")
PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT = os.path.join(os.path.dirname(__file__), "out_neighborhood_worldstates.txt")

def main():
    packets = list(parse5.iter_packets(PATH))

    # Find exterior transition
    exterior_idx = next((p['idx'] for p in packets if p['opcode'] == 0x42002B
                         and len(p['body']) >= 4
                         and struct.unpack_from('<I', p['body'], 0)[0] == 2735), None)
    print(f"Exterior map entry (map 2735) at idx {exterior_idx}")

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== SMSG_INIT_WORLD_STATES + SMSG_UPDATE_WORLD_STATE on neighborhood map entry ===\n\n")

        # SMSG_INIT_WORLD_STATES
        init_ws = [p for p in packets if p['opcode'] == 0x4201EE]
        for p in init_ws:
            body = p['body']
            w.write(f"[idx {p['idx']}] SMSG_INIT_WORLD_STATES size={p['size']} bodyLen={len(body)}\n")
            # Standard layout: uint32 mapID, uint32 areaID, uint32 subAreaID(?), uint32 count + count*(uint32 id, uint32 value)
            off = 0
            map_id = struct.unpack_from('<I', body, off)[0]; off += 4
            area_id = struct.unpack_from('<I', body, off)[0]; off += 4
            sub_area_id = struct.unpack_from('<I', body, off)[0]; off += 4
            count = struct.unpack_from('<I', body, off)[0]; off += 4
            w.write(f"  mapID=0x{map_id:04X}  areaID={area_id}  subAreaID={sub_area_id}  count={count}\n")
            # Each entry usually uint32 id + uint32 value
            if 4 + count*8 <= len(body) - 12:
                # Print first 40 entries and entries in plausible plot-ID range
                w.write("  First 40 worldstate entries:\n")
                for i in range(min(40, count)):
                    eid = struct.unpack_from('<I', body, off)[0]
                    val = struct.unpack_from('<I', body, off+4)[0]
                    off += 8
                    w.write(f"    id={eid:<10d} value={val:<12d} (0x{val:08X})\n")
            else:
                # Layout might have extra bits; just dump first 256 bytes
                w.write(f"  (count*8 exceeds body; body[:128]={body[:128].hex(' ')})\n")
            w.write("\n")

        # All SMSG_UPDATE_WORLD_STATE packets during exterior entry (idx 9964..10700)
        w.write("\n=== SMSG_UPDATE_WORLD_STATE packets idx 9964..10700 ===\n")
        ws_updates = [p for p in packets if p['opcode'] == 0x4201F0 and 9964 <= p['idx'] <= 10700]
        w.write(f"Count: {len(ws_updates)}\n\n")
        for p in ws_updates:
            body = p['body']
            # Actual layout: uint32 ID + int32 Value + 1 bit Hidden + FlushBits (9 bytes)
            if len(body) >= 9:
                ws_id = struct.unpack_from('<I', body, 0)[0]
                ws_val = struct.unpack_from('<i', body, 4)[0]
                hidden = (body[8] & 1) != 0
                w.write(f"[{p['idx']:5d}] id={ws_id:<10d} value={ws_val:<12d} (0x{ws_val:08X}) hidden={hidden}\n")

        # Also look for packets in the 0x5A group / 0x5C group that might carry plot info
        w.write("\n=== SMSG 0x5C group (NeighborhoodSystem) around exterior entry ===\n")
        for p in packets:
            if 9964 <= p['idx'] <= 10700 and (p['opcode'] >> 16) == 0x5C:
                w.write(f"[{p['idx']:5d}] 0x{p['opcode']:08X} size={p['size']} body[:64]={p['body'][:64].hex(' ')}\n")

        # And 0x50 group (HouseExterior) which might carry per-plot state
        w.write("\n=== SMSG 0x50 group (HouseExterior) around exterior entry ===\n")
        for p in packets:
            if 9964 <= p['idx'] <= 10700 and (p['opcode'] >> 16) == 0x50:
                w.write(f"[{p['idx']:5d}] 0x{p['opcode']:08X} size={p['size']} body[:64]={p['body'][:64].hex(' ')}\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
