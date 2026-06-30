# Playerbot V2 — Configuration

**Status**: Pass B
**Last updated**: 2026-05-01
**Purpose**: Every config key, what it does, default value, hot-reload eligibility. The full schema for `playerbot.conf` (loaded by `Util/ConfigReader`).

## 1. File location & format

`playerbot.conf` lives next to `worldserver.conf`. Same INI-like format TrinityCore uses elsewhere:

```ini
KeyName = value
```

`playerbot.conf.dist` ships the documented defaults; admins copy to `playerbot.conf` and override.

## 2. Hot-reload semantics

Each key is tagged:
- **HR-yes**: change applies on next config reload (`.playerbot reload`) without restart.
- **HR-no**: requires worldserver restart (because it changes thread counts, table layouts, or invariants).

## 3. Key reference

### 3.1 Core / threading

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.AiWorkerThreads` | `0` | HR-no | 0 = auto (`min(8, hardware_concurrency)`). Set explicit count to force. |
| `Playerbot.FleetThreadEnabled` | `true` | HR-no | Per Pass A §14.5 default. False = collapse onto world thread. |
| `Playerbot.IntentQueueCapacity` | `4096` | HR-no | Per-bot intent queue ring size. |
| `Playerbot.SnapshotMaxBytes` | `8192` | HR-no | Hard cap for `BotSnapshot` size; build aborts at compile time if exceeded. |
| `Playerbot.TickBudgetMs` | `10` | HR-yes | World-tick time budget for V2 (per `REQUIREMENTS.md` §2.1). Bots skip ticks beyond this. |

### 3.2 Population

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Population.TotalTarget` | `2000` | HR-yes | Target concurrent bot count. |
| `Playerbot.Population.Floor` | `100` | HR-yes | Always at least this many bots online. |
| `Playerbot.Population.Ceiling` | `5000` | HR-yes | Hard upper limit. |
| `Playerbot.Population.AutoScale` | `true` | HR-yes | Whether to dynamically scale to real-player count. |
| `Playerbot.Population.AutoScaleMultiplier` | `2.0` | HR-yes | Bots-per-real-player target. |
| `Playerbot.Population.HordePct` | `50` | HR-yes | Faction balance bias (Horde percentage). |
| `Playerbot.Population.LevelDistribution` | `bell` | HR-yes | One of: `flat`, `bell`, `endgame_heavy`, `custom`. |
| `Playerbot.Population.LevelCustom` | (empty) | HR-yes | Comma-list of `level:weight` pairs when distribution is `custom`. |
| `Playerbot.Population.RoleTankPct` | `15` | HR-yes | Targeted tank percentage. |
| `Playerbot.Population.RoleHealerPct` | `20` | HR-yes | Targeted healer percentage. |
| `Playerbot.Population.RoleDpsPct` | `65` | HR-yes | Targeted DPS percentage. Sum of three should be 100. |
| `Playerbot.Population.ClassDistribution` | `realistic` | HR-yes | `realistic`, `uniform`, `custom`. |
| `Playerbot.Population.ClassCustom` | (empty) | HR-yes | Comma-list of `class:weight`. |
| `Playerbot.Population.TimeOfDayCurve` | `enabled` | HR-yes | Apply real-world time-of-day modulation. |
| `Playerbot.Population.WeekendBoost` | `1.2` | HR-yes | Multiplier on weekends. |

