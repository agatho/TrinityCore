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

## Build order

P1 = packet layer (all CMSG + both SMSG, byte-exact above) + GET_MAP_INFO→MAP_INFO from live
arenas. P2 = spectator subsystem in `Battleground` (add/remove team-less observer, teleport
in/out) for ENTER/EXIT/SPECTATE. P3 = PLAYER_INFO/COOLDOWNS. P4 = START_WARGAME (reuse arranged
wargame, see `feature/war-games`).
