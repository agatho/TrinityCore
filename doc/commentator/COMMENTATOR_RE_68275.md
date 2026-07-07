# Commentator (Spectator) System — RE dossier (12.0.7.68275)

Arena/wargame spectator ("commentator") system. 8 CMSG + 3 SMSG, all `Handle_NULL` /
`STATUS_UNHANDLED` in TC master. **Wire is FULLY recovered offline** (client serializers +
`C_Commentator` Lua API `CommentatorFrameDocumentation.lua`) — no sniff needed for structure.

## Method note (this corrects an earlier draft that called these "sniff-gated")

The AutoDump wire extractor mislabels these opcodes: it reported `u8` where the field is a
**string** or a **bit-length prefix**, and the IDB auto-names the primitive readers
`ai_Process_HousingDataPacket` / `ai_Read_CompressedUInt32FromPacket` — both **misnomers**.
Decompiling the readers themselves shows they are plain **fixed-width little-endian** copies
(no varints, no housing): `0x7FF72BE6C370`=read u8, `…C3C0`=u16, `…C410`=u32, `…C460`=u64,
`0x7FF72BEBDEA0`=PackedGuid→ObjectGuid. Send-side writers: `…CD20`=u8, `…CE60`=u32,
`…D060`=u64, `…D520`=string bytes. Field **names** come from the `C_Commentator` API return
structs. Lesson (again): decompile the serializer; never trust the extractor's u8/u32 for
these families.

## CMSG (client→server) — byte-exact

| Opcode | Wire | API mapping |
|---|---|---|
| ENABLE | `{ uint32 Enable }` | **BUILT (P0)** |
| GET_MAP_INFO | `{ string TargetPlayer }` | `UpdateMapInfo(targetPlayer)` |
| SPECTATE | `{ string TargetName }` | spectate-by-name |
| ENTER_INSTANCE | `{ uint32 MapID, uint32 InstanceIDLow, uint32 InstanceIDHigh, bit Field }` | `GetInstanceInfo`→mapID/instanceIDLow/High; `EnterInstance()` sends selected instance |
| EXIT_INSTANCE | `{}` (empty) | `ExitInstance()` |
| GET_PLAYER_INFO | `{ uint32 A, uint32 B, uint32 C, uint8 D }` | `GetPlayerData(teamIndex,playerIndex)` — hypothesis {contextId, teamIndex, playerIndex, flag} |
| GET_PLAYER_COOLDOWNS | `{ ObjectGuid Player, uint32 Count, Count×{uint32 SpellID, uint32 Category} }` | `RequestPlayerCooldownInfo`; pair = `CommentatorTrackedItemCooldown{spellID, category}` |
| START_WARGAME | bit-block `{6b len1, 6b len2, 1b TournamentRules}` + `uint64 (ListID low \| TeamSize high)` + `char[len1] TeamOneCaptain` + `char[len2] TeamTwoCaptain` | `StartWargame(listID, teamSize, tournamentRules, teamOneCaptain, teamTwoCaptain)` |

## SMSG_COMMENTATOR_STATE_CHANGED — `{ ObjectGuid, bit Enabled }` — **BUILT (P0)**

## SMSG_COMMENTATOR_MAP_INFO (deserializer `sub_7FF7290A1700`) — byte-exact

```
uint64  DirectoryId                       (unnamed; packed id for the blob)
uint32  MapCount
Map[MapCount] (JamCommentatorMap, 40B):
  uint32  TeamSize                        ← GetMapInfo.teamSize
  uint32  MinLevel                        ← GetMapInfo.minLevel
  uint32  MaxLevel                        ← GetMapInfo.maxLevel
  uint16  Field12                         (unnamed; no getter — bracket/season/flags?)
  uint32  InstanceCount                   ← GetMapInfo.numInstances
  Instance[InstanceCount] (JamCommentatorInstance, 112B):
    uint32     MapID                      ← GetInstanceInfo.mapID
    {u32,u32,u8} Tuple                    (unnamed per-instance triple)
    uint64     InstanceID                 ← instanceIDLow (low32) + instanceIDHigh (high32)
    uint32     Status                     ← GetInstanceInfo.status
    Team[2] (fixed, 40B):                 (arena = 2 factions)
      PackedGuid  TeamGUID
      uint32      PlayerCount
      Player[PlayerCount] (JamCommentatorPlayer, 32B):
        PackedGuid    PlayerGUID
        {u32,u32,u8}  Tuple               (hypothesis {specID, ?, faction})
```

