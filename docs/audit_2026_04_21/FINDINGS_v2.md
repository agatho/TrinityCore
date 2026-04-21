# Housing System Audit v2 — Corrected Findings

**Date:** 2026-04-21 / 22 (updated 23:xx)
**Build:** 12.0.1.66838

## Correction to v1

The v1 FINDINGS.md claimed "we emit 15 entities vs retail's 494" based on analysing only idx 295 of our sniff. That was **wrong**. Our server actually splits the initial bundle across two UPDATE_OBJECTs:

- **idx 295 (57 KB)**: 15 entities — Player CREATE + session housing entities + proxies + Housing/4 mirror
- **idx 310 (552 KB)**: 1216 entities — grid visibility dump

**Total: 1231 entities**, which is **2.5× MORE than retail** (494 in one packet).

Retail bundles everything into one UPDATE_OBJECT; we split into two.

## Corrected role inventory

| Role | Retail (idx 9984) | OUR idx 295 | OUR idx 310 | OUR total |
|------|-------------------|-------------|-------------|-----------|
| Player/ActivePlayer | 1 | 1 | 0 | 1 ✓ |
| Housing/3 (identity) | 46 | 3 | 0 | 3 |
| Housing/4 (nbh mirror) | 1 | 1 | 0 | 1 ✓ |
| Items (inventory) | 353 | 8 | 0 | 8 |
| GameObjects | 51 | 0 | **1009** | **1009 (20× retail)** |
| Creatures | 24 | 0 | **186** | **186 (8× retail)** |
| AreaTriggers | 2 | 0 | 18 | 18 (plot ATs) |
| MeshObjects | 3 | 0 | 3 | 3 ✓ |
| Entity mirrors | 8 | 0 | 0 | 0 (new code not yet deployed) |

Retail per-plot entity count ratio is clearly lower — retail sends only entities within visibility range of the player's spawn position. We send the entire map.

### Why we over-send

`HousingMap::InitVisibilityDistance` sets `m_VisibleDistance = MAX_VISIBILITY_DISTANCE` (~533 yards, effectively "entire map"). The comment says:
> Housing neighborhoods are small, self-contained maps where ALL entities are relevant to every player.

In retail, that assumption is wrong — the client does care about distance for grid visibility. The 533-yard visibility pulls in 1009 GOs (trees, fences, background scenery) that the client has to process for every login.

### Potential fix — bounded visibility

Reducing `m_VisibleDistance` to ~100-150 yards would:
- Cut our idx 310 payload from 552 KB to ~50 KB (closer to retail)
- Reduce client CPU load at map entry
- Keep all essential entities visible (plot ATs are within ~50 yards of any spawn position)

Risks:
- Plot ATs for distant plots might not be visible at login; client would only see them as player approaches. Check whether this breaks the world-map icon picker (which indexes all plots via `GetNeighborhoodMapData`).
- Our deferred ENTER_PLOT callback references the player's own plot AT; own plot is always in range.

**Recommendation:** prototype with 150 yards, sniff-diff, iterate.

## Real deviations (beyond v1 bundle-count red herring)

### 1. HOUSE_STATUS_RESPONSE wire format — STRUCTURAL DIFFERENCE

**Our code** writes:
```cpp
_worldPacket << HouseGuid;        // PackedGuid
_worldPacket << AccountGuid;      // PackedGuid
_worldPacket << OwnerPlayerGuid;  // PackedGuid
_worldPacket << NeighborhoodGuid; // PackedGuid
_worldPacket << uint8(Status);
_worldPacket << uint8(FlagByte);
```
Produces 24 bytes typical.

**Retail wire (2 samples from idx 9984):**
- 21 bytes: `HouseGuid (9B PackedGuid) + AccountGuid (6B PackedGuid) + 6B zeros`
- 27 bytes: `HouseGuid (9B) + AccountGuid (6B) + ???PackedGuid(8B) + 4B zeros`

**Deviation:** retail does NOT send `OwnerPlayerGuid` + `NeighborhoodGuid` as distinct PackedGuids. Our `IDA 0x550000: PackedGUID×4 + uint8 + uint8` assumption appears to be wrong — sniff shows PackedGUID×2 plus a variable-size trailing section (6 or 8 bytes).

