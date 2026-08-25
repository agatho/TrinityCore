import re, os
os.chdir(r"I:/TrinityCore/_migrate121")
S="src/server/game/DataStores/DB2Structure.h"; L="src/server/game/DataStores/DB2LoadInfo.h"
sd=open(S,encoding="utf-8",newline="").read(); ld=open(L,encoding="utf-8",newline="").read()
NL="\r\n" if "\r\n" in ld else "\n"
def R(d, old, new, tag):
    old=old.replace("\n",NL); new=new.replace("\n",NL)
    assert d.count(old)==1, f"{tag}: count={d.count(old)}"
    return d.replace(old,new,1)
def Rn(d, pat, repl, tag, expect=1):
    # regex replace on normalized text, count check
    norm=d.replace("\r\n","\n")
    ms=list(re.finditer(pat, norm))
    assert len(ms)==expect, f"{tag}: regex count={len(ms)}"
    norm=re.sub(pat, repl, norm, count=1)
    return norm.replace("\n", NL) if NL!="\n" else norm

# PlayerCompanionInfo
sd=R(sd,"struct PlayerCompanionInfoEntry\n{\n    LocalizedString UnlockDescription;\n    uint32 ID;\n",
       "struct PlayerCompanionInfoEntry\n{\n    LocalizedString UnlockDescription;\n    LocalizedString Field_12_1_0_68209_001;\n    uint32 ID;\n","PCIs1")
sd=R(sd,"    int32 Field_12_0_0_64499_012;\n    int32 ParentID;",
       "    int32 Field_12_0_0_64499_012;\n    int32 FlavorNodeID;\n    int32 ParentID;","PCIs2")
ld=R(ld,
'        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },',
'        { .IsSigned = false, .Type = FT_STRING, .Name = "UnlockDescription" },\n        { .IsSigned = false, .Type = FT_STRING, .Name = "Field_12_1_0_68209_001" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },',"PCIload_str")
ld=R(ld,
'        { .IsSigned = true, .Type = FT_INT, .Name = "Field_12_0_0_64499_012" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ParentID" },\n    };\n    static constexpr DB2LoadInfo Instance{ Fields, 15, &PlayerCompanionInfoMeta::Instance, HOTFIX_SEL_PLAYER_COMPANION_INFO };',
'        { .IsSigned = true, .Type = FT_INT, .Name = "Field_12_0_0_64499_012" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "FlavorNodeID" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ParentID" },\n    };\n    static constexpr DB2LoadInfo Instance{ Fields, 17, &PlayerCompanionInfoMeta::Instance, HOTFIX_SEL_PLAYER_COMPANION_INFO };',"PCIload_tail")
ld=R(ld,"static constexpr DB2FieldMeta Fields[15] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"UnlockDescription\" },",
       "static constexpr DB2FieldMeta Fields[17] =\n    {\n        { .IsSigned = false, .Type = FT_STRING, .Name = \"UnlockDescription\" },","PCIload_cnt")

# CharShipment
sd=R(sd,"    int32 Flags;\n    uint16 GarrFollowerID;\n    uint8 MaxShipments;\n};",
       "    uint8 MaxShipments;\n    uint16 GarrFollowerID;\n    int32 Flags;\n};","CSs")
ld=R(ld,'        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n        { .IsSigned = false, .Type = FT_SHORT, .Name = "GarrFollowerID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "MaxShipments" },\n    };',
       '        { .IsSigned = false, .Type = FT_BYTE, .Name = "MaxShipments" },\n        { .IsSigned = false, .Type = FT_SHORT, .Name = "GarrFollowerID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n    };',"CSl")

# ItemConversion
sd=R(sd,"    int32 AlternateItemLogicalCostGroupID;\n    int32 PlayerConditionID;\n};",
       "    int32 AlternateItemLogicalCostGroupID;\n    int32 PlayerConditionID;\n    int32 Flags;\n};","ICs")
