import sys, os
REPO = sys.argv[1].rstrip("/")
def p(rel): return os.path.join(REPO, rel)
def read(fp): return open(fp, encoding="utf-8", newline="").read()
def patch(rel, edits, append=None):
    fp = p(rel); d = read(fp); nl = "\r\n" if "\r\n" in d else "\n"
    for old, new in edits:
        old = old.replace("\n", nl); new = new.replace("\n", nl)
        assert d.count(old) == 1, f"{rel}: count={d.count(old)} for {old[:55]!r}"
        d = d.replace(old, new, 1)
    if append is not None:
        if not d.endswith(nl): d += nl
        d += append.replace("\n", nl)
    open(fp, "w", encoding="utf-8", newline="").write(d); print("  OK", rel)

QPH="src/server/game/Server/Packets/QuestPackets.h"
QPC="src/server/game/Server/Packets/QuestPackets.cpp"
WSH="src/server/game/Server/WorldSession.h"
QHD="src/server/game/Handlers/QuestHandler.cpp"
OPC="src/server/game/Server/Protocol/Opcodes.cpp"
WPH="src/server/game/Server/Packets/WorldStatePackets.h"
WPC="src/server/game/Server/Packets/WorldStatePackets.cpp"
WMH="src/server/game/World/WorldStates/WorldStateMgr.h"
WMC="src/server/game/World/WorldStates/WorldStateMgr.cpp"
GEH="src/server/game/Events/GameEventMgr.h"
GEC="src/server/game/Events/GameEventMgr.cpp"

if "struct AreaPoiUpdateInfo" in read(p(QPH)):
    print("world-quests slice already present ->", REPO); sys.exit(0)

# A) QuestPackets.h — 4 classes
qp_classes="""        class RequestAreaPoiUpdate final : public ClientPacket
        {
        public:
            explicit RequestAreaPoiUpdate(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_AREA_POI_UPDATE, std::move(packet)) { }

            void Read() override { }
        };

        class RequestScheduledAreaPoiUpdate final : public ClientPacket
        {
        public:
            explicit RequestScheduledAreaPoiUpdate(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_SCHEDULED_AREA_POI_UPDATE, std::move(packet)) { }

            void Read() override { }
        };

        struct AreaPoiUpdateInfo
        {
            AreaPoiUpdateInfo(time_t lastUpdate, uint32 areaPoiID, uint32 timer, int32 variableID, int32 value) :
                LastUpdate(lastUpdate), AreaPoiID(areaPoiID), Timer(timer), VariableID(variableID), Value(value) { }
            Timestamp<> LastUpdate;
            uint32 AreaPoiID;
            uint32 Timer;
            // WorldState
            int32 VariableID;
            int32 Value;
        };

        class AreaPoiUpdateResponse final : public ServerPacket
        {
        public:
            explicit AreaPoiUpdateResponse() : ServerPacket(SMSG_AREA_POI_UPDATE_RESPONSE, 100) { }

            WorldPacket const* Write() override;

            std::vector<AreaPoiUpdateInfo> AreaPois;
        };

"""
patch(QPH, [("        struct PlayerChoiceResponseRewardEntry", qp_classes + "        struct PlayerChoiceResponseRewardEntry")])

# B) QuestPackets.cpp — Write (after ForceSpawnTrackingUpdate, before namespace close)
qp_write="""
WorldPacket const* AreaPoiUpdateResponse::Write()
{
    _worldPacket << Size<uint32>(AreaPois);

    for (AreaPoiUpdateInfo const& areaPoi : AreaPois)
    {
        _worldPacket << areaPoi.LastUpdate;
        _worldPacket << uint32(areaPoi.AreaPoiID);
        _worldPacket << uint32(areaPoi.Timer);
        _worldPacket << int32(areaPoi.VariableID);
        _worldPacket << int32(areaPoi.Value);
    }

    return &_worldPacket;
}
"""
patch(QPC, [("    _worldPacket << int32(QuestID);\n\n    return &_worldPacket;\n}\n}",
             "    _worldPacket << int32(QuestID);\n\n    return &_worldPacket;\n}\n" + qp_write + "}")])

# C) WorldSession.h — fwd-decls + handler decls
patch(WSH, [
 ("        class RequestWorldQuestUpdate;",
  "        class RequestWorldQuestUpdate;\n        class RequestAreaPoiUpdate;\n        class RequestScheduledAreaPoiUpdate;"),
 ("        void HandleRequestWorldQuestUpdate(WorldPackets::Quest::RequestWorldQuestUpdate& packet);",
  "        void HandleRequestWorldQuestUpdate(WorldPackets::Quest::RequestWorldQuestUpdate& packet);\n"
  "        void HandleRequestAreaPoiUpdate(WorldPackets::Quest::RequestAreaPoiUpdate& packet);\n"
  "        void HandleRequestScheduledAreaPoiUpdate(WorldPackets::Quest::RequestScheduledAreaPoiUpdate& packet);")])

