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

/*
 * CMSG wire-layout tests.
 *
 * Every layout in this file is the field sequence the *client* writes for that
 * opcode, transcribed from the client's own serializer in build 68275 by the
 * offline sweep in tools/cmsg_sweep. Only opcodes the sweep rates HIGH
 * confidence AND straight-line (no branches, no nested writer, no unresolved
 * blob) are used - on that subset the sweep measured 99.4% byte-exact agreement
 * with implemented readers, and in every straight-line disagreement that was
 * hand-checked against the disassembly the client transcription was right.
 *
 * The layout comment above each case is the sweep's token string. It is the
 * expectation; our Read() is what is under test. See PacketLayoutFixture.h.
 */

#include "PacketLayoutFixture.h"

#include "AreaTriggerPackets.h"
#include "AzeritePackets.h"
#include "BattlePetPackets.h"
#include "BlackMarketPackets.h"
#include "CalendarPackets.h"
#include "CharacterPackets.h"
#include "EquipmentSetPackets.h"
#include "GuildPackets.h"
#include "LootPackets.h"
#include "MailPackets.h"
#include "MiscPackets.h"
#include "NPCPackets.h"
#include "PetPackets.h"
#include "QueryPackets.h"
#include "QuestPackets.h"
#include "SpellPackets.h"
#include "TaxiPackets.h"
#include "TotemPackets.h"
#include "ToyPackets.h"
#include "TradePackets.h"

using namespace PacketLayout;

