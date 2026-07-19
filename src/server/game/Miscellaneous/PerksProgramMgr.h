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

#ifndef TRINITYCORE_PERKS_PROGRAM_MGR_H
#define TRINITYCORE_PERKS_PROGRAM_MGR_H

#include "Define.h"
#include "PerksProgramPacketsCommon.h"
#include <vector>

// Assembles the Trading Post (Perks Program) vendor listing from the perks DB2 stores and
// resolves each vendor item's collectible so it can be sent as SMSG_PERKS_PROGRAM_VENDOR_UPDATE.
class TC_GAME_API PerksProgramMgr
{
    PerksProgramMgr() = default;

public:
    static PerksProgramMgr* instance();

    std::vector<WorldPackets::PerksProgram::PerksVendorItem> const& GetCurrentVendorItems();

    // Returns the currently-offered vendor item (with its resolved collectible) for a purchase
    // request, or nullptr if the id is not part of the active listing.
    WorldPackets::PerksProgram::PerksVendorItem const* GetVendorItem(int32 vendorItemId);

    // Fills the current Trading Post period as UTC unix timestamps for the current calendar month
    // [periodStart, periodEnd). The client uses periodEnd to show the "time remaining" countdown.
    void GetCurrentPeriod(uint64& periodStart, uint64& periodEnd) const;

    void Reload() { _loaded = false; }

private:
    void BuildVendorList();

    bool _loaded = false;
    std::vector<WorldPackets::PerksProgram::PerksVendorItem> _vendorItems;
};

#define sPerksProgramMgr PerksProgramMgr::instance()

#endif // TRINITYCORE_PERKS_PROGRAM_MGR_H
