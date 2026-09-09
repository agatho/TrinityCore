#!/usr/bin/env python3
"""Generate the curated talent-build seed for the current WoW build.

Reads:
  - trait_data.inc         simc engine/dbc/generated/trait_data.inc for the target build
                           (https://raw.githubusercontent.com/simulationcraft/simc/midnight/engine/dbc/generated/trait_data.inc)
  - <profiles_dir>/*.simc  simc raid profiles (profiles/MID2/*.simc); the `talents=` line is a
                           Blizzard talent import string
  - ExtraProfiles.h        optional hand-curated import strings for specs simc does not publish
                           (healers + Augmentation); same array layout as SimcMidnight1Profiles.h

Writes:
  - sql/playerbot_v2/<migration>.sql   REPLACE INTO playerbot_v2_talent_build rows for
                                       context 0 (Default) and 1 (Raid)
  - <out_json>                         {spec_id: {"class_id", "label", "entries": [[node, entry, rank]],
                                        "spells": [[spell_id, name, tree_index, sub_tree]]}}

Usage:
  gen_talent_seed.py --inc trait_data.inc --profiles simc_mid2/ [--extra ExtraProfiles.h]
                     --migration 0016_talent_builds_12_1 --json talent_selection.json
"""
import argparse, json, re, sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
from decode_simc import parse_trait_data, parse_trait_data_build, decode_loadout, parse_profiles_header  # noqa: E402

# simc spec token -> (class_id, spec_id)
SIMC_SPEC = {
    ("warrior", "arms"): (1, 71), ("warrior", "fury"): (1, 72), ("warrior", "protection"): (1, 73),
    ("paladin", "holy"): (2, 65), ("paladin", "protection"): (2, 66), ("paladin", "retribution"): (2, 70),
    ("hunter", "beast_mastery"): (3, 253), ("hunter", "marksmanship"): (3, 254), ("hunter", "survival"): (3, 255),
    ("rogue", "assassination"): (4, 259), ("rogue", "outlaw"): (4, 260), ("rogue", "subtlety"): (4, 261),
    ("priest", "discipline"): (5, 256), ("priest", "holy"): (5, 257), ("priest", "shadow"): (5, 258),
    ("deathknight", "blood"): (6, 250), ("deathknight", "frost"): (6, 251), ("deathknight", "unholy"): (6, 252),
    ("shaman", "elemental"): (7, 262), ("shaman", "enhancement"): (7, 263), ("shaman", "restoration"): (7, 264),
    ("mage", "arcane"): (8, 62), ("mage", "fire"): (8, 63), ("mage", "frost"): (8, 64),
    ("warlock", "affliction"): (9, 265), ("warlock", "demonology"): (9, 266), ("warlock", "destruction"): (9, 267),
    ("monk", "brewmaster"): (10, 268), ("monk", "windwalker"): (10, 269), ("monk", "mistweaver"): (10, 270),
    ("druid", "balance"): (11, 102), ("druid", "feral"): (11, 103), ("druid", "guardian"): (11, 104), ("druid", "restoration"): (11, 105),
    ("demonhunter", "havoc"): (12, 577), ("demonhunter", "vengeance"): (12, 581), ("demonhunter", "devourer"): (12, 1480),
    ("evoker", "devastation"): (13, 1467), ("evoker", "preservation"): (13, 1468), ("evoker", "augmentation"): (13, 1473),
}
CLASS_TOKENS = ["warrior", "paladin", "hunter", "rogue", "priest", "deathknight", "shaman", "mage",
                "warlock", "monk", "druid", "demonhunter", "evoker"]


