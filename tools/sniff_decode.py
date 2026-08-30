"""Decode skyriding-relevant packets with structured output.

Identifies known skyriding spells in SPELL_GO/SPELL_START and decodes impulse vectors.
"""
from __future__ import annotations
import struct, sys
sys.path.insert(0, r"L:\TrinityCore\skyriding\TrinityCore\tools")
from sniff_parse import PktReader, load_opcode_names

SKYRIDING_SPELLS = {
    372608: 'SurgeForward(372608)',
    372610: 'SkywardAscent(372610)',
    361584: 'WhirlingSurge(361584)',
    392752: 'LaunchBoost(392752)',
    374763: 'LiftOff(374763)',
    372773: 'DragonriderEnergy(372773)',
    406095: 'DynamicFlight(406095)',
    433547: 'VigorCache(433547)',
    404191: 'LiftOffVisual(404191)',
    423624: 'MountSpeed1(423624)',
    432503: 'MountSpeed2(432503)',
    404468: 'StaticFlight(404468)',
    443825: 'SwapDynamicFlightMode(443825)',
}

def parse_packed_guid(data, off):
    if off >= len(data):
        return None, off
    mask_lo = data[off]
    off += 1
    if off >= len(data):
        return None, off
    mask_hi = data[off]
    off += 1
    val = 0
    for i in range(8):
        if mask_lo & (1 << i):
            if off >= len(data):
                return None, off
            val |= data[off] << (i * 8)
            off += 1
    for i in range(8):
        if mask_hi & (1 << i):
            if off >= len(data):
                return None, off
            val |= data[off] << ((i + 8) * 8)
            off += 1
    return val, off

def find_spell_id(body):
    """Scan body for known skyriding spell IDs (4-byte LE int).
    Returns (spell_id, offset) or None."""
    for off in range(0, len(body) - 3):
        sid = struct.unpack_from('<I', body, off)[0]
        if sid in SKYRIDING_SPELLS:
            return sid, off
    return None

def main():
    pkt_path = sys.argv[1]
    names = load_opcode_names()
    reader = PktReader(pkt_path)
    print(f"# build={reader.build}")

    impulse_op = 0x005A0064  # SMSG_MOVE_ADD_IMPULSE
    set_can_adv_fly = 0x005A0066
    unset_can_adv_fly = 0x005A0067
    set_can_drive = 0x005A007A
    unset_can_drive = 0x005A007B
    apply_inertia = 0x005A0060
    remove_inertia = 0x005A0061
    cmsg_set_adv_fly = 0x003E0070

    spell_ids_by_time = []  # (time_ms, spell_id, opcode_name)

    for pkt in reader:
        op = pkt['opcode']
        body = pkt['body']
        name = names.get(op, f"0x{op:08X}")
        t = pkt['tick_ms']

        if op == impulse_op:
            # PackedGuid + uint32 counter + Vec3
            mover, off = parse_packed_guid(body, 0)
            if off + 16 > len(body):
                continue
            counter = struct.unpack_from('<I', body, off)[0]
            x, y, z = struct.unpack_from('<fff', body, off + 4)
            mag = (x*x + y*y + z*z) ** 0.5
            print(f"{t:>10} ms  IMPULSE  cnt={counter:>3}  vec=({x:+8.3f},{y:+8.3f},{z:+8.3f}) mag={mag:6.2f}  guid=0x{mover or 0:016X}")

        elif op in (set_can_adv_fly, unset_can_adv_fly, set_can_drive, unset_can_drive):
            mover, off = parse_packed_guid(body, 0)
            counter = struct.unpack_from('<I', body, off)[0] if off + 4 <= len(body) else 0
            print(f"{t:>10} ms  {name}  cnt={counter}  guid=0x{mover or 0:016X}  body={body.hex()}")

        elif op == apply_inertia:
            print(f"{t:>10} ms  APPLY_INERTIA  body={body.hex()}")
        elif op == remove_inertia:
            print(f"{t:>10} ms  REMOVE_INERTIA  body={body.hex()}")

        elif op == cmsg_set_adv_fly:
            print(f"{t:>10} ms  CMSG_MOVE_SET_ADV_FLY  len={len(body)}  body={body.hex()}")

        elif name in ('SMSG_SPELL_GO', 'SMSG_SPELL_START'):
            sid_info = find_spell_id(body)
            if sid_info:
                sid, off = sid_info
                kind = 'GO' if name == 'SMSG_SPELL_GO' else 'START'
                print(f"{t:>10} ms  SPELL_{kind}  spell={SKYRIDING_SPELLS[sid]}  off={off}")

        elif name == 'SMSG_POWER_UPDATE':
            mover, off = parse_packed_guid(body, 0)
            if off + 5 > len(body):
                continue
            count = body[off]
            off += 1
            for i in range(count):
                if off + 5 > len(body):
                    break
                power = struct.unpack_from('<i', body, off)[0]
                ptype = body[off + 4]
                off += 5
                if ptype == 25:  # POWER_ALTERNATE_MOUNT (vigor)
                    print(f"{t:>10} ms  POWER  vigor={power}")

        elif name in ('SMSG_MOUNT_RESULT', 'SMSG_DISMOUNT_RESULT'):
            print(f"{t:>10} ms  {name}  body={body.hex()}")

if __name__ == '__main__':
    main()
