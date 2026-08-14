--
-- Arathi Returning Player Experience - farm Thrall was hostile to Horde.
--
-- 244656 carried faction 2057, which is hostile to Horde players, so the Go'shek Farm Thrall
-- turned red and aggroed instead of offering his quest. Bruvk (244729) and the farm Jaina
-- (244655) are friendly ambient (35); match them.
--
-- UNVERIFIED: creature 244656, faction template 35 for this NPC.
--

UPDATE `creature_template`
SET `faction`=35
WHERE `entry`=244656
  AND `faction`<>35;
