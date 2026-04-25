# Overnight 2026-04-25 — Housing 12.0.5.67186 Wire-Format Audit + Handler Wiring

Branch: `feature/housing-system`. Source: `C:\sniff\housing_click_all\dumps\dump_12.0.5.67186_2026-04-24_13-23-54.pkt` (174 housing packets, 56 distinct opcodes).

## Headline finding

**`Opcodes.cpp` lost 68 handler wirings during the 12.0.5 master sync (commit `4c149885786`).**
Every housing CMSG was registered as `STATUS_UNHANDLED` + `Handle_NULL`. The diagnostic logs in `WorldSession::Update` confirmed `status=5 (STATUS_UNHANDLED)` for every housing packet received from the test player. So the entire housing control plane (decor placement, fixture editing, room editing, neighborhood services) was silently dropping client requests.

Re-applied the wirings from the lost commit `21b5118e927` (Feb 14, 2026 — *"Wire all 68 CMSG handlers"*) and added 30 more `DEFINE_HANDLER` entries for handlers that were added to `HousingHandler.cpp` / `NeighborhoodHandler.cpp` after the Feb 14 patch but never registered.

End state: **114 of 122 housing/neighborhood handlers wired** (the remaining 8 correspond to TC-CUSTOM speculative opcodes in the `0xF0000000` range that the retail client never sends).

## Commits (per subsystem)

| Hash | Title |
|---|---|
| `914fa045817` | Housing: Close 12.0.5 wire-format gaps and add CMSG 0x380003 handler |
| `cb986f9fd22` | Housing: Fix SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR wire format |
| `8ae3433850f` | Housing: Fix SMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS_RESPONSE name length |
| `6cecda337d7` | Housing: Format potential-owner names as `<Char>-<Realm>` |
| `93664756925` | Housing: Wire 110+ housing/neighborhood handlers (regression from 12.0.5 sync) |

## Per-subsystem status

### HousingDecor (0x30 / 0x51) — closed

- **CMSG_HOUSING_DECOR_PLACE** (0x300001): added optional 14-byte trailing block (`PackedGUID + uint8 + int32`) for placements with a complex source reference. Sniff: 1 of 3 samples carried the tail (53/54/68-byte body lengths). Detected by remaining bytes — keeps standard storage→plot path unchanged. New fields: `SourceGuid`, `SourceFlags`, `SourceField`, `HasSourceTail`.
- **CMSG_HOUSING_DECOR_MOVE / REMOVE / LOCK / SET_EDIT_MODE / REQUEST_STORAGE / REDEEM_DEFERRED_DECOR**: byte-for-byte verified, no changes.
- **SMSG_HOUSING_DECOR_*_RESPONSE**: all 7 verified against retail bytes — match exactly.
- Re-wired `HandleHousingDecorPlace` + 14 other decor handlers.

### HousingFixture (0x31 / 0x52) — closed

- **CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE** (0x310004): added trailing `Bits<1> ApplyImmediate` flag. Sniff: 1 trailing byte (0x00) in observed sample.
- **CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE** (0x310005): added trailing `Bits<1> ApplyImmediate` flag. Sniff: 1 trailing byte in all 4 captures.
- **SMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE_RESPONSE / SET_CORE_FIXTURE_RESPONSE**: verified, match exactly.
- Re-wired `HandleHousingFixtureSetHouseType` + 5 other fixture handlers.

### HousingRoom (0x32 / 0x53) — closed

- **CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS** (0x320006): added trailing `Bits<1> ApplyImmediate` flag (was 22 bytes read vs 23 sniff).
- **CMSG_HOUSING_ROOM_ADD / SET_COMPONENT_THEME / SET_DOOR_TYPE / SET_LAYOUT_EDIT_MODE**: verified.
- **SMSG_HOUSING_ROOM_*_RESPONSE**: all 5 verified.
- Re-wired all 9 room handlers.

### HousingServices (0x33 / 0x54) — closed

