/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PlayerbotsDatabase.h"
#include "MySQLPreparedStatement.h"

PlayerbotsDatabaseConnection::PlayerbotsDatabaseConnection(MySQLConnectionInfo& connInfo, ConnectionFlags connectionFlags) :
    MySQLConnection(connInfo, connectionFlags)
{
}

PlayerbotsDatabaseConnection::~PlayerbotsDatabaseConnection() = default;

void PlayerbotsDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_PLAYERBOTS_STATEMENTS);

    // Custom Strategy Management
    PrepareStatement(PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER, "SELECT * FROM playerbots_custom_strategy WHERE owner = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME, "SELECT * FROM playerbots_custom_strategy WHERE owner = ? AND name = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME_AND_IDX, "SELECT * FROM playerbots_custom_strategy WHERE owner = ? AND name = ? AND idx = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_DEL_CUSTOM_STRATEGY, "DELETE FROM playerbots_custom_strategy WHERE owner = ? AND name = ? AND idx = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_UPD_CUSTOM_STRATEGY, "UPDATE playerbots_custom_strategy SET value = ? WHERE owner = ? AND name = ? AND idx = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_INS_CUSTOM_STRATEGY, "INSERT INTO playerbots_custom_strategy (owner, name, idx, value) VALUES (?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_NAME, "SELECT * FROM playerbots_custom_strategy WHERE name = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_NAME_AND_IDX, "SELECT * FROM playerbots_custom_strategy WHERE name = ? AND idx = ?", CONNECTION_SYNCH);

    // Database Store Management
    PrepareStatement(PLAYERBOTS_SEL_DB_STORE, "SELECT value FROM playerbots_db_store WHERE guid = ? AND `key` = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_DEL_DB_STORE, "DELETE FROM playerbots_db_store WHERE guid = ? AND `key` = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_INS_DB_STORE, "REPLACE INTO playerbots_db_store (guid, `key`, value) VALUES (?, ?, ?)", CONNECTION_ASYNC);

    // Equipment and Item Management
    PrepareStatement(PLAYERBOTS_SEL_ENCHANTS, "SELECT class, spec, item, enchant FROM playerbots_enchants WHERE class = ? AND spec = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_EQUIP_CACHE, "SELECT item, slot, quality, ilvl, clazz, subclazz, price FROM playerbots_equip_cache WHERE clazz = ? AND spec = ? AND level >= ? AND level <= ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_EQUIP_CACHE, "INSERT INTO playerbots_equip_cache (item, slot, quality, ilvl, clazz, subclazz, price, spec, level) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_RARITY_CACHE, "SELECT item, probability FROM playerbots_rarity_cache WHERE item = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_RARITY_CACHE, "INSERT INTO playerbots_rarity_cache (item, probability) VALUES (?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_RNDITEM_CACHE, "SELECT id, item, lvl FROM playerbots_rnditem_cache WHERE id = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_RNDITEM_CACHE, "INSERT INTO playerbots_rnditem_cache (id, item, lvl) VALUES (?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_INS_EQUIP_CACHE_NEW, "INSERT INTO playerbots_equip_cache (item, slot, quality, ilvl, clazz, subclazz, price, spec, level) SELECT item, slot, quality, ilvl, clazz, subclazz, price, spec, level FROM playerbots_equip_cache WHERE item = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_EQUIP_CACHE_NEW, "DELETE FROM playerbots_equip_cache WHERE item = ?", CONNECTION_ASYNC);

    // Guild Task Management
    PrepareStatement(PLAYERBOTS_SEL_GUILD_TASKS_BY_VALUE, "SELECT * FROM playerbots_guild_tasks WHERE value = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER, "SELECT * FROM playerbots_guild_tasks WHERE owner = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_AND_TYPE, "SELECT * FROM playerbots_guild_tasks WHERE owner = ? AND type = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_DISTINCT, "SELECT DISTINCT type FROM playerbots_guild_tasks WHERE owner = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_ORDERED, "SELECT * FROM playerbots_guild_tasks WHERE owner = ? ORDER BY type", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_DEL_GUILD_TASKS, "DELETE FROM playerbots_guild_tasks WHERE owner = ? AND type = ? AND value = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_INS_GUILD_TASKS, "INSERT INTO playerbots_guild_tasks (owner, type, value, time) VALUES (?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_UPD_GUILD_TASKS, "UPDATE playerbots_guild_tasks SET time = ? WHERE owner = ? AND type = ? AND value = ?", CONNECTION_ASYNC);

    // Random Bot Management
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_VALUE, "SELECT value FROM playerbots_random_bots WHERE owner = ? AND bot = ? AND event = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_BOT, "SELECT bot FROM playerbots_random_bots WHERE owner = ? AND event = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_EVENT, "SELECT * FROM playerbots_random_bots WHERE owner = ? AND event = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_BOT, "SELECT * FROM playerbots_random_bots WHERE owner = ? AND bot = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_BY_EVENT_AND_VALUE, "SELECT * FROM playerbots_random_bots WHERE event = ? AND value = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_RANDOM_BOTS, "INSERT INTO playerbots_random_bots (owner, bot, event, value, time, validIn) VALUES (?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_RANDOM_BOTS, "DELETE FROM playerbots_random_bots WHERE owner = ? AND bot = ? AND event = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER, "DELETE FROM playerbots_random_bots WHERE owner = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER_AND_EVENT, "DELETE FROM playerbots_random_bots WHERE owner = ? AND event = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_UPD_RANDOM_BOTS, "UPDATE playerbots_random_bots SET time = ?, validIn = ? WHERE owner = ? AND bot = ? AND event = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_RANDOM_BOTS_ALL, "SELECT * FROM playerbots_random_bots", CONNECTION_SYNCH);

    // Travel System Management
    PrepareStatement(PLAYERBOTS_SEL_TRAVELNODE, "SELECT * FROM playerbots_travelnode WHERE id = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_TRAVELNODE, "INSERT INTO playerbots_travelnode (id, name, map_id, x, y, z, linked) VALUES (?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_TRAVELNODE, "DELETE FROM playerbots_travelnode WHERE id = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_TRAVELNODE_LINK, "SELECT * FROM playerbots_travelnode_link WHERE from_node = ? AND to_node = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_TRAVELNODE_LINK, "INSERT INTO playerbots_travelnode_link (from_node, to_node, distance, swim, fly) VALUES (?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_TRAVELNODE_LINK, "DELETE FROM playerbots_travelnode_link WHERE from_node = ? AND to_node = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_TRAVELNODE_PATH, "SELECT * FROM playerbots_travelnode_path WHERE from_node = ? AND to_node = ? ORDER BY `index`", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_TRAVELNODE_PATH, "INSERT INTO playerbots_travelnode_path (from_node, to_node, `index`, x, y, z) VALUES (?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_TRAVELNODE_PATH, "DELETE FROM playerbots_travelnode_path WHERE from_node = ? AND to_node = ?", CONNECTION_ASYNC);

    // Speech and Text Management
    PrepareStatement(PLAYERBOTS_SEL_SPEECH, "SELECT text FROM playerbots_speech WHERE type = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_SPEECH_PROBABILITY, "SELECT probability FROM playerbots_speech_probability WHERE type = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_TEXT, "SELECT text FROM ai_playerbot_texts WHERE type = ? AND `index` = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_DUNGEON_SUGGESTION, "SELECT dungeonId, type, probability FROM playerbots_dungeon_suggestion WHERE level >= ? AND level <= ?", CONNECTION_SYNCH);

    // Cache and Utility
    PrepareStatement(PLAYERBOTS_SEL_TELE_CACHE, "SELECT level, alliance, horde FROM playerbots_tele_cache WHERE map_id = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_TELE_CACHE, "REPLACE INTO playerbots_tele_cache (map_id, level, alliance, horde) VALUES (?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_WEIGHTSCALES, "SELECT class, spec, name FROM playerbots_weightscales WHERE class = ? AND spec = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_WEIGHTSCALE_DATA, "SELECT field, val FROM playerbots_weightscale_data WHERE class = ? AND spec = ? AND name = ?", CONNECTION_SYNCH);

    // Additional Bot Management
    PrepareStatement(PLAYERBOTS_SEL_ITEM_INFO_CACHE, "SELECT * FROM playerbots_item_info_cache WHERE item = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_ITEM_INFO_CACHE, "REPLACE INTO playerbots_item_info_cache (item, slot, quality, ilvl, clazz, subclazz, price, scale1, scale2, scale3, scale4, scale5, scale6, scale7, scale8, scale9, scale10, scale11, scale12, scale13, scale14, scale15, scale16, scale17, scale18, scale19, scale20, scale21, scale22, scale23, scale24, scale25, scale26, scale27, scale28, scale29, scale30, scale31, scale32) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_UPD_ITEM_INFO_CACHE, "UPDATE playerbots_item_info_cache SET slot = ?, quality = ?, ilvl = ?, clazz = ?, subclazz = ?, price = ? WHERE item = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_ACCOUNT_LINKS, "SELECT linked_account FROM playerbots_account_links WHERE master_account = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_ACCOUNT_LINKS, "INSERT INTO playerbots_account_links (master_account, linked_account) VALUES (?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_DEL_ACCOUNT_LINKS, "DELETE FROM playerbots_account_links WHERE master_account = ? AND linked_account = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_PREFERRED_MOUNTS, "SELECT type, entry FROM playerbots_preferred_mounts WHERE guid = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_PREFERRED_MOUNTS, "REPLACE INTO playerbots_preferred_mounts (guid, type, entry) VALUES (?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_BOT_CONFIG, "SELECT value FROM playerbots_db_store WHERE guid = ? AND `key` = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_UPD_BOT_CONFIG, "REPLACE INTO playerbots_db_store (guid, `key`, value) VALUES (?, ?, ?)", CONNECTION_ASYNC);

    // Profession Management
    PrepareStatement(PLAYERBOTS_SEL_PROFESSION_DATA, "SELECT profession_id, skill_level, max_skill_level, total_crafted, gold_spent, gold_earned, profitability FROM playerbots_profession_data WHERE guid = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_INS_PROFESSION_DATA, "REPLACE INTO playerbots_profession_data (guid, profession_id, skill_level, max_skill_level, total_crafted, gold_spent, gold_earned, profitability) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_UPD_PROFESSION_DATA, "UPDATE playerbots_profession_data SET skill_level = ?, max_skill_level = ?, total_crafted = ?, gold_spent = ?, gold_earned = ?, profitability = ? WHERE guid = ? AND profession_id = ?", CONNECTION_ASYNC);

    // Social Relations Management - CRITICAL for PlayerbotSocialManager.cpp functionality
    PrepareStatement(PLAYERBOTS_SEL_SOCIAL_RELATIONS_NOTES, "SELECT notes FROM playerbot_social_relations WHERE botGuid = ? AND playerGuid = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_SEL_SOCIAL_RELATIONS_ALL, "SELECT playerGuid, playerName, relationType, trustLevel, interactionCount, positiveInteractions, negativeInteractions, lastInteractionTime, notes FROM playerbot_social_relations WHERE botGuid = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_DEL_SOCIAL_RELATIONS_ALL, "DELETE FROM playerbot_social_relations WHERE botGuid = ?", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_REP_SOCIAL_RELATIONS, "REPLACE INTO playerbot_social_relations (botGuid, playerGuid, playerName, relationType, trustLevel, interactionCount, positiveInteractions, negativeInteractions, lastInteractionTime, notes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(PLAYERBOTS_SEL_SOCIAL_RELATIONS_BY_PLAYER, "SELECT botGuid, relationType, trustLevel, interactionCount, positiveInteractions, negativeInteractions, lastInteractionTime, notes FROM playerbot_social_relations WHERE playerGuid = ?", CONNECTION_SYNCH);
    PrepareStatement(PLAYERBOTS_DEL_SOCIAL_RELATIONS_BY_PLAYER, "DELETE FROM playerbot_social_relations WHERE botGuid = ? AND playerGuid = ?", CONNECTION_ASYNC);
}