# D) QuestHandler.cpp — include + 2 handler bodies
handlers="""

void WorldSession::HandleRequestAreaPoiUpdate(WorldPackets::Quest::RequestAreaPoiUpdate& /*packet*/)
{
    WorldPackets::Quest::AreaPoiUpdateResponse response;
    sAreaPoiMgr->FillActiveAreaPois(response.AreaPois);
    SendPacket(response.Write());
}

// The client sends this variant on its own timer to refresh timed/scheduled area POIs (world-boss and event
// countdowns). The response is identical to the on-demand request, so it mirrors the handler above.
void WorldSession::HandleRequestScheduledAreaPoiUpdate(WorldPackets::Quest::RequestScheduledAreaPoiUpdate& /*packet*/)
{
    WorldPackets::Quest::AreaPoiUpdateResponse response;
    sAreaPoiMgr->FillActiveAreaPois(response.AreaPois);
    SendPacket(response.Write());
}"""
patch(QHD, [
 ('#include "QuestPackets.h"', '#include "AreaPoiMgr.h"\n#include "QuestPackets.h"'),
 ("    sWorldQuestMgr->FillActiveWorldQuests(response.WorldQuestUpdates);\n\n    SendPacket(response.Write());\n}",
  "    sWorldQuestMgr->FillActiveWorldQuests(response.WorldQuestUpdates);\n\n    SendPacket(response.Write());\n}" + handlers)])

# E) Opcodes.cpp — wire the 2 CMSG
patch(OPC, [
 ("    DEFINE_HANDLER(CMSG_REQUEST_AREA_POI_UPDATE,                            STATUS_UNHANDLED, PROCESS_THREADUNSAFE, &WorldSession::Handle_NULL);",
  "    DEFINE_HANDLER(CMSG_REQUEST_AREA_POI_UPDATE,                            STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestAreaPoiUpdate);"),
 ("    DEFINE_HANDLER(CMSG_REQUEST_SCHEDULED_AREA_POI_UPDATE,                  STATUS_UNHANDLED, PROCESS_THREADUNSAFE, &WorldSession::Handle_NULL);",
  "    DEFINE_HANDLER(CMSG_REQUEST_SCHEDULED_AREA_POI_UPDATE,                  STATUS_LOGGEDIN,  PROCESS_THREADUNSAFE, &WorldSession::HandleRequestScheduledAreaPoiUpdate);")])

# F) WorldStatePackets.h — struct + packet
ws_pkt="""            uint32 VariableID = 0;
        };

        // Scheduled/recurring world states (area-POI + repeating game-event cycles). Duration is the whole cycle
        // length; the client derives the countdown from StartTime + Duration. (per feature/world-quests)
        struct ScheduledWorldStateInfo
        {
            ScheduledWorldStateInfo(time_t startTime, uint32 duration, uint32 variableID, int32 value)
                : StartTime(startTime), Duration(duration), VariableID(variableID), Value(value) { }

            Timestamp<> StartTime;
            uint32 Duration;
            uint32 VariableID;
            int32 Value;
        };

        class TC_GAME_API ActiveScheduledWorldStateInfo final : public ServerPacket
        {
        public:
            explicit ActiveScheduledWorldStateInfo() : ServerPacket(SMSG_ACTIVE_SCHEDULED_WORLD_STATE_INFO, 4) { }

            WorldPacket const* Write() override;

            std::vector<ScheduledWorldStateInfo> Schedules;
        };
    }
}"""
patch(WPH, [("            uint32 VariableID = 0;\n        };\n    }\n}", ws_pkt)])

# G) WorldStatePackets.cpp — Write after UpdateWorldState::Write
ws_write="""
WorldPacket const* ActiveScheduledWorldStateInfo::Write()
{
    _worldPacket.reserve(4 + Schedules.size() * 20);

    _worldPacket << Size<uint32>(Schedules);

    for (ScheduledWorldStateInfo const& schedule : Schedules)
    {
        _worldPacket << schedule.StartTime;
        _worldPacket << uint32(schedule.Duration);
        _worldPacket << uint32(schedule.VariableID);
        _worldPacket << int32(schedule.Value);
    }

    return &_worldPacket;
}
"""
patch(WPC, [("    _worldPacket.FlushBits();\n\n    return &_worldPacket;\n}\n}",
             "    _worldPacket.FlushBits();\n\n    return &_worldPacket;\n}\n" + ws_write + "}")])