### 3.3 Lifecycle

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Lifecycle.WarmPoolSize` | `200` | HR-yes | Pre-spawned ready bots for instant fill. |
| `Playerbot.Lifecycle.SpawnRateLimit` | `100` | HR-yes | Spawns per second cap. |
| `Playerbot.Lifecycle.LoginRateLimit` | `100` | HR-yes | Concurrent login cap (anti-storm). |
| `Playerbot.Lifecycle.IdleDespawnMinutes` | `0` | HR-yes | 0 = never despawn from idle. >0 = minutes idle to trigger. |
| `Playerbot.Lifecycle.GracefulLogoutDelayMs` | `5000` | HR-yes | Time given to save state before logout. |
| `Playerbot.Lifecycle.MaxBotsPerAccount` | `30` | HR-no | Pseudo-account capacity. |

### 3.4 Player interaction

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Interact.AutoAcceptInvites` | `friends_or_higher` | HR-yes | `none`, `friends_or_higher`, `guild_or_higher`, `anyone`. |
| `Playerbot.Interact.AutoLeaderHandover` | `true` | HR-yes | Bot relinquishes leadership on player join. Per `SYSTEM_FEATURES.md` §3.2. |
| `Playerbot.Interact.AcceptTradeFromAnyone` | `false` | HR-yes | False = friends only; bots accept items from friends only by default. |
| `Playerbot.Interact.RespondToWhispersFromAnyone` | `true` | HR-yes | If false, only friends/group/guild get bot responses. |
| `Playerbot.Interact.ChatRateLimitPerPlayer` | `5` | HR-yes | Max bot chat replies to one player per minute. |
| `Playerbot.Interact.RespectPlayerIgnore` | `true` | HR-no | If a player /ignores a bot, bot does not interact. |
| `Playerbot.Interact.CommandPrefix` | `!` | HR-yes | Whisper command prefix. |

### 3.5 LFG / BG / Arena fill

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Lfg.AutoFillEnabled` | `true` | HR-yes | Player queues → bots fill empty roles. |
| `Playerbot.Lfg.MaxBotsPerGroup` | `4` | HR-yes | Cap to ensure not all 5 are bots when one player queues solo. |
| `Playerbot.Bg.AutoFillEnabled` | `true` | HR-yes | Fill BG queues from both factions. |
| `Playerbot.Bg.MaxBotPctPerMatch` | `100` | HR-yes | 100 = all-bot allowed. Lower to require real-player presence. |
| `Playerbot.Bg.SkillTierMatchmaking` | `true` | HR-yes | Match bot skill tier to player's rating. |
| `Playerbot.Arena.AutoFillEnabled` | `true` | HR-yes | |
| `Playerbot.Arena.SoloShuffleFillEnabled` | `true` | HR-yes | |
| `Playerbot.Arena.MercenaryAllowed` | `true` | HR-yes | Bot may queue for opposite faction in mercenary mode. |
| `Playerbot.Lfg.QueueThrottleMs` | `2000` | HR-yes | Min delay between bot LFG queues to avoid flooding. |

### 3.6 Housing / neighborhoods

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Housing.Enabled` | `true` | HR-yes | Master switch for bot housing participation. |
| `Playerbot.Housing.NeighborhoodTargetOccupancy` | `60` | HR-yes | Per-neighborhood bot residency target percent of capacity. |
| `Playerbot.Housing.PlotPurchaseEligibilityCheck` | `strict` | HR-no | `strict` = bot must meet level/quest/currency exactly as player; `lenient` (DEV ONLY) = relaxed for testing. **Production must be `strict`** per `REQUIREMENTS.md` §1.1 #7. |
| `Playerbot.Housing.MaxDecorationsPerHouse` | `0` | HR-yes | 0 = use server max. >0 = bot self-cap. |
| `Playerbot.Housing.PublicHousePct` | `30` | HR-yes | Percent of bots that make their house public. |
| `Playerbot.Housing.OpenHouseEventCadenceDays` | `7` | HR-yes | Bots host showcase events ~once per N days. |

### 3.7 Combat

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Combat.AplHotReload` | `true` | HR-yes | Per Pass A §14.7. APLs reload from disk without server restart. |
| `Playerbot.Combat.DefaultEncounterEnabled` | `true` | HR-yes | Use generic AoE-dodge for un-scripted bosses. |
| `Playerbot.Combat.SkillTierAffectReactions` | `true` | HR-yes | Lower tiers slower reactions per `SYSTEM_FEATURES.md` §7.1. |
| `Playerbot.Combat.ReactionDelayBaseMs` | `300` | HR-yes | Mean reaction delay at competent tier. |
| `Playerbot.Combat.ReactionDelayJitterMs` | `100` | HR-yes | σ jitter. |
| `Playerbot.Combat.MistakeRatePct` | `2` | HR-yes | Default mistake rate at competent tier. |

### 3.8 Movement

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Movement.FollowDistance` | `5.0` | HR-yes | Default leader-follow distance in yards. |
| `Playerbot.Movement.StuckThresholdSeconds` | `5` | HR-yes | Time motionless while wanting to move → unstick. |
| `Playerbot.Movement.UnstickHearthFallback` | `true` | HR-yes | Hearth if jump+retry fails. |
| `Playerbot.Movement.MountThresholdYards` | `60` | HR-yes | Mount up for travels longer than this. |

