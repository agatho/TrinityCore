# Major Factions: Titles, Achievements, Items - Audit

Audit run 2026-05-18. Compares wowhead / warcraft.wiki.gg / local jsonexport CSVs
against the TrinityCore world DB to find seed gaps for grand-factions phase 10.

## Account-wide Achievements

**Status: implementation-complete, no SQL gap.**

TrinityCore promotes any AchievementEntry with `Flags & ACHIEVEMENT_FLAG_ACCOUNT`
into `warband_achievement` automatically (see
`src/server/game/Achievements/AchievementMgr.cpp:321-324`). The flag lives in
`Achievement.db2` (a hotfix-loaded table) and ships from the client. No
`achievement_*` migration needed for the major factions.

Migration `2026_02_21_04_characters.sql` already created the
`warband_achievement` / `warband_achievement_progress` storage.

## Renown-track Titles

There are three Blizzard-side title-grant pathways. Each maps to a different
TrinityCore world-DB site, so the gap analysis is split per pathway.

### 1. Title granted via `RenownRewards.db2` row (DB2-driven)

Two of the 20 major factions grant their renown title directly through a
`RenownRewards` row with non-zero `CharTitlesID`. Both are dispatched at
runtime by Phase 10C's `MajorFactionMgr::GrantRenownLevelRewards`. They do
**not** need world-DB seeding.

| FactionID | Faction | CovenantID | Level | RR.ID | CharTitlesID | Title |
|---:|---|---:|---:|---:|---:|---|
| 2616 | Keg Leg Thrasher (Plunderstorm) | 23 | 40 | 1265 | 810 | Plunderlord |
| 2696 | Amani Tribe (Midnight) | 37 | 20 | 1658 | 935 | Loa-Speaker |

Source: `C:\dumps\jsonexport\RenownRewards_67186.csv`.

### 2. Title granted via quest-complete (`quest_template.RewardTitle`)

The classic Dragonflight pattern. The renown-25 or renown-30 follow-up quest
sets `quest_template.RewardTitle` to the CharTitle ID; turning the quest in
calls `Player::SetTitle()`.

**Seeded in `2026_05_18_01_world.sql`** (this migration):

| FactionID | Faction | Quest ID | Quest | Title ID | Title | Renown |
|---:|---|---:|---|---:|---|---:|
| 2503 | Maruuk Centaur | 71091 | The Highest Honor | 734 | Khansguard | 25 |
| 2507 | Dragonscale Expedition | 70834 | Titled Story | 736 | Intrepid Explorer | 25 |
| 2510 | Valdrakken Accord | 70916 | Beknownst and Glorious | 735 | Ally of Dragons | 30 |
| 2511 | Iskaara Tuskarr | 70969 | Becoming One of Our Community | 737 | Of Iskaara | 30 |

All four titles are account-wide (CharTitles.Flags & 0x01); the title is
inherited by alts via the existing client-side title-share path once a single
character on the warband earns it.

### 3. Title granted via `achievement_reward` row

Not used by any of the 20 currently-shipping major factions per the data
available. The Blizzard convention since DF has been quest-driven (path 2) or
DB2-driven (path 1).

## Known Open Gaps (need verification before SQL seeding)

