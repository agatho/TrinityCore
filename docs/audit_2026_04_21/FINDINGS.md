# Housing System Audit — Sniff-by-Sniff Deviations vs Blizzard

**Date:** 2026-04-21 / 22
**Build:** 12.0.1.66838
**Scope:** Login → housing-map entry → plot interaction flow

## Source sniffs

| Purpose | Path | Notes |
|---------|------|-------|
| OUR latest | `Downloads/ymir_retail_12.0.1.66198/dumps/dump_12.0.1.66838_2026-04-21_22-31-49.pkt` | 1.82 MB, 1274 packets — fresh login, no interactions |
| OUR earlier | `…/dump_12.0.1.66838_2026-04-21_18-11-22.pkt` | 1.82 MB, 1584 packets — pre-ghost-GUID-fix |
| RETAIL editor | `c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt` | 14.8 MB, 12615 packets — full editor session |
| RETAIL floorplan | `c:/sniff/floorplan_editor_rotation/dumps/dump_12.0.1.66838_2026-04-10_08-45-23.pkt` | 15.4 MB |
| RETAIL wallcustomize | `c:/sniff/wall_floor_ceiling_customize/dumps/dump_12.0.1.66838_2026-04-12_10-11-26.pkt` | 11.4 MB |

## Caveats affecting interpretation

1. **Sessions are at different lifecycle stages.** Retail sniffs contain deep-play traffic (combat, flight syncs, chat). Ours is a fresh login with idle. Most packet-count deltas (SMSG_AURA_UPDATE 690 vs 103, SMSG_ON_MONSTER_MOVE 627 vs 23, SMSG_FLIGHT_SPLINE_SYNC 136 vs 0) are environmental, not bugs.
2. **Opcode name space.** Opcodes live in a flat 32-bit enum; both SMSG and CMSG share numeric values and direction is only set by the wire tag. The first-pass classifier in `33_opcode_sequence.py` guessed groups by prefix — several guesses were wrong. Canonical map generated at `docs/audit_2026_04_21/opcode_map.json` (2457 opcodes from `Opcodes.h`).
3. **Heuristic vs strict parsing.** The byte-level entity scanner (`30_scan_all_high_types.py`) is tolerant but over-counts false positives; the strict parser (`35_inventory_mapentry.py`) is authoritative. Use strict counts as ground truth when available.

## Findings by severity

### SEVERITY 1 — breaks icon/rendering

#### 1.1 FHousingPlayerHouse_C.EntityGUID pointed at a ghost (FIXED 2026-04-21)
- **Was:** `SetEntityGUID(housing->GetHouseGuid())` on own + proxy entities. Client chased the pointer back into the identity entity (no position fragment) → icon could never resolve.
- **Retail:** points at a `HighGuid::Entity` (57) mirror carrying `FMirroredPositionData_C` plus `Tag_HouseExteriorPiece/Root` (Group A, 4 entries at retail idx 9984).
- **Fix:** commits 7a8be9e015 + 4262499682 — new `HousingMirrorEntity` class, spawn paired with exterior root MeshObject, `EntityGUID` rewired to mirror GUID. Binary built, awaiting deploy + sniff to verify on the wire.
- **Verification:** expected sniff to contain 4 `HighGuid::Entity objectType=18` CREATEs in the map-entry UPDATE_OBJECT. Current OUR sniff has 0.

#### 1.2 Second round of mirror entities: per-MeshObject anchors (Group B)
- **Retail:** also emits 4 additional `HighGuid::Entity objType=18` entities with *only* `FMirroredPositionData_C` attached to `HighGuid::MeshObject` parents. These are per-piece anchors (e.g. for doors, windows) used by the client for finer-grained spatial queries (door-hover detection, placement previews).
- **Ours:** emits zero.
- **Recommended:** extend `HousingMap::SpawnExtCompTree` to pair a Group-B mirror per MeshObject piece as it is created. `AttachParent = piece's MeshObject GUID`, local pos = `(0,0,0)`, identity rotation, `Tag_*` omitted.
- **Impact:** unknown — possibly needed for expert-mode placement previews or mid-house GO click targeting. Low risk since it is pure add.

### SEVERITY 2 — missing SMSG emissions that the client expects

#### 2.1 SMSG_HOUSING_CATALOG_STATE_SYNC (0x0056000E) — never sent
- **Retail:** sent twice per session (idx 6381 shortly after login; idx 10406 after map entry).
- **Ours:** `Housing::BuildCatalogStateSync()` exists at `Housing.cpp:2474` but has **zero call sites**.
- **Impact:** the client's catalog state machine may never see a server sync, leading to stale/incorrect catalog quantities or missing Account entity updates.
- **Fix:** call from `HousingMap::AddPlayerToMap` after decor storage population, and from decor placement/removal handlers that mutate catalog quantities.

