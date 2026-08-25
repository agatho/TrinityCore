import sys, re, os

OWNER = sys.argv[1]
ROOTS = {
    "chromie":  r"I:/TrinityCore/chromie/TrinityCore",
    "housing":  r"I:/TrinityCore/housing-system/TrinityCore",
    "garrison": r"I:/TrinityCore/garrison/TrinityCore",
    "delves":   r"I:/TrinityCore/delves/TrinityCore",
    "mythic":   r"I:/TrinityCore/mythic-plus/TrinityCore",
}
root = ROOTS[OWNER]

def load(rel):
    p = os.path.join(root, rel)
    d = open(p, encoding="utf-8", newline="").read()
    nl = "\r\n" if "\r\n" in d else "\n"
    return p, d, nl

def R(d, nl, old, new, tag):
    old = old.replace("\n", nl); new = new.replace("\n", nl)
    assert d.count(old) == 1, f"{tag}: exact count={d.count(old)}"
    return d.replace(old, new, 1)

def Rn(d, nl, pat, repl, tag):
    norm = d.replace("\r\n", "\n")
    ms = list(re.finditer(pat, norm))
    assert len(ms) == 1, f"{tag}: regex count={len(ms)}"
    norm = re.sub(pat, repl, norm, count=1)
    return norm.replace("\n", nl) if nl != "\n" else norm

changed = {}

if OWNER == "chromie":
    p, d, nl = load("src/server/game/Conditions/ConditionMgr.cpp")
    d = R(d, nl,
        '    { .Name = "Group status",              .HasConditionValue1 =  true, .HasConditionValue2 = false, .HasConditionValue3 = false, .HasConditionStringValue1 = false }\n};',
        '    { .Name = "Group status",              .HasConditionValue1 =  true, .HasConditionValue2 = false, .HasConditionValue3 = false, .HasConditionStringValue1 = false },\n    { .Name = "Chromie Time",              .HasConditionValue1 =  true, .HasConditionValue2 = false, .HasConditionValue3 = false, .HasConditionStringValue1 = false }\n};',
        "chromie_row")
    changed[p] = d

