import subprocess, sys
STORE = sys.argv[1]                      # e.g. GroupFinderActivity
import os
WT=os.environ.get("MIGRATE_WT","I:/TrinityCore/_migrate121")
GITDIR=os.environ.get("MIGRATE_BARE","I:/TrinityCore/.bare")
SRC=os.environ.get("MIGRATE_SRC","origin/integration/all-systems")  # pre-migration integration to graft the fork struct/LoadInfo from
def gitshow(p):
    r=subprocess.run(["git","--git-dir="+GITDIR,"show",f"{SRC}:{p}"],capture_output=True)
    return r.stdout.decode("utf-8","replace").replace('\r','')
def rd(p): return open(f"{WT}/{p}",encoding="utf-8",errors="replace",newline="").read()
def wr(p,s): open(f"{WT}/{p}","w",encoding="utf-8",newline="").write(s)
def extract_struct(text,name):
    L=text.split('\n'); out=[]; p=False
    for l in L:
        if l.strip()==f"struct {name}": p=True
        if p: out.append(l)
        if p and l.rstrip()=="};": break
    return '\n'.join(out)
def ins_after_struct(path,anchor_struct,block):
    L=rd(path).split('\n')
    for i,l in enumerate(L):
        if l.strip()==f"struct {anchor_struct}":
            for j in range(i,len(L)):
                if L[j].rstrip()=="};": L[j:j+1]=[L[j],"",block]; wr(path,'\n'.join(L)); return True
    print("  MISS struct",anchor_struct,"in",path); return False
def ins_after_line(path,anchor,block):
    L=rd(path).split('\n')
    for i,l in enumerate(L):
        if anchor in l: L[i:i+1]=[l,block]; wr(path,'\n'.join(L)); return True
    print("  MISS line",anchor,"in",path); return False

S=STORE
# 1) struct
st=extract_struct(gitshow("src/server/game/DataStores/DB2Structure.h"), f"{S}Entry")
if st: ins_after_struct("src/server/game/DataStores/DB2Structure.h","AchievementEntry",st)
# 2) loadinfo
li=extract_struct(gitshow("src/server/game/DataStores/DB2LoadInfo.h"), f"{S}LoadInfo")
if li: ins_after_struct("src/server/game/DataStores/DB2LoadInfo.h","AchievementLoadInfo",li)
# 3) meta only if 12.1 lacks it
if f"struct {S}Meta" not in rd("src/server/game/DataStores/DB2Metadata.h"):
    me=extract_struct(gitshow("src/server/game/DataStores/DB2Metadata.h"), f"{S}Meta")
    if me: ins_after_struct("src/server/game/DataStores/DB2Metadata.h","AchievementMeta",me)
# 4) store decl
for l in gitshow("src/server/game/DataStores/DB2Stores.h").split('\n'):
    if f"s{S}Store;" in l: ins_after_line("src/server/game/DataStores/DB2Stores.h","sAchievementStore;",l.strip() if False else l); break
# 5) store def + LOAD_DB2
for l in gitshow("src/server/game/DataStores/DB2Stores.cpp").split('\n'):
    if f's{S}Store("' in l: ins_after_line("src/server/game/DataStores/DB2Stores.cpp",'sAchievementStore("Achievement.db2"',l); break
for l in gitshow("src/server/game/DataStores/DB2Stores.cpp").split('\n'):
    if f"LOAD_DB2(s{S}Store)" in l: ins_after_line("src/server/game/DataStores/DB2Stores.cpp","LOAD_DB2(sAchievementStore)",l); break
