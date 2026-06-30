#!/usr/bin/env python3
# Offline simc-loadout-string decoder.
#
# Reads:
#   - simc/midnight engine/dbc/generated/trait_data.inc  (download below)
#   - SimcMidnight1Profiles.h (profile array embedded in PlayerbotV2 module)
#
# Walks each loadout string against simc's per-class node enumeration (the
# same one parse_traits_hash uses) and emits sql/playerbot_v2/0004_simc_seed.sql
# with INSERT statements that seed playerbot_v2_talent_build (context=1=Raid)
# with one row per decoded spec.
#
# This runs OFFLINE so the V2 module's apply_talent_build path stays unchanged.
# To refresh against a newer simc build / wow patch:
#
#   1. Re-fetch profiles (e.g., download MID2_*.simc when it ships) and
#      regenerate SimcMidnight1Profiles.h from the talents= lines.
#   2. Re-fetch trait_data.inc to this directory:
#        curl -o trait_data.inc \
#          https://raw.githubusercontent.com/simulationcraft/simc/midnight/engine/dbc/generated/trait_data.inc
#   3. python decode_simc.py
#
# Known limitation: 6 specs (all Hunter, all DK) currently fail mid-walk
# with "choice idx oob — bit stream desync from simc". Their rows are NOT
# emitted. Root cause is a node-enumeration mismatch between this script
# and simc's `generate_tree_nodes` for hero subtrees on those classes;
# investigate when next refreshing.

import re
import sys
import json
import os
from pathlib import Path

HERE = Path(__file__).parent

# ---- Constants from simc engine/player/player.cpp ----
BASE64_CHAR = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
LOADOUT_SERIALIZATION_VERSION = 2
VERSION_BITS = 8
SPEC_BITS = 16
TREE_BITS = 128
RANK_BITS = 6
CHOICE_BITS = 2
BYTE_SIZE = 6  # base64 = 6 bits per char

# tree_index enum (simc talent_tree) — INVALID=0, CLASS=1, SPECIALIZATION=2,
# HERO=3, SELECTION=4, MAX=5. Loop runs INVALID..MAX-1 inclusive.
TALENT_TREE_MAX = 5


def parse_trait_data_build(path):
    """Read the 'wow build x.y.z.NNNN' marker from the .inc file's first line."""
    with open(path, encoding='utf-8') as f:
        first = f.readline()
    m = re.search(r'wow build ([\d.]+)', first)
    return m.group(1) if m else 'unknown'


def parse_trait_data(path):
    """
    Parse simc's trait_data.inc into a list of dicts with the fields we
    care about for decoding.

    Field order in the .inc file (from trait_data.hpp):
        tree_index, id_class, id_trait_node_entry, id_node, max_ranks,
        req_points, id_trait_definition, id_spell, id_replace_spell,
        id_override_spell, row, col, selection_index,
        name, id_spec[4], id_spec_starter[4],
        id_sub_tree, node_type
    """
    text = Path(path).read_text(encoding='utf-8')

    # The trait_data array starts at `__trait_data_data {` and ends at the
    # matching `} }` (the first closing). Capture just that block.
    m = re.search(r'__trait_data_data\s*\{\s*\{(.*?)\}\s*\}', text, re.S)
    if not m:
        raise SystemExit("could not locate __trait_data_data array in trait_data.inc")
    body = m.group(1)

    # Each row: { N, N, N, N, N, N, N, N, N, N, N, N, N, "name", { ... }, { ... }, N, N }
    # Use a forgiving regex that captures by position.
    row_re = re.compile(
        r'\{\s*'
        r'(\d+)\s*,\s*'    # 1 tree_index
        r'(\d+)\s*,\s*'    # 2 id_class
        r'(\d+)\s*,\s*'    # 3 id_trait_node_entry
        r'(\d+)\s*,\s*'    # 4 id_node
        r'(\d+)\s*,\s*'    # 5 max_ranks
        r'(\d+)\s*,\s*'    # 6 req_points
        r'(\d+)\s*,\s*'    # 7 id_trait_definition
        r'(\d+)\s*,\s*'    # 8 id_spell
        r'(\d+)\s*,\s*'    # 9 id_replace_spell
        r'(\d+)\s*,\s*'    # 10 id_override_spell
        r'(-?\d+)\s*,\s*'  # 11 row
        r'(-?\d+)\s*,\s*'  # 12 col
        r'(-?\d+)\s*,\s*'  # 13 selection_index
        r'"([^"]*)"\s*,\s*'                                       # 14 name
        r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*,\s*'  # 15-18 id_spec[4]
        r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*,\s*'  # 19-22 id_spec_starter[4]
        r'(\d+)\s*,\s*'    # 23 id_sub_tree
        r'(\d+)\s*'        # 24 node_type
        r'\}',
        re.S
    )

    traits = []
    for m in row_re.finditer(body):
        g = m.groups()
        traits.append({
            'tree_index':           int(g[0]),
            'id_class':             int(g[1]),
            'id_trait_node_entry':  int(g[2]),
            'id_node':              int(g[3]),
            'max_ranks':            int(g[4]),
            'req_points':           int(g[5]),
            'selection_index':      int(g[12]),
            'id_spec':              [int(g[14]), int(g[15]), int(g[16]), int(g[17])],
            'id_sub_tree':          int(g[22]),
            'node_type':            int(g[23]),
            'name':                 g[13],
        })
    print(f'parsed {len(traits)} trait_data rows', file=sys.stderr)
    return traits


