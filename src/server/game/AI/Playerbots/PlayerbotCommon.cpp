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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"
#include "PlayerbotPlayerAI.h"
#include <sstream>
#include <algorithm>

// PlayerbotAction implementation
PlayerbotAction::PlayerbotAction(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotStrategy implementation
PlayerbotStrategy::PlayerbotStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name), _type(0)
{
}

// PlayerbotTrigger implementation
PlayerbotTrigger::PlayerbotTrigger(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotUntypedValue implementation
PlayerbotUntypedValue::PlayerbotUntypedValue(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotMultiplier implementation
PlayerbotMultiplier::PlayerbotMultiplier(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotAIAware(ai), _name(name)
{
}

// PlayerbotUtils implementation
namespace PlayerbotUtils
{
    std::vector<std::string> Split(std::string const& str, char delimiter)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        
        while (std::getline(ss, token, delimiter))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        return tokens;
    }

    std::string Join(std::vector<std::string> const& strings, std::string const& delimiter)
    {
        if (strings.empty())
            return "";
        
        std::stringstream ss;
        for (size_t i = 0; i < strings.size(); ++i)
        {
            if (i > 0)
                ss << delimiter;
            ss << strings[i];
        }
        
        return ss.str();
    }

    std::string ToLower(std::string const& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    std::string Trim(std::string const& str)
    {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
}

#endif // WITH_PLAYERBOTS