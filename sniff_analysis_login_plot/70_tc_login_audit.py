#!/usr/bin/env python3
"""Audit the new TC-server login sniff for whether our login changes
actually reached the client.

Specifically look for:
  1. SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE  (0x510006) — storage ack
  2. SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE         (0x5C0012) — roster replay
  3. SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE         (0x460012) — name cache
  4. SMSG_QUERY_PLAYER_NAMES_RESPONSE              (0x460040 or similar)
  5. SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE (0x54000B)
  6. SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR  (0x540011)
  7. SMSG_UPDATE_OBJECT packets with Housing/4 (mirror) entity

Report timestamps relative to login so we see if the 500ms defer fires.
"""
import sys
import struct

sys.path.insert(0, 'sniff_analysis_login_plot')
from importlib import import_module
m = import_module('65_dashboard_delta')

PATH = r'C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-23_05-17-07.pkt'

INTERESTING = {
    0x460012: 'SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE',
    0x510006: 'SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE',
    0x5C0012: 'SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE',
    0x54000B: 'SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE',
    0x540011: 'SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR',
    0x580000: 'SMSG_UPDATE_OBJECT',
    0x490000: 'SMSG_AUTH_CHALLENGE',
    0x400016: 'CMSG_PLAYER_LOGIN',
    0x39000E: 'CMSG_NEIGHBORHOOD_GET_ROSTER',
    0x30000E: 'CMSG_HOUSING_DECOR_REQUEST_STORAGE',
    0x330011: 'CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO',
}

# PKT header has timestamps - let me extract and report them
def iter_packets_with_time(path):
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
        # PKT header v3+: dir4 + conn4 + timestamp4 + millis4 + length4
        conn = struct.unpack_from('<I', h, 0)[0]
        ts = struct.unpack_from('<I', h, 4)[0]
        ms = struct.unpack_from('<I', h, 8)[0]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1; continue
        ps = off+29; pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from('<I', data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe], ts, ms
        off = pe; idx += 1


def main():
    # First collect all packets with their timing
    pkts = list(iter_packets_with_time(PATH))
    print(f'Total packets: {len(pkts)}')

    # Find login marker - player login CMSG
    login_idx = None
    login_ms = None
    for i, (idx, d, op, body, ts, ms) in enumerate(pkts):
        if op == 0x400016 and d == 'CMSG':  # CMSG_PLAYER_LOGIN
            login_idx = idx
            login_ms = ms
            print(f'\nLogin at packet idx {idx} ts={ts} ms={ms}')
            break

    if login_idx is None:
        print('No CMSG_PLAYER_LOGIN found')
        return

    # Look for CMSG_NEIGHBORHOOD_GET_ROSTER — marks end of login phase
    first_roster_cmsg_idx = None
    for i, (idx, d, op, body, ts, ms) in enumerate(pkts):
        if idx <= login_idx: continue
        if op == 0x39000E and d == 'CMSG':
            first_roster_cmsg_idx = idx
            print(f'First CMSG_NEIGHBORHOOD_GET_ROSTER at idx {idx} (+{ms - login_ms}ms from login)')
            break

    cutoff = first_roster_cmsg_idx if first_roster_cmsg_idx is not None else len(pkts)
    print(f'\nLogin-phase window: idx {login_idx} to {cutoff}')

    # Tally interesting SMSG opcodes in login window
    print('\n========== Login-phase SMSGs ==========')
    counts = {}
    for i, (idx, d, op, body, ts, ms) in enumerate(pkts):
        if idx < login_idx or idx >= cutoff: continue
        if d == 'SMSG' and op in INTERESTING:
            counts.setdefault(op, []).append((idx, ms - login_ms if login_ms else 0, len(body)))

    for op, events in sorted(counts.items()):
        name = INTERESTING.get(op, f'0x{op:08X}')
        print(f'{name}: {len(events)} emissions')
        for idx, rel_ms, body_len in events[:5]:
            print(f'  idx={idx} +{rel_ms}ms body_len={body_len}')

    # Did storage ack fire? (our 500ms defer emission)
    print('\n========== Key check: 500ms defer firing? ==========')
    if 0x510006 in counts:
        ack_events = counts[0x510006]
        print(f'✅ STORAGE_RSP (0x510006) emitted {len(ack_events)} time(s) in login window')
        for idx, rel_ms, body_len in ack_events:
            print(f'  +{rel_ms}ms at idx {idx}')
    else:
        print('❌ STORAGE_RSP (0x510006) NEVER emitted in login window')
        print('   → 500ms defer is NOT firing')

    if 0x5C0012 in counts:
        roster_events = counts[0x5C0012]
        print(f'✅ ROSTER_RESPONSE (0x5C0012) emitted {len(roster_events)} time(s) in login window')
        for idx, rel_ms, body_len in roster_events:
            print(f'  +{rel_ms}ms at idx {idx} body_len={body_len}')
    else:
        print('❌ ROSTER_RESPONSE (0x5C0012) NEVER emitted in login window')


if __name__ == '__main__':
    main()
