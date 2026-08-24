# SMSG_AURA_POINTS_DEPLETED — WoW 12.0.7.68275

Branch: `feature/aura-points-depleted` (base `560165c0a6`)

`SMSG_AURA_POINTS_DEPLETED` (0x620012) was `STATUS_UNHANDLED` with no packet class.
It is a lightweight server→client notification that one of an aura's "points"
(the `Points`/`EstimatedPoints` values carried per-effect in `SMSG_AURA_UPDATE`'s
`AuraDataInfo`) has been drained to zero, letting the client zero-out that value in
the aura's UI without a full aura update.

## Wire (recovered from a live 12.0.7 capture)

```
ObjectGuid Unit      // PackedGuid
uint16     Slot      // aura application slot on Unit (TC AuraApplication::GetSlot is uint16)
uint8      EffectIndex
```

The offline layout extractor mislabelled this as `{ObjectGuid, uint8}` (it stopped after
two reads). The real wire was recovered from **408 real `0x620012` records** in
`C:\sniff\m+ run12.0.7.pkt` (`python c:/dumps/sniff_tool.py "<file>" dump 0x620012`):
every payload is a PackedGuid followed by a constant-width 3-byte tail `SS SS EE`
(`Slot` as uint16 LE, then `EffectIndex`, `EE`=0 in every capture → effect 0, matching
absorb auras whose absorb is effect 0). Six distinct unit GUIDs appear; slot values are
per-unit and rise over the run (aura slots), and the **same slot depletes many times**
(e.g. one slot 31×), i.e. it tracks a *persisting* aura whose point pool refills and
drains repeatedly — a frequently-refreshed shield in an M+ run.

## Trigger

Emitted at the three absorb point-pool exhaustion sites in `Unit::CalcAbsorbResist`
(`SPELL_AURA_SCHOOL_ABSORB`, `SPELL_AURA_MANA_SHIELD`, `SPELL_AURA_SCHOOL_HEAL_ABSORB`):
when `AuraEffect::ChangeAmount` drives the shield amount to `<= 0`, we send
`SendAuraPointsDepleted(slot, effIndex)` for the aura's target before the aura is torn
down. The reported state (this unit's aura at this slot now has 0 points on this effect)
is truthful.

Note: retail keeps such shields alive and re-fills them (hence the repeated-slot
pattern), whereas TrinityCore removes a `SCHOOL_ABSORB` aura the instant its amount hits
0. The message *content* is identical in both models; only the emission cadence differs
(TC emits once per break rather than per refill/break cycle). A future refinement, if TC
grows persisting/refilling absorbs, would emit on the in-place `ChangeAmount(→0)` without
the removal.

Build-verified (`--target game`, game.dll, 0 errors).
