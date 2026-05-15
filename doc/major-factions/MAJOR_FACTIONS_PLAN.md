# Phase 10 — Major Factions & Campaigns: Master Implementation Plan

## Scope

Complete, 100% blizzlike implementation of the **Major Factions** system (introduced in Dragonflight 10.0, refined through The War Within 11.x and Midnight 12.0) and the **Campaign** quest-chapter overlay for TrinityCore. Target build **12.0.5.67186** ("Midnight").

> **Phase 10 of the Warband series.** This work **requires Phases 1–9** of the warband chain to be present (account-wide reputation, account-wide achievements, PlayerCompanionInfo DB2, etc.). Testers must build from a branch that contains the full warband chain. This branch (`major-factions`) is rebased onto `origin/warband/phase9-delves` for exactly this reason.

The user-facing system consists of:
- **20 Major Factions** (Dragonscale Expedition, Iskaara Tuskarr, Maruuk Centaur, Valdrakken Accord, Loamm Niffen, Dream Wardens, Hallowfall Arathi, Council of Dornogal, Assembly of the Deeps, Severed Threads, Cartels of Undermine, K'aresh Trust, Gallagio Loyalty Rewards Club, Flame's Radiance, Amani Tribe, Singularity, Hara'ti, Silvermoon Court, Ritual Sites, Keg Leg Thrasher; see §2.1 for full ID list).
- Per-faction **Renown track** (levels 1..30, level rewards from `RenownRewards.db2`).
- Per-faction **Paragon** overlay (10 000-rep cache loop above max renown).
- Per-faction **Campaign** (multi-chapter story quest line, `Campaign` / `CampaignXQuestLine` / `QuestLine` DB2 chain).
- **Account-wide reputation** for 11.x+ majors (already in place from warband Phase 4; we only register new factionIDs).
- **Renown quartermaster UI** opened by `PlayerInteractionType::MajorFactionRenown` (value **64**), routes to the `Blizzard_EncounterJournal` "Journeys" tab.
- **Renown catch-up** for alts via the existing covenant catchup pipe (`SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE`, write impl currently missing).

---

## Architectural Findings That Shape the Plan

The research phase (six parallel agents covering UI Lua, DB2 schema, IDA decomp, retail sniffs, web design, and current TC state) produced **four findings** that directly determine the implementation strategy:

1. **No dedicated Major-Faction opcodes exist.** Both IDA decomp of the 12.0.5.67186 client and retail packet sniffs agree: every `C_MajorFactions.*` Lua function resolves to **DB2 lookups + classic reputation packets**. The `MajorFactionData` table (19 fields) the UI consumes is built **client-side** from `Faction.db2` + `Covenant.db2` + `RenownRewards.db2` + the player's `FactionState`. No new packet schema is required. (See `C:\dumps\MAJORFACTIONS_HANDLERS_67186.md` and `C:\dumps\MAJORFACTIONS_SNIFF_67186.md`.)
2. **Major-Faction identity is `ParagonFactionID != 0 AND RenownCurrencyID != 0`.** No single `Faction.Flags` bit marks the set — 2616 *Keg Leg Thrasher* has `Flags=0` and is still a renown track (Plunderstorm). The 20-faction list is data-driven from `Faction.db2`. (See `C:\dumps\MAJORFACTIONS_DB2_67186.md` §2.)
3. **The RenownRewards track is keyed by `CovenantID`, not `FactionID`.** The mapping `RenownRewards.CovenantID → Covenant.ID → Covenant.FactionID / Covenant.CurrencyTypesID` is canonical. We must load `Covenant.db2` (currently MISSING) or hard-code the mapping — the plan loads the DB2.
4. **Warband Phase 4 already ships exactly the account-wide-reputation persistence we need.** `warband_reputation(battlenetAccountId, faction, standing, renownLevel)`, `warband_reputation_faction` config, `ReputationMgr::LoadAccountWideFromDB` / `SaveAccountWideToDB`, `ObjectMgr::IsWarbandReputationFaction`, login query, prepared statements — all done. Phase 10 only **adds rows** to `warband_reputation_faction` for the 10.x majors that retroactively became account-wide. The level-up math (`ReputationMgr.cpp:566–590`) already converts excess reputation into renown currency via `_player->ModifyCurrency(CurrencyGainSource::RenownRepGain, …)`.

> **Implication.** Phase 10 is overwhelmingly a **data-binding and DB2 problem**, not a packet-engineering problem. The bulk of the work is: (a) load 7 missing DB2 tables, (b) build a `MajorFactionMgr` that exposes the right queries the rest of the server needs, (c) wire renown-reward grants on level-up, (d) honor `Campaign.RewardQuestID` on campaign completion, (e) implement the one missing packet writer (`SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE`), (f) seed world data (per-faction config + scripts).

---

## Sub-phase Overview

| Sub-phase | Name | Focus | Depends On |
|----------|------|-------|------------|
| **10A** | DB2 Infrastructure | Add 7 missing DB2 structs/LoadInfos/Stores | Phase 9 (DB2 add-pattern proven) |
| **10B** | MajorFactionMgr | Server-authoritative read model for the 20 majors | 10A |
| **10C** | Renown Reward Dispatch | On-level-up grant of items/spells/mounts/transmog/titles/companions | 10A, 10B, warband Phase 7 (account-wide achievements collateral), warband Phase 9 (PlayerCompanionInfo) |
| **10D** | Account-wide Reputation Expansion | Register DF (10.x) majors in `warband_reputation_faction`; fix the level-tiebreaker for 10.x retro account-wide | warband Phase 4 |
| **10E** | Renown Catch-up Writer | Implement `SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` (opcode exists, body missing) | 10B, 10D |
| **10F** | Campaign Chapter Wiring | Honor `Campaign.RewardQuestID`, send `SMSG_QUEST_LINE_INFO`, surface `QuestLine.Name/Description`, `CampaignXCondition` stall-tooltip | 10A (QuestLine, CampaignXCondition) |
| **10G** | PlayerInteractionType::MajorFactionRenown | Allow value 64; set `C_MajorFactions.GetRenownNPCFactionID()` state; clear on close | 10B |
| **10H** | Per-Faction Script Seed | World-DB rows + zone scripts for at least one fully-blizzlike pilot faction (Dragonscale Expedition end-to-end) | 10A–10G |
| **10I** | Paragon Reward Cache Loot Template | Per-faction paragon cache loot with rare-tier (mounts/pets/toys) drop chances | 10A, 10B |
| **10J** | Weekly Reset Hooks | Major-Faction weekly catch-up quests, weekly renown cap, weekly community quests | 10D |
| **10K** | Tests & Smoke Scripts | Catch2 unit tests + smoke-test script for in-game verification on `M:\FeatureServer` | all |

Each sub-phase is independently committable. Recommended commit-message prefix: `feat(major-factions): 10X - <imperative>` to fit the warband series cadence.

---

## §1. Reference Material (research artifacts produced this branch)

All paths below contain authoritative research output. **Read these before writing code in a given sub-phase** — they contain the exact `.dbd` line ranges, IDA EAs, sniff hex dumps, Lua file:line citations.

| Artifact | Authoritative for | Owner |
|----------|-------------------|-------|
| `C:\dumps\MAJORFACTIONS_LUA_INDEX_67186.md` | Every `C_MajorFactions.*` / `C_Reputation.*` / `C_CampaignInfo.*` call, exact returned-table shape, every consumer file:line. 19-field `MajorFactionData` schema. | UI Lua trace |
| `C:\dumps\MAJORFACTIONS_DB2_67186.md` | 19-table catalog, .dbd block line ranges for build 67186, drop-in C++ for the 7 missing tables, 20-major-faction roster. | DB2 catalog |
| `C:\dumps\MAJORFACTIONS_HANDLERS_67186.md` + `.json` | IDA decomp of every reputation/renown/campaign handler in the 12.0.5.67186 client. Proves no dedicated MF opcodes exist. | IDA decomp |
| `C:\dumps\MAJORFACTIONS_SNIFF_67186.md` + `.json` | Wire format of `SMSG_INITIALIZE_FACTIONS` (two parallel arrays), `SMSG_FACTION_BONUS_INFO`, `SMSG_SET_FACTION_STANDING`. Decoded `Faction.Flags` bits. Quest-complete vs rep-update ordering invariant. | Sniff survey |
| `C:\dumps\MAJORFACTIONS_DESIGN_67186.md` | Player-facing design: every faction's renown cap, paragon, account-wide flag, intro quest, theme. Hotfix history. Reward-category taxonomy. | Web/wowhead |
| Audit summary (TC state) | What master+warband Phases 1–9 already provide vs what's missing. | TC audit |

> **Note:** the TC audit agent's docs file (`docs/MAJOR_FACTIONS_TC_AUDIT.md`) was accidentally deleted during the warband-phase-9 rebase pre-clean. Re-run that audit if a written copy is required — the executive summary survives in the conversation transcript and is reproduced in §3 below.

---

## §2. Identity, Constants, and DB2 Topology

### 2.1 The 20 Major Factions (build 67186)

`Faction.ParagonFactionID != 0 AND Faction.RenownCurrencyID != 0` ⇒ exactly:

| FactionID | Name | Paragon | RenownCurrency | Expansion | TC Flags | Acct-wide today? |
|---:|------|---:|---:|---:|---:|---:|
| 2503 | Maruuk Centaur | 2504 | 2002 | 9 (DF) | 6 | retro TWW |
| 2507 | Dragonscale Expedition | 2508 | 2021 | 9 | 6 | retro TWW |
| 2510 | Valdrakken Accord | 2552 | 2088 | 9 | 6 | retro TWW |
| 2511 | Iskaara Tuskarr | 2551 | 2087 | 9 | 6 | retro TWW |
| 2564 | Loamm Niffen | 2565 | 2402 | 9 (10.1) | 6 | retro TWW |
| 2574 | Dream Wardens | 2575 | 2653 | 9 (10.2) | 6 | retro TWW |
| 2570 | Hallowfall Arathi | 2611 | 2901 | 10 (TWW) | 4 | **yes (Phase 4)** |
| 2590 | Council of Dornogal | 2612 | 2900 | 10 | 4 | **yes (Phase 4)** |
| 2594 | The Assembly of the Deeps | 2613 | 2898 | 10 | 6 | **yes (Phase 4)** |
| 2600 | The Severed Threads | 2596 | 2904 | 10 | 6 | **yes (Phase 4)** |
| 2653 | The Cartels of Undermine | 2667 | 3120 | 10 (11.1) | 6 | yes |
| 2658 | The K'aresh Trust | 2659 | 3128 | 10 (11.2) | 6 | yes |
| 2685 | Gallagio Loyalty Rewards Club | 2684 | 3315 | 10 (11.2) | 6 | yes |
| 2688 | Flame's Radiance | 2689 | 3140 | 10 (11.1) | 6 | yes |
| 2616 | Keg Leg Thrasher | 2604 | 2814 | 9 | **0** | n/a (Plunderstorm) |
| 2696 | Amani Tribe | 2705 | 3355 | 11 (Midnight) | 6 | yes |
| 2699 | The Singularity | 2725 | 3388 | 11 | 6 | yes |
| 2704 | Hara'ti | 2726 | 3369 | 11 | 6 | yes |
| 2710 | Silvermoon Court | 2727 | 3371 | 11 | 6 | yes |
| 2792 | Ritual Sites | 2793 | 3428 | 11 | 6 | yes |

> Identity rule encoded in `MajorFactionMgr::IsMajorFaction(uint32 factionId)` — does not depend on Flags bit; mirrors the client's own rule. Plunderstorm faction (2616) is handled specially via the `RenownRewardsPlunderstorm` track and is excluded from the warband-shared set.

### 2.2 Constants confirmed from research

- **Paragon cycle:** every row in `ParagonReputation.db2` has `LevelThreshold = 10000`. Cache-quest auto-grant on `state.Standing % 10000 == 0` is already implemented in `ReputationMgr` (paragon overflow + reward-quest dispatch). See `ReputationMgr.cpp` paragon block.
- **Renown level cost:** 2 500 reputation per level (constant across all DF/TWW/Midnight majors). Confirmed by inspecting `RenownRewards.Level` distribution per `CovenantID` and the existing `ReputationMgr.cpp:566–590` renown-currency-on-overflow loop.
- **Renown caps:** per-faction. Source = `CurrencyTypes.MaxQty` of the matching `RenownCurrencyID`. Ranges in dataset: 8 (Ritual Sites), 20 (most 10.2+/Plunderstorm), 25 (TWW core), 30 (Tuskarr, Valdrakken).
- **`Faction.Flags` decoded** (from sniff): `0x01 Visible`, `0x02 AtWar`, `0x04 Hidden`, `0x08 Invisible`, `0x10 Peace`, `0x40 Inactive/Rival`, `0x80 Header`, upper bits Major-Faction-only: **`0x800` inferred = AccountWideReputation, `0x1000` = freshly unlocked** (verified on Severed Threads first-unlock capture). The MajorFaction discriminator bit is `0x04` (a.k.a. *Hidden* in the legacy bitset name — TC already uses this position; rename in plan §10B.4).
- **PlayerInteractionType::MajorFactionRenown = 64.** Already declared in `Enum.PlayerInteractionType`; need to allow this value in `WorldSession::HandlePlayerInteractionInfo` (already opened for warband via `AccountBanker`).
- **No `MajorFactionLevels.db2` exists** — the sniff agent's reference to `FactionRenownLevels.db2` is a misnomer; per-level data lives in `RenownRewards.db2` (`Level` column) and the renown-cap lives in `CurrencyTypes.MaxQty`.
- **`SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` (`0x42030D`):** opcode is defined in TC `Opcodes.h` but no write implementation exists. The IDA decomp shows the body is a length-prefixed blob of per-faction `{factionID(u32), percent(u8)}` records (exact stride to confirm with a synthetic capture).

### 2.3 DB2 catalog summary (full detail in `C:\dumps\MAJORFACTIONS_DB2_67186.md`)

| Status | Tables |
|--------|--------|
| **PRESENT in TC** | `Faction`, `FactionTemplate`, `FriendshipReputation`, `FriendshipRepReaction`, `ParagonReputation`, `Campaign`, `CampaignXQuestLine`, `QuestLineXQuest`, `QuestV2`, `QuestFactionReward`, `CurrencyTypes` (one minor field rename), `CurrencyContainer` (one minor field rename) |
| **MISSING (no struct/LoadInfo/store, only Meta shell)** | `RenownRewards`, `RenownRewardsPlunderstorm`, `Covenant`, `CampaignXCondition`, `QuestLine`, `FactionGroup`, `CurrencyCategory` |

The drop-in `struct` + `LoadInfo` C++ for all 7 missing tables is already drafted in `C:\dumps\MAJORFACTIONS_DB2_67186.md` §4.1–§4.8. Sub-phase 10A consists entirely of pasting these into the DB2 files and adding the `LOAD_DB2` calls + Hotfix selectors.

---

## §3. What TrinityCore Currently Has That We Lean On

From the TC audit (executive summary preserved):

- **`ReputationMgr` (`src/server/game/Reputation/ReputationMgr.{h,cpp}`)** — per-character reputation storage (`character_reputation`), threshold logic, friendship-rep + paragon rank tables, and a working **renown skeleton** that already piggybacks on `FactionEntry::RenownCurrencyID` (`ReputationMgr.cpp:566–590` converts excess reputation into renown-currency level-ups and back-spills via `_player->ModifyCurrency(CurrencyGainSource::RenownRepGain, ...)`). Paragon roll-up correctly (rolling cap, auto-grant of reward quest, child→parent spill).
- **`QuestMgr` (`src/server/game/Quests/QuestMgr.cpp:27–222`)** — already builds Campaign→QuestLine→Quest indexes; exposes `IsQuestLineCompletedByPlayer`, `IsCampaignCompletedByPlayer`, `GetQuestLineStatsForPlayer`, skip helpers. The map-overlay packet `SMSG_UI_MAP_QUEST_LINES_RESPONSE` works end-to-end.
- **`QuestDef.h:751–754`** — `RewardFactionId/Value/Override/CapIn` plumbing on quests.
- **`ScriptMgr.h:1225`** — `OnPlayerReputationChange` hook.
- **`BattlePetMgr` (`auth.battle_pets.battlenetAccountId`)** — proven account-shared persistence template; warband Phase 4 mirrors it.
- **Warband Phase 4** — `warband_reputation` characters table + `warband_reputation_faction` config + `ReputationMgr::Load/SaveAccountWideFromDB` + `ObjectMgr::IsWarbandReputationFaction()` + `_accountReputation` cache + login query + prepared statements `CHAR_SEL_ACCOUNT_REPUTATION` / `CHAR_REP_ACCOUNT_REPUTATION`.
- **Warband Phase 7** — `warband_achievement` + `warband_achievement_progress` tables (Major-Faction unlock achievements can ride this).
- **Warband Phase 9** — `PlayerCompanionInfoEntry` DB2 struct + LoadInfo + Store (the UI Lua trace flagged `MajorFaction.playerCompanionID` as a 12.0.5 field that references this exact table; Delves-companion factions like Brann's render through this).
- **Warband currency transfer** — renown rewards include currency grants; transfer plumbing already exists.

The **single biggest gap** is conceptual: there is no `MajorFactions` module. The renown the client sees today is "currency value pretending to be renown" via `RenownCurrencyID`, not a server-authoritative MajorFactions track. Sub-phase 10B builds the missing module.

---

## §4. Sub-phase Detail

### 10A — DB2 Infrastructure (foundation for everything else)

**Goal:** load all 7 missing DB2s. Build target: every `LOAD_DB2(...)` succeeds against client `dbc/enUS/*.db2`, hotfix tables exist and ingest cleanly.

**Files to touch:**

1. `src/server/game/DataStores/DB2Structure.h` — paste 7 structs (alphabetic insertion). Templates already in `C:\dumps\MAJORFACTIONS_DB2_67186.md` §4.
   - `CampaignXConditionEntry` (after `CampaignXQuestLineEntry` at line ~605)
   - `CovenantEntry` (alphabetic — after `CountryEntry` if present, else before `CreatureFamilyEntry`)
   - `CurrencyCategoryEntry` (before `CurrencyContainerEntry` at line ~1429)
   - `FactionGroupEntry` (between `FactionEntry` and `FactionTemplateEntry` ~line 1700)
   - `QuestLineEntry` (before `QuestLineXQuestEntry` at line ~3522)
   - `RenownRewardsEntry` (alphabetic, ~line 3585 between `RandPropPointsEntry` and `RewardPackEntry`)
   - `RenownRewardsPlunderstormEntry` (immediately after `RenownRewardsEntry`)
2. `src/server/game/DataStores/DB2LoadInfo.h` — paste 7 `LoadInfo` structs (alphabetic).
3. `src/server/game/DataStores/DB2Stores.h` — 7 new `extern DB2Storage<...>` declarations.
4. `src/server/game/DataStores/DB2Stores.cpp` — 7 storage definitions + 7 `LOAD_DB2(...)` calls inside `DB2Manager::LoadStores`. Alphabetic order matters for diff hygiene.
5. `src/server/database/Database/Implementation/HotfixDatabase.h` — 7 new enum values:
   - `HOTFIX_SEL_CAMPAIGN_X_CONDITION`
   - `HOTFIX_SEL_COVENANT`
   - `HOTFIX_SEL_CURRENCY_CATEGORY`
   - `HOTFIX_SEL_FACTION_GROUP`
   - `HOTFIX_SEL_QUEST_LINE`
   - `HOTFIX_SEL_RENOWN_REWARDS`
   - `HOTFIX_SEL_RENOWN_REWARDS_PLUNDERSTORM`
6. `src/server/database/Database/Implementation/HotfixDatabase.cpp` — 7 `PrepareStatement` calls. Mirror existing patterns: `RewardPack`-style for 1-locstring (`FactionGroup`, `CurrencyCategory`), `Covenant`-style for 2-locstrings, `RenownRewards`-style for 3-locstrings.
7. `sql/updates/hotfixes/master/<DATE>_00_hotfixes.sql` — `CREATE TABLE` for 7 hotfix tables. Use the warband Phase 9 hotfix SQL (`2026_02_22_00_hotfixes.sql`) as template — that one creates `delves_season`, `delves_season_x_spell`, `player_companion_info` and is the most recent multi-table addition.

**Minor renames included** (low-risk, grepability):
- `CurrencyTypesEntry::AccountTransferPercentage` → `WarbondTransferPercentage` (12.0.5 wago canonical).
- `CurrencyContainerEntry::CurrencyTypesID` → `CurrencyTypeID` (singular, wago canonical).
- `FriendshipReputationEntry::StandingChanged` → `StandingChangedText` (wago canonical).

**Validation:** `worldserver` startup logs report `Loaded X RenownRewards entries` etc., counts match `C:\dumps\jsonexport\<Table>_67186.csv` (RenownRewards = 1318, Covenant = 31, QuestLine = 1648, etc.).

**Commit candidates:**
- `feat(major-factions): 10A.1 - add Covenant.db2 (required for renown-track FK chain)`
- `feat(major-factions): 10A.2 - add RenownRewards.db2 and RenownRewardsPlunderstorm.db2`
- `feat(major-factions): 10A.3 - add QuestLine.db2 and CampaignXCondition.db2 for campaign UI`
- `feat(major-factions): 10A.4 - add FactionGroup.db2 and CurrencyCategory.db2`
- `chore(db2): rename CurrencyTypes/Container/FriendshipReputation fields to wago canonical names`

---

### 10B — MajorFactionMgr (server-authoritative read model)

**Goal:** a single module that knows, for any (player, factionID), what the client's `C_MajorFactions.*` calls expect. Pure reads — no persistence beyond what already exists in `ReputationMgr` / `character_reputation` / `warband_reputation`.

**New module:** `src/server/game/MajorFactions/MajorFactionMgr.{h,cpp}` (mirrors `src/server/game/Reputation/`).

**Registered in:** `src/server/game/CMakeLists.txt` (alphabetic), `PrecompiledHeaders/gamePCH.h` if needed.

**Class:** `class TC_GAME_API MajorFactionMgr` — process-wide singleton (`MajorFactionMgr::Instance()`). Loaded by `World::SetInitialWorldSettings()` after `sDB2Manager.LoadStores()`. Owns no per-player state — the per-player projection comes from `ReputationMgr`.

**Indexes built on load:**

```cpp
// All maps are read-only after LoadFromDB2; safe for concurrent reads.

// faction.ID  -> faction.ID (canonical -> covenant link helper)
std::unordered_map<uint32, uint32>                             _factionToCovenant;       // FactionID -> CovenantID

// covenant.ID -> faction.ID
std::unordered_map<uint32, uint32>                             _covenantToFaction;       // reverse

// covenant.ID -> sorted RenownRewardsEntry* list (by Level asc, then UiOrder)
std::unordered_map<uint32, std::vector<RenownRewardsEntry const*>>  _renownRewardsByCovenant;

// faction.ID  -> max renown level for that faction (computed from RenownRewards.Level max)
std::unordered_map<uint32, uint32>                             _maxRenownByFaction;

// faction.ID  -> faction's renown currency ID (cached from Faction.RenownCurrencyID)
std::unordered_map<uint32, uint32>                             _renownCurrencyByFaction;

// faction.ID  -> ParagonReputationEntry* (cached from sParagonReputationStore by ParagonFactionID)
std::unordered_map<uint32, ParagonReputationEntry const*>     _paragonByFaction;

// faction.ID  -> faction's Covenant row (for BountySetID, PlayerCompanionID via PlayerCompanionInfo, etc.)
std::unordered_map<uint32, CovenantEntry const*>              _covenantByFaction;

// Set of all 20 major faction IDs (for IsMajorFaction)
std::unordered_set<uint32>                                     _majorFactions;
```

**Public API:**

```cpp
class MajorFactionMgr
{
public:
    static MajorFactionMgr* instance();
    void Load();                                                  // call once after DB2 load

    // Identity ---------------------------------------------------------------
    bool IsMajorFaction(uint32 factionId) const;                  // ParagonFactionID && RenownCurrencyID
    std::vector<uint32> const& GetMajorFactionIDs() const;        // stable order: by Faction.ID
    std::vector<uint32> GetMajorFactionIDsForExpansion(uint8 exp) const;  // 9/10/11 -> filtered list

    // Renown -----------------------------------------------------------------
    uint32 GetMaxRenownLevel(uint32 factionId) const;             // RenownRewards.Level max for the faction's covenant
    uint32 GetRenownCurrencyID(uint32 factionId) const;           // alias to FactionEntry::RenownCurrencyID
    uint32 GetRenownLevelOfPlayer(Player const* player, uint32 factionId) const;   // wraps existing ReputationMgr::GetRenownLevel
    uint32 GetReputationToNextRenownLevel() const { return RENOWN_REPUTATION_PER_LEVEL; }  // 2500 (constant)
    uint32 GetReputationEarnedThisLevel(Player const* player, uint32 factionId) const;     // standing % 2500

    // Rewards ----------------------------------------------------------------
    std::vector<RenownRewardsEntry const*> GetRenownRewardsForLevel(uint32 factionId, uint32 renownLevel) const;
    std::vector<RenownRewardsEntry const*> GetAllRenownRewards(uint32 factionId) const;
    RenownRewardsEntry const* FindRenownReward(uint32 renownRewardId) const;

    // Campaign / journey
    CovenantEntry const* GetCovenantForFaction(uint32 factionId) const;            // for BountySetID, etc.
    uint32 GetCompanionForFaction(uint32 factionId) const;                          // 0 if not a Delves-companion faction

    // Paragon
    ParagonReputationEntry const* GetParagonForMajorFaction(uint32 factionId) const;

    // Unlock state (server-side mirror of MajorFactionData.isUnlocked)
    bool IsUnlockedForPlayer(Player const* player, uint32 factionId) const;        // ties to FactionState visibility / unlock-quest completion
    bool IsAccountWide(uint32 factionId) const;                                     // ObjectMgr::IsWarbandReputationFaction
    bool IsHiddenFromExpansionPage(uint32 factionId) const;                         // world-data column (see 10G)
    bool ShouldDisplayAsJourney(uint32 factionId) const;                            // world-data column
    bool ShouldUseJourneyRewardTrack(uint32 factionId) const;                       // world-data column

private:
    void IndexFactions();                                          // walk sFactionStore for ParagonFactionID && RenownCurrencyID
    void IndexCovenants();                                         // walk sCovenantStore, build both directions
    void IndexRenownRewards();                                     // walk sRenownRewardsStore, group by CovenantID, sort by Level asc + UiOrder asc
    void IndexParagons();                                          // walk sParagonReputationStore, build _paragonByFaction by Faction.ParagonFactionID
    void LoadWorldData();                                          // load `major_faction_config` table (see 10G)

    // …per-faction config (world DB, see 10G.1)
    struct MajorFactionConfig
    {
        bool   HiddenFromExpansionPage   = false;
        bool   DisplayAsJourney          = false;
        bool   UseJourneyRewardTrack     = false;
        bool   UseJourneyUnlockToast     = false;
        int32  UiPriority                = 0;
        uint32 IntroQuestID              = 0;
        std::string TextureKit;          // override for Faction-row textureKit if needed
    };
    std::unordered_map<uint32, MajorFactionConfig> _configByFaction;
};

#define sMajorFactionMgr MajorFactionMgr::instance()
```

**Constants:**
- `RENOWN_REPUTATION_PER_LEVEL = 2500` — class-level `constexpr`. Confirm by inspecting `ReputationMgr::IsRenownReputation` and the renown-currency overflow loop; the constant lives there today, lift it into a `SharedDefines.h` enum entry `RENOWN_REPUTATION_PER_LEVEL`.

**Hook from `ReputationMgr`:** replace the inline check `IsRenownReputation(factionEntry)` (`ReputationMgr.cpp:266`) with `sMajorFactionMgr->IsMajorFaction(factionEntry->ID)`. Keep backward-compat for the existing currency-overflow path — it's correct, just rename the predicate.

**Validation tests:** `tests/game/MajorFactions/test_majorfaction_mgr.cpp` (Catch2):
- `[MajorFactions] enumerates exactly 20 factions`
- `[MajorFactions] Dragonscale Expedition has 60 RenownRewards rows`
- `[MajorFactions] paragon factionID of Iskaara Tuskarr is 2551`
- `[MajorFactions] reputation per level == 2500 for every major`

**Commit:** `feat(major-factions): 10B - introduce MajorFactionMgr read-model singleton`.

---

### 10C — Renown Reward Dispatch

**Goal:** when a player crosses a renown level (from ReputationMgr's currency-overflow path), grant every `RenownRewards` row for that `(covenantID, level)` according to its `RewardType` / explicit field set, **without** double-granting if the level is recrossed and **respecting account-wide flag for collectibles**.

**Wire-up point:** `ReputationMgr.cpp:566–590` — the existing loop that calls `_player->ModifyCurrency(CurrencyGainSource::RenownRepGain, …)` when standing overflows. Immediately after the `ModifyCurrency` call (which returns the new currency total = new renown level), call:

```cpp
sMajorFactionMgr->GrantRenownLevelRewards(_player, factionEntry->ID, newRenownLevel, oldRenownLevel);
```

**Implementation:** new method on `MajorFactionMgr`:

```cpp
void GrantRenownLevelRewards(Player* player, uint32 factionId, uint32 newLevel, uint32 oldLevel) const;
```

Walks all `RenownRewards` rows with `Level > oldLevel && Level <= newLevel` (handles multi-level jumps from catch-up), sorted by `(Level asc, UiOrder asc)`. For each row, dispatches by populated reward field. Order matters: server should grant rewards then fire the level-up toast event — but as established in §10B the client already fires `MAJOR_FACTION_RENOWN_LEVEL_CHANGED` from observer chains, so the server just needs to make the data available in time. The `SMSG_SET_FACTION_STANDING` packet that drives the level transition must precede any inventory/spell deltas (no in-flight client confusion).

**Per reward-field dispatch:**

| Field set | Action | Notes |
|----------|--------|-------|
| `ItemID != 0` | `Player::SendItemMailWithDelay` or direct `Player::StoreNewItem` | If `isAccountUnlock` (Flags & 0x8) and item is a collectible (mount/pet/toy), grant via collection path instead of inventory |
| `SpellID != 0` | `Player::LearnSpell(spellID, false, FACTION_RENOWN_REWARD)` | Should also persist; renown-granted spells must survive logout |
| `MountID != 0` | `CollectionMgr::AddMount(mountID, MountStatusFlags::None, false)` | Account-wide always (mount collection is per-bnet) |
| `TransmogID != 0` (ItemModifiedAppearance) | `CollectionMgr::AddItemAppearance(itemModifiedAppearanceID)` | Per-bnet |
| `TransmogSetID != 0` | iterate set's appearances, add each | Per-bnet |
| `TransmogIllusionID != 0` | `CollectionMgr::AddTransmogIllusion(illusionID)` | Per-bnet — needs new helper if it doesn't exist (warband Phase 8 may have already added it) |
| `CharTitlesID != 0` | `Player::SetTitle(titleEntry, false, true)` | Account-wide if `isAccountUnlock`, otherwise per-character |
| `GarrFollowerID != 0` | `Player::GetGarrison(GarrType::Garrison9_0)->AddFollower(garrFollowerID)` (Shadowlands soulbinds) | Vestigial — only nonzero for SL covenants in dataset; safe to gate on covenant.ID < 5 |
| `PlayerCompanionID != 0` (via Faction's Covenant) | Delves-companion grant via `PlayerCompanionInfo` system | Warband Phase 9 already loads the DB2; need helper on `Player` for "register companion availability" |
| `Field_12_0_0_63534_016 == 8` AND `Icon` set | "NPC unlock" cosmetic flag — store in player-flag bitmask, fire `SHOW_NPC_UNLOCK_TOAST` if 12.0.5 has one (verify in Lua trace) | This is the "unlocks the Adventurer at the camp" line |

**De-dup guard:** new column on `character_renown_rewards_granted(characterId, renownRewardId)` to prevent re-grant on rep-loss/re-gain (rare but possible during paragon overflow). Account-wide rewards de-dup on `(battlenetAccountId, renownRewardId)` in `warband_renown_rewards_granted`.

**Persistence:** two new tables under `sql/updates/characters/master/<DATE>_00_characters.sql`:
```sql
CREATE TABLE IF NOT EXISTS `character_renown_rewards_granted` (
  `characterId`     bigint unsigned NOT NULL,
  `renownRewardId`  int    unsigned NOT NULL,
  PRIMARY KEY (`characterId`, `renownRewardId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `warband_renown_rewards_granted` (
  `battlenetAccountId` int unsigned NOT NULL,
  `renownRewardId`     int unsigned NOT NULL,
  PRIMARY KEY (`battlenetAccountId`, `renownRewardId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

**Commit:** `feat(major-factions): 10C - grant RenownRewards on level cross with account/character de-dup`.

---

### 10D — Account-wide Reputation Expansion

**Goal:** retroactively-account-wide Dragonflight majors (10.x), plus 11.2+/12.0 majors not yet in the warband Phase 4 seed.

**Files:**
- `sql/updates/world/master/<DATE>_00_world.sql` — `INSERT` 14 new rows into `warband_reputation_faction` (every major faction in §2.1 *except* 2616 Plunderstorm and the 4 already seeded by Phase 4).
- Optional: `sql/updates/world/master/<DATE>_01_world.sql` — backfill `Faction.Flags |= 0x800` for those rows in any custom `faction_hotfix` table if we maintain one (no — we should not patch Blizzard DB2 rows; the warband_reputation_faction table is the gate).

**Bugfix candidate** (during this sub-phase): the Phase 4 `LoadAccountWideFromDB` renown-level tiebreaker. Today, when account renownLevel > char renownLevel, the code computes `_player->ModifyCurrency(currency->ID, accountRenownLevel - charRenownLevel, …)` (`ReputationMgr.cpp` ~line 815 in the rebased tree). This calls `ModifyCurrency` which can cascade through the renown-rep overflow loop — **risk:** if the account row's `standing` and `renownLevel` are out of phase (e.g. saved during a paragon transition), we may double-grant rewards through 10C. Mitigation: bypass the reward-grant path on the account-merge load by passing a new `CurrencyGainSource::RenownAccountSync` and gating `MajorFactionMgr::GrantRenownLevelRewards` on `source != RenownAccountSync` — alts inherit the renown level but not duplicate reward grants (the rewards were already granted on the higher toon and live in `warband_renown_rewards_granted`).

**Commit:** `feat(major-factions): 10D - register all post-DF major factions as account-wide`.

---

### 10E — Renown Catch-up Writer

**Goal:** implement the body of `SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` (opcode `0x42030D`) which TC currently has declared but never sends. Triggered by the existing `CMSG_COVENANT_RENOWN_REQUEST_CATCHUP_STATE` (`0x3B0111`) which TC already handles.

**Wire format (from IDA + sniff):**
- Header byte: `len << 1` (TC-side: emit as a 7-bit-len prefix).
- Body: array of `{ uint32 factionID, uint8 catchupPercent }` per major faction where the account has any progress and the player has fewer renown levels than the account max.
- Catch-up percent = `min(1.0, (accountMaxRenown - charRenown) / maxRenown) * 100`.

**File:** `src/server/game/Server/Packets/MajorFactionPackets.{h,cpp}` — new file. Contains `WorldPackets::MajorFactions::CovenantRenownSendCatchupState`.

**Handler:** `src/server/game/Handlers/MiscHandler.cpp` (where the request handler likely already lives — verify; if not, look in `CharacterHandler.cpp`). Append a builder that walks `sMajorFactionMgr->GetMajorFactionIDs()` and for each emits the record if the account-wide row's renown beats the character's.

**Commit:** `feat(major-factions): 10E - implement SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE writer`.

---

### 10F — Campaign Chapter Wiring

**Goal:**
1. Honor `Campaign.RewardQuestID` — when the player completes the `Completed` quest, auto-give the reward quest if not already taken (`QuestMgr` change).
2. Surface `QuestLine.Name/Description` to the client (the existing `SMSG_UI_MAP_QUEST_LINES_RESPONSE` already sends QuestLineIDs; the client now resolves the name via DB2 once `QuestLine.db2` is loaded).
3. Implement `CampaignXCondition` stall-tooltip lookup — `QuestMgr::IsCampaignStalled(player, campaignID)` already exists; extend it to return the **string** of the failed `CampaignXCondition` so packet handlers can attach it to `SMSG_QUEST_GIVER_QUEST_DETAILS` / similar.

**Files:**
- `src/server/game/Quests/QuestMgr.cpp` — extend `OnQuestCompleted` to call `MaybeRewardCampaignCompletion(player, questId)`. Walk `_campaignsByCompletionQuest` (need a reverse index) and, on match, dispatch `Campaign.RewardQuestID`.
- `src/server/game/Quests/QuestMgr.h` — add the new method + reverse index `std::unordered_map<uint32, std::vector<CampaignEntry const*>>  _campaignsByCompletionQuest`.

**Commit:** `feat(major-factions): 10F - honor Campaign.RewardQuestID and surface QuestLine metadata`.

---

### 10G — PlayerInteractionType::MajorFactionRenown (value 64)

**Goal:** allow renown-quartermaster NPCs to open the Journey UI for a specific faction.

**Files:**
- `src/server/game/Server/WorldSession.cpp` — `HandlePlayerInteractionInfo` (or whichever new method warband Phase 3 added for `AccountBanker = 13`): allow `MajorFactionRenown = 64`.
- `src/server/game/Globals/ObjectMgr.cpp` — `ObjectMgr::LoadMajorFactionRenownNPCs()` reads new world table `creature_template_addon` extension or a dedicated `major_faction_renown_npc(creatureId, factionId)` table, building `_majorFactionRenownNPCByCreature`.
- `src/server/game/Handlers/GossipHandler.cpp` (or NPC interaction path) — when the player talks to a registered renown NPC, send `SMSG_PLAYER_INTERACTION_INFO` with `MajorFactionRenown` and the faction stashed in the interaction state.
- New world table:
```sql
CREATE TABLE IF NOT EXISTS `major_faction_renown_npc` (
  `creatureId` int unsigned NOT NULL,
  `factionId`  int unsigned NOT NULL,
  PRIMARY KEY (`creatureId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

**Commit:** `feat(major-factions): 10G - PlayerInteractionType::MajorFactionRenown opens Journey UI per faction`.

---

### 10H — Per-Faction Script Seed (pilot: Dragonscale Expedition end-to-end)

**Goal:** one Major Faction wired *fully blizzlike* from intro to renown 30 to paragon cache, used as the reference template for the other 19. Pilot choice: **Dragonscale Expedition (2507)** — most complete public data, biggest renown reward table (60 rows), well-documented on wowhead.

**Files:**
- `src/server/scripts/DragonIsles/<zone>/` — campaign scripts, quartermaster NPC scripts, paragon cache reward triggers, weekly community quest pool.
- `sql/updates/world/master/<DATE>_NN_world.sql` — `creature_template`, `gossip_menu`, `npc_text`, `quest_template`, `quest_template_addon`, `creature_questender`, `creature_queststarter` rows for: intro quest, every campaign chapter quest, every renown reward dispenser, the paragon quest target, the Adventurer's reward cache item, the weekly community quest pool. Source: wowhead.
- Per-faction config row in new world table `major_faction_config` (see 10G):
```sql
CREATE TABLE IF NOT EXISTS `major_faction_config` (
  `factionId`               int unsigned NOT NULL,
  `hiddenFromExpansionPage` tinyint(1) NOT NULL DEFAULT 0,
  `displayAsJourney`        tinyint(1) NOT NULL DEFAULT 0,
  `useJourneyRewardTrack`   tinyint(1) NOT NULL DEFAULT 0,
  `useJourneyUnlockToast`   tinyint(1) NOT NULL DEFAULT 0,
  `uiPriority`              int NOT NULL DEFAULT 0,
  `introQuestId`            int unsigned NOT NULL DEFAULT 0,
  `textureKit`              varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`factionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

The other 19 factions are stubbed in `major_faction_config` with sensible defaults; per-faction quest/script work for them is **out of scope of this branch** but tracked in follow-up tickets.

**Commit:** `feat(major-factions): 10H - Dragonscale Expedition pilot end-to-end (world data + scripts)`.

---

### 10I — Paragon Reward Cache Loot Template

**Goal:** open a paragon cache → rolls loot from a per-faction template with rare-tier drops (mounts, pets, transmog).

**Files:**
- `sql/updates/world/master/<DATE>_NN_world.sql` — new world table `paragon_cache_loot_template(factionId, itemId, chance, group, mincount, maxcount)`. Seeded for Dragonscale Expedition from wowhead drop tables for the "Dragonscale Expedition Supplies" item.
- `src/server/game/Globals/ObjectMgr.cpp` — `LoadParagonCacheLootTemplates()` + `RollParagonCacheLoot(uint32 factionId) -> std::vector<LootItem>`.
- `src/server/game/Spells/SpellEffects.cpp` — `SPELL_EFFECT_OPEN_PARAGON_REPUTATION_CACHE` handler (if exists), or `Spell::EffectGiveCurrency` extension for paragon currencies.

**Commit:** `feat(major-factions): 10I - paragon cache loot template with rare-tier rolls`.

---

### 10J — Weekly Reset Hooks

**Goal:** major-faction weekly catch-up quests and weekly caps.

**Files:**
- `src/server/game/Quests/QuestPools.cpp` — register weekly Major-Faction quest pools (Aiding the Accord variants, Theater Troupe, Awakening the Machine, Pact of <leader>, etc.). Pool entries come from world data seeded in 10H.
- `src/server/game/World/World.cpp:DailyReset()` / `WeeklyReset()` — call `sMajorFactionMgr->OnWeeklyReset()` to clear `character_renown_questlog` (if used for weekly state tracking).
- New table `character_renown_weekly(characterId, factionId, weeklyQuestPoolMember, expiresAt)` if needed.

**Commit:** `feat(major-factions): 10J - weekly reset hooks for renown catch-up and weekly quests`.

---

### 10K — Tests & Smoke Scripts

**Files:**
- `tests/game/MajorFactions/CMakeLists.txt` — new test target.
- `tests/game/MajorFactions/test_majorfaction_mgr.cpp` — Catch2 cases:
  - identity (`IsMajorFaction` returns true for exactly 20 IDs)
  - renown level math (`GetReputationEarnedThisLevel` correctness)
  - reward dispatch (`GrantRenownLevelRewards` grants every reward and de-duplicates on re-crossing)
  - catchup writer payload shape
- `tests/game/MajorFactions/test_campaign_reward.cpp`:
  - completing a campaign's `Completed` quest triggers `RewardQuestID`
  - `CampaignXCondition` failure surfaces the localized string

**In-game smoke script:** `doc/major-factions/SMOKE_PILOT_DRAGONSCALE.md` — manual test plan for the pilot faction on `M:\FeatureServer`: intro quest → first chapter → renown level 1 → 2 → 30 → paragon → cache loot → alt login inherits.

---

## §5. File-by-File Change Inventory (forecast)

For pre-PR sizing. Estimated based on warband Phases 4/7/9 deltas.

| File | Sub-phases | Net LOC |
|------|-----------|---------|
| `src/server/game/DataStores/DB2Structure.h` | 10A | +90 |
| `src/server/game/DataStores/DB2LoadInfo.h` | 10A | +140 |
| `src/server/game/DataStores/DB2Stores.{h,cpp}` | 10A | +25 |
| `src/server/database/Database/Implementation/HotfixDatabase.{h,cpp}` | 10A | +60 |
| `src/server/game/MajorFactions/MajorFactionMgr.{h,cpp}` | 10B (new module) | +500 |
| `src/server/game/CMakeLists.txt` | 10B | +1 |
| `src/server/game/Reputation/ReputationMgr.cpp` | 10B, 10C, 10D | +60 |
| `src/server/game/Quests/QuestMgr.{h,cpp}` | 10F | +80 |
| `src/server/game/Server/Packets/MajorFactionPackets.{h,cpp}` | 10E (new file) | +90 |
| `src/server/game/Handlers/MiscHandler.cpp` | 10E | +30 |
| `src/server/game/Server/WorldSession.cpp` | 10G | +20 |
| `src/server/game/Globals/ObjectMgr.{h,cpp}` | 10G, 10H, 10I, 10J | +120 |
| `src/server/game/Spells/SpellEffects.cpp` | 10I | +60 |
| `src/server/game/World/World.cpp` | 10J | +5 |
| `src/server/scripts/DragonIsles/...` | 10H (pilot) | +400 |
| `sql/updates/hotfixes/master/<DATE>_00_hotfixes.sql` | 10A | +60 |
| `sql/updates/world/master/<DATE>_NN_world.sql` | 10D, 10G, 10H, 10I, 10J | +600 |
| `sql/updates/characters/master/<DATE>_NN_characters.sql` | 10C, 10J | +30 |
| `tests/game/MajorFactions/...` | 10K (new dir) | +250 |
| `doc/major-factions/*.md` | already done | — |

**Total forecast:** ~2 600 LOC added, 0 LOC removed, ~30 files touched. Comparable to warband Phase 4 + Phase 7 combined.

---

## §6. Risks & Unknowns

| # | Risk | Mitigation |
|---|------|------------|
| 1 | `RENOWN_REPUTATION_PER_LEVEL = 2500` is asserted constant from inspection of the existing TC renown loop and wowhead; if any faction uses a different value (e.g. Ritual Sites with `MaxQty=8`), the formula breaks | Confirm during 10B implementation by reading the existing `ReputationMgr.cpp:566–590` constants; if per-faction, lift into `MajorFactionMgr::GetRepPerRenownLevel(factionId)` |
| 2 | `SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE` inner record stride inferred from IDA blob walker — not confirmed against a sniffed catchup capture | Send synthetic traffic from worldserver to a 12.0.5 client on `M:\FeatureServer`, capture, compare; adjust struct if needed |
| 3 | `Faction.Flags & 0x800` (AccountWideReputation) inferred from one Severed Threads capture | Re-verify against TWW DF retro-rollout captures in `C:\sniff` once a sniff with mixed account-state is available; gate behavior on `ObjectMgr::IsWarbandReputationFaction` (the world-config table) regardless |
| 4 | `RenownRewards.Flags` field bit semantics (`0x2/0x8/0x10`) inferred from sample rows | Confirm during 10C implementation by cross-referencing reward rows with their in-game behavior on wowhead (`Expedition Scout Packs` row 627 has Flags=8 and is an NPC unlock) |
| 5 | Per-faction `RenownRewards.GarrFollowerID` is largely SL-era (covenants 1-4) but the Lua **does** read it for any nonzero value | Gate dispatch on `covenant.ID < 5` to avoid grant-storm on misdata; warn-log if a non-SL row has GarrFollowerID set |
| 6 | The `playerCompanionID` Lua field has no obvious DB2 column source; the UI agent inferred it from consumer code | The field is a per-faction property; load it from `major_faction_config.playerCompanionId` (world DB), not from a DB2 column. Confirm by trying to read a DB2 column we'd expect to find it on (`Faction.dbd` post-67186 blocks, `Covenant.dbd` blocks); if not present, world-config is authoritative |
| 7 | Account-wide reward grant ordering: if two characters cross the same renown level near-simultaneously in different sessions, the account-wide grant could double-fire | `warband_renown_rewards_granted` has a `(battlenetAccountId, renownRewardId)` PK; the duplicate INSERT will deduplicate at the SQL level. Guard the grant code with `INSERT IGNORE` and check rows-affected before applying the in-memory grant |
| 8 | Existing TC `Faction.Flags & 0x04` is named `Hidden` in `ReputationMgr` but in 67186 schema this bit is co-opted as the **MajorFaction** indicator (the wago canonical name) | Do not rename mid-flight — `MajorFactionMgr::IsMajorFaction` uses the `ParagonFactionID && RenownCurrencyID` rule which is robust regardless |

---

## §7. Out of Scope (this branch)

Stage to follow-up branches; do not block this one:

- Full per-faction quest/script implementation for the other 19 majors (10H delivers only Dragonscale Expedition).
- World quests / treasures / bonus objectives that grant major-faction reputation in zones — these will hot-fix into `creature_loot_template` / `quest_template` over time per zone.
- Renown-restricted vendor inventories (UI gating works via `PlayerCondition.db2`; world-data seeding belongs to follow-up zone branches).
- Plunderstorm renown track (DB2 is loaded in 10A.2 so the data is present; gameplay belongs to a separate Plunderstorm branch).
- Renown-bonus events (Winds of Mysterious Fortune +100/200%): hot-fixable through standard event system after this branch lands.

---

## §8. Definition of Done

- `worldserver` boots without warnings; all 7 new DB2 loaders report nonzero counts matching `C:\dumps\jsonexport\<Table>_67186.csv`.
- A Midnight 12.0.5 client connecting can open the Journey UI for any of the 20 major factions and see correct renown level, threshold, and reward track.
- Completing a renown-rep-granting quest crosses a renown level, fires `SMSG_SET_FACTION_STANDING`, the client emits `MAJOR_FACTION_RENOWN_LEVEL_CHANGED`, and the reward (item/spell/mount/transmog/title) is granted exactly once.
- An alt logging in inherits the account-max renown for every account-wide major; collectibles already granted are not re-granted.
- A new character clicking the renown quartermaster receives the catch-up bonus (verified via packet capture and in-game tooltip).
- The pilot Dragonscale Expedition campaign runs end-to-end: intro → all chapters → completion reward quest delivered → renown 30 reached → paragon cache obtainable.
- Catch2 test suite for the MajorFactions module passes (`ctest --test-dir ./build -L MajorFactions`).
- Documented end-to-end smoke result in `doc/major-factions/SMOKE_PILOT_DRAGONSCALE.md`.

---

## §9. Tester Quick-Start (for QA on `M:\FeatureServer`)

> **Tester pre-req:** Build from a branch that contains the **entire warband chain (Phases 1–9)** plus this Phase 10. The branch `major-factions` (rebased onto `warband/phase9-delves`) provides this. Do **not** test on `master` alone — the account-wide reputation column will be missing and Phase 10 will degrade to per-character behavior, producing false-negative bug reports.

1. Pull DB2s from current Midnight client install (`M:\WorldofWarcraft`) into the server's `dbc/enUS/` directory.
2. Apply SQL updates: world, characters, hotfixes — all migrations from this branch.
3. Bring up worldserver; check the startup log for the 7 new `Loaded …` lines (RenownRewards, Covenant, QuestLine, CampaignXCondition, FactionGroup, CurrencyCategory, RenownRewardsPlunderstorm).
4. Log in a 12.0.5 client. Open the Journey UI manually (`/run EncounterJournal_OpenToJourney(2507)`). Verify all 19 fields render (icons, colors, level/threshold/rewards strip).
5. Use a GM command (`.modify rep 2507 +30000`) to push past several renown levels; verify rewards appear in inventory/collection and the toast fires.
6. Log in an alt on the same Bnet account; verify the renown level is inherited.
7. Repeat for the TWW majors (2570, 2590, 2594, 2600) — these should be account-wide from day one (Phase 4 seed).

---

## §10. Glossary

- **Major Faction:** a reputation track introduced in DF 10.0+ that uses **Renown** levels (instead of Exalted) and a **Paragon** loop above max. Identity in 67186: `Faction.ParagonFactionID != 0 AND Faction.RenownCurrencyID != 0`.
- **Renown:** an integer level 1..N for each major faction. Stored client-side as the value of the per-faction renown currency. Server-side: derived from `ReputationMgr` standing via the currency-overflow loop.
- **Paragon:** a repeating rep loop above the renown cap that grants a quest item (cache) every `LevelThreshold` reputation (10 000 for all current paragon factions).
- **Covenant.db2 (in this context):** despite the Shadowlands-era name, this table is the canonical bridge between a `RenownRewards` row's `CovenantID` and a `Faction.ID` + `CurrencyTypes.ID`. Major Factions are Covenants from the DB2's perspective (rows 5..30+).
- **Campaign:** a multi-chapter story quest line attached to a Major Faction (or zone). Composed of `Campaign` → `CampaignXQuestLine` → `QuestLine` → `QuestLineXQuest` → `Quest`.
- **Warband:** the player's Battle.net-account-wide identity introduced in TWW 11.0. Account-wide reputation, achievements, collectibles, taxi mask, etc. all live "on the warband" rather than "on the character."
- **Journey:** post-12.0.5 UI panel (in `Blizzard_EncounterJournal`) that supersedes the old `MajorFactionRenownFrame`. Opened by `PlayerInteractionType::MajorFactionRenown = 64`.