## SMSG_COMMENTATOR_PLAYER_INFO (deserializer `sub_7FF7290A19D0`) — byte-exact

```
uint32  LeadingId                         (match/update id)
{u32,u32,u8}  SpellTuple                  (tracked-spell triple)
uint64  PackedId                          (unnamed handle)
uint32  Count
Record[Count] (152B, sub_7FF72906EFA0) → CommentatorPlayerData:
  PackedGuid  UnitToken(GUID)             ← unitToken (name resolved client-side / override channel)
  uint8   Faction                         ← faction
  uint32  Specialization                  ← specialization
  uint8   Field24, uint8 Field25          (2 extra wire bytes, not in Lua struct)
  uint16  Kills                           ← kills
  uint16  Deaths                          ← deaths
  uint32  DamageDone / DamageTaken        ← damageDone / damageTaken
  uint32  HealingDone / HealingTaken      ← healingDone / healingTaken
  uint8   SoloShuffleRoundWins / Losses   ← soloShuffleRoundWins / soloShuffleRoundLosses
  uint32  CountA,CountB,CountC,CountD
  CountB × {u32,u32,u32,u8}  (tracked auras/spells)
  CountC × {u32,u32}         (spellID→value)
  CountD × {u32}             (spellID list)
  CountA × 44B (sub_7FF72906DC60)  cooldown records {spellID + timers + flags, 2 optional u32}
bool  Field80  (stored in bit7 of a byte)
```
Note: `CommentatorPlayerData.name` is **NOT** in the record — carried via
`COMMENTATOR_PLAYER_NAME_OVERRIDE_UPDATE` / resolved from the GUID.

## Cooldown record layout — CONFIRMED by IDA + Ghidra (both agree byte-for-byte)

The 44-byte per-cooldown record reader (`sub_7FF72906DC60`) and its parent 152-byte player
record reader (`sub_7FF72906EFA0`) were re-decompiled in Ghidra (`wow_co3` / `wow_dump.bin`)
and match the IDA output exactly. Primitive readers reconfirmed: `func_0x7ff72be6c410`=u32,
`func_0x7ff72be6c370`=u8, `func_0x7ff72be6c3c0`=u16, `func_0x7ff72bebdea0`=PackedGuid.

### The 44-byte record IS `WorldPackets::Spells::SpellHistoryEntry` (SMSG_SEND_SPELL_HISTORY, opcode 0x62001A)

Triangulated three independent ways: (1) the client reader `sub_7FF72906DC60`; (2) the message
vtable `off_7FF72C4BB608` that owns this vector references `WowGetRawTypeName<struct
SpellHistoryEntry>` and returns opcode `6422554 == 0x62001A`; (3) TrinityCore's own serializer
`operator<<(ByteBuffer&, SpellHistoryEntry const&)` (`SpellPackets.cpp:680`) matches the reader
field-for-field and bit-for-bit. So the semantics are TC's existing struct — no guessing:

| mem | wire | type | gating | SpellHistoryEntry field |
|---|---|---|---|---|
| +0  | 1 | uint32 | fixed | **SpellID** (the lookup key) |
| +4  | 2 | uint32 | fixed | **ItemID** |
| +8  | 3 | uint32 | fixed | **Category** |
| +12 | 4 | int32  | fixed | **RecoveryTime** (cooldown duration, ms) |
| +16 | 5 | int32  | fixed | **CategoryRecoveryTime** (ms) |
| +40 | 6 | float  | fixed | **ModRate** (default 1.0) |
| +20 | 7 | int32  | optional bit7 | **RecoveryTimeStartOffset** |
| +28 | 8 | int32  | optional bit6 | **CategoryRecoveryTimeStartOffset** |
| +36 | — | bool   | bit5          | **OnHold** |

