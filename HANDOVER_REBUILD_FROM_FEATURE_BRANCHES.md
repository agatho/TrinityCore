# Handover: re-merge integration from the golden-source feature branches (2026-08-08)

Process rule (now permanent): **feature branches are the golden source; integration/all-systems is a
disposable test merge.** The transport branches `content/midnight-s1` and `feature/major-factions-1207`
are RETIRED — never merge from or develop on them again. Everything they carried has been backported to
its owning feature branch; this round brings integration up to date FROM those branches.

## 1. What to merge (all pushed, all compiled standalone; worldserver linked on the starred ones)

Merge in this order (small deltas first, the one real conflict source last):

| # | Branch | Tip | Delta vs what integration already has |
|---|---|---|---|
| 1 | feature/world-quests | e77ff54e7b | none beyond content/midnight-s1's copies (same content, new hashes) |
| 2 | feature/housing-system | c3ccfa3e8f | none beyond midnight-s1 |
| 3 | feature/ingame-shop-battlepay | bcf0aa71a6 | none beyond midnight-s1 |
| 4 | feature/wow-token | 9452454901 | **NEW**: purchase-list writer record-final fix (was missed before — the packet was born here) |
| 5 | feature/gap-closers | 59693ece4d | **NEW**: 2 standalone-build repairs (WorldSession.h fwd decls; stray chromie handler removed) |
| 6 | feature/mythic-plus* | 70159fa61d | none beyond midnight-s1 + the vault-bridge commits integration already has |
| 7 | feature/delves* | 59f1f2ed9a | none beyond midnight-s1 + that branch's great-vault merge |
| 8 | major-factions* | bdff1efd00 | **NEW**: modernized to your exact upstream base (60bd51b968); catchup packet in bool form; 10K tests dropped |

## 2. Expected conflicts and how to resolve them

**a) Split SQL files (CERTAIN, by design).** These filenames exist on TWO branches with complementary
halves; git will conflict when the second branch merges. Resolve as the UNION of both sides' statements.
The union must equal the file content integration ALREADY has from content/midnight-s1 — diff against
your existing copy to verify byte-equality (then the updates tracker sees an already-applied file and
does nothing):
- `2026_08_07_66_world.sql` — mythic-plus half (Font of Power GO) + delves half (Gulf template/scenarios)
- `2026_08_08_01_world.sql` — mythic-plus half (Lindormi 197711/gossip/vendor) + world-quests half (75 WQ rows)
- `2026_08_08_02_world.sql` — mythic-plus half (restamps) + world-quests half (quest 49091 drift)

**b) Catchup-packet double ownership (the one real decision).** Integration answers
CMSG_COVENANT_RENOWN_REQUEST_CATCHUP_STATE in `CovenantHandler.cpp` (from feature/garrison-systems).
The modernized `major-factions` branch now ALSO ships `MajorFactionPackets.{h,cpp}` (bool form, wire-
correct) with a sender in `MiscHandler.cpp`. Two handlers for one opcode will collide in Opcodes.cpp.
**Keep the garrison/covenant side** (first owner, identical semantics — IsActive=false either way) and
drop major-factions' handler registration during conflict resolution; keep its packet files if they
merge cleanly (harmless) or drop them too — your choice, they are redundant. Long-term the two golden
sources should agree on ownership; flag it to the user if you want it settled properly.

**c) Battle-pay purchase writers.** wow-token (#4) and ingame-shop-battlepay (#3) both fix
`BattlePayPackets.cpp` with the identical record-final layout; if both sides touch the same functions
git may conflict — both sides are textually identical in intent, take either (verify walletName is
written LAST in each record and STATUS_DONE == 6).

**d) Everything else** should auto-merge: the backports are content-identical to what integration
already received via content/midnight-s1, just under new hashes on new lineages.

## 3. After the merges

1. Reconfigure + rebuild (14.44 toolset), full worldserver link, boot test — integration should end up
   functionally identical to its current state plus the four NEW items (wow-token fix, gap-closers
   repairs, major-factions modernization, catchup cleanup).
2. No new SQL to apply IF you already ran content/midnight-s1's updates (world 2026_08_08_00..03,
   hotfixes 2026_08_08_00); the union-resolved files must be byte-equal to the applied ones.
3. New conf key since your last deploy: `Delves.Companion.FactionId = 2742`.
4. Housekeeping when green: delete the retired transport branches (`content/midnight-s1`,
   `feature/major-factions-1207`, `content/midnight-s1-sqlfix` — its fixes are merged everywhere) or
   tag them as archive refs. Also: your `I:\TrinityCore\mythic-plus` worktree is 11+ commits behind
   origin (the backport push published your local vault-bridge commits) — pull before committing there.

