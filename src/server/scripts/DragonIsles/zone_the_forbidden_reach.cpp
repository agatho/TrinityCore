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

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "Conversation.h"
#include "Creature.h"
#include "CreatureTextMgr.h"
#include "DB2Stores.h"
#include "EventProcessor.h"
#include "G3DPosition.hpp"
#include "Log.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SceneMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include <mutex>
#include <unordered_map>

enum DracthyrForbiddenReach
{
    MAP_FORBIDDEN_REACH = 2570,
    AREA_WAR_CRECHE     = 13806,
    AREA_FORBIDDEN_REACH = 13769 // 369728's triggered Bind effect (377713) hardcodes this zone-level area
};

enum DracthyrLoginSpells
{
    // Spells
    // DB2 SpellEffect for 369728: EFFECT_0 teleport (native TARGET_DEST_NEARBY_DB — see
    // spell_dracthyr_login / spell_target_position), EFFECT_1 rested XP bonus (runs via default
    // handling, untouched), EFFECT_2 triggers 377713 (Bind — suppressed, see
    // AREA_FORBIDDEN_REACH). It does NOT cast the room movie/scene; that is a separate
    // CastSpell we issue ourselves in OnDracthyrRoomEntered.
    SPELL_DRACTHYR_LOGIN                = 369728,
    SPELL_STASIS_1                      = 369735, // triggers 366620
    SPELL_STASIS_2                      = 366620, // triggers 366636
    SPELL_STASIS_3                      = 366636, // removes 365560, sends first quest (64864)
    SPELL_STASIS_4                      = 365560, // freeze the target
    SPELL_DRACTHYR_MOVIE_ROOM_01        = 394245, // scene for room 1
    SPELL_DRACTHYR_MOVIE_ROOM_02        = 394279, // scene for room 2
    SPELL_DRACTHYR_MOVIE_ROOM_03        = 394281, // scene for room 3
    SPELL_DRACTHYR_MOVIE_ROOM_04        = 394282, // scene for room 4
    //SPELL_DRACTHYR_MOVIE_ROOM_05        = 394283, // scene for room 5 (only plays sound, unused?)
    SPELL_MAINTAIN_DERVISHIAN           = 369731, // Alliance Personal Summon
    SPELL_SUMMON_DERVISHIAN             = 369730,
    SPELL_MAINTAIN_KODETHI              = 370112, // Horde Personal Summon
    SPELL_SUMMON_KODETHI                = 370111,
    SPELL_AWAKEN_DRACTYHR_QUEST_ABANDON = 369744,
    SPELL_STASIS_FEEDBACK_KNOCKBACK     = 364074,
    SPELL_STASIS_FEEDBACK_VISUAL        = 374633,
    SPELL_DISINTEGRATE_SPELLCLICK       = 362355,
    SPELL_DRACTHYR_STASIS_NPC           = 382137,
    SPELL_DRACTHYR_STASIS_LOWER         = 362331,

    NPC_DERVISHIAN                      = 181494,
    NPC_KODETHI                         = 187223,
    NPC_TETHALASH                       = 181680,
    NPC_AZURATHEL_STASIS                = 183380,
    NPC_AZURATHEL_QUEST_ENDER           = 181056,
    NPC_KILL_CREDIT_CAVE_IN             = 187015,

    // Back-room "reunion" cast (B5/B6, dracthyr-forbidden-reach-handoff). Dervishian (181596) is
    // a genuinely separate, always-present ambient NPC standing beside Scalecommander Azurathel's
    // stasis spot — spawned via 2026_06_30_05_world_evry_dracthyr_back_room_reunion.sql.
    // NPC_TETHALASH_REUNITED is *not* a separate spawn: it's the entry the freed personal
    // Tethalash (181680) is `UpdateEntry`'d into (mirrors Azurathel's 183380->181056 morph) right
    // before she walks herself to the back room — see DracthyrTethalashWakeEvent.
    NPC_TETHALASH_REUNITED              = 186864,

    QUEST_AWAKEN_DRACTYHR               = 64864,

    SCENE_PKG_DRACTHYR_EVOKER_INTRO     = 3730
};

// creature_text GroupID 0 rows added by 2026_06_30_05_world_evry_dracthyr_back_room_reunion.sql —
// both broadcast text ids already exist in base client BroadcastText.db2 (confirmed via retail
// sniff SMSG_DB_REPLY), only the creature_text row linking them to these NPCs is fork content.
enum DracthyrBackRoomReunionText
{
    TEXT_GROUP_TETHALASH_REUNITED_WAKE  = 0, // 216876 "I remember... obeying an order."
    TEXT_GROUP_AZURATHEL_ENDER_WAKE     = 0  // 213820 "Ungh... my head... Alright, we need to get our bearings."
};

// Retail 12.0.7.68275 sniff (Horde/Kodethi, Room 3): right after the disintegrate-awaken hit,
// the guide runs a 2-line Conversation ("Wha... what... $p! What has happened? Who did this to
// us?!" / "Something is very wrong here. We must find our wing mates. This way!") then walks a
// short distance away from their frozen spot. ConversationLine 48297/48298 (BroadcastText
// 221640/223150) already exist in base client ConversationLine.db2 — only the server-side
// conversation_template row is fork content (see SQL). Actor ids below are taken verbatim from
// the sniff's Conversation object fields; actor 0 is present in the packet but unused by either
// line (kept for parity, no functional effect either way).
enum DracthyrGuideWakeConversation
{
    CONVERSATION_DRACTHYR_GUIDE_WAKE               = 18934,
    CONVERSATION_ACTOR_DRACTHYR_GUIDE_WAKE_UNUSED   = 83185,
    CONVERSATION_ACTOR_DRACTHYR_GUIDE_WAKE_SPEAKER  = 83422
};

// Fixed world-space target for the guide's final approach into the shared corridor, used *after*
// the glide-down leg below lands him back on walkable ground. Two independent retail sniffs of
// Kodethi's wake walk — Room 2 (SummonPosition ~5754,-3056,251) and Room 4 (SummonPosition
// ~5818,-3054,251) — both terminate at this *exact* same coordinate (matching to 2-3 decimal
// places) before he's removed from the client's object range. This is the shared War Creche
// corridor the four login rooms all funnel into (each room's door sits right at its own
// SummonPosition — see the `Starting Room Collision` (378473) door-gameobject finding). Reached
// via navmesh-pathed `MovePoint` (generatePath=true) from the glide's landing spot — TrinityCore's
// own pathing handles the corridor's bends fine from there; it's only the steep glide-only drop
// beforehand (see below) that has no walkable navmesh route. Orientation is the always-present
// static double (NPC_KODETHI_STATIC-equivalent, entry 186946) final facing from the same sniff —
// applied via `DracthyrFaceOnArrivalEvent` since `MovePoint` itself doesn't force a landing facing.
constexpr Position DRACTHYR_GUIDE_WAKE_WALK_TARGET = { 5814.37f, -2914.19f, 207.19063f, 2.681175f };

