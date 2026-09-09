#!/usr/bin/env python3
"""Audit PlayerbotV2 APL spell IDs against wago.tools DB2 CSV exports + simc trait_data.inc.

Usage: audit_spells.py <db2dir> <new_build> <old_build> <module_dir> <out_dir>
Env:   TRAIT_DATA_INC     path to simc trait_data.inc for <new_build> (default <db2dir>/../trait_data.inc)
       TRAIT_DATA_INC_OLD path to simc trait_data.inc for <old_build> (optional)
"""
import csv, re, sys, os, collections
from pathlib import Path

csv.field_size_limit(1 << 30)
DB2, NEW, OLD, MOD, OUT = sys.argv[1:6]
OUT = Path(OUT); OUT.mkdir(parents=True, exist_ok=True)

CLASS_BY_NAME = {"Warrior":1,"Paladin":2,"Hunter":3,"Rogue":4,"Priest":5,"DeathKnight":6,"DK":6,
                 "Shaman":7,"Mage":8,"Warlock":9,"Monk":10,"Druid":11,"DemonHunter":12,"DH":12,"Evoker":13}
PASSIVE = 0x40  # SPELL_ATTR0_PASSIVE

def load(build, table):
    p = Path(DB2) / build / f"{table}.csv"
    with open(p, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))

