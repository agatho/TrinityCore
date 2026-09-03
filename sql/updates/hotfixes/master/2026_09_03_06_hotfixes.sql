--
-- Warband scene placement: add the columns the reconstructed WarbandScenePlacementFilterReq
-- DB2 loader (HotfixDatabase.cpp / DB2LoadInfo.h) SELECTs but the TDB base table lacks.
--

-- ---- column reconciliation (existing table missing columns) ----
ALTER TABLE `warband_scene_placement_filter_req` ADD COLUMN `Field_11_1_0_58221_003_0` int NOT NULL DEFAULT '0';
ALTER TABLE `warband_scene_placement_filter_req` ADD COLUMN `Field_11_1_0_58221_003_1` int NOT NULL DEFAULT '0';

