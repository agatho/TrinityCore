# Loss of Control system — RE dossier (WoW 12.0.7.68275)

Status: **ADD (school-interrupt case) BUILT** on `feature/loss-of-control-interrupt`.
AURA_UPDATE (aura-based CC) still needs the per-mechanic LocType map.

## RESOLVED empirically (no guessing): SMSG_ADD_LOSS_OF_CONTROL == school interrupt
Every ADD packet in the capture (distinct SpellIDs 1766 Kick, 19647 Spell Lock, 47528 Mind
Freeze, 57994 Wind Shear, 96231 Rebuke, 147362 Counter Shot — ALL interrupt abilities) had:
`LocType = 11`, `DisplayType = 0`, `DurationA == DurationB` (lockout ms), and `Field3` = a
**school mask** (2=Holy, 4=Fire, 8=Nature, 0x44=Fire+Frost, 0x7C=all-magic) = the interrupted
spell's school. So `SMSG_ADD_LOSS_OF_CONTROL` is the **spell-lockout / school-interrupt**
notification (aura CC uses AURA_UPDATE). LocType 11 = the observed retail value for interrupts —
sent as-is, no enum guessing. Built: emit from `Spell::EffectInterruptCast` right after
`SpellHistory::LockSpellSchool`, unicast to the interrupted player (mirrors the LoC-UI SpellCooldown):
`SpellID=m_spellInfo->Id (the interrupt)`, `Caster=m_caster`, `LockoutSchoolMask=curSpellInfo->GetSchoolMask()`,
`Duration=DurationLeft=duration`, `Type=11`, `DisplayType=0`. See `SendAddLossOfControl`.

---

