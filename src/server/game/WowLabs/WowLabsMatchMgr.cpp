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

#include "WowLabsMatchMgr.h"
#include "Config.h"
#include "LobbyMatchmakerPackets.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include <algorithm>
#include <random>

// Cadence of the out-of-ring storm damage tick - retail ticks every 3 seconds.
static constexpr uint32 STORM_DAMAGE_INTERVAL_MS = 3000;
// Plunder - the Plunderstorm match currency (CurrencyTypes.db2 id 2922).
static constexpr uint32 PLUNDER_CURRENCY = 2922;
// Retail: players level from 1 to 10 during a match.
static constexpr uint8 MAX_MATCH_LEVEL = 10;

// Apply the max-health bonus for a player's current match level, capturing their base health once so it can be
// restored when the match ends. Each level adds WowLabs.HealthPercentPerLevel of the base pool; the health
// gained is added to current health so a level-up makes the player tankier mid-fight (retail behaviour).
static void ApplyMatchLevelHealth(Player* player, WowLabsMatchMgr::Match* match, uint8 level)
{
    uint64 const key = player->GetGUID().GetCounter();
    uint64& base = match->BaseMaxHealth[key];
    if (!base)
        base = player->GetMaxHealth();

    uint32 const pct = sConfigMgr->GetIntDefault("WowLabs.HealthPercentPerLevel", 20);
    uint64 const newMax = base + base * uint64(level - 1) * pct / 100;
    uint64 const oldMax = player->GetMaxHealth();
    if (newMax == oldMax)
        return;

    player->SetMaxHealth(newMax);
    player->SetHealth(std::min(newMax, player->GetHealth() + (newMax > oldMax ? newMax - oldMax : 0)));
}

bool WowLabsMatchMgr::Match::HasMember(ObjectGuid bnet) const
{
    for (MatchMember const& m : Members)
        if (m.BnetAccountGuid == bnet)
            return true;
    return false;
}

WowLabsMatchMgr* WowLabsMatchMgr::instance()
{
    static WowLabsMatchMgr instance;
    return &instance;
}

uint32 WowLabsMatchMgr::GenerateInstanceId()
{
    return _nextInstanceId++;
}

