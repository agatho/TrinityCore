"""Try to match the 667 IDs in 0x0056000E against known DB2 tables that might
be the source. Candidates:
 - HouseDecor
 - HouseDecorData
 - HouseRoom
 - HouseRoomComponent
 - ExteriorComponent
 - HousingMarket (new in 66838)

We dump the IDs to a file so they can be compared against any DB2 ID column.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")
PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT = os.path.join(os.path.dirname(__file__), "out_56000e_id_value_table.txt")

def main():
    packets = list(parse5.iter_packets(PATH))
    p = next((p for p in packets if p['opcode'] == 0x56000E), None)
    if not p:
        print("no 0x56000E packet")
        return
    body = p['body']
    count = struct.unpack_from('<I', body, 0)[0]

    rows = []
    for i in range(count):
        off = 4 + i*8
        u1 = struct.unpack_from('<I', body, off)[0]
        u2 = struct.unpack_from('<I', body, off+4)[0]
        rows.append((u1, u2))

    # Sort by u1
    rows_sorted = sorted(rows)

    # Check for gaps — if IDs are contiguous with gaps, this resembles a DB2 ID dump
    with open(OUT, 'w', encoding='utf-8') as w:
        w.write(f"0x0056000E contents: count={count}\n")
        w.write(f"Total bytes: {len(body)} (4 + {count}*8 = {4+count*8})\n\n")

        # Value histogram
        from collections import Counter
        val_counts = Counter(v for _, v in rows)
        w.write(f"Value distribution:\n")
        for v, c in val_counts.most_common():
            w.write(f"  value={v:5d} (0x{v:08X}): {c} occurrences\n")
        w.write("\n")

        # Full table (sorted by ID)
        w.write("Full (id, value) table sorted by id:\n")
        for id_, val in rows_sorted:
            w.write(f"  {id_:>6d}  →  {val:>3d}\n")

        # Also output just IDs as a comma-separated list for DB2 lookup
        w.write("\nIDs (sorted, comma-separated for DB2 query):\n")
        all_ids = sorted(set(r[0] for r in rows))
        w.write(",".join(str(i) for i in all_ids[:50]))
        w.write(f", ... (first 50 of {len(all_ids)})\n")

    # Also print summary to stdout
    print(f"Wrote {OUT}")
    print(f"ID range: {min(r[0] for r in rows)}..{max(r[0] for r in rows)}")
    print(f"Value histogram:")
    for v, c in val_counts.most_common():
        print(f"  value={v}: {c} ({c*100.0/count:.1f}%)")

if __name__ == '__main__':
    main()
