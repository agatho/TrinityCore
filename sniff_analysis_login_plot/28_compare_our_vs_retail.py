#!/usr/bin/env python3
"""Compare OUR server's UPDATE_OBJECT emissions at housing-map entry against
retail sniff idx 9984 of dump_12.0.1.66838_2026-04-15_09-35-59.pkt.

For each sniff:
  - count SMSG_UPDATE_OBJECT packets
  - within those, scan for CREATE/CREATE2 block headers (updateType 1/2)
    followed by PackedGuid128. For housing-typed GUIDs (HighGuid == 55),
    bucket by subType and report per-packet counts.
  - also look for byte pattern (updateType, objectType, PackedGuid) to see
    what objectType byte our server is emitting for Housing/3 entities.
"""
import struct, os

OUR = r"C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt"
OUR_PREV = r"C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_18-11-22.pkt"
RETAIL = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

OP_UPDATE_OBJECT = 0x00580000


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    if not cands: return
    off = min(cands); idx = 0
    while off + 4 + 25 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        header = data[off+4:off+4+25]
        dlen = struct.unpack_from('<I', header, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+4+25; pe = ps+dlen
        if pe > len(data): return
        opcode = struct.unpack_from('<I', data, ps)[0]
        body = data[ps+4:pe]
        yield idx, tag.decode(), opcode, body
        off = pe; idx += 1


def read_packed_guid(body, off):
    if off + 2 > len(body):
        return None, off
    mask_lo = body[off]; mask_hi = body[off+1]; off += 2
    lo = 0; hi = 0
    for b in range(8):
        if mask_lo & (1 << b):
            if off >= len(body): return None, off
            lo |= body[off] << (b*8); off += 1
    for b in range(8):
        if mask_hi & (1 << b):
            if off >= len(body): return None, off
            hi |= body[off] << (b*8); off += 1
    return (lo, hi), off


def scan_housing_creates(body):
    """Return list of (offset, updateType, maybeObjType, subType, lo, hi).
    Heuristic: find (ut=1|2) byte, attempt PackedGuid immediately after AND
    after +1 (to allow an intermediate objectType byte). Accept if GUID's
    HighGuid high bits == 55 (Housing).
    """
    hits = []
    for o in range(1, len(body) - 32):
        ut = body[o]
        if ut not in (1, 2):
            continue
        # Try: guid starts at o+1 (no intermediate byte)
        for slack in (0, 1):
            guid, _end = read_packed_guid(body, o+1+slack)
            if guid is None:
                continue
            lo, hi = guid
            if (hi >> 58) != 55:
                continue
            sub = (hi >> 53) & 0x1F
            maybe_ot = body[o+1] if slack == 1 else None
            hits.append((o, ut, maybe_ot, sub, lo, hi))
            break
    return hits


def per_packet_summary(path, label):
    print(f"\n{'='*72}\n{label}: {path}\n{'='*72}")
    totals_by_sub = {s: set() for s in range(8)}
    ot_hist_by_sub = {s: {} for s in range(8)}
    upd_hits = []
    upd_count = 0
    for idx, direction, opcode, body in iter_packets(path):
        if opcode != OP_UPDATE_OBJECT:
            continue
        upd_count += 1
        pkt_subs = {s: set() for s in range(8)}
        for (o, ut, ot, sub, lo, hi) in scan_housing_creates(body):
            if sub not in totals_by_sub:
                continue
            if (lo, hi) in pkt_subs[sub]:
                continue
            pkt_subs[sub].add((lo, hi))
            totals_by_sub[sub].add((lo, hi))
            if ot is not None:
                ot_hist_by_sub[sub][ot] = ot_hist_by_sub[sub].get(ot, 0) + 1
        if any(pkt_subs.values()):
            upd_hits.append((idx, len(body), {s: len(v) for s, v in pkt_subs.items() if v}))
    print(f"Total UPDATE_OBJECT packets: {upd_count}")
    print(f"Packets containing housing CREATEs: {len(upd_hits)}")
    print("Per-packet summary (first 30):")
    for idx, size, counts in upd_hits[:30]:
        print(f"  idx={idx:5} size={size:6}  housing_counts={counts}")
    print("\nDistinct GUIDs by subtype across the whole sniff:")
    for s in sorted(totals_by_sub):
        if totals_by_sub[s]:
            print(f"  subType={s}: {len(totals_by_sub[s])} distinct GUIDs")
            # Dump first few
            for lo, hi in sorted(totals_by_sub[s])[:6]:
                print(f"     raw lo={lo:016x} hi={hi:016x}")
    print("\nobjectType byte histogram (when seen 1 byte after updateType):")
    for s in sorted(ot_hist_by_sub):
        if ot_hist_by_sub[s]:
            hist = sorted(ot_hist_by_sub[s].items(), key=lambda x: -x[1])
            print(f"  subType={s}: {hist}")


if __name__ == '__main__':
    for path, label in [(OUR, 'OUR NEW (22:31)'), (OUR_PREV, 'OUR PREV (18:11 — before ghost-GUID fix)'), (RETAIL, 'RETAIL (idx 9984 reference)')]:
        if os.path.exists(path):
            per_packet_summary(path, label)
        else:
            print(f"MISSING: {path}")