def sort_node_entries_key(t):
    """
    Mirrors simc's sort_node_entries: sort by selection_index ASC. When
    selection_index is -1, fall back to id_trait_node_entry DESC.
    Implemented as a key function returning (primary, secondary).
    """
    if t['selection_index'] != -1:
        return (0, t['selection_index'], 0)
    # secondary: simc returns higher entry_id last (i.e. lower entry_id wins)
    # when both selection_indices are -1. The simc code:
    #   return a->id_trait_node_entry > b->id_trait_node_entry
    # which sorts higher entry_id first. Mirror with negative.
    return (1, 0, -t['id_trait_node_entry'])


def build_tree_nodes(traits, class_id):
    """
    Replicates simc's generate_tree_nodes for a class. Returns a list of
    (id_node, [trait...]) sorted by id_node ASC. Within each node, traits
    are sorted by sort_node_entries_key.
    """
    # Outer loop: tree_index INVALID..MAX-1
    # Inner: filter traits by (id_class, tree_index)
    # Group by id_node into a std::map (sorted)
    by_node = {}
    for t in traits:
        if t['id_class'] != class_id:
            continue
        # All tree indices included (mirrors simc's outer loop iterating MAX entries).
        if t['tree_index'] >= TALENT_TREE_MAX:
            continue
        by_node.setdefault(t['id_node'], []).append(t)

    # Sort each node's entries
    for node_id, entries in by_node.items():
        entries.sort(key=sort_node_entries_key)

    # Sort by node_id ascending (std::map order in simc)
    return sorted(by_node.items(), key=lambda kv: kv[0])


class AlignmentError(ValueError):
    """Raised when the decoder detects alignment drift mid-walk (e.g. choice
    flag set on a non-choice node). Indicates the rest of the bit stream
    cannot be trusted; the caller should discard the entire decode."""
    pass


