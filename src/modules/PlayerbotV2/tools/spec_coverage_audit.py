#!/usr/bin/env python3
"""spec_coverage_audit — wago.tools-driven per-spec coverage check.

For each (class, spec) we ship a rotation for, list the spells the
spec's SpecializationSpells DB2 table says are CORE (i.e. spec-defining
abilities like Frostbolt for Frost mage, Mortal Strike for Arms warrior,
Riptide for Restoration shaman) and check whether the Apl_<Class>_<Spec>.cpp
declares them. Prints per-spec missing reports.

Runs from repo root:
    python src/modules/PlayerbotV2/tools/spec_coverage_audit.py

Requires CSVs in /tmp/wago/:
    SpellName.csv, SpellLevels.csv, SpecializationSpells.csv, ChrSpecialization.csv

    cd /tmp/wago && for t in SpellName SpellLevels SpecializationSpells \
        ChrSpecialization; do curl -so $t.csv "https://wago.tools/db2/$t/csv"; done
"""

import csv
import os
import re
from pathlib import Path
from collections import defaultdict

_TMP = os.environ.get("TEMP") or os.environ.get("TMP") or "/tmp"
WAGO_DIR  = Path(_TMP) / "wago"
REPO_ROOT = Path(__file__).resolve().parents[3]
APL_DIR   = REPO_ROOT / "modules/PlayerbotV2/Combat/Apl"

# Maps the Apl_<Class>_<Spec>.cpp filename → ChrSpecialization.ID.
# Cross-checked against the RegisterRotation calls in each spec file
# (e.g. Apl_Hunter_BeastMastery.cpp uses spec id 253 → Beast Mastery).
SPECS = {
    # Death Knight
    "DeathKnight_Blood":      250,
    "DeathKnight_Frost":      251,
    "DeathKnight_Unholy":     252,
    # Demon Hunter
    "DemonHunter_Havoc":      577,
    "DemonHunter_Vengeance":  581,
    # Druid
    "Druid_Balance":          102,
    "Druid_Feral":            103,
    "Druid_Guardian":         104,
    "Druid_Restoration":      105,
    # Evoker
    "Evoker_Devastation":     1467,
    "Evoker_Preservation":    1468,
    "Evoker_Augmentation":    1473,
    # Hunter
    "Hunter_BeastMastery":    253,
    "Hunter_Marksmanship":    254,
    "Hunter_Survival":        255,
    # Mage
    "Mage_Arcane":            62,
    "Mage_Fire":              63,
    "Mage_Frost":             64,
    # Monk
    "Monk_Brewmaster":        268,
    "Monk_Mistweaver":        270,
    "Monk_Windwalker":        269,
    # Paladin
    "Paladin_Holy":           65,
    "Paladin_Protection":     66,
    "Paladin_Retribution":    70,
    # Priest
    "Priest_Discipline":      256,
    "Priest_Holy":            257,
    "Priest_Shadow":          258,
    # Rogue
    "Rogue_Assassination":    259,
    "Rogue_Outlaw":           260,
    "Rogue_Subtlety":         261,
    # Shaman
    "Shaman_Elemental":       262,
    "Shaman_Enhancement":     263,
    "Shaman_Restoration":     264,
    # Warlock
    "Warlock_Affliction":     265,
    "Warlock_Demonology":     266,
    "Warlock_Destruction":    267,
    # Warrior
    "Warrior_Arms":           71,
    "Warrior_Fury":           72,
    "Warrior_Protection":     73,
}


