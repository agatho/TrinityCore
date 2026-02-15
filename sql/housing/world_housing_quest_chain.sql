-- Housing Tutorial Quest Chain Setup
-- Chain: 93057 "A House For You" → 91863 "My First Home" → 91968 "Welcome Home" → 91969 "Time to Decorate"
-- Note: 93647 "Lumber For You" is an alternate breadcrumb to 91863 (auto-complete, no objectives)

-- ============================================================================
-- 1. Set UNIT_NPC_FLAG_QUESTGIVER (0x2) on housing quest NPCs
-- ============================================================================

-- Lyssabel Dawnpetal (Stormwind, near housing portal) - starts the housing chain
UPDATE creature_template SET npcflag = npcflag | 2 WHERE entry = 256078;

-- Lyssabel Dawnpetal (Founder's Point neighborhood) - quest hub in the neighborhood
UPDATE creature_template SET npcflag = npcflag | 2 WHERE entry = 233063;

-- ============================================================================
-- 2. Quest chain prerequisites via quest_template_addon
-- ============================================================================
-- PrevQuestID links enforce sequential completion

DELETE FROM quest_template_addon WHERE ID IN (93057, 91863, 91968, 91969);

-- 93057 "A House For You": Entry quest, no prerequisite
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (93057, 0);

-- 91863 "My First Home": Requires completing "A House For You"
-- Note: Also auto-offered via 93057.RewardNextQuest = 91863
INSERT INTO quest_template_addon (ID, PrevQuestID) VALUES (91863, 93057);

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