TEST_CASE("CMSG layout: guid-shaped payloads", "[packet][layout]")
{
    SECTION("CMSG_BATTLE_PET_CLEAR_FANFARE = pguid")
    {
        Wire wire;
        wire.PGuid(LongGuid());

        AssertLayout<WorldPackets::BattlePet::BattlePetClearFanfare>(CMSG_BATTLE_PET_CLEAR_FANFARE, wire);

        WorldPackets::BattlePet::BattlePetClearFanfare packet(MakePacket(CMSG_BATTLE_PET_CLEAR_FANFARE, wire.Bytes()));
        packet.Read();
        CHECK(packet.PetGuid == LongGuid());
    }

    SECTION("CMSG_ACTIVATE_TAXI = pguid u32 u32 u32")
    {
        Wire wire;
        wire.PGuid(ShortGuid()).U32(0x11223344).U32(0x22334455).U32(0x33445566);

        AssertLayout<WorldPackets::Taxi::ActivateTaxi>(CMSG_ACTIVATE_TAXI, wire);

        WorldPackets::Taxi::ActivateTaxi packet(MakePacket(CMSG_ACTIVATE_TAXI, wire.Bytes()));
        packet.Read();
        CHECK(packet.Vendor == ShortGuid());
        CHECK(packet.Node == 0x11223344u);
        CHECK(packet.GroundMountID == 0x22334455u);
        CHECK(packet.FlyingMountID == 0x33445566u);
    }

    SECTION("CMSG_AUTO_STORE_GUILD_BANK_ITEM = pguid u8 u8")
    {
        Wire wire;
        wire.PGuid(LongGuid()).U8(0x11).U8(0x22);

        AssertLayout<WorldPackets::Guild::AutoStoreGuildBankItem>(CMSG_AUTO_STORE_GUILD_BANK_ITEM, wire);

        WorldPackets::Guild::AutoStoreGuildBankItem packet(MakePacket(CMSG_AUTO_STORE_GUILD_BANK_ITEM, wire.Bytes()));
        packet.Read();
        CHECK(packet.Banker == LongGuid());
        CHECK(packet.BankTab == 0x11);
        CHECK(packet.BankSlot == 0x22);
    }

    SECTION("CMSG_LOOT_ROLL = pguid u8 u8")
    {
        Wire wire;
        wire.PGuid(OtherGuid()).U8(0x33).U8(0x44);

        AssertLayout<WorldPackets::Loot::LootRoll>(CMSG_LOOT_ROLL, wire);

        WorldPackets::Loot::LootRoll packet(MakePacket(CMSG_LOOT_ROLL, wire.Bytes()));
        packet.Read();
        CHECK(packet.LootObj == OtherGuid());
        CHECK(packet.LootListID == 0x33);
        CHECK(packet.RollType == 0x44);
    }

    SECTION("CMSG_MERGE_GUILD_BANK_ITEM_WITH_GUILD_BANK_ITEM = pguid u8 u8 u8 u8 u32")
    {
        Wire wire;
        wire.PGuid(LongGuid()).U8(0x11).U8(0x22).U8(0x33).U8(0x44).U32(0x55667788);

        AssertLayout<WorldPackets::Guild::MergeGuildBankItemWithGuildBankItem>(CMSG_MERGE_GUILD_BANK_ITEM_WITH_GUILD_BANK_ITEM, wire);

        WorldPackets::Guild::MergeGuildBankItemWithGuildBankItem packet(MakePacket(CMSG_MERGE_GUILD_BANK_ITEM_WITH_GUILD_BANK_ITEM, wire.Bytes()));
        packet.Read();
        CHECK(packet.Banker == LongGuid());
        CHECK(packet.BankTab == 0x11);
        CHECK(packet.BankSlot == 0x22);
        CHECK(packet.BankTab1 == 0x33);
        CHECK(packet.BankSlot1 == 0x44);
        CHECK(packet.StackCount == 0x55667788u);
    }

    SECTION("CMSG_SPLIT_GUILD_BANK_ITEM = pguid u8 u8 u8 u8 u32")
    {
        Wire wire;
        wire.PGuid(ShortGuid()).U8(0x44).U8(0x33).U8(0x22).U8(0x11).U32(0x11223344);

        AssertLayout<WorldPackets::Guild::SplitGuildBankItem>(CMSG_SPLIT_GUILD_BANK_ITEM, wire);

        WorldPackets::Guild::SplitGuildBankItem packet(MakePacket(CMSG_SPLIT_GUILD_BANK_ITEM, wire.Bytes()));
        packet.Read();
        CHECK(packet.Banker == ShortGuid());
        CHECK(packet.BankTab == 0x44);
        CHECK(packet.BankSlot == 0x33);
        CHECK(packet.BankTab1 == 0x22);
        CHECK(packet.BankSlot1 == 0x11);
        CHECK(packet.StackCount == 0x11223344u);
    }

    SECTION("CMSG_SAVE_GUILD_EMBLEM = pguid u32 u32 u32 u32 u32")
    {
        // Read order is Vendor, EStyle, EColor, BStyle, BColor, Bg.
        Wire wire;
        wire.PGuid(LongGuid()).U32(0x11223344).U32(0x22334455).U32(0x33445566).U32(0x44556677).U32(0x55667788);

        AssertLayout<WorldPackets::Guild::SaveGuildEmblem>(CMSG_SAVE_GUILD_EMBLEM, wire);

        WorldPackets::Guild::SaveGuildEmblem packet(MakePacket(CMSG_SAVE_GUILD_EMBLEM, wire.Bytes()));
        packet.Read();
        CHECK(packet.Vendor == LongGuid());
        CHECK(packet.EStyle == 0x11223344);
        CHECK(packet.EColor == 0x22334455);
        CHECK(packet.BStyle == 0x33445566);
        CHECK(packet.BColor == 0x44556677);
        CHECK(packet.Bg == 0x55667788);
    }

    SECTION("CMSG_QUEST_PUSH_RESULT = pguid u32 u32")
    {
        Wire wire;
        wire.PGuid(OtherGuid()).U32(0x11223344).U32(0x22334455);

        AssertLayout<WorldPackets::Quest::QuestPushResult>(CMSG_QUEST_PUSH_RESULT, wire);

        WorldPackets::Quest::QuestPushResult packet(MakePacket(CMSG_QUEST_PUSH_RESULT, wire.Bytes()));
        packet.Read();
        CHECK(packet.SenderGUID == OtherGuid());
        CHECK(packet.QuestID == 0x11223344u);
        CHECK(packet.Result == 0x22334455u);
    }
}

