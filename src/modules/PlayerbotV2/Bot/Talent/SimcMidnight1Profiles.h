// Generated from simc/midnight (https://github.com/simulationcraft/simc/tree/midnight/profiles/MID1).
// Coverage: 31 of 39 specs (DPS + tanks). Healers + Augmentation absent from
// simc and populated separately. Each entry's `talents` is the simc
// bit-packed loadout string, decoded at module init via SimcLoadoutDecoder
// into {node_id, entry_id, ranks} triples and stored as the entries_json
// column of playerbot_v2_talent_build (context=1=Raid).

#pragma once

#include <cstdint>
#include <string_view>

namespace Playerbot::V2::Talent {

struct SimcRaidProfile
{
    uint8_t          class_id; // ChrClasses.db2 id
    uint16_t         spec_id;  // ChrSpecialization.db2 id
    std::string_view label;    // Human-readable, includes hero spec suffix
    std::string_view talents;  // simc bit-packed (base64 alphabet)
};

inline constexpr SimcRaidProfile kSimcMidnight1Profiles[] = {
    { 1,   71, "MID1_Warrior_Arms", "CcEAAAAAAAAAAAAAAAAAAAAAAAzMzsMzMzMDAAAghphxYmxyMzMzgxMDAAAAgZWmZAZMWWGYBMgZYCZGsBMjNz2YwMGgZGAmxwA"},
    { 1,   72, "MID1_Warrior_Fury", "CgEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgGDjxMsMzMzMDjZmZGzMzsMzMGzMbDzMAAQMWWGYBMBzwEYG2AmZ2Y2GAAMzYYMzMMYA"},
    { 1,   73, "MID1_Warrior_Protection", "CkEAAAAAAAAAAAAAAAAAAAAAA02AAAzMDzMzMzMzmxsMjxYmGGDLzMzMDGzMAAAAYZAYGDwAbwyiRjZAMbYmNYGzMY2GAMzAAwMgB"},
    { 2,   66, "MID1_Paladin_Protection", "CIEAAAAAAAAAAAAAAAAAAAAAAsMzAzyMLmZMDLLDzYmFbzYAAAAAAAAg0MziZMmxYmt2AgBADsNAAwMTbzMbzAEYzADWMzMAzMAALzAMzAG"},
    { 2,   70, "MID1_Paladin_Retribution", "CYEAAAAAAAAAAAAAAAAAAAAAAAAAAAAQz22MzsMMzAAAAAAwoMmhZGbDz2wMbzYMmZYGbsNMAAkZm2mZ2mBAsBYAwYGmBzYMbYZGMMmxgB"},
    { 3,  253, "MID1_Hunter_Beast_Mastery", "C0PAAAAAAAAAAAAAAAAAAAAAAAMmxwCsBzwQDbAAYG2GzsMzwMmZYYmxYmxMzYGzwMzYGzghmBAAAAAMDAAAzMzMAzshwwsA2MA"},
    { 3,  254, "MID1_Hunter_Marksmanship", "C4PAAAAAAAAAAAAAAAAAAAAAAwCMwMGNWGQmBbAAAAAAAAAzYGzwMmZGzgx0MmZmZ222MzMDzMYmZbwsMYGAAwMzMAwMjNmFDwGG"},
    { 3,  255, "MID1_Hunter_Survival_Sentinel_2H", "C8PAAAAAAAAAAAAAAAAAAAAAAMWgBmxoxyAYmgNjZmxMPwy8AAAAAAAMjZmZYGDjZwYaGAAAAwAAYZbmZWMzMzYmZMAMDbMMGzYjB"},
    { 4,  259, "MID1_Rogue_Assassination", "CMQAAAAAAAAAAAAAAAAAAAAAAYmZMbzgBAAAAAmlBbzAAAAAAabbmZmZmZMmZmZ2mZZmZGMmZmZMzYYAMwCMjRjZBklBsZAwMzgB"},
    { 4,  260, "MID1_Rogue_Outlaw_Fatebound", "CQQAAAAAAAAAAAAAAAAAAAAAAAgx2MGjZMzsNzMzMjHwswDMzMLTLD2mBAAAAAMbbzMzwMzMzYmZWGAAAAGADsBzY0Y2AsNhFGAMzMwA"},
    { 4,  261, "MID1_Rogue_Subtlety", "CUQAAAAAAAAAAAAAAAAAAAAAAAgx2MAAAAAwsMGLTMbbjxMDDmZmZGjZbMzMbbjZMzMjBjZWGAAAAGMmFzyADYBsMMhMLYGmZAmZGA"},
    { 5,  258, "MID1_Priest_Shadow_Voidweaver", "CIQAAAAAAAAAAAAAAAAAAAAAAMMjZGAAAAAAAAAAAgxYxMGLzMMz2MDzw2MzYmZGbIzYxMNAzMzAABY2mtFwsxAMDwYmZGz2YGMzgZwA"},
    { 6,  250, "MID1_Death_Knight_Blood_San'layn", "CoPAAAAAAAAAAAAAAAAAAAAAAwYWGzMmZmZmhZbmZmmZxMjhxAAAAAzMzMzwMzYmZMDAMzMzAAAwADMjNNW2AZZAbAmhBAAMzgBGA"},
    { 6,  251, "MID1_Death_Knight_Frost", "CsPAAAAAAAAAAAAAAAAAAAAAAMDwMjZMmZY2mZmZmZxMjMjxMDzw4BMzgZmZmZAAAAAAAAAwY2GYALglhJkxCmZMzMwMAGmZAmBM"},
    { 6,  252, "MID1_Death_Knight_Unholy_Rider", "CwPAAAAAAAAAAAAAAAAAAAAAAAwMjZMDDz2MzMTzmZmZMjBAAAAAAAgZeAmZAwyMmZ2mZGzYGwmZxwQGY2YoxCAmBAmZGzAMzMjZMA"},
    { 7,  262, "MID1_Shaman_Elemental_Stormbringer", "CYQAAAAAAAAAAAAAAAAAAAAAAAAAAAzMbLzMzMzMLbbDMmZAAAAAbmZbzMzwmhFmtZmGamNAYWmZmxYbxEmZ2GLzMzMGWmlZsYmhZWAAGAzMzMGGG"},
    { 7,  263, "MID1_Shaman_Enhancement_Totemic", "CcQAAAAAAAAAAAAAAAAAAAAAAMzMjZmZmZmZmZmZmZGAAAAAAAAAYB2gZsox2AYmgNAsMjZMWWmBmZ2GLzMzMMWGzAAYAGzMxMDAMGA"},
    { 8,   62, "MID1_Mage_Arcane_Spellslinger", "C4DAAAAAAAAAAAAAAAAAAAAAAYGGLzMzswMzQzMzAAAwAAgAmZmZZZmZYBAgtxMzMmtFLzMzYmxYMzMGLMzMjZAAGAAAzsAAmBADD"},
    { 8,   63, "MID1_Mage_Fire_Sunfury", "C8DAAAAAAAAAAAAAAAAAAAAAAYGGLzMzswMDZmZGAAAGAwMz0sssMDAwmZmx2wYmBAAAAAsZmZmZAAwYGzYmZMz2AwMDxMGDmhB"},
    { 8,   64, "MID1_Mage_Frost_Spellslinger", "CAEAAAAAAAAAAAAAAAAAAAAAAYGGLzMzsMmZmYmZmZMjZWMzMzMjZAAAgZmZWWmZaDAAAAAAsBw2yYmZGMLzDYMDLAAAMzCwMhBMDGA"},
    { 9,  265, "MID1_Warlock_Affliction_Soul_Harvester", "CkQAAAAAAAAAAAAAAAAAAAAAAwMzMzoZhhZmZmlBAAYmZZ2MzsMzAAjllBGwEMDbBG2GAAAmBAAwMDzMjxwwMmZmxgZmZGAwMwA"},
    { 9,  266, "MID1_Warlock_Demonology_Soul_Harvester", "CoQAAAAAAAAAAAAAAAAAAAAAAwMzMzoZjhZmxsMAAAAAAAjtlBGwAmhtQGbGjx2sMzMjZAAzMzMzAMzMmxMDAAwYmZmZMDLDAD"},
    { 9,  267, "MID1_Warlock_Destruction_Hellcaller", "CsQAAAAAAAAAAAAAAAAAAAAAAwMzMzoZjhZmZmlZhxMLGjFzAAgZmxMzsYBGYWMaMDgZL2YAAgxAjNAgZGYmxYAAAYmZmBAwMDD"},
    {10,  268, "MID1_Monk_Brewmaster", "CwQAAAAAAAAAAAAAAAAAAAAAAAAAAgZbzYGzM2mxGmZAAAAAAAYZBjYmBmhBzYMzMzwsMmZMzywymttxMmFAAYZWmWmtZWGAAIAzwGYmBMNGAAwA"},
    {10,  269, "MID1_Monk_Windwalker", "C0QAAAAAAAAAAAAAAAAAAAAAAMzYw2wwsMzMLzAAAAAAAAAAAAsMMCzYbYAzYYmZmhZZYGmlZCAYzMbzMMmZGAAbAoZZWamZmFAMwMDAsMGwAG"},
    {11,  102, "MID1_Druid_Balance", "CYGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAWoMbNjxMDwsYmZmZhBjZZmlZYmZswyMLzMGzshhBYstMzgxsNCMBAAAYxMzMzgNDjxAAwMDMA"},
    {11,  103, "MID1_Druid_Feral_Wildstalker", "CcGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAjZwMzMzMmtFPwyMbzYGzMDAAAALBzGMmZUzYWYmZGjZmZAAAAAAAGAAAABAz2MLNbzssBmZAWMDGAAzMAYA"},
    {11,  104, "MID1_Druid_Guardian", "CgGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgZmxsYmZMzmZZgZbZgxMMaimZmFzMzMLjZeADAAAAgZYGLzAAAAQNzysMzMDAgFMDgFzgBsYZbAwMbwA"},
    {12,  577, "MID1_Demon_Hunter_Havoc_Fel-Scarred", "CEkAAAAAAAAAAAAAAAAAAAAAAYgZmZMjZmZmhJjZGAAAAAAwsZMbzMmZmtZmx2sNPwMMGzYZgtZxMGmNNNmZGDbAAAAAAAAMzgBAAAgB"},
    {12,  581, "MID1_Demon_Hunter_Vengeance_Annihilator", "CUkAAAAAAAAAAAAAAAAAAAAAAAAYMzMjhZkZmZGDzMzMDGzMmxMmhxMmZsMmZZMmBAAAAAAAgZmxGAAAAGYmZmZ2abmZGAYAAAAMA"},
    {13, 1467, "MID1_Evoker_Devastation_SC", "CsbBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAzMDMDzgBmZGjZaYmpZMWmxMzMz8AzMzAmxMGzMLzMDMwYwCsMGN2GQmBBbYGMzghB"},
};

inline constexpr size_t kSimcMidnight1ProfileCount =
    sizeof(kSimcMidnight1Profiles) / sizeof(kSimcMidnight1Profiles[0]);

} // namespace Playerbot::V2::Talent
