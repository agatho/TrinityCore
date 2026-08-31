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

#ifndef TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H
#define TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"

enum class TransmogOutfitDisplayType : uint8;
enum class TransmogOutfitEntrySource : uint8;
enum class TransmogOutfitSetType : uint8;
enum class TransmogOutfitSlot : int8;
enum class TransmogOutfitSlotOption : uint8;
enum class TransmogOutfitSlotOptionSheatheCategory : uint8;

namespace WorldPackets
{
    namespace Transmogrification
    {
        struct TransmogrifyItem
        {
            int32 ItemModifiedAppearanceID = 0;
            uint32 Slot = 0;
            int32 SpellItemEnchantmentID = 0;
            int32 SecondaryItemModifiedAppearanceID = 0;
        };

        class TransmogrifyItems final : public ClientPacket
        {
        public:
            enum
            {
                MAX_TRANSMOGRIFY_ITEMS = 13
            };

            explicit TransmogrifyItems(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOGRIFY_ITEMS, std::move(packet)) { }

            void Read() override;

            ObjectGuid Npc;
            Array<TransmogrifyItem, MAX_TRANSMOGRIFY_ITEMS> Items;
            bool CurrentSpecOnly = false;
        };

        struct TransmogOutfitDataInfo
        {
            TransmogOutfitSetType SetType = { };
            bool SituationsEnabled = false;
            uint32 Icon = 0;
            std::string_view Name;
        };

        class TransmogOutfitNew final : public ClientPacket
        {
        public:
            explicit TransmogOutfitNew(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_NEW, std::move(packet)) { }

            void Read() override;

            ObjectGuid Npc;
            TransmogOutfitDataInfo Info;
            TransmogOutfitEntrySource Source = { };
        };

        class TransmogOutfitNewEntryAdded final : public ServerPacket
        {
        public:
            explicit TransmogOutfitNewEntryAdded() : ServerPacket(SMSG_TRANSMOG_OUTFIT_NEW_ENTRY_ADDED, 4) { }

            WorldPacket const* Write() override;

            uint32 TransmogOutfitID = 0;
        };

        class TransmogOutfitUpdateInfo final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateInfo(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_INFO, std::move(packet)) { }

            void Read() override;

            uint32 OutfitID = 0;
            ObjectGuid Npc;
            TransmogOutfitDataInfo Info;
        };

        class TransmogOutfitInfoUpdated final : public ServerPacket
        {
        public:
            explicit TransmogOutfitInfoUpdated() : ServerPacket(SMSG_TRANSMOG_OUTFIT_INFO_UPDATED, 4 + 1 + 4 + 1 + 1 + 128) { }

            WorldPacket const* Write() override;

            uint32 TransmogOutfitID = 0;
            TransmogOutfitDataInfo const* OutfitInfo = nullptr;
        };

        struct TransmogOutfitSituationInfo
        {
            uint32 SituationID = 0;
            uint32 SpecID = 0;
            uint32 LoadoutID = 0;
            uint32 EquipmentSetID = 0;
        };

        class TransmogOutfitUpdateSituations final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateSituations(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS, std::move(packet)) { }

            void Read() override;

            uint32 OutfitID = 0;
            ObjectGuid Npc;
            bool SituationsEnabled = false;
            Array<TransmogOutfitSituationInfo, 100> Situations;
        };

        class TransmogOutfitSituationsUpdated final : public ServerPacket
        {
        public:
            explicit TransmogOutfitSituationsUpdated() : ServerPacket(SMSG_TRANSMOG_OUTFIT_SITUATIONS_UPDATED, 4 + 4 + 10 * (4 + 4 + 4 + 4) + 1) { }

            WorldPacket const* Write() override;

            uint32 TransmogOutfitID = 0;
            bool SituationsEnabled = false;
            std::span<TransmogOutfitSituationInfo const> Situations;
        };

        struct TransmogOutfitSlotData
        {
            TransmogOutfitSlot Slot = { };
            TransmogOutfitSlotOption SlotOption = { };
            TransmogOutfitSlotOptionSheatheCategory SheatheCategory = { };
            TransmogOutfitDisplayType AppearanceDisplayType = { };
            TransmogOutfitDisplayType IllusionDisplayType = { };
            uint32 ItemModifiedAppearanceID = 0;
            uint32 SpellItemEnchantmentID = 0;
            uint32 Flags = 0;
        };

        class TransmogOutfitUpdateSlots final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateSlots(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS, std::move(packet)) { }

            void Read() override;

            uint32 OutfitID = 0;
            Array<TransmogOutfitSlotData, 30> Slots;
            ObjectGuid Npc;
            uint64 Cost = 0;
            bool UseAvailableDiscount = false;
        };

        class TransmogOutfitSlotsUpdated final : public ServerPacket
        {
        public:
            explicit TransmogOutfitSlotsUpdated() : ServerPacket(SMSG_TRANSMOG_OUTFIT_SLOTS_UPDATED, 4 + 4 + 30 * (1 + 1 + 4 + 1 + 4 + 1 + 4)) { }

            WorldPacket const* Write() override;

            uint32 TransmogOutfitID = 0;
            std::span<TransmogOutfitSlotData const> Slots;
        };

        class AccountTransmogUpdate final : public ServerPacket
        {
        public:
            explicit AccountTransmogUpdate() : ServerPacket(SMSG_ACCOUNT_TRANSMOG_UPDATE) { }

            WorldPacket const* Write() override;

            bool IsFullUpdate = false;
            bool IsSetFavorite = false;
            std::vector<uint32> FavoriteAppearances;
            std::vector<uint32> NewAppearances;
        };

        // CMSG_CLEAR_NEW_APPEARANCE (12.1 value 0x2A0005) -- "I have seen this wardrobe entry, drop its NEW
        // badge". Wire, from the 12.1.0.69382 client serializer RVA 0x746D30: it writes the opcode
        // (2752517 = 0x2A0005) and then one Write<uint32>. Fixed 8 bytes on the wire, 4 in the body.
        // No 12.1 or 12.0.x capture of this opcode exists in any of the 51 available recordings, so the
        // structure is carried by the serializer, not by reference bytes -- but there is nothing in an
        // 8-byte fixed message left to get wrong.
        //
        // The field is an ItemModifiedAppearance.ID ("source id"), NOT an ItemAppearance.ID ("visual id").
        // C_TransmogCollection.ClearNewAppearance(visualID) (client RVA 0x9D5D80) resolves the visual to a
        // VECTOR of source ids (RVA 0x22D4BD0) and sends ONE packet per element, erasing each from the client's
        // "new" set at 0x43EEB98 first. The key space of that set is settled by the client itself and needs no
        // inference from our field names: besides SMSG_ACCOUNT_TRANSMOG_UPDATE, the set is fed by the
        // ActivePlayerData descriptor handler at RVA 0x22D7BD0, which watches the Transmog bit array and for
        // every bit going 0 -> 1 inserts id = bitIndex + 32 * blockIndex. That is precisely the index
        // TrinityCore writes in CollectionMgr::AddItemAppearance (AddTransmogFlag(ID / 32, 1 << (ID % 32)) with
        // ID = ItemModifiedAppearance.ID), so both feeds of the set agree on ItemModifiedAppearance.ID.
        class ClearNewAppearance final : public ClientPacket
        {
        public:
            explicit ClearNewAppearance(WorldPacket&& packet) : ClientPacket(CMSG_CLEAR_NEW_APPEARANCE, std::move(packet)) { }

            void Read() override;

            uint32 ItemModifiedAppearanceID = 0;
        };
    }
}

#endif // TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H
