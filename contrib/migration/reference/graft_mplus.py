import io
WT = "I:/TrinityCore/_migrate121"
NL = "\r\n"

def read(p):
    return open(f"{WT}/{p}", encoding="utf-8", errors="replace", newline="").read()
def write(p, s):
    open(f"{WT}/{p}", "w", encoding="utf-8", newline="").write(s)

def insert_after_line(text, anchor_substr, block):
    lines = text.split("\n")
    for i,l in enumerate(lines):
        if anchor_substr in l:
            lines[i:i+1] = [l] + block.split("\n")
            return "\n".join(lines)
    raise SystemExit(f"anchor not found (line): {anchor_substr}")

def insert_after_struct(text, anchor_substr, block):
    lines = text.split("\n")
    for i,l in enumerate(lines):
        if anchor_substr in l:
            # find closing '};' after i
            for j in range(i, len(lines)):
                if lines[j].rstrip() == "};":
                    lines[j:j+1] = [lines[j], ""] + block.split("\n")
                    return "\n".join(lines)
    raise SystemExit(f"anchor not found (struct): {anchor_substr}")

def insert_before_line(text, anchor_substr, block):
    lines = text.split("\n")
    for i,l in enumerate(lines):
        if anchor_substr in l:
            lines[i:i] = block.split("\n")
            return "\n".join(lines)
    raise SystemExit(f"anchor not found (before): {anchor_substr}")

# 1) DB2Structure.h — struct
p="src/server/game/DataStores/DB2Structure.h"
blk="""struct MythicPlusSeasonTrackedMapEntry
{
    uint32 ID;
    int32 MapChallengeModeID;
    uint32 DisplaySeasonID;
};"""
write(p, insert_after_struct(read(p), "struct MythicPlusSeasonEntry", blk))

# 2) DB2Metadata.h — Meta
p="src/server/game/DataStores/DB2Metadata.h"
blk="""struct MythicPlusSeasonTrackedMapMeta
{
    static constexpr DB2MetaField Fields[2] =
    {
        { .Type = FT_INT,                  .ArraySize =  1, .IsSigned =  true },
        { .Type = FT_INT,                  .ArraySize =  1, .IsSigned =  true },
    };

    static constexpr DB2Meta Instance =
    {
        .FileDataId         = 4521365,
        .IndexField         = -1,
        .ParentIndexField   = 1,
        .FieldCount         = 2,
        .FileFieldCount     = 1,
        .LayoutHash         = 0x03958F0D,
        .Fields             = Fields
    };
};"""
# 12.1 already ships MythicPlusSeasonTrackedMapMeta — do NOT graft (avoids C2011 redefinition)

# 3) DB2LoadInfo.h — LoadInfo
p="src/server/game/DataStores/DB2LoadInfo.h"
blk="""struct MythicPlusSeasonTrackedMapLoadInfo
{
    static constexpr DB2FieldMeta Fields[3] =
    {
        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },
        { .IsSigned = true, .Type = FT_INT, .Name = "MapChallengeModeID" },
        { .IsSigned = false, .Type = FT_INT, .Name = "DisplaySeasonID" },
    };

    static constexpr DB2LoadInfo Instance{ Fields, 3, &MythicPlusSeasonTrackedMapMeta::Instance, HOTFIX_SEL_MYTHIC_PLUS_SEASON_TRACKED_MAP };
};"""
write(p, insert_after_struct(read(p), "struct MythicPlusSeasonLoadInfo", blk))

# 4) DB2Stores.h — extern
p="src/server/game/DataStores/DB2Stores.h"
blk='TC_GAME_API extern DB2Storage<MythicPlusSeasonTrackedMapEntry>      sMythicPlusSeasonTrackedMapStore;'
write(p, insert_after_line(read(p), "sMythicPlusSeasonStore;", blk))

# 5) DB2Stores.cpp — store def + LOAD_DB2
p="src/server/game/DataStores/DB2Stores.cpp"
t=read(p)
t=insert_after_line(t, 'sMythicPlusSeasonStore("MythicPlusSeason.db2"',
    'DB2Storage<MythicPlusSeasonTrackedMapEntry>     sMythicPlusSeasonTrackedMapStore("MythicPlusSeasonTrackedMap.db2", &MythicPlusSeasonTrackedMapLoadInfo::Instance);')
t=insert_after_line(t, "LOAD_DB2(sMythicPlusSeasonStore)",
    "    LOAD_DB2(sMythicPlusSeasonTrackedMapStore);")
write(p, t)

# 6) HotfixDatabase.h — enum entries
p="src/server/database/Database/Implementation/HotfixDatabase.h"
blk="    HOTFIX_SEL_MYTHIC_PLUS_SEASON_TRACKED_MAP,\n    HOTFIX_SEL_MYTHIC_PLUS_SEASON_TRACKED_MAP_MAX_ID,"
write(p, insert_after_line(read(p), "HOTFIX_SEL_MYTHIC_PLUS_SEASON_MAX_ID,", blk))

