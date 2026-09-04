--
-- Rated Solo Shuffle battleground_template (BattlemasterList 1065 = "All Arenas", solo queue entry).
-- The handler (HandleBattlemasterJoinRatedSoloShuffle) needs a template row to queue; the match runs on a
-- solo-only rated-arena map from the DB2 (pool 1053-1064). 3v3 rounds (MinPlayersPerTeam=MaxPlayersPerTeam=3).
--
DELETE FROM `battleground_template` WHERE `ID` = 1065;
INSERT INTO `battleground_template`
  (`ID`,`MinPlayersPerTeam`,`MaxPlayersPerTeam`,`MinLvl`,`MaxLvl`,`AllianceStartLoc`,`HordeStartLoc`,`StartMaxDist`,`Weight`,`ScriptName`,`Comment`) VALUES
  (1065, 3, 3, 10, 90, 0, 0, 0, 1, '', 'Arena - Solo Shuffle');
