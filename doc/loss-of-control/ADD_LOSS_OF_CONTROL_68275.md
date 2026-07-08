# SMSG_ADD_LOSS_OF_CONTROL — school interrupt (WoW 12.0.7.68275)

Branch: `feature/loss-of-control-interrupt` (base `560165c0a6`)

Drives the client loss-of-control UI (`C_LossOfControl` / `Blizzard_FrameXML/LossOfControlFrame.lua`)
for **spell-school interrupts**. The opcode was `STATUS_UNHANDLED` with no packet; TrinityCore had
no loss-of-control infrastructure (only `SPELL_COOLDOWN_FLAG_LOSS_OF_CONTROL_UI`).

## What this opcode is (recovered no-guess from a live rated-BG capture)

Every `SMSG_ADD_LOSS_OF_CONTROL` in the capture carried `LossOfControlType = 11`, `DisplayType = 0`,
equal total/remaining durations, and an **interrupt ability** as the spell:

| SpellID | Ability        | Field3 (lockout school masks seen) |
|---------|----------------|-------------------------------------|
| 1766    | Kick           | 2 (Holy), 4 (Fire), 8 (Nature), 0x44 (Fire+Frost), 0x7C (all-magic) |
| 19647   | Spell Lock     | " |
| 47528   | Mind Freeze    | " |
| 57994   | Wind Shear     | " |
| 96231   | Rebuke         | " |
| 147362  | Counter Shot   | " |

So this opcode is specifically the **school-lockout / interrupt** notification. Aura-based crowd
control (stun/root/fear/silence/...) travels via `SMSG_LOSS_OF_CONTROL_AURA_UPDATE` (0x420119), not
implemented here. `Type = 11` is the value retail sends for interrupts — used as-is, no enum guessing.

## Wire (client reader `sub_7FF729098D70`)

```
PackedGuid Target            // interrupted unit
int32      SpellID           // the interrupt ability
PackedGuid Caster            // the interrupter
int32      Duration          // total lockout ms
int32      DurationLeft      // remaining lockout ms (== Duration on apply)
uint32     LockoutSchoolMask // SpellSchoolMask of the interrupted spell
uint8      Type              // LossOfControlType (11 = school interrupt)
uint8      DisplayType       // 0
```
The reader's `ai_Read_CompressedUInt32FromPacket` calls read full 4-byte uint32 (sniff-confirmed).

## Trigger

`Spell::EffectInterruptCast`, immediately after `SpellHistory::LockSpellSchool`, unicast to the
interrupted player (mirrors the existing LoC-UI `SpellCooldown` send): `SpellID = m_spellInfo->Id`
(the interrupt), `Caster = m_caster`, `LockoutSchoolMask = curSpellInfo->GetSchoolMask()` (interrupted
spell's school), `Duration = DurationLeft = duration`, `Type = 11`.

Build-verified (`--target game`, game.dll, 0 errors). Full system RE incl. the AURA_UPDATE
follow-up: `c:/dumps/LOSS_OF_CONTROL_RE_68275.md`.
