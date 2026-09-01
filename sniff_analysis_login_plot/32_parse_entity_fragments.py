#!/usr/bin/env python3
"""Parse HighGuid::Entity CREATE blocks (objectType=18) from retail idx 9984.

Wire format of a CREATE block (from BaseEntity::BuildCreateUpdateBlockForPlayer):
    uint8  updateType              (1=CREATE_OBJECT, 2=CREATE_OBJECT2)
    PackedGuid GUID                (16 bytes packed with 2 mask bytes)
    uint8  objectType              (18 = HOUSING_ENTITY here)
    <movement block>
        21 WriteBit calls -> flushed to 3 bytes (bit 0 = HasEntityPosition,
                                                 bit 20 = MeshObject)
        Conditional fields per flag (MovementUpdate, Stationary, Rotation, ...)
        uint32 PauseTimes.size()   (always present, 0 for non-gameobjects)
    uint32 valuesBufferSize         (= remaining bytes in this CREATE block
                                       starting from fieldFlags)
    uint8  fieldFlags
    <fragmentIdList>                (bytes, terminated by 0xFF)
    <per-fragment write-create blob>
"""
from __future__ import annotations
import struct
from dataclasses import dataclass, field
from typing import List, Tuple

RETAIL = r"c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt"

# Fragment ID -> name map, from WowCSEntityDefinitions.h
FRAGMENT_NAMES = {
    1:   "FEntityPosition",
    2:   "CGObject",
    5:   "FTransportLink",
    13:  "FPlayerOwnershipLink",
    15:  "CActor",
    17:  "FVendor_C",
    18:  "FMirroredObject_C",
    19:  "FMeshObjectData_C",
    20:  "FHousingDecor_C",
    21:  "FHousingRoom_C",
    22:  "FHousingRoomComponentMesh_C",
    23:  "FHousingPlayerHouse_C",
    27:  "FJamHousingCornerstone_C",
    28:  "FHousingDecorActor_C",
    29:  "FHousingPlotAreaTrigger_C",
    30:  "FNeighborhoodMirrorData_C",
    31:  "FMirroredPositionData_C",
    32:  "PlayerHouseInfoComponent_C",
    33:  "FHousingStorage_C",
    34:  "FHousingFixture_C",
    37:  "PlayerInitiativeComponent_C",
    200: "Tag_Item",
    201: "Tag_Container",
    202: "Tag_AzeriteEmpoweredItem",
    203: "Tag_AzeriteItem",
    204: "Tag_Unit",
    205: "Tag_Player",
    206: "Tag_GameObject",
    207: "Tag_DynamicObject",
    208: "Tag_Corpse",
    209: "Tag_AreaTrigger",
    210: "Tag_SceneObject",
    211: "Tag_Conversation",
    212: "Tag_AIGroup",
    213: "Tag_Scenario",
    214: "Tag_LootObject",
    215: "Tag_ActivePlayer",
    216: "Tag_ActiveClient_S",
    217: "Tag_ActiveObject_C",
    218: "Tag_VisibleObject_C",
    219: "Tag_UnitVehicle",
    220: "Tag_HousingRoom",
    221: "Tag_MeshObject",
    224: "Tag_HouseExteriorPiece",
    225: "Tag_HouseExteriorRoot",
    255: "End",
}

INDIRECT = {2, 13, 15, 32, 37}  # IsIndirectFragment -> prefixed with 1 byte (0/1)


# ---------------------------------------------------------------------------
# Packet scanning
# ---------------------------------------------------------------------------
def iter_packets(path):
    data = open(path, "rb").read()
    off = min(x for x in (data.find(b"SMSG", 0, 4096), data.find(b"CMSG", 0, 4096)) if x > 0)
    idx = 0
    while off + 29 <= len(data):
        tag = data[off:off+4]
        if tag not in (b"SMSG", b"CMSG"):
            nx = [x for x in (data.find(b"SMSG", off+1), data.find(b"CMSG", off+1)) if x > 0]
            if not nx:
                return
            off = min(nx)
            continue
        h = data[off+4:off+29]
        dlen = struct.unpack_from("<I", h, 12)[0]
        if dlen > 50*1024*1024 or dlen < 4:
            off += 1
            continue
        ps = off+29
        pe = ps+dlen
        if pe > len(data):
            return
        op = struct.unpack_from("<I", data, ps)[0]
        yield idx, tag.decode(), op, data[ps+4:pe]
        off = pe
        idx += 1