TEST_CASE("CMSG layout: scalars ahead of the guid", "[packet][layout]")
{
    // Field order matters as much as field width: these are the shapes where
    // reordering a guid past a scalar still produces a plausible-looking parse.

    SECTION("CMSG_QUERY_GAME_OBJECT = u32 pguid")
    {
        Wire wire;
        wire.U32(0x11223344).PGuid(LongGuid());

        AssertLayout<WorldPackets::Query::QueryGameObject>(CMSG_QUERY_GAME_OBJECT, wire);

        WorldPackets::Query::QueryGameObject packet(MakePacket(CMSG_QUERY_GAME_OBJECT, wire.Bytes()));
        packet.Read();
        CHECK(packet.GameObjectID == 0x11223344u);
        CHECK(packet.Guid == LongGuid());
    }

    SECTION("CMSG_TOTEM_DESTROYED = u8 pguid")
    {
        Wire wire;
        wire.U8(0x11).PGuid(ShortGuid());

        AssertLayout<WorldPackets::Totem::TotemDestroyed>(CMSG_TOTEM_DESTROYED, wire);

        WorldPackets::Totem::TotemDestroyed packet(MakePacket(CMSG_TOTEM_DESTROYED, wire.Bytes()));
        packet.Read();
        CHECK(packet.Slot == 0x11);
        CHECK(packet.TotemGUID == ShortGuid());
    }

    SECTION("CMSG_SET_PET_SLOT = u32 u8 pguid")
    {
        Wire wire;
        wire.U32(0x11223344).U8(0x22).PGuid(LongGuid());

        AssertLayout<WorldPackets::NPC::SetPetSlot>(CMSG_SET_PET_SLOT, wire);

        WorldPackets::NPC::SetPetSlot packet(MakePacket(CMSG_SET_PET_SLOT, wire.Bytes()));
        packet.Read();
        CHECK(packet.PetNumber == 0x11223344u);
        CHECK(packet.DestSlot == 0x22);
        CHECK(packet.StableMaster == LongGuid());
    }

    SECTION("CMSG_UPDATE_AREA_TRIGGER_VISUAL = u32 u32 u32 pguid")
    {
        // The middle pair is the nested SpellCastVisual struct.
        Wire wire;
        wire.U32(0x11223344).U32(0x22334455).U32(0x33445566).PGuid(OtherGuid());

        AssertLayout<WorldPackets::AreaTrigger::UpdateAreaTriggerVisual>(CMSG_UPDATE_AREA_TRIGGER_VISUAL, wire);

        WorldPackets::AreaTrigger::UpdateAreaTriggerVisual packet(MakePacket(CMSG_UPDATE_AREA_TRIGGER_VISUAL, wire.Bytes()));
        packet.Read();
        CHECK(packet.SpellID == 0x11223344);
        CHECK(packet.Visual.SpellXSpellVisualID == 0x22334455);
        CHECK(packet.Visual.ScriptVisualID == 0x33445566);
        CHECK(packet.TargetGUID == OtherGuid());
    }
}

