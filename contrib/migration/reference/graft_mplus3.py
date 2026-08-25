WT = "I:/TrinityCore/_migrate121"
def read(p): return open(f"{WT}/{p}", encoding="utf-8", errors="replace", newline="").read()
def write(p, s): open(f"{WT}/{p}", "w", encoding="utf-8", newline="").write(s)
def after_line(text, anchor, block):
    L=text.split("\n")
    for i,l in enumerate(L):
        if anchor in l: L[i:i+1]=[l]+block.split("\n"); return "\n".join(L)
    raise SystemExit("MISS line "+anchor)
def after_struct(text, anchor, block):
    L=text.split("\n")
    for i,l in enumerate(L):
        if anchor in l:
            for j in range(i,len(L)):
                if L[j].rstrip()=="};": L[j:j+1]=[L[j],""]+block.split("\n"); return "\n".join(L)
    raise SystemExit("MISS struct "+anchor)

STORES = {
 "MythicPlusSeasonRewardLevels": dict(
  struct="""struct MythicPlusSeasonRewardLevelsEntry
{
    uint32 ID;
    uint32 MythicPlusSeasonID;
    int32 ActivityTierID;
    int32 DifficultyLevel;
    int32 WeeklyRewardLevel;
    int32 EndOfRunRewardLevel;
};""",
  meta="""struct MythicPlusSeasonRewardLevelsMeta
{
    static constexpr DB2MetaField Fields[5] =
    {
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
    };
    static constexpr DB2Meta Instance = { .FileDataId = 2123783, .IndexField = -1, .ParentIndexField = 0, .FieldCount = 5, .FileFieldCount = 5, .LayoutHash = 0xA256317C, .Fields = Fields };
};""",
  loadinfo="""struct MythicPlusSeasonRewardLevelsLoadInfo
{
    static constexpr DB2FieldMeta Fields[6] =
    {
        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },
        { .IsSigned = false, .Type = FT_INT, .Name = "MythicPlusSeasonID" },
        { .IsSigned = true, .Type = FT_INT, .Name = "ActivityTierID" },
        { .IsSigned = true, .Type = FT_INT, .Name = "DifficultyLevel" },
        { .IsSigned = true, .Type = FT_INT, .Name = "WeeklyRewardLevel" },
        { .IsSigned = true, .Type = FT_INT, .Name = "EndOfRunRewardLevel" },
    };
    static constexpr DB2LoadInfo Instance{ Fields, 6, &MythicPlusSeasonRewardLevelsMeta::Instance, HOTFIX_SEL_MYTHIC_PLUS_SEASON_REWARD_LEVELS };
};""",
  extern='TC_GAME_API extern DB2Storage<MythicPlusSeasonRewardLevelsEntry>    sMythicPlusSeasonRewardLevelsStore;',
  storedef='DB2Storage<MythicPlusSeasonRewardLevelsEntry>   sMythicPlusSeasonRewardLevelsStore("MythicPlusSeasonRewardLevels.db2", &MythicPlusSeasonRewardLevelsLoadInfo::Instance);',
  load='    LOAD_DB2(sMythicPlusSeasonRewardLevelsStore);',
  hfenum='    HOTFIX_SEL_MYTHIC_PLUS_SEASON_REWARD_LEVELS,\n    HOTFIX_SEL_MYTHIC_PLUS_SEASON_REWARD_LEVELS_MAX_ID,',
  hfcpp='''    PrepareStatement(HOTFIX_SEL_MYTHIC_PLUS_SEASON_REWARD_LEVELS, "SELECT ID, MythicPlusSeasonID, ActivityTierID, DifficultyLevel, WeeklyRewardLevel, "
        "EndOfRunRewardLevel FROM mythic_plus_season_reward_levels WHERE (`VerifiedBuild` > 0) = ?", CONNECTION_SYNCH);
    PREPARE_MAX_ID_STMT(HOTFIX_SEL_MYTHIC_PLUS_SEASON_REWARD_LEVELS, "SELECT MAX(ID) + 1 FROM mythic_plus_season_reward_levels", CONNECTION_SYNCH);'''),
 "MythicPlusSeasonKeyFloor": dict(
  struct="""struct MythicPlusSeasonKeyFloorEntry
{
    uint32 ID;
    int32 KeyFloor;
    int32 PlayerConditionID;
    uint32 DisplaySeasonID;
};""",
  meta="""struct MythicPlusSeasonKeyFloorMeta
{
    static constexpr DB2MetaField Fields[3] =
    {
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
    };
    static constexpr DB2Meta Instance = { .FileDataId = 6684235, .IndexField = -1, .ParentIndexField = 2, .FieldCount = 3, .FileFieldCount = 2, .LayoutHash = 0x4033E02C, .Fields = Fields };
};""",
  loadinfo="""struct MythicPlusSeasonKeyFloorLoadInfo
{
    static constexpr DB2FieldMeta Fields[4] =
    {
        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },
        { .IsSigned = true, .Type = FT_INT, .Name = "KeyFloor" },
        { .IsSigned = true, .Type = FT_INT, .Name = "PlayerConditionID" },
        { .IsSigned = false, .Type = FT_INT, .Name = "DisplaySeasonID" },
    };
    static constexpr DB2LoadInfo Instance{ Fields, 4, &MythicPlusSeasonKeyFloorMeta::Instance, HOTFIX_SEL_MYTHIC_PLUS_SEASON_KEY_FLOOR };
};""",
  extern='TC_GAME_API extern DB2Storage<MythicPlusSeasonKeyFloorEntry>        sMythicPlusSeasonKeyFloorStore;',
  storedef='DB2Storage<MythicPlusSeasonKeyFloorEntry>       sMythicPlusSeasonKeyFloorStore("MythicPlusSeasonKeyFloor.db2", &MythicPlusSeasonKeyFloorLoadInfo::Instance);',
  load='    LOAD_DB2(sMythicPlusSeasonKeyFloorStore);',
  hfenum='    HOTFIX_SEL_MYTHIC_PLUS_SEASON_KEY_FLOOR,\n    HOTFIX_SEL_MYTHIC_PLUS_SEASON_KEY_FLOOR_MAX_ID,',
  hfcpp='''    PrepareStatement(HOTFIX_SEL_MYTHIC_PLUS_SEASON_KEY_FLOOR, "SELECT ID, KeyFloor, PlayerConditionID, DisplaySeasonID"
        " FROM mythic_plus_season_key_floor WHERE (`VerifiedBuild` > 0) = ?", CONNECTION_SYNCH);
    PREPARE_MAX_ID_STMT(HOTFIX_SEL_MYTHIC_PLUS_SEASON_KEY_FLOOR, "SELECT MAX(ID) + 1 FROM mythic_plus_season_key_floor", CONNECTION_SYNCH);'''),
 "WeeklyRewardChestThreshold": dict(
  struct="""struct WeeklyRewardChestThresholdEntry
{
    uint32 ID;
    int8 Type;
    int32 Threshold;
    int32 Index;
};""",
  meta="""struct WeeklyRewardChestThresholdMeta
{
    static constexpr DB2MetaField Fields[3] =
    {
        { .Type = FT_BYTE, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
        { .Type = FT_INT, .ArraySize = 1, .IsSigned = true },
    };
    static constexpr DB2Meta Instance = { .FileDataId = 3580962, .IndexField = -1, .ParentIndexField = -1, .FieldCount = 3, .FileFieldCount = 3, .LayoutHash = 0x66D9A6D5, .Fields = Fields };
};""",
  loadinfo="""struct WeeklyRewardChestThresholdLoadInfo
{
    static constexpr DB2FieldMeta Fields[4] =
    {
        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },
        { .IsSigned = true, .Type = FT_BYTE, .Name = "Type" },
        { .IsSigned = true, .Type = FT_INT, .Name = "Threshold" },
        { .IsSigned = true, .Type = FT_INT, .Name = "Index" },
    };
    static constexpr DB2LoadInfo Instance{ Fields, 4, &WeeklyRewardChestThresholdMeta::Instance, HOTFIX_SEL_WEEKLY_REWARD_CHEST_THRESHOLD };
};""",
  extern='TC_GAME_API extern DB2Storage<WeeklyRewardChestThresholdEntry>      sWeeklyRewardChestThresholdStore;',
  storedef='DB2Storage<WeeklyRewardChestThresholdEntry>     sWeeklyRewardChestThresholdStore("WeeklyRewardChestThreshold.db2", &WeeklyRewardChestThresholdLoadInfo::Instance);',
  load='    LOAD_DB2(sWeeklyRewardChestThresholdStore);',
  hfenum='    HOTFIX_SEL_WEEKLY_REWARD_CHEST_THRESHOLD,\n    HOTFIX_SEL_WEEKLY_REWARD_CHEST_THRESHOLD_MAX_ID,',
  hfcpp='''    PrepareStatement(HOTFIX_SEL_WEEKLY_REWARD_CHEST_THRESHOLD, "SELECT ID, Type, Threshold, `Index` FROM weekly_reward_chest_threshold"
        " WHERE (`VerifiedBuild` > 0) = ?", CONNECTION_SYNCH);
    PREPARE_MAX_ID_STMT(HOTFIX_SEL_WEEKLY_REWARD_CHEST_THRESHOLD, "SELECT MAX(ID) + 1 FROM weekly_reward_chest_threshold", CONNECTION_SYNCH);'''),
}

