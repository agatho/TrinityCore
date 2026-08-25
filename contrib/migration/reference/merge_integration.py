import subprocess, os, sys
WT = "I:/TrinityCore/_rebuild-integ"
SC = os.path.dirname(os.path.abspath(__file__))

def g(*a, **k):
    return subprocess.run(["git", "-C", WT, *a], capture_output=True, text=True, errors="replace", **k)

def gb(*a):
    # bytes-safe git for file content
    return subprocess.run(["git", "-C", WT, *a], capture_output=True)

branches = [l.strip() for l in open(os.path.join(SC, "merge_set.txt")) if l.strip()]
start = sys.argv[1] if len(sys.argv) > 1 else None
started = start is None
results = []
for br in branches:
    if not started:
        if br == start: started = True
        else: continue
    ref = f"origin/{br}"
    m = g("merge", "--no-ff", "--no-edit", "-m", f"Merge {br} into integration/all-systems (12.1)", ref)
    if m.returncode == 0:
        results.append((br, "clean")); continue
    # conflicts: enumerate unmerged
    st = g("diff", "--name-only", "--diff-filter=U")
    conflicted = [f for f in st.stdout.splitlines() if f.strip()]
    # also handle add/add and modify/delete
    unmerged = g("ls-files", "-u").stdout
    if not conflicted and "CONFLICT" not in (m.stdout + m.stderr):
        # merge failed for another reason
        g("merge", "--abort")
        results.append((br, "MERGE-FAIL: " + (m.stderr.strip()[-160:] or m.stdout.strip()[-160:])))
        continue
    union_ok = []; hard = []
    for f in conflicted:
        # stages (bytes-safe)
        b = gb("show", f":1:{f}"); o = gb("show", f":2:{f}"); t = gb("show", f":3:{f}")
        absf = os.path.join(WT, f)
        if o.returncode != 0 and t.returncode != 0:
            hard.append(f + "(both-del)"); continue
        if o.returncode != 0:  # ours deleted, theirs modified -> take theirs
            open(absf, "wb").write(t.stdout)
            g("add", f); union_ok.append(f + "(take-theirs)"); continue
        if t.returncode != 0:  # theirs deleted, ours modified -> take ours
            g("add", f); union_ok.append(f + "(keep-ours)"); continue
        # 3-way union (bytes)
        import tempfile
        d = tempfile.mkdtemp()
        pb, po, pt = os.path.join(d, "b"), os.path.join(d, "o"), os.path.join(d, "t")
        open(pb, "wb").write(b.stdout); open(po, "wb").write(o.stdout); open(pt, "wb").write(t.stdout)
        subprocess.run(["git", "merge-file", "--union", po, pb, pt], capture_output=True)
        open(absf, "wb").write(open(po, "rb").read())
        g("add", f); union_ok.append(f)
    if hard:
        results.append((br, "HARD-CONFLICT: " + ", ".join(hard)))
        # leave in conflicted state for manual handling; abort to keep assembly clean
        g("merge", "--abort")
        break
    c = g("commit", "--no-edit")
    results.append((br, f"union-merged ({len(union_ok)} files)"))

print("=== merge results ===")
for br, st in results:
    print(f"{br:42s} {st}")
print(f"\nassembly HEAD: {g('rev-parse','--short','HEAD').stdout.strip()}")
print(f"merged so far: {sum(1 for _,s in results if s=='clean' or s.startswith('union'))}/{len(branches)}")
