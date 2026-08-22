-- ============================================================================
-- Arathi Catch-Up / RPE -- consolidation regression fixups (2026-08-21, post-live-test)
-- ============================================================================
-- Live testing after the single-branch consolidation surfaced regressions. This file now carries
-- only the feign-death revert; the phasing is fixed properly in 02 (see below), NOT by unphasing.
--
-- PHASING -- superseded approach. An earlier revision of this file did
--   `UPDATE creature SET PhaseId=0 ... 8000000-8009999` to force everything always-visible. That was
--   WRONG: the RPE questgivers are implemented as SEPARATE creature entries per POI (Jaina =
--   244643 pad / 244655 farm / 244657 siege / 244667 climax / 244714 hub; Thrall likewise), each in
--   its own quest-step phase, so they read as ONE character following you from POI to POI. Unphasing
--   them to 0 shows ALL of them at once (five Jainas scattered across the map). The real bug was
--   narrow: the two HAMMERFALL ARRIVAL phases (1961 town+leaders, 37 gnoll camp) were gated on
--   QUESTTAKEN(90883) -- one quest too late, because the wire phase graph had no 90882/arrival
--   window -- so the 90882 questgivers and slay-target gnolls were hidden at arrival. That is fixed
--   at the source in 2026_08_21_02 (the two phase conditions re-gated to "active until 90883
--   REWARDED"). The blanket PhaseId=0 UPDATE is REMOVED here; the per-POI leader phasing stands.
--
-- FEIGN-DEATH revert. The 7 pad Gnoll Assailants (245027) were given a permanent Feign-Death aura
--   (29266) during consolidation (ruling R1), on the strength of the third-party 68453 file calling
--   them "corpses". Our OWN capture (69382) contains ZERO occurrences of aura 29266 on 245027 --
--   they are LIVE gnolls in our data, not corpses. The feign-death addon is exactly what froze them.
--   Revert it: 245027 is a normal live standing gnoll. (No wander was captured, so they stand until
--   engaged -- capture-faithful; patrol/wander would be fabrication.)
--
-- Idempotent. Applies after 2026_08_21_02 (spawns/phasing) and 01 (templates).
-- ============================================================================

-- "Mirror Image" (entry 31216, guid 8000192) at the pad is a capture artifact -- the standard Mage
-- Mirror Image combat summon, mined from a passing/present mage near the pad (same class of artifact
-- as the Arcane Phoenix). Not an RPE spawn; remove it.
DELETE FROM `creature` WHERE `guid` = 8000192;

-- Gnoll Assailants (245027) at the pad are the battle-aftermath CORPSES (tester-confirmed they
-- should be dead) -> permanent Feign Death (aura 29266). (An earlier revision cleared this on the
-- mistaken read that "gnolls don't move" meant they should be alive; the tester clarified they are
-- meant to be dead.) The live fightable gnolls are the camp mobs 244670/671/672, not these.
UPDATE `creature_template_addon` SET `auras` = '29266' WHERE `entry` = 245027;

-- Training Dummy (249245) was faction 35 (friendly/unattackable). Use faction 7, the faction 110 of
-- this realm's real training dummies use (attackable practice target).
UPDATE `creature_template` SET `faction` = 7 WHERE `entry` = 249245;

-- Training dummies (249245) are physically at the Hammerfall pad but were phased 1959 (the FARM
-- phase, area 16456) -- so they never show at the pad. Move them to the Hammerfall phase 1961
-- (area 16466), where the leaders/base town live.
UPDATE `creature` SET `PhaseId` = 1961 WHERE `map` = 2927 AND `id` = 249245;

-- ---- PHASING ROOT-CAUSE FIXES (2026-08-21, from reading the live realm) ----
-- (a) Phase 37 (Hammerfall gnoll camp) was NOT applied to the player at the pad while phase 1961
--     (leaders) WAS -- despite identical area (16466) and condition. Empirically 1961 renders and 37
--     does not, so move ALL Hammerfall gnoll content onto the proven-working phase 1961 (both are
--     "Hammerfall, visible until 90883 rewarded" anyway). Applies to the whole gnoll camp.
UPDATE `creature` SET `PhaseId` = 1961 WHERE `map` = 2927 AND `PhaseId` = 37;
-- (b) CONDITION_SOURCE_TYPE_PHASE requires SourceEntry = the AreaId (the phase's area), not 0. Ours
--     were 0, so every phase condition was orphaned ("Area 0 does not have phase X" in DBErrors.log)
--     and the phases applied unconditionally. Fix the SourceEntry to each phase's real area.
UPDATE `conditions` SET `SourceEntry` = 16466 WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` = 1961 AND `SourceEntry` = 0;
UPDATE `conditions` SET `SourceEntry` = 16456 WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` = 1959 AND `SourceEntry` = 0;
UPDATE `conditions` SET `SourceEntry` = 16453 WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` = 1610 AND `SourceEntry` = 0;
UPDATE `conditions` SET `SourceEntry` = 16458 WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` = 3    AND `SourceEntry` = 0;
-- Phase 37 is now emptied of spawns; drop its (orphaned) condition.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` = 37;

-- Restore the leader POSE auras the consolidation dropped. BOTH are present in OUR OWN capture
-- (aura-update opcode 0x670011, 6 frames): spell 1237118 "Casting (DNT)" and 1237057 "Kneel (DNT)"
-- (names verified in SpellName.db2 -- developer pose spells, Effect=apply-aura, NO summon effect).
-- Attribution 1237118->Jaina (244643, the mage in a persistent casting stance) / 1237057->Thrall
-- (244642) matches the 50-57 third-party capture + the spell-name semantics; our capture confirms
-- the auras exist. These make the leader HOLD an animation -- Jaina's casting stance is this aura,
-- which the consolidation had replaced with a bare StandState=0. They do NOT summon the flying
-- gnolls (no persistent gnoll-launch spell exists in the capture -- the flying gnolls are the intro
-- cinematic's own choreography, fired by SendCinematicStart(77)).
-- THE FLYING GNOLLS (persist after the intro; not selectable) = a SCENE, not creatures. Confirmed
-- from OUR capture: SPELL_AURA_PLAY_SCENE auras 1237116 -> SceneID 3692 (ambient pad) and 1248494 ->
-- SceneID 3749 ("Jaina stasis presentation" = the flying gnolls); both scene ids play via
-- SMSG_PLAY_SCENE (opcode 0x4500DF) at the arrival pad (-1101.7,-3554.4) in both sniffs. The
-- scene_template rows 3692/3749 are already authored (2026_08_21_04); what was missing is APPLYING
-- the scene-play auras. Anchor them on Jaina (244643) -- the pad centerpiece / the scene's namesake --
-- alongside her casting pose. This is why they are non-selectable (scene actors) and persist (a
-- creature PLAY_SCENE aura re-plays for every player in range), independent of the intro cinematic.
-- NOTE: exact anchor unit for the scene auras is inferred (Jaina); if the scene plays offset from the
-- pad, the anchor may be a separate invisible scene-bunny -- adjust after in-game check.
-- Jaina keeps ONLY her casting-pose aura. The scene-play spells (1237116->3692, 1248494->3749) were
-- moved OFF her: SPELL_AURA_PLAY_SCENE only fires when the aura target is a PLAYER (HandlePlayScene),
-- so on a creature they did nothing. The scenes are now played on the arriving player by the zone
-- script (player_arathi_rpe_intro_cinematic -> GetSceneMgr().PlayScene(3692/3749)).
UPDATE `creature_template_addon` SET `auras` = '1237118' WHERE `entry` = 244643;  -- Jaina: Casting pose (DNT)
UPDATE `creature_template_addon` SET `auras` = '1237057' WHERE `entry` = 244642;  -- Thrall: Kneel (DNT)
