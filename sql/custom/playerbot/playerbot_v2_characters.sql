-- ============================================================================
-- PlayerbotV2 tables that live in the CHARACTERS database.
--
-- Companion to playerbot_shared.sql (which covers the separate shared
-- playerbot database named by the `Playerbot.SharedDatabase` config key).
-- These 11 tables are read/written by the module and by cs_playerbot_v2.cpp
-- against CharacterDatabase, e.g. playerbot_v2_character,
-- playerbot_v2_talent_build and playerbot_v2_world_metadata.
--
-- This is a real `mysqldump --no-data` of a working PlayerbotV2 test
-- environment, NOT a schema inferred from the loader queries - inferring one
-- previously produced a structurally-right but materially-wrong schema
-- (wrong defaults, wrong column widths, and columns no query touches were
-- invisible entirely).
--
-- NOT applied automatically: the TC updater only runs sql/updates/**, and
-- these tables are only needed by a build with BUILD_PLAYERBOT_V2=ON. Apply
-- manually against the characters database when deploying bots:
--     mysql -u <user> -p <characters-db> < playerbot_v2_characters.sql
--
-- CREATE TABLE IF NOT EXISTS is used deliberately: re-running must never
-- destroy live bot state. (Note the general hazard - IF NOT EXISTS silently
-- keeps a pre-existing table of the same name with a different shape - does
-- not apply here, as these names are module-specific.)
-- ============================================================================

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
CREATE TABLE IF NOT EXISTS `playerbot_v2_account` (
  `account_id` int unsigned NOT NULL,
  `pseudo_account_idx` int unsigned NOT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_used_at` datetime DEFAULT NULL,
  PRIMARY KEY (`account_id`),
  KEY `idx_pseudo` (`pseudo_account_idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_character` (
  `character_guid_low` bigint unsigned NOT NULL,
  `spawned_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_active_at` datetime DEFAULT NULL,
  `rng_seed` bigint unsigned NOT NULL,
  `spawn_state` tinyint unsigned NOT NULL,
  `owner_account_id` int unsigned NOT NULL DEFAULT '0',
  `owner_player_guid` bigint unsigned NOT NULL DEFAULT '0',
  `formation_type` tinyint unsigned NOT NULL DEFAULT '0',
  `formation_slot` tinyint unsigned NOT NULL DEFAULT '0',
  `follow_distance_yd` float NOT NULL DEFAULT '5',
  `owner_verbose` tinyint(1) NOT NULL DEFAULT '0',
  `distribution_level` tinyint unsigned NOT NULL DEFAULT '0',
  `distribution_at` timestamp NULL DEFAULT NULL,
  `setup_pipeline_state` tinyint unsigned NOT NULL DEFAULT '0',
  `jit_for_queue` varchar(48) DEFAULT NULL,
  `last_seen_at` timestamp NULL DEFAULT NULL,
  `leveling_target_hub` int unsigned NOT NULL DEFAULT '0',
  `leveling_target_map` smallint unsigned NOT NULL DEFAULT '0',
  `leveling_bracket_lo` tinyint unsigned NOT NULL DEFAULT '0',
  `leveling_bracket_hi` tinyint unsigned NOT NULL DEFAULT '0',
  `leveling_chosen_at` timestamp NULL DEFAULT NULL,
  `archetype_id` tinyint unsigned NOT NULL DEFAULT '0',
  `session_start_at` datetime DEFAULT NULL,
  `cumulative_session_minutes` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`character_guid_low`),
  KEY `idx_active` (`last_active_at`),
  KEY `idx_state` (`spawn_state`),
  KEY `idx_owner_account` (`owner_account_id`),
  KEY `idx_owner_player` (`owner_player_guid`),
  KEY `idx_distribution_level` (`distribution_level`),
  KEY `idx_jit_for_queue` (`jit_for_queue`),
  KEY `idx_last_seen_at` (`last_seen_at`),
  KEY `idx_leveling_target_hub` (`leveling_target_hub`),
  KEY `idx_archetype_id` (`archetype_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_fleet_vitals_sample` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `sample_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `in_world` int unsigned NOT NULL DEFAULT '0',
  `alive` int unsigned NOT NULL DEFAULT '0',
  `in_combat` int unsigned NOT NULL DEFAULT '0',
  `wedged` int unsigned NOT NULL DEFAULT '0',
  `wedged_navmesh` int unsigned NOT NULL DEFAULT '0',
  `wedged_offmesh` int unsigned NOT NULL DEFAULT '0',
  `wedged_travel` int unsigned NOT NULL DEFAULT '0',
  `wedged_combatloop` int unsigned NOT NULL DEFAULT '0',
  `wedged_pickernone` int unsigned NOT NULL DEFAULT '0',
  `wedged_goalunreach` int unsigned NOT NULL DEFAULT '0',
  `tick_p50_us` int unsigned NOT NULL DEFAULT '0',
  `tick_p99_us` int unsigned NOT NULL DEFAULT '0',
  `intents_per_sec` int unsigned NOT NULL DEFAULT '0',
  `intents_dropped` int unsigned NOT NULL DEFAULT '0',
  `path_fail_per_min` int unsigned NOT NULL DEFAULT '0',
  `avg_level` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_sample_at` (`sample_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_personality` (
  `character_guid_low` bigint unsigned NOT NULL,
  `skill_tier` tinyint unsigned NOT NULL,
  `verbosity` tinyint unsigned NOT NULL,
  `aggression` tinyint unsigned NOT NULL,
  `risk_tolerance` tinyint unsigned NOT NULL,
  `politeness` tinyint unsigned NOT NULL,
  `loyalty` tinyint unsigned NOT NULL,
  `activity_pref` tinyint unsigned NOT NULL,
  `response_delay_ms` smallint unsigned NOT NULL,
  `response_jitter_ms` smallint unsigned NOT NULL,
  `mistake_rate` tinyint unsigned NOT NULL,
  PRIMARY KEY (`character_guid_low`),
  CONSTRAINT `fk_pers_char` FOREIGN KEY (`character_guid_low`) REFERENCES `playerbot_v2_character` (`character_guid_low`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_population_target` (
  `target_id` int unsigned NOT NULL AUTO_INCREMENT,
  `realm_id` int unsigned NOT NULL,
  `effective_at` datetime NOT NULL,
  `total_target` int unsigned NOT NULL,
  `floor` int unsigned NOT NULL,
  `ceiling` int unsigned NOT NULL,
  `horde_pct` tinyint unsigned NOT NULL,
  `payload_json` text NOT NULL,
  PRIMARY KEY (`target_id`),
  KEY `idx_realm_effective` (`realm_id`,`effective_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_preferences` (
  `character_guid_low` bigint unsigned NOT NULL,
  `preferred_neighborhood` int unsigned DEFAULT NULL,
  `preferred_house_template` int unsigned DEFAULT NULL,
  `opt_in_dungeons` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_raids` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_pvp` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_arena` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_delves` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_professions` tinyint(1) NOT NULL DEFAULT '1',
  `opt_in_housing` tinyint(1) NOT NULL DEFAULT '1',
  `accept_player_invites` tinyint unsigned NOT NULL DEFAULT '1',
  `follow_distance_yd` float NOT NULL DEFAULT '5',
  PRIMARY KEY (`character_guid_low`),
  CONSTRAINT `fk_prefs_char` FOREIGN KEY (`character_guid_low`) REFERENCES `playerbot_v2_character` (`character_guid_low`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_schema_version` (
  `version` int unsigned NOT NULL,
  `applied_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `sha256` char(64) NOT NULL,
  PRIMARY KEY (`version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_squad_preset` (
  `owner_account_id` int unsigned NOT NULL,
  `preset_name` varchar(64) NOT NULL,
  `saved_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `payload_json` text NOT NULL,
  PRIMARY KEY (`owner_account_id`,`preset_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_stuck_objective` (
  `quest_id` int unsigned NOT NULL,
  `obj_id` int unsigned NOT NULL DEFAULT '0',
  `category` varchar(24) COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  `hit_count` bigint unsigned NOT NULL DEFAULT '0',
  `first_seen` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_seen` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `sample_map` int unsigned NOT NULL DEFAULT '0',
  `sample_zone` int unsigned NOT NULL DEFAULT '0',
  `sample_x` float NOT NULL DEFAULT '0',
  `sample_y` float NOT NULL DEFAULT '0',
  `sample_bot` varchar(48) COLLATE utf8mb4_general_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`quest_id`,`obj_id`),
  KEY `idx_hit_count` (`hit_count`),
  KEY `idx_last_seen` (`last_seen`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_talent_build` (
  `class_id` tinyint unsigned NOT NULL,
  `spec_id` int unsigned NOT NULL,
  `context` tinyint unsigned NOT NULL,
  `label` varchar(128) NOT NULL DEFAULT '',
  `entries_json` mediumtext NOT NULL,
  `source_url` varchar(512) NOT NULL DEFAULT '',
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`class_id`,`spec_id`,`context`),
  KEY `idx_spec` (`spec_id`,`context`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `playerbot_v2_world_metadata` (
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
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
