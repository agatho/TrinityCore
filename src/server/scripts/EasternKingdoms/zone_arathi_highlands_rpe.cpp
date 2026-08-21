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

#include "ScriptMgr.h"
#include "Duration.h"
#include "GossipDef.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Spell.h"
#include "SpellInfo.h"

/*######
## Arathi Returning Player Experience ("Catch Up"), map 2927
######*/

enum ArathiRpe
{
    // UNVERIFIED: taken from a third-party capture of retail 12.0.7.68453 and not confirmed
    // against our own data.
    MAP_ARATHI_RPE                  = 2927,
    QUEST_GNOLL_WAY                 = 90882,   // first RPE quest, offered on the entry pad
    QUEST_TO_GOSHEK_FARM            = 90883,
    NPC_CREDIT_ARATHI_RPE_MOUNT     = 239009,

    // Finale: PlayerChoice 902 is the last beat of the Catch Up experience (content branch's
    // player_choice/player_choice_response 902 rows); quest 90911 carries the player up to it.
    QUEST_ARATHI_RPE_FINALE          = 90911,
    PLAYERCHOICE_ARATHI_RPE_FINALE   = 902,

    // The "Dragonflight" finale option routes to a faction-specific destination quest. A single
    // player_choice_response row carries only ONE RewardQuestID, so whichever literal the content
    // authored has to be remapped to the player's own team here (retail serves the faction-correct
    // one per character). The other two options (TWW-Recap 93929, TWW 92405) are faction-neutral.
    QUEST_DRAGONFLIGHT_ALLIANCE      = 65436,
    QUEST_DRAGONFLIGHT_HORDE         = 65435,

    // "Leave Catch Up Experience" early-exit affordance. The capture proves there is NO client
    // opcode/API/string for leaving (only CMSG_ENCOUNTER_JOURNAL_START_ARATHI_RPE exists, for
    // entering) - retail drives the exit through the RPE guide NPC's gossip. The guide NPCs already
    // carry gossip in the capture: Alliance Jaina 244714 -> menu 39348, Horde Thrall 244715 -> menu
    // 39349. The content branch (61_gossip.sql) adds a "Leave Catch Up Experience" option (OptionID
    // = GOSSIP_OPTION_LEAVE_RPE) to those menus; this script handles its selection.
    // The RPE faction leaders. Both stand together on the map (allied story beat, visible to both
    // factions), but the shared quests they co-give (90882/90883 at the arrival pad, 90911 at the
    // Stromgarde hub) must be offered ONLY by the player's own leader. NPC entries by side:
    NPC_RPE_JAINA_PAD                = 244643,   // Alliance pad greeter (90882/90883)
    NPC_RPE_THRALL_PAD               = 244642,   // Horde pad greeter (90882/90883)
    NPC_ARATHI_RPE_GUIDE_ALLIANCE    = 244714,   // Alliance hub Jaina (90911 + Leave gossip)
    NPC_ARATHI_RPE_GUIDE_HORDE       = 244715,   // Horde hub Thrall (90911 + Leave gossip)
    GOSSIP_MENU_RPE_GUIDE_ALLIANCE   = 39348,
    GOSSIP_MENU_RPE_GUIDE_HORDE      = 39349,
    GOSSIP_OPTION_LEAVE_RPE          = 1,    // gossip_menu_option.OptionID -> arrives as gossipListId

    // Catch Up intro cinematic - an in-engine CINEMATIC_START (not a movie) played on entering the
    // RPE map, before any quest. CinematicSequences id PINNED FROM THE WIRE = 77: SMSG_TRIGGER_CINEMATIC
    // (opcode 0x4C0005, 4-byte body = the sequence id) fires id 77 at the arrival tick in BOTH captures
    // (Alliance 69382 arrival+328, Horde 69404 arrival+419) - the same 0x4C0005 also fires the finale
    // cinematic 107 ~30min later in both, confirming it is the cinematic-trigger opcode. (An earlier
    // DB2-join guess of "15 candidates 2..259" was wrong - it read the CinematicSequences enumeration
    // stream, not the trigger. The wire is authoritative.) Its camera Conversation carries the 10-line
    // Arathi narration (broadcast_text 295416-295418/295519-295520/301757-301761).
    CINEMATIC_ARATHI_RPE_INTRO       = 77
};

