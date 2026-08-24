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
 * Delvers' Supplies - creature 207283 ("Delvers' Supplies", type 10, npcflag
 * 36310271995678849 = GOSSIP | VENDOR | REPAIR + the two high UNIT_NPC_FLAG_2 bits).
 *
 * Spawned on maps 2552 (Khaz Algar surface), 2680, 2683 and 2962 (Atal'Aman) in integ_world.
 * creature_template_gossip binds it to menu 31704, whose options are (integ_world, VerifiedBuild
 * 59302 / 60257 / 64978 / 66709):
 *
 *   OptionID 0  OptionNpc 51 (TraitSystem)  GossipNpcOptionID 43271  "<View Brann Bronzebeard's supplies.>"
 *   OptionID 1  OptionNpc 51 (TraitSystem)  GossipNpcOptionID 58442  "<View companion supplies.>"
 *   OptionID 2  OptionNpc  1 (Vendor)                                "<View goods and repair gear.>"
 *   OptionID 3  OptionNpc  0 (None)         ActionMenuID 0           "<Signal your companion to gather here.>"
 *   OptionID 4  OptionNpc 51 (TraitSystem)  GossipNpcOptionID 59191  "<View companion supplies.>"
 *   OptionID 5  OptionNpc 51 (TraitSystem)  GossipNpcOptionID 59370  "<View companion supplies.>"
 *
 * Options 0/1/4/5 (TraitSystem = the companion curio trees) and option 2 (vendor + repair, backed
 * by the five npc_vendor rows on 207283) are all handled by Player::OnGossipSelect from the DB row
 * alone. Option 3 is the only dead one: OptionNpc None + ActionMenuID 0 + ActionPoiID 0 means the
 * client sends CMSG_GOSSIP_SELECT_OPTION and nothing at all is bound to it - the same failure mode
 * as the Oribos Ring of Transference options fixed in 2026_08_08_20_oribos_*.sql.
 *
 * This script implements ONLY option 3, and returns false for everything else so the native
 * vendor/trait handling keeps working untouched.
 *
 * "Signal your companion to gather here" moves the player's delve companion (the creature summoned
 * by DelvesCompanion::SpawnCompanion, entry = Delves.Companion.CreatureId, 248567 Valeera
 * Sanguinar for Midnight S1) to the supplies NPC, then hands it back to its follow behaviour. No
 * ids, coordinates or spells are invented here: the companion entry comes from config and the
 * destination is this NPC's own spawn position.
 */

#include "Creature.h"
#include "Config.h"
#include "DelvesCompanion.h"
#include "DelvesDefines.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include <list>

using namespace Delves;

namespace
{

// gossip_menu_option.OptionID (== OrderIndex, which is what CreatureAI::OnGossipSelect receives)
// of "<Signal your companion to gather here.>" on menu 31704.
constexpr uint32 GOSSIP_OPTION_SIGNAL_COMPANION = 3;

// The companion is summoned next to its owner and never strays far, but the supplies NPC can sit a
// room away; 250 yards is the grid-search cap and comfortably covers a delve instance.
constexpr float COMPANION_SEARCH_RANGE = 250.0f;

// Where the companion stops. Standing exactly on the NPC would push it around, so aim just short
// of it; MovePoint's own arrival tolerance does the rest.
constexpr float COMPANION_GATHER_OFFSET = 2.0f;

// MotionMaster point id, script-local.
constexpr uint32 POINT_GATHER_AT_SUPPLIES = 1;

struct npc_delvers_supplies : public ScriptedAI
{
    npc_delvers_supplies(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        if (!player || gossipListId != GOSSIP_OPTION_SIGNAL_COMPANION)
            return false;   // let Player::OnGossipSelect handle vendor / trait-system options

        Creature* companion = FindCompanionOf(player);
        if (!companion)
        {
            // Nothing to signal - no companion is out. Close the menu rather than leaving the
            // client waiting on a response it will never get.
            TC_LOG_DEBUG("scripts.delves",
                "npc_delvers_supplies: player {} signalled for a companion, but none is summoned.",
                player->GetName());
            CloseGossipMenuFor(player);
            return true;
        }

        CloseGossipMenuFor(player);

        // Stop just short of the crate, on the side the companion is approaching from.
        Position const dest = me->GetNearPosition(COMPANION_GATHER_OFFSET,
            me->GetPosition().GetAbsoluteAngle(companion->GetPosition()) - me->GetOrientation());

        companion->GetMotionMaster()->Clear();
        companion->GetMotionMaster()->MovePoint(POINT_GATHER_AT_SUPPLIES, dest);

        TC_LOG_DEBUG("scripts.delves",
            "npc_delvers_supplies: player {} signalled companion {} (entry {}) to gather at ({:.2f}, {:.2f}, {:.2f}).",
            player->GetName(), companion->GetGUID().ToString(), companion->GetEntry(),
            dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());

        return true;
    }

private:
    // The delve companion is a TempSummon owned by the player, of the configured creature entry.
    Creature* FindCompanionOf(Player* player) const
    {
        uint32 const companionEntry = uint32(sConfigMgr->GetIntDefault("Delves.Companion.CreatureId", 0));
        if (!companionEntry)
            return nullptr;

        std::list<Creature*> candidates;
        me->GetCreatureListWithEntryInGrid(candidates, companionEntry, COMPANION_SEARCH_RANGE);

        for (Creature* candidate : candidates)
        {
            if (!candidate->IsAlive())
                continue;

            TempSummon const* summon = candidate->ToTempSummon();
            if (!summon)
                continue;

            if (summon->GetSummonerGUID() == player->GetGUID())
                return candidate;
        }

        return nullptr;
    }
};

} // anonymous namespace

void AddSC_npc_delvers_supplies()
{
    RegisterCreatureAI(npc_delvers_supplies);
}