// Retail sniff (Room 4, 12.0.7.68275, `dump_..._2026-06-30_18-15-43`): Kodethi's wake walk is
// *not* one point-to-point move. It's three phases: (1) a short, nearly-flat walk from the door
// out onto the room's balcony/ledge; (2) an explicit **flying** spline (wire flags
// `Flying|CatmullRom|UncompressedPath`, 3 waypoints) that arcs up briefly then dives ~35yd down to
// the shared corridor floor — this is the actual "jumps off and glides down" moment the user
// identified as the Dracthyr glide ability visual, confirmed by wire evidence this time (an
// earlier pass over an older/shallower sniff window missed this leg and wrongly concluded no
// fly-flagged spline existed — see `DRACTHYR_GUIDE_WAKE_WALK_TARGET`'s history in the handoff
// doc); and (3) normal ground walking again (`DRACTHYR_GUIDE_WAKE_WALK_TARGET` above) once
// landed. All four offsets below are `Room.SummonPosition.GetPositionWithOffset(offset)` deltas
// measured directly from the Room 4 capture in the door/summon-facing's local frame (+X forward
// out the door, +Y right, +Z up; same convention as `Position::GetPositionOffsetTo`) —
// generalizes to all 4 rooms the same way the original Room-3-derived ledge-only offset did,
// just now covering the glide too, not only the first flat hop.
constexpr Position DRACTHYR_GUIDE_WAKE_LEDGE_LOCAL_OFFSET      = { -15.7082f,  0.0837f,  -0.9122f, 0.0f };
constexpr Position DRACTHYR_GUIDE_WAKE_GLIDE_BUMP_LOCAL_OFFSET = { -24.2977f,  0.2702f,   1.6491f, 0.0f };
constexpr Position DRACTHYR_GUIDE_WAKE_GLIDE_MID_LOCAL_OFFSET  = { -36.3327f,  0.4019f,  -8.7222f, 0.0f };
constexpr Position DRACTHYR_GUIDE_WAKE_GLIDE_LAND_LOCAL_OFFSET = { -53.2492f, -0.6711f, -34.9623f, 0.0f };

// Sniffed spline durations for the ledge walk (~15.7yd at run speed) and the glide (~56yd
// Catmull-Rom arc at the explicit 10.0 velocity set on the glide spline below — matches the
// sniff's own "Computed Speed"). Used to chain each phase's scheduled event off of the previous
// one: there's no CreatureAI/MovementInform hook wired for this bare `Creature*` (would need a
// dedicated AI script to use a movement-completion callback instead), which isn't worth the
// extra surface for a one-off cinematic-adjacent move whose durations are already known-good from
// two independent sniffs.
constexpr Milliseconds DRACTHYR_GUIDE_WAKE_LEDGE_WALK_DURATION = 1967ms;
constexpr Milliseconds DRACTHYR_GUIDE_WAKE_GLIDE_DURATION = 5610ms;

struct DracthyrLoginRoom
{
    uint32 MovieSpellId;
    Position PlayerPosition;
    Position SummonPosition;
};

static constexpr std::array<DracthyrLoginRoom, 4> LoginRoomData =
{{
    {
        SPELL_DRACTHYR_MOVIE_ROOM_01,
        { 5725.32f, -3024.26f, 251.047f, 0.01745329238474369f },
        { 5739.97216796875f, -3023.970458984375f, 251.172332763671875f, 3.193952560424804687f }
    },
    {
        SPELL_DRACTHYR_MOVIE_ROOM_02,
        { 5743.03f, -3067.28f, 251.047f, 0.798488140106201171f },
        { 5754.3046875f, -3056.34716796875f, 251.1725006103515625f, 3.926990747451782226f }
    },
    {
        SPELL_DRACTHYR_MOVIE_ROOM_03,
        { 5787.1597f, -3083.3906f, 251.04698f, 1.570796370506286621f },
        { 5787.44970703125f, -3069.335205078125f, 251.168121337890625f, 4.729842185974121093f }
    },
    {
        SPELL_DRACTHYR_MOVIE_ROOM_04,
        { 5829.32f, -3064.49f, 251.047f, 2.364955902099609375f },
        { 5818.533203125f, -3054.5625f, 251.3630828857421875f, 5.480333805084228515f }
    }
}};

struct DracthyrIntroPlayerState
{
    Optional<uint32> RoomIndex;
    bool IntroPending = false;
    bool QuestOffered = false;
    // Set once the personal guide's scripted wake-up departure (ledge walk / glide / land, see
    // DracthyrGuideWake*Event below) has begun. Guards EnsureDracthyrGuide (B5, back-room
    // reunion handoff): that function's job is recovering from teleport/summon *failures* right
    // after the player lands in their login room — it's not meant to keep "fixing" the guide's
    // position for the rest of the quest. Once the guide has deliberately left SummonPosition to
    // rejoin the rest of the War Creche cast, `IsDracthyrGuideAtExpectedRoom`'s >20yd check would
    // otherwise treat that intentional departure as a failure and despawn+resummon the guide back
    // at the login room on the next `OnUpdateZone` (e.g. player walking through the Lower War
    // Creche, or all the way through to the quest turn-in) — see dracthyr-forbidden-reach-handoff
    // B5 for the "guide vanished after turning in the quest" report this caused.
    bool GuideWakeStarted = false;
};

// The intro state is touched from map update threads (spell/AI/zone hooks) and from the world
// thread (login/logout), so the container is mutex guarded. Never hand out a pointer or reference
// into the map - the state is a handful of scalars, so copy it out or mutate it under the lock.
static std::mutex s_dracthyrIntroMutex;
static std::unordered_map<ObjectGuid, DracthyrIntroPlayerState> s_dracthyrIntroByPlayer;

static Optional<DracthyrIntroPlayerState> GetDracthyrIntroState(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(s_dracthyrIntroMutex);

    auto itr = s_dracthyrIntroByPlayer.find(guid);
    if (itr == s_dracthyrIntroByPlayer.end())
        return {};

    return itr->second;
}

template<typename Mutator>
static void UpdateDracthyrIntroState(ObjectGuid guid, Mutator&& mutator)
{
    std::lock_guard<std::mutex> lock(s_dracthyrIntroMutex);

    mutator(s_dracthyrIntroByPlayer[guid]);
}

// Test-and-set of IntroPending under a single lock. Returns true when the intro was already
// pending (i.e. the caller must not start another one).
static bool ExchangeDracthyrIntroPending(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(s_dracthyrIntroMutex);

    DracthyrIntroPlayerState& state = s_dracthyrIntroByPlayer[guid];
    if (state.IntroPending)
        return true;

    state.IntroPending = true;
    return false;
}

