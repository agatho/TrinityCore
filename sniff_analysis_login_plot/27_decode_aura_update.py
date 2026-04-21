#!/usr/bin/env python3
"""Decode SMSG_AURA_UPDATE wire format for the 6 map-entry auras at
idx 9985/9988/9991/9994/9997/10000 in
dump_12.0.1.66838_2026-04-15_09-35-59.pkt, extracting Slot, Flags,
ActiveFlags, CastLevel, Applications, Points[] per spell.

AURA_UPDATE wire (from TC SpellPackets.cpp):
  1 bit: UpdateAll
  9 bits: AuraCount
  per aura:
    (FlushBits on uint16)
    uint16 Slot
    1 bit: HasAuraData
    (FlushBits)
    if HasAuraData: AuraDataInfo:
      PackedGuid128 CastID
      int32 SpellID
      uint32 Visual.SpellXSpellVisualID
      uint32 Visual.ScriptVisualID
      uint16 Flags
      uint32 ActiveFlags
      uint16 CastLevel
      uint8 Applications
      int32 ContentTuningID
      <DstLocation block — sniff shows 3 floats + something>
      1 bit: has CastUnit
      1 bit: has CastItem
      1 bit: has Duration
      1 bit: has Remaining
      1 bit: has TimeMod
      6 bits: PointsCount
      6 bits: EstimatedPointsCount
      1 bit: has ContentTuning
      (FlushBits)
      (if ContentTuning: struct)
      (if CastUnit: PackedGuid128)
      (if CastItem: PackedGuid128)
      (if Duration: uint32)
      (if Remaining: uint32)
      (if TimeMod: float)
      Points: PointsCount * float (appended bytes)
      EstimatedPoints: EstimatedPointsCount * float
  PackedGuid128 UnitGUID
"""
import struct

PKT = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'


