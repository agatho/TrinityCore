"""Decode the two mystery bulk-state packets retail sends on EVERY map entry:
 - 0x0056000E (5344 bytes, count=667 in sniff) — sent first
 - 0x0056001B (30,082 bytes, count=145 rows of 204 bytes? in sniff) — sent second

Both are emitted WITHOUT a preceding CMSG; server pushes them proactively
after NEW_WORLD. If they look like worldstate data or feature flags we can
correlate with known TrinityCore opcodes; otherwise mark as housing-specific.

Dumps the full byte content of each occurrence to a file so we can eyeball
repeated row structures.
"""
import sys, os, struct, importlib.util
sys.path.insert(0, os.path.dirname(__file__))

def _load(name, file):
    spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), file))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

parse5 = _load("parse5", "05_final_parse.py")

PATH = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"
OUT = os.path.join(os.path.dirname(__file__), "out_bulk_state_decode.txt")

def hex_block(b, width=32):
    lines = []
    for i in range(0, len(b), width):
        row = b[i:i+width]
        hexs = " ".join(f"{c:02x}" for c in row)
        ascii_ = "".join(chr(c) if 32 <= c < 127 else "." for c in row)
        lines.append(f"  {i:05x}: {hexs:<{width*3}} | {ascii_}")
    return "\n".join(lines)

def try_row_analysis(body, row_size):
    """Assume body = uint32 count + (row_size)*count bytes. Return (count, match?)."""
    if len(body) < 4 + row_size:
        return None
    count = struct.unpack_from('<I', body, 0)[0]
    expected = 4 + count * row_size
    if expected == len(body):
        return (count, True)
    return (count, False)

def main():
    packets = list(parse5.iter_packets(PATH))
    occurrences_0E = [p for p in packets if p['opcode'] == 0x0056000E]
    occurrences_1B = [p for p in packets if p['opcode'] == 0x0056001B]

    with open(OUT, 'w', encoding='utf-8') as w:
        w.write("=== 0x0056000E and 0x0056001B bulk state decode ===\n\n")

        for p in occurrences_0E:
            body = p['body']
            w.write(f"-- 0x0056000E @ idx {p['idx']} size={p['size']} (body {len(body)} B) --\n")
            # Try row sizes
            for rs in (4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 52, 60, 64):
                r = try_row_analysis(body, rs)
                if r and r[1]:
                    w.write(f"   row-size {rs}: count={r[0]} FITS exactly\n")
            # First 128 bytes, then sample rows
            w.write("   first 128 bytes:\n")
            w.write(hex_block(body[:128]))
            w.write("\n")
            # If fits with row-size 8, print first 10 rows
            r8 = try_row_analysis(body, 8)
            if r8 and r8[1]:
                w.write(f"   count={r8[0]} rows @ 8 bytes:\n")
                for i in range(min(10, r8[0])):
                    off = 4 + i*8
                    u32a = struct.unpack_from('<I', body, off)[0]
                    u32b = struct.unpack_from('<I', body, off+4)[0]
                    w.write(f"     [{i:4d}] u32a={u32a} (0x{u32a:08X})  u32b={u32b} (0x{u32b:08X})\n")
            w.write("\n")

        for p in occurrences_1B:
            body = p['body']
            w.write(f"-- 0x0056001B @ idx {p['idx']} size={p['size']} (body {len(body)} B) --\n")
            # Try common row sizes
            for rs in (4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 60, 64, 72, 80, 96, 128, 160, 200, 204, 256):
                r = try_row_analysis(body, rs)
                if r and r[1]:
                    w.write(f"   row-size {rs}: count={r[0]} FITS exactly\n")
            # If count is at offset 0 as uint32, print first value
            if len(body) >= 4:
                count_guess = struct.unpack_from('<I', body, 0)[0]
                w.write(f"   first u32 (likely count) = {count_guess}\n")
                if count_guess > 0 and count_guess < 10000:
                    # derived row size
                    derived = (len(body) - 4) / count_guess
                    w.write(f"   derived row-size if count at [0] = {derived:.3f}\n")
            w.write("   first 256 bytes:\n")
            w.write(hex_block(body[:256]))
            w.write("\n   last 64 bytes:\n")
            w.write(hex_block(body[-64:]))
            w.write("\n\n")

    print(f"Wrote {OUT}")

if __name__ == '__main__':
    main()
