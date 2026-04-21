"""Final PKT parser using the same layout as the existing parse_full_sniff.py.

Per-packet (after 'SMSG' or 'CMSG' marker):
    uint32 connIdx
    uint32 field2
    uint32 field3
    uint32 dataLen
    uint64 timestamp
    uint8  padding
    bytes  payload (dataLen bytes; payload[0..3] = opcode)
"""
import struct, sys, collections

def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()

    # Header size search
    probe = data[:64]
    header_size = None
    for i in range(54, 16, -1):
        if probe[i-4:i] == b'\x00\x00\x00\x00' or data.find(b'SMSG', 0, 128) == i:
            pass
    # Easier: find first SMSG/CMSG tag in first 128 bytes
    first_tag = min(x for x in (data.find(b'SMSG', 0, 256), data.find(b'CMSG', 0, 256)) if x > 0)
    header_size = first_tag
    off = header_size
    idx = 0

    while off + 4 + 25 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            next_s = data.find(b'SMSG', off + 1)
            next_c = data.find(b'CMSG', off + 1)
            cands = [x for x in (next_s, next_c) if x > 0]
            if not cands:
                return
            off = min(cands)
            continue

        header = data[off+4:off+4+25]
        conn, f2, f3, dlen = struct.unpack_from('<IIII', header, 0)
        ts = struct.unpack_from('<Q', header, 16)[0]

        if dlen > 50 * 1024 * 1024:
            off += 1
            continue

        payload_start = off + 4 + 25
        payload_end = payload_start + dlen
        if payload_end > len(data):
            return

        if dlen < 4:
            off = payload_end
            idx += 1
            continue

        opcode = struct.unpack_from('<I', data, payload_start)[0]
        body = data[payload_start+4:payload_end]

        yield {
            'idx': idx,
            'off': off,
            'dir': tag.decode(),
            'conn': conn,
            'f2': f2,
            'f3': f3,
            'ts': ts,
            'size': dlen,
            'opcode': opcode,
            'body': body,
            'payload_start': payload_start,
        }
        idx += 1
        off = payload_end

if __name__ == '__main__':
    path = sys.argv[1]
    count = 0
    groups = collections.Counter()
    opcodes = collections.Counter()
    dirs = collections.Counter()
    for p in iter_packets(path):
        count += 1
        groups[(p['opcode'] >> 16) & 0xFFFF] += 1
        opcodes[p['opcode']] += 1
        dirs[p['dir']] += 1
    print(f'Total packets: {count:,}')
    print(f'Direction split: {dict(dirs)}')
    print(f'\nTop opcode groups:')
    for g, c in groups.most_common(30):
        print(f'  0x{g:04X}: {c:,}')
