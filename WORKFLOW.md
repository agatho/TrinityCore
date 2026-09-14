# Repository layout, branching and integration workflow

The rule this document exists to protect:

> **Every feature branch is MERGED into the integration branch. Never cherry-picked.**

Not because merging is tidier, but because testers work on the integration branch and
their bug reports have to travel *back* to the feature branch. With a merge, git knows
the feature's commits are in integration, so a fix on the feature branch flows forward
on the next merge. With a cherry-pick, git knows nothing: the work is present under a
different SHA, ancestry lies, and every later merge re-applies or conflicts.

---

## 1. Layout

A single **bare** repository is the parent of every worktree, so no feature
directory is load-bearing:

```
I:\TrinityCore\
  .bare\                       the repository - objects and refs live here only
  <feature-name>\              worktree for feature/<feature-name>
  _integration\                worktree for integration/with-bots
  _integration-all-systems\    worktree for integration/all-systems
```

Why bare: the tree previously hung off `pet-battles\TrinityCore\.git`, so that
one feature folder was the parent of all seventeen worktrees. Deleting it (which
happened, 2026-07-21) took every worktree down at once. With `.bare` there is no
feature directory whose removal can break anything else, and `git worktree list`
is a complete, trustworthy inventory.

Run git commands from inside any worktree as normal. To address the repository
itself: `git --git-dir=I:/TrinityCore/.bare <cmd>`.

