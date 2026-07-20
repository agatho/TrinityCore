#!/usr/bin/env python3
"""baseline_coverage_audit — wago.tools-driven coverage check.

For each class, list spells that SkillLineAbility says auto-learn at
SpellLevel <= 9 (the pre-spec band) and check whether
Apl_Baseline.cpp's baseline_<class>::*_IDS arrays cover them. Prints
a per-class "missing" report.

Run from the repo root:
    python src/modules/PlayerbotV2/tools/baseline_coverage_audit.py

Requires the four wago.tools CSVs in /tmp/wago/:
    SkillLineAbility.csv, SpellName.csv, SpellLevels.csv, SkillLine.csv
The shell command in the docstring below downloads them. They're
~12 MB total and rarely change; cache locally and re-pull on patch.

    mkdir -p /tmp/wago && cd /tmp/wago && \\
    for t in SkillLineAbility SpellName SpellLevels SkillLine; do \\
      curl -so $t.csv "https://wago.tools/db2/$t/csv"; \\
    done
"""

import csv
import os
import re
from pathlib import Path
from collections import defaultdict

# /tmp/wago resolves to %TEMP%\wago on Windows when bash creates it.
# Python's os.path doesn't share git-bash's mount table, so use TEMP.
_TMP = os.environ.get("TEMP") or os.environ.get("TMP") or "/tmp"
WAGO_DIR     = Path(_TMP) / "wago"
REPO_ROOT    = Path(__file__).resolve().parents[3]
APL_DIR      = REPO_ROOT / "modules/PlayerbotV2/Combat/Apl"

# WoW class id → (ClassMask bit, audit slug, class SkillLine ID).
# SkillLineAbility rows can membership-tag a class via EITHER ClassMask
# (the bitfield) OR by pointing at the class's dedicated SkillLine ID
# with ClassMask=0. Hunter's Mark is in the latter form: ClassMask=0,
# SkillLine=795 (Hunter skills). Without checking both, hunter +
# evoker + several other classes drop out of the audit entirely.
# SkillLine IDs grepped from wago.tools SkillLine table for retail.
CLASSES = {
    1:  (1 << 0,  "warrior",     840),
    2:  (1 << 1,  "paladin",     800),
    3:  (1 << 2,  "hunter",      795),
    4:  (1 << 3,  "rogue",       921),
    5:  (1 << 4,  "priest",      804),
    6:  (1 << 5,  "dk",          796),    # slug aligns with Apl_Baseline_DK.cpp
    7:  (1 << 6,  "shaman",      924),
    8:  (1 << 7,  "mage",        904),
    9:  (1 << 8,  "warlock",     849),
    10: (1 << 9,  "monk",        829),
    11: (1 << 10, "druid",       798),
    12: (1 << 11, "dh",          1848),   # slug aligns with Apl_Baseline_DH.cpp
    13: (1 << 12, "evoker",      2810),
}
SKILLLINE_TO_CLASS = {sl: name for _id, (_bit, name, sl) in CLASSES.items()}

# Level threshold for "pre-spec baseline" coverage. Bots use the
# baseline rotation when level < 10, so any spell auto-learned at
# level <= 9 should appear in a baseline candidate list.
LEVEL_CAP = 9


