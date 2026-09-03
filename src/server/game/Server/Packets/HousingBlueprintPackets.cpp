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

#include "HousingBlueprintPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Housing
{
// ---------------------------------------------------------------------------
// JAM struct writers. Field ORDER/NAME/OFFSET is binary-verified (spec §2); the
// scalar WIDTHS and string length-prefix widths are inferred (offline ceiling),
// so this framing is [INF] and must be confirmed against a 12.1 capture before a
// live 12.1 realm relies on it. Wire order = ascending in-memory offset.
// ---------------------------------------------------------------------------
static void WriteBlueprint(WorldPacket& data, JamHousingBlueprint const& bp)
{
    data << uint32(bp.Id);
    data << SizedString::BitsSize<7>(bp.Uuid);
    data << SizedString::BitsSize<7>(bp.Name);
    data << uint32(bp.Type);
    data << int64(bp.DateCreated);
    data << int64(bp.DateDeleted);
    data << uint8(bp.Flags);
    data << uint8(bp.DeleteReason);
    data << SizedString::Data(bp.Uuid);
    data << SizedString::Data(bp.Name);
}

static void WriteUInt32Vector(WorldPacket& data, std::vector<uint32> const& v)
{
    data << uint32(v.size());
    for (uint32 id : v)
        data << uint32(id);
}

static void WriteItemList(WorldPacket& data, JamBlueprintItemList const& list)
{
    WriteUInt32Vector(data, list.DecorIDs);
    WriteUInt32Vector(data, list.DyeItemIDs);
    WriteUInt32Vector(data, list.RoomIDs);
    WriteUInt32Vector(data, list.FixtureIDs);
}

static void WriteBudgetEntry(WorldPacket& data, JamHouseBudgetEntry const& e)
{
    data << uint32(e.BudgetType);
    data << int32(e.Max);
    data << int32(e.Current);
    data << int32(e.Cost);
}

// ===================== CMSG Read() =====================

void HousingBlueprintRequestContents::Read()
{
    _worldPacket >> BlueprintId;
}

void HousingBlueprintExport::Read()
{
    // wire: bits<6> u8 pguid Blob  (spec §4)
    _worldPacket >> SizedString::BitsSize<6>(Name);
    _worldPacket >> TypeByte;
    _worldPacket >> HouseGuid;
    _worldPacket >> SizedString::Data(Name);
}

void HousingBlueprintExportRoom::Read()
{
    // wire: bits<24> bits<1> u8 pguid Blob  (spec §4)
    _worldPacket >> SizedString::BitsSize<24>(Name);
    _worldPacket >> Bits<1>(Flag);
    _worldPacket >> TypeByte;
    _worldPacket >> RoomGuid;
    _worldPacket >> SizedString::Data(Name);
}

void HousingBlueprintRename::Read()
{
    // wire: u64 bits<6> Blob  (spec §4)
    _worldPacket >> BlueprintId;
    _worldPacket >> SizedString::BitsSize<6>(Name);
    _worldPacket >> SizedString::Data(Name);
}

void HousingBlueprintDelete::Read()
{
    _worldPacket >> BlueprintId;
}

void HousingBlueprintImport::Read()
{
    // wire: bits<24> bits<1> u8 pguid u32 Blob  (spec §4)
    _worldPacket >> SizedString::BitsSize<24>(Code);
    _worldPacket >> Bits<1>(Flag);
    _worldPacket >> TypeByte;
    _worldPacket >> HouseGuid;
    _worldPacket >> BlueprintId;
    _worldPacket >> SizedString::Data(Code);
}

// ===================== SMSG Write() =====================

WorldPacket const* HousingBlueprintCollection::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Blueprints.size());
    for (JamHousingBlueprint const& bp : Blueprints)
        WriteBlueprint(_worldPacket, bp);
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintContents::Write()
{
    _worldPacket << uint32(Result);
    WriteBlueprint(_worldPacket, Blueprint);
    WriteItemList(_worldPacket, Items);
    _worldPacket << uint32(MissingItems.size());
    for (JamBlueprintMissingItem const& m : MissingItems)
    {
        _worldPacket << int32(m.Id);
        _worldPacket << int32(m.Count);
    }
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintExportResult::Write()
{
    _worldPacket << uint32(Result);
    WriteBlueprint(_worldPacket, Blueprint);
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintImportResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    WriteItemList(_worldPacket, Items);
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintDeleteResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint64(BlueprintId);
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintRenameResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint64(BlueprintId);
    _worldPacket << SizedString::BitsSize<7>(Name);
    _worldPacket << SizedString::Data(Name);
    return &_worldPacket;
}

WorldPacket const* HousingBlueprintsAvailabilityChanged::Write()
{
    _worldPacket << Bits<1>(Available);
    _worldPacket << uint32(MaxPerBnetAccount);
    _worldPacket << uint32(MaxBackupsPerBnetAccount);
    return &_worldPacket;
}

WorldPacket const* HousingHouseBudgetsUpdate::Write()
{
    // JamHouseBudgets: interiorBudgets@0, exteriorBudgets@24 (spec §2). HouseGuid is a
    // convenience prefix so the client can associate the update; framing is [INF].
    _worldPacket << HouseGuid;
    _worldPacket << uint32(InteriorBudgets.size());
    for (JamHouseBudgetEntry const& e : InteriorBudgets)
        WriteBudgetEntry(_worldPacket, e);
    _worldPacket << uint32(ExteriorBudgets.size());
    for (JamHouseBudgetEntry const& e : ExteriorBudgets)
        WriteBudgetEntry(_worldPacket, e);
    return &_worldPacket;
}
}