class Build:
    def __init__(self, build):
        self.build = build
        self.name = {int(r["ID"]): r["Name_lang"] for r in load(build, "SpellName")}
        self.attr0 = {}
        for r in load(build, "SpellMisc"):
            if r["DifficultyID"] == "0":
                self.attr0[int(r["SpellID"])] = int(r["Attributes_0"])
        cat = {int(r["ID"]): int(r["CategoryID"]) for r in load(build, "SkillLine")}
        self.class_skills = collections.defaultdict(set)
        for r in load(build, "SkillRaceClassInfo"):
            sk = int(r["SkillID"]); cm = int(r["ClassMask"])
            if cat.get(sk) != 7: continue
            if cm <= 0 or (cm & (cm - 1)) != 0: continue  # single-class lines only
            for c in range(1, 14):
                if cm & (1 << (c - 1)): self.class_skills[c].add(sk)
        self.class_spells = collections.defaultdict(set)
        self.supersedes = {}
        for r in load(build, "SkillLineAbility"):
            sk = int(r["SkillLine"]); sp = int(r["Spell"]); cm = int(r["ClassMask"])
            for c in range(1, 14):
                if sk in self.class_skills[c] and (cm == 0 or cm & (1 << (c - 1))):
                    self.class_spells[c].add(sp)
            sup = int(r["SupercedesSpell"])
            if sup: self.supersedes[sup] = sp
        self.learn = collections.defaultdict(list)
        try:
            for r in load(build, "SpellLearnSpell"):
                self.learn[int(r["SpellID"])].append(int(r["LearnSpellID"]))
        except FileNotFoundError:
            pass
        self.specs = {}
        for r in load(build, "ChrSpecialization"):
            cid = int(r["ClassID"])
            if 1 <= cid <= 13 and r["Name_lang"]:
                self.specs[int(r["ID"])] = (cid, r["Name_lang"])
        self.spec_spells = collections.defaultdict(set)
        self.spec_overrides = collections.defaultdict(dict)
        for r in load(build, "SpecializationSpells"):
            s = int(r["SpecID"]); sp = int(r["SpellID"]); ov = int(r["OverridesSpellID"])
            self.spec_spells[s].add(sp)
            if ov: self.spec_overrides[s][ov] = sp
        skill_tree = collections.defaultdict(set)
        for r in load(build, "SkillLineXTraitTree"):
            skill_tree[int(r["SkillLineID"])].add(int(r["TraitTreeID"]))
        self.class_trees = {c: set().union(*(skill_tree[s] for s in sks)) if sks else set()
                            for c, sks in self.class_skills.items()}
        tree_of_class = {}
        for c, ts in self.class_trees.items():
            for t in ts: tree_of_class[t] = c
        tdef = {int(r["ID"]): (int(r["SpellID"]), int(r["OverridesSpellID"]), int(r["VisibleSpellID"]))
                for r in load(build, "TraitDefinition")}
        tentry = {int(r["ID"]): (int(r["TraitDefinitionID"]), int(r["MaxRanks"]), int(r["NodeEntryType"]), int(r["TraitSubTreeID"]))
                  for r in load(build, "TraitNodeEntry")}
        self.node_tree = {int(r["ID"]): int(r["TraitTreeID"]) for r in load(build, "TraitNode")}
        self.node_entries = collections.defaultdict(list)
        for r in load(build, "TraitNodeXTraitNodeEntry"):
            self.node_entries[int(r["TraitNodeID"])].append(int(r["TraitNodeEntryID"]))
        specset = collections.defaultdict(set)
        for r in load(build, "SpecSetMember"):
            specset[int(r["SpecSet"])].add(int(r["ChrSpecializationID"]))
        cond_specs = {}
        for r in load(build, "TraitCond"):
            ss = int(r["SpecSetID"])
            # CondType 0=Available 1=Visible restrict availability; 2=Granted does not
            if ss and int(r["CondType"]) in (0, 1): cond_specs[int(r["ID"])] = specset.get(ss, set())
        node_specs = collections.defaultdict(set)
        node_has_speccond = set()
        for r in load(build, "TraitNodeXTraitCond"):
            cid = int(r["TraitCondID"]); n = int(r["TraitNodeID"])
            if cid in cond_specs:
                node_specs[n] |= cond_specs[cid]; node_has_speccond.add(n)
        group_nodes = collections.defaultdict(set)
        for r in load(build, "TraitNodeGroupXTraitNode"):
            group_nodes[int(r["TraitNodeGroupID"])].add(int(r["TraitNodeID"]))
        for r in load(build, "TraitNodeGroupXTraitCond"):
            cid = int(r["TraitCondID"]); g = int(r["TraitNodeGroupID"])
            if cid in cond_specs:
                for n in group_nodes[g]:
                    node_specs[n] |= cond_specs[cid]; node_has_speccond.add(n)
        self.subtree_name = {int(r["ID"]): r["Name_lang"] for r in load(build, "TraitSubTree")}
        self.spec_talents = collections.defaultdict(dict)
        self.spec_talent_overrides = collections.defaultdict(dict)
        self.class_talents = collections.defaultdict(dict)
        for node, tree in self.node_tree.items():
            c = tree_of_class.get(tree)
            if not c: continue
            allowed = node_specs[node] if node in node_has_speccond else {s for s, (cc, _) in self.specs.items() if cc == c}
            for e in self.node_entries[node]:
                d, ranks, ntype, sub = tentry.get(e, (0, 0, 0, 0))
                sp, ov, vis = tdef.get(d, (0, 0, 0))
                if not sp: continue
                for s in allowed:
                    self.spec_talents[s][sp] = (node, e, sub)
                    if ov: self.spec_talent_overrides[s][ov] = sp
                self.class_talents[c][sp] = (node, e, sub)

    def _expand_learn(self, spells):
        # SpellLearnSpell: learning X also learns its children (TC Player::LearnSpell follows this).
        out = set(spells); frontier = list(spells)
        while frontier:
            sp = frontier.pop()
            for child in self.learn.get(sp, ()):
                if child not in out: out.add(child); frontier.append(child)
        return out

    def base_set(self, spec):
        return self._expand_learn(self.class_spells[self.specs[spec][0]] | self.spec_spells[spec])

    def spec_set(self, spec):
        return self._expand_learn(self.base_set(spec) | set(self.spec_talents[spec]))

    def class_set(self, c):
        s = set(self.class_spells[c]) | set(self.class_talents[c])
        for sid, (cc, _) in self.specs.items():
            if cc == c: s |= self.spec_spells[sid]
        return self._expand_learn(s)

    def is_passive(self, sp):
        return bool(self.attr0.get(sp, 0) & PASSIVE)

