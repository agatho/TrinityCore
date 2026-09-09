# TrinityCore + Playerbot V2

A [TrinityCore](https://www.trinitycore.org/) server with the **Playerbot V2**
module — large-scale, AI-controlled player bots that quest, group, run dungeons
and battlegrounds, trade, and populate the world, for a single-player or
low-population MMORPG experience.

This is a standard TrinityCore tree with one extra optional module under
`src/modules/PlayerbotV2`. With the module disabled it builds and runs exactly
like upstream TrinityCore.

## Build

Configure with the module enabled, then build `worldserver`:

```
cmake <path-to-source> -DBUILD_PLAYERBOT_V2=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --config RelWithDebInfo --target worldserver
```

`BUILD_PLAYERBOT_V2` is the only Playerbot build flag. On Windows the
`configure_*.bat` scripts at the repo root wrap this (override `VCPKG_ROOT`,
`BOOST_ROOT`, `QT_PREFIX` via environment variables or by editing the script).

## Setup

See **[`v2/INSTALL.md`](v2/INSTALL.md)** for the full setup: creating the
playerbot database schema, applying the SQL (the name pool and tables auto-apply
at first boot), the world-database fixes, and the configuration file
(`playerbot.conf`).

## Documentation

- [`v2/INSTALL.md`](v2/INSTALL.md) — installation and setup
- [`v2/CONFIG.md`](v2/CONFIG.md) — configuration options (also documented inline in `playerbot.conf.dist`)
- [`v2/ARCHITECTURE.md`](v2/ARCHITECTURE.md), [`v2/BUILD.md`](v2/BUILD.md), [`v2/SCHEMA.md`](v2/SCHEMA.md) — design, build wiring, database schema

For the underlying emulator, see the upstream
[TrinityCore documentation](https://trinitycore.info/) and `INSTALL`.

## License

GPL-2.0, same as TrinityCore. See `COPYING`.