// Faction capitals to send the player to once the Catch Up finale choice has been made. These are
// the same literal coordinates CharacterHandler.cpp::HandleCharRaceOrFactionChangeOpcode already
// uses to reset a character's homebind after a faction change (Stormwind / Orgrimmar), reused here
// rather than duplicated as a new pair of magic numbers.
enum ArathiRpeLeaveDestination
{
    MAP_EASTERN_KINGDOMS = 0,
    MAP_KALIMDOR         = 1
};

constexpr float ARATHI_RPE_LEAVE_ALLIANCE_X = -8867.68f;
constexpr float ARATHI_RPE_LEAVE_ALLIANCE_Y = 673.373f;
constexpr float ARATHI_RPE_LEAVE_ALLIANCE_Z = 97.9034f;

constexpr float ARATHI_RPE_LEAVE_HORDE_X = 1633.33f;
constexpr float ARATHI_RPE_LEAVE_HORDE_Y = -4439.11f;
constexpr float ARATHI_RPE_LEAVE_HORDE_Z = 15.7588f;

// Single exit path out of the Catch Up experience, shared by the finale PlayerChoice and the guide
// NPC's "Leave Catch Up Experience" gossip option: send the player to their own faction capital.
// IsPlayerInRPE note (Phase K, resolved): three independent wire/RE analyses concluded there is NO
// server-side RPE UpdateField to set or clear here - PlayerFlags/PlayerFlagsEx were disproven on the
// wire, and this build's protocol-generated ActivePlayerData/PlayerData carry no RPE field at all,
// so C_PlayerInfo.IsPlayerInRPE() is client-local (the client knows it is in RPE because it
// initiated entry via CMSG_ENCOUNTER_JOURNAL_START_ARATHI_RPE / character-select). Nothing to write
// on entry or exit; the client tutorial coaches are driven client-side. See the Phase-K reports.
inline void SendPlayerHomeFromRpe(Player* player)
{
    if (player->GetTeamId() == TEAM_ALLIANCE)
        player->TeleportTo(MAP_EASTERN_KINGDOMS, ARATHI_RPE_LEAVE_ALLIANCE_X, ARATHI_RPE_LEAVE_ALLIANCE_Y, ARATHI_RPE_LEAVE_ALLIANCE_Z, 0.0f);
    else
        player->TeleportTo(MAP_KALIMDOR, ARATHI_RPE_LEAVE_HORDE_X, ARATHI_RPE_LEAVE_HORDE_Y, ARATHI_RPE_LEAVE_HORDE_Z, 0.0f);
}

// Quest 90883 has a kill-credit objective that retail satisfies when the player mounts up:
// the capture shows SMSG_QUEST_UPDATE_ADD_CREDIT for QuestID 90883 / ObjectID 239009 with an
// empty VictimGUID right after the mount spell resolves.
//
// This deliberately does NOT live in AuraEffect::HandleAuraMounted - that runs for every mount
// application of every player on the server. Spell::_cast is the cast-completion point (the
// same moment retail sends SPELL_GO), and the checks below keep the credit confined to the RPE
// map and to a character actually on that quest.
class player_arathi_rpe_mount_credit : public PlayerScript
{
public:
    player_arathi_rpe_mount_credit() : PlayerScript("player_arathi_rpe_mount_credit") { }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (player->GetMapId() != MAP_ARATHI_RPE)
            return;

        if (player->GetQuestStatus(QUEST_TO_GOSHEK_FARM) != QUEST_STATUS_INCOMPLETE)
            return;

        if (!spell->GetSpellInfo()->HasAura(SPELL_AURA_MOUNTED))
            return;

        player->KilledMonsterCredit(NPC_CREDIT_ARATHI_RPE_MOUNT);
    }
};

