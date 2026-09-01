#!/usr/bin/env python3
"""Inventory EVERY CREATE block in a map-entry UPDATE_OBJECT bundle.

Strategy:
  * Parse PKT -> find UPDATE_OBJECT body.
  * Parse UpdateData header (uint16 map, uint32 blockCount, bits, optional destroy list,
    uint32 dataSize) -> get to payload.
  * Iterate blockCount blocks sequentially. Each block:
      uint8 updateType
      if updateType in (1,2):
          PackedGuid guid, uint8 objectType, <movement block>, uint32 valuesSize,
          uint8 fieldFlags, frag ids + 0xFF, per-frag payloads
  * We don't know the movement block length for non-housing types, so we
    HEURISTICALLY scan forward for the valuesSize uint32: a candidate p is
    valid iff
        p+4+vsz <= end_of_data
        vsz >= 2 and vsz < 0x20000
        body[p+4] (fieldFlags) < 0x40 or plausible
        fragment list at p+5 terminates with 0xFF within (vsz-1) bytes
        all intermediate IDs are in FRAGMENT_NAMES
  * Emit CSV + summary.
"""
from __future__ import annotations
import csv
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).parent
DEFS_H = Path(r"c:/TrinityBots/wt/housing-system/src/server/game/Entities/Object/Updates/WowCSEntityDefinitions.h")
OUT_DIR = Path(r"c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21")
OUT_DIR.mkdir(parents=True, exist_ok=True)

RETAIL_PKT = Path(r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt")
RETAIL_IDX = 9984
OUR_PKT = Path(r"C:/Users/daimon/Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt")
OUR_IDX = 295

UPDATE_OBJECT_OPCODE = 0x00580000

# HighGuid enum (from ObjectGuid.h)
HIGHGUID_NAMES = {
    0: "Null", 1: "Uniq", 2: "Player", 3: "Item", 4: "WorldTransaction",
    5: "StaticDoor", 6: "Transport", 7: "Conversation", 8: "Creature",
    9: "Vehicle", 10: "Pet", 11: "GameObject", 12: "DynamicObject",
    13: "AreaTrigger", 14: "Corpse", 15: "LootObject", 16: "SceneObject",
    17: "Scenario", 18: "AIGroup", 19: "DynamicDoor", 21: "Vignette",
    22: "CallForHelp", 29: "WowAccount", 30: "BNetAccount",
    55: "Housing", 56: "MeshObject", 57: "Entity",
}

# TypeID enum (from ObjectGuid.h)
TYPEID_NAMES = {
    0: "Object", 1: "Item", 2: "Container", 3: "AzeriteEmpoweredItem",
    4: "AzeriteItem", 5: "Unit", 6: "Player", 7: "ActivePlayer",
    8: "GameObject", 9: "DynamicObject", 10: "Corpse", 11: "AreaTrigger",
    12: "SceneObject", 13: "Conversation", 14: "MeshObject", 15: "AIGroup",
    16: "Scenario", 17: "LootObject", 18: "HousingEntity",
}

INDIRECT_FRAG_IDS = {2, 13, 15, 32, 37}


def parse_fragment_names():
    """Parse the enum EntityFragment block in WowCSEntityDefinitions.h."""
    text = DEFS_H.read_text(encoding="utf-8", errors="replace")
    # Find "enum ... EntityFragment" and capture until matching closing brace
    start = text.find("EntityFragment")
    if start < 0:
        raise RuntimeError("EntityFragment not found")
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError("EntityFragment opening brace not found")
    end = text.find("}", brace)
    if end < 0:
        raise RuntimeError("EntityFragment closing brace not found")
    block = text[brace+1:end]
    names = {}
    for line in block.splitlines():
        mm = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\d+)", line)
        if mm:
            names[int(mm.group(2))] = mm.group(1)
    if not names:
        raise RuntimeError("EntityFragment: no ids parsed")
    return names


FRAGMENT_NAMES = parse_fragment_names()
VALID_FRAG_IDS = set(FRAGMENT_NAMES) - {255}