elif OWNER == "housing":
    # DB2Structure.h
    p, d, nl = load("src/server/game/DataStores/DB2Structure.h")
    d = R(d, nl,
        '    uint32 ID;\n    uint8 Size;                             // Meta[3] BYTE: WoWDBDefs "Size"\n    int32 ParentComponentID;',
        '    uint32 ID;\n    uint8 Size;                             // Meta[3] BYTE\n    uint32 HouseExteriorWmoDataID;          // 12.1 DC257C27: relation moved after Size\n    int32 ParentComponentID;', "EC_add_s")
    d = Rn(d, nl, r"\n    int32 ItemID;[^\n]*\n    uint32 HouseExteriorWmoDataID;[^\n]*\n\};",
        "\n    int32 ItemID;                           // 12.1 references Item.ID\n};", "EC_rm_s")
    d = R(d, nl,
        "    uint32 ID;                               // Meta field 2: IndexField\n    int32 Field_003;\n    int32 GameObjectID;",
        "    uint32 ID;                               // Meta field 2: IndexField\n    int32 GameObjectID;", "HD_s")
    d = R(d, nl,
        "struct HouseRoomEntry\n{\n    uint32 ID;\n    LocalizedString Name;\n    int8 Size;",
        "struct HouseRoomEntry\n{\n    LocalizedString Name;\n    uint32 ID;\n    int8 Size;", "HR_s1")
    d = R(d, nl,
        "    int32 Field_007;                         // NEW in 12.0.5.66330 (per WoWDBDefs layout 0xFC6C2118)\n};",
        "    int32 Field_007;                         // NEW in 12.0.5.66330\n    int32 Field_008;                         // NEW in 12.1 (0xF04DC279)\n};", "HR_s2")
    changed[p] = d
    # DB2LoadInfo.h
    p, d, nl = load("src/server/game/DataStores/DB2LoadInfo.h")
    d = R(d, nl,
        '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "Size" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "ParentComponentID" },',
        '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "Size" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "HouseExteriorWmoDataID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "ParentComponentID" },', "EC_add_l")
    d = Rn(d, nl,
        r'\n        \{ \.IsSigned = true, \.Type = FT_INT, \.Name = "ItemID" \},[^\n]*\n        \{ \.IsSigned = false, \.Type = FT_INT, \.Name = "HouseExteriorWmoDataID" \},[^\n]*',
        '\n        { .IsSigned = true, .Type = FT_INT, .Name = "ItemID" },', "EC_rm_l")
    d = R(d, nl,
        '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Field_003" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "GameObjectID" },',
        '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "GameObjectID" },', "HD_l")
    d = R(d, nl, "static constexpr DB2FieldMeta Fields[20] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"Name\" },\n        { .IsSigned = false, .Type = FT_FLOAT, .Name = \"InitialRotationX\" },",
                 "static constexpr DB2FieldMeta Fields[19] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"Name\" },\n        { .IsSigned = false, .Type = FT_FLOAT, .Name = \"InitialRotationX\" },", "HD_cnt")
    d = R(d, nl, "static constexpr DB2LoadInfo Instance{ Fields, 20, &HouseDecorMeta::Instance",
                 "static constexpr DB2LoadInfo Instance{ Fields, 19, &HouseDecorMeta::Instance", "HD_inst")
    d = R(d, nl,
        '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_STRING, .Name = "Name" },\n        { .IsSigned = true, .Type = FT_BYTE, .Name = "Size" },',
        '        { .IsSigned = false, .Type = FT_STRING, .Name = "Name" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_BYTE, .Name = "Size" },', "HR_l1")
    d = R(d, nl, "static constexpr DB2FieldMeta Fields[9] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"Name\" },\n        { .IsSigned = false, .Type = FT_INT, .Name = \"ID\" },",
                 "static constexpr DB2FieldMeta Fields[10] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"Name\" },\n        { .IsSigned = false, .Type = FT_INT, .Name = \"ID\" },", "HR_cnt")
    d = R(d, nl,
        '        { .IsSigned = true, .Type = FT_INT, .Name = "Field_007" },  // NEW in 12.0.5.66330\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 9, &HouseRoomMeta::Instance',
        '        { .IsSigned = true, .Type = FT_INT, .Name = "Field_007" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Field_008" },  // NEW in 12.1 (0xF04DC279)\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 10, &HouseRoomMeta::Instance', "HR_l2")
    changed[p] = d
    # HousingMgr.cpp
    p, d, nl = load("src/server/game/Housing/HousingMgr.cpp")
    d = R(d, nl, "data.Field_003 = entry->Field_003;", "data.Field_003 = 0;", "HMgr")
    changed[p] = d

elif OWNER == "garrison":
    p, d, nl = load("src/server/game/DataStores/DB2Structure.h")
    d = R(d, nl, "    int32 Flags;\n    uint16 GarrFollowerID;\n    uint8 MaxShipments;\n};",
                 "    uint8 MaxShipments;\n    uint16 GarrFollowerID;\n    int32 Flags;\n};", "CS_s")
    d = R(d, nl, "    int8 GarrFollowerTypeID;\n    uint8 FollowerLevel;", "    uint8 GarrFollowerTypeID;\n    uint8 FollowerLevel;", "GFLXP_s")
    d = R(d, nl, "    uint32 QualityItemID;\n    int8 Quality;", "    uint32 QualityItemID;\n    uint8 Quality;", "GFQ_s")
    d = R(d, nl, "    int32 MaxItemLevel;\n    int8 FollowerTypeID;", "    int32 MaxItemLevel;\n    uint8 FollowerTypeID;", "GILUD_s")
    changed[p] = d
    p, d, nl = load("src/server/game/DataStores/DB2LoadInfo.h")
    d = R(d, nl,
        '        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n        { .IsSigned = false, .Type = FT_SHORT, .Name = "GarrFollowerID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "MaxShipments" },\n    };',
        '        { .IsSigned = false, .Type = FT_BYTE, .Name = "MaxShipments" },\n        { .IsSigned = false, .Type = FT_SHORT, .Name = "GarrFollowerID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n    };', "CS_l")
    d = R(d, nl, '{ .IsSigned = true, .Type = FT_BYTE, .Name = "GarrFollowerTypeID" },', '{ .IsSigned = false, .Type = FT_BYTE, .Name = "GarrFollowerTypeID" },', "GFLXP_l")
    d = R(d, nl, '{ .IsSigned = true, .Type = FT_BYTE, .Name = "Quality" },', '{ .IsSigned = false, .Type = FT_BYTE, .Name = "Quality" },', "GFQ_l")
    d = R(d, nl, '{ .IsSigned = true, .Type = FT_BYTE, .Name = "FollowerTypeID" },', '{ .IsSigned = false, .Type = FT_BYTE, .Name = "FollowerTypeID" },', "GILUD_l")
    changed[p] = d

