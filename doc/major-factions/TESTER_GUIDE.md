# Phase 10 — Major Factions: Tester Guide

Branch tip at the time of writing: `5f18d7d404`. Anything at or above this
commit on `origin/major-factions` includes the four post-publish fixes
(build, SQL idempotency, currency-column casing, DB2 ParentIndexField
signedness, and the warband-reputation save-ordering bug).

---

## 1. Setup

### 1.1 Build

```
git fetch origin
git checkout major-factions
git reset --hard origin/major-factions
cmake -S . -B ./build -G "Visual Studio 17 2022" -A x64 \
    -DSERVERS=1 -DTOOLS=1 -DSCRIPTS=static \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build ./build --config RelWithDebInfo --parallel
```

`worldserver.exe` lands in `./build/bin/RelWithDebInfo/`. Copy it (and
`bnetserver.exe`) over your runtime install — the SQL strings are
compiled in, so running an older binary against the new schema will
re-surface the bugs that have already been fixed.

### 1.2 SQL migrations

Let the auto-updater apply everything; do **not** import files by hand.
The relevant new migrations on top of the warband Phase-1–9 base are:

| File | What it does |
|---|---|
| `2026_05_15_00_hotfixes.sql` | `covenant`, `renown_rewards`, `renown_rewards_plunderstorm`, `quest_line`, `campaign_x_condition`, `faction_group`, `currency_category` schemas |
| `2026_05_15_01_hotfixes.sql` | Same wave, locale partner tables |
| `2026_05_15_02..03` | Wave continuation |
| `2026_05_16_00_hotfixes.sql` | `ui_texture_kit` |
| `2026_05_16_00_characters.sql` | `character_renown_rewards_granted`, `warband_renown_rewards_granted` |
| `2026_05_16_0{0..6}_world.sql` | `major_faction_config` + 20-row seed, quartermaster `npc_text` rows, `item_loot_template` seeds for paragon caches |

If the updater complains about a hash mismatch on
`2026_02_12_00_hotfixes.sql` or `2026_02_21_00_world.sql`, you're on a
commit older than `3537423241` / `00e9b5728d`. Pull, rebuild, retry.

### 1.3 DB2 files

Drop the following 12.0.5.67186 client extracts into
`<install>/dbc/enUS/` alongside the existing TC ones:

```
Covenant.db2
RenownRewards.db2
RenownRewardsPlunderstorm.db2
QuestLine.db2
CampaignXCondition.db2
FactionGroup.db2
CurrencyCategory.db2
UiTextureKit.db2
```

(`Campaign.db2`, `CampaignXQuestLine.db2`, `PlayerCompanionInfo.db2`,
`DelvesSeasonXSpell.db2` should already be there from warband Phase 9 or
earlier.)

### 1.4 First boot smoke checks

Watch the worldserver console. Pass if and only if you see:

```
MajorFactionMgr: indexed 20 Major Factions, N covenants, M factions
    with renown rewards in ... ms
Loaded 20 Major Faction configs in ... ms
Loaded N warband reputation factions
```

Common boot failures and what they mean:

| Symptom | Meaning |
|---|---|
| `Field <X> in <db2> must be unsigned (ParentIndexField...)` | You're on an old binary — `36316d0b27` fixes all four. Rebuild. |
| `Unknown column 'pc.guid' in 'on clause'` | Same — `0b2e7c28c1` fixes it. |
| `Hotfix locale table for storage BroadcastText.db2 references row that does not exist 308171 itIT` | Stale locale row, non-fatal. Ignore or `DELETE FROM broadcast_text_locale WHERE ID=308171 AND locale='itIT'`. |
| `Table hash 0x65F2637D points to a loaded DB2 store RenownRewards.db2, fill related table instead of hotfix_blob` | Stale `hotfix_blob` row from a pre-Phase-10A.2 import. Cosmetic; functionally TC reads from the `.db2` file. Cleanup query at the bottom of this doc. |

---

## 2. What you should see in-game

### 2.1 Renown UI opens correctly

Talk to any registered renown quartermaster (18 NPCs; full list in
`sql/updates/world/master/2026_05_16_02_world.sql`). The Journey UI
opens directly to that faction's renown panel.

Texture atlas suffixes (`MajorFaction-DragonscaleExpedition`, etc.)
render correctly — they're resolved at runtime through
`Campaign.UiTextureKitID → UiTextureKit.KitPrefix`. Empty or generic
fallback art means the chain broke; check that `Campaign.db2` and
`UiTextureKit.db2` are both in `dbc/enUS/` and the
`major_faction_config.renownCampaignId` column matches the
`Campaign.ID`.

### 2.2 Reputation drives renown levels