# --- PKT iteration ---
def iter_packets(path: Path):
    data = path.read_bytes()
    off = min(x for x in (data.find(b"SMSG", 0, 8192), data.find(b"CMSG", 0, 8192)) if x > 0)
    idx = 0
    n = len(data)
    while off + 29 <= n:
        tag = data[off:off+4]
        if tag not in (b"SMSG", b"CMSG"):
            candidates = [x for x in (data.find(b"SMSG", off+1), data.find(b"CMSG", off+1)) if x > 0]
            if not candidates:
                return
            off = min(candidates)
            continue
        dlen = struct.unpack_from("<I", data, off+4+12)[0]
        if dlen < 4 or dlen > 50*1024*1024:
            off += 1
            continue
        ps = off + 29
        pe = ps + dlen
        if pe > n:
            return
        op = struct.unpack_from("<I", data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe
        idx += 1


def find_packet(path: Path, want_op: int, want_idx: int) -> bytes:
    for idx, _, op, body in iter_packets(path):
        if idx == want_idx and op == want_op:
            return body
    raise RuntimeError(f"No packet op=0x{want_op:X} idx={want_idx} found in {path}")


# --- BitBuffer reader for UpdateData header ---
class BitReader:
    def __init__(self, body: bytes, off: int):
        self.body = body
        self.off = off          # byte cursor for stream bytes
        self.bitpos = 8          # next bit in byte buffer (8 = need new byte)
        self.bitbuf = 0

    def _refill(self):
        self.bitbuf = self.body[self.off]
        self.off += 1
        self.bitpos = 0

    def read_bit(self) -> int:
        if self.bitpos == 8:
            self._refill()
        # Bits are written MSB-first
        bit = (self.bitbuf >> (7 - self.bitpos)) & 1
        self.bitpos += 1
        return bit

    def flush_to_byte(self):
        # not strictly needed — in ByteBuffer, the u8/u16/u32 read APIs
        # flush bits implicitly. But in TrinityCore, reads are sequential
        # and after a FlushBits() any further byte reads start at current off.
        self.bitpos = 8

    def read_u16(self) -> int:
        self.flush_to_byte()
        v = struct.unpack_from("<H", self.body, self.off)[0]
        self.off += 2
        return v

    def read_u32(self) -> int:
        self.flush_to_byte()
        v = struct.unpack_from("<I", self.body, self.off)[0]
        self.off += 4
        return v

    def read_raw(self, n: int) -> bytes:
        self.flush_to_byte()
        b = self.body[self.off:self.off+n]
        self.off += n
        return b


def parse_update_object_header(body: bytes):
    """Return (payload_bytes, blockCount, mapId)."""
    # Structure:
    #   uint16 map
    #   uint32 blockCount
    #   1 bit unk
    #   1 bit hasOutOfRange
    #   if hasOutOfRange:
    #       uint16 destroyCount
    #       uint32 totalCount
    #       for each destroy: ObjectGuid (written via operator<<)
    #       for each oor:      ObjectGuid
    #   uint32 dataSize
    #   bytes  data[dataSize]
    map_id = struct.unpack_from("<H", body, 0)[0]
    block_count = struct.unpack_from("<I", body, 2)[0]
    # Bit region starts at offset 6. But ByteBuffer writes bits after
    # the last byte ops: Initialize wrote uint16 then uint32, then WriteBit.
    # ByteBuffer flushes bits when a byte write happens. So bits live in
    # their own byte starting at off=6.
    # Two bits -> occupy 1 byte at off=6 (rest of byte is padding).
    bits_byte = body[6]
    unk = (bits_byte >> 7) & 1
    has_oor = (bits_byte >> 6) & 1
    cursor = 7
    if has_oor:
        destroy_count = struct.unpack_from("<H", body, cursor)[0]; cursor += 2
        total_count = struct.unpack_from("<I", body, cursor)[0]; cursor += 4
        # ObjectGuid on-wire: format is PackedGuid128 (2-byte mask + bytes)
        # Skip each by reading its PackedGuid layout.
        for _ in range(total_count):
            _, cursor = read_packed_guid(body, cursor)
    data_size = struct.unpack_from("<I", body, cursor)[0]; cursor += 4
    payload = body[cursor:cursor+data_size]
    return payload, block_count, map_id


# --- PackedGuid + utils ---
def read_packed_guid(body: bytes, off: int):
    m_lo, m_hi = body[off], body[off+1]
    off += 2
    lo = hi = 0
    for b in range(8):
        if m_lo & (1 << b):
            lo |= body[off] << (b*8); off += 1
    for b in range(8):
        if m_hi & (1 << b):
            hi |= body[off] << (b*8); off += 1
    return (lo, hi), off


def decode_guid(lo: int, hi: int):
    htype = (hi >> 58) & 0x3F
    subtype = (hi >> 53) & 0x1F  # 5 bits sub (hi bits 53..57)
    return htype, subtype


# --- Heuristic search for valuesSize + fragment list ---
def is_valid_fragment_list(body: bytes, start: int, limit: int) -> tuple[bool, int, list[int]]:
    """Starting at `start`, walk bytes as fragment IDs until 0xFF.
       Returns (ok, endOffset_inclusive_of_FF, ids).
       Fails if a non-FF unknown id is encountered, or limit reached."""
    cursor = start
    ids = []
    while cursor < start + limit:
        fid = body[cursor]
        cursor += 1
        if fid == 255:
            return True, cursor, ids
        if fid not in VALID_FRAG_IDS:
            return False, cursor, ids
        ids.append(fid)
    return False, cursor, ids


VALID_FIELD_FLAGS = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}