static void EraseDracthyrIntroState(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(s_dracthyrIntroMutex);

    s_dracthyrIntroByPlayer.erase(guid);
}

static Optional<uint32> GetDracthyrLoginRoomIndex(Player const* player)
{
    if (!player)
        return {};

    Optional<DracthyrIntroPlayerState> state = GetDracthyrIntroState(player->GetGUID());
    if (!state || !state->RoomIndex)
        return {};

    return *state->RoomIndex;
}

static uint32 GetNearestDracthyrLoginRoomIndex(Position const& pos)
{
    auto currentRoom = std::ranges::min_element(LoginRoomData, [&pos](DracthyrLoginRoom const& left, DracthyrLoginRoom const& right)
    {
        return pos.GetExactDist(left.PlayerPosition) < pos.GetExactDist(right.PlayerPosition);
    });

    if (currentRoom == LoginRoomData.end())
        return 0;

    return std::distance(LoginRoomData.begin(), currentRoom);
}

static uint32 GetNearestDracthyrLoginRoomIndex(Player const* player)
{
    return GetNearestDracthyrLoginRoomIndex(player->GetPosition());
}

static uint32 GetDracthyrGuideEntry(Player const* player)
{
    return player->GetRace() == RACE_DRACTHYR_ALLIANCE ? NPC_DERVISHIAN : NPC_KODETHI;
}

static uint32 GetDracthyrMaintainSpell(Player const* player)
{
    return player->GetRace() == RACE_DRACTHYR_ALLIANCE ? SPELL_MAINTAIN_DERVISHIAN : SPELL_MAINTAIN_KODETHI;
}

static uint32 GetDracthyrSummonSpell(Player const* player)
{
    return player->GetRace() == RACE_DRACTHYR_ALLIANCE ? SPELL_SUMMON_DERVISHIAN : SPELL_SUMMON_KODETHI;
}

static Creature* FindDracthyrGuide(Player const* player)
{
    if (!player)
        return nullptr;

    uint32 const entry = GetDracthyrGuideEntry(player);

    std::list<Creature*> creatures;
    player->GetCreatureListWithOptionsInGrid(creatures, 100.0f, { .CreatureId = entry, .IgnorePhases = true, .PrivateObjectOwnerGuid = player->GetGUID() });
    return creatures.empty() ? nullptr : creatures.front();
}

static bool IsDracthyrEvoker(Player const* player)
{
    if (!player || player->GetClass() != CLASS_EVOKER)
        return false;

    switch (player->GetRace())
    {
        case RACE_DRACTHYR_HORDE:
        case RACE_DRACTHYR_ALLIANCE:
            return true;
        default:
            return false;
    }
}

static Optional<uint32> GetExpectedDracthyrLoginRoomIndex(Player const* player)
{
    if (Optional<uint32> stored = GetDracthyrLoginRoomIndex(player))
        return stored;

    return GetNearestDracthyrLoginRoomIndex(player);
}

static bool IsDracthyrGuideAtExpectedRoom(Player const* player)
{
    Creature* guide = FindDracthyrGuide(player);
    if (!guide)
        return false;

    Optional<uint32> roomIndex = GetExpectedDracthyrLoginRoomIndex(player);
    if (!roomIndex || *roomIndex >= LoginRoomData.size())
        return false;

    return guide->GetDistance(LoginRoomData[*roomIndex].SummonPosition) <= 20.0f;
}

static void DespawnDracthyrGuide(Player* player)
{
    if (!player)
        return;

    std::list<Creature*> guides;
    player->GetCreatureListWithOptionsInGrid(guides, 100.0f,
        { .CreatureId = GetDracthyrGuideEntry(player), .IgnorePhases = true, .PrivateObjectOwnerGuid = player->GetGUID() });
    for (Creature* guide : guides)
        guide->DespawnOrUnsummon(0s);

    player->RemoveAura(SPELL_MAINTAIN_KODETHI);
    player->RemoveAura(SPELL_MAINTAIN_DERVISHIAN);
}

static void SummonDracthyrGuideDirect(Player* player, uint32 roomIndex)
{
    if (!player || roomIndex >= LoginRoomData.size())
        return;

    uint32 const entry = GetDracthyrGuideEntry(player);
    Position const pos = LoginRoomData[roomIndex].SummonPosition;

    SummonPropertiesEntry const* props = nullptr;
    if (SpellInfo const* summonInfo = sSpellMgr->GetSpellInfo(GetDracthyrSummonSpell(player), DIFFICULTY_NONE))
    {
        for (SpellEffectInfo const& effect : summonInfo->GetEffects())
        {
            if (effect.IsEffect(SPELL_EFFECT_SUMMON) || effect.IsEffect(SPELL_EFFECT_SUMMON_PET))
            {
                props = sSummonPropertiesStore.LookupEntry(effect.MiscValueB);
                break;
            }
        }
    }

    player->GetMap()->SummonCreature(entry, pos, props, 0ms, player, GetDracthyrSummonSpell(player), 0, player->GetGUID());
}

static void EnsureDracthyrGuide(Player* player)
{
    if (!IsDracthyrEvoker(player))
        return;

    Optional<DracthyrIntroPlayerState> state = GetDracthyrIntroState(player->GetGUID());
    if (state && state->GuideWakeStarted)
    {
        // Guide is mid (or past) its own scripted departure — never snap it back to
        // SummonPosition for a mere distance mismatch once that's underway; only recover if it
        // has vanished outright (some unrelated despawn edge case).
        if (FindDracthyrGuide(player))
            return;
    }
    else if (IsDracthyrGuideAtExpectedRoom(player))
        return;

    Optional<uint32> roomIndex = GetExpectedDracthyrLoginRoomIndex(player);
    if (!roomIndex || *roomIndex >= LoginRoomData.size())
        return;

    DespawnDracthyrGuide(player);

    player->CastSpell(player, GetDracthyrMaintainSpell(player), true);
    player->CastSpell(player, GetDracthyrSummonSpell(player), true);

    if (!IsDracthyrGuideAtExpectedRoom(player))
        SummonDracthyrGuideDirect(player, *roomIndex);
}

static void ClearDracthyrIntroPresentationAuras(Player* player)
{
    if (!player)
        return;

    player->RemoveAurasDueToSpell(SPELL_STASIS_4);
    player->RemoveAurasDueToSpell(SPELL_DRACTHYR_MOVIE_ROOM_01);
    player->RemoveAurasDueToSpell(SPELL_DRACTHYR_MOVIE_ROOM_02);
    player->RemoveAurasDueToSpell(SPELL_DRACTHYR_MOVIE_ROOM_03);
    player->RemoveAurasDueToSpell(SPELL_DRACTHYR_MOVIE_ROOM_04);
    player->GetSceneMgr().CancelSceneByPackageId(SCENE_PKG_DRACTHYR_EVOKER_INTRO);
}