| FactionID | Faction | Title (per source) | Source | Why omitted |
|---:|---|---|---|---|
| 2564 | Loamm Niffen | "Smelly" @ Renown 20 (CharTitle **768** VERIFIED) | Wowhead title=768, news 332072, RR.db2 row 995 | Title ID and faction/level confirmed; awarding quest ID still unknown - RenownRewards row 995 has CharTitlesID=0, matching the DF pattern where the title comes from a quest_template.RewardTitle binding, not from RR.db2. Need to verify the quest ID before seeding. |
| 2574 | Dream Wardens | (uncertain) | local JSON only | Local C:\dumps JSON suggests a "Friend of the Wardens" achievement but neither wowhead nor warcraft.wiki.gg confirms a renown title for this faction. The Legion "the Dreamer" title (id 484) is unrelated. |
| 2570 | Hallowfall Arathi | "Champion of the Arathi" @ Renown 20 | local JSON only | No wowhead corroboration |
| 2590 | Council of Dornogal | "Stalwart of the Council" @ Renown 20 | local JSON only | No wowhead corroboration |
| 2594 | Assembly of the Deeps | "Cog-Reader of the Assembly" @ Renown 20 | local JSON only | No wowhead corroboration |
| 2600 | The Severed Threads | "Web-Weaver of Note" @ Renown 20 | local JSON only | No wowhead corroboration |
| 2653 | Cartels of Undermine | "Undermine Tycoon" @ Renown 15 | local JSON only | No wowhead corroboration |
| 2658 | K'aresh Trust | "Trusted Ethereal" @ Renown 15 | local JSON only | No wowhead corroboration |
| 2685 | Gallagio Loyalty Rewards Club | "Member of the Club" @ Renown 4 | local JSON only | No wowhead corroboration |
| 2688 | Flame's Radiance | "Sacred Flame Bearer" @ Renown 8 | local JSON only | No wowhead corroboration |
| 2699 | Singularity | "Void Scholar" @ Renown 15 | local JSON only | Midnight 12.0 - retail data pending |
| 2704 | Hara'ti | "Druid of the Mists" @ Renown 15 | local JSON only | Midnight 12.0 - retail data pending |
| 2710 | Silvermoon Court | "Friend of the Sin'dorei" @ Renown 15 | local JSON only | Midnight 12.0 - retail data pending |
| 2792 | Ritual Sites | "Site Investigator" @ Renown 5 | local JSON only | Midnight 12.0 - retail data pending |

Per the "no false data" rule, these rows are intentionally not seeded. The
local C:\dumps JSON files were drafted from a mix of datamining + speculative
inference and are flagged with `uncertain_fields[]` blocks; the safe default
is to omit them until cross-verified against a 12.0.5 retail client capture
or the published CharTitles.db2.

## Items / Quests

Existing world-data seed already covers:

* `2026_05_16_03_world.sql` - 17 paragon cache `item_loot_template` entries
  (one per faction with a confirmed cache item; Plunderstorm and Ritual Sites
  intentionally omitted - see file header).
* `2026_05_16_04_world.sql` - quartermaster gossip menus (18 of 20 NPCs;
  Severed Threads + Ritual Sites omitted due to research data collisions).
* `2026_05_16_06_world.sql` - 19 weekly community quest pools.

What's still missing (out of scope for this audit, tracked in
`MAJOR_FACTIONS_PLAN.md` 10H follow-ups):

* `creature_questender` / `creature_queststarter` rows for renown vendor
  NPCs and campaign chapter quest-givers.
* `npc_text` entries for renown vendor gossip - currently using generic
  BroadcastTextID 233333.
* `creature_loot_template` for world-quest / treasure / bonus-objective rep
  drops (per the master plan: deliberately staged into per-zone follow-up
  branches, not phase-10 scope).

## Migrations Summary (this branch)

| File | Phase | Scope |
|---|---|---|
| `2026_05_16_00_world.sql` | 10D | `warband_reputation_faction` rows (account-wide rep) |
| `2026_05_16_01_world.sql` | 10H | `major_faction_config` (20 rows) |
| `2026_05_16_02_world.sql` | 10H | `major_faction_renown_npc` (18 rows) |
| `2026_05_16_03_world.sql` | 10I | Paragon cache `item_loot_template` (17 factions) |
| `2026_05_16_04_world.sql` | 10H | Quartermaster gossip wiring (18 menus) |
| `2026_05_16_06_world.sql` | 10J | Weekly community quest pools (19 pools) |
| `2026_05_18_00_world.sql` | 10H | Add `introQuestIdAlliance` column to `major_faction_config` |
| `2026_05_18_01_world.sql` | 10H | **THIS** - quest title-reward UPDATEs (4 DF majors) |
