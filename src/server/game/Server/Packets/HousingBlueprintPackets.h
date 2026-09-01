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

// ============================================================================
// Patch 12.1.0 (build 69299) Housing Blueprint + Budget packets.
// RE spec: c:\dumps\tools\dump121\housing\housing_12_1_spec.md
//
// CMSG wire = client serializer sweep (cmsg_layouts_69299.json) — [BIN].
// SMSG bodies follow the JAM reflection field layout (names/order/offsets [BIN];
//   scalar widths + which SMSG carries which JAM type [INF]). SMSG serializers were
//   not swept offline, so the exact SMSG framing is inferred (spec §4). These packets
//   are NOT wired into the live 68275 opcode table (spec §7); binding activates at the
//   TC-wide 12.1 opcode migration.
// ============================================================================

#ifndef TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H
#define TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"
#include <string>
#include <vector>

namespace WorldPackets::Housing
{
    // JamHousingBlueprint (tag 0x3928b18, count=8) — spec §2.
    struct JamHousingBlueprint
    {
        uint32 Id = 0;                  // +0
        std::string Uuid;               // +8   export uuid (blz::string)
        std::string Name;               // +48  player-set name
        uint32 Type = 0;                // +88  HousingBlueprintType
        int64 DateCreated = 0;          // +96
        int64 DateDeleted = 0;          // +104
        uint8 Flags = 0;                // +112 HousingBlueprintFlags (opaque)
        uint8 DeleteReason = 0;         // +113
    };

    // JamHouseBudgetEntry (tag 0x3928b78, count=4) — spec §2. [BIN layout]
    struct JamHouseBudgetEntry
    {
        uint32 BudgetType = 0;          // +0 HouseBudgetType
        int32 Max = 0;                  // +4
        int32 Current = 0;              // +8
        int32 Cost = 0;                 // +12
    };

    // JamBlueprintItemList (tag 0x3928bf8, count=4) — spec §2. [BIN layout]
    struct JamBlueprintItemList
    {
        std::vector<uint32> DecorIDs;   // +0
        std::vector<uint32> DyeItemIDs; // +24
        std::vector<uint32> RoomIDs;    // +48
        std::vector<uint32> FixtureIDs; // +72
    };

    // JamBlueprintMissingItem (tag 0x39291e8, count=2) — spec §2. [BIN layout]
    struct JamBlueprintMissingItem
    {
        int32 Id = 0;                   // +0
        int32 Count = 0;                // +4
    };

    // ===================== CMSG =====================

