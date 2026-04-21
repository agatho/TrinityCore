#!/usr/bin/env python3
"""Re-run the strict parser from script 35 on BOTH our map-entry packets
(idx 295 and idx 310) and emit a combined inventory. The earlier analysis
missed that our server splits the initial entity bundle across two
UPDATE_OBJECTs; retail packs everything into one."""
import importlib.util
import sys
from pathlib import Path

spec = importlib.util.spec_from_file_location('m35', r'c:/TrinityBots/wt/housing-system/sniff_analysis_login_plot/35_inventory_mapentry.py')
m35 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m35)


def run_on(label, pkt_path, idx):
    rows = m35.run_one(label, pkt_path, idx)
    return rows


if __name__ == '__main__':
    retail = run_on('RETAIL_idx9984', m35.RETAIL_PKT, 9984)
    our_295 = run_on('OUR_idx295',  m35.OUR_PKT, 295)
    our_310 = run_on('OUR_idx310',  m35.OUR_PKT, 310)

    # Combined our counts
    from collections import Counter
    def role_count(rows):
        c = Counter()
        for r in rows:
            if r.get('kind') != 'CREATE':
                continue
            h = r.get('highGuid', '?')
            ot = r.get('objectType', '?')
            c[(h, ot)] += 1
        return c

    # run_one returns a list of dicts (or similar); role_count may not work directly.
    # Just print the totals.
    print('\n\n=== COMBINED SUMMARY ===')
    print(f'retail idx 9984: {len(retail)} entities')
    print(f'our idx 295: {len(our_295)} entities')
    print(f'our idx 310: {len(our_310)} entities')
    print(f'our combined: {len(our_295) + len(our_310)} entities')
