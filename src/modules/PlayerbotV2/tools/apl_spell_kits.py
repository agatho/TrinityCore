#!/usr/bin/env python3
"""Build per-spec 12.1 ability kits (markdown + json) for the APL revision.

Usage: gen_kits.py <db2dir> <build> <trait_data.inc> <talent_selection.json> <module_dir> <audit.md> <out_dir>
"""
import csv, json, re, sys, collections
from pathlib import Path

csv.field_size_limit(1 << 30)
DB2, BUILD, INC, SEL, MOD, AUDIT, OUT = sys.argv[1:8]
D = Path(DB2) / BUILD
OUT = Path(OUT); OUT.mkdir(parents=True, exist_ok=True)

def load(t):
    with open(D / f"{t}.csv", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))

CLASS_NAME = {1:"Warrior",2:"Paladin",3:"Hunter",4:"Rogue",5:"Priest",6:"DeathKnight",7:"Shaman",8:"Mage",9:"Warlock",10:"Monk",11:"Druid",12:"DemonHunter",13:"Evoker"}
POWER = {-2:"Health",0:"Mana",1:"Rage",2:"Focus",3:"Energy",4:"ComboPoints",5:"Runes",6:"RunicPower",7:"SoulShards",8:"LunarPower",9:"HolyPower",11:"Maelstrom",12:"Chi",13:"Insanity",16:"ArcaneCharges",17:"Fury",18:"Pain",19:"Essence"}

name = {int(r["ID"]): r["Name_lang"] for r in load("SpellName")}
desc = {}
for r in load("Spell"):
    d = r["Description_lang"] or r["AuraDescription_lang"]
    if d: desc[int(r["ID"])] = re.sub(r"\s+", " ", d)[:260]
misc = {}
for r in load("SpellMisc"):
    if r["DifficultyID"] == "0":
        misc[int(r["SpellID"])] = r
cast_ms = {int(r["ID"]): int(r["Base"]) for r in load("SpellCastTimes")}
range_y = {int(r["ID"]): float(r["RangeMax_0"]) for r in load("SpellRange")}
dur_ms = {int(r["ID"]): int(r["Duration"]) for r in load("SpellDuration")}
cd = {}
for r in load("SpellCooldowns"):
    if r["DifficultyID"] == "0":
        cd[int(r["SpellID"])] = (int(r["RecoveryTime"]), int(r["CategoryRecoveryTime"]))
power = collections.defaultdict(list)
for r in load("SpellPower"):
    pt = int(r["PowerType"]); cost = int(r["ManaCost"]); pct = float(r["PowerCostPct"])
    if cost or pct:
        power[int(r["SpellID"])].append(f"{cost if cost else str(pct)+'%'} {POWER.get(pt, pt)}")
levels = {}
for r in load("SpellLevels"):
    if r["DifficultyID"] == "0":
        levels[int(r["SpellID"])] = int(r["SpellLevel"]) or int(r["BaseLevel"])
learn = collections.defaultdict(list)  # spell -> learned spells
for r in load("SpellLearnSpell"):
    learn[int(r["SpellID"])].append(int(r["LearnSpellID"]))
cat = {int(r["ID"]): int(r["CategoryID"]) for r in load("SkillLine")}
class_skills = collections.defaultdict(set)
for r in load("SkillRaceClassInfo"):
    sk = int(r["SkillID"]); cm = int(r["ClassMask"])
    if cat.get(sk) != 7 or cm <= 0 or (cm & (cm - 1)): continue
    for c in range(1, 14):
        if cm & (1 << (c - 1)): class_skills[c].add(sk)
class_spells = collections.defaultdict(dict)  # class -> spell -> (acquire, supersedes)
for r in load("SkillLineAbility"):
    sk = int(r["SkillLine"]); sp = int(r["Spell"]); cm = int(r["ClassMask"])
    for c in range(1, 14):
        if sk in class_skills[c] and (cm == 0 or cm & (1 << (c - 1))):
            class_spells[c][sp] = (int(r["AcquireMethod"]), int(r["SupercedesSpell"]))