`.modify rep 2507 +10000` on Dragonscale Expedition should cross
**4** renown levels (2500 rep per level). For each level crossed:

1. The client renown bar advances and the level-up toast pops
   (`MAJOR_FACTION_RENOWN_LEVEL_CHANGED`).
2. The reward row(s) for that level fire from
   `RenownRewards.db2` via `MajorFactionMgr::GrantRenownLevelRewards`.
3. Each grant is recorded so re-crossing the level (rep loss + regain)
   does **not** re-grant.

### 2.3 Reward dispatch — by reward type

A single `RenownRewardsEntry` row can populate any combination of these
fields. Each non-zero field fires a distinct handler. To exercise the
matrix end-to-end:

| Field set on the row | Verify with |
|---|---|
| `ItemID` | item shows in bags (or in mail if bags full — subject "Renown Reward") |
| `SpellID` | `/run print(IsSpellKnown(<id>))` returns `true` |
| `MountID` | mount appears under Collections → Mounts |
| `TransmogID` | appearance unlocked under Wardrobe |
| `TransmogSetID` | full set unlocked |
| `TransmogIllusionID` | illusion shows in enchant illusion list |
| `CharTitlesID` | title appears in Character → Titles |
| `QuestID` | quest auto-added to the log (Dream Wardens 10.2 chapter pattern) |
| `PlayerConditionID` | reward is **skipped** if the condition fails; not an error |

`GarrFollowerID` is intentionally a no-op on 12.0.5 with a debug log
("Skipping vestigial SL covenant GarrFollower …"). Don't report it as a
miss.

### 2.4 De-duplication

Repeat the same `.modify rep 2507 +10000` after the first one. None of
the rewards should re-fire. Verify by spot-checking:

```sql
SELECT * FROM character_renown_rewards_granted WHERE characterId = <guid>;
SELECT * FROM warband_renown_rewards_granted WHERE battlenetAccountId = <bnet>;
```

`character_*` holds char-bound rewards (titles, learnable spells,
quests). `warband_*` holds the `Flags & 0x8 (AccountUnlock)` rewards —
mounts, transmog, pets — keyed by Battle.net account so alts inherit
the "already granted" state automatically.

### 2.5 Account-wide reputation (warband sync)

1. Push a character to, say, renown 8 on Council of Dornogal (2590).
2. **Log out** of that character (this triggers `Player::SaveToDB` which
   now writes `warband_reputation` thanks to the save-ordering fix).
3. Verify the row landed:
   ```sql
   SELECT * FROM warband_reputation
       WHERE battlenetAccountId = <bnet> AND faction = 2590;
   ```
   `standing` is the rep within the current level, `renownLevel` is the
   level.
