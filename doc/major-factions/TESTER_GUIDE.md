# Phase 10 — Major Factions: Tester Guide

Branch: `origin/major-factions` @ `ef249e50e0`

## Setup

1. **Build from this branch.** It sits on top of warband Phases 1–9 (rebased onto `warband/phase9-delves`). Building from plain `master` will produce false-negative bug reports because the account-wide reputation column will be missing.
2. **Apply SQL migrations**, in order:
   - `sql/updates/hotfixes/master/2026_05_15_0{0,1,2,3}_hotfixes.sql` — 7 new DB2 hotfix tables (`covenant`, `renown_rewards{,_plunderstorm}`, `quest_line`, `campaign_x_condition`, `faction_group`, `currency_category`).
   - `sql/updates/hotfixes/master/2026_05_16_00_hotfixes.sql` — `ui_texture_kit`.
   - `sql/updates/characters/master/2026_05_16_00_characters.sql` — 2 reward-grant tracking tables.
   - `sql/updates/world/master/2026_05_16_0{0..6}_world.sql` — world data seed for all 20 Major Factions.
3. **Pull DB2 files** from the WoW 12.0.5 client into `dbc/enUS/`. Required: `Covenant.db2`, `RenownRewards.db2`, `RenownRewardsPlunderstorm.db2`, `QuestLine.db2`, `CampaignXCondition.db2`, `FactionGroup.db2`, `CurrencyCategory.db2`, `UiTextureKit.db2`.
4. **Start worldserver.** The boot log should report:
   - `MajorFactionMgr: indexed 20 Major Factions, N covenants, M factions with renown rewards in … ms`
   - `Loaded 20 Major Faction configs in … ms`

## What you should see in-game

### Renown UI opens correctly
- Talk to any registered renown quartermaster (18 NPCs seeded; full list in `sql/updates/world/master/2026_05_16_02_world.sql`). The Journey UI opens to that faction's renown panel.
- Texture atlas suffixes (`MajorFaction-DragonscaleExpedition` etc.) render correctly — they resolve through `Campaign.UiTextureKitID → UiTextureKit.KitPrefix` at runtime.

### Reputation drives renown
- `.modify rep 2507 +10000` should cross **4** renown levels for Dragonscale Expedition (2500 rep per level). Each level should:
  - Fire client toast `MAJOR_FACTION_RENOWN_LEVEL_CHANGED`.
  - Grant the level's `RenownRewards` row(s): item to inventory (or mail if full), spell learned, mount/transmog/title added to collection, etc.
  - Reward grants are **deduplicated** — re-crossing a level does not re-grant.

### Account-wide reputation
- Log in a high-renown character. Note the level.
- Log in an alt on the same Battle.net account. The alt should inherit account-max renown.
- **Account-wide rewards** (Flags & 0x8 — mounts, pets, transmog appearances) are **not** re-granted; the alt already has them in their collection. **Character-bound rewards** (Crafter's Knowledge, certain spells) **are** granted to the alt.

### Paragon
- Push reputation past Renown cap. Paragon counter accumulates at 7 500 per cycle.
- Crossing a paragon threshold should grant the paragon-reward quest (per `ParagonReputation.db2` row).
- Turning that quest in delivers the faction's cache item; opening the cache rolls loot via `item_loot_template`.

### Campaign / story
- Completing a campaign's "Completed" quest auto-grants the `Campaign.RewardQuestID` (Phase 10F).
- Stalled campaigns surface the correct localized failure-reason string from `CampaignXCondition.FailureReason`.

### Catchup
- Open the Journey UI on a low-renown alt. The server replies with `SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` containing per-faction catchup percentages, computed as `min(100, (accountMax − charRenown) × 100 / maxRenown)`.

## Suggested test matrix

| # | Test | Expected |
|---|---|---|
| 1 | `.modify rep 2507 +2500` ×4 | Renown 1→2→3→4→5, 4 toasts, 4 reward sets granted |
| 2 | Repeat step 1 | NO re-grants (de-dup table works) |
| 3 | Log alt | Renown level inherited; account-wide rewards already in collection |
| 4 | `.modify rep 2507 +999999` | Reach cap, paragon accumulates, paragon quest offered every 7 500 |
| 5 | Talk to Cataloger Jakes (NPC 189226) | Journey UI opens to Dragonscale (faction 2507) |
| 6 | Talk to Quartermaster for any TWW faction (2570/2590/2594/2600) | Journey UI opens to that faction; renown shared across alts |
| 7 | Open Journey UI as low-renown alt | Catchup percent visible per faction; rep gain accelerated until parity |
| 8 | Complete a campaign chapter on `Campaign.Completed` quest | `Campaign.RewardQuestID` auto-added to log |
| 9 | View stalled campaign tooltip | Shows the correct CampaignXCondition.FailureReason string |
| 10 | Cross-faction: gain rep on one Severed Threads Pact (Vizier/Weaver/General weekly) | Pact rotates correctly weekly |

## Known limitations

These are flagged in commit messages; **report them only if you see incorrect behavior beyond what's noted**:

- **2 missing quartermaster NPCs:** Severed Threads (2600) and Ritual Sites (2792) — research data collision and PTR uncertainty respectively. Both faction UIs work, but no in-world NPC opens them; use `/run EncounterJournal_OpenToJourney(<factionId>)` to bring up the panel manually.
- **Paragon cache rare drops (TWW+):** the research data provided Mount/Pet collection IDs where Item.db2 IDs were expected — some rare drops may resolve to wrong items. Flagged in `2026_05_16_03_world.sql` row comments.
- **Lore-specific gossip text:** all renown quartermasters use a generic "Greetings, champion…" broadcast text (BroadcastText 233333). Per-faction quartermaster banter is out of scope for this branch.
- **`playerCompanionId`** is zero on every config row — `MajorFactionData.db2` is not in public extracts; the Delves-companion link is fillable in a follow-up world-data PR once a source surfaces.
- **Two factions have no associated campaign** (Gallagio raid-only, Ritual Sites aux track) — `renownCampaignId = 0` is intentional. `GetTextureKitPrefix` returns an empty string for them; the client falls back to a generic atlas.

## Reporting

When filing a regression:
1. Capture branch tip (`git rev-parse HEAD`) — should be `ef249e50e0` or a descendant.
2. Capture relevant packet sniff if possible.
3. State which faction (`factionId`), which renown level, account-wide vs character-bound, and what you expected vs what happened.
4. Include the `Server.major-factions` log channel output (the dispatch code logs every grant at DEBUG).
