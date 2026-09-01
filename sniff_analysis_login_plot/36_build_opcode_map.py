#!/usr/bin/env python3
"""Parse src/server/game/Server/Protocol/Opcodes.h into a (int → name) map
usable by every sniff-analysis script. The previous script 33 classified
opcodes by guessing group prefixes; many guesses turned out wrong
(e.g. 0x5A005D is SMSG_FLIGHT_SPLINE_SYNC, not CMSG_NeighborhoodSystem[5D]).

This authoritative map is generated once and written as JSON+Python.
"""
import json
import os
import re

OPCODES_H = r'c:/TrinityBots/wt/housing-system/src/server/game/Server/Protocol/Opcodes.h'
OUT_JSON = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21/opcode_map.json'
OUT_PY = r'c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21/opcode_map.py'

pat = re.compile(r'^\s*(CMSG_[A-Z0-9_]+|SMSG_[A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+)\s*,')
mapping = {}
with open(OPCODES_H, 'r', encoding='utf-8') as f:
    for line in f:
        m = pat.match(line)
        if m:
            name, hex_val = m.group(1), m.group(2)
            op = int(hex_val, 16)
            mapping[op] = name

with open(OUT_JSON, 'w') as f:
    json.dump({f'0x{k:08X}': v for k, v in sorted(mapping.items())}, f, indent=2)

with open(OUT_PY, 'w') as f:
    f.write('# Auto-generated from Opcodes.h — do not edit by hand.\n')
    f.write('OPCODE_MAP = {\n')
    for op, name in sorted(mapping.items()):
        f.write(f'    0x{op:08X}: "{name}",\n')
    f.write('}\n')

print(f'Wrote {len(mapping)} opcodes to {OUT_JSON} and {OUT_PY}')
