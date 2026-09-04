--
-- Solo Shuffle (1065) + Training Grounds (1145): give the arena templates valid WorldSafeLocs start
-- locations. The initial rows left AllianceStartLoc/HordeStartLoc = 0, so LoadBattlegroundTemplates
-- rejected them ("non-existing WorldSafeLocs.dbc id 0 in field AllianceStartLoc. BG not created"), leaving
-- both modes non-functional. Reuse Nagrand Arena's start locs (Alliance 929 / Horde 936) - a stock 3v3
-- arena that fits Solo Shuffle's 3v3 rounds and Training Grounds' 1v1 practice; Nagrand is also part of the
-- retail Solo Shuffle map rotation, so the placement is faithful.
--
UPDATE `battleground_template` SET `AllianceStartLoc` = 929, `HordeStartLoc` = 936 WHERE `ID` = 1065;
UPDATE `battleground_template` SET `AllianceStartLoc` = 929, `HordeStartLoc` = 936 WHERE `ID` = 1145;
