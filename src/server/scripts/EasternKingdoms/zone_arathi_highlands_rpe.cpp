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
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "QuestDef.h"
#include "SceneMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"

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
    CINEMATIC_ARATHI_RPE_INTRO       = 77,

    // Ambient pad SCENES (verified on the wire, SMSG_PLAY_SCENE 0x4500DF at the arrival pad in both
    // captures). Scene 3749 "Jaina stasis presentation" is the persistent flying-gnolls set-piece;
    // 3692 is the general pad ambience. They are played FOR THE PLAYER (SceneMgr) on entry - the
    // SPELL_AURA_PLAY_SCENE handler only fires when the aura target is a player, so anchoring the
    // scene auras on Jaina (a creature) did nothing; the server plays them on the arriving player.
    SCENE_ARATHI_RPE_PAD_AMBIENT     = 3692,
    SCENE_ARATHI_RPE_JAINA_GNOLLS    = 3749,

    // "Gnoll Way" (90882) turn-in send-off. On reward the two pad leaders remark on the farm raid and
    // walk out toward Go'shek Farm along the paths our capture recorded them taking (OOC one-shot
    // WP_START, 69404). Text and paths are authored in SQL: creature_text group 0 on each leader, and
    // waypoint_path 2218835 (Jaina) / 2218842 (Thrall). Personal-phasing (phase 1961, "until 90883
    // rewarded") clears the pad leaders once the player hands the follow-up flight quest in at the farm,
    // so no explicit despawn is needed - the walk is the visible flourish before that.
    SAY_RPE_LEADER_SENDOFF           = 0,
    PATH_RPE_JAINA_PAD_SENDOFF       = 2218835,
    PATH_RPE_THRALL_PAD_SENDOFF      = 2218842,

    // "My Beautiful Pumpkins" (90885) escort. WIRE-CONFIRMED from the Horde capture (69404): the
    // player spellclicks each Prized Pumpkin (244956) -- CMSG_SPELLCLICK x4, matching the 4-pumpkin
    // objective 461736 -- whereupon the pumpkin casts spell 1236771 (its own launch/fly-to-peon visual;
    // creature_template_spell 244956->1236771) and the Hammerfall Peon (249249) casts spell 382691
    // (its carry visual; creature_template_spell 249249->382691), the peon following the player and
    // ending the quest visibly carrying 4 pumpkins. Farmer Bruvk (244729) starts the quest. Implemented
    // as a PERSONAL peon summon (private to the accepting player) that follows, accrues a carry stack
    // per click, and self-despawns when the quest leaves the "in progress / ready to turn in" states.
    QUEST_MY_BEAUTIFUL_PUMPKINS      = 90885,
    NPC_ARATHI_RPE_FARMER_BRUVK      = 244729,   // 90885 quest starter
    NPC_ARATHI_RPE_PRIZED_PUMPKIN    = 244956,   // spellclick target (x4)
    NPC_ARATHI_RPE_PUMPKIN_PEON      = 249249,   // the carrying peon (summoned, follows player)
    SPELL_ARATHI_RPE_PUMPKIN_LAUNCH  = 1236771,  // pumpkin's own on-click visual (wire: 244956 casts this)
    SPELL_ARATHI_RPE_PEON_CARRY      = 382691,    // peon carry visual, stacked to the pumpkins carried
    ARATHI_RPE_PUMPKINS_NEEDED       = 4
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
            Player* p = ObjectAccessor::FindPlayer(guid);
            if (!p || p->GetMapId() != MAP_ARATHI_RPE || p->GetQuestRewardStatus(QUEST_ARATHI_RPE_FINALE))
                return;

            // Persistent pad set-dressing: the flying-gnolls scene (3749) + ambient (3692). These
            // must be played FOR THE PLAYER (SceneMgr), not as an aura on Jaina. They keep playing
            // after the intro cinematic (that is what the tester sees "persist"), and the cinematic
            // camera films them.
            p->GetSceneMgr().PlayScene(SCENE_ARATHI_RPE_PAD_AMBIENT);
            p->GetSceneMgr().PlayScene(SCENE_ARATHI_RPE_JAINA_GNOLLS);
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

    // One pad leader's send-off beat: say the captured "head to the farm" line and walk out along the
    // captured path. Static so the clicked leader can drive its sibling too (see OnQuestReward).
    static void StartPadLeaderSendoff(Creature* leader)
    {
        if (CreatureAI* ai = leader->AI())
            ai->Talk(SAY_RPE_LEADER_SENDOFF);

        uint32 pathId = leader->GetEntry() == NPC_RPE_THRALL_PAD ? uint32(PATH_RPE_THRALL_PAD_SENDOFF)
                      : leader->GetEntry() == NPC_RPE_JAINA_PAD  ? uint32(PATH_RPE_JAINA_PAD_SENDOFF)
                      : 0u;
        if (pathId)
            leader->GetMotionMaster()->MovePath(pathId, false);
    }

    void OnQuestReward(Player* /*player*/, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_GNOLL_WAY)
            return;

        // The pad leaders stand side by side as an allied pair (both visible to both factions), but
        // OnQuestReward fires only on the leader the player actually handed the quest to. Drive both so
        // Jaina and Thrall react together - each says its own creature_text group 0 and takes its own path.
        StartPadLeaderSendoff(me);

        uint32 siblingEntry = me->GetEntry() == NPC_RPE_THRALL_PAD ? uint32(NPC_RPE_JAINA_PAD) : uint32(NPC_RPE_THRALL_PAD);
        if (Creature* sibling = me->FindNearestCreature(siblingEntry, 60.0f))
            StartPadLeaderSendoff(sibling);
    }
};

