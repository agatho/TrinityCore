# Playerbot V2 — SQL migrations

Per `v2/SCHEMA.md`. Migration files in this directory are numbered, append-only, and idempotent. Run by `Persistence/PlayerbotMigrationMgr` at module init.

For full setup instructions (schema creation, config, world fixes) see
`v2/INSTALL.md`.

## Order
Applied in ascending numeric order; each file records itself in
`playerbot_v2_schema_version`. Current set:

- `0001_init.sql` — schema_version + core tables (account, character, personality, preferences, population_target)
- `0002`–`0014` — owner/squad control, talent builds, population columns, guild metadata, world metadata, leveling zone, fleet vitals, archetype, craft orders, stuck-objective ledger
- `0015_name_pool.sql` — the curated ~107k-row random bot **name pool** (`playerbots_names`), shipped with use-state reset so a fresh install starts with the whole pool available

## Discipline
- Never edit a numbered migration after it ships. Add a new one to amend.
- Each migration is self-contained: no cross-file dependencies on application order beyond numbering.
- Reversibility documented in the file header.