## 3b. ADDENDUM (2026-08-08): tester-regression fixes — re-merge two branches + DB action

Tester reported (a) group finder returns nothing, (b) world quests not on the map. Both root-caused
against the 68974 captures and FIXED on their golden-source branches:

1. **`feature/lfg-list` @ 795ca3eb4f** — primary cause was server logic: the listing descriptor field
   is a GroupFinderCategory id, but LFGListMgr::Search treated it as a GroupFinderActivity id, so EVERY
   search filtered to zero rows. Also: removed the never-on-retail SEARCH_STATUS send, retail's
   empty-then-populated SEARCH_RESULTS order, the 456-entry blacklist reply, triple UPDATE_STATUS on
   create, silent GET_STATUS while unlisted, corrected member spec-role/leader bits and the compact
   SEARCH_RESULTS_UPDATE row. No DB changes — just re-merge.
2. **`feature/world-quests` @ 74847ddfef** — WorldQuestMgr never set the activation worldstates
   (client hides any WQ whose VariableID worldstate isn't broadcast); now registered realm-wide on
   activation, plus a World.cpp load-order fix (WorldStateMgr before the quest managers).
3. **DB ACTION REQUIRED on the integrated realm**: `integ_world.world_quest_template` still holds 482
   placeholder rows with VariableID=0 — the 2026_08_08 world updates were never applied there. Re-apply
   them (the reseed is what makes quests appear). Also 27 of 389 seeded quests (Midnight 93k-97k block)
   are missing from that realm's quest_template until the Midnight quest import lands - they are skipped
   with logged errors, not fatal.

## 3c. ADDENDUM (2026-08-08 late): chromie-time audit outcome — merge directives

Full audit: C:\dumps\CHROMIE_AUDIT_REPORT.md (21 gaps: 2 critical, 8 major). Remediation is landing on
feature/chromie-time (in flight; re-merge that branch when its push appears). Directives for the merge:
1. **The branch's ChromieTimeNpc gossip case WINS over integration's NYI stub** (integration stubbed the
   interaction; the branch has the working generic StartInteraction path).
2. **Integration carries the same critical ContentTuning-redirect bit-test bug at its own
   DB2Stores.cpp:2485** — the branch fix (audit item R1) must be ported/merged there; without it ALL
   chromie scaling (creatures, quests, LFG, items, areas) is inert.
3. The chromie world SQL at the branch's old filename 2026_03_06_00_world.sql was CLOBBERED by an
   upstream warrior commit sharing the filename — the branch re-authors it as 2026_08_08_07_world.sql;
   apply that new file on the realm DB (the old one currently contains only the warrior content).

## 3d. ADDENDUM (2026-08-09): commerce audit — merge directives + one integration-side fix

Full audit: C:\dumps\COMMERCE_AUDIT_REPORT.md (33 gaps). Remediation landing on feature/ingame-shop-battlepay
(shop + BattlePay + the NEW catalog-admin system) and feature/wow-token (token + anti-abuse ledger); re-merge
both when their pushes appear. Directives:
1. **IN-1 (integration owns this one):** integration/all-systems is BEHIND both branch tips on the
   purchase-record wire fix — it still has STATUS_DONE=3 and mid-record walletName in
   BattlePayHandler.cpp / BattlePayPackets.cpp. Re-merging feature/wow-token + feature/ingame-shop-battlepay
   brings the fix; verify by 2-file diff against the branch tips after merge.
2. **Catalog-admin system** (feature/ingame-shop-battlepay): new world tables shop_product /
   shop_product_deliverable / shop_slot_override + RBAC reload perm + `.shop` / `.reload shop_catalog`
   commands. The old battlepay_product (4 rows) is migrated into shop_product via INSERT..SELECT and its
   reader re-targeted. Apply the new world SQL. The 58KB catalog blob is a data/battlepay/ file (template);
   the server reskins its 9 slots from DB rows — an admin edits shop_product rows, not the blob.
3. **account_battlepay_purchase** (auth table, born on feature/wow-token): the shared purchase ledger both
   branches use for GetPurchaseList + idempotency. Apply the auth SQL. Also account_wow_token (auth) was
   never applied to integ_auth — apply it too (WowTokenMgr needs it).
4. **New conf keys:** Shop.Enabled, CommercePricePollTimeSeconds. Merge conf.dist.

## 3e. SUPERSEDES 3d (2026-08-09): commerce consolidated into feature/commerce

The two commerce branches are RETIRED and REPLACED by a single clean golden source:
**`feature/commerce` @ 37e42f536d** (Shop + BattlePay + WoW Token + catalog-admin, worldserver linked).
- **Merge `feature/commerce`, NOT feature/ingame-shop-battlepay and NOT feature/wow-token.** The latter is
  a fork of the whole original dev line (~230 cross-system commits) — never merge it; its non-commerce
  content already lives on the per-feature branches + integration.
- SQL to apply from feature/commerce: auth 2026_07_20_00 (account_wow_token), 2026_07_20_01
  (account_battlepay_purchase ledger), 2026_08_09_00 (RBAC 886/887); world 2026_08_09_00 (catalog-admin
  tables, drops battlepay_product) + 2026_08_09_01 (token product row, slot 574806).
- Data blobs to <DataDir>/battlepay/: product_list + distribution_list.
- Conf keys: Shop.Enabled, Shop.PurchaseConfirmation (default off), CommercePricePollTimeSeconds,
  WowToken.Market.Enabled (default off).
- The old §3d IN-1 purchase-wire fix is included; the GrantType-3->WowTokenMgr deliverable is wired here
  (was the cross-branch gap), so a token sells through the catalog end-to-end.

## 4. Do NOT

- Merge `content/midnight-s1` or `feature/major-factions-1207` (retired transport branches).
- Develop anything on integration — fixes found during boot/testing go to the OWNING feature branch
  first, then re-merge here.

### §4 — NEW MIDNIGHT CONTENT SYSTEMS (gap backlog build 2026-08-12, user order 2->5->4->3->1)
Five net-new golden-source branches, all off baseline 560165c0a6, all game+worldserver GREEN, integ realm untouched.
Blueprints in C:\dumps\*_BLUEPRINT.md; per-branch continuation memory in the memory dir.

 feature/omnium-folio      @ d9d4efb1ce  — #5 seasonal rune ledger. It's a STOCK Trait tree (1186/sys48); core DONE.
     SQL: world/master/2026_08_12_00_world_omnium_folio.sql (omnium_folio_season, seeds season1=active);
          characters/master/2026_08_12_00_characters_omnium_folio.sql (character_omnium_folio).
 feature/quelthalas-zone-events @ e857d66597 — #2 renown loop. ZoneEventMgr + Stormarion built; 3 events capture-gated.
     SQL: world/master/2026_08_12_00_world_quelthalas_zone_events.sql (zone_event_template + scenarios(2771,1,3021,3021)
          + zone_event_scenario_step 3 rows + zone_event_spawn schema/0 rows).
     Merge order: content/midnight-s1 -> feature/world-quests -> feature/warband -> feature/quelthalas-zone-events.
 feature/prey-voidforge    @ 0c4734fedc  — #4 solo hunts. Economy slice built (debug-triggered); Hunt Table opcode blocked.
     SQL: world/master/2026_08_12_00_world_prey_voidforge.sql (prey_hunt_template, seeds commented);
          characters/master/2026_08_12_00_characters_prey_voidforge.sql (character_prey_hunt).
     Merge order: feature/mythic-plus (WeeklyRewardsMgr/vault) -> major-factions -> world-quests -> delves -> prey-voidforge.
 feature/void-assaults     @ 8b7e810383  — #3 invasion framework. Core slice built (debug-triggered). DESIGNED TO FOLD
     INTO ZoneEventMgr (ships mirrored VoidAssaultMgr since baseline lacks it). SQL: world/master/2026_08_12_00_world_void_assaults.sql
     (void_assault_template 2 rows + void_assault_spawn empty). Merge order: quelthalas-zone-events FIRST, then void-assaults last.
 feature/devourer-spec     @ 3ead6a3a12  — #1 DH spec. NOT net-new (baseline+upstream already ship it); added the one
     real gap = Void-Metamorphosis Fury-drain (1217607). SQL: world/master/2026_08_12_00_world_devourer_spec.sql
     (spell_script_names: 1217605/1217607/1234195). MUST apply for the scripts to attach.

NOTE: every branch's LoadFromDB tolerates absent tables (realm-safe no-op) so merging code without the SQL is safe;
apply each branch's SQL to activate. Debug commands (.prey / .voidassault) are RBAC_PERM_COMMAND_DEBUG, TEMP-flagged
for removal once the capture-blocked real activation wires land. See §4-CAPTURES below.

### §4-CAPTURES — tester captures that unblock the remaining content work (highest leverage)
 1. FULL Stormarion Assault + Void Incursion run to COMPLETION (zone 15968 / Scenario 3021 + 3173) -> meter->reset
    flip, completion reward packet (currency/renown ids + AMOUNTS), SCENARIO_STATE n>0, destructible/boss spawn coords.
    Unblocks #2 Stormarion reward tail AND #3 Void Assault reward tail (shared machinery).
 2. SMSG_ACTIVE_SCHEDULED_WORLD_STATE_INFO raw bytes (7x per zone-change in 15968/15969) -> timer packing for all events.
 3. OPEN the Prey Hunt Table (npc 245824, Astalor's Sanctum Silvermoon): right-click board, select contract+difficulty.
    First CMSG on interact identifies the opcode family -> unblocks #4 hunt activation.
 4. Enter Naigtal (Map 3075) / Val (Map 3047) + the Normal/Heroic portal prompt; kill a portal boss -> #3 portal worlds.
 5. Abundance cave across a rotation boundary -> PlayerConditionID 149863-7 gate -> #2 Abundance event.
 6. A Devourer combat log -> exact Fury-drain tick (#1); Legends of Haranir warband scenario started (map 2694, SCENARIO_STATE n>0) -> #2.

### §4-VALIDATED — the 5 new branches were pre-merged & build-tested TOGETHER (2026-08-12, throwaway scratch)
Scratch worktree off baseline 560165c0a6, merged all 5 in order omnium->zone-events->void-assaults->prey->devourer.
COMBINED worldserver builds GREEN (0 compile/0 link errors). No SQL-filename collisions (7 files, all distinct
descriptive suffixes, none pre-existing upstream). Central realm untouched; scratch branch never pushed.
 THE ONE CONFLICT HOTSPOT = src/server/game/World/World.cpp (all 4 branches insert into the SAME 3 regions:
 includes block, LoadFromDB region after WorldStateMgr::LoadFromDB, Update region after WorldStateMgr::Update).
 Resolve by UNION (keep all 4 includes + all 4 LoadFromDB() + all 4 Update() calls). Player.cpp / cs_script_loader.cpp
 / spell_script_loader.cpp AUTO-MERGE clean. Total shared-seam delta = +41 lines, 0 deletions.
 NOTE: Omnium login hook is OnPlayerLogin (not EnsureFolioForPlayer). Live smoke-test of .prey/.voidassault/
 Void-Meta debug commands still DEFERRED to the integration session on a disposable DB (build-only check here).
 Scratch worktree left at I:/TrinityCore/valint (branch _validate-new-systems, local-only) for inspection.

### §4-CAPTURES refinement (2026-08-12, from Hunt Table RE — C:\dumps\PREY_HUNT_TABLE_RE.md)
Capture #3 (Prey Hunt Table) SHARPENED: client binary can't resolve it (68275 predates the Prey UI; open is
gossip-gated/server-driven). The capture needs exactly the SMSG_GOSSIP_MESSAGE OptionNPC byte of the contract
option when npc 245824 is OPENED (27=garrison-mission -> HandleOpenMissionNpc already exists / 31=Adventure Map /
quest-option=plain gossip) + the CMSG sent after selecting it. Mouse-over does not trigger the gossip exchange.

### §4 update (2026-08-12) — Omnium questlines + Saltheril event added
feature/omnium-folio advanced d9d4efb1ce..a1ab111366: adds sql/updates/world/master/2026_08_12_02_world_omnium_questlines.sql
(17 quest shells + chain + starter/ender on NPCs 237504/246025). APPLY it so 96233 exists -> ach 62606 fires ->
Omnium engine engages. Quests are completable SHELLS (real objectives are TODO comments, entities unseeded) - the
user-accepted wowhead-sourced fidelity tradeoff. feature/quelthalas-zone-events advanced e857d66597..26a07296f5
(Saltheril's Soiree weekly event = 2nd of 4; hooks live quest 89289; AreaPOI 8600 shipped LISTED/commented pending
AreaPoiMgr from feature/world-quests).

### §5 — OVERNIGHT BATCH 2 (2026-08-13, user order 7->6->10->9) — 4 more golden-source branches, all worldserver-green
 feature/slayers-rise-bg   @ 6f92a9ab7f — #7 40v40 epic BG (map 2799 REAL). BattlegroundScript by MapID + IoC node
   capture + reinforcement + S1 PvP rules (16s DR, -20% healing, PvPSeasonRules.h). SQL: world/master/2026_08_13_00_world_slayers_rise_bg.sql
   (battleground_scripts + battleground_template placeholder start locs). CAPTURE: Vidious/Ziadan creature ids, WorldSafeLocs
   graveyards/start-locs (WorldSafeLocs.db2 NOT exposed @68887), INIT_WORLD_STATES reinforcement counts. USER DECISION
   pending: S1 rules stay here or move to feature/pvp-rated-bg (self-contained/movable).
 feature/haranir-allied-race @ e6fcba98ae — #6 allied race. BASE RACE ALREADY AT BASELINE (upstream); creation+racials
   from DB2 (ChrRaces 86 Ally/91 Horde, SkillLine 2930). Adds permissive HasRaceUnlockAchievement seam (no regression).
   SQL: hotfixes/master/2026_08_13_00_hotfixes.sql (COMMENTED heritage-link, client-blocked HeritageArmorAchievementID=0).
   Heritage armor client-blocked; unlock enforcement config-gated refinement pending achiev 61506 earnable.
 feature/loa-blessings     @ 8b139a882f — #10 Zul'Aman altar worship. LoaBlessingMgr + npc_altar_of_blessings on creature
   256508; worship spine + 8 confirmed blessings LIVE. SQL: world/master/2026_08_13_00_world_loa_blessings.sql
   (loa_blessing_option + 8 rows + ScriptName UPDATE on 256508). CAPTURE: major×minor matrix pairings, Abundance reward wire.
 feature/delve-nemesis     @ 902deca352 — #9 T4+ escalation. NemesisMgr (folds into feature/delves DelveInstance/DelvesRewards);
   Pactsworn spine + Strongbox banding + Nullaeus solo achievement tail LIVE. SQL: world/master/2026_08_13_00_world_delve_nemesis.sql
   (nemesis_pactsworn_pack, empty). MERGE feature/delves FIRST. CAPTURE: Pactsworn creature entries, Torment's Rise scenario id, Strongbox loot.
NOTE: batch 2 NOT yet integration-validated together (batch 1's 5 were). All realm-safe (absent tables tolerated). Debug
 commands (.voidassault/.prey precedent; slayers/delve-nemesis GM cmds) TEMP-flagged.

---

## INTEGRATION-ONLY SHOP FIXES — must not be lost in a re-merge (2026-08-13)

Three pieces of work live **only on `integration/all-systems`** and are reachable from no feature
branch. If `feature/ingame-shop-battlepay` or `feature/commerce` is ever re-merged and these files are
resolved to the branch side, all three are silently lost and the in-game Shop breaks in ways that
produce **no error anywhere**.

| commit | what it does | symptom if lost |
|---|---|---|
| `4c03e9de9a` (merge resolution) | adapts `BattlePayMgr::AssembleCatalog` to the 94-record writer API | catalog assembly fails to compile, or falls back to serving 9 records |
| `81ea8d113a` | stops emitting `DISPLAY_FLAG_HIDE_WHEN_OWNED` / `HIDDEN_PRICE`, whose values were never verified | every product renders as already owned |
| `b8ea0ff99d` | clears `Product.Eligibility` and `Deliverable.AlreadyOwns` from the captured retail blob; stops overwriting `Product.Flags` | 12 products show as owned with a greyed Buy button, and every slot-pinned product becomes unpurchasable (`buyableHere` cleared) |

**Why they are not on a feature branch** (checked, not assumed):
`feature/ingame-shop-battlepay` (tip `a96906dacd`) still carries the OLD 9-record writer, so
`AssembleCatalog` there has a different shape and these hunks have nowhere to apply. Meanwhile the
current `BattlePayMgr.cpp` on this line is an assembly of at least four branches —
`ingame-shop-battlepay`, `commerce` (3 commits), `catalog-writer-94`, plus a token ledger commit — so
no single branch owns the file's present state. Transplanting this line's version onto the owner would
push other branches' work into it.

**Rule for the next re-merge:** for the four files below, `integration/all-systems` is the superset.
Resolve conflicts by keeping THIS line's version and re-applying the branch's genuinely new hunks on
top — never by taking the branch side wholesale.

    src/server/game/BattlePay/BattlePayCatalogWriter.h
    src/server/game/BattlePay/BattlePayCatalogWriter.cpp
    src/server/game/BattlePay/BattlePayMgr.h
    src/server/game/BattlePay/BattlePayMgr.cpp

Evidence for the fixes themselves: `c:\dumps\BATTLEPAY_DISPLAY_FLAGS_68275.md`,
`c:\dumps\BATTLEPAY_CATALOG_RECORD_FORMAT_68275.md`, `c:\dumps\SHOP_PURCHASE_BROKEN_DIAGNOSIS.md`.

### Also outstanding

`feature/pvp-rated-bg` (tip `6b16315443`) is committed but **INCOMPLETE and does not compile** — it is
deliberately NOT merged into either integration line. The commit message lists exactly what is missing.