4. Log in an alt on the same Battle.net account. The alt should join
   the world with the renown currency already at 8, the toast
   suppressed (because this is account-sync, not a real cross), and
   character-bound rewards granted (mounts/transmog **not** re-granted
   because they're in the collection already).

The original character must log out at least once **after** commit
`5f18d7d404`. Anything saved by a pre-fix binary never reached
`warband_reputation` and won't sync. To backfill from per-char rep
without grinding, on the characters DB:

```sql
INSERT INTO warband_reputation (battlenetAccountId, faction, standing, renownLevel)
SELECT c.battlenetAccount, r.faction, MAX(r.standing), 0
FROM character_reputation r
JOIN characters c ON c.guid = r.guid
JOIN warband_reputation_faction wrf ON wrf.factionId = r.faction
GROUP BY c.battlenetAccount, r.faction
ON DUPLICATE KEY UPDATE standing = GREATEST(standing, VALUES(standing));
```

(`renownLevel` stays 0 here — that field is sourced from the renown
currency at next save, so logging the original character once after the
backfill brings it up to date.)

### 2.6 Catchup state packet

Open the Journey UI on a low-renown alt. The server replies with
`SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` containing per-faction
catchup percentages:

```
catchupPercent = min(100, (accountMax − charRenown) × 100 / maxRenown)
```

A sniff filter for opcode `0x42030D` will show one byte length header
plus N × 8-byte (factionID, percent) records.

### 2.7 Paragon

Push rep past max renown (`.modify rep 2507 +999999`). Paragon counter
accumulates at **7,500** per cycle (verified against
`ParagonReputation.LevelThreshold`; the previous 10,000 figure in
older docs is incorrect).

Each threshold cross:
- Adds the faction's paragon reward quest to the log
  (`ParagonReputation.QuestID`).
- Turning the quest in delivers the faction's cache item.
- Opening the cache rolls loot via the seeded `item_loot_template`
  rows.

### 2.8 Campaign auto-grant + stalled tooltip

- Completing the quest pointed at by `Campaign.Completed` should
  auto-add `Campaign.RewardQuestID` to your log (Phase 10F).
- A stalled campaign's tooltip should show the localized failure-reason
  string from `CampaignXCondition.FailureReason` matching the condition
  that's not met.

---

## 3. Suggested test matrix

Run these in order on a fresh character pair (toon A = main, toon B =
alt on the same bnet account).

| # | On | Action | Expected |
|---|---|---|---|
| 1 | A | `.modify rep 2507 +2500` ×4 | Renown 1→2→3→4→5, 4 toasts, 4 reward sets granted |
| 2 | A | Repeat step 1 | NO re-grants (de-dup tables work) |
| 3 | A | `.logout` | `warband_reputation` row appears with `faction=2507, renownLevel≥5` |
| 4 | B | Log in fresh | Renown 2507 inherited at level 5, char-bound rewards granted, account-wide rewards not re-granted |
| 5 | B | Open Journey UI | Catchup percentages visible for any faction where account > char |
| 6 | A | `.modify rep 2507 +999999` | Reach cap; paragon counter accumulates at 7500/cycle; paragon quest offered each threshold |
| 7 | A | Turn in paragon quest | Cache item delivered; opening rolls loot from `item_loot_template` |
| 8 | A | Complete a campaign chapter quest (any major faction) | `Campaign.RewardQuestID` auto-added to log |
| 9 | A | View stalled campaign tooltip | Correct localized `CampaignXCondition.FailureReason` |
| 10 | A | Talk to a registered quartermaster (e.g., NPC 189226 Cataloger Jakes) | Journey UI opens directly to that faction's renown panel |
| 11 | A | Same for any TWW quartermaster (2570/2590/2594) | Journey UI opens to that faction; renown shared across alts |
| 12 | A | Cross-faction: gain rep on Severed Threads (2600) Vizier/Weaver/General weekly | Pact rotation correct across reset |

---

## 4. Known limitations (do **not** file these as bugs)

- **2 missing quartermaster NPCs**: Severed Threads (2600) and Ritual
  Sites (2792). Research data collision and PTR uncertainty
  respectively. Both faction UIs work — use
  `/run EncounterJournal_OpenToJourney(<factionId>)` from the game
  console to bring up the panel directly.
- **Paragon cache rare drops (TWW+)**: research data sometimes provided
  Mount/Pet collection IDs where `Item.db2` IDs were expected. A few
  rare drops may resolve to wrong items. Flagged inline in
  `2026_05_16_03_world.sql`.
- **Lore-specific gossip text**: all renown quartermasters use the
  generic "Greetings, champion…" broadcast text (`BroadcastText`
  233333). Per-faction quartermaster banter is out of scope for this
  branch.
- **`playerCompanionId = 0` everywhere**: `MajorFactionData.db2` is not
  in public client extracts. The Delves-companion link is fillable in a
  follow-up world-data PR once a source surfaces.
- **Two factions have no campaign**: Gallagio (raid-only) and Ritual
  Sites (aux track) — `renownCampaignId = 0` is intentional. The
  texture-kit chain returns an empty string; the client falls back to
  a generic atlas.

---

## 5. Cosmetic console-warning cleanup

These warnings are harmless but noisy in DB log channels. Run only if
they bother you:

```sql
-- Stale RenownRewards blob rows from a pre-Phase-10A.2 dump
DELETE FROM hotfix_data WHERE TableHash = 0x65F2637D;
DELETE FROM hotfix_blob WHERE TableHash = 0x65F2637D;

-- Stale BroadcastText 308171 itIT
DELETE FROM broadcast_text_locale WHERE ID = 308171 AND locale = 'itIT';
```

The first set deletes legacy blob copies of `RenownRewards.db2`
records. TC was already skipping them and reading from the `.db2` file
directly, so this is purely silencing the log. Don't run it if you
were relying on those blobs as a hotfix source — but you weren't,
because the typed `renown_rewards` table is empty on this branch and
the runtime is happy with the file.

---

## 6. Reporting

When filing a regression:

1. `git rev-parse HEAD` — should be `5f18d7d404` or a descendant.
2. Worldserver log lines from the `server.major-factions` channel
   (every grant is logged at DEBUG level — set `Log.MajorFactions=3`
   in `worldserver.conf` to capture them).
3. Packet sniff if possible (opcode 0x42030D and the renown
   currency-change SMSG are the most useful).
4. State precisely:
   - `factionId`
   - which renown level
   - account-wide vs character-bound
   - what you expected vs what happened
5. Whether the original character has logged out **at least once after
   `5f18d7d404`** (without this, `warband_reputation` will be empty and
   alt-sync tests will look broken even though they aren't).