# 7) HotfixDatabase.cpp — statements
p="src/server/database/Database/Implementation/HotfixDatabase.cpp"
blk='''    PrepareStatement(HOTFIX_SEL_MYTHIC_PLUS_SEASON_TRACKED_MAP, "SELECT ID, MapChallengeModeID, DisplaySeasonID"
        " FROM mythic_plus_season_tracked_map WHERE (`VerifiedBuild` > 0) = ?", CONNECTION_SYNCH);
    PREPARE_MAX_ID_STMT(HOTFIX_SEL_MYTHIC_PLUS_SEASON_TRACKED_MAP, "SELECT MAX(ID) + 1 FROM mythic_plus_season_tracked_map", CONNECTION_SYNCH);'''
write(p, insert_after_line(read(p), 'PREPARE_MAX_ID_STMT(HOTFIX_SEL_MYTHIC_PLUS_SEASON,', blk))

# 8) CharacterDatabase.h — 9 enum entries before MAX
p="src/server/database/Database/Implementation/CharacterDatabase.h"
blk="""    CHAR_SEL_CHARACTER_MYTHIC_PLUS,
    CHAR_INS_CHARACTER_MYTHIC_PLUS,
    CHAR_DEL_CHARACTER_MYTHIC_PLUS,
    CHAR_SEL_CHARACTER_MYTHIC_PLUS_WEEKLY,
    CHAR_INS_CHARACTER_MYTHIC_PLUS_WEEKLY,
    CHAR_DEL_CHARACTER_MYTHIC_PLUS_WEEKLY,
    CHAR_SEL_CHARACTER_MYTHIC_PLUS_VAULT,
    CHAR_INS_CHARACTER_MYTHIC_PLUS_VAULT,
    CHAR_DEL_CHARACTER_MYTHIC_PLUS_VAULT,"""
write(p, insert_before_line(read(p), "MAX_CHARACTERDATABASE_STATEMENTS", blk))

# 9) CharacterDatabase.cpp — 9 PrepareStatements after function opening
p="src/server/database/Database/Implementation/CharacterDatabase.cpp"
blk='''    PrepareStatement(CHAR_SEL_CHARACTER_MYTHIC_PLUS, "SELECT challengeModeId, level, durationMs, deaths, completionDate, score, affix1, affix2, affix3, affix4 FROM character_mythic_plus WHERE guid = ?", CONNECTION_ASYNC);
    PrepareStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS, "INSERT INTO character_mythic_plus (guid, challengeModeId, level, durationMs, deaths, completionDate, score, affix1, affix2, affix3, affix4) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS, "DELETE FROM character_mythic_plus WHERE guid = ?", CONNECTION_ASYNC);
    PrepareStatement(CHAR_SEL_CHARACTER_MYTHIC_PLUS_WEEKLY, "SELECT challengeModeId, level, timed, completionDate, resetTime FROM character_mythic_plus_weekly WHERE guid = ?", CONNECTION_ASYNC);
    PrepareStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_WEEKLY, "INSERT INTO character_mythic_plus_weekly (guid, challengeModeId, level, timed, completionDate, resetTime) VALUES (?, ?, ?, ?, ?, ?)", CONNECTION_ASYNC);
    PrepareStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS_WEEKLY, "DELETE FROM character_mythic_plus_weekly WHERE guid = ?", CONNECTION_ASYNC);
    PrepareStatement(CHAR_SEL_CHARACTER_MYTHIC_PLUS_VAULT, "SELECT claimedResetTime, keystoneResetTime, prevWeekResetTime, prevWeekBestLevel, prevWeekBestTimedLevel FROM character_mythic_plus_vault WHERE guid = ?", CONNECTION_ASYNC);
    PrepareStatement(CHAR_INS_CHARACTER_MYTHIC_PLUS_VAULT, "INSERT INTO character_mythic_plus_vault (guid, claimedResetTime, keystoneResetTime, prevWeekResetTime, prevWeekBestLevel, prevWeekBestTimedLevel) VALUES (?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE claimedResetTime = VALUES(claimedResetTime), keystoneResetTime = VALUES(keystoneResetTime), prevWeekResetTime = VALUES(prevWeekResetTime), prevWeekBestLevel = VALUES(prevWeekBestLevel), prevWeekBestTimedLevel = VALUES(prevWeekBestTimedLevel)", CONNECTION_ASYNC);
    PrepareStatement(CHAR_DEL_CHARACTER_MYTHIC_PLUS_VAULT, "DELETE FROM character_mythic_plus_vault WHERE guid = ?", CONNECTION_ASYNC);'''
t=read(p)
# insert right after the DoPrepareStatements opening brace
import re
idx=t.find("DoPrepareStatements()")
brace=t.find("{", idx)
nl_after=t.find("\n", brace)+1
t=t[:nl_after]+blk+"\n"+t[nl_after:]
write(p, t)

print("graft complete")