def iter_packets(path):
    with open(path, 'rb') as f: data = f.read()
    first_tag = min(x for x in (data.find(b'SMSG', 0, 256), data.find(b'CMSG', 0, 256)) if x > 0)
    off = first_tag; idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b'SMSG', b'CMSG'):
            nx = [x for x in (data.find(b'SMSG', off+1), data.find(b'CMSG', off+1)) if x > 0]
            if not nx: return
            off = min(nx); continue
        h = data[off+4:off+4+25]
        dlen = struct.unpack_from('<I', h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4: off += 1; continue
        ps = off+4+25; pe = ps+dlen
        if pe > len(data): return
        op = struct.unpack_from('<I', data, ps)[0]
        body = data[ps+4:pe]
        yield idx, tag.decode(), op, body
        off = pe; idx += 1


class BitReader:
    def __init__(self, data):
        self.data = data
        self.byte_pos = 0
        self.bit_pos = 8  # bits consumed in current accumulator; 8 forces fresh load

    def flush(self):
        # byte_pos already points to next unloaded byte after a partial consume
        self.bit_pos = 8

    def read_bits(self, n):
        v = 0
        while n > 0:
            if self.bit_pos == 8:
                if self.byte_pos >= len(self.data): return None
                self.cur = self.data[self.byte_pos]
                self.byte_pos += 1
                self.bit_pos = 0
            take = min(n, 8 - self.bit_pos)
            shift = 8 - self.bit_pos - take
            v = (v << take) | ((self.cur >> shift) & ((1 << take) - 1))
            self.bit_pos += take
            n -= take
        return v

    def read_u16_le(self):
        self.flush()
        v = struct.unpack_from('<H', self.data, self.byte_pos)[0]
        self.byte_pos += 2
        return v

    def read_u32_le(self):
        self.flush()
        v = struct.unpack_from('<I', self.data, self.byte_pos)[0]
        self.byte_pos += 4
        return v

    def read_i32_le(self):
        self.flush()
        v = struct.unpack_from('<i', self.data, self.byte_pos)[0]
        self.byte_pos += 4
        return v

    def read_u8(self):
        self.flush()
        v = self.data[self.byte_pos]
        self.byte_pos += 1
        return v

    def read_f32_le(self):
        self.flush()
        v = struct.unpack_from('<f', self.data, self.byte_pos)[0]
        self.byte_pos += 4
        return v

    def read_packed_guid(self):
        self.flush()
        mask_lo = self.data[self.byte_pos]
        mask_hi = self.data[self.byte_pos + 1]
        self.byte_pos += 2
        lo = 0
        hi = 0
        for b in range(8):
            if mask_lo & (1 << b):
                lo |= self.data[self.byte_pos] << (b * 8)
                self.byte_pos += 1
        for b in range(8):
            if mask_hi & (1 << b):
                hi |= self.data[self.byte_pos] << (b * 8)
                self.byte_pos += 1
        return lo, hi


def decode_aura_update(body):
    r = BitReader(body)
    update_all = r.read_bits(1)
    aura_count = r.read_bits(9)
    auras = []
    for _ in range(aura_count):
        slot = r.read_u16_le()
        has_aura_data = r.read_bits(1)
        if has_aura_data:
            cast_lo, cast_hi = r.read_packed_guid()
            spell_id = r.read_i32_le()
            vis1 = r.read_u32_le()
            vis2 = r.read_u32_le()
            flags = r.read_u16_le()
            active_flags = r.read_u32_le()
            cast_level = r.read_u16_le()
            applications = r.read_u8()
            content_tuning_id = r.read_i32_le()
            # Unknown DstLocation block — seems to be read even when not "present".
            # Peek bit-init pattern: next 8 bits (7 Optional flags + padding) then aligned.
            has_cast_unit = r.read_bits(1)
            has_cast_item = r.read_bits(1)
            has_duration = r.read_bits(1)
            has_remaining = r.read_bits(1)
            has_time_mod = r.read_bits(1)
            points_count = r.read_bits(6)
            est_points_count = r.read_bits(6)
            has_content_tuning = r.read_bits(1)
            r.flush()
            cast_unit = r.read_packed_guid() if has_cast_unit else None
            cast_item = r.read_packed_guid() if has_cast_item else None
            duration = r.read_u32_le() if has_duration else None
            remaining = r.read_u32_le() if has_remaining else None
            time_mod = r.read_f32_le() if has_time_mod else None
            points = [r.read_f32_le() for _ in range(points_count)]
            est_points = [r.read_f32_le() for _ in range(est_points_count)]
            auras.append({
                'slot': slot, 'spell': spell_id,
                'vis1': vis1, 'vis2': vis2,
                'flags': flags, 'active_flags': active_flags,
                'cast_level': cast_level, 'applications': applications,
                'content_tuning_id': content_tuning_id,
                'has_cast_unit': has_cast_unit, 'cast_unit': cast_unit,
                'has_duration': has_duration, 'duration': duration,
                'has_remaining': has_remaining, 'remaining': remaining,
                'points_count': points_count, 'points': points,
                'est_points_count': est_points_count, 'est_points': est_points,
                'has_content_tuning': has_content_tuning,
            })
        else:
            auras.append({'slot': slot, 'removed': True})
    return update_all, auras


def decode_flags(f):
    names = []
    flagmap = {
        0x0001: 'NOCASTER',
        0x0002: 'POSITIVE',
        0x0004: 'DURATION',
        0x0008: 'SCALABLE',
        0x0010: 'NEGATIVE',
        0x0020: 'UNK20',
        0x0040: 'UNK40',
        0x0080: 'UNK80',
        0x0100: 'MAW_POWER',
    }
    for bit, n in flagmap.items():
        if f & bit: names.append(n)
    residual = f & ~sum(flagmap.keys())
    if residual: names.append(f'residual=0x{residual:04X}')
    return '|'.join(names) if names else '0'


def main():
    targets = {9985, 9988, 9991, 9994, 9997, 10000}
    for idx, dir_, op, body in iter_packets(PKT):
        if idx in targets and op == 0x00620011:
            print(f'\n=== idx={idx} op=0x{op:08X} size={len(body)} ===')
            upd, auras = decode_aura_update(body)
            print(f'  UpdateAll={upd}, AuraCount={len(auras)}')
            for a in auras:
                if a.get('removed'):
                    print(f'  [REMOVE] slot={a["slot"]}')
                    continue
                pts = ','.join(f'{p:.3f}' for p in a['points'])
                eps = ','.join(f'{p:.3f}' for p in a['est_points'])
                print(f'  spell={a["spell"]:8} slot={a["slot"]:3} flags=0x{a["flags"]:04X}({decode_flags(a["flags"])}) '
                      f'active={a["active_flags"]} lvl={a["cast_level"]} app={a["applications"]} '
                      f'vis={a["vis1"]}/{a["vis2"]} ctid={a["content_tuning_id"]} '
                      f'points=[{pts}] est=[{eps}]')


if __name__ == '__main__':
    main()
