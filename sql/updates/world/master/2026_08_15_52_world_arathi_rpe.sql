--
-- Arathi Returning Player Experience - gate 90883 behind 90882.
-- Without this both quests are offered at once, before the gnoll turn-in.
-- quest_template_addon had no rows at all for 90882-90887.
--
-- UNVERIFIED: quests 90882 / 90883.
--

DELETE FROM `quest_template_addon` WHERE `ID`=90883;
INSERT INTO `quest_template_addon` (`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`, `BreadcrumbForQuestId`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`, `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`, `ProvidedItemCount`, `SpecialFlags`, `ScriptName`) VALUES
(90883, 0, 0, 0, 90882, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '');