Note: durations are the *fixed* fields (+12/+16), NOT the optionals; the optionals are start
*offsets*. So my earlier "+20=startTime / +28=duration" hypothesis was wrong — corrected here.
Bit order matches (MSB-first): OptionalInit(RecoveryTimeStartOffset)→bit7,
OptionalInit(CategoryRecoveryTimeStartOffset)→bit6, Bits<1>(OnHold)→bit5, then FlushBits.

**POPULATED (P3+):** the cooldown array (array A) is now filled by reusing TC's own
`SpellHistory::WritePacket(SendSpellHistory*)` + `operator<<(SpellHistoryEntry)` — byte-identical
to what the client already parses for spell history, so zero fabrication. No charges/maxCharges in
this record (charges are a sibling vector at player+0x68, array C, still empty).

The 152-byte player record reader confirms P3's scalar order exactly: PackedGuid, u8 faction,
u32 spec, 2 spare bytes, u16 kills, u16 deaths, u32 damageDone/Taken + healingDone/Taken, u8
soloShuffle win/loss, then 4 array counts (A,B,C,D) — matching the shipped CommentatorPlayerInfo writer.

## Residual sniff items (VALUES only — structure is byte-exact and buildable now)

- Meaning of the top-level `uint64` ids (MAP_INFO DirectoryId, PLAYER_INFO PackedId).
- The `{u32,u32,u8}` tuple sub-field meanings (per-instance and per-player); best guess
  player = {specID, ?, faction}.
- The three `uint32` in GET_PLAYER_INFO (vtable-dispatched, no offline populator).
These are field VALUES, not structure — build sends honest 0/best-guess and names them FieldNN.

## Server data sources (all present in TC)

- MAP_INFO: enumerate active arenas via `sBattlegroundMgr` `m_Battlegrounds` (isArena),
  `GetArenaType()` (teamSize), `GetMapId()`, `GetInstanceID()`, `GetPlayers()` grouped by team.
- ENTER_INSTANCE: `BattlegroundMgr::GetBattleground(instanceId, bgTypeId)` → teleport into `GetBgMap()`.
- PLAYER_INFO: `GetBattlegroundScore(player)` for damage/healing/kills/deaths; spec/faction from Player.

## Build status

- **P0 BUILT** — ENABLE (RBAC gate) + STATE_CHANGED.
- **P1 BUILT** — GET_MAP_INFO → SMSG_COMMENTATOR_MAP_INFO from live arenas
  (`BattlegroundMgr::GetActiveArenas`).
- **P2 BUILT** — spectator subsystem: `Battleground` gains a spectator set
  (Add/Remove/HasSpectator, ejected in `EndBattleground`); ENTER_INSTANCE finds the arena,
  sets the commentator's BattlegroundId (satisfies `BattlegroundMap::CannotEnter`), makes them
  an inert game-master observer, and teleports into the arena map; EXIT_INSTANCE restores +
  teleports out; SPECTATE sets the native `PlayerData::SpectateTarget` UF to the followed unit.
- **P3 BUILT** — GET_PLAYER_INFO / GET_PLAYER_COOLDOWNS → SMSG_COMMENTATOR_PLAYER_INFO for the
  spectated arena; scalar stats from `GetBattlegroundScore` (kills/deaths/damage/healing) +
  spec/faction. The four tracked-spell/cooldown arrays are sent empty (count 0) — structure
  known, but the 44-byte cooldown record's optional-field layout is unconfirmed, so no
  fabricated timings (empty is byte-exact regardless of array order). Sole residual.
- **P4 BUILT** — CMSG_COMMENTATOR_START_WARGAME reader (bit-packed: 6-bit name lengths +
  tournament-rules bit, byte-aligned `u64(ListID | TeamSize<<32)`, two captain strings) +
  handler: validates both captains (online, distinct, lead distinct groups), resolves the
  arena template + level bracket, creates a non-rated Wargame-type arena
  (`CreateNewBattleground`), registers it, and ports both groups in as opposing sides
  (`SetBattlegroundId`/`SetBGTeam`/`SendToBattleground`; `AddPlayer` auto-fires on map arrival),
  then `StartBattleground`. TC base had no wargame-start path — this builds it from the BG
  primitives.

**ALL 8 CMSG + 3 SMSG implemented** (0 remaining Handle_NULL). Residual is the one documented
sniff item (cooldown-record optional-field layout + a few opaque u64/tuple VALUES).
