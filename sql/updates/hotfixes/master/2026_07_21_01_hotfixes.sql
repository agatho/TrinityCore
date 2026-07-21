-- ============================================================================
-- WarbandScenePlacement.db2 - finish the 12.0.x layout update (LayoutHash
-- EF845637, the layout used by 12.0.7.68275).
--
-- DB2Metadata.h already described 11 meta fields matching the client file, but
-- DB2Structure.h / DB2LoadInfo.h still carried the older 10-field layout, so
-- LoadDB2 aborted startup with:
--   "WarbandScenePlacement.db2 C++ structure fields fffiiiffiiii do not match
--    generated types from the client fffiiiffiiiii"
--
-- Per WoWDBDefs the EF845637 layout is:
--   Position[3], ID, WarbandSceneID, SlotType, Rotation, Scale,
--   Field_11_0_0_54210_004, Field_11_0_0_54210_005,
--   Field_12_0_0_63534_008, SlotID, Field_12_0_0_63534_010
-- i.e. a field is inserted *before* SlotID and the trailing field is renamed
-- (it is not an append). Field_12_0_0_63534_008 was already added to this
-- table; only the trailing rename is still outstanding.
-- ============================================================================

ALTER TABLE `warband_scene_placement`
  CHANGE COLUMN `Field_11_1_0_58221_009` `Field_12_0_0_63534_010` int NOT NULL DEFAULT '0';
