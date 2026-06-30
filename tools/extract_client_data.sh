#!/usr/bin/env bash
#
# extract_client_data.sh
#
# Linux / macOS helper to extract WoW client data for TrinityCore.
# Runs mapextractor -> vmap4extractor -> vmap4assembler -> mmaps_generator
# in the correct order.
#
# For the road-aware mmap workstream (P1.0b corpus dumping) pass --maps-only
# to stop after mapextractor.
#
# See src/modules/PlayerbotV2/docs/CLIENT_DATA_EXTRACTION.md for the full
# user-facing reference.
#
# Usage:
#   ./extract_client_data.sh \
#       --client-path "/path/to/_retail_" \
#       --output-path "/path/to/build/bin" \
#       [--maps-only] \
#       [--threads N] \
#       [--locale enUS]

set -euo pipefail

CLIENT_PATH=""
OUTPUT_PATH=""
MAPS_ONLY=0
THREADS=0
LOCALE="enUS"

die() {
    printf '\nERROR: %s\n' "$*" >&2
    exit 1
}

usage() {
    sed -n '3,25p' "$0"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --client-path) CLIENT_PATH="$2"; shift 2 ;;
        --output-path) OUTPUT_PATH="$2"; shift 2 ;;
        --maps-only)   MAPS_ONLY=1; shift ;;
        --threads)     THREADS="$2"; shift 2 ;;
        --locale)      LOCALE="$2"; shift 2 ;;
        -h|--help)     usage ;;
        *)             die "Unknown argument: $1 (use --help)" ;;
    esac
done

[[ -n "$CLIENT_PATH" ]] || die "--client-path is required"
[[ -n "$OUTPUT_PATH" ]] || die "--output-path is required"

step() {
    printf '\n==============================================================\n'
    printf ' %s\n' "$*"
    printf '==============================================================\n'
}

human_size() {
    # bytes -> human readable
    local b=$1
    if   [[ $b -ge $((1024*1024*1024)) ]]; then awk "BEGIN{printf \"%.2f GB\", $b/1073741824}"
    elif [[ $b -ge $((1024*1024)) ]];     then awk "BEGIN{printf \"%.1f MB\", $b/1048576}"
    else                                       awk "BEGIN{printf \"%.1f KB\", $b/1024}"
    fi
}

summary() {
    local root="$1"; shift
    local total=0
    printf '\n--- Output summary ---\n'
    for name in "$@"; do
        local p="$root/$name"
        if [[ -d "$p" ]]; then
            local count
            count=$(find "$p" -type f | wc -l)
            local bytes
            bytes=$(du -sb "$p" 2>/dev/null | awk '{print $1}')
            total=$((total + bytes))
            printf '  %-12s files=%-8s size=%s\n' "$name" "$count" "$(human_size "$bytes")"
        else
            printf '  %-12s MISSING\n' "$name"
        fi
    done
    printf '  %-12s %s\n' "TOTAL" "$(human_size "$total")"
}

# --- Validate output path + extractor binaries -----------------------------
[[ -d "$OUTPUT_PATH" ]] || die "OutputPath does not exist: $OUTPUT_PATH"
OUTPUT_PATH="$(cd "$OUTPUT_PATH" && pwd)"

required=(mapextractor)
if [[ $MAPS_ONLY -eq 0 ]]; then
    required+=(vmap4extractor vmap4assembler mmaps_generator)
fi
for bin in "${required[@]}"; do
    if [[ ! -x "$OUTPUT_PATH/$bin" ]]; then
        die "Required binary not found or not executable: $OUTPUT_PATH/$bin
Build the worldserver project first (cmake --build build)."
    fi
done

# --- Validate client install -----------------------------------------------
[[ -d "$CLIENT_PATH" ]] || die "ClientPath does not exist: $CLIENT_PATH"
if [[ ! -f "$CLIENT_PATH/.build.info" && ! -d "$CLIENT_PATH/Data" ]]; then
    die "ClientPath '$CLIENT_PATH' has no .build.info or Data/ — not a valid WoW install root.
Did you point at _retail_/Data/ instead of _retail_/ ?"
fi

printf 'Client : %s\n' "$CLIENT_PATH"
printf 'Output : %s\n' "$OUTPUT_PATH"
printf 'Locale : %s\n' "$LOCALE"
if [[ $THREADS -le 0 ]]; then printf 'Threads: (all cores)\n'; else printf 'Threads: %s\n' "$THREADS"; fi
if [[ $MAPS_ONLY -eq 1 ]]; then printf 'Mode   : MAPS-ONLY (road workflow)\n'; else printf 'Mode   : FULL\n'; fi

cd "$OUTPUT_PATH"

# --- Step 1: mapextractor --------------------------------------------------
step "Step 1/4: mapextractor (maps + dbc + Cameras + gt)"
e_flag=15
[[ $MAPS_ONLY -eq 1 ]] && e_flag=3
./mapextractor -i "$CLIENT_PATH" -o "$OUTPUT_PATH" -e "$e_flag" -f 1 -l "$LOCALE" \
    || die "mapextractor failed (exit $?). Check stdout above; usual culprits: CASC init failure, wrong client path."

if [[ $MAPS_ONLY -eq 1 ]]; then
    step "Maps-only mode: skipping vmap4/mmaps generation."
    summary "$OUTPUT_PATH" dbc maps Cameras gt
    printf '\nDONE (maps-only).\n'
    exit 0
fi

# --- Step 2: vmap4extractor ------------------------------------------------
step "Step 2/4: vmap4extractor (CASC -> Buildings/)"
vmap_args=(-d "$CLIENT_PATH")
[[ $THREADS -gt 0 ]] && vmap_args+=(--threads "$THREADS")
./vmap4extractor "${vmap_args[@]}" \
    || die "vmap4extractor failed (exit $?). Delete Buildings/dir_bin/ if it has leftovers from a prior run."

# --- Step 3: vmap4assembler ------------------------------------------------
step "Step 3/4: vmap4assembler (Buildings/ -> vmaps/)"
asm_args=(Buildings vmaps)
[[ $THREADS -gt 0 ]] && asm_args+=(--threads "$THREADS")
./vmap4assembler "${asm_args[@]}" \
    || die "vmap4assembler failed (exit $?)."

# --- Step 4: mmaps_generator -----------------------------------------------
step "Step 4/4: mmaps_generator (dbc+maps+vmaps -> mmaps/)"
mmap_args=(--input "$OUTPUT_PATH" --output "$OUTPUT_PATH")
[[ $THREADS -gt 0 ]] && mmap_args+=(--threads "$THREADS")
./mmaps_generator "${mmap_args[@]}" \
    || die "mmaps_generator failed (exit $?). Check dbc/, maps/, vmaps/0000/*.vmtree exist in OutputPath."

summary "$OUTPUT_PATH" dbc maps Cameras gt vmaps mmaps Buildings
printf '\nDONE (full extraction).\n'
printf "Tip: 'Buildings/' is intermediate and can be deleted to reclaim ~40 GB:\n"
printf '    rm -rf %s/Buildings %s/dir_bin\n' "$OUTPUT_PATH" "$OUTPUT_PATH"
