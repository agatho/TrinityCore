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

/* ScriptData
Name: mythic_plus_commandscript
%Complete: 100
Comment: Mythic+ (Challenge Mode) GM/testing commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "ChallengeModeMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DB2Stores.h"
#include "Item.h"
#include "ItemDefines.h"
#include "Player.h"
#include "RBAC.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class mythic_plus_commandscript : public CommandScript
{
public:
    mythic_plus_commandscript() : CommandScript("mythic_plus_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable mythicPlusCommandTable =
        {
            { "keystone", HandleMythicPlusKeystoneCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "affixes",  HandleMythicPlusAffixesCommand,  rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "mythicplus", mythicPlusCommandTable },
        };
        return commandTable;
    }

    // .mythicplus keystone <level> [challengeModeId] — create (or restamp) the selected player's keystone with
    // the current week's affixes; the dungeon is rolled from the season pool unless given explicitly.
    static bool HandleMythicPlusKeystoneCommand(ChatHandler* handler, uint32 level, Optional<uint32> challengeModeId)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;

        uint32 dungeon = challengeModeId.value_or(0);
        if (dungeon && !sChallengeModeMgr.GetMapChallengeMode(dungeon))
        {
            handler->PSendSysMessage("Unknown MapChallengeMode id {}.", dungeon);
            return false;
        }
        if (!dungeon)
            dungeon = sChallengeModeMgr.RollSeasonDungeon();
        if (!dungeon)
        {
            handler->SendSysMessage("No Mythic+ season dungeon pool available (check MapChallengeMode.db2 / season config).");
            return false;
        }

        Item* keystone = sChallengeModeMgr.CreateOrUpdateKeystone(target, dungeon, level);
        if (!keystone)
        {
            handler->SendSysMessage("Failed to create the keystone (bag space / keystone item id config).");
            return false;
        }

        handler->PSendSysMessage("Keystone for {}: dungeon {} level {} (affixes {}/{}/{}/{}).", target->GetName(),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_MAP_CHALLENGE_MODE_ID),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_LEVEL),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_1),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_2),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_3),
            keystone->GetModifier(ITEM_MODIFIER_CHALLENGE_KEYSTONE_AFFIX_ID_4));
        return true;
    }

    // .mythicplus affixes — print the current rotation week and the advertised weekly affix set.
    static bool HandleMythicPlusAffixesCommand(ChatHandler* handler)
    {
        std::vector<uint32> const weekly = sChallengeModeMgr.GetWeeklyAffixes();

        std::string list;
        for (uint32 affixId : weekly)
        {
            if (!list.empty())
                list += ", ";
            list += std::to_string(affixId);
        }

        handler->PSendSysMessage("Mythic+ rotation week {}: weekly affixes [{}].",
            sChallengeModeMgr.GetCurrentWeekIndex(), list.empty() ? "none" : list);
        return true;
    }
};

void AddSC_mythic_plus_commandscript()
{
    new mythic_plus_commandscript();
}
