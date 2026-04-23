# Trinity Housing — SQL

This folder contains every SQL file required to install the Trinity Housing
system on top of a stock TrinityCore 12.0.1 deployment. Two ways to use it:

1. **Bundled install (recommended for testers)** — run the three aggregate
   master files in `master/`. See `master/README.md` for step-by-step
   instructions. These bundle every file in the correct order.

2. **Per-file install (for development / debugging)** — run individual files
   in this folder as needed. Ordering is documented in `master/build.sh`.

## File layout

- `master/` — bundled master installers (one per target database) plus a
  PowerShell install script and build tooling. **Start here.**
- `housing_schema.sql` — `CREATE TABLE` statements for all housing tables in
  the characters database.
- `characters_housing_*.sql` — per-character migrations. Run after the schema
  if upgrading an older database.
- `hotfixes_*.sql` — DB2 hotfix data (plot positions, room components, etc.)
- `world_*.sql` — GO templates, area triggers, quest chain, neighborhood
  spawns, door scripts.
- `characters_housing_reset_all.sql` — **destructive** full wipe of housing
  data. Not bundled in the masters. Use only when intentionally resetting.
- `*.bin` — opaque packet captures used by the server init path, not SQL.

## Targets

| Database | What goes in it |
| --- | --- |
| `characters` | housing schema + per-character migrations |
| `hotfixes` | plot/map DB2 overrides, room component data |
| `world` | GO/creature templates, area triggers, neighborhood spawns |

## Quick install

Windows / PowerShell:

```powershell
& '.\sql\housing\master\install.ps1' -User <user> -Password <password>
```

Anything else:

```bash
mysql -u <user> -p characters < sql/housing/master/MASTER_housing_characters.sql
mysql -u <user> -p hotfixes   < sql/housing/master/MASTER_housing_hotfixes.sql
mysql -u <user> -p world      < sql/housing/master/MASTER_housing_world.sql
```
