"""Sequential PKT parser. Walks strictly from one packet to the next using size.

If we land on an invalid tag, we know the size field interpretation is off.
"""
import struct, sys, collections

def parse(path, max_dump=None):
    with open(path, 'rb') as f:
        data = f.read()

    # Skip fixed 72-byte header (verified empirically: first 'SMSG' tag is at offset 72)
    off = 72

    # For each valid packet, record (idx, dir, ts, opcode, size, off)
    records = []
    bad_at = None
    while off + 20 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            bad_at = off
            break
        conn, ts, sessidx, size = struct.unpack_from('<4I', data, off+4)
        if off + 20 + size > len(data):
            bad_at = off
            break
        payload = data[off+20:off+20+size]
        opcode = struct.unpack_from('<I', payload, 0)[0] if size >= 4 else 0
        records.append({
            'idx': len(records),
            'dir': tag.decode(),
            'ts': ts,
            'conn': conn,
            'sessidx': sessidx,
            'opcode': opcode,
            'size': size,
            'off': off,
        })
        off = off + 20 + size

    return records, bad_at, len(data)

if __name__ == '__main__':
    path = sys.argv[1]
    records, bad, total = parse(path)
    print(f'Parsed {len(records):,} packets')
    print(f'Stopped at offset {bad} / total size {total:,}')
    if bad is not None:
        tail = total - bad
        print(f'Trailing unparsed bytes: {tail:,}')

    # Show opcode distribution by high byte (indicates service group)
    group_count = collections.Counter()
    for r in records:
        group_count[(r['opcode'] >> 16) & 0xFFFF] += 1
    print('\nTop opcode groups (high 16 bits):')
    for grp, c in group_count.most_common(15):
        print(f'  0x{grp:04X}: {c:,}')

    # Show first 30 packets
    print('\nFirst 30 packets:')
    for r in records[:30]:
        print(f'  [{r["idx"]:5d}] {r["dir"]} ts={r["ts"]:>9d} op=0x{r["opcode"]:08X} size={r["size"]:>6d}')
