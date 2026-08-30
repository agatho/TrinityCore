--
-- Table structure for table `ui_arrow_callout`
--
DROP TABLE IF EXISTS `ui_arrow_callout`;
CREATE TABLE `ui_arrow_callout` (
  `CalloutText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CalloutFrame` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `Direction` tinyint unsigned NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `UiWidgetSetID` int NOT NULL DEFAULT '0',
  `OffsetX` int NOT NULL DEFAULT '0',
  `OffsetY` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `ui_arrow_callout_locale`
--
DROP TABLE IF EXISTS `ui_arrow_callout_locale`;
CREATE TABLE `ui_arrow_callout_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `CalloutText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);

--
-- Table structure for table `ui_event_toast`
--
DROP TABLE IF EXISTS `ui_event_toast`;
CREATE TABLE `ui_event_toast` (
  `Title` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Subtitle` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `InstructionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `SubIcon` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `TitleTooltip` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `SubtitleTooltip` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `UiTextureAtlasMemberID` int NOT NULL DEFAULT '0',
  `UiTextureKitID` int NOT NULL DEFAULT '0',
  `EventType` tinyint unsigned NOT NULL DEFAULT '0',
  `DisplayType` tinyint unsigned NOT NULL DEFAULT '0',
  `EventAsset` int NOT NULL DEFAULT '0',
  `Field_9_1_0_38312_011` int NOT NULL DEFAULT '0',
  `IconFileID` int NOT NULL DEFAULT '0',
  `UiWidgetSetID` int NOT NULL DEFAULT '0',
  `ExtraUiWidgetSetID` int NOT NULL DEFAULT '0',
  `TitleTooltipUiWidgetSetID` int NOT NULL DEFAULT '0',
  `SubtitleTooltipUiWidgetSetID` int NOT NULL DEFAULT '0',
  `ShowSoundKitID` int NOT NULL DEFAULT '0',
  `HideSoundKitID` int NOT NULL DEFAULT '0',
  `Field_10_2_5_52554_021` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `ui_event_toast_locale`
--
DROP TABLE IF EXISTS `ui_event_toast_locale`;
CREATE TABLE `ui_event_toast_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Title_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Subtitle_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `InstructionText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `SubIcon_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `TitleTooltip_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `SubtitleTooltip_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);

--
-- Table structure for table `ui_generic_widget_display`
--
DROP TABLE IF EXISTS `ui_generic_widget_display`;
CREATE TABLE `ui_generic_widget_display` (
  `Title` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ExtraButtonText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CloseButtonText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `UiTextureKitID` int NOT NULL DEFAULT '0',
  `UiWidgetSetID` int NOT NULL DEFAULT '0',
  `FrameWidth` int NOT NULL DEFAULT '0',
  `FrameHeight` int NOT NULL DEFAULT '0',
  `Field_10_1_0_48480_008` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `ui_generic_widget_display_locale`
--
DROP TABLE IF EXISTS `ui_generic_widget_display_locale`;
CREATE TABLE `ui_generic_widget_display_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Title_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ExtraButtonText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CloseButtonText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);

--
-- Table structure for table `ui_party_pose`
--
DROP TABLE IF EXISTS `ui_party_pose`;
CREATE TABLE `ui_party_pose` (
  `TitleText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ExtraButtonText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ID` int unsigned NOT NULL DEFAULT '0',
  `UiWidgetSetID` int NOT NULL DEFAULT '0',
  `VictoryUiModelSceneID` int NOT NULL DEFAULT '0',
  `DefeatUiModelSceneID` int NOT NULL DEFAULT '0',
  `VictorySoundKitID` int NOT NULL DEFAULT '0',
  `DefeatSoundKitID` int NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `UiTextureKitID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `MapID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

--
-- Table structure for table `ui_party_pose_locale`
--
DROP TABLE IF EXISTS `ui_party_pose_locale`;
CREATE TABLE `ui_party_pose_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `TitleText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ExtraButtonText_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB);
