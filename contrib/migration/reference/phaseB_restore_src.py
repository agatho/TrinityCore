import subprocess, sys, os, collections
GD = "I:/TrinityCore/.bare"
SC = os.path.dirname(os.path.abspath(__file__))
SRC = "origin/integration/all-systems"

def git(*a, **k):
    return subprocess.run(["git", "--git-dir", GD, *a], capture_output=True, text=True, **k)

# branch -> owning display name -> actual ref head name
def head_ref(branch):
    # major-factions-1207 is the real branch name
    return f"feature/{branch}" if branch != "major-factions-1207" else "feature/major-factions-1207"

# load manifest, SQL only, skip INTEGRATION/UNKNOWN
by = collections.defaultdict(list)
for line in open(os.path.join(SC, "restore_manifest.tsv"), encoding="utf-8"):
    br, path = line.rstrip("\n").split("\t", 1)
    if not path.startswith("src/server/"):
        continue
    if br in ("INTEGRATION", "UNKNOWN"):
        continue
    by[br].append(path)

only = sys.argv[1] if len(sys.argv) > 1 else None
results = []
for br in sorted(by):
    if only and br != only:
        continue
    ref = head_ref(br)
    tip = git("rev-parse", f"origin/{ref}").stdout.strip()
    if not tip:
        results.append((br, "NO-ORIGIN-REF", 0)); continue
    idx = os.path.join(SC, f"_idx_{br.replace('/','_')}")
    if os.path.exists(idx): os.remove(idx)
    env = dict(os.environ, GIT_INDEX_FILE=idx)
    subprocess.run(["git", "--git-dir", GD, "read-tree", tip], env=env, check=True)
    added = 0; skipped = []
    for path in by[br]:
        # skip if already present on branch tip
        chk = git("cat-file", "-e", f"{tip}:{path}")
        if chk.returncode == 0:
            skipped.append(path); continue
        blob = git("rev-parse", f"{SRC}:{path}").stdout.strip()
        if not blob or len(blob) != 40:
            skipped.append("MISSING-BLOB:" + path); continue
        r = subprocess.run(["git", "--git-dir", GD, "update-index", "--add",
                            "--cacheinfo", f"100644,{blob},{path}"], env=env, capture_output=True, text=True)
        if r.returncode != 0:
            skipped.append("ADD-FAIL:" + path + ":" + r.stderr.strip()); continue
        added += 1
    if added == 0:
        results.append((br, f"nothing to add (skipped {len(skipped)})", 0))
        if os.path.exists(idx): os.remove(idx)
        continue
    tree = subprocess.run(["git", "--git-dir", GD, "write-tree"], env=env, capture_output=True, text=True).stdout.strip()
    msg = (f"feat({br}): restore the dropped scripts / core files for the 12.1 reconstruction\n\n"
           f"The system reconstruction onto TC 12.1.0 carried the C++ and CharDB statement\n"
           f"enums but not the sql/updates/*/master files that create the tables and seed the\n"
           f"data. This restores {added} fork-custom source file(s) verbatim from the assembled\n"
           f"fork state (integration/all-systems) so the system's scripted content and code are present (integration build is the gate).\n\n"
           f"Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>")
    commit = subprocess.run(["git", "--git-dir", GD, "commit-tree", tree, "-p", tip, "-m", msg],
                            capture_output=True, text=True).stdout.strip()
    if os.path.exists(idx): os.remove(idx)
    if not commit:
        results.append((br, "COMMIT-FAIL", 0)); continue
    push = git("push", "origin", f"{commit}:refs/heads/{ref}")
    ok = push.returncode == 0
    results.append((br, ("pushed " + commit[:10]) if ok else ("PUSH-FAIL: " + push.stderr.strip()[-200:]), added))

print("=== Phase A results ===")
for br, st, n in results:
    print(f"{br:28s} +{n:3d}  {st}")