static Creature* DespawnExtraDracthyrGuidesAndGetKeeper(Player* player)
{
    if (!player)
        return nullptr;

    uint32 const entry = GetDracthyrGuideEntry(player);
    std::list<Creature*> guides;
    player->GetCreatureListWithOptionsInGrid(guides, 100.0f,
        { .CreatureId = entry, .IgnorePhases = true, .PrivateObjectOwnerGuid = player->GetGUID() });

    Position keeperPos = player->GetPosition();
    if (Optional<uint32> roomIndex = GetExpectedDracthyrLoginRoomIndex(player))
        if (*roomIndex < LoginRoomData.size())
            keeperPos = LoginRoomData[*roomIndex].SummonPosition;

    Creature* keeper = nullptr;
    for (Creature* guide : guides)
    {
        if (!keeper || guide->GetDistance(keeperPos) < keeper->GetDistance(keeperPos))
            keeper = guide;
    }

    for (Creature* guide : guides)
    {
        if (guide != keeper)
            guide->DespawnOrUnsummon(0s);
    }

    return keeper;
}

static bool TryOfferDracthyrAwakenQuest(Player* player)
{
    if (!player)
        return false;

    Optional<DracthyrIntroPlayerState> state = GetDracthyrIntroState(player->GetGUID());
    if (state && state->QuestOffered)
        return true;

    Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_AWAKEN_DRACTYHR);
    if (!quest || player->GetQuestStatus(QUEST_AWAKEN_DRACTYHR) != QUEST_STATUS_NONE)
        return true;

    if (!player->CanTakeQuest(quest, false))
        return false;

    ClearDracthyrIntroPresentationAuras(player);
    player->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Interacting);

    EnsureDracthyrGuide(player);
    Creature* guide = DespawnExtraDracthyrGuidesAndGetKeeper(player);
    if (!guide)
    {
        TC_LOG_ERROR("scripts", "Dracthyr intro: cannot offer quest {} — personal guide not spawned", QUEST_AWAKEN_DRACTYHR);
        return false;
    }

    player->PlayerTalkClass->SendCloseGossip();
    player->PlayerTalkClass->SendQuestQueryResponse(quest);

    // Retail 12.0.7 sniff: player GUID, AutoLaunched, DisplayPopup=false, QuestGiverCreatureID=0.
    // Accept: CMSG_QUEST_GIVER_ACCEPT_QUEST then CLOSE; Decline: CLOSE only (grant on ACCEPT only).
    player->PlayerTalkClass->SendQuestGiverQuestDetails(
        quest, player->GetGUID(), true, false);

    UpdateDracthyrIntroState(player->GetGUID(), [](DracthyrIntroPlayerState& introState) { introState.QuestOffered = true; });
    return true;
}

struct DracthyrGuideSummonCheckEvent : public BasicEvent
{
    explicit DracthyrGuideSummonCheckEvent(Player* player) : _player(player) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (!_player || !_player->IsInWorld())
            return true;

        if (_player->GetQuestStatus(QUEST_AWAKEN_DRACTYHR) == QUEST_STATUS_INCOMPLETE)
            EnsureDracthyrGuide(_player);
        else if (!FindDracthyrGuide(_player))
            EnsureDracthyrGuide(_player);

        return true;
    }

private:
    Player* _player;
};

// Shared "player has landed in a room" continuation: cast the room's movie, (re-)freeze via
// stasis, ensure the personal guide is up. Used by both the native-teleport path (369728,
// AfterHit below) and the hand-rolled abandon-restart path (369744, RunDracthyrAbandonRestart).
static void OnDracthyrRoomEntered(Player* player, uint32 roomIndex)
{
    if (!player || !player->IsInWorld() || roomIndex >= LoginRoomData.size())
        return;

    DracthyrLoginRoom const& room = LoginRoomData[roomIndex];

    UpdateDracthyrIntroState(player->GetGUID(), [roomIndex](DracthyrIntroPlayerState& state)
    {
        state.IntroPending = false;
        state.RoomIndex = roomIndex;
        state.QuestOffered = false;
    });

    // First-ever login already applies SPELL_STASIS_4 from `playercreateinfo_cast_spell`
    // (matches retail timing — frozen before the room teleport, not after). Re-cast here
    // is a no-op refresh for that path and the only source of the freeze on the 369744
    // quest-abandon restart path, which never goes through playercreateinfo_cast_spell.
    player->CastSpell(player, SPELL_STASIS_4, true);
    player->CastSpell(player, room.MovieSpellId, true);
    EnsureDracthyrGuide(player);
    player->m_Events.AddEventAtOffset(new DracthyrGuideSummonCheckEvent(player), 500ms);
}

// 369744 quest-abandon restart only: unlike 369728, this spell's own DB2 effects do not
// describe a room-randomizing teleport (EFFECT_0 target is a self-facing reorient, not
// TARGET_DEST_NEARBY_DB; no `spell_target_position` rows exist for it in retail data we have
// evidence of), so the relocate-to-a-new-random-room behavior stays hand-rolled here.
static void RunDracthyrAbandonRestart(Player* player, uint32 roomIndex)
{
    if (!player || !player->IsInWorld() || roomIndex >= LoginRoomData.size())
        return;

    DracthyrLoginRoom const& room = LoginRoomData[roomIndex];

    DespawnDracthyrGuide(player);

    // See AREA_FORBIDDEN_REACH: homebind must be set to the pre-teleport (hub-ish) position,
    // matching 369728's native Bind-trigger timing/position (§5c). Restart shares that contract.
    player->SetHomebind(*player, AREA_FORBIDDEN_REACH);

    WorldLocation dest(player->GetMapId(), room.PlayerPosition);
    if (!player->TeleportTo(dest, TELE_TO_SPELL, {}, SPELL_AWAKEN_DRACTYHR_QUEST_ABANDON))
    {
        TC_LOG_WARN("server", "Dracthyr login intro: abandon-restart teleport failed for {}", player->GetName());
        return;
    }

    OnDracthyrRoomEntered(player, roomIndex);
}

struct DracthyrAbandonRestartEvent : public BasicEvent
{
    DracthyrAbandonRestartEvent(Player* player, uint32 roomIndex) : _player(player), _roomIndex(roomIndex) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        RunDracthyrAbandonRestart(_player, _roomIndex);
        return true;
    }

private:
    Player* _player;
    uint32 _roomIndex;
};

static void ScheduleDracthyrAbandonRestart(Player* player, Milliseconds delay = 1s)
{
    if (!player)
        return;

    if (ExchangeDracthyrIntroPending(player->GetGUID()))
        return;

    uint32 const roomIndex = urand(0, LoginRoomData.size() - 1);
    player->m_Events.AddEventAtOffset(new DracthyrAbandonRestartEvent(player, roomIndex), delay);
}

