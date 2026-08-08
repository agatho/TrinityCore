--
-- VerifiedBuild restamp to the dev client build 68887 for the Midnight S1 hotfix rows we seeded
-- with 68275 (2026_08_07_70_hotfixes.sql, 2026_08_07_71_hotfixes.sql) - the dev client is
-- 12.0.7.68887, so the seeds should advertise that build. Companion world-DB restamps live in
-- 2026_08_08_02_world.sql.
UPDATE `mythic_plus_season_tracked_map` SET `VerifiedBuild` = 68887 WHERE `ID` BETWEEN 253 AND 260;
UPDATE `mythic_plus_season_tracked_affix` SET `VerifiedBuild` = 68887 WHERE `ID` IN (315, 316, 317, 318, 319, 320, 321, 325);
UPDATE `mythic_plus_season_key_floor` SET `VerifiedBuild` = 68887 WHERE `ID` BETWEEN 46 AND 64;
UPDATE `mythic_plus_season_reward_levels` SET `VerifiedBuild` = 68887 WHERE `ID` BETWEEN 409 AND 445;
UPDATE `item_conversion` SET `VerifiedBuild` = 68887 WHERE `ID` = 12;
UPDATE `item_conversion_entry` SET `VerifiedBuild` = 68887 WHERE `ItemConversionID` = 12;
