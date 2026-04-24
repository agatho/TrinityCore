#!/usr/bin/env python3
"""Proper UPDATE_OBJECT parser that follows block boundaries using the
official MoP+ wire format: uint16 mapId + uint32 blockCount + blocks.

Each block:
  uint8 updateType
  PackedGUID128 guid (loMask + hiMask + data bytes)
  if updateType in (1, 2):  CREATE / CREATE2
      uint8 objectTypeId
      uint32 movementUpdateFlags (optional, depending on flags bit)
      fragment data...
  if updateType == 0: VALUES
      uint8 fragmentFlags
      per-fragment data...
  if updateType == 3: OUT_OF_RANGE (just the GUID)

This won't parse every UpdateField — too complex — but it will correctly
identify block boundaries and extract GUIDs + type bytes.
"""
import struct
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'

SMSG_LOGIN_VERIFY_WORLD = 0x42002F
SMSG_UPDATE_OBJECT = 0x580000

HG_NAMES = {0: 'Null', 2: 'Player', 3: 'Item', 4: 'GameObject', 5: 'Unit',
            6: 'Corpse', 8: 'AreaTrigger', 11: 'Scenario', 30: 'BNet',
            55: 'Housing', 56: 'MeshObject', 57: 'Entity'}


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


class Reader:
    def __init__(self, body):
        self.buf = body
        self.pos = 0

    def remaining(self):
        return len(self.buf) - self.pos

    def read_u8(self):
        v = self.buf[self.pos]
        self.pos += 1
        return v

    def read_u16(self):
        v = struct.unpack_from('<H', self.buf, self.pos)[0]
        self.pos += 2
        return v

    def read_u32(self):
        v = struct.unpack_from('<I', self.buf, self.pos)[0]
        self.pos += 4
        return v

    def read_u64(self):
        v = struct.unpack_from('<Q', self.buf, self.pos)[0]
        self.pos += 8
        return v

    def read_packed_guid128(self):
        """Read packed GUID128: loMask + hiMask + data bytes for each set bit."""
        if self.remaining() < 2:
            return None, None
        lo_mask = self.read_u8()
        hi_mask = self.read_u8()
        lo = hi = 0
        for b in range(8):
            if lo_mask & (1 << b):
                if self.remaining() < 1:
                    return None, None
                lo |= self.read_u8() << (b*8)
        for b in range(8):
            if hi_mask & (1 << b):
                if self.remaining() < 1:
                    return None, None
                hi |= self.read_u8() << (b*8)
        return lo, hi

    def skip(self, n):
        self.pos += n


def guid_info(lo, hi):
    high = (hi >> 58) & 0x3F
    sub = (hi >> 53) & 0x1F
    return high, sub


def parse_block(r, log_func):
    """Try to parse one block. Returns the end position after the block.
    Since we don't know the full fragment format, we skip to what looks like
    the next block (updateType byte 0-3 followed by valid mask bytes)."""
    start = r.pos
    if r.remaining() < 3:
        return False

    ut = r.read_u8()
    if ut > 3:
        return False

    lo, hi = r.read_packed_guid128()
    if lo is None:
        return False

    high, sub = guid_info(lo, hi)
    high_name = HG_NAMES.get(high, f'HG{high}')

    # For UT=3 (OUT_OF_RANGE), just the GUID; this block is done.
    if ut == 3:
        log_func(start, ut, high, sub, lo, hi, None, 0)
        return True

    # For UT=1/2 (CREATE), read objectType byte
    obj_type = -1
    if ut in (1, 2):
        if r.remaining() < 1:
            return False
        obj_type = r.read_u8()
        if obj_type > 20:
            return False

    # Rest is UpdateField data — variable length. We can't fully parse without
    # knowing the field definitions. Skip heuristically: look for the next
    # plausible block start. Scan forward for a byte sequence that looks like
    # a valid block boundary.
    data_start = r.pos
    remaining_body = r.buf[r.pos:]

    # Heuristic: find the next valid updateType byte followed by valid-looking
    # PackedGUID128 masks (2 bytes that could be masks — anything 0x00..0xFF)
    # AND the resulting GUID decodes to a valid HighGuid.
    best_skip = len(remaining_body)  # default: consume everything (last block)
    for i in range(1, min(len(remaining_body) - 20, 65536)):
        next_ut = remaining_body[i]
        if next_ut > 3:
            continue
        # Peek the next GUID to see if it's valid
        if i + 3 > len(remaining_body):
            continue
        try:
            test_r = Reader(remaining_body[i:])
            test_r.skip(1)  # skip ut
            tlo, thi = test_r.read_packed_guid128()
            if tlo is None or (tlo == 0 and thi == 0):
                continue
            thigh, tsub = guid_info(tlo, thi)
            if thigh == 0 or thigh > 60:
                continue
            if next_ut in (1, 2):
                if test_r.remaining() < 1:
                    continue
                tobj = test_r.buf[test_r.pos]
                if tobj > 20:
                    continue
            # Good candidate
            best_skip = i
            break
        except Exception:
            continue

    data_len = best_skip
    log_func(start, ut, high, sub, lo, hi, obj_type, data_len)
    r.pos = data_start + data_len
    return True