// 369728 - Dracthyr Login
// 369744 - Awaken, Dracthyr OnQuestAbandon
//
// 369728 EFFECT_0's implicit target is TARGET_DEST_NEARBY_DB (DB2 SpellEffect evidence,
// 12.0.7.67808): TrinityCore's generic handler for that target (Spell.cpp) already picks a
// random in-range row from `spell_target_position` — this is retail's actual room-rotation
// mechanism, so the default teleport now runs natively (rows: 2026_06_30_02 SQL) instead of
// being suppressed and reimplemented by hand. EFFECT_2 (triggers Bind spell 377713) is still
// suppressed: homebind is set explicitly in HandleCast (before the teleport resolves) so the
// result doesn't depend on TrinityCore's internal effect-processing order matching retail's.
// 369744 keeps the old fully-manual path (see RunDracthyrAbandonRestart) since its own DB2
// effects are not a room-randomizing teleport.
class spell_dracthyr_login : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRACTHYR_MOVIE_ROOM_01, SPELL_DRACTHYR_MOVIE_ROOM_02, SPELL_DRACTHYR_MOVIE_ROOM_03, SPELL_DRACTHYR_MOVIE_ROOM_04 });
    }

    bool IsAbandonRestart() const
    {
        return GetSpellInfo()->Id == SPELL_AWAKEN_DRACTYHR_QUEST_ABANDON;
    }

    void HandleCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        if (IsAbandonRestart())
        {
            ScheduleDracthyrAbandonRestart(player);
            return;
        }

        player->SetHomebind(*player, AREA_FORBIDDEN_REACH);
        DespawnDracthyrGuide(player);
        UpdateDracthyrIntroState(player->GetGUID(), [](DracthyrIntroPlayerState& state) { state.QuestOffered = false; });
    }

    void SuppressAbandonTeleport(SpellEffIndex effIndex)
    {
        // 369728's native TARGET_DEST_NEARBY_DB teleport is allowed to run; only 369744's
        // self-reorient teleport (no useful room data) stays suppressed/hand-rolled.
        if (IsAbandonRestart())
            PreventHitDefaultEffect(effIndex);
    }

    void SuppressBindTrigger(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    // Same-map Player::TeleportTo() does not Relocate() synchronously — it only sends the
    // teleport packet and waits for the client's CMSG_MOVE_TELEPORT_ACK (handled in
    // WorldSession::HandleMoveTeleportAck) before actually updating position. AfterHit fires
    // long before that round trip completes, so deriving the room from player->GetPosition()
    // there would always read the pre-teleport (hub) position. Capture the room here instead,
    // at the moment TC's native TARGET_DEST_NEARBY_DB handler has already picked the random
    // spell_target_position row — this runs synchronously, before any teleport packet is sent.
    void CaptureRoomTarget(SpellDestination& dest)
    {
        if (IsAbandonRestart())
            return;

        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        uint32 const roomIndex = GetNearestDracthyrLoginRoomIndex(dest._position);
        UpdateDracthyrIntroState(player->GetGUID(), [roomIndex](DracthyrIntroPlayerState& state) { state.RoomIndex = roomIndex; });

        TC_LOG_DEBUG("scripts", "Dracthyr login intro: player {} native room target resolved to room index {}",
            player->GetName(), roomIndex);
    }

    void HandleAfterHit()
    {
        if (IsAbandonRestart())
            return;

        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        // Room index was already captured from the native destination in CaptureRoomTarget,
        // before the (still in-flight) teleport. Falls back to nearest-by-position only in the
        // defensive case where no spell_target_position rows were found.
        OnDracthyrRoomEntered(player, GetExpectedDracthyrLoginRoomIndex(player).value_or(0));
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_dracthyr_login::HandleCast);
        OnEffectHitTarget += SpellEffectFn(spell_dracthyr_login::SuppressAbandonTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
        OnEffectHitTarget += SpellEffectFn(spell_dracthyr_login::SuppressBindTrigger, EFFECT_2, SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE);
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_dracthyr_login::CaptureRoomTarget, EFFECT_0, TARGET_DEST_NEARBY_DB);
        AfterHit += SpellHitFn(spell_dracthyr_login::HandleAfterHit);
    }
};

// First-login intro: now drives the actual room teleport by casting 369728 (native
// TARGET_DEST_NEARBY_DB target — see spell_dracthyr_login above), rather than bypassing the
// spell and hand-picking a room. `playercreateinfo_cast_spell`-driven auto-cast at character
// creation is still not used (unreliable during the login handshake); casting it ourselves
// from OnLogin, once the player is fully in world, has no such reliability problem.
class player_dracthyr_intro_login : public PlayerScript
{
public:
    player_dracthyr_intro_login() : PlayerScript("player_dracthyr_intro_login") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        if (!IsDracthyrEvoker(player))
            return;

        if (firstLogin)
        {
            if (!ExchangeDracthyrIntroPending(player->GetGUID()))
                player->CastSpell(player, SPELL_DRACTHYR_LOGIN, true);
            return;
        }

        if (player->GetMapId() == MAP_FORBIDDEN_REACH &&
            player->GetQuestStatus(QUEST_AWAKEN_DRACTYHR) == QUEST_STATUS_INCOMPLETE)
            EnsureDracthyrGuide(player);
    }

    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 newArea) override
    {
        if (!IsDracthyrEvoker(player) || player->GetMapId() != MAP_FORBIDDEN_REACH)
            return;

        // Hub spawn fires zone update before OnLogin intro; spell_area would summon at nearest room (wrong).
        if (!GetDracthyrLoginRoomIndex(player) &&
            player->GetQuestStatus(QUEST_AWAKEN_DRACTYHR) == QUEST_STATUS_NONE)
            return;

        if (newArea != AREA_WAR_CRECHE &&
            player->GetQuestStatus(QUEST_AWAKEN_DRACTYHR) != QUEST_STATUS_INCOMPLETE)
            return;

        EnsureDracthyrGuide(player);
    }

    void OnLogout(Player* player) override
    {
        EraseDracthyrIntroState(player->GetGUID());
    }
};

// 3730 - Dracthyr Evoker Intro (Post Movie)
class scene_dracthyr_evoker_intro : public SceneScript
{
public:
    scene_dracthyr_evoker_intro() : SceneScript("scene_dracthyr_evoker_intro") { }

    void OnSceneEnd(Player* player)
    {
        EnsureDracthyrGuide(player);
        player->CastSpell(player, SPELL_STASIS_1, true);
    }

    void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        OnSceneEnd(player);
    }

    void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        OnSceneEnd(player);
    }
};

// 369730 - Summon Dervishian
// 370111 - Summon Kodethi
class spell_dracthyr_summon_dervishian : public SpellScript
{
    void SetDest(SpellDestination& dest) const
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        uint32 roomIndex = GetExpectedDracthyrLoginRoomIndex(player).value_or(0);
        if (roomIndex >= LoginRoomData.size())
            return;

