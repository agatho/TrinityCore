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

## SHOP FIXES — now on feature/commerce (2026-08-13, superseded note)

An earlier version of this section argued that three shop fixes could reasonably live only on
`integration/all-systems`. **That reasoning was wrong and the note is withdrawn.** Integration is a
test assembly that gets rebuilt from the feature branches; anything living only here is not stored at
all. All of it now sits on **`feature/commerce`** (commit `fefb03ea97`), which owns the BattlePay and
Shop2 files:

- the 94-record `BattlePayCatalogWriter` (was 9) plus three corrected field mappings
- `SMSG_BATTLE_PAY_PURCHASE_UPDATE` has no leading `Result` — the fix that made purchases work
- clearing `Product.Eligibility` / `Deliverable.AlreadyOwns` (the captured retail account's ownership)
- not overwriting `Product.Flags` with admin display flags (`buyableHere` lives in those bits)
- the two VAS status handlers

`feature/pvp-rated-bg` (`6b16315443`) still holds content not in any integration line, and is
deliberately unmerged because it does not compile. Its commit message lists what is missing.

> **STALE (verified 2026-08-13 late).** There is no branch `feature/pvp-rated-bg` in `.bare` any more,
> and `git branch -a --contains 6b16315443` returns nothing — that WIP commit is unreferenced. It is
> also superseded: the finished rated-battleground wire is `feature/pvp-queue-variants` @ `535244448c`,
> which is merged into BOTH integration lines. Nothing is missing; only the pointer was.


### §6 — NEXT INTEGRATION SESSION PROMPT (value-based test merge, prepared 2026-08-13)
Integration test merge — Midnight gap-build (session 2026-08-12/13). Full detail: §3h/§4/§5 above.

MERGE these branches (value-positive, testable now) from origin:
  feature/crafting-orders        7c0c615143   (dupe fix)
  feature/garrison-systems       fab2715fab   (dupe fix + covenant)
  feature/perks-program          fbe91d15a5   (Tender dupe fix)  + APPLY 2 auth SQL
  feature/pet-battles            eec4e6f053   (double-XP fix)
  feature/club-finder            cc01f30f17   (auto-accept/consent/authz)
  feature/omnium-folio           a1ab111366   (functional end-to-end)
  feature/devourer-spec          3ead6a3a12   (playable spec)
  feature/loa-blessings          8b139a882f   (altar worship + 8 blessings)
  feature/quelthalas-zone-events 26a07296f5   (framework + Saltheril, live quest 89289)

DO NOT MERGE yet (debug-harness/stub, no player value until captures land):
  feature/prey-voidforge, feature/void-assaults, feature/slayers-rise-bg, feature/delve-nemesis
SKIP: feature/haranir-allied-race (base race already at baseline).

ORDER / DEPS: substrate must be present first — content/midnight-s1, feature/world-quests,
  feature/warband, feature/delves, feature/mythic-plus, major-factions. Then the new content.
CONFLICT: src/server/game/World/World.cpp is the one guaranteed conflict — resolve by UNION
  (keep every manager's include + LoadFromDB() + Update() call).

SQL to APPLY (auto-updates are OFF — apply manually to activate; realm-safe if omitted, but dark):
  perks:  sql/updates/auth/master/2026_08_12_00_auth.sql, 2026_08_12_01_auth.sql
  omnium: world/master/2026_08_12_00_world_omnium_folio.sql,
          characters/master/2026_08_12_00_characters_omnium_folio.sql,
          world/master/2026_08_12_02_world_omnium_questlines.sql
  zone-events: world/master/2026_08_12_00_world_quelthalas_zone_events.sql
  loa:    world/master/2026_08_13_00_world_loa_blessings.sql
  devourer: world/master/2026_08_12_00_world_devourer_spec.sql (spell_script_names)

Build worldserver green, then bring the realm up centrally. Report any conflict beyond World.cpp.

### §6-VALIDATED (2026-08-13) — merge-set content branches pre-merged & build-tested TOGETHER, GREEN
Throwaway scratch off baseline 560165c0a6, merged omnium-folio -> quelthalas-zone-events -> devourer-spec ->
loa-blessings (deferred prey/void-assaults EXCLUDED; remediations excluded = disjoint files, not World.cpp).
COMBINED worldserver GREEN (0 compile/0 link errors). 6 SQL files, NO collisions.
 ONLY conflict = World.cpp LoadFromDB() region: union-keep ALL THREE sOmniumFolioMgr->LoadFromDB() +
 sZoneEventMgr->LoadFromDB() + sLoaBlessingMgr->LoadFromDB() (loa has NO per-tick Update BY DESIGN - altar/event
 driven, not a dropped call). spell_script_loader/custom_script_loader + per-manager Update() auto-merge (disjoint
 lines). No dup-symbol/missing-include fallout. Scratch branch _validate-mergeset local-only, never pushed; central
 realm untouched. => §6 merge set is a CONFIRMED GO.

### §7 — STATUS AUDIT + RE-MERGE FROM GOLDEN SOURCE (2026-08-13 late)

Every claim in §1-§6 was re-checked against `.bare` this session. Read this section BEFORE acting on
anything above it.

**Merged this session (both lines now agree with the golden source on these):**

| Branch | Tip | into `integration/all-systems` | into `integration/with-bots` |
|---|---|---|---|
| feature/encounter-start-end | 5e2c5f8686 | `2f318be572` | `203dd8c246` |
| feature/garrison-systems | 3addcd0356 | `3d4bdab9a2` | `fa380dc031` |
| feature/commerce | fefb03ea97 | `8ac9321f82` | `d1fcdd22ee` |
| feature/pvp-queue-variants | 535244448c | already in (`f480f0a3ab`) | `34e073c878` |
| feature/tradeskill-npc | fdf268f7ca | already in | `a31b35f04e` |

On all-systems the first three were content-neutral (the work had already arrived through short-lived
branches that were merged and deleted); the whole net delta is one superseded comment block in
`GarrisonPackets.h` plus one deduped pair of VAS declarations in `WorldSession.h`. On with-bots
`feature/commerce` was a REAL merge (22 conflicted files) because that line carried an older lineage of
the same commerce work — see commit `d1fcdd22ee` for the per-file decision record.

**Two things that merge as duplicates rather than as conflicts — check for them every time:**
git happily produces two copies of a declaration when the two sides put it in different places, and a
duplicate declaration is a compile error, not a merge marker. This session that hit
`WorldSession.h` (the two VAS handlers, both lines), `CharacterDatabase.{h,cpp}`
(`CHAR_SEL_ACCOUNT_TOTAL_MONEY`) and `BattlePayHandler.cpp` (a whole second
`HandleBattlePayGetPurchaseList` + a second VAS pair + a repeated `#include`). After every merge, grep
the touched files for repeated symbols; a clean `git status` proves nothing.

**RBAC id collision, now hit twice.** `feature/commerce` ships the two shop perms as 886/887, but 886
is `RBAC_PERM_USE_COMMENTATOR_MODE` (feature/commentator, already applied to the live auth DB). Both
integration lines renumber them to 887/888 in `RBAC.h` and in `2026_08_09_00_auth.sql`. **This should
be back-ported to `feature/commerce`** so the third merge does not have to rediscover it.

**SQL filename collisions (both lines resolved identically):**
- `auth/master/2026_07_20_01_auth.sql` — club-finder RBAC 1001 + the BattlePay purchase ledger. UNION.
- `auth/master/2026_08_12_00_auth.sql` — commerce's `account_battlepay_entitlement`; the two perks
  halves live at `2026_08_12_02/03_auth.sql` (byte-identical content, renamed).
- `auth/master/2026_08_09_00_auth.sql` — RBAC 887/888 per the renumber above.
NOTHING WAS APPLIED. The SQL still outstanding for with-bots is listed in the session report.

**Stale entries corrected:**
- §1's branch tips are historical; all eight of those branches are ancestors of `integration/all-systems`
  today. §1 is DONE, not a to-do list.
- §3b/§3c/§3d/§3e branch tips are all superseded, and every branch they name is now merged into
  all-systems. §3d is superseded by §3e (as it says); §3e's `feature/commerce @ 37e42f536d` is now
  `fefb03ea97`.
- The retired transport branches `content/midnight-s1`, `feature/major-factions-1207` and
  `content/midnight-s1-sqlfix` NO LONGER EXIST, so §3's housekeeping step 4 is done and §4's/§6's
  "merge order" and "substrate must be present first" lines that name them (and `feature/warband`,
  also gone) cannot be followed literally. The substrate they refer to IS present: world-quests,
  delves, mythic-plus, major-factions and lfg-list are all ancestors of all-systems.
- The `feature/pvp-rated-bg` note above §6 is stale — see the block there.

**Genuinely still outstanding from §6 — the four content branches are in NEITHER line:**
`feature/omnium-folio` (a1ab111366), `feature/loa-blessings` (8b139a882f),
`feature/quelthalas-zone-events` and `feature/devourer-spec`. The other five §6 entries
(crafting-orders, garrison-systems, perks-program, pet-battles, club-finder) are already in
all-systems. Two warnings before merging the four:
- the §6 tips are wrong for two of them, and the LOCAL branches are not the origin branches:
  `feature/quelthalas-zone-events` local `e857d66597` vs origin `26a07296f5` (local is BEHIND — §6's
  Saltheril work is only on origin), and `feature/devourer-spec` local `aa65673156` vs origin
  `3ead6a3a12` (DIVERGED, local is not a descendant). Decide which side is golden before merging.
- with-bots is missing far more than §6: `feature/world-quests`, `feature/mythic-plus`,
  `feature/delves`, `major-factions`, `feature/lfg-list` and `feature/ingame-shop-battlepay` are all
  in all-systems but NOT in with-bots. Rebuilding with-bots to parity is its own session.

### §7 — BACKLOG TIER 2 (2026-08-13, #8 + raid-season + Turbulent Timeways + small activities + housing pair)
All golden-source, all worldserver-green. NOT yet integration-validated together.
 feature/voidscar-arena         @ a9201cbea3 — #8 8th dungeon (map 2923 REAL). InstanceMapScript + 3-boss encounter journal
   (Taz'Rah/Atroxus/Charonus, DungeonEncounter 3285/3286/3287). SQL 2026_08_13_00_world_voidscar_arena.sql (instance_template
   2923). Bosses capture-blocked. Auto-joins M+ on MapChallengeMode DB2 row. DEP: feature/mythic-plus.
 feature/raid-season-s1         @ 104e16f467 — flex-Mythic Difficulty 233 (15-25) BUILT; Chiming Void Curio item 249367 =
   vendor token (Kirana, capture-blocked); Sporefall Map 1592 = DevMapE dev shell. SQL 2026_08_13_00_world_raid_season_s1.sql
   (raid_season_curio_reward EMPTY). No DB2 hotfix (233 stock).
 feature/turbulent-timeways     @ ff61add5c9 — weekly TW-rotation OFFER layer (chromie-time lacks it). Currency 1166, Holiday
   1425. SQL 2026_08_13_50_world_turbulent_timeways.sql (turbulent_timeways_rotation, 10 rows). DEP: feature/chromie-time.
   Gate worldstates + vendors capture-blocked.
 feature/midnight-small-activities @ e41b15724d — Ritual Sites BUILT (faction 2792/currency 3428/title 1291), Abyss Anglers
   reward-seam (currency 3373; dive capture-blocked), Darkspear Dash research-only. SQL 2026_08_13_30/31_world_*.
 feature/housing-system         @ d092de76b6 (advanced a08039ea49..d092de76b6) — OUTDOOR LIGHTING live + closes housing-audit
   A3 (cap 300/350) + A4 (light overlap, result 44); DECOR DUELS scaffolding (achiev cat 15574) + capture-blocked round seam.
   SQL 2026_08_13_00_world_decor_duel.sql (decor_duel_template, disabled seed).
VALUE NOTE for merge triage: buildable/testable-now = Voidscar (instanceable dungeon shell), flex-Mythic 233, Ritual Sites,
 housing outdoor-lighting (+audit fixes). Turbulent Timeways/Abyss/Decor-Duels/Darkspear = stub/scaffolding until captures.

### §6-UPDATED (2026-08-13) — SUPERSEDES §6. Full value-based test-merge prompt (both overnight runs + tier 2)
Integration test merge — Midnight gap-build. Value-based cut: merge what's testable NOW, hold stubs.
Full per-system detail: §3h/§4/§5/§7. Batch-1 content validated-together GREEN (§6-VALIDATED). Tier-2 value set
NOT yet validated-together — either run the same throwaway merge-build check first, or watch the World.cpp union.

MERGE (value-positive, testable now) from origin:
  # remediations (fix live dupe/security bugs)
  feature/crafting-orders          7c0c615143
  feature/garrison-systems         fab2715fab
  feature/perks-program            fbe91d15a5   + APPLY 2 auth SQL
  feature/pet-battles              eec4e6f053
  feature/club-finder              cc01f30f17
  # batch-1 content (validated together, §6-VALIDATED)
  feature/omnium-folio             a1ab111366   (functional end-to-end)
  feature/devourer-spec            3ead6a3a12   (playable spec)
  feature/loa-blessings            8b139a882f   (altar worship + 8 blessings)
  feature/quelthalas-zone-events   26a07296f5   (framework + Saltheril, live quest 89289)
  # tier-2 value (NOT yet validated together)
  feature/voidscar-arena           a9201cbea3   (instanceable 8th dungeon + encounter journal; map 2923 real)
  feature/raid-season-s1           104e16f467   (flex-Mythic Difficulty 233 live; no SQL needed for the difficulty)
  feature/midnight-small-activities e41b15724d  (Ritual Sites rep/title spine usable)
  feature/housing-system           d092de76b6   (outdoor lighting live + closes audit A3/A4; was already an integration branch)

DO NOT MERGE yet (debug-harness/stub/dep-gated — no player value until captures/deps land):
  feature/prey-voidforge, feature/void-assaults, feature/slayers-rise-bg, feature/delve-nemesis,
  feature/turbulent-timeways (needs feature/chromie-time + gate-worldstate captures)
SKIP: feature/haranir-allied-race (base race already at baseline).

ORDER / DEPS: substrate first — content/midnight-s1, feature/world-quests, feature/warband, feature/delves,
  feature/mythic-plus (also req by voidscar M+ + prey vault), major-factions, feature/chromie-time. Then content.
  Within new content: quelthalas-zone-events before void-assaults (deferred); feature/delves before delve-nemesis (deferred).
CONFLICT: src/server/game/World/World.cpp is the one guaranteed conflict — resolve by UNION, keeping ALL manager
  LoadFromDB()/Update() calls: sOmniumFolioMgr, sZoneEventMgr, sLoaBlessingMgr (+ sDecorDuelMgr from housing).
  (Loa + DecorDuel have no per-tick Update by design — not dropped calls.) Script-module loaders auto-merge (disjoint lines).

SQL to APPLY (auto-updates OFF — apply manually to activate; realm-safe if omitted, but dark):
  perks:  auth/master/2026_08_12_00_auth.sql, 2026_08_12_01_auth.sql
  omnium: world/master/2026_08_12_00_world_omnium_folio.sql, characters/master/2026_08_12_00_characters_omnium_folio.sql,
          world/master/2026_08_12_02_world_omnium_questlines.sql
  zone-events: world/master/2026_08_12_00_world_quelthalas_zone_events.sql
  devourer:    world/master/2026_08_12_00_world_devourer_spec.sql (spell_script_names)
  loa:         world/master/2026_08_13_00_world_loa_blessings.sql
  voidscar:    world/master/2026_08_13_00_world_voidscar_arena.sql (instance_template 2923)
  raid-season: world/master/2026_08_13_00_world_raid_season_s1.sql (curio table empty; flex-Mythic needs no SQL)
  small-act:   world/master/2026_08_13_30_world_ritual_sites.sql, world/master/2026_08_13_31_world_abyss_anglers.sql
  housing:     world/master/2026_08_13_00_world_decor_duel.sql (disabled seed)
  haranir hotfix = COMMENTED (client-blocked) — skip.

Build worldserver green, then bring the realm up centrally. Report any conflict beyond World.cpp.

### §6-UPDATED correction (2026-08-13): feature/loa-blessings tip advanced 8b139a882f -> 8679501705
Deepen pass seeded the FULL 44-option major×minor blessing matrix (36 pairings triple-confirmed from DB2, no capture;
SQL-only, in 2026_08_13_00_world_loa_blessings.sql). Merge the NEWER tip 8679501705 (matrix-complete) instead of
8b139a882f. Same SQL file, now with the full grid. Loa Blessings is content-complete for worship.
