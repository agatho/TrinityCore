#!/usr/bin/env python3
"""Extract raw bytes surrounding retail's Housing/sub{8,11,15,16} blocks in
login-bundle UPDATE_OBJECTs so we can identify what those entity types are.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
SMSG_UPDATE_OBJECT = 0x580000
HIGH_GUID_HOUSING = 55
TARGET_SUBTYPES = {0, 1, 2, 3, 4, 8, 11, 15, 16, 22, 31}


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


def scan_blocks_with_pos(body):
    i = 0
    scans = 0
    while i < len(body) - 10 and scans < 1_000_000:
        scans += 1
        ut = body[i]
        if ut not in (0, 1, 2, 3):
            i += 1; continue
        if i + 3 > len(body):
            break
        m_lo, m_hi = body[i+1], body[i+2]
        off = i + 3
        lo = hi = 0
        ok = True
        for b in range(8):
            if m_lo & (1 << b):
                if off >= len(body):
                    ok = False; break
                lo |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        for b in range(8):
            if m_hi & (1 << b):
                if off >= len(body):
                    ok = False; break
                hi |= body[off] << (b*8); off += 1
        if not ok:
            i += 1; continue
        if hi == 0 and lo == 0:
            i += 1; continue
        high = (hi >> 58) & 0x3F
        sub = (hi >> 53) & 0x1F
        if high == 0 or high > 60:
            i += 1; continue
        if ut in (1, 2):
            if off < len(body) and body[off] <= 20:
                yield (i, ut, high, sub, lo, hi, off)
                i = off + 1
                continue
        else:
            yield (i, ut, high, sub, lo, hi, off)
        i += 1


def main():
    pkts = list(iter_packets(PATH))
    lvw = None
    for idx, d, op, body in pkts:
        if d == 'SMSG' and op == SMSG_LOGIN_VERIFY_WORLD:
            lvw = idx
            break
    if lvw is None:
        print('No LVW'); return

    first_h_cmsg = None
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if d == 'CMSG':
            grp = (op >> 16) & 0xFF
            if grp in (0x2E, 0x30, 0x31, 0x32, 0x33, 0x35, 0x37, 0x38, 0x39):
                first_h_cmsg = idx
                break
    upper = first_h_cmsg if first_h_cmsg else len(pkts)

    print(f'File: {PATH}')
    print(f'LVW={lvw}, first-housing-CMSG={first_h_cmsg}')
    print()

    # Find all Housing blocks in post-LVW UPDATE_OBJECTs
    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        print(f'=== UPDATE_OBJECT idx={idx} (LVW+{idx-lvw}) len={len(body)} ===')
        for pos, ut, high, sub, lo, hi, nxt in scan_blocks_with_pos(body):
            if high != HIGH_GUID_HOUSING:
                continue
            ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}[ut]
            typebyte = body[nxt] if (ut in (1, 2) and nxt < len(body)) else -1
            # Show 16 bytes of surrounding hex for context
            start = max(0, pos)
            endp = min(len(body), nxt + 30)
            context = body[start:endp].hex(' ')
            print(f'  [pos={pos:6d}]  Housing/sub{sub:2d}  {ut_name:8s}  '
                  f'GUID lo={lo:016X} hi={hi:016X}  '
                  f'typeByte={typebyte}  hex={context[:120]}')
        print()


if __name__ == '__main__':
    main()
