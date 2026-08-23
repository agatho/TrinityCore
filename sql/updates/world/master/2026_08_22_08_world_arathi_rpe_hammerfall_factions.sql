-- ============================================================================
-- Arathi Catch-Up / RPE -- Hammerfall combat factions  (2026-08-22)
-- ============================================================================
-- Tester: "on retail gnolls and the existing hammerfall npcs combat each other" -- ours did not.
-- ROOT CAUSE: the whole Hammerfall garrison sits on faction 35 ("friendly to everyone"), so it can
-- neither attack nor be attacked -- the gnoll assault had nobody to fight. This is collateral from the
-- earlier Horde cross-validation pass that moved STORY ALLIES from faction 14 -> 35 (correct for
-- Jaina/Thrall/questgivers, which BOTH factions must be able to use, since TC cannot personally-phase
-- one NPC per faction the way retail does) -- but it was over-applied to the garrison combatants.
--
-- EVIDENCE (POV-independent wire FactionTemplate, and BOTH captures agree exactly):
--     entry            ALLIANCE 69382     HORDE 69404      DB (wrong)
--     244669/70/71/72  16                 16               14
--     230248, 232019   2361               2361             35
--     245028           714                714              35
--     245052           (decode garbage)   10               35
-- The Alliance capture recording the garrison as 2361/714 while the Alliance player stood safely among
-- them is what makes this safe to restore: those are story/neutral Horde factions, not player-hostile
-- ones (2361 is shared with friendly quest NPCs elsewhere on this realm, e.g. Lady Liadrin, Halduron
-- Brightwing). Restoring them puts the gnolls (monster faction 16) and the garrison (Horde factions)
-- back into a real hostility relationship via the client's own FactionTemplate -- which is how retail
-- gets them fighting -- instead of us hand-authoring aggro.
--
-- NOT TOUCHED: the story allies (Jaina/Thrall/Win'sa/questgivers/vendors) stay on 35 by design, and the
-- Gnoll Assailant corpses (245027) keep their faction since they are permanent feign-death set-dressing.
-- STILL OPEN: the same 35-flattening also hit the Stromgarde cast (Citizens/Footmen decode to wire 534,
-- an ALLIANCE faction). That POI has not been walked yet, and 534 could be hostile to a Horde player, so
-- it is deliberately left alone until the siege is tested.

UPDATE `creature_template` SET `faction`=16   WHERE `entry` IN (244669,244670,244671,244672);  -- gnoll assault
UPDATE `creature_template` SET `faction`=2361 WHERE `entry` IN (230248,232019);                -- Hammerfall / Mag'har Grunt
UPDATE `creature_template` SET `faction`=714  WHERE `entry` = 245028;                          -- Horde Grunt
UPDATE `creature_template` SET `faction`=10   WHERE `entry` = 245052;                          -- Horde Grunt
