WT = "I:/TrinityCore/_migrate121"
SC = "C:/Users/daimon/AppData/Local/Temp/claude/c--dumps/d61f229a-840a-4add-ab28-452509f4a22b/scratchpad"

def read(p): return open(f"{WT}/{p}", encoding="utf-8", errors="replace", newline="").read()
def write(p, s): open(f"{WT}/{p}", "w", encoding="utf-8", newline="").write(s)
def rd(f): return open(f, encoding="utf-8", newline="").read().rstrip("\n")

def after_line(text, anchor, block, nth=1):
    lines = text.split("\n"); cnt=0
    for i,l in enumerate(lines):
        if anchor in l:
            cnt+=1
            if cnt==nth:
                lines[i:i+1] = [l] + block.split("\n"); return "\n".join(lines)
    raise SystemExit(f"MISS line: {anchor}")

def after_struct(text, anchor, block):
    lines = text.split("\n")
    for i,l in enumerate(lines):
        if anchor in l:
            for j in range(i, len(lines)):
                if lines[j].rstrip()=="        };":  # packet classes indented 8
                    lines[j:j+1]=[lines[j],""]+block.split("\n"); return "\n".join(lines)
    raise SystemExit(f"MISS struct: {anchor}")

def before_line(text, anchor, block):
    lines=text.split("\n")
    for i,l in enumerate(lines):
        if anchor in l:
            lines[i:i]=block.split("\n"); return "\n".join(lines)
    raise SystemExit(f"MISS before: {anchor}")

# A) MiscPackets.h — timer classes after StartTimer
p="src/server/game/Server/Packets/MiscPackets.h"
write(p, after_struct(read(p), "class StartTimer final", rd(f"{SC}/mp_misc_h_trim.txt")))
# B) MiscPackets.cpp — Write impls after StartTimer::Write
p="src/server/game/Server/Packets/MiscPackets.cpp"
t=read(p); a=t.find("StartTimer::Write"); b=t.find("\n}", a)+2
t=t[:b]+"\n"+rd(f"{SC}/mp_misc_cpp.txt")+"\n"+t[b:]; write(p,t)

# C) Player.h edits
p="src/server/game/Entities/Player/Player.h"
t=read(p)
t=after_line(t,"class Garrison;","class MythicPlusData;")
t=before_line(t,"MAX_PLAYER_LOGIN_QUERY","    PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS,\n    PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS_WEEKLY,\n    PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS_VAULT,")
# method + member near Garrison equivalents
import re
t=t.replace("        Garrison* GetGarrison() const { return _garrison.get(); }",
            "        Garrison* GetGarrison() const { return _garrison.get(); }\n        MythicPlusData* GetMythicPlusData() const { return _mythicPlusData.get(); }\n        void UpdateDungeonScore();",1)
t=t.replace("        std::unique_ptr<Garrison> _garrison;",
            "        std::unique_ptr<Garrison> _garrison;\n        std::unique_ptr<MythicPlusData> _mythicPlusData;",1)
write(p,t)

# D) Player.cpp edits
p="src/server/game/Entities/Player/Player.cpp"
t=read(p)
t=after_line(t,'#include "Garrison.h"','#include "MythicPlusData.h"\n#include "MythicPlusPacketsCommon.h"')
load_blk='''
    _mythicPlusData = std::make_unique<MythicPlusData>(this);
    _mythicPlusData->LoadFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS));
    _mythicPlusData->LoadVaultFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS_VAULT));
    _mythicPlusData->LoadWeeklyFromDB(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_MYTHIC_PLUS_WEEKLY));
    UpdateDungeonScore();'''
t=after_line(t,"_garrison = std::move(garrison);",load_blk,nth=1)
save_blk='''
    if (_mythicPlusData)
        _mythicPlusData->SaveToDB(trans);'''
t=after_line(t,"_garrison->SaveToDB(trans);",save_blk,nth=1)
impl='''
void Player::UpdateDungeonScore()
{
    WorldPackets::MythicPlus::DungeonScoreSummary summary;
    WorldPackets::MythicPlus::DungeonScoreData data;
    if (MythicPlusData* mythicPlus = GetMythicPlusData())
    {
        mythicPlus->BuildDungeonScoreSummary(summary);
        mythicPlus->BuildDungeonScoreData(data);
    }
    SetUpdateFieldValue(m_values.ModifyValue(&Player::m_playerData).ModifyValue(&UF::PlayerData::DungeonScore), std::move(summary));
    SetUpdateFieldValue(m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::DungeonScore), std::move(data));
}
'''
t=t.rstrip("\n")+"\n"+impl
write(p,t)

# E) WorldSession.h — ChallengeMode/Mythic+ handler declarations
p="src/server/game/Server/WorldSession.h"
ws_fwd='''    namespace ChallengeMode
    {
        class RequestMythicPlusSeasonData;
        class RequestMythicPlusAffixes;
        class StartChallengeMode;
        class ResetChallengeMode;
        class MythicPlusRequestMapStats;
    }
'''
write(p, before_line(read(p), "    namespace Movement", ws_fwd))
ws_decls='''        // Challenge Mode (Mythic+)
        void HandleRequestMythicPlusSeasonData(WorldPackets::ChallengeMode::RequestMythicPlusSeasonData& requestMythicPlusSeasonData);
        void HandleRequestMythicPlusAffixes(WorldPackets::ChallengeMode::RequestMythicPlusAffixes& requestMythicPlusAffixes);
        void HandleStartChallengeMode(WorldPackets::ChallengeMode::StartChallengeMode& startChallengeMode);
        void HandleResetChallengeMode(WorldPackets::ChallengeMode::ResetChallengeMode& resetChallengeMode);
        void HandleMythicPlusRequestMapStats(WorldPackets::ChallengeMode::MythicPlusRequestMapStats& request);'''
write(p, after_line(read(p), "HandleMoveRemoveInertiaAck", ws_decls))

# F) Map.h — InstanceMap ChallengeMode fwd-decl + accessors + member
p="src/server/game/Maps/Map.h"
t=read(p)
t=after_line(t, "class InstanceScript;", "class ChallengeMode;")
t=after_line(t, "InstanceScenario const* GetInstanceScenario() const { return i_scenario.get(); }",
    "        ChallengeMode* GetChallengeMode() { return i_challengeMode.get(); }\n        ChallengeMode const* GetChallengeMode() const { return i_challengeMode.get(); }")
t=after_line(t, "std::unique_ptr<InstanceScenario> i_scenario;", "        std::unique_ptr<ChallengeMode> i_challengeMode;")
write(p,t)
# G) Map.cpp — include ChallengeMode.h (unique_ptr complete-type at dtor)
p="src/server/game/Maps/Map.cpp"
write(p, after_line(read(p), '#include "Map.h"', '#include "ChallengeMode.h"'))
print("graft2 complete")