def load_spell_names() -> dict[int, str]:
    out: dict[int, str] = {}
    with open(WAGO_DIR / "SpellName.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try: out[int(row["ID"])] = row.get("Name_lang", "")
            except ValueError: pass
    return out


def load_spell_levels() -> dict[int, int]:
    out: dict[int, int] = {}
    with open(WAGO_DIR / "SpellLevels.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                sid = int(row["SpellID"]); lvl = int(row["SpellLevel"])
            except ValueError: continue
            if sid not in out or lvl < out[sid]: out[sid] = lvl
    return out


def load_specialization_spells() -> dict[int, list[tuple[int, int]]]:
    """SpecID → list of (SpellID, OverridesSpellID). OverridesSpellID
    is non-zero when the spec's variant replaces a baseline spell at
    runtime (e.g. Holy Paladin Word of Glory overrides the baseline
    cast). The rotation should know about both forms if possible."""
    out: dict[int, list[tuple[int, int]]] = defaultdict(list)
    with open(WAGO_DIR / "SpecializationSpells.csv", newline="",
              encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                sid = int(row["SpecID"])
                spell = int(row["SpellID"])
                override = int(row.get("OverridesSpellID", 0) or 0)
            except ValueError: continue
            out[sid].append((spell, override))
    return out


# Per-spec rotation file constants. Each Apl_<Class>_<Spec>.cpp declares
# spell IDs via `constexpr uint32 NAME = <id>;` and/or candidate-list
# arrays `constexpr uint32 NAME_IDS[] = { ... };`. Scan both.
CONST_RE = re.compile(r"constexpr\s+uint32\s+\w+\s*=\s*(\d+)\s*;")
ARRAY_RE = re.compile(
    r"constexpr\s+uint32\s+\w+(?:_IDS)?\s*\[\s*\]\s*=\s*\{([^}]*)\}",
    re.DOTALL,
)


def parse_spec_constants(filename_part: str) -> set[int] | None:
    path = APL_DIR / f"Apl_{filename_part}.cpp"
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    ids: set[int] = set()
    for m in CONST_RE.finditer(text):
        ids.add(int(m.group(1)))
    for m in ARRAY_RE.finditer(text):
        for tok in re.findall(r"\d+", m.group(1)):
            ids.add(int(tok))
    return ids


def main() -> int:
    print(f"[spec-audit] loading wago.tools CSVs from {WAGO_DIR}")
    names    = load_spell_names()
    levels   = load_spell_levels()
    spec_map = load_specialization_spells()
    print(f"[spec-audit] {len(names):,} spells, "
          f"{len(spec_map):,} specs in SpecializationSpells")

    summary: list[tuple[str, int, int, int]] = []
    detail_lines: list[str] = []

    # Aggressive noise filter — SpecializationSpells contains lots of
    # internal markers, role flags, mastery passives, and proficiency
    # auras that have no rotational meaning. Strip them by name
    # patterns + spec-name match + level=0 unknowns.
    SKIP_PREFIXES = (
        "Mastery: ", "Aura - ", "Stat Negation Aura",
        "<unknown:", "Plate Specialization",
        "Mail Specialization", "Leather Specialization",
        "Cloth Specialization", "Single-Button",
    )
    SKIP_SUBSTRINGS = (
        "Specialization", "Fortification", "Passive",
        "Activation", " Aura", "(DND)",
    )
    SKIP_EXACT = {
        "Death Knight", "Demon Hunter", "Druid", "Evoker",
        "Hunter", "Mage", "Monk", "Paladin", "Priest",
        "Rogue", "Shaman", "Warlock", "Warrior",
        "Plate Mail", "Mail", "Leather", "Cloth",
        "Riposte", "Crimson Scourge",
    }
    def is_noise(nm: str, spec_file: str) -> bool:
        if not nm: return True
        if nm in SKIP_EXACT: return True
        if any(nm.startswith(p) for p in SKIP_PREFIXES): return True
        if any(s in nm for s in SKIP_SUBSTRINGS): return True
        # Skip names that look like spec/class identity markers (e.g.
        # "Blood Death Knight" appears in Blood DK SpecializationSpells).
        spec_parts = spec_file.split("_")
        # "DeathKnight_Blood" → ["DeathKnight", "Blood"]
        for p in spec_parts:
            # Strip the literal spec word AND its expansion ("DeathKnight" → "Death Knight")
            expanded = p.replace("DeathKnight", "Death Knight") \
                        .replace("DemonHunter", "Demon Hunter")
            if expanded.lower() in nm.lower() and len(nm.split()) <= 4:
                # Only suppress short class-identity tags, not real
                # abilities that happen to mention the spec name.
                return True
        return False

    for spec_file, spec_id in SPECS.items():
        rotation_ids = parse_spec_constants(spec_file)
        if rotation_ids is None:
            print(f"[spec-audit] WARN: Apl_{spec_file}.cpp not found")
            continue
        core_spells = spec_map.get(spec_id, [])
        relevant: list[tuple[int, str, int]] = []
        for sid, _override in core_spells:
            nm = names.get(sid, f"<unknown:{sid}>")
            if is_noise(nm, spec_file): continue
            lvl = levels.get(sid, 0)
            relevant.append((sid, nm, lvl))

        covered = sum(1 for sid, _nm, _lvl in relevant
                      if sid in rotation_ids)
        missing = [(sid, nm, lvl) for sid, nm, lvl in relevant
                   if sid not in rotation_ids]
        summary.append((spec_file, len(relevant), covered, len(missing)))

        detail_lines.append(
            f"\n=== {spec_file} (spec={spec_id}): "
            f"{len(relevant)} spec spells, covers {covered}, "
            f"missing {len(missing)} ==="
        )
        if not missing:
            detail_lines.append("  (complete — no gaps)")
            continue
        for sid, nm, lvl in sorted(missing, key=lambda r: (r[2], r[0])):
            detail_lines.append(f"  L{lvl:>2}  {sid:>7}  {nm}")

    # Summary table first.
    print("\n=== SUMMARY ===")
    print(f"{'Spec':<28} {'Total':>6} {'Cover':>6} {'Miss':>6} {'%':>4}")
    for name, tot, cov, miss in sorted(summary, key=lambda r: r[3], reverse=True):
        pct = (cov * 100 // tot) if tot else 100
        print(f"{name:<28} {tot:>6} {cov:>6} {miss:>6} {pct:>3}%")
    print()
    # Detail per spec.
    for line in detail_lines:
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
