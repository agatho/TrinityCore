#!/bin/bash
# usage: reconstruct_system.sh <SYSDIR> <NS>   (NS defaults to SYSDIR)
SYS="$1"; NS="${2:-$1}"; BASE="${3:-upstream/master}"
# Paths are overridable via env; defaults match the 12.0.7->12.1 migration layout.
WT="${MIGRATE_WT:-I:/TrinityCore/_migrate121}"          # scratch worktree the reconstruction is staged in
BARE="${MIGRATE_BARE:-I:/TrinityCore/.bare}"; B="git --git-dir=$BARE"
mp="${MIGRATE_SRC:-origin/integration/all-systems}"     # graft SOURCE (the pre-migration integration)
SC="${MIGRATE_TOOLS:-$(cd "$(dirname "$0")" && pwd)}"   # this toolkit dir (holds graft_db2_store.py etc.)
git -C "$WT" cherry-pick --abort 2>/dev/null; git -C "$WT" checkout --detach "$BASE" >/dev/null 2>&1; git -C "$WT" reset --hard "$BASE" >/dev/null 2>&1; git -C "$WT" clean -fd src/ sql/ >/dev/null 2>&1
git -C "$WT" config user.name "Johannes Zoeller"; git -C "$WT" config user.email "johannes.zoeller@cws.com"

# 1) overlay owned files: dir + <SYS>-named packets/handler + <NS>-named packets
STEM="${SYS%s}"
owned=$($B ls-tree -r --name-only $mp 2>/dev/null | grep -E "src/server/game/$SYS/|/${STEM}Packets|/${STEM}Handler|/${STEM}Mgr|/${NS}Packets|/${NS}Handler" | while read f; do $B cat-file -e upstream/master:"$f" 2>/dev/null || echo "$f"; done)
for f in $owned; do git -C "$WT" checkout $mp -- "$f" 2>/dev/null; done
echo "overlaid $(echo "$owned" | wc -w) owned files"

# 2) overlay a <NS>PacketsCommon.h if integration has one 12.1 lacks or extends (superset-safe)
for cf in src/server/game/Server/Packets/${NS}PacketsCommon.h; do
  $B cat-file -e $mp:$cf 2>/dev/null && git -C "$WT" checkout $mp -- "$cf" 2>/dev/null && echo "overlaid $cf"
done

# 3) auto-discover DB2 stores referenced by overlaid files but absent from 12.1, graft each
stores=$(grep -rhoE 's[A-Z][A-Za-z0-9]+Store\b' $(for f in $owned; do echo "$WT/$f"; done) 2>/dev/null | sort -u | sed 's/^s//;s/Store$//')
for X in $stores; do
  if ! $B show upstream/master:src/server/game/DataStores/DB2Stores.h 2>/dev/null | grep -q "s${X}Store;"; then
    if $B show $mp:src/server/game/DataStores/DB2Structure.h 2>/dev/null | grep -q "struct ${X}Entry"; then
      python "$SC/graft_db2_store.py" "$X" 2>&1 | sed 's/^/   db2: /'
    fi
  fi
done

# 3b) CharacterDatabase statements the system adds (CHAR_*_<UPPER>*), graft enum + prepared stmts
UP=$(python3 -c "import re,sys;print(re.sub(r'(?<!^)(?=[A-Z])','_',sys.argv[1]).upper())" "$NS")
$B show $mp:src/server/database/Database/Implementation/CharacterDatabase.h 2>/dev/null | tr -d '\r' | grep -E "^    CHAR_[A-Z]+_${UP}(_|,)" > "$SC/cdb_enum.txt"
$B show $mp:src/server/database/Database/Implementation/CharacterDatabase.cpp 2>/dev/null | tr -d '\r' | grep -E "PrepareStatement\(CHAR_[A-Z]+_${UP}[_,]" > "$SC/cdb_stmt.txt"
python3 - "$SC" <<'PY'
import sys; import os as _os; SC=sys.argv[1]; WT=_os.environ.get("MIGRATE_WT","I:/TrinityCore/_migrate121")
en=open(f"{SC}/cdb_enum.txt",encoding="utf-8").read().rstrip('\n'); st=open(f"{SC}/cdb_stmt.txt",encoding="utf-8").read().rstrip('\n')
if en:
    p=f"{WT}/src/server/database/Database/Implementation/CharacterDatabase.h"; L=open(p,encoding="utf-8",newline="").read().split('\n')
    for i,l in enumerate(L):
        if "MAX_CHARACTERDATABASE_STATEMENTS" in l: L[i:i]=en.split('\n'); break
    open(p,"w",encoding="utf-8",newline="").write('\n'.join(L))
if st:
    p=f"{WT}/src/server/database/Database/Implementation/CharacterDatabase.cpp"; s=open(p,encoding="utf-8",newline="").read()
    idx=s.find("DoPrepareStatements()"); br=s.find("{",idx); nl=s.find("\n",br)+1
    open(p,"w",encoding="utf-8",newline="").write(s[:nl]+st+"\n"+s[nl:])
print(f"   cdb: {len(en.splitlines()) if en else 0} enum, {len(st.splitlines()) if st else 0} stmts")
PY

# 4) WorldSession graft: fwd-decl namespace + Handle*/Send* decls (both)
$B show $mp:src/server/game/Server/WorldSession.h 2>/dev/null | tr -d '\r' | awk -v ns="$NS" '$0=="    namespace "ns{p=1} p{print} p&&/^    }/{exit}' > "$SC/ws_fwd.txt"
$B show $mp:src/server/game/Server/WorldSession.h 2>/dev/null | tr -d '\r' | grep -E "void (Handle|Send)[A-Za-z0-9]*${NS}|WorldPackets::${NS}::" | grep -E "void (Handle|Send)" > "$SC/ws_decls.txt"
python3 - "$SC" "$NS" <<'PY'
import sys; SC,NS=sys.argv[1],sys.argv[2]; WT=__import__("os").environ.get("MIGRATE_WT","I:/TrinityCore/_migrate121"); p=f"{WT}/src/server/game/Server/WorldSession.h"
L=open(p,encoding="utf-8",newline="").read().split('\n')
fwd=open(f"{SC}/ws_fwd.txt",encoding="utf-8").read().rstrip('\n')
decls=open(f"{SC}/ws_decls.txt",encoding="utf-8").read().rstrip('\n')
whole='\n'.join(L)
if fwd and f"namespace {NS}\n" not in whole+'\n':
    for i,l in enumerate(L):
        if "namespace Movement" in l and l.strip().startswith("namespace"): L[i:i]=fwd.split('\n'); break
if decls:
    # dedup: only add decls not already present
    present=set(x.strip() for x in L)
    add=[d for d in decls.split('\n') if d.strip() and d.strip() not in present]
    for i,l in enumerate(L):
        if "HandleMoveRemoveInertiaAck" in l: L[i+1:i+1]=[f"        // {NS}"]+add; break
open(p,"w",encoding="utf-8",newline="").write('\n'.join(L))
print(f"   ws: fwd+{len(add) if decls else 0} handler decls")
PY
git -C "$WT" branch -f migrated/$SYS HEAD >/dev/null 2>&1
echo "$SYS: reconstruction staged (build next)"
