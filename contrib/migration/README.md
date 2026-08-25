# 12.0.7 → 12.1 migration toolkit

Tooling used to migrate the agatho fork from WoW **12.0.7 (68974/68275)** to upstream
TrinityCore **12.1.0 (client 69404)**. Kept here for the next client bump.

## Method

The feature branches were **malformed carriers** (cut from integration, dragging
integration-wide fork state), so they are not individually rebased. Instead each fork
*system* is **reconstructed onto a clean `upstream/master`** base:

1. `git reset --hard upstream/master` in a scratch worktree (→ correct, current 12.1
   `DB2Metadata.h`, opcode tables, PacketIO, engine APIs).
2. **Overlay** the system's dedicated files (its `src/server/game/<Sys>/` dir + its
   `*Packets`/`*Handler`/`*Mgr`) from the pre-migration integration.
3. **Graft** the fork's DB2 stores that upstream lacks (struct + LoadInfo + store decl/def
   + hotfix statements), and the system's CharacterDatabase statements + WorldSession
   fwd-decls/handler-decls.
4. Build `worldserver`; iterate grafts until green; push to `feature/<sys>`.
5. After all systems: rebuild the integration lines **by merging** the reconstructed
   branches (never cherry-pick).

`upstream/master` is authoritative for everything the client ships (DB2 schema, opcodes,
UpdateFields). The client `.db2` files are the arbiter for any hand-reconciled layout.

## Scripts

| Script | Role |
|---|---|
| `reconstruct_system.sh <SysDir> [NS] [base]` | Driver for steps 1-4 for one system. |
| `graft_db2_store.py <Store>` | Graft one fork DB2 store (struct/LoadInfo/Meta/store/hotfix). |
| `batch_systems.sh` | Run `reconstruct_system.sh` + build + push for a list of systems. |
| `isolation_test.sh` | Cherry-pick each orphan commit onto clean 12.1 to classify clean/conflict/already-present. |
| `reference/` | System-specific one-offs (M+, housing, covenant/world-quest slice restores, DB2 layout fixes). Historical; still contain absolute session paths. |

### Paths (env-overridable)

Top-level scripts read these (defaults are the migration layout):

- `MIGRATE_WT`   — scratch worktree the reconstruction is staged in (`I:/TrinityCore/_migrate121`)
- `MIGRATE_BARE` — bare repo (`I:/TrinityCore/.bare`)
- `MIGRATE_SRC`  — **graft source**: the *pre-migration* integration to pull fork struct/LoadInfo from (`origin/integration/all-systems`)
- `MIGRATE_TOOLS`— this toolkit dir (defaults to the script's own dir)

## ⚠ Known pitfall: stale-struct vs current Meta (fixed by a guard)

`graft_db2_store.py` grafts a fork db2's **struct + LoadInfo from `MIGRATE_SRC`** (the old
integration) but keeps the **Meta from the `upstream/master` base** (it only grafts the
Meta when 12.1 lacks it). If upstream advanced that db2's layout between the two builds,
the result is **new Meta + stale struct/LoadInfo** — a mismatch that crashes at boot
(`LoadDB2` record-size / signedness asserts, or the fmt/DB2FileLoader path).

This bit the 12.1 bring-up: `PlayerCompanionInfo` (15→17), `ItemConversion` (+Flags),
`HouseDecor` (−Field_003), `HouseRoom` (Name-before-ID +Field_008), plus `CharShipment`
field order and three `Garr*` signedness fields. Each was reconciled to the client layout.

`graft_db2_store.py` now carries a **guard**: after grafting it compares the
`upstream/master` Meta `FieldCount` against the graft-source Meta `FieldCount` for the
store and exits non-zero with a reconcile message if they differ, so the mismatch surfaces
at reconstruction time instead of at boot. (It catches field-count changes; field
*order*/*signedness* changes still surface at the boot assertions.)

Note: the reconstruction never reads fork db2 structs from the feature branches — only
from `MIGRATE_SRC`. Branch DB2 staleness does not affect the reconstructed output.