# ---------------------------------------------------------------------------
# Low-level parsers
# ---------------------------------------------------------------------------
def read_packed_guid(body: bytes, off: int) -> Tuple[Tuple[int, int], int]:
    m_lo, m_hi = body[off], body[off+1]
    off += 2
    lo = hi = 0
    for b in range(8):
        if m_lo & (1 << b):
            lo |= body[off] << (b*8)
            off += 1
    for b in range(8):
        if m_hi & (1 << b):
            hi |= body[off] << (b*8)
            off += 1
    return (lo, hi), off


def guid_to_str(lo: int, hi: int) -> str:
    htype = (hi >> 58) & 0x3F
    return f"Guid<type={htype},lo=0x{lo:016x},hi=0x{hi:016x}>"


def parse_bits(bits_bytes: bytes, nbits: int) -> List[bool]:
    """ByteBuffer packs WriteBit MSB-first starting at bit 7 of first byte.
       The first WriteBit goes into bit 7, second into bit 6, etc."""
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


# ---------------------------------------------------------------------------
# Movement block length computation for housing entities
# ---------------------------------------------------------------------------
def movement_block_length(body: bytes, off: int) -> Tuple[int, dict]:
    """Returns (length_bytes, parsed_flags_dict). Only supports housing entity
       shape (no MovementUpdate). Supported flag tails: Stationary, Rotation,
       Room/Decor/MeshObject extensions."""
    bits = parse_bits(body[off:off+3], 21)
    flags = {n: v for n, v in zip(MOVEMENT_BIT_NAMES, bits)}
    length = 3
    # Always: uint32 PauseTimes.size() after bits, per BuildMovementUpdate
    length += 4  # uint32 PauseTimes.size() (we assume 0 for non-GO)
    if flags["Stationary"]:
        length += 16  # XYZO
    if flags["CombatVictim"]:
        length += 16  # PackedGuid worst case -- rare, unlikely
    if flags["ServerTime"]:
        length += 4
    if flags["Vehicle"]:
        length += 8
    if flags["AnimKit"]:
        length += 6
    if flags["Rotation"]:
        length += 8
    if flags["Room"]:
        length += 16  # ObjectGuid
    if flags["Decor"]:
        length += 16
    if flags["MeshObject"]:
        length += 16 + 12 + 16 + 4 + 1  # GUID + pos(12) + quat(16) + scale(4) + flags(1)
    return length, flags


# ---------------------------------------------------------------------------
# Fragment list parser
# ---------------------------------------------------------------------------
def parse_fragment_ids(body: bytes, off: int) -> Tuple[List[int], int]:
    ids = []
    while off < len(body):
        fid = body[off]
        off += 1
        if fid == 255:
            return ids, off
        ids.append(fid)
    raise ValueError("Fragment list not terminated")


# ---------------------------------------------------------------------------
# FMirroredPositionData_C write-create parser
# ---------------------------------------------------------------------------
def parse_mirrored_position(body: bytes, off: int):
    """Read FMirroredPositionData_C.WriteCreate:
         PackedGuid AttachParentGUID
         float XYZ
         float QuaternionXYZW
         float Scale
         uint8 AttachmentFlags
       Returns (dict, new_off)."""
    start = off
    guid, off = read_packed_guid(body, off)
    x, y, z = struct.unpack_from("<fff", body, off); off += 12
    qx, qy, qz, qw = struct.unpack_from("<ffff", body, off); off += 16
    scale, = struct.unpack_from("<f", body, off); off += 4
    flags = body[off]; off += 1
    return {
        "AttachParentGUID": guid_to_str(*guid),
        "Position": (x, y, z),
        "Rotation": (qx, qy, qz, qw),
        "Scale": scale,
        "AttachmentFlags": flags,
        "bytes": body[start:off],
    }, off


# ---------------------------------------------------------------------------
# Entity CREATE block parser
# ---------------------------------------------------------------------------
@dataclass
class Entity:
    start: int
    update_type: int
    guid_raw: Tuple[int, int]
    guid_range: Tuple[int, int]
    object_type: int
    movement_bytes: bytes
    movement_flags: dict
    values_size: int
    field_flags: int
    fragment_ids: List[int]
    fragment_ids_range: Tuple[int, int]
    fragment_data: List[Tuple[int, str, bytes]] = field(default_factory=list)
    end: int = 0
    parsed: dict = field(default_factory=dict)