### 3.9 Economy

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Economy.AhMarketPresence` | `true` | HR-yes | Bots maintain AH listings of common items for liveliness. |
| `Playerbot.Economy.AhMarketPresenceTargetListings` | `5000` | HR-yes | Server-wide target of bot-posted listings. |
| `Playerbot.Economy.RepairThresholdPct` | `30` | HR-yes | Durability percent below which to repair. |
| `Playerbot.Economy.GoldReserveBase` | `100` | HR-yes | Bot keeps at least N silver pieces as reserve. (Multiplied by level.) |

### 3.10 Diagnostics

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Diag.PerfCountersEnabled` | `true` | HR-yes | Counter collection; cheap. |
| `Playerbot.Diag.HealthEndpointPort` | `0` | HR-no | TCP port for monitoring. 0 = disabled. |
| `Playerbot.Diag.TickTraceEnabled` | `false` | HR-yes | Per-bot 256-entry ring buffer. Costs ~16KB/bot. |
| `Playerbot.Diag.IntentLogEnabled` | `false` | HR-yes | Write to `playerbot_v2_intent_log` (sampled). Dev-only. |
| `Playerbot.Diag.IntentLogSamplePct` | `1` | HR-yes | Percent of intents to log. |

### 3.11 Database

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.DB.Driver` | `MySQL` | HR-no | DB driver (TrinityCore standard). |
| `Playerbot.DB.AsyncMigrations` | `false` | HR-no | Run schema migrations off the world thread on startup. |
| `Playerbot.DB.RelationshipPruneDays` | `90` | HR-yes | Drop relationship rows untouched for N days. |

### 3.12 Logging

| Key | Default | HR | Description |
|---|---|---|---|
| `Playerbot.Log.Level` | `info` | HR-yes | `error`, `warn`, `info`, `debug`, `trace`. |
| `Playerbot.Log.PerBotMaxKBPerHour` | `64` | HR-yes | Cap to prevent disk-filling from one runaway bot. |
| `Playerbot.Log.SystemEvents` | `true` | HR-yes | Log spawn/despawn/group-form/LFG-fill events. |
| `Playerbot.Log.PlayerInteraction` | `true` | HR-yes | Whisper, trade, group-form. |

## 4. `playerbot.conf.dist` shipping content

Pass B Implementation note: when the `playerbot.conf.dist` file ships, every key above appears with:
- A heading comment (purpose, allowed values)
- The default value (commented if it represents the in-code default; uncommented otherwise)
- A `# HR-yes` or `# HR-no` tag

Sample entry:
```ini
###################
# Population sizing
###################

# Target concurrent bot count. The system tries to maintain this many bots
# online, scaling between Floor and Ceiling as real-player count varies
# (when AutoScale is true).
# HR-yes
Playerbot.Population.TotalTarget = 2000

# Always have at least this many bots online, even when real player count is high.
# HR-yes
Playerbot.Population.Floor = 100
```

## 5. Validation

`Util/ConfigReader::load(path)`:
- Loads file, parses keys.
- Validates types and ranges per a hard-coded schema in `ConfigReader.cpp`.
- Reports errors line-numbered to the log; refuses to start with malformed config.
- For HR-yes keys, supports `.playerbot reload`. Updates atomic config snapshot. AI workers see new values on next tick.
- For HR-no keys, reload logs a warning and ignores.

## 6. What's locked vs open

**Locked**: every key name and HR-tag above. Defaults are starting points; admins override per server.

**Open**:
- The `Population.LevelCustom` and `Population.ClassCustom` formats — likely JSON in v1.1 if simple comma-lists prove too rigid.
- Whether population curves should support multiple realm-specific overrides via DB rows (`playerbot_v2_population_target`) in addition to file config — leaning yes; deferred to V1.1.
- Localization of `playerbot.conf.dist` comments — out of scope.