#### 2.2 SMSG_HOUSING_HOUSE_STATUS_RESPONSE over-sent (5 vs retail 2)
- **Retail:** twice per map entry (once in the login CMSG round-trip and once after ENTER_PLOT).
- **Ours:** 5 times — the `at_housing_plot.cpp` AT script re-sends on every plot-enter AT trigger plus the deferred ENTER_PLOT callback repeats it.
- **Impact:** possibly harmless but client may flap editor state if flag bytes differ between sends.
- **Fix:** audit `at_housing_plot.cpp` + `HousingMap::AddPlayerToMap` deferred block — consolidate to exactly two emissions: one synchronous after CURRENT_HOUSE_INFO, one in the ENTER_PLOT block.

#### 2.3 SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE over-sent (5 vs retail 2)
- Same cause as 2.2 — the AT script re-sends on every overlap. Consolidate.

#### 2.4 SMSG_MIRROR_VARS content incomplete (41 vars vs retail 116)
- **76 var names** in retail that we do not send:
  - 29 shop/market/Shop2 variables (shop2HostUrlAuth, shop2VCPlacementStr, …) — platform-level, low housing impact
  - 47 other (Lua resource caps, image sharing API, damage meter, recent allies, hardcore throttling, addonChatRestrictions…) — not housing
  - **Zero housing/neighborhood names are missing.**
- **Our singular extra:** `bypassItemLevelScalingCode` is retained — OK.
- **Impact on icon:** none directly. Some (`addonChatRestrictionsEnabled`) affect base UI, but do not drive plot rendering.
- **Fix priority:** LOW for housing audit, but worth a follow-up pass for full retail parity.

#### 2.5 SMSG_INIT_WORLD_STATES count 1 vs retail 2
- **Retail:** sends two (on map entry, and again after a sub-area change).
- **Ours:** sends one (map entry).
- **Impact:** probably cosmetic; all world state values are applied from the first packet. Low priority unless we observe world-state flicker in-game.

### SEVERITY 3 — entity bundle composition deficits

Strict parser of the map-entry `SMSG_UPDATE_OBJECT` (retail idx 9984, ours idx 295):

| Role | Retail | Ours | Gap | Notes |
|------|--------|------|-----|-------|
| Items (inventory) | 191 | 1 | +190 | Retail player has a fuller inventory. Not a housing bug. |
| GameObjects in bundle | 16 | 0 | +16 | Our plot/house/cornerstone GOs are not in the initial bundle — they arrive via grid visibility later. Investigate whether the client's icon picker needs them synchronously. |
| Housing/3 identity | 5 | 3 | +2 | Proxy emission works (our log says emitted=3), strict parser only sees 2 proxies + session = 3. Investigate the missing one. Possibly an edit-mode or post-bundle resend. |
| Entity mirrors (Group A, exterior) | 4 | 0 | +4 | Will be fixed by deploying commit 4262499682. |
| Entity mirrors (Group B, per-piece) | 4 | 0 | +4 | Not yet implemented — see finding 1.2. |
| MeshObjects in bundle | 2 | 0 | +2 | Our MeshObjects spawn correctly but arrive outside the bundle. |
| Creatures | 6 | 0 | +6 | Ambient NPCs not spawned in our housing map. Not icon-related. |
| Corpses | 32 | 1 | +31 | Other players' corpses. Environmental. |

Biggest actionable items: GameObjects + MeshObjects not in the map-entry bundle. Retail clearly bundles them; we rely on grid visibility which means they arrive a tick later.

### SEVERITY 4 — opcode coverage / handler completeness

- **All 105 declared housing/neighborhood CMSG opcodes** in `Opcodes.h` have real handlers registered (not `Handle_NULL`). No unhandled CMSGs detected.
- **CMSG coverage verified against retail activity list:**
  - Decor: PLACE, MOVE, LOCK, SET_EDIT_MODE, REMOVE, REQUEST_STORAGE — all handled.
  - Fixture: CREATE_BASIC_HOUSE, SET_EDIT_MODE, CREATE/DELETE_FIXTURE, SET_HOUSE_SIZE/TYPE — all handled.
  - Room: ADD, MOVE, REMOVE, ROTATE, SET_LAYOUT_EDIT_MODE, SET_COMPONENT_THEME, SET_DOOR_TYPE, SET_CEILING_TYPE, APPLY_COMPONENT_MATERIALS — all handled.
  - Svcs: Neighborhood creation, teleport, BNet friend list, plot reservation — all handled.
  - System: GET_CURRENT_HOUSE_INFO, HOUSE_STATUS, GET_PLAYER_PERMISSIONS — all handled.
- **Not yet exercised in any of our sniffs** (because the user only logged in):
  - Decor lock/move/place/remove interactions
  - Room layout changes
  - Photo sharing
  - Teleport-to-plot
  - Any Fixture interaction