def find_values_size_anchor(body: bytes, search_from: int, search_to: int,
                            payload_total: int, require_next_valid: bool = True) -> tuple[int, int, int, list[int]]:
    """Search for valuesSize anchor in [search_from, search_to).
       Returns the SMALLEST-vsz plausible anchor (shortest block). Checks:
         - fieldFlags in VALID_FIELD_FLAGS (0..0x0F)
         - fragment list walks only valid IDs and ends on 0xFF
         - frag list's 0xFF ends at position < block_end (fragments consume < vsz-1 bytes)
         - after block_end, either at exact end-of-payload OR next byte is
           a valid updateType (0..4)
    """
    best = None  # (vsz, p, ff, ids)
    for p in range(search_from, search_to - 4):
        vsz = struct.unpack_from("<I", body, p)[0]
        if vsz < 2 or vsz > 0x40000:
            continue
        block_end = p + 4 + vsz
        if block_end > payload_total:
            continue
        ff = body[p + 4]
        if ff not in VALID_FIELD_FLAGS:
            continue
        ok, end_frags, ids = is_valid_fragment_list(body, p + 5, vsz - 1)
        if not ok:
            continue
        if not ids:
            continue
        # Require fragment list 0xFF terminator lands strictly inside the block
        if end_frags > block_end:
            continue
        if require_next_valid and block_end < payload_total:
            nxt = body[block_end]
            if nxt not in (0, 1, 2, 3, 4):
                continue
        # Keep smallest vsz (shortest declared block). We'll do smarter
        # object-type-specific parsing upstream.
        key = (vsz, p)
        if best is None or key < (best[0], best[1]):
            best = (vsz, p, ff, ids)
    if best is None:
        return -1, 0, 0, []
    vsz, p, ff, ids = best
    return p, vsz, ff, ids


# --- Block iteration ---
# Required "Tag" fragment per object type (client-side enforced).
OBJTYPE_REQUIRED_TAGS = {
    1: 200,   # Item -> Tag_Item
    2: 200,   # Container -> Tag_Item (+ Tag_Container)
    3: 200,   # AzeriteEmpoweredItem -> Tag_Item
    4: 200,   # AzeriteItem -> Tag_Item
    5: 204,   # Unit -> Tag_Unit
    6: 204,   # Player -> Tag_Unit (+Tag_Player)
    7: 204,   # ActivePlayer -> Tag_Unit (+Tag_Player+Tag_ActivePlayer)
    8: 206,   # GameObject -> Tag_GameObject
    9: 207,   # DynamicObject -> Tag_DynamicObject
    10: 208,  # Corpse -> Tag_Corpse
    11: 209,  # AreaTrigger -> Tag_AreaTrigger
    12: 210,  # SceneObject -> Tag_SceneObject
    13: 211,  # Conversation -> Tag_Conversation
    14: 221,  # MeshObject -> Tag_MeshObject
    15: 212,  # AIGroup -> Tag_AIGroup
    16: 213,  # Scenario -> Tag_Scenario
    17: 214,  # LootObject -> Tag_LootObject
    # 18 HousingEntity does NOT require a single tag — uses housing fragments.
}


