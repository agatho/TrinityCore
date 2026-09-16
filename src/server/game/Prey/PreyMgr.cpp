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

#include "PreyMgr.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "Timer.h"
#include "Util.h"
#include "WorldStateMgr.h"
#include <map>

namespace
{
    // Special Nightmare targets placed per week: 3 of the 4 in each of the six captured weeks.
    constexpr std::size_t PREY_SPECIAL_TARGETS_PER_WEEK = 3;

    // A prey POI condition is exactly "world state == constant":
    //   enabled | WorldState <id> | no operator | Equal | Constant <value> | no operator | no further term
    bool DecodeWorldStateEqualsConstant(WorldStateExpressionEntry const* expression, int32& worldStateId, int32& value)
    {
        std::vector<uint8> bytes = HexStrToByteVector(expression->Expression);
        if (bytes.size() != 15)
            return false;

        if (bytes[0] != 1
            || bytes[1] != AsUnderlyingType(WorldStateExpressionValueType::WorldState)
            || bytes[6] != AsUnderlyingType(WorldStateExpressionOperatorType::None)
            || bytes[7] != AsUnderlyingType(WorldStateExpressionComparisonType::Equal)
            || bytes[8] != AsUnderlyingType(WorldStateExpressionValueType::Constant)
            || bytes[13] != AsUnderlyingType(WorldStateExpressionOperatorType::None)
            || bytes[14] != AsUnderlyingType(WorldStateExpressionLogic::None))
            return false;

        worldStateId = int32(bytes[2] | (bytes[3] << 8) | (bytes[4] << 16) | (uint32(bytes[5]) << 24));
        value = int32(bytes[9] | (bytes[10] << 8) | (bytes[11] << 16) | (uint32(bytes[12]) << 24));
        return true;
    }
}

PreyMgr::PreyMgr() = default;
PreyMgr::~PreyMgr() = default;

PreyMgr* PreyMgr::instance()
{
    static PreyMgr instance;
    return &instance;
}

void PreyMgr::Initialize()
{
    uint32 oldMSTime = getMSTime();

    _targets.clear();
    _regularSlots.clear();
    _specialSlots.clear();

    std::map<int32, PreyHuntTarget> targets;
    uint32 poiCount = 0;
    for (AdventureMapPOIEntry const* poi : sAdventureMapPOIStore)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(poi->QuestID);
        if (!quest || quest->GetZoneOrSort() != -int32(QUEST_SORT_PREY))
            continue;

        PlayerConditionEntry const* condition = sPlayerConditionStore.LookupEntry(poi->PlayerConditionID);
        WorldStateExpressionEntry const* expression = condition ? sWorldStateExpressionStore.LookupEntry(condition->WorldStateExpressionID) : nullptr;
        int32 worldStateId = 0;
        int32 slot = 0;
        if (!expression || !DecodeWorldStateEqualsConstant(expression, worldStateId, slot))
        {
            TC_LOG_ERROR("misc", "PreyMgr: AdventureMapPOI {} (quest {}) is not gated by a \"world state == slot\" condition (PlayerCondition {}), skipped",
                poi->ID, poi->QuestID, poi->PlayerConditionID);
            continue;
        }

        PreyHuntTarget& target = targets[worldStateId];
        target.WorldStateId = worldStateId;
        target.Slots.insert(slot);
        if (std::find(target.QuestIds.begin(), target.QuestIds.end(), poi->QuestID) == target.QuestIds.end())
            target.QuestIds.push_back(poi->QuestID);
        if (target.Name.empty())
            target.Name = poi->Title[DEFAULT_LOCALE];
        ++poiCount;
    }

    for (auto& [worldStateId, target] : targets)
    {
        target.IsSpecial = target.Slots.contains(0);
        std::ranges::sort(target.QuestIds);

        if (!WorldStateMgr::GetWorldStateTemplate(worldStateId))
            TC_LOG_ERROR("sql.sql", "PreyMgr: hunt target '{}' uses world state {} which has no `world_state` row - its rotation slot is neither saved nor sent to clients",
                target.Name, worldStateId);

        for (int32 slot : target.Slots)
            if (slot)
                (target.IsSpecial ? _specialSlots : _regularSlots).insert(slot);

        _targets.push_back(std::move(target));
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} prey hunt targets ({} Adventure Map POIs, {} rotation slots) in {} ms",
        _targets.size(), poiCount, _regularSlots.size(), GetMSTimeDiffToNow(oldMSTime));

    if (!_targets.empty() && !HasRotation())
        Rotate();
}

void PreyMgr::OnWeeklyReset()
{
    if (!_targets.empty())
        Rotate();
}

void PreyMgr::Rotate()
{
    std::vector<PreyHuntTarget const*> regular;
    std::vector<PreyHuntTarget const*> special;
    for (PreyHuntTarget const& target : _targets)
        (target.IsSpecial ? special : regular).push_back(&target);

    std::map<int32, int32> slotByWorldState;
    for (PreyHuntTarget const& target : _targets)
        slotByWorldState[target.WorldStateId] = 0;

    // one different regular target per slot
    Trinity::Containers::RandomShuffle(regular);
    auto nextRegular = regular.begin();
    for (int32 slot : _regularSlots)
    {
        auto itr = std::find_if(nextRegular, regular.end(), [slot](PreyHuntTarget const* target) { return target->Slots.contains(slot); });
        if (itr == regular.end())
        {
            TC_LOG_ERROR("misc", "PreyMgr::Rotate: no unused hunt target can fill slot {}", slot);
            continue;
        }

        std::swap(*nextRegular, *itr);
        slotByWorldState[(*nextRegular)->WorldStateId] = slot;
        ++nextRegular;
    }

    // special targets on different Nightmare slots
    std::vector<int32> specialSlots(_specialSlots.begin(), _specialSlots.end());
    Trinity::Containers::RandomShuffle(special);
    Trinity::Containers::RandomShuffle(specialSlots);
    std::size_t specialCount = std::min({ PREY_SPECIAL_TARGETS_PER_WEEK, special.size(), specialSlots.size() });
    for (std::size_t i = 0; i < specialCount; ++i)
        slotByWorldState[special[i]->WorldStateId] = specialSlots[i];

    for (auto const& [worldStateId, slot] : slotByWorldState)
        WorldStateMgr::SetValueAndSaveInDb(worldStateId, slot, false, nullptr);

    TC_LOG_INFO("misc", "PreyMgr: new hunt rotation drawn ({} targets, {} special)", std::min(regular.size(), _regularSlots.size()), specialCount);
}

int32 PreyMgr::GetSlot(PreyHuntTarget const& target) const
{
    return WorldStateMgr::GetValue(target.WorldStateId, nullptr);
}

bool PreyMgr::HasRotation() const
{
    return std::ranges::any_of(_targets, [this](PreyHuntTarget const& target) { return !target.IsSpecial && GetSlot(target) != 0; });
}
