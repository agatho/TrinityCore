"""Minimal WPP-compatible .pkt reader for build 66709 (V3_1, ymir/sniffer 0x15-0x16).

Outputs one line per packet:  TIME_MS  DIR  OPCODE_HEX  NAME  LEN  PAYLOAD_HEX_PREVIEW

Filters: positional arg `--filter` accepts comma-separated opcode names or substring.
Saves payloads to `--bin-dir` if set (one file per packet, named by index).
"""
from __future__ import annotations
import argparse, os, re, struct, sys
from typing import Dict

OPCODES_H = r"L:\TrinityCore\skyriding\TrinityCore\src\server\game\Server\Protocol\Opcodes.h"

def load_opcode_names() -> Dict[int, str]:
    names: Dict[int, str] = {}
    rx = re.compile(r"^\s*((?:CMSG|SMSG)_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)\s*,?\s*$")
    with open(OPCODES_H, encoding='utf-8') as f:
        for line in f:
            m = rx.match(line)
            if not m:
                continue
            names[int(m.group(2), 16)] = m.group(1)
    return names

class PktReader:
    def __init__(self, path: str):
        self.f = open(path, 'rb')
        self.path = path
        self.start_time = 0
        self.start_tick = 0
        self.sniffer_id = 0
        self.build = 0
        self._read_header()

    def _u16(self):
        return struct.unpack('<H', self.f.read(2))[0]

    def _u32(self):
        return struct.unpack('<I', self.f.read(4))[0]

    def _i32(self):
        return struct.unpack('<i', self.f.read(4))[0]

    def _read_header(self):
        magic = self.f.read(3)
        if magic != b'PKT':
            raise RuntimeError(f"not a PKT file: {magic!r}")
        ver = self._u16()
        if ver != 0x301:
            raise RuntimeError(f"unsupported PKT version 0x{ver:04X}")
        self.sniffer_id = self.f.read(1)[0]
        self.build = self._u32()
        self.locale = self.f.read(4).decode('ascii', 'replace')
        self.f.read(40)  # session key
        self.start_time = self._u32()
        self.start_tick = self._u32()
        addl_len = self._i32()
        self.f.read(addl_len)  # ignore optional header data

    def read_packet(self):
        hdr = self.f.read(4)
        if len(hdr) < 4:
            return None
        if hdr == b'SMSG':
            direction = 'S2C'
        elif hdr == b'CMSG':
            direction = 'C2S'
        else:
            direction = hdr.decode('ascii', 'replace')
        cidx = self._i32()
        tick = self._u32()
        addl_size = self._i32()
        length = self._i32()
        # ymir 0x15/0x16: read additional and ignore (don't decode timestamp here)
        if addl_size > 0:
            self.f.read(addl_size)
        opcode = self._u32()
        body = self.f.read(length - 4)
        return {
            'direction': direction,
            'cidx': cidx,
            'tick_ms': tick - self.start_tick,
            'opcode': opcode,
            'length': length - 4,
            'body': body,
        }

    def __iter__(self):
        i = 0
        while True:
            p = self.read_packet()
            if p is None:
                return
            p['index'] = i
            i += 1
            yield p

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pkt')
    ap.add_argument('--filter', default='', help='comma-separated opcode names or substrings (case-insensitive)')
    ap.add_argument('--bin-dir', default='', help='write each matching packet body to bin-dir/<idx>_<name>.bin')
    ap.add_argument('--max', type=int, default=0, help='stop after N matches')
    ap.add_argument('--preview', type=int, default=64, help='hex preview length')
    args = ap.parse_args()

    names = load_opcode_names()
    needles = [n.strip().lower() for n in args.filter.split(',') if n.strip()]

    if args.bin_dir:
        os.makedirs(args.bin_dir, exist_ok=True)

    reader = PktReader(args.pkt)
    print(f"# build={reader.build} sniffer=0x{reader.sniffer_id:02X} locale={reader.locale}")
    matches = 0
    total = 0
    for pkt in reader:
        total += 1
        name = names.get(pkt['opcode'], f"UNK_0x{pkt['opcode']:08X}")
        if needles and not any(n in name.lower() for n in needles):
            continue
        preview = pkt['body'][:args.preview].hex()
        print(f"{pkt['tick_ms']:>10} ms  {pkt['direction']}  0x{pkt['opcode']:08X}  {name}  len={pkt['length']}  {preview}")
        if args.bin_dir:
            safe = re.sub(r'\W+', '_', name)
            with open(os.path.join(args.bin_dir, f"{pkt['index']:06d}_{safe}.bin"), 'wb') as f:
                f.write(pkt['body'])
        matches += 1
        if args.max and matches >= args.max:
            break
    print(f"# total_packets={total} matches={matches}", file=sys.stderr)

if __name__ == '__main__':
    main()
