-- ---------------------------------------------------------------------------
-- 2026-08-10  Housing: correct SYSTEM public neighborhoods after the
--             NeighborhoodMap faction/map seed correction.
--
-- Background
-- ----------
-- The server seed for `hotfixes`.`neighborhood_map` had NeighborhoodMap IDs 1
-- and 2 swapped relative to the client NeighborhoodMap.db2:
--     WRONG seed: ID1 = Map 2736 / Horde,    ID2 = Map 2735 / Alliance
--     CLIENT db2: ID1 = Map 2735 / Alliance,  ID2 = Map 2736 / Horde
-- Because of that, EnsurePublicNeighborhoods() built the two SYSTEM public
-- neighborhoods on the wrong NeighborhoodMap IDs:
--     Alliance system neighborhood -> neighborhoodMapId = 2  (should be 1)
--     Horde    system neighborhood -> neighborhoodMapId = 1  (should be 2)
-- The client UI routes each faction by the DB2 ID (Alliance -> ID1,
-- Horde -> ID2), so one faction could never reach its public neighborhood.
--
-- This migration realigns the two SYSTEM neighborhoods with the corrected map
-- seed by moving each to the NeighborhoodMap ID that resolves to the SAME
-- physical MapID it was already built on (2735 stays Alliance, 2736 stays
-- Horde). factionRestriction is left as-is because it is already correct for
-- each system neighborhood and, after this move, matches the corrected map
-- flags (so NeighborhoodMgr::VerifyNeighborhoodFactions converges instead of
-- fighting the fix).
--
-- Why the UPDATE (relabel) approach and not DELETE + recreate
-- ----------------------------------------------------------
-- Public neighborhoods can already contain player-purchased plots, houses,
-- rooms and decor (that is the entire point of a public neighborhood).
-- Deleting a system neighborhood would orphan/destroy that player data.
-- Relabeling only the `neighborhoodMapId` of the two SYSTEM rows preserves
-- every plot/house/decor/member row unchanged (they reference the
-- neighborhood by its stable `guid`, which this migration never changes).
--
-- Safety: player data is NEVER touched
-- ------------------------------------
-- The two SYSTEM public neighborhoods are the ONLY neighborhoods whose
-- `ownerGuid` is 0: EnsurePublicNeighborhoods() creates them with a sentinel
-- housing owner guid whose counter is 0 (see NeighborhoodMgr.cpp). Every
-- player- or guild-founded neighborhood has a non-zero ownerGuid. The WHERE
-- clauses below are therefore scoped to `ownerGuid = 0 AND isPublic = 1 AND
-- guildId = 0`, which can only match the two system-generated public
-- neighborhoods. No character_housing / *_decor / *_rooms / plot row is read
-- or modified.
--
-- factionRestriction values (see HousingDefines.h):
--     1 = Horde, 2 = Alliance
--
-- Ordering / ops note
-- -------------------
-- Apply this file to the `characters` database together with the corrected
-- `hotfixes`.`neighborhood_map` seed, BEFORE the first server restart that
-- loads the corrected seed. In that (standard) maintenance flow the system
-- neighborhoods are still in the original swapped state, so the faction-keyed
-- WHERE clauses match exactly. Idempotent: re-running is a no-op once the two
-- rows are on the correct NeighborhoodMap IDs.
-- ---------------------------------------------------------------------------

-- Alliance system public neighborhood: physically on Map 2735, currently
-- mislabeled with neighborhoodMapId = 2. The corrected seed maps 2735 -> ID 1.
UPDATE `neighborhoods`
   SET `neighborhoodMapId` = 1,
       `factionRestriction` = 2
 WHERE `ownerGuid` = 0
   AND `isPublic` = 1
   AND `guildId` = 0
   AND `factionRestriction` = 2
   AND `neighborhoodMapId` = 2;

-- Horde system public neighborhood: physically on Map 2736, currently
-- mislabeled with neighborhoodMapId = 1. The corrected seed maps 2736 -> ID 2.
UPDATE `neighborhoods`
   SET `neighborhoodMapId` = 2,
       `factionRestriction` = 1
 WHERE `ownerGuid` = 0
   AND `isPublic` = 1
   AND `guildId` = 0
   AND `factionRestriction` = 1
   AND `neighborhoodMapId` = 1;
