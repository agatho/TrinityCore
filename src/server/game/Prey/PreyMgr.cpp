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
#include "MapUtils.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Util.h"
#include "WorldStateMgr.h"

namespace
{
    // Special Nightmare targets placed per week: 3 of the 4 in each of the seven captured weeks.
    constexpr std::size_t PREY_SPECIAL_TARGETS_PER_WEEK = 3;

    // The "<prey> slain" objective is the hunt quest's second objective; its POI blobs mark the prey per zone.
    constexpr int32 PREY_SLAIN_OBJECTIVE_INDEX = 1;

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

    // Slots 1..12 cycle Normal / Hard / Nightmare per zone, 13 and 14 are extra Hard positions and slot 0 only
    // carries the special Nightmare targets.
    PreyHuntDifficulty GetSlotDifficulty(int32 slot)
    {
        if (slot == 0)
            return PreyHuntDifficulty::Nightmare;
        if (slot > 12)
            return PreyHuntDifficulty::Hard;

        switch (slot % 3)
        {
            case 1: return PreyHuntDifficulty::Normal;
            case 2: return PreyHuntDifficulty::Hard;
            default: return PreyHuntDifficulty::Nightmare;
        }
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
    _hunts.clear();
    _huntsByPrey.clear();

    std::map<int32, PreyHuntTarget> targets;
    std::map<uint32, std::set<int32>> questSlots;
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
        questSlots[poi->QuestID].insert(slot);
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

    // _targets is final from here on, hunts may point into it
    for (PreyHuntTarget const& target : _targets)
    {
        for (uint32 questId : target.QuestIds)
        {
            PreyHuntDifficulty difficulty = PreyHuntDifficulty::None;
            for (int32 slot : questSlots[questId])
            {
                PreyHuntDifficulty slotDifficulty = GetSlotDifficulty(slot);
                if (difficulty != PreyHuntDifficulty::None && difficulty != slotDifficulty)
                    TC_LOG_ERROR("misc", "PreyMgr: hunt quest {} ({}) is shown on slots of different difficulties", questId, target.Name);
                difficulty = slotDifficulty;
            }

            PreyHunt& hunt = _hunts[questId];
            hunt.QuestId = questId;
            hunt.Target = &target;
            hunt.Difficulty = difficulty;
        }
    }

    LoadZones();
    LoadSlots();
    LoadTargets();
    LoadHunts();
    LoadSpells();

    TC_LOG_INFO("server.loading", ">> Loaded {} prey hunt targets ({} hunts, {} Adventure Map POIs, {} rotation slots, {} zones) in {} ms",
        _targets.size(), _hunts.size(), poiCount, _regularSlots.size(), _zones.size(), GetMSTimeDiffToNow(oldMSTime));

    if (!_targets.empty() && !HasRotation())
        Rotate();
}

void PreyMgr::LoadZones()
{
    _zones.clear();

    //                                                 0          1       2        3
    QueryResult result = WorldDatabase.Query("SELECT ZoneIndex, AreaId, UiMapId, Name FROM prey_hunt_zone");
    if (!result)
    {
        TC_LOG_ERROR("sql.sql", "PreyMgr: `prey_hunt_zone` is empty, hunts cannot find their zone");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        PreyHuntZone zone;
        zone.ZoneIndex = fields[0].GetUInt8();
        zone.AreaId = fields[1].GetUInt32();
        zone.UiMapId = fields[2].GetUInt32();
        zone.Name = fields[3].GetString();

        if (!sAreaTableStore.LookupEntry(zone.AreaId))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_zone` references non-existing area {} for zone {}, skipped", zone.AreaId, zone.ZoneIndex);
            continue;
        }

        if (!sUiMapStore.LookupEntry(zone.UiMapId))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_zone` references non-existing UiMap {} for zone {}, skipped", zone.UiMapId, zone.ZoneIndex);
            continue;
        }

        _zones[zone.ZoneIndex] = std::move(zone);
    } while (result->NextRow());
}

