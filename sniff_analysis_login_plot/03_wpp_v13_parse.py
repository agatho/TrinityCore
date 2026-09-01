"""WPP PKT v1.3 parser using the correct per-packet layout.

Per packet:
    uint32  direction ('SMSG' or 'CMSG')
    uint32  connectionIdx
    uint32  arrivalTicks (ms since capture start)
    uint32  optionalDataLen
    bytes   optionalData (optionalDataLen bytes; session-specific)
    uint32  size
    bytes   payload (size bytes; payload[0..3] is the wowOpcode)
"""
import struct, sys, collections

def iter_packets(path, debug=False):
    with open(path, 'rb') as f:
        data = f.read()

    # First SMSG tag at offset 72 (empirically confirmed)
    off = 72
    idx = 0
    bad_recoveries = 0
    while off + 20 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            # Search forward for next valid tag
            next_s = data.find(b'SMSG', off + 1)
            next_c = data.find(b'CMSG', off + 1)
            cands = [x for x in (next_s, next_c) if x > 0]
            if not cands:
                if debug: print(f'[stop] no more tags after {off}')
                return
            if debug: print(f'[recover] off={off} tag={tag!r} → jumping to {min(cands)}')
            off = min(cands)
            bad_recoveries += 1
            continue

        conn, ts, opt_len = struct.unpack_from('<3I', data, off+4)
        if opt_len > 4096:
            if debug: print(f'[bad] idx={idx} off={off} opt_len={opt_len} too large, seeking next tag')
            off += 4
            continue
        header_tail = off + 16 + opt_len
        if header_tail + 4 > len(data):
            return
        size = struct.unpack_from('<I', data, header_tail)[0]
        if size > 10_000_000 or size < 0:
            if debug: print(f'[bad] idx={idx} off={off} size={size} weird, seeking next tag')
            off += 4
            continue
        payload_start = header_tail + 4
        payload_end = payload_start + size
        if payload_end > len(data):
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
            'opt_data': data[off+16:header_tail],
            'size': size,
            'opcode': opcode,
            'body': body,
            'payload_start': payload_start,
        }
        idx += 1
        off = payload_end

if __name__ == '__main__':
    path = sys.argv[1]
    counts = collections.Counter()
    groups = collections.Counter()
    total = 0
    for p in iter_packets(path):
        counts[p['opcode']] += 1
        groups[(p['opcode'] >> 16) & 0xFFFF] += 1
        total += 1
    print(f'Total packets parsed: {total:,}')
    print(f'\nTop opcode groups (high 16 bits):')
    for g, c in groups.most_common(20):
        print(f'  0x{g:04X}: {c:,}')
    print(f'\nTop opcodes:')
    for op, c in counts.most_common(20):
        print(f'  0x{op:08X}: {c:,}')
