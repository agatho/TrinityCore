-- ============================================================================
-- Arathi Catch-Up / RPE -- consolidation regression fixups (2026-08-21, post-live-test)
-- ============================================================================
-- Live testing after the single-branch consolidation surfaced three regressions, all rooted in
-- the consolidation having replaced the always-visible PhaseId-0 opening-segment spawns (50-57,
-- now retired by 00_cleanup) with the content set, which carried two latent bugs:
--
-- (1)+(2) Initial Jaina/Thrall and the vendor (Win'sa) vanished on arrival. The content set
--   authored them at PhaseId 1961 -- a phase 22_conditions gates on QUESTTAKEN(90883). But those
--   NPCs GIVE the first quest 90882 (before 90883), so gating them on 90883 makes them invisible at
--   arrival (chicken-and-egg). The retired 50-57 copies sat at PhaseId 0 (always visible), which is
--   what made the pad work. The content per-quest phasing is UNVERIFIED (see 22_conditions banner)
--   and is what breaks the base state, so we restore the known-working behaviour: unphase the whole
--   RPE spawn set (PhaseId 0 = always visible, exactly as the 50-57 set was). Re-introducing a
--   correct per-quest phase layer (e.g. the warzone->peace swap after Ro'grok) is a later task.
--
-- (3) "Gnoll mobs at Hammerfall do not move." The 7 pad Gnoll Assailants (245027) had been given a
--   permanent Feign-Death aura (29266) during consolidation (ruling R1), on the strength of the
--   third-party 68453 opening-segment file calling them "corpses". Our OWN capture (69382) contains
--   ZERO occurrences of aura 29266 on 245027 -- they are LIVE gnolls in our data, not corpses. The
--   feign-death addon is exactly what froze them. Revert it: 245027 is a normal live standing gnoll.
--   (Their positions were never captured with wander movement, so they stand until engaged -- that is
--   capture-faithful; patrol/wander would be fabrication.)
--
-- Idempotent. Applies after 2026_08_21_02 (spawns) and 01 (templates).
-- ============================================================================

-- (1)+(2) Make the whole RPE spawn set always-visible again (restore the 50-57 PhaseId-0 behaviour).
UPDATE `creature` SET `PhaseId` = 0 WHERE `map` = 2927 AND `guid` BETWEEN 8000000 AND 8009999;

-- (3) Revert the R1 feign-death corpse aura on 245027 -- our capture shows no aura 29266; they are
-- live gnolls. Clear the aura but keep the row (standing, sheathed) so re-apply is clean.
UPDATE `creature_template_addon` SET `auras` = '' WHERE `entry` = 245027;