TEST_CASE("CMSG layout: raw uint64 fields", "[packet][layout]")
{
    // A raw uint64 is always 8 bytes; a PackedGuid is 2 + popcount bytes. Mixing
    // the two up is the single most common wire defect we have hit, so every
    // opcode that carries both shapes is pinned here.

    SECTION("CMSG_MAIL_RETURN_TO_SENDER = u64 pguid")
    {
        Wire wire;
        wire.U64(UI64LIT(0x1122334455667788)).PGuid(LongGuid());

        AssertLayout<WorldPackets::Mail::MailReturnToSender>(CMSG_MAIL_RETURN_TO_SENDER, wire);

        WorldPackets::Mail::MailReturnToSender packet(MakePacket(CMSG_MAIL_RETURN_TO_SENDER, wire.Bytes()));
        packet.Read();
        CHECK(packet.MailID == UI64LIT(0x1122334455667788));
        CHECK(packet.SenderGUID == LongGuid());
    }

    SECTION("CMSG_MAIL_TAKE_ITEM = pguid u64 u64")
    {
        Wire wire;
        wire.PGuid(ShortGuid()).U64(UI64LIT(0x1122334455667788)).U64(UI64LIT(0x2233445566778899));

        AssertLayout<WorldPackets::Mail::MailTakeItem>(CMSG_MAIL_TAKE_ITEM, wire);

        WorldPackets::Mail::MailTakeItem packet(MakePacket(CMSG_MAIL_TAKE_ITEM, wire.Bytes()));
        packet.Read();
        CHECK(packet.Mailbox == ShortGuid());
        CHECK(packet.MailID == UI64LIT(0x1122334455667788));
        CHECK(packet.AttachID == UI64LIT(0x2233445566778899));
    }

    SECTION("CMSG_GUILD_BANK_DEPOSIT_MONEY = pguid u64")
    {
        Wire wire;
        wire.PGuid(LongGuid()).U64(UI64LIT(0x0102030405060708));

        AssertLayout<WorldPackets::Guild::GuildBankDepositMoney>(CMSG_GUILD_BANK_DEPOSIT_MONEY, wire);

        WorldPackets::Guild::GuildBankDepositMoney packet(MakePacket(CMSG_GUILD_BANK_DEPOSIT_MONEY, wire.Bytes()));
        packet.Read();
        CHECK(packet.Banker == LongGuid());
        CHECK(packet.Money == UI64LIT(0x0102030405060708));
    }

    SECTION("CMSG_CALENDAR_MODERATOR_STATUS = pguid u64 u64 u64 u8")
    {
        Wire wire;
        wire.PGuid(OtherGuid())
            .U64(UI64LIT(0x1122334455667788))
            .U64(UI64LIT(0x2233445566778899))
            .U64(UI64LIT(0x0102030405060708))
            .U8(0x11);

        AssertLayout<WorldPackets::Calendar::CalendarModeratorStatusQuery>(CMSG_CALENDAR_MODERATOR_STATUS, wire);

        WorldPackets::Calendar::CalendarModeratorStatusQuery packet(MakePacket(CMSG_CALENDAR_MODERATOR_STATUS, wire.Bytes()));
        packet.Read();
        CHECK(packet.Guid == OtherGuid());
        CHECK(packet.EventID == UI64LIT(0x1122334455667788));
        CHECK(packet.InviteID == UI64LIT(0x2233445566778899));
        CHECK(packet.ModeratorID == UI64LIT(0x0102030405060708));
        CHECK(packet.Status == 0x11);
    }

    SECTION("CMSG_CALENDAR_REMOVE_EVENT = u64 u64 u64 u32")
    {
        // Read order is EventID, ModeratorID, ClubID, Flags.
        Wire wire;
        wire.U64(UI64LIT(0x1122334455667788))
            .U64(UI64LIT(0x2233445566778899))
            .U64(UI64LIT(0x0102030405060708))
            .U32(0x11223344);

        AssertLayout<WorldPackets::Calendar::CalendarRemoveEvent>(CMSG_CALENDAR_REMOVE_EVENT, wire);

        WorldPackets::Calendar::CalendarRemoveEvent packet(MakePacket(CMSG_CALENDAR_REMOVE_EVENT, wire.Bytes()));
        packet.Read();
        CHECK(packet.EventID == UI64LIT(0x1122334455667788));
        CHECK(packet.ModeratorID == UI64LIT(0x2233445566778899));
        CHECK(packet.ClubID == UI64LIT(0x0102030405060708));
        CHECK(packet.Flags == 0x11223344u);
    }

    SECTION("CMSG_SET_ACTION_BUTTON = u64 u8")
    {
        Wire wire;
        wire.U64(UI64LIT(0x1122334455667788)).U8(0x22);

        AssertLayout<WorldPackets::Spells::SetActionButton>(CMSG_SET_ACTION_BUTTON, wire);

        WorldPackets::Spells::SetActionButton packet(MakePacket(CMSG_SET_ACTION_BUTTON, wire.Bytes()));
        packet.Read();
        CHECK(packet.Action == UI64LIT(0x1122334455667788));
        CHECK(packet.Index == 0x22);
    }

    SECTION("CMSG_DELETE_EQUIPMENT_SET = u64")
    {
        Wire wire;
        wire.U64(UI64LIT(0x0102030405060708));

        AssertLayout<WorldPackets::EquipmentSet::DeleteEquipmentSet>(CMSG_DELETE_EQUIPMENT_SET, wire);

        WorldPackets::EquipmentSet::DeleteEquipmentSet packet(MakePacket(CMSG_DELETE_EQUIPMENT_SET, wire.Bytes()));
        packet.Read();
        CHECK(packet.ID == UI64LIT(0x0102030405060708));
    }

    SECTION("CMSG_SET_TRADE_GOLD = u64")
    {
        Wire wire;
        wire.U64(UI64LIT(0x1122334455667788));

        AssertLayout<WorldPackets::Trade::SetTradeGold>(CMSG_SET_TRADE_GOLD, wire);

        WorldPackets::Trade::SetTradeGold packet(MakePacket(CMSG_SET_TRADE_GOLD, wire.Bytes()));
        packet.Read();
        CHECK(packet.Coinage == UI64LIT(0x1122334455667788));
    }

    SECTION("CMSG_BLACK_MARKET_REQUEST_ITEMS = pguid u64")
    {
        // LastUpdateID is a Timestamp<>, which is 8 bytes on the wire despite
        // the smaller reserve hint its Write() side uses.
        Wire wire;
        wire.PGuid(LongGuid()).U64(UI64LIT(0x0000000012345678));

        AssertLayout<WorldPackets::BlackMarket::BlackMarketRequestItems>(CMSG_BLACK_MARKET_REQUEST_ITEMS, wire);

        WorldPackets::BlackMarket::BlackMarketRequestItems packet(MakePacket(CMSG_BLACK_MARKET_REQUEST_ITEMS, wire.Bytes()));
        packet.Read();
        CHECK(packet.Guid == LongGuid());
    }
}