    // CMSG_HOUSING_BLUEPRINT_REQUEST_COLLECTION 0x310001 — wire: (empty) [BIN]
    class HousingBlueprintRequestCollection final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestCollection(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_COLLECTION, std::move(packet)) { }
        void Read() override { }
    };

    // CMSG_HOUSING_BLUEPRINT_REQUEST_CONTENTS 0x310003 — wire: u64 [BIN]
    class HousingBlueprintRequestContents final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestContents(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_CONTENTS, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintId = 0;
    };

    // CMSG_HOUSING_BLUEPRINT_EXPORT 0x310000 — wire: bits<6> u8 pguid Blob [BIN]
    // bits<6> = Name length prefix (SizedString), u8 = Type/flags byte, pguid = HouseGuid,
    // Blob = Name bytes. Field roles are inferred (spec §4).
    class HousingBlueprintExport final : public ClientPacket
    {
    public:
        explicit HousingBlueprintExport(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_EXPORT, std::move(packet)) { }
        void Read() override;

        std::string Name;
        uint8 TypeByte = 0;
        ObjectGuid HouseGuid;
    };

    // CMSG_HOUSING_BLUEPRINT_EXPORT_ROOM 0x310008 — wire: bits<24> bits<1> u8 pguid Blob [BIN]
    class HousingBlueprintExportRoom final : public ClientPacket
    {
    public:
        explicit HousingBlueprintExportRoom(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_EXPORT_ROOM, std::move(packet)) { }
        void Read() override;

        std::string Name;               // bits<24> length prefix + Blob data
        bool Flag = false;              // bits<1>
        uint8 TypeByte = 0;             // u8
        ObjectGuid RoomGuid;            // pguid
    };

    // CMSG_HOUSING_BLUEPRINT_RENAME 0x310002 — wire: u64 bits<6> Blob [BIN]
    class HousingBlueprintRename final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRename(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_RENAME, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintId = 0;
        std::string Name;
    };

    // CMSG_HOUSING_BLUEPRINT_IMPORT 0x310005 — wire: bits<24> bits<1> u8 pguid u32 Blob [BIN]
    class HousingBlueprintImport final : public ClientPacket
    {
    public:
        explicit HousingBlueprintImport(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_IMPORT, std::move(packet)) { }
        void Read() override;

        std::string Code;               // bits<24> length prefix + Blob data (serialized blueprint code)
        bool Flag = false;              // bits<1> (e.g. keep-backup)
        uint8 TypeByte = 0;             // u8
        ObjectGuid HouseGuid;           // pguid (target house)
        uint32 BlueprintId = 0;         // u32
    };

    // ===================== SMSG =====================
    // Bodies follow the JAM reflection layout; SMSG framing is [INF] (spec §4).

    // SMSG_HOUSING_BLUEPRINT_COLLECTION 0x540000
    class HousingBlueprintCollection final : public ServerPacket
    {
    public:
        HousingBlueprintCollection() : ServerPacket(SMSG_HOUSING_BLUEPRINT_COLLECTION) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;
        std::vector<JamHousingBlueprint> Blueprints;
    };

    // SMSG_HOUSING_BLUEPRINT_CONTENTS 0x540001
    class HousingBlueprintContents final : public ServerPacket
    {
    public:
        HousingBlueprintContents() : ServerPacket(SMSG_HOUSING_BLUEPRINT_CONTENTS) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;
        JamHousingBlueprint Blueprint;
        JamBlueprintItemList Items;
        std::vector<JamBlueprintMissingItem> MissingItems;
    };

    // SMSG_HOUSING_BLUEPRINT_EXPORT_RESULT 0x540002
    class HousingBlueprintExportResult final : public ServerPacket
    {
    public:
        HousingBlueprintExportResult() : ServerPacket(SMSG_HOUSING_BLUEPRINT_EXPORT_RESULT) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;
        JamHousingBlueprint Blueprint;
    };

    // SMSG_HOUSING_BLUEPRINT_IMPORT_RESULT 0x540003
    class HousingBlueprintImportResult final : public ServerPacket
    {
    public:
        HousingBlueprintImportResult() : ServerPacket(SMSG_HOUSING_BLUEPRINT_IMPORT_RESULT) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;              // 0 = success; else HousingBlueprintUnmetRequirementFlags
        ObjectGuid HouseGuid;
        JamBlueprintItemList Items;
    };

    // SMSG_HOUSING_BLUEPRINT_DELETE_RESULT 0x540004
    class HousingBlueprintDeleteResult final : public ServerPacket
    {
    public:
        HousingBlueprintDeleteResult() : ServerPacket(SMSG_HOUSING_BLUEPRINT_DELETE_RESULT) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;
        uint64 BlueprintId = 0;
    };

    // SMSG_HOUSING_BLUEPRINT_RENAME_RESULT 0x540005
    class HousingBlueprintRenameResult final : public ServerPacket
    {
    public:
        HousingBlueprintRenameResult() : ServerPacket(SMSG_HOUSING_BLUEPRINT_RENAME_RESULT) { }
        WorldPacket const* Write() override;

        uint32 Result = 0;
        uint64 BlueprintId = 0;
        std::string Name;
    };

    // SMSG_HOUSING_BLUEPRINTS_AVAILABILITY_CHANGED 0x540007
    class HousingBlueprintsAvailabilityChanged final : public ServerPacket
    {
    public:
        HousingBlueprintsAvailabilityChanged() : ServerPacket(SMSG_HOUSING_BLUEPRINTS_AVAILABILITY_CHANGED) { }
        WorldPacket const* Write() override;

        bool Available = false;
        uint32 MaxPerBnetAccount = 0;
        uint32 MaxBackupsPerBnetAccount = 0;
    };

    // SMSG_HOUSING_HOUSE_BUDGETS_UPDATE 0x620000 — JamHouseBudgets (spec §2).
    class HousingHouseBudgetsUpdate final : public ServerPacket
    {
    public:
        HousingHouseBudgetsUpdate() : ServerPacket(SMSG_HOUSING_HOUSE_BUDGETS_UPDATE) { }
        WorldPacket const* Write() override;

        ObjectGuid HouseGuid;
        std::vector<JamHouseBudgetEntry> InteriorBudgets;
        std::vector<JamHouseBudgetEntry> ExteriorBudgets;
    };
}

#endif // TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H
