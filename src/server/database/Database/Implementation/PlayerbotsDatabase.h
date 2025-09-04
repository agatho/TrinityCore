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

#ifndef _PLAYERBOTSDATABASE_H
#define _PLAYERBOTSDATABASE_H

#include "MySQLConnection.h"

enum PlayerbotsDatabaseStatements : uint32
{
    /*  Naming standard for defines:
        {DB}_{SEL/INS/UPD/DEL/REP}_{Summary of data changed}
        When updating more than one field, consider looking at the calling function
        name for a suiting suffix.
    */

    // Custom Strategy Management (8 statements)
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME_AND_IDX,
    PLAYERBOTS_DEL_CUSTOM_STRATEGY,
    PLAYERBOTS_UPD_CUSTOM_STRATEGY,
    PLAYERBOTS_INS_CUSTOM_STRATEGY,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_NAME,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_NAME_AND_IDX,

    // Database Store Management (3 statements)
    PLAYERBOTS_SEL_DB_STORE,
    PLAYERBOTS_DEL_DB_STORE,
    PLAYERBOTS_INS_DB_STORE,

    // Equipment and Item Management (9 statements)
    PLAYERBOTS_SEL_ENCHANTS,
    PLAYERBOTS_SEL_EQUIP_CACHE,
    PLAYERBOTS_INS_EQUIP_CACHE,
    PLAYERBOTS_SEL_RARITY_CACHE,
    PLAYERBOTS_INS_RARITY_CACHE,
    PLAYERBOTS_SEL_RNDITEM_CACHE,
    PLAYERBOTS_INS_RNDITEM_CACHE,
    PLAYERBOTS_INS_EQUIP_CACHE_NEW,
    PLAYERBOTS_DEL_EQUIP_CACHE_NEW,

    // Guild Task Management (8 statements)
    PLAYERBOTS_SEL_GUILD_TASKS_BY_VALUE,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_AND_TYPE,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_DISTINCT,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_ORDERED,
    PLAYERBOTS_DEL_GUILD_TASKS,
    PLAYERBOTS_INS_GUILD_TASKS,
    PLAYERBOTS_UPD_GUILD_TASKS,

    // Random Bot Management (11 statements)
    PLAYERBOTS_SEL_RANDOM_BOTS_VALUE,
    PLAYERBOTS_SEL_RANDOM_BOTS_BOT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_EVENT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_BOT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_EVENT_AND_VALUE,
    PLAYERBOTS_INS_RANDOM_BOTS,
    PLAYERBOTS_DEL_RANDOM_BOTS,
    PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER,
    PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER_AND_EVENT,
    PLAYERBOTS_UPD_RANDOM_BOTS,
    PLAYERBOTS_SEL_RANDOM_BOTS_ALL,

    // Travel System Management (9 statements)
    PLAYERBOTS_SEL_TRAVELNODE,
    PLAYERBOTS_INS_TRAVELNODE,
    PLAYERBOTS_DEL_TRAVELNODE,
    PLAYERBOTS_SEL_TRAVELNODE_LINK,
    PLAYERBOTS_INS_TRAVELNODE_LINK,
    PLAYERBOTS_DEL_TRAVELNODE_LINK,
    PLAYERBOTS_SEL_TRAVELNODE_PATH,
    PLAYERBOTS_INS_TRAVELNODE_PATH,
    PLAYERBOTS_DEL_TRAVELNODE_PATH,

    // Speech and Text Management (4 statements)
    PLAYERBOTS_SEL_SPEECH,
    PLAYERBOTS_SEL_SPEECH_PROBABILITY,
    PLAYERBOTS_SEL_TEXT,
    PLAYERBOTS_SEL_DUNGEON_SUGGESTION,

    // Cache and Utility (4 statements)
    PLAYERBOTS_SEL_TELE_CACHE,
    PLAYERBOTS_INS_TELE_CACHE,
    PLAYERBOTS_SEL_WEIGHTSCALES,
    PLAYERBOTS_SEL_WEIGHTSCALE_DATA,

    // Additional Bot Management (10 statements)
    PLAYERBOTS_SEL_ITEM_INFO_CACHE,
    PLAYERBOTS_INS_ITEM_INFO_CACHE,
    PLAYERBOTS_UPD_ITEM_INFO_CACHE,
    PLAYERBOTS_SEL_ACCOUNT_LINKS,
    PLAYERBOTS_INS_ACCOUNT_LINKS,
    PLAYERBOTS_DEL_ACCOUNT_LINKS,
    PLAYERBOTS_SEL_PREFERRED_MOUNTS,
    PLAYERBOTS_INS_PREFERRED_MOUNTS,
    PLAYERBOTS_SEL_BOT_CONFIG,
    PLAYERBOTS_UPD_BOT_CONFIG,

    MAX_PLAYERBOTS_STATEMENTS
};

class TC_DATABASE_API PlayerbotsDatabaseConnection : public MySQLConnection
{
public:
    typedef PlayerbotsDatabaseStatements Statements;

    PlayerbotsDatabaseConnection(MySQLConnectionInfo& connInfo, ConnectionFlags connectionFlags);
    ~PlayerbotsDatabaseConnection();

    //- Loads database type specific prepared statements
    void DoPrepareStatements() override;
};

#endif