-- ============================================================================
-- Initiative DB2 Data Population
-- Apply to: tc_hotfixes database
-- Requires: hotfixes_initiative_tables.sql (table schema)
-- ============================================================================
-- Provides initial data for the neighborhood initiative system:
--   4 NeighborhoodInitiative entries (Gathering, Crafting, Combat, Exploration)
--   5 InitiativeTask entries with TargetCounts
--   InitiativeXTask join entries
--   InitiativeCycle entries (1 per initiative)
--   InitiativeMilestone entries (3 per cycle: 33%, 66%, 100%)
--   InitiativeReward + InitiativeRewardXMilestone entries
-- ============================================================================

-- ============================================================
-- 1. NeighborhoodInitiative — 4 initiative types
-- ============================================================
DELETE FROM `neighborhood_initiative` WHERE `ID` IN (1, 2, 3, 4);
INSERT INTO `neighborhood_initiative` (`Name`, `Description`, `ID`, `InitiativeType`, `Duration`, `RequiredParticipants`, `RewardCurrencyID`, `VerifiedBuild`) VALUES
('Gathering Initiative', 'Gather resources from across the world to contribute to your neighborhood.', 1, 1, 604800, 1, 0, 65940),
('Crafting Initiative', 'Craft items to improve your neighborhood and earn rewards.', 2, 2, 604800, 1, 0, 65940),
('Combat Initiative', 'Defeat enemies in the world to prove your neighborhood''s strength.', 3, 3, 604800, 1, 0, 65940),
('Exploration Initiative', 'Complete quests and explore the world for your neighborhood.', 4, 4, 604800, 1, 0, 65940);

-- ============================================================
-- 2. InitiativeTask — 5 tasks across initiative types
-- ============================================================
DELETE FROM `initiative_task` WHERE `ID` IN (1, 2, 3, 4, 5);
INSERT INTO `initiative_task` (`Name`, `Description`, `ID`, `TaskType`, `TargetCount`, `CriteriaTreeID`, `SortOrder`, `Flags`, `UiTextureKitID`, `VerifiedBuild`) VALUES
('Gather Herbs', 'Gather herbs from the world.', 1, 1, 50, 0, 1, 0, 0, 65940),
('Mine Ore', 'Mine ore deposits throughout the world.', 2, 1, 50, 0, 2, 0, 0, 65940),
('Craft Items', 'Craft items using any profession.', 3, 2, 30, 0, 1, 0, 0, 65940),
('Defeat Enemies', 'Defeat enemies in the world.', 4, 3, 100, 0, 1, 0, 0, 65940),
('Complete Quests', 'Complete quests to aid exploration efforts.', 5, 4, 10, 0, 1, 0, 0, 65940);

-- ============================================================
-- 3. InitiativeXTask — Map tasks to initiatives
-- ============================================================
DELETE FROM `initiative_x_task` WHERE `ID` IN (1, 2, 3, 4, 5);
INSERT INTO `initiative_x_task` (`ID`, `InitiativeTaskID`, `SortOrder`, `NeighborhoodInitiativeID`, `VerifiedBuild`) VALUES
(1, 1, 1, 1, 65940),  -- Gathering: Gather Herbs
(2, 2, 2, 1, 65940),  -- Gathering: Mine Ore
(3, 3, 1, 2, 65940),  -- Crafting: Craft Items
(4, 4, 1, 3, 65940),  -- Combat: Defeat Enemies
(5, 5, 1, 4, 65940);  -- Exploration: Complete Quests

-- ============================================================
-- 4. InitiativeCycle — 1 cycle per initiative (weekly)
-- ============================================================
DELETE FROM `initiative_cycle` WHERE `ID` IN (1, 2, 3, 4);
INSERT INTO `initiative_cycle` (`ID`, `InitiativeID`, `CycleIndex`, `StartDay`, `Duration`, `Flags`, `VerifiedBuild`) VALUES
(1, 1, 0, 0, 604800, 0, 65940),  -- Gathering: 7-day cycle
(2, 2, 0, 0, 604800, 0, 65940),  -- Crafting: 7-day cycle
(3, 3, 0, 0, 604800, 0, 65940),  -- Combat: 7-day cycle
(4, 4, 0, 0, 604800, 0, 65940);  -- Exploration: 7-day cycle

