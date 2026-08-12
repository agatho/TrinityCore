--
-- Battleground Blitz (rated 8v8 solo/duo queue) aggregate template.
--
-- BattlemasterList 1101 in the 12.0.7 client DB2: PvpType 0 (Battleground), RatedPlayers 8,
-- MinPlayers 6, MaxPlayers 8, MaxGroupSize 2, levels 60-90. The player counts and level range are
-- read from that DB2 (BattlegroundTemplate::GetMin/MaxPlayersPerTeam return BattlemasterEntry->
-- Min/MaxPlayers), NOT from this table -- LoadBattlegroundTemplates only selects
-- ID, AllianceStartLoc, HordeStartLoc, StartMaxDist, Weight and ScriptName, so the remaining
-- columns here are legacy and take their defaults.
--
-- Start locations are 0/0 deliberately. This is an aggregate template: its maps come from
-- BattlemasterListXMap and BattlegroundMgr::CreateNewBattleground resolves each of them back to the
-- existing single-map template, which supplies the real start locations. The two other aggregates
-- already in this table -- 6 "All Arenas" and 32 "Random Battleground" -- also carry 0/0, and the
-- loader skips WorldSafeLocs validation for them; the accompanying code change adds 1101 to that
-- same exemption. Putting a made-up WorldSafeLocs id here instead would be a value that is never
-- read, and a wrong one silently drops the entire template.
--
-- Idempotent.

DELETE FROM `battleground_template` WHERE `ID` = 1101;
INSERT INTO `battleground_template`
  (`ID`, `MinPlayersPerTeam`, `MaxPlayersPerTeam`, `MinLvl`, `MaxLvl`, `AllianceStartLoc`, `HordeStartLoc`, `StartMaxDist`, `Weight`, `ScriptName`, `Comment`) VALUES
  (1101, 6, 8, 60, 90, 0, 0, 0, 1, '', 'BG - Battleground Blitz');