// "My Beautiful Pumpkins" (90885) -- the carrying peon. Summoned personally for the player who accepts
// the quest (see npc_arathi_rpe_farmer_bruvk), it follows them, gains one carry-stack of SPELL_ARATHI_RPE
// _PEON_CARRY per pumpkin recovered, and self-despawns once the quest is no longer in progress / ready.
struct npc_arathi_rpe_pumpkin_peon : public ScriptedAI
{
    npc_arathi_rpe_pumpkin_peon(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
        {
            _ownerGuid = player->GetGUID();
            me->SetReactState(REACT_PASSIVE);
            me->SetImmuneToAll(true);            // an escort prop, never a combatant
            me->GetMotionMaster()->MoveFollow(player, 2.0f, float(M_PI));  // trail just behind the player
        }
    }

    // Called by the pumpkin's OnSpellClick: accrue one carried pumpkin (capped at the objective count)
    // and refresh the carry visual to that stack count.
    void AddPumpkin()
    {
        if (_pumpkins >= ARATHI_RPE_PUMPKINS_NEEDED)
            return;
        ++_pumpkins;
        if (sSpellMgr->GetSpellInfo(SPELL_ARATHI_RPE_PEON_CARRY, DIFFICULTY_NONE))
        {
            me->CastSpell(me, SPELL_ARATHI_RPE_PEON_CARRY, true);   // wire: peon casts 382691 on pickup
            me->SetAuraStack(SPELL_ARATHI_RPE_PEON_CARRY, me, _pumpkins);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        // Entry 249249 is ALSO used for 6 ambient farm peons (static world spawns). This AI is on the
        // template, so those get it too -- but they are never IsSummonedBy'd. Only the personal quest
        // summon runs the escort/despawn logic; the ambient peons fall through and behave inertly.
        if (!me->IsSummon())
            return;

        // Self-despawn watchdog: the peon is a personal escort for the pumpkin quest only. Once the
        // owner is gone, or the quest is no longer in progress (INCOMPLETE) or ready to hand in
        // (COMPLETE) -- i.e. rewarded or abandoned -- it has served its purpose. Poll cheaply (~1s).
        _checkTimer += diff;
        if (_checkTimer < 1000)
            return;
        _checkTimer = 0;

        Player* owner = ObjectAccessor::FindPlayer(_ownerGuid);
        if (!owner)
        {
            me->DespawnOrUnsummon();
            return;
        }
        QuestStatus status = owner->GetQuestStatus(QUEST_MY_BEAUTIFUL_PUMPKINS);
        if (status != QUEST_STATUS_INCOMPLETE && status != QUEST_STATUS_COMPLETE)
            me->DespawnOrUnsummon(3s);   // brief beat so it lingers, carrying its 4 pumpkins, after turn-in
    }

    ObjectGuid GetOwnerGuid() const { return _ownerGuid; }

private:
    ObjectGuid _ownerGuid;
    uint32 _pumpkins = 0;
    uint32 _checkTimer = 0;
};

// Prized Pumpkin (244956): spellclick target. On click by a player on 90885, hand the pumpkin to that
// player's peon (carry stack + credit) and consume it. The pumpkin's own launch visual (1236771) is
// fired by the npc_spellclick_spells row (see the SQL), matching the wire; this AI does the rest.
struct npc_arathi_rpe_prized_pumpkin : public ScriptedAI
{
    npc_arathi_rpe_prized_pumpkin(Creature* creature) : ScriptedAI(creature) { }