def fragments_match_objtype(obj_type: int, frag_ids: list[int]) -> bool:
    """Sanity check: does this fragment list look consistent with this object type?"""
    if obj_type == 0 or obj_type > 18:
        return False
    if obj_type == 18:
        # HousingEntity: requires at least one housing-related fragment
        housing_frags = {19, 20, 21, 22, 23, 27, 28, 29, 30, 33, 34}
        if not (set(frag_ids) & housing_frags or 31 in frag_ids):
            return False
        return True
    req = OBJTYPE_REQUIRED_TAGS.get(obj_type)
    if req is None:
        return True
    return req in frag_ids


def parse_bits(bits_bytes: bytes, nbits: int) -> list[bool]:
    """MSB-first bit parsing from ByteBuffer WriteBit sequences."""
    out = []
    for i in range(nbits):
        byte = bits_bytes[i // 8]
        bit = (byte >> (7 - (i % 8))) & 1
        out.append(bool(bit))
    return out


MOVEMENT_BIT_NAMES = [
    "HasEntityPosition", "NoBirthAnim", "EnablePortals", "PlayHoverAnim",
    "ThisIsYou", "MovementUpdate", "MovementTransport", "Stationary",
    "CombatVictim", "ServerTime", "Vehicle", "AnimKit",
    "Rotation", "GameObject", "SmoothPhasing", "SceneObject",
    "ActivePlayer", "Conversation", "Room", "Decor", "MeshObject",
]


def compute_movement_length(payload: bytes, off: int, object_type: int) -> int | None:
    """Given a cursor at start of movement block, compute how many bytes
       the movement block occupies BEFORE the valuesSize u32. Returns None
       if we don't know how to parse for this object type (signals the caller
       to fall back on heuristic search)."""
    if off + 3 > len(payload):
        return None
    bits = parse_bits(payload[off:off+3], 21)
    flags = {n: v for n, v in zip(MOVEMENT_BIT_NAMES, bits)}
    length = 3 + 4  # bits + uint32 PauseTimes.size()
    # Skip types with MovementUpdate (complex) — handle via heuristic.
    if flags["MovementUpdate"]:
        return None
    if flags["Stationary"]:
        length += 16  # X,Y,Z,O
    if flags["CombatVictim"]:
        return None  # packedguid variable
    if flags["ServerTime"]:
        length += 4
    if flags["Vehicle"]:
        length += 8
    if flags["AnimKit"]:
        length += 6
    if flags["Rotation"]:
        length += 8
    if flags["GameObject"]:
        return None  # has WorldState updates, complex
    if flags["SmoothPhasing"]:
        return None
    if flags["SceneObject"]:
        return None
    if flags["ActivePlayer"]:
        return None
    if flags["Conversation"]:
        return None
    if flags["Room"]:
        length += 16  # ObjectGuid
    if flags["Decor"]:
        length += 16
    if flags["MeshObject"]:
        length += 16 + 12 + 16 + 4 + 1
    return length


def iterate_blocks(payload: bytes, block_count: int):
    """Yield (block_index, start_off, kind, info_dict)."""
    cursor = 0
    n = len(payload)
    blk_idx = 0
    while blk_idx < block_count and cursor < n:
        start = cursor
        ut = payload[cursor]; cursor += 1
        if ut in (1, 2):  # CREATE / CREATE2
            guid, guid_end = read_packed_guid(payload, cursor)
            cursor = guid_end
            obj_type = payload[cursor]; cursor += 1
            # Try deterministic movement-length first.
            mov_len = compute_movement_length(payload, cursor, obj_type)
            if mov_len is not None and cursor + mov_len + 5 <= n:
                p_det = cursor + mov_len
                vsz_det = struct.unpack_from("<I", payload, p_det)[0]
                ff_det = payload[p_det + 4] if p_det + 4 < n else 0xFF
                if (2 <= vsz_det <= 0x40000 and ff_det in VALID_FIELD_FLAGS
                        and p_det + 4 + vsz_det <= n):
                    ok, _eof, ids_det = is_valid_fragment_list(payload, p_det + 5, vsz_det - 1)
                    if ok and ids_det:
                        p, vsz, ff, frag_ids = p_det, vsz_det, ff_det, ids_det
                        block_end = p + 4 + vsz
                        yield blk_idx, start, "CREATE", {
                            "update_type": ut, "guid": guid, "object_type": obj_type,
                            "movement_bytes": payload[cursor:p],
                            "values_size_off": p,
                            "values_size": vsz, "field_flags": ff,
                            "fragment_ids": frag_ids,
                            "frag_payload_bytes": payload[p+5 + len(frag_ids) + 1 : block_end],
                            "block_end": block_end,
                        }
                        cursor = block_end
                        blk_idx += 1
                        continue
            search_end = min(n, cursor + 8192)
            p, vsz, ff, frag_ids = find_values_size_anchor(payload, cursor, search_end, n)
            # Plausibility check: object type vs fragments
            if p >= 0 and not fragments_match_objtype(obj_type, frag_ids):
                p = -1
            if p < 0:
                # Try to recover: scan forward for next plausible updateType byte
                recover = scan_for_next_block(payload, cursor, n)
                yield blk_idx, start, "CREATE_UNPARSED", {
                    "update_type": ut, "guid": guid, "object_type": obj_type,
                    "cursor_after_objtype": cursor,
                    "raw_from_cursor": payload[cursor:min(cursor+256, n)],
                }
                if recover < 0:
                    return
                cursor = recover
                blk_idx += 1
                continue
            block_end = p + 4 + vsz
            movement_bytes = payload[cursor:p]
            frag_payload = payload[p+5 + len(frag_ids) + 1 : block_end]
            yield blk_idx, start, "CREATE", {
                "update_type": ut, "guid": guid, "object_type": obj_type,
                "movement_bytes": movement_bytes,
                "values_size_off": p,
                "values_size": vsz,
                "field_flags": ff,
                "fragment_ids": frag_ids,
                "frag_payload_bytes": frag_payload,
                "block_end": block_end,
            }
            cursor = block_end
        elif ut == 0:  # VALUES
            guid, guid_end = read_packed_guid(payload, cursor)
            cursor = guid_end
            search_end = min(n, cursor + 8192)
            p, vsz, ff, frag_ids = find_values_size_anchor(payload, cursor, search_end, n)
            if p < 0:
                recover = scan_for_next_block(payload, cursor, n)
                yield blk_idx, start, "VALUES_UNPARSED", {"guid": guid, "cursor": cursor}
                if recover < 0:
                    return
                cursor = recover
                blk_idx += 1
                continue
            block_end = p + 4 + vsz
            yield blk_idx, start, "VALUES", {
                "guid": guid, "values_size": vsz, "field_flags": ff,
                "fragment_ids": frag_ids, "block_end": block_end,
            }
            cursor = block_end
        elif ut == 3:
            cnt = struct.unpack_from("<H", payload, cursor)[0]; cursor += 2
            for _ in range(cnt):
                _, cursor = read_packed_guid(payload, cursor)
            yield blk_idx, start, "OOR", {"count": cnt}
        elif ut == 4:
            cnt = struct.unpack_from("<H", payload, cursor)[0]; cursor += 2
            for _ in range(cnt):
                _, cursor = read_packed_guid(payload, cursor)
            yield blk_idx, start, "NEAR", {"count": cnt}
        else:
            # Invalid updateType — try to recover by scanning for the next one
            recover = scan_for_next_block(payload, start + 1, n)
            yield blk_idx, start, f"UNKNOWN_{ut}", {"raw": payload[start:min(start+32, n)]}
            if recover < 0:
                return
            cursor = recover
        blk_idx += 1


def scan_for_next_block(payload: bytes, start: int, n: int) -> int:
    """Walk forward from `start` looking for a byte that is a plausible updateType
       (1 or 2) followed by a valid PackedGuid + objectType that yields a valid
       valuesSize anchor. Returns -1 if nothing found."""
    for p in range(start, min(start + 32768, n - 12)):
        ut = payload[p]
        if ut not in (1, 2):
            continue
        try:
            guid, ge = read_packed_guid(payload, p + 1)
        except Exception:
            continue
        if ge >= n:
            continue
        # objectType sanity
        ot = payload[ge]
        if ot == 0 or ot > 18:
            continue
        ap, avsz, _, aids = find_values_size_anchor(payload, ge + 1, min(n, ge + 1 + 8192), n)
        if ap < 0:
            continue
        if not fragments_match_objtype(ot, aids):
            continue
        return p
    return -1


# --- Inventory ---
def build_inventory(label: str, payload_bytes: bytes, block_count: int):
    rows = []
    entity_idx = 0
    unparsed = 0
    non_create = 0
    blocks_seen = 0
    for blk_idx, start, kind, info in iterate_blocks(payload_bytes, block_count):
        blocks_seen += 1
        if kind == "CREATE":
            entity_idx += 1
            lo, hi = info["guid"]
            htype, subtype = decode_guid(lo, hi)
            ot = info["object_type"]
            frag_ids = info["fragment_ids"]
            row = {
                "entity_index": entity_idx,
                "byte_offset": f"0x{start:X}",
                "updateType": info["update_type"],
                "highGuid": htype,
                "highGuidName": HIGHGUID_NAMES.get(htype, f"Unknown({htype})"),
                "subType": subtype,
                "guid_lo": f"0x{lo:016x}",
                "guid_hi": f"0x{hi:016x}",
                "objectType": ot,
                "objectTypeName": TYPEID_NAMES.get(ot, f"Unknown({ot})"),
                "valuesSize": info["values_size"],
                "fragmentCount": len(frag_ids),
                "fragments": ",".join(FRAGMENT_NAMES.get(f, f"Unknown({f})") for f in frag_ids),
                "raw_bytes": "",
            }
            rows.append(row)
        elif kind == "CREATE_UNPARSED":
            entity_idx += 1
            unparsed += 1
            lo, hi = info["guid"]
            htype, subtype = decode_guid(lo, hi)
            rows.append({
                "entity_index": entity_idx,
                "byte_offset": f"0x{start:X}",
                "updateType": info["update_type"],
                "highGuid": htype,
                "highGuidName": HIGHGUID_NAMES.get(htype, f"Unknown({htype})"),
                "subType": subtype,
                "guid_lo": f"0x{lo:016x}",
                "guid_hi": f"0x{hi:016x}",
                "objectType": info["object_type"],
                "objectTypeName": TYPEID_NAMES.get(info["object_type"], f"Unknown({info['object_type']})"),
                "valuesSize": -1,
                "fragmentCount": -1,
                "fragments": "<UNPARSED>",
                "raw_bytes": info["raw_from_cursor"][:64].hex(),
            })
        else:
            non_create += 1
    return rows, {"unparsed": unparsed, "non_create": non_create,
                  "blocks_seen": blocks_seen, "block_count": block_count}


def write_csv(rows, path: Path):
    if not rows:
        path.write_text("(no rows)\n", encoding="utf-8")
        return
    cols = ["entity_index", "byte_offset", "updateType", "highGuid", "highGuidName",
            "subType", "guid_lo", "guid_hi", "objectType", "objectTypeName",
            "valuesSize", "fragmentCount", "fragments", "raw_bytes"]
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)