TEST_CASE("CMSG layout: narrow and mixed-width scalars", "[packet][layout]")
{
    // Reading a uint16 field as uint32 is a 2-byte over-read that only shows up
    // as a desync further down the stream - or not at all, when the field is last.

    SECTION("CMSG_SET_DUNGEON_DIFFICULTY = u16")
    {
        Wire wire;
        wire.U16(0x1234);

        AssertLayout<WorldPackets::Misc::SetDungeonDifficulty>(CMSG_SET_DUNGEON_DIFFICULTY, wire);

        WorldPackets::Misc::SetDungeonDifficulty packet(MakePacket(CMSG_SET_DUNGEON_DIFFICULTY, wire.Bytes()));
        packet.Read();
        CHECK(packet.DifficultyID == 0x1234);
    }

    SECTION("CMSG_SET_RAID_DIFFICULTY = u32 u16")
    {
        Wire wire;
        wire.U32(0x11223344).U16(0x2345);

        AssertLayout<WorldPackets::Misc::SetRaidDifficulty>(CMSG_SET_RAID_DIFFICULTY, wire);

        WorldPackets::Misc::SetRaidDifficulty packet(MakePacket(CMSG_SET_RAID_DIFFICULTY, wire.Bytes()));
        packet.Read();
        CHECK(packet.Legacy == 0x11223344);
        CHECK(packet.DifficultyID == 0x2345);
    }

    SECTION("CMSG_SET_FACTION_AT_WAR = u16")
    {
        Wire wire;
        wire.U16(0x3456);

        AssertLayout<WorldPackets::Character::SetFactionAtWar>(CMSG_SET_FACTION_AT_WAR, wire);

        WorldPackets::Character::SetFactionAtWar packet(MakePacket(CMSG_SET_FACTION_AT_WAR, wire.Bytes()));
        packet.Read();
        CHECK(packet.FactionIndex == 0x3456);
    }

    SECTION("CMSG_KEYBOUND_OVERRIDE = u16")
    {
        Wire wire;
        wire.U16(0x1234);

        AssertLayout<WorldPackets::Spells::KeyboundOverride>(CMSG_KEYBOUND_OVERRIDE, wire);

        WorldPackets::Spells::KeyboundOverride packet(MakePacket(CMSG_KEYBOUND_OVERRIDE, wire.Bytes()));
        packet.Read();
        CHECK(packet.OverrideID == 0x1234);
    }

    SECTION("CMSG_AZERITE_EMPOWERED_ITEM_SELECT_POWER = u8 u8 u8 u32")
    {
        // Read order is ContainerSlot, Slot, Tier, AzeritePowerID.
        Wire wire;
        wire.U8(0x11).U8(0x22).U8(0x33).U32(0x44556677);

        AssertLayout<WorldPackets::Azerite::AzeriteEmpoweredItemSelectPower>(CMSG_AZERITE_EMPOWERED_ITEM_SELECT_POWER, wire);

        WorldPackets::Azerite::AzeriteEmpoweredItemSelectPower packet(MakePacket(CMSG_AZERITE_EMPOWERED_ITEM_SELECT_POWER, wire.Bytes()));
        packet.Read();
        CHECK(packet.ContainerSlot == 0x11);
        CHECK(packet.Slot == 0x22);
        CHECK(packet.Tier == 0x33);
        CHECK(packet.AzeritePowerID == 0x44556677);
    }

    SECTION("CMSG_TOY_CLEAR_FANFARE = u32")
    {
        Wire wire;
        wire.U32(0x11223344);

        AssertLayout<WorldPackets::Toy::ToyClearFanfare>(CMSG_TOY_CLEAR_FANFARE, wire);

        WorldPackets::Toy::ToyClearFanfare packet(MakePacket(CMSG_TOY_CLEAR_FANFARE, wire.Bytes()));
        packet.Read();
        CHECK(packet.ItemID == 0x11223344u);
    }

    SECTION("CMSG_TIME_SYNC_RESPONSE = u32 u32")
    {
        // Read order is SequenceIndex, ClientTime - the reverse of the member
        // declaration order in the header.
        Wire wire;
        wire.U32(0x11223344).U32(0x22334455);

        AssertLayout<WorldPackets::Misc::TimeSyncResponse>(CMSG_TIME_SYNC_RESPONSE, wire);

        WorldPackets::Misc::TimeSyncResponse packet(MakePacket(CMSG_TIME_SYNC_RESPONSE, wire.Bytes()));
        packet.Read();
        CHECK(packet.SequenceIndex == 0x11223344u);
        CHECK(packet.ClientTime == 0x22334455u);
    }
}

