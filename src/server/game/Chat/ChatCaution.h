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

#ifndef TRINITYCORE_CHAT_CAUTION_H
#define TRINITYCORE_CHAT_CAUTION_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "SharedDefines.h"
#include <string>
#include <unordered_map>

class WorldSession;

// The "cautionary chat" mechanism of 12.x. The server holds a message back, hands the client a
// ConfirmNumber, and the client answers with
//   CMSG_CHAT_SEND_CAUTIONARY_CHAT_MESSAGE  (0x2C0009) -> deliver it
//   CMSG_CHAT_DROP_CAUTIONARY_CHAT_MESSAGE  (0x2C000A) -> throw it away
// The client only ever sends the number back; the text and the recipient are server state. That is
// the whole reason this class exists - the wire part is four bytes.
//
// Channel messages work differently and deliberately keep no pending entry: consumer 0x2667E30
// answers CMSG_CHAT_SEND_CAUTIONARY_CHANNEL_MESSAGE unconditionally and without a Lua binding, so
// there is no user decision to wait for (see AGENT_BRIEF_CHAT_2C_4A.md K4).
//
// Not persistent on purpose: a pending message dies with the session. Modelled on the pending
// instance bind (Player::_pendingBindId) - session state, swept lazily on access, dropped at logout.
class ChatCautionMgr
{
public:
    struct PendingMessage
    {
        ChatMsg Type = CHAT_MSG_WHISPER;
        /// the language the CLIENT asked for, never the one HandleChatMessage resolved it to -
        /// the confirmation re-enters that function at the top and resolves again
        Language Lang = LANG_UNIVERSAL;
        std::string Text;
        std::string TargetName;
        ObjectGuid TargetGuid;
        time_t Expiry = 0;
    };

    static constexpr uint32 ExpirySeconds = 5 * MINUTE;
    static constexpr std::size_t MaxPending = 10;

    explicit ChatCautionMgr(WorldSession* owner);
    ~ChatCautionMgr();

    ChatCautionMgr(ChatCautionMgr const&) = delete;
    ChatCautionMgr(ChatCautionMgr&&) = delete;
    ChatCautionMgr& operator=(ChatCautionMgr const&) = delete;
    ChatCautionMgr& operator=(ChatCautionMgr&&) = delete;

    // Stores the message and returns the ConfirmNumber to put into SMSG_CAUTIONARY_CHAT_MESSAGE.
    // Returns 0 when the session already has MaxPending live entries.
    uint32 Hold(PendingMessage&& message);

    // Removes and returns the entry, or nothing if the number is unknown or expired.
    Optional<PendingMessage> Take(uint32 confirmNumber);

    // Removes the entry without returning it. True if there was one.
    bool Drop(uint32 confirmNumber);

    void Clear() { _pending.clear(); }

private:
    void RemoveExpired();

    WorldSession* _owner;
    uint32 _nextConfirmNumber;
    std::unordered_map<uint32, PendingMessage> _pending;
};

#endif // TRINITYCORE_CHAT_CAUTION_H
