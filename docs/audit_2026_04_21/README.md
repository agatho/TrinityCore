# Housing System Audit — 2026-04-21/22 Overnight

## TL;DR

Ran a 5-hour bit-by-bit comparison of our server against retail build 66838 sniffs. Found 7 real deviations. Fixed 5 structurally, deferred 2 for next session. All changes committed to `feature/housing-system` branch and built into `build/bin/RelWithDebInfo/worldserver.exe`.

## What changed in the codebase

9 commits, in order:

| # | Commit | What |
|---|--------|------|
| 1 | 76fff332d5 | `TYPEID_HOUSING_ENTITY` on session Housing/3 + Housing/4 entities |
| 2 | b45b43446c | Bundle neighbour-plot HousingPlayerHouse proxies into Player CREATE |
| 3 | b860732bfd | Emit map-entry spell triples synchronously (not deferred 500 ms) |
| 4 | 2a1a6e3e11 | Drop ghost-GUID mapHouseEntity from Player CREATE |
| 5 | 7a8be9e015 | Add `HousingMirrorEntity` class skeleton |
| 6 | 4262499682 | Wire `HousingMirrorEntity` into map lifecycle + Player CREATE bundle |
| 7 | e263ae9ee5 | Emit `SMSG_HOUSING_CATALOG_STATE_SYNC` + 40 missing MIRROR_VARS |
| 8 | 5ae862a143 | Drop duplicate HouseStatus/Permissions emission from deferred ENTER_PLOT |
| 9 | 44ff971631 | Fix `SMSG_HOUSING_HOUSE_STATUS_RESPONSE` wire format (3 guids + uint32, not 4 guids + 2 uint8) |

## To deploy

```bash
# Stop the running worldserver first (Windows locks the exe while running)
# Then:
cp build/bin/RelWithDebInfo/worldserver.exe M:/Wplayerbot/worldserver.exe
# Relaunch worldserver
```

## What to verify after deploy

1. **Icon picker** — does the own-plot icon render as "Your House" with yellow spiral now?
2. **Entity mirrors** — capture new sniff, expect `HighGuid::Entity (57)` count ≥ 4 in map-entry UPDATE_OBJECT.
3. **Catalog state** — `SMSG_HOUSING_CATALOG_STATE_SYNC` (0x0056000E) should appear once in the packet stream.
4. **HouseStatus/Permissions count** drops from 5 to ~2-3 per session.
5. **No editor-mode flapping** — the corrected wire format should stop the HouseStatus double-emit at plot entry.

## What's still pending (next session)

1. **Visibility distance** (20× over-emission) — `HousingMap::InitVisibilityDistance` uses MAX (~533 y). Retail uses bounded visibility. Fix requires `SetActive(true)` on plot ATs to keep them in registry at any distance.
2. **Group B Entity mirrors** — retail sends 4 per-MeshObject `FMirroredPositionData_C`-only mirrors; we send 0.
3. **Second `SMSG_INIT_WORLD_STATES`** emission (retail sends 2, we send 1).
4. **Remaining 36 MIRROR_VARS** — Shop2 URLs and Pinterest API keys; not blocking.
5. **Full interaction audit** — our audit only covered login + map entry. Decor/fixture/room CMSGs haven't been exercised in any sniff yet.

## Reference

- **Main findings:** `docs/audit_2026_04_21/FINDINGS_FINAL.md`
- **Corrected inventory:** `docs/audit_2026_04_21/FINDINGS_v2.md`
- **Script inventory:** 44 scripts in `sniff_analysis_login_plot/` (numbered 00–43)
- **Canonical opcode map:** `docs/audit_2026_04_21/opcode_map.json` (2457 opcodes extracted from `Opcodes.h`)
- **Byte dumps:** `docs/audit_2026_04_21/WIRE_*.md`
- **Strict entity parses:** `docs/audit_2026_04_21/{RETAIL_idx9984,OUR_idx295,OUR_idx310}_entities.{csv,summary.md}`
