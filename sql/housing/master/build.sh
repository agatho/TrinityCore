#!/bin/bash
set -e

SRC="sql/housing"
DST="sql/housing/master"
DATE="$(date +%Y-%m-%d)"

# Helper: concatenate files with a section marker header.
emit_section() {
    local out="$1"
    local f="$2"
    echo "" >> "$out"
    echo "-- ============================================================================" >> "$out"
    echo "-- Source: sql/housing/$(basename "$f")" >> "$out"
    echo "-- ============================================================================" >> "$out"
    cat "$f" >> "$out"
    echo "" >> "$out"
}

emit_header() {
    local out="$1"
    local title="$2"
    local db="$3"
    cat > "$out" <<HEOF
-- =============================================================================
-- ${title}
-- =============================================================================
-- Trinity Housing — bundled installation master file
-- Target database: ${db}
-- Generated: ${DATE}
--
-- This file aggregates every housing-related SQL file from sql/housing/ in the
-- correct install order. To install, run against the ${db} database:
--
--     mysql -u <user> -p ${db} < $(basename "$out")
--
-- The individual source files under sql/housing/ remain authoritative; this
-- aggregate is a convenience bundle for testers. Regenerate with the script
-- under sql/housing/master/build.sh after any source edit.
-- =============================================================================
HEOF
}

# -----------------------------------------------------------------------------
# CHARACTERS — schema + migrations (skip the destructive reset)
# -----------------------------------------------------------------------------
CH="$DST/MASTER_housing_characters.sql"
emit_header "$CH" "Housing — characters database" "characters"
emit_section "$CH" "$SRC/housing_schema.sql"
emit_section "$CH" "$SRC/characters_housing_composite_pk_migration.sql"
emit_section "$CH" "$SRC/characters_housing_base_room_migration.sql"
emit_section "$CH" "$SRC/characters_housing_visual_room_migration.sql"
emit_section "$CH" "$SRC/characters_housing_grid_migration.sql"
emit_section "$CH" "$SRC/characters_housing_racial_style_migration.sql"
emit_section "$CH" "$SRC/characters_housing_texture_migration.sql"
emit_section "$CH" "$SRC/characters_housing_per_surface_theme.sql"
emit_section "$CH" "$SRC/characters_housing_decor_placement_time.sql"
emit_section "$CH" "$SRC/characters_housing_decor_scale.sql"

# -----------------------------------------------------------------------------
# HOTFIXES — plot/map DB2 hotfixes + initiative tables
# -----------------------------------------------------------------------------
HF="$DST/MASTER_housing_hotfixes.sql"
emit_header "$HF" "Housing — hotfixes database" "hotfixes"
emit_section "$HF" "$SRC/hotfixes_initiative_tables.sql"
emit_section "$HF" "$SRC/hotfixes_initiative_data.sql"
emit_section "$HF" "$SRC/hotfixes_housing.sql"
emit_section "$HF" "$SRC/hotfixes_housing_map_difficulty.sql"
emit_section "$HF" "$SRC/hotfixes_housing_cosmetic_phases.sql"
emit_section "$HF" "$SRC/hotfixes_room_component_option.sql"
emit_section "$HF" "$SRC/hotfixes_room_component_texture.sql"

# -----------------------------------------------------------------------------
# WORLD — templates, spawns, scripts
# -----------------------------------------------------------------------------
WL="$DST/MASTER_housing_world.sql"
emit_header "$WL" "Housing — world database" "world"
emit_section "$WL" "$SRC/world_housing_templates.sql"
emit_section "$WL" "$SRC/world_housing_go_templates.sql"
emit_section "$WL" "$SRC/world_interior_door_go.sql"
emit_section "$WL" "$SRC/world_housing_remove_db_cornerstones.sql"
emit_section "$WL" "$SRC/world_housing_areatrigger.sql"
emit_section "$WL" "$SRC/world_housing_spawn_difficulties.sql"
emit_section "$WL" "$SRC/world_housing_quest_chain.sql"
emit_section "$WL" "$SRC/world_housing_door_script.sql"
emit_section "$WL" "$SRC/world_spell_target_position_housing.sql"
emit_section "$WL" "$SRC/world_alliance_neighborhood_spawns.sql"
emit_section "$WL" "$SRC/world_horde_neighborhood_spawns.sql"

echo "Wrote: $CH ($(wc -l < "$CH") lines, $(du -h "$CH" | cut -f1))"
echo "Wrote: $HF ($(wc -l < "$HF") lines, $(du -h "$HF" | cut -f1))"
echo "Wrote: $WL ($(wc -l < "$WL") lines, $(du -h "$WL" | cut -f1))"
