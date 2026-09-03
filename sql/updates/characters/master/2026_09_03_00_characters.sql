--
-- CMSG_CRAFTING_ORDER_REPORT_PLAYER: persist player reports filed against a crafting-order participant.
--
DROP TABLE IF EXISTS `crafting_order_reports`;
CREATE TABLE `crafting_order_reports` (
  `ReportID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `OrderID` bigint unsigned NOT NULL DEFAULT '0',
  `ReporterGuid` bigint unsigned NOT NULL DEFAULT '0',
  `ReportedGuid` bigint unsigned NOT NULL DEFAULT '0',
  `ReportType` int NOT NULL DEFAULT '0',
  `MajorCategory` int NOT NULL DEFAULT '0',
  `MinorCategoryFlags` int NOT NULL DEFAULT '0',
  `Comment` text,
  `ReportTime` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`ReportID`),
  KEY `idx_order` (`OrderID`),
  KEY `idx_reported` (`ReportedGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
