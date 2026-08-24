-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr intro: post-movie scene hook (idempotent; live DB verified 2026-06-27).
-- SceneId 3064, ScriptPackageID 3730 — chains stasis after room movie in C++ scene_dracthyr_evoker_intro.

DELETE FROM `scene_template` WHERE `SceneId` = 3064;
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `Encrypted`, `ScriptName`) VALUES
(3064, 25, 3730, 0, 'scene_dracthyr_evoker_intro');
