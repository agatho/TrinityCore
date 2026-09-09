// BotGuildEvent - Phase D event descriptor table.
//
// Each entry describes a recurring guild-wide event (raid night,
// dungeon night, BG night, capital tavern party) with a weekday +
// hour-of-day schedule. The BotGuildEventScheduler ticks every world
// frame, computes whether *any* event is active for *any* bot-managed
// guild, and stamps the active event kind onto a per-guild slot the
// snapshot builder reads. Idle rules consume the active-event flag.
//
// Initial Phase D scope (this PR) ships only TavernParty — the other
// kinds are scaffolded in the enum + table so D.2/D.3 PRs can flip them
// on without re-architecting.
//
// World-thread only.

#pragma once

#include <cstdint>

namespace Playerbot::V2 {

enum class GuildEventKind : uint8_t
{
    None         = 0,
    // Saturday 22:00 server time, 60 min. All online members converge
    // on their faction's primary tavern (SW Pig & Whistle / Org Drag
    // inn) and emit dance / cheer / wave / flex emotes plus light
    // guild-chat smalltalk.
    TavernParty  = 1,
    // Deferred to D.2: members in level bracket walk to raid entrance
    // and queue. Needs content_id lookup + level-bracket eligibility.
    RaidNight    = 2,
    // Deferred to D.2: members in level bracket /lfg into 5-man.
    DungeonNight = 3,
    // Deferred to D.2: members /bg-queue together.
    BgNight      = 4,
};

struct GuildEventDescriptor
{
    GuildEventKind kind;
    // Bitmask of weekdays the event fires on. Bit 0 = Sunday, 1 = Mon,
    // ..., 6 = Sat. Matches localtime's tm_wday.
    uint8          weekday_mask;
    // Server local-time hour (0-23) at which the event starts.
    uint8          start_hour;
    // Duration in minutes. Active-event flag holds from start_hour:00
    // through start_hour + duration_min.
    uint16         duration_min;
    // Pre-event chatter lead time in minutes (officer guild-chat
    // callout "<event> starts in 15 min!"). 0 = no pre-announce.
    uint16         pre_announce_min;
};

inline constexpr GuildEventDescriptor kBotGuildEvents[] =
{
    // Saturday 22:00, 60 min, 15 min pre-announce.
    { GuildEventKind::TavernParty, /*weekday=Sat*/ 1u << 6, 22, 60, 15 },
    // Wednesday + Saturday 20:00, 120 min — raid night. Bots in the
    // level cap bracket converge on a faction-specific raid staging
    // point (Stormwind/Org main square) and broadcast "raid night".
    // D.3 will add the actual instance entry + group formation.
    { GuildEventKind::RaidNight, /*Wed|Sat*/ (1u << 3) | (1u << 6), 20, 120, 15 },
    // Tuesday + Thursday 20:00, 90 min — dungeon night. Same converge
    // pattern; LFG queue integration deferred to D.3.
    { GuildEventKind::DungeonNight, /*Tue|Thu*/ (1u << 2) | (1u << 4), 20, 90, 15 },
    // Friday 20:00, 90 min — BG night. Converge + LFM-style call.
    { GuildEventKind::BgNight, /*Fri*/ 1u << 5, 20, 90, 15 },
};

inline constexpr size_t kBotGuildEventCount =
    sizeof(kBotGuildEvents) / sizeof(*kBotGuildEvents);

// Faction tavern coordinates. Hardcoded to the canonical "home tavern"
// of each capital so the converge target is the same every Sat.
// Stormwind: Pig & Whistle Tavern, Trade District.
// Orgrimmar: Wyvern's Tail Inn (the Drag).
struct TavernSpot
{
    uint32 map_id;
    float  x, y, z;
};

inline constexpr TavernSpot kFactionTavern[2] =
{
    /* FACTION_ALLIANCE (Stormwind Pig & Whistle) */
    { /*map*/ 0, -8503.0f, 423.0f, 103.0f },
    /* FACTION_HORDE    (Orgrimmar Wyvern's Tail)  */
    { /*map*/ 1,  1654.0f, -4439.0f, 16.0f },
};

// Phase D.2 staging points — where bots converge for Raid / Dungeon /
// BG nights. Reuses the tavern spot but in different game time slots
// the rule changes the broadcast message. (Future D.3 will split these
// per-faction per-event-kind: raid entrances per content tier.)
inline constexpr TavernSpot kFactionStaging[2] =
{
    /* FACTION_ALLIANCE (Stormwind Trade District fountain) */
    { /*map*/ 0, -8829.0f, 626.0f, 94.0f },
    /* FACTION_HORDE    (Orgrimmar Valley of Strength)       */
    { /*map*/ 1,  1633.0f, -4382.0f, 16.0f },
};

} // namespace Playerbot::V2