- **SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR** (0x540011): rewrote `Write()` after sniff disagreed with the IDA-speculative 3-PackedGUID entry layout. Retail per-entry is `uint8 EntryFlags + uint32 EntryTimestamp + PackedGUID HouseGUID + int64 NewFavorTotal + uint32 Reserved + uint8 Terminator(0x80)` — exactly 27 bytes. Updated 3 callers (HousingHandler::HandleHousingSvcsQueryHouseLevelFavor, NeighborhoodHandler post-buy, Housing::SetLevel/AddFavor) to use the new struct fields.
- **SMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS_RESPONSE** (0x54001A): two bugs fixed:
  - The 9-bit name length included `+ 1` for a NUL terminator that retail does not send. Cross-verified `"Anondk-AltarofStorms"` (20 chars → `lenByte1=0x0A lenByte2=0x00`) and `"Dahuntermon-AltarofStorms"` (25 → `0x0C 0x80`). The trailing zero byte we appended also drifted the next entry's PackedGUID mask one byte off, corrupting the player list.
  - Player names now sent as `"<CharacterName>-<RealmNormalizedName>"` (cross-realm display format) instead of bare character name. Falls back if RealmList is unavailable.
- **CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS**, **GET_PLAYER_HOUSES_INFO**, **NEIGHBORHOOD_RESERVE_PLOT**, **GET_HOUSE_FINDER_INFO/NEIGHBORHOOD**, **GET_POTENTIAL_HOUSE_OWNERS** Read paths verified.
- Re-wired all 25+ services handlers.

### HousingSystem (0x35 / 0x55) — verified clean

- **CMSG_HOUSING_HOUSE_STATUS / GET_CURRENT_HOUSE_INFO / GET_PLAYER_PERMISSIONS** Read paths verified.
- **SMSG_HOUSING_HOUSE_STATUS_RESPONSE** (27 bytes), **GET_CURRENT_HOUSE_INFO_RESPONSE** (31 bytes), **GET_PLAYER_PERMISSIONS_RESPONSE** (11 bytes) all match sniff exactly.
- Re-wired 8 system handlers.

### NeighborhoodInitiative (0x38) — closed

- **CMSG_GET_NEIGHBORHOOD_INITIATIVE_INFO_REQUEST** (0x380003): NEW opcode + handler. Body = packed `NeighborhoodGuid` (7 bytes). Sent by the Lua C_NeighborhoodInitiative.RequestNeighborhoodInitiativeInfo API (function at IDA `0x7ff75cec26c0`, identified via housing_analysis.json subsystem index). Handler delegates to `InitiativeManager::SendPlayerInitiativeInfo` (same SMSG response as the paired `ACTIVITY_LOG_REQUEST`). Eliminates the only `"No defined handler for opcode"` log entry observed in `M:\Wplayerbot\logs\Server.log`.
- Wired `HandleGetAvailableInitiativeRequest`, `HandleGetInitiativeActivityLogRequest`, `HandleNeighborhoodInitiativeServiceStatusCheck`, `HandleInitiativeUpdateActiveNeighborhood` (were `Handle_NULL` despite having complete implementations).

### Neighborhood (0x39 / 0x5C) — verified, one note

- **CMSG_NEIGHBORHOOD_MOVE_HOUSE / OPEN_CORNERSTONE_UI** Read paths verified.
- **SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE** (40 bytes), **OPEN_CORNERSTONE_UI_RESPONSE** (43 bytes) match sniff exactly.
- **Note** (not actioned tonight): The `MOVE_HOUSE` request's second `PackedGUID` is named `PlotGuid` in our packet class but sniff decodes it as a `Housing/3` `HouseGUID`. Server's handler calls `HousingMgr::ResolvePlotIndex(PlotGuid, neighborhood)` which checks `cornerstoneGuid.GetHigh() != HighGuid::GameObject` and returns -1 silently. Implication: the destination plot for a move is determined by prior CornerstoneUI session state, not by the second GUID. Needs IDA verification of the actual server handler to fully spec — left as a known issue.
- Re-wired all 19 neighborhood handlers (charter, roster, invite, evict, buy/move/offer-ownership).

### HouseExterior (0x2E / 0x50) — verified existing

- Not represented in this sniff (the captured session was decor/fixture/room work). Existing `HouseExteriorCommitPosition` Read + `HouseExteriorLockResponse` Write retained from the build-66838 audit.
- Wired `HandleHouseExteriorSetHousePosition`, `HandleHouseExteriorLock`, `HandleHouseInteriorLeaveHouse`.

