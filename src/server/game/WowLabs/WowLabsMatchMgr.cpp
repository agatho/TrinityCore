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
#include "Creature.h"
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

// Normalize a player's health to their Plunderstorm match level, capturing their real character health once so
// it can be restored when the match ends. Retail values (Wowhead ability/level-up compendium): every pirate has
// 100 base health at level 1 and gains a flat +16 per level (level 10 = 244), regardless of gear - the mode
// equalizes everyone. Set to full on (re)normalization / level-up.
static void ApplyMatchLevelHealth(Player* player, WowLabsMatchMgr::Match* match, uint8 level)
{
    uint64 const key = player->GetGUID().GetCounter();
    uint64& realBase = match->BaseMaxHealth[key];
    if (!realBase)
        realBase = player->GetMaxHealth();   // the character's real health, restored in EndMatch

    uint32 const base = sConfigMgr->GetIntDefault("WowLabs.BaseHealth", 100);
    uint32 const perLevel = sConfigMgr->GetIntDefault("WowLabs.HealthPerLevel", 16);
    uint64 const newMax = uint64(base) + uint64(perLevel) * (level > 0 ? level - 1u : 0u);
    if (newMax == player->GetMaxHealth())
        return;

    player->SetMaxHealth(newMax);
    player->SetHealth(newMax);
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
    // The real Plunderstorm drop zones: every AreaPOI on map 2695 (Void Zone: Arathi Highlands) in the client's
    // own data, with client coordinates. Id = the AreaPOI id; AreaType 0. One set is shared by every match.
    static std::vector<DropZone> const zones =
    {
        { 7658, 0,  -979.51f, -3527.38f,  57.40f },   // Hammerfall
        { 7659, 0,  -883.26f, -1633.16f,  49.99f },   // Thoradin's Wall
        { 7660, 0,  -636.19f, -1912.89f,  64.06f },   // Clearfell's Patch
        { 7661, 0,  -793.63f, -1777.57f,  59.56f },   // Hatchet Ridge
        { 7662, 0,  -861.15f, -2068.44f,  63.26f },   // Ar'gorok
        { 7663, 0, -1088.74f, -1690.61f,  35.85f },   // Newstead
        { 7664, 0, -1357.86f, -1545.19f,  54.99f },   // Highlands Mill
        { 7665, 0, -1252.05f, -1650.84f,  48.09f },   // Labor's Rest
        { 7666, 0, -1351.97f, -1833.70f,  62.32f },   // Valorcall Pass
        { 7667, 0, -1676.58f, -1737.84f,  81.32f },   // Stromgarde Keep
        { 7668, 0, -1203.47f, -1903.33f,  87.52f },   // High Perch
        { 7669, 0, -1032.32f, -1951.11f,  60.67f },   // Northfold Crossing
        { 7670, 0,  -991.27f, -2256.61f,  13.51f },   // Drywhisker Mine
        { 7671, 0, -1183.44f, -2195.45f,  58.06f },   // Circle of Elements
        { 7672, 0,  -897.14f, -2413.26f,  56.45f },   // Contested Pass
        { 7673, 0, -1320.80f, -2051.44f,  63.49f },   // Marrow's Farm
        { 7674, 0, -1472.89f, -2073.56f,  27.21f },   // Galson's Lode
        { 7675, 0, -2090.02f, -2039.35f,   5.67f },   // Faldir's Cove
        { 7676, 0, -1773.73f, -2451.62f,  57.79f },   // Forsaken Wagon
        { 7677, 0, -1484.74f, -2628.56f,  54.59f },   // The Mangled Chord
        { 7678, 0, -1251.86f, -2521.64f,  21.15f },   // Refuge Pointe
        { 7679, 0, -1349.49f, -2738.33f,  58.92f },   // Circle of Outer Binding
        { 7680, 0, -1090.31f, -2843.17f,  42.22f },   // Dabyrie's Farmstead
        { 7681, 0, -1521.45f, -2949.39f,  13.94f },   // Go'shek Farm
        { 7682, 0, -1954.77f, -2833.32f,  79.72f },   // Boulderfist Hall
        { 7683, 0, -2072.97f, -2454.40f,  76.09f },   // Thandol Span
        { 7684, 0, -1825.41f, -3352.46f,  53.87f },   // Witherbark Village
        { 7685, 0,  -950.22f, -3117.47f,  48.32f },   // The Neglected Seat
        { 7686, 0,  -873.74f, -3284.15f,  75.14f },   // Circle of East Binding
        { 7687, 0, -1018.20f, -3823.99f, 145.26f },   // Drywhisker Gorge
        { 7688, 0, -1527.48f, -2165.09f,  17.37f },   // Circle of Inner Binding
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
    // Storm schedule centred on the real map: the "Circle of Inner Binding" AreaPOI (id 7688) is the final-ring
    // marker on map 2695, so the circle collapses toward it. Radii are scaled to Arathi Highlands (the void-zone
    // POIs span ~1500 x ~2300 yards). Phase timings are hand-authored (the retail curve is server-internal).
    static constexpr float CENTER_X = -1527.48f;   // Circle of Inner Binding
    static constexpr float CENTER_Y = -2165.09f;
    static std::vector<CirclePhase> const schedule =
    {
        { 30000u, 60000u, 1800.0f, 1000.0f, CENTER_X, CENTER_Y },   // hold 30s @1800, shrink to 1000 over 60s
        { 15000u, 45000u, 1000.0f,  500.0f, CENTER_X, CENTER_Y },
        { 15000u, 30000u,  500.0f,  200.0f, CENTER_X, CENTER_Y },
        { 10000u, 20000u,  200.0f,   40.0f, CENTER_X, CENTER_Y },
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

std::vector<WowLabsMatchMgr::AbilityDef> const& WowLabsMatchMgr::GetAbilityPool() const
{
    // The real Plunderstorm ability pool (Season 1: 10 offensive + 10 utility). Each ability's four rank spells
    // were recovered from the client's own data: the player-castable ability spells are tagged
    // Spell.NameSubtext = "Offensive"/"Utility", and their four rarity ranks (Common..Epic) are the variants with
    // descending cooldowns. These spell ids are present in this client's spell store (verified on 12.1.5.69594),
    // so they are genuinely learnable and castable here - not placeholders.
    static std::vector<AbilityDef> const pool =
    {
        {  1, "Earthbreaker",       KIND_OFFENSIVE,  { 435018, 435021, 435023, 435025 } },
        {  2, "Fire Whirl",         KIND_OFFENSIVE,  { 431777, 432746, 432748, 432750 } },
        {  3, "Holy Shield",        KIND_OFFENSIVE,  { 433380, 433472, 433473, 433474 } },
        {  4, "Rime Arrow",         KIND_OFFENSIVE,  { 435276, 435290, 435288, 435295 } },
        {  5, "Storm Archon",       KIND_OFFENSIVE,  { 442445, 442819, 442840, 442862 } },
        {  6, "Mana Sphere",        KIND_OFFENSIVE,  { 431501, 432730, 432737, 432744 } },
        {  7, "Searing Axe",        KIND_OFFENSIVE,  { 432490, 432752, 432754, 432756 } },
        {  8, "Slicing Winds",      KIND_OFFENSIVE,  { 433082, 433184, 433185, 433186 } },
        {  9, "Star Bomb",          KIND_OFFENSIVE,  { 434880, 435030, 435032, 435034 } },
        { 10, "Toxic Smackerel",    KIND_OFFENSIVE,  { 436254, 436310, 436308, 436312 } },
        { 11, "Steel Traps",        KIND_UTILITY,    { 434598, 434679, 434681, 434684 } },
        { 12, "Windstorm",          KIND_UTILITY,    { 433263, 433256, 433264, 433265 } },
        { 13, "Explosive Caltrops", KIND_UTILITY,    { 432541, 432759, 432762, 432764 } },
        { 14, "Hunter's Chains",    KIND_UTILITY,    { 436031, 436211, 436229, 436240 } },
        { 15, "Lightning Bulwark",  KIND_UTILITY,    { 442371, 442404, 442407, 442411 } },
        { 16, "Snowdrift",          KIND_UTILITY,    { 433364, 433373, 433374, 433375 } },
        { 17, "Fade to Shadow",     KIND_UTILITY,    { 432547, 432765, 432766, 432767 } },
        { 18, "Repel",              KIND_UTILITY,    { 435286, 435292, 435296, 435298 } },
        { 19, "Faeform",            KIND_UTILITY,    { 432594, 432768, 432769, 432770 } },
        { 20, "Quaking Leap",       KIND_UTILITY,    { 435454, 435510, 435729, 435757 } },
        // Newer-season abilities (Wowhead ability compendium), same recovery method (subtext + cooldown ranks).
        { 21, "Celestial Barrage",  KIND_OFFENSIVE,  { 471717, 472390, 472403, 472406 } },
        { 22, "Aura of Zealotry",   KIND_OFFENSIVE,  { 473810, 473819, 473823, 473828 } },
        { 23, "Call Galefeather",   KIND_UTILITY,    { 474121, 474650, 474651, 474652 } },
        { 24, "G.R.A.V. Glove",     KIND_UTILITY,    { 472908, 472908, 472908, 472908 } },   // only 1 castable rank found
        // Consumable-slot items (single use, one "rank"): the found-on-the-ground throwables/shields.
        { 25, "Stormproof Sloop",   KIND_CONSUMABLE, { 438619, 438619, 438619, 438619 } },
        { 26, "Rigged Chest",       KIND_CONSUMABLE, { 437334, 437334, 437334, 437334 } },
        { 27, "Chicken Coup",       KIND_CONSUMABLE, { 437263, 437263, 437263, 437263 } },
    };
    return pool;
}

WowLabsMatchMgr::AbilityDef const* WowLabsMatchMgr::FindAbility(uint32 abilityId) const
{
    for (AbilityDef const& a : GetAbilityPool())
        if (a.Id == abilityId)
            return &a;
    return nullptr;
}

uint8 WowLabsMatchMgr::GrantAbility(Player* player, Match* match, uint32 abilityId)
{
    if (!player || !match)
        return 0;
    AbilityDef const* def = FindAbility(abilityId);
    if (!def)
        return 0;

    uint64 const key = player->GetGUID().GetCounter();
    std::vector<HeldAbility>& held = match->Abilities[key];

    // Already held -> stack up one rank (capped at Epic).
    for (HeldAbility& h : held)
    {
        if (h.AbilityId != abilityId)
            continue;
        if (h.Rank >= ABILITY_RANKS)
            return h.Rank;                                   // already maxed
        player->RemoveSpell(def->RankSpell[h.Rank - 1]);
        ++h.Rank;
        uint32 const spell = def->RankSpell[h.Rank - 1];
        player->LearnSpell(spell, false);
        player->AddActionButton(h.ActionSlot, spell, ACTION_BUTTON_SPELL);
        return h.Rank;
    }

    // New pickup -> place on a free slot of the ability's kind. Retail action bar: 2 offensive slots (0/1),
    // 2 utility slots (2/3), 1 consumable slot (4).
    uint8 base = 0, slotCount = 2;
    switch (def->Kind)
    {
        case KIND_UTILITY:    base = 2; slotCount = 2; break;
        case KIND_CONSUMABLE: base = 4; slotCount = 1; break;
        case KIND_OFFENSIVE:
        default:              base = 0; slotCount = 2; break;
    }

    uint8 slot = 255;
    for (uint8 i = 0; i < slotCount; ++i)
    {
        uint8 const candidate = base + i;
        if (std::none_of(held.begin(), held.end(), [candidate](HeldAbility const& h) { return h.ActionSlot == candidate; }))
        {
            slot = candidate;
            break;
        }
    }

    if (slot == 255)
    {
        // Every slot of this kind is full: drop the one in the base slot to make room.
        for (auto it = held.begin(); it != held.end(); ++it)
        {
            if (it->ActionSlot != base)
                continue;
            if (AbilityDef const* old = FindAbility(it->AbilityId))
                player->RemoveSpell(old->RankSpell[it->Rank - 1]);
            held.erase(it);
            break;
        }
        slot = base;
    }

    uint32 const spell = def->RankSpell[0];
    player->LearnSpell(spell, false);
    player->AddActionButton(slot, spell, ACTION_BUTTON_SPELL);
    held.push_back({ abilityId, 1, slot });
    return 1;
}

void WowLabsMatchMgr::GrantRandomAbility(Player* player, Match* match)
{
    std::vector<AbilityDef> const& pool = GetAbilityPool();
    if (pool.empty())
        return;
    static std::mt19937 rng{ std::random_device{}() };
    GrantAbility(player, match, pool[rng() % pool.size()].Id);
}

void WowLabsMatchMgr::ClearMatchAbilities(Player* player, Match* match)
{
    if (!player || !match)
        return;
    auto it = match->Abilities.find(player->GetGUID().GetCounter());
    if (it == match->Abilities.end())
        return;
    for (HeldAbility const& h : it->second)
    {
        if (AbilityDef const* def = FindAbility(h.AbilityId))
            player->RemoveSpell(def->RankSpell[h.Rank - 1]);
        player->RemoveActionButton(h.ActionSlot);
    }
    match->Abilities.erase(it);
}

void WowLabsMatchMgr::OnPlayerEnterMatch(Player* player, Match* match)
{
    if (!player || !match)
        return;
    uint64 const key = player->GetGUID().GetCounter();
    uint8 const level = match->Level.count(key) ? match->Level[key] : uint8(1);
    ApplyMatchLevelHealth(player, match, level ? level : uint8(1));
}

void WowLabsMatchMgr::OnCreatureKill(Player* killer, Creature* killed)
{
    if (!killer || !killed)
        return;

    uint32 const instanceId = killer->GetWowLabsInstanceId();
    if (!instanceId)
        return;
    Match* match = FindByInstanceId(instanceId);
    if (!match || match->MatchPhase != Phase::Active)
        return;

    uint64 const key = killer->GetGUID().GetCounter();
    bool const elite = killed->IsElite();

    // Plunder banked at match end; elites are worth much more (retail).
    match->PlunderEarned[key] += sConfigMgr->GetIntDefault(elite ? "WowLabs.PlunderPerElite" : "WowLabs.PlunderPerMob", elite ? 50 : 10);

    // XP -> level, same curve as player kills.
    uint32 const xpPerLevel = std::max(1, sConfigMgr->GetIntDefault("WowLabs.XpPerLevel", 200));
    match->Xp[key] += sConfigMgr->GetIntDefault(elite ? "WowLabs.XpPerElite" : "WowLabs.XpPerMob", elite ? 100 : 25);
    uint8 const newLevel = uint8(std::min<uint32>(MAX_MATCH_LEVEL, 1 + match->Xp[key] / xpPerLevel));
    uint8& level = match->Level[key];
    if (newLevel > level)
    {
        level = newLevel;
        ApplyMatchLevelHealth(killer, match, level);
    }

    // Retail: elites guarantee a spell drop.
    if (elite && sConfigMgr->GetBoolDefault("WowLabs.AbilityDropOnKill", true))
        GrantRandomAbility(killer, match);
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

    // Retail: a kill drops an ability (guaranteed on the first kill / from elites). Auto-pick it onto the killer.
    if (sConfigMgr->GetBoolDefault("WowLabs.AbilityDropOnKill", true))
        GrantRandomAbility(killer, match);

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

        // Abilities reset between matches - strip everything picked up this match.
        ClearMatchAbilities(player, match);

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