for name, s in STORES.items():
    p="src/server/game/DataStores/DB2Structure.h"; write(p, after_struct(read(p),"struct MythicPlusSeasonEntry", s["struct"]))
    # 12.1 already ships these Metas — do NOT graft (avoids C2011 redefinition)
    p="src/server/game/DataStores/DB2LoadInfo.h";   write(p, after_struct(read(p),"struct MythicPlusSeasonLoadInfo", s["loadinfo"]))
    p="src/server/game/DataStores/DB2Stores.h";     write(p, after_line(read(p),"sMythicPlusSeasonStore;", s["extern"]))
    p="src/server/game/DataStores/DB2Stores.cpp"
    t=read(p); t=after_line(t,'sMythicPlusSeasonStore("MythicPlusSeason.db2"', s["storedef"]); t=after_line(t,"LOAD_DB2(sMythicPlusSeasonStore)", s["load"]); write(p,t)
    p="src/server/database/Database/Implementation/HotfixDatabase.h"; write(p, after_line(read(p),"HOTFIX_SEL_MYTHIC_PLUS_SEASON_MAX_ID,", s["hfenum"]))
    p="src/server/database/Database/Implementation/HotfixDatabase.cpp"; write(p, after_line(read(p),"PREPARE_MAX_ID_STMT(HOTFIX_SEL_MYTHIC_PLUS_SEASON,", s["hfcpp"]))
    print("grafted", name)
print("graft3 complete")
