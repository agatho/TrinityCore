"""Probe the actual PKT header layout for this specific sniff."""
import struct, sys

path = sys.argv[1]
with open(path, 'rb') as f:
    data = f.read()

print(f'Size: {len(data):,} bytes')
print(f'First 64 bytes hex: {data[:64].hex()}')
print(f'First 64 bytes ascii: {data[:64]!r}')
print()

# The PKT v1.3+ header is typically:
# 'PKT\0' (4) + uint16 version (2) + uint8 snifferId (1) + uint32 build (4)
# + 'en_US' or similar locale (4) + uint8[40] sessionKey  + uint32 startTime
# + uint32 startTicks + uint32 optDataLen + optData... then packets.
# Total ~ 54 or 62 bytes depending on version.

# Version + sniffer at offset 4..7
version = struct.unpack_from('<H', data, 4)[0]
print(f'Version (LE uint16 @4): 0x{version:04x}')
print(f'Version hi/lo bytes: {data[4]} {data[5]}')

# Build number is probably at offset 8
print(f'Bytes @8: {data[8:24].hex()}')
print(f'Bytes @24: {data[24:40].hex()}')
print(f'Bytes @40: {data[40:64].hex()}')

# Find first SMSG/CMSG
for tag in (b'SMSG', b'CMSG'):
    idx = data.find(tag)
    print(f'First {tag.decode()!r} at offset: {idx}')

# Dump context of first tag
tag_off = min(i for i in (data.find(b'SMSG'), data.find(b'CMSG')) if i > 0)
print()
print(f'Context 32 bytes before and 64 after first tag (@{tag_off}):')
start = max(0, tag_off-32)
print(data[start:tag_off+64].hex())
print()

# For each packet, WPP's tagged format is usually:
#  char[4] dir           'SMSG' or 'CMSG'
#  uint32  connection_id
#  uint32  timestamp_ms
#  uint32  size           (includes 4-byte opcode prefix? or not?)
#  bytes   payload
# The opcode is either:
#  - first uint32 of payload (if size includes it)
#  - not present at all if direction tag already implies it
# In WPP 1.3+, payload[0..3] is uint32 opcode, rest is body.

# Let's inspect a few packets manually around the first tag.
off = tag_off
for i in range(6):
    if off + 20 > len(data):
        break
    tag = data[off:off+4]
    conn, ts, unk, size = struct.unpack_from('<4I', data, off+4)
    op_bytes = data[off+20:off+24] if size >= 4 else b''
    op = struct.unpack_from('<I', op_bytes, 0)[0] if len(op_bytes) == 4 else 0
    body_preview = data[off+20:off+20+min(size, 32)]
    print(f'[{i}] off={off:7d} {tag!r} conn={conn} ts={ts:>10d} unk={unk:>10d} size={size:>6d} opcode=0x{op:08x}')
    print(f'     body[0..32] = {body_preview.hex()}')
    off += 20 + size
