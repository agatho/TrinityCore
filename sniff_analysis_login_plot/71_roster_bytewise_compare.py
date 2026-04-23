#!/usr/bin/env python3
"""Compare byte-by-byte:
 (a) SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE emitted by our 500ms defer
     at login (idx 425 in the new TC sniff)
 (b) Same opcode emitted in response to the user's manual roster click
     (after idx 2873)

If byte-equal → the client differentiates based on context (prior state
it sets when it initiates the CMSG), not server output. If different →
we have a code path discrepancy.
"""
import sys, struct

PATH = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    if not cands:
        return
    off = min(cands); idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx:
                return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def hexdump(b, limit=256):
    return ' '.join(f'{x:02X}' for x in b[:limit])


def main():
    pkts = list(iter_packets(PATH))

    # Defer-emitted roster: login-phase (idx < 2873), opcode 0x5C0012
    defer_roster = None
    manual_roster = None
    manual_idx_start = None

    for idx, d, op, body in pkts:
        if d != 'SMSG' or op != 0x5C0012:
            continue
        if idx < 2873:
            if defer_roster is None:
                defer_roster = (idx, body)
        else:
            if manual_roster is None:
                manual_roster = (idx, body)

    print(f'Defer roster:  idx={defer_roster[0]} len={len(defer_roster[1])}')
    print(f'Manual roster: idx={manual_roster[0]} len={len(manual_roster[1])}')

    if defer_roster[1] == manual_roster[1]:
        print('BYTE-IDENTICAL. Client differentiates on CONTEXT, not payload.')
    else:
        print('BYTES DIFFER. Payload discrepancy.')
        print(f'\nDefer bytes (first 256):\n  {hexdump(defer_roster[1])}')
        print(f'\nManual bytes (first 256):\n  {hexdump(manual_roster[1])}')
        # Find first diff position
        a, b = defer_roster[1], manual_roster[1]
        for i in range(min(len(a), len(b))):
            if a[i] != b[i]:
                print(f'\nFirst differing byte: offset {i} (defer=0x{a[i]:02X} manual=0x{b[i]:02X})')
                start = max(0, i - 16)
                end = min(len(a), i + 32)
                print(f'  Defer  [{start}:{end}]:  {" ".join(f"{x:02X}" for x in a[start:end])}')
                print(f'  Manual [{start}:{end}]:  {" ".join(f"{x:02X}" for x in b[start:end])}')
                break
        if len(a) != len(b):
            print(f'\nLength differs: defer={len(a)} manual={len(b)}')

    # Also compare the preceding and following packets
    print('\n=== Surrounding packet context ===')
    print('\nDefer context (idx 420-428):')
    for idx, d, op, body in pkts:
        if 420 <= idx <= 428:
            print(f'  idx={idx} {d} op=0x{op:08X} len={len(body)}')

    print('\nManual-click context (idx 2870-2880):')
    for idx, d, op, body in pkts:
        if 2870 <= idx <= 2885:
            print(f'  idx={idx} {d} op=0x{op:08X} len={len(body)}')


if __name__ == '__main__':
    main()
