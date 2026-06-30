# Playerbot V2 — Database Schema

**Status**: Pass B
**Last updated**: 2026-05-01
**Decisions answered from `REQUIREMENTS.md` §7 #2**: V2 starts with a **fresh schema**. V1's `playerbot_*` tables are not migrated. Bot character data continues to live in TrinityCore's existing `characters` table (bots are real characters); V2 owns *only* what TrinityCore doesn't already store.

## 1. Storage philosophy

Two stores. Strict separation.

| Store | What it owns | Owner |
|---|---|---|
| `characters` (TrinityCore standard) | Bot character: level, gear, gold, position, quest log, spells, talents, reputations, achievements, mounts/pets/toys, mail, AH listings, bank, friends, guild membership, **everything a player has** | TrinityCore — V2 does not write |
| `playerbot_v2_*` (this schema) | Only what's bot-specific: personality, skill tier, prefs, RNG seed, activity tier, neighborhood prefs, decoration plans, decision log | V2 only |

**Rule** (`REQUIREMENTS.md` §1.1 #7): if information would be tracked for a real player by TrinityCore, V2 must read it from `characters` (via `PlayerbotAPI`), not duplicate it. No mirror tables. V1's `playerbot_state` and similar are explicitly forbidden.

## 2. Migrations

Located in `sql/playerbot_v2/`. Numbered, append-only, idempotent. Migrations are run by `Persistence::PlayerbotMigrationMgr` at module init.

```
sql/playerbot_v2/
├── 0001_init.sql               # Bot identity flag, personality, prefs, progress
├── 0002_neighborhood.sql       # Housing-specific bot tables (post 12.0 housing release)
├── 0003_decision_log.sql       # Optional diagnostic log table
└── README.md                    # Migration discipline doc
```

Each file begins with a header comment:
```sql
-- Migration: 0001_init
-- Author: <name>
-- Date: <iso-date>
-- Purpose: <one sentence>
-- Reverts cleanly: <yes/no — if no, document why>
```

## 3. Schema (V1.0)

### 3.1 `playerbot_v2_account`
Marks an account as bot-owned (so the auth/login flow can permit bot logins, and so admin tools can filter).

```sql
CREATE TABLE playerbot_v2_account (
    account_id          INT UNSIGNED NOT NULL PRIMARY KEY,
    pseudo_account_idx  INT UNSIGNED NOT NULL,            -- Which slot in the bot account pool
    created_at          DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_used_at        DATETIME NULL,
    KEY idx_pseudo (pseudo_account_idx)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.2 `playerbot_v2_character`
One row per bot character. The `character_guid_low` joins to TrinityCore's `characters.guid`. Every other character attribute (level, race, gold, ...) lives in `characters` and is read via API.

```sql
CREATE TABLE playerbot_v2_character (
    character_guid_low   BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    spawned_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_active_at       DATETIME NULL,
    rng_seed             BIGINT UNSIGNED NOT NULL,
    spawn_state          TINYINT UNSIGNED NOT NULL,        -- 0=warm, 1=spawning, 2=active, 3=despawning
    KEY idx_active (last_active_at),
    KEY idx_state  (spawn_state)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.3 `playerbot_v2_personality`
Skill tier, verbosity, aggression, etc. Drives behavioral variance per `SYSTEM_FEATURES.md` §7.

```sql
CREATE TABLE playerbot_v2_personality (
    character_guid_low   BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    skill_tier           TINYINT UNSIGNED NOT NULL,        -- 0=novice 1=competent 2=expert 3=elite
    verbosity            TINYINT UNSIGNED NOT NULL,        -- 0=silent 1=terse 2=normal 3=chatty 4=rp
    aggression           TINYINT UNSIGNED NOT NULL,        -- 0=passive 1=defensive 2=normal 3=aggressive
    risk_tolerance       TINYINT UNSIGNED NOT NULL,        -- 0=cautious .. 3=reckless
    politeness           TINYINT UNSIGNED NOT NULL,        -- 0=rude 1=neutral 2=polite
    loyalty              TINYINT UNSIGNED NOT NULL,        -- 0=flighty 1=normal 2=devoted
    activity_pref        TINYINT UNSIGNED NOT NULL,        -- bitfield: solo|group|pvp|prof|social
    response_delay_ms    SMALLINT UNSIGNED NOT NULL,       -- Mean reaction delay
    response_jitter_ms   SMALLINT UNSIGNED NOT NULL,       -- σ for delay
    mistake_rate         TINYINT UNSIGNED NOT NULL,        -- 0=never .. 100=always
    CONSTRAINT fk_pers_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.4 `playerbot_v2_preferences`
Bot's preferred neighborhood, housing template choice, content opt-ins. Not derivable from game state.

```sql
CREATE TABLE playerbot_v2_preferences (
    character_guid_low       BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    preferred_neighborhood   INT UNSIGNED NULL,
    preferred_house_template INT UNSIGNED NULL,
    opt_in_dungeons          BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_raids             BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_pvp               BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_arena             BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_delves            BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_professions       BOOLEAN NOT NULL DEFAULT TRUE,
    opt_in_housing           BOOLEAN NOT NULL DEFAULT TRUE,
    accept_player_invites    TINYINT UNSIGNED NOT NULL,    -- 0=none 1=friends 2=guild 3=anyone
    follow_distance_yd       FLOAT NOT NULL DEFAULT 5.0,
    CONSTRAINT fk_prefs_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.5 `playerbot_v2_relationship`
Per-player relationship: friend, regular, blacklisted. Bots can ignore griefers; bots remember favorite players.

```sql
CREATE TABLE playerbot_v2_relationship (
    bot_character_guid_low      BIGINT UNSIGNED NOT NULL,
    player_character_guid_low   BIGINT UNSIGNED NOT NULL,
    tier                         TINYINT UNSIGNED NOT NULL,  -- 0=blacklist 1=neutral 2=friend 3=favorite
    last_interaction_at          DATETIME NOT NULL,
    interaction_count            INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (bot_character_guid_low, player_character_guid_low),
    CONSTRAINT fk_rel_bot FOREIGN KEY (bot_character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low) ON DELETE CASCADE,
    KEY idx_player (player_character_guid_low)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.6 `playerbot_v2_population_target`
Server-wide population curve config (also editable via `playerbot.conf`). Stored here for hot-reload + admin override audit.

```sql
CREATE TABLE playerbot_v2_population_target (
    target_id        INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    realm_id         INT UNSIGNED NOT NULL,
    effective_at     DATETIME NOT NULL,
    total_target     INT UNSIGNED NOT NULL,
    floor            INT UNSIGNED NOT NULL,
    ceiling          INT UNSIGNED NOT NULL,
    horde_pct        TINYINT UNSIGNED NOT NULL,            -- 0..100
    payload_json     TEXT NOT NULL,                         -- per-level/class quotas, full curve
    KEY idx_realm_effective (realm_id, effective_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 3.7 Migration: `0002_neighborhood.sql`
Created when 12.0 housing is wired. Purpose: bot housing decision history (which neighborhoods bot has joined, plot purchase intent log).

```sql
CREATE TABLE playerbot_v2_neighborhood_membership (
    character_guid_low      BIGINT UNSIGNED NOT NULL,
    neighborhood_id         INT UNSIGNED NOT NULL,
    joined_at               DATETIME NOT NULL,
    left_at                 DATETIME NULL,
    PRIMARY KEY (character_guid_low, neighborhood_id, joined_at),
    CONSTRAINT fk_nb_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE playerbot_v2_decoration_plan (
    plan_id                 BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    character_guid_low      BIGINT UNSIGNED NOT NULL,
    plot_id                 INT UNSIGNED NOT NULL,
    target_theme            VARCHAR(64) NOT NULL,
    decorations_planned     SMALLINT UNSIGNED NOT NULL,
    decorations_placed      SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    last_progress_at        DATETIME NULL,
    KEY idx_char_plot (character_guid_low, plot_id),
    CONSTRAINT fk_decoplan_char FOREIGN KEY (character_guid_low)
        REFERENCES playerbot_v2_character (character_guid_low) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

The actual decoration *placements* live in the standard housing tables (modified by `house_purchase_plot`/`deco_place` API calls — the **same tables players write to**). V2 never duplicates that data here.

### 3.8 Migration: `0003_decision_log.sql` (optional, dev/diagnostic)
Sampled per-bot intent log. Disabled in production by config; useful in dev to validate determinism and replay scenarios.

```sql
CREATE TABLE playerbot_v2_intent_log (
    log_id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    character_guid_low  BIGINT UNSIGNED NOT NULL,
    tick_id             BIGINT UNSIGNED NOT NULL,
    intent_kind         SMALLINT UNSIGNED NOT NULL,
    payload_json        TEXT NULL,
    emitted_at          DATETIME(3) NOT NULL,
    KEY idx_char_tick (character_guid_low, tick_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPRESSED;
```

## 4. Indexing & sizing (rough)

At 5K active bots:
- `playerbot_v2_character`: 5K rows + warm-pool depth (~5K total). Tiny.
- `playerbot_v2_personality`: 1:1 with character. Tiny.
- `playerbot_v2_preferences`: 1:1. Tiny.
- `playerbot_v2_relationship`: highly variable. Could grow to millions over a year if bots remember every player they meet. Consider TTL/pruning policy in V1.1.
- `playerbot_v2_intent_log`: high-write table when enabled. Default disabled. Sample at 1% if enabled. Rotate weekly.

## 5. Migration runner

Implemented in `Persistence/PlayerbotMigrationMgr.{cpp,h}`:

```cpp
class PlayerbotMigrationMgr {
public:
    void run_all();    // Called at module init. Idempotent.

private:
    int  current_version() const;
    void apply(int target_version);
    void record_applied(int version);
};
```

A single `playerbot_v2_schema_version` table tracks applied migrations:

```sql
CREATE TABLE playerbot_v2_schema_version (
    version       INT UNSIGNED NOT NULL PRIMARY KEY,
    applied_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    sha256        CHAR(64) NOT NULL                            -- file hash for tamper detection
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## 6. Backup, reset, wipe

- `playerbot_v2 backup`: dumps all `playerbot_v2_*` tables + a manifest of bot character GUIDs. Does not include the standard `characters` rows (those are in TC's normal backup flow).
- `playerbot_v2 wipe-confirm`: deletes all `playerbot_v2_*` rows + (optionally) the corresponding `characters` rows. Confirmation prompt mandatory.
- `playerbot_v2 reset <bot>`: drops V2 rows for one bot (keeps the character but the bot loses personality/prefs and is reseeded).

## 7. What is explicitly NOT in V2's schema

- Position, level, gold, inventory, equipment, spells, talents, quests, mail, AH, bank, friends, ignore list, guild, achievements, mounts, pets, toys, transmog wardrobe — all live in `characters` and TrinityCore's standard tables. V2 never writes them.
- Housing decorations, plots, neighborhood membership of *characters* — owned by the standard housing tables (player + bot use the same path).
- Combat / threat / aura / cooldown state — never persisted; rebuilt from runtime.

## 8. What's locked vs open

**Locked**: tables `playerbot_v2_account`, `playerbot_v2_character`, `playerbot_v2_personality`, `playerbot_v2_preferences`, `playerbot_v2_relationship`, `playerbot_v2_population_target`, `playerbot_v2_schema_version`. Migration runner shape.

**Open**:
- Whether `playerbot_v2_relationship` is needed in V1.0 (could defer to V1.1 if it's complexity for marginal gain).
- Whether `playerbot_v2_intent_log` ships at all in V1.0 (can be added in V1.1 for diagnostics, dev-build-only flag).
- Final form of housing tables — depends on stabilization of upstream 12.0+ housing schema.
