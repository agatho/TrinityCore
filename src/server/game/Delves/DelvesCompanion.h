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

#ifndef TRINITY_DELVES_COMPANION_H
#define TRINITY_DELVES_COMPANION_H

#include "Define.h"
#include "DelvesDefines.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Position.h"
#include <string>

class Creature;
class InstanceMap;
class Player;
struct PlayerCompanionInfoEntry;

namespace Delves
{

struct CompanionState
{
    uint32 CompanionId = 0;
    uint32 Level = 1;
    uint32 Xp = 0;
    CompanionRole SelectedRole = CompanionRole::Dps;
    uint32 CombatCurioNodeId = 0;
    uint32 UtilityCurioNodeId = 0;
};

class TC_GAME_API DelvesCompanion
{
public:
    // Database persistence (account-wide)
    static void LoadFromDB(uint32 battlenetAccountId, CompanionState& state);
    static void SaveToDB(uint32 battlenetAccountId, CompanionState const& state);

    // In-instance companion spawning
    static Creature* SpawnCompanion(InstanceMap* map, Player* owner, CompanionState const& state, Position const& pos);
    static void DespawnCompanion(InstanceMap* map, ObjectGuid companionGuid);

    // Companion info lookup
    static PlayerCompanionInfoEntry const* GetDefaultCompanionInfo();
    static uint32 GetCompanionDisplayId(PlayerCompanionInfoEntry const* info);

    // XP and leveling
    static void AwardCompanionXP(uint32 battlenetAccountId, CompanionState& state, uint32 xpAmount);
    static uint32 GetXPForLevel(uint32 level);
    static uint32 GetMaxCompanionLevel();

    // AI script name for the companion creature
    static constexpr char const* COMPANION_AI_SCRIPT_NAME = "npc_brann_bronzebeard_delves";
};

// DELVE_ASSIST_ACTION client event
// =================================
// IDA build 67186: Brann's "assist actions" (kicks, drinks, dispels, environment
// interactions) are delivered to the client via a UpdateField on the Brann
// CGUnit_C carrying a `JamAssistActionState_C` struct. The mirror handler
// signature is (typename string at IDA 0x7FF75F3A9AE0):
//
//   void(class CGUnit_C&, struct JamAssistActionState_C const&)
//
// JamAssistActionState_C field layout (decoded from `assistAction` Lua binding
// at IDA `0x7FF75C95D800` — pushes a 5-field Lua table):
//
//   AssistedPlayer    PackedGUID  optional (presence byte at offset +48)
//   MapName           string?     optional (presence byte at offset +64)
//   CreatureName      string      optional (presence byte at offset +76)
//   ReceivedSpellID   int32
//   AssistAction      int32       (enum — kick/drink/dispel/etc; values not
//                                  symbolicated in the 67186 IDA db)
//
// Server-side wiring is **NOT IMPLEMENTED**. Adding a new struct UpdateField on
// Unit/Creature requires placing a bit in the UnitData changes mask; the exact
// bit position used by the retail client is not exposed in the available IDA
// data, so a guess would risk desynchronising every UpdateObject for every Unit.
// Defer until either: (a) a sniff captures Brann firing this in a delve so the
// bit can be observed in SMSG_UPDATE_OBJECT, or (b) a future IDA extraction
// surfaces the UnitData field index explicitly.
//
// Until then, scripts can call the stub helpers below; they no-op (TC_LOG_TRACE
// only) so the call sites are visible for later wiring.
struct AssistActionEvent
{
    enum Action : int32
    {
        // Enum values not yet decoded — placeholders matching common Brann patterns.
        Unknown   = 0,
        KickedDoor = 1,
        Drinking  = 2,
        Dispelled = 3,
        Mining    = 4,
        Lockpick  = 5,
    };

    Optional<ObjectGuid> AssistedPlayer;
    Optional<std::string> MapName;
    Optional<std::string> CreatureName;
    int32 ReceivedSpellID = 0;
    Action ActionType = Unknown;
};

// Stub: documents the call site, no wire effect (see comment block above).
TC_GAME_API void NotifyDelveAssistAction(Creature* companion, AssistActionEvent const& event);

} // namespace Delves

#endif // TRINITY_DELVES_COMPANION_H