def scan_update_object(body, log_func):
    """Trinity UpdateData::BuildPacket format (MoP+):
      uint16 mapId
      uint32 blockCount
      1 bit: always true (unk)
      1 bit: has_destroy_guids
      IF has_destroy_guids:
         uint16 destroyCount
         uint32 totalCount (destroy + outOfRange)
         ObjectGuid * totalCount (each: 16 bytes full, not packed)
      uint32 dataSize
      dataSize bytes of blocks
    Bit-packed field: 1 byte holding up to 8 bits.
    """
    r = Reader(body)
    try:
        map_id = r.read_u16()
        block_count = r.read_u32()
    except Exception:
        return
    if r.remaining() < 1:
        return
    flag_byte = r.read_u8()  # bit-flag byte
    # Bits are written MSB-first per WriteBit. bit 7 = first written (unk=1), bit 6 = has_destroy_guids
    unk_bit = (flag_byte >> 7) & 1
    has_destroy = (flag_byte >> 6) & 1
    print(f'  Header: mapId={map_id} (0x{map_id:04X})  blockCount={block_count}  flags=0x{flag_byte:02X} unk={unk_bit} destroy={has_destroy}')

    if has_destroy:
        if r.remaining() < 6:
            return
        destroy_count = r.read_u16()
        total_destroy = r.read_u32()
        # ObjectGuid * totalCount — full 16 bytes each
        if r.remaining() < total_destroy * 16:
            return
        r.skip(total_destroy * 16)
        print(f'    Destroy block: count={destroy_count} total={total_destroy}')

    if r.remaining() < 4:
        return
    data_size = r.read_u32()
    print(f'  DataSize: {data_size}')

    # Now we're at the data blocks
    parsed = 0
    while parsed < block_count and r.remaining() > 0:
        if not parse_block(r, log_func):
            break
        parsed += 1
    print(f'  Parsed {parsed}/{block_count} blocks (remaining bytes: {r.remaining()})')


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

    housing_blocks = []
    def log(start, ut, high, sub, lo, hi, obj_type, data_len):
        ut_name = {0: 'VALUES', 1: 'CREATE', 2: 'CREATE2', 3: 'OUT'}[ut]
        hname = HG_NAMES.get(high, f'HG{high}')
        sub_str = f'/sub{sub}' if high == 55 else ''
        if high == 55:
            housing_blocks.append((ut, sub, lo, hi, obj_type, data_len))
        print(f'    [pos={start:6d}] {hname}{sub_str}  {ut_name:8s}  '
              f'GUID lo={lo:016X} hi={hi:016X}  type={obj_type}  datalen={data_len}')

    for idx, d, op, body in pkts:
        if idx <= lvw: continue
        if idx >= upper: break
        if d != 'SMSG' or op != SMSG_UPDATE_OBJECT:
            continue
        print(f'=== UPDATE_OBJECT idx={idx} (LVW+{idx-lvw}) len={len(body)} ===')
        scan_update_object(body, log)
        print()


if __name__ == '__main__':
    main()