def parse_create_block(body: bytes, off: int) -> Entity:
    start = off
    update_type = body[off]; off += 1
    guid_start = off
    guid, off = read_packed_guid(body, off)
    guid_end = off
    object_type = body[off]; off += 1
    mov_start = off
    mov_len, mov_flags = movement_block_length(body, off)
    off = mov_start + mov_len
    values_size = struct.unpack_from("<I", body, off)[0]; off += 4
    field_flags_pos = off
    field_flags = body[off]; off += 1
    frag_start = off
    frag_ids, off = parse_fragment_ids(body, off)
    frag_end = off
    block_end = field_flags_pos + values_size

    e = Entity(
        start=start,
        update_type=update_type,
        guid_raw=guid,
        guid_range=(guid_start, guid_end),
        object_type=object_type,
        movement_bytes=body[mov_start:mov_start+mov_len],
        movement_flags=mov_flags,
        values_size=values_size,
        field_flags=field_flags,
        fragment_ids=frag_ids,
        fragment_ids_range=(frag_start, frag_end),
        end=block_end,
    )

    # Split fragment data by next fragment boundary (naive: whole remainder per each).
    # Without full per-fragment layout we can at least parse FMirroredPositionData_C.
    frag_data_start = off
    frag_blob = body[frag_data_start:block_end]

    # Parse FMirroredPositionData_C if present (fixed 37 bytes worst-case incl packedguid)
    if 31 in frag_ids:
        # Heuristic: find a candidate starting offset where we can read a
        # valid PackedGuid then 12+16+4+1 = 33 bytes more. Simplest: parse at
        # frag_data_start ASSUMING FMirroredPositionData_C comes first in the
        # write sequence. For IsUpdateableFragment order we instead walk in
        # the order fragments were added.
        pass

    # Sequential fragment data walk. Each non-Tag, non-End fragment has a
    # WriteCreate payload. Tag fragments (>=200) have NO payload. Indirect
    # fragments have a leading 1-byte "IndirectFragmentActive" before the
    # write-create blob. We try to parse FMirroredPositionData_C in-stream;
    # we cannot safely parse others without their full layout, so we slurp
    # the remainder into the first non-tag non-parseable fragment.
    cursor = frag_data_start
    parsed_frags = []
    for i, fid in enumerate(frag_ids):
        name = FRAGMENT_NAMES.get(fid, f"Unknown({fid})")
        if fid >= 200 and fid < 255:
            # Tag fragment -> no payload
            parsed_frags.append((fid, name, b""))
            continue

        chunk_start = cursor
        indirect_byte = None
        if fid in INDIRECT:
            indirect_byte = body[cursor]
            cursor += 1

        # Try to parse known fragments
        if fid == 31:  # FMirroredPositionData_C
            mpd, new_cur = parse_mirrored_position(body, cursor)
            e.parsed.setdefault("MirroredPositionData", []).append(mpd)
            parsed_frags.append((fid, name, body[chunk_start:new_cur]))
            cursor = new_cur
            continue

        # Unknown payload -> give it rest of blob to block_end (we stop after
        # the first unknown so remaining fragments come out as empty slices).
        rest = body[chunk_start:block_end]
        parsed_frags.append((fid, name, rest))
        cursor = block_end
        # Continue so remaining fragments appear but with empty data
        for fid2 in frag_ids[i+1:]:
            name2 = FRAGMENT_NAMES.get(fid2, f"Unknown({fid2})")
            parsed_frags.append((fid2, name2, b""))
        break

    e.fragment_data = parsed_frags
    return e


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------
def find_entity_create_offsets(body: bytes):
    """Heuristic: scan for updateType=1/2 followed by a valid PackedGuid whose
       HighGuid==Entity(57) and whose next byte is objectType=18."""
    seen = set()
    offs = []
    for o in range(len(body) - 32):
        ut = body[o]
        if ut not in (1, 2):
            continue
        try:
            guid, end = read_packed_guid(body, o+1)
        except Exception:
            continue
        lo, hi = guid
        if hi < (1 << 56):
            continue
        if (hi >> 58) != 57:
            continue
        if end >= len(body):
            continue
        if body[end] != 18:
            continue
        if (lo, hi) in seen:
            continue
        seen.add((lo, hi))
        offs.append(o)
    return offs


def hex_row(b: bytes, per=16) -> str:
    return " ".join(f"{x:02x}" for x in b[:per])


