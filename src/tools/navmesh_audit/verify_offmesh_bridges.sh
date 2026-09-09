#!/usr/bin/env bash
# =============================================================================
# verify_offmesh_bridges.sh  --  post-regen navmesh offmesh-bridge GATE
# =============================================================================
# Probes every bridge in offmesh.txt From->To via mmap_probe and FAILs if any
# bridge no longer routes, or if the total bridge count drops below a baseline.
#
# This exists because the 2026-06-12 mmap regen ran a generator binary without
# --offMeshInput and silently dropped EVERY offmesh bridge across all maps
# (every NNNN.mmap header shrank to 40 bytes = 0 connections). It was found only
# because a single bot (Uraimus) was noticed wedged in Dolanaar. This gate makes
# that entire class of regression self-reporting: run it after every regen.
#
# Usage:
#   verify_offmesh_bridges.sh <mmaps_dir> <offmesh.txt> <mmap_probe.exe> \
#                             [baseline.json] [map_filter]
#   --write-baseline as 4th arg writes a fresh baseline from the current pass set.
#
# A bridge PASSES iff mmap_probe routes From->To (STATUS OK / EXIT 0). With the
# offmesh link baked into the tile the two endpoints are one short hop apart;
# without it they sit in different navmesh components and the probe returns
# PARTIAL/NOPATH -- exactly the dropped-bridge signature.
#
# Exit 0 = all bridges route AND count >= baseline.  Exit 1 = regression.
# =============================================================================
set -uo pipefail

MMAPS="${1:?usage: <mmaps_dir> <offmesh.txt> <mmap_probe> [baseline|--write-baseline] [map_filter]}"
OFFMESH="${2:?missing offmesh.txt}"
PROBE="${3:?missing mmap_probe path}"
BASELINE="${4:-}"
MAP_FILTER="${5:-}"      # optional: only probe this mapId (faster spot-check)

[ -f "$PROBE" ]   || { echo "FATAL: mmap_probe not found: $PROBE"; exit 2; }
[ -f "$OFFMESH" ] || { echo "FATAL: offmesh.txt not found: $OFFMESH"; exit 2; }
[ -d "$MMAPS" ]   || { echo "FATAL: mmaps dir not found: $MMAPS"; exit 2; }

pass=0; fail=0; total=0
declare -a FAILED

while IFS= read -r line || [ -n "$line" ]; do
    # skip comments / blanks
    case "${line}" in ''|\#*) continue;; esac
    case "${line}" in [0-9]*) : ;; *) continue;; esac
    # turn  '1 13,30 (a b c) (d e f) 12.0'  into  '1 13 30 a b c d e f 12.0'
    read -r map tx ty fx fy fz dx dy dz _rest <<<"$(echo "$line" | tr ',()' '   ')"
    [ -z "${dz:-}" ] && continue
    [ -n "$MAP_FILTER" ] && [ "$map" != "$MAP_FILTER" ] && continue
    total=$((total+1))

    out=$("$PROBE" "$MMAPS" "$map" "$fx" "$fy" "$fz" "$dx" "$dy" "$dz" 2>&1)
    status=$(printf '%s\n' "$out" | grep -aoE 'STATUS: [A-Z_]+' | tail -1)
    if printf '%s\n' "$out" | grep -qaE '^EXIT: 0'; then
        pass=$((pass+1))
        printf 'PASS  map=%s tile=%s,%s (%s %s %s)->(%s %s %s)  %s\n' \
               "$map" "$tx" "$ty" "$fx" "$fy" "$fz" "$dx" "$dy" "$dz" "$status"
    else
        fail=$((fail+1))
        FAILED+=("map=$map tile=$tx,$ty from=($fx,$fy,$fz) to=($dx,$dy,$dz) $status")
        printf 'FAIL  map=%s tile=%s,%s (%s %s %s)->(%s %s %s)  %s\n' \
               "$map" "$tx" "$ty" "$fx" "$fy" "$fz" "$dx" "$dy" "$dz" "$status"
    fi
done < "$OFFMESH"

echo "----------------------------------------------------------------------"
echo "bridges probed=$total  PASS=$pass  FAIL=$fail  (mmaps=$MMAPS filter='${MAP_FILTER:-all}')"

# --- baseline handling --------------------------------------------------------
if [ "$BASELINE" = "--write-baseline" ]; then
    bf="$(dirname "$0")/navmesh_bridge_baseline.json"
    printf '{ "generated": "manual", "bridge_count": %s, "pass": %s, "fail": %s }\n' \
           "$total" "$pass" "$fail" > "$bf"
    echo "baseline written: $bf"
elif [ -n "$BASELINE" ] && [ -f "$BASELINE" ]; then
    base_count=$(grep -aoE '"bridge_count"[ ]*:[ ]*[0-9]+' "$BASELINE" | grep -aoE '[0-9]+$')
    if [ -n "${base_count:-}" ] && [ "$total" -lt "$base_count" ]; then
        echo "GATE FAIL: bridge count $total < baseline $base_count (dropped-offmesh signature)"
        exit 1
    fi
fi

if [ "$fail" -gt 0 ]; then
    echo "GATE FAIL: $fail bridge(s) do not route:"
    printf '  - %s\n' "${FAILED[@]}"
    exit 1
fi
echo "GATE PASS: all $pass bridges route."
exit 0
