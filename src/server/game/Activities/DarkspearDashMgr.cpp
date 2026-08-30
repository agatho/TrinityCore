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

#include "DarkspearDashMgr.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Player.h"

DarkspearDashMgr::DarkspearDashMgr() = default;
DarkspearDashMgr::~DarkspearDashMgr() = default;

/*static*/ DarkspearDashMgr* DarkspearDashMgr::instance()
{
    static DarkspearDashMgr instance;
    return &instance;
}

void DarkspearDashMgr::GrantDasherTitle(Player* player)
{
    if (!player)
        return;

    // The one DB2-anchored piece. The completion trigger (finishing the "Darkspear
    // Dash" quest inside the game_event window) is RESEARCH-BLOCKED — the quests and
    // the event window are world-DB content absent from DB2 @68887 and from every
    // capture we hold. Until they are imported, this is reachable only via the debug
    // driver; the buff/portal spells (1280078/1279936) are recorded on the header for
    // the future quest-side script and are deliberately NOT cast here.
    if (CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(DarkspearDash::TITLE_DARKSPEAR_DASHER))
        if (!player->HasTitle(title))
            player->SetTitle(title);
}
