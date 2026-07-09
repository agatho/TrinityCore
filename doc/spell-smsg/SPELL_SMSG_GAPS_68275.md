# Spell/Aura server→client opcode gaps — WoW 12.0.7.68275

Branch: `feature/spell-smsg-gaps` (base `560165c0a6`)

The `Spell` system audit flagged six server→client opcodes as `STATUS_UNHANDLED`
in `Opcodes.cpp` with no packet class. This branch implements the one that has a
genuinely-real emission point offline, and documents why the rest are gated on a
live capture rather than shipping guessed emissions (which would put wrong data /
wrong timing on the wire).

## Implemented

### SMSG_GAME_OBJECT_PLAY_SPELL_VISUAL_KIT (0x62003D)
Wire (client reader, 68275): `ObjectGuid Object; int32 KitRecID; int32 KitType; uint32 Duration;`
— the GameObject variant of `SMSG_PLAY_SPELL_VISUAL_KIT`, carrying **no** trailing
`MountedVisual` bit (the Unit version does).

**Real gap closed:** the SmartAI action `SMART_ACTION_PLAY_SPELL_VISUAL_KIT` only
handled `Unit` targets (`Unit::SendPlaySpellVisualKit`). When a script pointed the
action at a `GameObject` target it silently did nothing, because the GO-specific
opcode was never implemented. Added:
- `WorldPackets::Spells::GameObjectPlaySpellVisualKit` (SpellPackets.h/.cpp)
- `GameObject::SendPlaySpellVisualKit(id, type, duration)` mirroring the Unit one
- opcode flipped `STATUS_UNHANDLED` → `STATUS_NEVER`
- the SmartAI action now dispatches to `GameObject` targets

DB-scripted GameObjects can now play spell visual kits, rendered client-side via
the GO opcode. Build-verified (`--target game`, game.dll, 0 errors).

## Gated on a live 12.0.7 capture (NOT shipped — would be a guessed emission)

| Opcode | Wire (offline) | Why gated |
|---|---|---|
| SMSG_LOSS_OF_CONTROL_AURA_UPDATE (0x420119) | nested `{ObjectGuid, u32, u32, u8, u8, u8}` list | TC has **no** loss-of-control subsystem (`AddLossOfControl`/LoC packets absent). No real trigger exists without first building that whole system. |
| SMSG_AURA_POINTS_DEPLETED (0x620012) | `ObjectGuid Unit; uint8 Slot` | Absorb depletion (`Unit::CalcAbsorbResist`, Unit.cpp:1945) already removes the aura → client gets the removal via aura update. The separate "points depleted" message is a distinct point-aura mechanic; which auras emit it and when needs a sniff. |
| SMSG_SPELL_CATEGORY_COOLDOWN (0x620006) | `u32, u32, u32, bit` (likely a count+array the extractor flattened) | TC already conveys category cooldowns through `SendSpellHistory`. A standalone emission would duplicate or conflict without a capture confirming the client's expectation. |
| SMSG_NOTIFY_DEST_LOC_SPELL_CAST (0x620036) | large struct: 2×ObjectGuid + many u32 (truncated) | Full field layout not resolved offline; emission trigger unknown. |
| SMSG_PLAYER_TUTORIAL_HIGHLIGHT_SPELL (0x5F0016) | `u32 SpellID; bytes[len]` | Only real driver is a scripted-tutorial system TC lacks; string payload needs reader decompile + sniff. |

These are the documented offline reflection ceiling: structure is recoverable, but
the *emission-trigger semantics* (when/why retail sends them, with what values)
require a live packet capture. Building emissions from a guess is exactly the
"wrong data on the wire" the project forbids.
