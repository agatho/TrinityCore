--
-- Housing exterior_component: add the columns the reconstructed ExteriorComponent DB2 loader
-- (HotfixDatabase.cpp / DB2LoadInfo.h) SELECTs but the TDB base table lacks.
--

-- ---- column reconciliation (existing table missing columns) ----
ALTER TABLE `exterior_component` ADD COLUMN `Size` tinyint unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `HouseExteriorWmoDataID` int unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ParentComponentID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ModelFileDataID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_7` tinyint unsigned NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_9` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `GameObjectID` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `Field_11` int NOT NULL DEFAULT '0';
ALTER TABLE `exterior_component` ADD COLUMN `ItemID` int NOT NULL DEFAULT '0';