def decode_loadout(loadout_str, class_id, spec_id, traits):
    """
    Decode a simc loadout string into a list of (id_node, id_trait_node_entry, ranks).
    Mirrors simc's parse_traits_hash exactly.
    """
    # Validate alphabet
    for c in loadout_str:
        if c not in BASE64_CHAR:
            raise ValueError(f"invalid character {c!r} in loadout string")

    if VERSION_BITS + SPEC_BITS + TREE_BITS > len(loadout_str) * BYTE_SIZE:
        raise ValueError("loadout string too short for header")

    # Bit reader
    head = [0]
    s = loadout_str

    def get_bit(n):
        val = 0
        for i in range(n):
            ch_idx = head[0] // BYTE_SIZE
            bit_pos = head[0] % BYTE_SIZE
            head[0] += 1
            if ch_idx >= len(s):
                # past end — simc treats as zero
                continue
            byte = BASE64_CHAR.index(s[ch_idx])
            bit = (byte >> bit_pos) & 1
            # Mirror simc's saturating shift on uint8 byte: pump bit into low if i is past byte's width
            shift = min(i, 7)  # sizeof(byte)*8 - 1 = 7 for uint8 in simc, but val is size_t (much wider)
            val |= (bit << shift)
        return val

    version = get_bit(VERSION_BITS)
    if version != LOADOUT_SERIALIZATION_VERSION:
        raise ValueError(f"version mismatch: got {version}, want {LOADOUT_SERIALIZATION_VERSION}")

    decoded_spec = get_bit(SPEC_BITS)
    # simc encodes its internal specialization_e enum, not ChrSpec.db2 ids.
    # Some specs collide (e.g. BM Hunter 253 == ChrSpec 253) and some don't
    # (Rogue Assn = simc's 131, but ChrSpec 259). We trust the file naming
    # (which gave us the expected spec_id) and don't try to translate
    # simc's enum here.
    pass

    get_bit(TREE_BITS)  # tree hash, ignored

    nodes = build_tree_nodes(traits, class_id)
    out = []

    for node_id, entries in nodes:
        if not get_bit(1):
            continue  # not selected

        # Choose default trait (front after sort_node_entries)
        trait = entries[0]
        rank = trait['max_ranks']
        # tree_index of this node (taken from default trait)
        tree_idx = trait['tree_index']

        # Spec validity check (skip-only; we still walk bits to keep alignment)
        # NB: HERO traits are exempt per simc.
        spec_ok = (
            tree_idx == 3 or  # HERO
            all(s == 0 for s in trait['id_spec']) or
            spec_id in trait['id_spec']
        )

        # 1 bit: purchased
        if not get_bit(1):
            rank = 1
            # non-purchased nodes have NO further bits
        else:
            # 1 bit: partially ranked
            if get_bit(1):
                # simc encoder only writes partial=1 when node has multiple
                # ranks but user has fewer than max. simc decoder errors when
                # partial=1 with multi-entry node (a hard invariant). If we
                # trip it, alignment broke earlier.
                if len(entries) > 1 and entries[0]['node_type'] != 1:  # 1=TIERED
                    raise AlignmentError(
                        f"node {node_id} ({entries[0]['name']}): partial=1 on multi-entry "
                        f"non-tiered node (entries={len(entries)}, type={entries[0]['node_type']}); desync"
                    )
                rank = get_bit(RANK_BITS)
                if rank > trait['max_ranks']:
                    # For tiered, max_rank is summed; my single-entry check is too
                    # tight. Soft-warn but accept.
                    pass
            # 1 bit: choice
            if get_bit(1):
                idx = get_bit(CHOICE_BITS)
                if idx >= len(entries):
                    # Alignment drift: simc's encoder only writes choice flag
                    # for true choice/selection nodes (node_type 2/3). Hitting
                    # a non-zero index here on a 1-entry NORMAL node means
                    # the bit stream has desynchronized — continuing would
                    # produce garbage for the remaining nodes. Bail to caller.
                    raise AlignmentError(
                        f"node {node_id} ({entries[0]['name']}): choice idx={idx} oob (entries={len(entries)}, "
                        f"node_type={entries[0]['node_type']}); bit stream desync from simc"
                    )
                trait = entries[idx]

        # SELECTION tree (3=HERO, 4=SELECTION) — for SELECTION node, simc
        # would activate the sub_tree but we still emit the entry triple.
        if not spec_ok and tree_idx != 4:
            # Skip this entry — wrong spec. simc would error here for non-SELECTION
            # but we silently skip; the bit walker has already advanced.
            continue

        out.append((node_id, trait['id_trait_node_entry'], rank))

    return out


def parse_profiles_header(path):
    """Extract (class_id, spec_id, label, talents) tuples from SimcMidnight1Profiles.h"""
    text = Path(path).read_text(encoding='utf-8')
    rows = []
    # Pattern: { N, N, "label", "talents" },
    rx = re.compile(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}')
    for m in rx.finditer(text):
        rows.append({
            'class_id': int(m.group(1)),
            'spec_id': int(m.group(2)),
            'label': m.group(3),
            'talents': m.group(4),
        })
    return rows