specs = {int(r["ID"]): (int(r["ClassID"]), r["Name_lang"], int(r["Role"])) for r in load("ChrSpecialization") if 1 <= int(r["ClassID"]) <= 13 and r["Name_lang"]}
spec_spells = collections.defaultdict(dict)
for r in load("SpecializationSpells"):
    spec_spells[int(r["SpecID"])][int(r["SpellID"])] = int(r["OverridesSpellID"])
subtree = {int(r["ID"]): r["Name_lang"] for r in load("TraitSubTree")}

# simc traits
ROW = re.compile(r'^\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*"([^"]*)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*(\d+),\s*(\d+)')
traits = []
for line in Path(INC).read_text(encoding="utf-8", errors="replace").splitlines():
    m = ROW.match(line)
    if not m: continue
    g = m.groups()
    traits.append(dict(tree=int(g[0]), cls=int(g[1]), entry=int(g[2]), node=int(g[3]), ranks=int(g[4]), spell=int(g[7]),
                       override=int(g[9]), name=g[13], specs=[int(x) for x in g[14].split(",") if x.strip() and int(x)], sub=int(g[16]), ntype=int(g[17])))
sel = json.loads(Path(SEL).read_text(encoding="utf-8"))

def is_passive(sp): return bool(int(misc.get(sp, {}).get("Attributes_0", 0)) & 0x40) if sp in misc else False
def attr(sp, idx, bit): return bool(int(misc.get(sp, {}).get(f"Attributes_{idx}", 0)) & bit) if sp in misc else False

def fmt(sp, extra=""):
    if sp not in name: return f"- {sp} (NOT IN SpellName {BUILD})"
    m = misc.get(sp)
    bits = []
    if is_passive(sp): bits.append("PASSIVE")
    if attr(sp, 1, 0x100): bits.append("hidden")  # SPELL_ATTR1_DO_NOT_DISPLAY_IN_SPELLBOOK? keep as hint
    if m:
        ct = cast_ms.get(int(m["CastingTimeIndex"]), 0)
        bits.append(f"cast {ct/1000:g}s" if ct else "instant")
        rg = range_y.get(int(m["RangeIndex"]))
        if rg is not None: bits.append(f"range {rg:g}y")
        du = dur_ms.get(int(m["DurationIndex"]))
        if du and du > 0: bits.append(f"dur {du/1000:g}s")
    if sp in cd:
        rt, crt = cd[sp]
        if rt: bits.append(f"cd {rt/1000:g}s")
        elif crt: bits.append(f"catcd {crt/1000:g}s")
    if power.get(sp): bits.append("cost " + "/".join(power[sp]))
    if levels.get(sp): bits.append(f"L{levels[sp]}")
    if learn.get(sp): bits.append("teaches " + ",".join(f"{x} '{name.get(x,'?')}'" for x in learn[sp]))
    d = desc.get(sp, "")
    return f"- **{sp}** {name[sp]} [{'; '.join(bits)}]{(' ' + extra) if extra else ''}" + (f"\n    - {d}" if d else "")

# audit rows per file
audit_rows = collections.defaultdict(list)
cur = None
for line in Path(AUDIT).read_text(encoding="utf-8").splitlines():
    m = re.match(r"^## (\S+)", line)
    if m: cur = m.group(1); continue
    if cur and line.startswith("- ["): audit_rows[cur].append(line)

def spec_talents(sid, cid):
    rows = []
    for t in traits:
        if t["cls"] != cid: continue
        if t["specs"] and sid not in t["specs"]: continue
        rows.append(t)
    return rows