TEST_CASE("CMSG layout: floats and positions", "[packet][layout]")
{
    SECTION("CMSG_SET_EMPOWER_MIN_HOLD_STAGE_PERCENT = f32")
    {
        Wire wire;
        wire.F32(0.5f);

        AssertLayout<WorldPackets::Spells::SetEmpowerMinHoldStagePercent>(CMSG_SET_EMPOWER_MIN_HOLD_STAGE_PERCENT, wire);

        WorldPackets::Spells::SetEmpowerMinHoldStagePercent packet(MakePacket(CMSG_SET_EMPOWER_MIN_HOLD_STAGE_PERCENT, wire.Bytes()));
        packet.Read();
        CHECK(packet.MinHoldStagePercent == 0.5f);
    }

    SECTION("CMSG_MISSILE_TRAJECTORY_COLLISION = pguid u32 pguid f32 f32 f32")
    {
        Wire wire;
        wire.PGuid(LongGuid()).U32(0x11223344).PGuid(ShortGuid()).F32(1.5f).F32(-2.25f).F32(3.75f);

        AssertLayout<WorldPackets::Spells::MissileTrajectoryCollision>(CMSG_MISSILE_TRAJECTORY_COLLISION, wire);

        WorldPackets::Spells::MissileTrajectoryCollision packet(MakePacket(CMSG_MISSILE_TRAJECTORY_COLLISION, wire.Bytes()));
        packet.Read();
        CHECK(packet.Target == LongGuid());
        CHECK(packet.SpellID == 0x11223344);
        CHECK(packet.CastID == ShortGuid());
        CHECK(packet.CollisionPos.Pos.GetPositionX() == 1.5f);
        CHECK(packet.CollisionPos.Pos.GetPositionY() == -2.25f);
        CHECK(packet.CollisionPos.Pos.GetPositionZ() == 3.75f);
    }

    SECTION("CMSG_PET_ACTION = pguid u32 pguid f32 f32 f32")
    {
        Wire wire;
        wire.PGuid(ShortGuid()).U32(0x22334455).PGuid(OtherGuid()).F32(-1.5f).F32(2.25f).F32(-3.75f);

        AssertLayout<WorldPackets::Pet::PetAction>(CMSG_PET_ACTION, wire);

        WorldPackets::Pet::PetAction packet(MakePacket(CMSG_PET_ACTION, wire.Bytes()));
        packet.Read();
        CHECK(packet.PetGUID == ShortGuid());
        CHECK(packet.Action == 0x22334455u);
        CHECK(packet.TargetGUID == OtherGuid());
        CHECK(packet.ActionPosition.Pos.GetPositionX() == -1.5f);
        CHECK(packet.ActionPosition.Pos.GetPositionY() == 2.25f);
        CHECK(packet.ActionPosition.Pos.GetPositionZ() == -3.75f);
    }
}