        Position const& targetPos = LoginRoomData[roomIndex].SummonPosition;
        dest.Relocate(targetPos);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_dracthyr_summon_dervishian::SetDest, EFFECT_0, TARGET_DEST_NEARBY_ENTRY);
    }
};

// 366636 - Stasis 3 (Awaken, Dracthyr quest offer — player GUID like EffectQuestStart)
class spell_dracthyr_stasis_3 : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return spellInfo->HasEffect(SPELL_EFFECT_QUEST_START);
    }

    void OfferAwakenQuest(SpellEffIndex effIndex)
    {
        if (GetEffectInfo().Effect != SPELL_EFFECT_QUEST_START)
            return;

        PreventHitDefaultEffect(effIndex);

        Player* player = GetHitPlayer();
        if (!player)
            player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        TryOfferDracthyrAwakenQuest(player);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dracthyr_stasis_3::OfferAwakenQuest, EFFECT_ALL, SPELL_EFFECT_ANY);
    }
};

// Retail sniff (Room 2, Horde/Kodethi, 12.0.7.68275): guide starts walking ~13.8s after the
// wake-up Conversation is created (SMSG_ON_MONSTER_MOVE at +13835ms). Server-authored delay,
// re-measured from precise packet timestamps (SPELL_GO -> Conversation create -> walk); supersedes
// an earlier, less precise 5.6s estimate.
//
// Local server sniff (tc-64864 kodethi 1) diffed against the same retail capture showed our guide
// moved (SMSG_ON_MONSTER_MOVE with a valid pathed spline) but never played a walk animation and
// instead appeared to "float" to the target — because we never cleared the guide's AI idle
// animation kit. Kodethi/Dervishian's creature_template.AIAnimKitID (23987, the "imprisoned"
// idle pose) is applied automatically by the core on spawn and stays active as an override for
// the lifetime of the creature unless explicitly cleared; the client keeps rendering that
// override's baked pose on top of spline position updates, which reads as "floating" instead of
// walking. Retail clears it (SMSG_SET_AI_ANIM_KIT AnimKitID: 0) twice: once right after the
// disintegrate hit (so the guide isn't frozen mid-conversation), and again in the same tick as
// the walk's SMSG_ON_MONSTER_MOVE (so normal locomotion animation takes over for the walk).
//
// B5/B6 follow-up (dracthyr-forbidden-reach-handoff): a `MovePoint(generatePath=true)` call
// doesn't force any particular landing facing — the creature just keeps whatever heading its last
// spline segment left it with, which rarely matches retail's exact static orientation once the
// navmesh route's last leg differs even slightly. Unlike the ledge/glide phases above (fixed
// splines with known-good sniffed durations), a navmesh-routed walk's duration varies with start
// position, so a guessed fixed delay isn't reliable here. Polls `MotionMaster` state on a short
// interval instead (bounded by `attemptsLeft` as a safety net) and snaps facing the instant the
// point-move generator clears, rather than adding a dedicated CreatureAI class just for a
// movement-complete callback on a couple of one-off cinematic-adjacent moves.
struct DracthyrFaceOnArrivalEvent : public BasicEvent
{
    DracthyrFaceOnArrivalEvent(Creature* creature, float orientation, uint32 attemptsLeft = 40)
        : _creature(creature), _orientation(orientation), _attemptsLeft(attemptsLeft) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (!_creature || !_creature->IsInWorld())
            return true;

        if (_attemptsLeft && _creature->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE)
        {
            _creature->m_Events.AddEventAtOffset(new DracthyrFaceOnArrivalEvent(_creature, _orientation, _attemptsLeft - 1), 250ms);
            return true;
        }

        _creature->SetFacingTo(_orientation);
        return true;
    }

private:
    Creature* _creature;
    float _orientation;
    uint32 _attemptsLeft;
};

// Phase 3 (final): lands the guide back on walkable ground after the glide and hands off to
// TrinityCore's own navmesh pathing for the rest of the corridor walk — see
// `DRACTHYR_GUIDE_WAKE_WALK_TARGET`'s comment for why `generatePath=true` is safe again here
// (unlike the glide's steep drop, this stretch has a normal walkable route).
struct DracthyrGuideWakeLandEvent : public BasicEvent
{
    DracthyrGuideWakeLandEvent(Creature* guide) : _guide(guide) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_guide && _guide->IsInWorld())
        {
            _guide->SetDisableGravity(false);
            _guide->GetMotionMaster()->MovePoint(0, DRACTHYR_GUIDE_WAKE_WALK_TARGET, true);
            _guide->m_Events.AddEventAtOffset(new DracthyrFaceOnArrivalEvent(_guide, DRACTHYR_GUIDE_WAKE_WALK_TARGET.GetOrientation()), 250ms);
        }

        return true;
    }

private:
    Creature* _guide;
};

// Phase 2: the actual glide-off-the-ledge moment. Built directly with `Movement::MoveSplineInit`
// (the same established pattern as e.g. `boss_razorscale.cpp`/`arena_empyrean_domain.cpp`) rather
// than `MotionMaster::MovePoint`, since `MovePoint` has no way to request the `Flying`/`CatmullRom`
// spline flags or a multi-point curve — matching retail's wire-level spline exactly requires the
// lower-level API here.
struct DracthyrGuideWakeGlideEvent : public BasicEvent
{
    DracthyrGuideWakeGlideEvent(Creature* guide, Position const& summonPosition) : _guide(guide), _summonPosition(summonPosition) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_guide && _guide->IsInWorld())
        {
            Position const bump = _summonPosition.GetPositionWithOffset(DRACTHYR_GUIDE_WAKE_GLIDE_BUMP_LOCAL_OFFSET);
            Position const mid = _summonPosition.GetPositionWithOffset(DRACTHYR_GUIDE_WAKE_GLIDE_MID_LOCAL_OFFSET);
            Position const land = _summonPosition.GetPositionWithOffset(DRACTHYR_GUIDE_WAKE_GLIDE_LAND_LOCAL_OFFSET);

            // path[0] is a required placeholder only — MoveSplineInit::Launch() always overwrites
            // it with the unit's real current position (the ledge spot from phase 1), matching
            // MoveSplineInit::MoveTo()'s own convention for its start element.
            Movement::PointsArray path =
            {
                G3D::Vector3(bump.GetPositionX(), bump.GetPositionY(), bump.GetPositionZ()),
                G3D::Vector3(bump.GetPositionX(), bump.GetPositionY(), bump.GetPositionZ()),
                G3D::Vector3(mid.GetPositionX(), mid.GetPositionY(), mid.GetPositionZ()),
                G3D::Vector3(land.GetPositionX(), land.GetPositionY(), land.GetPositionZ())
            };

            _guide->SetDisableGravity(true);

            std::function<void(Movement::MoveSplineInit&)> initializer = [path = std::move(path)](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(path);
                init.SetFly();
                init.SetSmooth();
                init.SetUncompressed();
                init.SetWalk(false);
                init.SetVelocity(10.0f);
            };
            _guide->GetMotionMaster()->LaunchMoveSpline(std::move(initializer), 0, MOTION_PRIORITY_NORMAL, POINT_MOTION_TYPE);

            _guide->m_Events.AddEventAtOffset(new DracthyrGuideWakeLandEvent(_guide), DRACTHYR_GUIDE_WAKE_GLIDE_DURATION);
        }

        return true;
    }

