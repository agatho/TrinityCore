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
#include "WorldSession.h"

ChatCautionMgr::ChatCautionMgr(WorldSession* owner) : _owner(owner), _nextConfirmNumber(1)
{
}

ChatCautionMgr::~ChatCautionMgr() = default;

uint32 ChatCautionMgr::Hold(PendingMessage&& message)
{
    RemoveExpired();

    if (_pending.size() >= MaxPending)
        return 0;

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