**Impact:** the client may read our extra PackedGuids as garbage fields, producing incorrect "status" / "flag" values at wrong offsets → wrong editor-mode state.

**Risk of fixing now:** HIGH. The structure interpretation needs proper IDA verification before rewrite. Our code has been shipping 4 GUIDs for weeks — blindly dropping two could break things that were compensating. **Defer until IDA re-reads `sub_0x550000` / the client's `HandleHousingHouseStatusResponse`.**

### 2. ENTER_PLOT GUID targets different entity type

Retail's `SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT` wire:
- 17 bytes: `PackedGuid(15B data)` with hi top-byte `0x34` → **HighGuid 13**
- Our 10-byte wire has hi top-byte `0x34` → same HighGuid 13? (we set it to `plotAt->GetGUID()` which is HighGuid=11 AreaTrigger)

Wait — decoding our hi: `0x34_00_05_55_e0_24_7b_80` → bits 58-63 = 0x34 >> 2 = 13. So our GUID IS HighGuid 13.

But HighGuid 13 in our `ObjectGuid.h` is `Conversation` (per line 234). Neither we nor retail are using a classic AreaTrigger (HighGuid 11) for ENTER_PLOT — both use HighGuid 13.

That's consistent — no deviation here. Just a reminder that our plot AT implementation uses something other than classic HighGuid::AreaTrigger.

**No action required.**

### 3. Plot AT count: ours 18, retail 2

Retail sends only 2 ATs in the map-entry UPDATE_OBJECT; our strict parser found 18. Retail likely filters to ATs near the player; we send all 55 plot ATs (we found 12 with `FHousingPlotAreaTrigger_C` in idx 310, plus 6 others).

Related to visibility distance fix (see above).

### 4. Creature count: ours 186, retail 24

Same pattern — our visibility is too broad. 186 creatures = ambient wildlife across the entire map; retail only loads those near the player.

Related to visibility distance fix.

## Changes committed in this audit

| Commit | Change |
|--------|--------|
| 7a8be9e015 | HousingMirrorEntity class skeleton (un-integrated) |
| 4262499682 | HousingMirrorEntity wired into map lifecycle + Player CREATE bundle |
| e263ae9ee5 | SMSG_HOUSING_CATALOG_STATE_SYNC emission from AddPlayerToMap + MIRROR_VARS backfilled (40 more vars) |

Binary rebuilt successfully. Worldserver PID 66124 still running the pre-audit binary; user must restart to deploy.

## Remaining work (prioritised)

### Must-do (next session)

1. **Deploy the new binary** and capture a fresh sniff.
2. **Verify mirrors appear on the wire** — expect 4+ Entity (57) CREATEs in the map-entry UPDATE_OBJECT.
3. **Verify CATALOG_STATE_SYNC arrives** — expect a new SMSG 0x0056000E packet in the map-entry phase.
4. **Visual test in-game** — does the own-plot icon now resolve to "Your House" / yellow spiral?

### Should-do (when confirmed stable)

5. **Tighten `HousingMap::InitVisibilityDistance`** to 150 yards. Sniff-diff to verify the GO/Creature counts drop without breaking plot AT coverage.
6. **IDA-re-verify `HOUSE_STATUS_RESPONSE` wire format** (`sub_0x550000`) and fix our packet writer to match sniff.
7. **Reduce over-emission** of `HOUSE_STATUS_RESPONSE` + `GET_PLAYER_PERMISSIONS_RESPONSE` from 5 to 2.
8. **Spawn ambient creatures** on housing maps for visual parity (if sniff shows same creature entries retail uses).

### Nice-to-have

9. **Fix the 62 unparsed blocks** in our idx 310 — the strict parser can't navigate past some Unit entities with `MovementUpdate` splines.
10. **Explore Group-B mirrors** (per-MeshObject `FMirroredPositionData_C` proxies) — retail has 4 of these; we have 0.
11. **Finish backfilling MIRROR_VARS** (40 added; retail still has 36 more we skipped — shop2 URLs + Pinterest API keys; low-impact).