def classify_role(row):
    ht = row["highGuid"]; st = row["subType"]
    frags = row["fragments"].split(",")
    fset = set(frags)
    if ht == 55 and st == 3 and "FHousingPlayerHouse_C" in fset:
        return "Housing/3 identity (FHousingPlayerHouse_C)"
    if ht == 55 and st == 2 and "FHousingRoom_C" in fset:
        return "Housing/2 room (FHousingRoom_C)"
    if ht == 55 and st == 1 and "FHousingDecor_C" in fset:
        return "Housing/1 decor (FHousingDecor_C)"
    if ht == 55 and st == 4:
        return "Housing/4 neighborhood-mirror"
    if ht == 55:
        return f"Housing/{st} other"
    if ht == 56:
        return "MeshObject (HighGuid=56)"
    if ht == 57:
        return "Entity mirror (HighGuid=57)"
    if ht == 13 and "FHousingPlotAreaTrigger_C" in fset:
        return "AreaTrigger plot (HighGuid=13)"
    if ht == 13:
        return "AreaTrigger other (HighGuid=13)"
    if ht == 11:
        return "GameObject (HighGuid=11)"
    if ht == 8:
        return "Creature (HighGuid=8)"
    if ht == 2:
        return "Player (HighGuid=2)"
    if ht == 3:
        return "Item (HighGuid=3)"
    if ht == 30:
        return "BNetAccount (HighGuid=30) -- non-standard for map entry"
    return f"Other ({HIGHGUID_NAMES.get(ht, ht)})"