def fmt_bytes(b: bytes, max_bytes=64) -> str:
    if len(b) <= max_bytes:
        return " ".join(f"{x:02x}" for x in b)
    return " ".join(f"{x:02x}" for x in b[:max_bytes]) + f" ... ({len(b)} bytes total)"


def main():
    for idx, dir_, op, body in iter_packets(RETAIL):
        if op != 0x00580000 or idx != 9984:
            continue

        print(f"=== idx={idx}  body_size={len(body)} ===")

        offs = find_entity_create_offsets(body)
        print(f"Found {len(offs)} HighGuid::Entity objectType=18 CREATEs")
        for o in offs:
            print(f"  offset=0x{o:X}")

        print()
        print("Fragment ID -> Name map:")
        for fid in sorted(FRAGMENT_NAMES):
            print(f"  {fid:3d} (0x{fid:02X}) = {FRAGMENT_NAMES[fid]}")
        print()

        # Parse all 8
        for i, off in enumerate(offs):
            print("=" * 80)
            print(f"ENTITY #{i}   absolute_offset=0x{off:X}")
            print("=" * 80)
            try:
                e = parse_create_block(body, off)
            except Exception as ex:
                print(f"  PARSE ERROR: {ex}")
                print(f"  raw @+0..64: {hex_row(body[off:off+64])}")
                continue

            print(f"  updateType      : {e.update_type} (byte[+0])")
            print(f"  GUID            : {guid_to_str(*e.guid_raw)}")
            gs, ge = e.guid_range
            print(f"    PackedGuid    @ +{gs-off}..+{ge-off}  ({ge-gs} bytes)")
            print(f"    mask/data     : {hex_row(body[gs:ge], 20)}")
            print(f"  objectType      : {e.object_type} @ +{ge-off}")
            mov_off = ge + 1
            print(f"  movement block  @ +{mov_off-off}..+{mov_off-off+len(e.movement_bytes)}  ({len(e.movement_bytes)} bytes)")
            print(f"    raw           : {hex_row(e.movement_bytes, 24)}")
            flags_on = [k for k, v in e.movement_flags.items() if v]
            print(f"    flags set     : {flags_on if flags_on else '(none -- movement all false)'}")
            vsz_off = mov_off + len(e.movement_bytes)
            print(f"  valuesSize      : {e.values_size} (0x{e.values_size:X}) @ +{vsz_off-off}")
            ff_off = vsz_off + 4
            print(f"  fieldFlags      : 0x{e.field_flags:02X} @ +{ff_off-off}")
            fs, fe_ = e.fragment_ids_range
            frag_id_hex = " ".join(f"{x:02X}" for x in body[fs:fe_])
            print(f"  fragment IDs    @ +{fs-off}..+{fe_-off}  raw=[{frag_id_hex}]")
            print("    resolved fragment list:")
            for fid in e.fragment_ids:
                name = FRAGMENT_NAMES.get(fid, f"Unknown({fid})")
                mark = "  <-- indirect" if fid in INDIRECT else ""
                tag = "  (tag, no payload)" if 200 <= fid < 255 else ""
                print(f"       {fid:3d} (0x{fid:02X}) = {name}{mark}{tag}")
            print(f"    End (0xFF) terminator consumed")
            print(f"  end of block    : +{e.end-off}  (absolute 0x{e.end:X})")

            # Per-fragment data
            print()
            print("  Fragment data (write-create payload bytes):")
            for fid, name, blob in e.fragment_data:
                if 200 <= fid < 255:
                    print(f"    [TAG] {name:<32s} -- no payload")
                    continue
                if not blob:
                    print(f"    [??]  {name:<32s} -- (empty after unknown parser ran out)")
                    continue
                print(f"    [{fid:3d}] {name:<32s} ({len(blob)} bytes): {fmt_bytes(blob, 56)}")

            # Parsed structs
            for mpd in e.parsed.get("MirroredPositionData", []):
                print()
                print(f"  FMirroredPositionData_C parsed:")
                print(f"    AttachParentGUID : {mpd['AttachParentGUID']}")
                print(f"    Position         : {mpd['Position']}")
                print(f"    Rotation (XYZW)  : {mpd['Rotation']}")
                print(f"    Scale            : {mpd['Scale']}")
                print(f"    AttachmentFlags  : {mpd['AttachmentFlags']}")
                print(f"    raw ({len(mpd['bytes'])} bytes): {fmt_bytes(mpd['bytes'], 64)}")
            print()

        break


if __name__ == "__main__":
    main()