new = Build(NEW); old = Build(OLD)
print(f"loaded {NEW}: {len(new.name)} spells; {OLD}: {len(old.name)} spells", file=sys.stderr)

# ---- simc trait_data.inc (authoritative live-client talent list) ----
SIMC_ROW = re.compile(r'^\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*"([^"]*)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*(\d+),\s*(\d+)')
def load_simc(path, build):
    cls_set = collections.defaultdict(set); spec_set = collections.defaultdict(set); info = {}
    if not path or not Path(path).exists(): return cls_set, spec_set, info
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        m = SIMC_ROW.match(line)
        if not m: continue
        tree_index, cls, entry, node, ranks, req, tdef, spell, repl, over = (int(m.group(i)) for i in range(1, 11))
        if spell == 0: continue
        name = m.group(14); specs = [int(x) for x in m.group(15).split(",") if x.strip() and int(x) != 0]
        sub = int(m.group(17))
        cls_set[cls].add(spell)
        info[spell] = (tree_index, name, specs, sub, over, cls)
        if specs:
            for sp in specs: spec_set[sp].add(spell)
        else:
            for sid, (cc, _) in build.specs.items():
                if cc == cls: spec_set[sid].add(spell)
    print(f"simc {path}: {sum(len(v) for v in cls_set.values())} talent spells", file=sys.stderr)
    return cls_set, spec_set, info

SIMC_CLASS, SIMC_SPEC, SIMC_INFO = load_simc(os.environ.get("TRAIT_DATA_INC", str(Path(DB2).parent / "trait_data.inc")), new)
OSIMC_CLASS, OSIMC_SPEC, OSIMC_INFO = load_simc(os.environ.get("TRAIT_DATA_INC_OLD", ""), old)

def avail_new(cid, spec):
    if spec: return new._expand_learn(new.base_set(spec) | set(new.spec_talents[spec]) | SIMC_SPEC.get(spec, set()))
    return new._expand_learn(new.class_set(cid) | SIMC_CLASS.get(cid, set()))
def avail_old(cid, spec):
    if spec: return old.base_set(spec) | set(old.spec_talents[spec]) | OSIMC_SPEC.get(spec, set())
    return old.class_set(cid) | OSIMC_CLASS.get(cid, set())

# ---- parse APL files ----
ID_RE = re.compile(r"^\s*(?:inline\s+)?constexpr\s+uint32(?:_t)?\s+([A-Za-z_0-9]+)\s*=\s*(\d+)\s*u?;\s*(?://\s*(.*))?$")
CAST_FN = r"(?:cast|cast_at|pet_cast|knows_spell|is_ready|can_cast_while_moving|charges|cd_remaining)"
AURA_FN = r"(?:has_aura|find_aura|enemy_without_my_aura|aura_stacks|find_pet_aura|has_mechanic|group_has_mechanic)"
def spec_lookup(build, cid, specname):
    key = specname.replace("_", "").lower()
    for sid, (c, n) in build.specs.items():
        if c == cid and n.replace(" ", "").lower() == key: return sid
    return None

apl_dir = Path(MOD) / "Combat" / "Apl"
files = sorted(p for p in apl_dir.glob("Apl_*.cpp") if not p.name.endswith(".bak"))
summary = []
per_file = {}
ROLES = {}
for p in files:
    m = re.match(r"Apl_(Baseline)_?(\w*)\.cpp", p.name) or re.match(r"Apl_(\w+?)_(\w+)\.cpp", p.name)
    if not m: continue
    if m.group(1) == "Baseline":
        cid = CLASS_BY_NAME.get(m.group(2)); spec = None
        if not cid: continue
    else:
        cid = CLASS_BY_NAME.get(m.group(1)); spec = spec_lookup(new, cid, m.group(2)) if cid else None
        if not cid: print("skip", p.name, file=sys.stderr); continue
    ids = []
    txt = p.read_text(encoding="utf-8", errors="replace")
    for ln, line in enumerate(txt.splitlines(), 1):
        mm = ID_RE.match(line)
        if mm and not mm.group(1).startswith("SPEC_"):
            ids.append((mm.group(1), int(mm.group(2)), (mm.group(3) or "").strip(), ln))
    roles = {}
    for name, sp, _, _ in ids:
        cast_use = re.search(r"\b" + CAST_FN + r"\(\s*" + re.escape(name) + r"\b", txt)
        aura_use = re.search(r"\b" + AURA_FN + r"\(\s*" + re.escape(name) + r"\b", txt)
        roles[name] = "CAST" if cast_use else ("AURA" if aura_use else "UNUSED")
    ROLES[p.name] = roles
    per_file[p.name] = (cid, spec, ids)