def write_summary(label: str, rows, diag, path: Path):
    total = len(rows)
    by_hg = Counter((r["highGuid"], r["highGuidName"]) for r in rows)
    by_hg_ot = Counter((r["highGuid"], r["highGuidName"], r["objectType"], r["objectTypeName"]) for r in rows)
    frag_sets = Counter(tuple(r["fragments"].split(",")) for r in rows)
    roles = Counter(classify_role(r) for r in rows)

    lines = []
    lines.append(f"# {label} UPDATE_OBJECT Entity Inventory\n")
    lines.append(f"- Total CREATE blocks parsed: **{total}**")
    lines.append(f"- Unparsed (slurped raw): **{diag['unparsed']}**")
    lines.append(f"- Non-CREATE blocks (values/oor/near): **{diag['non_create']}**")
    lines.append("")
    lines.append("## Breakdown by HighGuid\n")
    lines.append("| HighGuid | Name | Count |")
    lines.append("|---|---|---|")
    for (hg, name), c in sorted(by_hg.items(), key=lambda x: -x[1]):
        lines.append(f"| {hg} | {name} | {c} |")
    lines.append("")
    lines.append("## Breakdown by (HighGuid, ObjectType)\n")
    lines.append("| HighGuid | HGName | ObjType | OTName | Count |")
    lines.append("|---|---|---|---|---|")
    for (hg, hgn, ot, otn), c in sorted(by_hg_ot.items(), key=lambda x: -x[1]):
        lines.append(f"| {hg} | {hgn} | {ot} | {otn} | {c} |")
    lines.append("")
    lines.append("## Top 20 fragment signatures\n")
    lines.append("| Count | Fragments |")
    lines.append("|---|---|")
    for sig, c in frag_sets.most_common(20):
        lines.append(f"| {c} | `{', '.join(sig)}` |")
    lines.append("")
    lines.append("## Entities by role\n")
    lines.append("| Role | Count |")
    lines.append("|---|---|")
    for role, c in sorted(roles.items(), key=lambda x: -x[1]):
        lines.append(f"| {role} | {c} |")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def run_one(label: str, pkt: Path, idx: int):
    print(f"[{label}] reading {pkt} idx={idx}...")
    body = find_packet(pkt, UPDATE_OBJECT_OPCODE, idx)
    print(f"[{label}] UPDATE_OBJECT body size = {len(body)} bytes")
    payload, block_count, map_id = parse_update_object_header(body)
    print(f"[{label}] map={map_id} blockCount={block_count} payload={len(payload)} bytes")
    rows, diag = build_inventory(label, payload, block_count)
    csv_path = OUT_DIR / f"{label}_entities.csv"
    md_path = OUT_DIR / f"{label}_entities_summary.md"
    write_csv(rows, csv_path)
    write_summary(label, rows, diag, md_path)
    print(f"[{label}] wrote {csv_path.name}, {md_path.name}  (entities={len(rows)}, unparsed={diag['unparsed']}, blocks_seen={diag['blocks_seen']}/{diag['block_count']})")
    return rows


def side_by_side(retail_rows, our_rows):
    def role_counts(rows):
        c = Counter(classify_role(r) for r in rows)
        return c
    rc = role_counts(retail_rows)
    oc = role_counts(our_rows)
    all_roles = sorted(set(rc) | set(oc))
    print()
    print("=" * 78)
    print(f"{'ROLE':<45} {'RETAIL':>10} {'OURS':>10} {'DELTA':>10}")
    print("=" * 78)
    for role in all_roles:
        r = rc.get(role, 0); o = oc.get(role, 0)
        print(f"{role:<45} {r:>10} {o:>10} {o-r:>+10}")
    print("-" * 78)
    print(f"{'TOTAL':<45} {sum(rc.values()):>10} {sum(oc.values()):>10} {sum(oc.values())-sum(rc.values()):>+10}")
    print("=" * 78)


def main():
    print(f"Fragment names loaded: {len(FRAGMENT_NAMES)} IDs from {DEFS_H.name}")
    retail_rows = run_one("RETAIL_idx9984", RETAIL_PKT, RETAIL_IDX)
    our_rows    = run_one("OUR_idx295",    OUR_PKT,    OUR_IDX)
    side_by_side(retail_rows, our_rows)


if __name__ == "__main__":
    main()