// Retail plays the Catch Up intro cinematic on ENTERING the RPE map, before the first quest (the
// capture recorded CINEMATIC_START with an empty quest log, and the client RPE tutorial addon has
// no cinematic call - so it is server-fired, not client-auto-played). OnMapChanged runs after the
// teleport/login into map 2927 completes, which is the retail timing.
class player_arathi_rpe_intro_cinematic : public PlayerScript
{
public:
    player_arathi_rpe_intro_cinematic() : PlayerScript("player_arathi_rpe_intro_cinematic") { }

    void OnMapChanged(Player* player) override
    {
        if (player->GetMapId() != MAP_ARATHI_RPE)
            return;

        // Fire on entry to the RPE map until the experience is finished. Retail plays it once, on the
        // very first entry, but gating on "90882 not yet accepted" made it impossible to re-see while
        // testing (a character that already took the first quest never replays it). Gate only on the
        // finale being rewarded, so it plays each time you enter map 2927 until you complete the
        // experience, then stops. (QUEST_GNOLL_WAY is left in the enum; the once-per-first-entry
        // behaviour can be restored later once testing is done.)
        if (player->GetQuestRewardStatus(QUEST_ARATHI_RPE_FINALE))
            return;

        // OnMapChanged runs during the world-add, before the client has finished the loading screen,
        // so a cinematic sent right now is dropped. Defer it ~1.5s so the client is fully in-world
        // when SMSG_TRIGGER_CINEMATIC arrives. Re-resolve the player from GUID inside the event (the
        // raw pointer must not be captured -- the player may have left by the time it fires).
        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([guid]()
        {
            if (Player* p = ObjectAccessor::FindPlayer(guid))
                if (p->GetMapId() == MAP_ARATHI_RPE && !p->GetQuestRewardStatus(QUEST_ARATHI_RPE_FINALE))
                    p->SendCinematicStart(CINEMATIC_ARATHI_RPE_INTRO);
        }, Milliseconds(1500));
    }
};

// PlayerChoice 902 is the RPE finale: the client sends CMSG_CHOICE_RESPONSE, QuestHandler.cpp
// resolves it against player_choice/player_choice_response and calls sScriptMgr->OnPlayerChoiceResponse,
// which dispatches by playerchoice_template.ScriptId to a PlayerChoiceScript (not a PlayerScript -
// there is no PlayerScript::OnPlayerChoiceResponse hook in this fork's ScriptMgr, only
// PlayerChoiceScript::OnResponse; see ScriptMgr.cpp ScriptMgr::OnPlayerChoiceResponse).
//
// IMPORTANT for the content branch: player_choice_template row 902 needs
// ScriptName = "playerchoice_arathi_rpe_finale" for this to fire.
class playerchoice_arathi_rpe_finale : public PlayerChoiceScript
{
public:
    playerchoice_arathi_rpe_finale() : PlayerChoiceScript("playerchoice_arathi_rpe_finale") { }

    void OnResponse(WorldObject* /*object*/, Player* player, PlayerChoice const* choice, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        if (choice->ChoiceId != PLAYERCHOICE_ARATHI_RPE_FINALE)
            return;

        if (player->GetMapId() != MAP_ARATHI_RPE)
            return;

        // Retail's capture shows the finale choice answered while quest 90911 is still
        // QUEST_STATUS_INCOMPLETE (the choice widget is the quest's closing beat, not a reward of
        // turning it in elsewhere), so credit it here rather than depending on a turn-in NPC.
        if (player->GetQuestStatus(QUEST_ARATHI_RPE_FINALE) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_ARATHI_RPE_FINALE);

        // player_choice_response.RewardQuestID (authored per-response on the content branch) is
        // the destination quest for whichever option the player picked - Dragonflight 65435/65436,
        // TWW-Recap 93929, or TWW 92405 depending on faction/catch-up bucket. Core only puts
        // RewardQuestID on the wire for client display (see QuestPackets.cpp) - nothing grants it
        // server side - so do that explicitly here instead of duplicating the per-response quest
        // ids in this file.
        if (response->RewardQuestID)
        {
            // Remap the Dragonflight destination to the player's own faction. Either literal of the
            // pair maps to the team-correct quest; the faction-neutral options pass through unchanged.
            uint32 destinationQuestId = *response->RewardQuestID;
            if (destinationQuestId == QUEST_DRAGONFLIGHT_ALLIANCE || destinationQuestId == QUEST_DRAGONFLIGHT_HORDE)
                destinationQuestId = (player->GetTeamId() == TEAM_ALLIANCE) ? QUEST_DRAGONFLIGHT_ALLIANCE : QUEST_DRAGONFLIGHT_HORDE;

            if (Quest const* destination = sObjectMgr->GetQuestTemplate(destinationQuestId))
            {
                if (player->CanTakeQuest(destination, false) && !player->GetQuestRewardStatus(destination->GetQuestId()))
                    player->AddQuest(destination, nullptr);
            }
            else
                TC_LOG_ERROR("scripts", "playerchoice_arathi_rpe_finale: response {} destination quest {} is not a valid quest template",
                    response->ResponseId, destinationQuestId);
        }

        // Making the finale choice is itself an exit from the experience: send the player home to
        // their faction capital. Shares the exact path with the guide NPC's "Leave" gossip option.
        SendPlayerHomeFromRpe(player);
    }
};

