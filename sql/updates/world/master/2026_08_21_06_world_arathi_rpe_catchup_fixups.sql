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

-- Revert the R1 feign-death corpse aura on 245027 -- our capture shows no aura 29266; live gnolls.
UPDATE `creature_template_addon` SET `auras` = '' WHERE `entry` = 245027;

-- Training dummies (249245) are physically at the Hammerfall pad but were phased 1959 (the FARM
-- phase, area 16456) -- so they never show at the pad. Move them to the Hammerfall phase 1961
-- (area 16466), where the leaders/base town live.
UPDATE `creature` SET `PhaseId` = 1961 WHERE `map` = 2927 AND `id` = 249245;

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
UPDATE `creature_template_addon` SET `auras` = '1237118 1237116 1248494' WHERE `entry` = 244643;  -- Jaina: Casting pose + scenes 3692/3749 (flying gnolls)
UPDATE `creature_template_addon` SET `auras` = '1237057' WHERE `entry` = 244642;  -- Thrall: Kneel (DNT)
