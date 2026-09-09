#!/usr/bin/env python3
"""Fetch method.gg talent guide pages and extract Blizzard talent import strings.

For every (class, spec) the guide page https://www.method.gg/guides/<spec>-<class>/talents
embeds each recommended build as <div class="talent-embed" data-talent="C..."> under an
<h2>/<h3> heading ("Raid ... (Recommended)", "Mythic+ ...", hero-tree variants, ...).

Output JSON (consumed by gen_talent_seed.py --method):
  { "<spec_id>": { "class_id": N, "slug": "...", "builds": [ {"heading": "...", "talents": "C..."} ] } }

Usage: fetch_method_builds.py <out.json> [--cache-dir DIR]
"""
import argparse, html, json, re, sys, urllib.request
from pathlib import Path

# (slug, class_id, spec_id)
SPECS = [
    ("arms-warrior", 1, 71), ("fury-warrior", 1, 72), ("protection-warrior", 1, 73),
    ("holy-paladin", 2, 65), ("protection-paladin", 2, 66), ("retribution-paladin", 2, 70),
    ("beast-mastery-hunter", 3, 253), ("marksmanship-hunter", 3, 254), ("survival-hunter", 3, 255),
    ("assassination-rogue", 4, 259), ("outlaw-rogue", 4, 260), ("subtlety-rogue", 4, 261),
    ("discipline-priest", 5, 256), ("holy-priest", 5, 257), ("shadow-priest", 5, 258),
    ("blood-death-knight", 6, 250), ("frost-death-knight", 6, 251), ("unholy-death-knight", 6, 252),
    ("elemental-shaman", 7, 262), ("enhancement-shaman", 7, 263), ("restoration-shaman", 7, 264),
    ("arcane-mage", 8, 62), ("fire-mage", 8, 63), ("frost-mage", 8, 64),
    ("affliction-warlock", 9, 265), ("demonology-warlock", 9, 266), ("destruction-warlock", 9, 267),
    ("brewmaster-monk", 10, 268), ("windwalker-monk", 10, 269), ("mistweaver-monk", 10, 270),
    ("balance-druid", 11, 102), ("feral-druid", 11, 103), ("guardian-druid", 11, 104), ("restoration-druid", 11, 105),
    ("havoc-demon-hunter", 12, 577), ("vengeance-demon-hunter", 12, 581), ("devourer-demon-hunter", 12, 1480),
    ("devastation-evoker", 13, 1467), ("preservation-evoker", 13, 1468), ("augmentation-evoker", 13, 1473),
]
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/128"


def fetch(url, cache: Path | None):
    if cache and cache.exists():
        return cache.read_text(encoding="utf-8", errors="replace")
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as r:
        text = r.read().decode("utf-8", errors="replace")
    if cache:
        cache.write_text(text, encoding="utf-8")
    return text


def extract(text):
    heads = [(m.start(), html.unescape(re.sub(r"<[^>]+>", "", m.group(2))).strip())
             for m in re.finditer(r"<(h[1-4])[^>]*>(.*?)</\1>", text, re.S)]
    builds = []
    seen = set()
    for m in re.finditer(r'data-talent="(C[A-Za-z0-9+/]+)"', text):
        s = m.group(1)
        prev = [h for h in heads if h[0] < m.start()]
        heading = prev[-1][1] if prev else "?"
        key = (heading, s)
        if key in seen: continue
        seen.add(key)
        builds.append({"heading": heading, "talents": s})
    versions = sorted(set(re.findall(r"\b12\.\d(?:\.\d)?\b", text)))
    return builds, versions


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--cache-dir", default=None)
    args = ap.parse_args()
    cache_dir = Path(args.cache_dir) if args.cache_dir else None
    if cache_dir: cache_dir.mkdir(parents=True, exist_ok=True)
    out = {}
    for slug, cid, sid in SPECS:
        url = f"https://www.method.gg/guides/{slug}/talents"
        try:
            text = fetch(url, cache_dir / f"{slug}.html" if cache_dir else None)
        except Exception as e:  # noqa: BLE001
            print(f"{slug}: FAILED {e}", file=sys.stderr); continue
        builds, versions = extract(text)
        out[str(sid)] = {"class_id": cid, "slug": slug, "versions": versions, "builds": builds}
        print(f"{slug}: {len(builds)} builds, page versions {versions}", file=sys.stderr)
        for b in builds:
            print(f"    {b['heading'][:70]}", file=sys.stderr)
    Path(args.out).write_text(json.dumps(out, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