def parse_simc_profile(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    cls = spec = talents = None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"): continue
        m = re.match(r'^(\w+)="([^"]+)"$', line)
        if m and m.group(1) in CLASS_TOKENS and cls is None:
            cls = m.group(1); label = m.group(2); continue
        m = re.match(r'^spec=(\w+)$', line)
        if m: spec = m.group(1); continue
        m = re.match(r'^talents=([A-Za-z0-9+/]+)$', line)
        if m and talents is None: talents = m.group(1)
    if not (cls and spec and talents): return None
    key = (cls, spec)
    if key not in SIMC_SPEC:
        print(f"  ? unknown simc class/spec {key} in {path.name}", file=sys.stderr); return None
    cid, sid = SIMC_SPEC[key]
    return {"class_id": cid, "spec_id": sid, "label": f"simc/midnight {label}", "talents": talents,
            "_cls_tok": cls, "_spec_tok": spec,
            "source_url": "https://github.com/simulationcraft/simc/tree/midnight/profiles/" + path.parent.name}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inc", required=True)
    ap.add_argument("--profiles", required=True, help="directory of simc .simc profiles")
    ap.add_argument("--extra", default=None, help="header with hand-curated {class, spec, label, talents} rows")
    ap.add_argument("--method", default=None, help="JSON from fetch_method_builds.py: raid builds fill specs simc lacks, Mythic+ builds seed context 2")
    ap.add_argument("--migration", required=True, help="migration basename, e.g. 0016_talent_builds_12_1")
    ap.add_argument("--version", type=int, required=True, help="schema version number for playerbot_v2_schema_version")
    ap.add_argument("--json", required=True)
    ap.add_argument("--prefer", default="base", choices=["base", "hero"],
                    help="when simc ships several profiles per spec, prefer the base file (no hero suffix) or the hero variant")
    args = ap.parse_args()

    traits = parse_trait_data(args.inc)
    build = parse_trait_data_build(args.inc)
    name_by_entry = {t["id_trait_node_entry"]: t for t in traits}

    # Profiles: simc first, then extra (extra overrides simc on collision).
    by_spec = {}
    files = sorted(Path(args.profiles).glob("*.simc"))
    for f in files:
        p = parse_simc_profile(f)
        if not p: continue
        key = (p["class_id"], p["spec_id"])
        # Base profile is "<TIER>_<Class>_<Spec>"; any longer stem is a hero-tree variant.
        norm = re.sub(r"[_\-]", "", f.stem).lower()
        base_norm = (f.stem.split("_")[0] + p["_cls_tok"] + p["_spec_tok"]).replace("_", "").lower()
        is_hero_variant = norm != base_norm
        if key in by_spec:
            cur_hero = by_spec[key]["_hero"]
            if (args.prefer == "base" and cur_hero and not is_hero_variant) or (args.prefer == "hero" and not cur_hero and is_hero_variant):
                by_spec[key] = dict(p, _hero=is_hero_variant)
        else:
            by_spec[key] = dict(p, _hero=is_hero_variant)
    if args.extra and Path(args.extra).exists():
        for p in parse_profiles_header(args.extra):
            by_spec[(p["class_id"], p["spec_id"])] = dict(p, source_url="https://www.method.gg/guides", _hero=False)

    # method.gg: raid build for specs simc does not cover (ctx 0+1); Mythic+ build for every spec (ctx 2).
    mplus = {}
    if args.method and Path(args.method).exists():
        method = json.loads(Path(args.method).read_text(encoding="utf-8"))
        def pick(builds, want_mplus):
            def score(b):
                h = b["heading"].lower(); sc = 0
                is_m = ("mythic+" in h or "m+" in h or "aoe" in h)
                if want_mplus != is_m: return -1
                if any(k in h for k in ("recommended", "best", "preferred", "standard")): sc += 4
                if not want_mplus and "raid" in h: sc += 2
                if not want_mplus and "single target" in h: sc += 1
                return sc
            ranked = sorted(((score(b), -i, b) for i, b in enumerate(builds)), key=lambda x: (x[0], x[1]), reverse=True)
            return ranked[0][2] if ranked and ranked[0][0] >= 0 else None
        for sid_s, v in method.items():
            sid = int(sid_s); cid = v["class_id"]
            raid = pick(v["builds"], False); mp = pick(v["builds"], True)
            if (cid, sid) not in by_spec and raid:
                by_spec[(cid, sid)] = {"class_id": cid, "spec_id": sid, "label": f"method.gg {raid['heading']}",
                                       "talents": raid["talents"], "source_url": f"https://www.method.gg/guides/{v['slug']}/talents", "_hero": False}
            if mp:
                mplus[(cid, sid)] = {"class_id": cid, "spec_id": sid, "label": f"method.gg {mp['heading']}",
                                     "talents": mp["talents"], "source_url": f"https://www.method.gg/guides/{v['slug']}/talents"}
    profiles = sorted(by_spec.values(), key=lambda p: (p["class_id"], p["spec_id"]))
    print(f"trait_data build {build}; {len(profiles)} profiles", file=sys.stderr)

    sql = [
        f"-- Migration: {args.migration}",
        f"-- Purpose: Curated talent builds for WoW {build} (Midnight 12.1), decoded from",
        "--          simc/midnight MID2 raid profiles (+ hand-curated healer/Augmentation",
        "--          import strings) via Bot/Talent/data/gen_talent_seed.py.",
        "--          Rows are REPLACE INTO for context 0 (Default) and 1 (Raid) so re-running",
        "--          overwrites the previous build's rows for (class, spec, context).",
        f"-- Trait build: {build}. entries_json = nodeID:entryID:ranks triples referencing this",
        "--              client build's TraitNode/TraitNodeEntry ids.",
        "-- Reverts: yes (DELETE rows; re-run to repopulate).",
        "",
    ]
    out_json = {}
    ok = 0; failed = []
    for prof in profiles:
        try:
            triples = decode_loadout(prof["talents"], prof["class_id"], prof["spec_id"], traits)
        except Exception as e:  # noqa: BLE001
            failed.append((prof["label"], str(e))); continue
        if not triples:
            failed.append((prof["label"], "empty after decode")); continue
        csv = ",".join(f"{n}:{e}:{r}" for (n, e, r) in triples)
        label_esc = prof["label"].replace("'", "''")
        for ctx in (0, 1):
            sql.append("REPLACE INTO playerbot_v2_talent_build (class_id, spec_id, context, label, entries_json, source_url) VALUES "
                       f"({prof['class_id']}, {prof['spec_id']}, {ctx}, '{('Default: ' if ctx == 0 else '') + label_esc}', '{csv}', '{prof['source_url']}');")
        spells = []
        for (n, e, r) in triples:
            t = name_by_entry.get(e)
            if t: spells.append([t["id_spell"], t["name"], t["tree_index"], t["id_sub_tree"], r])
        out_json[str(prof["spec_id"])] = {"class_id": prof["class_id"], "label": prof["label"],
                                          "entries": [list(x) for x in triples], "spells": spells}
        ok += 1
    for prof in sorted(mplus.values(), key=lambda p: (p["class_id"], p["spec_id"])):
        try:
            triples = decode_loadout(prof["talents"], prof["class_id"], prof["spec_id"], traits)
        except Exception as e:  # noqa: BLE001
            failed.append((prof["label"] + " [M+]", str(e))); continue
        if not triples:
            failed.append((prof["label"] + " [M+]", "empty after decode")); continue
        csv = ",".join(f"{n}:{e}:{r}" for (n, e, r) in triples)
        label_esc = prof["label"].replace("'", "''")
        sql.append("REPLACE INTO playerbot_v2_talent_build (class_id, spec_id, context, label, entries_json, source_url) VALUES "
                   f"({prof['class_id']}, {prof['spec_id']}, 2, 'M+: {label_esc}', '{csv}', '{prof['source_url']}');")
        spells = []
        for (n, e, r) in triples:
            t = name_by_entry.get(e)
            if t: spells.append([t["id_spell"], t["name"], t["tree_index"], t["id_sub_tree"], r])
        out_json.setdefault(str(prof["spec_id"]), {"class_id": prof["class_id"]})["mplus"] = {"label": prof["label"], "entries": [list(x) for x in triples], "spells": spells}
        ok += 1
    sql += ["", "-- Record this migration as applied.",
            "INSERT INTO playerbot_v2_schema_version (version, sha256)",
            f"VALUES ({args.version}, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');", ""]
    sql_path = HERE.parents[5] / "sql" / "playerbot_v2" / f"{args.migration}.sql"
    sql_path.write_text("\n".join(sql), encoding="utf-8")
    Path(args.json).write_text(json.dumps(out_json, indent=1), encoding="utf-8")
    print(f"decoded {ok}/{len(profiles)} -> {sql_path}", file=sys.stderr)
    for label, err in failed:
        print(f"  FAILED {label}: {err}", file=sys.stderr)
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
