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

#ifndef _GROUPMGR_H
#define _GROUPMGR_H

#include "ObjectGuid.h"
#include <map>

class Group;

class TC_GAME_API GroupMgr
{
private:
    GroupMgr();
    ~GroupMgr();

public:
    GroupMgr(GroupMgr const&) = delete;
    GroupMgr(GroupMgr&&) = delete;
    GroupMgr& operator=(GroupMgr const&) = delete;
    GroupMgr& operator=(GroupMgr&&) = delete;

    static GroupMgr* instance();

    typedef std::map<ObjectGuid::LowType, Group*> GroupContainer;
    typedef std::vector<Group*>      GroupDbContainer;

    Group* GetGroupByGUID(ObjectGuid const& guid) const;

    // Read-only view of every live group. Needed by the Playerbot module's
    // group-hygiene pass to enumerate ALL groups â€” including those whose
    // members are currently offline â€” so it can disband stale pure-bot
    // groups (server shutdown) and human groups whose human(s) have been
    // logged out past the idle threshold. No other public API exposes
    // offline-member groups.
    GroupContainer const& GetGroupStore() const { return GroupStore; }

    uint32 GenerateNewGroupDbStoreId();
    void   RegisterGroupDbStoreId(uint32 storageId, Group* group);
    void   FreeGroupDbStoreId(Group* group);
    void   SetNextGroupDbStoreId(uint32 storageId) { NextGroupDbStoreId = storageId; }
    Group* GetGroupByDbStoreId(uint32 storageId) const;
    void   SetGroupDbStoreSize(uint32 newSize);

    void Update(uint32 diff);

    void   LoadGroups();
    ObjectGuid::LowType GenerateGroupId();
    void   AddGroup(Group* group);
    void   RemoveGroup(Group* group);

protected:
    ObjectGuid::LowType           NextGroupId;
    uint32           NextGroupDbStoreId;
    GroupContainer   GroupStore;
    GroupDbContainer GroupDbStore;
};

#define sGroupMgr GroupMgr::instance()

#endif