private:
    Creature* _guide;
    Position _summonPosition;
};

// Phase 1: short, flat walk from the door out onto the room's balcony/ledge — the last bit of
// normal ground movement before the glide. `generatePath=false` because this is a short local
// hop (no navmesh concerns at this distance); mirrors the walk's own retail spline flags (no
// Flying here, just a normal steered walk).
struct DracthyrGuideWakeLedgeWalkEvent : public BasicEvent
{
    DracthyrGuideWakeLedgeWalkEvent(Creature* guide, Position const& summonPosition) : _guide(guide), _summonPosition(summonPosition) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_guide && _guide->IsInWorld())
        {
            _guide->SetAIAnimKitId(0);
            Position const ledge = _summonPosition.GetPositionWithOffset(DRACTHYR_GUIDE_WAKE_LEDGE_LOCAL_OFFSET);
            _guide->GetMotionMaster()->MovePoint(0, ledge, false);

            _guide->m_Events.AddEventAtOffset(new DracthyrGuideWakeGlideEvent(_guide, _summonPosition), DRACTHYR_GUIDE_WAKE_LEDGE_WALK_DURATION);
        }

        return true;
    }

private:
    Creature* _guide;
    Position _summonPosition;
};

// Retail sniff (both Room 2 and Room 4 captures): neither the wake-up itself nor its Conversation
// are instant on the Disintegrate hit. AI idle AnimKit clears ~0.5s after the hit (485ms average
// of 512ms/461ms across the two captures — the guide isn't frozen mid-disintegrate-channel, but
// isn't unfrozen at the exact instant of the hit either), and the Conversation doesn't start
// until ~2.3s after the hit (2303ms average of 2284ms/2322ms — well after the 3s Disintegrate
// channel visual has mostly played out). Owner feedback: our previous synchronous (0ms) start
// felt "a little too fast" — this matches, since we started ~2.3s early.
struct DracthyrGuideWakeAnimKitClearEvent : public BasicEvent
{
    DracthyrGuideWakeAnimKitClearEvent(Creature* guide) : _guide(guide) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_guide && _guide->IsInWorld())
            _guide->SetAIAnimKitId(0);

        return true;
    }

private:
    Creature* _guide;
};

struct DracthyrGuideWakeConversationStartEvent : public BasicEvent
{
    DracthyrGuideWakeConversationStartEvent(ObjectGuid playerGuid, Creature* guide) : _playerGuid(playerGuid), _guide(guide) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (!_guide || !_guide->IsInWorld())
            return true;

        Player* player = ObjectAccessor::FindPlayer(_playerGuid);
        if (!player)
            return true;

        Conversation* conversation = Conversation::CreateConversation(
            CONVERSATION_DRACTHYR_GUIDE_WAKE, player, *_guide, player->GetGUID(), nullptr, false);
        if (!conversation)
            return true;

        conversation->AddActor(CONVERSATION_ACTOR_DRACTHYR_GUIDE_WAKE_UNUSED, 0, ObjectGuid::Empty);
        conversation->AddActor(CONVERSATION_ACTOR_DRACTHYR_GUIDE_WAKE_SPEAKER, 1, _guide->GetGUID());
        conversation->Start();

        if (Optional<uint32> roomIndex = GetExpectedDracthyrLoginRoomIndex(player); roomIndex && *roomIndex < LoginRoomData.size())
            _guide->m_Events.AddEventAtOffset(new DracthyrGuideWakeLedgeWalkEvent(_guide, LoginRoomData[*roomIndex].SummonPosition), 13835ms);

        return true;
    }

private:
    ObjectGuid _playerGuid;
    Creature* _guide;
};

static void StartDracthyrGuideWakeConversation(Player* player, Creature* guide)
{
    if (!player || !guide)
        return;

    UpdateDracthyrIntroState(player->GetGUID(), [](DracthyrIntroPlayerState& state) { state.GuideWakeStarted = true; });

    guide->m_Events.AddEventAtOffset(new DracthyrGuideWakeAnimKitClearEvent(guide), 485ms);
    guide->m_Events.AddEventAtOffset(new DracthyrGuideWakeConversationStartEvent(player->GetGUID(), guide), 2303ms);
}

// Retail sniff timing (12.0.7.68275, `dump_..._2026-06-30_18-15-43`): Tethalash's own MonsterSay
// (216876, sent by the always-present NPC_TETHALASH_REUNITED, not the personal 181680) lands
// 2284ms after the disintegrate SMSG_SPELL_GO; Azurathel's (213820, sent by the already-morphed
// NPC_AZURATHEL_QUEST_ENDER) lands 1229ms after hers. Both measured directly from packet
// timestamps (see dracthyr-forbidden-reach-handoff B5).
constexpr Milliseconds DRACTHYR_TETHALASH_WAKE_SPEAK_DELAY = 2284ms;
constexpr Milliseconds DRACTHYR_AZURATHEL_WAKE_SPEAK_DELAY = 1229ms;

// Retail sniff (12.0.7.68275, `dump_..._2026-06-30_18-15-43`): Tethalash does *not* just vanish
// from the Lower War Creche. A deeper re-pass (following the `ReplaceObject`/smooth-phasing field
// on her CreateObject block, missed on the first pass) found she's smoothly re-entried in place
// (181680 -> 186864, same technique conceptually as Azurathel's `UpdateEntry`) and then walks the
// whole way to the back room via 7 chained `SMSG_ON_MONSTER_MOVE` legs, all plain ground-steered
// splines (`Steering` flag only — never `Flying`) with gently sloped Z (210 -> 205 -> 207, i.e.
// stairs/ramps, not a cliff), landing at DRACTHYR_TETHALASH_REUNITED_TARGET facing
// DRACTHYR_TETHALASH_REUNITED_TARGET's own orientation ~18.3s after the first move command.
// That's a normal walkable route TrinityCore's own navmesh should reproduce fine with a single
// `MovePoint(generatePath=true)` call — no need to hand-replicate all 7 waypoints.
constexpr Position DRACTHYR_TETHALASH_REUNITED_TARGET = { 5803.7197f, -2921.448f, 207.1868f, 0.9247374f };

