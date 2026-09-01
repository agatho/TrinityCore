# Housing — deferred findings for a focused feature-branch session

Handed off from the `integration/all-systems` overnight bug-hunt (2026-07-31). These housing bugs were **adversarially verified** (found → independently confirmed) but deliberately **NOT fixed in integration** because housing is complex enough to warrant a focused session in this branch (`feature/housing-system`), with a live tester to confirm nothing legitimate breaks.

Line numbers below are for THIS worktree (`I:/TrinityCore/housing-system/TrinityCore`) as of handoff — verify by symbol, they drift. All four also exist in integration; fix here first, then it flows to integration on the next fold.

---

## Cluster A — visitor-can-edit-another-player's-plot (griefing / authorization gap)

Three sibling handlers apply an edit to `housing->GetPlotIndex()` on `player->GetMap()` **with no check that the player is on / owns that plot on the current map**. `housing` is the *acting player's own* Housing object, but its plot index is applied to whatever map the player is standing on. While **visiting another player's (public) neighborhood**, the same plot index addresses a **different** house, so a visitor mutates the host's plot.

- **#7 `HandleHousingDecorPlace`** (`HousingHandler.cpp:865`) — spawns the decor MeshObject at `housing->GetPlotIndex()` on the current map with no on-own-plot check → a visitor spawns their decor onto someone else's plot.
- **#8 `HandleHousingFixtureCreateFixture`** (`HousingHandler.cpp:1798`) — same pattern for a fixture edit.
- **#9 `HandleHousingFixtureDeleteFixture`** (`HousingHandler.cpp:1928`) — despawns the mesh (+ door GO) at `housing->GetPlotIndex()`+hook on the current map → a visitor removes another player's exterior fixtures.

**Failure scenario:** Player B opens Player A's public neighborhood, sends `CMSG_HOUSING_DECOR_PLACE` / fixture create/delete. B's own `GetPlotIndex()` resolves to A's plot on A's map → B edits A's house.

**Why deferred / what it needs:** there is currently **no clean "is the player on / owner of this plot on this map" check**. `HousingMap`/`HouseInteriorMap` don't expose owner or neighborhood guid (only `GetPlotIndexForAreaTrigger`/`GetPlotIndexForHouseGO`), and `Player` has no `IsInsidePlot`/`IsInOwnHouse` helper. Building that authorization correctly is the real work — and a wrong/over-strict check will break **legitimate** editing (which the tester uses heavily), so it must be validated live.

**Suggested fix direction (verify against the branch):**
- Add an authoritative "player is inside their OWN plot" gate. Prior RE (`[[housing_decor_oob_isinsideplot_68275]]` in the c:\dumps memory) found IsInsidePlot server-side = **Map.db2 `InstanceType == 8` (a garrison/house instance) + a non-zero plot GUID at object offset `+0x1C0`**. Mirror that: resolve the current map's plot → its owner (via `neighborhood_members` / `character_housing.neighborhoodGuid`+`plotIndex`) → require it equals the acting player.
- Cheapest correct guard: reject the edit unless the current map's plot-at-`GetPlotIndex()` is owned by the acting player (interior maps: the `HouseInteriorMap` is per-house, so `GetHouseGuid()` must equal the player's `housing->GetHouseGuid()`; exterior/neighborhood maps: resolve plot→owner and compare).
- Test matrix: edit your own plot (must still work, interior + exterior); visit a public neighborhood and attempt each edit (must be rejected); edit while standing off your plot.

---

## Cluster B — deliberate "delete from storage" is a no-op

- **#13 `HandleHousingDecorDeleteFromStorage`** (`HousingHandler.cpp:1170`) — routes to `Housing::RemoveDecor(guid)`, which only operates on `_placedDecor` (it UNPLACES placed decor, moving it back to storage). For a **storage-only** GUID it returns `HOUSING_RESULT_...` and does nothing, so a player can never actually discard a stored item.

**IMPORTANT domain note (from the user, 2026-07-31):** housing STORAGE is a real, correct feature — decor you own but haven't placed lives in `_catalog` and is pushed to the client as the Account entity `FHousingStorage_C` (GUID-keyed) via `PopulateCatalogStorageEntries()`. It **intentionally preserves decor across house deletion for delayed RELOCATION** (delete your house, keep the decor, rebuild it elsewhere). **Do NOT "fix" that away.** #13 is ONLY the narrow *explicit* delete-from-storage (a player deliberately discarding one owned item).

**Why deferred / what it needs:** the client references each storage item **by GUID** (`FHousingStorage_C` is a GUID→entry map), but our storage is a **count per `decorEntryId`** (`_catalog`, keyed `unordered_map<uint32 decorEntryId, CatalogEntry>` in `Housing.h`) with **no per-instance GUIDs**. So "delete the item with GUID X" has nowhere to land. Correct fix needs **stable per-instance storage GUIDs matching the client's `FHousingStorage_C` scheme** — best read from that sync + a **sniff** of `CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE`, then decrement/remove the matching catalog entry (and re-push storage).

---

## Fixed already in integration (context, so you don't re-flag)
The same overnight pass fixed a **housing decor-budget mis-accounting** bug (exterior-plot decor charged to the interior budget in `PlaceDecor` + `RecalculateBudgets`; unified behind `Housing::IsExteriorDecorPlacement()`). That's in integration commit `fb3f146d9f` — pull/rebase it in when you sync, or re-apply if this branch diverged.

## Where the fuller context lives
Detailed memory in the c:\dumps workspace: `overnight_bughunt_68275`, `housing_full_migration_68275`, `housing_decor_oob_isinsideplot_68275`, `housing_branch_reconcile_68275`. This file is the branch-local pointer so this worktree's session has them without that memory store.