# 6) hotfix enum entries (all HOTFIX_SEL_<STORE_UPPER>* lines, kept together & adjacent)
import re
up=re.sub(r'(?<!^)(?=[A-Z])','_',S).upper()   # CamelCase -> UPPER_SNAKE (approx)
hfh=gitshow("src/server/database/Database/Implementation/HotfixDatabase.h")
cur_hfh=rd("src/server/database/Database/Implementation/HotfixDatabase.h")
enum=[l for l in hfh.split('\n') if re.search(rf"HOTFIX_SEL_{up}\b|HOTFIX_SEL_{up}_", l)]
# skip enum lines 12.1 already defines (retail feature partial plumbing)
enum=[l for l in enum if l.strip().rstrip(',') not in set(x.strip().rstrip(',') for x in cur_hfh.split('\n'))]
if enum: ins_after_line("src/server/database/Database/Implementation/HotfixDatabase.h","HOTFIX_SEL_ACHIEVEMENT_LOCALE,","\n".join(enum))
else: print("  (no NEW hotfix enum for",up,")")
# 7) hotfix stmts: grab each statement (PrepareStatement/PREPARE_*_STMT ... );) whose macro line names HOTFIX_SEL_<up>
hfc=gitshow("src/server/database/Database/Implementation/HotfixDatabase.cpp").split('\n')
blk=[]; i=0; startre=re.compile(r"(PrepareStatement|PREPARE_MAX_ID_STMT|PREPARE_LOCALE_STMT)\(")
while i < len(hfc):
    l=hfc[i]
    if startre.search(l):
        # gather this whole statement (until a line ending in ');')
        stmt=[l]; j=i
        while not hfc[j].rstrip().endswith(");"):
            j+=1; stmt.append(hfc[j])
        if re.search(rf"HOTFIX_SEL_{up}\b", stmt[0]): blk.extend(stmt)
        i=j+1
    else: i+=1
# skip stmts 12.1 already prepares (avoid dup PrepareStatement)
cur_hfc=rd("src/server/database/Database/Implementation/HotfixDatabase.cpp")
already=set(re.findall(r"(?:PrepareStatement|PREPARE_MAX_ID_STMT|PREPARE_LOCALE_STMT)\((HOTFIX_SEL_[A-Z0-9_]+)",cur_hfc))
blk2=[]; i=0
while i<len(blk):
    if startre.search(blk[i]):
        j=i; stmt=[blk[i]]
        while not blk[j].rstrip().endswith(");"): j+=1; stmt.append(blk[j])
        nm=re.search(r"\((HOTFIX_SEL_[A-Z0-9_]+)",stmt[0])
        if not (nm and nm.group(1) in already): blk2.extend(stmt)
        i=j+1
    else: i+=1
blk=blk2
if blk: ins_after_line("src/server/database/Database/Implementation/HotfixDatabase.cpp","PREPARE_MAX_ID_STMT(HOTFIX_SEL_ACHIEVEMENT,","\n".join(blk))
else: print("  (no hotfix stmt for",up,")")

# 8) STALE-STRUCT GUARD: the struct/LoadInfo above were copied from the OLD 12.0.7
# integration (SRC), but the Meta comes from the 12.1 base (upstream/master) whenever
# the base already declares it (see step 3 "meta only if 12.1 lacks it"). If upstream
# advanced this db2's layout between 12.0.7 and 12.1, the grafted struct/LoadInfo is
# STALE and will mismatch the base Meta at runtime (LoadDB2 asserts type-string /
# record-size, or the fmt/DB2FileLoader crashes). Detect it here and fail loudly so the
# store is reconciled to the 12.1 client layout at reconstruction time, not at boot.
def meta_fieldcount(ref):
    r=subprocess.run(["git","--git-dir="+GITDIR,"show",f"{ref}:src/server/game/DataStores/DB2Metadata.h"],capture_output=True)
    txt=r.stdout.decode("utf-8","replace").replace('\r','').split('\n'); seen=False
    for l in txt:
        if l.strip()==f"struct {S}Meta": seen=True
        if seen and "FieldCount" in l:
            digits="".join(c for c in l if c.isdigit()); return int(digits) if digits else None
    return None
base_fc=meta_fieldcount("upstream/master"); src_fc=meta_fieldcount(SRC)
if base_fc is not None and src_fc is not None and base_fc!=src_fc:
    print("  !!!! STALE-STRUCT MISMATCH for",S,f": upstream/master Meta FieldCount={base_fc} but grafted-from Meta FieldCount={src_fc}.")
    print("       The struct/LoadInfo grafted from",SRC,"predate the 12.1 client layout.")
    print("       Reconcile",f"{S}Entry / {S}LoadInfo","to upstream/master's Meta (client .db2 is the arbiter) before building.")
    sys.exit(3)
print(f"grafted DB2 store: {S} (Meta FieldCount base={base_fc} src={src_fc})")
