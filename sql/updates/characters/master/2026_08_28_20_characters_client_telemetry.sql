--
-- Einheit w4_cmsg_43_3D, Block B3 (Telemetrie & Support).
-- Datenmodell nach TRACK_B_dienste.md, Abschnitt B3.
--
-- Traeger fuer CMSG_ENGINE_SURVEY (0x430113, 294 Byte Hardware-/Engine-Profil, einmal je
-- Schema-Version + Client-Patch) und CMSG_REPORT_SERVER_LAG (0x3D0273, leere Nutzlast).
-- `kind`: 1 = engine_survey, 2 = server_lag.
--
-- Idempotent: legt die Tabelle nur an, wenn sie fehlt.
--

CREATE TABLE IF NOT EXISTS `client_telemetry` (
  `id`             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `account_id`     INT UNSIGNED NOT NULL,
  `character_guid` BIGINT UNSIGNED NULL DEFAULT NULL,
  `kind`           TINYINT UNSIGNED NOT NULL,
  `recorded_at`    TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `payload`        LONGTEXT NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_account_recorded` (`account_id`, `recorded_at`),
  KEY `idx_kind_recorded` (`kind`, `recorded_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
