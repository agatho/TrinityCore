"""PKT file header + packet iterator for WoWPacketParser PKT v1.3 format.

Layout is:
  Header:
    'PKT\0'          4 bytes magic
    uint16           major (=0x0103 for 1.3 in older formats; newer hold major/minor)
    Then variable depending on version; we only need to skip the header
    and iterate packets after it.

Each packet:
  uint32 direction       0 = SMSG (server->client), 1 = CMSG (client->server)
  uint32 connectionId
  uint32 timestamp       ms since capture start
  uint32 wowOpcode
  uint32 payloadSize
  bytes  payload         payloadSize bytes
"""

import struct

DIR_SMSG = 0x47534D53  # 'SMSG'
DIR_CMSG = 0x47534D43  # 'CMSG'

def iter_packets(path, limit=None):
    with open(path, 'rb') as f:
        data = f.read()
    # WPP PKT v1.3 header:
    #   bytes [0..3]  'PKT\0'  magic
    #   bytes [4..5]  uint16 version (e.g. 0x0103)
    #   bytes [6..N]  additional header (varies); we scan for first valid CMSG/SMSG dir tag
    assert data[:3] == b'PKT', f'Bad PKT magic: {data[:4]!r}'
    # Known v1.3 header sizes: scan linearly until we land on a valid direction marker,
    # which is either 4-byte 'SMSG' / 'CMSG' or a uint32 0/1 in newer encodings.
    off = None
    for probe in (54, 52, 50, 48, 46, 44, 40, 32):
        if probe + 16 > len(data):
            continue
        tag = struct.unpack_from('<I', data, probe)[0]
        if tag in (DIR_SMSG, DIR_CMSG):
            off = probe
            break
    if off is None:
        # Fall back to a brute search
        for i in range(16, min(len(data), 256)):
            if data[i:i+4] in (b'SMSG', b'CMSG'):
                off = i
                break
    assert off is not None, 'Could not locate first packet dir marker'

    idx = 0
    while off + 4 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            # Sometimes there are trailing bytes between packets; find next
            next_s = data.find(b'SMSG', off+1)
            next_c = data.find(b'CMSG', off+1)
            candidates = [x for x in (next_s, next_c) if x > 0]
            if not candidates:
                break
            off = min(candidates)
            continue
        direction = 'SMSG' if tag == b'SMSG' else 'CMSG'
        # Next 4 bytes connection id
        conn_id = struct.unpack_from('<I', data, off+4)[0]
        # Timestamp (ms)
        ts_ms = struct.unpack_from('<I', data, off+8)[0]
        # 4-byte filler / session id
        unk = struct.unpack_from('<I', data, off+12)[0]
        # 4-byte size
        size = struct.unpack_from('<I', data, off+16)[0]
        # Opcode is the FIRST uint32 of the payload (little-endian)
        hdr_end = off + 20
        if hdr_end + size > len(data):
            break
        payload = data[hdr_end:hdr_end+size]
        opcode = struct.unpack_from('<I', payload, 0)[0] if size >= 4 else 0
        body = payload[4:] if size >= 4 else b''
        yield (idx, direction, ts_ms, opcode, body)
        idx += 1
        off = hdr_end + size
        if limit and idx >= limit:
            break

if __name__ == '__main__':
    import sys
    count = 0
    for i, d, ts, op, body in iter_packets(sys.argv[1], limit=10):
        print(f'{i:5d}  {d}  ts={ts:9d}  op=0x{op:08X}  len={len(body):6d}')
        count += 1
    print(f'Iterated {count} packets')
