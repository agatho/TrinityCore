#!/usr/bin/env python3
"""Extract all MIRROR_VARS name/value pairs from both OURS and RETAIL and
emit a side-by-side diff of which names retail sends that we don't.

Wire format per var (sniff-reverse-engineered):
  uint16 name_len
  bytes[name_len] name (null-terminated cstring inside)
  uint16 type  (?)
  bytes value (null-terminated cstring)

Actually, simpler approach: scan for null-terminated ASCII strings inside
the packet body and collect ones that look like var names (start with alpha,
mostly lowercase, >3 chars).
"""
import os
import re
import struct

OURS = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt'
RETAIL = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'
OPCODE = 0x0042036A


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    off = min(cands); idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4: off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe; idx += 1


def extract_var_names(body):
    """Heuristic: every run of ASCII printable bytes ending in 0 is a string.
    Keep strings that look like variable names (lowerCamelCase or underscore,
    starts with letter, 3-80 chars)."""
    names = set()
    pat = re.compile(rb'([a-zA-Z][a-zA-Z0-9_]{2,78})\0')
    for m in pat.finditer(body):
        s = m.group(1).decode('ascii', errors='ignore')
        # Filter: must have at least one lowercase, no consecutive uppercase
        if any(c.islower() for c in s):
            names.add(s)
    return names


def all_vars(path):
    seen = set()
    for idx, direction, op, body in iter_packets(path):
        if op != OPCODE:
            continue
        seen |= extract_var_names(body)
    return seen


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    os.makedirs(outdir, exist_ok=True)
    ours = all_vars(OURS)
    retail = all_vars(RETAIL)
    only_retail = retail - ours
    only_ours = ours - retail
    with open(os.path.join(outdir, 'MIRROR_VARS_DIFF.md'), 'w', encoding='utf-8') as f:
        f.write('# MIRROR_VARS name diff — OURS vs RETAIL\n\n')
        f.write(f'- Ours: {len(ours)} unique names\n')
        f.write(f'- Retail: {len(retail)} unique names\n')
        f.write(f'- Retail-only (missing from ours): **{len(only_retail)}**\n')
        f.write(f'- Ours-only (extra in ours): {len(only_ours)}\n\n')
        f.write('## Retail-only (we must add these)\n\n')
        # Group by prefix
        housing = sorted(n for n in only_retail if 'housing' in n.lower() or 'neighborhood' in n.lower())
        shop = sorted(n for n in only_retail if any(k in n.lower() for k in ('shop', 'market', 'cart', 'store', 'bpay')))
        transmog = sorted(n for n in only_retail if 'transmog' in n.lower())
        other = sorted(n for n in only_retail if n not in housing and n not in shop and n not in transmog)
        for label, items in [('housing/neighborhood', housing), ('shop/market', shop), ('transmog', transmog), ('other', other)]:
            if items:
                f.write(f'\n### {label} ({len(items)})\n')
                for n in items:
                    f.write(f'- `{n}`\n')
        f.write('\n## Ours-only (extras — check if still relevant)\n\n')
        for n in sorted(only_ours):
            f.write(f'- `{n}`\n')
    print(f"Wrote {outdir}/MIRROR_VARS_DIFF.md")
    print(f"Retail-only names: {len(only_retail)}")
    print(f"Ours-only names: {len(only_ours)}")