void PreyMgr::LoadSlots()
{
    _slotZones.clear();

    //                                                 0     1
    QueryResult result = WorldDatabase.Query("SELECT Slot, ZoneIndex FROM prey_hunt_slot");
    if (!result)
    {
        TC_LOG_ERROR("sql.sql", "PreyMgr: `prey_hunt_slot` is empty, hunts cannot find their zone");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        int32 slot = fields[0].GetInt32();
        uint8 zoneIndex = fields[1].GetUInt8();
        if (!_zones.contains(zoneIndex))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_slot` assigns slot {} to unknown zone {}, skipped", slot, zoneIndex);
            continue;
        }

        _slotZones[slot] = zoneIndex;
    } while (result->NextRow());

    for (int32 slot : _regularSlots)
        if (!_slotZones.contains(slot))
            TC_LOG_ERROR("sql.sql", "PreyMgr: rotation slot {} has no `prey_hunt_slot` row, hunts started from it have no zone", slot);
}

void PreyMgr::LoadTargets()
{
    //                                                 0             1            2
    QueryResult result = WorldDatabase.Query("SELECT WorldStateId, TargetIndex, PursuitSpellId FROM prey_hunt_target");
    if (!result)
    {
        TC_LOG_ERROR("sql.sql", "PreyMgr: `prey_hunt_target` is empty, hunts cannot publish their target");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        int32 worldStateId = fields[0].GetInt32();
        auto itr = std::ranges::find(_targets, worldStateId, &PreyHuntTarget::WorldStateId);
        if (itr == _targets.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_target` references world state {} which gates no prey Adventure Map POI, skipped", worldStateId);
            continue;
        }

        itr->TargetIndex = fields[1].GetUInt8();
        itr->PursuitSpellId = fields[2].GetUInt32();
        if (itr->PursuitSpellId && !sSpellMgr->GetSpellInfo(itr->PursuitSpellId, DIFFICULTY_NONE))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_target` references non-existing pursuit spell {} for '{}', set to 0", itr->PursuitSpellId, itr->Name);
            itr->PursuitSpellId = 0;
        }
    } while (result->NextRow());

    for (PreyHuntTarget const& target : _targets)
        if (!target.TargetIndex)
            TC_LOG_ERROR("sql.sql", "PreyMgr: hunt target '{}' (world state {}) has no `prey_hunt_target` row", target.Name, target.WorldStateId);
}

void PreyMgr::LoadHunts()
{
    //                                                 0        1
    QueryResult result = WorldDatabase.Query("SELECT QuestId, PreyEntry FROM prey_hunt");
    if (!result)
    {
        TC_LOG_ERROR("sql.sql", "PreyMgr: `prey_hunt` is empty, hunts have no prey to fight");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 questId = fields[0].GetUInt32();
        uint32 preyEntry = fields[1].GetUInt32();

        auto itr = _hunts.find(questId);
        if (itr == _hunts.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt` references quest {} which is no prey hunt on the Hunt Table, skipped", questId);
            continue;
        }

        if (!sObjectMgr->GetCreatureTemplate(preyEntry))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt` references non-existing creature {} for quest {}, skipped", preyEntry, questId);
            continue;
        }

        itr->second.PreyEntry = preyEntry;
        _huntsByPrey[preyEntry] = &itr->second;
    } while (result->NextRow());

    for (auto const& [questId, hunt] : _hunts)
        if (!hunt.PreyEntry)
            TC_LOG_ERROR("sql.sql", "PreyMgr: hunt quest {} ({}) has no `prey_hunt` row", questId, hunt.Target->Name);
}

void PreyMgr::LoadSpells()
{
    for (PreyHuntTarget& target : _targets)
    {
        target.RemnantSpells.clear();
        target.PreySpells.clear();
    }

    //                                                 0             1     2        3           4           5          6
    QueryResult result = WorldDatabase.Query("SELECT WorldStateId, Role, SpellId, InitialMin, InitialMax, RepeatMin, RepeatMax FROM prey_hunt_spell");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        int32 worldStateId = fields[0].GetInt32();
        uint8 role = fields[1].GetUInt8();

        auto itr = std::ranges::find(_targets, worldStateId, &PreyHuntTarget::WorldStateId);
        if (itr == _targets.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_spell` references world state {} which gates no prey Adventure Map POI, skipped", worldStateId);
            continue;
        }

        PreyHuntSpell spell;
        spell.SpellId = fields[2].GetUInt32();
        spell.InitialMin = Milliseconds(fields[3].GetUInt32());
        spell.InitialMax = Milliseconds(fields[4].GetUInt32());
        spell.RepeatMin = Milliseconds(fields[5].GetUInt32());
        spell.RepeatMax = Milliseconds(fields[6].GetUInt32());

        if (!sSpellMgr->GetSpellInfo(spell.SpellId, DIFFICULTY_NONE))
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_spell` references non-existing spell {} for '{}', skipped", spell.SpellId, itr->Name);
            continue;
        }

        if (spell.InitialMax < spell.InitialMin || spell.RepeatMax < spell.RepeatMin)
        {
            TC_LOG_ERROR("sql.sql", "Table `prey_hunt_spell` has a timer range with max < min for spell {} ('{}'), skipped", spell.SpellId, itr->Name);
            continue;
        }

        switch (PreyHuntSpellRole(role))
        {
            case PreyHuntSpellRole::Remnant:
                itr->RemnantSpells.push_back(spell);
                break;
            case PreyHuntSpellRole::Prey:
                itr->PreySpells.push_back(spell);
                break;
            default:
                TC_LOG_ERROR("sql.sql", "Table `prey_hunt_spell` has unknown Role {} for spell {}, skipped", role, spell.SpellId);
                break;
        }
    } while (result->NextRow());
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

PreyHunt const* PreyMgr::GetHunt(uint32 questId) const
{
    return Trinity::Containers::MapGetValuePtr(_hunts, questId);
}

PreyHunt const* PreyMgr::GetHuntByPreyEntry(uint32 creatureEntry) const
{
    auto itr = _huntsByPrey.find(creatureEntry);
    return itr != _huntsByPrey.end() ? itr->second : nullptr;
}

PreyHuntZone const* PreyMgr::GetZone(uint8 zoneIndex) const
{
    return Trinity::Containers::MapGetValuePtr(_zones, zoneIndex);
}

PreyHuntZone const* PreyMgr::GetZoneForNewHunt(PreyHunt const& hunt) const
{
    int32 slot = GetSlot(*hunt.Target);
    if (!slot)
        return nullptr;

    auto itr = _slotZones.find(slot);
    if (itr == _slotZones.end())
        return nullptr;

    return GetZone(itr->second);
}

bool PreyMgr::GetPreyLocation(PreyHunt const& hunt, PreyHuntZone const& zone, uint32& mapId, Position& position) const
{
    QuestPOIData const* poiData = sObjectMgr->GetQuestPOIData(hunt.QuestId);
    if (!poiData)
        return false;

    for (QuestPOIBlobData const& blob : poiData->Blobs)
    {
        if (blob.ObjectiveIndex != PREY_SLAIN_OBJECTIVE_INDEX || uint32(blob.UiMapID) != zone.UiMapId || blob.Points.empty())
            continue;

        float x = 0.0f;
        float y = 0.0f;
        for (QuestPOIBlobPoint const& point : blob.Points)
        {
            x += float(point.X);
            y += float(point.Y);
        }

        mapId = uint32(blob.MapID);
        position.Relocate(x / blob.Points.size(), y / blob.Points.size(), 0.0f);
        return true;
    }

    return false;
}

bool PreyMgr::HasRotation() const
{
    return std::ranges::any_of(_targets, [this](PreyHuntTarget const& target) { return !target.IsSpecial && GetSlot(target) != 0; });
}
