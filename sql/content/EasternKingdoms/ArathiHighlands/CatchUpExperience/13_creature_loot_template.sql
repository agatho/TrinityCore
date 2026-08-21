-- ADAPTED to integ_world 12.x schema (Arathi Catch-Up loot).
-- Schema mapping applied:
--   DROPPED `UPDATE creature_template SET lootid=entry`  (no lootid column in 12.x;
--            creature_loot_template is keyed directly by creature Entry)
--   DROPPED `Reference` column  (does not exist in integ_world creature_loot_template)
--   DROPPED `VerifiedBuild` column (also absent from this table in 12.x)
-- Real columns: Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment
--
-- IDEMPOTENCY NOTE: integ_world's creature_loot_template has NO unique key
-- (idx_primary on (Entry,ItemType,Item) is NON_UNIQUE), so INSERT ... ON DUPLICATE KEY
-- UPDATE never fires and re-applying would duplicate rows. TC's own loot convention is
-- DELETE-then-INSERT scoped to the owned Entry set. These are brand-new content creatures
-- (244669/244674/244676/244677 -- spawned only on RPE map 2927), so ALL their loot is
-- RPE-owned; the scoped DELETE touches no base data.
DELETE FROM `creature_loot_template` WHERE `Entry` IN (244669, 244674, 244676, 244677);

INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244674, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244676, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244677, 243573, 100.0, 1, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244674, 1376, 100.0, 0, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244676, 220232, 100.0, 0, 1, 0, 1, 1);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`) VALUES (244669, 192617, 100.0, 0, 1, 0, 2, 2);
