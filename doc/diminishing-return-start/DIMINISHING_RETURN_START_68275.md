# SMSG_UNIT_DIMINISHING_RETURN_START — WoW 12.0.7.68275

Branch: `feature/diminishing-return-start` (base `560165c0a6`)

Drives the retail "Spell Diminishing Returns" tray (`Blizzard_SpellDiminishUI`,
`C_SpellDiminish`, event `UNIT_SPELL_DIMINISH_CATEGORY_STATE_UPDATED`): when a
crowd-control aura of a diminishing-returns category lands on a unit, the client
starts/refreshes a per-category countdown icon showing how long the DR window lasts
and whether the target is now immune.

`SMSG_UNIT_DIMINISHING_RETURN_START` (0x420379) was `STATUS_UNHANDLED` with no packet.

## Wire (recovered from a live rated-BG capture, 598 records)

```
ObjectGuid Unit           // PackedGuid
uint8      Category        // = DiminishingGroup of the CC aura
Bits<1>    ShowCountdown   // wire bit7
Bits<1>    IsImmune        // wire bit6
FlushBits
```

The offline layout extractor mislabelled the two trailing bits as a second `uint8`.
The client reader (`sub_7FF7290BD4B0`, decompiled in IDA) reads the guid, one byte
(`Category`), then a byte whose bit7→a struct bool and bit6→another struct bool.

**Category maps 1:1 to TrinityCore's `DiminishingGroup`**: the captured category bytes
were exactly `{1,3,4,5,6,8}` = ROOT / INCAPACITATE / DISORIENT / SILENCE /
AOE_KNOCKBACK / LIMITONLY (STUN=2 and TAUNT=7 simply weren't triggered in the sample),
so TC sends its own group value with no remapping. (The client's `SpellDiminishCategory`
enum is a separate UI-layer numbering; the wire uses the server DR-group numbering.)

Field names come from the UI `SpellDiminishTrackerInfo` struct
(`category/startTime/duration/showCountdown/isImmune`); `startTime`/`duration` are
derived client-side, so the wire only carries `category` + the two bools. The client uses
`isImmune` for the immunity indicator and `showCountdown` as the 4th arg of
`CooldownFrame_Set` (whether to draw the numeric countdown on the swirl).

## Trigger

Emitted from `Spell::DoSpellHit` at the existing DR site (where `ApplyDiminishingToDuration`
decides immunity), for every negative aura carrying a DR group:
- `Category` = `hitInfo.DRGroup`
- `IsImmune` = the CC was diminished to 0 duration (`!ApplyDiminishingToDuration`)
- `ShowCountdown` = `diminishLevel > DIMINISHING_LEVEL_1` (draw the number once the
  category is actively diminishing)

`ShowCountdown` note: in the capture this bit is ~50/50 and independent of both category
and immunity, i.e. not derivable from server DR state alone (likely a client-influenced
display toggle). The `> LEVEL_1` heuristic is a sensible, honest server default; the
important fields (unit, category, immunity) are exact.

Build-verified (`--target game`, game.dll, 0 errors).