elif OWNER == "delves":
    p, d, nl = load("src/server/game/DataStores/DB2Structure.h")
    d = R(d, nl,
        "struct PlayerCompanionInfoEntry\n{\n    LocalizedString UnlockDescription;\n    uint32 ID;\n",
        "struct PlayerCompanionInfoEntry\n{\n    LocalizedString UnlockDescription;\n    LocalizedString Field_12_1_0_68209_001;\n    uint32 ID;\n", "PCI_s1")
    d = R(d, nl, "    int32 Field_12_0_0_64499_012;\n    int32 ParentID;",
                 "    int32 Field_12_0_0_64499_012;\n    int32 FlavorNodeID;\n    int32 ParentID;", "PCI_s2")
    changed[p] = d
    p, d, nl = load("src/server/game/DataStores/DB2LoadInfo.h")
    d = R(d, nl,
        'static constexpr DB2FieldMeta Fields[15] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },',
        'static constexpr DB2FieldMeta Fields[17] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },', "PCI_cnt")
    d = R(d, nl,
        '        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },',
        '        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },\n        { .IsSigned = false, .Type = FT_STRING, .Name = "Field_12_1_0_68209_001" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },', "PCI_str")
    d = R(d, nl,
        '        { .IsSigned = true, .Type = FT_INT, .Name = "Field_12_0_0_64499_012" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ParentID" },\n    };\n    static constexpr DB2LoadInfo Instance{ Fields, 15, &PlayerCompanionInfoMeta::Instance, HOTFIX_SEL_PLAYER_COMPANION_INFO };',
        '        { .IsSigned = true, .Type = FT_INT, .Name = "Field_12_0_0_64499_012" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "FlavorNodeID" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ParentID" },\n    };\n    static constexpr DB2LoadInfo Instance{ Fields, 17, &PlayerCompanionInfoMeta::Instance, HOTFIX_SEL_PLAYER_COMPANION_INFO };', "PCI_tail")
    changed[p] = d

elif OWNER == "mythic":
    p, d, nl = load("src/server/game/DataStores/DB2Structure.h")
    d = R(d, nl,
        "    int32 AlternateItemLogicalCostGroupID;\n    int32 PlayerConditionID;\n};",
        "    int32 AlternateItemLogicalCostGroupID;\n    int32 PlayerConditionID;\n    int32 Flags;\n};", "IC_s")
    changed[p] = d
    p, d, nl = load("src/server/game/DataStores/DB2LoadInfo.h")
    d = R(d, nl,
        '        { .IsSigned = true, .Type = FT_INT, .Name = "PlayerConditionID" },\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 6, &ItemConversionMeta::Instance, HOTFIX_SEL_ITEM_CONVERSION };',
        '        { .IsSigned = true, .Type = FT_INT, .Name = "PlayerConditionID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 7, &ItemConversionMeta::Instance, HOTFIX_SEL_ITEM_CONVERSION };', "IC_l1")
    d = R(d, nl, "static constexpr DB2FieldMeta Fields[6] =\n    {\n        { .IsSigned = false, .Type = FT_INT, .Name = \"ID\" },\n        { .IsSigned = true, .Type = FT_INT, .Name = \"Unknown920\" },",
                 "static constexpr DB2FieldMeta Fields[7] =\n    {\n        { .IsSigned = false, .Type = FT_INT, .Name = \"ID\" },\n        { .IsSigned = true, .Type = FT_INT, .Name = \"Unknown920\" },", "IC_cnt")
    changed[p] = d
else:
    raise SystemExit("unknown owner")

for p, d in changed.items():
    open(p, "w", encoding="utf-8", newline="").write(d)
    print("wrote", p)
print("OK", OWNER)
