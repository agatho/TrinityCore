-- Housing Tutorial Quest Chain Setup
-- Two paths to the same destination:
--   Path A (portal):   Accept 93057 from Stormwind Lyssabel → use portal → 93057 auto-offers 91863
--   Path B (tutorial): Click "Start Tutorial" → teleport to neighborhood → pick up 91863 from Lyssabel
-- Then: 91863 "My First Home" → 91968 "Welcome Home" → 91969 "Time to Decorate"
-- Note: 93647 "Lumber For You" is an alternate breadcrumb to 91863 (auto-complete, no objectives)

-- ============================================================================
-- 1. Set NPC flags and scripts on housing quest NPCs
-- ============================================================================

-- Lyssabel Dawnpetal (Stormwind, near housing portal) - starts the housing chain
UPDATE creature_template SET npcflag = npcflag | 2 WHERE entry = 256078;

-- Lyssabel Dawnpetal (Founder's Point neighborhood) - quest hub + gossip steward
-- GOSSIP (0x1) needed for gossip menu options, QUESTGIVER (0x2) for quests
UPDATE creature_template SET npcflag = npcflag | 3, ScriptName = 'npc_housing_steward' WHERE entry = 233063;

-- Tocho (Horde equivalent neighborhood steward) - gossip steward
UPDATE creature_template SET npcflag = npcflag | 3, ScriptName = 'npc_housing_steward' WHERE entry = 233708;

-- ============================================================================
-- 2. Quest chain prerequisites via quest_template_addon
-- ============================================================================
-- PrevQuestID links enforce sequential completion

DELETE FROM quest_template_addon WHERE ID IN (93057, 91863, 91968, 91969);

-- 93057 "A House For You": Entry quest, no prerequisite
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (93057, 0);

-- 91863 "My First Home": No hard prerequisite (reachable via portal path OR tutorial button)
-- When using portal path, 93057.RewardNextQuest auto-offers 91863 on turn-in
-- When using tutorial button, player picks up 91863 directly from neighborhood Lyssabel
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (91863, 0);

-- 91968 "Welcome Home": Requires completing "My First Home"
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (91968, 91863);

-- 91969 "Time to Decorate": Requires completing "Welcome Home"
-- Note: Also auto-offered via 91968.RewardNextQuest = 91969
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (91969, 91968);

-- ============================================================================
-- 3. Quest starters (who offers the quest)
-- ============================================================================

-- 93057 "A House For You": Stormwind Lyssabel (256078) near the housing portal
INSERT IGNORE INTO creature_queststarter (id, quest) VALUES (256078, 93057);

-- 91863 "My First Home": Already has starter (233063 Lyssabel in neighborhood)
-- Also auto-offered when turning in 93057 (RewardNextQuest)

-- 91968 "Welcome Home": Neighborhood Lyssabel, after completing My First Home
INSERT IGNORE INTO creature_queststarter (id, quest) VALUES (233063, 91968);

-- 91969 "Time to Decorate": Auto-offered via 91968.RewardNextQuest = 91969
-- Also available from Lyssabel as fallback
INSERT IGNORE INTO creature_queststarter (id, quest) VALUES (233063, 91969);

-- 94210 "Feathering the Nest": Alliance offered by Lyssabel, Horde offered by Tocho
INSERT IGNORE INTO creature_queststarter (id, quest) VALUES (233063, 94210);
INSERT IGNORE INTO creature_queststarter (id, quest) VALUES (233708, 94210);

-- ============================================================================
-- 4. Quest enders (who accepts the turn-in)
-- ============================================================================

-- 93057 "A House For You": Turn in to Neighborhood Lyssabel after entering neighborhood
INSERT IGNORE INTO creature_questender (id, quest) VALUES (233063, 93057);

-- 91863 "My First Home": Already has ender (233063 Lyssabel in neighborhood)

-- 91968 "Welcome Home": Turn in to Neighborhood Lyssabel
INSERT IGNORE INTO creature_questender (id, quest) VALUES (233063, 91968);

-- 91969 "Time to Decorate": Turn in to Neighborhood Lyssabel
INSERT IGNORE INTO creature_questender (id, quest) VALUES (233063, 91969);

-- 94210 "Feathering the Nest": Alliance ended by Lyssabel, Horde ended by Tocho
INSERT IGNORE INTO creature_questender (id, quest) VALUES (233063, 94210);
INSERT IGNORE INTO creature_questender (id, quest) VALUES (233708, 94210);
