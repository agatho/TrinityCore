-- =============================================================================
-- Housing — characters database
-- =============================================================================
-- Trinity Housing — bundled installation master file
-- Target database: characters
-- Generated: 2026-08-10
--
-- This file aggregates every housing-related SQL file from sql/housing/ in the
-- correct install order. To install, run against the characters database:
--
--     mysql -u <user> -p characters < MASTER_housing_characters.sql
--
-- The individual source files under sql/housing/ remain authoritative; this
-- aggregate is a convenience bundle for testers. Regenerate with the script
-- under sql/housing/master/build.sh after any source edit.
-- =============================================================================

-- ============================================================================
-- Source: sql/housing/housing_schema.sql
-- ============================================================================
-- ============================================================================
-- Housing System Schema - Character Database Tables
-- ============================================================================
--
-- This migration creates all 11 tables required by the WoW Housing system
-- in the character database. The housing system allows players to own and
-- customize houses within neighborhoods, place decorations, configure rooms
-- and fixtures, and participate in community initiatives.
--
-- Tables:
--   1.  character_housing                  - Core house ownership per player
--   2.  character_housing_decor            - Placed decoration instances
--   3.  character_housing_rooms            - Room layout and configuration
--   4.  character_housing_fixtures         - Fixture point assignments
--   5.  character_housing_catalog          - Decor catalog (account-wide unlocks)
--   6.  neighborhoods                      - Neighborhood instances
--   7.  neighborhood_members               - Residents, managers, and owners
--   8.  neighborhood_invites               - Pending neighborhood invitations
--   9.  neighborhood_charters              - Charter creation and tracking
--   10. neighborhood_charter_signatures    - Charter co-signer records
--   11. neighborhood_initiatives           - Community event progress
--   12. neighborhood_initiative_contributions - Per-player contribution tracking
--
-- Constants referenced from HousingDefines.h:
--   MAX_HOUSING_DECOR_PER_ROOM     = 50
--   MAX_HOUSING_ROOMS_PER_HOUSE    = 20
--   MAX_HOUSING_FIXTURES_PER_HOUSE = 10
--   MAX_HOUSING_DYE_SLOTS          = 3
--   MAX_NEIGHBORHOOD_PLOTS         = 16
--   MAX_NEIGHBORHOOD_MANAGERS      = 5
--   MIN_CHARTER_SIGNATURES         = 4
--   INVALID_PLOT_INDEX             = 255
--   HOUSING_MAX_NAME_LENGTH        = 64
--
-- Enum references from HousingDefines.h:
--   NeighborhoodMemberRole:        0=Resident, 1=Manager, 2=Owner
--   NeighborhoodFactionRestriction: 0=None, 1=Horde, 2=Alliance
--   HouseSettingsFlags:            0x01=AllowVisitors, 0x02=NeighborhoodOnly,
--                                  0x04=FriendsOnly, 0x08=Locked
--   HousingInitiativeType:         0=Gathering, 1=Crafting, 2=Combat,
--                                  3=Exploration
--
-- Idempotent: Uses DROP TABLE IF EXISTS before each CREATE TABLE.
-- Engine: InnoDB for transactional safety and foreign key support.
-- Charset: utf8mb4 with utf8mb4_unicode_ci collation.
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 1. character_housing - Core house ownership
--
-- One row per player. Tracks which house type (DB2 entry) the player owns,
-- which neighborhood and plot they belong to, house level, favor currency,
-- and privacy/settings flags (see HouseSettingsFlags enum).
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing`;
CREATE TABLE `character_housing` (
    `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID',
    `houseId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'House DB2 entry ID',
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'FK to neighborhoods.guid',
    `plotIndex` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Plot within neighborhood (0..MAX_NEIGHBORHOOD_PLOTS-1)',
    `houseLevel` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Current upgrade level',
    `favor` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Accumulated favor currency',
    `settingsFlags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Bitmask of HouseSettingsFlags',
    `exteriorLocked` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Whether exterior editing is locked (1=locked, 0=unlocked)',
    `houseSize` TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT 'HousingFixtureSize: 1=Any, 2=Small, 3=Medium, 4=Large',
    `houseType` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'HouseExteriorWmoData DB2 entry ID (architectural style)',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of house creation',
    `posX` FLOAT NOT NULL DEFAULT 0 COMMENT 'House X position on plot',
    `posY` FLOAT NOT NULL DEFAULT 0 COMMENT 'House Y position on plot',
    `posZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'House Z position on plot',
    `facing` FLOAT NOT NULL DEFAULT 0 COMMENT 'House facing angle on plot',
    `houseName` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'Player-set house display name',
    `houseDescription` VARCHAR(256) NOT NULL DEFAULT '' COMMENT 'Player-set house description',
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 2. character_housing_decor - Placed decorations
--
-- Each row represents a single decoration item placed in a player's house.
-- Positions are relative to the room/house coordinate system.
-- Rotation is stored as a quaternion (rotX, rotY, rotZ, rotW).
-- Up to MAX_HOUSING_DYE_SLOTS (3) dye slots per decoration.
-- Composite PK (ownerGuid, id): IDs are unique per player, not globally.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_decor`;
CREATE TABLE `character_housing_decor` (
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `id` BIGINT UNSIGNED NOT NULL COMMENT 'Decor instance ID (unique per owner)',
    `houseDecorId` INT UNSIGNED NOT NULL COMMENT 'HouseDecor DB2 entry ID',
    `posX` FLOAT NOT NULL DEFAULT 0 COMMENT 'X position in room/house coordinates',
    `posY` FLOAT NOT NULL DEFAULT 0 COMMENT 'Y position in room/house coordinates',
    `posZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'Z position in room/house coordinates',
    `rotX` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation X component',
    `rotY` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation Y component',
    `rotZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation Z component',
    `rotW` FLOAT NOT NULL DEFAULT 1 COMMENT 'Quaternion rotation W component',
    `dyeSlot0` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 0',
    `dyeSlot1` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 1',
    `dyeSlot2` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 2',
    `roomGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'FK to character_housing_rooms.id (0 = outdoor/unassigned)',
    `locked` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Whether the decor item is locked in place (1=locked, 0=unlocked)',
    `placementTime` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when decor was placed (for refund window)',
    `sourceType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item',
    `sourceValue` VARCHAR(128) NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)',
    PRIMARY KEY (`ownerGuid`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 3. character_housing_rooms - Room layout
--
-- Each row represents a room placed in a player's house. Rooms occupy
-- numbered slots and can be oriented and optionally mirrored.
-- MAX_HOUSING_ROOMS_PER_HOUSE = 20 rooms per house.
-- Composite PK (ownerGuid, id): IDs are unique per player, not globally.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_rooms`;
CREATE TABLE `character_housing_rooms` (
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `id` BIGINT UNSIGNED NOT NULL COMMENT 'Room instance ID (unique per owner)',
    `houseRoomId` INT UNSIGNED NOT NULL COMMENT 'HouseRoom DB2 entry ID',
    `slotIndex` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Room slot within the house layout',
    `orientation` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Room rotation orientation value',
    `mirrored` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = room layout is mirrored',
    `themeId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Visual theme applied to the room',
    `wallTextureId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'RoomComponentTexture ID for walls',
    `floorTextureId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'RoomComponentTexture ID for floors',
    `ceilingTextureId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'RoomComponentTexture ID for ceilings',
    `colorOverride` INT NOT NULL DEFAULT -1 COMMENT 'Color override for materials (-1 = default)',
    `doorTypeId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Door type for the room',
    `doorSlot` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Door slot index within the room',
    `ceilingTypeId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Ceiling type for the room',
    `ceilingSlot` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Ceiling slot index within the room',
    PRIMARY KEY (`ownerGuid`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 4. character_housing_fixtures - Fixture configuration
--
-- Fixtures are permanent structural elements at predefined points in a house
-- (e.g., door styles, window types, ceiling types). Each fixture point can
-- have one selected option. MAX_HOUSING_FIXTURES_PER_HOUSE = 10.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_fixtures`;
CREATE TABLE `character_housing_fixtures` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique fixture assignment ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `fixturePointId` INT UNSIGNED NOT NULL COMMENT 'Predefined fixture point identifier',
    `fixtureOptionId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Selected fixture option (0 = default)',
    PRIMARY KEY (`id`),
    INDEX `idx_owner` (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 5. character_housing_catalog - Decor catalog (account-wide unlocks)
--
-- Tracks which decorations a player has unlocked and how many they own.
-- Composite primary key on (ownerGuid, houseDecorId) ensures one row per
-- unique decoration type per player.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_catalog`;
CREATE TABLE `character_housing_catalog` (
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID (account-wide tracking)',
    `houseDecorId` INT UNSIGNED NOT NULL COMMENT 'HouseDecor DB2 entry ID',
    `quantity` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Number of this decor owned/available',
    `acquiredTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when first acquired',
    `sourceType` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item',
    `sourceValue` VARCHAR(128) NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)',
    PRIMARY KEY (`ownerGuid`, `houseDecorId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 6. neighborhoods - Neighborhood instances
--
-- A neighborhood is a shared zone where multiple players can have plots.
-- MAX_NEIGHBORHOOD_PLOTS = 16 plots per neighborhood.
-- Names are limited to HOUSING_MAX_NAME_LENGTH (64) characters.
-- Faction restriction uses NeighborhoodFactionRestriction enum values.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhoods`;
CREATE TABLE `neighborhoods` (
    `guid` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique neighborhood instance ID',
    `name` VARCHAR(64) NOT NULL COMMENT 'Neighborhood display name (max HOUSING_MAX_NAME_LENGTH)',
    `neighborhoodMapId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the neighborhood founder/owner',
    `factionRestriction` INT NOT NULL DEFAULT 0 COMMENT 'NeighborhoodFactionRestriction: 0=None, 1=Horde, 2=Alliance',
    `isPublic` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = publicly listed and joinable',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of neighborhood creation',
    `guildId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'M8: owning guild id for guild neighborhoods (0 = not guild-linked)',
    PRIMARY KEY (`guid`),
    INDEX `idx_owner` (`ownerGuid`),
    INDEX `idx_map` (`neighborhoodMapId`),
    INDEX `idx_guild` (`guildId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 7. neighborhood_members - Residents, managers, and owners
--
-- Tracks all members of each neighborhood. The role column uses
-- NeighborhoodMemberRole enum: 0=Resident, 1=Manager, 2=Owner.
-- MAX_NEIGHBORHOOD_MANAGERS = 5.
-- INVALID_PLOT_INDEX (255) means the member has no assigned plot.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_members`;
CREATE TABLE `neighborhood_members` (
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `playerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the member',
    `role` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'NeighborhoodMemberRole: 0=Resident, 1=Manager, 2=Owner',
    `joinTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the player joined',
    `plotIndex` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT 'Assigned plot index (255 = INVALID_PLOT_INDEX, no plot)',
    PRIMARY KEY (`neighborhoodGuid`, `playerGuid`),
    INDEX `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 8. neighborhood_invites - Pending neighborhood invitations
--
-- Stores outstanding invitations. Rows are deleted when the invite is
-- accepted, declined, or expires. Composite PK prevents duplicate invites.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_invites`;
CREATE TABLE `neighborhood_invites` (
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `inviteeGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the invited player',
    `inviterGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the player who sent the invite',
    `inviteTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the invite was sent',
    PRIMARY KEY (`neighborhoodGuid`, `inviteeGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 9. neighborhood_charters - Charter creation and tracking
--
-- Charters are the founding documents for new neighborhoods. A charter must
-- collect MIN_CHARTER_SIGNATURES (4) before it can be turned in to create
-- the neighborhood. Names limited to HOUSING_MAX_NAME_LENGTH (64) chars.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_charters`;
CREATE TABLE `neighborhood_charters` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique charter ID',
    `creatorGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the charter creator',
    `name` VARCHAR(64) NOT NULL COMMENT 'Proposed neighborhood name (max HOUSING_MAX_NAME_LENGTH)',
    `neighborhoodMapId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID for the target map',
    `factionFlags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Faction restriction flags for the neighborhood',
    `isGuild` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = guild-associated neighborhood',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of charter creation',
    PRIMARY KEY (`id`),
    INDEX `idx_creator` (`creatorGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 10. neighborhood_charter_signatures - Charter co-signer records
--
-- Each row records one player's signature on a charter. A charter needs
-- MIN_CHARTER_SIGNATURES (4) signatures before it can be submitted.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_charter_signatures`;
CREATE TABLE `neighborhood_charter_signatures` (
    `charterId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_charters.id',
    `signerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the signer',
    `signTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the signature was made',
    PRIMARY KEY (`charterId`, `signerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 11. neighborhood_initiatives - Community events
--
-- Initiatives are neighborhood-wide cooperative events that members
-- contribute to. Types defined by HousingInitiativeType enum:
-- 0=Gathering, 1=Crafting, 2=Combat, 3=Exploration.
-- Progress is tracked as a float from 0.0 to 1.0 (0% to 100%).
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_initiatives`;
CREATE TABLE `neighborhood_initiatives` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique initiative instance ID',
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `initiativeId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodInitiative DB2 entry ID',
    `startTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the initiative began',
    `progress` FLOAT NOT NULL DEFAULT 0 COMMENT 'Completion progress (0.0 to 1.0)',
    `completed` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = initiative completed',
    PRIMARY KEY (`id`),
    INDEX `idx_neighborhood` (`neighborhoodGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 12. neighborhood_initiative_contributions - Per-player contribution tracking
--
-- Tracks how much each player has contributed to each task within a
-- neighborhood initiative. One row per (initiative, player, task) triple.
-- The UNIQUE index allows INSERT ... ON DUPLICATE KEY UPDATE for upserts.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_initiative_task_progress`;
CREATE TABLE `neighborhood_initiative_task_progress` (
    `initiativeDbId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
    `taskId` INT UNSIGNED NOT NULL COMMENT 'InitiativeTask DB2 entry ID',
    `progress` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Current progress count towards TargetCount',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NOT_STARTED, 1=IN_PROGRESS, 2=COMPLETE',
    PRIMARY KEY (`initiativeDbId`, `taskId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 13. neighborhood_initiative_milestones - Milestone reached/claimed tracking
--
-- Tracks which milestones have been reached for each initiative instance,
-- and whether individual players have claimed their rewards.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_initiative_milestones`;
CREATE TABLE `neighborhood_initiative_milestones` (
    `initiativeDbId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
    `milestoneIndex` INT UNSIGNED NOT NULL COMMENT 'Milestone index (0, 1, 2)',
    `reached` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1 = milestone has been reached',
    `reachedTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when milestone was reached',
    PRIMARY KEY (`initiativeDbId`, `milestoneIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 14. neighborhood_initiative_reward_claims - Per-player reward claim tracking
--
-- Tracks which players have claimed rewards for which milestones.
-- Prevents double-claiming.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_initiative_reward_claims`;
CREATE TABLE `neighborhood_initiative_reward_claims` (
    `initiativeDbId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
    `milestoneIndex` INT UNSIGNED NOT NULL COMMENT 'Milestone index (0, 1, 2)',
    `playerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player character GUID',
    `claimTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when reward was claimed',
    PRIMARY KEY (`initiativeDbId`, `milestoneIndex`, `playerGuid`),
    INDEX `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 15. neighborhood_initiative_contributions - Per-player contribution tracking
--
-- Tracks how much each player has contributed to each task within a
-- neighborhood initiative. One row per (initiative, player, task) triple.
-- The UNIQUE index allows INSERT ... ON DUPLICATE KEY UPDATE for upserts.
-- ---------------------------------------------------------------------------
Drop TABLE IF EXISTS `neighborhood_initiative_contributions`;
CREATE TABLE `neighborhood_initiative_contributions` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `initiativeDbId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
    `playerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player character GUID',
    `taskId` INT UNSIGNED NOT NULL COMMENT 'InitiativeTask DB2 entry ID',
    `amount` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Cumulative contribution to this task',
    `lastUpdated` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of last contribution',
    PRIMARY KEY (`id`),
    UNIQUE INDEX `idx_initiative_player_task` (`initiativeDbId`, `playerGuid`, `taskId`),
    INDEX `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- ============================================================================
-- Source: sql/housing/characters_housing_composite_pk_migration.sql
-- ============================================================================
-- ============================================================================
-- Migration: Change PRIMARY KEY from (id) to (ownerGuid, id)
-- ============================================================================
-- The original schema used `id` as the sole PRIMARY KEY, requiring global
-- uniqueness across ALL players. This caused [1062] Duplicate entry errors
-- when two players' room/decor IDs collided.
--
-- This migration changes to a composite PRIMARY KEY (ownerGuid, id) so that
-- IDs only need to be unique PER PLAYER, not globally.
-- ============================================================================

-- 1. Fix character_housing_decor: change PK from (id) to (ownerGuid, id)
ALTER TABLE `character_housing_decor`
    MODIFY COLUMN `id` BIGINT UNSIGNED NOT NULL COMMENT 'Decor instance ID (unique per owner)',
    DROP PRIMARY KEY,
    ADD PRIMARY KEY (`ownerGuid`, `id`);

-- 2. Fix character_housing_rooms: change PK from (id) to (ownerGuid, id)
ALTER TABLE `character_housing_rooms`
    MODIFY COLUMN `id` BIGINT UNSIGNED NOT NULL COMMENT 'Room instance ID (unique per owner)',
    DROP PRIMARY KEY,
    ADD PRIMARY KEY (`ownerGuid`, `id`);

SELECT 'Schema migration complete: composite primary keys applied.' AS status;


-- ============================================================================
-- Source: sql/housing/characters_housing_base_room_migration.sql
-- ============================================================================
-- ---------------------------------------------------------------------------
-- Migration: Add base room (HouseRoom entry 18) to all existing houses
-- that don't already have one.
--
-- Houses created before the auto-PlaceRoom fix in Housing::Create() have
-- zero rows in character_housing_rooms. Without a base room, entering the
-- interior spawns nothing and decor placement has no Geobox.
--
-- Run ONCE against the characters database. Safe to re-run (idempotent).
-- ---------------------------------------------------------------------------

INSERT INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`,          -- ownerGuid
    18,                 -- houseRoomId (HouseRoom.db2 base room entry)
    0,                  -- slotIndex (base room = slot 0)
    0,                  -- orientation (default facing)
    0,                  -- mirrored (false)
    0,                  -- themeId (default)
    0,                  -- wallpaperId (default)
    0,                  -- materialId (default)
    0,                  -- doorTypeId (default)
    0,                  -- doorSlot (default)
    0,                  -- ceilingTypeId (default)
    0                   -- ceilingSlot (default)
FROM `character_housing` ch
WHERE NOT EXISTS (
    SELECT 1 FROM `character_housing_rooms` cr
    WHERE cr.`ownerGuid` = ch.`guid`
      AND cr.`slotIndex` = 0
);


-- ============================================================================
-- Source: sql/housing/characters_housing_visual_room_migration.sql
-- ============================================================================
-- ---------------------------------------------------------------------------
-- Migration: Add default visual room to all existing houses that only have
-- the base room (entry 18).
--
-- Base room 18 only provides a geobox boundary — it has no visible wall,
-- floor, or ceiling geometry. Players entering the interior would see an
-- empty void. This migration adds the first visual room (the room entry
-- with the most components) into slot 1 for all houses missing one.
--
-- The visual room entry ID must match what GetDefaultVisualRoomEntry()
-- returns at runtime. If the DB2 data changes, update the value below.
-- Currently the first non-base room with >1 component is used.
--
-- Run ONCE against the characters database. Safe to re-run (idempotent).
-- ---------------------------------------------------------------------------

-- Step 1: Ensure every house has a base room in slot 0 (prerequisite)
-- (Uses the same logic as characters_housing_base_room_migration.sql)
INSERT IGNORE INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
FROM `character_housing` ch
WHERE NOT EXISTS (
    SELECT 1 FROM `character_housing_rooms` cr
    WHERE cr.`ownerGuid` = ch.`guid`
      AND cr.`slotIndex` = 0
);

-- Step 2: Add visual room in slot 1 for houses that don't have any
-- non-base room yet. The runtime fixup in LoadFromDB also handles this,
-- but this SQL ensures the data is correct even before the player logs in.
--
-- Visual room entry: Use HouseRoom entry 1 (the "Main Room" with 9 components
-- including walls, floor, ceiling). If entry 1 doesn't exist in your DB2,
-- the runtime fixup will pick the correct entry automatically.
INSERT INTO `character_housing_rooms`
    (`ownerGuid`, `houseRoomId`, `slotIndex`, `orientation`, `mirrored`,
     `themeId`, `wallpaperId`, `materialId`, `doorTypeId`, `doorSlot`,
     `ceilingTypeId`, `ceilingSlot`)
SELECT
    ch.`guid`,          -- ownerGuid
    1,                  -- houseRoomId (visual room with walls/floor/ceiling)
    1,                  -- slotIndex (slot 1, next to base room in slot 0)
    0,                  -- orientation (default facing)
    0,                  -- mirrored (false)
    0,                  -- themeId (default — faction theme applied at runtime)
    0,                  -- wallpaperId (default)
    0,                  -- materialId (default)
    0,                  -- doorTypeId (default)
    0,                  -- doorSlot (default)
    0,                  -- ceilingTypeId (default)
    0                   -- ceilingSlot (default)
FROM `character_housing` ch
WHERE NOT EXISTS (
    SELECT 1 FROM `character_housing_rooms` cr
    WHERE cr.`ownerGuid` = ch.`guid`
      AND cr.`houseRoomId` != 18
);


-- ============================================================================
-- Source: sql/housing/characters_housing_grid_migration.sql
-- ============================================================================
-- Housing: Add 2D grid coordinates and floor index to room placement
-- GridX/GridY store yard offsets from the interior origin.
-- FloorIndex: 0=ground, 1+=upper floors (for stairwell rooms).

ALTER TABLE `character_housing_rooms`
  ADD COLUMN IF NOT EXISTS `gridX` INT NOT NULL DEFAULT 0 AFTER `slotIndex`,
  ADD COLUMN IF NOT EXISTS `gridY` INT NOT NULL DEFAULT 0 AFTER `gridX`,
  ADD COLUMN IF NOT EXISTS `floorIndex` INT NOT NULL DEFAULT 0 AFTER `gridY`;

-- Migrate existing rooms: slotIndex → yard offset (slot * 15), floor 0
UPDATE `character_housing_rooms` SET `gridX` = CAST(`slotIndex` AS SIGNED) * 15, `gridY` = 0, `floorIndex` = 0;
-- Entry room (slot 0) stays at gridX=0
UPDATE `character_housing_rooms` SET `gridX` = 0 WHERE `slotIndex` = 0;


-- ============================================================================
-- Source: sql/housing/characters_housing_racial_style_migration.sql
-- ============================================================================
-- Migrate existing houses to racial WMO styles.
-- Previously all Alliance houses got WMO=9 (Human) and Horde got WMO=87 (Orc).
-- Now: Night Elf → 55 (Woodland), Blood Elf → 56 (Engraved).

-- Night Elf (race=4): Alliance default 9 → Woodland 55
UPDATE character_housing ch
INNER JOIN characters c ON ch.guid = c.guid
SET ch.houseType = 55
WHERE c.race = 4 AND ch.houseType = 9;

-- Blood Elf (race=10): Horde default 87 → Engraved 56
UPDATE character_housing ch
INNER JOIN characters c ON ch.guid = c.guid
SET ch.houseType = 56
WHERE c.race = 10 AND ch.houseType = 87;


-- ============================================================================
-- Source: sql/housing/characters_housing_texture_migration.sql
-- ============================================================================
-- Migration: Rename wallpaperId/materialId to per-component-type texture fields
-- Date: 2026-04-13
-- Reason: Sniff analysis shows wall/floor/ceiling textures are stored independently.
--         Old single wallpaperId applied to all surfaces; now we have per-type fields.
--
-- Old columns: wallpaperId (wall texture), materialId (was misused as shared texture)
-- New columns: wallTextureId, floorTextureId, ceilingTextureId, colorOverride
--
-- Compatible with MySQL 9.4 (no IF NOT EXISTS on columns).
-- Safe to run multiple times: uses stored procedure with column existence checks.

DELIMITER //

DROP PROCEDURE IF EXISTS `housing_texture_migration`//
CREATE PROCEDURE `housing_texture_migration`()
BEGIN
    -- Step 1: Add new columns if they don't exist
    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'wallTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `wallTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `themeId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'floorTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `floorTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `wallTextureId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'ceilingTextureId') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `ceilingTextureId` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `floorTextureId`;
    END IF;

    IF NOT EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'colorOverride') THEN
        ALTER TABLE `character_housing_rooms`
            ADD COLUMN `colorOverride` INT NOT NULL DEFAULT -1 AFTER `ceilingTextureId`;
    END IF;

    -- Step 2: Migrate old data if old columns still exist
    -- wallpaperId was misread (got ColorOverride=-1 due to field-swap bug)
    -- materialId accidentally captured the real texture ID
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'materialId') THEN
        UPDATE `character_housing_rooms`
            SET `wallTextureId` = `materialId`
            WHERE `wallTextureId` = 0 AND `materialId` > 0;
    END IF;

    -- Step 3: Drop old columns if they exist
    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'wallpaperId') THEN
        ALTER TABLE `character_housing_rooms` DROP COLUMN `wallpaperId`;
    END IF;

    IF EXISTS (SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'character_housing_rooms' AND COLUMN_NAME = 'materialId') THEN
        ALTER TABLE `character_housing_rooms` DROP COLUMN `materialId`;
    END IF;
END//

DELIMITER ;

CALL `housing_texture_migration`();
DROP PROCEDURE IF EXISTS `housing_texture_migration`;


-- ============================================================================
-- Source: sql/housing/characters_housing_per_surface_theme.sql
-- ============================================================================
-- Per-surface theme columns for character_housing_rooms.
--
-- Previously a single themeId column was shared by walls/floors/ceilings,
-- so dyeing the ceiling's style overwrote the wall's style. Split into
-- three independent theme IDs. New rows default to 0 and the load path
-- seeds them from the legacy themeId so existing houses keep their look.

ALTER TABLE character_housing_rooms
    ADD COLUMN wallThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER ceilingSlot,
    ADD COLUMN floorThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER wallThemeId,
    ADD COLUMN ceilingThemeId INT UNSIGNED NOT NULL DEFAULT 0 AFTER floorThemeId;


-- ============================================================================
-- Source: sql/housing/characters_housing_decor_placement_time.sql
-- ============================================================================
-- Add placement_time column to character_housing_decor
-- Tracks when each decoration was placed, used for the refund window (2 hours)
-- Existing rows default to 0 (no refund eligibility for pre-existing placements)

ALTER TABLE `character_housing_decor`
    ADD COLUMN `placementTime` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when decor was placed (for refund window)'
    AFTER `locked`;


-- ============================================================================
-- Source: sql/housing/characters_housing_decor_scale.sql
-- ============================================================================
-- Add scale column to character_housing_decor for Advanced Mode decor scaling
-- Default 1.0 = original size. Sniff shows values like 0.45 to 1.62.
ALTER TABLE `character_housing_decor`
    ADD COLUMN `scale` FLOAT NOT NULL DEFAULT 1.0 AFTER `rotW`;


-- ============================================================================
-- Source: sql/housing/characters_neighborhood_guild_link.sql
-- ============================================================================
-- ---------------------------------------------------------------------------
-- 2026-08-09  M8: guild -> neighborhood link
-- Adds neighborhoods.guildId so guild neighborhoods created via
-- CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD persist their owning guild id and
-- NeighborhoodMgr::GetNeighborhoodByGuildId resolves after a server restart.
-- Idempotent: safe to re-run.
-- ---------------------------------------------------------------------------

SET @dbname := DATABASE();

-- Add the column only if it does not already exist.
SET @col_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = @dbname AND TABLE_NAME = 'neighborhoods' AND COLUMN_NAME = 'guildId'
);
SET @ddl := IF(@col_exists = 0,
    "ALTER TABLE `neighborhoods` ADD COLUMN `guildId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'M8: owning guild id for guild neighborhoods (0 = not guild-linked)' AFTER `createTime`",
    "SELECT 'neighborhoods.guildId already present'");
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Add the lookup index only if it does not already exist.
SET @idx_exists := (
    SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = @dbname AND TABLE_NAME = 'neighborhoods' AND INDEX_NAME = 'idx_guild'
);
SET @ddl := IF(@idx_exists = 0,
    "ALTER TABLE `neighborhoods` ADD INDEX `idx_guild` (`guildId`)",
    "SELECT 'neighborhoods.idx_guild already present'");
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;


-- ============================================================================
-- Source: sql/housing/characters_neighborhood_map_faction_fix.sql
-- ============================================================================
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

