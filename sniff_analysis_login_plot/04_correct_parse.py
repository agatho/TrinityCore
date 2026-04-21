"""Correct WPP PKT v1.3 parser.

Per-packet layout (verified empirically):
    uint32  direction ('SMSG' or 'CMSG')
    uint32  connectionIdx
    uint32  arrivalTicks (ms since capture start)
    uint32  sniffInfoLen   (bytes of trailing session metadata AFTER payload)
    uint32  size           (payload size including opcode)
    bytes   payload        (size bytes; payload[0..3] = opcode)
    bytes   sniffInfo      (sniffInfoLen bytes, session-specific, after payload)
"""
import struct, sys, collections, os, json

def iter_packets(path, debug=False):
    with open(path, 'rb') as f:
        data = f.read()

    off = 72  # first SMSG tag
    idx = 0
    while off + 20 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            # Try to recover
            next_s = data.find(b'SMSG', off + 1)
            next_c = data.find(b'CMSG', off + 1)
            cands = [x for x in (next_s, next_c) if x > 0]
            if not cands:
                return
            off = min(cands)
            continue

        conn, ts, sniff_info_len, size = struct.unpack_from('<4I', data, off+4)

        if sniff_info_len > 4096:
            off += 1; continue
        if size > 20_000_000 or size < 0:
            off += 1; continue

        payload_start = off + 20
        payload_end = payload_start + size
        sniff_info_end = payload_end + sniff_info_len
        if sniff_info_end > len(data):
            return

        payload = data[payload_start:payload_end]
        opcode = struct.unpack_from('<I', payload, 0)[0] if size >= 4 else 0
        body = payload[4:] if size >= 4 else b''

        yield {
            'idx': idx,
            'off': off,
            'dir': tag.decode(),
            'conn': conn,
            'ts': ts,
            'size': size,
            'sniff_info_len': sniff_info_len,
            'opcode': opcode,
            'body': body,
            'payload_start': payload_start,
        }
        idx += 1
        off = sniff_info_end

if __name__ == '__main__':
    path = sys.argv[1]
    count = 0
    groups = collections.Counter()
    opcodes = collections.Counter()
    for p in iter_packets(path):
        count += 1
        groups[(p['opcode'] >> 16) & 0xFFFF] += 1
        opcodes[p['opcode']] += 1
    print(f'Total packets: {count:,}')
    print(f'\nTop opcode groups (high 16 bits):')
    for g, c in groups.most_common(25):
        print(f'  0x{g:04X}: {c:,}')
    print(f'\nTop opcodes:')
    for op, c in opcodes.most_common(25):
        print(f'  0x{op:08X}: {c:,}')
