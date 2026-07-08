# SMSG_LOSS_OF_CONTROL_AURA_UPDATE — aura crowd-control (WoW 12.0.7.68275)

Branch: `feature/loss-of-control-interrupt` (base `560165c0a6`)

The aura-driven counterpart to `SMSG_ADD_LOSS_OF_CONTROL`. Where ADD notifies a spell-school **interrupt**
(lockout), this opcode (`0x420119`) drives the client loss-of-control UI (`C_LossOfControl` /
`Blizzard_FrameXML/LossOfControlFrame.lua`) for **aura crowd control** — stun / root / fear / silence /
disarm / polymorph / etc. It was `STATUS_UNHANDLED` with no packet.

## What this opcode is (recovered no-guess, offline handler decompile + live capture)

The client stores loss-of-control events in a global hashmap keyed by unit GUID. The message reader
(`sub_7FF729098AB0`) parses a `JamLossOfControlInfo` list; the event builder (`sub_7FF72ADE2540`) maps
each element into the internal event and **derives** the display category (LossOfControlType / priority /
displayType) from the referenced aura — so the wire carries only the aura reference + mechanics + timing.

### Wire (client reader `sub_7FF729098AB0`, confirmed vs. capture)

```
PackedGuid Unit                 // the unit under crowd control
uint32     Count
Count × {
    uint32 TimeRemaining        // remaining CC duration, ms          (event builder v8+0)
    uint16 AuraSlot             // client aura slot == AuraApplication::GetSlot()   (v8+4)
    uint8  EffectIndex          // aura effect index applying the CC   (v8+6; event key = (AuraSlot,EffectIndex))
    uint8  Mechanic             // effect-level SpellMechanic          (v8+8)
    uint8  Mechanic2            // spell-level  SpellMechanic          (v8+12)
}
```

### Field derivations (all TC-native — no guessed values)

| Field | Meaning | TC source |
|-------|---------|-----------|
| `Unit` | unit under CC | `GetGUID()` |
| `TimeRemaining` | remaining CC ms | `Aura::GetDuration()` |
| `AuraSlot` | client aura slot | `AuraApplication::GetSlot()` (same slot space as `SMSG_AURA_UPDATE`) |
| `EffectIndex` | CC effect index | `SpellEffectInfo::EffectIndex` of the control effect |
| `Mechanic` | effect mechanic | `SpellEffectInfo::Mechanic` (`Mechanics` enum) |
| `Mechanic2` | spell mechanic | `SpellInfo::Mechanic` (falls back to the effect mechanic when unset) |

Evidence for the `EffectIndex` / mechanic split:
- The event is keyed `(AuraSlot, EffectIndex)` in the builder (`event+48 == slot && event+52 == C`), so
  one aura can raise multiple entries — matching the 43 captured cases of one slot with several `C` values
  (multi-effect CC auras). `C` ranges 0..3 == max spell effects. (Ruled out `C = displayType`: that client
  table @RVA 0x41FDCF0 only has values {0,1,2}, but `C` reaches 3.)
- `Mechanic` (D) is checked `<= 0xF` on the builder fast path → the effect-level mechanic (stun 12, fear 5,
  root 7, silence 9, knockout 14 …, all ≤ 15). `Mechanic2` (E) carries the spell-level mechanic which can
  exceed 15 (e.g. polymorph 17). The captured pair `(14, 17)` = a Polymorph spell whose incapacitate
  effect has mechanic `KNOCKOUT(14)` while the spell mechanic is `POLYMORPH(17)` — exactly D=effect,
  E=spell. The client's `SpellMechanic` table (@RVA 0x3957DA0) == TC's `Mechanics` enum verbatim, so the
  values are sent as-is (no remapping, mirroring the `SMSG_UNIT_DIMINISHING_RETURN_START` approach).

## Server implementation

`Unit::SendLossOfControlAuraUpdate()` rebuilds the full list from the unit's visible auras, emitting one
entry per applied effect whose mechanic is in `MECHANIC_LOSS_CONTROL_MASK` (TrinityCore's authoritative
control-mechanic set). Unicast to the affected player (`ToPlayer()`), matching the guid-keyed client store.

Emit hook: `Unit::SetVisibleAura` / `Unit::RemoveVisibleAura` — the single funnel where auras gain/lose a
client slot. The refresh fires only when the aura's mechanic mask intersects `MECHANIC_LOSS_CONTROL_MASK`,
so non-control auras cost one mask test and nothing else. On removal the aura is erased from the visible
set *before* the rebuild, so it is correctly excluded.

## Files

- `src/server/game/Server/Protocol/Opcodes.cpp` — `SMSG_LOSS_OF_CONTROL_AURA_UPDATE` → `STATUS_NEVER`
- `src/server/game/Server/Packets/SpellPackets.{h,cpp}` — `LossOfControlAuraUpdate` packet + `Write()`
- `src/server/game/Entities/Unit/Unit.{h,cpp}` — `SendLossOfControlAuraUpdate()` + visible-aura hooks

## Residual

`MECHANIC_LOSS_CONTROL_MASK` omits a few mechanics retail also flags as CC (e.g. `DISTRACT`); this is
TrinityCore's own consistent control set (server truth), not wrong data. A live 12.0.7 sniff correlating
`EffectIndex` against a known multi-effect CC spell's effect layout would be the final confirmation of the
`C = EffIndex` reading. Full RE trace: `doc/loss-of-control/LOSS_OF_CONTROL_RE_68275.md`.
