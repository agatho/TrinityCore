-- Ported from the independent 12.0.7 fork (evry/master-track). The evidence cited below is the
-- source branch's; the creature / quest / gameobject / scene ids, coordinates and timings in this
-- file have NOT been independently re-verified against our own client data or captures.
-- Dracthyr intro: skip generic intro movie 969; room movie spells (394245–394282) handle the camera pan.

UPDATE `playercreateinfo` SET `intro_movie_id` = NULL WHERE `race` IN (52, 70) AND `class` = 13;
