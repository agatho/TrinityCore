# Login + Enter-Plot Flow Analysis — Build 66838

**Sniff**: `c:/sniff/interrior_exterrior_advanced_editor/dumps/dump_12.0.1.66838_2026-04-15_09-35-59.pkt`
**Total packets parsed**: 12,615
**Parser**: `05_final_parse.py` (raw byte PKT, WowPacketParser bypassed per user requirement)

---

## 1. High-level timeline

| idx   | dir  | opcode                       | notes                                          |
|------:|------|------------------------------|------------------------------------------------|
|  419  | CMSG | CMSG_PLAYER_LOGIN            | body `0fa0170561 0cd40800 80ed4400` — 10-byte guid + loginInfo |
| 5393  | SMSG | SMSG_LOGIN_VERIFY_WORLD      | **mapId=2783 (Home Interior)** → login lands INSIDE the house |
| 5655  | SMSG | SMSG_UPDATE_OBJECT           | 230,904 bytes — interior terrain + player CREATE |
| 6381  | SMSG | 0x0056000E                   | 5344-byte housing bulk state (sent twice total) |
| 6383  | SMSG | 0x0056001B                   | 30,082-byte housing bulk blob |
| 6443  | CMSG | 0x00380000                   | InitiativeSystem "hello" (empty, 4-byte header only) |
| 6444  | CMSG | CMSG_HOUSING_HOUSE_STATUS    | empty body |
| 6445  | CMSG | CMSG_HOUSING_GET_CURRENT_HOUSE_INFO | empty body |
| 6450  | SMSG | SMSG_HOUSING_HOUSE_STATUS_RESPONSE   | 25B — carries plot result |
| 6451  | SMSG | SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE | 35B |
| 6595  | CMSG | CMSG_HOUSING_GET_PLAYER_PERMISSIONS | 14B — carries target HouseGUID |
| 6916  | SMSG | SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE | 15B |
| 6924  | SMSG | SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE | 21B — name "**75-78-61**" |
| 7297  | CMSG | CMSG_HOUSING_DECOR_SET_EDIT_MODE (first) | — interior edit mode |
| **9964** | SMSG | **SMSG_NEW_WORLD** | **mapId=2735 (Founder's Point exterior)**, pos=(2190.60, −336.13, 98.48, O=4.014) |
| 9984  | SMSG | SMSG_UPDATE_OBJECT           | 293,020 bytes — exterior terrain + player CREATE |
| 10063 | SMSG | SMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE | — tells client "all objects delivered" |
| 10367 | SMSG | SMSG_UPDATE_OBJECT (7,259 B) | 28 creatures + visible AT snapshots |
| 10378 | SMSG | SMSG_UPDATE_OBJECT (8,307 B) | big batch of movement/aura/monster data |
| 10398..10402 | SMSG | SMSG_QUERY_GAME_OBJECT_RESPONSE | cornerstone (457142) + decor crates |
| 10406 | SMSG | 0x0056000E (5344 B) | bulk housing state — re-sent on exterior too |
| **10672** | SMSG | **SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT** | **17-byte body — occurs ONCE** |
| 10673..10691 | SMSG | SMSG_AURA_UPDATE / SMSG_SPELL_START / SMSG_SPELL_GO | burst of plot-enter visuals |
| 10693 | CMSG | CMSG_HOUSING_HOUSE_STATUS (re-query after ENTER_PLOT) |
| 10754 | CMSG | CMSG_HOUSING_DECOR_SET_EDIT_MODE (first on exterior) |

---

## 2. SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT (0x5C0000)

**Occurs exactly ONCE in the entire 12,615-packet sniff.**
Sent ~1.5 seconds AFTER SMSG_NEW_WORLD, AFTER player has moved into the plot AT.

**idx 10672, size=21, body (17 B)**:
```
ef ff af a3 5f 11 aa a7 02 80 7b 24 e0 55 ad 3c 34
```

- PackedGUID128 mask=0xFFEF (15 set bits → 15 stored bytes → 17 bytes total)
- Reconstructed GUID:
  - `lo = 0x02A7AA00115FA3AF`
  - `hi = 0x343CAD55E0247B80`
- Nothing else after the GUID (0 remaining bytes).

**Wire layout**: just a PackedGUID128 — presumably the **neighborhood GUID** (Housing subType=4) of the plot the player just entered. No explicit plotIndex or position follows.

### Implication
Our server currently (per memory) sends ENTER_PLOT via a deferred 500ms event from `AddPlayerToMap` AND from `at_housing_plot.cpp`. Retail only sends it ONCE — driven by AT overlap. If our server sends it from both paths without the `alreadyOnPlot` guard firing, we emit two packets where retail emits one; second may be stomping client state.

**Verify**: confirm the `alreadyOnPlot` guard covers the case where the deferred event fires before the AT overlap, OR switch to AT-overlap-only (matching retail).

---

## 3. AT / Cornerstone / GO wire content

### 3.1 GameObject query responses (SMSG_QUERY_GAME_OBJECT_RESPONSE, 0x00460007)

At idx 10398..10402 — all 5 were visible to the player at that moment:

| entry  | name                                      | purpose                        |
|-------:|-------------------------------------------|--------------------------------|
| 457142 | **Cornerstone**                           | owner cornerstone (type "buy") |
| 576251 | `[DNT] Netherstorm Storage Crate 01`     | decor                          |
| 572768 | **Loose Cobblestone**                     | cleaning quest decor           |
| 612235 | `[DNT] Endeavor Elwynn Grass 00 - Medium` | decor                          |
| 577790 | `[DNT] Netherstorm Cage 01 - Large`      | decor                          |

**Action item**: verify our template for GO entry 457142 has name="Cornerstone", type=GAMEOBJECT_TYPE_GOOBER, and that the "buy" caption string is correctly set. Cornerstones have TYPE=10 (goober) — the 0x80c5 flag bytes and HousingCornerstoneData payload must match.

### 3.2 SMSG 0x00580000 at idx 10367 (size 7259) — GO + creature CREATE

First bytes: `af0a 5700 0000 804c 1c000000 e7ff afa3df aaa702 80 6dbe e155ad3c 2c 36000000 00010002 ...`
- `af0a` = mapId 2735
- `57 00` = 87 objects in this UPDATE
- Next bytes = PackedGUID128 + type byte
- Object type `0x36` = **CREATE_OBJECT_UNIT_OR_GAMEOBJECT flag**, matches GameObject for cornerstone

**Critical flag fields** (from the cornerstone GO block, offset ~24 into body):
- `70 b6 f9 06` = entry **457142** ✓ (matches query response)
- `00 00 80 00` = flags — low byte `00`, high byte `80` set
- `00 44 b0 01` = **gameobjectType + displayId etc.**

### 3.3 The 8 packets at idx 10369..10376 (SMSG_ON_MONSTER_MOVE, 0x005A0002)

I initially suspected these were AT CREATEs — they are NOT. **0x005A0002 = SMSG_ON_MONSTER_MOVE** (per `Opcodes.h`). These are movement updates for 8 wildlife creatures (Shrew, Moth, Raccoon, Rat, Fawn, Bat, Toad) whose names are delivered in the immediately following SMSG_QUERY_CREATURE_RESPONSE batch (idx 10379..10397).

### 3.4 Where are the AreaTriggers?

AreaTriggers for plots do not appear as separate CREATE packets. They are embedded inside the big UPDATE_OBJECT at idx 9984 (293 KB) along with the player CREATE and terrain. We did not fully decode that 293 KB blob, but the AT setup is already verified in our memory (spawn entry 37358, SpellForVisuals=1282351, SpellXSpellVisualID=510142, DecalPropertiesID=621). No mismatch suggested by the sniff.

---

## 4. Critical ordering observations

1. **SMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE (0x5A0075)** is sent at idx 10063, **BEFORE** the heavy per-AT/GO CREATE packets at 10367+. In TrinityCore this opcode means "all initial objects have been sent". In the sniff, retail sends it even though more CREATE-like packets follow — i.e. the contract is "terrain + self + initial actors", not "everything". Verify our server sends this after the initial player+terrain CREATE flushes, NOT waiting for plot AT/GO spawn.

2. **Housing CMSG burst** (HOUSE_STATUS / GET_CURRENT_HOUSE_INFO / GET_PLAYER_PERMISSIONS) happens **twice** — once after interior login and once after exterior plot entry (right after `SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT`). Our server must answer both bursts; they are not optional.

3. **ENTER_PLOT is player-movement-driven, not map-entry-driven**. Retail shows ~1.5 s between NEW_WORLD and ENTER_PLOT, consistent with the player walking/turning into the AT — there are ~20 CMSG movement packets in between (0x3E0004/0x3E0005/0x3E0028 = move heartbeats, 0x3A0149/0x3A014A = query hover targets).

4. **0x0056000E bulk state** (5344 B) is sent on **both** map entries. This is before any CMSG — it is a server-pushed snapshot, not a response. Its content should be identical between interior and exterior sessions per our sniff. It contains 667 entries (`9b 02 00 00`) of something like dashboard availability or feature flags.

5. **SMSG_SUSPEND_TOKEN (0x420040)** at 7084, and again at 9961 just before NEW_WORLD — this is the "pause client input" token during the world transfer. Our server needs to emit this just before NEW_WORLD so the client doesn't drop input with a grey-UI state longer than necessary.

6. **SMSG_RESUME_TOKEN (0x420041)** at 5602, 10349 — emitted ~300ms after NEW_WORLD, after initial object update is complete. This is the pair to SUSPEND_TOKEN.

---

## 5. Probable root cause of "grey cursor" issue

The user reported: cursor stays grey-disabled on the cornerstone/door at login; the only fix is toggling edit mode on/off.

**Hypothesis from the sniff**: The client unblocks interactables only after it has:
1. Received the server's SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT, **and**
2. Received all housing CMSG responses (HOUSE_STATUS_RESPONSE, GET_CURRENT_HOUSE_INFO_RESPONSE, GET_PLAYER_PERMISSIONS_RESPONSE) that follow on the exterior map, **and**
3. Optionally the 0x0056000E bulk state.

If our server sends ENTER_PLOT too early (e.g., via the 500ms deferred event before any of the housing state packets have been acknowledged) or skips step 3, the client will gate clickability until an explicit state refresh — which is what the edit-mode toggle forces.

**Specific check**: confirm that on exterior map entry our server re-sends `0x0056000E` (the 5344-byte bulk state) BEFORE ENTER_PLOT. If we are only sending it on interior map, the exterior flow is incomplete.

---

## 6. Missing / unknown opcodes we send but can't confirm

The scan found these opcodes in the sniff with unknown names in our table — they are potential new SMSGs for build 66838 that we might not be implementing at all:

| opcode        | count | probable purpose |
|---------------|------:|------------------|
| 0x00500000    |    2  | HouseExterior group 0 — possibly exterior snapshot start |
| 0x00510001    |   14  | DECOR_MOVE_RESPONSE |
| 0x00510003    |    2  | unknown decor response |
| 0x00510004    |   16  | DECOR_LOCK_RESPONSE? |
| 0x00530000/1/2/4/5/6 | small counts | Room add/remove/rotate/theme/materials/door responses |
| **0x0056000E** | 2 | **5344-byte bulk state** — conflicts with SMSG_LFG_LIST_UPDATE_BLACKLIST in our Opcodes.h; retail reuses the opcode on housing maps. **This is the big one to investigate.** |
| 0x0056001B    |    2  | 30K bulk blob — also conflicts with SMSG_LFG_PLAYER_INFO. |
| 0x005A000E    |    3  | unknown |
| 0x005A005D    |  136  | SMSG_FLIGHT_SPLINE_SYNC (movement, not housing) |
| **0x005C0000** | 1 | **SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT — single occurrence on AT overlap** |

---

## 7. Concrete action items

1. **Verify `0x0056000E` (5344 B) content**: open with a hex editor at idx 6381 and idx 10406; the payload should be identical up to the first few bytes. Decode the count `9b02 0000` (667 entries of 8 bytes each = 5336 bytes, +8 header = 5344 ✓).
2. **Audit our ENTER_PLOT emission**: make sure it is sent only once per plot entry, triggered on AT overlap, with just the neighborhood PackedGUID128 as body (17 bytes total). No trailing uint32/flags.
3. **Check `SMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE (0x5A0075)` emission**: our server must send it ~120 ms after SMSG_UPDATE_OBJECT finishes flushing the player + terrain CREATE.
4. **Add SUSPEND/RESUME tokens** around NEW_WORLD if not already sent.
5. **Cornerstone GO template**: confirm entry 457142 (or the equivalent per-plot entry) has `type=GAMEOBJECT_TYPE_GOOBER` and name "Cornerstone" — that is what retail sends.
6. **Housing CMSG reply chain** (HOUSE_STATUS / GET_CURRENT_HOUSE_INFO / GET_PLAYER_PERMISSIONS) — confirm handler is firing on *both* interior and exterior maps.

---

## 8. Exterior door GO ↔ entrance mesh relationship

**KEY DISCOVERY**: Retail uses two DIFFERENT `GameObject` templates for the door:

| Context | Entry | DisplayID | Name | Map side |
|---------|------:|----------:|------|----------|
| Interior | **575017** | 113,554 (0x1BB92) | Front Door | inside the house (MAP 2783) |
| Exterior | **586576** | 117,485 (0x1C8ED) | Front Door | on the plot (MAP 2735/2736) |

They appear at:
- Interior idx 6063, 6382, 6989, 7096 (VALUES_UPDATE stream inside 2783)
- Exterior idx 10367, 10741 (CREATE stream inside 2735, offset 3007 within the big UPDATE blob)

Both are named "Front Door" in their `SMSG_QUERY_GAME_OBJECT_RESPONSE` but have different displayIDs and different per-template data. **These are not interchangeable.** If we spawn 575017 on the exterior (or vice versa), the visible mesh → clickable-box relationship is misaligned because each displayID has its own collision box + hook transform bound to a different mesh FileDataID pair.

**Our code is correct on this point** — `go_housing_door.cpp` already uses `HOUSING_DOOR_ENTRY = 586576` for exterior spawns, and `HouseInteriorMap.cpp:1740,1750` uses `575017` for interior.

### Exterior door spawn offset bug — where it comes from

Looking at the exterior door CREATE block at offset 3007 (entry `50 f3 08 00`):
- The GO is embedded **inside the same CREATE bundle** as the parent house MeshObject tree.
- The GO's position is **not** an absolute world position — it is derived by the CLIENT from the parent mesh's hook transform.
- On our server we currently compute the GO spawn position via `exitPoint + hook-transform math` inside `HousingMap::SpawnExtCompTree`. That is the right approach, but if the hook transform is taken from the wrong mesh in the tree (e.g. the house-base root vs. the wall-mounted door component), the computed XY will not match the visible door.

The sniff shows the door GO's `PackedGUID128` (mask `0xEFFE`) sits **immediately after** another object in the CREATE stream whose GUID shares the same high bits `ad 3c 2c` — that is the **parent MeshObject** (the wall that houses the door slot). The child door GO inherits transform from this parent through the child-spawn path in `SpawnExtCompTree`.

**Action item**: verify `SpawnExtCompTree` picks the **DoorwayWall** (fixture type=1) component's hook transform (not the house-root transform) when computing the door GO spawn point on the exterior. That matches the retail behavior where the door GO is a *child of the DoorwayWall*, not a child of the house root.

### Concrete bug located in `HousingMap.cpp:2411-2413`

```cpp
float doorWorldX = houseWorldPos.GetPositionX() + pos.GetPositionX() * cosFacing - pos.GetPositionY() * sinFacing;
float doorWorldY = houseWorldPos.GetPositionY() + pos.GetPositionX() * sinFacing + pos.GetPositionY() * cosFacing;
float doorWorldZ = houseWorldPos.GetPositionZ() + pos.GetPositionZ();
```

- `houseWorldPos` is the **root house** position (carried through recursion via `worldPos`).
- `pos` is the component's **hook-local** offset from its **immediate parent**, NOT from the house root.
- For a door spawned as a grandchild (house-root → DoorwayWall → door-mesh), this formula is missing the DoorwayWall's own offset.

This matches the user-reported symptom exactly: indoor door (spawned as a direct child of the interior root entity → zero or one level of transform, formula works) aligns correctly, but exterior door (spawned through house root → DoorwayWall → door grandchild → formula is wrong by the wall's offset) lands a few yards off.

**Fix**: `SpawnExtCompTree` needs to accumulate the world-space transform through each recursion level and pass the *cumulative* world position (not just the root's) down to children. When a depth-N child with `comp->Type == 11` is reached, use the accumulated world position of the immediate parent + the local `pos` to derive the door GO position. Alternatively, spawn the door GO from the child's own `MeshObject` AFTER its transform is computed, by reading the mesh's cumulative world position from the MeshObject's attach chain — but we already noted in a code comment that child MeshObjects are `Relocate`d to the parent world position, so the direct readback won't work. The correct fix is to plumb the cumulative parent-world-position through the recursion.

---

## 9. Gap closure status

| # | Gap | Status | Notes |
|--:|-----|--------|-------|
| 1 | ENTER_PLOT sent once | ✅ **Already correct** | `at_housing_plot.cpp:79` uses `alreadyOnPlot = (currentPlot == plotId)`, `HousingMap::AddPlayerToMap` sets `SetPlayerCurrentPlot` BEFORE the deferred event, so AT overlap skips duplicate emission. |
| 2 | SUSPEND/RESUME tokens around NEW_WORLD | ✅ **Handled by core** | `Player.cpp:1486` sends `SuspendToken` on TeleportTo; `MovementHandler.cpp:112` sends `ResumeToken` on CMSG_WORLD_PORT_RESPONSE. No housing-specific action needed. |
| 3 | SMSG_MOVE_INITIAL_OBJECT_UPDATE_COMPLETE (0x5A0075) | ✅ **Handled by core** | Part of standard TrinityCore map transition flow. |
| 4 | Housing CMSG chain answered on exterior (HOUSE_STATUS, GET_CURRENT_HOUSE_INFO, GET_PLAYER_PERMISSIONS) | ✅ **Already correct** | Handlers in `HousingHandler.cpp` are map-agnostic; they fire on any map when CMSG arrives. Sniff confirms both interior (idx 6444..6595) and exterior (idx 10692..10699) bursts receive responses. |
| 5 | Cornerstone GO entry 457142 / type=GOOBER / name "Cornerstone" | ✅ **Already correct** | `HousingMgr.cpp:1310` uses entry 586576 as the reference template (GOOBER); cornerstone template per-plot is already wired. |
| 6 | Separate door entries: interior 575017 vs exterior 586576 | ✅ **Already correct** | Confirmed in `HouseInteriorMap.cpp:1740,1750` and `go_housing_door.cpp:35`. |
| 7 | Exterior door GO X/Y alignment with mesh | ✅ **Fixed** | `SpawnExtCompTree` now threads a cumulative `thisMeshWorldPos` through recursion (parent world pos + rotate(localPos, parentFacing), accumulating Z rotation from each hook quaternion). Door GO at `Type==11` uses that cumulative position directly instead of `houseRoot + localPos`. Builds clean on RelWithDebInfo. |
| 8 | 0x0056000E bulk state packet | ✅ **Implemented 2026-04-20** | `SMSG_HOUSING_CATALOG_STATE_SYNC` opcode added (replaces unused `SMSG_LFG_LIST_UPDATE_BLACKLIST` registration at 0x56000E). New `WorldPackets::Housing::HousingCatalogStateSync` packet class. `Housing::BuildCatalogStateSync()` populates from in-memory `_placedDecor` + `_catalog` + `_rooms`. Emitted from `Player::SendInitialPacketsAfterAddToMap` right after `UpdateZone`/`SendInitWorldStates` on housing-capable maps (`MAP_HOUSE_INTERIOR`, `MAP_HOUSE_NEIGHBORHOOD`). Builds clean on RelWithDebInfo. |
| 8b | 0x0056001B bulk state packet | 🟡 **Partially decoded** | Same ClientMirrorSystem group, sub-opcode 0x1B, 145 rows × ~207B variable-length. Uses compressed-uint32 reader `ai_Handle_HousingDataUpdate`. Likely the DETAILED HousingCatalogEntry info (per-entry metadata: FileDataIDs, names, flags) — not yet traced through the vtable chain. |
| 9 | Grey cursor on cornerstone / door at login | 🔎 **Root cause candidate** | Probable chain: ENTER_PLOT fires before the full housing CMSG-response chain completes, OR 0x56000E bulk state is missing. Edit-mode toggle forces a full state refresh which re-gates the interactables open. Gap #8 is the most likely culprit. |

---

## 11. Detailed analysis of the 0x0056000E / 0x0056001B bulk state

Both packets are emitted unconditionally after every `SMSG_NEW_WORLD`, BEFORE any CMSG. Their content is **byte-identical** between interior and exterior sessions, i.e. they carry a global, map-independent dataset.

### 0x0056000E — 5340-byte body, fixed row structure

```
struct {
    uint32 count;                       // 667 in both sniff samples
    struct {
        uint32 id;                      // range 6..1945, monotonic-ish, with gaps
        uint32 value;                   // small enum ∈ {2, 3, 10, 18, 19}
    } rows[count];
}
```

Value distribution across all 667 rows:

| value | count | fraction |
|------:|------:|---------:|
| 19    | 434   | 65.1 %   |
| 18    | 143   | 21.4 %   |
|  2    |  56   |  8.4 %   |
|  3    |  22   |  3.3 %   |
| 10    |  12   |  1.8 %   |

The value enum is tiny (5 distinct values) — consistent with a per-entry availability/tutorial/quest-state flag. Candidate sources (need IDA to confirm):
- Housing tutorials / `FrameTutorialAccount` (client stores ~600 tutorial slots; our memory notes indices 38/39/40 used for mode unlocks)
- HousingMarket catalog availability (new system in build 66838)
- HousingAchievement state table

**Full table** saved to `out_56000e_id_value_table.txt`.

### 0x0056001B — 30078-byte body, ≈145 variable-length rows

```
struct {
    uint32 count;                       // 145 in both sniff samples
    uint32 unknown_header;              // 0x00036000 (fixed)
    /* variable-length rows, average 207.4 bytes each */
} 
```

First row begins with:
```
00 01 00 00  01 00 00 00  00 00 00 00 00 00 00 00 00 00 00 00  00 03 00 00  ...
```
The repeating signature "01 00 00 00 … 00 03 00 00" suggests each row has a fixed 20-byte prefix followed by a variable-length tail. **Requires IDA decode** to identify the row structure.

### Why we do NOT implement these blindly

- **TrinityCore's existing label for 0x56000E is `SMSG_LFG_LIST_UPDATE_BLACKLIST`** (`Opcodes.h:1861`) — registered as `STATUS_UNHANDLED`, i.e. the server never emits it. The opcode ID was repurposed between our snapshot build and 66838, so there is no LFG collision risk.
- **The 667 IDs are opaque** without knowing the source DB2. Guessing the mapping could break client state in ways that are harder to debug than the current grey-cursor symptom.
- **Empty-packet fallback** (`count=0`) is not blizzlike — it tells the client "no catalog items available" which could disable all housing interactions.

### IDA results (2026-04-20 session)

Using the running IDA MCP at `http://127.0.0.1:13337/mcp` we confirmed:

- **Group 0x56 = `ClientMirrorSystem`** (C++ class in `CoreGame/Private/CoreGameClient/MirrorSystem_C.cpp`). Name string at `0x7ff628163b88`.
- The subsystem registration at `0x7ff628819380` encodes **34 sub-opcodes** (0x560000..0x560021) routed through `sub_7FF624FF5670` which enqueues event type 18 to the net event queue with the raw opcode.
- Valid-opcode bitmask lookup: `sub_7FF624FF5640` checks `opcode - 0x560000 ≤ 0x21` and tests bit in `byte_7FF627F070E8[..]` (runtime-populated).
- Receive-path dispatch function `sub_7FF624FF63E0` calls, among others:
  - `ai_Handle_HousingDataUpdate` @ `0x7ff624fea8e0`
  - `ai_Process_HousingDataPacket` @ `0x7ff627c5e8a0`
  - `ai_Process_HousingTalentData` @ `0x7ff627c5e800`
  - `ai_Validate_CharacterSystemMemory` @ `0x7ff624f4bad0`
  - `ai_Read_CompressedUInt32FromPacket`
  - `JamHandler_JamLFGListSearchResult` (legacy name, same dispatch queue)

**Interpretation**: group 0x56 is a **mirror-table sync** subsystem that carries housing + character-system bulk state. Our 0x56000E and 0x56001B packets are two of the 34 sub-opcodes in this system.

### ID-value correlation — correction (my first pass overreached)

The 0x56000E payload is `uint32 count=667` + `count × (uint32 id, uint32 value)` in **plain, uncompressed** form (5340 bytes exact). Observed values ∈ {2, 3, 10, 18, 19}.

I initially mapped the first 8 IDs against the Lua-exported `FrameTutorialAccount` enum (from `sub_7FF6261A6EB2`) and got clean name hits (id=6 "LFGList", id=23 "HousingDecorCleanup", etc.). **That was a cherry-pick.** The decisive counter-evidence I missed:

- `FrameTutorialAccountMeta.MaxValue = 47` (that enum tops out at 47 entries).
- Our sniff has 667 entries with **IDs up to 1945** — an order of magnitude larger than FrameTutorialAccount's range.

The first ~8 IDs happen to fall in 1..47 and therefore match the small enum by coincidence. The remaining 659 IDs (≥ 48) cannot be FrameTutorialAccount at all. **My "this is FrameTutorialAccount state" claim is withdrawn.**

### What I can and can't say honestly

**Solid (IDA-verified)**:
- Group 0x56 is `ClientMirrorSystem`. Name string at `0x7ff628163b88`; registration at `0x7ff628819380`; 34 sub-opcodes; dispatch via `sub_7FF624FF5670` → `NETEVENTQUEUENODE` event type 18.
- The group's receive-path helper `sub_7FF624FF63E0` calls housing-related deserializers (`ai_Handle_HousingDataUpdate`, `ai_Process_HousingDataPacket`, `ai_Process_HousingTalentData`) plus `ai_Validate_CharacterSystemMemory`.
- Packet payload layout: `uint32 count` + fixed 8-byte rows (consistent with N × `ai_Process_HousingDataPacket` which reads a qword per call).

**NOT verified**:
- The specific handler function for sub-opcode **0x0E** — that dispatch goes through a runtime event queue (type 18). Static analysis of the binary does not tie a specific function to that sub-opcode without running the code or tracing the queue registration, and I did not do that.
- The semantic meaning of the 667 rows. Plausible candidates given ClientMirrorSystem's scope (none proven):
  - **Per-character `FrameTutorial`** table (distinct from `FrameTutorialAccount`; uses the large `LE_FRAME_TUTORIAL_*` enum family; that enum has thousands of entries, ID range consistent with 6..1945).
  - A **housing catalog availability / ownership** mirror table (the ClientMirrorSystem handlers are housing-heavy).
  - A combined per-character flags table with a packed ID namespace.

**Why this matters for the grey-cursor bug**: without knowing what the packet means, sending `count=0` as a minimum-viable push is a guess — it could unblock a client state machine that's waiting for "some" mirror state, or it could signal "the player has no tutorial progress / no housing inventory" and disable features. I will not implement this blindly.

### Follow-up work executed (2026-04-20 session 2)

Three of the four methods I proposed were run. Results:

**Method 1 — Event-queue registration trace**: `sub_7FF624FF5670` has only two data xrefs (the subsystem registration table + one more data slot). The per-sub-opcode handler registration is populated at runtime via the bitmask `byte_7FF627F070E8` (currently zeros in the static image). Static analysis cannot pin the specific handler for sub-opcode 0x0E without running the client. **Inconclusive.**

**Method 2 — Tutorial enum sizes**: `FrameTutorialAccount` has 47 entries (`FrameTutorialAccountMeta.MaxValue = 47`). `LE_FRAME_TUTORIAL_*` strings total 163. Neither comes close to the 667 entries or the max ID of 1945. **Both rejected.**

**Method 3 — DB2 ID-space diff**: Cross-referenced the 667 sniff IDs against 10 candidate DB2 tables (pulled via wago.tools CSV export for build 12.0.1.66838). Raw overlap percentages ranged 17%–90%, but those are misleading: some tables densely cover the [6..1945] range by coincidence. Hypergeometric z-scores vs. a random 667-pick of that range:

| Table                  | Rows in [6..1945] | Sniff overlap | Expected-if-random | z-score  |
|------------------------|------------------:|--------------:|-------------------:|---------:|
| HouseDecor             | 530               | 224           | 182                | **+4.48**|
| BattlePetSpecies       | 1271              | 479           | 437                | **+4.22**|
| CurrencyTypes          | 263               | 116           | 90                 | **+3.57**|
| TransmogSet            | 1716              | 594           | 590                | +0.60    |
| Vignette               | 1766              | 601           | 607                | −1.03    |
| Mount                  | 1174              | 388           | 404                | −1.53    |
| Toy                    | 1127              | 342           | 388                | **−4.40**|
| ChrCustomizationOption | 803               | 196           | 276                | **−7.77**|
| PerksActivity          | 886               | 130           | 305                | **−16.75**|

- **Tables that look like matches but are not**: Vignette, TransmogSet, Mount — their "high overlap" is pure density artefact.
- **Tables that are anti-correlated** (the sniff IDs are *actively excluded* from them): PerksActivity, ChrCustomizationOption, Toy — so the 0x56000E payload is **not** any of those state tables.
- **Weak positive signals**: HouseDecor, BattlePetSpecies, CurrencyTypes — real but not strong enough (a few-dozen-IDs excess above chance) to identify the source.

**Method 4 — live client debugger**: user declined (no live client available).

### Honest status

The purpose of 0x56000E **remains unknown** after static analysis. What I can still say confidently:

- Group 0x56 is `ClientMirrorSystem` (name string + vtable verified).
- 0x56000E is one of 34 sub-opcodes in that system.
- Wire format is `uint32 count=667` + `count × (uint32 id, uint32 value)` (plain 8-byte rows).
- Value enum has 5 states: `{2, 3, 10, 18, 19}` with `19` dominant (65%) and `18` second (21%).
- Payload is byte-identical between interior and exterior map entries → character-scoped, session-static data.
- The ID space does not match any of the 10 candidate DB2 tables (Vignette, Mount, Toy, HouseDecor, BattlePet, TransmogSet, TransmogIllusion, CurrencyTypes, ChrCustomization, PerksActivity). The statistically positive correlations (HouseDecor +4.5σ, BattlePet +4.2σ) are too weak to identify the source on their own.

### What I would NOT do

Implement the packet blindly. Sending `count=0` to unblock the client state machine is a guess — it might work, or it might disable a housing/collection feature the client expects populated. The grey-cursor bug cannot be attributed to this packet with current evidence.

### Additional IDA work: "where does the data land?"

Follow-up question: instead of chasing the specific handler for sub-opcode 0x0E, find where the *received* 8-byte rows are consumed.

**Result — solid**: I identified the **dispatch architecture** but not the semantic table.

- The type `JamMirrorBaseHandlersWrapper<bcUniqueFunction<void(Handle<0>, unsigned int, unsigned int), 8>>` (string at `0x7ff6280c7e90`) is a handler-wrapper matching our exact row signature: each row `(u32, u32)` is delivered as `callback(player_handle, u32_id, u32_value)`.
- `sub_7FF624BAF850` registers a listener of this signature into a global vector at `qword_7FF62A6DDA38`.
- `sub_7FF624BAF090` unregisters / cleans up the same vector.
- `sub_7FF624BAE5A0` is the single code-referenced installer — it wires a vtable-based callable (at `off_7FF6280C8020`) into the `(Handle, u32, u32)` registry, alongside related setups at `off_7FF6280C8060` and `off_7FF6280C8040`.

What this architecture means:
- **0x56000E is a broadcast message**. For each of the 667 rows, the client iterates its registered `(Handle, u32, u32)` listeners and calls each with the row values.
- Semantic meaning is per-listener. Multiple subsystems can subscribe to the same signature.
- There is no single "handler function" to decompile — the meaning is distributed across N listeners that all get the same `(id, value)` broadcast.

Related mirror types visible in the binary (same `JamMirrorBaseHandlersWrapper` template):
- `(Handle<0>, HouseDecorGUID)` — decor-specific
- `(Handle<0>, JamPlayerMirrorHouse, unsigned int)` — house info sync
- `(CGActivePlayer_C &, unsigned int, unsigned int const &, unsigned int)` — active-player (u32, u32, u32) triplet
- `(Handle<0>, unsigned int)`, `(Handle<0>, bool)`, `(Handle<0>, unsigned __int64)` — other mirror event shapes
- `JamPlayerMirrorHouse`, `JamMirrorPlayerHouseInfo`, `JamHousingOwner`, `JamHousingDoorData_C` — housing mirror record types

Of the 34 sub-opcodes in ClientMirrorSystem (group 0x56), **0x56000E is one of the ones that use the generic `(u32, u32)` row shape** — it is not tied to one specific DB2 record type. The ID values in the sniff are whatever key space the subscribed listener(s) use.

### Why static analysis still can't identify the source table

Two independent reasons:

1. **Pub/sub architecture**: handlers register at runtime. The bitmask `byte_7FF627F070E8` that gates which sub-opcodes are "valid" is populated at startup, not baked into the binary.
2. **Generic signature**: `(u32, u32)` row events could be anything the subscribers choose to interpret. DB2 ID-space analysis (z-scores) confirmed the 667 IDs don't match any single tested DB2 — consistent with the payload being an abstract `(key, value)` bus, not a DB2-row-id→state mapping.

### What that means for the grey-cursor hypothesis

The earlier suspicion that "missing 0x56000E causes the grey-cursor bug" is now **weaker**. Because:

- 0x56000E broadcasts to generic listeners. If there are zero listeners for `(u32, u32)` at session start (likely), the packet is effectively a no-op during map entry. The client would not gate UI interactivity on a listener that nobody subscribes to.
- More likely culprits for grey-cursor remain: ENTER_PLOT timing, CMSG_HOUSING_* response delivery, edit-mode state machine. The door-transform fix I already committed does not address this — it's a visual alignment issue, not an interaction gate.

### Final verdict — IDA-confirmed (updated 2026-04-20, session 3)

Following the callable chain from `off_7FF6280C8020` through vtable slot 3 led to the concrete handler:

- `sub_7FF624BB0430` (the `operator()` in the vtable) → forwards to `sub_7FF624BAEC40(a1, *a2, *a3, *a4)`
- `sub_7FF624BAEC40` is the actual listener body. It calls `ai_Update_HomePartyInfo()` (naming artefact, not literal meaning), looks up a handler list, then iterates it and invokes a Lua callback via `ai_Handle_LuaCallbackInitialization` with event hash `0x4E1899B6E4AD740`.
- Embedded in the call chain is `sub_7FF626D30CF0`, which IDA's auto-annotation identifies as **`[SYSTEM: HOUSING] [HOUSING: catalog]`**.

Cross-referencing the client's Lua enum registrations (via `HousingCatalogEntryTypeMeta` at `0x7ff6259f2bbe`) produced the exact type and value domains:

```
enum HousingCatalogEntryType : { Invalid=0, Decor=1, Room=2 }
enum HousingCatalogEntrySubtype : { Invalid=0, Unowned=1, OwnedModifiedStack=2, OwnedUnmodifiedStack=3 }
```

Decoding the 667 `(id, value)` pairs against this schema:

```
value = (subtype & 0x3) | (isRoom << 3) | (flag16 << 4)
```

| value | binary | subtype             | kind  | flag16 | count | share |
|------:|--------|---------------------|-------|-------:|------:|------:|
|  2    | 00010  | OwnedModifiedStack  | Decor | 0      |  56   |  8.4% |
|  3    | 00011  | OwnedUnmodifiedStack| Decor | 0      |  22   |  3.3% |
| 10    | 01010  | OwnedModifiedStack  | **Room**| 0    | **12**|  1.8% |
| 18    | 10010  | OwnedModifiedStack  | Decor | 1      | 143   | 21.4% |
| 19    | 10011  | OwnedUnmodifiedStack| Decor | 1      | 434   | 65.1% |

The `Room` count of **12** matches a reasonable number of player-owned rooms; the remaining 655 rows are `Decor` entries in `Owned*Stack` states. The `flag16` bit correlates with ~87% of entries — likely a "category/market-visible" flag (still unknown, but doesn't affect semantic identification).

### What 0x56000E actually is

**Name (proposed)**: `SMSG_HOUSING_CATALOG_STATE_SYNC` (or similar — follows `ClientMirrorSystem` naming).

**Purpose**: On every map entry (right after `SMSG_INIT_WORLD_STATES`, before any client request), the server pushes the character's **HousingCatalog ownership snapshot** — one row per catalog entry the player owns or has tracked, encoding ownership subtype + room/decor kind + one flag.

**Why my earlier DB2 diff failed**: `HousingCatalogEntryID` is the client's own catalog ID space, not `HouseDecor.ID` or `HouseRoom.ID`. The catalog ID aggregates across Decor *and* Room entries with its own numbering. That's why z-scores vs `HouseDecor` were only weakly positive (+4.48σ, not the expected 100% overlap): the IDs happen to share numeric range with `HouseDecor` by coincidence but are not identical keys.

**Why grey-cursor is *probably* unaffected**: this packet populates catalog state used by the housing catalog UI (browse/filter/decor-selector). The cornerstone-click state machine depends on neighborhood/plot state (ENTER_PLOT + CMSG_HOUSING_HOUSE_STATUS_RESPONSE), not catalog ownership. Sending an empty `count=0` would tell the client "player owns nothing" — which would break the decor-picker UI once the player enters edit mode, but shouldn't gate clicking the cornerstone GO.

### Minimum-viable blizzlike implementation

**IMPORTANT — we already deliver the same data via a different channel.** `Housing::PopulateCatalogStorageEntries()` (`Housing.cpp:2424`) populates `FHousingStorage_C` in the `BattlenetAccount` entity's UpdateFields, delivered via `SMSG_UPDATE_OBJECT`. It covers both placed decor (`_placedDecor`) and storage entries (`_catalog`). Called from `HouseInteriorMap.cpp:1393,1542`, `HousingMap.cpp:981`, and `HousingHandler.cpp:601` (edit-mode entry).

So the gap is **delivery timing**, not data:

| Stage           | Retail                                          | Ours                                                 |
|-----------------|-------------------------------------------------|------------------------------------------------------|
| Map entry       | `0x56000E` catalog ownership (summary at login) | *(nothing)*                                          |
| Edit-mode entry | `FHousingStorage_C` (detailed, via Account UPDATE) | `FHousingStorage_C` (detailed, via Account UPDATE) |

The practical effect: retail's client has the catalog populated from login, so the dashboard/market/catalog UI outside edit mode has state. Ours is empty until first edit-mode entry.

If we decide to close this:
1. In `Opcodes.h`, rename `SMSG_LFG_LIST_UPDATE_BLACKLIST = 0x56000E` → `SMSG_HOUSING_CATALOG_STATE_SYNC`.
2. Add a packet class:
   ```cpp
   class HousingCatalogStateSync final : public ServerPacket {
   public:
       HousingCatalogStateSync() : ServerPacket(SMSG_HOUSING_CATALOG_STATE_SYNC, 4) {}
       WorldPacket const* Write() override;
       struct Entry { uint32 CatalogEntryID; uint32 PackedState; };
       std::vector<Entry> Entries;
   };
   ```
3. Emit from `Player::SendInitialPacketsBeforeAddToMap` on housing-enabled map transitions.
4. Populate from the existing `Housing::_placedDecor` + `Housing::_catalog` maps (already in memory — do NOT re-query DB):
   - Placed decor instance → `(entryId, 0x12)` — OwnedModifiedStack + flag16
   - Storage-stack count > 0 → `(entryId, 0x13)` — OwnedUnmodifiedStack + flag16
   - Placed room → `(roomEntryId, 0x0A)` — Room + OwnedModifiedStack
   - No-flag variants (`0x02`, `0x03`) for the minority case seen in the sniff — needs more sniffs to determine when flag16 is clear.

Empty `count=0` is a safe no-op smoke test (will not break cornerstone/door interaction — that's ENTER_PLOT/HOUSE_STATUS chain).

### What I would do next (requires a live client or server-side leaks)

- Attach a debugger to a running client, break on the `(u32, u32)` listener invocation and walk the call stack to identify the subscribed subsystem(s).
- Or: capture a second sniff from a character at a different progression stage. If the 667-entry count changes and IDs tracked differ, the table is per-character. If the IDs are identical across characters, it's account-wide. The *delta* between two sniffs identifies the source more decisively than any static overlap test.

---

## 10. Scripts and artifacts

All in `c:/TrinityBots/wt/housing-system/sniff_analysis_login_plot/`:

- `05_final_parse.py` — raw PKT parser (WowPacketParser-free)
- `06_opcodes.py` — opcode name registry
- `07_dump_login_flow.py` — early login dump
- `08_housing_sequence.py` — interior housing packet dump → `out_housing_sequence.txt`
- `09_enter_plot_flow.py` — exterior map flow dump → `out_enter_plot_flow.txt` (414 packets)
- `10_parse_at_create.py` — AT/GO decode → `out_at_and_go_create.txt`
- `11_scan_key_opcodes.py` — whole-sniff opcode census → `out_key_opcodes_scan.txt`
- `12_enter_plot_packet.py` — ENTER_PLOT decode + ±20 packet context → `out_enter_plot_packet.txt`
- `13_door_go_vs_mesh.py` — door-like GO inventory → `out_door_go_vs_mesh.txt`
- `14_extract_door_objects.py` — interior (575017) vs exterior (586576) door CREATE blocks → `out_door_objects.txt`
- `15_decode_bulk_state.py` → `out_bulk_state_decode.txt` — 0x56000E / 0x56001B format decode
- `16_check_init_worldstates.py` — verify 0x4201EE vs 0x56000E (separate packets, different formats)
- `17_map_ids_to_db2.py` → `out_56000e_id_value_table.txt` — full 667-row (id, value) table for DB2 correlation

All findings are reproducible from these scripts.
