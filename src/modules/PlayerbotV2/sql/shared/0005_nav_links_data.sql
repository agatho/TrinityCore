-- ===========================================================================
-- PlayerbotV2: additional verified traversal links (playerbot_nav_links DATA)
-- Target: the SHARED playerbot database (Playerbot.SharedDatabase, e.g.
--         "wowc_playerbot"). Apply AFTER shared/0003 (table + first link).
--         NOT part of TC's world/char/login updater. Date: 2026-07-22
-- ===========================================================================
--
-- 0003 seeded only the WC Serpentis jump-split; these are the other owner/
-- session-verified links authored live (WC serpentine drop, SFK courtyard
-- climb, RFK entrance fall-strand rescue) and must ship so a tester's bots
-- cross the same real geometric splits. Only verified=1 rows load at runtime.
-- id is AUTO_INCREMENT (assigned on import); the runtime keys on coordinates.
-- ===========================================================================

INSERT INTO `playerbot_nav_links`
  (`map_id`, `from_x`, `from_y`, `from_z`, `to_x`, `to_y`, `to_z`,
   `radius`, `bidirectional`, `kind`, `comment`, `verified`, `created_by`)
VALUES
  (43, 0, 407.7, -61.6, 6.1, 443.2, -75.4,
   10, 0, 'drop', 'WC serpentine ledge drop crumb24->25: live-verified 2026-07-03 (hop + descent worked); re-inserted after accidental delete', 1, 'claude-live-forensics'),
  (33, -253.7, 2130.9, 75.3, -256.1, 2117.1, 81.3,
   10, 1, 'climb', 'SFK courtyard stairs to route crumb 1: straight-path COMPLETE 25.8y/3pts but bot stepper lip-truncates (campaign RED 2026-07-19, evidence shadowfang_keep_0719_1343)', 1, 'claude-campaign'),
  (47, 2168.7, 1628.6, 45.6, 2172.8, 1625.1, 82.1,
   12, 0, 'climb', 'RFK entrance fall-strand rescue: landing pocket at z45.6, probe to boss INCOMPLETE 34.7y unreached. One-way back onto last good crumb.', 1, 'claude-phase2');
