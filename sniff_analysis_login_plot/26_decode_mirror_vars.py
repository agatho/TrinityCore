#!/usr/bin/env python3
"""Decode the retail SMSG_MIRROR_VARS payload at idx 9976 so we know
every CVar/flag retail sets on the wire and can diff against AuthHandler.cpp."""
import struct, os

PKT = r'c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt'


def iter_packets(path):
    with open(path, 'rb') as f:
        data = f.read()
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
    """Reads LE bit-packed data same way TC ByteBuffer does.
    TC uses MSB-first within each byte accumulator (WriteBits writes high bit first).
    """
    def __init__(self, data):
        self.data = data
        self.byte_pos = 0
        self.bit_pos = 8  # bits remaining in current accumulator; forces fresh read

    def flush(self):
        # byte_pos already points to the NEXT unread byte (it was incremented
        # when the partial byte was first loaded). Just drop the remaining bits
        # in the current accumulator.
        self.bit_pos = 8

    def read_bits(self, n):
        # TC WriteBits MSB-first within each byte: value's high bits go into
        # the first unused bit position (8 - _bitpos).
        v = 0
        bits_needed = n
        while bits_needed > 0:
            if self.bit_pos == 8:
                if self.byte_pos >= len(self.data): return None
                self.cur = self.data[self.byte_pos]
                self.byte_pos += 1
                self.bit_pos = 0
            take = min(bits_needed, 8 - self.bit_pos)
            # bits in source: positions (8-bit_pos-take) .. (8-bit_pos-1) MSB-first
            shift = 8 - self.bit_pos - take
            chunk = (self.cur >> shift) & ((1 << take) - 1)
            v = (v << take) | chunk
            self.bit_pos += take
            bits_needed -= take
        return v

    def read_bytes(self, n):
        self.flush()
        out = self.data[self.byte_pos:self.byte_pos+n]
        self.byte_pos += n
        return out


def decode_mirror_vars(body):
    # struct: uint32 count; repeat { 1bit UpdateType; 24bit NameLen+1; 24bit ValueLen+1; flush-to-byte; Name bytes (NameLen+1 incl null); Value bytes (ValLen+1 incl null) }
    count = struct.unpack_from('<I', body, 0)[0]
    print(f'count = {count}, payload starts at offset 4, total payload = {len(body)-4} bytes')
    r = BitReader(body[4:])
    entries = []
    for i in range(count):
        upd = r.read_bits(1)
        name_len_plus1 = r.read_bits(24)
        val_len_plus1  = r.read_bits(24)
        if i < 5:
            print(f'  entry {i}: upd={upd} name_len+1={name_len_plus1} val_len+1={val_len_plus1} byte_pos={r.byte_pos} bit_pos={r.bit_pos}')
        if name_len_plus1 > 512 or val_len_plus1 > 512:
            print(f'  [ABORT] entry {i}: lengths out of bounds; reader desynced')
            break
        name = r.read_bytes(name_len_plus1).rstrip(b'\x00').decode('ascii', errors='replace')
        val  = r.read_bytes(val_len_plus1).rstrip(b'\x00').decode('ascii', errors='replace')
        entries.append((upd, name, val))
    return entries


def main():
    for idx, dir_, op, body in iter_packets(PKT):
        if idx == 9976 and op == 0x0042036A:
            print(f'idx={idx} op=0x{op:08X} size={len(body)}')
            entries = decode_mirror_vars(body)
            for upd, name, val in entries:
                print(f'  upd={upd}  {name!r:60} = {val!r}')
            break


if __name__ == "__main__":
    main()
