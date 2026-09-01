# Trinity Housing — Master SQL installation bundle

This folder contains **aggregate** master SQL files that consolidate every
housing-related change and addition from `sql/housing/` in a single, ordered
install per target database.

The individual files under `sql/housing/` remain authoritative. These masters
are a convenience bundle for testers who want a one-shot install without
having to remember file ordering.

## Files

| File | Target database | Size |
| --- | --- | --- |
| `MASTER_housing_characters.sql` | `characters` | schema + per-char migrations |
| `MASTER_housing_hotfixes.sql` | `hotfixes` | plot/map DB2 hotfixes, initiative tables, room components |
| `MASTER_housing_world.sql` | `world` | GO/creature templates, AT, quest chain, door script, neighborhood spawns (Alliance + Horde) |

## Install (Linux / macOS / Git Bash)

```bash
mysql -u <user> -p characters < MASTER_housing_characters.sql
mysql -u <user> -p hotfixes   < MASTER_housing_hotfixes.sql
mysql -u <user> -p world      < MASTER_housing_world.sql
```

Replace `<user>` with your Trinity MySQL user. You will be prompted for the
password interactively (or use `-p<password>` with no space to pass it
directly — note the shell history caveat).

## Install (Windows / PowerShell)

From the repository root run:

```powershell
& '.\sql\housing\master\install.ps1' -User <user> -Password <password>
```

The script runs the three masters against the matching databases in the
correct order and exits with a non-zero code on the first failure.

## Prerequisites

- MySQL server reachable from the install host.
- The three target databases (`characters`, `hotfixes`, `world`) already exist
  with TrinityCore's base schema applied. Run TrinityCore's standard
  `auth_database.sql` / `characters_database.sql` / `world` imports first.
- The user running the install must have `CREATE`, `ALTER`, `DROP`, `INSERT`,
  `UPDATE`, `DELETE`, and `INDEX` privileges on each target database.

## What's not bundled

- `sql/housing/characters_housing_reset_all.sql` — destructive full reset of
  all housing data. Run manually only when you know what you're doing.
- `.bin` files in `sql/housing/` — opaque packet captures used by the server
  init path, not SQL.
- Incremental migrations under `sql/updates/*/master/` — these are picked up
  automatically by TrinityCore's DB updater on server start. Don't run them
  manually alongside the masters or you'll double-apply a few migrations.

## Install order inside the masters

The bash script `build.sh` in this folder documents the exact ordering per
master file. Regenerate the bundle after any edit to `sql/housing/` by running
`bash build.sh` from the repository root. Never hand-edit the master files —
they are pure aggregates.

## Troubleshooting

- **`ERROR 1146 (42S02): Table '...' doesn't exist`** — the base TrinityCore
  schema isn't installed yet. Run the base imports first, then the masters.
- **`ERROR 1050 (42S01): Table '...' already exists`** — you ran the masters
  twice. The schema parts use `CREATE TABLE IF NOT EXISTS` where safe; the
  migrations use `ALTER TABLE` which will fail on a second run. The failure
  is safe to ignore if the second run is a re-install on top of an existing
  housing DB — but inspect the error to be sure.
- **`ERROR 1062 (23000): Duplicate entry`** — hotfix data is already present.
  The hotfix files use `DELETE` + `INSERT` patterns to be idempotent; if you
  see this, something interrupted a prior run. Re-running is safe.
