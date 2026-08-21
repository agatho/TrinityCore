-- ============================================================================
-- CONTENT SLICE -- Arathi Catch-Up Experience :: Phase E quest_poi (worldmap pins)
-- ============================================================================
-- Branch: content   Path: sql/content/EasternKingdoms/ArathiHighlands/CatchUpExperience/
-- Server mapID: 2927 (CORRECTED from 2796 -- real server map, verified from wire; uiMap 2451 display-only)   client uiMapID **2451** (Arathi Highlands worldmap tile -- used
--   here, unlike elsewhere in this slice where 2451 is display-only).
-- Source: plan Part 1.7 Arathi POI list (area_poi.sql, VerifiedBuild 69299) + Part 1.3
--   objective-cluster coordinates. quest_poi.sql from the harvest bundle has exactly ONE
--   908xx row (QuestID=90882, MapID=2927, UiMapID=2451) -- MapID 2927 MATCHES this
--   zone's real server map 2927 (CONFIRMED from wire), so it is VALID (the earlier
--   'noise' dismissal was the 2796 map error); MapID corrected to 2927 below;
--   every row below is authored fresh from the POI/cluster coordinates per Requirement 5.
-- CANDIDATE ONLY -- review before applying to any branch. Never applied to a live DB/realm.
-- Idempotent (INSERT ... ON DUPLICATE KEY UPDATE -> re-apply safe; PK is
--   (QuestID,BlobIndex,Idx1)). One pin (BlobIndex=1,Idx1=1) per quest, matching the
--   observed convention for simple single-objective quests elsewhere in the oracle data
--   (e.g. QuestID 34586/41141 in the captured quest_poi.sql).
-- Coordinates authored from plan clusters -- TODO Phase K: refine against a real capture.
-- ============================================================================

INSERT INTO `quest_poi` (`QuestID`, `BlobIndex`, `Idx1`, `ObjectiveIndex`, `QuestObjectiveID`, `QuestObjectID`, `MapID`, `UiMapID`, `Priority`, `Flags`, `WorldEffectID`, `PlayerConditionID`, `NavigationPlayerConditionID`, `SpawnTrackingID`, `AlwaysAllowMergingBlobs`) VALUES
 (90882, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Hammerfall (Part 1.7): 5 gnoll entries, multi-target
 (90883, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek/Dabyrie's Farmstead (Part 1.7): travel destination
 (90885, 1, 1, 0, 0, 244956, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: Prized Pumpkin
 (90886, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: item drops off farm mobs
 (90887, 1, 1, 0, 0, 244675, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Go'shek farm: Runk
 (90888, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde Keep (Part 1.7): travel destination
 (90893, 1, 1, 0, 0, 0,      2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde siege battlefield (Part 1.3): multi-target progress bar
 (90895, 1, 1, 0, 0, 249269, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Catapults cluster (Part 1.3)
 (90896, 1, 1, 0, 0, 244709, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- siege battlefield: Ro'grok (former Horde encampment)
 (90897, 1, 1, 0, 0, 244714, 2927, 2451, 0, 0, 0, 0, 0, 0, 0),  -- Stromgarde battlefield: Jaina @ Part 1.3 cluster coords
 (90911, 1, 1, 0, 0, 244714, 2927, 2451, 0, 0, 0, 0, 0, 0, 0)   -- Stromgarde battlefield: Jaina hub (same spot as 90897)
ON DUPLICATE KEY UPDATE `ObjectiveIndex`=VALUES(`ObjectiveIndex`), `QuestObjectiveID`=VALUES(`QuestObjectiveID`), `QuestObjectID`=VALUES(`QuestObjectID`), `MapID`=VALUES(`MapID`), `UiMapID`=VALUES(`UiMapID`), `Priority`=VALUES(`Priority`), `Flags`=VALUES(`Flags`), `WorldEffectID`=VALUES(`WorldEffectID`), `PlayerConditionID`=VALUES(`PlayerConditionID`), `NavigationPlayerConditionID`=VALUES(`NavigationPlayerConditionID`), `SpawnTrackingID`=VALUES(`SpawnTrackingID`), `AlwaysAllowMergingBlobs`=VALUES(`AlwaysAllowMergingBlobs`);