def write_kit(cid, sid):
    cname = CLASS_NAME[cid]
    if sid:
        sname, role = specs[sid][1], specs[sid][2]
        fname = f"Apl_{cname}_{sname.replace(' ', '')}.cpp"
        title = f"{cname} {sname} (spec {sid}, role {role})"
    else:
        sname, role = "Baseline", -1
        fname = f"Apl_Baseline_{cname if cname not in ('DeathKnight','DemonHunter') else ('DK' if cname=='DeathKnight' else 'DH')}.cpp"
        title = f"{cname} baseline (class-wide, all specs)"
    out = [f"# 12.1 kit: {title}", f"Build {BUILD}. APL file: `{fname}`", ""]
    # baseline
    out.append("## Class baseline spells (SkillLineAbility, active, non-passive)")
    for sp, (acq, sup) in sorted(class_spells[cid].items(), key=lambda kv: (levels.get(kv[0], 0), kv[0])):
        if is_passive(sp) or sp not in name: continue
        if name[sp].endswith("(OLD)") or name[sp].startswith("[DNT]"): continue
        if desc.get(sp, "").startswith("$@spelldesc") or "Hotbar Slot" in name[sp] or "Off-Hand" in name[sp] or name[sp] == "Attack": continue
        out.append(fmt(sp, f"(supersedes {sup})" if sup else ""))
    out.append("")
    if sid:
        out.append(f"## Spec spells (SpecializationSpells for {sid})")
        for sp, ov in sorted(spec_spells[sid].items()):
            if sp not in name or is_passive(sp): continue
            out.append(fmt(sp, f"(overrides {ov} '{name.get(ov,'?')}')" if ov else ""))
        out.append("")
        raid = sel.get(str(sid), {})
        raid_sp = {s[0]: s[4] for s in raid.get("spells", [])}
        mp_sp = {s[0]: s[4] for s in raid.get("mplus", {}).get("spells", [])}
        out.append(f"## Talents available to the spec (simc trait_data {BUILD})")
        out.append(f"Curated builds: raid/default = `{raid.get('label','?')}`; M+ = `{raid.get('mplus',{}).get('label','(none)')}`.")
        out.append("Marks: [R] selected in raid/default build, [M] selected in M+ build. Passive talents listed separately.")
        rows = spec_talents(sid, cid)
        def tree_label(t):
            return {1: "class", 2: "spec", 3: f"hero:{subtree.get(t['sub'], t['sub'])}", 4: "hero-select"}.get(t["tree"], str(t["tree"]))
        seen = set()
        for grp, label in ((1, "Class tree"), (2, "Spec tree"), (3, "Hero trees")):
            out.append(f"### {label} - active")
            for t in sorted(rows, key=lambda t: (t["tree"], t["sub"], t["name"])):
                if t["tree"] != grp or t["spell"] in seen or is_passive(t["spell"]): continue
                seen.add(t["spell"])
                marks = ("[R]" if t["spell"] in raid_sp else "") + ("[M]" if t["spell"] in mp_sp else "")
                extra = f"{marks} tree={tree_label(t)} node={t['node']} entry={t['entry']}" + (f" OVERRIDES {t['override']} '{name.get(t['override'],'?')}'" if t["override"] else "")
                out.append(fmt(t["spell"], extra))
            out.append(f"### {label} - passive (names only)")
            names = []
            for t in sorted(rows, key=lambda t: t["name"]):
                if t["tree"] != grp or t["spell"] in seen or not is_passive(t["spell"]): continue
                seen.add(t["spell"])
                marks = ("[R]" if t["spell"] in raid_sp else "") + ("[M]" if t["spell"] in mp_sp else "")
                names.append(f"{t['spell']} {t['name']}{marks}" + (f" (teaches {','.join(map(str, learn[t['spell']]))})" if learn.get(t["spell"]) else ""))
            out.append("; ".join(names)); out.append("")
    # current APL audit
    out.append(f"## Current APL audit rows ({fname})")
    out += audit_rows.get(fname, ["(none)"])
    out.append("")
    (OUT / fname.replace(".cpp", ".md")).write_text("\n".join(out), encoding="utf-8")

for cid in range(1, 14):
    write_kit(cid, None)
    for sid, (c, n, role) in specs.items():
        if c == cid and n != "Initial":
            write_kit(cid, sid)
print("kits written:", len(list(OUT.glob("*.md"))))