// Fires ~2.3s after the disintegrate hit (matches sniff: speak text and first move command land
// within ~114ms of each other) — re-entries the freed personal Tethalash into the always-visible
// NPC_TETHALASH_REUNITED identity, has her speak her wake-up line, then sends her walking to the
// back room. See DRACTHYR_TETHALASH_REUNITED_TARGET above for the wire evidence this replaces the
// earlier (owner-rejected) "despawn and let a separate ambient double speak" approach.
// The event lives on the creature, so the player may have logged out before it fires - keep only
// the guid and resolve it here instead of holding a raw Player pointer across the delay.
struct DracthyrTethalashWakeEvent : public BasicEvent
{
    DracthyrTethalashWakeEvent(ObjectGuid playerGuid, Creature* creature) : _playerGuid(playerGuid), _creature(creature) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (!_creature || !_creature->IsInWorld())
            return true;

        _creature->UpdateEntry(NPC_TETHALASH_REUNITED);

        if (Player* player = ObjectAccessor::FindPlayer(_playerGuid))
            sCreatureTextMgr->SendChat(_creature, TEXT_GROUP_TETHALASH_REUNITED_WAKE, player, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL, TEXT_RANGE_NORMAL);

        _creature->GetMotionMaster()->MovePoint(0, DRACTHYR_TETHALASH_REUNITED_TARGET, true);
        _creature->m_Events.AddEventAtOffset(new DracthyrFaceOnArrivalEvent(_creature, DRACTHYR_TETHALASH_REUNITED_TARGET.GetOrientation()), 250ms);

        return true;
    }

private:
    ObjectGuid _playerGuid;
    Creature* _creature;
};

struct DracthyrAzurathelWakeSpeakEvent : public BasicEvent
{
    DracthyrAzurathelWakeSpeakEvent(ObjectGuid playerGuid, Creature* commander) : _playerGuid(playerGuid), _commander(commander) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_commander && _commander->IsInWorld())
            if (Player* player = ObjectAccessor::FindPlayer(_playerGuid))
                sCreatureTextMgr->SendChat(_commander, TEXT_GROUP_AZURATHEL_ENDER_WAKE, player, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL, TEXT_RANGE_NORMAL);

        return true;
    }

private:
    ObjectGuid _playerGuid;
    Creature* _commander;
};

// 362355 - Disintegrate (spellclick on stasis dracthyr)
class spell_disintegrate_dracthyr_awaken : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRACTHYR_STASIS_NPC, SPELL_DRACTHYR_STASIS_LOWER });
    }

    void HandleHitTarget()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        target->RemoveAurasDueToSpell(SPELL_DRACTHYR_STASIS_NPC);
        target->RemoveAurasDueToSpell(SPELL_DRACTHYR_STASIS_LOWER);

        uint32 const entry = target->GetEntry();
        player->KilledMonsterCredit(entry, target->GetGUID());

        if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(entry))
        {
            for (uint32 killCredit : cInfo->KillCredit)
            {
                if (killCredit)
                    player->KilledMonsterCredit(killCredit, ObjectGuid::Empty);
            }
        }

        if (entry == NPC_AZURATHEL_STASIS)
        {
            player->KilledMonsterCredit(NPC_KILL_CREDIT_CAVE_IN, ObjectGuid::Empty);

            if (Creature* creature = target->ToCreature())
            {
                creature->UpdateEntry(NPC_AZURATHEL_QUEST_ENDER);
                creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                creature->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
                // B6 (dracthyr-forbidden-reach-handoff): 181056's creature_template_addon carries
                // the same frozen-stasis aiAnimKit (24297) as 183380 (base client data, not fork
                // content) — same root cause class as the Kodethi "floating" bug (see
                // DracthyrGuideWakeLedgeWalkEvent above): `UpdateEntry` doesn't re-run addon
                // application, so the override sticks unless cleared explicitly.
                creature->SetAIAnimKitId(0);
                creature->m_Events.AddEventAtOffset(new DracthyrAzurathelWakeSpeakEvent(player->GetGUID(), creature), DRACTHYR_AZURATHEL_WAKE_SPEAK_DELAY);
            }
        }
        else if (entry == NPC_KODETHI || entry == NPC_DERVISHIAN)
        {
            if (Creature* creature = target->ToCreature())
            {
                creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                StartDracthyrGuideWakeConversation(player, creature);
            }
        }
        else if (entry == NPC_TETHALASH)
        {
            // B5/B6 (dracthyr-forbidden-reach-handoff): Tethalash's wake-up MonsterSay, then a
            // real navmesh walk to the back room — see DracthyrTethalashWakeEvent.
            if (Creature* creature = target->ToCreature())
            {
                creature->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                creature->m_Events.AddEventAtOffset(new DracthyrTethalashWakeEvent(player->GetGUID(), creature), DRACTHYR_TETHALASH_WAKE_SPEAK_DELAY);
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_disintegrate_dracthyr_awaken::HandleHitTarget);
    }
};

// 64864 - Awaken, Dracthyr
class quest_awaken_dracthyr : public QuestScript
{
public:
    quest_awaken_dracthyr() : QuestScript("quest_awaken_dracthyr") { }

    void OnQuestStatusChange(Player* player, Quest const* quest, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (quest->GetQuestId() == QUEST_AWAKEN_DRACTYHR && newStatus == QUEST_STATUS_INCOMPLETE)
            EnsureDracthyrGuide(player);

        if (newStatus == QUEST_STATUS_NONE)
        {
            player->CastSpell(player, SPELL_AWAKEN_DRACTYHR_QUEST_ABANDON, false);
            // remove summon aura to relocate questgiver to new random room
            player->RemoveAura(SPELL_MAINTAIN_DERVISHIAN);
            player->RemoveAura(SPELL_MAINTAIN_KODETHI);
        }
    }
};

// 30308 - Stasis Feedback
struct at_dracthyr_stasis_feedback : AreaTriggerAI
{
    at_dracthyr_stasis_feedback(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->CastSpell(nullptr, SPELL_STASIS_FEEDBACK_KNOCKBACK, false);
        if (Unit* caster = at->GetCaster())
            caster->CastSpell(caster->GetPosition(), SPELL_STASIS_FEEDBACK_VISUAL, true);
    }
};

void AddSC_zone_the_forbidden_reach()
{
    new player_dracthyr_intro_login();
    RegisterSpellScript(spell_dracthyr_login);
    new scene_dracthyr_evoker_intro();
    RegisterSpellScript(spell_dracthyr_summon_dervishian);
    RegisterSpellScript(spell_dracthyr_stasis_3);
    RegisterSpellScript(spell_disintegrate_dracthyr_awaken);
    new quest_awaken_dracthyr();
    RegisterAreaTriggerAI(at_dracthyr_stasis_feedback);
}
