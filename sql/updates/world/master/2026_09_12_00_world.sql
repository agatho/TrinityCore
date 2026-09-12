--
-- delve_template.finalBossEntry is read by DelveMgr::LoadDelveTemplates (column 26) and written by
-- 2026_08_08_40_world.sql / 2026_08_08_41_world.sql, but no schema update ever added it: a fresh
-- database fails both data files ("Unknown column 'finalBossEntry'") and the loader.
--
ALTER TABLE `delve_template`
    ADD COLUMN `finalBossEntry` int unsigned NOT NULL DEFAULT '0' COMMENT 'creature_template entry of the delve boss (0 = none)' AFTER `worldState26903`;