No coverage bug; just zero test data for those paths. Follow-up audit after the user exercises each interaction.

### SEVERITY 5 — structural observations (no fix required)

- **Opcode namespace.** Some SMSG opcodes live in numeric ranges (0x55, 0x56, 0x5A, 0x5C) that an initial classifier flagged as "CMSG". The opcode space is flat; direction comes solely from the packet tag. Fixed the classifier in `37_opcode_diff.py`.
- **Wire format for Housing/3 entities.** All our entities use `objectType=0x12` (18) correctly after `TYPEID_HOUSING_ENTITY` fix (commit 76fff332d5).
- **Ghost GUID entity no longer sent.** Commit 2a1a6e3e11 removed the `arg2=neighborhoodMapID` phantom; sniff idx 295 has 4 distinct Housing/3 GUIDs matching plot HouseGuids exactly.

## Remediation plan (prioritised)

### Immediate (implement now)

1. **Deploy mirror-entity binary + verify** — stop worldserver, copy new exe, relaunch, relog, capture sniff, run `30_scan_all_high_types.py` to confirm `HighGuid::Entity (57) count ≥ 4`.
2. **Call `Housing::BuildCatalogStateSync`** from `HousingMap::AddPlayerToMap` (after decor storage population) and send the resulting packet. Add a log line for verification.
3. **Consolidate over-emission** of `HOUSE_STATUS_RESPONSE` and `GET_PLAYER_PERMISSIONS_RESPONSE` to exactly 2 per map entry.
4. **Implement Group B mirror pairing** for each MeshObject in `HousingMap::SpawnExtCompTree` and each room component.

### Follow-up (post-deploy verification)

5. **Bundle GameObjects into map-entry UPDATE_OBJECT** — cornerstone, plot GOs, door GO. Current path relies on grid visibility which fires a tick later.
6. **Bundle MeshObjects into map-entry UPDATE_OBJECT** — same pattern as (5).
7. **Add missing MIRROR_VARS** for non-housing system flags (shop2 family + Lua/image-sharing flags) if we see downstream client-side behaviour tied to them.
8. **Investigate the single missing proxy** in strict-parser count (3 expected, 2 found for Housing/3 at idx 295) — possibly in a later UPDATE_OBJECT and the bundle just fragmented.
9. **Second INIT_WORLD_STATES** — check if the retail pattern is area-change-triggered; mirror that.

### Long-running (nice to have)

10. **Creature / ambient NPC spawns** on housing maps for visual parity.
11. **Re-run the full audit** on a sniff captured from a full interactive session (not just login) — decor, fixtures, rooms, teleports — to identify wire-format deviations in the response paths.

## Tool inventory — generated during this audit

| Script | Purpose |
|--------|---------|
| `33_opcode_sequence.py` | Chronological opcode sequence dump per sniff |
| `34_align_login.py` | Side-by-side login→map-entry flow comparison |
| `35_inventory_mapentry.py` | Strict CREATE-block parser — authoritative entity inventory |
| `36_build_opcode_map.py` | Auto-generated `OPCODE_MAP` from `Opcodes.h` |
| `37_opcode_diff.py` | Housing/Neighborhood-filtered opcode count diff |
| `38_house_info_wireformat.py` | Wire dumps for key Housing SMSG responses |
| `39_mapentry_role_inventory.py` | Heuristic role summary for map-entry bundle |
| `40_compare_key_packets.py` | Byte-level dump for 9 key packets, side by side |
| `41_extract_mirror_vars.py` | MIRROR_VARS name diff (76 retail-only names extracted) |

## Audit artefacts

| File | Content |
|------|---------|
| `FLOW_COMPARISON.md` | Login→map-entry packet alignment (38 KB) |
| `OPCODE_DIFF.md` | Housing+Neighborhood opcode count diff |
| `OPCODE_0x5D_ANALYSIS.md` | False-alarm writeup (0x5A005D = FLIGHT_SPLINE_SYNC, not housing) |
| `OUR_idx295_entities.csv` + `_summary.md` | Strict-parsed entity inventory, our map-entry bundle |
| `RETAIL_idx9984_entities.csv` + `_summary.md` | Strict-parsed entity inventory, retail map-entry bundle |
| `MAPENTRY_ROLE_INVENTORY.md` | Role-grouped heuristic inventory |
| `WIRE_HOUSE_INFO.md` | Wire dumps of house-info/house-status/permissions packets |
| `WIRE_KEY_PACKETS.md` | Wire dumps of 9 login-phase packets |
| `MIRROR_VARS_DIFF.md` | Name diff of MIRROR_VARS vars |
| `opcode_map.json` / `opcode_map.py` | Canonical 2457-entry opcode→name map |
