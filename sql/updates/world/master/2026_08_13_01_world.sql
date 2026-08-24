--
-- PvP Brawl: "Brawl: Deep Six" aggregate template (BattlemasterList 879).
--
-- Deep Six is the brawl the server advertises by default (worldserver.conf.dist Brawl.PvpBrawlID = 11,
-- Brawl.BattlemasterListID = 879). It was chosen because it is the only brawl whose maps this core can
-- actually run: BattlemasterListXMap gives 879 the maps 727 (Silvershard Mines), 998 (Temple of Kotmogu),
-- 1803 (Seething Shore) and 2106 (Warsong Gulch), every one of which is already a random-battleground map
-- (BattlemasterList 32) with its own battleground_template row and its own battleground script. Brawls such
-- as Gravity Lapse (859), Southshore vs. Tarren Mill (789) or Classic Ashran (1021) point at maps with no
-- implementation here, so advertising one of those would produce a queue that could never start a match.
--
-- BattlemasterList 879 in the 12.0.7 client DB2: PvpType 0 (Battleground), MinPlayers 5, MaxPlayers 6,
-- MaxGroupSize 6, levels 50-90, Flags 0x20 (IsBrawl). Those numbers are read from the DB2 at runtime
-- (BattlegroundTemplate::GetMin/MaxPlayersPerTeam go through BattlemasterEntry), not from this table:
-- LoadBattlegroundTemplates only selects ID, AllianceStartLoc, HordeStartLoc, StartMaxDist, Weight and
-- ScriptName, so the remaining columns here are legacy and take their defaults. The 6v6 team size therefore
-- comes out of the brawl's own DB2 row, which is what makes this a Deep Six queue rather than a normal one.
--
-- Start locations are 0/0 deliberately. This is an aggregate template like 6 "All Arenas", 32 "Random
-- Battleground" and 1101 "Battleground Blitz": CreateNewBattleground resolves its BattlemasterListXMap maps
-- back to the individual single-map templates, and those supply the real start locations. The accompanying
-- code change exempts multi-map IsBrawl templates from the WorldSafeLocs requirement for that reason -
-- putting a made-up WorldSafeLocs id here would be a value that is never read, and a wrong one silently
-- drops the entire template.
--
-- Idempotent.

DELETE FROM `battleground_template` WHERE `ID` = 879;
INSERT INTO `battleground_template`
  (`ID`, `MinPlayersPerTeam`, `MaxPlayersPerTeam`, `MinLvl`, `MaxLvl`, `AllianceStartLoc`, `HordeStartLoc`, `StartMaxDist`, `Weight`, `ScriptName`, `Comment`) VALUES
  (879, 5, 6, 50, 90, 0, 0, 0, 1, '', 'BG - Brawl: Deep Six');
