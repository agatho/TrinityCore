--
-- Training Grounds battleground_template (BattlemasterList 1145 - solo unrated PvP practice).
-- The handler (HandleTrainingGroundsJoin) needs a template row to queue; the match is solo (1 player, no
-- opponents) and runs a BattlegroundTrainingGrounds instance that never auto-ends.
--
DELETE FROM `battleground_template` WHERE `ID` = 1145;
INSERT INTO `battleground_template`
  (`ID`,`MinPlayersPerTeam`,`MaxPlayersPerTeam`,`MinLvl`,`MaxLvl`,`AllianceStartLoc`,`HordeStartLoc`,`StartMaxDist`,`Weight`,`ScriptName`,`Comment`) VALUES
  (1145, 1, 1, 10, 90, 0, 0, 0, 1, '', 'Training Grounds');