    void OnSpellClick(Unit* clicker, bool /*spellClickHandled*/) override
    {
        Player* player = clicker ? clicker->ToPlayer() : nullptr;
        if (!player || player->GetQuestStatus(QUEST_MY_BEAUTIFUL_PUMPKINS) != QUEST_STATUS_INCOMPLETE)
            return;

        // Route the pumpkin to the clicking player's own peon (personal summon).
        if (Creature* peon = FindOwnedPeon(player))
            if (auto* peonAI = CAST_AI(npc_arathi_rpe_pumpkin_peon, peon->AI()))
                peonAI->AddPumpkin();

        player->KilledMonsterCredit(NPC_ARATHI_RPE_PRIZED_PUMPKIN);   // objective 461736 (ObjectID 244956)
        me->DespawnOrUnsummon(0s, Seconds(30));                       // consumed; respawns for the next run
    }

private:
    Creature* FindOwnedPeon(Player const* player) const
    {
        std::list<Creature*> peons;
        me->GetCreatureListWithEntryInGrid(peons, NPC_ARATHI_RPE_PUMPKIN_PEON, 100.0f);
        for (Creature* peon : peons)
            if (TempSummon* summon = peon->ToTempSummon())
                if (summon->GetSummonerGUID() == player->GetGUID())
                    return peon;
        return nullptr;
    }
};

// Farmer Bruvk (244729): starter of 90885. On accept, summon the player's personal carrying peon.
struct npc_arathi_rpe_farmer_bruvk : public ScriptedAI
{
    npc_arathi_rpe_farmer_bruvk(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_MY_BEAUTIFUL_PUMPKINS)
            return;

        // Guard against a duplicate peon if the quest is re-accepted before the old one despawns.
        std::list<Creature*> peons;
        me->GetCreatureListWithEntryInGrid(peons, NPC_ARATHI_RPE_PUMPKIN_PEON, 100.0f);
        for (Creature* peon : peons)
            if (TempSummon* summon = peon->ToTempSummon())
                if (summon->GetSummonerGUID() == player->GetGUID())
                    return;

        // Private summon -> only the accepting player sees their own peon (fits the personally-phased RPE).
        player->SummonCreature(NPC_ARATHI_RPE_PUMPKIN_PEON, *player, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID());
    }
};

void AddSC_arathi_highlands_rpe()
{
    new player_arathi_rpe_mount_credit();
    new player_arathi_rpe_intro_cinematic();
    new playerchoice_arathi_rpe_finale();
    RegisterCreatureAI(npc_arathi_rpe_leader);
    RegisterCreatureAI(npc_arathi_rpe_pumpkin_peon);
    RegisterCreatureAI(npc_arathi_rpe_prized_pumpkin);
    RegisterCreatureAI(npc_arathi_rpe_farmer_bruvk);
}
