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

#ifndef TRINITYCORE_TRAIT_PACKETS_H
#define TRINITYCORE_TRAIT_PACKETS_H

#include "Packet.h"
#include "TraitPacketsCommon.h"

namespace WorldPackets::Traits
{
class TraitsCommitConfig final : public ClientPacket
{
public:
    explicit TraitsCommitConfig(WorldPacket&& packet) : ClientPacket(CMSG_TRAITS_COMMIT_CONFIG, std::move(packet)) { }

    void Read() override;

    TraitConfig Config;
    int32 SavedConfigID = 0;
    int32 SavedLocalIdentifier = 0;
};

class TraitConfigCommitFailed final : public ServerPacket
{
public:
    explicit TraitConfigCommitFailed(int32 configId = 0, int32 spellId = 0, int32 reason = 0) : ServerPacket(SMSG_TRAIT_CONFIG_COMMIT_FAILED, 4 + 4 + 1),
        ConfigID(configId), SpellID(spellId), Reason(reason) { }

    WorldPacket const* Write() override;

    int32 ConfigID;
    int32 SpellID;
    int32 Reason;
};

class ClassTalentsRequestNewConfig final : public ClientPacket
{
public:
    explicit ClassTalentsRequestNewConfig(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_REQUEST_NEW_CONFIG, std::move(packet)) { }

    void Read() override;

    TraitConfig Config;
};

class ClassTalentsRenameConfig final : public ClientPacket
{
public:
    explicit ClassTalentsRenameConfig(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_RENAME_CONFIG, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
    String<259> Name;
};

class ClassTalentsDeleteConfig final : public ClientPacket
{
public:
    explicit ClassTalentsDeleteConfig(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_DELETE_CONFIG, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
};

class ClassTalentsSetStarterBuildActive final : public ClientPacket
{
public:
    explicit ClassTalentsSetStarterBuildActive(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_SET_STARTER_BUILD_ACTIVE, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
    bool Active = false;
};

// SMSG_STARTER_BUILD_ACTIVATE_FAILED (0x450076) - the failure answer to
// CMSG_CLASS_TALENTS_SET_STARTER_BUILD_ACTIVE (C_ClassTalents.SetStarterBuildActive).
// The reader (0x5E1C70) does not decompose anything, and the consumer (0x23995D0) never touches its
// argument at all:
//     sub    rsp, 0x28
//     call   0x7FF7828C8830                 ; event singleton
//     mov    r8, [rax+0x43b8]
//     lea    rdx, [rsp+0x38]
//     movabs rcx, 0x1D7DF894817106CB        ; murmur3 of "STARTER_BUILD_ACTIVATION_FAILED"
//     call   r8
// The Lua event is payload free (ClassTalentsDocumentation.lua:483-488) and the UI answers it with
// SetCommitStarted(nil, CommitUpdateReasons.CommitFailed) plus ResetToLastConfigID()
// (Blizzard_ClassTalentsFrame.lua:326-328). There is no reason code: the client does not read one,
// so this message is empty. The ERR_TALENT_FAILED_* strings are distributed purely client side
// (Blizzard_ClassTalentsFrame.lua:1717-1765) and are not this opcode's payload.
class StarterBuildActivateFailed final : public ServerPacket
{
public:
    explicit StarterBuildActivateFailed() : ServerPacket(SMSG_STARTER_BUILD_ACTIVATE_FAILED, 0) { }

    WorldPacket const* Write() override;
};

// CMSG_CLASS_TALENTS_NOTIFY_EMPTY_CONFIG (0x3D00C7), writer 0x6CD620: a single uint32, 4 bytes.
// A client to server diagnostic with no answer: the client reports that a trait configuration it
// holds is empty.
class ClassTalentsNotifyEmptyConfig final : public ClientPacket
{
public:
    explicit ClassTalentsNotifyEmptyConfig(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_NOTIFY_EMPTY_CONFIG, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
};

// CMSG_CLASS_TALENTS_NOTIFY_VALIDATION_FAILED (0x3D02C6), writer 0x6D2550: a single uint32, 4 bytes.
// Confirmed on the wire: one captured packet in C:\sniff, 4 bytes, d3 b9 51 06 -> 0x0651B9D3.
// The client reports that a configuration the server sent did not validate locally. No answer.
class ClassTalentsNotifyValidationFailed final : public ClientPacket
{
public:
    explicit ClassTalentsNotifyValidationFailed(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_NOTIFY_VALIDATION_FAILED, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
};

// CMSG_TRAITS_TALENT_TEST_UNLEARN_SPELLS (0x3D02BA), writer 0x6D21F0: empty, 0 bytes.
// Lua entry point C_Traits.TalentTestUnlearnSpells() (binding RVA 0x14C3160), which takes no
// arguments. No answer.
class TraitsTalentTestUnlearnSpells final : public ClientPacket
{
public:
    explicit TraitsTalentTestUnlearnSpells(WorldPacket&& packet) : ClientPacket(CMSG_TRAITS_TALENT_TEST_UNLEARN_SPELLS, std::move(packet)) { }

    void Read() override { }
};

class ClassTalentsSetUsesSharedActionBars final : public ClientPacket
{
public:
    explicit ClassTalentsSetUsesSharedActionBars(WorldPacket&& packet) : ClientPacket(CMSG_CLASS_TALENTS_SET_USES_SHARED_ACTION_BARS, std::move(packet)) { }

    void Read() override;

    int32 ConfigID = 0;
    bool UsesShared = false;
    bool IsLastSelectedSavedConfig = false;
};
}

#endif // TRINITYCORE_TRAIT_PACKETS_H
