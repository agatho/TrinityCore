#!/usr/bin/env python3
"""Dump the big 0x5F0027 packet at idx 6969 to see what the vignette system
carries right after the player enters a neighborhood map."""
import struct

PKT = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    first_tag = min(x for x in (data.find(b'SMSG', 0, 256), data.find(b'CMSG', 0, 256)) if x > 0)
    off = first_tag
    idx = 0
    while off + 4 + 25 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            next_s = data.find(b'SMSG', off + 1)
            next_c = data.find(b'CMSG', off + 1)
            cands = [x for x in (next_s, next_c) if x > 0]
            if not cands: return
            off = min(cands); continue
        header = data[off+4:off+4+25]
        conn, f2, f3, dlen = struct.unpack_from('<IIII', header, 0)
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        payload_start = off + 4 + 25
        payload_end = payload_start + dlen
        if payload_end > len(data): return
        opcode = struct.unpack_from('<I', data, payload_start)[0]
        body = data[payload_start+4:payload_end]
        yield idx, tag.decode(), opcode, body
        off = payload_end
        idx += 1


def hexdump(b, width=16):
    out = []
    for i in range(0, len(b), width):
        chunk = b[i:i+width]
        hex_ = ' '.join(f'{x:02x}' for x in chunk)
        ascii_ = ''.join(chr(x) if 32 <= x < 127 else '.' for x in chunk)
        out.append(f'  {i:04x}: {hex_:<48}  {ascii_}')
    return '\n'.join(out)


def main():
    targets = {6620, 6969, 1043}
    for idx, dir_, op, body in iter_packets(PKT):
        if idx in targets:
            print(f"=== idx={idx}  {dir_}  op=0x{op:08X}  size={len(body)} ===")
            print(hexdump(body[:512]))
            print()


if __name__ == "__main__":
    main()
