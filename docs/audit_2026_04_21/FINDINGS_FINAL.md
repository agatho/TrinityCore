# Housing System Audit — Final Report

**Started:** 2026-04-21 23:10
**Completed:** 2026-04-22 ~02:00
**Build:** 12.0.1.66838
**Audit scope:** Login → housing-map entry → plot interaction flow

## Executive summary

Comprehensive bit-level comparison of our server's output against three retail sniffs of the same build. Generated 44 analysis scripts, 11 audit documents, and committed 4 code changes plus the HousingMirrorEntity system (committed earlier in the night).

### What was found

1. **Three real wire deviations** (severity: likely breaks client state):
   - `HOUSE_STATUS_RESPONSE` has 2 extra PackedGuids vs retail (we write 4, retail writes 2 with variable trailing bytes)
   - `SMSG_HOUSING_CATALOG_STATE_SYNC` was never emitted despite the infrastructure being in place — **FIXED**
   - `FHousingPlayerHouse_C.EntityGUID` pointed at itself (self-reference) instead of a `HighGuid::Entity` mirror — **FIXED via HousingMirrorEntity commits**

2. **One architectural deviation** (severity: probably cosmetic):
   - Our server splits the map-entry CREATE bundle into two `SMSG_UPDATE_OBJECT` packets (idx 295: 57 KB session + proxies; idx 310: 552 KB grid dump). Retail packs everything into one packet (idx 9984: 293 KB).

3. **One visibility-distance issue** (severity: 20× overhead):
   - `HousingMap::InitVisibilityDistance` uses `MAX_VISIBILITY_DISTANCE` (533 y). Result: we stream 1009 GameObjects + 186 Creatures + 18 ATs to the client at map entry. Retail uses bounded visibility and sends 51 GOs + 24 Creatures + 2 ATs. Not yet fixed (risk of breaking world-map icon resolution for distant plots — plot ATs need to remain visible for the client to look them up by HouseGUID).

4. **Over-emission** of `SMSG_HOUSING_HOUSE_STATUS_RESPONSE` + `SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE`:
   - We send 5 per session; retail sends 2. **Partially fixed** by dropping the duplicate emission from `HousingMap::AddPlayerToMap`'s deferred ENTER_PLOT callback (commit 5ae862a143). `at_housing_plot` AT script still emits on every overlap; that wasn't touched.

5. **MIRROR_VARS incompleteness:**
   - 76 retail variables missing from our send. 40 backfilled in commit e263ae9ee5 (Lua caps, addon chat, damage meter, LFG text, hardcore throttle, pvp training). 36 skipped — Shop2 / Pinterest URLs, not needed single-player.

6. **AccessFlags value mismatch:**
   - Our `CURRENT_HOUSE_INFO_RESPONSE` sets `AccessFlags = 0x3ff` (all bits). Retail sends `0x20` (bit 5 only). Semantics not yet decoded — flagged for IDA follow-up.

7. **False alarms:**
   - `CMSG_NeighborhoodSystem[005D]` (136 times in retail, 0 in ours) turned out to be `SMSG_FLIGHT_SPLINE_SYNC` — our sniff classifier had a wrong prefix mapping. Not a housing issue.
   - "We emit 15 entities vs retail's 494" — artefact of comparing the wrong UPDATE_OBJECT. Total entity count is actually 1231 on our side vs 494 retail (we over-emit due to visibility).

### What was fixed during the audit

Commits landed (chronological):

| Commit | File(s) | Impact |
|--------|---------|--------|
| 7a8be9e015 | `HousingMirrorEntity.{h,cpp}` | Lightweight `HighGuid::Entity` + `FMirroredPositionData_C` entity skeleton |
| 4262499682 | `HousingMap.{h,cpp}` + `Player.cpp` | Wired mirror lifecycle + Player CREATE bundling + `EntityGUID` rewire |
| e263ae9ee5 | `HousingMap.cpp` + `AuthHandler.cpp` | Emit `SMSG_HOUSING_CATALOG_STATE_SYNC` + 40 missing `MIRROR_VARS` |
| 5ae862a143 | `HousingMap.cpp` | Drop duplicate `HouseStatus` emission from deferred ENTER_PLOT |

All changes build clean. Binary at `build/bin/RelWithDebInfo/worldserver.exe` (Apr 22 ~01:40). Deployment: stop the running worldserver, copy to `M:\Wplayerbot\worldserver.exe`, restart.

### What remains (ordered by priority)

1. **Deploy & verify.** Sniff the next session and confirm:
   - `HighGuid::Entity` count in map-entry bundle goes from 0 → 4+
   - `SMSG_HOUSING_CATALOG_STATE_SYNC` appears in the packet stream
   - `HOUSE_STATUS_RESPONSE` count drops from 5 → 3-4