(original status: wire fully RE'd, ONE blocker (LocType enum) before a no-wrong-data build)
The two highest-frequency *unimplemented* SMSG in the rated-BG capture
(`SMSG_LOSS_OF_CONTROL_AURA_UPDATE` x932, `SMSG_ADD_LOSS_OF_CONTROL` x42... note the
AURA_UPDATE opcode 0x420119 appears 466× in the trimmed sample too). TrinityCore has
**no** LoC infrastructure — only `SPELL_COOLDOWN_FLAG_LOSS_OF_CONTROL_UI` (0x4) for
interrupt cooldowns. Client system = `C_LossOfControl` / `Blizzard_FrameXML/LossOfControlFrame.lua`.

## Opcodes
- `SMSG_ADD_LOSS_OF_CONTROL`         = 0x42011A  (reader `sub_7FF729098D70`)
- `SMSG_LOSS_OF_CONTROL_AURA_UPDATE` = 0x420119  (reader `sub_7FF729098D00` → element `sub_7FF729098AB0`, type `JamLossOfControlInfo`)

## SMSG_ADD_LOSS_OF_CONTROL wire (decompiled, sniff-verified)
```
guid   Target        // PackedGuid  (reader field a1+32)
uint32 SpellID       // a1+56   (e.g. 96231)
guid   Caster        // PackedGuid  a1+64
uint32 DurationA     // a1+80   (ms, e.g. 3000)
uint32 DurationB     // a1+84   (ms, e.g. 3000 — likely lockout vs. remaining)
uint32 Field3        // a1+88   (e.g. 8 — lockoutSchool mask OR mechanic)
uint8  LocType       // a1+48   (e.g. 11 — index into the LossOfControlType enum)  <-- BLOCKER
uint8  DisplayType   // a1+52   (e.g. 0 — DISPLAY_TYPE_{NONE,ALERT,FULL})
```
The reader's `ai_Read_CompressedUInt32FromPacket` calls read **full 4-byte uint32** here
(sniff confirms fixed width; the "compressed" name is an AI-rename artifact). The offline
layout extractor mislabelled this as `{guid,u32,guid,u32,u32,u32,u8,u8}` — that part is
actually correct; only field *semantics* were unknown.

## SMSG_LOSS_OF_CONTROL_AURA_UPDATE wire
```
guid   Unit
uint32 Count
Count × JamLossOfControlInfo   // 9 bytes each in the capture
```
Element reader `sub_7FF729098AB0` decompiled — each `JamLossOfControlInfo` (9 bytes on the wire):
```
uint32 AuraInstanceID   // elem+0  (rolling counter, ~5000-6000 in the capture)
uint16 B                // elem+4  (115 / 10 — unresolved: startTime-ish? not lockout school)
uint8  C                // elem+6  (0 / 2)
uint8  D                // elem+8  (13 / 7 / 5)   <- LocType candidate
uint8  E                // elem+12 (5 / 18)       <- LocType/displayType candidate
```
Samples: `{6000,115,0,13,5}`, `{6000,115,2,7,5}`, `{5000,10,0,5,18}`.

## Correlation pass (2026-07-08) — results + the offline ceiling

Ran the correlation over the full capture (509 elements / 466 packets). Corrected the field model:
`A` = **timeRemaining in ms** (top values 3000/6000/4000/2000, decaying snapshots) — NOT an instance id.
`D` and `E` are the two **enum** fields; `C ∈ {0,1,2,3}`; `B` (u16, 8..183) unresolved (slot? startTime?).
Each `(C,D,E)` tuple carries a tight duration signature (CC categories):

| C | D | E | count | duration(s) |
|---|---|---|-------|-------------|
| 0 | 7 | 7 | 168 | 0.3–2.1 |
| 0 | 12| 12| 82  | 0.5–2.7 |
| 0 | 2 | 2 | 54  | 1.1–2.5 |
| 0 | 13| 5 | 47  | 3.6–5.2 |
| 2 | 7 | 5 | 27  | 4.2–5.3 |
| 0 | 5 | 13| 20  | 4.2–5.8 |
| 1 | 9 | 17| 15  | 0.5–3.0 |
| 3 | 14| 17| 15  | 0.5–3.0 |
| … (12 more tuples) | | | | |

`D==E` in the common `C=0` rows; they diverge otherwise (many-to-many). Duration tracks `D` more than `E`.

**Two offline walls block a no-guess mechanic map (either one, resolved, cracks it):**
1. The element carries no SpellID/instanceID/slot to join on. The only in-sniff join is `SMSG_AURA_UPDATE`
   (0x620011, 106,905 records) — a **pure bit-stream** (client reader `sub_7FF7290BE6E0` is a bit
   accumulator; TC's `append<T>` does NOT auto-flush bits, so the bit/byte interleave is too subtle to
   reimplement offline without risking garbage SpellIDs → a wrong join = wrong data).
2. The `LocType` enum is reached via a **computed jump table** in the getter — a `.text` scan found ZERO
   direct LEAs to the loc-type strings, so `dump_cfunc` can't recover the ordered enum.

## ENUM EXTRACTION SOLVED (2026-07-08, Ghidra + capstone deep dive)

Root cause of the "no references" wall: **`wow_dump.bin` is a runtime memory dump; its real image base is
`0x7FF7B3140000`** (PE `ImageBase` + `wow_offsets_68275.json image_base`), NOT the stale `0x7FF728AA0000`
in CLAUDE.md. Rip-relative LEAs resolve base-independently (so the getter decoded fine), but **absolute
pointer tables carry the ASLR base** — every earlier pointer search used the wrong base and found nothing.

The display-string builder (`sub @0x7FF72ADE2190`, called by the getter) does:
`movsxd rcx,[evt+0x28]; cmp; mov r8,[imgbase + rcx*8 + 0x3957DA0]` and a default path
`[imgbase + [evt]*8 + 0x41FDC30]`. Those are two pointer tables:

### LossOfControlType enum — table RVA `0x41FDC30` (ADD `Type` field, and the wire "locType")
```
0 NONE  1 POSSESS  2 CONFUSE  3 CHARM  4 FEAR  5 STUN  6 PACIFY  7 ROOT
8 SILENCE  9 PACIFYSILENCE  10 DISARM  11 SCHOOL_INTERRUPT  12 STUN_MECHANIC  13 FEAR_MECHANIC
```
**`SCHOOL_INTERRUPT = 11` confirms the ADD capture + shipped build.**

### SpellMechanic enum — table RVA `0x3957DA0` (== TrinityCore `Mechanics` enum, verbatim)
`1 Charm 2 Disorient 3 Disarm 4 Distract 5 Fear 6 Grip 7 Root 8 (dep) 9 Silence 10 Sleep 11 Snare
12 Stun 13 Freeze 14 Incapacitate 15 Bleed ... 17 Polymorph 18 Banish 19 Shield 20 Shackle 21 Mount
23 Turn 24 Horror 25 Invuln 26 Interrupt 27 Daze 28 Discovery 29 ImmuneShield 30 Sap 31 Enrage 32 Wound 36 Taunt`

### AURA_UPDATE element fields RESOLVED
`{u32 A=timeRemaining_ms, u16 B, u8 C∈{0..3}, u8 D=SpellMechanic, u8 E=SpellMechanic}`.
D/E are **SpellMechanic ids** (proved: D=14=Incapacitate is a valid mechanic but invalid LossOfControlType,
and durations fit — D=12 Stun short, D=5 Fear long). In C=0 rows D==E; divergent rows look like
D=DR/category mechanic, E=spell mechanic (e.g. D=14 Incapacitate / E=17 Polymorph). TC has Mechanics
natively per spell (`SpellInfo::Mechanic` / effect mechanic), so AURA_UPDATE is now buildable no-guess.
Remaining detail before shipping AURA_UPDATE: the exact D-vs-E role + `B`/`C` — decompile the AURA_UPDATE
*handler* (wire→internal event) to confirm; do not guess the two-mechanic split.

## Client-side struct (field-name oracle, `LossOfControlDocumentation.lua`)
`LossOfControlData = { locType(cstring), spellID, displayText, iconTexture, startTime,
timeRemaining, duration, lockoutSchool, priority, displayType, auraInstanceID }`.
`locType`/`displayText`/`iconTexture` are **derived client-side** from spellID+LocType+school;
the wire carries the numeric `LocType`, `lockoutSchool`(=Field3?), `displayType`, durations.
Events: `LOSS_OF_CONTROL_ADDED(unitTarget,effectIndex)`, `LOSS_OF_CONTROL_UPDATE(unitTarget)`,
`PLAYER_CONTROL_LOST/GAINED`, plus commentator variants.
Client uses `displayType == DISPLAY_TYPE_ALERT/FULL/NONE` and `priority` to pick what to show;
`locType == "SCHOOL_INTERRUPT"` special-cases the lockout-school text.

## THE BLOCKER: LocType enum
`locType` is a C-side string ("SCHOOL_INTERRUPT","STUN","STUN_MECHANIC","ROOT","SILENCE",
"PACIFY","PACIFYSILENCE","FEAR","FEAR_MECHANIC","CHARM","POSSESS","CONFUSE","DISARM",...) selected
by a **switch/jump-table on the uint8 LocType**. Strings live in `.rdata` ~0x3bcce88–0x3bcd008
but NOT as a flat pointer table, and string-xref extraction misses the jump-table access, so
`dump_cfunc` did not recover the ordered enum. Sample LocType values in the capture: **11** (ADD),
and the AURA_UPDATE elements carry their own.

### Two clean ways to finish (no guessing):
1. **GUI IDA** on `GetActiveLossOfControlData` @ RVA `0x1097820` (VA 0x7FF729B37820) — follow the
   uint8→string jump table to read the enum order directly. (Offline dump_cfunc truncated it.)
2. **Empirical / sniff-grounded**: the ADD capture pairs `(SpellID → LocType)`. Map each captured
   SpellID to its mechanic/aura via client DB2 (SpellMisc/Spell + SpellMechanic) and invert to get
   `LocType → {mechanic set}` with zero guessing. Then build TC's `Unit::GetLossOfControlType(SpellInfo)`
   from Mechanic + AuraType (STUN/FEAR/ROOT/SILENCE/PACIFY/CONFUSE/CHARM/POSSESS/DISARM/interrupt-school).

## Build plan once LocType is known (per-phase, off 560165c0a6)
- P0: packet classes (ADD + AURA_UPDATE, `JamLossOfControlInfo`), opcode STATUS_NEVER, `Unit`
  helpers `SendAddLossOfControl` / `SendLossOfControlAuraUpdate`.
- P1: `Unit::GetLossOfControlType(SpellInfo)` mechanic→LocType map (the enum from above).
- P2: hook CC aura apply → ADD; on the same aura's slot changes / removal → AURA_UPDATE list rebuild
  (send the target's full active-LoC list, mirroring how the client keys by unit).
- Field3/DurationA-vs-B semantics: DurationA=full duration, DurationB=remaining (both = full at apply);
  Field3 = lockoutSchool mask for SCHOOL_INTERRUPT/LOCKOUT else 0. Verify vs a fresh sniff before shipping
  the interrupt case.

Delivered this session on the same RBG-mining pass: **SMSG_UNIT_DIMINISHING_RETURN_START**
(feature/diminishing-return-start, built+pushed).
