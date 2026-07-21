-- Playerbot V2 shared database - schema only.
--
-- Dumped from the playerbot test environment, so this is the real DDL rather than a
-- reconstruction. Import into the database named by the worldserver config key
-- `Playerbot.SharedDatabase` (the test environment uses `wowc_playerbot`, NOT the
-- module default `playerbot`). This database is NOT managed by the TrinityCore SQL
-- updater - see README.md in this directory.
--
-- No credentials, no data, and no backup/scratch tables are included.

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;


/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_ability` (
  `abilityId` int unsigned NOT NULL COMMENT 'Ability ID',
  `name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Ability name',
  `petType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Pet type (0=Humanoid, 1=Dragonkin, etc.)',
  `baseDamage` int unsigned NOT NULL DEFAULT '0' COMMENT 'Base damage value',
  `cooldownDuration` int unsigned NOT NULL DEFAULT '0' COMMENT 'Cooldown in rounds',
  `flags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Ability flags (bit 0=multi-turn)',
  `effectCount` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Number of effects',
  `visualId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Visual effect ID',
  PRIMARY KEY (`abilityId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_species` (
  `speciesId` int unsigned NOT NULL COMMENT 'Battle Pet Species ID',
  `creatureId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Reference to creature_template.entry',
  `petType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Pet family: 0=Humanoid, 1=Dragonkin, 2=Flying, 3=Undead, 4=Critter, 5=Magic, 6=Elemental, 7=Beast, 8=Aquatic, 9=Mechanical',
  `flags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Flags: 0x1=Capturable, 0x2=Tradeable, 0x4=Rare, 0x8=Epic, 0x10=Legendary, 0x20=Untradeable',
  `source` varchar(50) DEFAULT '' COMMENT 'How pet is obtained: wild, vendor, quest, achievement, drop, etc.',
  `description` varchar(255) DEFAULT '' COMMENT 'Optional description',
  PRIMARY KEY (`speciesId`),
  KEY `idx_creature_id` (`creatureId`),
  KEY `idx_pet_type` (`petType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Battle pet species metadata for bot pet selection';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battle_pet_species_abilities` (
  `speciesId` int unsigned NOT NULL COMMENT 'Battle pet species ID',
  `abilityId1` int unsigned NOT NULL DEFAULT '0' COMMENT 'First ability slot',
  `abilityId2` int unsigned NOT NULL DEFAULT '0' COMMENT 'Second ability slot',
  `abilityId3` int unsigned NOT NULL DEFAULT '0' COMMENT 'Third ability slot',
  `abilityId4` int unsigned NOT NULL DEFAULT '0' COMMENT 'Fourth ability slot (alternate)',
  `abilityId5` int unsigned NOT NULL DEFAULT '0' COMMENT 'Fifth ability slot (alternate)',
  `abilityId6` int unsigned NOT NULL DEFAULT '0' COMMENT 'Sixth ability slot (alternate)',
  PRIMARY KEY (`speciesId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `handcrafted_road` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `mapId` int unsigned NOT NULL,
  `fromX` float NOT NULL,
  `fromY` float NOT NULL,
  `toX` float NOT NULL,
  `toY` float NOT NULL,
  `width` float NOT NULL DEFAULT '8',
  `comment` varchar(255) DEFAULT NULL,
  `verified` tinyint unsigned NOT NULL DEFAULT '0',
  `created_by` varchar(64) DEFAULT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_map` (`mapId`)
) ENGINE=InnoDB AUTO_INCREMENT=969 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_accounts` (
  `account_id` int unsigned NOT NULL,
  `account_name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `email` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `is_bot_account` tinyint(1) NOT NULL DEFAULT '1',
  `bot_count` tinyint unsigned NOT NULL DEFAULT '0',
  `max_bots` tinyint unsigned NOT NULL DEFAULT '10',
  `account_status` enum('ACTIVE','SUSPENDED','DISABLED') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'ACTIVE',
  `last_activity` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`account_id`),
  UNIQUE KEY `uk_account_name` (`account_name`),
  UNIQUE KEY `uk_email` (`email`),
  KEY `idx_bot_accounts` (`is_bot_account`,`account_status`),
  KEY `idx_activity` (`last_activity`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Bot account management';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_activity_patterns` (
  `pattern_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `display_name` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `is_system_pattern` tinyint(1) NOT NULL DEFAULT '0',
  `login_chance` float NOT NULL DEFAULT '1',
  `logout_chance` float NOT NULL DEFAULT '1',
  `min_session_duration` int unsigned NOT NULL DEFAULT '1800',
  `max_session_duration` int unsigned NOT NULL DEFAULT '7200',
  `min_offline_duration` int unsigned NOT NULL DEFAULT '3600',
  `max_offline_duration` int unsigned NOT NULL DEFAULT '28800',
  `activity_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`pattern_name`),
  KEY `idx_system_patterns` (`is_system_pattern`,`enabled`),
  KEY `idx_pattern_weight` (`activity_weight`,`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_bot_templates` (
  `template_id` int unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique template ID',
  `spec_id` int unsigned NOT NULL COMMENT 'Specialization ID (FK to spec_info)',
  `class_id` tinyint unsigned NOT NULL COMMENT 'Denormalized class ID for fast queries',
  `role` tinyint unsigned NOT NULL DEFAULT '2' COMMENT 'Combat role: 0=Tank, 1=Healer, 2=DPS',
  `template_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Human-readable name (e.g., Warrior_Arms)',
  `version` int unsigned NOT NULL DEFAULT '1' COMMENT 'Template version for updates',
  `patch_version` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '11.2.0' COMMENT 'WoW patch this template is for',
  `enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether template is active',
  `validated` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Whether template has been validated',
  `last_validated` timestamp NULL DEFAULT NULL COMMENT 'Last validation timestamp',
  `talent_blob` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT 'Hex-encoded serialized talent data',
  `actionbar_blob` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT 'Hex-encoded serialized action bar data',
  `default_pvp_talents` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Use PvP talent defaults',
  `hero_talent_tree_id` int unsigned DEFAULT NULL COMMENT 'Default hero talent tree',
  `priority_weight` int unsigned NOT NULL DEFAULT '100' COMMENT 'Selection priority weight',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`template_id`),
  UNIQUE KEY `uk_spec_id` (`spec_id`),
  KEY `idx_enabled` (`enabled`),
  KEY `idx_validated` (`validated`),
  KEY `idx_class_role` (`class_id`,`role`),
  CONSTRAINT `fk_template_spec` FOREIGN KEY (`spec_id`) REFERENCES `playerbot_spec_info` (`spec_id`) ON DELETE CASCADE,
  CONSTRAINT `chk_valid_class_id` CHECK (((`class_id` >= 1) and (`class_id` <= 13))),
  CONSTRAINT `chk_valid_role` CHECK (((`role` >= 0) and (`role` <= 2))),
  CONSTRAINT `chk_valid_spec_id` CHECK ((`spec_id` > 0)),
  CONSTRAINT `chk_valid_template_name` CHECK ((length(`template_name`) > 0))
) ENGINE=InnoDB AUTO_INCREMENT=40 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Main bot template registry';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trg_bot_templates_before_insert` BEFORE INSERT ON `playerbot_bot_templates` FOR EACH ROW BEGIN

    -- Validate class_id (WoW classes are 1-13)

    IF NEW.class_id < 1 OR NEW.class_id > 13 THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'Invalid class_id: must be between 1 and 13 (valid WoW class IDs)';

    END IF;



    -- Validate spec_id exists in spec_info

    IF NOT EXISTS (SELECT 1 FROM playerbot_spec_info WHERE spec_id = NEW.spec_id) THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'Invalid spec_id: must reference a valid entry in playerbot_spec_info';

    END IF;



    -- Validate class_id matches spec_info (critical: prevents mismatched class/spec)

    IF NOT EXISTS (SELECT 1 FROM playerbot_spec_info

                   WHERE spec_id = NEW.spec_id AND class_id = NEW.class_id) THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'class_id does not match the class for the given spec_id in playerbot_spec_info';

    END IF;

END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trg_bot_templates_before_update` BEFORE UPDATE ON `playerbot_bot_templates` FOR EACH ROW BEGIN

    -- Same validations as insert

    IF NEW.class_id < 1 OR NEW.class_id > 13 THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'Invalid class_id: must be between 1 and 13 (valid WoW class IDs)';

    END IF;



    IF NOT EXISTS (SELECT 1 FROM playerbot_spec_info WHERE spec_id = NEW.spec_id) THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'Invalid spec_id: must reference a valid entry in playerbot_spec_info';

    END IF;



    IF NOT EXISTS (SELECT 1 FROM playerbot_spec_info

                   WHERE spec_id = NEW.spec_id AND class_id = NEW.class_id) THEN

        SIGNAL SQLSTATE '45000'

        SET MESSAGE_TEXT = 'class_id does not match the class for the given spec_id in playerbot_spec_info';

    END IF;

END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_bracket_statistics` (
  `bracket_id` tinyint unsigned NOT NULL COMMENT 'Bracket ID (0-3)',
  `bracket_name` varchar(32) NOT NULL COMMENT 'Bracket name for reference',
  `avg_time_in_bracket_seconds` int unsigned NOT NULL DEFAULT '0' COMMENT 'Average time bots spend in this bracket',
  `median_time_in_bracket_seconds` int unsigned NOT NULL DEFAULT '0' COMMENT 'Median time in bracket',
  `min_time_in_bracket_seconds` int unsigned NOT NULL DEFAULT '0' COMMENT 'Minimum observed time',
  `max_time_in_bracket_seconds` int unsigned NOT NULL DEFAULT '0' COMMENT 'Maximum observed time',
  `sample_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Number of transitions sampled',
  `outflow_rate_per_hour` float NOT NULL DEFAULT '0' COMMENT 'Average bots leaving bracket per hour',
  `inflow_rate_per_hour` float NOT NULL DEFAULT '0' COMMENT 'Average bots entering bracket per hour',
  `last_updated` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last calculation time',
  PRIMARY KEY (`bracket_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Aggregate bracket flow statistics';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_bracket_transitions` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique transition ID',
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot character GUID',
  `from_bracket` tinyint unsigned NOT NULL COMMENT 'Source bracket (0=Starting, 1=Chromie, 2=DF, 3=TWW)',
  `to_bracket` tinyint unsigned NOT NULL COMMENT 'Destination bracket',
  `from_level` tinyint unsigned NOT NULL COMMENT 'Level before transition',
  `to_level` tinyint unsigned NOT NULL COMMENT 'Level after transition',
  `transition_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When transition occurred',
  `time_in_bracket_seconds` int unsigned NOT NULL COMMENT 'Seconds spent in source bracket',
  PRIMARY KEY (`id`),
  KEY `idx_bot_guid` (`bot_guid`),
  KEY `idx_from_bracket` (`from_bracket`),
  KEY `idx_to_bracket` (`to_bracket`),
  KEY `idx_transition_time` (`transition_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Bot level bracket transition history';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_character_distribution` (
  `race` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `faction` tinyint unsigned NOT NULL,
  `target_percentage` float NOT NULL DEFAULT '0',
  `current_count` int unsigned NOT NULL DEFAULT '0',
  `max_count` int unsigned NOT NULL DEFAULT '1000',
  `is_enabled` tinyint(1) NOT NULL DEFAULT '1',
  `priority_weight` float NOT NULL DEFAULT '1',
  `last_created` timestamp NULL DEFAULT NULL,
  `created_today` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`,`class`),
  KEY `idx_faction` (`faction`,`is_enabled`),
  KEY `idx_distribution` (`target_percentage`,`current_count`),
  KEY `idx_priority` (`priority_weight`,`is_enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Character race/class distribution control';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_class_race_matrix` (
  `class_id` tinyint unsigned NOT NULL COMMENT 'ChrClasses.db2 ID',
  `race_id` tinyint unsigned NOT NULL COMMENT 'ChrRaces.db2 ID',
  `race_name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Race name (English)',
  `faction` enum('ALLIANCE','HORDE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Faction',
  `weight` float NOT NULL DEFAULT '1' COMMENT 'Selection weight (popularity)',
  `enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether to use for bot creation',
  PRIMARY KEY (`class_id`,`race_id`),
  KEY `idx_faction` (`faction`),
  KEY `idx_class_faction` (`class_id`,`faction`),
  KEY `idx_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Valid class/race combinations for WoW 11.2';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_content_requirements` (
  `content_id` int unsigned NOT NULL COMMENT 'Dungeon/Raid/BG/Arena ID',
  `content_type` enum('DUNGEON','RAID','BATTLEGROUND','ARENA') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Type of content',
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Difficulty (0=Normal, 1=Heroic, 2=Mythic, etc.)',
  `content_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Human-readable name',
  `min_players` int unsigned NOT NULL DEFAULT '1' COMMENT 'Minimum players',
  `max_players` int unsigned NOT NULL DEFAULT '5' COMMENT 'Maximum players',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Minimum level',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80' COMMENT 'Maximum level',
  `min_tanks` int unsigned NOT NULL DEFAULT '1',
  `max_tanks` int unsigned NOT NULL DEFAULT '1',
  `min_healers` int unsigned NOT NULL DEFAULT '1',
  `max_healers` int unsigned NOT NULL DEFAULT '1',
  `min_dps` int unsigned NOT NULL DEFAULT '3',
  `max_dps` int unsigned NOT NULL DEFAULT '3',
  `min_gear_score` int unsigned NOT NULL DEFAULT '0',
  `recommended_gear_score` int unsigned NOT NULL DEFAULT '0',
  `requires_both_factions` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Whether both factions needed',
  `players_per_faction` int unsigned NOT NULL DEFAULT '0' COMMENT 'Players per faction for PvP',
  `estimated_duration_minutes` int unsigned NOT NULL DEFAULT '30' COMMENT 'Expected duration',
  PRIMARY KEY (`content_id`,`content_type`,`difficulty`),
  KEY `idx_content_type` (`content_type`),
  KEY `idx_level_range` (`min_level`,`max_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Instance Bot Pool - Content requirements for dungeons/raids/BGs';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_dragonriding_glyph_templates` (
  `glyph_id` int unsigned NOT NULL COMMENT 'Unique glyph ID',
  `map_id` int unsigned NOT NULL COMMENT 'Map ID (2444=Dragon Isles)',
  `zone_id` int unsigned NOT NULL COMMENT 'Zone/area ID',
  `zone_name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Zone name for reference',
  `pos_x` float NOT NULL COMMENT 'X coordinate',
  `pos_y` float NOT NULL COMMENT 'Y coordinate',
  `pos_z` float NOT NULL COMMENT 'Z coordinate',
  `collection_radius` float NOT NULL DEFAULT '10' COMMENT 'Radius to collect glyph',
  `achievement_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Achievement granted on collection',
  `name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Glyph name/description',
  PRIMARY KEY (`glyph_id`),
  KEY `idx_map_zone` (`map_id`,`zone_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Dragon Glyph world locations';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_dragonriding_glyphs` (
  `account_id` bigint unsigned NOT NULL COMMENT 'Account ID (links to auth.account)',
  `glyph_id` int unsigned NOT NULL COMMENT 'Unique glyph ID (1-72+)',
  `collected_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When the glyph was collected',
  `character_guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character that collected it (for logging)',
  `map_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Map where collected',
  `zone_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Zone where collected',
  PRIMARY KEY (`account_id`,`glyph_id`),
  KEY `idx_account` (`account_id`),
  KEY `idx_collected_time` (`collected_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Account-wide Dragon Glyph collection (retail-accurate)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_dragonriding_talent_templates` (
  `talent_id` int unsigned NOT NULL COMMENT 'Unique talent ID (DragonridingTalent enum)',
  `name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Talent display name',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT 'Talent description text',
  `branch` enum('vigor_capacity','regen_grounded','regen_flying','abilities','utility') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Talent tree branch',
  `tier` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Order in branch (1, 2, 3)',
  `glyph_cost` int unsigned NOT NULL COMMENT 'Dragon Glyphs required to learn',
  `prerequisite_talent_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Must have this talent first (0=none)',
  `effect_type` enum('vigor_max','regen_grounded','regen_flying','regen_ground_skim','ability_unlock') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'What this talent modifies',
  `effect_value` int NOT NULL COMMENT 'New value (vigor count, ms regen, or spell ID)',
  PRIMARY KEY (`talent_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Dragonriding talent definitions (retail-accurate values)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_dragonriding_talents` (
  `account_id` bigint unsigned NOT NULL COMMENT 'Account ID (links to auth.account)',
  `talent_id` int unsigned NOT NULL COMMENT 'Talent ID from DragonridingTalent enum',
  `learned_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When talent was learned',
  `glyphs_spent` int unsigned NOT NULL DEFAULT '0' COMMENT 'Glyphs spent on this talent',
  PRIMARY KEY (`account_id`,`talent_id`),
  KEY `idx_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Account-wide Dragonriding talent selections (retail-accurate)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_dungeon_routes` (
  `map_id` smallint unsigned NOT NULL,
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `seq` smallint unsigned NOT NULL,
  `position_x` float NOT NULL,
  `position_y` float NOT NULL,
  `position_z` float NOT NULL,
  PRIMARY KEY (`map_id`,`difficulty`,`seq`),
  KEY `map_idx` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Auto-generated dungeon route waypoints (entrance->bosses navmesh chain) for PlayerbotV2 far-boss advance';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_friend_references` (
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot character GUID',
  `player_guid` bigint unsigned NOT NULL COMMENT 'Player character GUID who has bot as friend',
  `added_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When friendship was established',
  PRIMARY KEY (`bot_guid`,`player_guid`),
  KEY `idx_player_guid` (`player_guid`),
  KEY `idx_bot_guid` (`bot_guid`),
  KEY `idx_added_at` (`added_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Player-to-bot friend list references';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_instance_pool` (
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot character GUID',
  `account_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Account ID the bot belongs to',
  `bot_name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Character name',
  `pool_type` enum('PVE','PVP_ALLIANCE','PVP_HORDE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'PVE' COMMENT 'Pool type',
  `role` enum('TANK','HEALER','DPS') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'DPS' COMMENT 'Combat role',
  `player_class` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'WoW class ID',
  `spec_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Specialization ID',
  `faction` enum('ALLIANCE','HORDE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'ALLIANCE' COMMENT 'Character faction',
  `level` tinyint unsigned NOT NULL DEFAULT '80' COMMENT 'Character level',
  `bracket` tinyint unsigned NOT NULL DEFAULT '7' COMMENT 'Level bracket (0=10-19, 1=20-29, ..., 7=80+)',
  `is_warm_pool` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'True if this is a warm pool bot (persists across restarts)',
  `gear_score` int unsigned NOT NULL DEFAULT '0' COMMENT 'Item level / gear score',
  `health_max` int unsigned NOT NULL DEFAULT '0' COMMENT 'Max health',
  `mana_max` int unsigned NOT NULL DEFAULT '0' COMMENT 'Max mana',
  `slot_state` enum('EMPTY','CREATING','WARMING','READY','RESERVED','ASSIGNED','COOLDOWN','MAINTENANCE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'READY' COMMENT 'Current slot state',
  `state_change_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When state last changed',
  `last_assignment` timestamp NULL DEFAULT NULL COMMENT 'When last assigned to instance',
  `current_instance_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current instance ID (0 if not assigned)',
  `current_content_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current dungeon/raid/bg ID',
  `current_instance_type` enum('DUNGEON','RAID','BATTLEGROUND','ARENA') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT 'DUNGEON' COMMENT 'Type of current instance',
  `reservation_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current reservation ID (0 if not reserved)',
  `assignment_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total lifetime assignments',
  `total_instance_time` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total seconds spent in instances',
  `successful_completions` int unsigned NOT NULL DEFAULT '0' COMMENT 'Instances completed successfully',
  `early_exits` int unsigned NOT NULL DEFAULT '0' COMMENT 'Times removed before completion',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When bot was added to pool',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last update time',
  PRIMARY KEY (`bot_guid`),
  KEY `idx_pool_type` (`pool_type`),
  KEY `idx_role` (`role`),
  KEY `idx_pool_role` (`pool_type`,`role`),
  KEY `idx_slot_state` (`slot_state`),
  KEY `idx_faction` (`faction`),
  KEY `idx_level` (`level`),
  KEY `idx_faction_role_state` (`faction`,`role`,`slot_state`),
  KEY `idx_assignment_count` (`assignment_count`),
  KEY `idx_pool_ready_faction_role` (`slot_state`,`faction`,`role`),
  KEY `idx_bracket_faction_role` (`bracket`,`faction`,`role`,`is_warm_pool`),
  KEY `idx_warm_pool` (`is_warm_pool`,`slot_state`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Instance Bot Pool - Main registry of warm pool bots';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_jit_bots` (
  `bot_guid` bigint unsigned NOT NULL COMMENT 'JIT bot character GUID',
  `account_id` int unsigned NOT NULL COMMENT 'Account ID the bot was created on',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When the bot was created',
  `instance_type` enum('DUNGEON','RAID','BATTLEGROUND','ARENA') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT 'DUNGEON' COMMENT 'Type of instance this bot was created for',
  `request_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Original request ID that triggered creation',
  PRIMARY KEY (`bot_guid`),
  KEY `idx_account_id` (`account_id`),
  KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Tracks JIT-created bots for targeted cleanup (preserves BotSpawner bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_lifecycle_events` (
  `event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `event_timestamp` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `event_category` enum('SCHEDULER','SPAWNER','SESSION','DATABASE','SYSTEM','ERROR') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `event_type` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Specific event type',
  `severity` enum('DEBUG','INFO','WARNING','ERROR','CRITICAL') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'INFO',
  `component` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Component that generated event',
  `bot_guid` int unsigned DEFAULT NULL COMMENT 'Related bot (if applicable)',
  `account_id` int unsigned DEFAULT NULL COMMENT 'Related account (if applicable)',
  `message` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Human-readable event description',
  `details` json DEFAULT NULL COMMENT 'Structured event data',
  `execution_time_ms` int unsigned DEFAULT NULL COMMENT 'Time to process operation',
  `memory_usage_mb` float DEFAULT NULL COMMENT 'Memory usage at time of event',
  `active_bots_count` int unsigned DEFAULT NULL COMMENT 'Number of active bots',
  `correlation_id` varchar(36) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'UUID for related events',
  `parent_event_id` bigint unsigned DEFAULT NULL COMMENT 'Parent event reference',
  PRIMARY KEY (`event_id`),
  KEY `idx_timestamp` (`event_timestamp`),
  KEY `idx_category_type` (`event_category`,`event_type`),
  KEY `idx_severity` (`severity`,`event_timestamp`),
  KEY `idx_component` (`component`,`event_timestamp`),
  KEY `idx_bot_events` (`bot_guid`,`event_timestamp`),
  KEY `idx_correlation` (`correlation_id`),
  KEY `idx_parent_child` (`parent_event_id`),
  CONSTRAINT `playerbot_lifecycle_events_ibfk_1` FOREIGN KEY (`parent_event_id`) REFERENCES `playerbot_lifecycle_events` (`event_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='System-wide lifecycle events for monitoring and debugging';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_lifecycle_metrics` (
  `date` date NOT NULL COMMENT 'Metrics date',
  `hour` tinyint unsigned NOT NULL COMMENT 'Hour of day (0-23)',
  `total_bots` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total bot count',
  `protected_bots` int unsigned NOT NULL DEFAULT '0' COMMENT 'Protected bot count',
  `bots_created` int unsigned NOT NULL DEFAULT '0' COMMENT 'Bots created this hour',
  `bots_retired` int unsigned NOT NULL DEFAULT '0' COMMENT 'Bots retired this hour',
  `bots_rescued` int unsigned NOT NULL DEFAULT '0' COMMENT 'Bots rescued from retirement',
  `bracket_0_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Starting bracket count',
  `bracket_1_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'ChromieTime bracket count',
  `bracket_2_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Dragonflight bracket count',
  `bracket_3_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'TheWarWithin bracket count',
  `avg_player_count` float NOT NULL DEFAULT '0' COMMENT 'Average player count',
  `peak_player_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Peak player count',
  PRIMARY KEY (`date`,`hour`),
  KEY `idx_date` (`date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Hourly lifecycle metrics for monitoring';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_migrations` (
  `version` varchar(20) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `applied_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `checksum` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `execution_time_ms` int unsigned DEFAULT '0',
  PRIMARY KEY (`version`),
  KEY `idx_applied` (`applied_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Database migration tracking';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_nav_links` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `map_id` smallint unsigned NOT NULL,
  `from_x` float NOT NULL,
  `from_y` float NOT NULL,
  `from_z` float NOT NULL,
  `to_x` float NOT NULL,
  `to_y` float NOT NULL,
  `to_z` float NOT NULL,
  `radius` float NOT NULL DEFAULT '12' COMMENT 'how close (3D, yards) a bot must be to a mouth to use the link',
  `bidirectional` tinyint unsigned NOT NULL DEFAULT '1',
  `kind` varchar(16) NOT NULL DEFAULT 'jump' COMMENT 'jump | walk (semantic only; both execute as a direct move)',
  `comment` varchar(255) DEFAULT NULL,
  `verified` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'only verified=1 rows are loaded',
  `created_by` varchar(64) DEFAULT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `map_idx` (`map_id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='PlayerbotV2 human-verified traversal links (behavioral off-mesh alternative; see DungeonScript.h NavLink)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_player_activity` (
  `player_guid` bigint unsigned NOT NULL COMMENT 'Player character GUID',
  `zone_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current zone ID',
  `map_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current map ID',
  `level` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Player level',
  `bracket_id` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Level bracket (0-3)',
  `last_update` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last activity update',
  `session_start` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When player logged in',
  `is_active` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether player is currently active',
  PRIMARY KEY (`player_guid`),
  KEY `idx_zone_id` (`zone_id`),
  KEY `idx_level` (`level`),
  KEY `idx_bracket_id` (`bracket_id`),
  KEY `idx_is_active` (`is_active`),
  KEY `idx_last_update` (`last_update`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Real player activity tracking for demand calculation';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_pool_assignments` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique assignment ID',
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot that was assigned',
  `instance_type` enum('DUNGEON','RAID','BATTLEGROUND','ARENA') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Type of instance',
  `instance_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Map instance ID',
  `content_id` int unsigned NOT NULL COMMENT 'Dungeon/Raid/BG ID',
  `bot_role` enum('TANK','HEALER','DPS') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Bot role at assignment',
  `bot_level` tinyint unsigned NOT NULL COMMENT 'Bot level at assignment',
  `bot_gear_score` int unsigned NOT NULL DEFAULT '0' COMMENT 'Bot gear score at assignment',
  `bot_faction` enum('ALLIANCE','HORDE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Bot faction',
  `assigned_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When assignment started',
  `released_at` timestamp NULL DEFAULT NULL COMMENT 'When bot was released',
  `duration_seconds` int unsigned DEFAULT NULL COMMENT 'Total time in instance',
  `completion_status` enum('IN_PROGRESS','SUCCESS','EARLY_EXIT','ERROR','TIMEOUT') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'IN_PROGRESS' COMMENT 'How the assignment ended',
  `reservation_id` int unsigned DEFAULT NULL COMMENT 'Reservation ID if pre-reserved',
  PRIMARY KEY (`id`),
  KEY `idx_bot_guid` (`bot_guid`),
  KEY `idx_instance_type` (`instance_type`),
  KEY `idx_content_id` (`content_id`),
  KEY `idx_instance` (`instance_type`,`instance_id`),
  KEY `idx_assigned_at` (`assigned_at`),
  KEY `idx_completion_status` (`completion_status`),
  KEY `idx_assignments_recent` (`assigned_at`),
  CONSTRAINT `playerbot_pool_assignments_ibfk_1` FOREIGN KEY (`bot_guid`) REFERENCES `playerbot_instance_pool` (`bot_guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Instance Bot Pool - Assignment history for analytics';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_pool_config` (
  `config_key` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Configuration key',
  `config_value` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Configuration value',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Description of the setting',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`config_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Instance Bot Pool - Runtime configuration';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_pool_statistics` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique snapshot ID',
  `snapshot_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When snapshot was taken',
  `total_slots` int unsigned NOT NULL DEFAULT '0',
  `empty_slots` int unsigned NOT NULL DEFAULT '0',
  `creating_slots` int unsigned NOT NULL DEFAULT '0',
  `warming_slots` int unsigned NOT NULL DEFAULT '0',
  `ready_slots` int unsigned NOT NULL DEFAULT '0',
  `reserved_slots` int unsigned NOT NULL DEFAULT '0',
  `assigned_slots` int unsigned NOT NULL DEFAULT '0',
  `cooldown_slots` int unsigned NOT NULL DEFAULT '0',
  `maintenance_slots` int unsigned NOT NULL DEFAULT '0',
  `ready_tanks` int unsigned NOT NULL DEFAULT '0',
  `ready_healers` int unsigned NOT NULL DEFAULT '0',
  `ready_dps` int unsigned NOT NULL DEFAULT '0',
  `ready_alliance` int unsigned NOT NULL DEFAULT '0',
  `ready_horde` int unsigned NOT NULL DEFAULT '0',
  `assignments_hour` int unsigned NOT NULL DEFAULT '0',
  `releases_hour` int unsigned NOT NULL DEFAULT '0',
  `jit_creations_hour` int unsigned NOT NULL DEFAULT '0',
  `reservations_hour` int unsigned NOT NULL DEFAULT '0',
  `cancellations_hour` int unsigned NOT NULL DEFAULT '0',
  `dungeons_filled_hour` int unsigned NOT NULL DEFAULT '0',
  `raids_filled_hour` int unsigned NOT NULL DEFAULT '0',
  `battlegrounds_filled_hour` int unsigned NOT NULL DEFAULT '0',
  `arenas_filled_hour` int unsigned NOT NULL DEFAULT '0',
  `successful_requests_hour` int unsigned NOT NULL DEFAULT '0',
  `failed_requests_hour` int unsigned NOT NULL DEFAULT '0',
  `timeout_requests_hour` int unsigned NOT NULL DEFAULT '0',
  `avg_assignment_time_us` int unsigned NOT NULL DEFAULT '0' COMMENT 'Avg assignment time in microseconds',
  `avg_warmup_time_ms` int unsigned NOT NULL DEFAULT '0' COMMENT 'Avg warmup time in milliseconds',
  `avg_jit_creation_time_ms` int unsigned NOT NULL DEFAULT '0' COMMENT 'Avg JIT creation time in milliseconds',
  `peak_assignment_time_us` int unsigned NOT NULL DEFAULT '0' COMMENT 'Peak assignment time in microseconds',
  `utilization_pct` decimal(5,2) NOT NULL DEFAULT '0.00' COMMENT 'Utilization percentage',
  `availability_pct` decimal(5,2) NOT NULL DEFAULT '0.00' COMMENT 'Availability percentage',
  `success_rate_pct` decimal(5,2) NOT NULL DEFAULT '100.00' COMMENT 'Request success rate',
  PRIMARY KEY (`id`),
  KEY `idx_snapshot_time` (`snapshot_time`),
  KEY `idx_utilization` (`utilization_pct`),
  KEY `idx_success_rate` (`success_rate_pct`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Instance Bot Pool - Hourly statistics snapshots';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_protection_status` (
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot character GUID',
  `protection_flags` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Bitmask of ProtectionReason flags',
  `guild_guid` bigint unsigned DEFAULT NULL COMMENT 'Guild GUID if bot is in a guild',
  `friend_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Number of players who have this bot as friend',
  `interaction_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total player interactions',
  `last_interaction` timestamp NULL DEFAULT NULL COMMENT 'Last time a player interacted with this bot',
  `last_group_time` timestamp NULL DEFAULT NULL COMMENT 'Last time bot was in a group with a player',
  `protection_score` float NOT NULL DEFAULT '0' COMMENT 'Calculated protection priority (higher = more protected)',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When protection tracking started',
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last update time',
  PRIMARY KEY (`bot_guid`),
  KEY `idx_protection_flags` (`protection_flags`),
  KEY `idx_protection_score` (`protection_score`),
  KEY `idx_guild_guid` (`guild_guid`),
  KEY `idx_last_interaction` (`last_interaction`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Bot protection status for lifecycle management';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_retirement_queue` (
  `bot_guid` bigint unsigned NOT NULL COMMENT 'Bot character GUID',
  `queued_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When bot was queued for retirement',
  `scheduled_deletion` timestamp NOT NULL COMMENT 'When bot will be deleted (after cooling period)',
  `retirement_reason` varchar(255) DEFAULT NULL COMMENT 'Reason for retirement',
  `retirement_state` enum('PENDING','COOLING','EXITING','CANCELLED','COMPLETED') DEFAULT 'PENDING' COMMENT 'Current retirement state',
  `bracket_at_queue` tinyint unsigned NOT NULL COMMENT 'Level bracket when queued (0-3)',
  `level_at_queue` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Character level when queued',
  `protection_score_at_queue` float NOT NULL DEFAULT '0' COMMENT 'Protection score when queued',
  `playtime_minutes` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total playtime in minutes',
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last update time',
  PRIMARY KEY (`bot_guid`),
  KEY `idx_scheduled_deletion` (`scheduled_deletion`),
  KEY `idx_retirement_state` (`retirement_state`),
  KEY `idx_bracket_at_queue` (`bracket_at_queue`),
  KEY `idx_queued_at` (`queued_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Bot retirement queue for lifecycle management';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_schedules` (
  `bot_guid` int unsigned NOT NULL,
  `pattern_name` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'default',
  `next_login` timestamp NULL DEFAULT NULL COMMENT 'When bot should next login',
  `next_logout` timestamp NULL DEFAULT NULL COMMENT 'When bot should logout',
  `last_activity` timestamp NULL DEFAULT NULL COMMENT 'Last recorded activity',
  `last_calculation` timestamp NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'When schedule was calculated',
  `total_sessions` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total login sessions',
  `total_playtime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Total seconds played',
  `current_session_start` timestamp NULL DEFAULT NULL COMMENT 'Current session start time',
  `is_scheduled` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Bot has active schedule',
  `is_active` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Bot is currently logged in',
  `schedule_priority` tinyint unsigned NOT NULL DEFAULT '5' COMMENT 'Schedule priority (1-10)',
  `consecutive_failures` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Failed login attempts',
  `last_failure_reason` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Last failure description',
  `next_retry` timestamp NULL DEFAULT NULL COMMENT 'When to retry after failure',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`bot_guid`),
  KEY `idx_next_login` (`next_login`),
  KEY `idx_next_logout` (`next_logout`),
  KEY `idx_active_schedules` (`is_scheduled`,`is_active`),
  KEY `idx_pattern` (`pattern_name`),
  KEY `idx_priority` (`schedule_priority`,`next_login`),
  KEY `idx_failures` (`consecutive_failures`,`next_retry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Individual bot schedule state and timing';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_spawn_log` (
  `log_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `bot_guid` int unsigned NOT NULL,
  `account_id` int unsigned NOT NULL,
  `event_type` enum('SPAWN_REQUEST','SPAWN_SUCCESS','SPAWN_FAILURE','DESPAWN_SCHEDULED','DESPAWN_FORCED','DESPAWN_ERROR') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `event_timestamp` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `map_id` int unsigned DEFAULT NULL COMMENT 'Map where event occurred',
  `zone_id` int unsigned DEFAULT NULL COMMENT 'Zone where event occurred',
  `area_id` int unsigned DEFAULT NULL COMMENT 'Area where event occurred',
  `position_x` float DEFAULT NULL,
  `position_y` float DEFAULT NULL,
  `position_z` float DEFAULT NULL,
  `reason` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Reason for spawn/despawn',
  `initiator` enum('SCHEDULER','SPAWNER','ADMIN','SYSTEM','PLAYER') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'SYSTEM',
  `pattern_name` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Activity pattern used',
  `processing_time_ms` int unsigned DEFAULT NULL COMMENT 'Time to process event',
  `queue_wait_time_ms` int unsigned DEFAULT NULL COMMENT 'Time waiting in queue',
  `extra_data` json DEFAULT NULL COMMENT 'Additional event-specific data',
  PRIMARY KEY (`log_id`),
  KEY `idx_bot_events` (`bot_guid`,`event_timestamp`),
  KEY `idx_event_type` (`event_type`,`event_timestamp`),
  KEY `idx_location` (`map_id`,`zone_id`,`event_timestamp`),
  KEY `idx_pattern_events` (`pattern_name`,`event_timestamp`),
  KEY `idx_initiator` (`initiator`,`event_timestamp`),
  KEY `idx_timestamp` (`event_timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Historical log of bot spawn and despawn events';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_spec_info` (
  `spec_id` int unsigned NOT NULL COMMENT 'ChrSpecialization.db2 ID',
  `class_id` tinyint unsigned NOT NULL COMMENT 'ChrClasses.db2 ID',
  `class_name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Class name (English)',
  `spec_name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Spec name (English)',
  `role` enum('TANK','HEALER','DPS') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Combat role',
  `spec_index` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Spec index within class (0-3)',
  `stat_priority` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Primary > Secondary stats',
  `armor_type` enum('CLOTH','LEATHER','MAIL','PLATE') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Armor class',
  `primary_stat` enum('STRENGTH','AGILITY','INTELLECT') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Primary stat',
  `enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether to generate templates',
  `notes` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Implementation notes',
  PRIMARY KEY (`spec_id`),
  KEY `idx_class` (`class_id`),
  KEY `idx_role` (`role`),
  KEY `idx_class_role` (`class_id`,`role`),
  KEY `idx_enabled` (`enabled`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Master class/spec reference for WoW 11.2 (The War Within)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_talent_loadouts` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `class_id` tinyint unsigned NOT NULL COMMENT 'Class ID (1-13)',
  `spec_id` tinyint unsigned NOT NULL COMMENT 'Specialization Index (0-3 per class)',
  `min_level` tinyint unsigned NOT NULL COMMENT 'Minimum level for this loadout',
  `max_level` tinyint unsigned NOT NULL COMMENT 'Maximum level for this loadout',
  `talent_string` text NOT NULL COMMENT 'Comma-separated talent entry IDs',
  `hero_talent_string` text COMMENT 'Comma-separated hero talent entry IDs (71+)',
  `description` varchar(255) DEFAULT NULL COMMENT 'Loadout description',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_class_spec` (`class_id`,`spec_id`),
  KEY `idx_level` (`min_level`,`max_level`)
) ENGINE=InnoDB AUTO_INCREMENT=313 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Talent loadouts for automated bot creation';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_actionbars` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `template_id` int unsigned NOT NULL COMMENT 'FK to templates',
  `action_bar` tinyint unsigned NOT NULL COMMENT 'Action bar number (0-7)',
  `slot` tinyint unsigned NOT NULL COMMENT 'Slot on bar (0-11)',
  `action_type` int unsigned NOT NULL DEFAULT '0' COMMENT '0=Spell, 1=Item, 64=Macro, 128=Companion',
  `action_id` int unsigned NOT NULL COMMENT 'Spell/Item/Macro ID',
  `enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether this action is enabled',
  `priority` int unsigned NOT NULL DEFAULT '0' COMMENT 'For rotation priority hints',
  `is_rotational` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Used in DPS rotation',
  `is_defensive` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Defensive cooldown',
  `is_interrupt` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Interrupt ability',
  `description` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Ability name for reference',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_template_bar_slot` (`template_id`,`action_bar`,`slot`),
  KEY `idx_template` (`template_id`),
  KEY `idx_action_id` (`action_id`),
  KEY `idx_enabled` (`enabled`),
  CONSTRAINT `fk_actionbar_template` FOREIGN KEY (`template_id`) REFERENCES `playerbot_bot_templates` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Action bar configurations for each template';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_config` (
  `config_key` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `config_value` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`config_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Template system configuration';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_gear_items` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `gear_set_id` int unsigned NOT NULL COMMENT 'FK to gear_sets',
  `slot_id` tinyint unsigned NOT NULL COMMENT 'Equipment slot (0-18)',
  `item_id` int unsigned NOT NULL COMMENT 'Item entry ID',
  `item_level` int unsigned NOT NULL DEFAULT '0' COMMENT 'Item level',
  `enchant_id` int unsigned DEFAULT '0' COMMENT 'Enchant ID',
  `gem1_id` int unsigned DEFAULT '0' COMMENT 'First gem ID',
  `gem2_id` int unsigned DEFAULT '0' COMMENT 'Second gem ID',
  `gem3_id` int unsigned DEFAULT '0' COMMENT 'Third gem ID',
  `bonus_list` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Comma-separated bonus IDs',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_gearset_slot` (`gear_set_id`,`slot_id`),
  KEY `idx_item_id` (`item_id`),
  CONSTRAINT `fk_gearitem_gearset` FOREIGN KEY (`gear_set_id`) REFERENCES `playerbot_template_gear_sets` (`gear_set_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Individual gear items per slot for each gear set';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_gear_sets` (
  `gear_set_id` int unsigned NOT NULL AUTO_INCREMENT,
  `template_id` int unsigned NOT NULL COMMENT 'FK to templates',
  `target_ilvl` int unsigned NOT NULL COMMENT 'Target average item level',
  `actual_gear_score` int unsigned NOT NULL DEFAULT '0' COMMENT 'Calculated gear score',
  `gear_set_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Human-readable name',
  `content_tier` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Content tier (e.g., Heroic_Dungeon, Normal_Raid)',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`gear_set_id`),
  UNIQUE KEY `uk_template_ilvl` (`template_id`,`target_ilvl`),
  KEY `idx_item_level` (`target_ilvl`),
  CONSTRAINT `fk_gearset_template` FOREIGN KEY (`template_id`) REFERENCES `playerbot_bot_templates` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gear set definitions per template and item level tier';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_statistics` (
  `template_id` int unsigned NOT NULL,
  `total_uses` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Total times cloned',
  `last_used` timestamp NULL DEFAULT NULL,
  `avg_creation_time_ms` int unsigned NOT NULL DEFAULT '0',
  `min_creation_time_ms` int unsigned NOT NULL DEFAULT '0',
  `max_creation_time_ms` int unsigned NOT NULL DEFAULT '0',
  `successful_clones` bigint unsigned NOT NULL DEFAULT '0',
  `failed_clones` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`template_id`),
  CONSTRAINT `fk_stats_template` FOREIGN KEY (`template_id`) REFERENCES `playerbot_bot_templates` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Template usage statistics';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_template_talents` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `template_id` int unsigned NOT NULL COMMENT 'FK to templates',
  `talent_tier` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Talent tier/row (0-6)',
  `talent_column` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Talent column (0-2)',
  `talent_id` int unsigned NOT NULL COMMENT 'Talent spell ID',
  `is_pvp_talent` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Whether this is a PvP talent',
  `enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Whether this talent is enabled',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_template_tier_col` (`template_id`,`talent_tier`,`talent_column`,`is_pvp_talent`),
  KEY `idx_template_pvp` (`template_id`,`is_pvp_talent`),
  CONSTRAINT `fk_talent_template` FOREIGN KEY (`template_id`) REFERENCES `playerbot_bot_templates` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Talent selections for each template';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_v2_world_metadata` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `map_id` int unsigned NOT NULL,
  `zone_id` int unsigned NOT NULL DEFAULT '0',
  `kind` tinyint unsigned NOT NULL,
  `pos_x` float NOT NULL,
  `pos_y` float NOT NULL,
  `pos_z` float NOT NULL,
  `radius` float NOT NULL DEFAULT '10',
  `label` varchar(96) NOT NULL DEFAULT '',
  `notes` varchar(255) NOT NULL DEFAULT '',
  `created_by` varchar(64) NOT NULL DEFAULT '',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_map_kind` (`map_id`,`kind`),
  KEY `idx_map_xy` (`map_id`,`pos_x`,`pos_y`),
  KEY `idx_zone` (`zone_id`),
  KEY `idx_kind` (`kind`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_zone_demand` (
  `zone_id` int unsigned NOT NULL COMMENT 'Zone ID',
  `map_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Map ID',
  `zone_name` varchar(100) DEFAULT NULL COMMENT 'Zone name for reference',
  `player_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current player count',
  `bot_count` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current bot count',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Minimum level for zone',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80' COMMENT 'Maximum level for zone',
  `demand_score` float NOT NULL DEFAULT '0' COMMENT 'Calculated demand score',
  `last_calculated` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Last calculation time',
  PRIMARY KEY (`zone_id`),
  KEY `idx_demand_score` (`demand_score` DESC),
  KEY `idx_player_count` (`player_count` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Zone demand statistics for spawn prioritization';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbot_zone_populations` (
  `zone_id` int unsigned NOT NULL,
  `map_id` int unsigned NOT NULL,
  `current_bots` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current bot count in zone',
  `target_population` int unsigned NOT NULL DEFAULT '0' COMMENT 'Target bot population',
  `max_population` int unsigned NOT NULL DEFAULT '50' COMMENT 'Maximum allowed bots',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1' COMMENT 'Minimum level for zone',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80' COMMENT 'Maximum level for zone',
  `spawn_weight` float NOT NULL DEFAULT '1' COMMENT 'Relative spawn probability',
  `is_enabled` tinyint(1) NOT NULL DEFAULT '1' COMMENT 'Zone allows bot spawning',
  `is_starter_zone` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Low-level character zone',
  `is_endgame_zone` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'High-level content zone',
  `population_multiplier` float NOT NULL DEFAULT '1' COMMENT 'Dynamic population adjustment',
  `last_spawn` timestamp NULL DEFAULT NULL COMMENT 'Last bot spawn in this zone',
  `spawn_cooldown_minutes` int unsigned NOT NULL DEFAULT '5' COMMENT 'Minimum time between spawns',
  `total_spawns_today` int unsigned NOT NULL DEFAULT '0' COMMENT 'Daily spawn counter',
  `average_session_time` int unsigned NOT NULL DEFAULT '3600' COMMENT 'Average session length',
  `peak_population` int unsigned NOT NULL DEFAULT '0' COMMENT 'Peak population reached',
  `peak_timestamp` timestamp NULL DEFAULT NULL COMMENT 'When peak was reached',
  `last_updated` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`zone_id`,`map_id`),
  KEY `idx_current_population` (`current_bots`,`target_population`),
  KEY `idx_spawn_eligibility` (`is_enabled`,`spawn_weight`),
  KEY `idx_level_range` (`min_level`,`max_level`),
  KEY `idx_zone_type` (`is_starter_zone`,`is_endgame_zone`),
  KEY `idx_spawn_cooldown` (`last_spawn`,`spawn_cooldown_minutes`),
  KEY `idx_updated` (`last_updated`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Real-time bot population tracking per zone';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_class_popularity` (
  `class_id` tinyint unsigned NOT NULL,
  `class_name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `popularity_weight` float NOT NULL DEFAULT '1',
  `min_level` tinyint unsigned NOT NULL DEFAULT '1',
  `max_level` tinyint unsigned NOT NULL DEFAULT '80',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`class_id`),
  KEY `idx_enabled_classes` (`enabled`,`popularity_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_gender_distribution` (
  `race_id` tinyint unsigned NOT NULL,
  `male_percentage` float NOT NULL DEFAULT '50',
  `female_percentage` float NOT NULL DEFAULT '50',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gender distribution by race';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_names` (
  `name_id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `gender` tinyint unsigned NOT NULL DEFAULT '2' COMMENT '0=male, 1=female, 2=neutral',
  `race_mask` int unsigned NOT NULL DEFAULT '4294967295' COMMENT 'Bitmask of compatible races',
  `is_taken` tinyint(1) NOT NULL DEFAULT '0',
  `is_used` tinyint(1) NOT NULL DEFAULT '0',
  `used_by_guid` bigint unsigned DEFAULT NULL,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`name_id`),
  UNIQUE KEY `idx_unique_name` (`name`),
  KEY `idx_available_names` (`is_taken`,`gender`,`race_mask`),
  KEY `idx_used_names` (`is_used`,`used_by_guid`)
) ENGINE=InnoDB AUTO_INCREMENT=107340 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_names_used` (
  `name_id` int unsigned NOT NULL,
  `character_guid` bigint unsigned NOT NULL,
  `assigned_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`character_guid`),
  UNIQUE KEY `idx_unique_name` (`name_id`),
  CONSTRAINT `fk_names_used_name_id` FOREIGN KEY (`name_id`) REFERENCES `playerbots_names` (`name_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_race_class_distribution` (
  `race_id` tinyint unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `distribution_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`,`class_id`),
  KEY `idx_race_class_enabled` (`enabled`,`distribution_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Race-class combination weights';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playerbots_race_class_gender` (
  `race_id` tinyint unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `gender` tinyint unsigned NOT NULL,
  `preference_weight` float NOT NULL DEFAULT '1',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`race_id`,`class_id`,`gender`),
  KEY `idx_combo_enabled` (`enabled`,`preference_weight`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Combined race-class-gender preferences';
/*!40101 SET character_set_client = @saved_cs_client */;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_gear_sets_overview` AS SELECT 
 1 AS `template_name`,
 1 AS `target_ilvl`,
 1 AS `gear_set_name`,
 1 AS `content_tier`,
 1 AS `actual_gear_score`,
 1 AS `items_defined`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_pool_roles` AS SELECT 
 1 AS `faction`,
 1 AS `role`,
 1 AS `slot_state`,
 1 AS `count`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_pool_status` AS SELECT 
 1 AS `slot_state`,
 1 AS `count`,
 1 AS `percentage`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_template_details` AS SELECT 
 1 AS `template_id`,
 1 AS `template_name`,
 1 AS `spec_id`,
 1 AS `class_id`,
 1 AS `class_name`,
 1 AS `spec_name`,
 1 AS `role`,
 1 AS `armor_type`,
 1 AS `primary_stat`,
 1 AS `enabled`,
 1 AS `validated`,
 1 AS `version`,
 1 AS `patch_version`,
 1 AS `total_uses`,
 1 AS `avg_creation_time_ms`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_templates_by_role` AS SELECT 
 1 AS `role`,
 1 AS `template_count`,
 1 AS `enabled_count`,
 1 AS `validated_count`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_warm_pool_bracket_summary` AS SELECT 
 1 AS `bracket`,
 1 AS `faction`,
 1 AS `tanks`,
 1 AS `healers`,
 1 AS `dps`,
 1 AS `total`*/;
SET character_set_client = @saved_cs_client;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `v_warm_pool_health` AS SELECT 
 1 AS `bracket`,
 1 AS `faction`,
 1 AS `actual_tanks`,
 1 AS `target_tanks`,
 1 AS `actual_healers`,
 1 AS `target_healers`,
 1 AS `actual_dps`,
 1 AS `target_dps`,
 1 AS `actual_total`,
 1 AS `target_total`,
 1 AS `status`*/;
SET character_set_client = @saved_cs_client;

/*!50001 DROP VIEW IF EXISTS `v_gear_sets_overview`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_gear_sets_overview` AS select `t`.`template_name` AS `template_name`,`gs`.`target_ilvl` AS `target_ilvl`,`gs`.`gear_set_name` AS `gear_set_name`,`gs`.`content_tier` AS `content_tier`,`gs`.`actual_gear_score` AS `actual_gear_score`,count(`gi`.`id`) AS `items_defined` from ((`playerbot_bot_templates` `t` join `playerbot_template_gear_sets` `gs` on((`t`.`template_id` = `gs`.`template_id`))) left join `playerbot_template_gear_items` `gi` on((`gs`.`gear_set_id` = `gi`.`gear_set_id`))) group by `t`.`template_name`,`gs`.`gear_set_id` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_pool_roles`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_pool_roles` AS select `playerbot_instance_pool`.`faction` AS `faction`,`playerbot_instance_pool`.`role` AS `role`,`playerbot_instance_pool`.`slot_state` AS `slot_state`,count(0) AS `count` from `playerbot_instance_pool` group by `playerbot_instance_pool`.`faction`,`playerbot_instance_pool`.`role`,`playerbot_instance_pool`.`slot_state` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_pool_status`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_pool_status` AS select `playerbot_instance_pool`.`slot_state` AS `slot_state`,count(0) AS `count`,round(((count(0) * 100.0) / (select count(0) from `playerbot_instance_pool`)),2) AS `percentage` from `playerbot_instance_pool` group by `playerbot_instance_pool`.`slot_state` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_template_details`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_template_details` AS select `t`.`template_id` AS `template_id`,`t`.`template_name` AS `template_name`,`t`.`spec_id` AS `spec_id`,`t`.`class_id` AS `class_id`,`s`.`class_name` AS `class_name`,`s`.`spec_name` AS `spec_name`,`t`.`role` AS `role`,`s`.`armor_type` AS `armor_type`,`s`.`primary_stat` AS `primary_stat`,`t`.`enabled` AS `enabled`,`t`.`validated` AS `validated`,`t`.`version` AS `version`,`t`.`patch_version` AS `patch_version`,`ts`.`total_uses` AS `total_uses`,`ts`.`avg_creation_time_ms` AS `avg_creation_time_ms` from ((`playerbot_bot_templates` `t` join `playerbot_spec_info` `s` on((`t`.`spec_id` = `s`.`spec_id`))) left join `playerbot_template_statistics` `ts` on((`t`.`template_id` = `ts`.`template_id`))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_templates_by_role`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_templates_by_role` AS select `s`.`role` AS `role`,count(0) AS `template_count`,sum((case when (`t`.`enabled` = 1) then 1 else 0 end)) AS `enabled_count`,sum((case when (`t`.`validated` = 1) then 1 else 0 end)) AS `validated_count` from (`playerbot_bot_templates` `t` join `playerbot_spec_info` `s` on((`t`.`spec_id` = `s`.`spec_id`))) group by `s`.`role` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_warm_pool_bracket_summary`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_warm_pool_bracket_summary` AS select `playerbot_instance_pool`.`bracket` AS `bracket`,`playerbot_instance_pool`.`faction` AS `faction`,sum((case when (`playerbot_instance_pool`.`role` = 'TANK') then 1 else 0 end)) AS `tanks`,sum((case when (`playerbot_instance_pool`.`role` = 'HEALER') then 1 else 0 end)) AS `healers`,sum((case when (`playerbot_instance_pool`.`role` = 'DPS') then 1 else 0 end)) AS `dps`,count(0) AS `total` from `playerbot_instance_pool` where (`playerbot_instance_pool`.`is_warm_pool` = 1) group by `playerbot_instance_pool`.`bracket`,`playerbot_instance_pool`.`faction` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!50001 DROP VIEW IF EXISTS `v_warm_pool_health`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `v_warm_pool_health` AS select `b`.`bracket` AS `bracket`,`b`.`faction` AS `faction`,coalesce(`p`.`tanks`,0) AS `actual_tanks`,10 AS `target_tanks`,coalesce(`p`.`healers`,0) AS `actual_healers`,15 AS `target_healers`,coalesce(`p`.`dps`,0) AS `actual_dps`,25 AS `target_dps`,coalesce(`p`.`total`,0) AS `actual_total`,50 AS `target_total`,(case when (coalesce(`p`.`total`,0) = 50) then 'HEALTHY' when (coalesce(`p`.`total`,0) >= 40) then 'WARNING' else 'CRITICAL' end) AS `status` from ((select `b`.`bracket` AS `bracket`,`f`.`faction` AS `faction` from ((select 0 AS `bracket` union all select 1 AS `1` union all select 2 AS `2` union all select 3 AS `3` union all select 4 AS `4` union all select 5 AS `5` union all select 6 AS `6` union all select 7 AS `7`) `b` join (select 'ALLIANCE' AS `faction` union all select 'HORDE' AS `HORDE`) `f`)) `b` left join `v_warm_pool_bracket_summary` `p` on(((`b`.`bracket` = `p`.`bracket`) and (`b`.`faction` = `p`.`faction`)))) */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
