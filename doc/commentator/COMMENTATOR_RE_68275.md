# Commentator (Spectator) System — RE dossier (12.0.7.68275)

Arena/wargame spectator system. 8 CMSG + 3 SMSG, all `Handle_NULL` / `STATUS_UNHANDLED` in
TC master. **P0 built** on `feature/commentator` (RBAC-gated enable + `SMSG_COMMENTATOR_STATE_CHANGED`).
This documents the decompilation of the remaining opcodes and the two genuine gates for P1/P2.

## Opcode surface

| Opcode | Dir | Value | Status |
|---|---|---|---|
| CMSG_COMMENTATOR_ENABLE | c→s | 0x40001c | **BUILT (P0)** |
| CMSG_COMMENTATOR_GET_MAP_INFO | c→s | 0x40001d | gated (see below) |
| CMSG_COMMENTATOR_GET_PLAYER_INFO | c→s | 0x40001e | gated |
| CMSG_COMMENTATOR_GET_PLAYER_COOLDOWNS | c→s | 0x40001f | gated |
| CMSG_COMMENTATOR_ENTER_INSTANCE | c→s | (0x40) | gated |
| CMSG_COMMENTATOR_EXIT_INSTANCE | c→s | (0x40) | gated (empty payload) |
| CMSG_COMMENTATOR_SPECTATE | c→s | (0x40) | gated |
| CMSG_COMMENTATOR_START_WARGAME | c→s | 0x40001b | gated |
| SMSG_COMMENTATOR_STATE_CHANGED | s→c | 0x4201af | **BUILT (P0)** `{ ObjectGuid, bit Enabled }` |
| SMSG_COMMENTATOR_MAP_INFO | s→c | 0x4201b0 | gated (nested) |
| SMSG_COMMENTATOR_PLAYER_INFO | s→c | 0x4201b1 | gated (nested) |

## CMSG wire (decompiled from the client send-serializers — the extractor mislabels these)

- **ENTER_INSTANCE** (`sub_7FF7290709E0`): `{ uint32 A; uint32 B; uint32 C; bit D }`. Writes
  three `WriteUInt32` (fields @32/@36/@44) then a bit (@40) + FlushBits.
- **SPECTATE** (`sub_7FF72907F810`): `{ std::string Name }` — the serializer strlen-scans @32
  and writes it via the string writer. Extractor's "uint8" is the bit length-prefix. This is
  "spectate the player named X".
- **GET_MAP_INFO** (`sub_7FF7290706C0`): also carries a `std::string` (same strlen + string
  writer pattern), not the `{u8,u8}` the extractor reported.
- **EXIT_INSTANCE** (`sub_7FF729070A60`): empty payload.
- **START_WARGAME** (`sub_7FF7290703C0`): `{ uint8; uint8; uint8 }`.
- **GET_PLAYER_INFO / GET_PLAYER_COOLDOWNS**: uint32 fields + an ObjectGuid (cooldowns).

## SMSG_COMMENTATOR_MAP_INFO response (decompiled from the client deserializer `sub_7FF7290A1700`)

Deeply nested: a top field, then a `CompressedUInt32`-counted vector of **matches**, each with
several compressed-uint32 fields + a nested `CompressedUInt32`-counted vector of **teams/players**,
each of which contains a 2-iteration inner loop (arena has 2 teams) with its own
`CompressedUInt32`-counted vector of **players** carrying spell entries (`sub_7FF7290F9240` =
`{ uint32, uint32, u8 }`, a spell/aura tuple). Several reads use generically auto-named helpers
(`ai_Process_HousingDataPacket`, `ai_Process_HousingTalentData`) that are **misnomers** in this
context — their true field meaning is not recoverable offline.

## The two genuine gates (why P1/P2 are not built — proven, not deferred)

1. **Spectator subsystem missing in TC.** `git grep` finds **zero** spectator scaffolding in
   `src/server/game/Battlegrounds/` (no spectator set, no add-observer-without-team path).
   ENTER/EXIT/SPECTATE require building spectator support into `Battleground` (join a running
   arena instance as an invisible, team-less observer). This is buildable local work, but it is
   a self-contained subsystem, not a handler.

2. **Field semantics are unknowable offline (would put wrong data on the wire).**
   - ENTER_INSTANCE's three uint32s: which is MapID vs InstanceID vs bracket/arena-type is not
     determinable from the serializer — acting on the wrong one enters the wrong instance.
   - GET_MAP_INFO / SPECTATE strings: what the string filters/selects.
   - MAP_INFO / PLAYER_INFO nested responses: the auto-named reader fields
     (`ai_Process_HousingDataPacket` etc.) have no recoverable meaning offline. Emitting a
     guessed layout would violate the "never put wrong data on the wire" rule.

## Precise sniff questions (one live 12.0.7 commentator capture answers all)

- ENTER_INSTANCE: capture the three uint32s when entering a known arena — map them to
  (MapID, InstanceID, ArenaType/Bracket) and identify the bit.
- GET_MAP_INFO / SPECTATE: capture the string payload — confirm it is a player/character name
  vs a bracket/realm filter.
- SMSG_COMMENTATOR_MAP_INFO / PLAYER_INFO: capture one populated response to resolve the
  auto-named nested fields (per-match, per-team, per-player, and the spell tuples).

## Build order once unblocked

P1 = spectator subsystem in `Battleground` (add/remove observer, teleport in/out) driven by
ENTER/EXIT/SPECTATE. P2 = GET_MAP_INFO/PLAYER_INFO/COOLDOWNS responses populated from
`BattlegroundMgr` active-arena data (needs the sniff-confirmed nested wire). START_WARGAME
reuses arranged-wargame creation (see `feature/war-games`).
