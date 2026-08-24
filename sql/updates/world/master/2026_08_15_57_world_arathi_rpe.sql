--
-- Arathi Returning Player Experience, map 2927 - pad poses, part 2.
--
-- 1) Thrall / Jaina hold a pose on the pad; both carry a cosmetic aura in the capture.
-- 2) The outdoor 245027 Assailants are corpses, not the unselectable stasis cloud (that is
--    scene 3749, see 2026_08_15_06). The FLOATING | SESSILE + AnimTier 3 presentation given
--    to them in 2026_08_15_05 made them hover as if alive, so drop it and use the permanent
--    Feign Death sibling this tree already uses for the Dracthyr Talon Kethahn set-piece.
--    The training dummies keep their floating presentation.
--
-- UNVERIFIED: creatures 244642 / 244643 / 245027, auras 1237057 / 1237118, and aura 29266
--             (taken from the existing Dracthyr usage in this tree rather than from a capture).
--

-- ---------------------------------------------------------------------------
-- Thrall / Jaina pad poses
-- ---------------------------------------------------------------------------
DELETE FROM `creature_template_addon` WHERE `entry` IN (244642, 244643);
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(244642, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '1237057'), -- Thrall pad pose
(244643, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '1237118'); -- Jaina cast-hold pose

-- ---------------------------------------------------------------------------
-- Outdoor Assailants: back on the ground, permanently feigning death
-- ---------------------------------------------------------------------------
UPDATE `creature_template_difficulty`
SET `StaticFlags1`=0
WHERE `Entry`=245027 AND `DifficultyID`=0;

DELETE FROM `creature_template_addon` WHERE `entry`=245027;
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(245027, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '29266'); -- Permanent Feign Death

-- Re-assert the training dummy float (unchanged from 2026_08_15_05), so the row survives
-- regardless of the order these two files land in.
DELETE FROM `creature_template_addon` WHERE `entry`=249245;
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(249245, 0, 0, 0, 3, 4, 1, 0, 0, 0, 0, 0, 0, NULL); -- Training Dummy float