# H) WorldStateMgr.h — fwd-decl + 2 decls
patch(WMH, [
 ("namespace WorldPackets::WorldState\n{\n    class InitWorldStates;\n}",
  "namespace WorldPackets::WorldState\n{\n    class InitWorldStates;\n    class ActiveScheduledWorldStateInfo;\n}"),
 ("    TC_GAME_API void SetValueAndSaveInDb(int32 worldStateId, int32 value, bool hidden, Map* map);",
  "    TC_GAME_API void SetValueAndSaveInDb(int32 worldStateId, int32 value, bool hidden, Map* map);\n"
  "    TC_GAME_API void FillActiveScheduledWorldStates(WorldPackets::WorldState::ActiveScheduledWorldStateInfo& packet);\n"
  "    TC_GAME_API void SendActiveScheduledWorldStateInfo(Player const* player = nullptr);")])

# I) WorldStateMgr.cpp — includes + bodies
wsm_bodies="""
void WorldStateMgr::FillActiveScheduledWorldStates(WorldPackets::WorldState::ActiveScheduledWorldStateInfo& packet)
{
    // Two schedulers: area POIs rotate their gating world state on the POI's duration, and repeating game events
    // flip theirs on the event's recurrence. Both are one thing to the client: a world state with a cycle.
    sAreaPoiMgr->FillScheduledWorldStates(packet.Schedules);
    sGameEventMgr->FillScheduledWorldStates(packet.Schedules);

    // The client keys these by VariableID, so a duplicate silently resolves to whichever was written last.
    // Resolve here; the stable sort makes a state claimed by both resolve to the area POI (the more specific)
    // and makes a no-op re-send produce identical bytes.
    std::ranges::stable_sort(packet.Schedules, {}, &WorldPackets::WorldState::ScheduledWorldStateInfo::VariableID);

    auto duplicates = std::ranges::unique(packet.Schedules, {}, &WorldPackets::WorldState::ScheduledWorldStateInfo::VariableID);
    packet.Schedules.erase(duplicates.begin(), duplicates.end());
}

void WorldStateMgr::SendActiveScheduledWorldStateInfo(Player const* player /*= nullptr*/)
{
    WorldPackets::WorldState::ActiveScheduledWorldStateInfo packet;
    FillActiveScheduledWorldStates(packet);

    // Empty is legitimate for a realm that rotates nothing, and the client clears its map from the payload, so
    // sending it is meaningful - but only to a player who asked. Broadcasting emptiness realm-wide is noise.
    if (packet.Schedules.empty() && !player)
        return;

    if (player)
        player->SendDirectMessage(packet.Write());
    else
        sWorld->SendGlobalMessage(packet.Write());
}
"""
patch(WMC, [('#include "WorldStatePackets.h"',
             '#include "WorldStatePackets.h"\n#include "AreaPoiMgr.h"\n#include "GameEventMgr.h"\n#include "Player.h"')],
      append=wsm_bodies)

# J) GameEventMgr.h — fwd-decl + public decl
patch(GEH, [
 ("#define max_ge_check_delay DAY  // 1 day in seconds",
  "namespace WorldPackets::WorldState { struct ScheduledWorldStateInfo; }\n\n#define max_ge_check_delay DAY  // 1 day in seconds"),
 ("        void HandleQuestComplete(uint32 quest_id);  // called on world event type quest completions",
  "        void HandleQuestComplete(uint32 quest_id);  // called on world event type quest completions\n        void FillScheduledWorldStates(std::vector<WorldPackets::WorldState::ScheduledWorldStateInfo>& schedules) const;")])

# K) GameEventMgr.cpp — include + body
gem_body="""
void GameEventMgr::FillScheduledWorldStates(std::vector<WorldPackets::WorldState::ScheduledWorldStateInfo>& schedules) const
{
    time_t const now = GameTime::GetGameTime();

    for (GameEventData const& event : mGameEvent)
    {
        // Only a repeating event that drives a world state has a cycle. `occurence` is that period in minutes.
        if (!event.WorldStateId || !event.occurence || !event.isValid())
            continue;

        // Outside the event's overall lifetime there is no current cycle. `end` is 0 for events that never expire.
        if (now < event.start || (event.end > event.start && now >= event.end))
            continue;

        time_t const period = time_t(event.occurence) * MINUTE;
        time_t const cycleStart = event.start + ((now - event.start) / period) * period;

        // Duration is the whole period; Value is the state's value right now (re-sent whenever the event flips).
        schedules.emplace_back(cycleStart, uint32(period), uint32(*event.WorldStateId),
            WorldStateMgr::GetValue(*event.WorldStateId, nullptr));
    }
}
"""
patch(GEC, [('#include "WorldStateMgr.h"', '#include "WorldStateMgr.h"\n#include "WorldStatePackets.h"')],
      append=gem_body)

print("WORLD-QUESTS SLICE APPLIED to", REPO)