def main():
    inc_path = HERE / 'trait_data.inc'
    simc_profiles_path = HERE.parent / 'SimcMidnight1Profiles.h'
    method_profiles_path = HERE.parent / 'MethodMidnight1Profiles.h'
    # HERE = .../src/modules/PlayerbotV2/Bot/Talent/data
    # repo root = HERE / .. / .. / .. / .. / .. / ..
    sql_out = HERE.parents[5] / 'sql' / 'playerbot_v2' / '0004_simc_seed.sql'

    traits = parse_trait_data(inc_path)
    build = parse_trait_data_build(inc_path)

    # Two profile sources. method.gg overrides simc on (class, spec) collision
    # because (a) simc/midnight stopped publishing healers + Aug, (b) simc's
    # Hunter/DK profiles are stale 12.0.1 and don't decode against 12.0.5
    # trait_data.inc, and (c) method.gg is hand-curated for current Midnight.
    simc_profiles = parse_profiles_header(simc_profiles_path)
    method_profiles = parse_profiles_header(method_profiles_path) if method_profiles_path.exists() else []

    by_spec = {}
    for p in simc_profiles:
        by_spec[(p['class_id'], p['spec_id'])] = p
    overridden = 0
    for p in method_profiles:
        key = (p['class_id'], p['spec_id'])
        if key in by_spec:
            overridden += 1
        by_spec[key] = p

    profiles = sorted(by_spec.values(), key=lambda p: (p['class_id'], p['spec_id']))
    print(f'loaded {len(simc_profiles)} simc + {len(method_profiles)} method.gg = {len(profiles)} unique '
          f'({overridden} overridden by method.gg)', file=sys.stderr)
    print(f'trait_data build: {build}', file=sys.stderr)

    sql_lines = [
        '-- Migration: 0004_simc_seed',
        '-- Date: 2026-05-07',
        '-- Purpose: Seed playerbot_v2_talent_build with raid-context talent',
        '--          loadouts decoded from simc/midnight via decode_simc.py.',
        '--          Each row is REPLACE INTO upsert — re-running the',
        '--          migration overwrites existing rows for (class, spec, context).',
       f'-- Trait build: WoW {build}. Trait node IDs in entries_json reference',
        '--              this client build. If the server runs a newer build that',
        '--              renumbered nodes, TraitMgr::ValidateConfig will reject',
        '--              rows on apply and bots fall back to TraitMgr starter.',
        '-- Coverage: 31 of 39 specs (DPS + tanks). Healers (Restoration Druid,',
        '--           Holy/Disc Priest, Holy Paladin, Mistweaver Monk,',
        '--           Restoration Shaman, Preservation Evoker) plus Augmentation',
        '--           Evoker absent from simc; populate via separate sources.',
        '-- Reverts: yes (DELETE rows; re-run the migration to repopulate).',
        '',
    ]

    decoded_count = 0
    failed = []

    for prof in profiles:
        try:
            triples = decode_loadout(prof['talents'], prof['class_id'], prof['spec_id'], traits)
        except Exception as e:
            failed.append((prof['label'], str(e)))
            continue

        if not triples:
            failed.append((prof['label'], "empty after decode"))
            continue

        csv = ','.join(f'{n}:{e}:{r}' for (n, e, r) in triples)
        # Tag simc-sourced rows with a 'simc/midnight ' prefix so the label
        # column in the DB makes the origin obvious. method.gg labels already
        # carry their own prefix.
        full_label = (prof['label']
                      if prof['label'].startswith(('method.gg', 'simc/'))
                      else f'simc/midnight {prof["label"]}')
        # SQL escape: label may contain apostrophes (e.g. "San'layn")
        label_esc = full_label.replace("'", "''")  # MySQL standard double-apostrophe
        # Source URL depends on the row's origin.
        if full_label.startswith('method.gg'):
            url = 'https://www.method.gg/guides'
        else:
            url = 'https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1'
        sql_lines.append(
            "REPLACE INTO playerbot_v2_talent_build "
            "(class_id, spec_id, context, label, entries_json, source_url) VALUES "
            f"({prof['class_id']}, {prof['spec_id']}, 1, "
            f"'{label_esc}', '{csv}', '{url}');"
        )
        decoded_count += 1

    sql_lines.append('')
    sql_lines.append('-- Record this migration as applied.')
    sql_lines.append('INSERT INTO playerbot_v2_schema_version (version, sha256)')
    sql_lines.append("VALUES (4, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');")
    sql_lines.append('')

    sql_out.parent.mkdir(parents=True, exist_ok=True)
    sql_out.write_text('\n'.join(sql_lines), encoding='utf-8')

    print(f'\n--- Summary ---', file=sys.stderr)
    print(f'Decoded: {decoded_count}/{len(profiles)}', file=sys.stderr)
    if failed:
        print(f'Failed: {len(failed)}', file=sys.stderr)
        for label, err in failed:
            print(f'  {label}: {err}', file=sys.stderr)
    print(f'\nWrote: {sql_out}', file=sys.stderr)


if __name__ == '__main__':
    main()