def classify(cid, spec, sp):
    nset = avail_new(cid, spec); oset = avail_old(cid, spec)
    nn = new.name.get(sp); on = old.name.get(sp)
    if nn is None: return "DEAD", f"not in {NEW} SpellName" + (f" (was '{on}')" if on else " (not in old either)")
    notes = []
    if on is not None and on != nn: notes.append(f"RENAMED '{on}' -> '{nn}'")
    if sp in nset: state = "OK"
    elif sp in oset: state = "LOST"
    else: state = "EXT"
    if new.is_passive(sp): notes.append("passive")
    if spec:
        in_simc = sp in SIMC_SPEC.get(spec, set()); in_db2t = sp in new.spec_talents[spec]
        if in_simc:
            sub = SIMC_INFO[sp][3]
            notes.append("talent" + (f"[{new.subtree_name.get(sub, 'hero')}]" if sub else ""))
        if SIMC_INFO and in_simc != in_db2t:
            notes.append("simc/db2-disagree" + ("(db2-only)" if in_db2t else "(simc-only)"))
        if not in_simc and sp in SIMC_INFO and sp not in new.base_set(spec):
            notes.append("talent-of-OTHER-spec:" + ",".join(map(str, SIMC_INFO[sp][2])) + f" cls{SIMC_INFO[sp][5]}")
        if state == "OK":
            ov = new.spec_overrides[spec].get(sp) or new.spec_talent_overrides[spec].get(sp)
            if ov: notes.append(f"overridden-by {ov} '{new.name.get(ov)}'")
    if state in ("LOST", "EXT"):
        cands = sorted(x for x in nset if new.name.get(x) == nn and x != sp)
        if cands: notes.append("same-name-available: " + ",".join(map(str, cands)))
        if sp in new.supersedes: notes.append(f"superseded-by {new.supersedes[sp]}")
    return state, "; ".join(notes)

report = []
tot = collections.Counter()
for fname, (cid, spec, ids) in per_file.items():
    specname = new.specs[spec][1] if spec else "(baseline)"
    rows = []
    cnt = collections.Counter()
    for name, sp, comment, ln in ids:
        st, note = classify(cid, spec, sp)
        role = ROLES[fname].get(name, "?")
        if st in ("EXT", "LOST", "DEAD") and role != "CAST": st = st + "-" + role
        cnt[st] += 1; tot[st] += 1
        rows.append((st, sp, name, new.name.get(sp) or old.name.get(sp) or "?", note, ln))
    added = []
    removed_from_spec = []
    if spec:
        nset = avail_new(cid, spec); oset = avail_old(cid, spec)
        used = {sp for _, sp, _, _ in ids}
        for sp in sorted(nset - oset):
            if sp in used or new.is_passive(sp): continue
            nm = new.name.get(sp, "?")
            if not nm or nm.endswith("(OLD)") or nm.startswith("[DNT]"): continue
            if sp in SIMC_SPEC.get(spec, set()):
                sub = SIMC_INFO[sp][3]; src = "talent" + (f" / {new.subtree_name.get(sub, 'hero')}" if sub else "")
            elif sp in new.spec_spells[spec]: src = "spec"
            elif sp in new.spec_talents[spec]: src = "db2-talent"
            else: src = "class"
            added.append((sp, nm, src))
        removed_from_spec = sorted((sp, old.name.get(sp, "?")) for sp in (oset - nset) if not old.is_passive(sp) and sp not in used)
    summary.append((fname, cid, specname, dict(cnt), len(added)))
    report.append(f"\n## {fname}  class={cid} spec={spec} {specname}\n")
    report.append(f"counts: {dict(cnt)}\n")
    for st, sp, name, nm, note, ln in rows:
        if st != "OK" or note:
            report.append(f"- [{st}] L{ln} {name} = {sp} '{nm}'  {note}")
    if added:
        report.append(f"\n### NEW active spells available to spec in {NEW} (not in {OLD} set, not referenced):")
        for sp, nm, src in added:
            report.append(f"- {sp} '{nm}' [{src}]")
    if removed_from_spec:
        report.append(f"\n### active spells no longer available to spec (were in {OLD} set; not referenced in file):")
        for sp, nm in removed_from_spec[:60]:
            report.append(f"- {sp} '{nm}'")

