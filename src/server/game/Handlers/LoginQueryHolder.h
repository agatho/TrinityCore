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

// LoginQueryHolder - Bundles the ~60 prepared queries needed to load a Player
// from the character DB. Submitted via CharacterDatabase.DelayQueryHolder so
// the loads run on async DB workers; the world thread picks the result up via
// AddQueryHolderCallback and calls WorldSession::HandlePlayerLogin.
//
// Originally a private class in CharacterHandler.cpp. Lifted to a header so
// non-network code paths (notably the headless bot login in PlayerbotV2) can
// build and submit the same holder, then hand the result to the existing
// WorldSession::HandlePlayerLogin path instead of re-implementing 250 lines
// of statement setup.

#ifndef TRINITY_LOGINQUERYHOLDER_H
#define TRINITY_LOGINQUERYHOLDER_H

#include "QueryHolder.h"
#include "DatabaseEnvFwd.h"
#include "ObjectGuid.h"

class TC_GAME_API LoginQueryHolder : public CharacterDatabaseQueryHolder
{
private:
    uint32 m_accountId;
    ObjectGuid m_guid;
public:
    LoginQueryHolder(uint32 accountId, ObjectGuid guid)
        : m_accountId(accountId), m_guid(guid) { }
    ObjectGuid GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }
    bool Initialize();
};

#endif // TRINITY_LOGINQUERYHOLDER_H
