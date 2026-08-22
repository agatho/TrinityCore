-- ============================================================================
-- Arathi Catch-Up / RPE -- farm quest reward + objective fixes  (2026-08-22, live test)
-- ============================================================================

-- (1) DOUBLED OBJECTIVES. Every farm quest carried a fabricated duplicate objective at the SAME
--     StorageIndex as its real Blizzard objective (the 90NNN00 id pattern) -> the client showed each
--     kill/collect target twice and the counter inflated. Same bug class as the 90882 six-fold
--     objective already removed. Keep only the real objectives (461xxx / 463871 / 465804); delete the
--     fabricated duplicates. (90886 legitimately keeps TWO objectives: 461734 items + 465804 Stuck Ogre.)
DELETE FROM `quest_objectives` WHERE `ID` IN (9088300,9088500,9088600,9088700);

-- (2) DOUBLED "To Go'shek Farm" (90883) BAG REWARD. The 4-bag reward (249773/249772/249771/188213)
--     was authored into the RewardChoiceItemID slots (a "pick one of 4" choice). OUR CAPTURE shows it
--     as reward_items = '249773:1,249772:1,249771:1,188213:1' -- a FIXED SET of all four, given
--     together. Move the four bags from the choice slots to the fixed RewardItem slots and clear the
--     choice slots, so the player receives exactly ONE set of 4 bags (not a choice / not doubled).
UPDATE `quest_template` SET
  `RewardItem1`=249773, `RewardAmount1`=1,
  `RewardItem2`=249772, `RewardAmount2`=1,
  `RewardItem3`=249771, `RewardAmount3`=1,
  `RewardItem4`=188213, `RewardAmount4`=1,
  `RewardChoiceItemID1`=0, `RewardChoiceItemQuantity1`=0,
  `RewardChoiceItemID2`=0, `RewardChoiceItemQuantity2`=0,
  `RewardChoiceItemID3`=0, `RewardChoiceItemQuantity3`=0,
  `RewardChoiceItemID4`=0, `RewardChoiceItemQuantity4`=0
WHERE `ID`=90883;
