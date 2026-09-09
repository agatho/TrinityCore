-- Migration: 0003_talent_builds
-- Date:    2026-05-07
-- Purpose: Curated per-spec talent build storage. Each (class, spec, context)
--          row stores a complete trait spend as a CSV of colon-triples.
--          The bot loads these on demand, picks the row matching its
--          current context (Raid / MythicPlus / PvP / Default), and applies
--          via TraitMgr commit.
-- Reverts: yes (DROP TABLE).

CREATE TABLE IF NOT EXISTS playerbot_v2_talent_build (
    class_id        TINYINT UNSIGNED NOT NULL,
    spec_id         INT UNSIGNED NOT NULL,
    context         TINYINT UNSIGNED NOT NULL,
    label           VARCHAR(128) NOT NULL DEFAULT '',
    entries_json    MEDIUMTEXT NOT NULL,
    source_url      VARCHAR(512) NOT NULL DEFAULT '',
    updated_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
                                ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (class_id, spec_id, context),
    KEY idx_spec (spec_id, context)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Column docs (kept here below the CREATE so the migration body stays
-- a single statement free of comment-embedded semicolons that the
-- migration parser used to choke on):
--   class_id     ChrClasses.db2 id (1=Warrior, 2=Paladin, 3=Hunter...)
--   spec_id      ChrSpecialization.db2 id (e.g., 71=Arms, 72=Fury)
--   context      0=Default, 1=Raid, 2=MythicPlus, 3=PvP, 4=Leveling.
--                Default is the fallback when no context-specific row
--                exists for the bot.
--   label        Human-readable name for diagnostics
--                ("Arms Slaughterhouse 11.1.5").
--   entries_json CSV of colon-triples: nodeID:entryID:ranks separated
--                by commas. Loader parses into a TraitConfig.Entries
--                vector. Empty string is allowed (bot falls back to
--                TraitMgr starter build).
--   source_url   wowhead / simc origin URL for human reference.

-- Record this migration as applied.
INSERT INTO playerbot_v2_schema_version (version, sha256)
VALUES (3, 'pending-fill-at-release-time-with-actual-sha256-of-this-file');