## Files touched

| File | Lines changed |
|---|---|
| `src/server/game/Server/Packets/HousingPackets.h` | +35 / -2 |
| `src/server/game/Server/Packets/HousingPackets.cpp` | +44 / -13 |
| `src/server/game/Server/Protocol/Opcodes.h` | +1 |
| `src/server/game/Server/Protocol/Opcodes.cpp` | +109 / -79 |
| `src/server/game/Server/WorldSession.h` | +2 |
| `src/server/game/Handlers/NeighborhoodHandler.cpp` | +35 / -4 |
| `src/server/game/Handlers/HousingHandler.cpp` | +14 / -7 |
| `src/server/game/Housing/Housing.cpp` | +6 / -8 |
| `docs/OVERNIGHT_2026_04_25.md` | new |

## Open items / future work

1. **CMSG_NEIGHBORHOOD_MOVE_HOUSE field naming** — second PackedGUID is a HouseGUID, not a cornerstone PlotGuid. The current handler resolves plot via the GO entry which silently fails for Housing/3 GUIDs. Move target is determined elsewhere (cornerstone UI session state). Needs IDA decompile of the server-side handler in retail to verify.
2. **CMSG_HOUSING_DECOR_PLACE source-tail field semantics** — read fields are now consumed but their meaning in placement logic is undocumented. Pending IDA on `HousingDecorSystem::SerializePlaceCMSG`.
3. **Speculative initiative opcodes** — 8 handlers (Claim Reward, Leaderboard, Open Chest, Task Accept/Abandon/Progress, Accept Milestone, Report Progress) remain unwired because their opcodes are TC-CUSTOM (`0xF0000000` range). If/when retail emission is observed, swap the speculative opcode for the real one and wire the handler.
4. **DB2 unknown fields** — `ExteriorComponent.Field_7/9/11`, `HouseExteriorWmoData.Field_003/004`, `HouseRoom.Field_007` still placeholder names. Per `memory/db2_housing_fields_research.md`, WoWDBDefs has no public name for these — safest left as-is.
5. **Decor GUID arg1 mismatch on MOVE / REMOVE / LOCK / DYE** — symptom: in-game error *"Das Dekor kann nicht aus deiner Haus-Truhe gelöscht werden"* (German localisation of `HOUSING_RESULT_DECOR_NOT_FOUND` = 0x0B). Server's `_placedDecor` / `HousingMap::_decorGuidToGoGuid` keys the decor GUID with `arg1 = sRealmList->GetCurrentRealmId().Realm` (=1 here), but the client's CMSG decor GUID arrives with `arg1 = 0`. Verified by decoding the wire bytes: response packet's PackedGUID Hi mask was `0xC2` (bits 1, 6, 7) — bit 4 absent → byte 4 = 0 → arg1 = 0. A server-side counter-only fallback (commit `e9630b4996d`) was tested as a band-aid and reverted (`4b0d8704fb6`) because it isn't blizzlike. Need IDA on `FHousingStorage_C` / `m_housingStorageData.Decor` MapUpdateField key emit/parse path to determine whether: (a) the client deserialiser zero-fills missing PackedGUID bytes and our server is dropping arg1 byte 4 on emit, (b) decor GUIDs are supposed to use `arg1 = 0` always (and our `sRealmList->GetCurrentRealmId().Realm` write at `Housing.cpp:272/755/978/2474` is the bug), or (c) the housing map key serialisation uses a different bit layout than the wire PackedGUID. Until then decor MOVE / REMOVE / LOCK / DYE on existing placed items always returns `DECOR_NOT_FOUND`.

## Tools used

- Sniff parser: `analyze_sniff_12_0_5_67186.py` + per-opcode dumps in `docs/sniff_12_0_5_67186/*.txt`.
- IDA research DB: `C:\dumps\research_copy\wow_dump.bin.tc_wow.db` (read-only) + `housing_analysis.json` for function/Lua-API maps.
- Live IDA MCP was unreachable at session start — fell back to the SQLite snapshot + `housing_decompiled.c` text file.
