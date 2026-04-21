# Opcode 0x005A005D Analysis

**TL;DR: It's not a CMSG, and it's not housing. It's `SMSG_FLIGHT_SPLINE_SYNC` — already implemented in our server. No action required.**

## Identification

| Field | Value |
|---|---|
| Opcode | `0x005A005D` |
| Group | `0x005A` (retail NeighborhoodSystem CMSG group per CLAUDE.md table — but see below) |
| Sub | `0x005D` (93) |
| Direction (sniff) | **SMSG** (server → client) — **not CMSG** |
| TC name | `SMSG_FLIGHT_SPLINE_SYNC` |
| Already in server | Yes — `Opcodes.h:1505`, `Unit::SendFlightSplineSyncUpdate()` at `Unit.cpp:638` |

The assumption that `0x005A` is CMSG_NeighborhoodSystem is incorrect for this build. In our
`Opcodes.h` the `0x5A` range is entirely the Movement SMSG group (99 opcodes from
`SMSG_TIME_SYNC_REQUEST` at `0x5A0000` through `SMSG_MOVE_SET_CAN_DRIVE` at `0x5A0078`). The
CLAUDE.md housing architecture table lists `0x5A` as CMSG NeighborhoodSystem, which is a
different mapping that doesn't apply here — the retail sniff direction is unambiguously SMSG.

The "CMSG_NeighborhoodSystem[005D]" label in `33_opcode_sequence.py` comes from the
`classify()` fallback, which assumes any opcode with group `0x005A` is
CMSG_NeighborhoodSystem. That classifier is wrong for this range.

## Sniff Evidence

Source: `c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt`

- **136 occurrences — all SMSG** (zero CMSG).
- Payload size: **24 or 25 bytes total** (= 4-byte opcode + 20 or 21-byte body).
- Body layout: `PackedGUID128 (17 bytes incl. 2-byte mask) + float32 syncFraction`.
- 8 unique GUIDs, each sending ~26 syncs across the session.
- Sample decoded floats: 0.28 → 0.26 → 0.24 → 0.23 → 0.21 (monotonically decreasing per GUID, i.e. a time interpolation value).

First 10 payloads (post-opcode hex, all 21 bytes: 17 PackedGUID + 4 float LE):

```
efffafa35f01aaa7028081f1e055ad3c20 ab81913e  float=0.2842
efffafa3df01aaa7028081f1e055ad3c20 1881913e  float=0.2842
efffb0a3df09aaa7028080f1e055ad3c20 3fe9863e  float=0.2635
efffb1a35f03aaa7028080f1e055ad3c20 45ff7e3e  float=0.2490
efffb1a35f08aaa7028080f1e055ad3c20 4534733e  float=0.2375
```

## Client-Side Behavior

`SMSG_FLIGHT_SPLINE_SYNC` is the standard TrinityCore/retail cyclic-spline sync. Sent by
the server at intervals during a unit's cyclic movement spline so the client can
re-synchronize its interpolation fraction. Triggered server-side by
`Unit::SendFlightSplineSyncUpdate()` (our `Unit.cpp:638`) — called from `UpdateSplineMovement`
when the cyclic-spline sync timer elapses. **Fire-and-forget** — no client response expected,
no corresponding CMSG.

The 8 GUIDs in the sniff are the visible flying/cyclic-spline units within the retail
neighborhood zone (ambient wildlife, flight-path NPCs, or decorative patrol mobs).

## Why Our Server Never Sees a CMSG 0x5D

Because there is no such CMSG. The "gap" the audit perceived is an artifact of the
classifier mis-labelling an SMSG as a CMSG.

## Implementation Priority

**None (already implemented).** The opcode is fully supported:

- `Opcodes.h:1505` — `SMSG_FLIGHT_SPLINE_SYNC = 0x5A005D`
- `Opcodes.cpp:1584` — registered `STATUS_NEVER, CONNECTION_TYPE_INSTANCE`
- `MovementPackets.h:176` — `FlightSplineSync` ServerPacket class
- `MovementPackets.cpp:659` — `Write()` impl
- `Unit.cpp:638` — `Unit::SendFlightSplineSyncUpdate()` sender, invoked from movement update

The reason our server won't emit 136 of these in the same session as the retail sniff is
environmental (fewer cyclic-spline units visible on our neighborhood map), not a handler
gap. If matching retail fidelity matters, audit which cyclic-spline creatures populate the
retail neighborhood and spawn equivalents — but this is a content/spawn question, not a
packet handler question.

## Audit Recommendation

Fix `sniff_analysis_login_plot/33_opcode_sequence.py` `classify()`:
the `0x005A` group should map to `SMSG_MovementSystem`, not `CMSG_NeighborhoodSystem`.
CMSG group numbering for Neighborhood in this build is likely different — verify against
an actual CMSG (SMSG group +something). Our server uses `0x39` for both SMSG and CMSG
NeighborhoodSystem, but retail may split them.

## References

- `c:/TrinityBots/wt/housing-system/src/server/game/Server/Protocol/Opcodes.h:1505`
- `c:/TrinityBots/wt/housing-system/src/server/game/Entities/Unit/Unit.cpp:638`
- `c:/TrinityBots/wt/housing-system/src/server/game/Server/Packets/MovementPackets.cpp:659`
- Sniff: `c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt`
- Extraction script: `c:/TrinityBots/wt/housing-system/docs/audit_2026_04_21/extract_5d.py`