# ---- generic scan of constexpr spell-ish IDs outside Combat/Apl ----
report.append("\n\n# Non-APL constexpr IDs (existence check only; class-agnostic)\n")
GEN_RE = re.compile(r"constexpr\s+uint32(?:_t)?\s+(k?[A-Za-z_0-9]*(?:SPELL|Spell|spell|AURA|Aura|BUFF|Buff|MOUNT|Mount|HEARTH|Hearth|CAST|Cast)[A-Za-z_0-9]*)\s*=\s*(\d+)")
gen_cnt = collections.Counter()
for p in sorted(Path(MOD).rglob("*.cpp")) + sorted(Path(MOD).rglob("*.h")):
    rel = p.relative_to(MOD).as_posix()
    if rel.startswith("Combat/Apl") or p.name.endswith(".bak"): continue
    txt = p.read_text(encoding="utf-8", errors="replace")
    for mm in GEN_RE.finditer(txt):
        sp = int(mm.group(2))
        if sp < 100: continue
        if re.search(r"(Ms|Interval|Lockout|Ttl|Timeout|Radius|Range|Count|Pct|ITEM|Item)\b|_ITEM$|Ms$", mm.group(1)): continue
        nn = new.name.get(sp); on = old.name.get(sp)
        ln = txt.count("\n", 0, mm.start()) + 1
        if nn is None:
            gen_cnt["DEAD"] += 1
            report.append(f"- [DEAD] {rel}:{ln} {mm.group(1)} = {sp}" + (f" (was '{on}')" if on else " (not in old either)"))
        elif on is not None and on != nn:
            gen_cnt["RENAMED"] += 1
            report.append(f"- [RENAMED] {rel}:{ln} {mm.group(1)} = {sp} '{on}' -> '{nn}'")
        else:
            gen_cnt["OK"] += 1
report.append(f"\ngeneric counts: {dict(gen_cnt)}")

(OUT / "apl_audit.md").write_text("\n".join(report), encoding="utf-8")
with open(OUT / "apl_summary.tsv", "w", encoding="utf-8") as f:
    cols = ["OK", "LOST", "DEAD", "EXT", "EXT-AURA", "EXT-UNUSED", "LOST-AURA", "DEAD-AURA", "DEAD-UNUSED"]
    f.write("file\tclass\tspec\t" + "\t".join(cols) + "\tnew\n")
    for fname, cid, sn, cnt, nadd in summary:
        f.write(f"{fname}\t{cid}\t{sn}\t" + "\t".join(str(cnt.get(c, 0)) for c in cols) + f"\t{nadd}\n")
print("TOTAL", dict(tot))
ns = set(new.specs); os_ = set(old.specs)
for s in sorted(ns - os_): print("NEW SPEC", s, new.specs[s])
for s in sorted(os_ - ns): print("GONE SPEC", s, old.specs[s])
