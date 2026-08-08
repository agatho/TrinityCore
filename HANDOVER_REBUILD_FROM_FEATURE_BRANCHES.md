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

## 4. Do NOT

- Merge `content/midnight-s1` or `feature/major-factions-1207` (retired transport branches).
- Develop anything on integration — fixes found during boot/testing go to the OWNING feature branch
  first, then re-merge here.