ld=R(ld,'        { .IsSigned = true, .Type = FT_INT, .Name = "PlayerConditionID" },\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 6, &ItemConversionMeta::Instance, HOTFIX_SEL_ITEM_CONVERSION };',
       '        { .IsSigned = true, .Type = FT_INT, .Name = "PlayerConditionID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Flags" },\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 7, &ItemConversionMeta::Instance, HOTFIX_SEL_ITEM_CONVERSION };',"ICl")

# ExteriorComponent: remove last HouseExterior (regex, ignore em-dash comment), then add after Size
sd=Rn(sd, r"\n    int32 ItemID;[^\n]*\n    uint32 HouseExteriorWmoDataID;[^\n]*\n\};", "\n    int32 ItemID;                           // 12.1 references Item.ID\n};", "ECs_rm")
sd=R(sd,'    uint32 ID;\n    uint8 Size;                             // Meta[3] BYTE: WoWDBDefs "Size"\n    int32 ParentComponentID;',
       '    uint32 ID;\n    uint8 Size;                             // Meta[3] BYTE\n    uint32 HouseExteriorWmoDataID;          // 12.1 DC257C27: relation moved after Size\n    int32 ParentComponentID;',"ECs_add")
ld=Rn(ld, r'\n        \{ \.IsSigned = true, \.Type = FT_INT, \.Name = "ItemID" \},[^\n]*\n        \{ \.IsSigned = false, \.Type = FT_INT, \.Name = "HouseExteriorWmoDataID" \},[^\n]*', '\n        { .IsSigned = true, .Type = FT_INT, .Name = "ItemID" },', "ECl_rm")
ld=R(ld,'        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "Size" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "ParentComponentID" },',
       '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_BYTE, .Name = "Size" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "HouseExteriorWmoDataID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "ParentComponentID" },',"ECl_add")

# HouseDecor
sd=R(sd,"    uint32 ID;                               // Meta field 2: IndexField\n    int32 Field_003;\n    int32 GameObjectID;",
       "    uint32 ID;                               // Meta field 2: IndexField\n    int32 GameObjectID;","HDs")
ld=R(ld,'        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Field_003" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "GameObjectID" },',
       '        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "GameObjectID" },',"HDl")
ld=R(ld,"static constexpr DB2LoadInfo Instance{ Fields, 20, &HouseDecorMeta::Instance",
       "static constexpr DB2LoadInfo Instance{ Fields, 19, &HouseDecorMeta::Instance","HDc")

# HouseRoom
sd=R(sd,"struct HouseRoomEntry\n{\n    uint32 ID;\n    LocalizedString Name;\n    int8 Size;",
       "struct HouseRoomEntry\n{\n    LocalizedString Name;\n    uint32 ID;\n    int8 Size;","HRs1")
sd=R(sd,"    int32 Field_007;                         // NEW in 12.0.5.66330 (per WoWDBDefs layout 0xFC6C2118)\n};",
       "    int32 Field_007;                         // NEW in 12.0.5.66330\n    int32 Field_008;                         // NEW in 12.1 (0xF04DC279)\n};","HRs2")
ld=R(ld,'        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = false, .Type = FT_STRING, .Name = "Name" },\n        { .IsSigned = true, .Type = FT_BYTE, .Name = "Size" },',
       '        { .IsSigned = false, .Type = FT_STRING, .Name = "Name" },\n        { .IsSigned = false, .Type = FT_INT, .Name = "ID" },\n        { .IsSigned = true, .Type = FT_BYTE, .Name = "Size" },',"HRl1")
ld=R(ld,'        { .IsSigned = true, .Type = FT_INT, .Name = "Field_007" },  // NEW in 12.0.5.66330\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 9, &HouseRoomMeta::Instance',
       '        { .IsSigned = true, .Type = FT_INT, .Name = "Field_007" },\n        { .IsSigned = true, .Type = FT_INT, .Name = "Field_008" },  // NEW in 12.1 (0xF04DC279)\n    };\n\n    static constexpr DB2LoadInfo Instance{ Fields, 10, &HouseRoomMeta::Instance',"HRl2")

open(S,"w",encoding="utf-8",newline="").write(sd); open(L,"w",encoding="utf-8",newline="").write(ld)
print("ALL 6 applied cleanly")
