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

-- Quest 90882 "Gnoll Way" had SIX kill objectives at the same StorageIndex 0 (the real Blizzard one
-- 461730 = kill 10 x 244672, plus five fabricated copies 9088200-9088204 for the other gnoll
-- entries) -> the client showed 6 "slay 10 gnolls" tasks and inflated the count. Keep only the real
-- objective (461730); all live gnolls credit 244672 via creature_template.KillCredit1.
DELETE FROM `quest_objectives` WHERE `QuestID`=90882 AND `ID` IN (9088200,9088201,9088202,9088203,9088204);

-- LEVEL SCALING: the RPE gnolls use ContentTuningID 4306 (scales to the player -> ~lvl 21) but the
-- Hammerfall grunts had ContentTuningID 0 (fixed lvl 80), so the friendly grunts and the enemy gnolls
-- were level-mismatched. Give the RPE-ONLY grunts (all on map 2927 only, safe to rescale globally)
-- the same 4306 on the normal difficulty. NOT the bosses 244675/244709 (shared with map 0).
UPDATE `creature_template_difficulty` SET `ContentTuningID`=4306 WHERE `Entry` IN (230248,232019,245028,245052) AND `DifficultyID`=0;

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

-- ---- GO'SHEK FARM ARRIVAL FIXES (2026-08-22, from live test) ----
-- The flight tutorial 90883 ("To Go'shek Farm") drops the player at the farm (map 2927, zone 16432,
-- area 16456) with 90883 still IN PROGRESS -- you turn it in AT the farm. Four bugs made arrival wrong:
--
-- (A) Farm questgivers Jaina(244655)/Thrall(244656)/Farmer Bruvk(244729) are on the farm arrival phase
--     1959, but its render condition was QUESTREWARDED(90883) -- i.e. only AFTER you hand in the very
--     quest whose turn-in NPCs these are. Chicken-and-egg: the turn-in NPCs stay hidden until you turn
--     in the quest. Re-gate 1959 to the PRECEDING quest 90882 "Gnoll Way" (rewarded just before you fly,
--     and stays rewarded for the whole farm chapter) so the leaders are visible the moment you land.
--     phase_area(16456,1959) already exists; this only fixes WHEN 1959 applies.
UPDATE `conditions` SET `ConditionValue1`=90882
 WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup`=1959 AND `ConditionTypeOrReference`=8 AND `ConditionValue1`=90883;
--
-- (B) The battle-aftermath corpses around the leaders are DEDICATED corpse entries -- 249254 Ogre
--     Destroyer (1 spawn) + 249255 Kobold Pillager (5 spawns), corpse-only, all 6 at the farm -- NOT
--     the live-kill mobs 244674/244676 (which drop quest items for 90886 and must stay killable). The
--     corpses were on phase 4 (the post-turn-in combat phase) so they never showed with the leaders on
--     arrival. Move them onto the leaders' arrival phase 1959 so the tableau is co-visible.
UPDATE `creature` SET `PhaseId`=1959 WHERE `map`=2927 AND `id` IN (249254,249255);
--
-- (C) Those corpse entries had no death state ("kobold pillager and the one ogre destroyer around them
--     has no death state"). Give them the permanent Feign-Death aura 29266 -- the exact mechanism the
--     Hammerfall pad Gnoll Assailants (245027) use. Both entries are corpse-only, so template-level is safe.
UPDATE `creature_template_addon` SET `auras`='29266' WHERE `entry` IN (249254,249255);
--
-- (D) Phase 4 (farm COMBAT phase: live 244674/244676/Runk 244675) had orphaned conditions -- SourceEntry
--     was 0 ("Area 0 does not have phase 4" in DBErrors), so the phase applied UNCONDITIONALLY, showing
--     the farm-fight mobs in every quest state (part of the "too many mobs" the tester saw). This phase
--     was missed by the earlier SourceEntry sweep (which fixed 1961/1959/1610/3). Set it to farm area 16456
--     so phase 4 is gated properly (90883 rewarded AND NOT 90888 rewarded).
UPDATE `conditions` SET `SourceEntry`=16456 WHERE `SourceTypeOrReferenceId`=26 AND `SourceGroup`=4 AND `SourceEntry`=0;