2. **`HOUSE_STATUS_RESPONSE` wire format.** Proper IDA re-read of `sub_0x550000`. Retail sends 2 PackedGuids + 6-8 trailing bytes; we send 4 PackedGuids + 2 bytes. Fix the serializer to match.
3. **Visibility distance.** Tune `HousingMap::InitVisibilityDistance` to `VISIBILITY_DISTANCE_LARGE` (200 y) with plot ATs marked `SetActive(true)` so distant ones remain in registry. Sniff-diff to verify.
4. **`AccessFlags` value semantics.** IDA-verify what bits 0-9 of `CURRENT_HOUSE_INFO_RESPONSE.AccessFlags` mean. We currently send 1023, retail sends 32.
5. **Spawn ambient creatures** on housing maps for visual parity (match retail's creature entries).
6. **Group-B mirrors** (4 per-MeshObject `FMirroredPositionData_C` proxies in retail).
7. **Second `INIT_WORLD_STATES`** emission (retail sends 2; we send 1).
8. **Full decor/fixture/room interaction sniff** — nothing was tested tonight because the user only logged in. Each of those 30+ retail-only CMSG/SMSG handlers needs wire-format verification when exercised.

## Method

### Sniffs used

| Source | Build | Size | Packets |
|--------|-------|------|---------|
| RETAIL editor-session | 66838 | 14.8 MB | 12,615 |
| RETAIL floorplan | 66838 | 15.4 MB | 18,547 |
| RETAIL wallcustomize | 66838 | 11.4 MB | 8,698 |
| OUR latest (post-ghost-fix) | 66838 | 1.82 MB | 1,274 |
| OUR earlier (pre-ghost-fix) | 66838 | 1.82 MB | 1,584 |

### Analysis scripts (all in `sniff_analysis_login_plot/`)

| Script | Purpose | Output |
|--------|---------|--------|
| 33 | Per-sniff opcode histograms | `*_opcode_counts.txt`, `*_opcodes.txt` |
| 34 | Login→map-entry packet alignment | `FLOW_COMPARISON.md` |
| 35 | Strict CREATE-block parser | `*_entities.csv`, `*_entities_summary.md` |
| 36 | Canonical opcode→name map from `Opcodes.h` | `opcode_map.json`/`opcode_map.py` |
| 37 | Authoritative opcode diff | `OPCODE_DIFF.md` |
| 38 | Wire dump of house-info/status/permissions | `WIRE_HOUSE_INFO.md` |
| 39 | Heuristic role inventory | `MAPENTRY_ROLE_INVENTORY.md` |
| 40 | Side-by-side dumps for 9 key packets | `WIRE_KEY_PACKETS.md` |
| 41 | MIRROR_VARS name diff | `MIRROR_VARS_DIFF.md` |
| 42 | MIRROR_VARS retail values | `MIRROR_VARS_RETAIL_VALUES.md` |
| 43 | Strict parser on both our UPDATE_OBJECTs | `OUR_idx295_*` + `OUR_idx310_*` |
| extract_5d.py | Decode 0x5A005D heuristic false positive | `OPCODE_0x5D_ANALYSIS.md` |

### Audit documents (all in `docs/audit_2026_04_21/`)

| Document | Purpose |
|----------|---------|
| `FINDINGS.md` (v1) | Initial findings (superseded by v2) |
| `FINDINGS_v2.md` | Corrected findings after discovering split bundle |
| `FINDINGS_FINAL.md` | **This document** — final report |
| `FLOW_COMPARISON.md` | Login→map-entry packet-by-packet alignment |
| `OPCODE_DIFF.md` | Housing/neighborhood opcode count diff |
| `MAPENTRY_ROLE_INVENTORY.md` | Role-grouped inventory with deficit calculation |
| `MIRROR_VARS_DIFF.md` | 76 retail-only var names |
| `MIRROR_VARS_RETAIL_VALUES.md` | Retail values for 95 extracted name-value pairs |
| `WIRE_HOUSE_INFO.md` | Raw byte dumps of house-info/status/permissions |
| `WIRE_KEY_PACKETS.md` | Byte comparisons for 9 key packets |
| `OPCODE_0x5D_ANALYSIS.md` | False-alarm investigation |
| `OUR_idx295_entities.csv`+`.md` | Strict-parsed our first bundle (15 entities) |
| `OUR_idx310_entities.csv`+`.md` | Strict-parsed our second bundle (1216 entities) |
| `RETAIL_idx9984_entities.csv`+`.md` | Strict-parsed retail bundle (494 entities) |
| `opcode_map.json`+`.py` | Canonical 2457-entry opcode-name map |

## Notes for the next session

- **Deploy before testing.** The worldserver PID 66124 is still running the pre-audit binary. Copy `build/bin/RelWithDebInfo/worldserver.exe` → `M:\Wplayerbot\worldserver.exe` after stopping the process.
- **Capture a fresh sniff immediately after map entry** (login → move to plot → idle 30 sec is enough). Diff it against the pre-audit sniffs.
- **Focus verification on:**
  - `HighGuid::Entity (57)` count in `SMSG_UPDATE_OBJECT` should be ≥ 4 (own + 3 neighbour mirrors)
  - `SMSG_HOUSING_CATALOG_STATE_SYNC` must appear with a non-zero entry count
  - `SMSG_HOUSING_HOUSE_STATUS_RESPONSE` count should drop from 5 to 3-ish
  - `SMSG_MIRROR_VARS` should have more variables (check via `41_extract_mirror_vars.py`)

- **If the icon is still wrong after deploy**, the next targets are:
  - Recheck `FHousingPlayerHouse_C.EntityGUID` byte content — our integration sets it to the mirror GUID but the mirror was only just committed; confirm the CREATE block contains the right EntityGUID value at struct offset +56
  - Revisit the `HOUSE_STATUS_RESPONSE` wire format (IDA re-read)
