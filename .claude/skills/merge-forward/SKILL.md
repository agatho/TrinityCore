---
name: merge-forward
description: Merge a feature or release branch forward into an integration branch (or sync a worktree) in this TrinityCore/PlayerbotV2 repo. Use when asked to merge into integration/12_1_with-bots or integration/12_1_all, to "merge forward", to ship a feature branch into integration, or when a change on a feature branch needs to reach a testable integration branch.
---

# Merging forward

`feature/*` and `playerbot-v2` are golden source. `integration/*` branches are **disposable merge
products**: they only ever receive merges. Never cherry-pick *out* of one, never treat one as the
authority for a file, and **never force-push `integration/12_1_with-bots`** — it is developed from
several machines at once.

The whole merge runs through git plumbing, so no worktree is checked out or mutated. That is
deliberate: it keeps a half-merged tree from ever reaching a build, and it means a concurrent
session's work is never disturbed.

## Procedure

**1. Refresh both refs.** Never merge against a stale remote.

```bash
git fetch origin <src-branch> <dst-branch>
git log --oneline -1 origin/<src-branch>
git log --oneline -1 origin/<dst-branch>
```

**2. Merge in memory and read the result.**

```bash
T=$(git merge-tree --write-tree origin/<dst-branch> <src-sha>)
echo "rc=$? tree=$T"
```

`rc=0` plus a bare tree SHA means a clean merge. A non-zero rc prints the conflicting paths — resolve
them deliberately (see *Conflicts*), do not paper over them.

**3. Confirm the merge moves only what you intend.** This is the step that catches a bad merge before
it is published.

```bash
git log --oneline origin/<dst-branch>..<src-sha>     # commits entering dst
git diff --stat origin/<dst-branch> $T               # net file change
```

If a file you did not touch appears here, stop and find out why. Historically this is where a
reverted rename or a clobbered hunk shows up.

**4. Spot-check that the result really carries the change**, especially when `dst` has diverged:

```bash
git show $T:<path> | grep -c <a symbol your change introduced>
```

**5. Check the API surface your change depends on is the same on `dst`.** `integration/*` carries
upstream merges that `playerbot-v2` may not have yet. A clean text merge does not imply it compiles.

```bash
git diff --numstat <src-base> origin/<dst-branch> -- <header your change calls into>
```

If the header differs, diff it for the specific symbols you use before trusting the merge.

**6. Create the merge commit and push.**

```bash
M=$(GIT_AUTHOR_NAME=... GIT_AUTHOR_EMAIL=... GIT_COMMITTER_NAME=... GIT_COMMITTER_EMAIL=... \
    git commit-tree $T -p $(git rev-parse origin/<dst-branch>) -p <src-sha> -F- <<'MSG'
Merge <src-branch> into <dst-branch>

<what this brings in, in one short paragraph>
MSG
)
git push origin $M:refs/heads/<dst-branch>
```

Use an explicit `<sha>:refs/heads/<name>` refspec. A bare branch name can create a *new* branch when
the local spelling differs in case from the remote one — that has happened here.

**7. Build-verify the merged result** when the merge touches code (see *Verification*).

## Conflicts

Resolve hunk by hunk, in the merged content. Two shortcuts have caused real damage in this repo:

- **`git checkout --ours <file>`** discards the incoming side of the *whole file*, including hunks
  that merged cleanly. It has silently deleted entire classes that the other branch added.
- **Replacing a conflicted file wholesale** with one side's copy reverts every unrelated change the
  other side made to it — including renames, which then fail to compile.

If a conflict is large, resolve it in a scratch worktree, verify it builds, and merge from there —
never by picking a whole-file winner.

**`CLAUDE.md` add/add conflicts resolve to the reconciled root file** (the one carrying the branch
model, the VS-generator build notes and the "never assume the schema is called `world`" warning).
Several dormant branches still carry an older generic copy whose build and database sections
describe something this repo does not do; taking that side restores guidance that has already cost
a session. Feature-specific status notes belong on the feature branch, not in the shared file.

## Verification

A text-clean merge is not a working merge. When code moved, build the merge commit before calling it
done:

```bash
git worktree add --detach <tmp-path> <merge-sha>
# configure + build (see CLAUDE.md / CLAUDE.local.md for the toolchain)
git worktree remove --force <tmp-path>
```

Never build a worktree while editing it, and never reuse a build tree configured for a different
worktree — a stale cmake glob produces phantom `LNK2019`s that look like real errors.

Report the actual result: the MSBuild return code and the error count. `grep -c` returning zero
matches exits 1, so a command chain ending in a grep can report failure for a build that succeeded —
check the build's own return code, not the chain's.
