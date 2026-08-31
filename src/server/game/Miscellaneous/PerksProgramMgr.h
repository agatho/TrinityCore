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
#include <unordered_map>
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

    // Returns a vendor item from the WHOLE catalogue, regardless of whether it is offered this rotation, or
    // nullptr if the id does not exist at all. This is what CMSG_PERKS_PROGRAM_ITEMS_REFRESHED needs: the
    // client asks precisely about items it purchased in an earlier rotation, so a lookup restricted to the
    // active listing would miss every one of them. Do NOT use it to authorise a purchase.
    WorldPackets::PerksProgram::PerksVendorItem const* GetCatalogueVendorItem(int32 vendorItemId);

    // Fills the current Trading Post period as UTC unix timestamps for the current calendar month
    // [periodStart, periodEnd). The client uses periodEnd to show the "time remaining" countdown.
    void GetCurrentPeriod(uint64& periodStart, uint64& periodEnd) const;

    // Counts how often the listing has been (re)built. A session stamps the listing it last handed a client
    // with this value; when it moves on, that client's copy is stale and has to be replaced with
    // SMSG_PERKS_PROGRAM_VENDOR_UPDATE. The rotation rollover in EnsureCurrent is the ONLY thing that bumps it
    // at runtime in this tree -- so it is the only moment that message is due.
    uint32 GetListingGeneration();

    // Inherited from the base implementation and currently unreachable: grep over src/ finds no caller for
    // Reload() (nor for sPerksProgramMgr->Reload) anywhere. It would land in the same rebuild as the rollover
    // and bump the same generation, but until something calls it, it is not a trigger -- do not cite it as one.
    void Reload() { _loaded = false; }

private:
    void BuildVendorList();
    // Rebuilds the listing if it was never built or belongs to a Trading Post period that has since rolled over.
    void EnsureCurrent();

    bool _loaded = false;
    uint64 _listingPeriod = 0;      // Trading Post period the current listing was built for
    uint32 _listingGeneration = 0;  // bumped by every BuildVendorList; 0 = never built
    std::vector<WorldPackets::PerksProgram::PerksVendorItem> _vendorItems;
    std::unordered_map<int32, WorldPackets::PerksProgram::PerksVendorItem> _catalogue;
};

#define sPerksProgramMgr PerksProgramMgr::instance()

#endif // TRINITYCORE_PERKS_PROGRAM_MGR_H