// Which faction a given RPE leader NPC belongs to (TEAM_NEUTRAL if it is not one of the four).
inline TeamId ArathiRpeLeaderTeam(uint32 entry)
{
    switch (entry)
    {
        case NPC_RPE_JAINA_PAD:
        case NPC_ARATHI_RPE_GUIDE_ALLIANCE:
            return TEAM_ALLIANCE;
        case NPC_RPE_THRALL_PAD:
        case NPC_ARATHI_RPE_GUIDE_HORDE:
            return TEAM_HORDE;
        default:
            return TEAM_NEUTRAL;
    }
}

// AI for the four RPE faction leaders (Alliance Jaina 244643/244714, Horde Thrall 244642/244715).
// Both leaders are visible to everyone (they are fighting together), but the quests they co-give are
// single shared ids (90882/90883/90911) that retail personally-phases so only the player's OWN
// leader offers them. TrinityCore gates quests per-quest, never per-(NPC, team), so this AI does the
// personal-phase equivalent: for a player of the OTHER faction the leader shows no questgiver marker
// (GetDialogStatus -> None) and his interaction offers nothing (OnGossipHello -> handled/closed),
// while the player's own leader falls through to default questgiver behaviour. The hub leaders
// (244714/244715) additionally carry the "Leave Catch Up Experience" gossip option, handled below.
// Requires creature_template.ScriptName = 'npc_arathi_rpe_leader' on all four NPCs (content branch).
struct npc_arathi_rpe_leader : public ScriptedAI
{
    npc_arathi_rpe_leader(Creature* creature) : ScriptedAI(creature) { }

    bool IsWrongFactionLeaderFor(Player const* player) const
    {
        TeamId leaderTeam = ArathiRpeLeaderTeam(me->GetEntry());
        return leaderTeam != TEAM_NEUTRAL && player->GetTeamId() != leaderTeam;
    }

    Optional<QuestGiverStatus> GetDialogStatus(Player const* player) override
    {
        if (IsWrongFactionLeaderFor(player))
            return QuestGiverStatus::None;   // the other faction's leader: no '!' / no status-driven offer
        return {};                           // own leader: default computation
    }

    bool OnGossipHello(Player* player) override
    {
        if (IsWrongFactionLeaderFor(player))
        {
            CloseGossipMenuFor(player);      // silent story ally for the other faction - offers nothing
            return true;
        }
        return false;                        // own leader: default quest/gossip handling (offer proceeds)
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if ((menuId == GOSSIP_MENU_RPE_GUIDE_ALLIANCE || menuId == GOSSIP_MENU_RPE_GUIDE_HORDE)
            && gossipListId == GOSSIP_OPTION_LEAVE_RPE)
        {
            CloseGossipMenuFor(player);
            SendPlayerHomeFromRpe(player);
            return true;
        }
        return false;
    }
};

void AddSC_arathi_highlands_rpe()
{
    new player_arathi_rpe_mount_credit();
    new player_arathi_rpe_intro_cinematic();
    new playerchoice_arathi_rpe_finale();
    RegisterCreatureAI(npc_arathi_rpe_leader);
}