TEST_CASE("CMSG layout: bit-packed trailer", "[packet][layout]")
{
    // A trailing bit run costs exactly one byte no matter how few bits it holds,
    // because the next byte-sized read resets the bit position. Getting that
    // wrong shifts every subsequent field.

    SECTION("CMSG_BATTLE_PET_SET_FLAGS = pguid u16 bits<2>")
    {
        Wire wire;
        wire.PGuid(LongGuid()).U16(0x1234).Bits(2, 0x3);

        AssertLayout<WorldPackets::BattlePet::BattlePetSetFlags>(CMSG_BATTLE_PET_SET_FLAGS, wire);

        WorldPackets::BattlePet::BattlePetSetFlags packet(MakePacket(CMSG_BATTLE_PET_SET_FLAGS, wire.Bytes()));
        packet.Read();
        CHECK(packet.PetGuid == LongGuid());
        CHECK(packet.Flags == 0x1234);
        CHECK(packet.ControlType == 0x3);
    }

    SECTION("bits are read most significant first")
    {
        // 0b10 in a 2-bit field must land as the value 2, not 1.
        Wire wire;
        wire.PGuid(ShortGuid()).U16(0x0001).Bits(2, 0x2);

        WorldPackets::BattlePet::BattlePetSetFlags packet(MakePacket(CMSG_BATTLE_PET_SET_FLAGS, wire.Bytes()));
        packet.Read();
        CHECK(packet.ControlType == 0x2);
        CHECK(packet.GetRawPacket()->rpos() == wire.Bytes().size());
    }
}
