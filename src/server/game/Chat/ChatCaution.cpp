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

#include "ChatCaution.h"
#include "Containers.h"
#include "GameTime.h"
#include "Log.h"
#include "WorldSession.h"
#include <algorithm>

ChatCautionMgr::ChatCautionMgr(WorldSession* owner) : _owner(owner), _nextConfirmNumber(1)
{
}

ChatCautionMgr::~ChatCautionMgr() = default;

uint32 ChatCautionMgr::Hold(PendingMessage&& message)
{
    RemoveExpired();

    // The limit must not become a way out of the check. Whether an entry is held is decided by the
    // SERVER, but whether it is ever answered is decided by the CLIENT - so refusing to hold once
    // the list is full, and letting the message through unheld, hands any client that simply never
    // answers a permanent bypass: ten unanswered messages keep every slot occupied for
    // ExpirySeconds and can be renewed indefinitely. The oldest entry gives way instead.
    //
    // UNVERIFIED: what retail does at this limit, or that it has one. The pending list is pure
    // server state - the client only ever hands a ConfirmNumber back and cannot tell a held message
    // from a delivered one - so nothing in the binary or the UI source can decide it and only a
    // recording could. Discarding the oldest applies the one rule this mechanism already has: an
    // unanswered message is thrown away (RemoveExpired), here merely earlier. The player sees the
    // same outcome as an expiry - the rewrite links on the echoed line stop working - and the
    // check itself stays in force for every message.
    while (_pending.size() >= MaxPending)
    {
        // Expiry is Hold time + a constant, so the smallest Expiry is the entry held longest ago.
        auto oldest = std::min_element(_pending.begin(), _pending.end(),
            [](std::pair<uint32 const, PendingMessage> const& left, std::pair<uint32 const, PendingMessage> const& right)
            {
                return left.second.Expiry < right.second.Expiry;
            });

        TC_LOG_DEBUG("network", "ChatCautionMgr: {} reached {} pending cautionary messages, discarding confirmNumber {} unanswered",
            _owner->GetPlayerInfo(), MaxPending, oldest->first);
        _pending.erase(oldest);
    }

    message.Expiry = GameTime::GetGameTime() + ExpirySeconds;

    // The client hands the number back verbatim, so any non zero value works. Keep it monotonic so
    // a stale answer for an already dropped message cannot hit a fresh entry.
    uint32 confirmNumber = _nextConfirmNumber++;
    if (!_nextConfirmNumber)
        _nextConfirmNumber = 1;

    _pending[confirmNumber] = std::move(message);
    return confirmNumber;
}

Optional<ChatCautionMgr::PendingMessage> ChatCautionMgr::Take(uint32 confirmNumber)
{
    RemoveExpired();

    auto itr = _pending.find(confirmNumber);
    if (itr == _pending.end())
        return {};

    PendingMessage message = std::move(itr->second);
    _pending.erase(itr);
    return message;
}

bool ChatCautionMgr::Drop(uint32 confirmNumber)
{
    RemoveExpired();

    return _pending.erase(confirmNumber) != 0;
}

void ChatCautionMgr::RemoveExpired()
{
    // Lazily in the query path, like WorldSession::_instanceResetTimes - no tick hook needed
    time_t now = GameTime::GetGameTime();
    Trinity::Containers::EraseIf(_pending, [now](std::pair<uint32 const, PendingMessage> const& entry)
    {
        return entry.second.Expiry <= now;
    });
}
