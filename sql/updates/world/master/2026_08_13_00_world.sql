--
-- Rated Battleground (classic 10v10 premade) aggregate template.
--
-- BattlemasterList 100. Like 6 "All Arenas", 32 "Random Battleground" and 1101 "Battleground Blitz",
-- this is an AGGREGATE: its maps come from BattlemasterListXMap and
-- BattlegroundMgr::CreateNewBattleground resolves each back to the real single-map template, which
-- supplies the actual start locations. Start locs are therefore 0/0 and the accompanying code change
-- adds 100 to the WorldSafeLocs exemption alongside the other aggregates. Inventing a WorldSafeLocs id
-- here would be a value that is never read, and a wrong one silently drops the whole template.
--
-- Player counts and level range come from the client DB2 (BattlegroundTemplate::GetMin/MaxPlayersPerTeam
-- return BattlemasterEntry->Min/MaxPlayers), NOT from this table - LoadBattlegroundTemplates only reads
-- ID, AllianceStartLoc, HordeStartLoc, StartMaxDist, Weight and ScriptName. The remaining columns are
-- legacy and take their defaults.
--
-- Idempotent.

DELETE FROM `battleground_template` WHERE `ID` = 100;
INSERT INTO `battleground_template`
  (`ID`, `MinPlayersPerTeam`, `MaxPlayersPerTeam`, `MinLvl`, `MaxLvl`, `AllianceStartLoc`, `HordeStartLoc`, `StartMaxDist`, `Weight`, `ScriptName`, `Comment`) VALUES
  (100, 10, 10, 60, 90, 0, 0, 0, 1, '', 'BG - Rated Battleground');
