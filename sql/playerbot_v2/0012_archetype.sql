-- Migration: 0012_archetype
-- Date:    2026-06-14
-- Purpose: Per-bot ARCHETYPE foundation (#4A of
--          docs/LIVING_SERVER_PLAN_20260614.md). Personality (HOW a bot
--          plays) already exists; this adds Archetype (WHAT it does + WHEN it
--          is online) so the fleet is heterogeneous — casual-solo, hardcore-
--          raider, social-guildie, gatherer/AH-flipper, PvPer, altoholic-
--          explorer — the basis of a believable living server and of economy /
--          role variety.
--
--          Columns:
--            archetype_id                The curated archetype id (index into
--                                        BotArchetype.cpp's kArchetypeTable ==
--                                        ArchetypeId enum). 0 = CasualSolo (the
--                                        default + the value an un-rolled bot
--                                        reads as). Assigned on first spawn via
--                                        RollArchetype(SeedForBot(id)) and
--                                        written back; deterministic per bot so
--                                        it is stable across restarts.
--            session_start_at            When the bot's CURRENT play session
--                                        began. NULL when logged out. Reserved
--                                        for the session-rhythm logout layer
--                                        (documented followup) — stored now so
--                                        the data is available when that lands.
--            cumulative_session_minutes  Lifetime online minutes accrued by
--                                        this bot across sessions. Also for the
--                                        future session-rhythm layer. Not
--                                        decremented; monotonic.
-- Reverts: yes (DROP COLUMN per added column).

ALTER TABLE playerbot_v2_character
    ADD COLUMN archetype_id               TINYINT  UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN session_start_at           DATETIME          NULL DEFAULT NULL,
    ADD COLUMN cumulative_session_minutes BIGINT   UNSIGNED NOT NULL DEFAULT 0,
    ADD KEY idx_archetype_id (archetype_id);

-- Record this migration as applied (mirrors the 0011 record-version pattern).
INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (12, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
