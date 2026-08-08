# Handover: merge feature/delves into integration/all-systems (2026-08-07)

From the feature-development session. Context docs: `I:\TrinityCore\delves\DELVES_PLAN.md`,
`DELVES_RETAIL_FACTS.md`; memory notes under `C:\Users\daimon\.claude\projects\I--TrinityCore-mythic-plus\memory\`.

## 1. What to merge

```
git merge feature/delves        # c277da6a76..05a24fa311 (4 commits, pushed to origin)
```

The 4 commits: core-loop wiring (deaths/completion/weekly reset/bountiful rotation fix), real reward
payouts (Dawncrests/gear/coffer/shards), companion spawn fix + tier gating, companion role abilities.
All compile-verified; full worldserver linked on the branch (build44, 14.44 toolset).

Also check: `feature/mythic-plus` had 2 unpushed commits from another session (ab8be786f9, da139c4009 —
elapsed timer/criteria + keystone weekly rollover). If they are pushed by now, re-merge that branch too.

## 2. Expected merge conflicts (all resolvable by taking BOTH sides / union)

| Location | Note |
|---|---|
| `Opcodes.cpp` client block ~lines 958-960 | CERTAIN: M+ edited 958/959, delves edits 960 (adjacent) |
| `DB2Stores.{h,cpp}`, `DB2LoadInfo.h`, `DB2Structure.h` | append-regions from all three branches; keep alphabetical order |
| `HotfixDatabase.{h,cpp}` | enum + PrepareStatement tail appends |
| `WorldSession.h` | forward-decl + handler-decl tail inserts (~6 lines apart) |
| `World.cpp` | includes + Initialize calls + ResetWeeklyQuests (M+ has nothing there; delves adds DelvesRewards::ResetAllWeeklyProgress) |
| `CharacterDatabase.{h,cpp}`, `AllPackets.h`, `cs_script_loader.cpp`, `worldserver.conf.dist` | trivial tail appends |

`UpdateFields.{h,cpp}` / `UpdateFieldImpl.h` are touched by delves only — no conflict.

## 3. After the merge succeeds, in order

1. **Reconfigure before building** — the source glob is configure-time; new files (ItemUpgradeMgr,
   ItemConversionMgr, LFGList/, Delves/, npc_mythic_keystone, cs_mythic_plus) won't compile otherwise:
   `cmake <builddir>` then build with the **14.44 toolset** (14.38 cannot link against Boost 1.89).
2. **Great Vault World row bridge** (the one real post-merge coding task): delves and M+ each half-own the
   vault. Delete the dead duplicate `DelvesRewards::GetGreatVaultSlotCount` (hardcoded 2/4/8) and
   `UpdateGreatVaultProgress` stub; instead feed `delve_progress.WeeklyCompletions` /
   `HighestTierThisWeek` into the vault handler in `ChallengeModeHandler.cpp`
   (`HandleRequestWeeklyRewards`): add a second activity row/threshold set read from
   `WeeklyRewardChestThreshold.db2` with the World/Activities Type (same highest-ID-per-index rule as
   `ChallengeModeMgr::GetMythicPlusVaultThresholds`; M+ Type=1 — check the DB2 for the world type value,
   sniff evidence: vault rows 196-198 world / 199-201 raid / 202-204 M+ per
   `C:\dumps\LFGLIST_SNIFF_DEEP_68275.md` §vault). Reward level per slot from the delve tier ilvl table in
   `I:\TrinityCore\delves\DELVES_RETAIL_FACTS.md` (T1=233 ... T8+=259, vault caps at Hero 1/6).
3. **Apply SQL updates** on the integration databases (if not yet applied there):
   characters `2026_08_06_00`, `2026_08_07_00`; world `2026_08_06_00/01`, `2026_08_07_00` (delves
   finalBossEntry); hotfixes `2026_08_06_00/01`, `2026_08_07_00`. Delves branch also carries its own
   older `2026_03_27_*`/`2026_04_2*` files if the integration DB never got them.
4. **Smoke test** (GM): `.mythicplus keystone 2` -> run start; `.delve info` / `.delve complete` -> crest
   payout + tier unlock; premade group finder list/browse/apply between two clients.
5. **Content-ops checklist to hand to the data team** (server code is ready, world data missing):
   - delve_template rows for the 8 remaining Midnight S1 delves + Torment's Rise (map ids from client),
     `finalBossEntry` values for Atal'Aman (2962) + Shadow Enclave (2952)
   - Shadow Enclave gossip_menu/option rows (menu 40277 — currently unreachable via the entrance NPC)
   - 7 dangling ScriptNames referenced by delves SQL (spell_delve_entry, go_delve_campfire,
     npc_delvers_supplies, npc_lord_antenorian, npc_darkcaller, npc_void_focus_se, npc_valeera_companion)
   - Delves.Companion.CreatureId (Valeera creature) + Delves.Companion.<Role>.*SpellId ability ids
   - M+: ChallengeMode.Reward/Vault.LootId loot pools, Catalyst item_conversion_output rows + GO,
     Midnight affix creatures (Ascendant orbs / Void Emissary), challenge_mode_enemy_forces counts,
     Lindormi (244792, ScriptName npc_lindormi) + Font of Power spawns
   - Delves.Reward/Coffer.LootId loot pools

## 3b. NEW (2026-08-07 evening): merge feature/major-factions-1207

`git merge feature/major-factions-1207` (@ ca762c5c9f, pushed) — based directly on integration's tip
9619f2f2f8, so it merges clean. Brings the Phase 10 Major Factions system (MajorFactionMgr, renown
reward dispatch, 4 new DB2s with 68275-verified layouts, 20-faction world seed) ported from the retired
`major-factions` branch, plus a REAL BUG FIX integration needs: account-wide reputation save-order
(account-wide must save before per-char wipes the dirty flags). Full worldserver linked in
`I:\TrinityCore\grand-factions\build44`. Details, skipped-commit rationale, and SQL list:
`I:\TrinityCore\grand-factions\MAJOR_FACTIONS_1207_PORT.md`. After this, the old `major-factions`
branch is retired — do not merge it (its warband lineage duplicates integration's cherry-picks).

## 3c. NEW (2026-08-08 overnight): merge content/midnight-s1

`git merge content/midnight-s1` (@ fc3caf0f42, pushed) — based on feature/major-factions-1207's tip, so
merge it AFTER (or instead of) that branch; it contains 3b's commits plus 11 more. Contents: Midnight S1
season-table seeds, 9 delve templates, weighted Enemy Forces (new table + code) for all 8 M+ dungeons,
per-dungeon M+ gear pools + delve gear pool (conf defaults now point at pools 300000/301000), Catalyst
conversion-12 data, affix creature ids, Lindormi 259053 + Font of Power spawns, Gulf of Memory delve
fully wired, TIERED_ENTRANCE_OPEN wire pair, FACTION_BONUS_INFO / REATTACH_RESURRECT / CLEAR_RESURRECT,
M+ record-packet send fix. Full worldserver linked in grand-factions\build44. SQL updates to apply:
world 2026_08_07_61..67, hotfixes 2026_08_07_70/71. Conf keys changed: ChallengeMode.Reward/Vault.LootId,
Delves.Reward/Coffer.LootId, Delves.Companion.CreatureId, ChallengeMode.Affix.*.CreatureId defaults.

## 4. Do NOT

- Merge `warband/phase9-delves` thinking it's the delve branch — it's the warband account-systems branch.
- Build with the 14.38 toolset target `worldserver` (links fail on Boost 1.89 symbols); lib-only compiles
  are fine there.
