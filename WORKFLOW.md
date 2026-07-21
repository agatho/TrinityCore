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

One worktree per feature, all under `I:\TrinityCore\`:

```
I:\TrinityCore\
  <feature-name>\              worktree for feature/<feature-name>
  _integration\                worktree for integration/with-bots
  _integration-all-systems\    worktree for integration/all-systems
```

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
