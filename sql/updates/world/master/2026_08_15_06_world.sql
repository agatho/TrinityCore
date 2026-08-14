--
-- Arathi Returning Player Experience, map 2927 - pad poses and the pad scenes.
--
-- 1) StandStates on the pad: Win'sa kneels, one Horde Grunt sits, one lies dead.
-- 2) scene_template rows for the two pad scenes. The stasis cloud around Jaina is a scene,
--    not creatures: SPELL_AURA_PLAY_SCENE (430) carries the SceneID in EffectMiscValue, and
--    without a scene_template row the aura has nothing to play.
--      spell 1237116 -> SceneID 3692 (ambient pad)
--      spell 1248494 -> SceneID 3749 (Jaina stasis presentation)
--
-- UNVERIFIED: scene ids 3692 / 3749, their Flags and ScriptPackageID (4617 / 4681), and the
--             spells 1237116 / 1248494 that carry them. Nothing in this tree could confirm
--             these, and this file does not by itself cast either spell.
--
-- Guids are the pad spawns from 2026_08_15_05 (@CGUID 11002024): +7 Win'sa, +8/+9 the Grunts.
--

-- UnitStandStateType: SIT = 1, DEAD = 7, KNEEL = 8
DELETE FROM `creature_addon` WHERE `guid` IN (11002031, 11002032, 11002033);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(11002031, 0, 0, 8, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL), -- 245026 Win'sa @ -1089.58,-3545.22 - KNEEL
(11002032, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL), -- 245028 Horde Grunt @ -1088.34,-3542.44 - SIT
(11002033, 0, 0, 7, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL); -- 245028 Horde Grunt @ -1083.78,-3553.58 - DEAD

DELETE FROM `scene_template` WHERE `SceneId` IN (3692, 3749);
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES
(3692, 16, 4617, 0, ''), -- ambient pad; played by spell 1237116
(3749, 17, 4681, 0, ''); -- Jaina stasis presentation; played by spell 1248494