uint32 WowLabsMatchMgr::GenerateToken()
{
    // A non-zero opaque handle the client echoes back; only needs to be unpredictable within a run.
    static std::mt19937 rng{ std::random_device{}() };
    uint32 token = 0;
    do
        token = rng();
    while (token == 0);
    return token;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::CreateMatch(std::vector<MatchMember> members, uint8 gameMode)
{
    uint64 const id = _nextMatchId++;
    Match& match = _matches[id];
    match.Id = id;
    match.InstanceId = GenerateInstanceId();
    match.Token = GenerateToken();
    match.GameMode = gameMode;
    match.Members = std::move(members);

    TC_LOG_DEBUG("network", "WowLabs: reserved match {} (instance {}, token {}, mode {}) for {} players.",
        id, match.InstanceId, match.Token, uint32(gameMode), match.Members.size());
    return &match;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByToken(uint32 token)
{
    if (!token)
        return nullptr;
    for (auto& kv : _matches)
        if (kv.second.Token == token)
            return &kv.second;
    return nullptr;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByInstanceId(uint32 instanceId)
{
    if (!instanceId)
        return nullptr;
    for (auto& kv : _matches)
        if (kv.second.InstanceId == instanceId)
            return &kv.second;
    return nullptr;
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::FindByMember(ObjectGuid bnet)
{
    for (auto& kv : _matches)
        if (kv.second.HasMember(bnet))
            return &kv.second;
    return nullptr;
}

void WowLabsMatchMgr::RemoveMatch(uint64 matchId)
{
    _matches.erase(matchId);
}

WowLabsMatchMgr::Match* WowLabsMatchMgr::GetNewestJoinableMatch()
{
    Match* newest = nullptr;
    for (auto& kv : _matches)
    {
        Match& m = kv.second;
        if (m.MatchPhase == Phase::Ended)
            continue;
        if (!newest || m.Id > newest->Id)
            newest = &m;
    }
    return newest;
}

void WowLabsMatchMgr::AddMemberToMatch(Match* match, ObjectGuid bnet, std::string const& name)
{
    if (!match || match->HasMember(bnet))
        return;
    match->Members.push_back({ bnet, name });
}

uint32 WowLabsMatchMgr::OwnRealmAddress() const
{
    return GetVirtualRealmAddress();
}

uint32 WowLabsMatchMgr::WireState(Phase phase)
{
    // Only Prematch is verified against the client (state == 3). The rest are provisional - they only ever
    // reach the client under GameRule::CharacterlessLogin, which a normal realm never sets.
    switch (phase)
    {
        case Phase::Prematch: return 3;   // verified
        case Phase::Active:   return 4;   // provisional
        case Phase::Ended:    return 5;   // provisional
        case Phase::Reserved:
        default:              return 0;
    }
}

std::vector<WowLabsMatchMgr::DropZone> const& WowLabsMatchMgr::GetDropZones(Match const* /*match*/) const
{
    // Hand-authored drop zones for MAP_WOWLABS. The real set lives in encrypted WoW Labs DB2s and is not
    // offline-obtainable; these are provisional coordinates purely so the area-selection flow has real,
    // validatable options. One set is shared by every match for now.
    static std::vector<DropZone> const zones =
    {
        { 1, 0,  1000.0f,  1000.0f, 200.0f },
        { 2, 0,  1000.0f, -1000.0f, 200.0f },
        { 3, 0, -1000.0f,  1000.0f, 200.0f },
        { 4, 0, -1000.0f, -1000.0f, 200.0f },
        { 5, 0,     0.0f,     0.0f, 250.0f },
        { 6, 0,  1500.0f,     0.0f, 200.0f },
        { 7, 0, -1500.0f,     0.0f, 200.0f },
        { 8, 0,     0.0f,  1500.0f, 200.0f },
    };
    return zones;
}

bool WowLabsMatchMgr::SelectArea(Match* match, ObjectGuid bnet, uint32 areaId)
{
    if (!match || !areaId)
        return false;

    std::vector<DropZone> const& zones = GetDropZones(match);
    bool const valid = std::any_of(zones.begin(), zones.end(), [areaId](DropZone const& z) { return z.Id == areaId; });
    if (!valid)
        return false;

    match->SelectedArea[bnet.GetCounter()] = areaId;
    return true;
}

uint32 WowLabsMatchMgr::GetSelectedArea(Match const* match, ObjectGuid bnet) const
{
    if (!match)
        return 0;
    auto itr = match->SelectedArea.find(bnet.GetCounter());
    return itr != match->SelectedArea.end() ? itr->second : 0;
}

// Send SMSG..MATCH_STATE_CHANGED for a phase to every player currently in a match instance. (Pre-login
// characterless sessions are not on the map; reaching them is a live-client concern the offline build cannot
// exercise.)
static void SendMatchStateToInstance(Map* map, WowLabsMatchMgr::Phase phase)
{
    if (!map)
        return;

    WorldPackets::WowLabs::WowLabsNotifyPlayersMatchStateChanged packet;
    packet.State = WowLabsMatchMgr::WireState(phase);
    WorldPacket const* built = packet.Write();

    for (MapReference const& ref : map->GetPlayers())
        if (Player* player = ref.GetSource())
            player->SendDirectMessage(built);
}

void WowLabsMatchMgr::BeginPrematch(Match* match)
{
    if (!match)
        return;

    bool const firstEntry = match->MatchPhase != Phase::Prematch;
    if (match->MatchPhase == Phase::Reserved)
        match->MatchPhase = Phase::Prematch;

    // Broadcast on every entry (not only the first) so a player arriving into an already-running pre-match is
    // told the state too; re-sending the same phase to players already there is harmless. Do not down-shift a
    // match that is already Active back to Prematch - a late arrival just gets the current phase.
    SendMatchStateToInstance(sMapMgr->FindMap(MAP_ID, match->InstanceId), match->MatchPhase);

    if (firstEntry)
        TC_LOG_DEBUG("network", "WowLabs: match {} (instance {}) entered pre-match.", match->Id, match->InstanceId);
}

std::vector<WowLabsMatchMgr::CirclePhase> const& WowLabsMatchMgr::GetCircleSchedule() const
{
    // Hand-authored storm schedule (real timings/coords are encrypted DB2). Centred on the map origin, matching
    // the provisional drop zones (±1000..1500). Radii in yards; times in ms.
    static std::vector<CirclePhase> const schedule =
    {
        { 30000u, 60000u, 2000.0f, 1200.0f, 0.0f, 0.0f },   // hold 30s @2000, shrink to 1200 over 60s
        { 15000u, 45000u, 1200.0f,  600.0f, 0.0f, 0.0f },
        { 15000u, 30000u,  600.0f,  200.0f, 0.0f, 0.0f },
        { 10000u, 20000u,  200.0f,   50.0f, 0.0f, 0.0f },
    };
    return schedule;
}

bool WowLabsMatchMgr::ComputeCircle(Match const* match, float& centerX, float& centerY, float& radius) const
{
    if (!match || match->MatchPhase != Phase::Active)
        return false;

    std::vector<CirclePhase> const& schedule = GetCircleSchedule();
    if (schedule.empty())
        return false;

    uint32 t = match->ActiveElapsedMs;
    for (CirclePhase const& p : schedule)
    {
        centerX = p.CenterX;
        centerY = p.CenterY;
        if (t < p.HoldMs)                       // holding at the phase's start radius
        {
            radius = p.FromRadius;
            return true;
        }
        t -= p.HoldMs;
        if (t < p.ShrinkMs)                      // shrinking toward the phase's end radius
        {
            float const f = p.ShrinkMs ? float(t) / float(p.ShrinkMs) : 1.0f;
            radius = p.FromRadius + (p.ToRadius - p.FromRadius) * f;
            return true;
        }
        t -= p.ShrinkMs;                         // this phase is done, fall through to the next
    }

    // Past the last phase: settle at the final radius / centre.
    CirclePhase const& last = schedule.back();
    centerX = last.CenterX;
    centerY = last.CenterY;
    radius = last.ToRadius;
    return true;
}

void WowLabsMatchMgr::UpdateInstance(Map* map, uint32 diff)
{
    if (!map)
        return;

    Match* match = FindByInstanceId(map->GetInstanceId());
    if (!match)
        return;

    if (match->MatchPhase == Phase::Prematch)
    {
        match->PrematchElapsedMs += diff;
        uint32 const prematchMs = sConfigMgr->GetIntDefault("WowLabs.PrematchSeconds", 30) * IN_MILLISECONDS;
        if (match->PrematchElapsedMs >= prematchMs)
        {
            match->MatchPhase = Phase::Active;
            match->ActiveElapsedMs = 0;
            match->DamageAccumMs = 0;
            SendMatchStateToInstance(map, Phase::Active);
            TC_LOG_DEBUG("network", "WowLabs: match {} (instance {}) is now active.", match->Id, match->InstanceId);
        }
        return;
    }

    if (match->MatchPhase != Phase::Active)
        return;

    match->ActiveElapsedMs += diff;

    // Storm circle: when the schedule advances to a new phase, push the new current+predicted ring to clients
    // (SET_PREDICTION_CIRCLE). Wire recovered from the client (see the packet class); the client interpolates
    // between the two rings locally, so one message per phase boundary is enough.
    {
        std::vector<CirclePhase> const& schedule = GetCircleSchedule();
        uint32 t = match->ActiveElapsedMs;
        int32 phaseIdx = int32(schedule.size()) - 1;
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            uint32 const span = schedule[i].HoldMs + schedule[i].ShrinkMs;
            if (t < span) { phaseIdx = int32(i); break; }
            t -= span;
        }
        if (phaseIdx != match->SentCirclePhase && phaseIdx >= 0)
        {
            match->SentCirclePhase = phaseIdx;
            CirclePhase const& p = schedule[phaseIdx];

            WorldPackets::WowLabs::WowLabsSetPredictionCircle packet;
            packet.CircleGuid = ObjectGuid::Create<HighGuid::AreaTrigger>(MAP_ID, 0, match->InstanceId);
            packet.CenterCurrentX = packet.CenterNextX = p.CenterX;
            packet.CenterCurrentY = packet.CenterNextY = p.CenterY;
            packet.RadiusCurrent = p.FromRadius;   // the ring now
            packet.RadiusNext = p.ToRadius;        // where it is shrinking to
            WorldPacket const* built = packet.Write();
            for (MapReference const& ref : map->GetPlayers())
                if (Player* player = ref.GetSource())
                    player->SendDirectMessage(built);
        }
    }

    // Win condition: count the living, record death order, and end when a winner emerges. A multi-player match
    // ends the moment one player is left (last one standing); a match ends at zero alive too (e.g. a solo run
    // where the storm takes the lone player) - but a still-living solo player does NOT instantly "win".
    uint32 aliveCount = 0;
    Player* lastAlive = nullptr;
    for (MapReference const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || player->IsGameMaster())
            continue;
        if (player->IsAlive())
        {
            ++aliveCount;
            lastAlive = player;
        }
        else if (std::find(match->FinishOrder.begin(), match->FinishOrder.end(), player->GetGUID()) == match->FinishOrder.end())
            match->FinishOrder.push_back(player->GetGUID());   // just died -> next-lowest placement
    }
    match->PeakPlayers = std::max(match->PeakPlayers, aliveCount);

    if ((match->PeakPlayers >= 2 && aliveCount <= 1) || aliveCount == 0)
    {
        EndMatch(map, match, lastAlive ? lastAlive->GetGUID() : ObjectGuid::Empty);
        return;
    }

    // Out-of-ring storm damage. Retail value (researched): 12% of the player's MAX health every 3 seconds -
    // percentage-based, not a flat amount, so it scales with a levelled player's larger health pool.
    match->DamageAccumMs += diff;
    if (match->DamageAccumMs < STORM_DAMAGE_INTERVAL_MS)
        return;
    match->DamageAccumMs -= STORM_DAMAGE_INTERVAL_MS;

    float cx, cy, radius;
    if (!ComputeCircle(match, cx, cy, radius))
        return;

    uint32 const pct = sConfigMgr->GetIntDefault("WowLabs.StormDamagePercent", 12);
    if (!pct)
        return;

    for (MapReference const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsAlive() || player->IsGameMaster())
            continue;
        if (player->GetDistance2d(cx, cy) > radius)
        {
            uint32 const damage = uint32(player->GetMaxHealth() * pct / 100);
            if (damage)
                player->EnvironmentalDamage(DAMAGE_FIRE, damage);
        }
    }
}

void WowLabsMatchMgr::OnPlayerKill(Player* killer, Player* killed)
{
    if (!killer || !killed || killer == killed)
        return;

    uint32 const instanceId = killer->GetWowLabsInstanceId();
    if (!instanceId || killed->GetWowLabsInstanceId() != instanceId)
        return;   // not the same WoW Labs match (or not in one)

    Match* match = FindByInstanceId(instanceId);
    if (!match || match->MatchPhase != Phase::Active)
        return;

    uint64 const key = killer->GetGUID().GetCounter();

    // Retail: Plunder is collected during the match but only *counts once you finish* (win or death), so the
    // kill bounty is tracked here and banked to the account currency in EndMatch, not granted immediately.
    uint32 const bounty = sConfigMgr->GetIntDefault("WowLabs.PlunderPerKill", 100);
    match->Kills[key] += 1;
    match->PlunderEarned[key] += bounty;

    // XP -> level (1..10). The retail XP curve is server-internal; use a flat per-level threshold (config).
    uint32 const xpPerLevel = std::max(1, sConfigMgr->GetIntDefault("WowLabs.XpPerLevel", 200));
    match->Xp[key] += sConfigMgr->GetIntDefault("WowLabs.XpPerKill", 100);
    uint8 const newLevel = uint8(std::min<uint32>(MAX_MATCH_LEVEL, 1 + match->Xp[key] / xpPerLevel));
    uint8& level = match->Level[key];
    if (newLevel > level)
    {
        level = newLevel;
        ApplyMatchLevelHealth(killer, match, level);
    }

    TC_LOG_DEBUG("network", "WowLabs: match {} - {} killed {} (+{} Plunder banked, level {}).",
        match->Id, killer->GetName(), killed->GetName(), bounty, level);
}

void WowLabsMatchMgr::EndMatch(Map* map, Match* match, ObjectGuid winner)
{
    if (!match || match->MatchPhase == Phase::Ended)
        return;

    match->MatchPhase = Phase::Ended;

    // Placement: the winner is 1st; the death order fills the rest from the bottom up (first to die = last
    // place). total is the field size (winner + everyone who died).
    uint32 const total = uint32(match->FinishOrder.size()) + (winner.IsEmpty() ? 0u : 1u);
    std::unordered_map<uint64, uint32> placement;
    if (!winner.IsEmpty())
        placement[winner.GetCounter()] = 1;
    for (size_t i = 0; i < match->FinishOrder.size(); ++i)
        placement[match->FinishOrder[i].GetCounter()] = total - uint32(i);

    // Retail reward model: every player banks the Plunder they collected this match; ONLY the top placement gets
    // the win bonus (500 Plunder since the 2024-03-21 hotfix, was 100) - it is a flat winner bonus, not a reward
    // scaled per place.
    uint32 const winBonus = sConfigMgr->GetIntDefault("WowLabs.PlunderWinBonus", 500);
    // Default on: the MATCH_END wire is RE-confirmed (GetEndOfMatchDetails reads matchType@+12 int32,
    // matchEnded@+16 bool, detailsList@+24 of 8-byte MatchDetail{int32 type, int32 value}); the JAM bool/array
    // conventions match the sibling area packets. Left as a config only so an operator can silence it.
    bool const sendEnd = sConfigMgr->GetBoolDefault("WowLabs.SendMatchEnd", true);

    // Tell the clients the match is over (single-uint32 phase - the Ended wire value is provisional, like Active).
    SendMatchStateToInstance(map, Phase::Ended);

    for (MapReference const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || player->IsGameMaster())
            continue;

        uint64 const key = player->GetGUID().GetCounter();

        // Undo the in-match level health scaling so nothing leaks back to the open world.
        if (auto baseItr = match->BaseMaxHealth.find(key); baseItr != match->BaseMaxHealth.end() && baseItr->second)
        {
            player->SetMaxHealth(baseItr->second);
            player->SetHealth(std::min<uint64>(player->GetHealth(), baseItr->second));
        }

        auto itr = placement.find(key);
        uint32 const place = itr != placement.end() ? itr->second : std::max(total, 1u);   // stragglers = last
        int32 const collected = int32(match->PlunderEarned.count(key) ? match->PlunderEarned[key] : 0);
        int32 const bonus = (place == 1) ? int32(winBonus) : 0;   // flat winner bonus only
        int32 const plunderAcquired = collected + bonus;
        if (plunderAcquired > 0)
            player->ModifyCurrency(PLUNDER_CURRENCY, plunderAcquired, CurrencyGainSource::PvPScriptedAward);

        // Per-player end-of-match summary: placement, kills scored, and total Plunder acquired this match.
        if (sendEnd)
        {
            uint32 const kills = match->Kills.count(key) ? match->Kills[key] : 0;

            WorldPackets::WowLabs::WowLabsNotifyPlayersMatchEnd packet;
            packet.MatchType = 1;   // Plunderstorm
            packet.MatchEnded = true;
            packet.Details.push_back({ 0 /*Placement*/,       int32(place) });
            packet.Details.push_back({ 1 /*Kills*/,           int32(kills) });
            packet.Details.push_back({ 2 /*PlunderAcquired*/, plunderAcquired });
            player->SendDirectMessage(packet.Write());
        }
    }

    TC_LOG_INFO("network", "WowLabs: match {} (instance {}) ended - {} ranked, winner {}.",
        match->Id, match->InstanceId, total, winner.IsEmpty() ? "none" : winner.ToString());
}
