#!/usr/bin/env python3
"""Properly parse MIRROR_VARS body and extract (name, value) pairs from retail
sniff so we can feed exact retail values into our AuthHandler.

Wire format (reverse-engineered from packet structure + sniff bytes):
  uint32 count
  per-entry:
    uint8  name_len_plus_1 or uint16 depending on length (BitReader?)
    Not fully reverse-engineered — try a string-table heuristic: scan for
    null-terminated ASCII runs and pair adjacent (name, value) based on
    proximity in the binary.

Simpler: parse as C-string pairs; retail packs `name\0value\0` with small
prefix bytes we can detect heuristically.
"""
import os
import re
import struct

RETAIL = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'
OPCODE = 0x0042036A


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
    cands = [x for x in (data.find(b'SMSG', 0, 4096), data.find(b'CMSG', 0, 4096)) if x > 0]
    off = min(cands)
    idx = 0
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


def extract_all_strings(body):
    """Return list of (offset, string) for all printable null-terminated runs."""
    out = []
    start = None
    for i, b in enumerate(body):
        if 32 <= b < 127:
            if start is None:
                start = i
        else:
            if start is not None and i - start >= 1:
                s = body[start:i].decode('ascii', errors='ignore')
                out.append((start, s))
            start = None
    return out


def parse_name_value(body):
    """Extract (name, value) pairs heuristically.
    Every name-looking string (starts with lowercase letter, matches camelCase)
    is treated as a var name; the next string after it (>=0 chars) is its value.
    """
    strs = extract_all_strings(body)
    # Filter: include every string run >= 1 char
    pairs = []
    name_pat = re.compile(r'^[a-z][a-zA-Z0-9_]*$')
    i = 0
    while i < len(strs) - 1:
        off, s = strs[i]
        if name_pat.match(s) and len(s) >= 3:
            # Next non-overlapping string is the value
            _, v = strs[i + 1]
            pairs.append((s, v))
            i += 2
        else:
            i += 1
    return pairs


if __name__ == '__main__':
    outdir = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21'
    os.makedirs(outdir, exist_ok=True)
    # Use the first retail MIRROR_VARS packet we see (they should all be identical)
    chosen = None
    for idx, direction, op, body in iter_packets(RETAIL):
        if op == OPCODE:
            chosen = (idx, body)
            break
    if not chosen:
        print('No MIRROR_VARS found')
        raise SystemExit
    idx, body = chosen
    pairs = parse_name_value(body)
    with open(os.path.join(outdir, 'MIRROR_VARS_RETAIL_VALUES.md'), 'w', encoding='utf-8') as f:
        f.write(f'# Retail MIRROR_VARS (from idx {idx}, {len(body)} bytes)\n\n')
        f.write(f'Total pairs extracted: {len(pairs)}\n\n')
        f.write('| name | value |\n|---|---|\n')
        for name, value in pairs:
            # Escape pipe in value
            v = value.replace('|', r'\|')
            f.write(f'| `{name}` | `{v}` |\n')
    print(f'Wrote {outdir}/MIRROR_VARS_RETAIL_VALUES.md with {len(pairs)} pairs')