def load_spell_names() -> dict[int, str]:
    out: dict[int, str] = {}
    with open(WAGO_DIR / "SpellName.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try: out[int(row["ID"])] = row.get("Name_lang", "")
            except ValueError: pass
    return out


def load_spell_levels() -> dict[int, int]:
    """SpellLevels can have multiple rows per spell (per difficulty);
    take the minimum SpellLevel across difficulties so we don't miss
    "available at L1" entries that also have a M+ tuning row.
    """
    out: dict[int, int] = {}
    with open(WAGO_DIR / "SpellLevels.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                sid = int(row["SpellID"]); lvl = int(row["SpellLevel"])
            except ValueError: continue
            if sid not in out or lvl < out[sid]: out[sid] = lvl
    return out


def load_skill_line_names() -> dict[int, str]:
    out: dict[int, str] = {}
    with open(WAGO_DIR / "SkillLine.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try: out[int(row["ID"])] = row.get("DisplayName_lang", "")
            except ValueError: pass
    return out


def load_skill_line_abilities() -> list[dict]:
    out: list[dict] = []
    with open(WAGO_DIR / "SkillLineAbility.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                out.append({
                    "spell":     int(row["Spell"]),
                    "skillline": int(row["SkillLine"]),
                    "classmask": int(row["ClassMask"]),
                    "minrank":   int(row["MinSkillLineRank"]),
                    "supersede": int(row.get("SupercedesSpell", 0) or 0),
                })
            except ValueError: continue
    return out


# Per-class baseline files. Each Apl_Baseline_<Class>.cpp declares its
# spell IDs as `constexpr uint32 NAME = <id>;` at the top of an
# anonymous namespace. We scan every such constant — and also any
# *_IDS arrays for hunter, which uses a candidate-list style — to get
# the full set of spell IDs the class baseline references.
CONST_RE = re.compile(
    r"constexpr\s+uint32\s+\w+\s*=\s*(\d+)\s*;"
)
ID_ARRAY_RE = re.compile(
    r"constexpr\s+uint32\s+\w+_IDS\s*\[\s*\]\s*=\s*\{([^}]*)\}",
    re.DOTALL,
)
# Filename casing: Apl_Baseline_Warrior.cpp → audit slug "warrior".
# Two exceptions where filename matches the namespace abbreviation:
# Apl_Baseline_DK.cpp → "dk", Apl_Baseline_DH.cpp → "dh".
FILENAME_TO_SLUG = {
    "Warrior": "warrior", "Paladin": "paladin", "Hunter": "hunter",
    "Rogue": "rogue", "Priest": "priest", "DK": "dk", "Shaman": "shaman",
    "Mage": "mage", "Warlock": "warlock", "Monk": "monk",
    "Druid": "druid", "DH": "dh", "Evoker": "evoker",
}


def parse_baseline_constants() -> dict[str, set[int]]:
    out: dict[str, set[int]] = {}
    for fn_part, slug in FILENAME_TO_SLUG.items():
        path = APL_DIR / f"Apl_Baseline_{fn_part}.cpp"
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        ids: set[int] = set()
        for m in CONST_RE.finditer(text):
            ids.add(int(m.group(1)))
        for arr in ID_ARRAY_RE.finditer(text):
            for tok in re.findall(r"\d+", arr.group(1)):
                ids.add(int(tok))
        out[slug] = ids
    return out


def main() -> int:
    print(f"[audit] loading wago.tools CSVs from {WAGO_DIR}")
    names      = load_spell_names()
    levels     = load_spell_levels()
    sl_names   = load_skill_line_names()
    abilities  = load_skill_line_abilities()
    print(f"[audit] {len(names):,} spell names, {len(levels):,} spell-levels,"
          f" {len(sl_names):,} skill lines, {len(abilities):,} ability rows")

    baseline_ids = parse_baseline_constants()
    print(f"[audit] parsed baseline constants for {len(baseline_ids)} classes:"
          f" {sorted(baseline_ids)}\n")

    # Per-class: collect spells whose SpellLevel <= LEVEL_CAP and whose
    # SkillLineAbility row references the class via ClassMask.
    # Some SkillLineAbility rows have ClassMask=0 (open to all classes,
    # mostly profession recipes). We include those when their SkillLine
    # is a class skill line (we tag with the class via cross-reference).
    coverage = defaultdict(lambda: {"learned": dict(), "missing": dict()})

    # SkillLine display-name prefix/exact filters: noise to exclude from
    # the rotation-coverage audit. Racials/proficiencies/mounts/glyphs/
    # languages don't appear in any combat APL and never will.
    SKIPPED_PREFIXES = ("Racial - ", "Racial: ", "Internal - ")
    SKIPPED_EXACT = {
        "Mounts", "Riding", "Languages", "Adventurer",
        "All - Glyphs", "GENERIC (DND)",
        # Armor / weapon proficiencies — passives, not rotational.
        "Plate Mail", "Mail", "Leather", "Cloth", "Shield",
        "Axes", "Two-Handed Axes", "Maces", "Two-Handed Maces",
        "Polearms", "Swords", "Two-Handed Swords", "Daggers",
        "Staves", "Fist Weapons", "Wands", "Bows", "Crossbows", "Guns",
        "Thrown",
    }
    def is_noise(skill_str: str) -> bool:
        if skill_str in SKIPPED_EXACT: return True
        return any(skill_str.startswith(p) for p in SKIPPED_PREFIXES)

    for r in abilities:
        sid       = r["spell"]
        mask      = r["classmask"]
        skill_id  = r["skillline"]
        if r["supersede"]:            # superseded by higher rank — skip
            continue
        # Determine which classes this row applies to. Two paths:
        # (a) ClassMask bits set → every bit-matched class.
        # (b) SkillLine ID is a class skill line → that one class.
        # A row may match neither (profession/shared) — skip those.
        matched_classes: set[str] = set()
        if mask != 0:
            for cls_id, (bit, cls_name, _sl) in CLASSES.items():
                if mask & bit: matched_classes.add(cls_name)
        if skill_id in SKILLLINE_TO_CLASS:
            matched_classes.add(SKILLLINE_TO_CLASS[skill_id])
        if not matched_classes: continue
        lvl = levels.get(sid, 0)
        # Require an actual character-level gate. L=0 entries are
        # passives / racials / proficiencies that aren't rotational.
        if lvl < 1: continue
        if lvl > LEVEL_CAP: continue
        skill_str = sl_names.get(skill_id, f"sl#{skill_id}")
        if is_noise(skill_str): continue
        name      = names.get(sid, f"<unknown:{sid}>")
        for cls_name in matched_classes:
            coverage[cls_name]["learned"][sid] = (name, lvl, skill_str)

    # Diff each class's learned-set against the parsed baseline constants.
    for cls in sorted(coverage.keys()):
        learned = coverage[cls]["learned"]
        covered = baseline_ids.get(cls, set())
        missing = {sid: meta for sid, meta in learned.items()
                   if sid not in covered}
        print(f"=== {cls.upper()}: {len(learned)} pre-spec spells in DB2, "
              f"baseline covers {len(covered & set(learned))}, "
              f"missing {len(missing)} ===")
        if not missing:
            print("  (nothing missing)\n")
            continue
        for sid, (nm, lvl, sk) in sorted(missing.items(),
                                          key=lambda kv: (kv[1][1], kv[0])):
            print(f"  L{lvl:>2}  {sid:>7}  {nm:<35}  [{sk}]")
        print()

    # Reverse audit: what's in baseline constants but DB2 says no class
    # auto-learns it pre-L10? Catches stale constants left from prior
    # patches. We check against the full learned union for the class.
    print("\n--- Reverse audit (baseline constants that no L<=9 hunter "
          "/ etc. learns per wago DB2) ---")
    for cls in sorted(baseline_ids):
        learned = set(coverage.get(cls, {"learned": {}})["learned"])
        stale = baseline_ids[cls] - learned
        if not stale: continue
        print(f"\n{cls.upper()}: {len(stale)} constants not in pre-L10 SkillLineAbility:")
        for sid in sorted(stale):
            nm = names.get(sid, "<unknown>")
            sl = levels.get(sid, "?")
            print(f"  {sid:>7}  {nm:<35}  (DB2 SpellLevel={sl})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