* Folder name == branch name minus the `feature/` prefix.
* Integration worktrees are prefixed `_` so they sort first and read as not-a-feature.
* **Nothing outside `I:\TrinityCore\`.** Worktrees elsewhere (e.g. `I:\tc_*`) break the
  convention and make it impossible to see the set at a glance.
* Two legacy worktrees nest a redundant level (`pet-battles\TrinityCore`,
  `housing-system\TrinityCore`). Left alone deliberately; flatten only when those
  branches are next touched, since moving invalidates their build directories.

Create and move worktrees with git so its metadata follows:

```sh
git worktree add    I:/TrinityCore/<name> feature/<name>
git worktree move   <old-path> I:/TrinityCore/<name>
git worktree list                       # the inventory - check it after any change
```

> Moving a worktree invalidates its `build/` directory: CMake caches absolute paths.
> Re-run `cmake -S . -B build ...` (or delete `build/`) after a move.

One worktree per feature is also what lets each feature have its own focused agent
session and its own memory, without cross-contamination.

---

## 2. Feature branches

* **Branch from the current integration base**, not from an arbitrary older commit.
* One feature, or one coherent system, per branch. It must be independently testable.
* Naming: `feature/<kebab-case-name>`.
* Keep the branch mergeable. Conflicts are fine and expected — *impossible* merges are not.

### If a feature branch cannot be merged

That is a signal about the **branch**, not a reason to cherry-pick. Diagnose it:

| Symptom | Cause | Fix |
|---|---|---|
| No merge-base at all | branch has a separate root (imported tree) | re-create the branch on top of the real base and re-apply the work |
| merge-base far behind | branched from an ancient commit | `git rebase --onto <base>` |
| Enormous unrelated diff | branch carries upstream churn or foreign work | isolate the feature commits onto a fresh branch |

Only once it merges does it go into integration.

---

## 3. Integration

```sh
git switch integration/with-bots
git merge --no-ff feature/<name>       # resolve conflicts here, never on the feature branch
```

* `--no-ff` keeps the merge visible in history.
* Resolve conflicts in the integration branch. Do **not** rewrite the feature branch to
  dodge a conflict; the feature branch stays the clean source of truth.
* After merging, verify ancestry is now true:

```sh
git merge-base --is-ancestor feature/<name> integration/with-bots && echo merged
```

That command is the whole point. When every feature branch answers `merged`, integration
completeness is a one-line check instead of a content hunt.

### Bug found by a tester on integration

1. Reproduce on the **feature branch**.
2. Fix it there, on that feature's worktree.
3. Merge the feature branch into integration again.

Never fix a feature's bug directly on integration — that fix would be invisible to the
feature branch and lost on the next merge.

---

## 4. Current state (2026-07-21)

Audited across 49 `feature/*` branches:

| State | Count |
|---|---|
| Already merged, ancestry intact | 20 |
| Mergeable from the shared base | 2 |
| Mergeable, but branched off a different commit | 25 |
| Cannot merge (no common ancestor) | **0** |
| Excluded — foreign code | 2 |

**No branch of ours is unmergeable.** The 25 are mergeable today; most sit on a commit
*newer* than the shared base, which is harmless. Ten are based ~29 commits *behind* it
and should be rebased onto the base before their next merge:
`cmsg-set-currency-flags`, `encounter-start-end`, `housing-system`,
`item-interaction-quest-enhancements`, `loss-of-control`, `misc-packet-structures`,
`quest-session`, `sell-all-junk-button`, `spell-category-cooldowns`, `pet-battles` (5 behind).

Integration was previously assembled with a mix of 105 real merges plus cherry-picks and
content ports for the awkward branches. The ported work is present and verified by
content, but its ancestry is not recorded — which is exactly why an audit needed
per-feature symbol probes. To restore truthful ancestry without changing any files:

```sh
git merge -s ours feature/<name>        # records the merge, keeps integration's tree
```

Use that only where the content is genuinely already present and verified.

### Permanently excluded

`feature/fake-party-frames-for-bots` and `feature/shaman-ground-targeted-spells` are a
third party's code, reviewed but **not ours**. Never merge, cherry-pick, or copy from
them. Verified absent from both integration lines (`Followship`, `FakePartyFrame`,
`fake_party`, `GroundTargeted`: 0 hits).

---

## 5. Verification before a push

* Build `--target worldserver`, never only `--target game`: `game` is a static library and
  never link-checks, so missing definitions stay invisible until the final link.
* After editing any core header the playerbot module includes, fully recompile the module
  (`find build-bots -name '*.obj' -path '*layerbot*' -delete`); stale objects give ABI skew
  that manifests as ACCESS_VIOLATION at varying addresses.
* Then confirm `remote == local` after pushing.

### Debugging a crash

Build with **Release codegen plus symbols** — stock RelWithDebInfo is not equivalent:

```sh
cmake -S . -B build-bots "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/Zi /O2 /Ob2 /DNDEBUG"
cmake --build build-bots --target worldserver --config RelWithDebInfo --parallel 2
```

CMake defaults RelWithDebInfo to `/Ob1` against Release's `/Ob2`; the different inlining
changes timing enough to hide races, and runs far slower. Put `worldserver.pdb` beside the
exe and TC's handler writes a fully symbolized stack to `<rundir>/Crashes/`. Do not try to
infer a crash site from a stripped binary — it produces confident, wrong answers.

Use `--parallel 2` for RelWithDebInfo; higher parallelism exhausts the PCH heap
(`C1076`/`C3859`). Pass `/Zi`-style flags from PowerShell — Git Bash rewrites a leading
`/` into a path.

---

## 6. Database migrations

* A new table needs a `sql/updates/` migration. A `sql/base/` entry alone is applied only
  when a database is created from scratch, and `sql/base/dev/` is never applied at all.
* `sql/custom/**` is never applied by the updater — anything there is a manual step.
* Prefer `DROP TABLE IF EXISTS` + `CREATE` for tables we own. `CREATE TABLE IF NOT EXISTS`
  silently keeps a pre-existing table of the same name with a *different* shape, and the
  next statement then fails on a missing column.
* Make migrations **idempotent** and guarded on `information_schema`. TC re-applies any
  released file whose hash changed, and MySQL stops at the first error — so one
  inapplicable statement silently skips the rest of the file, and TC treats a failed
  update as fatal.

---

## 7. Keeping current with upstream TrinityCore

There is **one remote**, `origin` = our fork (`github.com/agatho/TrinityCore.git`).
`origin/master` is our mirror of upstream TrinityCore and is the canonical base every
feature branch should sit on. Wire the real upstream once:

```sh
git --git-dir=I:/TrinityCore/.bare remote add upstream https://github.com/TrinityCore/TrinityCore.git
```

There are **two update cadences**, and they are not the same job.

### 7a. Minor bump (a patch within the same expansion, e.g. 12.0.5 -> 12.0.7)

Bugfixes and a handful of opcode/struct tweaks. Handle it at integration level:

1. `git fetch upstream && git checkout master && git merge --ff-only upstream/master && git push origin master`
2. `git merge master` into each integration line; resolve conflicts once, here.
3. Compile `--target worldserver`, run the offline validators (7d), boot-test.
4. Active feature branches rebase onto the new master; done ones ride in via integration.

### 7b. Major update (a version jump, e.g. 12.0.x -> 12.1) — this is a MIGRATION

A version jump renumbers opcodes, restructures DB2/wire, and adds or reworks whole
systems. **Do NOT try to verify this on the integration branch in one pass — the
codebase is too large and you WILL be overwhelmed.** The reason feature branches exist
is to make this tractable: each is a bounded, focusable subsystem (housing, warband,
delves, mythic+, crafting-orders ...). You never verify "everything" at once; you verify
one subsystem at a time, in its own branch, with its own tooling and agent context.

The subsystems are independent, so this **parallelizes**: give each feature branch its
own focused agent session (that is what the per-feature wrapper — `CLAUDE.md`, `.claude/`,
`.mcp.json`, and analysis tooling like `housing/sniff_verify/` — is FOR; never discard it).

Per feature branch, in focus:

1. Rebase onto the new base: `git rebase --onto <new-master> <old-base> feature/X`.
2. **Compile** the subsystem — catches renamed APIs and changed signatures.
3. **Verify opcodes / wire / DB2** — these do NOT compile-fail, they break silently.
   Use the deep sources in 7c and the offline validators in 7d.
4. Fix what the update broke; commit on the branch.
5. Record the subsystem's state in the migration manifest (7e).

Only after the branches are re-verified do you **rebuild integration from them** and
boot-test the whole. Integration is the product of the verified branches, not the place
the migration happens.

The 67186 -> 68275 migration was exactly this process (see
`MIGRATION_68275_PIPELINE_RESULT.md` and the per-system dossiers in memory). It was
hardest precisely where feature-branch focus had been lost.

### 7c. Deep information sources — you MUST dig into ALL of these

Verifying an update is NOT a code-reading exercise on the TC repo alone. TC is the
*server's* view; the ground truth for opcodes, wire layouts, DB2 structures, enums and
event payloads lives in the client and its data. Getting an update right means going deep
and thorough into every one of these. Do not shortcut this — a missed opcode renumber or
a changed DB2 field is silent until it corrupts the wire at runtime.

- **IDA Pro (client disassembly)** — `"C:/Program Files/IDA Professional 9.3/idat.exe"`,
  enriched IDB `c:/dumps/wow_dump.bin.i64`, cfunc-cache SQLite `c:/dumps/wow_dump.bin.tc_wow.db`
  (pseudocode keyed by real VA), plugin `tc_wow_analyzer` (~70 analyzers). Authority for:
  opcode dispatch, (de)serializer internals, wire field order, packet handlers,
  RTTI/vtables, hash/FDID resolution. Disassemble the client's own serializer for a CMSG
  to get its exact wire with no sniff (see `[[cmsg_wire_from_serializer_68275]]`).
- **Ghidra (fallback decompiler)** — `C:/Users/daimon/Downloads/ghidra_12.0.2_PUBLIC`,
  project `c:/dumps/ghidra_proj3/`. For functions Hex-Rays crashes on. Import `-noanalysis`.
- **WoWDBDefs** — `c:/dumps/WoWDBDefs/definitions/*.dbd` (1320 defs). Per-build, per-
  layout-hash DB2 column layouts. THE source for how a DB2 struct changed between builds
  (field inserted/renamed/resigned). A `.dbd` lists every BUILD a LAYOUT applies to — match
  the new build's layout hash for the exact field order (this caught the
  WarbandScenePlacement field insertion in 68275).
- **wago.tools** (online: https://wago.tools) — browsable DB2 data per build with
  build-to-build diffs, DBD, hotfixes, and the listfile. Fastest way to see WHAT changed in
  the data/structures between two builds before digging into the binary. Cross-check
  against WoWDBDefs. CASC listfile locally: `C:/Users/daimon/Downloads/CASCExplorer/listfile.csv`
  (FileDataID -> path).
- **WoW UI Lua / API docs** — `c:/dumps/external/wow-ui-source-12.0.5/` and the newer
  `external/wow-ui-source/`; 587 `*Documentation.lua` files under
  `Interface/AddOns/Blizzard_APIDocumentationGenerated/`. Authority for: enum value->name,
  event payload shapes, and UI-side field semantics. When the binary gives a bitfield but
  not its meaning, the Lua UI source names it (how CLUB_FINDER_REQUEST_TYPE and the
  neutral-faction u8 mapping were resolved). Diff the 12.1 UI source against 12.0.x to see
  which enums/events changed.
- **Client binary + AutoDump JSONs** — raw `c:/dumps/wow_dump.bin` (memory dump; real base
  `0x7FF7B3140000`, see `[[wow_dump_runtime_base_68275]]`), plus per-build
  `wow_opcode_dispatch_<build>.json`, `wow_db2_metadata_<build>.json`,
  `wow_jam_messages_<build>.json`, `wow_string_xrefs`, `wow_rtti`, etc. Regenerate via
  AutoDump on the new client; diff `wow_build_diff` for the moved/added/removed function
  buckets to target the migration.
- **Live sniffs** — `C:/sniff/*.pkt` (custom PKT 3.1; parser `parse_sniff_pkt.py`). Only a
  live capture on the new build settles a field VALUE the binary can't (reflection ceiling,
  `[[re_blindspot_map_and_full_catalog_68275]]`). Structure is recoverable offline; some
  semantics are not.
- **TrinityCore master** — `origin/master` / upstream. 15 years of RE; its opcode and
  struct declarations are authoritative for the new build once it updates
  (`[[tc_opcodes_verified_68275]]`, `[[feedback_tc_authority]]`). But the CLIENT BINARY
  outranks it in a wire conflict (`[[feedback_client_binary_is_arbiter]]`).

Rule of thumb: **the client binary is the final arbiter of wire; WoWDBDefs + wago for DB2
structure; UI Lua for enum/event semantics; TC for the server contract; a live sniff only
for values nothing else can give.** Consult ALL of them — no single source is complete,
and each update you must dig deep and thorough through every one.

### 7d. Offline validators (run per branch, and on integration)

Built during the 68275 migration; reusable every update (scratchpad / memory):

- `db2_validate.py` — replicates LoadDB2's structure assert offline (DB2Metadata vs
  DB2LoadInfo); catches every mismatched store in one pass. Note: both sides of that assert
  come from OUR headers — the `.db2` file is never read.
- `db2_signcheck.py` — replicates DB2FileLoader's signedness rule; Index/ParentIndexField
  are always unsigned.
- `stmt_check.py` — every prepared statement's columns vs the actual table shape.
- Opcode-value scan against the client binary + TC `Opcodes.h`; PE `.pdata` vtable-walk for
  handler recovery (`[[opcode_handler_recovery]]`).
- Then, always, a **boot test** — the only thing that exercises SQL, DB2 load, and the
  send/handler gates together.

### 7e. Migration manifest

Track every subsystem so nothing is silently skipped across dozens of branches. One row per
feature branch, columns: rebased / compiles / opcodes-verified / wire-verified /
DB2-verified / booted. A branch is "done" only when every column is checked. Silence is not
success — an unchecked column is an unverified subsystem, not a passing one.