-- ============================================================
-- 5. InitiativeMilestone — 3 milestones per cycle (33%, 66%, 100%)
-- ============================================================
DELETE FROM `initiative_milestone` WHERE `ID` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
INSERT INTO `initiative_milestone` (`ID`, `MilestoneIndex`, `ProgressRequired`, `Flags`, `InitiativeCycleID`, `VerifiedBuild`) VALUES
-- Gathering cycle milestones
(1,  0, 0.33, 0, 1, 65940),
(2,  1, 0.66, 0, 1, 65940),
(3,  2, 1.00, 0, 1, 65940),
-- Crafting cycle milestones
(4,  0, 0.33, 0, 2, 65940),
(5,  1, 0.66, 0, 2, 65940),
(6,  2, 1.00, 0, 2, 65940),
-- Combat cycle milestones
(7,  0, 0.33, 0, 3, 65940),
(8,  1, 0.66, 0, 3, 65940),
(9,  2, 1.00, 0, 3, 65940),
-- Exploration cycle milestones
(10, 0, 0.33, 0, 4, 65940),
(11, 1, 0.66, 0, 4, 65940),
(12, 2, 1.00, 0, 4, 65940);

-- ============================================================
-- 6. InitiativeReward — Rewards for each milestone
-- ============================================================
DELETE FROM `initiative_reward` WHERE `ID` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
INSERT INTO `initiative_reward` (`RewardData`, `Name`, `Description`, `ID`, `RewardType`, `RewardAmount`, `CurrencyID`, `ItemID`, `Flags`, `VerifiedBuild`) VALUES
-- Gathering rewards (favor-based, RewardType=0 with RewardAmount as favor points)
-- IDA-verified: HouseInitiativeFavor is AccountTransType 66, granted via Housing::AddFavor
(0, 'Gathering Milestone 1', 'Reward for reaching 33% of the Gathering Initiative.', 1, 0, 100, 0, 0, 0, 65940),
(0, 'Gathering Milestone 2', 'Reward for reaching 66% of the Gathering Initiative.', 2, 0, 200, 0, 0, 0, 65940),
(0, 'Gathering Milestone 3', 'Reward for completing the Gathering Initiative.',      3, 0, 500, 0, 0, 0, 65940),
-- Crafting rewards
(0, 'Crafting Milestone 1', 'Reward for reaching 33% of the Crafting Initiative.', 4, 0, 100, 0, 0, 0, 65940),
(0, 'Crafting Milestone 2', 'Reward for reaching 66% of the Crafting Initiative.', 5, 0, 200, 0, 0, 0, 65940),
(0, 'Crafting Milestone 3', 'Reward for completing the Crafting Initiative.',      6, 0, 500, 0, 0, 0, 65940),
-- Combat rewards
(0, 'Combat Milestone 1', 'Reward for reaching 33% of the Combat Initiative.', 7, 0, 100, 0, 0, 0, 65940),
(0, 'Combat Milestone 2', 'Reward for reaching 66% of the Combat Initiative.', 8, 0, 200, 0, 0, 0, 65940),
(0, 'Combat Milestone 3', 'Reward for completing the Combat Initiative.',      9, 0, 500, 0, 0, 0, 65940),
-- Exploration rewards
(0, 'Exploration Milestone 1', 'Reward for reaching 33% of the Exploration Initiative.', 10, 0, 100, 0, 0, 0, 65940),
(0, 'Exploration Milestone 2', 'Reward for reaching 66% of the Exploration Initiative.', 11, 0, 200, 0, 0, 0, 65940),
(0, 'Exploration Milestone 3', 'Reward for completing the Exploration Initiative.',      12, 0, 500, 0, 0, 0, 65940);

-- ============================================================
-- 7. InitiativeRewardXMilestone — Link rewards to milestones
-- ============================================================
DELETE FROM `initiative_reward_x_milestone` WHERE `ID` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
INSERT INTO `initiative_reward_x_milestone` (`ID`, `InitiativeRewardID`, `InitiativeMilestoneID`, `VerifiedBuild`) VALUES
(1,  1,  1,  65940),
(2,  2,  2,  65940),
(3,  3,  3,  65940),
(4,  4,  4,  65940),
(5,  5,  5,  65940),
(6,  6,  6,  65940),
(7,  7,  7,  65940),
(8,  8,  8,  65940),
(9,  9,  9,  65940),
(10, 10, 10, 65940),
(11, 11, 11, 65940),
(12, 12, 12, 65940);
