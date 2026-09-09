# Curated talent builds — refresh procedure

Bots apply curated talent loadouts from the `playerbot_v2_talent_build` table
(`nodeID:entryID:ranks` triples, one row per `(class, spec, context)`; see
`sql/playerbot_v2/0003_talent_builds.sql`). The rows are generated offline from
public raid/M+ builds and checked in as a migration. Regenerate them whenever the
client build changes talent trees (every major patch, e.g. 12.0 -> 12.1).

## Inputs

1. `trait_data.inc` — simc's generated trait table for the exact client build:
   `https://raw.githubusercontent.com/simulationcraft/simc/<branch>/engine/dbc/generated/trait_data.inc`
   (first line carries `wow build x.y.z.NNNNN`; it must match the server's client build).
2. simc raid profiles — `profiles/<TIER>/*.simc` from the same branch (e.g. `MID2`).
   The `talents=` line is a Blizzard talent import string. simc covers DPS + tanks only.
3. method.gg guide builds — `fetch_method_builds.py` scrapes
   `https://www.method.gg/guides/<spec>-<class>/talents` for every spec and records each
   embedded build with its section heading. These fill the specs simc does not publish
   (healers, Druid, Evoker) and provide the Mythic+ builds (context 2) for every spec.

## Steps

```
# 1. fetch inputs (scratch directory)
curl -o trait_data.inc https://raw.githubusercontent.com/simulationcraft/simc/midnight/engine/dbc/generated/trait_data.inc
mkdir simc_mid2 && cd simc_mid2 && <download profiles/MID2/*.simc> && cd ..
python fetch_method_builds.py method_builds.json --cache-dir method_html

# 2. decode + emit the migration (pick the next free migration number)
python gen_talent_seed.py --inc trait_data.inc --profiles simc_mid2 --method method_builds.json \
    --migration 0016_talent_builds_12_1 --version 16 --json talent_selection.json
```

`gen_talent_seed.py` writes `sql/playerbot_v2/<migration>.sql` with `REPLACE INTO`
rows for context 0 (Default) + 1 (Raid) for all specs and context 2 (Mythic+) where a
build exists, and a JSON side file listing the selected talent spells per spec (used
by the APL revision tooling to know which talents a bot actually owns).

Selection rules: simc base profile (no hero-tree suffix) is preferred for raid/default;
method.gg "Raid ... (Recommended|Best|Preferred|Standard)" fills specs without a simc
profile; method.gg "Mythic+/AoE ..." headings seed context 2.

## Files

- `decode_simc.py` — import-string decoder (mirror of simc `parse_traits_hash`).
- `gen_talent_seed.py` — driver: profiles + method.gg JSON -> migration SQL + selection JSON.
- `fetch_method_builds.py` — method.gg scraper -> builds JSON.

A decode failure (`AlignmentError`) means the import string was made for a different
tree layout than `trait_data.inc` — re-fetch both for the same build.
